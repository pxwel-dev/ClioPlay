# Version history

This history records implemented changes. Entries marked **Unreleased** are prepared locally and have not been published.
Release dates are added when a version is published; older versions are not reconstructed from incomplete notes.

## Version 2.2.0 — Unreleased

### Changes in this version

- Introduced optional, offline driving-history learning. Fresh GPS observations are recorded independently of active navigation and only successful Barcelona power-off sessions train the model.
- Added contextual destination suggestions to an empty Itinerary using the last saved GPS position, raw GPS time/day patterns and early trajectory evidence, with nearby Favourites/Recents matching.
- Added directional historical road-time profiles, traffic-adjusted ETAs and an explicit local Diversion choice with gold/yellow bypassed geometry and an aqua/teal alternative. Existing map formats and static CH/QAD7 shortcut weights remain unchanged.
- Shared route geometry remains magenta; dashed teal alternatives, yellow bypasses and junction markers distinguish proposed changes. A small centred frosted-glass map badge shows the approximate saving near the decision point; the glass HUD announces the offer with context-sensitive Back/OK instructions; direct map confirmation preserves Bluetooth controls and driving zoom.
- Diversion invitations use a persistent single line with the established success green announcement and blue context-sensitive review instruction, until acceptance, rejection or expiry.
- Declining a diversion suppresses only its bypassed section; later independent offers remain available, and diversion confirmation initially selects Yes. Restored the original empty-Itinerary instructions alongside learned destinations.
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
