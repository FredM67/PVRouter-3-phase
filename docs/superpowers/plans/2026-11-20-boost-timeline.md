# Boost Timeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A timeline in the configurator's dual-tariff section, one lane per scheduled boost, whose bars can be dragged (move) and stretched (duration), kept in sync with the unchanged text fields.

**Architecture:** The geometry is pure functions: `M.boostWindow()` in `model.js` (shared with `validate.js`), and the drag maths and labels in a new `timeline.js` (UMD like the other modules, `root.PVRTimeline`), tested with `node --test`. `app.js` mounts the timeline above the boost cards and handles the Pointer Events.

**Tech Stack:** plain JavaScript (no dependency, no build step), `node --test`, headless Chrome for the page check.

**Spec:** `docs/superpowers/specs/2026-11-20-boost-timeline-design.md`

## Global Constraints

- Worktree `D:/Git/IoT/pvr-wt/boosts`, branch `feat/scheduled-boosts` (PR #223). Commits end with `Co-Authored-By: Claude <noreply@anthropic.com>`. Never commit `Mk2_3phase_RFdatalog_temp/version.h` or `.vscode/extensions.json`.
- No dependency, no build step; every module loads both as a `<script>` and with `require()` (the UMD wrapper of `validate.js`).
- Node only in WSL: `MSYS_NO_PATHCONV=1 wsl bash /mnt/d/...script.sh`, node at `~/.local/node/bin/node`; tests `node --test configurator/test/*.test.js` from `/mnt/d/Git/IoT/pvr-wt/boosts`.
- The model is unchanged: a boost is `{ from: 'start' | 'end', at: <minutes>, for: <minutes> | null, targets }`; the period is `m.dualTariff.offPeakHours * 60` minutes. `config.h` output does not change (the default page still gives back the shipped file byte for byte).
- A window fits when its start is in `[0, period)` and, with `for`, `start + for <= period` (#223's rule). Dragging never produces a window that does not fit, nor a duration under 15 min.
- Snap: 15 minutes while dragging; the text fields keep taking any minute.
- A boost keeps its anchor: dragging changes `at` (and `for`), never `from`.
- Boosts with `for: null` are drawn open-ended (dashed end, up to the end of the period) and can only be moved.
- Strings in `i18n.js`, en and fr; French: U+00A0 before `:`, `;`, `?`, `!`.

## Review Focus

1. **Shortening the off-peak period after placing boosts.** Bars that no longer fit are drawn in red, the existing `err.boostWindow` shows, nothing throws. Task 1 `boostWindow` test with a start past the period; Task 2 page check with 4 h.
2. **A boost with an empty time field (`at: null`).** The timeline must skip or flag it, not draw `NaN%`. Task 1 test `boostWindow` returns `null`; Task 2 renders no bar for it.
3. **Dragging past either end.** The window stops at the start / end of the period, the duration at 15 min minimum. Task 1 `move` / `resize` clamp tests.
4. **A boost typed to the minute (`at: 7`) then dragged.** It lands on the 15-minute grid. Task 1 snap test.
5. **Several boosts on the same load.** Each keeps its own lane; labels name the targets. Task 1 `targetsLabel` tests.

---

### Task 1: The geometry, tested

**Files:**
- Modify: `configurator/model.js` (add and export `boostWindow`)
- Modify: `configurator/validate.js` (the boost window rule uses `M.boostWindow`)
- Create: `configurator/timeline.js`
- Test: `configurator/test/timeline.test.js`

**Interfaces:**
- Produces, in `model.js`: `boostWindow(b, periodMin) -> { start, end, open } | null` (minutes from the off-peak start; `end` is `periodMin` when `open`; `null` when `at` is not an integer or `from` is unknown) and `boostFits(b, periodMin) -> boolean` (the #223 rule; `false` for `null` windows).
- Produces, `timeline.js` (`root.PVRTimeline`, `require('./model.js')`):
  - `SNAP = 15`
  - `snap(min) -> number` (nearest multiple of `SNAP`)
  - `move(b, deltaMin, periodMin) -> { at, for }`: shift the window by `deltaMin` (positive = later), snap the new start, clamp so it fits (`start >= 0`; with `for`: `start + for <= period`; open: `start <= period - SNAP`); `from: 'end'` turns the new start into `at = period - start`
  - `resize(b, deltaMin, periodMin) -> { at, for }`: the free edge moves by `deltaMin`; `from: 'start'`: the end moves (`for` changes, `at` stays); `from: 'end'`: the start moves, the end stays (`at` and `for` change together); snapped, `for >= SNAP`, window fits; open boosts are returned unchanged
  - `toPercent(min, periodMin) -> number` (0-100)
  - `axisLabels(hours, t) -> string[]` (`hours + 1` labels: `t('tl.start')`, `+1h` … `+(hours-1)h`, `t('tl.end')`)
  - `targetsLabel(m, targets, t) -> string` (a group: `t('targets.<GROUP>')`; a list: local loads by their number from 1 — `loads 1, 3` / `load 2` —, remote loads by their number among remote loads from 1 — `remote 2` —, relays from 1 — `relay 1` —, joined with `, `; words from `t('tl.load')`, `t('tl.loads')`, `t('tl.remote')`, `t('tl.relay')`)

- [ ] **Step 1: Write the failing tests** (`periodMin = 480`)

```js
test('boostWindow', () => {
  assert.deepStrictEqual(M.boostWindow({ from: 'start', at: 120, for: 120 }, 480), { start: 120, end: 240, open: false });
  assert.deepStrictEqual(M.boostWindow({ from: 'end', at: 180, for: 90 }, 480), { start: 300, end: 390, open: false });
  assert.deepStrictEqual(M.boostWindow({ from: 'end', at: 120, for: null }, 480), { start: 360, end: 480, open: true });
  assert.strictEqual(M.boostWindow({ from: 'start', at: null, for: 60 }, 480), null);
  assert.strictEqual(M.boostFits({ from: 'start', at: 420, for: 120 }, 480), false); // ends after
  assert.strictEqual(M.boostFits({ from: 'start', at: 300, for: 60 }, 240), false);  // period shortened to 4 h
  assert.strictEqual(M.boostFits({ from: 'end', at: 480, for: null }, 480), true);
});
test('snap', () => { assert.strictEqual(TL.snap(7), 0); assert.strictEqual(TL.snap(8), 15); assert.strictEqual(TL.snap(27), 30); });
test('move keeps the anchor and stays inside', () => {
  assert.deepStrictEqual(TL.move({ from: 'start', at: 7, for: 60 }, 20, 480), { at: 30, for: 60 });
  assert.deepStrictEqual(TL.move({ from: 'start', at: 400, for: 60 }, 60, 480), { at: 420, for: 60 });   // stops at the end
  assert.deepStrictEqual(TL.move({ from: 'start', at: 30, for: 60 }, -90, 480), { at: 0, for: 60 });     // stops at the start
  assert.deepStrictEqual(TL.move({ from: 'end', at: 180, for: 120 }, 60, 480), { at: 120, for: 120 });  // later = closer to the end
  assert.deepStrictEqual(TL.move({ from: 'end', at: 120, for: null }, 200, 480), { at: 15, for: null }); // open: start <= period - 15
});
test('resize moves the free edge', () => {
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 120 }, 45, 480), { at: 120, for: 165 });
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 120 }, 400, 480), { at: 120, for: 360 }); // up to the end
  assert.deepStrictEqual(TL.resize({ from: 'start', at: 120, for: 30 }, -60, 480), { at: 120, for: 15 });   // 15 min minimum
  assert.deepStrictEqual(TL.resize({ from: 'end', at: 180, for: 120 }, -60, 480), { at: 240, for: 180 });   // start earlier, end stays
  assert.deepStrictEqual(TL.resize({ from: 'end', at: 120, for: null }, 30, 480), { at: 120, for: null });  // open: unchanged
});
test('labels', () => {
  const t = (k) => ({ 'tl.start': 'start', 'tl.end': 'end', 'tl.load': 'load', 'tl.loads': 'loads', 'tl.remote': 'remote', 'tl.relay': 'relay', 'targets.ALL_LOADS': 'all loads' })[k];
  assert.deepStrictEqual(TL.axisLabels(4, t), ['start', '+1h', '+2h', '+3h', 'end']);
  const m = M.defaults(); m.loads = [{ type: 'local', pin: 5 }, { type: 'remote', unit: 0 }, { type: 'local', pin: 7 }, { type: 'remote', unit: 1 }];
  assert.strictEqual(TL.targetsLabel(m, [{ load: 0 }, { load: 2 }], t), 'loads 1, 3');
  assert.strictEqual(TL.targetsLabel(m, [{ load: 3 }, { relay: 0 }], t), 'remote 2, relay 1');
  assert.strictEqual(TL.targetsLabel(m, 'ALL_LOADS', t), 'all loads');
  assert.strictEqual(TL.toPercent(120, 480), 25);
});
```

(Check the shape of a remote load in `model.js` and adapt the fixture, not the expectations.)

- [ ] **Step 2: Run, expect failures** (`boostWindow` / `PVRTimeline` missing).
- [ ] **Step 3: Implement** the interfaces above; `validate.js`'s boost window check calls `M.boostFits` (behaviour unchanged: its existing tests must pass as they are).
- [ ] **Step 4: Run all configurator tests, expect them to pass.**
- [ ] **Step 5: Commit** `feat: the geometry of the boost timeline`

---

### Task 2: The timeline on the page

**Files:**
- Modify: `configurator/index.html` (`<script src="timeline.js"></script>` after `model.js`)
- Modify: `configurator/app.js` (`dualTariff()` ~:657, `boostEditor()` ~:620)
- Modify: `configurator/style.css`
- Modify: `configurator/i18n.js` (en/fr: `tl.start`, `tl.end`, `tl.load`, `tl.loads`, `tl.remote`, `tl.relay`, `tl.help`)
- Modify: `configurator/README.md` (`timeline.js` in the file table)

**Interfaces:**
- Consumes: Task 1 (`M.boostWindow`, `M.boostFits`, `PVRTimeline.*`).
- Produces: `function boostTimeline(d)` in `app.js`, returning the timeline element; boost cards get `id="boost-<i>"` and a `selected` class when chosen.

- [ ] **Step 1: Mount.** In `dualTariff()`, when there is at least one boost, insert `boostTimeline(d)` before the cards: the axis (`axisLabels(d.offPeakHours, T)`), then one lane per boost: label `${T('boost')} ${i + 1}`, a bar at `left = toPercent(start)`, `width = toPercent(end - start)`, text `targetsLabel(m, b.targets, T)` (title attribute: the same plus the times), class `open` when open (dashed right end), class `bad` when `!boostFits` (red); no bar when `boostWindow` is `null`. A one-line `help('tl.help')` under it ("Drag a bar to move it, its end to change its duration; the fields below do the same with the keyboard." / fr equivalent).
- [ ] **Step 2: Drag.** Pointer Events on the bar (`setPointerCapture`): on the free edge (a 8 px handle: right edge for `from: 'start'` with `for`, left edge for `from: 'end'` with `for`, none for open bars) → `resize`; on the body → `move`. `deltaMin = round((clientX - startX) / laneWidth * period)`, applied to the boost as it was at `pointerdown`. While dragging, write the result into the boost and update only the bar's `left` / `width` and its title (no full re-render, which would drop the captured element); on `pointerup`, call `changed()` (fields, problems and outputs refresh). A press without movement (under 3 px) is a click: it selects the boost.
- [ ] **Step 3: Select.** Clicking a bar sets the selected boost (a module variable in `app.js`, not saved in the model), re-renders, and scrolls `#boost-<i>` into view (`block: 'nearest'`); the card gets the `selected` class (a visible outline) and the bar too.
- [ ] **Step 4: Style** in `style.css`: lane height ~26 px, bars with rounded corners, one colour per lane cycling over 4, `.open` dashed right border, `.bad` red, `.selected` outline, handles with `cursor: ew-resize`, body `cursor: grab`; `touch-action: none` on bars so touch drags do not scroll.
- [ ] **Step 5: Tests and page check.** `node --test configurator/test/*.test.js` passes (the generate byte-for-byte test included). Headless Chrome from Git Bash on `file:///D:/Git/IoT/pvr-wt/boosts/configurator/index.html` (find `chrome.exe` under `/c/Program Files/Google/Chrome/Application/` or use Edge), with the `relays-dualtariff-temp` preset loaded the way the page loads a saved JSON, or by setting `localStorage` through a small test page in the scratch directory: `--screenshot` at 1200×1600 shows the lanes where the fields say; repeat with `offPeakHours` 4 (red bars, `err.boostWindow`) and in French. No console error (`--enable-logging=stderr --v=0`, or `--dump-dom` of a page that records `window.onerror`).
- [ ] **Step 6: Commit** `feat: drag the scheduled boosts on a timeline`
