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

// One boost of the default model replaced: its line in config.h
function boostsLine(boosts) {
  const m = M.defaults();
  m.dualTariff.boosts = boosts;
  return G.configH(m)
    .split('\n')
    .find((l) => l.startsWith('inline constexpr ScheduledBoosts'));
}

test('scheduled boosts: each kind of window gives its form', () => {
  const one = (b) => boostsLine([{ targets: [{ load: 0 }], ...b }]);
  assert.match(one({ from: 'start', at: 0, for: 120 }), /scheduledBoosts\{ \{ \{ FIRST\(HOURS\(2\)\), \{ LOAD\(0\) \} \} \} \};/);
  assert.match(one({ from: 'end', at: 90, for: null }), /scheduledBoosts\{ \{ \{ LAST\(MINUTES\(90\)\), \{ LOAD\(0\) \} \} \} \};/);
  assert.match(one({ from: 'start', at: 60, for: null }), /scheduledBoosts\{ \{ \{ FROM_START\(HOURS\(1\)\), \{ LOAD\(0\) \} \} \} \};/);
  assert.match(one({ from: 'end', at: 180, for: 90 }), /scheduledBoosts\{ \{ \{ BEFORE_END\(HOURS\(3\)\), FOR\(MINUTES\(90\)\), \{ LOAD\(0\) \} \} \} \};/);
  assert.match(one({ from: 'start', at: 30, for: 60 }), /scheduledBoosts\{ \{ \{ FROM_START\(MINUTES\(30\)\), FOR\(HOURS\(1\)\), \{ LOAD\(0\) \} \} \} \};/);
});

test('scheduled boosts: an empty list, a group of targets', () => {
  assert.match(boostsLine([]), /^inline constexpr ScheduledBoosts scheduledBoosts\{\};/);
  assert.match(boostsLine([{ from: 'start', at: 0, for: 120, targets: 'ALL_RELAYS' }]), /\{ \{ \{ FIRST\(HOURS\(2\)\), ALL_RELAYS\(\) \} \} \}; \/\*\*< all relays, the first 2 hours \*\/$/);
});

test('scheduled boosts: several, one per line, a comment says what each one forces', () => {
  const m = M.defaults();
  m.dualTariff.boosts = [
    { from: 'end', at: 180, for: 120, targets: [{ load: 0 }] },
    { from: 'start', at: 0, for: 60, targets: 'ALL_LOADS' },
  ];
  assert.match(
    G.configH(m),
    /^\/\/ What each scheduled boost forces .*\n\/\/ {3}boost 1: load 1 \(D5\)\n\/\/ {3}boost 2: all loads\ninline constexpr ScheduledBoosts scheduledBoosts\{ \{ \{ BEFORE_END\(HOURS\(3\)\), FOR\(HOURS\(2\)\), \{ LOAD\(0\) \} \},\n {52}\{ FIRST\(HOURS\(1\)\), ALL_LOADS\(\) \} \} \};/m
  );
});

test('normalize: the former force list is dropped, the default boosts take its place', () => {
  const m = M.normalize({ dualTariff: { enabled: true, pin: 3, force: [{ start: -3, duration: 2 }] } });
  assert.ok(!('force' in m.dualTariff));
  assert.deepStrictEqual(m.dualTariff.boosts, M.defaults().dualTariff.boosts);
  assert.deepStrictEqual(M.defaults().dualTariff.boosts, [{ from: 'end', at: 180, for: 120, targets: [{ load: 0 }] }]);
});

const calibrationFile = () => fs.readFileSync(path.join(repo, 'Mk2_3phase_RFdatalog_temp/calibration.h'), 'utf8');
// The text with one value of an array replaced: the tests follow the values of calibration.h
function withValue(text, name, phase, value) {
  const t = M.calibrationArray(text, name).tokens[phase];
  return text.slice(0, t.start) + value + text.slice(t.end);
}

test('calibration: new value = current value x meter / router, phase by phase', () => {
  const m = M.defaults();
  Object.assign(m.calibration, { ctRating: [50, 50, 30.3], powerCalPerAmp: [0.0029225, 0.0029225, 0.0029225], voltageCalNewBoard: [0.8952, 0.8952, 0.8952] });
  m.calibration.power[0] = { router: 2000, meter: 2100 };
  m.calibration.voltage[2] = { router: 232, meter: 230 };
  const r = M.calibrationResult(m);
  assert.deepStrictEqual(
    r.power.map((v) => v.toFixed(7)),
    ['0.0030686', '0.0029225', '0.0029225']
  );
  assert.strictEqual(r.voltage[2].toFixed(4), '0.8875');
  assert.deepStrictEqual(G.calibrationLines(m), [
    'inline constexpr float f_ctRating[NO_OF_PHASES]{ 50.0F, 50.0F, 30.3F };',
    'inline constexpr float f_powerCalPerAmp[NO_OF_PHASES]{ 0.0030686F, 0.0029225F, 0.0029225F };',
    'inline constexpr float f_voltageCalNewBoard[NO_OF_PHASES]{ 0.8952F, 0.8952F, 0.8875F };',
  ]);
});

test('calibration: the old board corrects its own values', () => {
  const m = M.defaults();
  m.pcbVersion = 'OLD';
  Object.assign(m.calibration, { powerCalOldBoard: [0.05, 0.05, 0.05], voltageCalOldBoard: [0.8151, 0.8184, 0.8195] });
  m.calibration.power[1] = { router: 1000, meter: 1010 };
  assert.deepStrictEqual(G.calibrationLines(m), [
    'inline constexpr float f_powerCalOldBoard[NO_OF_PHASES]{ 0.05000F, 0.05050F, 0.05000F };',
    'inline constexpr float f_voltageCalOldBoard[NO_OF_PHASES]{ 0.8151F, 0.8184F, 0.8195F };',
  ]);
  m.pcbVersion = 'NEW';
  assert.strictEqual(G.calibrationLines(m), null, 'no values of the new board yet');
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
  assert.strictEqual(out, withValue(text, 'f_powerCalPerAmp', 1, `${(m.calibration.powerCalPerAmp[1] * 1.01).toFixed(7)}F`));
  assert.ok(G.files(m).some((f) => f.path === 'Mk2_3phase_RFdatalog_temp/calibration.h' && f.text === out));
  m.calibrationMode = false;
  assert.ok(!G.files(m).some((f) => f.path.endsWith('calibration.h')), 'only in calibration mode');

  m.calibrationMode = true;
  m.calibration.power[1] = { router: null, meter: null };
  m.calibration.ctRating[2] = 30.3;
  assert.strictEqual(G.calibrationH(m), withValue(text, 'f_ctRating', 2, '30.3F'), 'a new CT rating');
  m.pcbVersion = 'OLD';
  assert.strictEqual(G.calibrationH(m), null, 'no CT rating on the old board');
  m.calibration.power[1] = { router: 1000, meter: 1010 };
  assert.strictEqual(G.calibrationH(m), withValue(text, 'f_powerCalOldBoard', 1, `${(m.calibration.powerCalOldBoard[1] * 1.01).toFixed(5)}F`), 'the old board: its own values');
});

test('calibration: a commented-out line is not the one read or rewritten', () => {
  const text = '// inline constexpr float f_powerCalPerAmp[NO_OF_PHASES]{ 1.0F, 1.0F, 1.0F };\n' + calibrationFile();
  const inFile = M.parseCalibration(calibrationFile()).powerCalPerAmp;
  assert.deepStrictEqual(M.parseCalibration(text).powerCalPerAmp, inFile);
  const m = M.defaults();
  m.calibrationMode = true;
  Object.assign(m.calibration, { file: text, ...M.parseCalibration(text) });
  m.calibration.power[0] = { router: 500, meter: 1000 };
  assert.ok(G.calibrationH(m).startsWith('// inline constexpr float f_powerCalPerAmp[NO_OF_PHASES]{ 1.0F, 1.0F, 1.0F };\n'));
  assert.strictEqual(G.calibrationH(m), withValue(text, 'f_powerCalPerAmp', 0, `${(inFile[0] * 2).toFixed(7)}F`));
  assert.strictEqual(M.parseCalibration('int a;'), null);
  // the calibration.h of an older firmware, with f_powerCal set directly, is not this one
  assert.strictEqual(M.parseCalibration('inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05F, 0.05F, 0.05F };\ninline constexpr float f_voltageCal[NO_OF_PHASES]{ 0.8F, 0.8F, 0.8F };'), null);
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

test('calibration mode: config.h and the new calibration.h, nothing else', () => {
  const m = M.defaults();
  m.loads[2] = { type: 'remote', unit: 1, led: null };
  m.calibrationMode = true;
  assert.deepStrictEqual(
    G.files(m).map((f) => f.path),
    ['Mk2_3phase_RFdatalog_temp/config.h']
  );
  const text = calibrationFile();
  Object.assign(m.calibration, { file: text, ...M.parseCalibration(text) });
  m.calibration.power[0] = { router: 1000, meter: 1020 };
  assert.deepStrictEqual(
    G.files(m).map((f) => f.path),
    ['Mk2_3phase_RFdatalog_temp/config.h', 'Mk2_3phase_RFdatalog_temp/calibration.h']
  );
});

test('temperature threshold: the sensor it is checked on', () => {
  const m = M.defaults();
  m.dualTariff.tempSensor = 1;
  assert.match(G.configH(m), /^inline constexpr uint8_t BOOST_TEMP_SENSOR\{ 1 \};/m);
});

test('removing a sensor: the boosts keep the same sensor, or fall back to the first', () => {
  const three = () => {
    const m = M.defaults();
    m.temperature.sensors = [{ address: 'A', name: '' }, { address: 'B', name: '' }, { address: 'C', name: '' }];
    return m;
  };
  let m = three();
  m.dualTariff.tempSensor = 2;
  M.removeSensor(m, 0); // one before it: same sensor, one place lower
  assert.deepStrictEqual([m.temperature.sensors.length, m.dualTariff.tempSensor], [2, 1]);
  m = three();
  m.dualTariff.tempSensor = 1;
  M.removeSensor(m, 1); // the chosen one: the first
  assert.strictEqual(m.dualTariff.tempSensor, 0);
  m = three();
  m.dualTariff.tempSensor = 0;
  M.removeSensor(m, 2); // one after it: unchanged
  assert.strictEqual(m.dualTariff.tempSensor, 0);
});

test('temperature: the router reads the probes on D3, wired on the board', () => {
  const m = M.normalize({ temperature: { mode: 'router', pin: 4 } }); // a former choice of pin is dropped
  assert.ok(!('pin' in m.temperature));
  assert.match(G.configH(m), /^inline constexpr TemperatureSensing temperatureSensing\{ 3,$/m);
});
