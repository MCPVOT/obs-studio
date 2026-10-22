#!/usr/bin/env -S uv run --script
"""Cold setup experiment; run Docker phases only on disposable GitHub runners.

Producer cost and image transfer are separate from consumer unpack/start/verify.
No result changes normal test selection, publishes a cache, or attests a lane.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

BASE = 'ubuntu:26.04@sha256:f144425ff09be612d6d9ad965196e9cdc23dae1f42110a8a11a3e9a8198759f7'
PACKAGE = re.compile(r'[a-z0-9][a-z0-9+.-]*(?::[a-z0-9][a-z0-9-]*)?\Z')
VERSION = re.compile(r'[0-9][a-zA-Z0-9.+:~_-]*\Z')
INVENTORY_FORMAT = '${binary:Package}\t${Version}\n'


def command(*args: str, output: Path | None = None) -> str:
    if output is not None:
        with output.open('wb') as stream:
            subprocess.run(args, stdout=stream, check=True)
        return ''
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}): {args[0]}\n{result.stdout[-8192:]}\n{result.stderr[-8192:]}')
    return result.stdout.strip()


def image_id(value: str) -> str:
    if not re.fullmatch(r'sha256:[0-9a-f]{64}', value):
        raise ValueError('Invalid immutable image identity')
    return value


def inventory(text: str) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        name, separator, version = line.partition('\t')
        if not separator or not PACKAGE.fullmatch(name) or not VERSION.fullmatch(version) or name in result:
            raise ValueError('Invalid or duplicate package inventory record')
        result[name] = version
    if not result:
        raise ValueError('Empty package inventory')
    return dict(sorted(result.items()))


def package_specs(text: str) -> list[str]:
    result = []
    for line in text.splitlines():
        name, separator, version = line.partition('=')
        if not separator or not PACKAGE.fullmatch(name) or not VERSION.fullmatch(version):
            raise ValueError('Every package must have an exact version')
        result.append(line)
    if not result or len(set(result)) != len(result):
        raise ValueError('Empty or duplicate package specifications')
    return result


def checksum(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_bundle(root: Path) -> dict:
    data = json.loads((root / 'bundle.json').read_text())
    if checksum(root / 'image.tar.zst') != data['archive_sha256']:
        raise ValueError('Prepared image archive checksum mismatch')
    image_id(data['image_id'])
    inventory(data['inventory'])
    return data


def timed(events: dict, name: str, *args: str, output: Path | None = None) -> str:
    start = time.monotonic()
    result = command(*args, output=output)
    events[name] = time.monotonic() - start
    return result


def container(events: dict, archives: Path) -> str:
    # Cache contents are a bind, so committing a producer never embeds .debs.
    ident = timed(events, 'create', 'docker', 'create', '--env', 'DEBIAN_FRONTEND=noninteractive',
                  '--mount', f'type=bind,src={archives.resolve()},dst=/var/cache/apt/archives',
                  BASE, 'sleep', 'infinity')
    timed(events, 'start', 'docker', 'start', ident)
    command('docker', 'exec', ident, 'sh', '-c',
            'test ! -f /etc/apt/apt.conf.d/docker-clean || mv /etc/apt/apt.conf.d/docker-clean /tmp/docker-clean.disabled')
    return ident


def installed(ident: str) -> str:
    return command('docker', 'exec', ident, 'dpkg-query', '-W', '-f=' + INVENTORY_FORMAT)


def produce(root: Path, packages: Path, archives: Path) -> None:
    specs = package_specs(packages.read_text())
    events = {}
    timed(events, 'base_pull', 'docker', 'pull', BASE)
    ident = container(events, archives)
    try:
        timed(events, 'apt_update', 'docker', 'exec', ident, 'apt-get', 'update', '-qq')
        timed(events, 'apt_install', 'docker', 'exec', ident, 'apt-get', 'install', '-y',
              '--no-install-recommends', *specs, 'git', 'ca-certificates', 'curl', 'sudo', 'zstd')
        package_inventory = installed(ident)
        inventory(package_inventory)
        digest = image_id(timed(events, 'commit', 'docker', 'commit', ident))
    finally:
        command('docker', 'stop', ident)
    timed(events, 'save', 'docker', 'image', 'save', digest, output=root / 'image.tar')
    timed(events, 'compress', 'zstd', '-T2', '-3', '-f', str(root / 'image.tar'), '-o', str(root / 'image.tar.zst'))
    data = {'image_id': digest, 'base': BASE, 'inventory': package_inventory,
            'archive_sha256': checksum(root / 'image.tar.zst'),
            'compressed_bytes': (root / 'image.tar.zst').stat().st_size,
            'producer_seconds': events, 'input_package_specs': specs,
            'revision': os.environ.get('GITHUB_SHA'), 'run_id': os.environ.get('GITHUB_RUN_ID')}
    (root / 'bundle.json').write_text(json.dumps(data, indent=2) + '\n')


def measure(root: Path, variant: str, archives: Path) -> None:
    # Both variants verify the same locked bundle. Download/restore durations
    # come from Actions steps and must be added to these phase measurements.
    start = time.monotonic()
    events = {}
    data = verify_bundle(root) if variant == 'prepared' else json.loads((root / 'bundle.json').read_text())
    if data.get('base') != BASE:
        raise ValueError('Bundle base does not match the pinned stock environment')
    events['bundle_verify'] = time.monotonic() - start
    expected = inventory(data['inventory'])
    if variant == 'prepared':
        present = subprocess.run(['docker', 'image', 'inspect', data['image_id']], capture_output=True)
        if present.returncode == 0:
            raise ValueError('Prepared image is already resident; this is not a cold-load sample')
        timed(events, 'decompress', 'zstd', '-d', '-f', str(root / 'image.tar.zst'), '-o', str(root / 'image.tar'))
        timed(events, 'load', 'docker', 'image', 'load', '--input', str(root / 'image.tar'))
        actual_id = command('docker', 'image', 'inspect', '--format', '{{.Id}}', data['image_id'])
        if actual_id != data['image_id']:
            raise ValueError('Loaded image identity mismatch')
        actual = timed(events, 'start_and_inventory', 'docker', 'run', '--entrypoint', 'dpkg-query',
                       data['image_id'], '-W', '-f=' + INVENTORY_FORMAT)
    else:
        timed(events, 'base_pull', 'docker', 'pull', BASE)
        ident = container(events, archives)
        try:
            timed(events, 'apt_update', 'docker', 'exec', ident, 'apt-get', 'update', '-qq')
            specs = [f'{name}={version}' for name, version in expected.items()]
            timed(events, 'apt_install', 'docker', 'exec', ident, 'apt-get', 'install', '-y', '--no-install-recommends', *specs)
            actual = timed(events, 'inventory', 'docker', 'exec', ident, 'dpkg-query', '-W', '-f=' + INVENTORY_FORMAT)
        finally:
            command('docker', 'stop', ident)
    if inventory(actual) != expected:
        raise ValueError('Consumer package inventory differs from the producer')
    report = {'variant': variant, 'image_id': data['image_id'], 'seconds': events,
              'compressed_bytes': data['compressed_bytes'], 'inventory_verified': True,
              'download_and_restore_seconds': 'join Actions step timings; excluded from phase sum',
              'comparison_limit': 'stock installs the same complete locked inventory in one transaction; not an exact model of split CI bootstrap',
              'run_id': os.environ.get('GITHUB_RUN_ID'), 'revision': os.environ.get('GITHUB_SHA')}
    (root / f'{variant}-measurement.json').write_text(json.dumps(report, indent=2) + '\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('produce', 'stock', 'prepared'))
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--archives', required=True, type=Path)
    parser.add_argument('--packages', type=Path)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('Docker phases require a disposable GitHub runner; local builds use bosn')
    args.bundle.mkdir(parents=True, exist_ok=True)
    args.archives.mkdir(parents=True, exist_ok=True)
    if args.phase == 'produce':
        if args.packages is None:
            parser.error('produce requires --packages')
        produce(args.bundle, args.packages, args.archives)
    else:
        measure(args.bundle, args.phase, args.archives)


if __name__ == '__main__':
    main()
