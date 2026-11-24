# Forced Outputs Tests

Tests for what is forced ON once per second (`override_masks.h`): the targets of each override pin held LOW (`Forcing::pinForcing()`), and the targets of each scheduled boost whose window is open (`Forcing::boostForcing()`), joined with `|`. `main.cpp` reads `millis()`, the pins and the boost table in flash, and calls these functions.

The pins and boosts are built with the real `OverrideTable` and `BoostSchedule`, and the boosts resolved with `DualTariff::resolve()`, as `dualtariff.h` does at compile time. The test configuration: override pins D3 (D5, remote load 1), D4 (D5, D6) and D12 (remote load 0); over 8 hours of off-peak, boosts `FIRST(HOURS(2))` (D5), `FROM_START(HOURS(1)), FOR(HOURS(2))` (D6, remote load 2) and `LAST(HOURS(3))` (D7); a threshold of 60 °C.

## Tests

| Test | Description |
|------|-------------|
| `test_no_active_pin_forces_nothing` | No pin LOW: nothing forced |
| `test_an_active_pin_forces_its_targets` | A pin LOW forces its local targets |
| `test_a_pin_forces_its_remote_targets` | Remote targets go to the remote mask, local ones to the local mask |
| `test_only_the_override_pins_count` | Other pins LOW force nothing |
| `test_several_active_pins_add_up` | Pins sharing a target: the union |
| `test_each_pin_is_read_once` | Every override pin is read, once |
| `test_boost_window_opens_at_its_start` | Closed 1 ms before its start, open at it |
| `test_boost_window_closes_at_its_end` | Open 1 ms before its end, closed at it |
| `test_first_boost_is_open_from_the_start` | `FIRST()` is open at 0 |
| `test_no_boost_between_the_windows` | Nothing between two windows |
| `test_last_boost_lasts_until_the_meter_ends_off_peak` | `LAST()` stays open past the nominal end, until the meter switches to peak |
| `test_boost_across_the_millis_wraparound` | Elapsed time across the wraparound of `millis()` |
| `test_no_boost_outside_off_peak` | Not in off-peak: no boost, even in a window |
| `test_boost_at_the_threshold` | At the threshold: the boost runs |
| `test_no_boost_above_the_threshold` | 0.01 °C above: no boost |
| `test_boost_below_the_threshold` | Below, frost included: the boost runs |
| `test_a_dead_probe_lets_the_boost_run` | `DEVICE_DISCONNECTED_RAW` counts as cold: the heater's thermostat stays the safety limit |
| `test_an_out_of_range_reading_stops_the_boost` | `OUTOFRANGE_TEMPERATURE` (302 °C) is above any threshold |
| `test_overlapping_boosts_add_up` | Two open windows: the union, local and remote |
| `test_only_the_boosts_of_the_count_are_read` | Boosts `0` to `count - 1` are read, no more |
| `test_no_boost_at_all` | `count` 0: nothing read, nothing forced |
| `test_the_table_is_not_read_outside_off_peak_or_when_too_hot` | The table is only read when a boost may run |
| `test_operator_or_joins_both_masks` | `\|` joins the local and the remote masks |
| `test_pins_and_boosts_add_up` | Pins and boosts together: the union |
| `test_pins_force_outside_off_peak_and_when_too_hot` | The pins force whatever the tariff and the temperature |

## Running

```bash
pio test -e native -f "*test_override_masks*"
```
