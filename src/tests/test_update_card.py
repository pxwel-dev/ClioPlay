
"""Software-only update: validate, preserve data, back up and roll back."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import update_card as updater
from release_version import header
from runtime_integrity import sha256


class UpdateTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name).resolve()
        self.root = base/'source'
        self.card = base/'card'
        self.root.mkdir()
        self.card.mkdir()
        for name in updater.PAYLOAD:
            source = self.root/(
                'card-template/'+name if name in updater.BOOTSTRAP else name
            )
            destination = self.card/name
            source.parent.mkdir(parents=True, exist_ok=True)
            destination.parent.mkdir(parents=True, exist_ok=True)
            raw = b'new software: '+name.encode()
            if name in ('ttn', 'clio843/btstart.sh'):
                raw = b'# ClioPlay bootstrap.\nCLIOPLAY_VERSION=2.2.0\n'
            if name in updater.PROGRAMS[:3]:
                elf = bytearray(52)
                elf[:7] = b'\x7fELF\x01\x01\x01'
                elf[18:20] = b'\x28\x00'
                raw = bytes(elf)+raw
            source.write_bytes(raw)
            destination.write_bytes(b'old software: '+name.encode())
        (self.card/'ttn').write_bytes(
            b'# ClioPlay bootstrap.\nCLIOPLAY_VERSION=2.1.8\n'
        )
        (self.root/'VERSION').write_text('2.2.0\n')
        (self.root/'clio843/src').mkdir()
        version_header = self.root/'clio843/src/release_version.h'
        version_header.write_text(header('2.2.0'))
        prefs = self.root/'card-template/clio843/prefs.ini'
        prefs.write_text('default prefs')
        bootstrap = {
            n: sha256(self.root/'card-template'/n)
            for n in (*updater.BOOTSTRAP, 'clio843/prefs.ini')
        }
        (self.root/'bootstrap-SHA256.json').write_text(json.dumps(bootstrap))
        self.reports = self.root/'build-reports'
        self.reports.mkdir()
        for name in updater.PROGRAMS[:3]:
            data = {
                'version': '2.2.0',
                'sha256': sha256(self.root/name),
                'inputs': {
                    'clio843/src/release_version.h': sha256(version_header)
                }
            }
            (self.reports/(Path(name).name+'.build.json')).write_text(
                json.dumps(data)
            )
        vendor = {}
        for name in ('ttsystem', 'vehicledaemon'):
            f = self.card/name
            f.write_bytes(b'vendor fixture '+name.encode())
            vendor[name] = (name, sha256(f))
        self.vendor_patch = patch.object(updater, 'VENDOR', vendor)
        self.vendor_patch.start()
        self.addCleanup(self.vendor_patch.stop)
        sync_patch = patch.object(updater, 'sync')
        sync_patch.start()
        self.addCleanup(sync_patch.stop)
        for name in (
            'prefs.ini', 'bond.dat', 'faves.dat', 'recent.dat', 'trip.dat',
            'audlast.dat', 'gpslast.dat', 'history.dat', 'history.prev.dat',
            'mobility.dat', 'mobility.tmp', 'card-runtime.json', 'cswmap.ini',
            'maps/build.json', 'maps/region/route.cpr', 'user-file.txt'
        ):
            file = self.card/'clio843'/name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'preserve '+name.encode())
        (self.card/'unrelated.txt').write_bytes(b'unrelated')
        self.before = self.snapshot()

    def snapshot(self):
        return {
            str(f.relative_to(self.card)): f.read_bytes()
            for f in self.card.rglob('*')
            if f.is_file()
            and not f.relative_to(self.card).parts[0].startswith(
                'clioplay-update-backup-'
            )
        }

    def test_update_preserves_every_non_software_file(self):
        backup = updater.install(self.card, self.root)
        after = self.snapshot()
        self.assertEqual(set(after), set(self.before))
        for name in self.before:
            if name in updater.PAYLOAD:
                source = self.root/(
                    'card-template/'+name
                    if name in updater.BOOTSTRAP else name
                )
                self.assertEqual(after[name], source.read_bytes())
                self.assertEqual(
                    (backup/name).read_bytes(), self.before[name]
                )
            else:
                self.assertEqual(after[name], self.before[name])
        receipt = json.loads((backup/'update-backup.json').read_text())
        self.assertEqual(receipt['previous_version'], '2.1.8')
        self.assertEqual(receipt['new_version'], '2.2.0')
        self.assertEqual(
            set(receipt['original_sha256']), set(updater.PAYLOAD)
        )
        for name in updater.PAYLOAD:
            self.assertEqual(
                receipt['original_sha256'][name], sha256(backup/name)
            )
        self.assertFalse(list(self.card.glob('.clioplay-update-stage-*')))

    def test_dry_run_and_decline_leave_card_unchanged(self):
        with patch('builtins.input', side_effect=AssertionError('prompted')):
            updater.run_update(self.card, True, self.root)
        with patch('builtins.input', return_value='no'):
            updater.run_update(self.card, False, self.root)
        self.assertEqual(self.snapshot(), self.before)
        self.assertFalse(list(self.card.glob('clioplay-update-backup-*')))

    def test_late_failure_rolls_back(self):
        real_replace = os.replace
        failure_target = (self.card/'clio843/cliospl').resolve()
        failure_injected = False

        def replace(source, target, *args, **kwargs):
            nonlocal failure_injected

            if (
                not failure_injected
                and Path(target).resolve() == failure_target
            ):
                failure_injected = True
                raise OSError('injected late write failure')

            return real_replace(source, target, *args, **kwargs)

        with patch.object(updater.os, 'replace', side_effect=replace):
            with self.assertRaisesRegex(
                RuntimeError, 'Original software retained or restored'
            ):
                updater.install(self.card, self.root)

        self.assertTrue(
            failure_injected, 'The simulated write failure was not triggered'
        )
        self.assertEqual(self.snapshot(), self.before)
        self.assertTrue(list(self.card.glob('clioplay-update-backup-*')))
        self.assertFalse(list(self.card.glob('.clioplay-update-stage-*')))

    def test_install_entrypoint_routes_to_update_only(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'clioplay_update_entrypoint',
            Path(__file__).resolve().parents[1]/'install.py'
        )
        entry = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(entry)
        with patch.object(updater, 'run_update') as update, \
             patch.object(
                 entry.subprocess, 'run',
                 side_effect=AssertionError('fresh install invoked')
             ):
            entry.main(['--update', str(self.card), '--dry-run'])
        update.assert_called_once_with(self.card, True)

    def test_stale_report_rejected_before_mutation(self):
        report = self.reports/'clio040.build.json'
        data = json.loads(report.read_text())
        data['version'] = '2.1.8'
        report.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'No current build report'):
            updater.install(self.card, self.root)
        self.assertEqual(self.snapshot(), self.before)

    def test_corrupt_bootstrap_rejected(self):
        (self.root/'card-template/ttn').write_text(
            'CLIOPLAY_VERSION=2.2.0\n'
        )
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            updater.install(self.card, self.root)
        self.assertEqual(self.snapshot(), self.before)

    def test_incompatible_vendor_rejected(self):
        (self.card/'ttsystem').write_bytes(b'changed')
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'Navcore'):
            updater.install(self.card, self.root)
        self.assertEqual(self.snapshot(), before)

    def test_redirected_target_rejected(self):
        target = self.card/'clio843/font.bin'
        target.unlink()
        try:
            target.symlink_to(self.root/'clio843/font.bin')
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f'Symbolic links unavailable: {exc}')
        with self.assertRaisesRegex(ValueError, 'redirected'):
            updater.install(self.card, self.root)
        self.assertTrue(target.is_symlink())

    def test_missing_software_rejected(self):
        (self.card/'clio843/font.bin').unlink()
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'Required regular file missing'):
            updater.install(self.card, self.root)
        self.assertEqual(self.snapshot(), before)


if __name__ == '__main__':
    unittest.main()
