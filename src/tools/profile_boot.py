#!/usr/bin/env python3
"""Profile real startup loaders on the host, with read-only navigation inputs.

Requires existing navigation data and a host C/C++ compiler. Host timings do
not predict SD-card/head-unit boot time. Kernel, AUX, Focus, Bluetooth hardware
and remote acquisition are outside this measurement.
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
    parser.add_argument('--maps', type=Path, required=True)
    parser.add_argument('--lat', type=float, default=54.97548, help='saved map view latitude')
    parser.add_argument('--lon', type=float, default=-1.64050, help='saved map view longitude')
    args = parser.parse_args()
    maps = args.maps.resolve()
    if not (maps/'fonts.cpf').is_file():
        parser.error('--maps must contain the installed navigation bundle')
    if not -90 <= args.lat <= 90 or not -180 <= args.lon <= 180:
        parser.error('invalid saved-view coordinates')
    with tempfile.TemporaryDirectory(prefix='clioplay-boot-profile-') as directory:
        work = Path(directory)
        (work/'clio843').mkdir()
        (work/'clio843/maps').symlink_to(maps, target_is_directory=True)
        shutil.copyfile(ROOT/'clio843/font.bin', work/'clio843/font.bin')
        shutil.copyfile(ROOT/'card-template/clio843/prefs.ini', work/'clio843/prefs.ini')
        obj, exe = work/'profile.o', work/'profile'
        subprocess.run(['cc','-O2','-DCLIO_HOST_ROOT="'+str(work)+'/"',
                        '-c',str(ROOT/'tests/boot-profile.c'),'-o',str(obj)], check=True)
        subprocess.run(['c++',str(obj),str(build('host-pool')),'-o',str(exe)], check=True)
        subprocess.run([str(exe),str(round(args.lat*100000)),str(round(args.lon*100000))], check=True)


if __name__ == '__main__':
    main()
