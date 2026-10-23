#!/usr/bin/env python3
"""Dependency-light host checks for a clean ClioPlay Git checkout.

No map extracts, firmware, website, network, device or third-party Python modules
are needed. Host success does not establish physical-unit behaviour.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(list(map(str, args)), check=True, cwd=ROOT.parent)


def main():
    if sys.version_info < (3, 11):
        raise SystemExit('Python 3.11+ is required')
    for tool in ('git', 'cc'):
        if not shutil.which(tool):
            raise SystemExit('Required host tool missing: '+tool)
    run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
    run(sys.executable, ROOT/'tools/verify_publication.py')
    for name in ('test_vendor_download.py', 'test_clean_install.py',
                 'test_bundle_layout.py', 'test_publication.py'):
        run(sys.executable, '-m', 'unittest', 'discover', '-s', ROOT/'tests', '-p', name)
    with tempfile.TemporaryDirectory(prefix='clioplay-ci-') as temp:
        for name in ('map-antialias', 'map-areas', 'map-layers', 'roundabout', 'audio-chime'):
            exe = Path(temp)/name
            run('cc', '-O1', '-g', '-fsanitize=undefined',
                '-fno-sanitize-recover=all', ROOT/'tests'/f'{name}.c', '-lm', '-o', exe)
            args = [exe]
            if name == 'audio-chime':
                args.append(Path(temp)/'cue.pcm')
            run(*args)
    run(sys.executable, ROOT/'tools/refresh_latest.py', '--check')
    print('ClioPlay publication and host checks passed; no hardware validation claimed.')


if __name__ == '__main__':
    main()
