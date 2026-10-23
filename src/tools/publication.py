"""Select the portable publication source tree, without local build artefacts."""
from pathlib import Path
import json

SOURCE_ROOTS = ('clio843/src', 'navigation-builder', 'tools', 'tests', 'repository',
                'fonts', 'locales', 'third_party')
EXCLUDED_PARTS = {'.git', '.venv', '.cache', '.build', '__pycache__', 'node_modules'}
EXCLUDED_NAMES = {'.DS_Store', 'build_qadj', 'build_uk.command',
                  'bond.dat', 'faves.dat', 'recent.dat', 'trip.dat', 'audlast.dat',
                  'ui_translations 2.h'}

def source_files(root):
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
    names = ('README.md', 'BUILDING.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md',
             '.gitignore', 'Install.command', 'install.py', 'bootstrap-SHA256.json',
             'docs/source-status.md', 'docs/camera-support.md', 'docs/repository-readme.md',
             'docs/next-release/hardware-trial.md', 'docs/github-launch.md', 'legacy-binaries/clioaux-init',
             'legacy-binaries/manifest.json', 'clio843/font.bin',
             'card-template/ttn', 'card-template/clio843/btstart.sh', 'card-template/clio843/prefs.ini',
             'card-template/clio843/focusd', 'card-template/clio843/hidd')
    for name in names:
        files['src/' + name] = root / name
    for name in ('clio040', 'cliospl', 'clioa2dp'):
        for directory in ('clio843',):
            files['src/' + directory + '/' + name] = root / directory / name
        report = root / 'build' / (name + '.build.json')
        if not report.exists():
            report = root / 'build-reports' / (name + '.build.json')
        files['src/build-reports/' + report.name] = report
    demo = root/'docs/demo'
    if (demo/'manifest.json').is_file():
        for name in ('README.md', 'index.html', 'manifest.json', 'poster.png', 'preview.gif', 'clioplay-demo.mp4'):
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
'''

def source_gitignore(root):
    return (root / '.gitignore').read_bytes()
