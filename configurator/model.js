/*
 * PVRouter configurator - the settings and their defaults.
 *
 * The defaults are the shipped firmware files: generating them must give back
 * Mk2_3phase_RFdatalog_temp/config.h, config_system.h and config_rf.h byte for byte
 * (checked by test/generate.test.js).
 *
 * Works as a classic <script> (global PVRModel) and as a Node module.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.PVRModel = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const FORMAT_VERSION = 1;

  // Firmware limits (remote_loads_core.h, utils_rf.h, validation.h)
  const LIMITS = {
    MAX_REMOTE_UNITS: 3,
    MAX_LOADS_PER_UNIT: 8,
    MAX_REMOTE_LOADS: 8,
    MAX_IOT_LOADS: 9, // TeleInfo tags D1..D9: a 10th load would need a 2-digit index
    MAX_IOT_RELAYS: 9,
    MAX_IOT_SENSORS: 9,
  };

  // Router pins
  const DIGITAL_PINS = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
  const RF_PINS = [2, 10, 11, 12, 13]; // IRQ, CS, MOSI, MISO, SCK

  // mk2Wifi: router pin -> ESP32-C6 GPIO, each through a solder jumper (open by default)
  const MK2WIFI_GPIO = { 5: 0, 6: 5, 7: 4, 8: 3, 9: 1 };
  const MK2WIFI_ONEWIRE_GPIO = 23;
  const MK2WIFI_UART = { tx: 16, rx: 17 };

  const PCB_VERSIONS = ['OLD', 'NEW'];
  const SERIAL_OUTPUTS = ['HumanReadable', 'IoT', 'JSON'];
  const ROTATION_MODES = ['OFF', 'AUTO', 'PIN'];
  const RF_FREQUENCIES = ['RF69_433MHZ', 'RF69_868MHZ', 'RF69_915MHZ'];
  const OVERRIDE_GROUPS = ['ALL_LOADS', 'ALL_LOCAL_LOADS', 'ALL_REMOTE_LOADS', 'ALL_RELAYS', 'ALL_LOADS_AND_RELAYS'];
  const TEMP_MODES = ['off', 'router', 'esp'];
  const TEMP_PIN = 3; // the probes' bus on the new board, wired there with its pull-up (router mode)
  const PHASES = 3; // NO_OF_PHASES of the 3-phase firmware

  function defaults() {
    return {
      formatVersion: FORMAT_VERSION,
      router: 3, // 3: this page, 1: the single-phase page (a save without it is a 3-phase one)

      // config.h - general
      pcbVersion: 'NEW', // OLD: former 3-phase board, NEW: universal 3phaseDiverter board
      serialOutput: 'IoT',
      enableDebug: true,
      calibrationMode: false,

      // calibration helper, in calibration mode: new value = current value x meter / router
      calibration: {
        file: null, // the user's calibration.h, as loaded: the new values are written into it
        fileName: '',
        // the values the router runs with, as in calibration.h
        ctRating: [null, null, null], // f_ctRating, A, new board
        powerCalPerAmp: [null, null, null], // f_powerCalPerAmp, new board
        voltageCalNewBoard: [null, null, null], // f_voltageCalNewBoard
        powerCalOldBoard: [null, null, null], // f_powerCalOldBoard
        voltageCalOldBoard: [null, null, null], // f_voltageCalOldBoard
        power: [0, 1, 2].map(() => ({ router: null, meter: null })), // W, P1..P3
        voltage: [0, 1, 2].map(() => ({ router: null, meter: null })), // V, V1..V3
      },

      // config_system.h
      requiredExport: 20,
      diversionStartThreshold: 0,
      supplyFrequency: 50,
      datalogPeriod: 5,

      // load map: { type: 'local', pin } or { type: 'remote', unit, led } (led: pin or null)
      loads: [
        { type: 'local', pin: 5 },
        { type: 'local', pin: 6 },
        { type: 'local', pin: 7 },
      ],
      // array index = priority (0 = highest), value = load index
      priorities: [0, 1, 2],

      // control inputs (active LOW). 'wifi': driven by the mk2Wifi module (pin in D5..D9)
      diversion: { enabled: false, pin: null, wifi: false },
      routerOff: { enabled: false, pin: null, wifi: false },
      // groups: lists of load indices that take turns (rotationGroups), only written with rotation on
      rotation: { mode: 'OFF', pin: null, wifi: false, afterSeconds: 8 * 3600, groups: [] },
      watchdog: { enabled: false, pin: null },

      relays: {
        enabled: false,
        filterDelay: 2,
        // pins are only written when relays are enabled
        list: [
          { pin: null, surplus: 100, import: 200, minOn: 1, minOff: 1 },
          { pin: null, surplus: 300, import: 400, minOn: 1, minOff: 1 },
          { pin: null, surplus: 500, import: 600, minOn: 1, minOff: 1 },
        ],
      },

      // targets: one of OVERRIDE_GROUPS, or a list of { load: i } / { relay: i }
      overrides: {
        enabled: true,
        list: [{ pin: 4, wifi: false, targets: 'ALL_LOADS' }],
      },

      dualTariff: {
        enabled: false,
        pin: null,
        wifi: false,
        offPeakHours: 8,
        // scheduled boosts: from 'start' or 'end' of the off-peak period, 'at' minutes after the
        // start or before the end, for 'for' minutes (null: until the end); targets as overrides
        boosts: [{ from: 'end', at: 180, for: 120, targets: [{ load: 0 }] }],
        tempThreshold: 100,
        tempSensor: 0, // the probe checked against it, from 0 (temperature.sensors)
      },

      temperature: {
        mode: 'off', // 'router': the router reads the probes; 'esp': the mk2Wifi module does
        pin: 3, // old board: the user wires the bus, on the pin of their choice (new board: TEMP_PIN)
        sensors: [
          { address: '28BE416B090000A4', name: '' },
          { address: '281BD76A090000B7', name: '' },
        ],
      },

      rf: {
        logging: false,
        frequency: 'RF69_868MHZ',
        networkId: 210,
        routerId: 10,
        gatewayId: 1,
        remoteIds: [15, 16, 17],
        isHW: false,
        powerLevel: 31,
      },

      // RemoteLoadReceiver, one per remote unit (unit 1 first)
      units: [
        { loadPins: [4, 3], statusLeds: true, greenLed: 5, redLed: 7 },
        { loadPins: [4, 3], statusLeds: true, greenLed: 5, redLed: 7 },
        { loadPins: [4, 3], statusLeds: true, greenLed: 5, redLed: 7 },
      ],

      mk2wifi: {
        enabled: false,
        name: 'mk2pvrouter',
        friendlyName: 'Mk2PVRouter',
        language: 'fr', // entity names in Home Assistant
      },
    };
  }

  const clone = (o) => JSON.parse(JSON.stringify(o));

  // The router a save is for: a save without the field is a 3-phase one
  const saveRouter = (data) => (data && data.router) ?? 3;

  const SINGLE_PHASE_URL = 'https://fredm67.github.io/Mk2PVRouter/configurateur-monophase/';
  // Where the "single-phase" link goes: relative on the docs site (it follows the version being
  // read), absolute from a file or any other host. loc: { protocol, pathname }
  function singlePhaseUrl(loc) {
    return loc && /^https?:$/.test(loc.protocol) && String(loc.pathname).includes('/Mk2PVRouter/') ? '../configurateur-monophase/' : SINGLE_PHASE_URL;
  }

  // Fill what an older or hand-edited JSON lacks, keep the rest
  function normalize(input) {
    if (saveRouter(input) !== 3) throw new Error('router');
    const base = defaults();
    const merge = (b, v) => {
      if (Array.isArray(b)) return Array.isArray(v) ? clone(v) : b;
      if (b && typeof b === 'object') {
        const out = {};
        for (const k of Object.keys(b)) out[k] = v && k in v ? merge(b[k], v[k]) : b[k];
        return out;
      }
      return v === undefined ? b : v;
    };
    const m = merge(base, input || {});
    m.formatVersion = FORMAT_VERSION;
    while (m.units.length < LIMITS.MAX_REMOTE_UNITS) m.units.push(clone(base.units[0]));
    while (m.rf.remoteIds.length < LIMITS.MAX_REMOTE_UNITS) m.rf.remoteIds.push(15 + m.rf.remoteIds.length);
    return m;
  }

  // ---- calibration.h ----

  // The active (not commented out) 'name[NO_OF_PHASES]{ a, b, c }' of calibration.h:
  // { values, tokens: [{ start, end }] } with positions in the text, or null
  function calibrationArray(text, name) {
    const re = new RegExp(`\\b${name}\\s*\\[\\s*NO_OF_PHASES\\s*\\]\\s*\\{([^}]*)\\}`, 'g');
    for (let r; (r = re.exec(text)); ) {
      const lineStart = text.lastIndexOf('\n', r.index) + 1;
      if (text.slice(lineStart, r.index).includes('//')) continue;
      const tokens = [];
      const listStart = r.index + r[0].indexOf('{') + 1;
      const tokenRe = /[^,\s][^,]*?(?=\s*(,|$))/g;
      for (let t; (t = tokenRe.exec(r[1])); ) tokens.push({ start: listStart + t.index, end: listStart + t.index + t[0].length, text: t[0] });
      const values = tokens.map((t) => parseFloat(t.text));
      if (values.length !== PHASES || !values.every(Number.isFinite)) return null;
      return { values, tokens };
    }
    return null;
  }

  // The values calibration corrects, for each board: calibration.h has a set per board, and the
  // firmware uses the one of PCB_VERSION (new board: f_powerCal = f_powerCalPerAmp x f_ctRating)
  const CAL_VALUES = {
    NEW: {
      power: { name: 'f_powerCalPerAmp', key: 'powerCalPerAmp', decimals: 7 },
      voltage: { name: 'f_voltageCalNewBoard', key: 'voltageCalNewBoard', decimals: 4 },
    },
    OLD: {
      power: { name: 'f_powerCalOldBoard', key: 'powerCalOldBoard', decimals: 5 },
      voltage: { name: 'f_voltageCalOldBoard', key: 'voltageCalOldBoard', decimals: 4 },
    },
  };
  const calValuesOf = (m) => CAL_VALUES[m.pcbVersion] || CAL_VALUES.NEW;
  // The rated current of the CTs, new board only: set by the user, not corrected by calibration
  const CT_RATING = { name: 'f_ctRating', key: 'ctRating', decimals: 1 };
  const ctRatingUsed = (m) => m.pcbVersion === 'NEW';
  const CAL_NAMES = [CT_RATING, ...Object.values(CAL_VALUES).flatMap((b) => [b.power, b.voltage])];

  // { ctRating, powerCalPerAmp, voltageCalNewBoard, powerCalOldBoard, voltageCalOldBoard }, or null
  function parseCalibration(text) {
    const out = {};
    for (const { name, key } of CAL_NAMES) {
      const a = calibrationArray(text, name);
      if (!a) return null;
      out[key] = a.values;
    }
    return out;
  }

  // The corrected values: current x meter / router, for each phase with both readings.
  // { power, voltage }: the new values of calValuesOf(m).power and .voltage
  function calibrationResult(m) {
    const c = m.calibration;
    const cal = calValuesOf(m);
    const fix = (current, readings) =>
      current.map((cur, i) => {
        const { router, meter } = readings[i];
        return cur > 0 && router > 0 && meter > 0 ? (cur * meter) / router : cur;
      });
    return { power: fix(c[cal.power.key], c.power), voltage: fix(c[cal.voltage.key], c.voltage) };
  }

  // ---- derived values, shared by the validator, the generator and the page ----

  const remoteUnitsUsed = (m) => {
    let n = 0;
    for (const l of m.loads) if (l.type === 'remote' && l.unit > n) n = l.unit;
    return n;
  };
  const remoteLoadCount = (m) => m.loads.filter((l) => l.type === 'remote').length;
  const loadsOfUnit = (m, unit) => m.loads.filter((l) => l.type === 'remote' && l.unit === unit).length;
  const remoteLoadsPresent = (m) => remoteLoadCount(m) > 0;

  // A scheduled boost's window, in minutes from the start of the off-peak period ({ from, at, for }:
  // 'at' counted from the start or back from the end; for: null = until the meter goes back to
  // peak, drawn up to the end). null when its time cannot be read.
  function boostWindow(b, periodMin) {
    if (!Number.isInteger(b.at) || !['start', 'end'].includes(b.from)) return null;
    const start = b.from === 'end' ? periodMin - b.at : b.at;
    const open = b.for === null;
    return { start, end: open ? periodMin : start + b.for, open };
  }

  // The same window counted from the other end: switching "after the start" / "before the end"
  // must not move the boost (0 h after the start, for 2 h = 8 h before the end, for 2 h, with 8 h)
  function reanchor(b, from, periodMin) {
    const w = boostWindow(b, periodMin);
    if (!w || from === b.from || !(periodMin > 0)) return { from, at: b.at, for: b.for }; // no period (field emptied): as typed
    return { from, at: periodMin - b.at, for: b.for };
  }

  // A new duration (null: until the end) keeps the anchored edge: the start of a boost counted
  // from the start, the end of one counted back from the end (it then starts earlier or later).
  // A boost that would no longer fit moves to end with the period, or start with it; a duration
  // longer than the period is cut to it. No period (field emptied): as typed.
  function withDuration(b, duration, periodMin) {
    const w = boostWindow(b, periodMin);
    if (duration === null || !w || !(periodMin > 0)) return { at: b.at, for: duration };
    const length = Math.min(duration, periodMin);
    const anchored = b.from === 'end' ? w.end - length : w.start; // w.end: the period's end when open
    const start = Math.min(Math.max(anchored, 0), periodMin - length);
    return { at: b.from === 'end' ? periodMin - start : start, for: length };
  }

  // A boost brought inside a shorter off-peak period: its anchored edge (the start counted from the
  // start, or back from the end) stays where it is and the duration is cut to the period; an edge
  // now outside the period comes back to it, keeping what it can of the duration. A boost that
  // fits, or with unreadable times, is unchanged.
  function fitBoost(b, periodMin) {
    const w = boostWindow(b, periodMin);
    if (!w || !(periodMin > 0) || boostFits(b, periodMin) || (!w.open && b.for <= 0)) return { at: b.at, for: b.for };
    if (b.from === 'end') {
      const at = Math.min(b.at, periodMin);
      return { at, for: w.open ? null : Math.min(b.for, at) };
    }
    if (w.open) return { at: Math.min(b.at, periodMin - 15), for: null };
    if (b.at < periodMin) return { at: b.at, for: periodMin - b.at };
    const length = Math.min(b.for, periodMin);
    return { at: periodMin - length, for: length };
  }

  // A new start ('at') keeps the duration while the window fits; one that would run past the end
  // of the period is cut there (3 h before the end, for 3 h -> 2 h before the end, for 2 h). A start
  // outside the period is left to err.boostWindow.
  function withStart(b, at, periodMin) {
    const w = boostWindow({ ...b, at }, periodMin);
    if (!w || w.open || w.start < 0 || w.start >= periodMin || w.end <= periodMin) return { at, for: b.for };
    return { at, for: periodMin - w.start };
  }

  // As validation.h (startsWithinOffPeak(), endsWithinOffPeak(), hasLength()): the whole window
  // lies within the period, and lasts.
  function boostFits(b, periodMin) {
    const w = boostWindow(b, periodMin);
    if (!w || w.start < 0 || w.start >= periodMin) return false;
    return w.open || (b.for > 0 && w.end <= periodMin);
  }
  const rfChipPresent = (m) => m.rf.logging || remoteLoadsPresent(m);

  // the router pins a pin list offers: not those of the RF chip when it is there; the pin already
  // chosen stays, so the page shows it with the error that says why
  const offeredPins = (m, current) => DIGITAL_PINS.filter((p) => p === current || !(rfChipPresent(m) && RF_PINS.includes(p)));
  const tempSensorPresent = (m) => m.temperature.mode === 'router';
  // The pin of the probes' bus: wired on the new board, chosen on the old one
  const tempPin = (m) => (m.pcbVersion === 'OLD' ? m.temperature.pin : TEMP_PIN);

  // Remove probe i; the boosts keep the probe they check, or fall back to the first if it was this one
  function removeSensor(m, i) {
    m.temperature.sensors.splice(i, 1);
    const d = m.dualTariff;
    if (d.tempSensor === i) d.tempSensor = 0;
    else if (d.tempSensor > i) --d.tempSensor;
  }
  const overridePresent = (m) => m.overrides.enabled && m.overrides.list.length > 0;
  // calibration reads P1..P3 on the serial monitor: human-readable, whatever the usual choice
  const serialOutputOf = (m) => (m.calibrationMode ? 'HumanReadable' : m.serialOutput);

  // Router inputs that the mk2Wifi module may drive, with their default (fail-safe) role
  function controlInputs(m) {
    const out = [];
    if (m.diversion.enabled) out.push({ kind: 'diversion', pin: m.diversion.pin, wifi: m.diversion.wifi });
    if (m.routerOff.enabled) out.push({ kind: 'routerOff', pin: m.routerOff.pin, wifi: m.routerOff.wifi });
    if (m.rotation.mode === 'PIN') out.push({ kind: 'rotation', pin: m.rotation.pin, wifi: m.rotation.wifi });
    if (m.dualTariff.enabled) out.push({ kind: 'dualTariff', pin: m.dualTariff.pin, wifi: m.dualTariff.wifi });
    if (overridePresent(m))
      m.overrides.list.forEach((o, i) => out.push({ kind: 'override', index: i, pin: o.pin, wifi: o.wifi, targets: o.targets }));
    return out;
  }

  const wifiInputs = (m) => (m.mk2wifi.enabled ? controlInputs(m).filter((c) => c.wifi) : []);

  // "28BE416B090000A4" or "0x28, 0xBE, ..." -> [0x28, 0xBE, ...] (null if not 8 bytes)
  function parseAddress(text) {
    const hex = String(text || '')
      .replace(/0x/gi, '')
      .replace(/[^0-9a-f]/gi, '');
    if (hex.length !== 16) return null;
    const out = [];
    for (let i = 0; i < 16; i += 2) out.push(parseInt(hex.slice(i, i + 2), 16));
    return out;
  }

  // Dallas/Maxim CRC-8 (poly 0x31 reflected), the last byte of a 1-Wire ROM code
  function crc8(bytes) {
    let crc = 0;
    for (const b of bytes) {
      let v = b;
      for (let i = 0; i < 8; ++i) {
        const mix = (crc ^ v) & 1;
        crc >>= 1;
        if (mix) crc ^= 0x8c;
        v >>= 1;
      }
    }
    return crc;
  }

  // Keep the per-load arrays in step with the number of loads
  // The boosts and override pins drop the targets that no longer exist (a group stays)
  function dropTargets(m, keep) {
    const trim = (o) => {
      if (Array.isArray(o.targets)) o.targets = o.targets.filter(keep);
    };
    m.dualTariff.boosts.forEach(trim);
    m.overrides.list.forEach(trim);
  }

  // the delay without diversion only applies to automatic rotation without dual tariff (with it,
  // rotation happens at off-peak start): only then is its field shown and checked
  const rotationDelayShown = (m) => m.rotation.mode === 'AUTO' && !m.dualTariff.enabled;
  const ROTATION_DELAY_MAX = 65534; // the firmware counts the seconds in 16 bits (validation.h)
  const rotationDelayValid = (s) => Number.isInteger(s) && s >= 1 && s <= ROTATION_DELAY_MAX;

  // the loads a rotation group editor offers: every load, plus any load of the group that is no
  // longer in the map, so that it can be unticked (validate.js flags it as not a load)
  function rotationChoices(m, group) {
    const n = m.loads.length;
    return [...m.loads.map((_, l) => l), ...group.filter((l) => l >= n).sort((x, y) => x - y)];
  }

  function resizeLoads(m, count) {
    while (m.loads.length < count) {
      const used = new Set(m.loads.filter((l) => l.type === 'local').map((l) => l.pin));
      if (tempSensorPresent(m)) used.add(tempPin(m)); // the probes' bus
      const pin = [5, 6, 7, 3, 4, 8, 9].find((p) => !used.has(p)) || 2;
      m.loads.push({ type: 'local', pin });
    }
    m.loads.length = count;
    m.priorities = m.loads.map((_, i) => i);
    dropTargets(m, (x) => !('load' in x) || x.load < count);
    return m;
  }

  // New relays start without a pin, with the thresholds of the README example
  function resizeRelays(m, count) {
    while (m.relays.list.length < count) m.relays.list.push({ pin: null, surplus: 1000, import: 200, minOn: 5, minOff: 5 });
    m.relays.list.length = count;
    dropTargets(m, (x) => !('relay' in x) || x.relay < count);
    return m;
  }

  return {
    FORMAT_VERSION,
    LIMITS,
    DIGITAL_PINS,
    RF_PINS,
    MK2WIFI_GPIO,
    MK2WIFI_ONEWIRE_GPIO,
    MK2WIFI_UART,
    PCB_VERSIONS,
    SERIAL_OUTPUTS,
    ROTATION_MODES,
    RF_FREQUENCIES,
    OVERRIDE_GROUPS,
    TEMP_MODES,
    TEMP_PIN,
    PHASES,
    defaults,
    normalize,
    saveRouter,
    singlePhaseUrl,
    clone,
    remoteUnitsUsed,
    remoteLoadCount,
    loadsOfUnit,
    remoteLoadsPresent,
    boostWindow,
    boostFits,
    reanchor,
    withDuration,
    withStart,
    fitBoost,
    rfChipPresent,
    offeredPins,
    tempSensorPresent,
    tempPin,
    removeSensor,
    overridePresent,
    serialOutputOf,
    controlInputs,
    wifiInputs,
    parseAddress,
    crc8,
    calibrationArray,
    parseCalibration,
    calibrationResult,
    calValuesOf,
    CT_RATING,
    ctRatingUsed,
    CAL_NAMES,
    rotationChoices,
    rotationDelayShown,
    rotationDelayValid,
    resizeLoads,
    resizeRelays,
  };
});
