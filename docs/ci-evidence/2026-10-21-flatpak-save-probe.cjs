const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const assert = require('node:assert/strict');

async function run(source, save, exact, bundle) {
  const calls = {exec: [], restore: [], save: []};
  const inputs = {
    'manifest-path': 'build-aux/com.obsproject.Studio.json',
    'cache-key': 'fixture-key', arch: 'x86_64',
    'repository-url': 'https://flathub.org/repo/flathub.flatpakrepo',
    'repository-name': 'flathub',
    'mirror-screenshots-url': 'https://dl.flathub.org/media',
    cache: true, 'restore-cache': true, 'save-cache': save,
    'build-bundle': bundle, 'build-debug-bundle': false,
  };
  const context = {
    path, process: {env: {GITHUB_SHA: 'fixture'}},
    core: {getInput: key => inputs[key] ?? '', getBooleanInput: key => !!inputs[key],
      info: () => {}, error: message => {throw Error(message)}},
    exec: {exec: async (...args) => calls.exec.push(args)},
    cache: {
      restoreCache: async (...args) => {calls.restore.push(args); return exact ? 'fixture-key-x86_64' : 'older-key'},
      saveCache: async (...args) => calls.save.push(args),
    },
  };
  const constructor = source.slice(source.indexOf('class Configuration {'), source.indexOf('const startDBusSession ='));
  const methods = source.slice(source.indexOf('const build ='), source.indexOf('const run ='));
  assert(constructor && methods);
  vm.runInNewContext("const CACHE_PATH = ['.flatpak-builder'];\n" + constructor + methods +
    '\nthis.probe = {Configuration, build, prepareBuild};', context, {timeout: 1000});
  const config = new context.probe.Configuration();
  const hit = await context.probe.prepareBuild(config);
  await context.probe.build({'app-id': 'com.obsproject.Studio'}, inputs['manifest-path'], hit, config);
  // Normalize objects from the isolated VM before comparison.
  return JSON.parse(JSON.stringify(calls));
}

(async () => {
  const old = fs.readFileSync(process.argv[2], 'utf8');
  const next = fs.readFileSync(process.argv[3], 'utf8');
  const results = [];
  for (const save of [false, true]) for (const exact of [false, true]) for (const bundle of [false, true]) {
    const before = await run(old, save, exact, bundle);
    const after = await run(next, save, exact, bundle);
    assert.deepEqual(after.exec, before.exec, 'build and bundle commands must match');
    assert.deepEqual(after.restore, before.restore, 'restore contract must match');
    assert(after.exec[0][1].includes('--ccache'));
    assert.equal(after.save.length, save && !exact ? 1 : 0);
    assert.equal(before.save.length, !exact ? 1 : 0, 'old action ignores independent save setting');
    results.push({save, exact, bundle, before_saves: before.save.length, after_saves: after.save.length});
  }
  console.log(JSON.stringify({scenarios: results, actual_build_executed: false,
    limits: 'Actual pinned Configuration/build/prepareBuild bodies, injected cache/exec adapters; this is not a packaged app runtime test.'}, null, 2));
})().catch(error => {console.error(error); process.exitCode = 1});
