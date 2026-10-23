# Third-party notices

The project MIT licence applies to original ClioPlay contributions only. It does
not relicense dependencies, fonts, map databases or vendor firmware.

- HarfBuzz **10.4.0**: `clio843/src/vendor/harfbuzz/COPYING`, MIT-style licences
  including retained generated-table notices. Source: https://github.com/harfbuzz/harfbuzz.
- SheenBidi **3.0.0**: `clio843/src/vendor/sheenbidi/LICENSE`, Apache-2.0.
  Source: https://github.com/Tehreer/SheenBidi. Local single-thread patch in
  `Source/API/SBBase.h` avoids unavailable ARMv4T atomics; build disables scratch memory.
- LLVM compiler-rt builtins **19.1.7**: `clio843/src/vendor/compiler-rt/LICENSE.TXT`,
  Apache-2.0 with LLVM exception. Only required arithmetic sources are compiled.
- Existing BlueZ-derived SBC decoder in `clio843/src/clioa2dp.c`: LGPL-2.1-or-later.
  Full source is included; licence text is in `fonts/LGPL-2.1.txt`. The independently
  built daemon and its complete source/build command remain replaceable.
- Bitstream Vera: `fonts/LICENSE.txt`; retained font files and map-label atlas.
- Noto fonts: SIL OFL 1.1, `navigation-builder/data/OFL.txt`. Individual source URLs,
  hashes, copyrights and licence notices are in `global-fonts.cpf.json` and
  `font-notices.json`. `fonts.cpf` is a derived bitmap/layout-table collection.
  Font names in the directory identify provenance; they do not imply endorsement.
- Natural Earth country polygons: public-domain data from
  https://github.com/nvkelso/natural-earth-vector/blob/master/geojson/ne_10m_admin_0_countries.geojson.
  Terms: https://www.naturalearthdata.com/about/. Bundled source SHA-256:
  `239eec57ac17f100a11e2536cffc56752c318b50ae765b0918ff7aab4ce8f255`.
- OpenStreetMap/coastal data: © OpenStreetMap contributors, ODbL.
  https://www.openstreetmap.org/copyright and https://opendatacommons.org/licenses/odbl/.
  Derived navigation databases retain these obligations; optional imported camera
  sources retain their own terms. Runtime attribution remains in the menu.
- Desktop Python dependencies are fetched by the builder, not vendored or linked
  into the head unit.
  Pyosmium, Shapely, pyshp, fontTools and freetype-py retain their upstream licences.
- Debian armel development headers are automatically fetched as build tools with
  pinned SHA-256 hashes. They are not shipped in release archives; no Debian libc
  or libstdc++ binary is linked into the freestanding output.

- Country context/traffic profiles include ISC country-coder and BSD-3-Clause
  osm-default-speeds snapshots; their licenses and source hashes are in builder
  `data/`. Context values are evidence-based estimates, not a worldwide legal audit.
- Offline timezone polygons derive from timezone-boundary-builder 2026e (ODbL),
  simplified .0005 degrees, with source/bundle hashes and the ODbL text in `data/`.
  IANA timezone transitions use pinned `tzdata==2026.5`; that Python package is a
  desktop builder dependency, not a linked device library.
- The Spain country refinement derives from Instituto Geográfico Nacional via
  geoBoundaries ESP ADM0, pinned commit 9469f09, simplified .00005 degrees.
  Attribution: Instituto Geográfico Nacional / geoBoundaries, CC BY 4.0.
  Full metadata, hashes and `CC-BY-4.0.txt` are in builder `data/`. Other country
  geometry remains the public-domain Natural Earth fallback.
- The project legacy ARM `hidd` was identified by the earlier audit as BlueZ
  2.15-based. BlueZ upstream sources, GPL-2.0 notices, hashes and build notes
  accompany it in `third_party/bluez-2.15/`. The maintainer identifies the project
  helper as their work; their original project build tree is not recovered.
  Exact downstream correspondence remains unconfirmed.

Public ZIPs exclude TomTom `ttsystem`, `PNDNavigator`, `vehicledaemon`, the vendor
CAB, navigation packs, personal data, bond stores and caches. New-card preparation
downloads the official CAB automatically and verifies the supported firmware locally. The default project startup
scripts, legacy focusd and embedded AUX helper are retained as existing ClioPlay
bootstrap artifacts; their original helper C sources are unavailable. They are
not claimed to be rebuilt or reverse-engineered equivalents. See
`docs/source-status.md` and `legacy-binaries/manifest.json` for roles and hashes.

The offline Pinyin character table derives from Unicode 17.0.0 Unihan
`kMandarin`/`kHanyuPinlu` readings (Unicode, Inc., Unicode-3.0). The pinned
[archive](https://www.unicode.org/Public/17.0.0/ucd/Unihan.zip), property/range
metadata and checksums are recorded in `clio843/src/keyboard-data/pinyin.json`;
the full licence is in `clio843/src/keyboard-data/UNICODE-LICENSE.txt`.
The modern Hangul composer follows Unicode's algorithmic syllable model:
[Unicode 17, Chapter 3](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/).
