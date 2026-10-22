#!/usr/bin/env -S uv run --script
"""Offline HTTP-boundary checks; no GitHub writes or native build lanes."""

import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import urllib.error

SPEC = importlib.util.spec_from_file_location('native_cache_release_store', Path(__file__).with_name('native_cache_release_store.py'))
STORE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STORE)
IDENTITY = {'platform': 'windows', 'architecture': 'x64'}


class Response(io.BytesIO):
    def __init__(self, value):
        super().__init__(value if isinstance(value, bytes) else json.dumps(value).encode())


class ReleaseStoreTest(unittest.TestCase):
    def release(self, **changes):
        return dict({'id': 17, 'tag_name': 'ci-native-cache-v1-windows-x64',
                     'prerelease': True, 'draft': False, 'body': 'native-cache-protocol-v1'}, **changes)

    def test_read_only_inventory_never_creates_release(self):
        store = STORE.ReleaseStore(IDENTITY)
        with mock.patch.object(STORE.urllib.request, 'urlopen', side_effect=urllib.error.HTTPError('url', 404, '', {}, None)) as request:
            self.assertEqual(store.assets(), [])
            self.assertEqual(request.call_args.args[0].get_method(), 'GET')
        for operation in (lambda: store.ensure('a' * 40), lambda: store.delete('x')):
            with self.assertRaises(ValueError):
                operation()

    def test_forbidden_lookup_never_creates_release(self):
        store = STORE.ReleaseStore(IDENTITY, token='job-secret', writable=True)
        with mock.patch.object(STORE.urllib.request, 'urlopen', side_effect=urllib.error.HTTPError('url', 403, '', {}, None)) as request:
            with self.assertRaises(OSError):
                store.ensure('a' * 40)
            self.assertEqual(request.call_count, 1)

    def test_create_is_cache_prerelease_and_never_latest(self):
        store = STORE.ReleaseStore(IDENTITY, token='job-secret', writable=True)
        missing = urllib.error.HTTPError('url', 404, '', {}, None)
        with mock.patch.object(STORE.urllib.request, 'urlopen', side_effect=[missing, Response(self.release())]) as request:
            store.ensure('a' * 40)
            sent = request.call_args.args[0]
            body = json.loads(sent.data)
            self.assertTrue(body['prerelease'])
            self.assertFalse(body['draft'])
            self.assertEqual(body['make_latest'], 'false')
            self.assertEqual(body['target_commitish'], 'a' * 40)
            self.assertEqual(sent.get_header('Authorization'), 'Bearer job-secret')

    def test_foreign_or_product_release_is_rejected(self):
        for changes in ({'prerelease': False}, {'draft': True}, {'body': 'product release'}, {'tag_name': 'v1.0'}):
            with self.subTest(changes=changes):
                store = STORE.ReleaseStore(IDENTITY)
                with mock.patch.object(STORE.urllib.request, 'urlopen', return_value=Response(self.release(**changes))):
                    with self.assertRaises(ValueError):
                        store.assets()

    def test_download_uses_public_url_without_credentials_and_bounds_bytes(self):
        store = STORE.ReleaseStore(IDENTITY, token='job-secret')
        name = 'native-' + 'a' * 64 + '-' + 'b' * 40 + '.tar.gz'
        asset = {'id': 1, 'name': name, 'size': 3}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / name
            with mock.patch.object(store, 'assets', return_value=[asset]), mock.patch.object(STORE.urllib.request, 'urlopen', return_value=Response(b'abcd')) as request:
                with self.assertRaises(ValueError):
                    store.download(name, path)
                self.assertIsNone(request.call_args.args[0].get_header('Authorization'))
                self.assertIn('/releases/download/ci-native-cache-v1-windows-x64/', request.call_args.args[0].full_url)
                self.assertFalse(path.exists())

    def test_api_json_is_bounded(self):
        store = STORE.ReleaseStore(IDENTITY)
        with mock.patch.object(STORE.urllib.request, 'urlopen', return_value=Response(b' ' * ((1 << 20) + 1))):
            with self.assertRaises(ValueError):
                store.assets()


if __name__ == '__main__':
    unittest.main()
