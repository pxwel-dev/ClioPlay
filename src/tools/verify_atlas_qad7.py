#!/usr/bin/env python3
"""Check QAD7 scheduling and repair using an existing UK atlas, read-only.

Requires Newcastle coverage. Runs mapped and forced-paged variants under UBSan;
temporary state stays outside the publication. Host timing is not car timing.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, default=ROOT/'navigation-output')
    args = parser.parse_args()
    maps = args.maps.resolve()
    if not (maps/'atlas.cpa').is_file():
        parser.error('--maps must be a UK atlas covering Newcastle')
    with tempfile.TemporaryDirectory(prefix='clioplay-atlas-qad7-') as folder:
        work = Path(folder)
        (work/'clio843').mkdir()
        (work/'clio843/maps').symlink_to(maps, target_is_directory=True)
        # Exercise the minimum 1 MiB query arena, not desktop memory capacity.
        (work/'meminfo').write_text('MemTotal: 65536 kB\nMemAvailable: 2048 kB\n')
        exe = work/'qad7'
        subprocess.run(['cc', '-O1', '-g', '-fsanitize=undefined',
                        '-fno-sanitize-recover=all',
                        '-DCLIO_HOST_ROOT="'+str(work)+'/"',
                        str(ROOT/'tests/atlas-qad7.c'), '-o', str(exe)], check=True)
        for mode in ('mapped', 'paged'):
            subprocess.run([str(exe), mode], check=True, timeout=300)


if __name__ == '__main__':
    main()
