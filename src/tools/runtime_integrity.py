#!/usr/bin/env python3
"""Verify packaged ClioPlay runtime artifacts before installation.

This is intentionally separate from the navigation builder and vendor downloader:
checksum problems should be detected locally, before expensive work or SD writes.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOTSTRAP = ('ttn', 'clio843/btstart.sh', 'clio843/focusd',
             'clio843/hidd', 'clio843/prefs.ini')
BINARIES = ('clio040', 'cliospl', 'clioa2dp')
SHA256_RE = re.compile(r'[0-9a-fA-F]{64}\Z')


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def _report_digest(path):
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise ValueError(f'Cannot read build report {path}: {exc}') from exc
    digest = report.get('sha256') if isinstance(report, dict) else None
    if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
        raise ValueError(f'Invalid SHA-256 in build report: {path}')
    return digest.lower()


def verify_bootstrap(root=ROOT):
    root = Path(root)
    path = root / 'bootstrap-SHA256.json'
    try:
        hashes = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise ValueError(f'Cannot read bootstrap manifest {path}: {exc}') from exc
    for name in BOOTSTRAP:
        expected = hashes.get(name)
        if not isinstance(expected, str) or not SHA256_RE.fullmatch(expected):
            raise ValueError(f'Missing/invalid bootstrap hash: {name}')
        artifact = root / 'card-template' / name
        actual = sha256(artifact)
        if actual != expected.lower():
            raise ValueError(f'Checksum mismatch: {artifact}\n'
                             f'  expected {expected.lower()}\n  actual   {actual}')


def verify_programs(root=ROOT):
    root = Path(root)
    for name in BINARIES:
        artifact = root / 'clio843' / name
        with artifact.open('rb') as stream:
            elf = stream.read(52)
        if elf[:7] != b'\x7fELF\x01\x01\x01' or elf[18:20] != b'\x28\x00':
            raise ValueError(f'Missing ARM runtime: {artifact}')
        candidates = [root / 'build-reports' / f'{name}.build.json',
                      root / 'build' / f'{name}.build.json']
        reports = [(path, _report_digest(path)) for path in candidates if path.is_file()]
        if not reports:
            raise ValueError(f'Missing SHA-256 build report for {artifact}')
        actual = sha256(artifact)
        if not any(expected == actual for _, expected in reports):
            expected_details = '\n'.join(f'  {path}: {expected}' for path, expected in reports)
            raise ValueError(
                f'Checksum mismatch: {artifact}\n'
                f'  actual SHA-256: {actual}\n'
                f'  recorded SHA-256:\n{expected_details}\n'
                'This usually means the runtime binary and its build report are from '
                'different builds. Use the matching official release artifacts; '
                'do not bypass verification.'
            )


def verify_local_runtime(root=ROOT):
    verify_bootstrap(root)
    verify_programs(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT,
                        help='src/ directory to verify (defaults to this script\'s src/)')
    args = parser.parse_args()
    try:
        verify_local_runtime(args.root)
    except (ValueError, OSError) as exc:
        parser.exit(1, f'Runtime integrity check failed: {exc}\n')
    print('Runtime integrity verified: 5 bootstrap files, 3 ARM executables.')


if __name__ == '__main__':
    main()
