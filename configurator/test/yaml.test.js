// The mk2Wifi YAML: what the router sends, and fail-safe controls.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const M = require('../model.js');
const Y = require('../yaml.js');

function wifi(change) {
  const m = M.defaults();
  m.serialOutput = 'IoT';
  m.mk2wifi.enabled = true;
  if (change) change(m);
  return Y.yaml(m);
}

// the YAML list entry holding 'id: <id>'
function block(yaml, id) {
  const lines = yaml.split('\n');
  const at = lines.indexOf(`    id: ${id}`);
  assert.ok(at > 0, id);
  let start = at;
  while (!lines[start].startsWith('  - ')) --start;
  let end = at;
  while (end < lines.length && lines[end] !== '') ++end;
  return lines.slice(start, end).join('\n');
}

test('routing stop: released (routing) by default and at each boot', () => {
  const b = block(wifi((m) => (m.diversion = { enabled: true, pin: 8, wifi: true })), 'sw_diversion');
  assert.match(b, /number: GPIO3/);
  assert.match(b, /open_drain: true/);
  assert.doesNotMatch(b, /inverted/);
  assert.match(b, /restore_mode: ALWAYS_ON/);
});

test('boost, router OFF, off-peak: pulled LOW only when ON, OFF at each boot', () => {
  const y = wifi((m) => {
    m.overrides.list = [{ pin: 9, wifi: true, targets: 'ALL_LOADS' }];
    m.routerOff = { enabled: true, pin: 8, wifi: true };
    m.loads[0].pin = 4;
    Object.assign(m.dualTariff, { enabled: true, pin: 5, wifi: true });
  });
  for (const id of ['sw_boost_1', 'sw_router_off', 'sw_off_peak']) {
    const b = block(y, id);
    assert.match(b, /open_drain: true/, id);
    assert.match(b, /inverted: true/, id);
    assert.match(b, /restore_mode: ALWAYS_OFF/, id);
  }
  assert.match(y, /Solder jumpers to close on the module: D5, D8, D9/);
});

test('rotation: a pulse from a button, the pin released otherwise', () => {
  const y = wifi((m) => (m.rotation = { mode: 'PIN', pin: 8, wifi: true, afterSeconds: 3600 }));
  assert.match(block(y, 'sw_rotation'), /restore_mode: ALWAYS_OFF/);
  assert.match(y, /switch\.turn_on: sw_rotation\n {6}- delay: 2s.*\n {6}- switch\.turn_off: sw_rotation/);
});

test('inputs not driven by the module get no control', () => {
  const y = wifi((m) => (m.diversion = { enabled: true, pin: 8, wifi: false }));
  assert.doesNotMatch(y, /sw_diversion/);
  assert.match(y, /Solder jumpers to close on the module: none/);
});

test('sensors follow what the router sends', () => {
  const y = wifi((m) => {
    m.relays.enabled = true;
    m.relays.list = [{ pin: 8, surplus: 1000, import: 200, minOn: 5, minOff: 5 }];
    m.temperature.mode = 'router';
    Object.assign(m.dualTariff, { enabled: true, pin: 9 });
  });
  for (const tag of ['P', 'P1', 'P3', 'V1', 'V3', 'D1', 'D3', 'R', 'R1', 'T1', 'T2', 'N', 'TA', 'S', 'S_MC']) assert.match(y, new RegExp(`tag_name: "${tag}"\n`), tag);
  assert.doesNotMatch(y, /tag_name: "(D4|R2|T3)"/);
  assert.doesNotMatch(y, /dallas_temp/);
});

test('probes on the module: dallas_temp on GPIO23, no T tags', () => {
  const y = wifi((m) => (m.temperature.mode = 'esp'));
  assert.match(y, /one_wire:\n {2}- platform: gpio\n {4}pin: GPIO23/);
  assert.match(y, /platform: dallas_temp\n {4}address: 0x28BE416B090000A4/);
  assert.doesNotMatch(y, /tag_name: "T1"/);
});

test('without the IoT output, no UART and no router sensor', () => {
  const y = wifi((m) => (m.serialOutput = 'HumanReadable'));
  assert.doesNotMatch(y, /uart:|mk2pvrouter:|platform: mk2pvrouter/);
});

test('entity names follow the chosen language', () => {
  assert.match(wifi(), /name: "Puissance instantanée"/);
  assert.match(
    wifi((m) => (m.mk2wifi.language = 'en')),
    /name: "Grid power"/
  );
});
