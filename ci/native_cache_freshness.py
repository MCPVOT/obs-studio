#!/usr/bin/env -S uv run --script
"""Temporary source overlay and installed-binary evidence for native cache qualification."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

SOURCE_PATH = Path('frontend/OBSApp.cpp.hpp')
ANCHOR = b'blog(LOG_INFO, "Qt Version: %s (runtime), %s (compiled)", qVersion(), QT_VERSION_STR);'
MAX_SOURCE = 1 << 20
MAX_BINARY = 512 << 20


class FreshnessError(ValueError):
    """A fixed diagnostic owned by this helper, safe to persist in reports."""


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def source_bytes(root: Path) -> bytes:
    path = root / SOURCE_PATH
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_SOURCE:
        raise FreshnessError('invalid freshness source')
    return path.read_bytes()


def replace_source(root: Path, data: bytes) -> None:
    path = root / SOURCE_PATH
    # Same-directory replace prevents a failed write leaving half an overlay.
    with tempfile.TemporaryDirectory(prefix='.native-freshness-', dir=path.parent) as directory:
        temporary = Path(directory) / path.name
        temporary.write_bytes(data)
        temporary.chmod(path.stat().st_mode)
        os.replace(temporary, path)


def load_state(state: Path) -> tuple[dict, bytes]:
    if state.is_symlink() or state.stat().st_size > 2 * MAX_SOURCE:
        raise FreshnessError('invalid freshness state')
    data = json.loads(state.read_bytes())
    original = base64.b64decode(data['original_base64'], validate=True)
    if len(original) > MAX_SOURCE or digest(original) != data['original_sha256']:
        raise FreshnessError('freshness baseline checksum mismatch')
    if not re.fullmatch(r'native-cache-freshness-[0-9a-f]{40}-[0-9]{1,20}-[0-9]{1,20}', data['marker']):
        raise FreshnessError('invalid freshness marker')
    expected = original.replace(ANCHOR, ANCHOR.replace(b'Qt Version:', data['marker'].encode() + b'; Qt Version:'))
    if original.count(ANCHOR) != 1 or digest(expected) != data['modified_sha256']:
        raise FreshnessError('freshness overlay checksum mismatch')
    return data, original


def prepare(root: Path, state: Path, context: dict) -> dict:
    if state.exists() or state.is_symlink():
        raise FileExistsError('freshness state already exists')
    if any(context.get(key) != value for key, value in (
            ('GITHUB_REPOSITORY', 'obs-rust/obs-studio'), ('GITHUB_REF', 'refs/heads/master'),
            ('GITHUB_EVENT_NAME', 'workflow_dispatch'), ('GITHUB_ACTIONS', 'true'))):
        raise FreshnessError('freshness overlay requires trusted manual master context')
    sha = context.get('GITHUB_SHA', '')
    run = context.get('GITHUB_RUN_ID', '')
    attempt = context.get('GITHUB_RUN_ATTEMPT', '')
    if not re.fullmatch('[0-9a-f]{40}', sha) or not all(re.fullmatch('[0-9]{1,20}', v) for v in (run, attempt)):
        raise FreshnessError('invalid freshness run identity')
    original = source_bytes(root)
    if original.count(ANCHOR) != 1:
        raise FreshnessError('freshness source layout changed')
    marker = f'native-cache-freshness-{sha}-{run}-{attempt}'
    modified = original.replace(ANCHOR, ANCHOR.replace(b'Qt Version:', marker.encode() + b'; Qt Version:'))
    data = {'schema': 1, 'marker': marker, 'source_sha': sha,
            'original_sha256': digest(original), 'modified_sha256': digest(modified),
            'original_base64': base64.b64encode(original).decode()}
    with state.open('x', encoding='utf-8') as output:
        json.dump(data, output, sort_keys=True)
    replace_source(root, modified)
    return {key: value for key, value in data.items() if key != 'original_base64'} | {'status': 'prepared'}


def verify(root: Path, state: Path, platform: str, architecture: str) -> dict:
    data, _ = load_state(state)
    if digest(source_bytes(root)) != data['modified_sha256']:
        raise FreshnessError('freshness source changed before verification')
    if platform == 'windows' and architecture in ('x64', 'arm64'):
        candidates = [root / f'build_{architecture}/install/bin/64bit/obs64.exe']
        magics = (b'MZ',)
    elif platform == 'macos' and architecture in ('arm64', 'x86_64'):
        # The macOS target helper overrides the generic Unix GUI name with
        # OBS_MACOS_BUNDLE_NAME (cmake/macos/helpers.cmake), for both brands.
        candidates = [root / f'build_macos/{bundle}.app/Contents/MacOS/{bundle}'
                      for bundle in ('OBS-Studio-Rust', 'OBS')]
        magics = (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca')
    else:
        raise FreshnessError('unsupported freshness axis')
    binaries = [path for path in candidates if path.exists()]
    if len(binaries) != 1 or binaries[0].is_symlink():
        raise FreshnessError('expected exactly one installed main binary')
    binary = binaries[0]
    if not binary.is_file() or binary.stat().st_size > MAX_BINARY:
        raise FreshnessError('invalid installed main binary')
    marker = data['marker'].encode()
    checksum = hashlib.sha256()
    found = False
    tail = b''
    with binary.open('rb') as stream:
        prefix = stream.read(4)
        if not any(prefix.startswith(magic) for magic in magics):
            raise FreshnessError('installed main binary format mismatch')
        stream.seek(0)
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            checksum.update(chunk)
            found |= marker in tail + chunk
            tail = chunk[-len(marker):]
    if not found:
        raise FreshnessError('installed main binary lacks changed-source freshness marker')
    return {'status': 'fresh', 'marker': data['marker'], 'source_sha': data['source_sha'],
            'modified_sha256': data['modified_sha256'], 'binary': binary.relative_to(root).as_posix(),
            'binary_bytes': binary.stat().st_size, 'binary_sha256': checksum.hexdigest()}


def restore(root: Path, state: Path) -> dict:
    data, original = load_state(state)
    current = source_bytes(root)
    if digest(current) not in (data['original_sha256'], data['modified_sha256']):
        raise FreshnessError('refusing to overwrite an unrelated source edit')
    if current != original:
        replace_source(root, original)
    return {'status': 'restored', 'original_sha256': digest(original)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('prepare', 'verify', 'restore'))
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--platform', choices=('windows', 'macos'))
    parser.add_argument('--architecture')
    args = parser.parse_args()
    with args.report.open('x', encoding='utf-8') as output:
        try:
            if args.mode == 'prepare':
                report = prepare(args.root, args.state, os.environ)
            elif args.mode == 'verify':
                report = verify(args.root, args.state, args.platform, args.architecture)
            else:
                report = restore(args.root, args.state)
            code = 0
        except (OSError, ValueError, KeyError) as error:
            report = {'status': 'error', 'error_type': type(error).__name__}
            if isinstance(error, FreshnessError):
                report['reason'] = str(error)
            code = 1
        json.dump(report, output, sort_keys=True, indent=2)
        output.write('\n')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
