# Dual Tariff Tests

Tests for the forcing windows of the dual tariff (`utils_dualtariff.h`). Each load has a pair `{ offset, duration }` in `rg_ForceLoad`: hours from -24 to 24, minutes beyond; a negative offset counts back from the end of the off-peak period; a duration of `UINT16_MAX` lasts until the end. The firmware computes each window at compile time (`rg_OffsetForce`, `dualtariff.h`), in ms from the start of the off-peak period, and forces the load while `millis()` minus that start is in it.

## Tests

| Test | Description |
|------|-------------|
| `test_positive_offset_*` / `test_negative_offset_*` / `test_zero_offset_*` | Offsets from the start, back from the end, at the start |
| `test_offset_and_duration_beyond_24_are_minutes` / `test_negative_offset_in_minutes` | Minutes beyond +-24 |
| `test_24_is_hours_25_is_minutes` | The trap: 24 is 24 hours, 25 is 25 minutes, for the offset and the duration |
| `test_the_units_of_offset_and_duration_are_independent` | An offset in minutes with a duration in hours, and the other way round |
| `test_uint16_max_lasts_until_the_end` | `UINT16_MAX`, or the one-value constructor: no end |
| `test_a_load_left_out_of_rg_ForceLoad_is_forced_the_whole_period` | A default pair is `{ 0, UINT16_MAX }`: the whole off-peak period |
| `test_zero_zero_is_no_boost` | `{ 0, 0 }` never forces |
| `test_negative_offsets_with_10_to_12_hours` / `test_12_hours_whole_range` | The long periods, in hours and in minutes |
| `test_windows_are_computed_at_compile_time` | The window functions are `constexpr` |
| `test_the_window_includes_its_start_and_excludes_its_end` | Start included, end excluded |
| `test_until_the_end_never_ends_by_itself` | Only the end of the off-peak period stops it |
| `test_elapsed_time_across_the_millis_wraparound` | An off-peak period across the wraparound of `millis()` (49.7 days) |
| `test_a_window_starting_after_the_period_never_opens` / `test_a_window_starting_before_the_period_never_opens` | `{ 10, 20 }` or `{ -9, 1 }` with 8 hours: rejected at compile time (`validation.h`) |
| `test_windows_within_the_period_are_accepted` / `test_no_boost_is_accepted_whatever_its_offset` | What the check lets through |

## Bug 5 (#214)

`OffPeakDuration * 3600` was computed in 16-bit `int`, which overflowed from 10 hours. The wrapped value happened to be right once stored in a `uint16_t`, so no test of the windows can see it, and on the host `int` has 32 bits anyway. The compiler flags it ("overflow in constant expression"), but the Arduino core adds `-fpermissive`, which turned the error into a warning: `platformio.ini` now removes that flag.

## Running

```bash
pio test -e native -f "*test_dual_tariff*"
```
