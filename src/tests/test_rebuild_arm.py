"""Offline regression tests for ClioPlay's single-command ARM/version workflow."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import rebuild_arm
import release_version


class RebuildArmTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'VERSION').write_text('2.1.5\n')
        (self.root / release_version.HEADER).parent.mkdir(parents=True)
        (self.root / release_version.HEADER).write_text(release_version.header('2.1.5'))
        hashes = {}
        for name in release_version.SCRIPTS:
            p = self.root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('#!/bin/sh\nCLIOPLAY_VERSION=2.1.5\necho ok\n')
            p.chmod(0o755)
            hashes[name.removeprefix('card-template/')] = hashlib.sha256(p.read_bytes()).hexdigest()
        hashes['clio843/focusd'] = 'unrelated-hash-preserved'
        (self.root / 'bootstrap-SHA256.json').write_text(json.dumps(hashes, indent=2) + '\n')

    def test_version_sync_keeps_scripts_and_manifest_aligned(self):
        self.assertEqual(release_version.synchronise(self.root, '2.1.6'), '2.1.6')
        self.assertEqual(release_version.check(self.root), '2.1.6')
        hashes = json.loads((self.root / 'bootstrap-SHA256.json').read_text())
        self.assertEqual(hashes['clio843/focusd'], 'unrelated-hash-preserved')
        for name in release_version.SCRIPTS:
            path = self.root / name
            self.assertTrue(path.stat().st_mode & 0o111)
            self.assertIn('CLIOPLAY_VERSION=2.1.6\n', path.read_text())
            self.assertEqual(hashes[name.removeprefix('card-template/')], hashlib.sha256(path.read_bytes()).hexdigest())
        original = {p: p.read_bytes() for p in (self.root/'VERSION', self.root/'bootstrap-SHA256.json')}
        release_version.synchronise(self.root, '2.1.6')
        for p, content in original.items():
            self.assertEqual(p.read_bytes(), content)

    def test_bad_version_cannot_modify_files(self):
        original = (self.root / 'VERSION').read_bytes()
        with self.assertRaises(ValueError):
            release_version.synchronise(self.root, '2.1.6;rm -rf /')
        self.assertEqual((self.root / 'VERSION').read_bytes(), original)

    def test_duplicate_version_assignment_refused(self):
        p = self.root / release_version.SCRIPTS[0]
        p.write_text(p.read_text() + 'CLIOPLAY_VERSION=1.0.0\n')
        with self.assertRaisesRegex(ValueError, 'exactly one'):
            release_version.synchronise(self.root, '2.1.6')
        self.assertEqual((self.root / 'VERSION').read_text(), '2.1.5\n')

    def test_dry_run_does_not_modify(self):
        with patch.object(rebuild_arm, 'ROOT', self.root), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rebuild_arm.main(['--version', '2.1.6', '--dry-run']), 0)
        self.assertEqual((self.root / 'VERSION').read_text(), '2.1.5\n')

    def test_installer_stages_all_and_respects_hashes(self):
        folder = self.root / 'build'
        folder.mkdir()
        results = []
        for name in rebuild_arm.PROGRAMS:
            p = folder / name
            p.write_bytes(f'new-{name}-2.1.6'.encode())
            results.append((p, self.root/'clio843'/name, {'sha256': rebuild_arm.sha256(p)}))
        rebuild_arm.install(results)
        for original, destination, _ in results:
            self.assertEqual(destination.read_bytes(), original.read_bytes())
            self.assertTrue(destination.stat().st_mode & 0o111)
        # Corrupt a build and verify it is caught before any replacement.
        installed_before = {dest: dest.read_bytes() for _, dest, _ in results}
        results[1][0].write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'staged copy failed'):
            rebuild_arm.install(results)
        self.assertEqual({dest: dest.read_bytes() for _, dest, _ in results}, installed_before)

    def test_one_command_wires_version_build_and_install(self):
        version = '2.1.6'
        def fake_run(argv, **kwargs):
            # Simulate successful C toolchain output without requiring target libs.
            self.assertNotIn('--install', argv)
            build = self.root/'build'
            build.mkdir(exist_ok=True)
            for name in rebuild_arm.PROGRAMS:
                binary = build/name
                binary.write_bytes(b'ELF-test-ClioPlay '+version.encode()+b' '+name.encode())
                (build/f'{name}.build.json').write_text(json.dumps({
                    'version':version, 'sha256':rebuild_arm.sha256(binary), 'bytes':binary.stat().st_size
                }))
        with (patch.object(rebuild_arm, 'ROOT', self.root),
              patch.object(rebuild_arm, 'find_compiler', return_value='/fake/clang'),
              patch.object(rebuild_arm, 'find_linker', return_value='/fake/lld'),
              patch.object(rebuild_arm, 'inspect_elf', side_effect=lambda p: {'sha256': rebuild_arm.sha256(p)}),
              patch.object(rebuild_arm.subprocess, 'run', side_effect=fake_run),
              contextlib.redirect_stdout(io.StringIO())):
            self.assertEqual(rebuild_arm.main(['--version', version]), 0)
        self.assertEqual(release_version.check(self.root), version)
        for name in rebuild_arm.PROGRAMS:
            self.assertIn(version.encode(), (self.root/'clio843'/name).read_bytes())


if __name__ == '__main__':
    unittest.main()
