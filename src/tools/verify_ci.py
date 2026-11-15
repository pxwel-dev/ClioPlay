#!/usr/bin/env python3
"""Dependency-light host checks for a clean ClioPlay Git checkout.

No map extracts, firmware, website, network, device or third-party Python modules
are needed. Host success does not establish physical-unit behaviour.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys
import tempfile
import release_version

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(list(map(str, args)), check=True, cwd=ROOT.parent)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-only', action='store_true',
                        help='run the local regression suite without release packaging checks')
    options = parser.parse_args()
    release_version.check()
    if sys.version_info < (3, 11):
        raise SystemExit('Python 3.11+ is required')
    for tool in ('git', 'cc'):
        if not shutil.which(tool):
            raise SystemExit('Required host tool missing: '+tool)
    if not options.host_only:
        run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
        run(sys.executable, ROOT/'tools/verify_publication.py')
    for name in ('test_vendor_download.py', 'test_clean_install.py',
                 'test_bundle_layout.py', 'test_publication.py', 'test_bootstrap.py', 'test_release_version.py'):
        run(sys.executable, '-m', 'unittest', 'discover', '-s', ROOT/'tests', '-p', name)
    with tempfile.TemporaryDirectory(prefix='clioplay-ci-') as temp:
        (Path(temp)/'clio843').mkdir()
        for name in ('map-antialias', 'map-areas', 'map-layers', 'map-pois', 'vehicle-marker', 'vehicle-input', 'roundabout', 'ui-labels-alerts', 'hud-status', 'hud-metadata', 'bluetooth-sources', 'coordinates', 'hud-playback', 'hud-pages', 'hud-zoom', 'popups', 'frame-renderer', 'render-fast-paths', 'audio-chime', 'audio-decoder', 'audio-runtime', 'auto-theme', 'splash', 'boot-transition'):
            exe = Path(temp)/name
            run('cc', '-O1', '-g', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', '-DCLIO_HOST_ROOT="'+temp+'/"', ROOT/'tests'/f'{name}.c', '-lm', '-o', exe)
            args = [exe]
            if name == 'audio-chime':
                args.append(Path(temp)/'cue.pcm')
            if name == 'splash':
                args.append(temp)
            run(*args)
        # Repeat complete workflows using physical records rather than key injection.
        for name in ('coordinates', 'hud-zoom', 'bluetooth-sources', 'auto-theme'):
            exe = Path(temp)/('console-'+name)
            run('cc', '-O1', '-g', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', '-DCLIO_CONSOLE_TEST=1',
                '-DCLIO_HOST_ROOT="'+temp+'/"', ROOT/'tests'/f'{name}.c', '-lm', '-o', exe)
            run(exe)
        print('Console parity: coordinate/naming/trip, Explore/zoom/dialog, Bluetooth transport and all Settings workflows passed.')
    if not options.host_only:
        run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
    print('ClioPlay host checks passed; no hardware validation claimed.')


if __name__ == '__main__':
    main()
