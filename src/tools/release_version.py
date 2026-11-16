#!/usr/bin/env python3
"""Keep native and bootstrap release identities aligned with src/VERSION."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = Path('clio843/src/release_version.h')
SCRIPTS = ('card-template/ttn', 'card-template/clio843/btstart.sh')


def validate_version(version):
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?', version):
        raise ValueError('VERSION must contain a complete release number')
    return version


def read_version(root=ROOT):
    return validate_version((root/'VERSION').read_text().strip())


def header(version):
    return ('/* Generated from VERSION by tools/release_version.py. */\n'
            '#ifndef CLIOPLAY_RELEASE_VERSION_H\n#define CLIOPLAY_RELEASE_VERSION_H\n'
            f'#define CLIOPLAY_VERSION "{version}"\n#endif\n')


def generate(root=ROOT):
    (root/HEADER).write_text(header(read_version(root)))


def _write_if_changed(path, contents):
    """Replace text without changing executable permissions on bootstrap scripts."""
    raw = contents.encode('utf-8')
    if path.exists() and path.read_bytes() == raw:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.' + path.name + '-', delete=False) as f:
        tmp = Path(f.name)
        try:
            f.write(raw)
            if path.exists():
                os.chmod(tmp, stat.S_IMODE(path.stat().st_mode))
        except BaseException:
            tmp.unlink(missing_ok=True)
            raise
    try:
        os.replace(tmp, path)
    finally:
        tmp.unlink(missing_ok=True)


def synchronise(root=ROOT, version=None):
    """Update the canonical version, native header, bootstrap labels and hashes.

    The card installer verifies bootstrap-SHA256.json, so updating the scripts
    without their hashes (as older manual bumps did) breaks card preparation.
    """
    version = validate_version(version if version is not None else read_version(root))
    scripts = {}
    for name in SCRIPTS:
        path = root/name
        source = path.read_text()
        matches = list(re.finditer(r'(?m)^CLIOPLAY_VERSION=([^\s]+)$', source))
        if len(matches) != 1:
            raise ValueError(f'{name}: expected exactly one CLIOPLAY_VERSION assignment')
        scripts[name] = (path, source[:matches[0].start(1)] + version + source[matches[0].end(1):])

    manifest_path = root/'bootstrap-SHA256.json'
    hashes = json.loads(manifest_path.read_text())
    for name, (_, source) in scripts.items():
        manifest_key = name.removeprefix('card-template/')
        if manifest_key not in hashes:
            raise ValueError(f'{manifest_path}: missing bootstrap hash for {manifest_key}')
        hashes[manifest_key] = hashlib.sha256(source.encode('utf-8')).hexdigest()

    _write_if_changed(root/'VERSION', version + '\n')
    _write_if_changed(root/HEADER, header(version))
    for path, source in scripts.values():
        _write_if_changed(path, source)
    _write_if_changed(manifest_path, json.dumps(hashes, indent=2) + '\n')
    check(root)
    return version


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
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--check', action='store_true')
    group.add_argument('--set', metavar='X.Y.Z', help='Synchronise version and bootstrap checksums')
    group.add_argument('--sync', action='store_true', help='Repair bootstrap labels/checksums for the current VERSION')
    args = parser.parse_args()
    if args.set or args.sync:
        synchronise(ROOT, args.set)
    elif not args.check:
        generate()
    print('ClioPlay release version:', check())
