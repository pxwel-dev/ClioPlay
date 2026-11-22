# Version history

This history records implemented changes for each release.
Release dates are added when a version is published; older versions are not reconstructed from incomplete notes.

## Version 2.2.6 — Release

- **Unguided local ML learning & alerts** (full backwards compatibility with existing `mobility-ml.dat` `MML1` files — no active route required to learn everyday habits or receive repeat-slowdown warnings):
  - **Unguided Nearby-POI affinity:** finishing any unguided drive (or short chained stop) right at a mapped **Fuel** (`≤ 55 m`), **Parking** (`≤ 75 m`) or **Hospital** (`≤ 100 m`) POI now automatically learns preferred-POI affinity (deduplicated per session so explicit **Trip → Nearby** selections never double-count).
  - **Unguided Usual Parking learning & habitual car-park fallback:** arriving unguided at a mapped **Parking** POI (`≤ 75 m`) located `100–600 m` from a unique saved **Favourite** or **Recent** (skipping spots `< 100 m` from any saved place or ambiguous between multiple Favourites) automatically teaches that destination’s Usual Parking spot; in addition, any mapped **Parking** POI visited `≥ 3` times unguided — even with zero Favourites or Recents nearby — now acts as a Priority-2 Usual Parking fallback whenever you later route to a new destination `100–600 m` away from it (skipped if the target is itself a car park, a place with `≥ 2` direct arrivals `< 100 m`, or your current departure car park).
  - **Unguided Frequent & Cold-weather Slowdown alerts:** `traffic_service()` now runs a lightweight forward lookahead across learned hard-braking hotspots (`40–350 m` ahead, filtered by directed road-arc heading and forward bearing cone to ignore opposite carriageways and cross streets) so **Frequent slowdown ahead** and **Cold-weather slowdown ahead** warn you during unguided commutes as well as active guidance.
  - **Unguided-learned Usual Route proposals:** in addition to avoided arcs (`score ≤ -30`), **Usual route available** now also detects rejoin junctions where an alternate incoming road has strong unguided familiarity (`score ≥ +40`, driven 4+ times unguided) while the routed arrival road is unfamiliar (`score ≤ 0`), favouring familiar roads in the usual-route search and verifying `alt_pref ≥ old_pref + 40`.
  - **Auto-named unguided Recurring Intermediate Stops:** short `≤ 45 min` unguided chained stops now automatically resolve a label from matching saved **Favourites**, mapped Fuel/Parking/Hospital POIs, or **Recents** so **Add usual stop?** displays a real place name.
- **Aligned Adaptive Auto-zoom & sustained-override guard:**
  - Aligned Adaptive Auto-zoom’s three learned speed regimes with `Z9–Z4`’s hysteresis tiers (`< 25 mph` / `Z9–Z7`, `25–45 mph` / `Z6`, `≥ 45 mph` / `Z5–Z4`) and keyed the applied `±1` offset from the hysteresis-smoothed base `auto_zoom_level`, eliminating boundary oscillation at `20` and `40 mph`.
  - Required manual zoom overrides in Auto zoom to remain active for at least **20 seconds** before recording a session vote (resetting the timer on follow-up zoom taps and cancelling if restored via **MAP**, **Explore**, or undone back to the starting Auto-zoom level), preventing accidental misclicks or brief look-ahead peeks from biasing learned Auto-zoom preferences.

## Version 2.2.5 — Release

- Added an **expandable live GPS HUD inside the top clock/temperature pill** (building on `qspi`'s NMEA `$GPGST`/HDOP/altitude and compass dial contribution in PR #8), designed to stay genuinely usable while driving without blocking the map, car arrow, status notices or controls:
  - **Adaptive three-tier layout with persistent seconds:** toggling the GPS HUD on immediately adds live seconds (`HH:MM:SS`) to the clock pill (persisted in `prefs.ini` as `gpshud=1`) and expands smoothly downward (`164 × 148 px`) on the driving map to display a recessed antialiased compass dial with pivot-aligned `N/E/S/W` markers, centre pivot cap and theme-tuned Day/Night heading needle, bold heading (`074° NE`), colour-coded horizontal accuracy (`$GPGST` error estimate with HDOP × 5 m fallback, green/amber/red at `≤ 5 m` / `≤ 15 m` / `> 15 m`), 3-bar signal indicator with satellite count, and a recessed key–value tray for **Altitude**, **Lat** and **Lon** (marine degrees & decimal minutes, right-to-left mirrored in Arabic and Hebrew, Metric/Imperial units).
  - **Popup-aware & non-blocking:** stays fully expanded while driving and alongside active status notices (`ETA`, `VOL`, `BT CTRL`, slowdown alerts and `OK to review` invitations), automatically switches to a **Compact** live strip (`164 × 52 px`: header + mini live compass, heading, accuracy and signal-bar count) above popups so confirmation and modal dialogs remain vertically centred, and folds cleanly to **Minimised** (`27 px`, keeping seconds visible) while browsing **Menus** or **Explore** — then smoothly glides back open on returning to the driving map. Mirrors cleanly with **Clock left** (`hudtopswap`).
  - **Direct toggle controls:** kept **Menu** compact (`Trip`, `Bluetooth`, `Settings`); toggle the live GPS HUD on/off from the driving map by pressing **Centre / OK** (or pushing the console knob) **twice within 4 seconds** (the first press still shows road name/ETA, and existing `OK to review`, Explore and `BT CTRL` priorities stay intact) or with one press of the physical console **REPEAT** button (`btn7`, default `GPS`, remappable via `cswmap.ini`).
  - Added full translations across all 18 languages, native regression tests, and a real-map demo preview (`src/docs/demo/v225-gps-panel.gif`).
- Expanded **Auto zoom** from Z6–Z4 to **Z9–Z4** (and allowed manual driving zoom and persisted `manualzoom` up to Z9 on standard Z6 map packs), widening the Adaptive Auto-zoom clamp to Z4–Z9. Default speed bands and hysteresis thresholds (`mph` / approx. `km/h`):
  - **Z9:** `< 5 mph` (`< 8 km/h`), zooms out to Z8 at `≥ 6 mph` (`10 km/h`).
  - **Z8:** `5 to < 12 mph` (`8–19 km/h`), zooms out to Z7 at `≥ 14 mph` (`23 km/h`), zooms in to Z9 at `≤ 4 mph` (`6 km/h`).
  - **Z7:** `12 to < 25 mph` (`19–40 km/h`), zooms out to Z6 at `≥ 27 mph` (`43 km/h`), zooms in to Z8 at `≤ 10 mph` (`16 km/h`).
  - **Z6:** `25 to < 45 mph` (`40–72 km/h`), zooms out to Z5 at `≥ 48 mph` (`77 km/h`), zooms in to Z7 at `≤ 23 mph` (`37 km/h`).
  - **Z5:** `45 to < 65 mph` (`72–105 km/h`), zooms out to Z4 at `≥ 68 mph` (`109 km/h`), zooms in to Z6 at `≤ 42 mph` (`68 km/h`).
  - **Z4:** `≥ 65 mph` (`≥ 105 km/h`), zooms in to Z5 at `≤ 62 mph` (`100 km/h`).
- Preserved full sub-unit vertex precision at cropped zooms **Z7–Z9** (retaining all four fractional bits of Z6’s 12-bit road vertices) and placed street names beside visible screen-clipped road segments where there is room, avoiding the car marker, HUD cards, clipped route geometry and temporary bottom street-name pill (refreshing once on handoff) and deduplicating full-length road names. Works with existing map packs.
- Tightened **View diversion** / **View route** Explore framing to use the full `480 × 272` viewport (`448 × 240` fit box up to Z9, visually centred vertically) instead of dropping out prematurely.
- Added `--only` / `--gif` and `--list` support to `tools/regenerate_gifs.py`, `tools/render_previews.py` and `tools/render_mapless_gifs.py` so individual demo GIFs can be re-rendered in seconds (merging cleanly into `docs/demo/manifest.json`), and regenerated all demo GIFs with Auto zoom enabled at road-matched driving speeds (or `0 mph` / Z9 for stationary workflows).

## Version 2.2.4 — Release

- Added **expanded local on-device machine learning** in a fixed-capacity, integer-only companion model (`clio843/mobility-ml.dat`, `MML1`, ~26 KiB, FNV-1a checksummed atomic saves, strictly opt-in under **Learn driving history** and wiped by **Clear driving history**, with zero cloud or telemetry):
  - **Personal road-class & cold-weather ETA calibration:** learns per-road-class and roundabout travel-time multipliers with Bayesian shrinkage to calibrate unfamiliar roads, plus separate cold-weather profiles whenever vehicle exterior temperature is `<= 3 °C`.
  - **Dwell-time, multi-leg trip chains & recurring intermediate stops:** tracks `(previous origin, current origin, dwell duration) -> destination` chains across reboots to improve contextual destination predictions, and detects recurring intermediate stops (from short `<= 45 min` chained stops or explicitly added stops) to offer **Usual stop available · OK to review** → **Add usual stop?** confirmation.
  - **Preferred-route ("Usual route") recommendations:** learns regularly driven arcs and voluntarily avoided arcs on forward off-route forks (ignoring U-turns, active traffic warnings and already-known congested arcs so temporary traffic detours never pollute permanent route preferences). Solidifies both visual and textual distinctions from historical traffic diversions: traffic offers highlight the congested bypassed section in yellow and display **Traffic diversion available** → **Take traffic diversion?** (**Likely traffic ahead**, **View diversion**, **Diversion selected** / **Diversion unavailable**, Itinerary **Diversion**), while usual-route proposals keep the active route in its normal route colour (no yellow) alongside the dashed teal usual road and display **Usual route available** → **Use usual route?** (**Based on your usual route**, **View route**, **Usual route selected** / **Route unavailable**, Itinerary **Usual route**).
  - **ETA reliability / uncertainty bands:** maintains online integer Mean Absolute Deviation (MAD) per directed road and 2-hour time bucket, displaying realistic **`~X-Y min`** ETA ranges in the HUD and Itinerary when route spread is at least 3 minutes and preferring more consistent diversion candidates.
  - **Adaptive Auto-zoom & Day/Night brightness personalisation:** gently learns per-speed-band Auto-zoom preferences (`±1` zoom level after consistent overrides, clamped to Z4–Z6) and remembers separate Day and Night brightness levels applied automatically on **Auto** theme transitions.
  - **Learned Nearby-POI affinity & Usual Parking spot:** boosts regularly chosen Fuel, Parking and Hospital POIs in **Trip → Nearby** with a cyan star indicator, and offers **Usual parking available · OK to review** → **Route to usual parking?** confirmation when routing to a destination where you repeatedly park 100–600 m away.
  - **Repeat hard-braking / unmapped slowdown alerts:** detects recurring `>= 9.0 mph/s` deceleration hotspots across sessions (and separately in cold `<= 3 °C` weather), surfacing a subtle amber **Frequent slowdown ahead** or **Cold-weather slowdown ahead** cue once per road per trip and penalising bumpy cut-throughs in diversion ranking.
- Added **route-coloured stop/destination endpoint markers and POI highlighting** on the map (`draw_trip_markers()`, `poi_trip_prepare()`, `poi_icon_tinted()`): every active and upcoming stop renders a route-coloured ring matching the diversion junction dot aesthetic, and the final destination renders a route-coloured bullseye ring. When a stop or destination targets a visible map POI (Fuel, Parking or Hospital), the POI badge itself is highlighted in the route colour (`C_ROUTE`) instead of drawing an overlapping dot over the icon.
- Made the **secondary ("Then …") manoeuvre pill expandable for roundabouts**: `HC_TURN_NEXT` stays compact (`24 px`, 17 × 17 icon) for normal turns and smoothly expands upward (`43 px`, locked bottom edge via symmetric `hud_lerp()` rounding) when the follow-up manoeuvre is a roundabout so the full 35 × 35 roundabout diagram with centred exit number and **Take exit N** detail is clearly legible.
- Added the desktop offline **`tools/replay_history.py`** CLI (Python stdlib only) to replay `history.prev.dat` / `history.dat` against a single `route.cpr` or multi-region `atlas.cpa` map rebuild, run pure-Python DBSCAN endpoint denoising, preserve UI preferences (`zoom`, `brightness`, `POI affinity`, `parking`), and emit verified `mobility.dat` (`MOB1`) and `mobility-ml.dat` (`MML1`) files without printing raw coordinates.
- Added translations for all new local ML, traffic-diversion and usual-route prompts across all 18 interface languages, widened `notice_text` to prevent multi-byte UTF-8 truncation, added comprehensive UBSan host tests and Python↔C binary parity tests (`tests/test_replay_history.py`), and added/updated real-map demo GIFs (`v224-ml-eta-and-habits.gif` on a clear-road Durham route, `v224-ml-slowdowns.gif` on a York route, `v224-ml-parking-and-pois.gif`, `v224-ml-adaptive-hud.gif`, `v223-secondary-manoeuvre.gif`, `v223-view-diversion.gif`, `v220-intelligent-diversions.gif`).

## Version 2.2.3 — Release

- Added **Offline Nearby POIs** as a fourth Trip tab (**Itinerary → Recents → Favourites → Nearby**) and as a source inside **Add stop** and **Change destination**. Scans installed map tiles completely offline (two-pass Z5 3 × 3 inner tiles plus expanding Z3 rings) for **Fuel**, **Parking** and **Hospital** POIs, sorted by distance with 16 × 16 antialiased category badges and route, add-stop and save-to-favourites actions.
- Added a **secondary ("Then …") manoeuvre cue** (`HC_TURN_NEXT`): when a follow-up turn or roundabout occurs within 250 m of the next manoeuvre, a compact frosted-glass pill with a precomputed 17 × 17 antialiased icon docks above the Turn card and smoothly promotes as you complete the current manoeuvre.
- Added a **View diversion** third button to the **Take diversion?** confirmation popup. Selecting it closes the popup and opens north-up Explore framed around the proposed diversion split, bypass and rejoin points for inspection; pressing **MAP** returns to the driving view and re-opens the same confirmation popup (or reports **Diversion unavailable** if the offer expired).
- Added **AVRCP 1.3 `RegisterNotification`** support (`EVENT_PLAYBACK_STATUS_CHANGED` and `EVENT_TRACK_CHANGED`) in `clioa2dp` for immediate track, playback-status and ClioPlay Send destination updates, with automatic fallback to periodic polling for phones that reject notifications.
- Deferred regional `atlas.cpa` graph validation until routing actually opens an atlas (`atlas_validate_graph()`), avoiding eager cold-boot scans of large multi-region atlases while preserving full structural validation before routing.
- Fixed scrollable Itinerary layout and connector lines: itineraries that exceed one page now display whole, vertically centred stops and draw vertical connector lines strictly between consecutive visible circles on the same page, preventing a stray vertical line from appearing above the top stop while scrolling.
- Added Debug-only `RSS_KB` and `FREE_KB` memory telemetry to five-second `FRAME 5S` summaries in `clio843/state.log`.
- Added the desktop `tools/favourites_tool.py` CLI to list, export and import `faves.dat` and `recent.dat` (`CLP2`) in CSV, JSON and GPX formats with atomic writes and deduplication.
- Added translations for all new interface labels across all 18 languages, unified the freestanding ARM runtime header (`arm_runtime.h`), cleaned up stale AVRCP comments, and expanded host regression tests and real-map Newcastle demo GIFs.

## Version 2.2.2 — Release (Major Performance Optimisation)

- Increased the Bluetooth PCM prefill from 150 to 200 ms for more playback headroom under UI load.
- Reused the finished map while parked and updated only changed framebuffer rows when the driver supports partial writes.
- Prepared Explore rotation backgrounds and parked menu, popup and pill transitions before playback. Prepared frames play in order when a refresh runs late. Routine Bluetooth and clock updates wait for a prepared transition to finish.
- Raised live and buffered animations and steady HUD updates to a 20 FPS target. Moving maps still target 10 FPS and route calculation caps the entire display at 5 FPS.
- Kept the current menu page visible as taller status notices resize it. Scrollable rows move and fade at the edges as the available height changes.
- Made Itinerary's footer a single row. Use Left/Right to wrap between its available buttons, including Cancel route and Diversion.
- Added wraparound on menu lists, dialogs, and keyboard controls. Trip tabs use Left/Right; the soft keys retain their zoom and text-entry actions.
- Removed the right-soft-key Bluetooth pairing shortcut; start pairing from Pair new phone at the top of the Bluetooth list.
- Replaced the heading notice with on-demand road names. Press Centre / OK to show the current road for about four seconds. During guidance, the ETA and remaining distance appear too. Console INFO and REPEAT offer the same information.
- Centred the Latitude and Longitude labels and values in the manual coordinate entry fields.

## Version 2.2.1 — Release

- The guided installer and navigation builder accept multiple matching OSM extracts for one cross-border build. The installer merges them locally and rejects conflicting versions of shared objects.
- Bluetooth SBC decoding uses less computation while preserving the same PCM output and playback configuration.

## Version 2.2.0 — Release

### Changes in this version

- Introduced optional, offline driving-history learning. Fresh GPS observations are recorded independently of active navigation and only successful Barcelona power-off sessions train the model.
- Added contextual suggested trips in the map status pill, with provisional startup GPS reconciliation, adaptive day/time and recency scoring, ordered early-trajectory evidence, confidence hysteresis and offline destination naming. OK opens an explicit Yes/No confirmation; BT CTRL retains playback priority and displays BACK to review. No suppresses that destination for the current recording session. The empty Itinerary retains the original destination-entry layout.
- Added an optional stationary-startup frequent-place favourite prompt after five completed visits across three dates within 90 days. It never repeats for the same place unless history is cleared; Yes opens the existing Favourite keyboard with a suggested name when available; only Save adds the favourite, without starting navigation or changing prediction confidence.
- Added directional historical road-time profiles, traffic-adjusted ETAs and an explicit local Diversion choice with gold/yellow bypassed geometry and an aqua/teal alternative. Existing map formats and static CH/QAD7 shortcut weights remain unchanged.
- Shared route geometry remains magenta; dashed teal alternatives, yellow bypasses and junction markers distinguish proposed changes. A small centred frosted-glass map badge shows the approximate saving near the decision point; the glass HUD announces the offer with context-sensitive Back/OK instructions; direct map confirmation preserves Bluetooth controls and driving zoom.
- Diversion invitations use a persistent single line with the established success green announcement and blue context-sensitive review instruction, until acceptance, rejection or expiry.
- Declining a diversion suppresses only its bypassed section; later independent offers remain available, and diversion confirmation initially selects Yes.
- Organised Settings into Display, HUD layout, Navigation, Driving history and System subpages; preferences and their stored values are unchanged.
- Added history, destination suggestion and diversion controls in Settings, including a confirmed clear-history action. Private data is excluded from the publication.
- Fixed Bluetooth phone names and connected-state identity when switching between saved devices; both remain keyed by hardware address.
- Completed the new navigation and HUD translations across all 18 interface languages, and added missing-key regression checks.
- Added native regression cases and two controlled preview storyboards. Historical traffic is not live traffic; a physical head-unit trial remains necessary.
- Added a software-only update mode with verified staged copies, a dated software backup and rollback; existing maps, settings and personal history remain untouched.

## Version 2.1.8 — Release

### Changes in this version

- Fixed itinerary connector lines extending towards the Add stop action when additional targets are off-screen. Connections now join visible markers only.
- Increased trip capacity from five to ten targets in total (nine intermediate stops plus the final destination). Saved trips use CTR4 only; unfinished trips from older formats cannot be resumed.
- Removed obsolete saved-place, map, routing and bundle-format readers. Current CLP2 places and current navigation packs remain supported; unsupported formats require fresh data.
- Reduced the size of confirmation dialogs and their action menus while retaining readable labels, existing choices, focus targets and animation behaviour.

## Version 2.1.7 — Release

### Changes in this version

- Restored **BACK** to return to the previous page without advancing or applying a coordinate draft.
- Added a full-width **Next** key at the bottom right of the coordinate keypad. The **±** and **.** keys share the bottom-left column; **0** remains in the middle.
- **Next** validates both fields and opens optional naming; invalid entries stay editable. Soft-right still cycles latitude/longitude and soft-left deletes.
- Updated directional/rotary navigation, regression checks and demo capture scripts.

## Version 2.1.6 — Release

### Changes in this version

1) Simplify coordinate page. Only keypad. Soft right will cycle between lat/long and wrap back around to the other side, while soft left is delete.
2) Vertically recentre the trip symbols (circle & squares) within the selection window
3) Add further customisability in settings:
   - Status pill on left / Time & temp pill on right (DEFAULT VALUE), Status pill on right / Time & temp pill on left
   - Speed pill in left / Direction pill on right (DEFAULT VALUE), Speed pill on right / Direction pill on left
4) If hw_bluetooth = 0, hide all bluetooth-related functionality in the UI.

## Version 2.1.5 — Release

### Changes in this version

- Resized the trips pages to fit in-between the top and bottom pills. Only the keyboard expands to allow more room for entering destination names.

## 2.1.0 — Release

###  Changes in this version

- Replaced full-screen menus with animated frosted-glass panels on the map.
  Menu, Bluetooth and Settings retain the lower driving pills when space allows;
  Trip and entry pages expand for more room. The clock and live status notices
  retain their position and lifetime across pages.
- Rebuilt Trip around Itinerary, Recents and Favourites tabs, opening Itinerary
  first. Each tab remembers its selection; saved-place lists can show seven rows.
  Add stop and Change destination share Recents/Favourites/Coordinates sources
  and a review step before applying changes. Stop actions preserve the route's
  destination and use the existing persisted trip model.
- Reworked Settings into centred label/value groups with adjustment chevrons,
  a language chooser and no brightness slider. The version footer is retained.
- Pinned Pair new phone above saved devices, with a right-soft-key shortcut.
  Device actions follow the Bluetooth address if the list changes asynchronously.
- Rebuilt naming keyboards for all 18 languages with native-script keys,
  QWERTY/AZERTY/QWERTZ ordering where appropriate, separate symbol modes, denser
  character pages and wrapping directional navigation. Japanese separates kana
  scripts; Chinese offers common Hanzi plus a syllable/character browser; Korean
  retains Hangul composition. Accented keys align on the base character.
- Kept the map at its normal 10 FPS target under menus, with 15 FPS panel/focus
  transitions and the existing whole-UI 5 FPS route-calculation cap. Text bounds
  are cached from glyph masks rather than scanning a temporary framebuffer.

- Made the exceeded-speed-limit outline continuous and equally thick along its
  straight edges and rounded corners, preserving its smooth red fade and pulse.
- Thickened the inset speed-limit badge outline while retaining clearance from
  the surrounding speed pill in both themes and for every limit type.
- Removed the explanatory tip line from latitude/longitude entry. Input
  validation and error messages remain available.

### Added

- A current-version summary in the README and this maintained version history.
- Updated native GIF previews and remote/physical-console workflow checks for
  the new menu, keyboard and itinerary presentation. Host checks and previews do
  not establish physical head-unit performance or compatibility.

## 2.0.0 — Baseline

The baseline for this history includes the major UI redesign: full-screen maps,
animated frosted-glass pills and popups, updated roads/POIs/vehicle marker,
remote and physical-console controls, coordinate destinations, and day/night
boot artwork with a cross-fade into the UI. Detailed behavior and validation
limits are documented in the current user and build guides.
