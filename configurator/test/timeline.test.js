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

test('switching the anchor keeps the window', () => {
  // 0 h after the start, for 2 h -> 8 h before the end, for 2 h (8 h period): still 0h-2h
  assert.deepStrictEqual(M.reanchor({ from: 'start', at: 0, for: 120 }, 'end', 480), { from: 'end', at: 480, for: 120 });
  assert.deepStrictEqual(M.reanchor({ from: 'end', at: 180, for: 90 }, 'start', 480), { from: 'start', at: 300, for: 90 });
  assert.deepStrictEqual(M.reanchor({ from: 'end', at: 120, for: null }, 'start', 480), { from: 'start', at: 360, for: null });
  assert.deepStrictEqual(M.reanchor({ from: 'start', at: 60, for: 60 }, 'start', 480), { from: 'start', at: 60, for: 60 }); // same anchor
  assert.deepStrictEqual(M.reanchor({ from: 'start', at: null, for: 60 }, 'end', 480), { from: 'end', at: null, for: 60 }); // unreadable time: kept
});

test('changing the duration keeps the anchored edge', () => {
  // 2 h before the end, for 2 h (6h-8h) -> for 2 h 30: starts 2 h 30 before the end (5h30-8h)
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 120, for: 120 }, 150, 480), { at: 150, for: 150 });
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 180, for: 90 }, 60, 480), { at: 150, for: 60 }); // shorter: starts later
  assert.deepStrictEqual(M.withDuration({ from: 'start', at: 60, for: 60 }, 120, 480), { at: 60, for: 120 }); // after the start: start stays
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 120, for: null }, 120, 480), { at: 120, for: 120 }); // was until the end: end is the period's
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 120, for: 120 }, null, 480), { at: 120, for: null }); // until the end
});

test('changing the start keeps the boost inside the period', () => {
  // 3 h before the end, for 3 h (5h-8h) -> starts 2 h before the end: cut to 2 h (6h-8h)
  assert.deepStrictEqual(M.withStart({ from: 'end', at: 180, for: 180 }, 120, 480), { at: 120, for: 120 });
  assert.deepStrictEqual(M.withStart({ from: 'end', at: 180, for: 90 }, 240, 480), { at: 240, for: 90 }); // still fits: duration kept
  assert.deepStrictEqual(M.withStart({ from: 'start', at: 300, for: 120 }, 420, 480), { at: 420, for: 60 }); // after the start, cut too
  assert.deepStrictEqual(M.withStart({ from: 'start', at: 60, for: 60 }, 120, 480), { at: 120, for: 60 });
  assert.deepStrictEqual(M.withStart({ from: 'end', at: 120, for: null }, 60, 480), { at: 60, for: null }); // until the end
  assert.deepStrictEqual(M.withStart({ from: 'end', at: 120, for: 60 }, 600, 480), { at: 600, for: 60 }); // starts before the period: left to the error
});

test('minutes field: quarters of an hour', () => {
  assert.strictEqual(TL.quarter(0), 0);
  assert.strictEqual(TL.quarter(7), 0);
  assert.strictEqual(TL.quarter(8), 15);
  assert.strictEqual(TL.quarter(40), 45);
  assert.strictEqual(TL.quarter(59), 45); // never a full hour: the hours field does that
  assert.strictEqual(TL.quarter(-10), 0);
  assert.strictEqual(TL.quarter(null), 0);
});

test('shortening the off-peak period keeps the boosts inside it', () => {
  // 6 h from the start, period cut from 8 h to 5 h: 5 h from the start
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: 0, for: 360 }, 300), { at: 0, for: 300 });
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: 240, for: 120 }, 300), { at: 240, for: 60 }); // start kept, cut
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: 360, for: 60 }, 300), { at: 240, for: 60 }); // started after the end
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: 360, for: null }, 300), { at: 285, for: null });
  assert.deepStrictEqual(M.fitBoost({ from: 'end', at: 180, for: 240 }, 300), { at: 180, for: 180 }); // start kept, cut
  assert.deepStrictEqual(M.fitBoost({ from: 'end', at: 360, for: 360 }, 300), { at: 300, for: 300 }); // started before the start
  assert.deepStrictEqual(M.fitBoost({ from: 'end', at: 420, for: null }, 300), { at: 300, for: null });
  assert.deepStrictEqual(M.fitBoost({ from: 'end', at: 180, for: 120 }, 300), { at: 180, for: 120 }); // fits: unchanged
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: null, for: 60 }, 300), { at: null, for: 60 }); // unreadable: left to the error
});

test('no off-peak length (field emptied): the boost keeps what is typed', () => {
  // null * 60 = 0: no period to keep the boost in, so nothing is moved or cut
  assert.deepStrictEqual(M.reanchor({ from: 'start', at: 60, for: 60 }, 'end', 0), { from: 'end', at: 60, for: 60 });
  assert.deepStrictEqual(M.withStart({ from: 'end', at: 180, for: 180 }, 120, 0), { at: 120, for: 180 });
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 120, for: 120 }, 150, 0), { at: 120, for: 150 });
  assert.deepStrictEqual(M.fitBoost({ from: 'start', at: 0, for: 360 }, 0), { at: 0, for: 360 });
});

test('a new duration that does not fit moves the boost to end with the period', () => {
  // 30 min from 7 h, made 2 h: from 6 h to the end
  assert.deepStrictEqual(M.withDuration({ from: 'start', at: 420, for: 30 }, 120, 480), { at: 360, for: 120 });
  // 6 h-7 h counted from the end, made 8 h: the whole period
  assert.deepStrictEqual(M.withDuration({ from: 'end', at: 120, for: 60 }, 480, 480), { at: 480, for: 480 });
  // longer than the period: the period
  assert.deepStrictEqual(M.withDuration({ from: 'start', at: 0, for: 60 }, 600, 480), { at: 0, for: 480 });
  // 'until the end' unticked at 7 h: 2 h, from 6 h
  assert.deepStrictEqual(M.withDuration({ from: 'start', at: 420, for: null }, 120, 480), { at: 360, for: 120 });
});
