// SPDX-License-Identifier: GPL-3.0-or-later
// Saves carry the router they are for; the page switches to the single-phase configurator.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const M = require('../model.js');
const I = require('../i18n.js');

test('saveRouter: a save without the field is a 3-phase save', () => {
  assert.strictEqual(M.defaults().router, 3);
  assert.strictEqual(M.saveRouter({ formatVersion: 1 }), 3);
  assert.strictEqual(M.saveRouter({ router: 1 }), 1);
  assert.strictEqual(M.saveRouter({ router: 3 }), 3);
});

test('normalize: refuses the saves of the single-phase router, loads the older ones without the field', () => {
  assert.throws(() => M.normalize({ ...M.defaults(), router: 1 }), /router/);
  const older = M.defaults();
  delete older.router;
  assert.strictEqual(M.normalize(older).router, 3);
  assert.strictEqual(M.normalize(M.defaults()).router, 3);
});

test('singlePhaseUrl: relative on the docs site, absolute elsewhere', () => {
  const abs = 'https://fredm67.github.io/Mk2PVRouter/configurateur-monophase/';
  assert.strictEqual(M.singlePhaseUrl({ protocol: 'https:', pathname: '/Mk2PVRouter/configurateur/' }), '../configurateur-monophase/');
  assert.strictEqual(M.singlePhaseUrl({ protocol: 'https:', pathname: '/Mk2PVRouter/branch/dev/configurateur/index.html' }), '../configurateur-monophase/');
  assert.strictEqual(M.singlePhaseUrl({ protocol: 'file:', pathname: '/x/Mk2PVRouter/configurator/index.html' }), abs);
  assert.strictEqual(M.singlePhaseUrl({ protocol: 'https:', pathname: '/PVRouter-3-phase/configurator/' }), abs);
});

test('texts: the 3-phase page, the switch and the refused save, in both languages', () => {
  for (const lang of ['en', 'fr']) {
    assert.match(I.t(lang, 'title'), /3-phase|triphasé/);
    assert.ok(I.t(lang, 'router.refused', { link: 'U' }).includes('U'));
    for (const k of ['router.label', 'router.one', 'router.three', 'router.dismiss']) assert.ok(I.t(lang, k) !== k, k);
  }
});
