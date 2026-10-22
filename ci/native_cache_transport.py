#!/usr/bin/env -S uv run --script
"""Bounded native compiler-cache archive transport (#1269).

Native cache stores must contain ordinary files/directories; unsupported entries
fail rather than disappearing. Publication uses an explicit trusted job token.
"""

from __future__ import annotations

import argparse
import hashlib
import gzip
from datetime import datetime
import json
import os
from pathlib import Path
import re
import shutil
import stat
import tarfile
import tempfile
import time

# GitHub release assets must be strictly below 2 GiB. Expanded/native-store
# and entry limits are separate, explicit safeguards, not storage reservations.
MAX_ARCHIVE_BYTES = (2 << 30) - 1
MAX_EXPANDED_BYTES = 4 << 30
MAX_ENTRIES = 250_000
IDENTITY_FIELDS = {'platform', 'architecture', 'configuration', 'compiler', 'sdk', 'cache_format'}


def digest(stream) -> str:
    value = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1 << 20), b''):
        value.update(chunk)
    return value.hexdigest()


def validate_identity(identity: dict) -> None:
    if not isinstance(identity, dict) or set(identity) != IDENTITY_FIELDS:
        raise ValueError('cache identity fields do not match schema')
    if any(not isinstance(value, str) or not value or len(value) > 2048 for value in identity.values()):
        raise ValueError('cache identity values must be bounded nonempty strings')
    if (identity['platform'], identity['architecture']) not in {
        ('windows', 'x64'), ('windows', 'arm64'), ('macos', 'x86_64'), ('macos', 'arm64'),
    }:
        raise ValueError('unsupported native cache identity')


def safe_name(name: str) -> None:
    # Backslashes, drive prefixes and Windows-special names must not acquire
    # a different meaning when the same archive is restored on Windows.
    parts = name.split('/')
    reserved = re.compile(r'(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?', re.IGNORECASE)
    if (not name or any(part in ('', '.', '..') or part.endswith(('.', ' ')) or reserved.fullmatch(part) for part in parts)
            or any(char in name for char in '\\:<>"|?*')):
        raise ValueError('unsupported archive path: ' + name[:200])


def reject_case_collision(name: str, aliases: dict) -> None:
    parts = name.split('/')
    for count in range(1, len(parts) + 1):
        prefix = '/'.join(parts[:count])
        key = prefix.casefold()
        if key in aliases and aliases[key] != prefix:
            raise ValueError('case-insensitive archive path collision')
        aliases[key] = prefix


def publish_directory(staged: Path, destination: Path) -> None:
    # mkdir is the portable exclusive ownership operation: POSIX rename can
    # replace an empty directory created by another owner after an exists check.
    try:
        destination.mkdir()
    except FileExistsError as error:
        raise ValueError('destination already exists') from error
    complete = False
    try:
        # Link verified payloads on the same filesystem without copying bytes.
        # The caller may consume the directory only after successful return.
        shutil.copytree(staged, destination, copy_function=os.link, dirs_exist_ok=True)
        complete = True
    finally:
        if not complete:
            shutil.rmtree(destination)


def pack(source: Path, output: Path, identity: dict, source_sha: str) -> dict:
    validate_identity(identity)
    if not isinstance(source_sha, str) or not re.fullmatch('[0-9a-f]{40}', source_sha):
        raise ValueError('producer source_sha must be a complete commit identity')
    source, output = Path(source), Path(output)
    if output.exists() or output.is_symlink():
        raise ValueError('output already exists')
    if source.is_symlink():
        raise ValueError('source link is not supported')
    source = source.resolve(strict=True)
    if not source.is_dir():
        raise ValueError('source must be a directory')
    output = output.absolute()
    if source == output.resolve() or source in output.resolve().parents:
        raise ValueError('output must be outside source')
    entries, expanded, aliases = [], 0, {}
    for directory, dirs, files in os.walk(source, followlinks=False):
        dirs.sort()
        for name in sorted(dirs + files):
            path = Path(directory) / name
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode):
                raise ValueError('source link is not supported: ' + str(path.relative_to(source)))
            if not (stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode)):
                raise ValueError('source special file is not supported')
            relative = path.relative_to(source).as_posix()
            safe_name(relative)
            reject_case_collision(relative, aliases)
            expanded += info.st_size if stat.S_ISREG(info.st_mode) else 0
            entries.append((path, relative, info))
            if expanded > MAX_EXPANDED_BYTES or len(entries) > MAX_ENTRIES:
                raise ValueError('native cache expanded size/entry budget exceeded')
    # A private temporary directory ensures failures never publish partial data.
    # Hard-link publication is atomic, same-filesystem, and never clobbers output.
    with tempfile.TemporaryDirectory(prefix='native-cache-pack-', dir=output.parent) as directory:
        staged = Path(directory) / 'seed.tar.gz'
        with (
            staged.open('wb') as raw,
            gzip.GzipFile(fileobj=raw, filename='', mode='wb', mtime=0, compresslevel=1) as compressed,
            tarfile.open(fileobj=compressed, mode='w|', dereference=True) as archive,
        ):
            for path, relative, status in entries:
                info = archive.gettarinfo(str(path), arcname=relative)
                if not (info.isdir() or info.isfile()):
                    raise ValueError('source changed type during packaging')
                if info.isfile() and info.size != status.st_size:
                    raise ValueError('source changed size during packaging')
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                info.mode &= 0o777
                if info.isfile():
                    with path.open('rb') as data:
                        archive.addfile(info, data)
                else:
                    archive.addfile(info)
        size = staged.stat().st_size
        if size > MAX_ARCHIVE_BYTES:
            raise ValueError('archive asset size budget exceeded')
        with staged.open('rb') as stream:
            checksum = digest(stream)
        os.link(staged, output)
    return {'schema': 1, 'format': 'tar.gz', 'identity': dict(identity), 'source_sha': source_sha,
            'bytes': size, 'expanded_bytes': expanded, 'entries': len(entries), 'sha256': checksum}


def validate_metadata(metadata: dict, identity: dict) -> None:
    validate_identity(identity)
    if not isinstance(metadata, dict) or type(metadata.get('schema')) is not int or metadata.get('schema') != 1 or metadata.get('format') != 'tar.gz':
        raise ValueError('unsupported cache metadata schema/format')
    if metadata.get('identity') != identity:
        raise ValueError('cache identity mismatch')
    if not re.fullmatch('[0-9a-f]{40}', str(metadata.get('source_sha', ''))):
        raise ValueError('invalid producer identity')
    for field, limit in (('bytes', MAX_ARCHIVE_BYTES), ('expanded_bytes', MAX_EXPANDED_BYTES), ('entries', MAX_ENTRIES)):
        value = metadata.get(field)
        if type(value) is not int or not 0 <= value <= limit:
            raise ValueError('cache metadata budget exceeded: ' + field)
    if not re.fullmatch('[0-9a-f]{64}', str(metadata.get('sha256', ''))):
        raise ValueError('invalid archive digest')


def validate_archive(archive: Path, metadata: dict, identity: dict, staged: Path | None = None) -> None:
    validate_metadata(metadata, identity)
    with Path(archive).open('rb') as stream:
        if os.fstat(stream.fileno()).st_size != metadata['bytes']:
            raise ValueError('archive size mismatch')
        if digest(stream) != metadata['sha256']:
            raise ValueError('archive digest mismatch')
        stream.seek(0)
        seen, expanded, aliases, types = set(), 0, {}, {}
        try:
            with tarfile.open(fileobj=stream, mode='r|gz') as payload:
                for member in payload:
                    safe_name(member.name)
                    reject_case_collision(member.name, aliases)
                    if not (member.isfile() or member.isdir()) or member.name in seen:
                        raise ValueError('unsupported or duplicate archive entry')
                    parts = member.name.split('/')
                    for count in range(1, len(parts)):
                        parent = '/'.join(parts[:count])
                        if types.get(parent) == 'file':
                            raise ValueError('archive file conflicts with parent directory')
                        types[parent] = 'directory'
                    kind = 'file' if member.isfile() else 'directory'
                    if member.name in types and types[member.name] != kind:
                        raise ValueError('archive file/directory conflict')
                    types[member.name] = kind
                    seen.add(member.name)
                    expanded += member.size if member.isfile() else 0
                    if len(seen) > MAX_ENTRIES or expanded > MAX_EXPANDED_BYTES:
                        raise ValueError('archive expanded size/entry budget exceeded')
                    if staged is not None:
                        payload.extract(member, staged, filter='data')
                    elif member.isfile():
                        # Validate the complete stream without creating another
                        # expanded copy of a potentially multi-gigabyte cache.
                        with payload.extractfile(member) as data:
                            count = 0
                            for chunk in iter(lambda: data.read(1 << 20), b''):
                                count += len(chunk)
                            if count != member.size:
                                raise ValueError('truncated cache payload')
        except (tarfile.TarError, EOFError) as error:
            raise ValueError('invalid native cache archive') from error
        if expanded != metadata['expanded_bytes'] or len(seen) != metadata['entries']:
            raise ValueError('archive contents do not match metadata')


def restore(archive: Path, metadata: dict, destination: Path, identity: dict) -> None:
    destination = Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise ValueError('destination already exists')
    with tempfile.TemporaryDirectory(prefix='native-cache-restore-', dir=destination.parent) as directory:
        staged = Path(directory) / 'payload'
        staged.mkdir()
        validate_archive(archive, metadata, identity, staged)
        publish_directory(staged, destination)


# Per OS/architecture release: at most two complete steady-state generations
# plus one in-flight generation. All bytes, including orphans, count toward
# the hard ceiling. Four architecture releases imply a 24 GiB remote maximum;
# these are proposed remote bounds, never an Actions-cache capacity claim.
MAX_RELEASE_BYTES = 6 << 30
MAX_RELEASE_ASSETS = 6
MAX_MANIFEST_BYTES = 64 << 10
ASSET_NAME = re.compile(r'native-([0-9a-f]{64})-([0-9a-f]{40})\.(tar\.gz|json)')


def profile(identity: dict) -> str:
    validate_identity(identity)
    return hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def checked_assets(store) -> list:
    assets = store.assets()
    if (len(assets) > MAX_RELEASE_ASSETS
            or any(not ASSET_NAME.fullmatch(asset['name']) or type(asset['size']) is not int or asset['size'] < 0 for asset in assets)
            or sum(asset['size'] for asset in assets) > MAX_RELEASE_BYTES):
        raise ValueError('native release inventory exceeds owned namespace/budget')
    if len({asset['name'] for asset in assets}) != len(assets):
        raise ValueError('duplicate native release asset names')
    return assets


def uploaded_asset(asset: dict) -> bool:
    return (asset.get('state') == 'uploaded' and asset['size'] > 0
            and re.fullmatch(r'sha256:[0-9a-f]{64}', str(asset.get('digest', ''))) is not None)


def complete_generations(assets: list) -> list:
    ready = {asset['name']: asset for asset in assets if uploaded_asset(asset)}
    return sorted((asset for asset in ready.values() if asset['name'].endswith('.json')
                   and asset['size'] <= MAX_MANIFEST_BYTES
                   and asset['name'][:-5] + '.tar.gz' in ready
                   and ready[asset['name'][:-5] + '.tar.gz']['size'] <= MAX_ARCHIVE_BYTES),
                  key=lambda asset: (asset['created_at'], asset['id']), reverse=True)


def trim_orphans(store, assets: list, current_stem: str) -> None:
    # A serialized publisher may reclaim abandoned uploads only after a
    # ten-minute grace period. Never classify a valid complete pair as orphaned.
    committed = {asset['name'][:-5] for asset in complete_generations(assets)}
    now = time.time()
    for asset in assets:
        match = ASSET_NAME.fullmatch(asset['name'])
        stem = 'native-' + match.group(1) + '-' + match.group(2)
        if stem in committed or (stem == current_stem and uploaded_asset(asset)):
            continue
        created = datetime.fromisoformat(asset['created_at'].replace('Z', '+00:00'))
        if created.tzinfo is None:
            raise ValueError('orphan creation time must include timezone')
        if now - created.timestamp() >= 600:
            store.delete(asset['name'])


def trim_generations(store, assets: list) -> None:
    for manifest in complete_generations(assets)[2:]:
        # Remove the commit point first; readers can fall back to a retained
        # complete generation if deletion races a previously captured listing.
        store.delete(manifest['name'])
        store.delete(manifest['name'][:-5] + '.tar.gz')


def matching_asset(asset: dict, path: Path) -> bool:
    with path.open('rb') as stream:
        return asset['size'] == path.stat().st_size and asset.get('digest') == 'sha256:' + digest(stream)


def publish(archive: Path, metadata: dict, store, context: dict) -> dict:
    if (not isinstance(context, dict) or not isinstance(metadata, dict)
            or context.get('repository') != 'obs-rust/obs-studio' or context.get('ref') != 'refs/heads/master'
            or context.get('event_name') not in ('push', 'schedule', 'workflow_dispatch')
            or context.get('actions') != 'true' or context.get('sha') != metadata.get('source_sha')):
        raise ValueError('publication requires trusted master Actions context')
    validate_archive(archive, metadata, metadata['identity'])
    identity = metadata['identity']
    fingerprint = profile(identity)
    source_sha = metadata['source_sha']
    if not re.fullmatch('[0-9a-f]{40}', str(source_sha)):
        raise ValueError('publication requires trusted producer commit')
    archive = Path(archive)
    if (not 0 < archive.stat().st_size <= MAX_ARCHIVE_BYTES
            or not matching_asset({'size': metadata['bytes'], 'digest': 'sha256:' + metadata['sha256']}, archive)):
        raise ValueError('local publication archive size/digest mismatch')
    stem = 'native-' + fingerprint + '-' + source_sha
    manifest = dict(metadata, archive_name=stem + '.tar.gz')
    encoded = json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode() + b'\n'
    if len(encoded) > MAX_MANIFEST_BYTES:
        raise ValueError('native cache manifest budget exceeded')
    assets = checked_assets(store)
    # Recover a prior interrupted cleanup without discarding either retained
    # complete generation. Incomplete uploads still count against admission.
    trim_generations(store, assets)
    trim_orphans(store, checked_assets(store), stem)
    assets = checked_assets(store)
    names = {asset['name']: asset for asset in assets}
    additions = [(stem + '.tar.gz', archive.stat().st_size), (stem + '.json', len(encoded))]
    new_sizes = [size for name, size in additions if name not in names]
    if (sum(asset['size'] for asset in assets) + sum(new_sizes) > MAX_RELEASE_BYTES
            or len(assets) + len(new_sizes) > MAX_RELEASE_ASSETS):
        raise ValueError('native release publication budget exceeded')
    store.ensure(source_sha)
    with tempfile.TemporaryDirectory(prefix='native-cache-generation-', dir=archive.parent) as directory:
        root = Path(directory)
        payload = root / (stem + '.tar.gz')
        os.link(archive, payload)
        pointer = root / (stem + '.json')
        pointer.write_bytes(encoded)
        for path in (payload, pointer):
            existing = names.get(path.name)
            asset = existing if existing is not None else store.upload(path)
            if not matching_asset(asset, path):
                raise ValueError('remote asset size/digest mismatch')
    # Manifest upload is the commit point: no old generation is retired before
    # both immutable new assets are uploaded and verified.
    trim_generations(store, checked_assets(store))
    retained = checked_assets(store)
    return {'status': 'published', 'source_sha': source_sha, 'profile': fingerprint,
            'remote_bytes': sum(asset['size'] for asset in retained), 'remote_assets': len(retained)}


def consume(identity: dict, destination: Path, store) -> dict:
    fingerprint = profile(identity)
    destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        raise ValueError('destination already exists')
    started = time.monotonic()
    errors = []
    try:
        assets = checked_assets(store)
    except (OSError, ValueError) as error:
        return {'status': 'cold', 'errors': [str(error)[:300]], 'elapsed_seconds': time.monotonic() - started}
    names = {asset['name']: asset for asset in assets}
    candidates = [asset for asset in complete_generations(assets)
                  if asset['name'].startswith('native-' + fingerprint + '-')][:2]
    for candidate in candidates:
        try:
            if not 0 < candidate['size'] <= MAX_MANIFEST_BYTES:
                raise ValueError('remote manifest budget exceeded')
            with tempfile.TemporaryDirectory(prefix='native-cache-download-', dir=destination.parent) as directory:
                root = Path(directory)
                manifest_path = root / candidate['name']
                store.download(candidate['name'], manifest_path)
                if not matching_asset(candidate, manifest_path):
                    raise ValueError('remote manifest size/digest mismatch')
                metadata = json.loads(manifest_path.read_text())
                if not isinstance(metadata, dict):
                    raise ValueError('remote manifest must be an object')
                expected_archive = candidate['name'][:-5] + '.tar.gz'
                if (metadata.get('archive_name') != expected_archive
                        or metadata.get('source_sha') != ASSET_NAME.fullmatch(candidate['name']).group(2)):
                    raise ValueError('remote manifest generation identity mismatch')
                asset = names[expected_archive]
                if not 0 < asset['size'] <= MAX_ARCHIVE_BYTES:
                    raise ValueError('remote archive budget exceeded')
                payload = root / expected_archive
                store.download(expected_archive, payload)
                if not matching_asset(asset, payload):
                    raise ValueError('remote archive size/digest mismatch')
                restore(payload, metadata, destination, identity)
                return {'status': 'hit', 'source_sha': metadata['source_sha'], 'profile': fingerprint,
                        'downloaded_bytes': candidate['size'] + asset['size'],
                        'elapsed_seconds': time.monotonic() - started, 'errors': errors}
        except (OSError, ValueError, KeyError, tarfile.TarError) as error:
            errors.append(str(error)[:300])
    return {'status': 'cold', 'elapsed_seconds': time.monotonic() - started, 'errors': errors}


def main(argv=None, store_factory=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('publish', 'consume', 'verify-local'))
    parser.add_argument('--identity', type=Path, required=True)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--require-hit', action='store_true')
    parser.add_argument('--expected-source')
    args = parser.parse_args(argv)
    if args.expected_source is not None and (args.mode != 'consume' or not re.fullmatch('[0-9a-f]{40}', args.expected_source)):
        parser.error('--expected-source requires consume and a complete commit identity')
    if args.mode != 'consume' and args.require_hit:
        parser.error('--require-hit applies only to consume')
    if args.mode != 'consume' and (args.source is None or args.destination is not None):
        parser.error('archive modes require --source and no --destination')
    if args.mode == 'consume' and (args.destination is None or args.source is not None):
        parser.error('consume requires --destination and no --source')
    # Reserve the report before any operation: a preexisting report cannot cause
    # publication to succeed without leaving the requested evidence.
    with args.report.open('x') as output:
        started = time.monotonic()
        try:
            with args.identity.open('rb') as stream:
                encoded = stream.read(MAX_MANIFEST_BYTES + 1)
            if len(encoded) > MAX_MANIFEST_BYTES:
                raise ValueError('identity document budget exceeded')
            identity = json.loads(encoded)
            validate_identity(identity)
            context = {field: os.environ.get(env, '') for field, env in (
                ('repository', 'GITHUB_REPOSITORY'), ('ref', 'GITHUB_REF'),
                ('event_name', 'GITHUB_EVENT_NAME'), ('sha', 'GITHUB_SHA'), ('actions', 'GITHUB_ACTIONS'))}
            if args.mode == 'publish' and (
                    context['repository'] != 'obs-rust/obs-studio' or context['ref'] != 'refs/heads/master'
                    or context['event_name'] not in ('push', 'schedule', 'workflow_dispatch')
                    or context['actions'] != 'true' or not re.fullmatch('[0-9a-f]{40}', context['sha'])):
                raise ValueError('publication requires trusted master Actions context')
            if args.mode != 'verify-local':
                if store_factory is None:
                    from native_cache_release_store import ReleaseStore
                    store_factory = ReleaseStore
                store = store_factory(identity, token=os.environ.get('NATIVE_CACHE_JOB_TOKEN', ''),
                                      writable=args.mode == 'publish')
            if args.mode != 'consume':
                with tempfile.TemporaryDirectory(prefix='native-cache-pack-', dir=args.report.parent) as directory:
                    archive = Path(directory) / 'cache.tar.gz'
                    packed_at = time.monotonic()
                    metadata = pack(args.source, archive, identity, context['sha'])
                    packing_seconds = time.monotonic() - packed_at
                    published_at = time.monotonic()
                    if args.mode == 'publish':
                        report = publish(archive, metadata, store, context)
                    else:
                        restore(archive, metadata, Path(directory) / 'restored', identity)
                        report = {'status': 'verified-local', 'source_sha': context['sha'], 'profile': profile(identity)}
                    report.update(packing_seconds=packing_seconds,
                                  validation_and_publication_seconds=time.monotonic() - published_at,
                                  archive_bytes=metadata['bytes'], expanded_bytes=metadata['expanded_bytes'])
            else:
                report = consume(identity, args.destination, store)
            code = int((args.require_hit and report['status'] != 'hit')
                       or (args.expected_source is not None and report.get('source_sha') != args.expected_source))
            if args.expected_source is not None:
                report['expected_source_sha'] = args.expected_source
        except (OSError, ValueError, KeyError, tarfile.TarError) as error:
            report = {'status': 'error', 'error_type': type(error).__name__}
            if args.mode == 'verify-local':
                report['error'] = str(error)[:300]
            # HTTP exception URLs/bodies and external tool messages may contain
            # credentials. Persist only the exception type at this boundary.
            code = 1
        report.update(mode=args.mode, total_seconds=time.monotonic() - started)
        json.dump(report, output, sort_keys=True, indent=2)
        output.write('\n')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
