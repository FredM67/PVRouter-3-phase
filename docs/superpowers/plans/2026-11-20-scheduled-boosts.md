# Scheduled Boosts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the positional `rg_ForceLoad` with a list of scheduled boosts (`FIRST` / `LAST` / `FROM_START` / `BEFORE_END` + `FOR`, `HOURS()` / `MINUTES()`), targeting loads, remote loads and relays with the `overridePins` helpers, in the firmware, the configurator and the docs.

**Architecture:** The types and the window maths live in `utils_dualtariff.h` (Arduino-free, host-tested). `ScheduledBoosts` mirrors `OverridePins` (`utils_override.h`): entries keyed by a window instead of a pin, with targets turned into local/remote masks. `dualtariff.h` resolves every boost at compile time to `{ start ms, end ms, local mask, remote mask }`, in a PROGMEM table. `getDualTariffForcingBitmask()` (main.cpp) ORs the masks of the open windows.

**Tech Stack:** C++17 / avr-gcc 7.3 / PlatformIO (run in WSL), Unity native tests, plain JavaScript configurator with `node --test`.

**Spec:** `docs/superpowers/specs/2026-11-20-scheduled-boosts-design.md`

## Global Constraints

- Worktree `D:/Git/IoT/pvr-wt/boosts`, branch `feat/scheduled-boosts` (on top of #222). Never commit `Mk2_3phase_RFdatalog_temp/version.h` (builds rewrite it: `git checkout -- Mk2_3phase_RFdatalog_temp/version.h` after each build) nor `.vscode/extensions.json`.
- Commits end with `Co-Authored-By: Claude <noreply@anthropic.com>`.
- No backward compatibility: `rg_ForceLoad`, `pairForceLoad`, `rg_OffsetForce`, `offsetInMinutes`, `durationInMinutes` disappear; the configurator drops `force`.
- Not listed = never boosted. Shipped default: only `{ BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(0) } }`.
- Only the meter ends a boost: `LAST`, and `FROM_START`/`BEFORE_END` without `FOR`, end at `UINT32_MAX`. `ul_OFF_PEAK_DURATION` only places windows counted back from the end, and must match the meter.
- Start included, end excluded. The temperature condition (`<= iTemperatureThreshold`) applies to every boost, relays included.
- RAM must not grow against the #222 build of the same env. The resolved table goes in PROGMEM; `.data` only if PROGMEM fails AND RAM still does not grow (say so in the commit).
- No ISR change. `-fpermissive` stays removed.
- PlatformIO and node only run in WSL. Put multi-command runs in a script file and call `MSYS_NO_PATHCONV=1 wsl bash /mnt/d/...script.sh`; `$VARS` inside `wsl bash -lc '...'` from Git Bash vanish. pio: `~/.platformio/penv/bin/pio`; node: `~/.local/node/bin/node`.
- French strings: U+00A0 (no-break space) before `:`, `;`, `?`, `!`. Edit with `newline=''` in Python, or keep CRLF/LF as the file has it.

## Review Focus

1. **The meter goes back to peak in the middle of a boost.** Every boost must stop at once, `LAST` and no-`FOR` included. `getDualTariffForcingBitmask()` returns `{ 0, 0 }` when `!bOffPeak` before reading any window. Task 3 keeps that early return, and the reviewer checks it is first.
2. **The user lowers `NO_OF_DUMPLOADS` or turns relay diversion off, and a boost still names `LOAD(2)` or `RELAY(0)`.** That must fail to compile with the boost message, not force nothing silently. Task 3, step 9 compile-fail checks.
3. **`LAST(d)` with `d` equal to the period (start 0) is accepted, `d` one minute longer is rejected.** Task 1 test `test_starts_within_off_peak`.
4. **Off-peak runs past `ul_OFF_PEAK_DURATION`.** `LAST(HOURS(2))` keeps running (end `UINT32_MAX`); `FIRST(HOURS(2))` still stops at 2 h. Task 1 tests `test_last_*` / `test_first_*`.
5. **`DUAL_TARIFF` false with the empty list `ScheduledBoosts scheduledBoosts{};`** must compile, and so must a non-empty list (unused). Task 2 test `test_an_empty_list` and Task 3, step 8 (build the `sixty-hz-json` preset in Task 4).

---

### Task 1: Window types and maths

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/utils_dualtariff.h` (add the new types and functions next to the old ones; Task 3 deletes the old ones)
- Test: `Mk2_3phase_RFdatalog_temp/test/native/test_dual_tariff/test_main.cpp` (add the new tests; Task 3 deletes the old ones)

**Interfaces:**
- Produces (global, like `LOAD()`):
  - `struct Minutes { uint16_t value; };`
  - `constexpr Minutes HOURS(uint8_t n)` (`n * 60`), `constexpr Minutes MINUTES(uint16_t n)`
  - `struct BoostStart { bool fromEnd; uint16_t minutes; };`
  - `inline constexpr uint16_t UNTIL_THE_END{ UINT16_MAX };`
  - `struct BoostLength { uint16_t minutes; };` (`UNTIL_THE_END`: only the meter ends it)
  - `struct BoostWindow { BoostStart start; BoostLength length; };`
  - `constexpr BoostStart FROM_START(Minutes)`, `constexpr BoostStart BEFORE_END(Minutes)`
  - `constexpr BoostLength FOR(Minutes)`
  - `constexpr BoostWindow FIRST(Minutes d)` = `{ FROM_START(0), FOR(d) }`, `constexpr BoostWindow LAST(Minutes d)` = `{ BEFORE_END(d), UNTIL_THE_END }`
- Produces (namespace `DualTariff`):
  - `constexpr uint32_t windowStart(const BoostWindow&, uint8_t offPeakHours)`: ms from the off-peak start; `fromEnd`: `(offPeakHours * 60 - minutes) * 60000UL` (wraps when before the period; rejected by the check)
  - `constexpr uint32_t windowEnd(const BoostWindow&, uint8_t offPeakHours)`: `UINT32_MAX` for `UNTIL_THE_END`, else start + `length * 60000UL`
  - `constexpr bool inWindow(uint32_t elapsed, uint32_t start, uint32_t end)`: kept as is
  - `constexpr bool startsWithinOffPeak(const BoostWindow&, uint8_t offPeakHours)`: start in `[0, period)`, in minutes, signed arithmetic
  - `constexpr bool hasLength(const BoostWindow&)`: `length.minutes != 0`

The types are distinct so `FIRST(2)` or `FOR(FROM_START(...))` do not compile.

- [ ] **Step 1: Write the failing tests** (8 h period unless said; values in ms)

```cpp
void test_hours_and_minutes(void) {
  TEST_ASSERT_EQUAL_UINT16(120, HOURS(2).value);
  TEST_ASSERT_EQUAL_UINT16(720, HOURS(12).value);
  TEST_ASSERT_EQUAL_UINT16(90, MINUTES(90).value);
}
void test_first_starts_at_the_start_and_lasts_its_duration(void) {
  TEST_ASSERT_EQUAL_UINT32(0, DualTariff::windowStart(FIRST(HOURS(2)), 8));
  TEST_ASSERT_EQUAL_UINT32(7200000UL, DualTariff::windowEnd(FIRST(HOURS(2)), 8));
}
void test_last_starts_back_from_the_end_and_has_no_end(void) {
  TEST_ASSERT_EQUAL_UINT32(21600000UL, DualTariff::windowStart(LAST(HOURS(2)), 8));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::windowEnd(LAST(HOURS(2)), 8));
}
void test_from_start_for(void) {
  constexpr BoostWindow w{ FROM_START(HOURS(2)), FOR(HOURS(2)) };
  TEST_ASSERT_EQUAL_UINT32(7200000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(14400000UL, DualTariff::windowEnd(w, 8));
}
void test_before_end_for(void) {
  constexpr BoostWindow w{ BEFORE_END(HOURS(3)), FOR(MINUTES(90)) };
  TEST_ASSERT_EQUAL_UINT32(18000000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(23400000UL, DualTariff::windowEnd(w, 8));
}
void test_until_the_end_has_no_end(void) {
  constexpr BoostWindow w{ FROM_START(HOURS(1)), BoostLength{ UNTIL_THE_END } };
  TEST_ASSERT_EQUAL_UINT32(3600000UL, DualTariff::windowStart(w, 8));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::windowEnd(w, 8));
}
void test_twelve_hours(void) {
  TEST_ASSERT_EQUAL_UINT32(0, DualTariff::windowStart(LAST(HOURS(12)), 12));
  TEST_ASSERT_EQUAL_UINT32(43140000UL, DualTariff::windowStart(LAST(MINUTES(1)), 12));
  TEST_ASSERT_EQUAL_UINT32(43200000UL, DualTariff::windowEnd(FIRST(HOURS(12)), 12));
}
void test_starts_within_off_peak(void) {
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(FIRST(HOURS(10)), 8));   // ends after: the meter stops it
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(LAST(HOURS(8)), 8));     // starts at 0
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(LAST(MINUTES(481)), 8)); // 1 min before the start
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(LAST(MINUTES(0)), 8));   // at the end
  TEST_ASSERT_FALSE(DualTariff::startsWithinOffPeak(BoostWindow{ FROM_START(HOURS(8)), FOR(HOURS(1)) }, 8));
  TEST_ASSERT_TRUE(DualTariff::startsWithinOffPeak(BoostWindow{ FROM_START(MINUTES(479)), FOR(HOURS(1)) }, 8));
}
void test_a_boost_of_zero_minutes_has_no_length(void) {
  TEST_ASSERT_FALSE(DualTariff::hasLength(FIRST(MINUTES(0))));
  TEST_ASSERT_FALSE(DualTariff::hasLength(BoostWindow{ BEFORE_END(HOURS(2)), FOR(MINUTES(0)) }));
  TEST_ASSERT_TRUE(DualTariff::hasLength(LAST(HOURS(2))));
}
```

Plus `static_assert(DualTariff::windowEnd(FIRST(HOURS(2)), 8) == 7200000UL);` in `test_windows_are_computed_at_compile_time`. Keep #222's `inWindow` and `millis()` wraparound tests, rewritten on the new types.

- [ ] **Step 2: Run, expect a compile failure** (`HOURS` not declared)

Run: `MSYS_NO_PATHCONV=1 wsl bash -lc 'cd /mnt/d/Git/IoT/pvr-wt/boosts/Mk2_3phase_RFdatalog_temp && ~/.platformio/penv/bin/pio test -e native -f "*test_dual_tariff*"'`

- [ ] **Step 3: Implement the interfaces above in `utils_dualtariff.h`**, Doxygen comments as in the file today.

- [ ] **Step 4: Run, expect every `test_dual_tariff` test to pass**, old ones included.

- [ ] **Step 5: Commit** `feat: the window types of the scheduled boosts`

---

### Task 2: `ScheduledBoosts`

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/utils_dualtariff.h` (includes `utils_override.h` for `PinList`, which `test_utils_override` already builds on the host)
- Test: `Mk2_3phase_RFdatalog_temp/test/native/test_dual_tariff/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 types; `PinList<MaxPins>` (`toLocalBitmask()`, `toRemoteBitmask()`), `REMOTE_PIN_BASE`.
- Produces:
  - `template< uint8_t MaxPins = 16 > struct BoostEntry { BoostWindow window; PinList< MaxPins > targets; }` with constructors `(const BoostWindow&, const PinList&)`, `(const BoostStart&, const PinList&)` (length `UNTIL_THE_END`), `(const BoostStart&, const BoostLength&, const PinList&)`
  - `template< uint8_t N, uint8_t MaxPins = 16 > class ScheduledBoosts` built from `const BoostEntry< MaxPins > (&)[N]`, storing `{ BoostWindow, uint16_t local, uint8_t remote }` per entry, with `size()`, `getWindow(i)`, `getLocalBitmask(i)`, `getRemoteBitmask(i)` (the same accessor names as `OverridePins`, so the target checks of Task 3 take either)
  - partial specialisation `ScheduledBoosts< 0, MaxPins >` with a default constructor and `size() == 0`
  - deduction guides mirroring `OverridePins`, plus `ScheduledBoosts() -> ScheduledBoosts< 0 >`
  - `struct DualTariff::Boost { uint32_t start; uint32_t end; uint16_t local; uint8_t remote; };`
  - `template< uint8_t N, uint8_t MaxPins > constexpr DualTariff::Boost DualTariff::resolve(const ScheduledBoosts< N, MaxPins >&, uint8_t i, uint8_t offPeakHours)`

- [ ] **Step 1: Write the failing tests** (pins written raw: `LOAD()` needs `config.h`)

```cpp
constexpr ScheduledBoosts boosts{ {
  { FIRST(HOURS(2)), { 5, 7 } },
  { LAST(HOURS(1)), { REMOTE_PIN_BASE + 1 } },
  { FROM_START(HOURS(1)), { 5 } },                     // no FOR: until the end
  { BEFORE_END(HOURS(3)), FOR(MINUTES(90)), { 9 } },
} };
static_assert(boosts.size() == 4);

void test_targets_become_masks(void) {
  TEST_ASSERT_EQUAL_UINT16((1U << 5) | (1U << 7), boosts.getLocalBitmask(0));
  TEST_ASSERT_EQUAL_UINT8(0, boosts.getRemoteBitmask(0));
  TEST_ASSERT_EQUAL_UINT16(0, boosts.getLocalBitmask(1));
  TEST_ASSERT_EQUAL_UINT8(0b10, boosts.getRemoteBitmask(1));
}
void test_without_for_lasts_until_the_end(void) {
  TEST_ASSERT_EQUAL_UINT32(3600000UL, DualTariff::resolve(boosts, 2, 8).start);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, DualTariff::resolve(boosts, 2, 8).end);
}
void test_resolve(void) {
  constexpr auto b{ DualTariff::resolve(boosts, 3, 8) };
  static_assert(b.start == 18000000UL && b.end == 23400000UL && b.local == (1U << 9) && b.remote == 0);
}
void test_a_target_may_be_in_several_boosts(void) {
  TEST_ASSERT_TRUE(boosts.getLocalBitmask(0) & boosts.getLocalBitmask(2));  // pin 5 twice
}
void test_an_empty_list(void) {
  constexpr ScheduledBoosts none{};
  static_assert(none.size() == 0);
}
```

- [ ] **Step 2: Run, expect a compile failure** (same command as Task 1).
- [ ] **Step 3: Implement**, following `OverridePins` / `KeyIndexPair` (`utils_override.h:243-432`) for the template parameters and the deduction guides.
- [ ] **Step 4: Run, expect all `test_dual_tariff` tests to pass**, then the whole native suite: `... pio test -e native`.
- [ ] **Step 5: Commit** `feat: ScheduledBoosts, targets as for the override pins`

---

### Task 3: The firmware on scheduled boosts

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/config.h:183-186` (and the `#include "utils_dualtariff.h"` at :87 must reach `PinList`; `scheduledBoosts` goes after `overridePins`, i.e. after `utils_override_helpers.h` at :153)
- Modify: `Mk2_3phase_RFdatalog_temp/dualtariff.h` (whole file)
- Modify: `Mk2_3phase_RFdatalog_temp/main.cpp:60-128`
- Modify: `Mk2_3phase_RFdatalog_temp/validation.h:53-67, 81, 236-291`
- Modify: `Mk2_3phase_RFdatalog_temp/utils_dualtariff.h`, `test/native/test_dual_tariff/test_main.cpp` (delete the old format and its tests)

**Interfaces:**
- Consumes: Task 2 `ScheduledBoosts`, `DualTariff::resolve`, `DualTariff::Boost`, `DualTariff::inWindow`.
- Produces: `scheduledBoosts` (config.h); in `dualtariff.h`: `inline constexpr uint8_t NO_OF_BOOSTS{ scheduledBoosts.size() };`, the PROGMEM table, `inline DualTariff::Boost readBoost(uint8_t i)` (`memcpy_P`), `printDualTariffConfiguration()`; in `validation.h`: `template< typename List > constexpr bool targetsAreOutputs(const List&)` and `template< typename List > constexpr bool targetsAreRemoteLoads(const List&)`.

- [ ] **Step 1: Record the baseline sizes.** Script in `/mnt/d/Git/IoT/pvr-wt/tariff/Mk2_3phase_RFdatalog_temp`: `pio run -e basic` and `pio run -e rf`; note the RAM and Flash lines; `git checkout -- version.h` there.

- [ ] **Step 2: `config.h`.** Replace `rg_ForceLoad` with (adapt only to clang-format):

```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 }; /**< Duration of the off-peak period in hours: must match the meter (it places the boosts counted back from the end) */

// Scheduled boosts during the off-peak period (DUAL_TARIFF): when, and what they force.
// Targets as for the override pins; what is not listed is never boosted.
// Every boost stops when the meter switches back to peak.
//   FIRST(d)                the first d of the period
//   LAST(d)                 the last d of the period
//   FROM_START(t), FOR(d)   from t after the start, for d (without FOR: until the end)
//   BEFORE_END(t), FOR(d)   from t before the end, for d (without FOR: until the end)
//   t and d: HOURS(n) or MINUTES(n)
// inline constexpr ScheduledBoosts scheduledBoosts{ {
//     { FIRST(HOURS(2)), { LOAD(0), LOAD(2) } },    // loads #1 and #3, the first 2 hours
//     { LAST(MINUTES(90)), { RELAY(0) } } } };      // relay #1, the last 1 h 30
inline constexpr ScheduledBoosts scheduledBoosts{ { { BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(0) } } } }; /**< load #1, from 3 hours before the end, for 2 hours */
```

- [ ] **Step 3: `dualtariff.h`.** Delete `_rg_OffsetForce` / `rg_OffsetForce`. Build the table of `NO_OF_BOOSTS` `DualTariff::Boost` (size 1 if 0, never read) with `resolve(scheduledBoosts, i, ul_OFF_PEAK_DURATION)`, in PROGMEM. `printDualTariffConfiguration()`: the duration and threshold lines as today, then one line per boost: `Boost #k: from <h>h<mm> to <h>h<mm>` (or `until the end of off-peak`), counted from the off-peak start, then the targets: `D<pin>` per local bit, `remote load <n+1>` per remote bit. Integer maths only (no float in Print).

- [ ] **Step 4: `main.cpp`.** In `getDualTariffForcingBitmask()`: keep the `!bOffPeak` early return first; return `{ 0, 0 }` early if `currentTemperature_x100 > iTemperatureThreshold_x100`; then loop `i` over `NO_OF_BOOSTS`, `b = readBoost(i)`, OR `b.local` / `b.remote` when `inWindow(elapsed, b.start, b.end)`. No `physicalLoadPin` / `overridePinOf` any more. Update the Doxygen block (:60-80): "Each scheduled boost has its own window (scheduledBoosts)".

- [ ] **Step 5: `validation.h`.**
  - Replace `check_force_windows()` with `check_boost_windows()`: every window `startsWithinOffPeak(..., ul_OFF_PEAK_DURATION) && hasLength(...)`. Message: `"******** A scheduled boost starts outside the off-peak period, or lasts 0 minutes: it would never run. Please check scheduledBoosts in your config.h ! ********"`, under `!DUAL_TARIFF ||`.
  - Turn the bodies of `check_override_targets()` / `check_override_remote_targets()` into `targetsAreOutputs(list)` / `targetsAreRemoteLoads(list)`; keep both `check_override_*` (under `OVERRIDE_PIN_PRESENT`, same messages) and add, under `DUAL_TARIFF`:
    - `"******** A scheduled boost forces something that is neither a local load nor a relay (is relay diversion on?). Please check scheduledBoosts in your config.h ! ********"`
    - `"******** A scheduled boost forces a remote load that does not exist (REMOTE_LOAD() counts from 0). Please check scheduledBoosts in your config.h ! ********"`
  - Delete the `rg_ForceLoad` size check (:81).

- [ ] **Step 6: Delete the old format**: `pairForceLoad`, `offsetInMinutes`, `durationInMinutes`, the old overloads, and their tests. `grep -rn "rg_ForceLoad\|pairForceLoad\|rg_OffsetForce\|offsetInMinutes" Mk2_3phase_RFdatalog_temp --include=*.h --include=*.cpp` must print nothing.

- [ ] **Step 7: Native suite passes:** `... pio test -e native`.

- [ ] **Step 8: Builds pass, no new warning:** `pio run -e basic`, `-e basic_debug`, `-e rf`, `-e uno`, and once with `DUAL_TARIFF{ true }` + `dualTariffPin{ 3 }` temporarily (revert). Compare RAM and Flash of `basic` and `rf` with step 1: RAM must be equal or lower. Restore `version.h`.

- [ ] **Step 9: Compile-fail checks**, with `DUAL_TARIFF{ true }`, `dualTariffPin{ 3 }`, one temporary `scheduledBoosts` at a time; each must stop on its message, then revert:
  - `{ LAST(HOURS(9)), { LOAD(0) } }` → "starts outside the off-peak period"
  - `{ BEFORE_END(HOURS(3)), FOR(MINUTES(0)), { LOAD(0) } }` → same message
  - `{ FIRST(HOURS(2)), { RELAY(0) } }` with `RELAY_DIVERSION{ false }` → "neither a local load nor a relay" (if `RELAY()` itself fails first without relays, record that error instead)
  - `{ FIRST(HOURS(2)), { REMOTE_LOAD(3) } }` → "remote load that does not exist"
  - `{ FIRST(2), { LOAD(0) } }` → a type error (no bare numbers)

- [ ] **Step 10: Simulator still passes:** `make -C sim check` (script, WSL).

- [ ] **Step 11: Commit** `feat!: scheduled boosts replace rg_ForceLoad`, with the before/after RAM and Flash in the message.

---

### Task 4: The configurator

**Files:**
- Modify: `configurator/model.js:107-116, 302` (defaults, `resizeLoads`)
- Modify: `configurator/generate.js:92-96, 296-312`
- Modify: `configurator/validate.js:113-152`
- Modify: `configurator/app.js:597-660` (`dualTariff()`), reuse `targetsEditor` (:468)
- Modify: `configurator/i18n.js` (en ~:101, :256-258; fr ~:373, :527-529)
- Modify: `configurator/test/presets/mk2wifi-full.json`, `relays-dualtariff-temp.json`, `sixty-hz-json.json`
- Test: `configurator/test/generate.test.js`, `configurator/test/validate.test.js`

**Interfaces:**
- Consumes: the `config.h` text of Task 3, step 2 (the default model must give it back byte for byte, `@date` aside).
- Produces: `m.dualTariff.boosts: [{ from: 'start' | 'end', at: <minutes>, for: <minutes> | null, targets }]`, where `targets` is as for override pins (a group name or `[{ load: i } | { relay: i }]`). Default: `[{ from: 'end', at: 180, for: 120, targets: [{ load: 0 }] }]`.

Generator, per boost:

| model | emitted |
|---|---|
| `from: 'start', at: 0, for: d` | `FIRST(t(d))` |
| `from: 'end', for: null` | `LAST(t(at))` |
| `from: 'start', for: null` | `FROM_START(t(at))` |
| otherwise | `FROM_START(t(at)), FOR(t(d))` / `BEFORE_END(t(at)), FOR(t(d))` |

`t(m)`: `HOURS(m / 60)` when `m % 60 === 0`, else `MINUTES(m)`. Targets through `overrideTargets()`. An empty list: `inline constexpr ScheduledBoosts scheduledBoosts{};`. A comment line per boost saying what it forces, numbered from 1, as `overrideComments()` does, when a list names loads or relays one by one.

- [ ] **Step 1: Write the failing tests**
  - generate: the default model gives the shipped `config.h` (the existing byte-for-byte test); each row of the table above gives its form (`{ from: 'start', at: 0, for: 120 }` → `FIRST(HOURS(2))`, `{ from: 'end', at: 90, for: null }` → `LAST(MINUTES(90))`, `{ from: 'start', at: 60, for: null }` → `FROM_START(HOURS(1))`, `{ from: 'end', at: 180, for: 90 }` → `BEFORE_END(HOURS(3)), FOR(MINUTES(90))`); `boosts: []` → `ScheduledBoosts scheduledBoosts{};`; `targets: 'ALL_RELAYS'` → `ALL_RELAYS()`.
  - validate (replace the `err.forceWindow` cases, 8 h period, `enabled: true, pin: 3`): `{ from: 'end', at: 540, for: null }` → `err.boostWindow`; `{ from: 'start', at: 480, for: 60 }` → `err.boostWindow`; `{ from: 'start', at: 0, for: 0 }` → `err.boostWindow`; `{ from: 'end', at: 480, for: null }` → no error; `{ from: 'start', at: 0, for: 600 }` → no error; targets `[{ relay: 0 }]` with relays off → `err.boostNoRelay`; `[{ load: 5 }]` with 3 loads → `err.boostLoad`; `[]` → `err.boostTargets`.
  - normalize: an input with `dualTariff.force` and no `boosts` gives no `force` key and the default `boosts`.

- [ ] **Step 2: Run, expect failures:** `MSYS_NO_PATHCONV=1 wsl bash -lc 'cd /mnt/d/Git/IoT/pvr-wt/boosts && ~/.local/node/bin/node --test configurator/test/*.test.js'`

- [ ] **Step 3: Implement** model, generator, checks (factor the override target checks into one function both lists call, with the error keys as a parameter; `at` integer 0..720, `for` null or integer 0..65534), the page (a list of boosts with add/remove as for override pins; per boost: "after the start / before the end", a time in h + min, "until the end" or a duration in h + min, then `targetsEditor(boost)`), and the strings:
  - en `sec`/labels: "Scheduled boosts"; `'err.boostWindow': 'Scheduled boost {index} starts outside the off-peak period, or lasts 0 min: it would never run.'`; `err.boostTargets` / `err.boostLoad` / `err.boostRelay` / `err.boostNoRelay` worded as their `err.override*` twins, with "Scheduled boost {index}"
  - fr: "Marches forcées programmées"; `'err.boostWindow': 'La marche forcée programmée {index} démarre en dehors des heures creuses, ou dure 0 min : elle ne se déclencherait jamais.'` (U+00A0 before `:`), the others as their `err.override*` twins
  - delete `force`, `err.force*`, `untilEnd` if unused, and the `force` line of `resizeLoads`.

- [ ] **Step 4: Presets:** `mk2wifi-full.json` `force: [{-3,2},{-150,90}]` → `boosts: [{ from: 'end', at: 180, for: 120, targets: [{ load: 0 }] }, { from: 'end', at: 150, for: 90, targets: [{ load: 1 }] }]`; `relays-dualtariff-temp.json` `[{-3,2},{2,null}]` → `[{ from: 'end', at: 180, for: 120, targets: [{ load: 0 }] }, { from: 'start', at: 120, for: null, targets: [{ load: 1 }] }]` (its third load loses its all-night boost, as decided); `sixty-hz-json.json` → `boosts: []`.

- [ ] **Step 5: Run, expect all tests to pass**, then `bash configurator/test/compile-presets.sh` (WSL): every preset builds.

- [ ] **Step 6: Commit** `feat!: the configurator writes scheduled boosts`

---

### Task 5: Docs

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/Readme.md` and `Readme.en.md` (the dual-tariff sections, ~578-761 / ~573-756), `CHANGELOG.md`, `docs/architecture.md` (the dual-tariff paragraph, ~:93), `Mk2_3phase_RFdatalog_temp/test/native/test_dual_tariff/README.md`, `Mk2_3phase_RFdatalog_temp/test/README.md`, `CLAUDE.md` (wherever they name the old format)

- [ ] **Step 1: Readmes (fr and en, same content):** the forms table of the spec, the four-boost example of the spec, targets as for override pins, "not listed: never boosted", "every boost stops when the meter switches back to peak", "`ul_OFF_PEAK_DURATION` must match the meter: it places `LAST` / `BEFORE_END`", the compile-time messages, remove the ±24 rule, `UINT16_MAX` and the "left out" default.
- [ ] **Step 2: CHANGELOG**, breaking change: `rg_ForceLoad` → `scheduledBoosts`; how to rewrite: `{ -3, 2 }` → `{ BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(n) } }`, `{ 2 }` → `{ FROM_START(HOURS(2)), { LOAD(n) } }`, `{ -150, 90 }` → `{ BEFORE_END(MINUTES(150)), FOR(MINUTES(90)), { LOAD(n) } }`; a load left out used to boost all night: list it with `FIRST(HOURS(12))` to keep that; remote loads and relays can now be boosted.
- [ ] **Step 3: Test README:** the table of Tasks 1-2 tests; keep the "Bug 5 (#214)" section.
- [ ] **Step 4: Check:** `grep -rn "rg_ForceLoad\|pairForceLoad\|rg_OffsetForce" --include=*.md --include=*.h --include=*.cpp --include=*.js . | grep -v "CHANGELOG\|docs/superpowers"` prints nothing.
- [ ] **Step 5: Commit** `docs: scheduled boosts`
