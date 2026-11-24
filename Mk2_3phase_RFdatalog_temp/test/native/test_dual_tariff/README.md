# Dual Tariff Tests

Tests for the scheduled boosts of the dual tariff (`utils_dualtariff.h`: `BoostSchedule`, and the `ScheduledBoosts` of config.h it is built from). A boost is a window (`FIRST`, `LAST`, `FROM_START`, `BEFORE_END`, with `FOR`) and a list of targets, written with `HOURS()` / `MINUTES()`. The firmware resolves each boost at compile time, with `ul_OFF_PEAK_DURATION`, to `{ start, end, local mask, remote mask }`, in ms from the start of the off-peak period, and forces the targets while `millis()` minus that start is in the window. A boost without end (`LAST`, or no `FOR`) lasts until the meter switches back to peak.

## Tests

| Test | Description |
|------|-------------|
| `test_hours_and_minutes` | `HOURS(n)` and `MINUTES(n)` give minutes |
| `test_first_starts_at_the_start_and_lasts_its_duration` | `FIRST(d)`: from 0 to `d` |
| `test_last_starts_back_from_the_end_and_has_no_end` | `LAST(d)`: starts `d` before the end of the period, no end of its own |
| `test_from_start_for` / `test_before_end_for` | `FROM_START(t), FOR(d)` and `BEFORE_END(t), FOR(d)` |
| `test_until_the_end_has_no_end` | A start without `FOR`: until the meter switches back to peak |
| `test_twelve_hours` | The longest period (12 h) fits |
| `test_starts_within_off_peak` | What `validation.h` accepts: a start inside the period, even if `FOR` ends after it; a start before or at the end is rejected |
| `test_a_boost_of_zero_minutes_has_no_length` | `FOR(0)` and a zero `FIRST` / `LAST` are rejected |
| `test_boost_windows_are_computed_at_compile_time` | The window functions are `constexpr` |
| `test_boost_window_includes_its_start_and_excludes_its_end` | Start included, end excluded; an open end lasts |
| `test_boost_window_across_the_millis_wraparound` | An off-peak period across the wraparound of `millis()` (49.7 days) |
| `test_targets_become_masks` | Local pins and remote loads of a boost become the local and remote masks |
| `test_without_for_lasts_until_the_end` | `resolve()` of a start without `FOR` |
| `test_resolve` | A boost resolves to `{ start, end, local, remote }` at compile time |
| `test_a_target_may_be_in_several_boosts` | The same target in two boosts |
| `test_an_empty_list` | `BoostSchedule{}`: no boost |
| `test_boosts_from_the_settings` | The `ScheduledBoosts` of config.h, with `LOAD()` and `RELAY()`, turned into masks by `TargetMap` as in config_derived.h |
| `test_no_boost_in_the_settings` | `ScheduledBoosts{}` in config.h: no boost either |
| `test_a_start_before_the_period_wraps_to_a_large_value` | A start before the period wraps in `uint32_t` (what the compile-time check catches) |

## Bug 5 (#214)

`OffPeakDuration * 3600` was computed in 16-bit `int`, which overflowed from 10 hours. The wrapped value happened to be right once stored in a `uint16_t`, so no test of the windows can see it, and on the host `int` has 32 bits anyway. The compiler flags it ("overflow in constant expression"), but the Arduino core adds `-fpermissive`, which turned the error into a warning: `platformio.ini` now removes that flag.

## Running

```bash
pio test -e native -f "*test_dual_tariff*"
```
