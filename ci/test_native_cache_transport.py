#!/usr/bin/env -S uv run --script
"""Cheap archive-boundary checks; no network, compiler or additional CI lane."""

import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('native_cache_transport', Path(__file__).with_name('native_cache_transport.py'))
TRANSPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRANSPORT)
IDENTITY = {'platform': 'windows', 'architecture': 'x64', 'configuration': 'RelWithDebInfo',
            'compiler': 'cl-version-fixture', 'sdk': 'sdk-fixture', 'cache_format': 'ccache-4.14.1'}
SHA = 'a' * 40


class NativeCacheTransportTest(unittest.TestCase):
    def fixture(self, root):
        source = root / 'source'
        (source / 'nested').mkdir(parents=True)
        (source / 'nested/object').write_bytes(b'compiler cache object\x00')
        archive = root / 'seed.tar.gz'
        metadata = TRANSPORT.pack(source, archive, IDENTITY, SHA)
        return source, archive, metadata

    def test_round_trip_preserves_cache_and_producer_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, archive, metadata = self.fixture(root)
            self.assertEqual(metadata['source_sha'], SHA)
            self.assertEqual(metadata['identity'], IDENTITY)
            self.assertEqual(metadata['bytes'], archive.stat().st_size)
            self.assertEqual(metadata['sha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
            json.dumps(metadata)
            TRANSPORT.restore(archive, metadata, root / 'restored', IDENTITY)
            self.assertEqual((root / 'restored/nested/object').read_bytes(), (source / 'nested/object').read_bytes())

    def test_same_snapshot_archive_is_reproducible_across_retry_times(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, _, _ = self.fixture(root)
            with mock.patch.object(TRANSPORT.time, 'time', return_value=1000):
                first = TRANSPORT.pack(source, root / 'first.tar.gz', IDENTITY, SHA)
            with mock.patch.object(TRANSPORT.time, 'time', return_value=2000):
                second = TRANSPORT.pack(source, root / 'second.tar.gz', IDENTITY, SHA)
            self.assertEqual(first, second)
            self.assertEqual((root / 'first.tar.gz').read_bytes(), (root / 'second.tar.gz').read_bytes())

    def test_identity_changes_reject_seed_without_touching_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            for key in IDENTITY:
                changed = dict(IDENTITY, **{key: 'different'})
                with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'identity'):
                    TRANSPORT.restore(archive, metadata, root / 'restored', changed)
                self.assertFalse((root / 'restored').exists())

    def test_corrupt_payload_rejected_before_restore(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            data = bytearray(archive.read_bytes())
            data[len(data) // 2] ^= 1
            archive.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'digest'):
                TRANSPORT.restore(archive, metadata, root / 'restored', IDENTITY)
            self.assertFalse((root / 'restored').exists())

    def test_existing_destination_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            dest = root / 'restored'
            dest.mkdir()
            (dest / 'sentinel').write_text('keep')
            with self.assertRaisesRegex(ValueError, 'destination'):
                TRANSPORT.restore(archive, metadata, dest, IDENTITY)
            self.assertEqual((dest / 'sentinel').read_text(), 'keep')

    def test_verified_archive_still_rejects_path_escape_and_special_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            for name, kind in (('../escape', tarfile.REGTYPE), ('/absolute', tarfile.REGTYPE),
                               ('link', tarfile.SYMTYPE), ('hardlink', tarfile.LNKTYPE),
                               ('C:/escape', tarfile.REGTYPE), ('NUL', tarfile.REGTYPE),
                               ('trailing.', tarfile.REGTYPE), ('fifo', tarfile.FIFOTYPE)):
                member = tarfile.TarInfo(name)
                member.type = kind
                member.linkname = '../../escape'
                with tarfile.open(archive, 'w:gz') as stream:
                    stream.addfile(member, io.BytesIO(b''))
                modified = copy.deepcopy(metadata)
                modified['entries'] = 1
                modified['expanded_bytes'] = 0
                modified['bytes'] = archive.stat().st_size
                modified['sha256'] = hashlib.sha256(archive.read_bytes()).hexdigest()
                with self.subTest(name=name), self.assertRaises(ValueError):
                    TRANSPORT.restore(archive, modified, root / 'restored', IDENTITY)
                self.assertFalse((root / 'restored').exists())
                self.assertFalse((root / 'escape').exists())

    def test_hardlinked_regular_cache_files_remain_complete(self):
        import os
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'one').write_bytes(b'shared compiler object')
            os.link(source / 'one', source / 'two')
            archive = root / 'seed.tar.gz'
            metadata = TRANSPORT.pack(source, archive, IDENTITY, SHA)
            TRANSPORT.restore(archive, metadata, root / 'restored', IDENTITY)
            self.assertEqual((root / 'restored/one').read_bytes(), (root / 'restored/two').read_bytes())

    def test_destination_created_at_publication_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            destination = root / 'restored'
            original = Path.mkdir
            def racing_mkdir(path, *args, **kwargs):
                if path == destination and not destination.exists():
                    original(destination)
                    (destination / 'sentinel').write_text('other owner')
                return original(path, *args, **kwargs)
            # Publication must acquire an exclusive destination rather than
            # rename over a directory that appeared after its existence check.
            with mock.patch.object(Path, 'mkdir', racing_mkdir):
                with self.assertRaisesRegex(ValueError, 'destination'):
                    TRANSPORT.restore(archive, metadata, destination, IDENTITY)
            self.assertEqual((destination / 'sentinel').read_text(), 'other owner')

    def test_case_insensitive_archive_path_collisions_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, archive, metadata = self.fixture(root)
            for names in (('objects/AB', 'objects/ab'), ('Objects/one', 'objects/two')):
                with tarfile.open(archive, 'w:gz') as stream:
                    for name in names:
                        member = tarfile.TarInfo(name)
                        member.size = 1
                        stream.addfile(member, io.BytesIO(b'x'))
                changed = dict(metadata, entries=2, expanded_bytes=2, bytes=archive.stat().st_size,
                               sha256=hashlib.sha256(archive.read_bytes()).hexdigest())
                with self.subTest(names=names), self.assertRaisesRegex(ValueError, 'collision'):
                    TRANSPORT.restore(archive, changed, root / 'restored', IDENTITY)
                self.assertFalse((root / 'restored').exists())

    def test_resource_limits_reject_pack_and_restore_without_partial_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, archive, metadata = self.fixture(root)
            for setting, limit in (('MAX_ARCHIVE_BYTES', 1), ('MAX_EXPANDED_BYTES', 1), ('MAX_ENTRIES', 1)):
                with self.subTest(setting=setting), mock.patch.object(TRANSPORT, setting, limit):
                    with self.assertRaisesRegex(ValueError, 'budget'):
                        TRANSPORT.pack(source, root / 'rejected.tar.gz', IDENTITY, SHA)
                    with self.assertRaisesRegex(ValueError, 'budget'):
                        TRANSPORT.restore(archive, metadata, root / 'restored', IDENTITY)
                self.assertFalse((root / 'rejected.tar.gz').exists())
                self.assertFalse((root / 'restored').exists())
                self.assertFalse(list(root.glob('native-cache-*')))

    def test_pack_rejects_links_and_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'link').symlink_to(root / 'external')
            output = root / 'seed.tar.gz'
            with self.assertRaisesRegex(ValueError, 'link'):
                TRANSPORT.pack(source, output, IDENTITY, SHA)
            self.assertFalse(output.exists())
            (root / 'other').mkdir()
            output.write_bytes(b'keep')
            with self.assertRaisesRegex(ValueError, 'output'):
                TRANSPORT.pack(root / 'other', output, IDENTITY, SHA)
            self.assertEqual(output.read_bytes(), b'keep')


class MemoryReleaseStore:
    """Local protocol fixture: assets are immutable and downloads are real bytes."""
    def __init__(self):
        self.objects = {}
        self.calls = []
        self.fail_manifest = False
        self.clock = 0

    def assets(self):
        return [dict(asset) for _, asset in self.objects.values()]

    def ensure(self, source_sha):
        self.calls.append(('ensure', source_sha))

    def upload(self, path):
        self.calls.append(('upload', path.name))
        if self.fail_manifest and path.suffix == '.json':
            raise OSError('manifest publication failed')
        if path.name in self.objects:
            raise ValueError('immutable asset already exists')
        data = path.read_bytes()
        self.clock += 1
        asset = {'name': path.name, 'size': len(data), 'digest': 'sha256:' + hashlib.sha256(data).hexdigest(),
                 'created_at': f'2026-10-22T00:00:{self.clock:02d}Z', 'id': self.clock, 'state': 'uploaded'}
        self.objects[path.name] = (data, asset)
        return dict(asset)

    def download(self, name, destination):
        self.calls.append(('download', name))
        destination.write_bytes(self.objects[name][0])

    def delete(self, name):
        self.calls.append(('delete', name))
        del self.objects[name]


class NativeCacheReleaseProtocolTest(unittest.TestCase):
    def produce(self, root, store, sha=SHA):
        source = root / ('source-' + sha)
        source.mkdir()
        (source / 'object').write_text('compiler payload ' + sha)
        archive = root / (sha + '.tar.gz')
        metadata = TRANSPORT.pack(source, archive, IDENTITY, sha)
        context = {'repository': 'obs-rust/obs-studio', 'ref': 'refs/heads/master',
                   'event_name': 'workflow_dispatch', 'sha': sha, 'actions': 'true'}
        return TRANSPORT.publish(archive, metadata, store, context)

    def test_trusted_producer_and_read_only_consumer_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.produce(root, store)
            self.assertTrue(store.calls[-1][1].endswith('.json'), 'manifest is the commit point')
            store.calls.clear()
            report = TRANSPORT.consume(IDENTITY, root / 'restored', store)
            self.assertEqual(report['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + SHA)
            self.assertTrue(all(call[0] == 'download' for call in store.calls))

    def test_invalid_publication_contract_does_not_touch_previous_seed(self):
        for field, value in (('schema', 999), ('format', 'zip'), ('entries', 0),
                             ('expanded_bytes', TRANSPORT.MAX_EXPANDED_BYTES + 1),
                             ('expanded_bytes', 0), ('corrupt_payload', True)):
            with self.subTest(field=field, value=value), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                store = MemoryReleaseStore()
                self.produce(root, store)
                original = dict(store.objects)
                source = root / 'candidate'
                source.mkdir()
                (source / 'object').write_text('new payload')
                archive = root / 'candidate.tar.gz'
                metadata = TRANSPORT.pack(source, archive, IDENTITY, 'b' * 40)
                if field == 'corrupt_payload':
                    archive.write_bytes(b'not a tar archive')
                    metadata['bytes'] = archive.stat().st_size
                    metadata['sha256'] = hashlib.sha256(archive.read_bytes()).hexdigest()
                else:
                    metadata[field] = value
                store.calls.clear()
                context = {'repository': 'obs-rust/obs-studio', 'ref': 'refs/heads/master',
                           'event_name': 'workflow_dispatch', 'sha': 'b' * 40, 'actions': 'true'}
                with self.assertRaises(ValueError):
                    TRANSPORT.publish(archive, metadata, store, context)
                self.assertEqual(store.objects, original)
                self.assertEqual(store.calls, [])

    def test_structurally_conflicting_publication_never_touches_store(self):
        for paths in (('a', 'a/b'), ('a/b', 'a')):
            with self.subTest(paths=paths), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                store = MemoryReleaseStore()
                self.produce(root, store)
                original = dict(store.objects)
                archive = root / 'conflict.tar.gz'
                with tarfile.open(archive, 'w:gz') as payload:
                    for name in paths:
                        entry = tarfile.TarInfo(name)
                        entry.size = 1
                        payload.addfile(entry, io.BytesIO(b'x'))
                metadata = {'schema': 1, 'format': 'tar.gz', 'identity': IDENTITY,
                            'source_sha': 'b' * 40, 'bytes': archive.stat().st_size,
                            'expanded_bytes': 2, 'entries': 2,
                            'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}
                context = {'repository': 'obs-rust/obs-studio', 'ref': 'refs/heads/master',
                           'event_name': 'workflow_dispatch', 'sha': 'b' * 40, 'actions': 'true'}
                store.calls.clear()
                with self.assertRaises(ValueError):
                    TRANSPORT.publish(archive, metadata, store, context)
                self.assertEqual(store.calls, [])
                self.assertEqual(store.objects, original)

    def test_nonobject_newest_manifest_falls_back_to_previous_seed(self):
        for value in ([], None, 'invalid', 42):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                store = MemoryReleaseStore()
                self.produce(root, store)
                self.produce(root, store, 'b' * 40)
                name = next(name for name in store.objects if 'b' * 40 in name and name.endswith('.json'))
                data, asset = store.objects[name]
                data = json.dumps(value).encode()
                asset.update(size=len(data), digest='sha256:' + hashlib.sha256(data).hexdigest())
                store.objects[name] = (data, asset)
                report = TRANSPORT.consume(IDENTITY, root / 'restored', store)
                self.assertEqual(report['status'], 'hit')
                self.assertEqual(report['source_sha'], SHA)
                self.assertEqual(len(report['errors']), 1)

    def test_retry_reuses_identical_immutable_assets_without_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            with mock.patch.object(TRANSPORT.time, 'time', return_value=1000):
                self.produce(root, store)
            store.calls.clear()
            with mock.patch.object(TRANSPORT.time, 'time', return_value=2000):
                archive = root / 'retry.tar.gz'
                metadata = TRANSPORT.pack(root / ('source-' + SHA), archive, IDENTITY, SHA)
                context = {'repository': 'obs-rust/obs-studio', 'ref': 'refs/heads/master',
                           'event_name': 'workflow_dispatch', 'sha': SHA, 'actions': 'true'}
                TRANSPORT.publish(archive, metadata, store, context)
            self.assertEqual(len(store.objects), 2)
            self.assertFalse(any(call[0] in ('upload', 'delete') for call in store.calls))

    def test_failed_promotion_preserves_previous_usable_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.produce(root, store)
            store.fail_manifest = True
            with self.assertRaises(OSError):
                self.produce(root, store, 'b' * 40)
            self.assertFalse(any(call[0] == 'delete' for call in store.calls))
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + SHA)

    def test_old_orphan_recovery_allows_later_publication_within_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.produce(root, store)
            store.fail_manifest = True
            with self.assertRaises(OSError):
                self.produce(root, store, 'b' * 40)
            orphan = next(name for name in store.objects if 'b' * 40 in name)
            store.objects[orphan][1]['created_at'] = '2000-01-01T00:00:00Z'
            store.fail_manifest = False
            for digit in 'cd':
                self.produce(root, store, digit * 40)
            self.assertNotIn(orphan, store.objects)
            self.assertEqual(len(store.objects), 4)
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + 'd' * 40)

    def test_recent_orphan_is_protected_and_budget_refusal_preserves_seed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.produce(root, store)
            store.fail_manifest = True
            with self.assertRaises(OSError):
                self.produce(root, store, 'b' * 40)
            orphan = next(name for name in store.objects if 'b' * 40 in name)
            store.objects[orphan][1]['created_at'] = '2099-01-01T00:00:00Z'
            store.fail_manifest = False
            self.produce(root, store, 'c' * 40)
            with self.assertRaisesRegex(ValueError, 'budget'):
                self.produce(root, store, 'd' * 40)
            self.assertIn(orphan, store.objects)
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + 'c' * 40)

    def test_starter_manifests_do_not_hide_the_previous_complete_seed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.produce(root, store)
            original_manifest = next(name for name in store.objects if name.endswith('.json'))
            for digit in 'bc':
                name = original_manifest.replace(SHA, digit * 40)
                archive = root / (name[:-5] + '.tar.gz')
                archive.write_bytes(b'incomplete generation')
                store.upload(archive)
                manifest = root / name
                manifest.write_bytes(b'')
                store.upload(manifest)
                store.objects[name][1]['state'] = 'starter'
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + SHA)

    def test_retention_keeps_two_complete_generations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            for digit in 'abc':
                self.produce(root, store, digit * 40)
            self.assertEqual(len(store.objects), 4)
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'hit')
            self.assertEqual((root / 'restored/object').read_text(), 'compiler payload ' + 'c' * 40)

    def test_missing_or_incompatible_remote_seed_is_cold_without_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            self.assertEqual(TRANSPORT.consume(IDENTITY, root / 'restored', store)['status'], 'cold')
            self.produce(root, store)
            store.calls.clear()
            changed = dict(IDENTITY, sdk='different-sdk')
            self.assertEqual(TRANSPORT.consume(changed, root / 'restored', store)['status'], 'cold')
            self.assertFalse((root / 'restored').exists())
            self.assertFalse(any(call[0] in ('ensure', 'upload', 'delete') for call in store.calls))

    def test_untrusted_publication_context_rejected_before_external_operations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            archive = root / 'seed.tar.gz'
            metadata = TRANSPORT.pack(source, archive, IDENTITY, SHA)
            valid = {'repository': 'obs-rust/obs-studio', 'ref': 'refs/heads/master',
                     'event_name': 'workflow_dispatch', 'sha': SHA, 'actions': 'true'}
            for field, value in (('repository', 'other/repo'), ('ref', 'refs/heads/candidate'),
                                 ('event_name', 'pull_request'), ('sha', 'b' * 40), ('actions', 'false')):
                store = MemoryReleaseStore()
                with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'trusted'):
                    TRANSPORT.publish(archive, metadata, store, dict(valid, **{field: value}))
                self.assertEqual(store.calls, [])


class NativeCacheCommandTest(unittest.TestCase):
    def test_command_publishes_then_read_only_consumer_restores(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            identity = root / 'identity.json'
            identity.write_text(json.dumps(IDENTITY))
            source = root / 'source'
            source.mkdir()
            (source / 'object').write_text('command payload')
            store = MemoryReleaseStore()
            factory = mock.Mock(return_value=store)
            context = {'GITHUB_REPOSITORY': 'obs-rust/obs-studio', 'GITHUB_REF': 'refs/heads/master',
                       'GITHUB_EVENT_NAME': 'workflow_dispatch', 'GITHUB_SHA': SHA,
                       'GITHUB_ACTIONS': 'true', 'NATIVE_CACHE_JOB_TOKEN': 'explicit-job-token'}
            with mock.patch.dict(TRANSPORT.os.environ, context, clear=True):
                code = TRANSPORT.main(['publish', '--identity', str(identity), '--source', str(source),
                                       '--report', str(root / 'publish.json')], store_factory=factory)
            self.assertEqual(code, 0)
            self.assertTrue(factory.call_args.kwargs['writable'])
            self.assertEqual(factory.call_args.kwargs['token'], 'explicit-job-token')
            store.calls.clear()
            with mock.patch.dict(TRANSPORT.os.environ, {'GH_TOKEN': 'must-not-use-pat'}, clear=True):
                code = TRANSPORT.main(['consume', '--identity', str(identity), '--destination', str(root / 'restored'),
                                       '--report', str(root / 'consume.json')], store_factory=factory)
            self.assertEqual(code, 0)
            self.assertFalse(factory.call_args.kwargs['writable'])
            self.assertEqual(factory.call_args.kwargs['token'], '')
            self.assertEqual((root / 'restored/object').read_text(), 'command payload')
            self.assertTrue(all(call[0] == 'download' for call in store.calls))
            report = json.loads((root / 'consume.json').read_text())
            self.assertEqual(report['status'], 'hit')

    def test_qualification_rejects_older_fallback_after_new_generation_breaks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            store = MemoryReleaseStore()
            producer = NativeCacheReleaseProtocolTest()
            producer.produce(root, store)
            producer.produce(root, store, 'b' * 40)
            name = next(name for name in store.objects if 'b' * 40 in name and name.endswith('.tar.gz'))
            data, asset = store.objects[name]
            store.objects[name] = (b'corrupt', asset)
            identity = root / 'identity.json'
            identity.write_text(json.dumps(IDENTITY))
            code = TRANSPORT.main(['consume', '--identity', str(identity), '--destination', str(root / 'restored'),
                                   '--require-hit', '--expected-source', 'b' * 40,
                                   '--report', str(root / 'report.json')], store_factory=lambda *a, **kw: store)
            self.assertEqual(code, 1)
            report = json.loads((root / 'report.json').read_text())
            self.assertEqual(report['status'], 'hit')
            self.assertEqual(report['source_sha'], SHA)

    def test_local_verification_never_creates_network_store(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'object').write_text('local payload')
            identity = root / 'identity.json'
            identity.write_text(json.dumps(IDENTITY))
            factory = mock.Mock(side_effect=AssertionError('network store must not be created'))
            with mock.patch.dict(TRANSPORT.os.environ, {'GITHUB_SHA': SHA}, clear=True):
                code = TRANSPORT.main(['verify-local', '--identity', str(identity), '--source', str(source),
                                       '--report', str(root / 'report.json')], store_factory=factory)
            self.assertEqual(code, 0)
            factory.assert_not_called()
            self.assertEqual(json.loads((root / 'report.json').read_text())['status'], 'verified-local')
            self.assertEqual(sorted(path.name for path in root.iterdir()), ['identity.json', 'report.json', 'source'])

    def test_required_hit_command_fails_on_cold_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            identity = root / 'identity.json'
            identity.write_text(json.dumps(IDENTITY))
            factory = mock.Mock(return_value=MemoryReleaseStore())
            code = TRANSPORT.main(['consume', '--identity', str(identity), '--destination', str(root / 'restored'),
                                   '--require-hit', '--report', str(root / 'report.json')], store_factory=factory)
            self.assertEqual(code, 1)
            self.assertEqual(json.loads((root / 'report.json').read_text())['status'], 'cold')

    def test_untrusted_command_rejected_before_store_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            identity = root / 'identity.json'
            identity.write_text(json.dumps(IDENTITY))
            factory = mock.Mock()
            with mock.patch.dict(TRANSPORT.os.environ, {}, clear=True):
                code = TRANSPORT.main(['publish', '--identity', str(identity), '--source', str(root / 'missing'),
                                       '--report', str(root / 'report.json')], store_factory=factory)
            self.assertEqual(code, 1)
            factory.assert_not_called()
            self.assertEqual(json.loads((root / 'report.json').read_text())['status'], 'error')


if __name__ == '__main__':
    unittest.main()
