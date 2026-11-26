# Version history

This history records implemented changes for each release.
Release dates are added when a version is published; older versions are not reconstructed from incomplete notes.

## Version 2.2.7 - Camera system overhaul

Software-only camera warning overhaul — **100% compatible with existing `2.1.8–2.2.6.4` maps (`CPM5`, `CPR2 v5`, `QAD7`, `CPC1`)** and saved user data (no map rebuild required), preserving the strict fail-closed 19-country legal policy across map markers, HUD and audio:

- **Matching supersampled icons for the upcoming camera only (`src/tools/build_camera_icons.py` → `camera_icons.h`):**
  - Added three bespoke `8×8`-supersampled vector silhouettes (**Fixed speed camera**, **Red-light traffic signal** with accented red top lamp, and **Twin-camera average-speed check**) in matched `18×18` on-map badge and `14×14` HUD mini-icon sizes.
  - `draw_camera_marker()` renders **only** the active upcoming camera from RAM (zero per-frame SD-card reads and zero map clutter), projected via `map_project_q8()` so it honours 3D perspective tilt, `Z6–Z9` visual road snap and Explore panning, showing the entry gantry on approach and switching to the exit gantry once inside an average-speed zone. Rim and icon shift cleanly from cyan to warning red when exceeding a speed limit.
- **Redesigned flush `43 px` frosted-glass camera HUD pill (`hud.h`, `frame_renderer.h`):**
  - Aligned top-and-bottom with the Speed and Turn pills (`43 px` tall, compact `124–200 px` width) and cleanly dedicated to the active camera (suppressing automatic temporary street-name flashes while a camera warning is visible so the limit and progress bar stay uninterrupted; normal `29 px` road-name pills resume once the camera clears).
  - Clean typography: matching `14×14` twin icon, concise camera label (or bold `Avg XX mph/km/h` in-zone), right-aligned distance (`km`/`mi` above 1 km / 0.5 mi, `5 m`/`10 yd` close-in), subtle `Limit XX mph/km/h` subtitle (omitted cleanly on red-light / no-limit cameras with vertical centring), and full RTL mirroring for Arabic and Hebrew.
  - Added a smooth BT-CTRL-style 3 px antialiased capsule progress bar (`hud_playback_strip()`) along the bottom edge that drains smoothly as you approach a camera and fills from `0% → 100%` across an active average-speed zone, turning amber when over the limit.
  - Added a synced eased frosted-glass red rim pulse (`hud_camera_red()`) on `HC_ROAD` matching the Speed pill whenever exceeding the camera limit.
- **Detection & reliability fixes (`global_cameras.h`):**
  - **Speed-scaled warning & chime horizons** with `+60 m` hysteresis (`250–1000 m` routed / `250–800 m` unguided visual, `180–600 m` audio — ~14–22 s lookahead instead of fixed distances that warned two minutes early in town).
  - **Curved-road unguided corridor:** stays tight (`±18` units / ~20 m) close up to reject adjacent parallel streets while flaring gently at distance (`up to ±48` units) to catch sweeping motorway and A-road bends earlier.
  - **High-speed average-speed entry & abandoned-zone auto-clear:** segment-crossing check catches 70 mph 1 Hz GPS steps across an entry gantry without fabricating mid-zone averages; remaining zone distance now follows the active route when guided and auto-clears after 15 s of sustained divergence (`> +120 m`, e.g. taking a slip road mid-zone).
  - **Exact `mph10` overspeed parity** between the Speed pill and Camera pill, plus immediate wakeup on fresh GPS fixes.
- **Two-tier audio cues & new `Overspeed audio` mode (`audio_chime.h`, `clioa2dp.c`):**
  - Wired `/tmp/clalert` `kind=1` (existing calm 2-tone `880 → 1174 Hz` approach cue) and `kind=2` (distinct brisk 3-tone `988 → 1244 → 988 Hz` overspeed cue, with per-camera cooldowns and 30 s average-zone cooldown).
  - Added **`Overspeed audio`** (`pref.cameras=3`, alongside `Off`, `Visual`, `Phone audio`) in **Settings → Navigation → Camera alerts**: stays quiet on compliant speed/average-speed approaches, plays the urgent overspeed cue only when exceeding the limit, and plays only the soft approach cue for red-light cameras.
- **Translations, tests & preview:**
  - Added `"Limit "` and `"Overspeed audio"` across all 18 interface languages, expanded UBSan host tests (`global-cameras.c`, `audio-chime.c`, `ui-labels-alerts.c`, `hud-customization.c`), and added the real-map Central London preview (`src/docs/demo/v227-camera-alerts.gif`).

## Version 2.2.6.4 - Bug-fix release

Software-only physical console input bug-fix release, **100% compatible with existing `2.1.8–2.2.6.3` maps (`CPM5`, `CPR2 v5`, `QAD7`)** and saved user data (no map rebuild required):

- **Fixed Scénic III console knob pushes being ignored until rotation (#18, contributed and vehicle-tested by `@qspi`):**
  - On Scénic III hardware, the factory `vehicledaemon` sends a single `pressed=1` record per knob push and no release (`pressed=0`) record (confirmed from on-car `csw.log` captures). Previously waiting for a `0 -> 1` edge swallowed subsequent knob pushes (including Menu selection and the second press of double-OK for the GPS HUD) until a joystick or rotation record happened to clear `csw.pressed`.
  - `csw_record()` now treats each `pressed=1` record as a click guarded by a sliding 250 ms burst window (`CSW_PUSH_GUARD_MS`, matching the hard buttons) so duplicate or held push records still produce one OK, while an explicit `pressed=0` record continues to re-arm immediately.
  - Updated `src/docs/scenic-iii-hardware.md` and expanded host UBSan regression coverage in `src/tests/vehicle-input.c` (burst suppression, consecutive push sequences without release, immediate release re-arming, and `push -> MENU -> push`).

## Version 2.2.6.3 - Installer & map-builder update

Desktop installer, map-builder and release-integrity maintenance release (head-unit runtime behaviour is unchanged from `2.2.6.2`; 100% compatible with existing `2.1.8–2.2.6.2` maps and saved user data):

- **Fixed FAT32/exFAT SD-card install and update failures (#12, #14, contributed by `@bobczan`):**
  - Made POSIX executable `chmod` best-effort in both `install_nav.py` and `update_card.py` (ignoring `EPERM`, `EACCES`, `ENOTSUP`, `EOPNOTSUPP`, `EINVAL`, `NotImplementedError`) and switched persisted-data backups (`prefs.ini`, `recent.dat`, `faves.dat`, `trip.dat`) from `shutil.copy2` to `shutil.copyfile`, preventing aborts and rollbacks on FAT32/exFAT cards mounted through WSL 2 `drvfs` or Linux `vfat`.
- **Tightened Windows WSL launcher preflight (#13, #15, contributed by `@bobczan`):**
  - `src/tools/install_windows.ps1` now verifies `import venv, ensurepip` silently instead of `python3 -m venv --help`, catching missing `python3-venv` packages up front on fresh Ubuntu 24.04 WSL setups.
- **Opt-in parallel atlas builder & cached C++ helpers (#11, contributed by `@bobczan` and unified into `build_atlas.py`):**
  - Setting `CLIOPLAY_PIPELINE=1` enables bounded concurrent extraction and region-build worker pools directly inside `tools/build_atlas.py` (sized from host RAM and physical cores on Linux, WSL 2, ARM Linux and macOS, overridable via `CLIOPLAY_EXTRACT_JOBS` and `CLIOPLAY_BUILD_JOBS`).
  - Uses deterministic hierarchical split naming (`rNNNN_k`) and tree-order catalog assembly so sequential and pipelined runs stay byte-identical and cross-resumable even when dense regions split, with accurate progress/ETA, fail-fast subprocess cancellation on errors/`Ctrl-C`, deadlock-free RAM throttling, and atomic `atlas-region.json` stamps.
  - Added shared `tools/native_helper.py` so `ch_preprocess` and `build_qadj` compile once per source/platform/architecture/compiler into a private `0700` per-user cache (with automatic cleaned fallback for `noexec` `/tmp`), speeding up both sequential and pipelined builds.
- **Release integrity & CI (#16, reported by `@bobczan`):**
  - Added `.github/workflows/verify.yml` to check `SHA256SUMS` and `verify_ci.py` on pushes and tags (`--host-only` on PRs), plus new unit tests (`src/tests/test_atlas_pipeline.py` and FAT32 regression tests in `test_clean_install.py` and `test_update_card.py`).

## Version 2.2.6.2 - Bug-fix release

Software-only routing and close-up map bug-fix release, 100% compatible with existing `2.1.8–2.2.6.1` maps (`CPM5`, `CPR2 v5`, `QAD7`) and saved user data, no map rebuild required.

- **Eliminated unnecessary U-turn spurs at route starts, destinations and junctions:**
  - **Two-way start & destination direction selection:** Most endpoint U-turn spurs came from snapping onto one arbitrary direction of a two-way street. ClioPlay now considers both legal directions on two-way streets at the start and destination right inside the existing router (keeping one-way streets and dual carriageways strictly directional), fixing wrong-way departures and overshoot-and-U-turn arrivals at the source without rebuilding map files. While moving (`≥ 5 mph`), a gentle forward preference avoids reversing direction unless continuing ahead is substantially worse, and never inflates the displayed ETA.
  - **Bounded post-build U-turn & spur cleanup:** Remaining turn-restricted out-and-back spurs, sharp `160°–200°` reversals, and divided-carriageway V-pinch hairpins (separate one-way carriageways reversing across a junction) are replaced with a bounded local search that honours all turn restrictions, blocks self-intersecting loops, and can approach a two-way destination from the opposite end when a no-turn restriction sits right before arrival. Strict time and distance limits (`≤ +90 s / +1.5 km` for spurs, `≤ +60 s / +900 m` for junction U-turns, `≤ 1.2 s` total cleanup budget per leg) keep genuinely necessary U-turns intact.
  - **Moving-start stability:** Starting or recalculating a route while driving now lets the cleanup pass finish cleanly before trimming the moved prefix.
- **Prefer simpler routes with fewer turns:**
  - Pure shortest-time routing often cuts through multi-turn side streets just to save a few seconds. ClioPlay now scans clusters of closely spaced turns (`≥ 3`-turn zigzags prioritised ahead of `2`-turn pairs) and switches to a cleaner legal corridor whenever it removes turns within strict caps (`≤ +20%` travel time, `≤ 30 s` total per regional leg, `≤ +25%` / `300 m` distance per section). Roundabouts count as one manoeuvre, and the displayed ETA always reflects true driving time.
- **Fixed close-up road offset staying frozen after parking off-road (`Z6–Z9`):**
  - Pulling off-road into a car park or driveway and stopping could previously leave the `Z6–Z9` visual road snap locked onto the nearby street. The off-road grace period now expires even while stationary (`< 0.5 mph`), smoothly easing the map and vehicle arrow back to the GPS position with zero extra tile reads before halting idle redraws.
- **Tests & refreshed UK previews:**
  - Expanded host UBSan regression tests in `src/tests/map-car-snap.c`, `src/tests/mobility.c` and `src/tests/atlas-qad7.c` (covering two-way endpoint selection, moving-heading bias, detour caps, loop blocking, divided-carriageway V-pinches, multi-turn clusters and parked snap release).
  - Re-rendered all 34 published UI and routing previews across fresh UK routes and cities (Manchester, Central London, Milton Keynes, Bristol, Liverpool, Leeds, Edinburgh and Newcastle).

## Version 2.2.6.1 - Bug-fix release

- **Fixed close-up road alignment at cropped zooms (Z6–Z9):** normal consumer GPS scatter (3–10 m) magnified at `Z7–Z9` (`2×`–`8×` digital crop of Z6 tiles) could previously make the vehicle icon appear off the road beside thin road strokes and bend the start of the active magenta route sideways. ClioPlay now performs lightweight, display-only **Z6-reference visual road matching** across `Z6–Z9` (`Z4–Z5` remain untouched):
  - **Camera, marker and route origin move together in `1/16`-world-unit (Q4) space:** matches against exact sub-unit road vertices from the existing Z6 map data through the normal tile cache. A cache miss can read a tile from the SD card. The visual driving camera, vehicle marker and active route origin shift together so the car arrow stays at `(240, 211)`, clear of the bottom HUD cards, and the magenta route line starts beneath the arrow.
  - **Heading, active-route, road-class and hysteresis filtering:** penalises perpendicular cross streets using continuous `sin²θ` heading alignment (both while moving and while stopped at traffic lights via the latched driving heading), prefers Z6 roads aligned with the active route corridor (`≤ 45°`), applies a service-road (`cls 7`) penalty so nearby driveways/aisles do not pull highway or residential driving off-road, retains interior carriageway lock hysteresis on dual carriageways while releasing immediately at polyline endpoints for hitch-free Z6 tile-seam handoffs, and eases smoothly per frame with off-road debounce.
  - **Explore parity & untouched core navigation:** raw GPS, `car_lat()/car_lon()`, routing, off-route detection, road context, speed limits, speed cameras and ML history remain completely untouched; entering **Explore** starts on the visually snapped road position and preserves the snap offset across buffered orientation transitions.
  - Added host UBSan regression coverage in `src/tests/map-car-snap.c`, wired into both `src/tools/verify_ci.py` and `src/tools/verify.py`.

## Version 2.2.6 - Release

- **Unguided local ML learning & alerts** (full backwards compatibility with existing `mobility-ml.dat` `MML1` files - no active route required to learn everyday habits or receive repeat-slowdown warnings):
  - **Unguided Nearby-POI affinity:** finishing any unguided drive (or short chained stop) right at a mapped **Fuel** (`≤ 55 m`), **Parking** (`≤ 75 m`) or **Hospital** (`≤ 100 m`) POI now automatically learns preferred-POI affinity (deduplicated per session so explicit **Trip → Nearby** selections never double-count).
  - **Unguided Usual Parking learning & habitual car-park fallback:** arriving unguided at a mapped **Parking** POI (`≤ 75 m`) located `100–600 m` from a unique saved **Favourite** or **Recent** (skipping spots `< 100 m` from any saved place or ambiguous between multiple Favourites) automatically teaches that destination’s Usual Parking spot; in addition, any mapped **Parking** POI visited `≥ 3` times unguided - even with zero Favourites or Recents nearby - now acts as a Priority-2 Usual Parking fallback whenever you later route to a new destination `100–600 m` away from it (skipped if the target is itself a car park, a place with `≥ 2` direct arrivals `< 100 m`, or your current departure car park).
  - **Unguided Frequent & Cold-weather Slowdown alerts with dynamic decay:** `traffic_service()` now runs a lightweight forward lookahead across learned hard-braking hotspots (`40–350 m` ahead, filtered by directed road-arc heading and forward bearing cone to ignore opposite carriageways and cross streets) so **Frequent slowdown ahead** and **Cold-weather slowdown ahead** warn you during unguided commutes as well as active guidance. Hazard counts are now capped (`6` warm / `5` cold), decay smoothly by `-1` on non-braking passes once `≥ 4` journeys have elapsed (with cold counts decaying only at `≤ 3 °C` so summer drives never wipe winter ice spots), reclaim empty slots, and expire automatically when unvisited for `> 256` journeys (warm) or `> 512` journeys (cold) - mirrored in `tools/replay_history.py`.
  - **Weekday vs. weekend destination weighting:** `mob_journey_weight()` now pools same-regime days (`Mon–Fri` vs `Sat–Sun`) at `75%` when exact-day history is sparse (`35%` when exact-day routines have `≥ 3` sessions) while damping cross weekday↔weekend trips to `20–25%`, helping weekday commutes predict earlier in the week without leaking onto weekends.
  - **Unguided historical traffic warnings (`Likely traffic ahead`):** when **Route diversions** and **Learn driving history** are enabled on unguided drives (`≥ 5` recorded journeys), `traffic_service()` checks the current snapped corridor and a `~220 m` forward probe along `gps.course` against the current 2-hour time bucket, surfacing a one-shot amber **Likely traffic ahead** notice (`≥ 60 s` confirmed historical delay, once per congested road per trip) without proposing a diversion.
  - **Unguided-learned Usual Route proposals:** in addition to avoided arcs (`score ≤ -30`), **Usual route available** now also detects rejoin junctions where an alternate incoming road has strong unguided familiarity (`score ≥ +40`, driven 4+ times unguided) while the routed arrival road is unfamiliar (`score ≤ 0`), favouring familiar roads in the usual-route search and verifying `alt_pref ≥ old_pref + 40`.
  - **Auto-named unguided stops, predictions & frequent places:** short `≤ 45 min` unguided chained stops resolve labels from matching saved **Favourites**, mapped Fuel/Parking/Hospital POIs, or **Recents**; unguided **Destination Predictions** (`suggested_place()`) and **Add to favourites?** keyboard drafts (`frequent_named_place()`) now also prefer named mapped Fuel/Parking/Hospital POIs before falling back to street or town names.
- **Aligned Adaptive Auto-zoom & sustained-override guard:**
  - Aligned Adaptive Auto-zoom’s three learned speed regimes with `Z9–Z4`’s hysteresis tiers (`< 25 mph` / `Z9–Z7`, `25–45 mph` / `Z6`, `≥ 45 mph` / `Z5–Z4`) and keyed the applied `±1` offset from the hysteresis-smoothed base `auto_zoom_level`, eliminating boundary oscillation at `20` and `40 mph`.
  - Required manual zoom overrides in Auto zoom to remain active for at least **20 seconds** before recording a session vote (resetting the timer on follow-up zoom taps and cancelling if restored via **MAP**, **Explore**, or undone back to the starting Auto-zoom level), preventing accidental misclicks or brief look-ahead peeks from biasing learned Auto-zoom preferences.

## Version 2.2.5 - Release

- Added an **expandable live GPS HUD inside the top clock/temperature pill** (building on `qspi`'s NMEA `$GPGST`/HDOP/altitude and compass dial contribution in PR #8), designed to stay genuinely usable while driving without blocking the map, car arrow, status notices or controls:
  - **Adaptive three-tier layout with persistent seconds:** toggling the GPS HUD on immediately adds live seconds (`HH:MM:SS`) to the clock pill (persisted in `prefs.ini` as `gpshud=1`) and expands smoothly downward (`164 × 148 px`) on the driving map to display a recessed antialiased compass dial with pivot-aligned `N/E/S/W` markers, centre pivot cap and theme-tuned Day/Night heading needle, bold heading (`074° NE`), colour-coded horizontal accuracy (`$GPGST` error estimate with HDOP × 5 m fallback, green/amber/red at `≤ 5 m` / `≤ 15 m` / `> 15 m`), 3-bar signal indicator with satellite count, and a recessed key–value tray for **Altitude**, **Lat** and **Lon** (marine degrees & decimal minutes, right-to-left mirrored in Arabic and Hebrew, Metric/Imperial units).
  - **Popup-aware & non-blocking:** stays fully expanded while driving and alongside active status notices (`ETA`, `VOL`, `BT CTRL`, slowdown alerts and `OK to review` invitations), automatically switches to a **Compact** live strip (`164 × 52 px`: header + mini live compass, heading, accuracy and signal-bar count) above popups so confirmation and modal dialogs remain vertically centred, and folds cleanly to **Minimised** (`27 px`, keeping seconds visible) while browsing **Menus** or **Explore** - then smoothly glides back open on returning to the driving map. Mirrors cleanly with **Clock left** (`hudtopswap`).
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

## Version 2.2.4 - Release

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

## Version 2.2.3 - Release

- Added **Offline Nearby POIs** as a fourth Trip tab (**Itinerary → Recents → Favourites → Nearby**) and as a source inside **Add stop** and **Change destination**. Scans installed map tiles completely offline (two-pass Z5 3 × 3 inner tiles plus expanding Z3 rings) for **Fuel**, **Parking** and **Hospital** POIs, sorted by distance with 16 × 16 antialiased category badges and route, add-stop and save-to-favourites actions.
- Added a **secondary ("Then …") manoeuvre cue** (`HC_TURN_NEXT`): when a follow-up turn or roundabout occurs within 250 m of the next manoeuvre, a compact frosted-glass pill with a precomputed 17 × 17 antialiased icon docks above the Turn card and smoothly promotes as you complete the current manoeuvre.
- Added a **View diversion** third button to the **Take diversion?** confirmation popup. Selecting it closes the popup and opens north-up Explore framed around the proposed diversion split, bypass and rejoin points for inspection; pressing **MAP** returns to the driving view and re-opens the same confirmation popup (or reports **Diversion unavailable** if the offer expired).
- Added **AVRCP 1.3 `RegisterNotification`** support (`EVENT_PLAYBACK_STATUS_CHANGED` and `EVENT_TRACK_CHANGED`) in `clioa2dp` for immediate track, playback-status and ClioPlay Send destination updates, with automatic fallback to periodic polling for phones that reject notifications.
- Deferred regional `atlas.cpa` graph validation until routing actually opens an atlas (`atlas_validate_graph()`), avoiding eager cold-boot scans of large multi-region atlases while preserving full structural validation before routing.
- Fixed scrollable Itinerary layout and connector lines: itineraries that exceed one page now display whole, vertically centred stops and draw vertical connector lines strictly between consecutive visible circles on the same page, preventing a stray vertical line from appearing above the top stop while scrolling.
- Added Debug-only `RSS_KB` and `FREE_KB` memory telemetry to five-second `FRAME 5S` summaries in `clio843/state.log`.
- Added the desktop `tools/favourites_tool.py` CLI to list, export and import `faves.dat` and `recent.dat` (`CLP2`) in CSV, JSON and GPX formats with atomic writes and deduplication.
- Added translations for all new interface labels across all 18 languages, unified the freestanding ARM runtime header (`arm_runtime.h`), cleaned up stale AVRCP comments, and expanded host regression tests and real-map Newcastle demo GIFs.

## Version 2.2.2 - Release (Major Performance Optimisation)

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

## Version 2.2.1 - Release

- The guided installer and navigation builder accept multiple matching OSM extracts for one cross-border build. The installer merges them locally and rejects conflicting versions of shared objects.
- Bluetooth SBC decoding uses less computation while preserving the same PCM output and playback configuration.

## Version 2.2.0 - Release

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

## Version 2.1.8 - Release

### Changes in this version

- Fixed itinerary connector lines extending towards the Add stop action when additional targets are off-screen. Connections now join visible markers only.
- Increased trip capacity from five to ten targets in total (nine intermediate stops plus the final destination). Saved trips use CTR4 only; unfinished trips from older formats cannot be resumed.
- Removed obsolete saved-place, map, routing and bundle-format readers. Current CLP2 places and current navigation packs remain supported; unsupported formats require fresh data.
- Reduced the size of confirmation dialogs and their action menus while retaining readable labels, existing choices, focus targets and animation behaviour.

## Version 2.1.7 - Release

### Changes in this version

- Restored **BACK** to return to the previous page without advancing or applying a coordinate draft.
- Added a full-width **Next** key at the bottom right of the coordinate keypad. The **±** and **.** keys share the bottom-left column; **0** remains in the middle.
- **Next** validates both fields and opens optional naming; invalid entries stay editable. Soft-right still cycles latitude/longitude and soft-left deletes.
- Updated directional/rotary navigation, regression checks and demo capture scripts.

## Version 2.1.6 - Release

### Changes in this version

1) Simplify coordinate page. Only keypad. Soft right will cycle between lat/long and wrap back around to the other side, while soft left is delete.
2) Vertically recentre the trip symbols (circle & squares) within the selection window
3) Add further customisability in settings:
   - Status pill on left / Time & temp pill on right (DEFAULT VALUE), Status pill on right / Time & temp pill on left
   - Speed pill in left / Direction pill on right (DEFAULT VALUE), Speed pill on right / Direction pill on left
4) If hw_bluetooth = 0, hide all bluetooth-related functionality in the UI.

## Version 2.1.5 - Release

### Changes in this version

- Resized the trips pages to fit in-between the top and bottom pills. Only the keyboard expands to allow more room for entering destination names.

## 2.1.0 - Release

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

## 2.0.0 - Baseline

The baseline for this history includes the major UI redesign: full-screen maps,
animated frosted-glass pills and popups, updated roads/POIs/vehicle marker,
remote and physical-console controls, coordinate destinations, and day/night
boot artwork with a cross-fade into the UI. Detailed behavior and validation
limits are documented in the current user and build guides.
