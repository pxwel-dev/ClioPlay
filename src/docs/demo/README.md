# ClioPlay UI previews

| Preview | What it shows |
| --- | --- |
| [Live GPS panel](v225-gps-panel.gif) | Real Newcastle driving map showing double-**OK** entry: the first **OK** flashes the current street name and **ETA**, a second **OK** within 4 seconds opens the live frosted-glass **GPS** panel (compass dial, time to the second, heading, colour-coded `$GPGST` accuracy with satellite count, altitude and marine coordinates updating at 1 Hz), and **BACK** returns smoothly to driving. |
| [Usual route & calibrated ETA ranges](v224-ml-eta-and-habits.gif) | Real clear-road **Durham** (`Gilesgate` → `Neville's Cross`) route with zero traffic congestion, keeping the active route in its normal colour (no yellow) alongside the dashed teal usual road, showing a learned ETA reliability band (`~3-7 min`), **Usual route available · OK to review**, **Use usual route?** (**Based on your usual route**, **View route**), and **Usual route selected** on acceptance. |
| [Frequent & cold-weather slowdown alerts](v224-ml-slowdowns.gif) | Real **York** (`Station` → `Heworth`) route covering every repeat hard-braking scenario (with traffic diversions off and a solid-magenta route): warm `18 °C` **Frequent slowdown ahead** alert (`40–350 m` ahead), once-per-arc deduplication and automatic 4-second clear, silence at `18 °C` on a cold-only hotspot, **Cold-weather slowdown ahead** firing automatically when temperature drops to `1 °C` (`<= 3 °C`), and a winter-calibrated ETA check via **OK**. |
| [Usual parking, recurring stops & preferred POIs](v224-ml-parking-and-pois.gif) | Real Newcastle city-centre navigation demonstrating **Usual parking available** → **Route to usual parking?** (**Claremont Road Car Park**, tinting its mapped Parking POI badge in route colour instead of drawing a dot over it), recurring intermediate stop **Usual stop available** → **Add usual stop?** (**Coffee Stop**, marked with a route-coloured stop ring alongside the highlighted Parking POI), and **Trip → Nearby → Fuel** ranking a preferred station first with a cyan star badge. |
| [Adaptive Auto-zoom & Day/Night brightness](v224-ml-adaptive-hud.gif) | Real Newcastle driving map showing **Auto zoom** adopting a learned `+1` zoom preference in the `20–40 mph` band after consistent manual overrides, stepping out at higher speeds, and an **Auto** Day→Night transition inside **Settings → Display** automatically updating **Brightness** from learned `90%` (Day) to `30%` (Night). |
| [Offline Nearby POIs](v223-nearby-pois.gif) | Four-tab Trip header, Nearby categories (Fuel, Parking, Hospital) with 16 × 16 antialiased badges, live offline Newcastle hospital and fuel scans sorted by distance, and adding a nearby fuel station as a stop to the active itinerary. |
| [Expandable secondary manoeuvre cue](v223-secondary-manoeuvre.gif) | Real Newcastle city-centre route showing the **Then** secondary pill starting compact (`24 px`) for a normal turn, smoothly expanding (`43 px`) for a follow-up roundabout with the full roundabout picture and exit `3` clearly visible (`Take exit 3`), promoting into the main Turn card, and returning to compact `24 px` for a normal follow-up right turn. |
| [View and take a diversion](v223-view-diversion.gif) | Complete real-map Newcastle (`A167(M)` / Quayside) historical-traffic diversion flow: amber **Likely traffic ahead** warning and delayed ETA, live reveal of the bypass with **Traffic diversion available · OK to review** and floating **Save ~4 min** badge, the three-button **Take traffic diversion?** popup (**Likely traffic ahead**, **Yes**, **No**, **View diversion**), Explore inspection with zoom, pressing **MAP** to re-open confirmation, and accepting (**Diversion selected** followed by the shorter ETA). |
| [Frequent-place favourites](v220-frequent-place.gif) | Controlled stationary startup after five visits, name-free prompt, editable name prefilled from a Recent, then explicit Save and return to the map. |
| [Destination prediction](v220-destination-prediction.gif) | Controlled completed-journey model, a matched Favourite, green/blue map invitation in BT CTRL and normal driving, and explicit Yes/No confirmation. |
| [Intelligent diversions (2.2.0 reference)](v220-intelligent-diversions.gif) | Historical traffic warning, solid shared route, yellow bypass, dashed teal real-map alternative, a frosted-glass approximate saving badge beside the decision point, single-line green announcement and blue review hint, and animated BT CTRL → Back → OK → Yes confirmation. Traffic observations are controlled examples. Retained for reference alongside `v223-view-diversion.gif`. |
| [Day boot](boot-day.gif) / [Night boot](boot-night.gif) | Logo and wordmark on a soft theme-aware background, followed by the production half-second cross-fade into the live UI. The initial hold is illustrative, not a boot-time measurement. |
| [Popups](popups.gif) | Compact route confirmation, destination-source choices, saved-phone actions and the keyboard language picker share glass entrance, exit, text and focus transitions. |
| [Song and radio updates](metadata-updates.gif) | New song titles animate in, including during BT CTRL. Radio tuning updates even when the station name stays the same. |
| [Status notifications](status-notifications.gif) | GPS acquisition and recovery, phone connection, volume and no-active-route messages share one animated pill. |
| [Driving](driving.gif) | A new road name appears. A camera alert joins the same pill and remains after the name fades. |
| [Menus](menus.gif) | A glass panel opens over the map while the clock and current volume notice persist. Itinerary, Recents and Favourites, saved-phone actions, categorised Settings and return to driving use the production controller. |
| [Bluetooth](bluetooth.gif) | The volume notice expands to show controls and a thin track progress bar. A reported seek eases forward, and pause highlights when pressed. |
| [Phone destination](phone-destination.gif) | The real ClioPlay Send website beside the native head-unit UI: send, route progress and the received destination. No receive option is required; Bluetooth transport is simulated. |
| [Speeding warning](speed-warning.gif) | The speed pill pulses red above a confirmed 30 mph limit and eases back when the car slows down. |
| [Map browsing](map-browsing.gif) | Manual zoom is temporary; MAP restores Auto before entering Explore. Explore appears once on entry, stays silent during later pan/zoom, and returns to Auto with one notice. |
| [Roundabout and ETA](roundabout-eta.gif) | The turn pill shows the third exit. OK displays the estimated time and remaining distance. |
| [Coordinate route](coordinate-route.gif) | Choose Coordinates, enter latitude/longitude, press keypad Next, enter a name, review, calculate a real route, return to driving and find the destination in Recents. |
| [Map-point route](map-point-route.gif) | Enter Explore with MAP, pan to a point, press OK, confirm and calculate a real route back in the driving view. |
| [Coordinates](coordinates.gif) | Enter decimal latitude/longitude, use Next, name and review a destination, then confirm it into the route and Recents. |
| [Add a stop](add-stop.gif) | Trip chooses a favourite, reviews it, then inserts it before the original destination. |
| [Coordinate keypad (2.1.7)](v216-coordinates.gif) | Split sign/decimal keys, soft-key field toggle/delete, Next validation and optional naming. The existing filename is retained for compatibility. |
| [Trip layout](trips-layout.gif) | Compact Itinerary, Recents, Favourites, and destination-source review. |
| [Empty trip layout](trips-empty-layout.gif) | Open Coordinates from an empty itinerary and return using Back. |
| [Popup layout](popups-layout.gif) | Smaller confirmation cards, dynamic action menus and compact language picker; expanded name keyboard. |
| [Itinerary](v216-itinerary.gif) | Scroll the itinerary, wrap between Add stop and Cancel route, and dismiss the confirmation to keep the trip. |
| [Bluetooth hidden](v216-bluetooth-hidden.gif) | Able to use ClioPlay, even if hw_bluetooth=0 |
| [Hud Customisability](v216-hud-settings.gif) | Change top and bottom bar pill positions in Settings → HUD layout |

Native UI frames are 480 × 272 from the production C renderer. The phone-destination loop combines those unchanged frames with a captured browser panel on a 936 × 510 canvas. Website sources and capture tooling remain private. On regeneration, pass --phone-capture to supply new browser screenshots, or the existing GIF browser panel is reused while native UI frames are refreshed.
The boot previews use the production splash and cross-fade blend, with a cached
map position and simulated GPS acquisition. Their initial hold is illustrative;
loading time is not measured. Other previews show an example route around Newcastle upon Tyne. Car and phone inputs
are controlled samples. The destination preview feeds a sample message into
the ClioPlay daemon/UI handoff. Camera positions are illustrative. Motion is
sampled at 20 frames per second during short transitions, and
5 frames per second while calculating a route. GIF timing uses 50 ms animation samples and 200 ms calculation samples. The timings
describe controlled host captures, not achieved head-unit frame rates. The runtime
uses separate map and animation schedules, capped together during calculation;
the map targets 10 FPS. Steady HUD changes, fades, resizing and warning pulses
target 20 FPS.
Glass uses complete 8 × 8 area filtering plus a weighted blend of neighbouring
areas over a 24 × 24 footprint, with higher-precision colour calculations
and full-strength, screen-anchored 8 × 8 ordered dithering at the final RGB565
write. This reduces gradient banding with fine fixed grain; text, symbols and
exact selection colours retain their normal rendering. The pattern does not
cycle between frames. GIF palette reduction can obscure this detail compared
with the native RGB565 display. Unchanged-background samples are
cached during live overlay animations.
All popup windows use the same glass. Their entrances, exits, resizing, content
changes and selection highlights share the pills' independent 20 FPS transition
clock; map drawing remains on its normal schedule. The entire UI retains the
5 FPS cap during route calculation. Popup actions are handled immediately.
Explore camera passes require extra drawing work. Head-unit performance, Bluetooth and audio require a
physical head-unit trial.

The coordinate-route and map-point-route loops invoke the production remote
handlers and routing code against the existing local navigation bundle. The
requested target and nonempty computed route are checked before the ready-state
frames are captured. Their calculation display is held for a controlled 1.3-second
sampling window so its animation is visible; this is not a measurement of route
calculation time on either the host or head unit. Sample GPS remains stationary.

GIF encoding reserves the vehicle pointer’s Day/Night body and facet colours,
so its two-tone shading survives the shared 256-colour palette. Other map and
glass colours are still reduced for GIF delivery. This does not affect the
head-unit renderer.

## Regenerate

Build and copy the current executables as described in [BUILDING.md](../../BUILDING.md).
With Pillow and a host C/C++ compiler, run from `src/`:

```sh
# Regenerate all published GIFs
python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps

# Regenerate one or more individual GIFs (updates manifest.json in place)
python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps --only v223-view-diversion
python3 tools/regenerate_gifs.py --maps /path/to/clio843/maps --list

python3 tools/refresh_latest.py
```

Without maps, `python3 tools/regenerate_gifs.py` produces only the mapless
previews (also supporting `--only` and `--list`). Use the full map-backed
command above before publication; it refreshes all GIFs and the provenance
manifest.

The [manifest](manifest.json) records preview, source and executable hashes.
The map bundle is read in place. Captured PNGs, raw framebuffers and host
executables stay in a temporary directory.
