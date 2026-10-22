const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const source = fs.readFileSync(process.argv[2], 'utf8');
let entries = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
assert.equal(crypto.createHash('sha256').update(source).digest('hex'),
  '676fd5f2f7dd0116164a1db0d3abb1a8cb3f5488d764a64f5b4a8e126e05bcae');
const entry = entries.find(e => e.ref === 'refs/heads/master');
assert(entry && entry.key.endsWith('-x86_64'));
const current = entry.key.replace(/-x86_64$/, '');
const changed = current.replace(/[a-f0-9]{64}/, '0'.repeat(64));
assert.notEqual(current, changed);
async function probe(custom) {
  const calls = [];
  const inputs = {'cache-key': custom, arch: 'x86_64', cache: true,
    'restore-cache': true, 'save-cache': false,
    'repository-url': 'https://flathub.org/repo/flathub.flatpakrepo',
    'repository-name': 'flathub'};
  const context = {path, process: {env: {}},
    core: {getInput: k => inputs[k] ?? '', getBooleanInput: k => !!inputs[k],
      info: () => {}, error: m => {throw Error(m)}},
    exec: {exec: async () => {}},
    cache: {restoreCache: async (paths, key, prefixes) => {
      const exact = entries.find(e => e.key === key);
      const hit = exact ?? prefixes.flatMap(prefix => entries.filter(e => e.key.startsWith(prefix)))[0];
      calls.push({paths, primary: key, restore_prefixes: prefixes, matched_key: hit?.key ?? null});
      return hit?.key;
    }}};
  const constructor = source.slice(source.indexOf('class Configuration {'), source.indexOf('const startDBusSession ='));
  const methods = source.slice(source.indexOf('const build ='), source.indexOf('const run ='));
  vm.runInNewContext("const CACHE_PATH = ['.flatpak-builder'];\n" + constructor + methods +
    '\nthis.probe = {Configuration, prepareBuild};', context, {timeout: 1000});
  await context.probe.prepareBuild(new context.probe.Configuration());
  return JSON.parse(JSON.stringify(calls[0]));
}
(async () => {
  const legacyEntries = entries;
  const hash = current.match(/[a-f0-9]{64}/)[0];
  const candidate = `flatpak-builder-x86_64-${hash}`;
  const beforePublication = await probe(candidate);
  assert.equal(beforePublication.matched_key, null);
  const bridge = legacyEntries.find(e => e.key === `${current}-x86_64`);
  assert(bridge);
  entries = [{...bridge, key: `${candidate}-x86_64`}, ...legacyEntries];
  const afterPublication = await probe(candidate);
  assert.equal(afterPublication.matched_key, `${candidate}-x86_64`);
  const changed = await probe(candidate.replace(hash, '0'.repeat(64)));
  assert.equal(changed.matched_key, `${candidate}-x86_64`);
  console.log(JSON.stringify({beforePublication, legacyBridge: bridge.key,
    afterPublication, changedManifest: changed,
    limits: 'Candidate namespace using actual pinned action bodies and synthetic publication. No real cache transport, workflow change or build.'}, null, 2));
})().catch(error => {console.error(error); process.exitCode = 1});
