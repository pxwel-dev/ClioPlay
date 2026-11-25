"""The guided installer always prepares a full runtime before target approval."""
import errno
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'navigation-builder/tools'))
import install_nav
spec = importlib.util.spec_from_file_location('clioplay_clean_setup', ROOT/'install.py')
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class CleanInstallerTest(unittest.TestCase):
    def test_existing_boot_file_still_gets_complete_runtime(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp)
            (target/'ttn').write_text('existing installation')
            with patch.object(sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', side_effect=[str(target), 'yes']), \
                 patch.object(setup.subprocess, 'run') as run, \
                 patch('prepare_card.prepare') as prepare:
                setup.main([])
                self.assertEqual(run.call_count, 2)
                prepare.assert_called_once()
                self.assertEqual(prepare.call_args.kwargs, {})
                runtime = prepare.call_args.args[0]
                self.assertEqual(run.call_args.args[0][-2:], ['--runtime', str(runtime)])
                self.assertFalse(runtime.exists())
            self.assertEqual((target/'ttn').read_text(), 'existing installation')

    def test_speed_import_is_forwarded_without_changing_default_install(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'observed.json';path.write_text('{}')
            with patch.object(sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', return_value=''), \
                 patch.object(setup.subprocess, 'run') as run:
                setup.main(['--speed-import',str(path.resolve())])
                self.assertEqual(run.call_count,1)
                self.assertEqual(run.call_args.args[0][-2:],['--speed-import',str(path.resolve())])

    def test_multiple_extract_paths_reach_the_builder(self):
        with tempfile.TemporaryDirectory() as temp:
            paths = [Path(temp)/name for name in ("France north.osm.pbf", "Belgium.osm.pbf")]
            with patch.object(sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', return_value=''), \
                 patch.object(setup.subprocess, 'run') as run:
                setup.main(['--pbf', str(paths[0]), '--pbf', str(paths[1])])
            self.assertEqual(run.call_count, 1)
            self.assertEqual(run.call_args.args[0][2:4], [str(path.resolve()) for path in paths])

    def test_decline_cleans_prepared_runtime_without_installing(self):
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', side_effect=[temp, 'no']), \
                 patch.object(setup.subprocess, 'run') as run, \
                 patch('prepare_card.prepare') as prepare:
                setup.main([])
                self.assertEqual(run.call_count, 1)
                self.assertFalse(prepare.call_args.args[0].exists())
            self.assertEqual(list(Path(temp).iterdir()), [])

    def test_build_failure_never_reaches_target_selection(self):
        with patch.object(sys.stdin, 'isatty', return_value=True), \
             patch.object(setup.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'builder')), \
             patch('builtins.input') as prompt:
            with self.assertRaisesRegex(SystemExit, 'target has not been changed'):
                setup.main([])
            prompt.assert_not_called()

    def test_download_failure_stops_before_install_confirmation(self):
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', return_value=temp) as prompt, \
                 patch.object(setup.subprocess, 'run') as run, \
                 patch('prepare_card.prepare', side_effect=ValueError('Automatic Navcore download failed')):
                with self.assertRaisesRegex(SystemExit, 'target has not been changed'):
                    setup.main([])
                self.assertEqual(run.call_count, 1)
                self.assertEqual(prompt.call_count, 1)
            self.assertEqual(list(Path(temp).iterdir()), [])

    def test_install_nav_succeeds_on_fat32_without_posix_chmod(self):
        def sha(path):
            return hashlib.sha256(path.read_bytes()).hexdigest()

        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp).resolve()
            bundle = base / 'bundle'
            runtime = base / 'runtime'
            card = base / 'card'
            bundle.mkdir()
            runtime.mkdir()
            (card / 'clio843').mkdir(parents=True)
            (card / 'clio843/prefs.ini').write_text('existing=1\n')

            # Minimal valid Global single-region bundle (CPM5 Z0..Z6, CPR2 v5, QAD7).
            (bundle / 'map.cpm').write_bytes(b'CPM5\x05\x00\x00\x06' + b'\x00' * 16)
            route_hdr = struct.pack('<4sHH' + 'I' * 21 + 'iiii', b'CPR2', 5, 0, *([0] * 21), 0, 0, 0, 0)
            (bundle / 'route.cpr').write_bytes(route_hdr)
            qadj_hdr = struct.pack(
                '<4sHH18I',
                b'QAD7', 7, 16,
                0, 0, 0, 0, 0, len(route_hdr),
                80, 0, 80, 0, 80, 0, 94, 0, 80, 0, 0, 20,
            )
            (bundle / 'route.qadj').write_bytes(qadj_hdr)
            for name in ('regions.cpg', 'timezones.cpt', 'fonts.cpf', 'cameras.cpc'):
                (bundle / name).write_bytes(b'ok')
            bnames = ('map.cpm', 'route.cpr', 'route.qadj', 'regions.cpg', 'timezones.cpt', 'fonts.cpf', 'cameras.cpc')
            (bundle / 'build.json').write_text(json.dumps({
                'builder': 'global-navigation-2',
                'files': {n: {'bytes': (bundle / n).stat().st_size, 'sha256': sha(bundle / n)} for n in bnames},
            }))

            # Minimal valid prepared runtime with executable bits set.
            rfiles = (
                'ttn', 'ttsystem', 'vehicledaemon',
                'clio843/btstart.sh', 'clio843/focusd', 'clio843/hidd',
                'clio843/prefs.ini', 'clio843/clio040', 'clio843/cliospl',
                'clio843/clioa2dp', 'clio843/font.bin',
            )
            for name in rfiles:
                dst = runtime / name
                dst.parent.mkdir(parents=True, exist_ok=True)
                raw = b'runtime-' + name.encode()
                if name in ('clio843/clio040', 'clio843/cliospl', 'clio843/clioa2dp'):
                    elf = bytearray(52)
                    elf[:7] = b'\x7fELF\x01\x01\x01'
                    elf[18:20] = b'\x28\x00'
                    raw = bytes(elf) + raw
                dst.write_bytes(raw)
                if name != 'clio843/prefs.ini' and name != 'clio843/font.bin':
                    dst.chmod(0o755)
            (runtime / 'card-runtime.json').write_text(json.dumps({
                'format': 'global-card-runtime-2',
                'files': {n: sha(runtime / n) for n in rfiles},
            }))

            eperm = PermissionError(errno.EPERM, 'Operation not permitted')
            with patch.object(Path, 'chmod', side_effect=eperm), \
                 patch.object(os, 'chmod', side_effect=eperm):
                backup = install_nav.install(bundle, card, runtime=runtime)

            self.assertEqual((card / 'clio843/prefs.ini').read_text(), 'existing=1\n')
            self.assertEqual((backup / 'clio843/prefs.ini').read_text(), 'existing=1\n')
            self.assertTrue((card / 'clio843/btstart.sh').is_file())
            self.assertTrue((card / 'clio843/maps/route.qadj').is_file())
            self.assertFalse(list(card.glob('.clioplay-global-stage-*')))


if __name__ == '__main__':
    unittest.main()
