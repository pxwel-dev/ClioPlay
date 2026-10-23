# Install from Windows

Use **Install.cmd**. It runs the same guided ClioPlay installer through
**WSL 2 with Ubuntu 24.04**, which provides the map-building tools. Windows
Python, macOS and a separate ARM cross-compiler are not needed to install the
included programs. This is a WSL-based Windows path, not a native Windows builder.

## One-time setup

On Windows 11, or Windows 10 version 2004/build 19041 or newer, open PowerShell
**as administrator** and run:

```powershell
wsl --install -d Ubuntu-24.04
```

Restart if prompted. Open **Ubuntu 24.04** from Start and create its Linux user
account. In that Ubuntu window, install the desktop tools:

```sh
sudo apt update
sudo apt install python3 python3-venv python3-pip build-essential osmium-tool cabextract
```

The installer creates its own Python environment and installs the pinned map
libraries automatically. Internet access is needed for dependencies and the
verified official firmware download.

See Microsoft's [WSL setup](https://learn.microsoft.com/en-us/windows/wsl/install)
and [WSL commands](https://learn.microsoft.com/en-us/windows/wsl/basic-commands)
if the distribution is not installed or needs updating.

## Prepare the card

1. Download/extract the whole ClioPlay repository to a local Windows folder.
   Run it from the extracted folder, not inside the ZIP.
2. Download an `.osm.pbf` covering your journeys from
   [Geofabrik Downloads](https://download.geofabrik.de/).
3. Insert the SD card. Check its drive letter in File Explorer.
4. If the card is not already visible in Ubuntu, mount its Windows drive letter.
   **For a card shown as `E:` only**, run in Ubuntu:

   ```sh
   sudo mkdir -p /mnt/e
   sudo mount -t drvfs E: /mnt/e
   ```

   Substitute your actual card letter. This mounts the existing filesystem; it
   does not format it. Use the Windows drive-letter method for a card reader,
   not `wsl --mount` with a physical-disk number. See Microsoft's
   [drive-letter mounting guidance](https://learn.microsoft.com/en-us/windows/wsl/wsl2-mount-disk#differences-between-mounting-an-external-drive-with-windows-formatting-versus-linux-formatting).

## Run the installer

Double-click **Install.cmd** in the extracted ClioPlay folder. The launcher checks
Ubuntu and its tools, then opens the guided installer. At its prompts, use WSL
paths:

| Windows location | Path to enter in the installer |
| --- | --- |
| `C:\Users\you\Downloads\region.osm.pbf` | `/mnt/c/Users/you/Downloads/region.osm.pbf` |
| SD card `E:\` mounted as above | `/mnt/e` |

Confirm the map region, wait for the navigation build, then review and confirm
the SD-card target. Existing preferences, pairings and saved places are retained;
replaced files are backed up. The launcher never selects or formats a disk.

After installation, close any Ubuntu terminals using the card, run `sync` and
`sudo umount /mnt/e` in Ubuntu (using your card's mount path), then safely eject
the card in Windows. Keep the car off while preparing the card.

For a different installed Ubuntu distribution, run from PowerShell in the project
folder, replacing the distribution name:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\src\tools\install_windows.ps1 -Distribution Ubuntu-24.04
```

The script's execution-policy override applies only to that PowerShell process.
The public Windows CI checks its syntax, prerequisite failures, path forwarding
and installer exit codes with a mocked WSL command. An actual Windows/WSL card
installation and vehicle boot still require manual validation.
