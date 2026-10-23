# ClioPlay Global navigation builder

Build a complete region from **one original `.osm.pbf`**, without supplying old
ClioPlay packs, overlays, separate boundary files, fonts or a postal database.
Filled inland water/greenery and processed coastal water/estuaries are included.
Roads are drawn above all fill layers.

## Setup and guided build

Use Python 3.11+, osmium-tool and a C++17 compiler (`c++`). On macOS:

```sh
xcode-select --install
brew install python osmium-tool
```

On Ubuntu 24.04+ / WSL 2:

```sh
sudo apt install python3 python3-venv osmium-tool g++
```

From this directory run:

```sh
python3 build_navigation.py
```

The sequence is PBF path → scan actual contents/bounds → show intersecting
countries and ask confirmation → build/inspect all outputs. Automatic dependency
setup creates `.venv` when needed. Dragging a file into a macOS terminal is supported.
The country check verifies intersection, not completeness of country coverage.
Extracts above 350 MB automatically become a regional atlas; `--atlas` forces it and `--single-region` disables automatic splitting.

For unattended use, provide the expected country code:

```sh
python3 build_navigation.py /path/to/spain.osm.pbf --yes --country ES --output /path/to/output
```

For a cross-border extract repeat `--country` for each expected country. An absent
expected country aborts before map construction. `--work PATH` reuses scratch
space and `--cache PATH` reuses coastline downloads. Output/work must be separate.
The builder prints both locations; diagnostic reports stay in scratch. The output
contains only the installable files and their `build.json` manifest.

## Outputs

For a single region, keep these seven files together with their `build.json` SHA-256 manifest:

| File | Purpose |
|---|---|
| `map.cpm` | CPM5 display map, 32-bit tile coordinates, native road names, directional limits, filled water/greenery/ocean |
| `route.cpr` | CPR2 version 5 graph, turn restrictions and contraction hierarchy |
| `route.qadj` | Exactly matching QAD7 acceleration/reconstruction packets |
| `regions.cpg` | Offline country polygons, Auto units and camera policy flags |
| `cameras.cpc` | OSM/import fixed, red-light and supported average-speed records |
| `fonts.cpf` | Bundled 23 MB native-script glyphs and shaping tables |
| `timezones.cpt` | Offline GPS timezone/DST polygons and 2020–2060 transitions |

**Use the Global executable. Older ClioPlay versions cannot read the new map/route
formats.** `route.qadj` is specific to the exact `route.cpr`; never mix builds.
The wizard in the parent folder installs the executable and complete data set
with staging, checksum verification and backups. Upgrading an older Global card
also backs up and removes its unused search indexes. Alternatively:

```sh
python3 ../tools/prepare_card.py /path/to/prepared-runtime
.venv/bin/python tools/install_nav.py /path/to/output /Volumes/SDCARD --runtime /path/to/prepared-runtime
```

## Data, scale and coverage

The first build automatically downloads processed WGS84 coastal-water polygons
from [OSM water polygons](https://osmdata.openstreetmap.de/data/water-polygons.html)
(roughly 865 MB compressed, about 2 GB cache including extraction). This closes
sea and tidal river-mouth areas that cannot be derived from a regional coastline
alone. Inland geometry comes from the PBF. Missing banks/multipolygons cannot be
invented; centre-line-only rivers remain lines. Source coast/PBF dates can differ.

Allow substantial RAM, disk and time for country builds (scratch data can be
many times the PBF size). Each pack must remain **below 2 GiB** for the head unit's
signed seek ABI. Planet/date-line-crossing extracts and latitudes beyond ±85° are
rejected. Complicated area tiles are paged/clipped with holes retained; overflow
fails the build instead of silently publishing incomplete geometry.

Explicit OSM speed limits preserve native km/h or mph. Directional limits and
identical lane limits are supported; differing lane limits are unknown. Conditional
or variable restrictions show `VAR`; the device does not evaluate weather/time/
vehicle conditions. Explicit supported national context tags produce estimates
marked `?`. Untagged urban streets also have a runtime fallback: a network of at
least five distinct named local streets within about 300 m establishes urban
context. Three or more independently named streets must agree on a mapped limit
(at least 75% of the available numeric evidence), or an unambiguous bundled urban
country profile supplies the estimate. Conflicting nearby limits veto the country
fallback. Duplicate tile fragments count once; estimates never feed further
estimates. Local consensus works globally, including countries without a single
national urban default. The country-only fallback currently covers 75 countries;
Spanish street-configuration rules and differing UK/subnational defaults are not
collapsed into one guessed value.

Every automatic result retains `?`, including after unit conversion. Mapped or
confirmed limits take priority. Unknown country/context, sparse road networks,
conflicting evidence and unsupported restrictions can still show `--`. No rural,
motorway or ramp limit is guessed from the display road class. New map builds
preserve an explicit no-estimate flag for unsupported speed tags, living streets,
service roads and wrong-way directions. Older CPM5 data lacks some of these
qualifiers; a missing local zone or restriction can still make an estimate wrong.
Follow posted signs. Routing travel speeds remain separate estimates.

For verified missing limits, an optional source-attributed JSON import works with
global OSM way IDs (no street-name or country-specific patches):

```json
{
  "schema": "clioplay-speed-import-1",
  "source": "Survey or published dataset reference",
  "observed": "2026-10-21",
  "ways": {
    "123456": "30 mph",
    "234567": {"maxspeed:forward": "50", "maxspeed:backward": "30"}
  }
}
```

Bare numbers use km/h. Only missing evidence is filled; existing numeric,
conditional, lane and directional tags are preserved. Pass `--speed-import
/path/to/limits.json` to `build_navigation.py`, or to the top-level `install.py`.
Atlas child builds receive the same import and its contents invalidate the build
cache when changed. Import identity and coverage are recorded in build manifests;
no extra file is required on the card.

An existing CPM5 bundle can be corrected without rebuilding map geometry or
routing. Use the matching source PBF and an empty, separate backup directory:

```sh
.venv/bin/python tools/apply_speed_import.py --maps /path/to/bundle \
  --pbf /path/to/source.osm.pbf --speed-import /path/to/limits.json \
  --backup-dir /path/to/new-backup
```

The tool requires exact source-way geometry and original metadata matches,
validates affected maps, preserves unrelated metadata/geometry, updates checksums
and backs up changed maps/manifests. Copy the changed regional `map.cpm` files and
`build.json` to an existing installation together. Keep the import for future
fresh builds. From `src/`, run `navigation-builder/.venv/bin/python tools/verify_speed_estimates.py`
and `navigation-builder/.venv/bin/python tests/test_speed_import.py`.

Country unit/driving-side profiles cover 260 territories; metadata and upstream
licenses/hashes are bundled. Explicit mph road evidence informs Auto only when country resolution is unavailable.
657 supported explicit country context tags are in `data/speed-contexts.json`;
reviewed overrides remain in `tools/global_speeds.py`. Country polygons are
Natural Earth with a higher-resolution IGN/geoBoundaries Spain refinement.
These are not legal border surveys; a manual unit override remains available.

Native names are NFC-normalized and retained in the map. Destination searching
is handled by the phone sender. No offline search index is generated. The remote
keyboard remains Latin and is
only used to name favourites. Street/map labels are bounded to 28 code points;
phone destination/place names to 40, with complete UTF‑8 character boundaries.

## Camera imports

OSM cameras are included by default. Add one or more UTF‑8 CSV files:

```sh
python3 build_navigation.py /path/to/spain.osm.pbf --camera-import /path/to/cameras.csv
```

```csv
id,latitude,longitude,type,maxspeed,heading,end_latitude,end_longitude,osm_way_id
example-fixed,40.4168,-3.7038,fixed,50,90,,,
example-zone,40.42,-3.71,average,80,180,40.40,-3.71,
```

These example coordinates are synthetic, not camera claims. Types: `fixed`,
`red_light`, `average`. A bare maxspeed is km/h; `30 mph` is supported. Heading
is affected **traffic** direction, clockwise from north; leave blank if unknown.
Average cameras require valid entry and exit coordinates. Optional `osm_way_id`
associates an imported camera with a known OSM road; blank leaves geometric matching. OSM device-facing
headings are converted to traffic headings. Ambiguous relations, missing endpoints
are skipped and reported. Average `section` way chains are supported when continuous
and directionally unambiguous; branching, missing or undirected chains are not invented. Imports undergo the same policy filtering and deduplication as OSM.

Camera policy is fail-closed: information-only alerts are enabled in 19 reviewed
countries; FR/DE/CH/CY are blocked and other countries stay disabled pending review.
See the [country list and review evidence](../docs/camera-support.md).
Imports use the same policy. Both average endpoints and a conservative .1° buffer
around restricted borders are checked. Runtime country checks suppress again.
Sources and review status are in `data/camera-policy.json`. The database remains
offline and incomplete. CPC1 v2 retains original OSM road identity, matched against
the active CPR route to reject a different bridge/parallel road. Off-route matching
is still geometric. Average tracking requires an observed entry and cancels on
GPS loss. Off / Visual / Phone audio controls alerts; sound uses our selected Bluetooth
PCM source and does not interrupt radio or AUX.

## Regional atlas

The output contains `atlas.cpa`, shared region/timezone/font/camera data and one
folder per regional CPM/CPR/QAD pack. Install the **whole manifest**, not an
individual region. All files remain below the signed 2 GiB device seek limit.
Cores align to complete Z0 tiles. Display tiles read their canonical owner;
coastal shared-ocean levels retain the same ownership and draw under roads.
Overlapping extracts include full restriction relations. Cross-pack transfers use
shared original OSM default states and actual CH paths, never straight connectors
or resetting active arrival restrictions. The native planner shares upward CH
endpoint searches across gateways in small slices, retains the user's final target and treats region gateways
as internal routing points. Remaining global distance/time includes future legs.

CH portal states contract after interiors. Stopping an endpoint sweep at its
first portal is exact because CPA1 exports all directed CH arcs between these
protected portal states. Target-side reverse propagation computes full regional
costs in destination-candidate order, including one-way/restriction fallback.
Its temporary reverse index uses 10 bytes per target-region overlay edge and is
released before route reconstruction; query state still uses the bounded arena.
The catalog and QAD7 formats are unchanged; existing verified packs need no rebuild.
CPN1 is a build-side identity index;
CPA1 is the device overlay. Boundary cuts without a safe endpoint are reported,
so universal whole-country optimality is not implied by the small passing fixtures.
A PBF must cover the intended journey; independent finished country atlases cannot
be merged on the SD card. Use one cross-border extract (for example a suitable
regional/continental source) when building multi-country coverage.

Shared fonts and country/timezone data are prepared once for the whole atlas,
not repeated for every region. Atlas builds release bulky per-region SQLite and geometry scratch after publishing
each region. Completed-region caches are builder-fingerprinted; source metadata
caches are SHA-256 keyed. Very large atlases can still exceed catalog/RAM limits and
fail explicitly. Enough disk is required for scratch, outputs and card staging.

OSM and imported camera datasets retain their own licences. Natural Earth boundaries are public domain; Noto font notices
and OFL are in `data/`. The parent README describes the current release limitations.
