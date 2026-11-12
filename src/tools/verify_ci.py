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

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(list(map(str, args)), check=True, cwd=ROOT.parent)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-only', action='store_true',
                        help='run the local regression suite without release packaging checks')
    options = parser.parse_args()
    if sys.version_info < (3, 11):
        raise SystemExit('Python 3.11+ is required')
    for tool in ('git', 'cc'):
        if not shutil.which(tool):
            raise SystemExit('Required host tool missing: '+tool)
    if not options.host_only:
        run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
        run(sys.executable, ROOT/'tools/verify_publication.py')
    for name in ('test_vendor_download.py', 'test_clean_install.py',
                 'test_bundle_layout.py', 'test_publication.py', 'test_bootstrap.py'):
        run(sys.executable, '-m', 'unittest', 'discover', '-s', ROOT/'tests', '-p', name)
    with tempfile.TemporaryDirectory(prefix='clioplay-ci-') as temp:
        (Path(temp)/'clio843').mkdir()
        for name in ('map-antialias', 'map-areas', 'map-layers', 'roundabout', 'ui-labels-alerts', 'hud-status', 'hud-metadata', 'bluetooth-sources', 'coordinates', 'hud-playback', 'hud-pages', 'hud-zoom', 'frame-renderer', 'audio-chime', 'audio-decoder', 'audio-runtime', 'auto-theme', 'splash'):
            exe = Path(temp)/name
            run('cc', '-O1', '-g', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', '-DCLIO_HOST_ROOT="'+temp+'/"', ROOT/'tests'/f'{name}.c', '-lm', '-o', exe)
            args = [exe]
            if name == 'audio-chime':
                args.append(Path(temp)/'cue.pcm')
            if name == 'splash':
                args.append(temp)
            run(*args)
    if not options.host_only:
        run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
    print('ClioPlay host checks passed; no hardware validation claimed.')


if __name__ == '__main__':
    main()
