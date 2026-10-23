# BlueZ 2.15 source accompanying CLIOPLAY `hidd`

CLIOPLAY distributes the tested ARM executable at
`card-template/clio843/hidd`. That program is licensed under the GNU General
Public License, version 2 only (GPL-2.0-only), and is not covered by CLIOPLAY's
MIT licence.

This is a source-compliance directory for developers and redistributors. It is
not part of the SD-card payload: end users do not need to build it, open it, or
copy it to the card.

This directory accompanies the executable with the complete upstream source
distributions, their original build system, the licence text, and provenance
metadata:

| File | Purpose | SHA-256 |
| --- | --- | --- |
| `bluez-utils_2.15.orig.tar.gz` | Complete BlueZ utilities 2.15 source; `hidd/` contains the daemon source and build files | `dc6c4d4e1a89872d7d7859d0c909fa473c5af6da69a70a60cad278c6a36a2710` |
| `bluez-libs_2.15.orig.tar.gz` | Complete matching BlueZ library source and headers used by the utility | `d37423d9eb3c5d4306db5876be2876f4b409c4156c127604308b4638a7f726d5` |
| `bluez-utils_2.15-1.1.dsc` | Signed Debian source descriptor for the utilities archive | `cdfd49622adf4ed713edbfe6eb96b262ffb08c92d09393461f658cbb39ca7d26` |
| `bluez-libs_2.15-2.dsc` | Signed Debian source descriptor for the library archive | `ba036997b6021b56063f8a0a39400c5e63415d01a36d5a14cbb9e3a88d5755ec` |
| `COPYING` | GNU GPL version 2 licence text from the utilities distribution | `32b1062f7da84967e7019d01ab805935caa7ab7321a7ced0e30ebe75e5df1670` |

The archives were retrieved without modification from Debian's historical
archive:

- <https://archive.debian.org/debian-archive/debian-security/pool/updates/main/b/bluez-utils/>
- <https://archive.debian.org/debian/pool/main/b/bluez-libs/>

From this directory, verify the local copies with
`shasum -a 256 -c SHA256SUMS` (or `sha256sum -c SHA256SUMS`).

On Windows PowerShell, the equivalent spot check is:

```powershell
@(
  ".\bluez-utils_2.15.orig.tar.gz"
  ".\bluez-libs_2.15.orig.tar.gz"
  ".\COPYING"
) | Get-FileHash -Algorithm SHA256
```

This directory is not used during normal Windows installation.

The `hidd` sources identify Marcel Holtmann as copyright holder for the daemon
and state GPL version 2. Preserve all copyright and licence notices when
redistributing them.

## Correspondence to the shipped executable

The shipped project executable has SHA-256:

```text
6830044438d40b781ddcc49853d14e3291b6b70eda82176596209ce908c0e53d
```

The ClioPlay maintainer identifies the project's `hidd` as their work. Their
original project build tree has not been recovered. The prior static audit
associated the stripped ARM executable with BlueZ 2.15 `hidd/main.c`,
`hidd/sdp.c` and `hidd/hidd.h`, including its option handling, SDP/L2CAP and
kernel HIDP integration. It records GCC 3.3.4 and links against
`libbluetooth.so.1` and glibc.

The upstream archives are retained as the available source basis, with their
original licences and notices. They are not a recovered copy of the maintainer's
project tree. An exact downstream source/build correspondence has not been
established by a fresh rebuild, and possible project-specific changes remain
unconfirmed. Do not describe the original project source as recovered or assume
that the executable is entirely original MIT-licensed code.

See [`BUILDING.md`](BUILDING.md) for extraction and cross-build instructions.
Because this is old embedded software, recreating the exact byte sequence
requires the historical compiler, linker, configuration, and stripping
behavior; the GPL does not require a byte-identical rebuild.

## Redistribution

Keep all of these files in every source release that includes the `hidd`
binary. Do not publish a binary-only CLIOPLAY package that omits this directory,
the licence, or the notices. Anyone receiving the binary may copy, study,
modify, and rebuild the accompanying source under GPL version 2.
