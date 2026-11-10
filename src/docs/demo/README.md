# ClioPlay UI previews

| Preview | What it shows |
| --- | --- |
| [Driving](driving.gif) | A new road name appears. A camera alert joins the same pill and remains after the name fades. |
| [Menus](menus.gif) | Menu, Trip, favourites, Rename, Bluetooth and Settings, with a switch to Day. |
| [Bluetooth](bluetooth.gif) | The volume notice expands to show playback controls, then shrinks again. The pause button highlights when pressed. |
| [Phone destination](phone-destination.gif) | The head unit receives a destination, shows moving route progress and lists the destination in Trip. |

These silent loops use 480 × 272 frames from the production C renderer.
They show an example route around Newcastle upon Tyne. Car and phone inputs
are controlled samples. The destination preview feeds a sample message into
the ClioPlay daemon/UI handoff. Camera positions are illustrative. The timings
describe the GIFs. Head-unit performance, Bluetooth and audio require a
[vehicle trial](../next-release/hardware-trial.md).

## Regenerate

Build and copy the current executables as described in [BUILDING.md](../../BUILDING.md).
With Pillow and a host C/C++ compiler, run from `src/`:

```sh
python3 tools/render_previews.py --maps /path/to/clio843/maps
python3 tools/refresh_latest.py
```

The [manifest](manifest.json) records preview, source and executable hashes.
The map bundle is read in place. Captured PNGs, raw framebuffers and host
executables stay in a temporary directory.
