"""The guided installer always prepares a full runtime before target approval."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
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


if __name__ == '__main__':
    unittest.main()
