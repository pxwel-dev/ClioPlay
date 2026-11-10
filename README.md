<!-- Generated from src/docs/repository-readme.md by src/tools/refresh_latest.py. -->

<h1 align="center">CLIOPLAY 2.0.0</h1>
<p align="center">Offline navigation and Bluetooth audio for the original Carminat TomTom screen and remote.</p>

<p align="center"><a href="#install">Install</a> · <a href="https://pxwel-dev.github.io/ClioPlay-Send/">Send a destination from your phone</a></p>

## UI previews

| Driving and camera alerts | Trip, favourites and settings |
| --- | --- |
| ![Driving with a temporary road name and a camera warning](src/docs/demo/driving.gif) | ![Opening Trip, browsing favourites and switching themes](src/docs/demo/menus.gif) |

| Bluetooth playback and volume | Receiving a phone destination |
| --- | --- |
| ![Temporary volume notice and Bluetooth playback controls](src/docs/demo/bluetooth.gif) | ![Receiving a destination from the phone on the head unit](src/docs/demo/phone-destination.gif) |

## Compatibility

This preview release of ClioPlay currently only supports vehicles that have the following:

- Non-Bluetooth radio with a CD player
- Non-live Carminat TomTom head unit
- Bluetooth remote control used to operate the Carminat

## Features

- Offline OpenStreetMap navigation, routing and rerouting.
- Browse the map, choose destinations, manage stops, recents and favourites.
- Bluetooth phone audio with track information and remote playback controls.
- Steering wheel audio controls.
- Full-screen maps with animated, frosted notices for road names, camera alerts, route progress and audio changes.
- Day/Night themes with seasonal sunrise/sunset in Auto, automatic units and time zones, and 18 interface languages.
- Mapped speed limits and camera alerts where the bundled country policy allows them.

For new stops/destinations that aren’t already in ClioPlay’s recents/favourites list, use the separate [ClioPlay Send website](https://pxwel-dev.github.io/ClioPlay-Send/) to send the new route to ClioPlay from your phone via Bluetooth. For ease of use, add the website to your Home Screen as a web app. 

## Install

You need an SD card for the head unit (minimum 8GB, ensuring that it’s already formatted to FAT32) and an `.osm.pbf` map extract covering your
journeys. Get the map extract from [Geofabrik Downloads](https://download.geofabrik.de/)
by choosing your region and downloading its `.osm.pbf` file. Download and extract
this repository, then follow your desktop setup:

| Computer | Start here |
| --- | --- |
| **Windows 10/11** | Follow [Windows setup](WINDOWS.md), then double-click **Install.cmd**. Uses Ubuntu 24.04 through WSL 2. |
| **macOS** | Install `python`, `osmium-tool`, `llvm` and `cabextract` with Homebrew, then open **Install.command**. |
| **Linux (Ubuntu 24.04)** | Install `python3 python3-venv python3-pip build-essential osmium-tool cabextract` with `apt`, then run `python3 install.py`. |

1. Choose your PBF and confirm its region.
2. Let the installer build and check navigation data.
3. Select the SD-card root and confirm installation.
4. Safely eject it before use and insert into your car. 

The installer downloads and checks the required official TomTom firmware.
It does not format the card. Replaced files are backed up. Any existing ClioPlay preferences,
pairings and saved places are preserved. No previous ClioPlay install is needed.
Map building can take substantial time for large extracts.

## Use your remote

| Control | Action |
| --- | --- |
| **MAP** | Switch between driving and map browsing |
| **Direction pad** | Browse the map or move through menus |
| **Soft keys** | Zoom the map; follow the on-screen actions in menus |
| **Centre / OK** | Select or confirm |
| **MENU** | Open the menu for Trip, Bluetooth and Settings |
| **BACK** | Return; toggle phone playback controls on the driving screen |

See the [user guide](src/README.md) for trips, pairing and settings.

## Status and development

Some legacy helper sources remain unavailable: see [source status](SOURCE_STATUS.md).
Original ClioPlay contributions use the [MIT licence](LICENSE); dependencies and
map data retain their [own licences and notices](THIRD_PARTY_NOTICES.md).
ClioPlay is independent of Renault and TomTom.

For development, see [contributing](CONTRIBUTING.md) and [building](src/BUILDING.md).

Firmware, map extracts, generated navigation, personal settings and build caches
are excluded from Git. The [UI previews](src/docs/demo/README.md) use the
production renderer with controlled host inputs; it does not establish vehicle performance.

<sub>Release files refreshed 10 November 2026 (Europe/London).</sub>
