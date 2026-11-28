# ClioPlay rebuild and release workflow

This guide is for anyone rebuilding ClioPlay from source, whether for personal
use, development, a contribution or an official release. It complements the
[build guide](../BUILDING.md), which explains compiler requirements, optional
navigation tools and feature-specific tests.

## Quick start: rebuild without maps

From the repository root (the folder containing `src/`), with the desired
source changes already applied:

```sh
cd src
python3 tools/rebuild_arm.py
python3 tools/verify_ci.py --host-only
```

The first command builds and installs the three ARM executables into
`src/clio843/` (relative to the repository root). The second runs local
regression checks without requiring a current publication package.

**No specific map build, map download, vendor firmware, GIF regeneration or
publication refresh is needed to compile and run these local checks.** You do
not need to change the version, commit or push anything for a personal rebuild.
A rebuilt executable is not automatically deployed to a head unit or SD card. To deploy current programs to an existing compatible card, use `python3 install.py --update /path/to/card` from the repository root; see the [software update guide](updating.md).
For installation and runtime requirements, see the [user guide](../README.md).
Existing current-format navigation packs can be reused; only obsolete packs
need rebuilding. See the [navigation builder](../navigation-builder/README.md)
for the supported formats.

Stop at the first failed command and resolve the error before proceeding.
Commands below use `src/` as their working directory unless otherwise stated.

## 1. Prepare the build environment

- Use Linux, WSL or macOS with Python 3.12+, Git, ARM-capable Clang, Clang++,
  LLD (ELF linker), an ARM-compatible archive tool and a host C/C++ compiler.
- Use a complete source checkout or extracted source package. If using Git,
  review `git status` first and preserve any existing local changes.
- Apply source changes while preserving their relative paths. Edit canonical
  source and documentation files in `src/`, not generated root copies.
- Keep private firmware, GPS driving histories and learned models, map data, credentials, local captures and downloaded
  change archives outside the published repository payload.

On macOS, the build guide recommends Homebrew:

```sh
brew install python llvm lld
```

Ensure the compiler tools are on PATH. The first Unicode-engine build may
download pinned, SHA-256-checked Debian cross headers. See the
[build guide](../BUILDING.md) for compiler and dependency details.

Pillow is only needed for GIF generation, not for the normal ARM rebuild or
host-only regression checks. If generating GIFs, install it in your chosen
Python environment (an isolated environment outside the checkout is recommended):

```sh
python3 -m venv "$HOME/.venvs/clioplay-release"
source "$HOME/.venvs/clioplay-release/bin/activate"
python3 -m pip install Pillow
```

Use that same environment for subsequent commands.

The three intelligent navigation previews use controlled fixtures. The full
GIF regeneration command includes all three. See [intelligent navigation](intelligent-navigation.md) for testing and privacy requirements.

## 2. Check the version and rebuild

From `src/`:

```sh
cat VERSION
python3 tools/release_version.py --check
python3 tools/rebuild_arm.py
```

The rebuild uses the existing version by default. It builds, validates and
installs `clio040`, `cliospl` and `clioa2dp` in `clio843/` and saves build reports
in `build/`. Look for the three executable verification lines and
`Rebuild successful`.

Useful options:

- `--dry-run`: show the intended actions without modifying files.
- `--build-only`: build into `build/` without replacing installed executables.
- `--clang` and `--linker`: supply actual compiler/linker paths if discovery
  fails. Matching Clang++ and archive tools must also be available.

Do not use `--publish` for a normal local rebuild. Publication is a separate
operation and requires current release artifacts.

If deliberately preparing a new version (rather than recompiling an existing
one), replace `X.Y.Z` with the intended version before rebuilding:

```sh
python3 tools/release_version.py --set X.Y.Z
python3 tools/release_version.py --check
```

This synchronises `VERSION`, the native release header, bootstrap scripts and
their SHA-256 checksums. It does not rewrite prose documentation. Alternatively,
`python3 tools/rebuild_arm.py --version X.Y.Z` sets the version and rebuilds in
one command. There is no need to do both. Do not bump an already-versioned
change package again.

## 3. Run local regression checks

```sh
python3 tools/verify_ci.py --host-only
```

This includes release-version checks, package/bootstrap unit tests, native
host regression tests and physical-console input-record simulations. No map
extracts, firmware, device, network or third-party Python modules are needed
for the host checks themselves. The complete publication checks are skipped.

Review any feature-specific tests in [BUILDING.md](../BUILDING.md) as needed.
Some optional navigation tests require compatible map data; they are not
prerequisites for the ordinary rebuild.

A successful host test is not physical-hardware validation. Test the intended
changes on a device where practical, particularly timing, audio, Bluetooth,
ignition, power, GPS and performance. Follow the installation instructions
before transferring programs to an SD card.

**For a local rebuild, this is the end of the required workflow.** The remaining
sections apply only if you want to generate previews or prepare a publication.

## 4. Optional: regenerate demo GIFs

For development previews without map assets:

```sh
python3 tools/regenerate_gifs.py
```

This uses the live C renderer to regenerate a subset of GIFs under `docs/demo/`.
It requires Pillow and a host C compiler. Review the changed previews.
It does not regenerate the complete set or update full publication provenance.
See the [demo catalogue](demo/README.md).

For maintainers who need to regenerate the complete published preview set (or
quickly re-render one or more individual GIFs during development):

```sh
python3 tools/regenerate_gifs.py --maps /path/to/your/maps
python3 tools/regenerate_gifs.py --maps /path/to/your/maps --only v223-view-diversion
python3 tools/regenerate_gifs.py --maps /path/to/your/maps --list
```

Passing `--only` (or `--gif`, with or without `.gif`) renders only the scenes
and themes required for those previews and merges their updated entries into
the existing `docs/demo/manifest.json`.

Replace the placeholder with the location of an existing compatible built map
bundle, directly containing `build.json`, `fonts.cpf` and navigation data.
Full generation also requires the native text-engine dependencies. The current
published scenarios use fixed geographical locations and routes; map coverage
must match those scenarios. An arbitrary regional map is not guaranteed to
work. See the [build guide](../BUILDING.md) for the current demo requirements.
These are requirements of the full preview generator, not of ClioPlay's ARM
build or ordinary local testing. Do not build or download maps solely to
compile ClioPlay.

Optionally supply `--phone-capture /path/to/browser/captures` alongside
`--maps` to replace the browser panel in the ClioPlay Link demo. Otherwise the
existing published panel is reused. Keep private captures outside the checkout
and check the resulting images for personal information.

A complete successful run regenerates the published GIF set and its provenance
manifest. If the runtime or preview-generator inputs have changed, a mapless
preview run is not a substitute for the complete published set. If compatible
maps are unavailable, continue local development but leave the affected
publication pending; do not manually change provenance hashes.

## 5. Optional: prepare a release publication

For maintainers preparing a runtime release, update the
[canonical changelog](changelog.md), [README template](repository-readme.md),
[user guide](../README.md) and any other affected documentation. Finish the
version, rebuild and full preview steps above before refreshing the publication.
For documentation-only changes, if the existing binaries and previews are
already current, skip the rebuild and preview steps.

```sh
python3 tools/refresh_latest.py
python3 tools/refresh_latest.py --check
python3 tools/verify_ci.py
```

In a portable Git checkout (with `latest.json` in the parent directory), the
exporter refreshes the repository root in place. It exports managed sources,
current ARM binaries/reports, GIFs, generated root documentation, `SHA256SUMS`,
`latest.json` and other delivery files. It may preserve unrelated local files,
but verification rejects unexpected Git candidates. For a standalone source
layout without a parent `latest.json`, see [BUILDING.md](../BUILDING.md) for the
separate delivery directory workflow.

The full CI check includes publication integrity, documentation links,
source/binary and GIF provenance, plus the host regression suite. To run the
publication verifier separately:

```sh
python3 tools/verify_publication.py
```

Never edit generated manifests, root README/changelog or checksums to conceal
stale inputs. Re-run the affected generation steps instead. A publication is
ready only when the complete checks pass; record any outstanding physical
hardware testing separately.

### Which steps need repeating?

| Change since the last successful build/release | ARM rebuild | Full GIF generation | Publication refresh and full checks |
| --- | --- | --- | --- |
| Documentation only | No | No | Yes, if publishing |
| Runtime source or release version | Yes | Yes, if publishing | Yes, if publishing |
| Preview generator or demo content only | No, unless build inputs also changed | Yes, if publishing | Yes, if publishing |
| Publication tooling or packaging metadata only | No, unless build inputs changed | No, unless preview inputs changed | Yes, if publishing |

The table assumes unaffected artifacts were already current. Always repeat any
stage whose recorded inputs have changed. For local-only development, run
relevant host checks and skip publication.

## 6. Optional: review, commit and push

For contributors and maintainers working in a Git checkout, return to the
repository root and inspect your changes:

```sh
cd ..
git status --short
git diff --stat
git diff --check
git diff
```

Review new files as well as diffs (untracked files are not shown by `git diff`).
Stage only the intended changes, including managed generated files and intended
deletions for a publication. Replace the example path with the files to stage:

```sh
git add -- path/to/reviewed/file
```

If every change in the working tree has been reviewed, `git add -A` is an
alternative. Before committing:

```sh
git diff --cached --stat
git diff --cached --check
git status --short
```

Use a concise commit message describing the actual change. For a versioned
release, include its version (replace `X.Y.Z` and the description):

```sh
git commit -m "vX.Y.Z - Describe the changes"
git branch -vv
```

Push only if you intend to share the commit and have write access to the
configured upstream (for example, your own fork):

```sh
git push
```

If no upstream is configured, choose the intended remote and branch explicitly.
Git tags and hosted releases are separate, optional maintainer operations.

## Troubleshooting

| Problem | What to do |
| --- | --- |
| Python version or missing compiler | Use Python 3.12+ and the tools in the build guide. |
| Version labels or bootstrap checksums stale | Run `python3 tools/release_version.py --sync`, then repeat affected stages. |
| Cross-header download or checksum failure | Check network access and the pinned download; do not bypass verification. |
| Missing `build.json` during full GIF generation | Point `--maps` at the built map directory with suitable coverage, or skip full previews for local development. |
| Stale preview source or executable hashes | Rebuild if needed, then regenerate full previews before publishing. |
| `Changed source; compile and install first` | Repeat the normal ARM rebuild, then refresh any affected previews before publishing. |
| `Project changed; refresh the latest folder` | Run the affected generation stages and refresh the publication. For local tests, use `--host-only`. |
| `Git publication differs from managed files` | Inspect missing/unexpected paths; keep private files outside the checkout or register intended public paths in `tools/publication.py`. |
| Broken documentation link | Correct canonical Markdown or register its intended target, then refresh publication. |
| Host test failure | Read the failing test, fix the cause and rerun affected checks. Do not weaken checks to force a pass. |
