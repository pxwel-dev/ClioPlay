"""Release drift must fail before a package can be built or published."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import release_version


class ReleaseVersionTests(unittest.TestCase):
    def test_publication_labels_match(self):
        self.assertEqual(release_version.check(), (ROOT/'VERSION').read_text().strip())

    def test_changed_version_requires_both_bootstrap_labels(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'VERSION').write_text('9.8.7\n')
            (root/release_version.HEADER).parent.mkdir(parents=True)
            for name in release_version.SCRIPTS:
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True)
                p.write_text('CLIOPLAY_VERSION=9.8.6\n')
            release_version.generate(root)
            with self.assertRaisesRegex(ValueError, 'must match VERSION'):
                release_version.check(root)
            (root/release_version.SCRIPTS[0]).write_text('CLIOPLAY_VERSION=9.8.7\n')
            with self.assertRaisesRegex(ValueError, 'must match VERSION'):
                release_version.check(root)
            (root/release_version.SCRIPTS[1]).write_text('CLIOPLAY_VERSION=9.8.7\n')
            self.assertEqual(release_version.check(root), '9.8.7')
            (root/release_version.HEADER).write_text('stale header\n')
            with self.assertRaisesRegex(ValueError, 'header is stale'):
                release_version.check(root)

    def test_version_cannot_inject_native_or_shell_code(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for bad in ('2.0', '2.0.0"\n#error', '2.0.0;echo bad'):
                (root/'VERSION').write_text(bad)
                with self.assertRaises(ValueError):
                    release_version.read_version(root)


if __name__ == '__main__':
    unittest.main()
