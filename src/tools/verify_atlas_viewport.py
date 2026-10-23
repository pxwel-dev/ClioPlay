#!/usr/bin/env python3
"""Check GPS-free startup and later map views using an existing UK atlas.

Requires a UK atlas covering Newcastle. All map files are read-only; framebuffers,
host binaries and test state stay temporary. No country rebuild is performed.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from text_engine import build

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, default=ROOT/'navigation-output')
    args = parser.parse_args()
    maps = args.maps.resolve()
    if not (maps/'atlas.cpa').is_file():
        parser.error('--maps must contain a UK atlas.cpa covering Newcastle')
    with tempfile.TemporaryDirectory(prefix='clioplay-atlas-viewport-') as folder:
        work = Path(folder)
        (work/'clio843').mkdir()
        (work/'clio843/maps').symlink_to(maps, target_is_directory=True)
        shutil.copyfile(ROOT/'clio843/font.bin', work/'clio843/font.bin')
        flags = ['-O1', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                 '-DCLIO_HOST_ROOT="'+str(work)+'/"']
        obj, exe = work/'viewport.o', work/'viewport'
        subprocess.run(['cc', *flags, '-c', str(ROOT/'tests/atlas-viewport.c'),
                        '-o', str(obj)], check=True)
        subprocess.run(['c++', '-fsanitize=undefined', str(obj),
                        str(build('host-pool')), '-o', str(exe)], check=True)
        subprocess.run([str(exe), str(work)], check=True)


if __name__ == '__main__':
    main()
