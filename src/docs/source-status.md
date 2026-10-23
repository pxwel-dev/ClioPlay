# Source status

Paths below are relative to the project source root (`src/` in the publication folder).

Current UI (`clio040.c`), splash (`cliospl.c`), phone-audio receiver
(`clioa2dp.c`), startup scripts and navigation-builder sources are
included. The following legacy project build sources have not been recovered.
The maintainer identifies the project's `focusd` and `hidd` as their work.

## focusd

Binary: `card-template/clio843/focusd`. Original source name: `focusd_pac310.c`.

It binds a Unix sequenced-packet socket at `/var/run/PAC09900` and signals
readiness through `/tmp/cfready`. The startup script then restarts TomTom's
vehicle daemon so it connects to this replacement Focus endpoint. `focusd`
handles connect/disconnect and full-mode handshakes, template changes, and
vehicle display properties. It writes source, temperature, ice indication and
viewer/template IDs to `/tmp/cfstate`, and raw/decoded events to `/tmp/cfbus`.
The UI reads these to display temperature and reconstruct radio source, station,
frequency and volume. It does not implement maps, routing or Bluetooth audio.

The available decompiler listing and assembly/data references are included in
`clio843/src/recovered/`. They are reference material, not the original source
or a proven replacement. The official Navcore CAB does not contain this helper.

## hidd

Binary: `card-template/clio843/hidd`. The original project build tree has not been
recovered. BlueZ 2.15 upstream source archives and build instructions are
included in `third_party/bluez-2.15/`.

`btstart.sh` runs it with `--server -n` after bringing up the Bluetooth controller.
It accepts the Renault remote's Bluetooth HID connection. Its HID service uses
L2CAP control/interrupt channels and the kernel HIDP interface to attach the
remote's reports to Linux input handling; ClioPlay reads those input events for
remote navigation. It is separate from the `clioa2dp` phone-audio receiver.

The earlier binary audit associated this executable with BlueZ 2.15. Those
upstream sources and notices are retained. This does not recover the maintainer's
original build tree or establish whether any project-specific patch existed.

Before a public binary release containing `hidd`, confirm and provide its
corresponding source/build instructions, including any downstream changes, or
replace it with a tested build from available source. The upstream archives alone
do not establish that correspondence. See the
[GNU guidance on complete corresponding source](https://www.gnu.org/licenses/gpl-faq.html).

## clioaux-init

Binary: `legacy-binaries/clioaux-init`, also embedded byte-for-byte in the
`card-template/ttn` shell script. Original source name: `clioaux_init.c`.

The bootstrap decodes this ARM helper into RAM, checks its checksum and runs it
once before the splash and Bluetooth startup. It checks the model and codec
through `/proc/barcelona/`, requiring model ID 29 and a readable codec type
before making writes. It opens the hardware/audio devices and applies the
established AUX sequence: volume zero, navigation/internal unmute, codec register
34/37 values of 144 and external volume 118. It reports failed operations and
returns a status to the bootstrap. This initialisation is independent of phone
Bluetooth and remains enabled.

The extracted binary is preserved to make future source recovery possible;
installation continues using the established embedded copy in `ttn`.

`legacy-binaries/manifest.json` records sizes and SHA-256 hashes. These binaries
are included so the current card setup remains usable. The project must not be
described as fully rebuildable from source until these gaps are resolved.
