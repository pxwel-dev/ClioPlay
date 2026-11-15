#!/usr/bin/env python3
"""Keep native and bootstrap release identities aligned with src/VERSION."""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADER = Path('clio843/src/release_version.h')
SCRIPTS = ('card-template/ttn', 'card-template/clio843/btstart.sh')


def read_version(root=ROOT):
    version = (root/'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?', version):
        raise ValueError('VERSION must contain a complete release number')
    return version


def header(version):
    return ('/* Generated from VERSION by tools/release_version.py. */\n'
            '#ifndef CLIOPLAY_RELEASE_VERSION_H\n#define CLIOPLAY_RELEASE_VERSION_H\n'
            f'#define CLIOPLAY_VERSION "{version}"\n#endif\n')


def generate(root=ROOT):
    (root/HEADER).write_text(header(read_version(root)))


def check(root=ROOT):
    version = read_version(root)
    if (root/HEADER).read_text() != header(version):
        raise ValueError('Native release header is stale; rebuild with build_ui.py')
    for name in SCRIPTS:
        labels = re.findall(r'^CLIOPLAY_VERSION=([^\s]+)$', (root/name).read_text(), re.M)
        if labels != [version]:
            raise ValueError(f'{name}: CLIOPLAY_VERSION must match VERSION ({version})')
    return version


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not args.check:
        generate()
    print('ClioPlay release version:', check())
