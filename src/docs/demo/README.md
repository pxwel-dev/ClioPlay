# ClioPlay UI previews

| Preview | What it shows |
| --- | --- |
| [Popups](popups.gif) | Route confirmation, destination-source choices, saved-phone actions and the keyboard language picker share glass entrance, exit, text and focus transitions. |
| [Song and radio updates](metadata-updates.gif) | New song titles animate in, including during BT CTRL. Radio tuning updates even when the station name stays the same. |
| [Status notifications](status-notifications.gif) | GPS acquisition and recovery, phone connection, volume and no-active-route messages share one animated pill. |
| [Driving](driving.gif) | A new road name appears. A camera alert joins the same pill and remains after the name fades. |
| [Menus](menus.gif) | An active volume pill animates into Menu, persists when opening Trip, then fades out on its original timer. Favourites, glass keyboard, Bluetooth, Settings, custom Auto times and return to driving follow. |
| [Bluetooth](bluetooth.gif) | The volume notice expands to show controls and a thin track progress bar. A reported seek eases forward, and pause highlights when pressed. |
| [Phone destination](phone-destination.gif) | The real ClioPlay Send website beside the native head-unit UI: send, route progress and the received destination. No receive option is required; Bluetooth transport is simulated. |
| [Speeding warning](speed-warning.gif) | The speed pill pulses red above a confirmed 30 mph limit and eases back when the car slows down. |
| [Map browsing](map-browsing.gif) | Manual zoom is temporary; MAP restores Auto before entering Explore. Explore appears once on entry, stays silent during later pan/zoom, and returns to Auto with one notice. |
| [Roundabout and ETA](roundabout-eta.gif) | The turn pill shows the third exit. OK displays the estimated time and remaining distance. |
| [Coordinate route](coordinate-route.gif) | Choose Coordinates, enter latitude/longitude and a name, review, calculate a real route, return to driving and find the destination in Recents. |
| [Map-point route](map-point-route.gif) | Enter Explore with MAP, pan to a point, press OK, confirm and calculate a real route back in the driving view. |
| [Coordinates](coordinates.gif) | Enter decimal latitude/longitude, name and review a destination, then confirm it into the route and Recents. |
| [Add a stop](add-stop.gif) | Trip adds a favourite as an intermediate stop and keeps the original destination. |

Native UI frames are 480 × 272 from the production C renderer. The phone-destination loop combines those unchanged frames with a captured browser panel on a 936 × 510 canvas. Website sources and capture tooling remain private. On regeneration, pass --phone-capture to supply new browser screenshots, or the existing GIF browser panel is reused while native UI frames are refreshed.
They show an example route around Newcastle upon Tyne. Car and phone inputs
are controlled samples. The destination preview feeds a sample message into
the ClioPlay daemon/UI handoff. Camera positions are illustrative. Motion is
sampled at approximately 15 frames per second during short transitions, and
5 frames per second while calculating a route. GIF timing uses 67 ms animation samples and 200 ms calculation samples. The timings
describe controlled host captures, not achieved head-unit frame rates. The runtime
uses separate map and animation schedules, capped together during calculation;
steady speed, distance, clock, volume and playback-progress values use the
10 FPS map cadence. New information, fades, resizing and warning pulses retain
the 15 FPS transition target.
Glass uses complete 8 × 8 area filtering plus a weighted blend of neighbouring
areas over a 24 × 24 footprint, with higher-precision colour calculations
and full-strength, screen-anchored 8 × 8 ordered dithering at the final RGB565
write. This reduces gradient banding with fine fixed grain; text, symbols and
exact selection colours retain their normal rendering. The pattern does not
cycle between frames. GIF palette reduction can obscure this detail compared
with the native RGB565 display. Unchanged-background samples are
cached during live overlay animations.
All popup windows use the same glass. Their entrances, exits, resizing, content
changes and selection highlights share the pills' independent 15 FPS transition
clock; map drawing remains on its normal schedule. The entire UI retains the
5 FPS cap during route calculation. Popup actions are handled immediately.
Explore camera passes require extra drawing work. Head-unit performance, Bluetooth and audio require a
[vehicle trial](../next-release/hardware-trial.md).

The coordinate-route and map-point-route loops invoke the production remote
handlers and routing code against the existing local navigation bundle. The
requested target and nonempty computed route are checked before the ready-state
frames are captured. Their calculation display is held for a controlled 1.3-second
sampling window so its animation is visible; this is not a measurement of route
calculation time on either the host or head unit. Sample GPS remains stationary.

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
