#!/usr/bin/env python3
"""Update only the ClioPlay software on an existing Global installation."""
import argparse
import json
import os
import re
import shutil
import tempfile
import time
from pathlib import Path

from release_version import check as check_version
from runtime_integrity import sha256, verify_local_runtime
from prepare_vendor import VENDOR

ROOT = Path(__file__).resolve().parents[1]
BOOTSTRAP = ('ttn', 'clio843/btstart.sh', 'clio843/focusd', 'clio843/hidd')
PROGRAMS = ('clio843/clio040', 'clio843/cliospl', 'clio843/clioa2dp', 'clio843/font.bin')
PAYLOAD = BOOTSTRAP + PROGRAMS
EXECUTABLES = set(PAYLOAD) - {'clio843/font.bin'}


def sync():
    if hasattr(os, 'sync'):
        os.sync()


def checked_path(root, name):
    path = root
    for part in Path(name).parts:
        path = path / part
        if path.is_symlink() or getattr(path, 'is_junction', lambda: False)():
            raise ValueError('Refusing redirected path: ' + str(path))
    if not path.is_file():
        raise ValueError('Required regular file missing: ' + str(path))
    return path


def verify_release(root):
    """Reject stale binaries, even when an old build report still matches."""
    version = check_version(root)
    verify_local_runtime(root)
    inputs = {}
    for name in ('clio040', 'cliospl', 'clioa2dp'):
        binary = root/'clio843'/name
        digest = sha256(binary)
        reports = []
        for directory in ('build', 'build-reports'):
            path = root/directory/(name + '.build.json')
            if path.is_file():
                reports.append(json.loads(path.read_text(encoding='utf-8')))
        matching = [r for r in reports if r.get('sha256') == digest
                    and r.get('version') == version and isinstance(r.get('inputs'), dict)
                    and 'clio843/src/release_version.h' in r['inputs']]
        if not matching:
            raise ValueError('No current build report for ' + name +
                             '; use an official release or rebuild the current source.')
        for relative, expected in matching[0]['inputs'].items():
            path = Path(relative)
            if path.is_absolute() or '..' in path.parts or '\\' in relative:
                raise ValueError('Unsafe source path in build report: ' + relative)
            if relative in inputs and inputs[relative] != expected:
                raise ValueError('Programs were built from different sources: ' + relative)
            inputs[relative] = expected
    for relative, expected in inputs.items():
        if sha256(checked_path(root, relative)) != expected:
            raise ValueError('Changed build input; rebuild first: ' + relative)
    font = checked_path(root, 'clio843/font.bin')
    if not font.stat().st_size:
        raise ValueError('Empty runtime font')
    manifest = root.parent/'latest.json'
    if manifest.is_file():
        data = json.loads(manifest.read_text(encoding='utf-8'))
        expected = data.get('files', {}).get('src/clio843/font.bin')
        if expected and sha256(font) != expected:
            raise ValueError('Runtime font does not match the publication manifest')
    return version


def inspect_card(sd, root=ROOT):
    sd, root = Path(sd).expanduser().absolute(), Path(root).resolve()
    if sd.is_symlink() or getattr(sd, 'is_junction', lambda: False)() or not sd.is_dir():
        raise ValueError('Select an existing SD-card root folder, not a link.')
    sd = sd.resolve()
    if sd == root or root in sd.parents or sd in root.parents:
        raise ValueError('The installed card must be separate from the source tree.')
    for name in PAYLOAD:
        checked_path(sd, name)
    boot = (sd/'ttn').read_bytes()
    match = re.search(rb'(?m)^CLIOPLAY_VERSION=([^\s]+)$', boot)
    if b'ClioPlay' not in boot or not match:
        raise ValueError('Not a recognised versioned ClioPlay installation.')
    # The existing, pinned Navcore components remain on the card.
    for name, (_, digest) in VENDOR.items():
        if sha256(checked_path(sd, name)) != digest:
            raise ValueError('Existing Navcore component does not match the supported release: ' + name)
    return sd, match.group(1).decode('ascii', errors='replace')


def copy_checked(source, dest, expected, executable=False):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with source.open('rb') as inp, dest.open('xb') as out:
        shutil.copyfileobj(inp, out, 1024*1024)
        out.flush()
        os.fsync(out.fileno())
    if sha256(dest) != expected:
        raise ValueError('Copy checksum mismatch: ' + str(dest))
    if executable and os.name != 'nt':
        dest.chmod(0o755)


def plan(sd, root=ROOT):
    root = Path(root).resolve()
    sd, old_version = inspect_card(sd, root)
    version = verify_release(root)
    files = {}
    for name in PAYLOAD:
        source = checked_path(root, ('card-template/' if name in BOOTSTRAP else '') + name)
        dest = checked_path(sd, name)
        files[name] = (source, sha256(source), sha256(dest))
    need = sum(source.stat().st_size + (sd/name).stat().st_size
               for name, (source, _, _) in files.items())
    if shutil.disk_usage(sd).free < need + 1024*1024:
        raise ValueError('Not enough card space for staged software and backup.')
    return sd, old_version, version, files


def install(sd, root=ROOT):
    """Back up all eight existing software files before replacing any of them."""
    sd, old_version, version, files = plan(sd, root)
    backup = Path(tempfile.mkdtemp(prefix='clioplay-update-backup-' +
                                  time.strftime('%Y%m%d-%H%M%S') + '-', dir=sd))
    staging = None
    installed = []
    try:
        staging = Path(tempfile.mkdtemp(prefix='.clioplay-update-stage-', dir=sd))
        for name, (source, new_hash, old_hash) in files.items():
            copy_checked(source, staging/name, new_hash, name in EXECUTABLES)
            copy_checked(checked_path(sd, name), backup/name, old_hash, name in EXECUTABLES)
        receipt = {'format': 'clioplay-software-backup-1',
                   'previous_version': old_version, 'new_version': version,
                   'original_sha256': {name: v[2] for name, v in files.items()},
                   'replacement_sha256': {name: v[1] for name, v in files.items()}}
        with (backup/'update-backup.json').open('x', encoding='utf-8') as out:
            json.dump(receipt, out, indent=2)
            out.write('\n')
            out.flush()
            os.fsync(out.fileno())
        sync()
        # Recheck every target after staging, before the first mutation.
        for name, (_, _, old_hash) in files.items():
            if sha256(checked_path(sd, name)) != old_hash:
                raise ValueError('Card changed during update: ' + name)
        for name in PAYLOAD:
            os.replace(staging/name, sd/name)
            installed.append(name)
            if sha256(sd/name) != files[name][1]:
                raise ValueError('Installed checksum mismatch: ' + name)
        sync()
    except BaseException as error:
        failures = []
        for name in reversed(installed):
            try:
                restore = staging/('restore-' + name.replace('/', '-'))
                copy_checked(backup/name, restore, files[name][2], name in EXECUTABLES)
                os.replace(restore, sd/name)
                if sha256(sd/name) != files[name][2]:
                    raise ValueError('Restored checksum mismatch')
            except BaseException as exc:
                failures.append(f'{name}: {exc}')
        sync()
        status = ('Rollback incomplete: ' + '; '.join(failures) if failures
                  else 'Original software retained or restored.')
        raise RuntimeError(f'Update failed: {error}. {status} Backup: {backup}') from error
    finally:
        if staging is not None:
            shutil.rmtree(staging, ignore_errors=True)
    return backup


def run_update(sd, dry_run=False, root=ROOT):
    sd, previous, version, files = plan(sd, root)
    print(f'ClioPlay software update: {previous} -> {version}')
    print(f'Target: {sd}')
    for name in files:
        print('  ' + name)
    print('Maps, settings, pairing, saved places, trips, GPS/history and firmware stay untouched.')
    if dry_run:
        print('Dry run: no card files changed.')
        return
    if input('Update the existing installation? [y/N] ').strip().lower() not in ('y', 'yes'):
        print('Cancelled; no card files changed.')
        return
    backup = install(sd, root)
    print(f'Update complete. Software backup: {backup}')
    print('Safely eject the card before using it in the car.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sd_root', type=Path)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args(argv)
    try:
        run_update(args.sd_root, args.dry_run)
    except (OSError, ValueError, RuntimeError) as exc:
        parser.exit(1, f'{exc}\n')


if __name__ == '__main__':
    main()
