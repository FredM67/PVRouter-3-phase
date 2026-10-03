// The default choices must give back the shipped firmware files, byte for byte: a change to
// one of them fails here until the configurator follows.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const M = require('../model.js');
const G = require('../generate.js');

const repo = path.join(__dirname, '..', '..');
// the pre-commit hook rewrites @date, so it is left out of the comparison
const shipped = (file) => fs.readFileSync(path.join(repo, file), 'utf8').replace(/^( \* @date ).*$/m, '$1');
const generated = (text) => text.replace(/^( \* @date ).*$/m, '$1');

test('defaults give back config.h', () => {
  assert.strictEqual(generated(G.configH(M.defaults())), shipped('Mk2_3phase_RFdatalog_temp/config.h'));
});

test('defaults give back config_system.h', () => {
  assert.strictEqual(generated(G.configSystemH(M.defaults())), shipped('Mk2_3phase_RFdatalog_temp/config_system.h'));
});

test('defaults give back config_rf.h', () => {
  assert.strictEqual(generated(G.configRfH(M.defaults())), shipped('Mk2_3phase_RFdatalog_temp/config_rf.h'));
});

// the shipped receiver is unit 1, with 2 loads on D4 and D3
function oneRemoteUnit() {
  const m = M.defaults();
  m.loads.push({ type: 'remote', unit: 1, led: null }, { type: 'remote', unit: 1, led: null });
  m.priorities = m.loads.map((_, i) => i);
  return m;
}

test('defaults give back RemoteLoadReceiver/config.h', () => {
  assert.strictEqual(generated(G.receiverConfigH(oneRemoteUnit(), 1)), shipped('RemoteLoadReceiver/config.h'));
});

test('defaults give back RemoteLoadReceiver/config_rf.h', () => {
  assert.strictEqual(generated(G.receiverConfigRfH(oneRemoteUnit(), 1)), shipped('RemoteLoadReceiver/config_rf.h'));
});

test('trailing comments are aligned within runs of consecutive lines', () => {
  const text = G.render([['int a;', '/**< a */'], ['long bb;', '// b'], '', ['c;', '/**< c */']]);
  assert.strictEqual(text, 'int a;    /**< a */\nlong bb;  // b\n\nc; /**< c */\n');
});

test('files: RF and receiver files only when needed, YAML only with mk2Wifi', () => {
  const names = (m) => G.files(m, '2026-01-01').map((f) => f.path);
  assert.deepStrictEqual(names(M.defaults()), ['Mk2_3phase_RFdatalog_temp/config.h', 'Mk2_3phase_RFdatalog_temp/config_system.h']);
  const m = oneRemoteUnit();
  m.mk2wifi.enabled = true;
  assert.deepStrictEqual(names(m), [
    'Mk2_3phase_RFdatalog_temp/config.h',
    'Mk2_3phase_RFdatalog_temp/config_system.h',
    'Mk2_3phase_RFdatalog_temp/config_rf.h',
    'RemoteLoadReceiver-unit1/config.h',
    'RemoteLoadReceiver-unit1/config_rf.h',
    'mk2pvrouter.yaml',
  ]);
});

test('relay count: new relays get the README thresholds, a single relay fits on one line', () => {
  const m = M.defaults();
  m.relays.enabled = true;
  M.resizeRelays(m, 4);
  assert.deepStrictEqual(m.relays.list[3], { pin: null, surplus: 1000, import: 200, minOn: 5, minOff: 5 });
  M.resizeRelays(m, 1);
  m.relays.list[0].pin = 8;
  assert.match(G.configH(m), /^ {37}\{ \{ 8, 100, 200, 1, 1 \} \} \}; \/\*\*< config for relay diversion/m);
});

test('motherboard: the new board selects PcbVersion::NEW', () => {
  const m = M.defaults();
  m.pcbVersion = 'NEW';
  assert.match(G.configH(m), /^inline constexpr PcbVersion PCB_VERSION\{ PcbVersion::NEW \};$/m);
});

test('override pins: a comment says what each one forces, numbered as on the page', () => {
  const m = M.defaults();
  m.loads = [
    { type: 'remote', unit: 1, led: null },
    { type: 'local', pin: 6 },
  ];
  m.priorities = [0, 1];
  m.relays.enabled = true;
  M.resizeRelays(m, 1);
  m.relays.list[0].pin = 8;
  m.overrides.list = [
    { pin: 5, wifi: false, targets: [{ load: 0 }] },
    { pin: 9, wifi: false, targets: [{ load: 1 }, { relay: 0 }] },
    { pin: 14, wifi: false, targets: 'ALL_REMOTE_LOADS' },
  ];
  const text = G.configH(m);
  assert.match(text, /^\/\/ {3}D5: load 1 \(remote unit 1\)\n\/\/ {3}D9: load 2 \(D6\), relay 1 \(D8\)\n\/\/ {3}A0: all remote loads\ninline constexpr OverridePins/m);
  assert.doesNotMatch(G.configH(M.defaults()), /What each override pin forces/);
});
