# ClioPlay UI previews

| Preview | What it shows |
| --- | --- |
| [Song and radio updates](metadata-updates.gif) | New song titles animate in, including during BT CTRL. Radio tuning updates even when the station name stays the same. |
| [Status notifications](status-notifications.gif) | GPS acquisition and recovery, phone connection, volume and no-active-route messages share one animated pill. |
| [Driving](driving.gif) | A new road name appears. A camera alert joins the same pill and remains after the name fades. |
| [Menus](menus.gif) | An active volume pill animates into Menu, persists when opening Trip, then fades out on its original timer. Favourites, glass keyboard, Bluetooth, Settings, custom Auto times and return to driving follow. |
| [Bluetooth](bluetooth.gif) | The volume notice expands to show controls and a thin track progress bar. A reported seek eases forward, and pause highlights when pressed. |
| [Phone destination](phone-destination.gif) | The head unit receives a destination, shows moving route progress and lists the destination in Trip. |
| [Speeding warning](speed-warning.gif) | The speed pill pulses red above a confirmed 30 mph limit and eases back when the car slows down. |
| [Map browsing](map-browsing.gif) | MAP opens browsing, the direction pad pans and the right soft key zooms in. MAP returns to guidance. |
| [Roundabout and ETA](roundabout-eta.gif) | The turn pill shows the third exit. OK displays the estimated time and remaining distance. |
| [Add a stop](add-stop.gif) | Trip adds a favourite as an intermediate stop and keeps the original destination. |

These silent loops use 480 × 272 frames from the production C renderer.
They show an example route around Newcastle upon Tyne. Car and phone inputs
are controlled samples. The destination preview feeds a sample message into
the ClioPlay daemon/UI handoff. Camera positions are illustrative. Motion is
sampled at approximately 20 frames per second during short transitions, and
5 frames per second while calculating a route. GIF timing uses 50 ms animation samples and 200 ms calculation samples. The timings
describe controlled host captures, not achieved head-unit frame rates. The runtime
uses separate map and animation schedules, capped together during calculation;
Explore camera passes require extra drawing work. Head-unit performance, Bluetooth and audio require a
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
