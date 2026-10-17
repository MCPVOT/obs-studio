#!/usr/bin/env -S uv run --script
"""Retain selected paths when pending master Push builds are coalesced."""
import json
import subprocess


def scope_base(run=subprocess.run):
    try:
        result = run(
            ['gh', 'run', 'list', '--workflow', 'push.yaml', '--branch', 'master',
             '--event', 'push', '--status', 'success', '--limit', '30',
             '--json', 'headSha'], check=True, capture_output=True, text=True,
        )
        for item in json.loads(result.stdout):
            sha = item['headSha']
            if len(sha) != 40 or any(c not in '0123456789abcdef' for c in sha):
                continue
            if run(['git', 'merge-base', '--is-ancestor', sha, 'HEAD'],
                   capture_output=True).returncode == 0:
                return sha
    except (subprocess.CalledProcessError, ValueError, KeyError):
        pass
    # No trustworthy baseline: compare the entire tree. Never silently drop
    # paths because the API is unavailable or recent runs were unsuccessful.
    return 'ALL'


if __name__ == '__main__':
    print(scope_base())
