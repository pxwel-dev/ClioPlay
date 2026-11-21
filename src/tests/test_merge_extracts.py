"""Exercise the guided selector and real Osmium merge without map downloads."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'navigation-builder'))
from build_navigation import data_bounds, merge_extracts, prompt_extracts, sha


class ExtractSelectionTest(unittest.TestCase):
    def test_multiple_paths_accept_terminal_quoting_and_literal_spaces(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory)/"North France's map.osm.pbf"
            second = Path(directory)/"Belgium release.osm.pbf"
            first.touch()
            second.touch()
            with patch('builtins.input', side_effect=[str(first), f'"{second}"', '']):
                self.assertEqual(prompt_extracts(), [first, second])


@unittest.skipUnless(shutil.which('osmium'), 'osmium-tool is needed for the merge integration test')
class MergeExtractsTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.work = self.root/'work'
        self.work.mkdir()

    def fixture(self, name, west, east, entries):
        xml = self.root/(name+'.osm')
        xml.write_text(f'<osm version="0.6"><bounds minlat="50" minlon="{west}" '
                       f'maxlat="51" maxlon="{east}"/>{entries}</osm>')
        pbf = self.root/(name+'.osm.pbf')
        subprocess.run(['osmium', 'cat', str(xml), '-o', str(pbf)], check=True)
        return pbf

    def test_merge_deduplicates_border_and_rejects_conflicting_versions(self):
        shared = '<node id="2" version="1" lat="50.6" lon="3.0"/>'
        first = self.fixture(
            'France north', 2, 3,
            '<node id="1" version="1" lat="50.5" lon="2.5"/>'+shared+
            '<way id="11" version="1"><nd ref="1"/><nd ref="2"/>'
            '<tag k="highway" v="primary"/></way>')
        second_entries = (shared+
            '<node id="3" version="1" lat="50.7" lon="3.5"/>'
            '<node id="4" version="1" lat="50.7" lon="12"/>'
            '<way id="12" version="1"><nd ref="2"/><nd ref="3"/>'
            '<tag k="highway" v="primary"/></way>')
        second = self.fixture('Belgium', 3, 4, second_entries)
        merged, info, envelope, sources, digest = merge_extracts([first, second], self.work)
        self.assertEqual(info['data']['count']['nodes'], 4)
        self.assertEqual(info['data']['count']['ways'], 2)
        self.assertEqual(data_bounds(info, envelope), [2.5, 50.5, 4, 50.7])
        self.assertEqual([item['sha256'] for item in sources], [sha(first), sha(second)])
        self.assertEqual(merge_extracts([first, second], self.work)[4], digest)
        conflict = self.fixture('Different snapshot', 3, 4,
                                second_entries.replace('id="2" version="1"', 'id="2" version="2"'))
        with self.assertRaisesRegex(ValueError, 'different versions'):
            merge_extracts([first, conflict], self.work)
        self.assertEqual(sha(merged), digest)
        self.assertEqual(sorted(path.name for path in self.work.iterdir()),
                         ['merged-extracts.json', 'merged.osm.pbf'])


if __name__ == '__main__':
    unittest.main()
