#!/usr/bin/env -S uv run --script
"""GitHub release asset adapter for the bounded native-cache protocol.

Writes require an explicitly supplied job token and a writable instance. The
caller enforces trusted-master context; this module does not select workflows.
"""

import json
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = 'obs-rust/obs-studio'
API = 'https://api.github.com/repos/' + REPOSITORY
UPLOADS = 'https://uploads.github.com/repos/' + REPOSITORY
MARKER = 'native-cache-protocol-v1'
ASSET_NAME = re.compile(r'native-[0-9a-f]{64}-[0-9a-f]{40}\.(?:tar\.gz|json)')
MAX_JSON_BYTES = 1 << 20
MAX_ASSET_BYTES = (2 << 30) - 1


class ReleaseStore:
    def __init__(self, identity, token='', writable=False):
        axis = (identity.get('platform'), identity.get('architecture'))
        if axis not in {('windows', 'x64'), ('windows', 'arm64'), ('macos', 'x86_64'), ('macos', 'arm64')}:
            raise ValueError('unsupported native release axis')
        if writable and not token:
            raise ValueError('publication requires explicit job token')
        self.tag = 'ci-native-cache-v1-' + '-'.join(axis)
        self.token = token
        self.writable = writable
        self.release = None

    def require_write(self):
        if not self.writable:
            raise ValueError('read-only native release store')

    def api(self, url, method='GET', data=None, content_type='application/json', size=None):
        if not (url.startswith(API + '/') or url.startswith(UPLOADS + '/')):
            raise ValueError('unexpected GitHub API endpoint')
        headers = {'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28',
                   'User-Agent': 'obs-native-cache-v1', 'Content-Type': content_type}
        if self.token:
            headers['Authorization'] = 'Bearer ' + self.token
        if size is not None:
            headers['Content-Length'] = str(size)
        request = urllib.request.Request(url, data=data, headers=headers, method=method)
        with urllib.request.urlopen(request, timeout=60) as response:
            body = response.read(MAX_JSON_BYTES + 1)
        if len(body) > MAX_JSON_BYTES:
            raise ValueError('GitHub API response budget exceeded')
        return json.loads(body) if body else None

    def validate_release(self, release):
        if (not isinstance(release, dict) or type(release.get('id')) is not int
                or release['id'] <= 0 or release.get('tag_name') != self.tag
                or release.get('prerelease') is not True or release.get('draft') is not False
                or release.get('body') != MARKER):
            raise ValueError('release is not an owned native cache prerelease')
        return release

    def lookup(self):
        if self.release is not None:
            return self.release
        try:
            release = self.api(API + '/releases/tags/' + self.tag)
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return None
            raise
        self.release = self.validate_release(release)
        return self.release

    def ensure(self, source_sha):
        self.require_write()
        if not re.fullmatch('[0-9a-f]{40}', source_sha):
            raise ValueError('invalid native cache producer identity')
        if self.lookup() is None:
            body = {'tag_name': self.tag, 'target_commitish': source_sha,
                    'name': 'Native compiler cache: ' + self.tag, 'body': MARKER,
                    'draft': False, 'prerelease': True, 'make_latest': 'false'}
            self.release = self.validate_release(self.api(API + '/releases', 'POST', json.dumps(body).encode()))

    def assets(self):
        release = self.lookup()
        if release is None:
            return []
        assets = self.api(API + '/releases/' + str(release['id']) + '/assets?per_page=100')
        # Protocol limit is six, so a full page already exceeds admission; no
        # hidden subsequent pages may escape accounting.
        if not isinstance(assets, list) or len(assets) > 6:
            raise ValueError('native release asset count exceeded')
        for asset in assets:
            if (not isinstance(asset, dict) or not ASSET_NAME.fullmatch(str(asset.get('name', '')))
                    or type(asset.get('id')) is not int or asset['id'] <= 0
                    or type(asset.get('size')) is not int or not 0 <= asset['size'] <= MAX_ASSET_BYTES):
                raise ValueError('invalid native release asset')
        return assets

    def asset(self, name):
        if not ASSET_NAME.fullmatch(name):
            raise ValueError('invalid native release asset name')
        matches = [asset for asset in self.assets() if asset['name'] == name]
        if len(matches) != 1:
            raise ValueError('missing or duplicate native release asset')
        return matches[0]

    def upload(self, path):
        self.require_write()
        path = Path(path)
        if not ASSET_NAME.fullmatch(path.name) or not 0 < path.stat().st_size <= MAX_ASSET_BYTES:
            raise ValueError('invalid native release upload')
        release = self.lookup()
        if release is None:
            raise ValueError('native release must be ensured before upload')
        url = UPLOADS + '/releases/' + str(release['id']) + '/assets?name=' + urllib.parse.quote(path.name)
        with path.open('rb') as stream:
            return self.api(url, 'POST', stream, 'application/octet-stream', path.stat().st_size)

    def delete(self, name):
        self.require_write()
        asset = self.asset(name)
        self.api(API + '/releases/assets/' + str(asset['id']), 'DELETE')

    def download(self, name, destination):
        asset = self.asset(name)
        destination = Path(destination)
        # Public release URLs carry no Authorization header, including through
        # GitHub's object-storage redirects. API digests are verified by caller.
        url = 'https://github.com/' + REPOSITORY + '/releases/download/' + self.tag + '/' + name
        request = urllib.request.Request(url, headers={'User-Agent': 'obs-native-cache-v1'})
        owned = False
        complete = False
        try:
            with destination.open('xb') as stream:
                owned = True
                with urllib.request.urlopen(request, timeout=60) as response:
                    count = 0
                    for chunk in iter(lambda: response.read(1 << 20), b''):
                        count += len(chunk)
                        if count > asset['size']:
                            raise ValueError('native release download size exceeded')
                        stream.write(chunk)
                if count != asset['size']:
                    raise ValueError('native release download truncated')
            complete = True
        finally:
            if owned and not complete:
                destination.unlink()
