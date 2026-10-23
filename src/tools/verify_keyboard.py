#!/usr/bin/env python3
"""Check every remote keyboard layout using the production renderer and UBSan.

All framebuffer dumps and saved test settings stay in a temporary directory.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

from text_engine import build
import build_keyboard_data

ROOT = Path(__file__).resolve().parents[1]


def main():
    build_keyboard_data.main([])
    with tempfile.TemporaryDirectory(prefix='clioplay-keyboard-') as folder:
        work = Path(folder)
        (work/'clio843').mkdir()
        shutil.copyfile(ROOT/'clio843/font.bin', work/'clio843/font.bin')
        flags = ['-O1', '-g', '-fsanitize=undefined',
                 '-fno-sanitize-recover=all',
                 '-DCLIO_HOST_ROOT="'+str(work)+'/"']
        obj, exe = work/'keyboard.o', work/'keyboard'
        subprocess.run(['cc', *flags, '-c', str(ROOT/'tests/keyboard.c'),
                        '-o', str(obj)], check=True)
        subprocess.run(['c++', '-fsanitize=undefined', str(obj),
                        str(build('host-pool')), '-o', str(exe)], check=True)
        subprocess.run([str(exe), str(ROOT/'navigation-builder/data/global-fonts.cpf'),
                        str(work)], check=True)


if __name__ == '__main__':
    main()
