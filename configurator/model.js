/*
 * PVRouter configurator - the settings and their defaults.
 *
 * The defaults are the shipped firmware files: generating them must give back
 * Mk2_3phase_RFdatalog_temp/config.h, config_system.h and config_rf.h byte for byte
 * (checked by test/generate.test.js).
 *
 * Works as a classic <script> (global PVRModel) and as a Node module.
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

  const SERIAL_OUTPUTS = ['HumanReadable', 'IoT', 'JSON'];
  const ROTATION_MODES = ['OFF', 'AUTO', 'PIN'];
  const RF_FREQUENCIES = ['RF69_433MHZ', 'RF69_868MHZ', 'RF69_915MHZ'];
  const OVERRIDE_GROUPS = ['ALL_LOADS', 'ALL_LOCAL_LOADS', 'ALL_REMOTE_LOADS', 'ALL_RELAYS', 'ALL_LOADS_AND_RELAYS'];
  const TEMP_MODES = ['off', 'router', 'esp'];

  function defaults() {
    return {
      formatVersion: FORMAT_VERSION,

      // config.h - general
      serialOutput: 'HumanReadable',
      enableDebug: true,
      calibrationMode: false,

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
      rotation: { mode: 'OFF', pin: null, wifi: false, afterSeconds: 8 * 3600 },
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
        // one entry per load: start offset (hours, or minutes beyond 24) and duration
        // (null = until the end of the off-peak period)
        force: [{ start: -3, duration: 2 }],
        tempThreshold: 100,
      },

      temperature: {
        mode: 'off', // 'router': the router reads the probes; 'esp': the mk2Wifi module does
        pin: 3,
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

  // Fill what an older or hand-edited JSON lacks, keep the rest
  function normalize(input) {
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

  // ---- derived values, shared by the validator, the generator and the page ----

  const remoteUnitsUsed = (m) => {
    let n = 0;
    for (const l of m.loads) if (l.type === 'remote' && l.unit > n) n = l.unit;
    return n;
  };
  const remoteLoadCount = (m) => m.loads.filter((l) => l.type === 'remote').length;
  const loadsOfUnit = (m, unit) => m.loads.filter((l) => l.type === 'remote' && l.unit === unit).length;
  const remoteLoadsPresent = (m) => remoteLoadCount(m) > 0;
  const rfChipPresent = (m) => m.rf.logging || remoteLoadsPresent(m);
  const tempSensorPresent = (m) => m.temperature.mode === 'router';
  const overridePresent = (m) => m.overrides.enabled && m.overrides.list.length > 0;

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
  function resizeLoads(m, count) {
    while (m.loads.length < count) {
      const used = new Set(m.loads.filter((l) => l.type === 'local').map((l) => l.pin));
      const pin = [5, 6, 7, 3, 4, 8, 9].find((p) => !used.has(p)) || 2;
      m.loads.push({ type: 'local', pin });
    }
    m.loads.length = count;
    m.priorities = m.loads.map((_, i) => i);
    while (m.dualTariff.force.length > count) m.dualTariff.force.pop();
    return m;
  }

  // New relays start without a pin, with the thresholds of the README example
  function resizeRelays(m, count) {
    while (m.relays.list.length < count) m.relays.list.push({ pin: null, surplus: 1000, import: 200, minOn: 5, minOff: 5 });
    m.relays.list.length = count;
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
    SERIAL_OUTPUTS,
    ROTATION_MODES,
    RF_FREQUENCIES,
    OVERRIDE_GROUPS,
    TEMP_MODES,
    defaults,
    normalize,
    clone,
    remoteUnitsUsed,
    remoteLoadCount,
    loadsOfUnit,
    remoteLoadsPresent,
    rfChipPresent,
    tempSensorPresent,
    overridePresent,
    controlInputs,
    wifiInputs,
    parseAddress,
    crc8,
    resizeLoads,
    resizeRelays,
  };
});
