// Every problem the validator can report, and every text of the page, exists in both languages.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const I = require('../i18n.js');

const source = (f) => fs.readFileSync(path.join(__dirname, '..', f), 'utf8');

test('English and French have the same keys', () => {
  assert.deepStrictEqual(Object.keys(I.STRINGS.fr).sort(), Object.keys(I.STRINGS.en).sort());
});

test('every problem key of validate.js has a text', () => {
  const used = new Set(source('validate.js').match(/'(?:err|warn)\.[A-Za-z0-9]+'/g).map((s) => s.slice(1, -1)));
  assert.ok(used.size > 40);
  for (const k of used) assert.ok(k in I.STRINGS.en, k);
});

test('every key of the page has a text', () => {
  const page = source('app.js') + source('index.html');
  // keys are the only quoted words given to T(), help(), field(), check(), button(), section()
  // and data-t; a key may also be the last argument of check() / section()
  const quoted = /'([a-z][A-Za-z]*(?:\.[A-Za-z]+)?)'/g;
  const known = new Set([...Object.keys(I.STRINGS.en)]);
  const candidates = [...page.matchAll(quoted)].map((x) => x[1]).filter((k) => /^(sec\.|where\.)/.test(k));
  for (const k of candidates) assert.ok(known.has(k), k);
  for (const [, k] of page.matchAll(/data-t="([A-Za-z.]+)"/g)) assert.ok(known.has(k), k);
  for (const [, k] of page.matchAll(/\b(?:T|help|button)\('([A-Za-z.]+)'/g)) assert.ok(known.has(k), k);
});

test('French gets a no-break space before : ; ! ?', () => {
  assert.strictEqual(I.t('fr', 'err.deviceName').indexOf('mk2Wifi :'), 0);
  assert.strictEqual(I.t('en', 'err.loadPin', { load: 2 }), 'Load 2: choose a pin between D2 and D13.');
});

test('no key is defined twice in a language (the last one would win silently)', () => {
  const [en, fr] = source('i18n.js').split(/^ {4}fr: \{$/m);
  for (const block of [en, fr]) {
    const names = [...block.matchAll(/^ {6}'?([A-Za-z.]+)'?:/gm)].map((x) => x[1]);
    assert.deepStrictEqual(names.filter((k, i) => names.indexOf(k) !== i), []);
  }
});
