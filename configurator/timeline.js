/*
 * PVRouter configurator - the geometry of the boost timeline.
 *
 * Pure functions, so the page (app.js) and the tests share them: where a boost sits on the
 * off-peak period, and what dragging it does. Times in minutes from the start of the off-peak
 * period; a boost is { from: 'start' | 'end', at, for } (model.js). Dragging snaps to 15 minutes,
 * keeps the boost's anchor ('from') and never leaves the period (M.boostFits()).
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory(require('./model.js'));
  else root.PVRTimeline = factory(root.PVRModel);
})(typeof self !== 'undefined' ? self : this, function (M) {
  'use strict';

  const SNAP = 15;

  const snap = (min) => Math.round(min / SNAP) * SNAP;
  const clamp = (v, lo, hi) => Math.min(Math.max(v, lo), hi);

  // A start in minutes, back into the boost's own terms
  const atOf = (b, start, periodMin) => (b.from === 'end' ? periodMin - start : start);

  // Drag the whole bar by deltaMin (positive: later)
  function move(b, deltaMin, periodMin) {
    const w = M.boostWindow(b, periodMin);
    const last = w.open ? periodMin - SNAP : periodMin - b.for; // the latest start that still fits
    const start = clamp(snap(w.start + deltaMin), 0, last);
    return { at: atOf(b, start, periodMin), for: b.for };
  }

  // Drag the free edge by deltaMin: the end of a boost counted from the start, the start of one
  // counted back from the end (its end stays). An open boost has no free edge.
  function resize(b, deltaMin, periodMin) {
    const w = M.boostWindow(b, periodMin);
    if (w.open) return { at: b.at, for: b.for };
    if (b.from === 'start') {
      const end = clamp(snap(w.end + deltaMin), w.start + SNAP, periodMin);
      return { at: b.at, for: end - w.start };
    }
    const start = clamp(snap(w.start + deltaMin), 0, w.end - SNAP);
    return { at: atOf(b, start, periodMin), for: w.end - start };
  }

  const toPercent = (min, periodMin) => (min / periodMin) * 100;

  // "start", "+1h" ... "+(hours-1)h", "end"
  function axisLabels(hours, t) {
    const labels = [t('tl.start')];
    for (let k = 1; k < hours; ++k) labels.push(`+${k}h`);
    labels.push(t('tl.end'));
    return labels;
  }

  // What a bar forces, short: "loads 1, 3", "remote 2", "relay 1", or the group's name. Loads
  // are numbered from 1 as on the page; remote loads among the remote loads.
  function targetsLabel(m, targets, t) {
    if (typeof targets === 'string') return t(`targets.${targets}`);
    const local = [];
    const remote = [];
    const relays = [];
    for (const x of targets) {
      if ('relay' in x) relays.push(x.relay + 1);
      else if ((m.loads[x.load] || {}).type === 'remote') remote.push(m.loads.slice(0, x.load + 1).filter((l) => l.type === 'remote').length);
      else local.push(x.load + 1);
    }
    const parts = [];
    if (local.length) parts.push(`${t(local.length > 1 ? 'tl.loads' : 'tl.load')} ${local.join(', ')}`);
    if (remote.length) parts.push(`${t('tl.remote')} ${remote.join(', ')}`);
    if (relays.length) parts.push(`${t('tl.relay')} ${relays.join(', ')}`);
    return parts.join(', ');
  }

  return { SNAP, snap, move, resize, toPercent, axisLabels, targetsLabel };
});
