# Update an existing ClioPlay installation

Use the software-only updater if ClioPlay is already installed on your SD card
and you want the latest version while retaining your existing navigation maps,
settings and personal data. You do **not** need to rebuild maps, download an
OSM extract, download Navcore again, compile anything or regenerate previews.

For a new card, missing firmware, damaged installations or a new map format,
use the [fresh installation instructions](../README.md) instead.

## 1. Prepare

1. Fully switch off Carminat with the key, wait for it to finish shutting down,
   then remove the SD card.
2. Copy the **entire card** to a safe folder on your computer. Keep this private:
   it may contain saved places, pairings, GPS history and other personal data.
   The updater also keeps a small software backup on the card, but it is not a
   substitute for a full backup on a different disk.
3. Download the latest official [ClioPlay release](https://github.com/pxwel-dev/ClioPlay/releases/tag/Release)
   and extract the complete package to a folder on your computer, not onto the
   SD card. The updater uses the packaged ARM programs and their build reports.
4. Insert the existing SD card into your computer. Identify its root folder
   (the directory containing `ttn`, `ttsystem`, `vehicledaemon` and `clio843/`).

Use Python 3.11 or newer. No compiler, WSL, map-building dependencies or
additional Python packages are needed for this mode.

## 2. Update

Open a terminal in the extracted package's root, alongside `install.py`.

**macOS** (replace `CLIOPLAY` with the card's actual volume name):

```sh
python3 install.py --update "/Volumes/CLIOPLAY"
```

**Linux** (replace the example path with your card's actual mount point):

```sh
python3 install.py --update "/media/your-user/CLIOPLAY"
```

**Windows** using Python in PowerShell (replace `E:` with the actual
card drive letter; install Python 3.11+ if it is not already available):

```powershell
py -3 install.py --update E:/
```

Alternatively, from WSL use `python3 install.py --update /mnt/e` after mounting
the correct drive as described in the [Windows setup guide](../../WINDOWS.md).
Do not double-click the fresh-install launcher for a software-only update.

Add `--dry-run` to any of these commands to check the package, card and
available space without changing any card files. The normal command shows the
target and old/new versions, and requires explicit confirmation. Check the
path carefully before answering `yes`.

Wait for `Update complete` and note the backup directory reported by the
installer. Safely eject the card, insert it in the powered-off head unit and
start ClioPlay. Check the new version, existing saved places, map and Bluetooth.

## 3. What gets replaced?

The updater has a fixed list of eight software files:

| File | Purpose |
| --- | --- |
| `ttn` | Carminat boot script |
| `clio843/btstart.sh` | Bluetooth startup |
| `clio843/focusd` | Console integration |
| `clio843/hidd` | Bluetooth helper |
| `clio843/clio040` | Main application |
| `clio843/cliospl` | Splash program |
| `clio843/clioa2dp` | Bluetooth audio program |
| `clio843/font.bin` | Runtime font |

The original software files are copied into a dated
`clioplay-update-backup-.../` folder on the card before replacement.

**Everything else stays in place**, including:

- `clio843/maps/` and map build manifests.
- `clio843/prefs.ini` and any custom console configuration.
- `bond.dat`, `faves.dat`, `recent.dat`, `trip.dat`, `audlast.dat` and `gpslast.dat`.
- `history.dat`, `history.prev.dat`, `mobility.dat`, `mobility-places.dat`, `mobility-ml.dat` and other learned data.
- Navcore firmware, other files and folders on the SD card.

The existing `clio843/card-runtime.json`, if present, remains the record of the
original full installation; it is not a live software-version manifest.
The update backup contains its own checksums and version information.

## 4. Compatibility

File preservation and format compatibility are different things. The updater
does not migrate old data. ClioPlay 2.2.7 uses CTR4 trips, CLP2
saved places, CPM5 display maps, CPR2 v5 routing and optional QAD7, with
version-2 navigation bundle manifests, plus the optional companion
`mobility-ml.dat` (`MML1`) learning file alongside unchanged `mobility.dat`
(`MOB1`). Compatible 2.1.8–2.2.6.4 maps and state can be reused with this update.
When rebuilding maps with fresh OSM extracts, run `python3 tools/replay_history.py`
locally to migrate raw driving history onto the new map geometry. To add map
coverage with multiple extracts, use the fresh installation workflow.

Earlier saved formats may remain on disk but will be ignored by a newer
runtime. For obsolete maps, build/install compatible navigation data separately.
Check the [changelog](changelog.md) for future compatibility changes before updating.

This updater expects an existing versioned ClioPlay Global installation with
all eight software files and the supported Navcore 8.843 components already
present. It refuses missing or modified Navcore files, redirected paths and
stale or corrupt release binaries. It never formats the card or repairs vendor
firmware. Use the fresh installer if the existing installation is incomplete.

## 5. Roll back

If an ordinary write fails, the updater attempts to restore every replaced
file from its backup and reports whether restoration succeeded. A sudden power
loss or card removal can interrupt any filesystem operation; never unplug the
card while updating.

If the new version does not work on your head unit:

1. Fully switch off Carminat and remove the SD card.
2. Locate the backup directory reported for that update. Its
   `update-backup.json` identifies the old/new versions and SHA-256 checksums.
3. Copy the eight backed-up software files back to their original locations on
   the SD-card root, preserving the directory structure (copy `ttn` and merge the
   backup `clio843/` into the existing `clio843/`, **not** replacing that folder).
4. Safely eject and test the restored installation.

If the update changed how personal state files are written, restoring the old
software alone might not restore their compatibility. Restore the full external
card backup if necessary. Do not delete your only working backup until you have
confirmed the update succeeds.

## For developers

A local source rebuild can also be installed with this updater after
`python3 tools/rebuild_arm.py` has produced the current three programs and build
reports. Run the updater from the repository root. It checks current source
inputs as well as binary and bootstrap hashes; a stale build report is not
accepted just because an old binary matches it.

See the [release workflow](release-workflow.md) for development and publication
steps. Software-only installation does not require GIF regeneration or a
publication refresh when the local rebuilt artifacts are current.
