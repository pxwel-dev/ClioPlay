"""Select the portable publication source tree, without local build artefacts."""
from pathlib import Path
import json

SOURCE_ROOTS = ('clio843/src', 'navigation-builder', 'tools', 'tests', 'repository',
                'fonts', 'locales', 'third_party')
EXCLUDED_PARTS = {'.git', '.venv', '.cache', '.build', '__pycache__', 'node_modules'}
EXCLUDED_NAMES = {
    '.DS_Store', 'build_qadj', 'build_uk.command',
    'bond.dat', 'faves.dat', 'recent.dat', 'trip.dat',
    'audlast.dat', 'ui_translations 2.h', 'history.dat',
    'history.prev.dat', 'mobility.dat', 'mobility.tmp',
    'mobility-places.dat', 'mobility-places.tmp',
    'gpslast.dat', 'prepare_minimal_release.py'
}
PREVIEW_GIFS = ('boot-day.gif', 'boot-night.gif', 'popups.gif', 'driving.gif', 'menus.gif', 'bluetooth.gif', 'phone-destination.gif',
                'speed-warning.gif', 'map-browsing.gif', 'roundabout-eta.gif', 'add-stop.gif', 'status-notifications.gif', 'metadata-updates.gif', 'coordinates.gif', 'coordinate-route.gif', 'map-point-route.gif',
                'v216-coordinates.gif', 'v216-hud-settings.gif', 'v216-bluetooth-hidden.gif', 'v216-itinerary.gif',
                'trips-layout.gif', 'trips-empty-layout.gif', 'popups-layout.gif',
                'v220-destination-prediction.gif', 'v220-intelligent-diversions.gif', 'v220-frequent-place.gif',
                'v223-nearby-pois.gif', 'v223-secondary-manoeuvre.gif', 'v223-view-diversion.gif')

def source_files(root, reports):
    files = {}
    for directory in SOURCE_ROOTS:
        for path in sorted((root / directory).rglob('*')):
            relative = path.relative_to(root)
            if (set(relative.parts) & EXCLUDED_PARTS or path.name in EXCLUDED_NAMES
                    or path.suffix in ('.pyc', '.pbf', '.cab', '.log', '.o', '.a', '.rgb565')):
                continue
            if path.is_symlink():
                raise ValueError('Source contains a redirected file: ' + str(relative))
            if path.is_file():
                files['src/' + relative.as_posix()] = path
    names = ('README.md', 'VERSION', 'BUILDING.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md',
             '.gitignore', 'Install.command', 'install.py', 'bootstrap-SHA256.json',
             'docs/source-status.md', 'docs/camera-support.md', 'docs/repository-readme.md',
             'docs/changelog.md', 'docs/updating.md', 'docs/intelligent-navigation.md', 'docs/release-workflow.md', 'docs/scenic-iii-hardware.md', 'card-template/cswmap.example.ini', 'legacy-binaries/clioaux-init',
             'legacy-binaries/manifest.json', 'clio843/font.bin',
             'card-template/ttn', 'card-template/clio843/btstart.sh', 'card-template/clio843/prefs.ini',
             'card-template/clio843/focusd', 'card-template/clio843/hidd')
    for name in names:
        files['src/' + name] = root / name
    for name in ('clio040', 'cliospl', 'clioa2dp'):
        for directory in ('clio843',):
            files['src/' + directory + '/' + name] = root / directory / name
        report = reports[name]
        files['src/build-reports/' + report.name] = report
    demo = root/'docs/demo'
    for name in ('README.md', 'manifest.json', *PREVIEW_GIFS):
        files['src/docs/demo/'+name] = demo/name
    return files

def gitignore():
    return b'''# Personal/local installation inputs and generated outputs
**/LOCAL-PROPRIETARY-FILES/
*.cab
*.osm.pbf
**/.DS_Store
**/__pycache__/
*.pyc
**/.venv/
**/.cache
**/.build
**/node_modules/
**/navigation-output/
**/build/
**/build-work/
**/dist/
**/bond.dat
**/faves.dat
**/recent.dat
**/trip.dat
**/audlast.dat
**/gpslast.dat
**/history.dat
**/history.prev.dat
**/mobility.dat
**/mobility.tmp
**/mobility-places.dat
**/mobility-places.tmp
**/cswmap.ini
/csw
/vstat
**/*.log
**/*.rgb565
**/*.dSYM/
**/.env
**/.env.*
# Local hardware-test reports/captures; retain the trial instructions.
/src/docs/next-release/*
!/src/docs/next-release/hardware-trial.md
/src/clio843/prefs.ini
/src/clio843/routing-status.txt
# Proprietary outputs, even if imported into a runtime tree locally
**/ttsystem
**/PNDNavigator
**/vehicledaemon
src/tools/prepare_minimal_release.py
'''

def source_gitignore(root):
    return (root / '.gitignore').read_bytes()
