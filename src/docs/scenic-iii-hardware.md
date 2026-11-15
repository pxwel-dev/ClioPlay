# Hardware notes: Renault Scénic III / Mégane Carminat TomTom with physical console

Everything below was observed on a real car, not read out of the TomTom navigator.
It exists so a Scénic III can run ClioPlay without the Bluetooth remote, and so
the temperature can be shown rather than guessed.

Two ready-to-build helpers come with this document:

| File | Job |
| --- | --- |
| `src/clio843/src/cswd.c` | Armrest console bridge: reads the daemon's FIFO, decodes records, forwards ClioPlay key codes as `input_event` records on a FIFO that `clio040` already scans |
| `src/clio843/src/vstatd.c` | Outside temperature: listens on the PAC reflection sockets, writes `/tmp/cltemp` |
| `src/card-template/cswmap.example.ini` | Rebindable console map, no rebuild needed |

Both are self-contained (own `svc` syscall stubs, no libc, no dependencies), about
100 and 190 lines, and both build for ARM with a plain `clang -c`.

---

## 1. The vehicle and unit these facts come from

| Item | Value |
| --- | --- |
| Vehicle | Renault Scénic III, 2010, UK, right-hand drive |
| Unit | Carminat TomTom non-LIVE, model ID 29, `TomTom GO MARIGOT LYON C` |
| Firmware | 8.842 in internal memory; installer supplies 8.843 `ttsystem` + `vehicledaemon` from TomTom's own CAB |
| Kernel / CPU | Linux 2.6.13, Samsung S3C2443 (ARM920T) |
| Flags | `hw_remote=0`, `hw_handsfree=0`, `hw_bluetooth=1` |
| Controls | Armrest central switch: 9 buttons, joystick, rotary knob + knob press. **No handheld remote.** |
| Audio | Factory Bluetooth/audio module, separate from the navigation unit |

`hw_remote=0` is the important one: there is no IR/BT remote on this car, so
without a console reader there is **no input device at all**.

---

## 2. Card mount point — why a correct build appears to fail

Upstream `ttn` assumes `SD=/mnt/sdcard`. On this firmware **the card is mounted at
`/media/sdcard`**, and there is no `/mnt/sdcard`. Every binary lookup then fails,
`exec` never happens, and the unit falls through to the stock TomTom app — old
Carminat logo, "no map loaded". It looks like a broken build; it is a path failure.

Suggested fix: probe `/proc/mounts` plus a candidate list
(`/media/sdcard /mnt/sdcard /mnt/mmc /media/mmc`) and pick the directory that
actually contains `clio843/clio040`, then keep `/mnt/sdcard` as the fallback. Write
the result to a `bootdiag.txt` in the card root so a future boot report is
diagnosable from a photo of the card.

---

## 3. The console FIFO

### 3.1 What the daemon publishes

`vehicledaemon` publishes `UiInput::MCswInput::TEventStruct` records on the FIFO
`/dev/input/CswEvent0`.

**Nothing creates that node unless the stock navigator did.** The consumer has to:

```c
unlink(path);                                   /* optional */
mknod(path, S_IFIFO | 0666, 0);
```

`EEXIST` (-17) means it is already there — treat as success. This must happen
**before `vehicledaemon` is restarted**, i.e. before `focusd` takes over. If the
node is missing, `open()` fails forever and no control is ever seen, with no error
anywhere. This is the most likely integration bug.

### 3.2 Reading it

- Open `O_RDWR` (both helpers do) so the FIFO never reports EOF when the writer
  comes and goes; the inline variant uses `O_RDONLY|O_NONBLOCK` with a 1 s retry.
- There is **no magic header and no length prefix** — just back-to-back **28-byte**
  records. Accumulate partial reads; decode when the buffer is full.
- Poll from the existing input slice (the ~33 ms `poll_keys()` frame). No thread
  needed.

### 3.3 Record layout

`UiInput::MCswInput::TEventStruct`, from `vehicledaemon`'s
`CCentralSwitch::Set*` routines — little-endian `u32` fields:

| Offset | Field | Meaning |
| --- | --- | --- |
| 0 | `source` | 0 = joystick/knob, 1…9 = hard buttons |
| 4 | `dir` | 1…4 = up/down/right/left, 5 = centre |
| 8 | `rot` | 1/2 = rotation direction, 3 = none |
| 12 | `aux` | 1/2 |
| 16 | `pressed` | 1 = joystick/knob pushed |
| 20 | `duration` | 1 = short, 2 = long |
| 24 | `steps` | rotation steps, low byte |

```c
struct csw_event {s32 source,dir,rot,aux,pressed,duration; u8 steps,pad[3];};
typedef char csw_size_check[sizeof(struct csw_event)==28?1:-1];
```

**Trust `source` first.** For buttons the other fields are not reliably populated
and differ between presses of the same button, so buttons must be keyed by
`source` alone. `duration` is the one worth probing — see long-press below.

A packed alternative layout (`u16` source/dir, `s16` rot, `u16` pressed, then
`u32`s) was also seen on a different `vehicledaemon` build. If records decode as
nonsense, try that before blaming the FIFO.

### 3.4 Decode order and de-bounce

Order matters, because `source` wins over `dir`:

```
src 1..9           -> button slot btnN          (400 ms guard on the same src)
src == 0, dir 1..4 -> joystick slot dirN        (edge-triggered on dir change)
src == 0, rot 1|2  -> knob rot1/rot2, `steps` times (1..5 clamped)
src == 0, pressed  -> push, or longpush when duration == 2 (edge-triggered)
```

Values that felt right on the car:

| Guard | Value | Notes |
| --- | --- | --- |
| button repeat | 400 ms (helper) / 250 ms (inline) | the daemon repeats while held |
| knob press burst | one key per burst | 1200 ms guard in the inline reader |

Two ways to detect the knob press, pick one:

- **Edge on `pressed`** (what `cswd.c` does): fire when `pressed` goes 0→1, and use
  `duration` to choose push vs longpush. Requires the daemon to send a record with
  `pressed` back to 0 between presses — it does, but a decoder that only sees
  `pressed=1` records will swallow every press after the first.
- **Shape + burst guard** (what the Renatka 2.x inline reader does): treat
  `src=0, dir=5, rot=3` as a press and allow one per 1200 ms. Simpler and it
  cannot get stuck, but loses `duration`, so no long-press.

**Long-press:** the struct defines `duration` 1=short, 2=long, and `cswd.c` maps
`longpush` from it. It was never verified on this console. Treat it as "probably
available, needs a capture".

### 3.5 Recovered key map

`source` → the key codes `clio040.c` already defines, so no new key handling is
needed, only a new source of events:

| Control | `src` | maps to | default |
| --- | --- | --- | --- |
| top-left soft key | 1 | `K_ESC` (1) | zoom out / delete char |
| back | 2 | `K_BACK` (13) | back |
| top-right soft key | 3 | `K_SR` (14) | zoom in / next field |
| info | 4 | `K_ENTER` (28) | ETA |
| menu | 5 | `K_MENU` (68) | menu |
| light | 6 | — | unbound |
| repeat | 7 | — | unbound |
| dest | 8 | `K_MENU` (was `F23`=193 on an older build) | destination menu |
| map | 9 | `K_MAP` (67) | map browsing |
| joystick | 0 + dir 1‑4 | `K_UP`(103) / `K_DOWN`(108) / `K_RIGHT`(106) / `K_LEFT`(105) | pan / move |
| knob | 0 + rot 1‑2 | `K_ROTP`(192) / `K_ROTN`(191) | zoom / next item |
| knob press | 0 + `pressed` | `K_ENTER` | confirm / play-pause |

Two extra facts:

- The knob also appears on `/dev/input/event*` as `KEY_F21` (clockwise) /
  `KEY_F22` (anticlockwise), which the stock remote layout never maps.
- Logging each unmapped `(src, dir)` once — `UNMAPPED src=7 dir=5` — is what made
  this table recoverable. Worth keeping.

### 3.6 Two ways to integrate

**A. Keep the helper (`cswd.c`), no changes to the UI code.** It decodes records
and writes Linux `input_event` records (EV_KEY, value 1 then 0) into a FIFO at
`/dev/input/event7`, which `clio040`'s existing input scan already opens. Zero
coupling: the UI code does not know a console exists. Cost: one extra 3 KB binary
and a dependency on the event scan reaching `event7`.

**B. Read the FIFO inside `clio040`** (what the Renatka 2.x tree does): ~120 lines
in the main file, no extra process, no FIFO-in-a-FIFO. Cost: coupling, and the
decode lives in the file that changes most often. For an upstream integration this
is probably right long-term — but the record facts above are what matter, and the
place to put them is a decision for whoever owns `clio040`.

Either way, gate it: an empty `csw` file in the card root (or `/tmp/csw`) keeps the
path inert on normal installs, so nothing changes for remote-equipped cars.

---

## 4. Outside temperature

The unit's outside temperature is exposed through the **PAC reflection interface**,
not through `vehicledaemon`'s FIFO.

- `vehicledaemon` connects to `AF_UNIX` / `SOCK_SEQPACKET` sockets named
  `/var/run/PAC<port>`. `vstatd.c` listens on all nine it uses:
  **3400, 3401, 3700, 3800, 4000, 4001, 5600, 5700, 5800**.
- Only **3700** carries `VehicleStatus` updates. Packet:
  `u16 iid=37`, `u8 method=1`, `u32 id`, `u32 value` (little-endian, ≥11 bytes).
  **`id 4` = outside temperature in °C; `-1024` = unknown.**
- Nothing is ever sent back.
- `vstatd` writes `/tmp/cltemp` atomically (temp file + `rename()`), clamping
  outside −60…70 °C to `999`, and logs to `clio843/vstatd.log`. Its log recorded
  `STATUS id=4 value=14` — 14 °C — so interface 37 / item 4 is confirmed, not
  guessed.

**Suggestion for ClioPlay:** rather than adding a second temperature file, publish
the value as `temp=<n>` into `/tmp/cfstate`, which upstream already reads via
`car_read_temperature()` — and whose −60…85 validity window already rejects the
−1024 sentinel. Then `focusd` (or `vstatd`) is the only thing that has to know
about PAC, and the UI keeps one interface. `vstatd.c` is included here either way;
pointing it at `cfstate` instead of `/tmp/cltemp` is a small change.

---

## 5. Other hardware differences worth knowing

- **`hw_handsfree=0`** — the navigation unit does not handle calls; hands-free runs
  through the car's separate factory module. A missing call UI here is correct.
- **Two independent Bluetooth paths.** The Carminat's own module (`hw_bluetooth=1`)
  and the factory audio module are separate: a phone can pair with both, and audio
  does not come through the Carminat. Destination transfer from the phone may work
  while audio does not, or the reverse.
- **Radio/volume telemetry is sometimes unreadable.** Hiding the unavailable
  reading beats a placeholder — a `--` in the HUD reads as a fault.
- **The unit rejects firmware downgrades.** A factory card carrying 8.841 on a unit
  running 8.842 fails with
  `CheckSwVersion(Downgrade major version not possible!)` and
  `Start(Startup failed. retVal = 19)` — which is why such a card boots stock
  TomTom and looks bricked.
- The unit creates `loopdir/loopback.ex3` (~15 MB) itself at boot; it does not need
  to be supplied.

---

## 6. Verifying without a Scénic

`src/tools/test-cswd.py` does this for you:

```sh
python3 src/tools/test-cswd.py --build      # host cc, no ARM, no car
```

It creates both FIFOs in a temp dir, starts `cswd`, writes the 28-byte frames
documented above and reads back the emitted `input_event` records. Current result,
16 checks, 0 failures: buttons 1‑9 (including the two that are unbound by default),
the 400 ms repeat guard, all four joystick directions, knob rotation in both
directions with multi-step, knob press and long press, an unknown source ignored
without crashing, and a second run with `cswmap.example.ini` in place proving
`btn4=OK`, `longpush=BACK` and `rot2=ROTNEXT` rebind without a rebuild.

Both helpers also build clean for the device:

```sh
clang --target=armv6-linux-gnueabihf -marm -O2 -ffreestanding -nostdlib \
      -Wall -Wextra -I src/clio843/src -c src/clio843/src/cswd.c
clang --target=armv6-linux-gnueabihf -marm -O2 -ffreestanding -nostdlib \
      -Wall -Wextra -I src/clio843/src -c src/clio843/src/vstatd.c
```

Zero warnings under `-Wall -Wextra` on both, and both link static with
`-fuse-ld=lld -nostdlib`. `vstatd` has not been *run* anywhere except the car, since
it needs the real PAC sockets; `cswd` was run end to end on the host as above.

`src/tools/emit_csw.py` writes the same frames to any FIFO (buttons, joystick,
rotations, and a 20-record hold burst that must collapse to one key) if you want to
drive a real unit's FIFO by hand.

What stays untested without the car: whether 400 ms is right for a given thumb, and
whether the knob steps feel linear.

---

## 7. Open questions

1. Console input: helper on `event7`, or read the FIFO in `clio040`? Happy to cut
   the decoder to fit either.
2. Opt-in `csw` marker file, or auto-detect when `hw_remote=0`?
3. Temperature: is `cfstate`'s `temp=` the intended single interface?
4. Happy to test a build on the car — it is available, and since the console is the
   only input device, a regression is obvious immediately.
