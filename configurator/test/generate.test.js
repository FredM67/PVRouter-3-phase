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

const calibrationFile = () => fs.readFileSync(path.join(repo, 'Mk2_3phase_RFdatalog_temp/calibration.h'), 'utf8');

test('calibration: new value = current value x meter / router, phase by phase', () => {
  const m = M.defaults();
  Object.assign(m.calibration, { powerCal: [0.05, 0.05, 0.05], voltageCal: [0.8151, 0.8184, 0.8195] });
  m.calibration.power[0] = { router: 2000, meter: 2100 };
  m.calibration.voltage[2] = { router: 232, meter: 230 };
  const r = M.calibrationResult(m);
  assert.deepStrictEqual(
    r.powerCal.map((v) => v.toFixed(5)),
    ['0.05250', '0.05000', '0.05000']
  );
  assert.strictEqual(r.voltageCal[2].toFixed(4), '0.8124');
  assert.deepStrictEqual(G.calibrationLines(m), [
    'inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05250F, 0.05000F, 0.05000F };',
    'inline constexpr float f_voltageCal[NO_OF_PHASES]{ 0.8151F, 0.8184F, 0.8124F };',
  ]);
});

test('calibration: the loaded calibration.h gets only the changed values', () => {
  const m = M.defaults();
  m.calibrationMode = true;
  const text = calibrationFile();
  Object.assign(m.calibration, { file: text, ...M.parseCalibration(text) });
  assert.strictEqual(G.calibrationH(m), null, 'nothing changes without readings');
  assert.ok(!G.files(m).some((f) => f.path.endsWith('calibration.h')));

  m.calibration.power[1] = { router: 1000, meter: 1010 };
  const out = G.calibrationH(m);
  assert.strictEqual(out, text.replace('{ 0.05000F, 0.05000F, 0.05000F }', '{ 0.05000F, 0.05050F, 0.05000F }'));
  assert.ok(G.files(m).some((f) => f.path === 'Mk2_3phase_RFdatalog_temp/calibration.h' && f.text === out));
  m.calibrationMode = false;
  assert.ok(!G.files(m).some((f) => f.path.endsWith('calibration.h')), 'only in calibration mode');
});

test('calibration: a commented-out line is not the one read or rewritten', () => {
  const text = '// inline constexpr float f_powerCal[NO_OF_PHASES]{ 1.0F, 1.0F, 1.0F };\n' + calibrationFile();
  assert.deepStrictEqual(M.parseCalibration(text).powerCal, [0.05, 0.05, 0.05]);
  const m = M.defaults();
  m.calibrationMode = true;
  Object.assign(m.calibration, { file: text, ...M.parseCalibration(text) });
  m.calibration.power[0] = { router: 500, meter: 1000 };
  assert.ok(G.calibrationH(m).startsWith('// inline constexpr float f_powerCal[NO_OF_PHASES]{ 1.0F, 1.0F, 1.0F };\n'));
  assert.match(G.calibrationH(m), /^inline constexpr float f_powerCal\[NO_OF_PHASES\]\{ 0\.10000F, 0\.05000F, 0\.05000F \};/m);
  assert.strictEqual(M.parseCalibration('int a;'), null);
});

test('calibration mode: human-readable output and no YAML, the usual choice kept', () => {
  const m = M.defaults();
  m.serialOutput = 'IoT';
  m.mk2wifi.enabled = true;
  m.calibrationMode = true;
  assert.match(G.configH(m), /SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;/);
  assert.ok(!G.files(m).some((f) => f.path.endsWith('.yaml')));
  assert.strictEqual(m.serialOutput, 'IoT');
  m.calibrationMode = false;
  assert.match(G.configH(m), /SERIAL_OUTPUT_TYPE = SerialOutputType::IoT;/);
  assert.ok(G.files(m).some((f) => f.path.endsWith('.yaml')));
});
