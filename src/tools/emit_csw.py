#!/usr/bin/env python3
"""Emit Carminat armrest-console (CSW) records to a FIFO for protocol testing.

Writes the exact 28-byte frames documented in CONSOLE-CSW-NOTES.md §3.4:
7 x little-endian u32  (source, direction, rotation, pressed, duration, steps,
unnamed). For console buttons only `source` is meaningful; the rest is filled
with the same junk values the real daemon produced so a decoder that keys off
`source` alone still works.

Usage:
    python3 emit_csw.py /dev/input/CswEvent0 --all
    python3 emit_csw.py FIFO --button 4
    python3 emit_csw.py FIFO --joystick up
    python3 emit_csw.py FIFO --knob cw --times 5
    python3 emit_csw.py FIFO --press                 # 20-record hold burst
    python3 emit_csw.py FIFO --raw 0,5,1,0,0,0,0

Open the FIFO with O_NONBLOCK so the test does not block waiting for a reader;
if a reader is not attached yet, records are dropped rather than queued, so start
the target first.
"""

import argparse
import errno
import os
import struct
import sys
import time

REC = 28

BUTTONS = {
    1: "top-left soft key (ESC)",
    2: "back (EQUAL)",
    3: "top-right soft key (BACKSPACE)",
    4: "info (ENTER)",
    5: "menu (F10)",
    6: "light (unbound)",
    7: "repeat (unbound)",
    8: "dest (F23)",
    9: "map (F9)",
}

JOYSTICK = {"up": 1, "down": 2, "right": 3, "left": 4}
KNOB = {"acw": 1, "cw": 2}          # anticlockwise / clockwise
ROT_PRESS = 3


def pack(src, direction, rotation, pressed=0, duration=0, steps=0, unnamed=0):
    """Pack one 28-byte record, 7 x LE u32."""
    data = struct.pack("<7I", src, direction, rotation, pressed, duration, steps, unnamed)
    assert len(data) == REC, len(data)
    return data


def button(src):
    # dir/rot are uninitialised in real button records; use the observed junk.
    return pack(src, 2, 0)


def joystick(direction):
    return pack(0, direction, ROT_PRESS)


def knob(rotation):
    return pack(0, 5, rotation)


def press():
    return pack(0, 5, ROT_PRESS)


def open_fifo(path):
    """Open the FIFO for writing without hanging when no reader is attached.

    POSIX: O_NONBLOCK makes a readerless FIFO fail immediately with ENXIO.
    Windows: os.O_NONBLOCK does not exist, so fall back to a plain open — this
    is only reached for the regular-file capture used by the host tests.
    """
    flags = os.O_WRONLY
    nonblock = getattr(os, "O_NONBLOCK", 0)
    try:
        return os.open(path, flags | nonblock)
    except OSError as exc:
        # ENXIO/EAGAIN means "FIFO exists but nobody is reading yet". Retrying
        # blocking here would hang forever, so report it instead.
        if exc.errno in (errno.ENXIO, errno.EAGAIN):
            sys.exit(
                f"cannot open {path}: {exc}\n"
                "The FIFO exists but no reader is attached. Start the target first,\n"
                "or create the node with:  mkfifo /dev/input/CswEvent0"
            )
        if nonblock:
            try:
                return os.open(path, flags)
            except OSError as retry_exc:
                sys.exit(f"cannot open {path}: {retry_exc}")
        sys.exit(f"cannot open {path}: {exc}")


def send(fd, *records, gap=0.12):
    for rec in records:
        os.write(fd, rec)
        time.sleep(gap)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("fifo")
    ap.add_argument("--all", action="store_true", help="every documented control, in order")
    ap.add_argument("--button", type=int, metavar="1-9")
    ap.add_argument("--joystick", choices=sorted(JOYSTICK))
    ap.add_argument("--knob", choices=sorted(KNOB))
    ap.add_argument("--press", action="store_true", help="one knob-hold burst (20 records)")
    ap.add_argument("--raw", help="7 comma-separated u32 field values")
    ap.add_argument("--times", type=int, default=1)
    ap.add_argument("--gap", type=float, default=0.12, help="seconds between records")
    args = ap.parse_args()

    fd = open_fifo(args.fifo)
    try:
        if args.raw:
            fields = [int(x, 0) for x in args.raw.split(",")]
            if len(fields) != 7:
                sys.exit("--raw needs exactly 7 values")
            send(fd, pack(*fields), gap=args.gap)
        elif args.press:
            # Real daemon repeats the same record while held; a decoder without a
            # burst guard fires ~20 times here.
            for _ in range(20):
                os.write(fd, press())
                time.sleep(0.03)
        elif args.button is not None:
            if args.button not in BUTTONS:
                sys.exit(f"--button must be 1..9; {BUTTONS}")
            for _ in range(args.times):
                send(fd, button(args.button), gap=args.gap)
        elif args.joystick:
            for _ in range(args.times):
                send(fd, joystick(JOYSTICK[args.joystick]), gap=args.gap)
        elif args.knob:
            for _ in range(args.times):
                send(fd, knob(KNOB[args.knob]), gap=args.gap)
        elif args.all:
            print("buttons 1..9")
            for src in sorted(BUTTONS):
                send(fd, button(src), gap=args.gap)
            print("joystick up/down/right/left")
            for name in ("up", "down", "right", "left"):
                send(fd, joystick(JOYSTICK[name]), gap=args.gap)
            print("knob acw/cw x5")
            for _ in range(5):
                send(fd, knob(KNOB["acw"]), gap=0.06)
            for _ in range(5):
                send(fd, knob(KNOB["cw"]), gap=0.06)
            print("knob press burst (should be ONE key)")
            for _ in range(20):
                os.write(fd, press())
                time.sleep(0.03)
        else:
            sys.exit("nothing to send: use --all, --button, --joystick, --knob, --press or --raw")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
