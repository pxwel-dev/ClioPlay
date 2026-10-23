"""Release layout compatibility and safe removal of obsolete generated files."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'navigation-builder/tools'))
from bundle_layout import NAMES, REGIONAL, SHARED, bundle_names, remove_obsolete


class BundleLayoutTest(unittest.TestCase):
    def test_new_and_legacy_contracts(self):
        self.assertEqual(set(bundle_names({'builder':'global-navigation-2'})), set(NAMES))
        self.assertEqual(len(NAMES), 7)
        self.assertIn('search.cps', bundle_names({'builder':'global-navigation-1'}))
        for version in (1, 2):
            names = bundle_names({'builder':f'global-atlas-{version}', 'regions':[{'path':'r0001'}]})
            self.assertEqual('r0001/search.cps' in names, version == 1)
            self.assertTrue(all('r0001/'+name in names for name in REGIONAL))
            self.assertTrue(all(name in names for name in SHARED))

    def test_invalid_manifests(self):
        for regions in ([], [{'path':'../outside'}], [{'path':'same'}, {'path':'same'}]):
            with self.assertRaises(ValueError):
                bundle_names({'builder':'global-atlas-2', 'regions':regions})
        with self.assertRaises(ValueError):
            bundle_names({'builder':'global-atlas-region-2'})

    def test_cleanup_preserves_unmanaged_data(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root/'build.json').write_text(json.dumps({'builder':'global-atlas-1', 'regions':[{'path':'r0001'}]}))
            (root/'r0001').mkdir()
            for name in ('r0001/search.cps', 'r0001/search.cps.json', 'r0001/map.cpm',
                         'r0001/fonts.cpf', 'r0001/cameras.cpc.json', 'r0001/route.cpn', 'my-notes.txt'):
                (root/name).write_text('keep or retire')
            names = bundle_names({'builder':'global-atlas-2', 'regions':[{'path':'r0001'}]})
            remove_obsolete(root, names)
            self.assertFalse((root/'r0001/search.cps').exists())
            self.assertFalse((root/'r0001/search.cps.json').exists())
            self.assertFalse((root/'r0001/fonts.cpf').exists())
            self.assertFalse((root/'r0001/cameras.cpc.json').exists())
            self.assertFalse((root/'r0001/route.cpn').exists())
            self.assertTrue((root/'r0001/map.cpm').exists())
            self.assertTrue((root/'my-notes.txt').exists())


if __name__ == '__main__':
    unittest.main()
