# Physical console and temperature integration

ClioPlay reads physical console controls and VehicleStatus temperature inside
`clio040.c`, alongside Bluetooth-remote input and Focus temperature. Both readers
start automatically and use valid incoming events or readings. There is no
hardware-selection step or SD-card enable file. See the
[controls and configuration guide](../README.md#physical-console-and-temperature-sources).

## Hardware evidence and validation

The protocol contribution came from a 2010 UK Renault Scénic III with an armrest
console and non-LIVE Carminat TomTom, model ID 29 (`TomTom GO MARIGOT LYON C`).
The contributor reported `hw_remote=0`, `hw_handsfree=0`, `hw_bluetooth=1`, a
Linux 2.6.13 kernel and a Samsung S3C2443 CPU. The factory radio/audio module is
separate from the navigation unit.

Those observations establish the source of the protocol, not physical validation
of this combined ClioPlay build. Console legends, detents, held/released buttons,
source reconnects, simultaneous Bluetooth input and temperature still need a
head-unit trial. Factory-radio audio output, volume/mute and coexistence also
require vehicle testing; working input does not establish audio compatibility.

## Startup and card paths

`ttn` finds the mounted card containing `clio843/clio040`, changes to its root,
and prepares `/dev/input/CswEvent0` before Focus/vehicle startup. Existing FIFOs
are retained, and non-FIFO nodes are not replaced. Native card paths are relative
to that working directory; missing mount aliases are supplied without replacing
populated directories or mounts. Diagnostics use the existing Debug-only logs.

The factory vehicle daemon uses its normal vehicle configuration to publish
console events. ClioPlay opens the FIFO nonblocking with `O_RDWR`, handles
fragmented reads and writer reconnects, and retries an unavailable endpoint.
An open idle FIFO does not claim the controls. Valid FIFO records take priority
over duplicate evdev knob events; the Bluetooth remote remains usable.

## Console protocol

Records are 28 bytes, little-endian, without a magic header or length prefix:

| Offset | Field | Meaning |
| --- | --- | --- |
| 0 | `source`, 32 bits | 0 = joystick/knob; 1–9 = hard buttons |
| 4 | `direction`, 32 bits | 1 = up, 2 = down, 3 = right, 4 = left, 5 = neutral |
| 8 | `rotation`, 32 bits | 1 = anticlockwise, 2 = clockwise, 3 = none |
| 12 | Direction state, 32 bits | Not used for action selection |
| 16 | `pressed`, 32 bits | 0 = released, 1 = pressed |
| 20 | `duration`, 32 bits | Short/long indicator; optional long-press mapping remains unverified |
| 24 | `steps`, 8 bits | Rotation count; accepted range 1–14 |
| 25–27 | Padding | Ignored |

For hard buttons, only `source` is trusted: the remaining fields can contain
uninitialized data. Each button has a sliding 250 ms burst guard. Joystick
direction changes act immediately, with held-direction repeats at 150 ms.
Rotation preserves valid detent counts. OK uses the real `pressed` 0→1 edge;
neutral direction is not a click, and a rendering stall cannot create another OK.
Unknown sources and invalid joystick layouts are ignored. Fixed queues and
bounded reads keep input work within the existing approximately 33 ms slice.

Default actions, contextual knob navigation and optional `cswmap.ini` overrides
are documented in the [controls guide](../README.md#physical-console-and-temperature-sources).
Every remote button has a default physical console equivalent, listed in the
[shared input guide](../README.md#remote-and-console-controls). No long press is
needed to reach any remote action.

## Temperature protocol and selection

The inline receiver listens nonblocking on navigation-side Unix sequenced-packet
sockets `/var/run/PAC<port>` for ports 3400, 3401, 3700, 3800, 4000, 4001, 5600,
5700 and 5800. It does not replace a live endpoint owned by another service.

Temperature comes from port 3700 as an exact 11-byte little-endian packet:
`u16 interface=37`, `u8 method=1`, `u32 item=4`, `s32 temperature` in °C.
Unknown `-1024`, malformed packets and values outside −60…85 °C are rejected.
No replies or identity/VIN logs are produced.

Valid Focus `/tmp/cfstate` temperature takes priority. Otherwise a valid connected
VehicleStatus sample is used; with neither available, only the time is shown.
Unchanged temperature does not expire solely through age. Disconnecting or
replacing the VehicleStatus connection invalidates its cached sample until a
fresh reading arrives. This works independently of console input. Neither a
second temperature file nor writes into Focus state are needed.

## Verification and provenance

From the repository root, `python3 src/tools/verify_ci.py` runs the integrated
`tests/vehicle-input.c` checks: framing, button edges, detents, mapping reloads,
automatic startup, idle/invalid source isolation, real FIFO reconnects, UI actions,
both control paths, temperature precedence and malformed data. Socket transport
tests run on hosts with Unix `SOCK_SEQPACKET`; unsupported hosts skip that portion.
The coordinate-entry, Explore/zoom, Bluetooth transport and Settings regression
workflows also run through decoded console records, including press/release
edges, rather than injecting remote key codes directly. Host checks do not
establish head-unit timing or hardware compatibility.

The implementation incorporates protocol findings from
[Renatka](https://github.com/bobczan/Renatka) and
[ClioPlay PR #2](https://github.com/pxwel-dev/ClioPlay/pull/2).
The contributed `cswd.c` and `vstatd.c` remain source references; they are not
built or launched as runtime services. Attribution is retained in
[third-party notices](../THIRD_PARTY_NOTICES.md).
