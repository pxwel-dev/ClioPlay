"""Git boundaries must exclude private/local outputs and detect stray files."""
from pathlib import Path
from contextlib import chdir
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from publication import gitignore
from verify_publication import verify_git_candidates

ROOT = Path(__file__).resolve().parents[1]


class PublicationTest(unittest.TestCase):
    def test_check_from_source_subdirectory(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);managed=self.fixture(root)
            (root/'src').mkdir()
            with chdir(root/'src'):
                result=verify_git_candidates(root,managed)
            self.assertEqual(result['files'],2)

    def test_windows_launchers_keep_exact_bytes_through_git(self):
        # A working tree can contain CRLF while an older tracked Git blob is
        # LF. Exercise real Git storage/checkout with the published attributes,
        # including autocrlf=true, rather than checking only local file bytes.
        names = ('.gitattributes','Install.cmd','src/repository/Install.cmd')
        with tempfile.TemporaryDirectory(prefix='clioplay-git-bytes-') as temp:
            work, checkout = Path(temp)/'work', Path(temp)/'checkout'
            work.mkdir();checkout.mkdir()
            for name in names:
                destination=work/name;destination.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(ROOT.parent/name,destination)
            for name in names[1:]:
                raw=(work/name).read_bytes()
                self.assertGreater(raw.count(b'\r\n'),0)
                self.assertEqual(raw.count(b'\n'),raw.count(b'\r\n'))
            subprocess.run(['git','init','--quiet',str(work)],check=True)
            command=['git','-C',str(work),'-c','core.autocrlf=true']
            subprocess.run([*command,'add','--',*names],check=True)
            subprocess.run([*command,'checkout-index','--all','--prefix='+str(checkout)+'/'],check=True)
            for name in names:
                self.assertEqual((work/name).read_bytes(),(checkout/name).read_bytes(),name)

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
