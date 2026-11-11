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
  The favourite keyboard provides independent uppercase/accented and native-script
  layouts, with left-soft backspace, right-soft space and a wide language chooser.
  See [keyboard controls and input methods](locales/README.md#favourite-keyboard).

## Remote controls

### Driving, map browsing and BT CTRL

| Button | Driving map | Map browsing | BT CTRL on the driving map |
| --- | --- | --- | --- |
| **MAP** | Enter map browsing. | Return to the driving view. | Leave BT CTRL and enter map browsing. |
| **MENU** | Open Menu. | Open Menu. | Leave BT CTRL and open Menu. |
| **BACK** | Enter BT CTRL when Bluetooth audio is selected and a phone is connected. | | Leave BT CTRL. |
| **Centre / OK** | Show estimated remaining time and distance for about four seconds. With GPS and no active route, show the heading. | Open the route or add-stop prompt for the point under the crosshair. | Play or pause. |
| **Up / Down** | Enter map browsing and pan. | Pan up or down. | |
| **Left / Right** | Enter map browsing and pan. | Pan left or right. | Previous or next track. |
| **Left / right soft keys** | Zoom out or in. With Auto zoom enabled, this sets a temporary override. | Zoom out or in. | Zoom out or in on the driving map. |

After a manual zoom adjustment with Auto zoom enabled, **OK** on the driving
map restores Auto zoom first. A second press shows the journey estimate during
guidance. The estimate also appears when a route calculation or reroute finishes.

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
  Add stop choices, or the cancel confirmation. With an empty itinerary, **OK**
  opens the prompt to receive a destination from your phone. **BACK** or either
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

### Favourite keyboard and dialogs

- In **Rename**, the **direction pad** moves between characters and actions.
  **OK** types the highlighted character or activates the highlighted action,
  including character pages, the language chooser and Save. The **left soft key**
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
