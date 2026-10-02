/*
 * PVRouter configurator - model -> firmware files.
 *
 * Pure functions: each one returns the full text of a file. The layout follows the shipped
 * files (and clang-format: trailing comments aligned within runs of consecutive lines), so the
 * defaults give back the shipped files byte for byte.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory(require('./model.js'), require('./yaml.js'));
  else root.PVRGenerate = factory(root.PVRModel, root.PVRYaml);
})(typeof self !== 'undefined' ? self : this, function (M, Y) {
  'use strict';

  // ---- formatting helpers ----

  // Lines are strings, or [code, comment] pairs whose comments clang-format aligns: within a
  // run of consecutive pairs, every comment starts at the same column, at least 1 space after
  // the code for a block comment and 2 spaces for a line comment.
  function render(lines) {
    const out = [];
    let i = 0;
    while (i < lines.length) {
      if (!Array.isArray(lines[i])) {
        out.push(lines[i++]);
        continue;
      }
      let j = i;
      let column = 0;
      while (j < lines.length && Array.isArray(lines[j])) {
        const [code, comment] = lines[j++];
        column = Math.max(column, code.length + (comment.startsWith('//') ? 2 : 1));
      }
      for (; i < j; ++i) out.push(lines[i][0].padEnd(column) + lines[i][1]);
    }
    return out.join('\n') + '\n';
  }

  const bool = (b) => (b ? 'true' : 'false');
  const pin = (p) => (p === null || p === undefined ? 'unused_pin' : String(p));
  const today = () => new Date().toISOString().slice(0, 10);

  // A braced list: on one line if it has a single entry, else one entry per line, the next ones
  // aligned on the first. 'prefix' is the text before the list on its first line.
  function list(prefix, entries, suffix, comment) {
    if (entries.length === 1) return [[`${prefix}{ ${entries[0]} }${suffix}`, comment]];
    const indent = ' '.repeat(prefix.length + 2);
    const lines = [`${prefix}{ ${entries[0]},`];
    for (let k = 1; k < entries.length - 1; ++k) lines.push(`${indent}${entries[k]},`);
    lines.push([`${indent}${entries[entries.length - 1]} }${suffix}`, comment]);
    return lines;
  }

  const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');

  function overrideTargets(targets) {
    if (typeof targets === 'string') return `${targets}()`;
    const items = targets.map((t) => ('relay' in t ? `RELAY(${t.relay})` : `LOAD(${t.load})`));
    return `{ ${items.join(', ')} }`;
  }

  function rotationSeconds(s) {
    if (s % 3600 === 0) return `${s / 3600}UL * 3600UL`;
    if (s % 60 === 0) return `${s / 60}UL * 60UL`;
    return `${s}UL`;
  }

  // Trailing entries equal to the default ({ 0, UINT16_MAX }) can be left out
  function forceEntries(m) {
    const f = m.dualTariff.force.slice(0, m.loads.length);
    while (f.length && f[f.length - 1].start === 0 && f[f.length - 1].duration === null) f.pop();
    return f.map((e) => `{ ${e.start}, ${e.duration === null ? 'UINT16_MAX' : e.duration} }`);
  }

  // ---- config.h ----

  const SERIAL_COMMENT = {
    HumanReadable: '// Serial output type - Human readable for initial setup and commissioning',
    IoT: '// Serial output type - IoT, for the mk2Wifi module / Home Assistant (9600 baud, 7E1)',
    JSON: '// Serial output type - JSON',
  };

  function configH(m, date) {
    const L = [];
    const n = m.loads.length;
    const add = (...xs) => L.push(...xs);

    add(
      '/**',
      ' * @file config.h',
      ' * @brief Router configuration: loads, relays, control pins and features',
      ' *',
      ' * Edit it by hand, or create it with the configurator:',
      ' * https://fredm67.github.io/Mk2PVRouter/configurateur/',
      ' *',
      ' * @version 1.0',
      ` * @date ${date || today()}`,
      ' */',
      '',
      '#ifndef CONFIG_H',
      '#define CONFIG_H',
      '',
      '#include "config_system.h"',
      '#include "types.h"',
      '',
      SERIAL_COMMENT[m.serialOutput],
      `inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::${m.serialOutput};`,
      '',
      '// Debug messages (free RAM...), human-readable output only. The startup configuration and the',
      '// status messages do not depend on it: they are always printed in human-readable output.',
      `inline constexpr bool ENABLE_DEBUG{ ${bool(m.enableDebug)} };`,
      '',
      ['#include "debug.h"', '// needs SERIAL_OUTPUT_TYPE and ENABLE_DEBUG'],
      '',
      '// Calibration mode: the router measures and logs as usual, but never switches any load',
      '// (TRIACs, relays, remote loads, overrides), as with the router OFF. Set back to false once calibrated!',
      `inline constexpr bool CALIBRATION_MODE{ ${bool(m.calibrationMode)} };`,
      '',
      '//--------------------------------------------------------------------------------------------------',
      '// Basic Configuration',
      '//',
      [`inline constexpr uint8_t NO_OF_DUMPLOADS{ ${n} };`, '/**< TOTAL number of dump loads (local + remote) */'],
      '',
      '// ----------- Load map -----------',
      '//',
      '// One entry per dump load, in any order. Each entry says who drives the load:',
      '//   Load::local(pin)           - driven by the router itself, on that digital pin',
      "//   Load::remote(unit)         - driven by remote unit 'unit' (1..3), over RF",
      '//   Load::remote(unit, ledPin) - same, plus a status LED on the router',
      '//',
      '// See the pinout notes further down for which pins are available.',
      'inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{'
    );
    m.loads.forEach((l, i) => {
      const entry = l.type === 'local' ? `Load::local(${l.pin})` : l.led === null || l.led === undefined ? `Load::remote(${l.unit})` : `Load::remote(${l.unit}, ${l.led})`;
      add(`  ${entry}${i < n - 1 ? ',' : ''}`);
    });
    add(
      ['};', '/**< the load map: one packed entry per dump load */'],
      '',
      '// Derived from the load map above - never edit these by hand.',
      ['inline constexpr uint8_t NO_OF_REMOTE_LOADS{ Load::countRemoteLoads(physicalLoadPin) };', '/**< number of loads controlled via RF */'],
      ['inline constexpr uint8_t NO_OF_REMOTE_UNITS{ Load::countUnits(physicalLoadPin) };', '/**< number of remote units to talk to */'],
      ['inline constexpr bool REMOTE_LOADS_PRESENT{ NO_OF_REMOTE_LOADS != 0 };', '/**< automatically true if remote loads configured */'],
      '',
      '#include "remote_loads_core.h"',
      '',
      '/**',
      ' * @brief The one and only remote-load controller.',
      ' *',
      ' * @details Declared non-const on purpose: it holds the per-unit transmission state.',
      ' *          @c RemoteLoadCore has a @c constexpr default constructor and brace-or-equal',
      ' *          member initialisers, so this object is constant-initialized. It therefore',
      ' *          lands in @c .bss with no entry in @c .init_array, and @c --gc-sections drops',
      ' *          it outright when no remote unit is configured.',
      ' */',
      'inline RemoteLoadCore< NO_OF_REMOTE_UNITS > remoteLoads{};',
      '',
      '// Feature toggles',
      [`inline constexpr bool DIVERSION_PIN_PRESENT{ ${bool(m.diversion.enabled)} };`, "/**< set it to 'true' if you want to control diversion ON/OFF */"],
      [`inline constexpr bool ROUTER_OFF_PIN_PRESENT{ ${bool(m.routerOff.enabled)} };`, "/**< set it to 'true' if you want a pin that switches the router OFF */"],
      [`inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::${m.rotation.mode} };`, "/**< set it to 'OFF/AUTO/PIN' if you want manual/automatic rotation of priorities */"],
      [`inline constexpr bool OVERRIDE_PIN_PRESENT{ ${bool(M.overridePresent(m))} };`, "/**< set it to 'true' if there's a override pin */"],
      '',
      [`inline constexpr bool WATCHDOG_PIN_PRESENT{ ${bool(m.watchdog.enabled)} };`, "/**< set it to 'true' if there's a watch led */"],
      [`inline constexpr bool RELAY_DIVERSION{ ${bool(m.relays.enabled)} };`, "/**< set it to 'true' if a relay is used for diversion */"],
      [`inline constexpr bool DUAL_TARIFF{ ${bool(m.dualTariff.enabled)} };`, "/**< set it to 'true' if there's a dual tariff each day AND the router is connected to the billing meter */"],
      [`inline constexpr bool TEMP_SENSOR_PRESENT{ ${bool(M.tempSensorPresent(m))} };`, "/**< set it to 'true' if temperature sensing is needed */"],
      [`inline constexpr bool RF_LOGGING_PRESENT{ ${bool(m.rf.logging)} };`, "/**< set it to 'true' if RF data logging is needed */"],
      '',
      '#include "utils_dualtariff.h"',
      '#include "utils_relay.h"',
      '#include "utils_temp.h"',
      '',
      '// ----------- Pinout Assignments -----------',
      '//',
      '// ANALOG pins:',
      '// - All analog pins are reserved and hard-wired on the PCB.',
      '//',
      '// DIGITAL pins:',
      '// - D0 & D1: Reserved for the Serial interface.',
      '//',
      '// RFM69 module (RF data logging or remote loads):',
      '// - D2: Interrupt (IRQ).',
      '// - D10: Chip Select (CS).',
      '// - D11: Master Out Slave In (MOSI).',
      '// - D12: Master In Slave Out (MISO).',
      '// - D13: Serial Clock (SCK).',
      '//',
      '// mk2Wifi module (ESP32-C6, Home Assistant):',
      '// - D5 to D9 can be wired to the module, each through a solder jumper (open by default), as',
      '//   inputs driven from Home Assistant: routing stop, router OFF, boost, priority rotation...',
      '// - Only close the jumpers of pins that are free in this configuration: by default, the loads',
      '//   use D5, D6 and D7.',
      '',
      '// Load priority order at startup (array index = priority, 0 = highest)',
      '// Load indices refer to physicalLoadPin[] above, local and remote loads alike.',
      [`inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ ${m.priorities.join(', ')} };`, '/**< load priorities at startup (0=highest) */'],
      '',
      "// Set the value to 'unused_pin' when the pin is not needed (feature deactivated)",
      [`inline constexpr uint8_t dualTariffPin{ ${pin(m.dualTariff.enabled ? m.dualTariff.pin : null)} };`, '/**< for 3-phase PCB, off-peak trigger */'],
      [`inline constexpr uint8_t diversionPin{ ${pin(m.diversion.enabled ? m.diversion.pin : null)} };`, '/**< if LOW, no surplus diversion: only forced loads/relays are ON */'],
      [`inline constexpr uint8_t routerOffPin{ ${pin(m.routerOff.enabled ? m.routerOff.pin : null)} };`, '/**< if LOW, router OFF: every load and relay is OFF, forcing included */'],
      [`inline constexpr uint8_t rotationPin{ ${pin(m.rotation.mode === 'PIN' ? m.rotation.pin : null)} };`, '/**< if LOW, trigger a load priority rotation */'],
      [`inline constexpr uint8_t watchDogPin{ ${pin(m.watchdog.enabled ? m.watchdog.pin : null)} };`, '/**< watch dog LED */'],
      '',
      '//--------------------------------------------------------------------------------------------------',
      '// EWMA Filter Tuning for Cloud Immunity',
      '//',
      '// The RELAY_FILTER_DELAY parameter controls how aggressively the EWMA filter smooths',
      '// power measurements before making relay decisions. This directly affects cloud immunity vs responsiveness.',
      '//',
      '// \u{1F324}️ Quick Reference by Climate:',
      '// - Clear sky regions (desert, dry):     1 minute  (fast response, minimal clouds)',
      '// - Mixed conditions (most locations):   2 minutes (recommended default)',
      '// - Frequently cloudy (coastal):         3 minutes (enhanced stability)',
      '// - Very cloudy (mountain, tropical):    4 minutes (maximum stability)',
      '//',
      '// \u{1F9EA} Scientific Tuning:',
      '// Run cloud pattern analysis tests to determine optimal setting for your climate:',
      '//   cd Mk2_3phase_RFdatalog_temp && pio test -e native --filter="test_cloud_patterns" -v',
      '//',
      '// \u{1F4D6} Full guide: docs/Cloud_Pattern_Tuning_Guide.md',
      '//--------------------------------------------------------------------------------------------------',
      [`inline constexpr uint8_t RELAY_FILTER_DELAY{ ${m.relays.filterDelay} };`, '/**< EWMA filter delay in minutes for relay control */'],
      '',
      '// Relay configuration with tunable EWMA filter',
      '// For battery systems, use negative import threshold (turn OFF when surplus < abs(threshold))',
      '// Examples:',
      '//   Normal installation:  { pin, 1000, 200, 5, 5 }   // Turn OFF when importing > 200W',
      '//   Battery installation: { pin, 1000, -50, 5, 5 }   // Turn OFF when surplus < 50W'
    );
    const relayEntries = m.relays.list.map((r) => `{ ${pin(m.relays.enabled ? r.pin : null)}, ${r.surplus}, ${r.import}, ${r.minOn}, ${r.minOff} }`);
    const relayPrefix = 'inline constexpr RelayEngine relays{ ';
    add(`${relayPrefix}MINUTES(RELAY_FILTER_DELAY),`);
    add(...list(' '.repeat(relayPrefix.length), relayEntries, ' };', '/**< config for relay diversion with optimized EWMA filtering */'));
    add(
      '',
      ['#include "utils_override_helpers.h"', '// Provides LOAD(), RELAY(), ALL_LOADS(), ALL_RELAYS(), ALL_LOADS_AND_RELAYS()'],
      '',
      '// This is an example of override pin configuration.',
      '// You can modify the pin numbers and associated loads/relays as needed.',
      '// Ensure that the pins used do not conflict with other functionalities in your setup.',
      '//',
      '// Helper functions available:',
      '//   LOAD(n)           - Returns the pin for load n of physicalLoadPin[] (physical if the',
      '//                       load is local, virtual >= REMOTE_PIN_BASE if it is remote)',
      '//   LOCAL_LOAD(n)     - Same as LOAD(n), for a load known to be local',
      '//   REMOTE_LOAD(n)    - Returns the virtual pin for the n-th REMOTE load (>= 128).',
      '//                       n counts remote loads only, in physicalLoadPin[] order.',
      '//   ALL_LOCAL_LOADS() - uint32_t bitmask, lower 16 bits for local load pins',
      '//   ALL_REMOTE_LOADS()- uint32_t bitmask, upper 16 bits for remote loads (bit 16 = remote 0)',
      '//   ALL_LOADS()       - uint32_t combining local (lower 16 bits) and remote (upper 16 bits)',
      '//   RELAY(n)          - Returns pin for relay n',
      '//   ALL_RELAYS()      - uint32_t bitmask, lower 16 bits for relay pins',
      '//   ALL_LOADS_AND_RELAYS() - uint32_t combining all loads and relays',
      '//',
      '// Example configurations:',
      '// inline constexpr OverridePins overridePins{',
      '//   { { 4, ALL_LOADS() },                                  // Control all loads (local + remote)',
      '//     { 5, ALL_LOCAL_LOADS() },                            // Control only local loads',
      '//     { 6, ALL_REMOTE_LOADS() },                           // Control only remote loads',
      '//     { 7, { LOAD(0), REMOTE_LOAD(1) } },                  // Mixed: load 0 + the 2nd remote load',
      '//     { 8, { LOAD(0), LOAD(1), LOAD(2), LOAD(3) } },       // Using LOAD() for any load index',
      '//     { 9, ALL_LOADS_AND_RELAYS() } } };                   // All loads and relays',
      ''
    );
    // OVERRIDE_PIN_PRESENT false: the list is still compiled, but never read
    const overrides = m.overrides.list.length ? m.overrides.list : [{ pin: null, targets: 'ALL_LOADS' }];
    add(...list('inline constexpr OverridePins overridePins{ ', overrides.map((o) => `{ ${pin(o.pin)}, ${overrideTargets(o.targets)} }`), ' };', '/**< list of override pin/loads-relays pairs */'));
    const force = forceEntries(m);
    add(
      '',
      [`inline constexpr uint8_t ul_OFF_PEAK_DURATION{ ${m.dualTariff.offPeakHours} };`, '/**< Duration of the off-peak period in hours */'],
      [
        `inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]${force.length ? `{ ${force.join(', ')} }` : '{}'};`,
        force.length === 1 ? '/**< force config for load #1 ONLY for dual tariff */' : '/**< force config of each load for dual tariff */',
      ],
      '',
      [`inline constexpr int16_t iTemperatureThreshold{ ${m.dualTariff.tempThreshold} };`, '/**< the temperature threshold to stop overriding in °C */'],
      ''
    );
    const sensors = m.temperature.sensors.map((s) => M.parseAddress(s.address)).filter(Boolean);
    const addresses = (sensors.length ? sensors : M.defaults().temperature.sensors.map((s) => M.parseAddress(s.address))).map((a) => `{ ${a.map(hex).join(', ')} }`);
    const tempPrefix = 'inline constexpr TemperatureSensing temperatureSensing{ ';
    add(`${tempPrefix}${pin(M.tempSensorPresent(m) ? m.temperature.pin : null)},`);
    add(...list(' '.repeat(tempPrefix.length), addresses, ' };', '/**< list of temperature sensor Addresses */'));
    add(
      '',
      [`inline constexpr uint32_t ROTATION_AFTER_SECONDS{ ${rotationSeconds(m.rotation.afterSeconds)} };`, '/**< rotates load priorities after this period of inactivity */'],
      '',
      ['#include "remote_loads.h"', '// the RF shell; needs remoteLoads and the RF feature flags above'],
      '',
      '#endif  // CONFIG_H'
    );
    return render(L);
  }

  // ---- config_system.h ----

  function configSystemH(m, date) {
    return render([
      '/**',
      ' * @file config_system.h',
      ' * @author Frédéric Metrich (frederic.metrich@live.fr)',
      ' * @brief Basic configuration values to be set by the end-user',
      ' * @version 0.1',
      ` * @date ${date || today()}`,
      ' *',
      ' * @copyright Copyright (c) 2023-2026',
      ' *',
      ' */',
      '',
      '#ifndef CONFIG_SYSTEM_H',
      '#define CONFIG_SYSTEM_H',
      '',
      '#include "type_traits.hpp"',
      '',
      ['inline constexpr uint8_t NO_OF_PHASES{ 3 };', '/**< number of phases of the main supply. */'],
      '',
      '//--------------------------------------------------------------------------------------------------',
      '// for users with zero-export profile, this value will be negative',
      [`inline constexpr int16_t REQUIRED_EXPORT_IN_WATTS{ ${m.requiredExport} };`, '/**< when set to a negative value, this acts as a PV generator */'],
      [`inline constexpr int16_t DIVERSION_START_THRESHOLD_WATTS{ ${m.diversionStartThreshold} };`, '/**< Adjust value as needed - this means 50W surplus is needed to start diversion */'],
      '',
      '//--------------------------------------------------------------------------------------------------',
      '// other system constants, should match most of installations',
      [`inline constexpr uint8_t SUPPLY_FREQUENCY{ ${m.supplyFrequency} };`, '/**< number of cycles/s of the grid power supply */'],
      '',
      ['inline constexpr uint32_t WORKING_ZONE_IN_JOULES{ 3600UL };', '/**< number of joule for 1Wh */'],
      '',
      [`inline constexpr uint8_t DATALOG_PERIOD_IN_SECONDS{ ${m.datalogPeriod} };`, '/**< Period of datalogging in seconds */'],
      '',
      [
        'inline constexpr typename conditional< DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY >= UINT8_MAX, uint16_t, uint8_t >::type DATALOG_PERIOD_IN_MAINS_CYCLES{ DATALOG_PERIOD_IN_SECONDS * SUPPLY_FREQUENCY };',
        '/**< Period of datalogging in cycles */',
      ],
      '',
      "// Computes inverse value at compile time to use '*' instead of '/'",
      'inline constexpr float invDATALOG_PERIOD_IN_MAINS_CYCLES{ 1.0F / DATALOG_PERIOD_IN_MAINS_CYCLES };',
      '//--------------------------------------------------------------------------------------------------',
      '',
      '#endif  // CONFIG_SYSTEM_H',
    ]);
  }

  // ---- config_rf.h (router) ----

  function configRfH(m, date) {
    const r = m.rf;
    const ids = r.remoteIds.slice(0, M.LIMITS.MAX_REMOTE_UNITS);
    const g = String(r.gatewayId);
    return render([
      '/**',
      ' * @file config_rf.h',
      ' * @author Frédéric Metrich (frederic.metrich@live.fr)',
      ' * @brief RF module configuration for RFM69',
      ' * @version 1.0',
      ` * @date ${date || today()}`,
      ' *',
      ' * @copyright Copyright (c) 2025-2026',
      ' *',
      ' * @details Central configuration file for RF communication parameters.',
      ' *          These settings must match between the router and remote load receiver.',
      ' */',
      '',
      '#ifndef CONFIG_RF_H',
      '#define CONFIG_RF_H',
      '',
      '#include <RFM69.h>',
      '',
      '/**',
      ' * @brief RF Module Configuration',
      ' * @details RF Network Topology:',
      ' *',
      ` *   Router (ID=${r.routerId}) -----> Remote Load Unit 1 (ID=${ids[0]})  [controls TRIACs]`,
      ` *        |          \`---> Remote Load Unit 2 (ID=${ids[1]})  [optional]`,
      ` *        |          \`---> Remote Load Unit 3 (ID=${ids[2]})  [optional]`,
      ' *        |',
      ` *        +-------------> Gateway (ID=${g})${' '.repeat(Math.max(1, 16 - g.length))}[receives telemetry data]`,
      ' *',
      ` *   - NETWORK_ID (${r.networkId}): Same for ALL devices on this RF network`,
      ` *   - ROUTER_NODE_ID (${r.routerId}): THIS router's unique address`,
      ' *   - REMOTE_NODE_ID[]: Addresses of the Arduinos controlling remote loads, unit 1 first',
      ` *   - GATEWAY_ID (${g}): Address of the telemetry receiver (optional, for data logging)`,
      ' *',
      ' *   Up to MAX_REMOTE_UNITS units are supported, each a separate Arduino, each driving',
      ' *   up to MAX_LOADS_PER_UNIT loads. Which loads belong to which unit is declared in the',
      ' *   load map in config.h, with Load::remote(unit). The table below only has to be long',
      ' *   enough to cover the highest unit number used there.',
      ' */',
      'namespace RFConfig',
      '{',
      '// Frequency band - MUST match your hardware and local regulations',
      '// Options: RF69_433MHZ, RF69_868MHZ (Europe), RF69_915MHZ (USA)',
      `inline constexpr uint8_t FREQUENCY{ ${r.frequency} };`,
      '',
      '// Network configuration',
      [`inline constexpr uint8_t NETWORK_ID{ ${r.networkId} };`, '/**< Network ID (1-255, MUST be identical on ALL devices) */'],
      [`inline constexpr uint8_t ROUTER_NODE_ID{ ${r.routerId} };`, "/**< THIS router's unique ID (1-30) */"],
      '',
      '// Destination node IDs - Addresses of REMOTE devices this router talks to',
      [`inline constexpr uint8_t GATEWAY_ID{ ${g} };`, '/**< Gateway for telemetry data (only if RF_LOGGING_PRESENT=true) */'],
      '',
      '/** @brief Node ID of each remote load unit, unit 1 first (only if REMOTE_LOADS_PRESENT=true) */',
      `inline constexpr uint8_t REMOTE_NODE_ID[]{ ${ids.join(', ')} };`,
      '',
      '// Hardware configuration',
      [`inline constexpr bool IS_RFM69HW{ ${bool(r.isHW)} };`, '/**< true for RFM69HW/HCW (high power), false for RFM69W/CW */'],
      [`inline constexpr uint8_t POWER_LEVEL{ ${r.powerLevel} };`, '/**< TX power level: 0-31 (31 = max power) */'],
      '',
      '// Pin configuration for RFM69 module (standard SPI pins)',
      ['inline constexpr uint8_t RF_CS_PIN{ 10 };', '/**< SPI Chip Select pin */'],
      ['inline constexpr uint8_t RF_IRQ_PIN{ 2 };', '/**< Interrupt pin (must be 2 or 3 on Arduino UNO) */'],
      '}',
      '',
      '#endif  // CONFIG_RF_H',
    ]);
  }

  // ---- RemoteLoadReceiver/config.h and config_rf.h, for remote unit 'unit' (1..3) ----

  function receiverConfigH(m, unit, date) {
    const u = m.units[unit - 1];
    const count = M.loadsOfUnit(m, unit) || u.loadPins.length;
    const pins = u.loadPins.slice(0, count);
    return render([
      '/**',
      ' * @file config.h',
      ' * @brief Configuration settings for Remote Load Receiver',
      ' * @version 2.0',
      ` * @date ${date || today()}`,
      ' * @author Frédéric Metrich (frederic.metrich@live.fr)',
      ' *',
      ' * @copyright Copyright (c) 2025-2026',
      ' */',
      '',
      '#ifndef CONFIG_H',
      '#define CONFIG_H',
      '',
      '#include <Arduino.h>',
      '#include "config_rf.h"',
      '',
      '// Import RF configuration',
      'using namespace RFConfig;',
      '',
      '// Load Configuration',
      [`inline constexpr uint8_t NO_OF_LOADS{ ${count} };`, '/**< Number of loads controlled by this unit */'],
      [`inline constexpr uint8_t loadPins[NO_OF_LOADS]{ ${pins.join(', ')} };`, '/**< Output pins for loads (active HIGH) */'],
      '',
      '// Status LED Configuration',
      [`inline constexpr uint8_t GREEN_LED_PIN{ ${u.greenLed} };`, '/**< Green LED: blinks while the main loop runs */'],
      [`inline constexpr uint8_t RED_LED_PIN{ ${u.redLed} };`, '/**< Red LED for RF link lost (fast blink) */'],
      [`inline constexpr bool STATUS_LEDS_PRESENT{ ${bool(u.statusLeds)} };`, '/**< Enable status LED support */'],
      '',
      '// Timing Configuration',
      ['inline constexpr unsigned long RF_TIMEOUT_MS{ 500 };', '/**< Lost RF link after this many milliseconds */'],
      ['inline constexpr unsigned long GREEN_LED_INTERVAL_MS{ 1000 };', '/**< Toggle the green LED this often */'],
      '',
      '// Red LED: OFF when RF OK, fast blink (~4Hz) when RF lost',
      'inline constexpr unsigned long RED_LED_INTERVAL_MS{ 125 };',
      '',
      '// Data structure for received commands (must match transmitter)',
      'struct RemoteLoadPayload',
      '{',
      ['  uint8_t loadBitmask;', '/**< Bit 0 = Load 0, Bit 1 = Load 1, etc. */'],
      '};',
      '',
      '// RF Status enumeration',
      'enum class RfStatus : uint8_t',
      '{',
      ['  OK,', '/**< RF link is active */'],
      ['  LOST', '/**< RF link has been lost */'],
      '};',
      '',
      '#endif  // CONFIG_H',
    ]);
  }

  function receiverConfigRfH(m, unit, date) {
    const r = m.rf;
    const id = r.remoteIds[unit - 1];
    return render([
      '/**',
      ' * @file config_rf.h',
      ' * @brief RF module configuration for Remote Load Receiver',
      ' * @version 1.0',
      ` * @date ${date || today()}`,
      ' * @author Frédéric Metrich (frederic.metrich@live.fr)',
      ' *',
      ' * @copyright Copyright (c) 2025-2026',
      ' *',
      ' * @details Central configuration file for RF communication parameters.',
      ' *          These settings MUST match the router configuration.',
      ' */',
      '',
      '#ifndef CONFIG_RF_H',
      '#define CONFIG_RF_H',
      '',
      '#include <RFM69.h>',
      '',
      '/**',
      ' * @brief RF Module Configuration',
      ' * @details RF Network from Remote Load Unit perspective:',
      ' *',
      ` *   Router (ID=${r.routerId}) -----> THIS Remote Load Unit (ID=${id})`,
      ' *',
      ` *   - NETWORK_ID (${r.networkId}): MUST match the router's NETWORK_ID`,
      ` *   - ROUTER_NODE_ID (${r.routerId}): Address of the router sending commands (MUST match router's ROUTER_NODE_ID)`,
      ` *   - REMOTE_NODE_ID (${id}): THIS unit's unique address (MUST match router's REMOTE_NODE_ID)`,
      ' *',
      ' *   Example: If you have 2 remote load units:',
      " *     - Unit #1: REMOTE_NODE_ID = 15 (matches router's first REMOTE_NODE_ID)",
      ' *     - Unit #2: REMOTE_NODE_ID = 16 (different ID, configured in a second router or separate config)',
      ' */',
      'namespace RFConfig',
      '{',
      '// Frequency band - MUST match router and local regulations',
      '// Options: RF69_433MHZ, RF69_868MHZ (Europe), RF69_915MHZ (USA)',
      `inline constexpr uint8_t FREQUENCY{ ${r.frequency} };`,
      '',
      '// Network configuration - MUST match router',
      [`inline constexpr uint8_t NETWORK_ID{ ${r.networkId} };`, "/**< Network ID (MUST match router's NETWORK_ID) */"],
      [`inline constexpr uint8_t ROUTER_NODE_ID{ ${r.routerId} };`, "/**< Router's ID (MUST match router's ROUTER_NODE_ID) */"],
      [`inline constexpr uint8_t REMOTE_NODE_ID{ ${id} };`, "/**< THIS unit's unique ID (MUST match router's REMOTE_NODE_ID) */"],
      '',
      '// Hardware configuration',
      [`inline constexpr bool IS_RFM69HW{ ${bool(r.isHW)} };`, '/**< true for RFM69HW/HCW (high power), false for RFM69W/CW */'],
      '',
      '// Pin configuration for RFM69 module (standard SPI pins)',
      ['inline constexpr uint8_t RF_CS_PIN{ 10 };', '/**< SPI Chip Select pin */'],
      ['inline constexpr uint8_t RF_IRQ_PIN{ 2 };', '/**< Interrupt pin (must be 2 or 3 on Arduino UNO) */'],
      '}',
      '',
      '#endif  // CONFIG_RF_H',
    ]);
  }

  // ---- all files for a model: [{ path, text }] ----

  function files(m, date) {
    const out = [
      { path: 'Mk2_3phase_RFdatalog_temp/config.h', text: configH(m, date) },
      { path: 'Mk2_3phase_RFdatalog_temp/config_system.h', text: configSystemH(m, date) },
    ];
    if (M.rfChipPresent(m)) out.push({ path: 'Mk2_3phase_RFdatalog_temp/config_rf.h', text: configRfH(m, date) });
    for (let unit = 1; unit <= M.remoteUnitsUsed(m); ++unit) {
      out.push({ path: `RemoteLoadReceiver-unit${unit}/config.h`, text: receiverConfigH(m, unit, date) });
      out.push({ path: `RemoteLoadReceiver-unit${unit}/config_rf.h`, text: receiverConfigRfH(m, unit, date) });
    }
    if (m.mk2wifi.enabled) out.push({ path: `${m.mk2wifi.name || 'mk2pvrouter'}.yaml`, text: Y.yaml(m) });
    return out;
  }

  return { render, configH, configSystemH, configRfH, receiverConfigH, receiverConfigRfH, files };
});
