// Each rule rejects a broken configuration; the defaults and the presets pass.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const M = require('../model.js');
const V = require('../validate.js');

const keys = (m) => V.validate(m).map((p) => p.key);
const errors = (m) => V.validate(m).filter((p) => p.level === 'error');
const variant = (change) => {
  const m = M.defaults();
  change(m);
  return m;
};
const sensors = (n) => Array.from({ length: n }, (_, i) => ({ ...M.defaults().temperature.sensors[0], address: M.defaults().temperature.sensors[i % 2].address }));
const remote = (unit) => ({ type: 'remote', unit, led: null });
// dual tariff on, 8 hours of off-peak, one boost (load 1 unless told otherwise)
const boosts = (m, b) => Object.assign(m.dualTariff, { enabled: true, pin: 3, offPeakHours: 8, boosts: [{ targets: [{ load: 0 }], ...b }] });

test('the defaults have no problem', () => {
  assert.deepStrictEqual(V.validate(M.defaults()), []);
});

test('the presets have no error', () => {
  const dir = path.join(__dirname, 'presets');
  for (const f of fs.readdirSync(dir)) {
    const m = M.normalize(JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8')));
    assert.deepStrictEqual(errors(m), [], f);
  }
});

const CASES = [
  ['err.pcbVersion', (m) => (m.pcbVersion = 'AVCC')],
  ['err.frequency', (m) => (m.supplyFrequency = 55)],
  ['err.datalogPeriod', (m) => (m.datalogPeriod = 41)],
  ['err.diversionStart', (m) => (m.diversionStartThreshold = -1)],
  ['err.noLoad', (m) => M.resizeLoads(m, 0)],
  ['err.loadPin', (m) => (m.loads[0].pin = 1)],
  ['err.loadPin', (m) => (m.loads[0].pin = 14)],
  ['err.loadUnit', (m) => (m.loads[0] = remote(4))],
  ['err.loadLed', (m) => (m.loads[0] = { type: 'remote', unit: 1, led: 1 })],
  [
    'err.tooManyRemote',
    (m) => {
      m.loads = Array.from({ length: 9 }, (_, i) => remote((i % 3) + 1));
      m.priorities = m.loads.map((_, i) => i);
    },
  ],
  ['err.priorities', (m) => (m.priorities = [0, 0, 1])],
  ['err.priorities', (m) => (m.priorities = [0, 1])],
  ['err.diversionPin', (m) => (m.diversion.enabled = true)],
  ['err.routerOffPin', (m) => (m.routerOff = { enabled: true, pin: 0, wifi: false })],
  ['err.rotationPin', (m) => (m.rotation.mode = 'PIN')],
  ['err.watchdogPin', (m) => (m.watchdog.enabled = true)],
  ['err.dualTariffPin', (m) => (m.dualTariff.enabled = true)],
  ['err.rotationAfter', (m) => Object.assign(m.rotation, { mode: 'AUTO', groups: [[0, 1, 2]], afterSeconds: 65535 })],
  // rotation groups: lists of load indices, checked as validation.h does
  ['err.rotationNoGroup', (m) => (m.rotation.mode = 'AUTO')],
  ['err.rotationGroupSmall', (m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[0]])],
  ['err.rotationLoadTwice', (m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[0, 1], [1, 2]])],
  ['err.rotationLoadTwice', (m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[0, 0, 1]])],
  ['err.rotationGroupMixed', (m) => (m.loads[2] = remote(1)) && (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[1, 2]])],
  ['err.rotationGroupMixed', (m) => (m.loads[1] = remote(1)) && (m.loads[2] = remote(2)) && (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[1, 2]])],
  ['err.rotationNotALoad', (m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[0, 3]])], // a load removed after the groups were set
  ['err.noRelay', (m) => Object.assign(m.relays, { enabled: true, list: [] })],
  ['err.relayPin', (m) => (m.relays.enabled = true)],
  ['err.datalogDivides60', (m) => (m.relays.enabled = true) && (m.datalogPeriod = 7)],
  ['err.filterTooShort', (m) => (m.relays.enabled = true) && (m.relays.filterDelay = 1) && (m.datalogPeriod = 15)],
  ['err.relaySurplus', (m) => (m.relays.enabled = true) && (m.relays.list[0].surplus = 0)],
  ['err.relayMinTime', (m) => (m.relays.enabled = true) && (m.relays.list[0].minOn = 2000)],
  ['err.overridePin', (m) => (m.overrides.list[0].pin = null)],
  ['err.overrideTargets', (m) => (m.overrides.list[0].targets = [])],
  ['err.overrideNoRelay', (m) => (m.overrides.list[0].targets = 'ALL_RELAYS')],
  ['err.overrideNoRelay', (m) => (m.overrides.list[0].targets = [{ relay: 0 }])],
  ['err.overrideLoad', (m) => (m.overrides.list[0].targets = [{ load: 3 }])],
  ['warn.overrideNothing', (m) => (m.overrides.list[0].targets = 'ALL_REMOTE_LOADS')],
  ['err.offPeakHours', (m) => Object.assign(m.dualTariff, { enabled: true, pin: 3, offPeakHours: 13 })],
  // start of a boost: hours from -24 to 24, minutes beyond; 8 hours of off-peak
  ['err.boostWindow', (m) => boosts(m, { from: 'end', at: 540, for: null })], // 9 h before the end of 8 h
  ['err.boostWindow', (m) => boosts(m, { from: 'start', at: 480, for: 60 })], // at the end
  ['err.boostWindow', (m) => boosts(m, { from: 'start', at: 0, for: 0 })], // lasts 0 min
  ['err.boostWindow', (m) => boosts(m, { from: 'start', at: 0, for: 600 })], // FIRST(HOURS(10)): ends after the end
  ['err.boostWindow', (m) => boosts(m, { from: 'end', at: 60, for: 120 })], // BEFORE_END(HOURS(1)), FOR(HOURS(2))
  ['err.boostWindow', (m) => boosts(m, { from: 'start', at: 420, for: 61 })], // FROM_START(HOURS(7)), FOR(MINUTES(61))
  ['err.boost', (m) => boosts(m, { from: 'start', at: null, for: 60 })],
  ['err.boost', (m) => boosts(m, { from: 'start', at: 0, for: 721 })], // more than 12 h, the longest off-peak period
  ['err.boostNoRelay', (m) => boosts(m, { from: 'start', at: 0, for: 60, targets: [{ relay: 0 }] })],
  ['err.boostNoRelay', (m) => boosts(m, { from: 'start', at: 0, for: 60, targets: 'ALL_RELAYS' })],
  ['err.boostRelay', (m) => (m.relays.enabled = true) && boosts(m, { from: 'start', at: 0, for: 60, targets: [{ relay: 3 }] })],
  ['err.boostLoad', (m) => boosts(m, { from: 'start', at: 0, for: 60, targets: [{ load: 5 }] })],
  ['err.boostTargets', (m) => boosts(m, { from: 'start', at: 0, for: 60, targets: [] })],
  ['warn.boostNothing', (m) => boosts(m, { from: 'start', at: 0, for: 60, targets: 'ALL_REMOTE_LOADS' })],
  // dual tariff off: the boosts are still written into config.h, their values must be numbers
  ['err.boost', (m) => (m.dualTariff.boosts = [{ from: 'end', at: null, for: 120, targets: [{ load: 0 }] }])],
  ['err.noSensor', (m) => Object.assign(m.temperature, { mode: 'router', sensors: [] })],
  ['err.tempRoundTooLong', (m) => Object.assign(m, { datalogPeriod: 1 }) && Object.assign(m.temperature, { mode: 'router', sensors: sensors(4) })],
  ['err.tempRoundTooLong', (m) => Object.assign(m, { datalogPeriod: 1, supplyFrequency: 60 }) && Object.assign(m.temperature, { mode: 'router', sensors: sensors(5) })],
  ['err.tempThreshold', (m) => (m.temperature.mode = 'router') && Object.assign(m.dualTariff, { enabled: true, pin: 9, tempThreshold: 0 })],
  ['warn.sensorSomeAddresses', (m) => (m.mk2wifi.enabled = true) && (m.temperature.mode = 'esp') && (m.temperature.sensors[1].address = '')],
  ['err.tempSensor', (m) => (m.temperature.mode = 'router') && (m.dualTariff.tempSensor = 2)], // 2 sensors: 0 or 1
  ['err.sensorAddress', (m) => (m.temperature.mode = 'router') && (m.temperature.sensors[0].address = '28BE41')],
  ['warn.sensorCrc', (m) => (m.temperature.mode = 'router') && (m.temperature.sensors[0].address = '28BE416B090000A5')],
  ['err.tempEspNoWifi', (m) => (m.temperature.mode = 'esp')],
  ['err.duplicatePin', (m) => (m.overrides.list[0].pin = 5)],
  ['err.duplicatePin', (m) => (m.temperature.mode = 'router') && Object.assign(m.dualTariff, { enabled: true, pin: 3 })], // new board: probes on D3
  ['err.duplicatePin', (m) => (m.pcbVersion = 'OLD') && Object.assign(m.temperature, { mode: 'router', pin: 4 })], // old board: the chosen pin
  ['err.tempPin', (m) => (m.pcbVersion = 'OLD') && Object.assign(m.temperature, { mode: 'router', pin: null })],
  ['err.duplicatePin', (m) => Object.assign(m.dualTariff, { enabled: true, pin: 6 })],
  ['err.duplicatePin', (m) => (m.relays.enabled = true) && m.relays.list.forEach((r) => (r.pin = 8))],
  ['err.rfPin', (m) => (m.rf.logging = true) && (m.overrides.list[0].pin = 10)],
  ['err.networkId', (m) => Object.assign(m.rf, { logging: true, networkId: 251 })],
  ['err.routerId', (m) => Object.assign(m.rf, { logging: true, routerId: 31 })],
  ['err.powerLevel', (m) => Object.assign(m.rf, { logging: true, powerLevel: 32 })],
  ['err.gatewayId', (m) => Object.assign(m.rf, { logging: true, gatewayId: 10 })],
  ['err.remoteId', (m) => (m.loads[2] = remote(1)) && (m.rf.remoteIds[0] = 10)],
  ['err.remoteId', (m) => (m.loads[1] = remote(1)) && (m.loads[2] = remote(2)) && (m.rf.remoteIds[1] = 15)],
  ['warn.unitGap', (m) => (m.loads[2] = remote(2))],
  ['err.unitPin', (m) => (m.loads[2] = remote(1)) && (m.units[0].loadPins = [10])],
  ['err.unitDuplicate', (m) => (m.loads[2] = remote(1)) && (m.units[0].loadPins = [5])],
  [
    'err.iotLoads',
    (m) => {
      m.serialOutput = 'IoT';
      m.loads = [2, 3, 4, 5, 6, 7, 8, 9, 11, 12].map((pin) => ({ type: 'local', pin }));
      m.priorities = m.loads.map((_, i) => i);
      m.overrides.enabled = false;
    },
  ],
  ['err.deviceName', (m) => Object.assign(m.mk2wifi, { enabled: true, name: 'Mk2 Router' }) && (m.serialOutput = 'IoT')],
  ['warn.wifiNotIoT', (m) => (m.mk2wifi.enabled = true) && (m.serialOutput = 'HumanReadable')],
  ['err.wifiPin', (m) => (m.mk2wifi.enabled = true) && (m.diversion = { enabled: true, pin: 3, wifi: true })],
  ['warn.wifiKeepOpen', (m) => (m.mk2wifi.enabled = true) && (m.serialOutput = 'IoT')],
  ['warn.calibration', (m) => (m.calibrationMode = true)],
  ['warn.calProbe', (m) => (m.calibrationMode = true) && (m.calibration.power[0] = { router: -1500, meter: 1500 })],
  ['warn.calCurrent', (m) => (m.calibrationMode = true) && (m.calibration.power[0] = { router: 1500, meter: 1500 })],
  ['warn.calLowPower', (m) => (m.calibrationMode = true) && (m.calibration.powerCalPerAmp = [0.0029225, 0.0029225, 0.0029225]) && (m.calibration.power[1] = { router: 100, meter: 105 })],
  ['warn.calRatio', (m) => (m.calibrationMode = true) && (m.calibration.voltageCalNewBoard = [0.8952, 0.8952, 0.8952]) && (m.calibration.voltage[2] = { router: 100, meter: 230 })],
  ['warn.calReading', (m) => (m.calibrationMode = true) && (m.calibration.voltage[0] = { router: 230, meter: 0 })],
  ['warn.calCtRating', (m) => (m.calibrationMode = true) && Object.assign(m.calibration, { file: 'x', ctRating: [50, 0, 50] })],
];

CASES.forEach(([key, change], i) => {
  test(`${key} (case ${i + 1})`, () => {
    const m = variant(change);
    assert.ok(keys(m).includes(key), JSON.stringify(V.validate(m)));
  });
});

test('dual tariff: boosts that start within the off-peak period are accepted', () => {
  for (const b of [
    { from: 'end', at: 180, for: 120 }, // config.h, 3 h before the end
    { from: 'end', at: 480, for: null }, // from the very start
    { from: 'start', at: 0, for: 480 }, // FIRST(HOURS(8)): ends at the end
    { from: 'start', at: 420, for: 60 }, // FROM_START(HOURS(7)), FOR(HOURS(1))
    { from: 'start', at: 479, for: 1 }, // 1 min before the end
    { from: 'end', at: 1, for: null }, // the last minute
  ]) {
    const m = variant((m) => boosts(m, b));
    assert.deepStrictEqual(errors(m), [], JSON.stringify(b));
  }
  assert.deepStrictEqual(errors(variant((m) => Object.assign(m.dualTariff, { enabled: true, pin: 3, boosts: [] }))), []);
  // 12 h, the longest off-peak period and the longest boost
  assert.deepStrictEqual(errors(variant((m) => boosts(m, { from: 'start', at: 0, for: 720 }) && (m.dualTariff.offPeakHours = 12))), []);
});

test('dual tariff off: the windows of the boosts are not checked, nor what a group finds to force', () => {
  const m = variant((m) => (m.dualTariff.boosts = [{ from: 'start', at: 600, for: 0, targets: 'ALL_REMOTE_LOADS' }]));
  assert.deepStrictEqual(V.validate(m), []);
});

test('temperature: a round of reads that fits the datalog period is accepted (temp_schedule.h)', () => {
  const ok = (f, n, period, mode) => !keys(variant((m) => Object.assign(m, { datalogPeriod: period, supplyFrequency: f }) && Object.assign(m.temperature, { mode: mode || 'router', sensors: sensors(n) }))).includes('err.tempRoundTooLong');
  assert.ok(ok(50, 3, 1));
  assert.ok(!ok(50, 4, 1));
  assert.ok(ok(60, 4, 1));
  assert.ok(!ok(60, 5, 1));
  assert.ok(ok(50, 4, 2));
  assert.ok(ok(50, 6, 1, 'esp')); // the module reads the probes itself
});

test('remote units are only checked when used', () => {
  assert.deepStrictEqual(errors(variant((m) => (m.units[2].loadPins = [10]))), []);
});

test('mk2Wifi: inputs on D5-D9 driven by the module are accepted', () => {
  const m = variant((m) => {
    m.loads = [{ type: 'local', pin: 3 }];
    m.priorities = [0];
    m.overrides.list[0] = { pin: 9, wifi: true, targets: 'ALL_LOADS' };
    m.diversion = { enabled: true, pin: 8, wifi: true };
    m.mk2wifi.enabled = true;
    m.serialOutput = 'IoT';
  });
  assert.deepStrictEqual(V.validate(m), []);
});

test('calibration mode: the IoT rules and the mk2Wifi output warning do not apply', () => {
  const m = variant((m) => {
    m.serialOutput = 'JSON';
    m.mk2wifi.enabled = true;
    m.calibrationMode = true;
  });
  assert.ok(!keys(m).includes('warn.wifiNotIoT'));
});

test('the sensor of the boosts is only checked when the router reads the sensors', () => {
  assert.ok(!keys(variant((m) => (m.dualTariff.tempSensor = 5))).includes('err.tempSensor')); // no sensing
  assert.ok(!keys(variant((m) => (m.temperature.mode = 'router') && (m.dualTariff.tempSensor = 1))).includes('err.tempSensor'));
});

test('new board: the probes are on D3 whatever pin the model holds', () => {
  assert.ok(!keys(variant((m) => Object.assign(m.temperature, { mode: 'router', pin: null }))).includes('err.tempPin'));
});

test('probes on the module: an address is optional, but checked when given', () => {
  const esp = (sensors) => keys(variant((m) => (m.mk2wifi.enabled = true) && Object.assign(m.temperature, { mode: 'esp', sensors })));
  assert.ok(!esp([{ address: '', name: '' }, { address: '', name: '' }]).includes('err.sensorAddress'));
  assert.ok(esp([{ address: '28BE41', name: '' }, { address: '', name: '' }]).includes('err.sensorAddress'));
  assert.ok(esp([{ address: '28BE416B090000A5', name: '' }]).includes('warn.sensorCrc'));
});

test('dual tariff off: the targets of the boosts are not checked, the firmware compiles them', () => {
  // LOAD(), RELAY() and REMOTE_LOAD() give pin 0 out of range; validation.h checks them with dual tariff only
  const k = keys(variant((m) => (m.dualTariff.boosts = [{ from: 'end', at: 180, for: 120, targets: [{ load: 5 }, { relay: 0 }] }])));
  assert.deepStrictEqual(k, []);
});

test('no probe read by the router: the hidden threshold is not checked', () => {
  assert.ok(!keys(variant((m) => Object.assign(m.dualTariff, { enabled: true, pin: 3, tempThreshold: 0 }))).includes('err.tempThreshold'));
});

test('probes on the module: all or none of the addresses, no warning', () => {
  const esp = (change) => keys(variant((m) => (m.mk2wifi.enabled = true) && (m.temperature.mode = 'esp') && change(m)));
  assert.ok(!esp(() => true).includes('warn.sensorSomeAddresses'));
  assert.ok(!esp((m) => m.temperature.sensors.forEach((s) => (s.address = ''))).includes('warn.sensorSomeAddresses'));
});

test('rotation OFF: the groups left in the model are not checked', () => {
  const m = variant((m) => (m.rotation.groups = [[0]]));
  assert.deepStrictEqual(keys(m).filter((k) => k.startsWith('err.rotation')), []);
});

test('rotation: a save without groups loads with none, and is refused with rotation on', () => {
  const saved = M.defaults();
  delete saved.rotation.groups;
  saved.rotation.mode = 'PIN';
  saved.rotation.pin = 8;
  const m = M.normalize(JSON.parse(JSON.stringify(saved)));
  assert.deepStrictEqual(m.rotation.groups, []);
  assert.ok(keys(m).includes('err.rotationNoGroup'));
});

test('rotation: a good group of every local load passes', () => {
  const m = variant((m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[2, 0, 1]]));
  assert.deepStrictEqual(keys(m).filter((k) => k.startsWith('err.rotation')), []);
});

test('rotation: a load gone from the map stays in its group editor, to be unticked', () => {
  const m = variant((m) => (m.rotation.mode = 'AUTO') && (m.rotation.groups = [[0, 1, 2]]));
  M.resizeLoads(m, 2);
  assert.ok(keys(m).includes('err.rotationNotALoad'));
  assert.deepStrictEqual(M.rotationChoices(m, m.rotation.groups[0]), [0, 1, 2]);
  assert.deepStrictEqual(M.rotationChoices(m, [0]), [0, 1]);
});

test('rotation: dual tariff is no problem, in either mode (the page tells when it rotates)', () => {
  for (const mode of ['AUTO', 'PIN']) {
    const m = variant((m) => Object.assign(m.dualTariff, { enabled: true, pin: 3 }) && Object.assign(m.rotation, { mode, pin: 4, groups: [[0, 1, 2]] }));
    assert.deepStrictEqual(V.validate(m).filter((p) => p.field === 'rotation'), [], mode);
  }
});

test('rotation delay: checked only when its field is shown (automatic, without dual tariff)', () => {
  const delay = (change) => keys(variant((m) => change(m) || (m.rotation.afterSeconds = 0))).includes('err.rotationAfter');
  assert.ok(delay((m) => Object.assign(m.rotation, { mode: 'AUTO', groups: [[0, 1, 2]] }) && false));
  assert.ok(!delay((m) => Object.assign(m.rotation, { mode: 'AUTO', groups: [[0, 1, 2]] }) && Object.assign(m.dualTariff, { enabled: true, pin: 3 }) && false));
  assert.ok(!delay((m) => Object.assign(m.rotation, { mode: 'PIN', pin: 4, groups: [[0, 1, 2]] }) && false));
  assert.ok(!delay(() => false));
});

test('pins: the uses of a pin are keys, translated by the page, not English text', () => {
  const m = variant((m) => (m.loads[0] = { type: 'local', pin: 5 }) && (m.overrides.list[0].pin = 5));
  const dup = V.validate(m).find((p) => p.key === 'err.duplicatePin');
  assert.ok(dup.params.uses.some((u) => u.key === 'use.load' && u.n === 1));
  assert.ok(dup.params.uses.some((u) => u.key === 'use.override' && u.n === 1));
});

test('pins offered: not those of the RF chip when it is there; a chosen one stays', () => {
  const rf = variant((m) => (m.rf.logging = true));
  assert.deepStrictEqual(M.offeredPins(rf, 5).filter((p) => M.RF_PINS.includes(p)), []);
  assert.ok(M.offeredPins(rf, 10).includes(10));
  assert.ok(M.offeredPins(variant((m) => (m.rf.logging = false) && (m.loads = m.loads.filter((l) => l.type === 'local'))), 5).includes(10));
});
