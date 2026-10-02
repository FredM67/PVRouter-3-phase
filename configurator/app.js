/*
 * PVRouter configurator - the page: form, problems, files.
 * Every change updates the model and redraws the page from it.
 */
(function () {
  'use strict';

  const M = window.PVRModel;
  const V = window.PVRValidate;
  const G = window.PVRGenerate;
  const I = window.PVRI18n;
  const Z = window.PVRZip;

  const STORE = 'pvrouter-configurator';
  let m = M.defaults();
  let lang = (navigator.language || 'en').toLowerCase().startsWith('fr') ? 'fr' : 'en';
  let activeFile = 0;

  try {
    const saved = JSON.parse(localStorage.getItem(STORE) || 'null');
    if (saved) {
      m = M.normalize(saved.model);
      lang = saved.lang === 'fr' ? 'fr' : 'en';
    }
  } catch (e) {
    m = M.defaults();
  }

  const T = (key, params) => I.t(lang, key, params);

  // ---- DOM helpers ----

  const PROPS = ['value', 'checked', 'disabled', 'selected', 'hidden'];

  function h(tag, props, ...kids) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(props || {})) {
      if (v === undefined || v === null || v === false) continue;
      if (k === 'class') el.className = v;
      else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
      else if (PROPS.includes(k)) el[k] = v;
      else el.setAttribute(k, v === true ? '' : v);
    }
    for (const c of kids.flat(Infinity)) if (c !== null && c !== undefined && c !== false) el.append(c.nodeType ? c : String(c));
    return el;
  }

  function changed() {
    try {
      localStorage.setItem(STORE, JSON.stringify({ model: m, lang }));
    } catch (e) {
      // private mode: no persistence
    }
    render();
  }

  const help = (key) => (key ? h('small', { class: 'help' }, T(key)) : null);
  const field = (labelKey, control, helpKey) => h('label', { class: 'field' }, h('span', null, T(labelKey)), control, help(helpKey));

  function num(get, set, attrs) {
    const a = attrs || {};
    return h('input', {
      type: 'number',
      value: get() === null || get() === undefined ? '' : String(get()),
      step: a.step || 1,
      min: a.min,
      max: a.max,
      onchange: (e) => {
        set(e.target.value === '' ? null : Number(e.target.value));
        changed();
      },
    });
  }

  function text(get, set, attrs) {
    return h('input', {
      type: 'text',
      value: get() || '',
      placeholder: (attrs && attrs.placeholder) || '',
      spellcheck: 'false',
      onchange: (e) => {
        set(e.target.value.trim());
        changed();
      },
    });
  }

  function check(get, set, labelKey, helpKey) {
    return h(
      'label',
      { class: 'check' },
      h('input', {
        type: 'checkbox',
        checked: !!get(),
        onchange: (e) => {
          set(e.target.checked);
          changed();
        },
      }),
      h('span', null, T(labelKey)),
      help(helpKey)
    );
  }

  // options: [[value, label]]; values may be numbers or null
  function select(get, set, options) {
    const current = JSON.stringify(get() === undefined ? null : get());
    return h(
      'select',
      {
        onchange: (e) => {
          set(JSON.parse(e.target.value));
          changed();
        },
      },
      options.map(([v, label]) => h('option', { value: JSON.stringify(v), selected: JSON.stringify(v) === current }, label))
    );
  }

  const pinLabel = (p) => (p >= 14 ? `A${p - 14}` : `D${p}`);
  const pinSelect = (get, set, opts) => {
    const o = opts || {};
    const pins = o.pins || M.DIGITAL_PINS;
    const options = pins.map((p) => [p, pinLabel(p)]);
    if (o.none || get() === null || get() === undefined) options.unshift([null, o.none ? T('none') : '—']);
    return select(get, set, options);
  };

  const row = (...kids) => h('div', { class: 'row' }, kids);
  const button = (labelKey, onclick, cls) =>
    h(
      'button',
      {
        type: 'button',
        class: cls || '',
        onclick: () => {
          onclick();
          changed();
        },
      },
      T(labelKey)
    );

  const wifiCheck = (obj, pin) =>
    m.mk2wifi.enabled && pin in M.MK2WIFI_GPIO
      ? check(
          () => obj.wifi,
          (v) => (obj.wifi = v),
          'viaWifi'
        )
      : null;

  function section(id, titleKey, problems, ...kids) {
    const bad = problems.filter((p) => sectionOf(p.field) === id);
    const level = bad.some((p) => p.level === 'error') ? 'error' : bad.length ? 'warning' : '';
    return h('fieldset', { id: `sec-${id}`, class: level }, h('legend', null, T(titleKey)), kids);
  }

  function sectionOf(f) {
    const head = String(f || '').split('.')[0];
    const map = {
      loads: 'loads',
      priorities: 'loads',
      relays: 'relays',
      diversion: 'controls',
      routerOff: 'controls',
      rotation: 'controls',
      watchdog: 'controls',
      overrides: 'controls',
      dualTariff: 'dualTariff',
      temperature: 'temperature',
      rf: 'rf',
      units: 'units',
      mk2wifi: 'mk2wifi',
    };
    return map[head] || (head === 'pins' ? '' : 'general');
  }

  // ---- sections ----

  function general(p) {
    return section(
      'general',
      'sec.general',
      p,
      field(
        'serialOutput',
        select(
          () => m.serialOutput,
          (v) => (m.serialOutput = v),
          M.SERIAL_OUTPUTS.map((s) => [s, T(`serial.${s}`)])
        )
      ),
      m.serialOutput === 'HumanReadable'
        ? check(
            () => m.enableDebug,
            (v) => (m.enableDebug = v),
            'enableDebug',
            'enableDebugHelp'
          )
        : null,
      check(
        () => m.calibrationMode,
        (v) => (m.calibrationMode = v),
        'calibrationMode',
        'calibrationModeHelp'
      ),
      row(
        field(
          'supplyFrequency',
          select(
            () => m.supplyFrequency,
            (v) => (m.supplyFrequency = v),
            [
              [50, '50'],
              [60, '60'],
            ]
          )
        ),
        field(
          'datalogPeriod',
          num(
            () => m.datalogPeriod,
            (v) => (m.datalogPeriod = v),
            { min: 1, max: 40 }
          )
        )
      ),
      row(
        field(
          'requiredExport',
          num(
            () => m.requiredExport,
            (v) => (m.requiredExport = v)
          ),
          'requiredExportHelp'
        ),
        field(
          'diversionStartThreshold',
          num(
            () => m.diversionStartThreshold,
            (v) => (m.diversionStartThreshold = v),
            { min: 0 }
          ),
          'diversionStartThresholdHelp'
        )
      )
    );
  }

  const loadName = (i) => {
    const l = m.loads[i];
    return `${T('load')} ${i + 1} (${l.type === 'local' ? pinLabel(l.pin || 0) : `${T('unit')} ${l.unit}`})`;
  };

  function loads(p) {
    const n = m.loads.length;
    const rows = m.loads.map((l, i) =>
      row(
        h('strong', null, `${T('load')} ${i + 1}`),
        select(
          () => l.type,
          (v) => {
            if (v === l.type) return;
            if (v === 'remote') Object.assign(l, { type: 'remote', unit: 1, led: null, pin: undefined });
            else Object.assign(l, { type: 'local', pin: null, unit: undefined, led: undefined });
          },
          [
            ['local', T('local')],
            ['remote', T('remote')],
          ]
        ),
        l.type === 'local'
          ? field(
              'pin',
              pinSelect(
                () => l.pin,
                (v) => (l.pin = v)
              )
            )
          : [
              field(
                'unit',
                select(
                  () => l.unit,
                  (v) => (l.unit = v),
                  [1, 2, 3].map((u) => [u, String(u)])
                )
              ),
              field(
                'led',
                pinSelect(
                  () => l.led,
                  (v) => (l.led = v),
                  { none: true }
                )
              ),
            ]
      )
    );
    const prios =
      n > 1
        ? h(
            'div',
            { class: 'priorities' },
            h('h3', null, T('priorities')),
            m.priorities.map((loadIdx, rank) =>
              row(
                h('span', null, `${T('priority')} ${rank + 1}`),
                select(
                  () => loadIdx,
                  (v) => {
                    // swap, so that each load stays once in the list
                    const other = m.priorities.indexOf(v);
                    m.priorities[other] = loadIdx;
                    m.priorities[rank] = v;
                  },
                  m.loads.map((_, i) => [i, loadName(i)])
                )
              )
            )
          )
        : null;
    return section(
      'loads',
      'sec.loads',
      p,
      help('loadsHelp'),
      field(
        'loadCount',
        num(
          () => n,
          (v) => M.resizeLoads(m, Math.max(1, Math.min(12, v || 1))),
          { min: 1, max: 12 }
        )
      ),
      rows,
      prios
    );
  }

  function relays(p) {
    const r = m.relays;
    return section(
      'relays',
      'sec.relays',
      p,
      check(
        () => r.enabled,
        (v) => {
          r.enabled = v;
          if (v && !r.list.length) M.resizeRelays(m, 1);
        },
        'relaysEnabled'
      ),
      r.enabled
        ? [
            field(
              'filterDelay',
              num(
                () => r.filterDelay,
                (v) => (r.filterDelay = v),
                { min: 1, max: 10 }
              ),
              'filterDelayHelp'
            ),
            field(
              'relayCount',
              num(
                () => r.list.length,
                (v) => M.resizeRelays(m, Math.max(1, Math.min(M.LIMITS.MAX_IOT_RELAYS, v || 1))),
                { min: 1, max: M.LIMITS.MAX_IOT_RELAYS }
              )
            ),
            r.list.map((x, i) =>
              row(
                h('strong', null, `${T('relay')} ${i + 1}`),
                field(
                  'pin',
                  pinSelect(
                    () => x.pin,
                    (v) => (x.pin = v)
                  )
                ),
                field(
                  'surplus',
                  num(
                    () => x.surplus,
                    (v) => (x.surplus = v),
                    { min: 1 }
                  )
                ),
                field(
                  'import',
                  num(
                    () => x.import,
                    (v) => (x.import = v)
                  )
                ),
                field(
                  'minOn',
                  num(
                    () => x.minOn,
                    (v) => (x.minOn = v),
                    { min: 0 }
                  )
                ),
                field(
                  'minOff',
                  num(
                    () => x.minOff,
                    (v) => (x.minOff = v),
                    { min: 0 }
                  )
                )
              )
            ),
            help('importHelp'),
          ]
        : null
    );
  }

  function inputPin(obj, enabledKey, labelKey, helpKey) {
    return h(
      'div',
      { class: 'block' },
      check(
        () => obj[enabledKey],
        (v) => (obj[enabledKey] = v),
        labelKey,
        helpKey
      ),
      obj[enabledKey]
        ? row(
            field(
              'pin',
              pinSelect(
                () => obj.pin,
                (v) => (obj.pin = v)
              )
            ),
            wifiCheck(obj, obj.pin)
          )
        : null
    );
  }

  function targetsEditor(o) {
    const custom = typeof o.targets !== 'string';
    const options = M.OVERRIDE_GROUPS.map((g) => [g, T(`targets.${g}`)]);
    options.push(['custom', T('targets.custom')]);
    const has = (t) => custom && o.targets.some((x) => JSON.stringify(x) === JSON.stringify(t));
    const toggle = (t, on) => {
      const rest = o.targets.filter((x) => JSON.stringify(x) !== JSON.stringify(t));
      o.targets = on ? [...rest, t] : rest;
    };
    const items = [...m.loads.map((_, i) => [{ load: i }, loadName(i)]), ...(m.relays.enabled ? m.relays.list.map((_, i) => [{ relay: i }, `${T('relay')} ${i + 1}`]) : [])];
    return [
      field(
        'targets',
        select(
          () => (custom ? 'custom' : o.targets),
          (v) => (o.targets = v === 'custom' ? [{ load: 0 }] : v),
          options
        )
      ),
      custom
        ? h(
            'div',
            { class: 'targets' },
            items.map(([t, label]) =>
              h(
                'label',
                { class: 'check' },
                h('input', {
                  type: 'checkbox',
                  checked: has(t),
                  onchange: (e) => {
                    toggle(t, e.target.checked);
                    changed();
                  },
                }),
                h('span', null, label)
              )
            )
          )
        : null,
    ];
  }

  function controls(p) {
    const o = m.overrides;
    const rot = m.rotation;
    return section(
      'controls',
      'sec.controls',
      p,
      help('controlsHelp'),
      m.mk2wifi.enabled ? help('viaWifiHelp') : null,
      inputPin(m.diversion, 'enabled', 'diversion', 'diversionHelp'),
      inputPin(m.routerOff, 'enabled', 'routerOff', 'routerOffHelp'),
      h(
        'div',
        { class: 'block' },
        check(
          () => o.enabled,
          (v) => {
            o.enabled = v;
            if (v && !o.list.length) o.list.push({ pin: null, wifi: false, targets: 'ALL_LOADS' });
          },
          'overrides',
          'overridesHelp'
        ),
        o.enabled
          ? [
              o.list.map((x, i) =>
                h(
                  'div',
                  { class: 'sub' },
                  row(
                    h('strong', null, `${T('override')} ${i + 1}`),
                    field(
                      'pin',
                      pinSelect(
                        () => x.pin,
                        (v) => (x.pin = v)
                      )
                    ),
                    wifiCheck(x, x.pin),
                    o.list.length > 1 ? button('remove', () => o.list.splice(i, 1), 'small') : null
                  ),
                  targetsEditor(x)
                )
              ),
              button('add', () => o.list.push({ pin: null, wifi: false, targets: 'ALL_LOADS' })),
            ]
          : null
      ),
      h(
        'div',
        { class: 'block' },
        field(
          'rotation',
          select(
            () => rot.mode,
            (v) => (rot.mode = v),
            M.ROTATION_MODES.map((r) => [r, T(`rotation.${r}`)])
          )
        ),
        rot.mode === 'PIN'
          ? row(
              field(
                'pin',
                pinSelect(
                  () => rot.pin,
                  (v) => (rot.pin = v)
                )
              ),
              wifiCheck(rot, rot.pin)
            )
          : null,
        rot.mode === 'AUTO'
          ? field(
              'rotationAfter',
              num(
                () => rot.afterSeconds / 3600,
                (v) => (rot.afterSeconds = Math.round((v || 0) * 3600)),
                { min: 0.5, max: 24, step: 0.5 }
              )
            )
          : null
      ),
      inputPin(m.watchdog, 'enabled', 'watchdog')
    );
  }

  function dualTariff(p) {
    const d = m.dualTariff;
    const entry = (i) => {
      while (d.force.length <= i) d.force.push({ start: 0, duration: null });
      return d.force[i];
    };
    return section(
      'dualTariff',
      'sec.dualTariff',
      p,
      inputPin(d, 'enabled', 'dualTariffEnabled', 'dualTariffHelp'),
      d.enabled
        ? [
            row(
              field(
                'offPeakHours',
                num(
                  () => d.offPeakHours,
                  (v) => (d.offPeakHours = v),
                  { min: 1, max: 12 }
                )
              ),
              field(
                'tempThreshold',
                num(
                  () => d.tempThreshold,
                  (v) => (d.tempThreshold = v),
                  { min: 1, max: 100 }
                )
              )
            ),
            m.loads.map((_, i) => {
              const f = i < d.force.length ? d.force[i] : { start: 0, duration: null };
              return row(
                h('strong', null, `${T('force')} – ${loadName(i)}`),
                field(
                  'start',
                  num(
                    () => f.start,
                    (v) => (entry(i).start = v || 0)
                  )
                ),
                f.duration !== null
                  ? field(
                      'duration',
                      num(
                        () => f.duration,
                        (v) => (entry(i).duration = v),
                        { min: 0 }
                      )
                    )
                  : null,
                check(
                  () => f.duration === null,
                  (v) => (entry(i).duration = v ? null : 2),
                  'untilEnd'
                )
              );
            }),
          ]
        : null
    );
  }

  function temperature(p) {
    const t = m.temperature;
    return section(
      'temperature',
      'sec.temperature',
      p,
      field(
        'tempMode',
        select(
          () => t.mode,
          (v) => (t.mode = v),
          M.TEMP_MODES.map((x) => [x, T(`temp.${x}`)])
        )
      ),
      t.mode === 'router'
        ? field(
            'pin',
            pinSelect(
              () => t.pin,
              (v) => (t.pin = v)
            )
          )
        : null,
      t.mode !== 'off'
        ? [
            t.sensors.map((s, i) =>
              row(
                h('strong', null, `${T('sensor')} ${i + 1}`),
                field(
                  'address',
                  text(
                    () => s.address,
                    (v) => (s.address = v),
                    { placeholder: '28BE416B090000A4' }
                  )
                ),
                field(
                  'sensorName',
                  text(
                    () => s.name,
                    (v) => (s.name = v)
                  )
                ),
                button('remove', () => t.sensors.splice(i, 1), 'small')
              )
            ),
            button('add', () => t.sensors.push({ address: '', name: '' })),
          ]
        : null
    );
  }

  function rf(p) {
    const r = m.rf;
    const units = M.remoteUnitsUsed(m);
    return section(
      'rf',
      'sec.rf',
      p,
      help('rfHelp'),
      check(
        () => r.logging,
        (v) => (r.logging = v),
        'rfLogging'
      ),
      M.rfChipPresent(m)
        ? [
            row(
              field(
                'frequency',
                select(
                  () => r.frequency,
                  (v) => (r.frequency = v),
                  M.RF_FREQUENCIES.map((f) => [f, f.replace('RF69_', '').replace('MHZ', ' MHz')])
                )
              ),
              field(
                'networkId',
                num(
                  () => r.networkId,
                  (v) => (r.networkId = v),
                  { min: 1, max: 250 }
                )
              ),
              field(
                'routerId',
                num(
                  () => r.routerId,
                  (v) => (r.routerId = v),
                  { min: 1, max: 30 }
                )
              )
            ),
            row(
              r.logging
                ? field(
                    'gatewayId',
                    num(
                      () => r.gatewayId,
                      (v) => (r.gatewayId = v),
                      { min: 1, max: 30 }
                    )
                  )
                : null,
              Array.from({ length: units }, (_, i) =>
                field(
                  'remoteId',
                  num(
                    () => r.remoteIds[i],
                    (v) => (r.remoteIds[i] = v),
                    { min: 1, max: 30 }
                  )
                )
              ).map((el, i) => {
                el.firstChild.textContent += ` ${i + 1}`;
                return el;
              })
            ),
            row(
              check(
                () => r.isHW,
                (v) => (r.isHW = v),
                'isHW'
              ),
              field(
                'powerLevel',
                num(
                  () => r.powerLevel,
                  (v) => (r.powerLevel = v),
                  { min: 0, max: 31 }
                )
              )
            ),
          ]
        : null
    );
  }

  function units(p) {
    const count = M.remoteUnitsUsed(m);
    if (!count) return null;
    const pins = [3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19];
    return section(
      'units',
      'sec.units',
      p,
      help('unitsHelp'),
      Array.from({ length: count }, (_, k) => {
        const u = m.units[k];
        const n = M.loadsOfUnit(m, k + 1);
        while (u.loadPins.length < n) u.loadPins.push(pins.find((x) => !u.loadPins.includes(x) && x !== u.greenLed && x !== u.redLed));
        return h(
          'div',
          { class: 'sub' },
          h('h3', null, `${T('unit')} ${k + 1}`),
          row(
            Array.from({ length: n }, (_, i) =>
              field(
                'loadPin',
                pinSelect(
                  () => u.loadPins[i],
                  (v) => (u.loadPins[i] = v),
                  { pins }
                )
              )
            )
          ),
          row(
            check(
              () => u.statusLeds,
              (v) => (u.statusLeds = v),
              'statusLeds'
            ),
            u.statusLeds
              ? [
                  field(
                    'greenLed',
                    pinSelect(
                      () => u.greenLed,
                      (v) => (u.greenLed = v),
                      { pins }
                    )
                  ),
                  field(
                    'redLed',
                    pinSelect(
                      () => u.redLed,
                      (v) => (u.redLed = v),
                      { pins }
                    )
                  ),
                ]
              : null
          )
        );
      })
    );
  }

  function mk2wifi(p) {
    const w = m.mk2wifi;
    const jumpers = M.wifiInputs(m)
      .map((c) => `D${c.pin}`)
      .sort();
    return section(
      'mk2wifi',
      'sec.mk2wifi',
      p,
      check(
        () => w.enabled,
        (v) => {
          w.enabled = v;
          if (v) m.serialOutput = 'IoT';
        },
        'mk2wifiEnabled'
      ),
      w.enabled
        ? [
            row(
              field(
                'deviceName',
                text(
                  () => w.name,
                  (v) => (w.name = v)
                )
              ),
              field(
                'friendlyName',
                text(
                  () => w.friendlyName,
                  (v) => (w.friendlyName = v)
                )
              ),
              field(
                'haLanguage',
                select(
                  () => w.language,
                  (v) => (w.language = v),
                  [
                    ['fr', 'Français'],
                    ['en', 'English'],
                  ]
                )
              )
            ),
            h(
              'dl',
              { class: 'info' },
              h('dt', null, T('jumpers')),
              h('dd', null, jumpers.length ? jumpers.join(', ') : T('none')),
              h('dt', null, T('tempJumper')),
              h('dd', null, T(m.temperature.mode === 'esp' ? 'tempJumperEsp' : 'tempJumperRouter'))
            ),
            h('p', { class: 'note' }, T('vselNote')),
          ]
        : null
    );
  }

  // ---- problems and files ----

  function renderProblems(problems) {
    const ul = document.getElementById('problems');
    ul.replaceChildren(
      ...(problems.length
        ? problems.map((p) =>
            h(
              'li',
              {
                class: p.level,
                onclick: () => {
                  const s = document.getElementById(`sec-${sectionOf(p.field)}`);
                  if (s) s.scrollIntoView({ behavior: 'smooth', block: 'start' });
                },
              },
              T(p.key, p.params)
            )
          )
        : [h('li', { class: 'ok' }, T('noProblem'))])
    );
  }

  function download(name, data, type) {
    const url = URL.createObjectURL(new Blob([data], { type }));
    const a = h('a', { href: url, download: name });
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  function renderOutput(problems) {
    const box = document.getElementById('output');
    if (V.hasErrors(problems)) {
      box.replaceChildren(h('p', { class: 'blocked' }, T('outputBlocked')));
      return;
    }
    const files = G.files(m);
    if (activeFile >= files.length) activeFile = 0;
    const f = files[activeFile];
    const base = (path) => path.split('/').pop();
    const copyBtn = h(
      'button',
      {
        type: 'button',
        onclick: async () => {
          await navigator.clipboard.writeText(f.text);
          copyBtn.textContent = T('copied');
          setTimeout(() => (copyBtn.textContent = T('copy')), 1500);
        },
      },
      T('copy')
    );
    box.replaceChildren(
      h(
        'div',
        { class: 'tabs' },
        files.map((x, i) =>
          h(
            'button',
            {
              type: 'button',
              class: i === activeFile ? 'active' : '',
              title: x.path,
              onclick: () => {
                activeFile = i;
                renderOutput(problems);
              },
            },
            x.path.includes('/') ? `${x.path.split('/')[0].replace('Mk2_3phase_RFdatalog_temp', 'router')}/${base(x.path)}` : x.path
          )
        )
      ),
      h(
        'div',
        { class: 'actions' },
        copyBtn,
        h('button', { type: 'button', onclick: () => download(base(f.path), f.text, 'text/plain') }, T('download')),
        h('button', { type: 'button', class: 'primary', onclick: () => download('pvrouter-config.zip', Z.zip(files), 'application/zip') }, T('downloadAll'))
      ),
      h('pre', null, h('code', null, f.text)),
      h(
        'div',
        { class: 'where' },
        h('h3', null, T('where')),
        h(
          'ul',
          null,
          h('li', null, h('code', null, 'Mk2_3phase_RFdatalog_temp/'), ' ', T('where.router')),
          M.remoteUnitsUsed(m) ? h('li', null, h('code', null, 'RemoteLoadReceiver-unitN/'), ' ', T('where.unit')) : null,
          m.mk2wifi.enabled ? h('li', null, h('code', null, '*.yaml'), ' ', T('where.yaml')) : null
        )
      )
    );
  }

  function render() {
    document.documentElement.lang = lang;
    for (const el of document.querySelectorAll('[data-t]')) el.textContent = T(el.dataset.t);
    document.title = T('title');
    document.getElementById('lang').textContent = T('language');
    const problems = V.validate(m);
    const scroll = window.scrollY;
    document.getElementById('form').replaceChildren(
      ...[general, loads, relays, controls, dualTariff, temperature, rf, units, mk2wifi].map((s) => s(problems)).filter(Boolean)
    );
    window.scrollTo(0, scroll);
    renderProblems(problems);
    renderOutput(problems);
  }

  // ---- header buttons ----

  document.getElementById('lang').addEventListener('click', () => {
    lang = lang === 'fr' ? 'en' : 'fr';
    changed();
  });
  document.getElementById('save').addEventListener('click', () => download('pvrouter-config.json', JSON.stringify(m, null, 2) + '\n', 'application/json'));
  document.getElementById('load').addEventListener('change', async (e) => {
    const file = e.target.files[0];
    e.target.value = '';
    if (!file) return;
    try {
      const data = JSON.parse(await file.text());
      if (typeof data !== 'object' || data === null || !('formatVersion' in data)) throw new Error('not a save');
      m = M.normalize(data);
      activeFile = 0;
      changed();
    } catch (err) {
      alert(T('loadError'));
    }
  });
  document.getElementById('reset').addEventListener('click', () => {
    if (!confirm(T('resetConfirm'))) return;
    m = M.defaults();
    activeFile = 0;
    changed();
  });

  render();
})();
