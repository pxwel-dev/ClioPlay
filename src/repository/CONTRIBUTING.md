# Contributing

Edit canonical files in `src/`. The root README, installer entry points and
repository metadata are generated; their sources are in `src/docs/`,
`src/tools/` and `src/repository/`.

Run the dependency-light host checks from the repository root:

```sh
python3 src/tools/verify_ci.py
```

These need Python 3.11+, Git and a C compiler. They use temporary, synthetic
fixtures; no head unit, downloaded firmware, map extract or Node.js is needed.
They check publication integrity, installer behaviour, map rasterization and
remote navigation logic. They do not validate physical hardware.

After changes, refresh the maintained package and rerun the checks:

```sh
python3 src/tools/refresh_latest.py
python3 src/tools/verify_ci.py
```

Runtime changes first need current ARM programs and a matching native demo.
Follow [BUILDING.md](src/BUILDING.md) for that sequence and the wider navigation,
Unicode and hardware tests. Never hand-edit checksums or build reports to make
a stale binary pass.

Keep changes focused. Describe the trigger, resulting behaviour and checks in
pull requests. For bugs, include the build checksum, head-unit model, reproduction
steps and whether the result was observed on a real unit or a host fixture.
Remove precise home locations, pairings and other personal data from reports.

Do not contribute vendor firmware, CABs, generated maps, credentials or personal
settings. Retain upstream notices. The separately maintained phone website is
outside this repository's scope.
