# ClioPlay version 2.2.5

Version 2.2.5 adds a live **GPS** information panel (compass dial, heading,
GPS time to the second, `$GPGST`/HDOP accuracy, satellite count, altitude and
marine coordinates via double-pressing **Centre / OK** or pressing console
**REPEAT**), expands **Auto zoom** and manual driving zoom across **Z9–Z4**
with hysteresis and full close-up vector precision and street names at **Z7–Z9**,
and tightens **View diversion** / **View route** framing to fill the `480 × 272`
screen — building on ClioPlay’s offline local learning, Nearby POIs, expandable
roundabout secondary manoeuvre pill, AVRCP 1.3 instant Bluetooth notifications
and desktop migration/favourites tools. See
[intelligent navigation](docs/intelligent-navigation.md) and the
[changelog](docs/changelog.md).

## Set up a card

Already installed? Use the [software-only update guide](docs/updating.md) to update without rebuilding maps or downloading firmware. The steps below are for a fresh installation.

Current prebuilt programs are included in this source/publication folder.
To change and rebuild the runtime, follow [BUILDING.md](BUILDING.md).

Install Python 3.11+, osmium-tool, a C++17 compiler, and cabextract or 7-Zip. Run `Install.command` on macOS, or `python3 install.py`.

1. Enter or drag in your first `.osm.pbf` path. Add more paths when prompted, then
   press Enter on an empty prompt to continue. For multiple files, choose extracts
   from the same provider and snapshot. The installer merges them locally, rejects
   conflicting versions of shared objects and asks you to verify the coverage.
   Bounds alone do not prove that a country is complete. You can also supply
   `--pbf PATH` once per file when starting `python3 install.py`.
2. The builder creates and checks every navigation file. Fonts, country profiles,
   timezone boundaries and offline DST rules are bundled; coastline data is
   downloaded and cached automatically on first use.
3. A large combined extract automatically becomes an atlas of overlapping regional packs.
   Small extracts remain a single pack. Select the SD-card root after building.
4. The installer automatically downloads and caches TomTom Navcore 8.843,
   verifies its checksum, and combines `ttsystem` and `vehicledaemon` with the full AUX/remote/Bluetooth bootstrap,
   current programs, fonts and navigation. No previous ClioPlay installation is
   required. Stock firmware stays local and is never included in this publication.
5. Confirm the concrete target. Files are staged and checked before installation;
   replaced files and saved places/trip are backed up. Preferences and pairings
   stay in place. Failed commits attempt rollback, including atlas selectors.

The installer accepts an SD-card root or a local folder for inspecting the
complete installation. Rebuild with `python3 tools/rebuild_arm.py` after
changing native code, then regenerate the previews and refresh the publication as
described in [BUILDING.md](BUILDING.md). Current programs live in `clio843/`; `card-template/`
contains only default startup scripts, preferences and the legacy helpers. The
custom `ttn` embeds AUX initialization; the installer omits `PNDNavigator`.

## Behaviour

- **Boot:** the logo and wordmark sit on a soft, frosted day/night backdrop while
  services and data load. Once
  the first UI frame is ready, the splash cross-fades into the live display over
  about half a second. Map/model/input services continue normally; pressing a
  control reveals the UI immediately. The transition targets 20 FPS, or 5 FPS
  during route calculation. It adds no readiness wait or artificial loading stage.

- **Driving display:** the map fills the screen. The clock shows the outside
  temperature when a valid reading is available. Audio, volume, connection and
  navigation updates animate in as temporary notices. The road name appears
  briefly in the centre when you enter a named road or press **OK**. It shares
  its pill with any camera warning. The speed and turn pills flank it. When a
  follow-up turn or roundabout occurs within 250 m of the next manoeuvre, a
  **Then …** pill docks above the turn pill — staying compact (`24 px`) for
  normal turns and smoothly expanding (`43 px`) for roundabouts so the full
  roundabout diagram with centred exit number and **Take exit N** label fits
  clearly — and promotes automatically as you complete the current turn. Active
  and upcoming stops draw route-coloured endpoint rings on the map, with a
  route-coloured bullseye ring marking the final destination (or a
  route-coloured POI badge when heading directly to a mapped Fuel, Parking or
  Hospital POI). Exceeding a confirmed limit pulses the speed pill red.
- **Menus:** each page title shares the top row with the clock and temporary
  audio or connection notices. In Bluetooth control mode, Left and Right skip
  tracks, and OK toggles playback. The selected control briefly highlights.
- **Bluetooth audio:** on units with `hw_bluetooth=0`, the Bluetooth menu, phone
  audio, phone destinations and related controls are unavailable. On equipped units,
  selecting another audio source suspends decoding while
  keeping the phone connection, controls and destination reception available.
  AVRCP 1.3 `RegisterNotification` delivers track, playback-status and ClioPlay
  Send updates immediately when supported by the phone, with automatic fallback
  to periodic polling otherwise.
  If a media stream stalls without suspending its transport, playback closes
  after queued audio has had time to drain and restarts when packets return.
  Phone-audio camera cues keep their own playback window, including while paused.
- **Units:** Auto / Metric / Imperial. Country profiles cover 260 territories;
  explicit mph road evidence is a fallback when country resolution is unavailable. Speed, limits and distances agree.
  Numeric metric limits retain their exact value, including 130 km/h.
- **Limits:** directional OSM numbers and 657 explicit country context tags,
  with reviewed overrides. Estimates show `?`, conditional/variable limits `VAR`,
  unrestricted roads `INF`. Untagged urban streets can use local mapped consensus
  or an unambiguous country profile, always marked `?`. Sparse/conflicting context
  and unsupported restrictions remain `--`; unmapped local zones can still make
  an estimate wrong. No weather or live variable-sign input is available. See
  [speed evidence and optional corrections](navigation-builder/README.md).
  Routing travel-speed estimates are separate.
- **Routing:** motorcar-specific access overrides broader vehicle/access tags;
  directional access, oneway exceptions and blocking nodes are respected.
  Unresolved conditional access and purpose-only roads remain excluded from the
  all-times public graph. Roundabouts count legal exits where data permits, and
  their icons follow the country's driving side.
- **Regional maps:** CPA1 connects exact shared OSM default routing states across
  packs, preserving arrival restrictions. Atlas graph records are validated
  lazily on first routing use so cold boot stays fast on large multi-region
  atlases. Planning is scheduled in small slices;
  QAD7 endpoint sweeps share work across gateways, with ordered fallback for
  unreachable destination roads; geometry is built only for selected legs.
  Deviations can repair the current leg while retaining its exact gateway
  and remaining atlas plan. Paged query caches reset when changing packs.
  Tiles have a single owner and gateways never become user stops. Each file stays
  below the device's signed 2 GiB seek limit. Build one PBF covering the intended
  journey; independently built country atlases are not automatically merged.
- **Cameras:** OSM plus optional CSV imports; fixed, red-light and directional
  average-speed sections. Mapped road identities reject a different bridge or
  parallel road when a route is active. Average speed starts only after an
  observed entry. GPS loss/staleness cancels it. Off-route matching is geometric
  and can remain ambiguous. Speed values appear only on fixed/average-speed
  alerts; red-light alerts show their type and distance.
- **Camera policy:** information-only alerts enabled in **19 reviewed countries**;
  **FR/DE/CH/CY** excluded, with a conservative export border buffer. Other
  territories stay disabled pending review. Imports cannot bypass policy. See the
  [country list and review sources](docs/camera-support.md).
- **Camera sound:** Off / Visual / **Phone audio**. The two-tone cue ducks and mixes
  into our existing Bluetooth PCM stream, including paused playback. Radio/AUX
  retain visual alerts; FM interruption is not implemented. Physical playback
  timing still needs vehicle verification.
- **Clock:** offline automatic timezone and DST from GPS, with a manual fallback.
  Settings → System → Clock: Enter toggles Auto/manual; Left/Right selects a manual offset
  in 15-minute steps. Rules span 2020–2060 and retain the last offset at boot.
- **Auto theme:** a custom daily schedule using local time. In Settings → Display, choose
  Auto under Theme, then set **Day starts** and **Night starts**. Defaults are
  07:00 and 18:00. Scroll down to the schedule rows, use **OK** to select hours
  or minutes, and **Left / Right** to adjust the highlighted part. The times are
  saved and also apply to the boot splash. Day/Night overrides remain available.
  The unit clock and saved UTC offset provide the schedule before GPS is ready.
  With no valid clock, Auto retains the last theme, defaulting to Night.
- **Language:** 18 options in Settings → System, including Arabic, Simplified Chinese,
  Japanese, Korean, Hindi, Hebrew, Greek and Cyrillic languages. See
  [the complete language list](locales/README.md). Native destination and
  street names remain unchanged. Received UTF-8 names, Arabic/Indic shaping and
  bidirectional text run on the device. Fonts cover major scripts, not all Unicode
  or emoji. English uses “Bluetooth” for the page title; other languages retain their
  translated phone labels. The audio notice uses “BT” in every language.
  The naming keyboard provides independent uppercase/accented and native-script
  layouts, with left-soft backspace, right-soft space and a wide language chooser.
  See [keyboard controls and input methods](locales/README.md#favourite-keyboard).

## Physical console and temperature sources

The Bluetooth remote and Scénic III / Mégane armrest console feed the same UI
handlers and can be used together. No extra input or temperature daemon is
needed. Both input paths and both temperature readers are available automatically;
the installer requires no hardware-specific setup.
ClioPlay uses valid incoming events/readings from whichever hardware is present.
An absent source adds no UI notice or placeholder. The factory vehicle daemon
uses its normal vehicle configuration to supply physical console events.

| Console control | Action |
| --- | --- |
| Top-left / top-right soft buttons | Same as remote soft-left / soft-right: zoom, delete, next or space according to the current page. |
| BACK | Same as remote BACK, including BT CTRL on the driving map. |
| INFO | Briefly show the current road name and, during guidance, the journey estimate. |
| MENU | Open Menu. |
| LIGHT | Cycle Day, Night and custom-schedule Auto. |
| REPEAT | Open and close the live GPS information panel directly. |
| DEST | Open Trip directly, unless a confirmation is awaiting a decision. |
| MAP | The existing Auto/manual zoom, Explore and return-to-map behaviour. |
| Joystick | The existing arrow actions, including Explore panning. |
| Knob rotation | Zoom the map; previous/next track in BT CTRL; scroll menus; select dialog choices; traverse keyboard and coordinate keypad keys. |
| Knob press | OK/confirm; play/pause in BT CTRL. |

Duplicate evdev knob events are ignored only after a valid console FIFO record
has been received. An open but idle FIFO does not claim the controls. Without
valid FIFO input, evdev knob events remain usable and never acquire the
Bluetooth remote’s input lock. The optional `cswmap.ini` mapping file is retained
across installations.

Default mappings work without a configuration file. For overrides, copy
[the contributed example](card-template/cswmap.example.ini) to the card as
`clio843/cswmap.ini` (or `cswmap.ini` in the card root, which takes precedence).
The example contains the contributor’s conservative bindings; it disables LIGHT
and REPEAT and maps DEST to Menu. Change `btn6=THEME`, `btn7=GPS`, `btn8=TRIP`
and `btn4=INFO` to keep the integrated defaults. Actions accept upper/lower case;
`ROTNEXT`/`ROTPREV` or `knob+`/`knob-` select contextual knob movement.
`GPS` opens the GPS panel and closes it again (default on `btn7` / REPEAT; can
be moved to another button or replaced with `INFO` or `NONE`).
`NONE` disables a control. Add `verbose=1` for interpreted console-field logging
while Debug is enabled. File edits, including same-size edits, reload within
five seconds; invalid/oversize files keep the last valid bindings. Deleting the
file restores defaults. Long-press mapping is optional and remains unverified;
neutral records never act as OK, and holds do not become repeated confirmations.

**Both temperature sources remain active.** Valid Focus `cfstate` temperature
wins. When it is absent/invalid, a valid VehicleStatus item 4 from interface 37
is used. Unknown `-1024`, `999`, malformed packets and out-of-range readings
are hidden. A connected source is not expired just because the temperature has
not changed. Disconnecting VehicleStatus invalidates its cached reading; it
falls back to Focus or hides the temperature until a fresh reading arrives.
VehicleStatus temperature works independently of console input: no button press
or prior console detection is needed.

The inline receiver supplies the navigation-side PAC sinks reported by the
contributor (3400/3401/3700/3800/4000/4001/5600/5700/5800), decodes only the
11-byte temperature update, sends no replies and never logs identity/VIN
payloads. It does not replace a live socket owned by another service or write
into `focusd`'s state file. Startup creates the console FIFO before reconnecting
the stock daemon, preserves existing FIFOs, and resolves the card mount before
using relative runtime paths. Diagnostics remain Debug-only.

The original protocol was vehicle-tested by the contributor on a 2010 Scénic
III. The combined implementation has host tests for input framing, real FIFO
reconnects, both inputs, UI contexts, temperature precedence and malformed data.
Linux Unix-sequenced-packet transport tests run when that facility is available;
some hosts cannot exercise that transport. Physical validation of this integrated
build is still required, including detents, neutral/release frames, temperature
reconnect and unchanged Bluetooth operation. Factory-radio audio remains a
separate compatibility question. See [the contributed hardware findings](docs/scenic-iii-hardware.md).

## Remote and console controls

Every remote action below is available with the default console mapping. Use
these equivalents throughout the driving, Trip, Bluetooth, Settings, keyboard,
coordinate-entry and dialog instructions:

| Remote input | Physical console input |
| --- | --- |
| MAP / MENU / BACK | MAP / MENU / BACK buttons |
| Centre / OK | Press the knob |
| Up / Down / Left / Right | Move the joystick in that direction |
| Left / right soft keys | Upper-left / upper-right console buttons |

Turning the knob is an additional convenience: it zooms the map, selects tracks
in BT CTRL, scrolls lists and traverses keyboard/keypad buttons and dialog choices.
In Settings, turn it to select a row; joystick Left/Right changes the value and
knob press performs OK. In Trip, use the joystick Left/Right for tabs or
for the buttons on the Itinerary footer. Turning the knob moves between list
rows and wraps at the ends. No long-press binding is required.

The [console action table](#physical-console-and-temperature-sources) also covers
INFO, REPEAT, LIGHT and DEST shortcuts. Optional custom mappings can change these
bindings; the defaults provide complete remote-equivalent control.

### Driving, map browsing and BT CTRL

| Button | Driving map | Map browsing | BT CTRL on the driving map |
| --- | --- | --- | --- |
| **MAP** | Restore Auto zoom after a temporary manual adjustment; otherwise enter map browsing. | Return to the driving view and the configured zoom mode. | Leave BT CTRL; restore Auto zoom if temporarily adjusted, otherwise enter map browsing. |
| **MENU** | Open Menu. | Open Menu. | Leave BT CTRL and open Menu. |
| **BACK** | Enter BT CTRL when Bluetooth audio is selected and a phone is connected. | | Leave BT CTRL. |
| **Centre / OK** | Press once to briefly show the current road name and (during guidance) ETA and remaining distance; press again within 4 seconds to open the live **GPS** panel. | Open the route or add-stop prompt for the point under the crosshair. | Play or pause. |
| **Up / Down** | Enter map browsing and pan. | Pan up or down. | |
| **Left / Right** | Enter map browsing and pan. | Pan left or right. | Previous or next track. |
| **Left / right soft keys** | Zoom out or in. With Auto zoom enabled, this sets a temporary override. | Zoom out or in. | Zoom out or in on the driving map. |

With Auto zoom enabled, the soft keys temporarily adjust the driving zoom and
show **Manual zoom**. **MAP** restores Auto zoom without entering Explore; a
subsequent **MAP** enters Explore. Arrow keys can enter Explore and pan directly,
including from temporary manual zoom. **OK** briefly shows the road name in
the centre pill and, during guidance, the journey estimate in the status pill;
pressing **OK** a second time within 4 seconds opens the live **GPS** panel
(even with no GPS fix or active route, while keeping existing **OK to review**
invitations, Explore selection and **BT CTRL** playback priority intact).
The estimate also appears when a route calculation or reroute finishes.

Explore shows one entry notice. Zooming and panning within Explore neither replay
that notice nor extend it. Leaving Explore shows **Auto zoom**, or **Manual zoom**
when Auto zoom is disabled in Settings → Navigation. These mode notices share the usual glass
styling, transitions and temporary display time.

Confirming a point selected in Explore returns directly to the driving map with
the reverse rotation/tilt animation. The time/temperature pill remains visible;
calculation uses the existing five-frame-per-second cap. Declining keeps Explore.

#### Auto zoom and close-up street names

Auto zoom selects across **Z9–Z4**, giving closer views at low speeds while keeping roughly 33–51 seconds of forward sightline across driving speeds. Manual driving zoom (soft keys / knob rotation) also reaches **Z9** on standard Z6 map packs. Separate speed thresholds for speeding up (zooming out) and slowing down (zooming in) create a hysteresis band around each boundary so small speed fluctuations do not keep changing the zoom; large speed jumps step straight to the matching level. When **Learn driving history** is enabled, learned Adaptive Auto-zoom adjustments (`±1` level) are clamped across the full **Z4–Z9** range.

| Zoom | Default speed band | Zooms out (speeding up) | Zooms in (slowing down) | Forward view | Where it fits |
| --- | --- | --- | --- | --- | --- |
| **Z9** | `< 5 mph` (`< 8 km/h`) | `≥ 6 mph` (`≥ 10 km/h`) → Z8 | — | ~92 m | Parking and walking-pace manoeuvres |
| **Z8** | `5 to < 12 mph` (`8–19 km/h`) | `≥ 14 mph` (`≥ 23 km/h`) → Z7 | `≤ 4 mph` (`≤ 6 km/h`) → Z9 | ~184 m | Car parks, queues and tight streets |
| **Z7** | `12 to < 25 mph` (`19–40 km/h`) | `≥ 27 mph` (`≥ 43 km/h`) → Z6 | `≤ 10 mph` (`≤ 16 km/h`) → Z8 | ~367 m | Residential streets and town centres |
| **Z6** | `25 to < 45 mph` (`40–72 km/h`) | `≥ 48 mph` (`≥ 77 km/h`) → Z5 | `≤ 23 mph` (`≤ 37 km/h`) → Z7 | ~735 m | Urban through roads and suburban driving |
| **Z5** | `45 to < 65 mph` (`72–105 km/h`) | `≥ 68 mph` (`≥ 109 km/h`) → Z4 | `≤ 42 mph` (`≤ 68 km/h`) → Z6 | ~1.47 km | Faster A-roads and dual carriageways |
| **Z4** | `≥ 65 mph` (`≥ 105 km/h`) | — | `≤ 62 mph` (`≤ 100 km/h`) → Z5 | ~2.94 km | Motorway lookahead |

At **Z7–Z9**, roads preserve all four fractional bits of Z6’s 12-bit tile coordinates for smooth close-up geometry, and road names from the installed map are placed beside visible clipped road segments where there is room on screen (keeping clear of the active route, car marker, Explore crosshair and HUD cards, and handing off cleanly with the temporary bottom street-name pill). Existing map packs support both improvements with no rebuild required.

### Menu, Trip and Bluetooth

- **Menu:** use **Up / Down** to choose Trip, Bluetooth or Settings. Press **OK**
  to open it or **BACK** to return to driving. From a page, **MENU** or **BACK**
  returns to Menu. **MAP** goes directly to the driving map.
- **Trip:** always opens **Itinerary** first. Use **Left / Right** to cycle
  **Itinerary → Recents → Favourites → Nearby**. Each tab remembers its selection.
  **Up / Down** or knob rotation moves between rows and wraps at the ends.
  On the Itinerary footer, **Left / Right** wraps between **Add stop**,
  **Cancel route** and **Diversion** when offered. Move **Up / Down** to a
  target to switch tabs again. **OK** opens the highlighted action. A trip holds up to **10 targets** (9 stops plus the final
  destination), including stops already visited. Scroll to see targets beyond the
  visible rows; visible rows stay vertically centred and connector lines join
  only consecutive visible markers on the current page. **Add stop** is
  disabled at capacity; remove an unvisited stop or start a new trip to free space.
  The itinerary includes **Add stop** and **Cancel route**;
  an empty itinerary offers **Coordinates**. **Nearby** scans installed map tiles
  offline for **Fuel**, **Parking** and **Hospital** POIs around your current
  position, sorted by distance. Add stop and Change destination let
  you choose Recents, Favourites, Nearby or Coordinates, then review before confirmation.
  **BACK** returns one level without applying a draft. Phone destinations arrive
  automatically, with no receive option to open.
  On desktop, `python3 tools/favourites_tool.py` inspects, exports and imports
  `faves.dat` and `recent.dat` (`CLP2`) via CSV, JSON and GPX.
- **Bluetooth:** **Pair new phone** stays at the top of the saved-phone list;
  select it and press **OK** to start pairing. **Up / Down** or the knob
  selects a device, **OK** opens its Connect/Disconnect and Forget actions, and
  **Up / Down** selects the action. **OK** confirms; **BACK** returns to the list.
- **GPS:** a live information panel opened directly from the driving map by
  pressing **Centre / OK** (or pushing the console knob) twice within 4 seconds,
  or with a single press of the console **REPEAT** button (`GPS` action). A
  large compass on the left shows the heading as a light red arrow. Beside it
  are the GPS time to the second and three columns: **Heading** in degrees with
  N/NE/E…, estimated horizontal **Accuracy** with the satellite count, and
  **Altitude** above sea level. The position runs along the bottom in marine
  degrees and decimal minutes, for example `47° 29.887' N   019° 02.420' E`
  (mirrored right-to-left in Arabic and Hebrew). When the car stops, the heading
  and arrow hold the last course, as the map does; without a fix the values show
  `--` and the bottom row reads **No fix**. Accuracy uses the receiver’s
  `$GPGST` error estimate when present, otherwise HDOP × 5 m; green, amber and
  red mark up to 5 m, up to 15 m and worse. Accuracy and altitude follow the
  Metric/Imperial setting. Press **BACK**, **MAP**, or **REPEAT** again to
  return directly to the driving map.

All these pages are animated frosted-glass sheets over the live map. Menu,
Settings, Bluetooth, Trip, coordinates and review use compact sheets between
the persistent HUD pills. Only the naming keyboard expands into the lower HUD area. An unusually tall status notice
shrinks the sheet in sync with the notice. The current page stays visible and
its scrolling rows fade at the edges. The top clock and status pill keep their
position and notice timers across page changes.

### Intelligent navigation

Optional offline learning uses completed Barcelona power-off driving sessions, even without an active route. Enable **Learn driving history** in Settings → Driving history. The empty Itinerary retains the original destination-entry layout. With no active trip and sufficient evidence, the map status pill displays a green Suggested destination and blue OK to review (BACK to review during BT CTRL). OK opens a default-Yes confirmation; No suppresses that destination for the recording session, Back leaves it eligible, and ignoring it never changes the route. On a later stationary startup, five completed visits across three distinct dates in the last 90 days can prompt Add to favourites? for the current place. No is the safe default. Yes opens the existing Favourite naming keyboard with an editable inferred name (or an empty field); only Save adds the Favourite and returns to the map. The prompt is shown at most once per place until driving history is cleared, and never starts navigation. The predictor reconciles the stored starting point with live GPS, combines day/time, recency and early trajectory evidence, and resolves destinations via named Favourites/Recents, nearby offline map names or coordinates. During guidance, a historical traffic warning may appear and a **Diversion** action is available in Trip if a beneficial legal local alternative is found. Shared geometry stays solid magenta, bypassed geometry is yellow/gold and the distinct proposed diversion is dashed aqua/teal. The glass status pill continuously displays a single line with the success-green Faster route available announcement and blue OK to review (BACK to review when BT CTRL is active). A centred, anti-aliased frosted-glass badge near the actual decision point shows the approximate saving when space permits. The invitation disappears at the decision point if ignored. Press OK twice from the driving map to accept; with BT CTRL active, press Back first. Normal zoom and Bluetooth controls are preserved. The confirmation opens on Yes, alongside No and a **View diversion** button.
Selecting **View diversion** (or **View route** for a usual-route proposal) closes the popup and opens Explore tightly framed and vertically centred around
the proposed bypass across the full `480 × 272` viewport (up to Z9) so you can pan and zoom to inspect it; pressing **MAP**
returns to the driving view and re-opens the same confirmation (if still valid).
Declining it keeps the original route, warning and adjusted ETA, and only suppresses alternative offers overlapping that section. Later independent diversions remain eligible. No live traffic or automatic route selection is used. Learning still relies only on completed power-off sessions, not arrival prompts. Destination suggestions and diversions can be enabled separately; **Clear driving history** removes the private learned data after confirmation. Raw GPS time is independent of the UI clock adjustment. See [intelligent navigation](docs/intelligent-navigation.md) for the complete behavior and limitations.

### Settings

Use **Up / Down** to highlight a category and **OK** to open it. Use the same controls within each page to highlight a setting; **Back** returns to its category. The available actions depend on the setting.

| Category | Setting | Left / Right | Centre / OK |
| --- | --- | --- | --- |
| Display | Brightness | Adjust in 10-point steps. | |
| Display | Theme | Cycle Day, Night and Auto. | |
| Display | Day starts / Night starts | Adjust the highlighted hour or minute. | Switch between editing hours and minutes. |
| HUD layout | Top pill order | Toggle Status left / Clock left (status and time/temperature swap sides). | |
| HUD layout | Bottom pill order | Toggle Speed left / Direction left. | |
| Navigation | Units | Cycle Auto, Metric and Imperial. | |
| Navigation | Auto zoom | Toggle on or off. | |
| Navigation | Camera alerts | Cycle Off, Visual and Phone audio (Off/Visual only if Bluetooth hardware is absent). | |
| Driving history | Learn driving history | Toggle recording and learning. | Toggle. |
| Driving history | Destination suggestions | Toggle learned suggestions. | Toggle. |
| Driving history | Intelligent diversions | Toggle historical traffic and diversion offers. | Toggle. |
| Driving history | Clear driving history | | Open deletion confirmation. |
| System | Language | Cycle backwards or forwards. | Open the language chooser. |
| System | Clock | Set a manual UTC offset in 15-minute steps. | Toggle automatic and manual time. |
| System | Debug log | Toggle on or off. | |

The release version is shown at the bottom of Settings.

### Naming keyboard and dialogs

- In **Rename**, the **direction pad** moves between characters and actions.
  **OK** types the highlighted character or activates the highlighted action,
  including character pages, the language chooser and Save (or Next when naming
  a coordinate destination). Horizontal navigation wraps each row; vertical
  navigation wraps through the character grid and toolbar. Numbers and punctuation
  have their own mode. The **left soft key**
  deletes, the **right soft key** inserts a space, and **BACK** returns to the
  previous page without saving (or to longitude with the coordinate name retained). In the language chooser, **Up / Down** moves one language,
  **Left / Right** jumps six, both directions wrap, **OK** selects, and **BACK**
  closes the chooser. See [language-specific input](locales/README.md#favourite-keyboard).
- In option dialogs, **Up / Left** selects the previous option and **Down / Right**
  selects the next one. Choices wrap at the ends. In Yes/No dialogs, use
  **Left / Right**. **OK** confirms,
  and **BACK** dismisses ordinary dialogs. Menu action lists use **Up / Down**;
  their Yes/No confirmations still use **Left / Right**. At the saved-trip resume prompt, choose
  an option and confirm with **OK**. At arrival, **OK** continues from a stop or
  finishes the trip. The arrival countdown can also complete that step.

## UI previews

The [main README](../README.md#ui-previews) embeds short native framebuffer GIFs
of driving, menus, Bluetooth playback and receiving a destination. See the
[preview notes](docs/demo/README.md) for the sample inputs and regeneration steps.

## Contents and validation

| Folder | Purpose |
|---|---|
| `card-template/` | Default bootstrap scripts, preferences and legacy helpers used by the clean installer |
| `navigation-builder/` | One-PBF builder, atlas tools, profiles, fonts and timezone data |
| `clio843/src/` | Freestanding ARMv4T runtime and vendored text engines |
| `tools/`, `tests/`, `docs/` | Build tools, tests and essential documentation |

This is the single canonical Global edition. See
[BUILDING.md](BUILDING.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

Host tests cover the unsplit reference graph versus restricted two- and three-pack atlases,
including a long active via-way restriction,
an actual Hendaye–Irun France/Spain route, native text, cameras, clock transitions,
installer rollback, remote controls, water/bridge layering and antialiasing.
Audio regressions check exact SBC arithmetic and folded synthesis, bounded
reads/copies, identical PCM, rate fallbacks, source changes, camera cues, idle
drain and maintenance timers independent of Bluetooth packet wakeups.
Navigation bundles must finish building and pass their checks before installation.
Host fixtures do not establish complete country-scale coverage. Geographic metadata
is not a legal boundary survey; Auto units retain a manual override. Dateline-spanning/planet extracts and
latitudes beyond ±85° remain unsupported.

**Vehicle validation remains open:** boot, physical remote, AUX/radio, Bluetooth,
frame timing and memory need the actual head unit. The legacy
`focusd`/embedded AUX helpers remain binary-only; their original C source has not
been recovered. Their hashes and role are documented in
[source status](docs/source-status.md).

For desktop-specific installation steps, see the root [README](../README.md)
and [Windows setup](../WINDOWS.md). Windows uses the same installer inside WSL.

### Shared status notifications

GPS acquisition/recovery, missing maps, positions outside map coverage, route failures
and no-active-route changes use the animated status pill. Action confirmations and
errors (for example, saving a favourite or adding a stop) also use that pill on both
map and menu pages. They share space with audio, volume and Bluetooth feedback.
Messages wait for space and receive about four seconds of display time; unchanged
states are not repeated. Resolved GPS/map/route states cancel stale waiting messages.
Navigation and menus share the same expanding status layout and animations. On
menu pages the pill is centred in the header and can briefly overlay the upper
content while expanded. Entering from navigation gives it a fresh fade-in at that
anchor; all menu and settings pages share that same appearance and original timer.
Moving within the menu tree never replays its entrance. New status changes still
animate normally, and the clock/temperature pill stays persistent. Route calculation retains its moving
progress indicator. Road names and camera alerts retain their dedicated bottom pill.

Status text uses neutral theme ink for information, green for confirmations, amber
for warnings such as GPS acquisition, and red for errors. New Bluetooth song titles,
artists/albums and radio station/frequency changes trigger a short animated update.
Playback-position reports do not retrigger it. Blank or `--` metadata stays hidden.
These updates also work in menus and during BT CTRL; volume and transport controls
remain available alongside them.

In BT CTRL, a thin playback progress bar sits above the control symbols. It uses
reported track position and duration, smooths position/seek updates and freezes
when paused. Unknown durations (including live streams) leave the bar hidden.
The existing pill stays the same height; route calculation keeps its own moving
loading indicator. The bar displays playback position only; it is not a seek control.

### Frame scheduling

The normal map refresh targets 10 FPS and drops to 5 FPS during initial routing
and rerouting, whether parked or moving. During calculation the **entire display
is capped at 5 FPS**, including pills, loading indicators, playback, speed warnings,
menus and Explore rotation. This leaves more CPU time for routing. New input and
status changes queue for the next frame; input, GPS and audio services keep running.
On completion, failure or cancellation, normal presentation resumes immediately.
Outside calculation, live and prepared animations and steady HUD updates target
20 FPS. The compositor draws over the latest finished map. While parked, it
holds the current map until the scene changes and prepares finite pill, popup
and menu transitions before displaying them. The extra preparation adds a
short delay. Routine playback and clock updates let the queued transition
finish. Unchanged screens hold their last image. When the framebuffer driver
supports partial writes, only the changed overlay rows are sent.

Frosted surfaces combine complete 8 × 8 backdrop areas with a separable
1–2–1 blend over neighbouring areas in both axes, softening detail over a
24 × 24 footprint. Three small rolling rows avoid a full-screen blur.
They retain fractional colour
precision through sampling, tint, interpolation and opacity. Full-strength,
screen-anchored 8 × 8 ordered dithering at the final RGB565 write reduces colour
banding in broad gradients, especially on large dark popups. It introduces fine
fixed grain; the pattern does not cycle between frames. Text and symbols are
drawn afterwards, and exact RGB565 colours such as selection blue stay exact.
The dither uses a 256-byte constant table, without diffusion buffers or another
image pass. Samples are reused while the
finished background and pill geometry remain unchanged; overlapping cards and
speed-limit insets are sampled afresh.

All popup windows use this glass, including route confirmations, saved-place and
itinerary actions, phone prompts, Bluetooth actions and keyboard language choices.
Yes/No confirmations use compact cards with reduced padding; itinerary removal
and cancellation retain the target name. Action menus size to their available
choices. Their entrances, exits, resizing, text changes and moving selection highlights
share the pills' independent 20 FPS animation clock (5 FPS during calculation).
Popup-only input does not force a new map frame. Actions take effect immediately;
copied popup content remains available while closing, including after deletion
or a page change. A settled popup returns to the normal presentation cadence.

Scrollable Itinerary, Recents, Favourites, saved-phone, Settings and keyboard-language lists
show a proportional scrollbar only when more rows exist than fit onscreen.
The thumb tracks the actual scrollable list size and visible window. The
phone-pairing action is pinned separately above the device list. Popup scrollbars use translucent antialiased strokes over the glass
and share its entrance, exit and content fades.

Explore rotation and tilt follow a 500 ms elapsed-time transition rather than a
fixed number of map frames. ClioPlay normally prepares the ten map backgrounds
for the rotation before presenting them at 20 FPS. The HUD draws over each
prepared background and keeps its own notification timers. Offscreen strokes are rejected
conservatively before rasterisation, and tile coordinates use an exactly
equivalent shift/remainder conversion instead of repeated software division. Live model updates keep
running, and rotation never resets a pill's appearance or notification timer.
Outside calculation, long map drawing yields at safe points to present overlays.
Routing uses spare
deadline budget in chunks of at most 5 ms, with periodic cooperative clock checks.

These are scheduling targets, not measured frame rates on the 400 MHz head unit.
Slow SD reads or drawing can delay presentation. Live transitions sample the
current time. Prepared sequences keep every frame in order and take longer when
the unit is busy. With Debug enabled, five-second `FRAME 5S` summaries in the
existing `state.log` report presentations, scheduled map frames, extra camera
passes, lateness, drawing/routing time, animation preparation time and
`RSS_KB`/`FREE_KB` memory telemetry. No frame log is written with Debug off.

### Radio and Bluetooth source selection

CD is presented as BT. AUX stays wired AUX with or without a connected phone;
FM/MW/LW retain their radio labels and disable Bluetooth PCM output. When no valid
live radio source can be read, BT is the independent default source: output,
metadata and playback progress are available, and BT CTRL requires a connected
phone. A later valid radio source takes priority. This fallback also applies at
startup before a live source arrives; it does not classify the radio as incompatible.

Cached radio/volume values are hidden. Neither an unknown volume on first boot
nor a saved volume determines whether Bluetooth may play. Physical volume
controls remain with the vehicle; ClioPlay has no user-adjustable software volume.
Renault's [Carminat handbook, printed pages 5 and 16](https://www.user-manual.renault.com/sites/renault/files/Carminat-Tom-Tom-NX947_ENG.pdf)
covers dashboard radio displays and adjusting guidance volume with the audio
system's controls. This supports separating audio output from display telemetry;
it does not establish that our mixer setup works on every radio. Independent
playback, physical volume/mute response and coexistence with factory Bluetooth
need vehicle testing. Missing telemetry alone does not prove a missing audio path.

Source/track notices remain temporary and use the shared glass animations in
navigation and menus. Console input uses the independent reader described in the
[controls guide](#physical-console-and-temperature-sources).

### Decimal-degree destinations

**Trip → Itinerary → Coordinates** opens a single keypad sheet with latitude
and longitude fields. **Add stop** and **Change destination** expose the same
Coordinates source. Arrow keys or console knob rotation select among 13 keys;
**OK** presses the highlighted key. The bottom-left column contains two half-width
keys: **±** toggles the sign and **.** inserts the decimal separator. **0** is in
the middle and **Next** is at bottom right.

The left soft key deletes the last character; the right soft key cycles between
latitude and longitude, wrapping in both directions. Select **Next** to validate
both fields and open optional naming. An invalid or empty field is selected and
its warning shown above the keypad; nothing is saved. **BACK** returns to the
previous page (Itinerary, source selector or Review when editing) without applying
the draft. Reopening a new coordinate entry starts with empty fields.

On the name keyboard, the right soft key inserts a space and **Next** opens
Review. BACK from naming returns to the coordinate sheet with the draft name
retained. Review offers the route/stop action, Edit coordinates, Edit name and
Cancel; backing out of Edit coordinates restores the prior review values. MAP/MENU
leave without saving. No saved places or itinerary targets change until confirmation.

Latitude accepts −90…90, longitude −180…180; minus means south/west. Decimal
points use `.` in every interface language. Up to seven fractional digits can be
entered; the router stores 1e-5 degree integers and review displays the rounded
five-decimal values. Exact geographic bounds are checked before rounding. Parsing
uses bounded integer operations, without floating point or a geocoding service.
An empty/space-only name becomes the coordinate pair. Saved names use the existing
40-character Unicode limit. Recents and Favourites use CLP2 only; current saved places are retained.
Trips use CTR4 only. Older trip and saved-place formats are ignored, not migrated
or deleted on load. Subsequent saves replace the respective file. Current maps
use CPM5, CPR2 version 5 and matching QAD7; the installer accepts version-2
navigation/atlas manifests. Rebuild outdated navigation packs before use.
Existing nearby-place deduplication applies to Recents. Coordinate entry does not
expand map coverage; normal navigation-data/routing errors still apply.

Phone destinations wait while this editor is open. Explicit coordinate
confirmation takes precedence over a phone destination received during entry;
cancelling leaves the received destination available to the existing phone flow.
Physical audio, controls and navigation behaviour still need the vehicle checks.

POI layout uses a separate bounded candidate pool, reserves icon space before
road/place names, and selects independently of tile traversal order. Driving
and Explore share a limit of 16 non-overlapping icons. Status-pill animation
does not change which POIs belong in the map background; pills overlay the
finished map instead of removing its icons.
Parking, fuel and hospitals use the POIs available in the installed zoom tiles.
No map rebuild is required for this renderer correction; missing source POIs
cannot be added by the renderer.

Fuel, parking and hospital POIs use 16 × 16 translucent rounded badges with
precomputed antialiased vector symbols. Amber, blue and teal glyphs use stronger
day inks and brighter night inks. The icons remain inside their existing layout
footprints and add no runtime blur or font dependency.

For device validation, compare Explore entry/exit with Bluetooth playing and
paused, including simultaneous volume/track notices, reversal and route start.
The camera and HUD target 20 FPS. Actual frame rates depend on the head unit and its workload.
During route calculation the whole UI remains capped at 5 FPS.
