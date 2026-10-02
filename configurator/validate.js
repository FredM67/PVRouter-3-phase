/*
 * PVRouter configurator - checks of a configuration.
 *
 * Mirrors the static_asserts of Mk2_3phase_RFdatalog_temp/validation.h, so that what the page
 * accepts also compiles, and adds what the compiler cannot see: the mk2Wifi wiring, the
 * remote units' own pins, settings that compile but make no sense.
 *
 * Each problem: { level: 'error' | 'warning', key, params, field }. 'key' is an i18n key
 * (i18n.js), 'field' the setting to highlight.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory(require('./model.js'));
  else root.PVRValidate = factory(root.PVRModel);
})(typeof self !== 'undefined' ? self : this, function (M) {
  'use strict';

  const isInt = (v) => Number.isInteger(v);
  const inRange = (v, lo, hi) => isInt(v) && v >= lo && v <= hi;
  const isRouterPin = (p) => inRange(p, 2, 13);

  function validate(m) {
    const out = [];
    const error = (key, field, params) => out.push({ level: 'error', key, field, params: params || {} });
    const warning = (key, field, params) => out.push({ level: 'warning', key, field, params: params || {} });

    // ---- general (config_system.h) ----
    if (m.supplyFrequency !== 50 && m.supplyFrequency !== 60) error('err.frequency', 'supplyFrequency');
    if (!inRange(m.datalogPeriod, 1, 40)) error('err.datalogPeriod', 'datalogPeriod');
    if (!inRange(m.requiredExport, -32768, 32767)) error('err.requiredExport', 'requiredExport');
    if (!inRange(m.diversionStartThreshold, 0, 32767)) error('err.diversionStart', 'diversionStartThreshold');
    if (!M.SERIAL_OUTPUTS.includes(m.serialOutput)) error('err.serialOutput', 'serialOutput');
    if (m.calibrationMode) warning('warn.calibration', 'calibrationMode');

    // ---- load map ----
    const n = m.loads.length;
    if (n < 1) error('err.noLoad', 'loads');
    m.loads.forEach((l, i) => {
      const field = `loads.${i}`;
      if (l.type === 'local') {
        if (!isRouterPin(l.pin)) error('err.loadPin', field, { load: i + 1 });
      } else if (l.type === 'remote') {
        if (!inRange(l.unit, 1, M.LIMITS.MAX_REMOTE_UNITS)) error('err.loadUnit', field, { load: i + 1, max: M.LIMITS.MAX_REMOTE_UNITS });
        if (l.led !== null && l.led !== undefined && !isRouterPin(l.led)) error('err.loadLed', field, { load: i + 1 });
      } else error('err.loadType', field, { load: i + 1 });
    });
    const remote = M.remoteLoadCount(m);
    if (remote > M.LIMITS.MAX_REMOTE_LOADS) error('err.tooManyRemote', 'loads', { max: M.LIMITS.MAX_REMOTE_LOADS });
    const units = Math.min(M.remoteUnitsUsed(m), M.LIMITS.MAX_REMOTE_UNITS); // more: reported above
    for (let u = 1; u < units; ++u) if (!M.loadsOfUnit(m, u)) warning('warn.unitGap', 'loads', { unit: u });

    // priorities: a permutation of 0..n-1
    const prio = m.priorities || [];
    if (prio.length !== n || new Set(prio).size !== n || prio.some((p) => !inRange(p, 0, n - 1))) error('err.priorities', 'priorities');

    // ---- control pins ----
    const needPin = (enabled, p, key, field) => {
      if (enabled && !isRouterPin(p)) error(key, field);
    };
    needPin(m.diversion.enabled, m.diversion.pin, 'err.diversionPin', 'diversion');
    needPin(m.routerOff.enabled, m.routerOff.pin, 'err.routerOffPin', 'routerOff');
    needPin(m.rotation.mode === 'PIN', m.rotation.pin, 'err.rotationPin', 'rotation');
    needPin(m.watchdog.enabled, m.watchdog.pin, 'err.watchdogPin', 'watchdog');
    needPin(m.dualTariff.enabled, m.dualTariff.pin, 'err.dualTariffPin', 'dualTariff');
    needPin(M.tempSensorPresent(m), m.temperature.pin, 'err.tempPin', 'temperature');
    if (!M.ROTATION_MODES.includes(m.rotation.mode)) error('err.rotationMode', 'rotation');
    if (!inRange(m.rotation.afterSeconds, 1, 86400)) error('err.rotationAfter', 'rotation');
    if (m.dualTariff.enabled && m.rotation.mode !== 'OFF') warning('warn.rotationDualTariff', 'rotation');

    // ---- relays ----
    const relays = m.relays.enabled ? m.relays.list : [];
    if (m.relays.enabled) {
      if (!relays.length) error('err.noRelay', 'relays');
      if (60 % m.datalogPeriod !== 0) error('err.datalogDivides60', 'datalogPeriod');
      if (!inRange(m.relays.filterDelay, 1, 10)) error('err.filterDelay', 'relays');
      relays.forEach((r, i) => {
        const field = `relays.${i}`;
        if (!isRouterPin(r.pin)) error('err.relayPin', field, { relay: i + 1 });
        if (!inRange(r.surplus, 1, 32767)) error('err.relaySurplus', field, { relay: i + 1 });
        if (!inRange(r.import, -32767, 32767)) error('err.relayImport', field, { relay: i + 1 });
        if (!inRange(r.minOn, 0, 1092) || !inRange(r.minOff, 0, 1092)) error('err.relayMinTime', field, { relay: i + 1 });
      });
    }

    // ---- overrides ----
    if (M.overridePresent(m)) {
      m.overrides.list.forEach((o, i) => {
        const field = `overrides.${i}`;
        if (!isRouterPin(o.pin)) error('err.overridePin', field, { index: i + 1 });
        const t = o.targets;
        if (typeof t === 'string') {
          if (!M.OVERRIDE_GROUPS.includes(t)) error('err.overrideTargets', field, { index: i + 1 });
          if (t === 'ALL_RELAYS' && !m.relays.enabled) error('err.overrideNoRelay', field, { index: i + 1 });
          if (t === 'ALL_REMOTE_LOADS' && !remote) warning('warn.overrideNothing', field, { index: i + 1 });
          if (t === 'ALL_LOCAL_LOADS' && remote === n) warning('warn.overrideNothing', field, { index: i + 1 });
        } else if (!Array.isArray(t) || !t.length) error('err.overrideTargets', field, { index: i + 1 });
        else
          for (const x of t) {
            if ('relay' in x) {
              if (!m.relays.enabled) error('err.overrideNoRelay', field, { index: i + 1 });
              else if (!inRange(x.relay, 0, relays.length - 1)) error('err.overrideRelay', field, { index: i + 1, relay: x.relay + 1 });
            } else if (!inRange(x.load, 0, n - 1)) error('err.overrideLoad', field, { index: i + 1, load: x.load + 1 });
          }
      });
    }

    // ---- dual tariff ----
    if (m.dualTariff.enabled) {
      if (!inRange(m.dualTariff.offPeakHours, 1, 12)) error('err.offPeakHours', 'dualTariff');
      if (!inRange(m.dualTariff.tempThreshold, 1, 100)) error('err.tempThreshold', 'dualTariff');
      if (m.dualTariff.force.length > n) error('err.forceCount', 'dualTariff');
      m.dualTariff.force.forEach((f, i) => {
        if (!inRange(f.start, -32768, 32767) || (f.duration !== null && !inRange(f.duration, 0, 65534))) error('err.force', 'dualTariff', { load: i + 1 });
      });
    }

    // ---- temperature ----
    if (!M.TEMP_MODES.includes(m.temperature.mode)) error('err.tempMode', 'temperature');
    if (m.temperature.mode !== 'off') {
      const sensors = m.temperature.sensors;
      if (!sensors.length) error('err.noSensor', 'temperature');
      sensors.forEach((s, i) => {
        const a = M.parseAddress(s.address);
        const optional = m.temperature.mode === 'esp' && sensors.length === 1 && !s.address;
        if (!a) {
          if (!optional) error('err.sensorAddress', `temperature.${i}`, { sensor: i + 1 });
        } else if (M.crc8(a.slice(0, 7)) !== a[7]) warning('warn.sensorCrc', `temperature.${i}`, { sensor: i + 1 });
      });
      if (m.temperature.mode === 'esp' && !m.mk2wifi.enabled) error('err.tempEspNoWifi', 'temperature');
    }

    // ---- one pin, one use ----
    const uses = [];
    const use = (p, what) => {
      if (isRouterPin(p)) uses.push([p, what]);
    };
    if (M.tempSensorPresent(m)) use(m.temperature.pin, 'temperature');
    if (m.diversion.enabled) use(m.diversion.pin, 'diversion');
    if (m.routerOff.enabled) use(m.routerOff.pin, 'routerOff');
    if (m.rotation.mode === 'PIN') use(m.rotation.pin, 'rotation');
    if (m.dualTariff.enabled) use(m.dualTariff.pin, 'dualTariff');
    if (M.overridePresent(m)) m.overrides.list.forEach((o, i) => use(o.pin, `override ${i + 1}`));
    if (m.watchdog.enabled) use(m.watchdog.pin, 'watchdog');
    m.loads.forEach((l, i) => use(l.type === 'local' ? l.pin : l.led, `load ${i + 1}`));
    relays.forEach((r, i) => use(r.pin, `relay ${i + 1}`));
    const byPin = new Map();
    for (const [p, what] of uses) byPin.set(p, [...(byPin.get(p) || []), what]);
    for (const [p, whats] of byPin) if (whats.length > 1) error('err.duplicatePin', 'pins', { pin: p, uses: whats.join(', ') });
    if (M.rfChipPresent(m)) for (const [p, whats] of byPin) if (M.RF_PINS.includes(p)) error('err.rfPin', 'pins', { pin: p, uses: whats.join(', ') });

    // ---- RF ----
    if (M.rfChipPresent(m)) {
      const r = m.rf;
      if (!M.RF_FREQUENCIES.includes(r.frequency)) error('err.rfFrequency', 'rf');
      if (!inRange(r.networkId, 1, 250)) error('err.networkId', 'rf');
      if (!inRange(r.routerId, 1, 30)) error('err.routerId', 'rf');
      if (!inRange(r.powerLevel, 0, 31)) error('err.powerLevel', 'rf');
      const ids = r.remoteIds.slice(0, units);
      ids.forEach((id, i) => {
        if (!inRange(id, 1, 30) || id === r.routerId || ids.indexOf(id) !== i) error('err.remoteId', 'rf', { unit: i + 1 });
      });
      if (r.logging && (!inRange(r.gatewayId, 1, 30) || r.gatewayId === r.routerId || ids.includes(r.gatewayId))) error('err.gatewayId', 'rf');
    }

    // ---- remote units (RemoteLoadReceiver: no static_assert there) ----
    for (let u = 1; u <= units; ++u) {
      const unit = m.units[u - 1];
      const count = M.loadsOfUnit(m, u);
      const field = `units.${u - 1}`;
      if (!count) continue;
      if (unit.loadPins.length < count) error('err.unitPins', field, { unit: u, count });
      // D0/D1 serial, D2 and D10-D13 the RFM69
      const ok = (p) => inRange(p, 3, 9) || inRange(p, 14, 19); // A0-A5 too
      const pins = unit.loadPins.slice(0, count);
      const all = [...pins, ...(unit.statusLeds ? [unit.greenLed, unit.redLed] : [])];
      all.forEach((p) => {
        if (!ok(p)) error('err.unitPin', field, { unit: u, pin: p });
      });
      if (new Set(all).size !== all.length) error('err.unitDuplicate', field, { unit: u });
    }

    // ---- IoT output: one-digit TeleInfo indexes ----
    if (m.serialOutput === 'IoT') {
      if (n > M.LIMITS.MAX_IOT_LOADS) error('err.iotLoads', 'loads', { max: M.LIMITS.MAX_IOT_LOADS });
      if (relays.length > M.LIMITS.MAX_IOT_RELAYS) error('err.iotRelays', 'relays', { max: M.LIMITS.MAX_IOT_RELAYS });
      if (M.tempSensorPresent(m) && m.temperature.sensors.length > M.LIMITS.MAX_IOT_SENSORS) error('err.iotSensors', 'temperature', { max: M.LIMITS.MAX_IOT_SENSORS });
    }

    // ---- mk2Wifi ----
    if (m.mk2wifi.enabled) {
      if (!/^[a-z0-9-]{1,31}$/.test(m.mk2wifi.name || '')) error('err.deviceName', 'mk2wifi');
      if (m.serialOutput !== 'IoT') warning('warn.wifiNotIoT', 'serialOutput');
      for (const c of M.wifiInputs(m)) if (!(c.pin in M.MK2WIFI_GPIO)) error('err.wifiPin', 'mk2wifi', { pin: c.pin });
      // router outputs on D5-D9: the matching jumper must stay open
      const outputs = [...byPin.entries()].filter(([p]) => p in M.MK2WIFI_GPIO && !M.wifiInputs(m).some((c) => c.pin === p)).map(([p]) => `D${p}`);
      if (outputs.length) warning('warn.wifiKeepOpen', 'mk2wifi', { pins: outputs.join(', ') });
    }

    return out;
  }

  const hasErrors = (problems) => problems.some((p) => p.level === 'error');

  return { validate, hasErrors };
});
