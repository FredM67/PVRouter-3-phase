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
const remote = (unit) => ({ type: 'remote', unit, led: null });

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
  ['err.rotationAfter', (m) => (m.rotation.afterSeconds = 86401)],
  ['warn.rotationDualTariff', (m) => Object.assign(m.dualTariff, { enabled: true, pin: 3 }) && (m.rotation.mode = 'AUTO')],
  ['err.noRelay', (m) => Object.assign(m.relays, { enabled: true, list: [] })],
  ['err.relayPin', (m) => (m.relays.enabled = true)],
  ['err.datalogDivides60', (m) => (m.relays.enabled = true) && (m.datalogPeriod = 7)],
  ['err.relaySurplus', (m) => (m.relays.enabled = true) && (m.relays.list[0].surplus = 0)],
  ['err.relayMinTime', (m) => (m.relays.enabled = true) && (m.relays.list[0].minOn = 2000)],
  ['err.overridePin', (m) => (m.overrides.list[0].pin = null)],
  ['err.overrideTargets', (m) => (m.overrides.list[0].targets = [])],
  ['err.overrideNoRelay', (m) => (m.overrides.list[0].targets = 'ALL_RELAYS')],
  ['err.overrideNoRelay', (m) => (m.overrides.list[0].targets = [{ relay: 0 }])],
  ['err.overrideLoad', (m) => (m.overrides.list[0].targets = [{ load: 3 }])],
  ['warn.overrideNothing', (m) => (m.overrides.list[0].targets = 'ALL_REMOTE_LOADS')],
  ['err.offPeakHours', (m) => Object.assign(m.dualTariff, { enabled: true, pin: 3, offPeakHours: 13 })],
  ['err.tempThreshold', (m) => Object.assign(m.dualTariff, { enabled: true, pin: 3, tempThreshold: 0 })],
  ['err.noSensor', (m) => Object.assign(m.temperature, { mode: 'router', sensors: [] })],
  ['err.sensorAddress', (m) => (m.temperature.mode = 'router') && (m.temperature.sensors[0].address = '28BE41')],
  ['warn.sensorCrc', (m) => (m.temperature.mode = 'router') && (m.temperature.sensors[0].address = '28BE416B090000A5')],
  ['err.tempEspNoWifi', (m) => (m.temperature.mode = 'esp')],
  ['err.duplicatePin', (m) => (m.overrides.list[0].pin = 5)],
  ['err.duplicatePin', (m) => Object.assign(m.temperature, { mode: 'router', pin: 4 })],
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
  ['warn.wifiNotIoT', (m) => (m.mk2wifi.enabled = true)],
  ['err.wifiPin', (m) => (m.mk2wifi.enabled = true) && (m.diversion = { enabled: true, pin: 3, wifi: true })],
  ['warn.wifiKeepOpen', (m) => (m.mk2wifi.enabled = true) && (m.serialOutput = 'IoT')],
  ['warn.calibration', (m) => (m.calibrationMode = true)],
  ['warn.calProbe', (m) => (m.calibrationMode = true) && (m.calibration.power[0] = { router: -1500, meter: 1500 })],
  ['warn.calCurrent', (m) => (m.calibrationMode = true) && (m.calibration.power[0] = { router: 1500, meter: 1500 })],
  ['warn.calLowPower', (m) => (m.calibrationMode = true) && (m.calibration.powerCal = [0.05, 0.05, 0.05]) && (m.calibration.power[1] = { router: 100, meter: 105 })],
  ['warn.calRatio', (m) => (m.calibrationMode = true) && (m.calibration.voltageCal = [0.8, 0.8, 0.8]) && (m.calibration.voltage[2] = { router: 100, meter: 230 })],
  ['warn.calReading', (m) => (m.calibrationMode = true) && (m.calibration.voltage[0] = { router: 230, meter: 0 })],
];

CASES.forEach(([key, change], i) => {
  test(`${key} (case ${i + 1})`, () => {
    const m = variant(change);
    assert.ok(keys(m).includes(key), JSON.stringify(V.validate(m)));
  });
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
