# ClioPlay UI previews

## Featured showcases

These 16 curated previews match the [main README](../../../README.md#ui-previews) and avoid redundant single-feature clips (features such as the secondary `Then` manoeuvre cue, pulsing red speed warning, Explore map browsing, offline Nearby POIs, status pill animations and AVRCP metadata already appear inside these fuller workflows). They are ordered around what makes a **100% offline** head unit unique:

| Preview | What it shows |
| --- | --- |
| [Multi-leg future routing & on-road turn chevrons](v300-multileg-and-chevron.gif) | Real **Manchester** 3-stop itinerary (`Northern Quarter` → `Shell Deansgate` → `Manchester Piccadilly` → `MediaCityUK`) solved offline by `v3.0` cooperative multi-leg CH routing: `Z8` driving approaching a real turn with the perspective-scaled white on-carriageway directional chevron (`Z7–Z9`) and Option B next-stop HUD pill, `Z5` overview showing active Leg 0 in magenta seamlessly continuing into future Legs 1–2 in soft lavender, and **Menu → Trip** displaying both **Next** and **Total** journey ETAs with per-stop arrival times. |
| [Zero-menu optical QR sync & built-in Connect QR](v300-qr-sync-and-connect.gif) | Real **Manchester** driving map showing `v3.0`'s optical QR workflow: **Bluetooth → Pair new phone** displays a built-in Connect QR that stays open on pairing (**Connected** + **Done** button) until `ClioPlay Link` sends a silent `CLP3:H` Ping to auto-close it; back on the live driving map, `CLP3:Q:S` pops up the frosted-glass floating map QR pill, steps through stacked `CS1` frames in real time on `CLP3:Q:A` progress dots, flashes **Synced** on `CLP3:Q:D`, and auto-dismisses cleanly back to the map. |
| [ClioPlay Link companion & bidirectional CLP3 sync](v300-clioplay-link.gif) | Retina captures of the redesigned **ClioPlay Link v3.0** companion web app beside ClioPlay's native `v3.0` UI on the real **Manchester** map: sending a 3-stop itinerary via `CLP3` (`Receiving trip` proportional capsule bar → atomic multi-leg magenta + lavender route without opening a receive screen), **Trip** showing **Next + Total** ETAs alongside **Places** symmetrical sync & Parked Car, and **Car** Driving Insights + live `CLP3` Car Settings (`Settings updated`). Bluetooth transport is simulated. |
| [Offline traffic diversions & Explore preview](v223-view-diversion.gif) | Real **Central London** (`Paddington` → `City of London`) journey demonstrating `v2.2.6.2`'s fewer-turns preference reducing the manoeuvre count from `27` to `24` for `+1.0 s` of clear-road driving time with zero U-turn spurs. Between the split and rejoin (`390 m` span), the active route takes `51 s` over `451 m`, while the diversion takes `63 s` over `535 m` in clear traffic; simulated local historical traffic makes the diversion save about `2.5` minutes. The driver inspects it in Explore, returns to guidance and accepts it. |
| [Learned usual routes & calibrated ETA ranges](v224-ml-eta-and-habits.gif) | Real clear-road **Central London** (`Westminster` → `Paddington` via Mayfair) spur-free route with zero traffic congestion, keeping the active route in its normal colour (no yellow) alongside a straight-ahead dashed teal parallel street (`Park Street` / `Bryanston Street`) bypassing the `Oxford Street` dogleg, showing a learned ETA reliability band, **Usual route available · OK to review**, **Use usual route?** (**Based on your usual route**, **View route**), and **Usual route selected** on acceptance. |
| [Offline slowdown & cold-weather hazard alerts](v224-ml-slowdowns.gif) | Real **Liverpool** (`Lime Street` → `Sefton Park`) spur-free route covering every repeat hard-braking scenario (with traffic diversions off and a solid-magenta route): warm `18 °C` **Frequent slowdown ahead** alert (`40–350 m` ahead), once-per-arc deduplication and automatic 4-second clear, silence at `18 °C` on a cold-only hotspot, **Cold-weather slowdown ahead** firing automatically when temperature drops to `1 °C` (`<= 3 °C`), and a winter-calibrated ETA check via **OK**. |
| [Usual parking, recurring stops & offline Nearby POIs](v224-ml-parking-and-pois.gif) | Real **Leeds** city-centre navigation demonstrating **Usual parking available** → **Route to usual parking?** (**Q-Park The Light**, tinting its mapped Parking POI badge in route colour instead of drawing a dot over it), recurring intermediate stop **Usual stop available** → **Add usual stop?** (**Coffee Stop**, marked with a route-coloured stop ring alongside the highlighted Parking POI), and **Trip → Nearby → Fuel** scanning offline POIs sorted by distance and ranking a preferred station first with a cyan star badge. |
| [On-device destination prediction](v220-destination-prediction.gif) | Controlled completed-journey model over **Manchester**, a matched Favourite, green/blue map invitation in BT CTRL and normal driving, and explicit Yes/No confirmation — predicting destinations purely from local history. |
| [Day/Night themes, learned brightness & adaptive Auto-zoom](v224-ml-adaptive-hud.gif) | Real **Edinburgh** (`Princes Street`) Day-theme driving map showing **Auto zoom** adopting a learned `+1` zoom preference in the `25–45 mph` band after consistent sustained manual overrides, stepping out at higher speeds, and an **Auto** Day→Night transition inside **Settings → Display** automatically updating **Brightness** from learned `90%` (Day) to `30%` (Night). |
| [Bluetooth A2DP phone audio & playback controls](bluetooth.gif) | The top status pill expands into `BT CTRL` with live track title/artist, playback controls and a thin track progress bar. A reported seek eases forward, pause highlights when pressed, and volume notices share the pill cleanly. |
| [Offline camera warnings, map badges & overspeed alerts](v227-camera-alerts.gif) | Real **Central London** (`A3211 Victoria Embankment` → `Blackfriars`) calculated street route showing the `v2.2.7` camera overhaul: matching `8×8`-supersampled on-map upcoming-camera badges directly on the carriageway, flush `43 px` frosted-glass camera card with a BT-CTRL-style 3 px capsule progress bar, synced pulsing red speed pill and overspeed rim with red map marker when exceeding a fixed camera limit, uncluttered centred red-light alert, and active average-speed zone with filling progress bar and twin-camera exit marker. |
| [Live expandable GPS telemetry HUD](v225-gps-panel.gif) | Real **Liverpool Waterfront** (`Liver Street` → `Lime Street`) driving map showing double-**OK** expanding the top time/temperature pill into the live non-blocking **GPS** HUD (`HH:MM:SS` seconds, recessed compass dial, heading, colour-coded `$GPGST` accuracy, signal bars/satellites, altitude and marine coordinates), staying active beside `BT CTRL`, switching smoothly to the compact `52 px` live strip above a centred confirmation popup, folding to a seconds-only clock pill in **Menu**, and returning smoothly on the driving map. |
| [Explore the map & route to any point offline](map-point-route.gif) | Enter top-down **Explore** with MAP over **Manchester**, pan the crosshair to any point on the offline map, press OK, confirm and calculate a real `v2.2.6.2` route back in the driving view. |
| [Offline coordinate entry & on-device routing](coordinate-route.gif) | Choose Coordinates, enter latitude/longitude (`53.47740, -2.23090`, **Manchester Piccadilly**), press keypad Next, enter a name (`PICCADILLY`), review, calculate a real `v2.2.6.2` route, return to driving and find the destination in Recents. |
| [Animated frosted-glass menus & phone management](menus.gif) | A dynamically-resizing glass panel opens over the **Manchester** map while the clock and current volume notice persist. Itinerary, Recents and Favourites, saved-phone Bluetooth actions, categorised Settings and return to driving use the production controller. |
| [Customisable HUD layout (LHD & RHD)](v216-hud-settings.gif) | Swap top and bottom bar pill positions live in **Settings → HUD layout** to suit left- or right-hand-drive dashboards. |

## Additional & legacy regression captures

<details>
<summary>Show 21 single-feature & historical regression captures (whose UI elements already appear in the featured showcases above)</summary>

| Preview | What it shows |
| --- | --- |
| [Expandable secondary manoeuvre cue](v223-secondary-manoeuvre.gif) | Real **Milton Keynes** (`Central Milton Keynes` → `Bletchley`) spur-free route focusing specifically on the **Then** secondary pill expanding (`43 px`) for chained roundabouts and returning compact (`24 px`) for normal turns. |
| [Offline Nearby POIs (Bristol)](v223-nearby-pois.gif) | Isolated view of the four-tab Trip header, Nearby categories (Fuel, Parking, Hospital) in **Bristol**, and adding a nearby fuel station (`Princes Service Station`) as a stop. |
| [Speeding warning](speed-warning.gif) | Isolated speed pill pulsing red above a confirmed 30 mph limit and easing back when speed drops. |
| [Map browsing](map-browsing.gif) | Isolated manual-zoom and Explore pan/zoom transition cycle over **Manchester**. |
| [Song and radio updates](metadata-updates.gif) | Track title updates inside `BT CTRL` followed by FM radio frequency tuning. |
| [Status notifications](status-notifications.gif) | GPS acquisition and recovery, phone connection, volume and no-active-route notices sharing one animated pill. |
| [Frequent-place favourites](v220-frequent-place.gif) | Controlled stationary startup in **Manchester** after five visits, name-free prompt, editable name keyboard prefilled from a Recent, and Save. |
| [Intelligent diversions (2.2.0 reference)](v220-intelligent-diversions.gif) | Earlier **Central London** (`Paddington` → `City of London`) diversion clip taking the diversion via `BT CTRL → Back → OK → Yes`. |
| [Day boot](boot-day.gif) / [Night boot](boot-night.gif) | Logo and wordmark on a soft theme-aware background, followed by the production half-second cross-fade into the live **Manchester** UI. |
| [Driving](driving.gif) | A new road name (`Deansgate`) appears on the **Manchester** baseline route before a camera alert takes over the centre pill. |
| [Popups](popups.gif) | Compact route confirmation, destination-source choices, saved-phone actions and the keyboard language picker sharing glass transitions. |
| [Roundabout and ETA](roundabout-eta.gif) | The turn pill shows the third exit while OK displays estimated time and remaining distance. |
| [Coordinates](coordinates.gif) | Static-stage walkthrough of entering decimal coordinates (`53.47740, -2.23090`), naming (`PICCADILLY`), reviewing and saving. |
| [Add a stop](add-stop.gif) | Choosing a Favourite from Trip, reviewing it, and inserting it before the original **Manchester** destination. |
| [Coordinate keypad (2.1.7)](v216-coordinates.gif) | Split sign/decimal keys, soft-key field toggle/delete, Next validation and optional naming (`53.47, -2.2`). |
| [Trip layout](trips-layout.gif) | Compact Itinerary, Recents (`Northern Quarter`, `Spinningfields`), Favourites, and destination-source review. |
| [Empty trip layout](trips-empty-layout.gif) | Open Coordinates from an empty itinerary and return using Back. |
| [Popup layout](popups-layout.gif) | Smaller confirmation cards, dynamic action menus and compact language picker; expanded name keyboard. |
| [Itinerary](v216-itinerary.gif) | Scroll the itinerary, wrap between Add stop and Cancel route, and dismiss the confirmation to keep the trip. |
| [Bluetooth hidden](v216-bluetooth-hidden.gif) | Using ClioPlay cleanly on head units without Bluetooth hardware (`hw_bluetooth=0`). |

</details>

Native UI frames are 480 × 272 from the production C renderer. The v300-clioplay-link loop combines those unchanged frames with a captured browser panel on a 936 × 510 canvas. On regeneration, pass --phone-capture to supply new browser screenshots, or the existing GIF browser panel is reused while native UI frames are refreshed.
The boot previews use the production splash and cross-fade blend, with a cached
map position and simulated GPS acquisition. Their initial hold is illustrative;
loading time is not measured. The general UI previews use a real **Manchester**
(`Deansgate` → `Salford Quays`, `-2` turns via `v2.2.6.2` fewer-turns routing,
zero U-turn spurs) baseline route, while feature-specific previews run across
real UK map regions in **Central London**, **Milton Keynes**, **Liverpool**,
**Leeds**, **Bristol**, **Edinburgh** and **Newcastle**. Car and phone inputs
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
