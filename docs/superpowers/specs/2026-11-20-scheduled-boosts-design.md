# Scheduled boosts: a clear way to say what runs when during off-peak

Status: design, awaiting approval. Built on top of #222 (dual-tariff window tests), which it replaces in part.

## Intent

With a dual tariff, the router can force loads at full power during the off-peak period (a "boost"), so the water is hot by morning even without sun. Today this is configured in `config.h` with a positional array:

```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } };
```

One `{ offset, duration }` pair per local load, by position. It is hard to read and easy to get wrong:

- a number from -24 to 24 is hours, beyond that minutes: `24` is 24 hours, `25` is 25 minutes;
- a negative offset counts back from the end of the period;
- `UINT16_MAX` means "until the end";
- a load left out of the array is boosted **the whole night** (`{ 0, UINT16_MAX }`), and `{ 0, 0 }` means no boost;
- only local loads can be boosted, by position, while remote loads (and relays) now exist and the override pins (`overridePins`) can already target all of them with `LOAD()`, `REMOTE_LOAD()`, `RELAY()`, `ALL_LOADS()`...

The user wants something "crystal clear", configured "in a common way" with `overridePins`. A user reading the config should know, without the README, which load runs when.

## Goals

- A config that reads as a schedule: when, for how long, which targets.
- The same target helpers as `overridePins`: local loads, remote loads, relays, groups.
- Explicit units: `HOURS()` / `MINUTES()`. No ±24 rule, no `UINT16_MAX`.
- Anything not listed is never boosted.
- A load may be in several boosts (e.g. 2 h at the start and 1 h before the end).
- Wrong configs rejected at compile time, with a message naming the problem, as `validation.h` does today.
- The configurator (web tool in `configurator/`) produces the new form, and checks the same rules.

## Non-goals

- Per-boost temperature thresholds: the temperature condition stays global (`iTemperatureThreshold`, as today).
- Changing how loads are numbered: `LOAD(0)` is the first load, as in `overridePins` today. The user said: "leave the 0-index for the moment".
- Boosts outside the off-peak period, or tied to a clock time: everything is relative to the off-peak start/end, as today.
- Changing the ISR: the forcing runs in the 1 Hz task in `main.cpp`, and stays there.

## Anti-goals

- **No backward compatibility.** `rg_ForceLoad`, `pairForceLoad` and `rg_OffsetForce` go. The user: "I don't care about old stuff, that'd make things more complex." No adapter, no deprecated alias, and the configurator does not read the old `force` field.
- No RAM growth. An ATmega328P has 2 KB, and today's `rg_OffsetForce` already sits in `.data`.
- No new magic numbers in the config for the user to learn.

## Constraints

- ATmega328P, avr-gcc 7.3, C++17, LTO. Everything resolved at compile time, as `OverridePins` does.
- The window maths (Arduino-free, in `utils_dualtariff.h`) stays testable on the host (`pio test -e native`, run through WSL).
- `-fpermissive` is no longer in the build (#222): overflows in constant expressions are errors.
- The off-peak period is at most 12 h (`validation.h`). 12 h is 43 200 s, so seconds fit in 16 bits; windows in ms need 32 bits.
- The forcing result is ORed with the override pins in `getOverrideBitmask()` (`main.cpp`), which hands local and remote masks to the load/relay logic. Relays already react to their pin's bit in the local mask (`RelayEngine::proceed_relays`), so boosting a relay needs no new plumbing.

## Approaches

**A. A list of boosts, keyed by a time window (chosen).** Same shape as `overridePins`, with a window in place of the pin:

```cpp
inline constexpr ScheduledBoosts scheduledBoosts{ {
  { FIRST(HOURS(2)), { LOAD(0), LOAD(2) } },                // the first 2 h
  { LAST(HOURS(2)), { REMOTE_LOAD(1) } },                   // the last 2 h
  { FROM_START(HOURS(2)), FOR(HOURS(2)), { LOAD(1) } },     // from 2 h after the start, for 2 h
  { BEFORE_END(HOURS(3)), FOR(MINUTES(90)), { RELAY(0) } }, // from 3 h before the end, for 1 h 30
} };
```

Trades off: replaces every config that has a boost, and the configurator's model. Gains: reads as a schedule, any target, several windows per load, one syntax for both lists.

**B. Keep the per-load array, with clearer helpers** (`{ BEFORE_END(HOURS(3)), FOR(HOURS(2)) }` per position). Smaller change, but still positional, local loads only, one window per load, and the "left out = all night" default stays. Does not give the common way the user asked for.

**C. A + keep `rg_ForceLoad` working through an adapter.** Rejected by the user: complexity for configs nobody needs to keep.

## Decided: syntax and meaning

These were decided with the user, or proposed and accepted:

| Form | Meaning |
|------|---------|
| `FIRST(d)` | from the start of the off-peak period, for `d` |
| `LAST(d)` | the last `d` of the period, until its end |
| `FROM_START(t), FOR(d)` | from `t` after the start, for `d` |
| `BEFORE_END(t), FOR(d)` | from `t` before the end, for `d` |
| `FROM_START(t)` / `BEFORE_END(t)` without `FOR` | from there until the end of the period |
| `HOURS(n)` / `MINUTES(n)` | the only way to write a time; no bare numbers |

- Targets: a `PinList`, written with the `overridePins` helpers (`LOAD()`, `REMOTE_LOAD()`, `RELAY()`, `ALL_LOADS()`, `ALL_RELAYS()`, `ALL_LOADS_AND_RELAYS()`...). Counting from 0, as today.
- Not listed: never boosted. An empty list means no boost at all.
- Several boosts may name the same target; their windows add up (OR).
- A window includes its start and excludes its end (as `DualTariff::inWindow` today).
- The end of the off-peak period stops every boost, whatever its window. That end is the off-peak pin going back to peak, not `ul_OFF_PEAK_DURATION`, as today (see call 6).
- The temperature condition (at or below `iTemperatureThreshold`) applies to all boosts, as today.

## Decided: how it works

- **Compile time:** each boost resolves, with `ul_OFF_PEAK_DURATION`, to `{ start ms, end ms, local mask, remote mask }`. The window functions are Arduino-free and `constexpr` in `utils_dualtariff.h` (they replace `windowStart`/`windowEnd`/`offsetInMinutes`/`durationInMinutes`); the masks come from `PinList::toLocalBitmask()` / `toRemoteBitmask()`, as in `OverridePins`.
- **Run time:** `getDualTariffForcingBitmask()` loops over the resolved boosts and ORs the masks of the open windows, if the temperature condition holds. It no longer goes through `physicalLoadPin` / `overridePinOf`.
- **Checks (`validation.h`):**
  - each window starts within the off-peak period (the rule #222 added, now on the new form): `LAST(10h)` or `BEFORE_END(9h)` with 8 h are rejected;
  - a window may end after the nominal period (`FIRST(10h)` with 8 h boosts all night): the pin stops it;
  - `FOR(0)` and a zero `FIRST`/`LAST` are rejected (a boost that never runs);
  - targets are valid: the same checks as `overridePins` (load and remote-load indexes in range, relays that exist...), factored so both lists share them.
- **Printing:** `printDualTariffConfiguration()` (called from `utils.h`) prints one line per boost: its window (start and end, in hours from the off-peak start) and its targets.

## The rest of the change

- `config.h`: `rg_ForceLoad` replaced by `scheduledBoosts`, next to `overridePins`, with a commented example like the one above.
- Native tests (`test/native/test_dual_tariff`): rewritten for the new forms. Each form, start/end edges, cut at the end of the period, rejected windows, the 12 h period, `millis()` wraparound (kept from #222), masks from the targets, overlapping boosts.
- Configurator: new model (a list of boosts: kind, time, duration, targets), an editor reusing `targetsEditor` from the override pins, generator, the same checks, en/fr strings, the three presets that have a boost, tests.
- Docs: Readme.md / Readme.en.md (the dual-tariff sections), CHANGELOG (breaking change: how to rewrite an old `rg_ForceLoad`), `docs/architecture.md`, the test READMEs, `CLAUDE.md`.

## Calls made (for the user to check)

1. **The shipped default** (decided by the user). Today's `config.h` has 3 loads and boosts load #1 from 3 h before the end for 2 h; by the "left out" default, loads #2 and #3 are boosted all night. The new default is only `{ BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(0) } }`: loads #2 and #3 are no longer boosted. The same goes for the configurator default and the presets (`relays-dualtariff-temp.json` loses its third load's all-night boost). The CHANGELOG says so, and shows how to keep a whole-night boost (`FIRST(HOURS(12))` or `FROM_START(MINUTES(0))`).
2. **Relays can be boosted.** It costs nothing: the override mask already drives them.
3. **The temperature condition applies to relays too**, one rule for every target.
4. **`FROM_START`/`BEFORE_END` without `FOR` last until the end**, like `LAST`.
5. **Resolved boosts live in flash (PROGMEM) if that keeps RAM flat or lower**; `.data` is acceptable only if RAM does not grow against today's default config.
6. **No cut at the nominal end.** `ul_OFF_PEAK_DURATION` only places the windows counted back from the end. `LAST`, and `FROM_START`/`BEFORE_END` without `FOR`, resolve to an open end (`UINT32_MAX`), as `UINT16_MAX` does today: if the meter keeps off-peak past the configured duration, they keep running until the pin says peak. A window with `FOR` ends at its start plus `FOR`, or earlier if the pin says peak first.

## Left to the builder

- The C++ types behind `FIRST`, `FOR`, `HOURS`... (distinct types, so a time cannot be passed where a start is expected and a mistake fails to compile), and whether they are functions or macros: follow how `LOAD()` is written in `utils_override_helpers.h`.
- How `ScheduledBoosts` is built (deduction guide like `OverridePins`, storage layout, PROGMEM access).
- The wording of the compile-time messages, en/fr strings and README prose.
- Whether the target checks move to a shared helper in `validation.h` or `utils_override.h`.
- Splitting the work into commits, and whether the PR goes before or after #222 merges (it must contain #222's changes either way).
