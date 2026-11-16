#!/usr/bin/env python3
"""One-command ClioPlay ARM rebuild, with optional release-version bump.

    python3 tools/rebuild_arm.py                  # rebuild current VERSION
    python3 tools/rebuild_arm.py --version 2.1.6  # set version everywhere + rebuild

Compiles all source-built ARM programs with build_ui.py, validates their ELF
headers/build reports/version strings, then installs all three into clio843/.
Does not overwrite an SD card or publish a release unless explicitly requested.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from build_ui import inspect_elf
import release_version

ROOT = Path(__file__).resolve().parents[1]
PROGRAMS = ('clio040', 'cliospl', 'clioa2dp')


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def find_compiler(user_path):
    options = ([user_path] if user_path else []) + [
        '/opt/homebrew/opt/llvm/bin/clang',
        '/usr/local/opt/llvm/bin/clang',
        shutil.which('clang'),
    ]
    if user_path and not Path(user_path).is_file():
        raise RuntimeError(f'--clang path does not exist: {user_path}')
    for entry in options:
        if entry and Path(entry).is_file():
            return str(Path(entry).resolve())
    raise RuntimeError('Clang not found. Install LLVM (macOS: brew install llvm lld), or pass --clang PATH.')


def find_linker(user_path):
    options = ([user_path] if user_path else []) + [
        '/opt/homebrew/opt/lld/bin/ld.lld',
        '/opt/homebrew/opt/llvm/bin/ld.lld',
        '/usr/local/opt/lld/bin/ld.lld',
        '/usr/local/opt/llvm/bin/ld.lld',
        shutil.which('ld.lld'),
    ]
    if user_path and not Path(user_path).is_file():
        raise RuntimeError(f'--linker path does not exist: {user_path}')
    for entry in options:
        if entry and Path(entry).is_file():
            # Homebrew often provides ld.lld as a symlink to the multi-call lld.
            # LLD chooses its ELF driver from argv[0], so resolving that symlink
            # changes the invoked name to "lld" and makes linking fail.
            selected = Path(entry).absolute()  # Preserve the ld.lld basename.
            if selected.name == 'lld':
                raise RuntimeError(
                    'The generic "lld" driver cannot link ClioPlay. '
                    'Use the ELF driver "ld.lld" (e.g. '
                    '/opt/homebrew/opt/lld/bin/ld.lld).'
                )
            return str(selected)
    raise RuntimeError('LLD not found. Install it (macOS: brew install lld), or pass --linker PATH.')


def verify_build(version):
    """Do not install anything until every build output is independently verified."""
    results = []
    for name in PROGRAMS:
        binary = ROOT / 'build' / name
        report_path = ROOT / 'build' / f'{name}.build.json'
        if not binary.is_file() or not report_path.is_file():
            raise RuntimeError(f'{name}: binary or build report is missing')
        report = json.loads(report_path.read_text())
        elf = inspect_elf(binary)
        if report.get('version') != version:
            raise RuntimeError(f'{name}: wrong version in build report')
        if report.get('sha256') != sha256(binary) or elf['sha256'] != report['sha256']:
            raise RuntimeError(f'{name}: build-report SHA-256 mismatch')
        # All three executables reference CLIOPLAY_VERSION in their compiled UI/log strings.
        if version.encode('ascii') not in binary.read_bytes():
            raise RuntimeError(f'{name}: release version was not embedded in the executable')
        results.append((binary, ROOT / 'clio843' / name, report))
    return results


def install(results):
    """Stage and verify every executable, then promote with rollback on failure."""
    folder = ROOT / 'clio843'
    folder.mkdir(parents=True, exist_ok=True)
    staged, backups, replaced = [], [], []
    try:
        for source, destination, report in results:
            fd, filename = tempfile.mkstemp(prefix=f'.{destination.name}-', suffix='.new', dir=folder)
            os.close(fd)
            temp = Path(filename)
            staged.append((temp, destination, report))
            shutil.copyfile(source, temp)
            temp.chmod(0o755)
            if sha256(temp) != report['sha256']:
                raise RuntimeError(f'{destination.name}: staged copy failed verification')
        for _, destination, _ in staged:
            backup = None
            if destination.exists():
                fd, filename = tempfile.mkstemp(prefix=f'.{destination.name}-', suffix='.old', dir=folder)
                os.close(fd)
                backup = Path(filename)
                backups.append(backup)
                shutil.copy2(destination, backup)
            replaced.append((destination, backup))
        for temp, destination, _ in staged:
            os.replace(temp, destination)
    except BaseException:
        # An OS-level rename failure can happen halfway through promotion.
        # Restore the prior runtime where possible.
        for destination, backup in reversed(replaced):
            if backup is not None and backup.exists():
                os.replace(backup, destination)
            elif backup is None:
                destination.unlink(missing_ok=True)
        raise
    finally:
        for temp, _, _ in staged:
            temp.unlink(missing_ok=True)
        for backup in backups:
            backup.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('new_version', nargs='?', help='Optional new version, e.g. 2.1.6')
    parser.add_argument('--version', dest='version_option', help='Set version in all sources/bootstraps before building')
    parser.add_argument('--build-only', action='store_true', help='Build without installing binaries in src/clio843/')
    parser.add_argument('--publish', action='store_true', help='Also refresh the clean-install publication (requires up-to-date README GIFs)')
    parser.add_argument('--dry-run', action='store_true', help='Show the intended actions without changing any files')
    parser.add_argument('--clang', help='Full path to Clang with ARM backend')
    parser.add_argument('--linker', help='Full path to ld.lld')
    parser.add_argument('--no-map-antialias', action='store_true', help='Forward the map rasterizer fallback flag to build_ui.py')
    args = parser.parse_args(argv)
    if args.new_version and args.version_option:
        parser.error('Specify either a positional version or --version, not both')
    if args.publish and args.build_only:
        parser.error('--publish requires installing binaries (omit --build-only)')
    if sys.version_info < (3, 12):
        parser.error('Python 3.12+ is required by the ClioPlay build system')

    target = args.version_option or args.new_version or release_version.read_version(ROOT)
    # Validate before touching any file, including in --dry-run mode.
    release_version.validate_version(target)
    if args.dry_run:
        print(f'Would synchronise version {target} across src/VERSION, native header, bootstrap scripts and their SHA-256 manifest.')
        print('Would compile ARM7TDMI executables:', ', '.join(PROGRAMS))
        print('Would install into src/clio843/' if not args.build_only else 'Would leave executables in src/build/')
        if args.publish:
            print('Would refresh the release publication (requires existing GIF previews to be up to date).')
        return 0

    release_version.synchronise(ROOT, target)
    release_version.check(ROOT)
    print(f'ClioPlay {target} — rebuilding {", ".join(PROGRAMS)}', flush=True)
    clang = find_compiler(args.clang)
    linker = find_linker(args.linker)
    cmd = [sys.executable, str(ROOT / 'tools/build_ui.py'), '--clang', clang, '--linker', linker]
    if args.no_map_antialias:
        cmd.append('--no-map-antialias')
    # build_ui.py deliberately runs *without* --install: a failed third program
    # must not leave the runtime containing a mixture of old and new binaries.
    env = os.environ.copy()
    # The Unicode text engine invokes clang++ by name. Make sure it comes from
    # the same LLVM installation as the selected ARM-capable clang.
    env['PATH'] = str(Path(clang).parent) + os.pathsep + env.get('PATH', '')
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)
    results = verify_build(target)
    if not args.build_only:
        install(results)
    for binary, installed, report in results:
        label = installed if not args.build_only else binary
        print(f'  OK  {label.relative_to(ROOT)}   {report["bytes"]:,} bytes   SHA-256 {report["sha256"][:16]}…')
    if args.publish:
        subprocess.run([sys.executable, str(ROOT / 'tools/refresh_latest.py')], cwd=ROOT, check=True)
    print(f'\nRebuild successful: ClioPlay {target}.', flush=True)
    if not args.build_only:
        print('Binaries installed in src/clio843/. Rebuild is not a deployment to the SD card.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as exc:
        print(f'\nRebuild failed: {exc}', file=sys.stderr)
        print('Review the error above before deploying the binaries to the SD card.', file=sys.stderr)
        sys.exit(1)
