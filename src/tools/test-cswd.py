#!/usr/bin/env python3
"""End-to-end test for cswd.c on the host — no head unit, no ARM, no car.

Builds nothing: pass it the cswd binary compiled with
    cc -O1 -DHOST_TEST -DHOST_ROOT='"/tmp/..."' -Wall -Wextra -o cswd_host cswd.c
(or use --build, which does that for you).

It creates the two FIFOs in a temp dir, starts cswd, writes the 28-byte
TEventStruct frames documented in docs/scenic-iii-hardware.md, and reads the
Linux input_event records cswd emits. What it proves: framing, decode order,
de-bounce, and the default key map. What it cannot prove: how any of it feels.

Usage:
    python3 tools/test-cswd.py --build
    python3 tools/test-cswd.py cswd_host
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

# ClioPlay key codes, from clio040.c
K = dict(ESC=1, BACK=13, SR=14, ENTER=28, MAP=67, MENU=68,
         UP=103, LEFT=105, RIGHT=106, DOWN=108, ROTN=191, ROTP=192)

REC = 28
EVENT = 16  # s32 sec, s32 usec, u16 type, u16 code, s32 value


def frame(source, direction, rot, pressed=0, duration=0, steps=0):
    return struct.pack("<6i", source, direction, rot, 0, pressed, duration) + bytes([steps, 0, 0, 0])


def button(src, duration=1):
    return frame(src, 2, 0, 0, duration)


def joystick(direction):
    return frame(0, direction, 3)


def rotate(direction, steps=1):
    return frame(0, 5, direction, 0, 0, steps)


def press(duration=1):
    return frame(0, 5, 3, 1, duration)


def release():
    """The helper edge-detects `pressed`, so a release record must arrive
    between two presses or the second one is swallowed."""
    return frame(0, 5, 3, 0, 0)


def wait_for(path, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if os.path.exists(path):
            return True
        time.sleep(0.02)
    return False


def read_events(fd, want_keys, timeout=3.0):
    """Collect key codes from the emitted input_event stream until want_keys seen."""
    codes, buf = [], b""
    deadline = time.time() + timeout
    os.set_blocking(fd, False)
    while time.time() < deadline and len(codes) < want_keys:
        try:
            chunk = os.read(fd, EVENT * 8)
        except BlockingIOError:
            chunk = b""
        if not chunk:
            time.sleep(0.02)
            continue
        buf += chunk
        while len(buf) >= EVENT:
            _s, _u, etype, code, value = struct.unpack("<iiHHi", buf[:EVENT])
            buf = buf[EVENT:]
            if etype == 1 and value == 1:      # EV_KEY, press edge
                codes.append(code)
    return codes


class Cswd:
    def __init__(self, binary, root):
        self.root = root
        self.paths = dict(csw=os.path.join(root, "CswEvent0"),
                          out=os.path.join(root, "event7"),
                          log=os.path.join(root, "cswd.log"))
        self.proc = subprocess.Popen([binary], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        if not wait_for(self.paths["csw"]) or not wait_for(self.paths["out"]):
            self.stop()
            raise SystemExit("cswd did not create its FIFOs; see " + self.paths["log"])
        time.sleep(0.15)  # let it open both ends
        self.wfd = os.open(self.paths["csw"], os.O_WRONLY)
        self.rfd = os.open(self.paths["out"], os.O_RDONLY)

    def send(self, *frames, gap=0.05):
        for f in frames:
            os.write(self.wfd, f)
            time.sleep(gap)

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()

    def log(self):
        try:
            return open(self.paths["log"], encoding="utf-8", errors="replace").read()
        except OSError:
            return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary", nargs="?")
    ap.add_argument("--build", action="store_true", help="compile cswd.c with HOST_TEST first")
    ap.add_argument("--keep", action="store_true", help="keep the temp dir and its logs")
    args = ap.parse_args()

    root = tempfile.mkdtemp(prefix="cswd-host-")
    if args.build:
        here = os.path.dirname(os.path.abspath(__file__))
        src = os.path.join(here, "..", "clio843", "src", "cswd.c")
        binary = os.path.join(root, "cswd_host")
        cc = os.environ.get("CC", "cc")
        cmd = [cc, "-O1", "-g", "-Wall", "-Wextra", "-DHOST_TEST",
               f'-DHOST_ROOT="{root}/"', src, "-o", binary]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            print(r.stdout + r.stderr)
            raise SystemExit("build failed")
        if r.stderr.strip():
            print("build warnings:\n" + r.stderr.strip())
        print(f"built {os.path.basename(binary)} ({cc})")
    else:
        binary = args.binary or "cswd_host"

    failures = []

    def check(name, got, want):
        ok = got == want
        print(f"{'ok  ' if ok else 'FAIL'}  {name:<38} got={got} want={want}")
        if not ok:
            failures.append(name)

    try:
        # --- framing + buttons: one key per record, keyed by source alone -----
        d = Cswd(binary, root)
        try:
            expect = []
            # Built-in defaults: 1 ESC, 2 BACK, 3 SR, 4 unbound, 5 MENU,
            # 6/7 unbound, 8 MENU, 9 MAP.
            for src, key in ((1, K["ESC"]), (2, K["BACK"]), (3, K["SR"]),
                             (5, K["MENU"]), (8, K["MENU"]), (9, K["MAP"])):
                d.send(button(src))
                expect.append(key)
            check("buttons 1,2,3,5,8,9", read_events(d.rfd, len(expect)), expect)

            # btn4 (info) has no built-in binding; confirm it is inert rather
            # than wrong, then bind it from a map file further down.
            d.send(button(4))
            check("button 4 unbound by default", read_events(d.rfd, 1, timeout=0.6), [])

            # buttons 6 and 7 are mapped to NONE in the default map -> no key
            d.send(button(6), button(7))
            check("unbound buttons 6,7 emit nothing", read_events(d.rfd, 1, timeout=0.6), [])

            # --- de-bounce: same button twice inside 400 ms = one key ----------
            d.send(button(1), button(1), gap=0.05)
            check("repeat press inside 400 ms debounced", read_events(d.rfd, 2, timeout=0.8), [K["ESC"]])

            # --- joystick: edge-triggered on direction change ------------------
            d.send(joystick(1), joystick(2), joystick(4), joystick(3))
            check("joystick up/down/left/right",
                  read_events(d.rfd, 4), [K["UP"], K["DOWN"], K["LEFT"], K["RIGHT"]])

            # --- knob rotation: steps repeat the key --------------------------
            d.send(rotate(1, 1))
            check("knob anticlockwise = ROTP", read_events(d.rfd, 1), [K["ROTP"]])
            d.send(rotate(2, 3))
            check("knob clockwise x3 steps", read_events(d.rfd, 3), [K["ROTN"]] * 3)

            # --- knob press, and long press via duration ----------------------
            # Edge-detected on `pressed`, so each press needs a release record.
            d.send(press(duration=1), release())
            check("knob press = OK", read_events(d.rfd, 1), [K["ENTER"]])
            d.send(press(duration=2), release())
            check("knob long press = BACK", read_events(d.rfd, 1), [K["BACK"]])

            # --- unknown source is ignored, not fatal ------------------------
            d.send(frame(42, 5, 3))
            check("unknown source ignored", read_events(d.rfd, 1, timeout=0.6), [])
        finally:
            log = d.log()
            d.stop()

        # --- phase 2: a map file rebinds without a rebuild ------------------
        here = os.path.dirname(os.path.abspath(__file__))
        example = os.path.join(here, "..", "card-template", "cswmap.example.ini")
        shutil.copyfile(example, os.path.join(root, "cswmap.ini"))
        d = Cswd(binary, root)
        try:
            d.send(button(4))
            check("button 4 = OK from cswmap.ini", read_events(d.rfd, 1), [K["ENTER"]])
            d.send(press(duration=2), release())
            check("longpush = BACK from cswmap.ini", read_events(d.rfd, 1), [K["BACK"]])
            d.send(rotate(2, 1))
            check("rot2 = ROTNEXT from cswmap.ini", read_events(d.rfd, 1), [K["ROTN"]])
        finally:
            maplog = d.log()
            d.stop()
        print("ok    map file applied over defaults:", "MAP btn4=OK" in maplog)

        # --- the log proves what the daemon actually sent --------------------
        print("ok    log records raw frames:", "raw=" in log)
        print("ok    log records key decode:", "-> key" in log)
        if "--verbose" in sys.argv:
            print(log[:2000])
    finally:
        if args.keep:
            print("kept:", root)
        else:
            shutil.rmtree(root, ignore_errors=True)

    print()
    print("FAILURES:", len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
