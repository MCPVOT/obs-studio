#!/usr/bin/env -S uv run --script
"""Bind the temporary Actions transport seed to its native producer profile."""

import argparse
import importlib.util
import json
from pathlib import Path
import re

SPEC = importlib.util.spec_from_file_location('native_cache_transport', Path(__file__).with_name('native_cache_transport.py'))
TRANSPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRANSPORT)


def seed_key(identity, source_sha):
    fingerprint = TRANSPORT.profile(identity)
    if not re.fullmatch('[0-9a-f]{40}', source_sha):
        raise ValueError('native seed requires a complete source identity')
    configuration = identity['configuration']
    if configuration not in ('Release', 'RelWithDebInfo', 'Debug', 'MinSizeRel'):
        raise ValueError('unsupported native seed configuration')
    family = 'windows-ccache' if identity['platform'] == 'windows' else 'macOS-xcode'
    return f'{family}-{identity["architecture"]}-{configuration}-{source_sha}-transport-{fingerprint}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity', required=True, type=Path)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--output', required=True, type=Path, help='Append the validated suffix to the Actions step output file')
    args = parser.parse_args()
    with args.identity.open('rb') as stream:
        encoded = stream.read((64 << 10) + 1)
    if len(encoded) > 64 << 10:
        raise ValueError('identity document budget exceeded')
    identity = json.loads(encoded)
    key = seed_key(identity, args.source_sha)
    family = 'windows-ccache' if identity['platform'] == 'windows' else 'macOS-xcode'
    with args.output.open('a') as output:
        output.write('suffix=' + key.removeprefix(family + '-') + '\n')


if __name__ == '__main__':
    main()
