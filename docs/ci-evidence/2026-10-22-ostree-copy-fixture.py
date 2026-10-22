#!/usr/bin/env -S uv run --script
"""Real OSTree object-copy/fsck boundary fixture; never builds OBS."""

import hashlib
import importlib.util
import json
import os
import shutil
import sys
from pathlib import Path
import subprocess
import tempfile

TASK = Path(sys.argv[1]).resolve(strict=True)
spec = importlib.util.spec_from_file_location('probe', Path(__file__).resolve().parents[2] / 'ci/flatpak_pruned_build_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

def command(args):
    return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True).stdout

class StopAfterFsck(Exception):
    pass

with tempfile.TemporaryDirectory(prefix='ostree-real-fixture-', dir=TASK) as directory:
    root = Path(directory)
    state, tree = root / 'state', root / 'tree'
    (state / 'cache').mkdir(parents=True)
    tree.mkdir()
    target = root / 'original-target'
    target.write_text('original target sentinel')
    (tree / 'payload').write_text('dependency payload')
    (tree / 'link').symlink_to(target)
    command(['ostree', '--repo=' + str(state / 'cache'), 'init', '--mode=bare'])
    command(['ostree', '--repo=' + str(state / 'cache'), 'commit', '--branch=dependency', '--subject=fixture', '--tree=dir=' + str(tree)])
    command(['ostree', '--repo=' + str(state / 'cache'), 'fsck'])
    links = [path for path in (state / 'cache/objects').rglob('*') if path.is_symlink()]
    assert links
    config_before = (state / 'cache/config').read_bytes()
    manifest = root / 'manifest.json'
    manifest.write_text(json.dumps({'id': 'com.obsproject.Studio', 'command': 'obs', 'modules': [{'name': 'dependency'}, {'name': 'obs'}]}))
    source_ref = probe.cache_branch(manifest) + '/build-dependency'
    command(['ostree', '--repo=' + str(state / 'cache'), 'refs', '--create=' + source_ref, 'dependency'])
    original_commit = command(['ostree', '--repo=' + str(state / 'cache'), 'rev-parse', source_ref]).strip()
    recipe_before = manifest.read_bytes()
    checked = []
    def execute(args, seconds):
        if args[0] == 'ostree':
            checked.append(args)
            return command(args)
        assert 'flatpak-builder' in args
        copied = Path(args[args.index('--state-dir') + 1])
        selected_ref = probe.cache_branch(args[-1]) + '/build-dependency'
        assert selected_ref != source_ref
        assert command(['ostree', '--repo=' + str(copied / 'cache'), 'rev-parse', selected_ref]).strip() == original_commit
        assert not (state / 'cache/refs/heads' / selected_ref).exists()
        for link in links:
            copied_link = copied / link.relative_to(state)
            assert copied_link.is_symlink()
            assert os.readlink(copied_link) == os.readlink(link)
        (copied / 'cache/config').write_text('isolated mutation')
        assert (state / 'cache/config').read_bytes() == config_before
        raise StopAfterFsck
    try:
        probe.measure(state, manifest, root, execute=execute)
        raise AssertionError('fixture must stop before actual application build')
    except StopAfterFsck:
        pass
    assert any('fsck' in args for args in checked)
    assert not list(root.glob('flatpak-pruned-build-*'))
    assert not list(root.glob('flatpak-pruned-*.json'))
    assert manifest.read_bytes() == recipe_before
    assert target.read_text() == 'original target sentinel'
    command(['ostree', '--repo=' + str(state / 'cache'), 'fsck'])
    binary = Path(shutil.which('ostree')).resolve(strict=True)
    result = {'ostree_binary_path': str(binary), 'ostree_binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'status': 'success', 'scope': 'real OSTree copy/fsck boundary only; stops before application build', 'repository_mode': 'bare', 'copied_link_objects': len(links), 'remapped_dependency_ref_verified': True, 'original_config_and_link_target_preserved': True, 'temporary_state_and_manifest_cleaned': True, 'ostree_version': command(['ostree', '--version']).strip()}
    (TASK / 'ostree-copy-fixture.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
