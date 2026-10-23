# ClioPlay Global 1.1.0-rc.2

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
- **Language:** 18 options in Settings, including Arabic, Simplified Chinese,
  Japanese, Korean, Hindi, Hebrew, Greek and Cyrillic languages. See
  [the complete language list](locales/README.md). Native destination and
  street names remain unchanged. Received UTF-8 names, Arabic/Indic shaping and
  bidirectional text run on the device. Fonts cover major scripts, not all Unicode
  or emoji. English uses “Bluetooth” for the page title; other languages retain their
  translated phone labels. The top-bar source badge uses “BT” in every language,
  with the playback symbol on the same centred row. The favourite keyboard provides independent uppercase/accented and native-script
  layouts, with left-soft backspace, right-soft space and a wide language chooser.
  See [keyboard controls and input methods](locales/README.md#favourite-keyboard).

## Framebuffer showcase

[Watch the showcase](docs/demo/index.html) or [download the MP4](docs/demo/clioplay-demo.mp4).
The main presentation uses Night mode and demonstrates everyday remote controls,
Trip and favourites, Bluetooth/audio, useful settings and the actual ClioPlay
Send workflow. Day/Auto themes appear briefly. The video uses short descriptions
and a remote drawing with synchronized key highlights. See the
[chapters and regeneration notes](docs/demo/README.md).

These are independent host examples with controlled sample state. The Sender
publishes real browser metadata, with the Bluetooth radio hop simulated;
physical head-unit timing, audio and wireless operation need hardware validation.

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
