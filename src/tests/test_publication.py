"""Git boundaries must exclude private/local outputs and detect stray files."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from publication import gitignore
from verify_publication import verify_git_candidates


class PublicationTest(unittest.TestCase):
    def fixture(self, root):
        (root/'.gitignore').write_bytes(gitignore())
        (root/'latest.json').write_text('{}')
        return {'.gitignore': 'unused-hash'}

    def test_local_reports_and_private_links_are_ignored(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);managed=self.fixture(root)
            for name in ('src/docs/next-release/atlas-verification.json',
                         'src/build/fixture/private.txt', 'src/clio843/prefs.ini',
                         '.env', 'src/navigation-builder/.cache/token'):
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('private fixture')
            (root/'src/navigation-builder/.build').symlink_to(root/'src/build', target_is_directory=True)
            (root/'src/docs/next-release/hardware-trial.md').write_text('public instructions')
            managed['src/docs/next-release/hardware-trial.md']='unused-hash'
            result=verify_git_candidates(root,managed)
            self.assertEqual(result['files'],3)

    def test_unmanaged_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);managed=self.fixture(root)
            (root/'accidental-notes.txt').write_text('not a release file')
            with self.assertRaisesRegex(ValueError,'unexpected'):
                verify_git_candidates(root,managed)

    def test_accidental_website_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);managed=self.fixture(root)
            (root/'SENDER').mkdir();(root/'SENDER/index.html').write_text('separate project')
            with self.assertRaisesRegex(ValueError,'unexpected'):
                verify_git_candidates(root,managed)


if __name__ == '__main__':
    unittest.main()
