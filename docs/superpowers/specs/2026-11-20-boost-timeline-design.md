# Configurator: a timeline to edit the scheduled boosts

Status: approved (short design). On top of the scheduled boosts (#223, `2026-11-20-scheduled-boosts-design.md`).

## Intent

The user wants a graphical way to set up each scheduled boost in the configurator's dual-tariff section, while keeping the text fields so a user without a mouse can still enter the values. Seeing the whole night at once shows what the fields can't: overlaps, gaps, whether the last hours are covered.

## Goals

- One timeline for the whole off-peak period, one row ("lane") per boost, the boost's targets written on its bar (layout A of the mockups, chosen by the user).
- Drag a bar to move it; drag its free edge to change the duration.
- Text fields unchanged, kept in sync both ways, live.

## Non-goals

- Clock times on the axis (an optional "off-peak starts at" field, for the labels only). Waiting on a colleague of the user; a separate, small change if wanted. The axis reads "start, +1h, … end".
- Keyboard control of the bars: the text fields are the keyboard way.
- Creating a boost by dragging on an empty part of the timeline (the "add" button stays the way).

## Anti-goals

- No dependency, no build step (the configurator's rule: plain scripts, also loaded by Node for the tests).
- The mouse must not be able to build a window the firmware rejects.

## Approaches

- **HTML bars + Pointer Events (chosen):** absolutely positioned elements in a lane, moved with `pointerdown/move/up` (mouse, touch and pen alike), styled by `style.css`, clickable like the rest of the page.
- Canvas: own drawing and hit-testing, loses the page's styles and click-to-select. Rejected.
- SVG: works, adds nothing for flat bars. Rejected.
- A chart library: breaks the no-dependency rule. Rejected.

## Decided

- **Layout:** in the dual-tariff section, above the list of boost cards: an axis ("start", "+1h"…"+(n-1)h", "end", from `offPeakHours`), then one lane per boost labelled "Boost N" (same numbering as the cards), the bar showing its targets in short form (as in the generated comments: "loads 1, 3", "remote 2", "relay 1", or the group name).
- **Moving:** drag the bar body. The boost keeps its anchor (`from: 'start'` stays counted from the start, `'end'` from the end): only `at` changes, so the generated form (`FIRST` / `FROM_START` / `LAST` / `BEFORE_END`) keeps its kind, as the generator already derives it.
- **Resizing:** drag the free edge: the right edge of a boost counted from the start, the left edge of one counted from the end (for a `before the end` boost with `FOR`, its start moves and its end stays: `at` and `for` change together). Boosts without `FOR` (`LAST`, `FROM_START` / `BEFORE_END` alone) are drawn open-ended (dashed end, up to "end") and can only be moved.
- **Snap:** 15 minutes while dragging, and in the minutes fields too (step 15, a typed value snapped to the nearest quarter, 0 to 45), as the user asked after trying the page.
- **Limits while dragging:** the window stays inside the off-peak period (the rule of #223: start within [0, period), start + `for` ≤ period); a duration never goes below 15 minutes by dragging.
- **Selection:** clicking a bar highlights its boost card and scrolls it into view.
- **Sync:** dragging updates the model and the fields live; editing a field redraws the bar. Changing the off-peak length, adding or removing a boost redraws the timeline. A boost that does not fit (e.g. after shortening the period) is drawn in red, next to the existing `err.boostWindow`.
- **Strings:** en/fr in `i18n.js` (French: U+00A0 before `:` etc.).
- **Where the code goes:** a new `configurator/timeline.js`, loaded like the other scripts. Its geometry is pure functions, tested with `node --test`: minutes ↔ position, snap, clamp to the period, drag (move / resize) → new `{ at, for }` keeping the anchor. `app.js` only mounts it.

## Left to the builder

The exact look (colours per lane, heights), how the pointer capture and the re-render of `app.js` fit together, the module shape of `timeline.js` (follow how `model.js` / `validate.js` export for Node), and a headless-Chrome check that the page loads and a bar renders where its fields say.
