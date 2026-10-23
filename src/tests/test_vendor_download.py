"""Official-CAB acquisition, integrity failures and running-installer compatibility."""
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import prepare_vendor as vendor


class VendorDownloadTest(unittest.TestCase):
    payload = b'controlled CAB download fixture'

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.cache = Path(self.temporary.name)
        self.cab = self.cache/vendor.CAB_NAME
        digest = hashlib.sha256(self.payload).hexdigest()
        self.hash_patch = patch.object(vendor, 'CAB_SHA', digest)
        self.hash_patch.start()
        self.addCleanup(self.hash_patch.stop)

    def test_verified_cache_never_opens_network(self):
        self.cab.write_bytes(self.payload)
        with patch.object(vendor.urllib.request, 'urlopen') as network:
            self.assertEqual(vendor.fetch_cab(self.cache), self.cab)
            network.assert_not_called()

    def test_download_uses_official_url_and_replaces_invalid_cache(self):
        self.cab.write_bytes(b'invalid old cache')
        with patch.object(vendor.urllib.request, 'urlopen', return_value=io.BytesIO(self.payload)) as network:
            self.assertEqual(vendor.fetch_cab(self.cache), self.cab)
            network.assert_called_once_with(vendor.CAB_URL, timeout=60)
        self.assertEqual(self.cab.read_bytes(), self.payload)
        self.assertEqual(list(self.cache.iterdir()), [self.cab])

    def test_bad_download_is_not_published(self):
        with patch.object(vendor.urllib.request, 'urlopen', return_value=io.BytesIO(b'wrong content')):
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                vendor.fetch_cab(self.cache)
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_network_failure_cleans_partial_download(self):
        with patch.object(vendor.urllib.request, 'urlopen', side_effect=OSError('offline')):
            with self.assertRaises(OSError):
                vendor.fetch_cab(self.cache)
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_download_failure_never_asks_for_a_manual_cab(self):
        with patch.object(vendor, 'fetch_cab', side_effect=OSError('offline')), \
             patch('builtins.input') as prompt:
            with self.assertRaisesRegex(ValueError, 'Check your internet connection'):
                vendor.obtain_cab(self.cache)
            prompt.assert_not_called()

    def test_existing_installer_entry_point_uses_automatic_acquisition(self):
        with patch.object(vendor, 'obtain_cab', return_value=self.cab) as obtain, \
             patch('builtins.input') as prompt:
            self.assertEqual(vendor.prompt_cab(), self.cab)
            obtain.assert_called_once_with()
            prompt.assert_not_called()


if __name__ == '__main__':
    unittest.main()
