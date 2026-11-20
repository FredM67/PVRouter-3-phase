# Relay Engine Tests

Tests for the decisions of the relay diversion (`relay_engine.h`): when each relay switches, from the sliding average of the grid power, the override bits and the diversion switch. `loop()` runs the engine once per second (`relays.inc_duration()`, then `relays.proceed_relays()`); `utils_relay.h` adds the pins and the printing.

The relays record each switch (pin, new state) instead of driving a pin. Each relay pin differs from its index, so that an override bit read by index would show. The average is negative on a surplus, positive on an import; the tests use a 1-minute sliding average, brought exactly to each power.

## Tests

| Test | Description |
|------|-------------|
| `test_the_minimum_times_*` / `test_the_surplus_threshold_*` / `test_the_defaults` | Minutes kept in seconds; the surplus threshold positive whatever its sign in the config, the import threshold keeps its sign; the defaults |
| `test_exactly_the_surplus_threshold_does_not_turn_on` / `test_exactly_the_import_threshold_does_not_turn_off` | Both thresholds are strict: 1 W beyond switches |
| `test_between_the_thresholds_nothing_changes` | Between them, an OFF relay stays OFF, an ON relay stays ON |
| `test_a_negative_import_threshold_turns_off_on_a_small_surplus` | Battery mode: OFF when the surplus drops below the threshold |
| `test_a_new_relay_waits_its_minimum_off_time` | At start-up the relay is OFF since 0 s: the first ON waits the minimum OFF time |
| `test_the_minimum_on_time_ends_after_exactly_min_on_seconds` / `test_the_minimum_off_time_restarts_at_each_switch_off` | Each minimum time ends after exactly that many seconds, and restarts at each switch |
| `test_the_duration_stops_at_its_maximum_instead_of_wrapping` | After 18 h in a state, the duration stays at 65535 s |
| `test_a_forced_relay_*` / `test_forced_only_*` | An override bit (that of the pin) turns the relay ON during an import and keeps it ON; the minimum times still apply; released, the relay is left to the engine |
| `test_nothing_switches_during_the_first_minute` / `test_a_change_of_any_relay_holds_every_relay_for_a_minute` / `test_the_settle_time_holds_a_switch_off_after_a_switch_on` / `test_no_change_starts_no_settle_time` | The settle time: 60 s at start-up and after any change, for every relay; none without a change |
| `test_on_a_surplus_the_relays_come_in_*` / `test_on_an_import_the_relays_go_out_*` | One relay per settle time: in by increasing index, out by decreasing index |
| `test_a_relay_held_by_its_minimum_*_lets_the_*` / `test_each_relay_comes_in_at_its_own_threshold` | A relay that cannot switch is skipped: the next one may |
| `test_battery_relays_go_out_by_decreasing_index_on_a_small_surplus` | Battery mode: on a small surplus or at 0 W, the last relay goes out first, as on an import |
| `test_a_minimum_time_beyond_1092_minutes_is_marked_for_validation` | 1093 min do not fit in 16 bits of seconds: kept as UINT16_MAX, refused by `validation.h` |
| `test_a_forced_relay_comes_in_during_an_import` / `test_a_forced_relay_stays_on_while_the_others_go_out` / `test_a_released_relay_is_left_to_the_engine` | Overrides through the engine |
| `test_diversion_off_*` | Diversion OFF: every unforced relay OFF in one go, the forced ones ON, the average ignored; the minimum times and the settle time still apply |
| `test_the_engine_decides_on_the_average_not_on_the_last_value` / `test_the_engine_decides_on_the_triple_ema` | One datalog does not make the average; the decision is on the triple EMA |

## Running

```bash
pio test -e native -f "*test_relay_engine*"
```
