// The geometry of the boost timeline: windows, snap, drag (move / resize) and labels.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const M = require('../model.js');
const TL = require('../timeline.js');

test('boostWindow: minutes from the off-peak start', () => {
  assert.deepStrictEqual(M.boostWindow({ from: 'start', at: 120, for: 120 }, 480), { start: 120, end: 240, open: false });
  assert.deepStrictEqual(M.boostWindow({ from: 'end', at: 180, for: 90 }, 480), { start: 300, end: 390, open: false });
  assert.deepStrictEqual(M.boostWindow({ from: 'end', at: 120, for: null }, 480), { start: 360, end: 480, open: true });
  assert.strictEqual(M.boostWindow({ from: 'start', at: null, for: 60 }, 480), null);
  assert.strictEqual(M.boostWindow({ from: 'middle', at: 60, for: 60 }, 480), null);
});

test('boostFits: the whole window inside the off-peak period', () => {
  assert.strictEqual(M.boostFits({ from: 'start', at: 420, for: 120 }, 480), false); // ends after
  assert.strictEqual(M.boostFits({ from: 'start', at: 300, for: 60 }, 240), false); // period shortened to 4 h
  assert.strictEqual(M.boostFits({ from: 'end', at: 480, for: null }, 480), true);
  assert.strictEqual(M.boostFits({ from: 'end', at: 0, for: null }, 480), false); // starts at the end
  assert.strictEqual(M.boostFits({ from: 'start', at: null, for: 60 }, 480), false);
  assert.strictEqual(M.boostFits({ from: 'start', at: 0, for: 0 }, 480), false); // lasts 0 min
});

test('snap: to 15 minutes', () => {
  assert.strictEqual(TL.snap(7), 0);
  assert.strictEqual(TL.snap(8), 15);
  assert.strictEqual(TL.snap(27), 30);
});

test('move: keeps the anchor and stays inside the period', () => {
  assert.deepStrictEqual(TL.move({ from: 'start', at: 7, for: 60 }, 20, 480), { at: 30, for: 60 });
  assert.deepStrictEqual(TL.move({ from: 'start', at: 400, for: 60 }, 60, 480), { at: 420, for: 60 }); // stops at the end
  assert.deepStrictEqual(TL.move({ from: 'start', at: 30, for: 60 }, -90, 480), { at: 0, for: 60 }); // stops at the start
  assert.deepStrictEqual(TL.move({ from: 'end', at: 180, for: 120 }, 60, 480), { at: 120, for: 120 }); // later = closer to the end
  assert.deepStrictEqual(TL.move({ from: 'end', at: 120, for: null }, 200, 480), { at: 15, for: null }); // open: start <= period - 15
});

test('resize: moves the free edge only', () => {
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 120 }, 45, 480), { at: 120, for: 165 });
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 120 }, 400, 480), { at: 120, for: 360 }); // up to the end
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 30 }, -60, 480), { at: 120, for: 15 }); // 15 min minimum
  assert.deepStrictEqual(TL.resize({ from: 'end', at: 180, for: 120 }, -60, 480), { at: 240, for: 180 }); // start earlier, end stays
  assert.deepStrictEqual(TL.resize({ from: 'end', at: 120, for: null }, 30, 480), { at: 120, for: null }); // open: unchanged
});

test('labels: axis, targets, position', () => {
  const words = {
    'tl.start': 'start',
    'tl.end': 'end',
    'tl.load': 'load',
    'tl.loads': 'loads',
    'tl.remote': 'remote',
    'tl.relay': 'relay',
    'targets.ALL_LOADS': 'all loads',
  };
  const t = (k) => words[k];
  assert.deepStrictEqual(TL.axisLabels(4, t), ['start', '+1h', '+2h', '+3h', 'end']);
  const m = M.defaults();
  m.loads = [
    { type: 'local', pin: 5 },
    { type: 'remote', unit: 1, led: null },
    { type: 'local', pin: 7 },
    { type: 'remote', unit: 1, led: null },
  ];
  assert.strictEqual(TL.targetsLabel(m, [{ load: 0 }, { load: 2 }], t), 'loads 1, 3');
  assert.strictEqual(TL.targetsLabel(m, [{ load: 2 }], t), 'load 3');
  assert.strictEqual(TL.targetsLabel(m, [{ load: 3 }, { relay: 0 }], t), 'remote 2, relay 1');
  assert.strictEqual(TL.targetsLabel(m, 'ALL_LOADS', t), 'all loads');
  assert.strictEqual(TL.toPercent(120, 480), 25);
});

test('drag a window that no longer fits: the result fits', () => {
  // period shortened to 3 h under boosts set for a longer one
  assert.deepStrictEqual(TL.move({ from: 'start', at: 0, for: 240 }, 15, 180), { at: 0, for: 180 });
  assert.deepStrictEqual(TL.move({ from: 'end', at: 60, for: 240 }, 15, 180), { at: 180, for: 180 });
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 300, for: 60 }, -15, 180), { at: 165, for: 15 });
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 170, for: 60 }, 15, 180), { at: 165, for: 15 });
  assert.deepStrictEqual(TL.resize({ from: 'end', at: 60, for: 240 }, -15, 180), { at: 75, for: 75 });
});
