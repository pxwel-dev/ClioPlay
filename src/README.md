# ClioPlay Global

A standalone edition of the compact 480×272 ClioPlay interface for non-LIVE
Carminat TomTom. It keeps the remote workflow, continuous antialiased roads/routes,
Trip workspace, our Bluetooth receiver and physical AUX initialization. It targets
cars **without factory Bluetooth**. Factory Bluetooth/hands-free coexistence stays
outside this version.

## Set up a card

Current prebuilt programs are included in this source/publication folder.
To change and rebuild the runtime, follow [BUILDING.md](BUILDING.md).

Install Python 3.11+, osmium-tool, a C++17 compiler, and cabextract or 7-Zip. Run `Install.command` on macOS, or `python3 install.py`.

1. Select your `.osm.pbf` first. The installer scans it and asks you to verify the
   intended coverage. Complete relations can contain remote nodes; both source
   metadata and the installation envelope are retained. Bounds alone do not prove
   that a country is complete.
2. The builder creates and checks every navigation file. Fonts, country profiles,
   timezone boundaries and offline DST rules are bundled; coastline data is
   downloaded and cached automatically on first use.
3. Large extracts automatically become an atlas of overlapping regional packs.
   Small extracts remain a single pack. Select the SD-card root after building.
4. The installer automatically downloads and caches TomTom Navcore 8.843,
   verifies its checksum, and combines `ttsystem` and `vehicledaemon` with the full AUX/remote/Bluetooth bootstrap,
   current programs, fonts and navigation. No previous ClioPlay installation is
   required. Stock firmware stays local and is never included in this publication.
5. Confirm the concrete target. Files are staged and checked before installation;
   replaced files and saved places/trip are backed up. Preferences and pairings
   stay in place. Failed commits attempt rollback, including atlas selectors.

The installer accepts an SD-card root or a local folder for inspecting the
complete installation. Rebuild with `python3 tools/build_ui.py --install` after
changing native code. Current programs live in `clio843/`; `card-template/`
contains only default startup scripts, preferences and the legacy helpers. The
custom `ttn` embeds AUX initialization; the installer omits `PNDNavigator`.

## Behaviour

- **Driving display:** the map fills the screen. The clock shows the outside
  temperature when a valid reading is available. Audio, volume, connection and
  navigation updates animate in as temporary notices. A new road name appears
  briefly in the centre and shares its pill with any camera warning. The speed
  and turn pills flank it. Exceeding a confirmed limit pulses the speed pill red.
- **Menus:** each page title shares the top row with the clock and temporary
  audio or connection notices. In Bluetooth control mode, Left and Right skip
  tracks, and OK toggles playback. The selected control briefly highlights.
- **Bluetooth audio:** selecting another audio source suspends decoding while
  keeping the phone connection, controls and destination reception available.
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
  packs, preserving arrival restrictions. Planning is scheduled in small slices;
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
  [country list and review sources](docs/camera-support.md). No invented French
  danger zones are substituted.
- **Camera sound:** Off / Visual / **Phone audio**. The two-tone cue ducks and mixes
  into our existing Bluetooth PCM stream, including paused playback. Radio/AUX
  retain visual alerts; FM interruption is not implemented. Physical playback
  timing still needs vehicle verification.
- **Clock:** offline automatic timezone and DST from GPS, with a manual fallback.
  Settings → Clock: Enter toggles Auto/manual; Left/Right selects a manual offset
  in 15-minute steps. Rules span 2020–2060 and retain the last offset at boot.
- **Auto theme:** a custom daily schedule using local time. In Settings, choose
  Auto under Theme, then set **Day starts** and **Night starts**. Defaults are
  07:00 and 18:00. Scroll down to the schedule rows, use **OK** to select hours
  or minutes, and **Left / Right** to adjust the highlighted part. The times are
  saved and also apply to the boot splash. Day/Night overrides remain available.
  The unit clock and saved UTC offset provide the schedule before GPS is ready.
  With no valid clock, Auto retains the last theme, defaulting to Night.
- **Language:** 18 options in Settings, including Arabic, Simplified Chinese,
  Japanese, Korean, Hindi, Hebrew, Greek and Cyrillic languages. See
  [the complete language list](locales/README.md). Native destination and
  street names remain unchanged. Received UTF-8 names, Arabic/Indic shaping and
  bidirectional text run on the device. Fonts cover major scripts, not all Unicode
  or emoji. English uses “Bluetooth” for the page title; other languages retain their
  translated phone labels. The audio notice uses “BT” in every language.
  The naming keyboard provides independent uppercase/accented and native-script
  layouts, with left-soft backspace, right-soft space and a wide language chooser.
  See [keyboard controls and input methods](locales/README.md#favourite-keyboard).

## Remote controls

### Driving, map browsing and BT CTRL

| Button | Driving map | Map browsing | BT CTRL on the driving map |
| --- | --- | --- | --- |
| **MAP** | Restore Auto zoom after a temporary manual adjustment; otherwise enter map browsing. | Return to the driving view and the configured zoom mode. | Leave BT CTRL; restore Auto zoom if temporarily adjusted, otherwise enter map browsing. |
| **MENU** | Open Menu. | Open Menu. | Leave BT CTRL and open Menu. |
| **BACK** | Enter BT CTRL when Bluetooth audio is selected and a phone is connected. | | Leave BT CTRL. |
| **Centre / OK** | Show estimated remaining time and distance for about four seconds. With GPS and no active route, show the heading. | Open the route or add-stop prompt for the point under the crosshair. | Play or pause. |
| **Up / Down** | Enter map browsing and pan. | Pan up or down. | |
| **Left / Right** | Enter map browsing and pan. | Pan left or right. | Previous or next track. |
| **Left / right soft keys** | Zoom out or in. With Auto zoom enabled, this sets a temporary override. | Zoom out or in. | Zoom out or in on the driving map. |

With Auto zoom enabled, the soft keys temporarily adjust the driving zoom and
show **Manual zoom**. **MAP** restores Auto zoom without entering Explore; a
subsequent **MAP** enters Explore. Arrow keys can enter Explore and pan directly,
including from temporary manual zoom. **OK** continues to show the journey estimate
during guidance, including while the zoom is temporarily adjusted. The estimate
also appears when a route calculation or reroute finishes.

Explore shows one entry notice. Zooming and panning within Explore neither replay
that notice nor extend it. Leaving Explore shows **Auto zoom**, or **Manual zoom**
when Auto zoom is disabled in Settings. These mode notices share the usual glass
styling, transitions and temporary display time.

Confirming a point selected in Explore returns directly to the driving map with
the reverse rotation/tilt animation. The time/temperature pill remains visible;
calculation uses the existing five-frame-per-second cap. Declining keeps Explore.

### Menu, Trip and Bluetooth

- **Menu:** use **Up / Down** to choose Trip, Bluetooth or Settings. Press **OK**
  to open it or **BACK** to return to driving. From a page, **MENU** or **BACK**
  returns to Menu. **MAP** goes directly to the driving map.
- **Trip:** use the **left soft key** to focus destinations and the **right soft
  key** to focus the itinerary. In destinations, **Left / Right** switches
  between Recents and Favourites, and **Up / Down** moves through places.
  **OK** opens the place's actions or selects it for the current Add stop or
  Change destination task. In the itinerary, **Up / Down** moves through the
  route, and **OK** opens the selected stop or destination's actions, the
  Add stop choices, or the cancel confirmation. With an empty itinerary, choose
  **Send from phone** or **Coordinates**, then press **OK**. **BACK** or either
  soft key cancels an in-progress destination selection and returns to the itinerary.
- **Bluetooth:** use **Up / Down** to move between the connected phone, saved
  phones and **Pair new phone**. On the connected phone, **Left / Right** selects
  Disconnect or Forget, and **OK** runs it. **OK** on a saved phone opens its
  Connect or Forget options. Choose with **Left / Right** and confirm with **OK**.
  **BACK** or the **left soft key** closes those options. **OK** on **Pair new
  phone** starts pairing.

### Settings

Use **Up / Down** to highlight a row. The available actions depend on the setting.

| Setting | Left / Right | Centre / OK |
| --- | --- | --- |
| Brightness | Adjust in 10-point steps. | |
| Theme | Cycle Day, Night and Auto. | |
| Units | Cycle Auto, Metric and Imperial. | |
| Auto zoom | Toggle on or off. | |
| Camera alerts | Cycle Off, Visual and Phone audio. | |
| Clock | Set a manual UTC offset in 15-minute steps. | Toggle automatic and manual time. |
| Debug log | Toggle on or off. | |
| Language | Cycle backwards or forwards. | Select the next language. |
| Day starts / Night starts | Adjust the highlighted hour or minute. | Switch between editing hours and minutes. |

### Naming keyboard and dialogs

- In **Rename**, the **direction pad** moves between characters and actions.
  **OK** types the highlighted character or activates the highlighted action,
  including character pages, the language chooser and Save (or Next when naming
  a coordinate destination). The **left soft key**
  deletes, the **right soft key** inserts a space, and **BACK** returns to Trip
  without saving. In the language chooser, **Up / Down** moves one language,
  **Left / Right** jumps six, **OK** selects, and **BACK** or the **left soft key**
  closes the chooser. See [language-specific input](locales/README.md#favourite-keyboard).
- In option dialogs, **Up / Left** selects the previous option and **Down / Right**
  selects the next one. In Yes/No dialogs, use **Left / Right**. **OK** confirms,
  and **BACK** dismisses ordinary dialogs. At the saved-trip resume prompt, choose
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
[BUILDING.md](BUILDING.md), [third-party notices](THIRD_PARTY_NOTICES.md), and the
[hardware trial procedure](docs/next-release/hardware-trial.md).

Host tests cover the unsplit reference graph versus restricted two- and three-pack atlases,
including a long active via-way restriction,
an actual Hendaye–Irun France/Spain route, native text, cameras, clock transitions,
installer rollback, legacy remote screens, water/bridge layering and antialiasing.
Audio regressions check exact SBC arithmetic and folded synthesis, bounded
reads/copies, identical PCM, rate fallbacks, source changes, camera cues, idle
drain and maintenance timers independent of Bluetooth packet wakeups.
Country-scale France/Spain build status is recorded separately; downloaded inputs
are not evidence of finished builds. Geographic metadata is not a legal boundary
survey; Auto units retain a manual override. Dateline-spanning/planet extracts and
latitudes beyond ±85° remain unsupported.

**Vehicle validation remains open:** boot, physical remote, AUX/radio, Bluetooth,
frame timing and memory need the actual head unit. Use the concrete
[hardware trial procedure](docs/next-release/hardware-trial.md). The legacy
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
Outside calculation, a separate compositor targets approximately 15 FPS while overlays animate. It
restores the latest finished background before drawing each overlay frame, so
map updates can continue during an animation without showing partial roads or
retaining old glyphs. Two extra RGB565 backgrounds use about 510 KiB of RAM.
Quiet screens do not redraw at 15 FPS unnecessarily.

Frosted surfaces combine complete 8 × 8 backdrop areas with a separable
1–2–1 blend over neighbouring areas in both axes, softening detail over a
24 × 24 footprint. Three small rolling rows avoid a full-screen blur.
They retain fractional colour
precision through sampling, tint, interpolation and opacity. Full-strength,
screen-anchored 8 × 8 ordered dithering at the final RGB565 write reduces colour
banding in broad gradients, especially on large dark popups. It introduces fine
fixed grain; the pattern does not cycle between frames. Text and symbols are
drawn afterwards, and exact RGB565 colours such as selection blue stay exact.
Only a 256-byte constant table is added, without diffusion buffers or another
image pass. Samples are reused while the
finished background and pill geometry remain unchanged; overlapping cards and
speed-limit insets are sampled afresh.

All popup windows use this glass, including route confirmations, saved-place and
itinerary actions, phone prompts, Bluetooth actions and keyboard language choices.
Their entrances, exits, resizing, text changes and moving selection highlights
share the pills' independent 15 FPS animation clock (5 FPS during calculation).
Popup-only input does not force a new map frame. Actions take effect immediately;
copied popup content remains available while closing, including after deletion
or a page change. A settled popup returns to the normal presentation cadence.

Scrollable Recents, Favourites, saved-phone, Settings and keyboard-language lists
show a proportional scrollbar only when more rows exist than fit onscreen.
The thumb tracks the actual list size and visible window, including the
pairing action in the phone list. Popup scrollbars use translucent antialiased strokes over the glass
and share its entrance, exit and content fades.

Explore rotation and tilt follow a 500 ms elapsed-time transition rather than a
fixed number of map frames. Camera presentation requests additional map drawing
passes using the tile cache; these still cost CPU time. Normal map deadlines keep
running, and rotation never resets a pill's appearance or notification timer.
Outside calculation, long map drawing yields at safe points to present overlays.
Routing uses spare
deadline budget in chunks of at most 5 ms, with periodic cooperative clock checks.

These are scheduling targets, not measured frame rates on the 400 MHz head unit.
Slow SD reads or drawing can miss deadlines; missed frames are skipped rather than
replayed in a burst. With Debug enabled, five-second `FRAME 5S` summaries in the
existing `state.log` report presentations, scheduled map frames, extra camera
passes, lateness and drawing/routing time. No frame log is written with Debug off.

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

Renault's [2009 Grand Scénic announcement](https://media.renault.at/772/article/827)
lists an MP3-CD radio with AUX. Primary [radio hardware research](https://hackaday.io/page/20334-types-of-renault-radios-displays-protocols)
distinguishes basic R1 CD radios from R2 Bluetooth radios and their different
protocols. The surveyed evidence does not establish a factory AUX-only radio
without CD or Bluetooth in the target 2009–2012 non-LIVE range, so AUX is not used
as a heuristic for Bluetooth routing or radio capability.

Source/track notices remain temporary and use the shared glass animations in
navigation and menus. These paths have host checks; sound output and coexistence
on other radios still need vehicle trials. No automatic radio-source selection
or physical console control is implemented by these UI source rules.

### Decimal-degree destinations

**Trip → right soft key → Coordinates** opens latitude, longitude, optional name,
and review. **Add stop** and **Change destination** expose the same Coordinates
source. Arrow keys move around the numeric keypad; OK presses a key. **±** toggles
the sign, **.** adds the decimal separator, **Delete** removes the last character,
and **Clear** clears the current field. The left soft key deletes; the right soft
key advances a numeric field. On the name keyboard, the right soft key inserts a
space, and **Next** opens review. The existing keyboard-language selector works
for destination names. BACK keeps entered values while going back; MAP/MENU leave
without saving. Review offers the route/stop action, Edit coordinates, Edit name
and Cancel. No saved places or itinerary targets change until confirmation.

Latitude accepts −90…90, longitude −180…180; minus means south/west. Decimal
points use `.` in every interface language. Up to seven fractional digits can be
entered; the router stores 1e-5 degree integers and review displays the rounded
five-decimal values. Exact geographic bounds are checked before rounding. Parsing
uses bounded integer operations, without floating point or a geocoding service.
An empty/space-only name becomes the coordinate pair. Saved names use the existing
40-character Unicode limit and the existing Recents/favourites/trip formats.
Existing nearby-place deduplication applies to Recents. Coordinate entry does not
expand map coverage; normal navigation-data/routing errors still apply.

Phone destinations wait while this editor is open. Explicit coordinate
confirmation takes precedence over a phone destination received during entry;
cancelling leaves the received destination available to the existing phone flow.
Physical audio, controls and navigation behaviour still need the vehicle checks.
