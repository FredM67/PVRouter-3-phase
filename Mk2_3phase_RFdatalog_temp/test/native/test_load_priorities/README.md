# Load Priority Tests

Tests for the load priority state machine (`load_priorities.h`), which the ADC ISR runs once per mains cycle: it decides which load to switch from the level of the energy bucket, then gives the physical load states. The priority array holds one logical load per element, the highest priority first: the physical load number in the low 7 bits, the ON state in the high bit.

The tests use a bucket of 1000 units, with both thresholds at the middle (normal mode) or 100 units away from it (anti-flicker mode).

## Tests

| Test | Description |
|------|-------------|
| `test_next_to_add_*` / `test_next_to_remove_*` | The next load to switch ON is the first one OFF; the next to switch OFF is the last one ON; none when all are ON / OFF |
| `test_next_ignores_the_load_numbers` | Positions in priority order, whatever the physical load numbers |
| `test_high_energy_switches_the_first_load_on` | Above the upper threshold, the first load OFF comes ON, and a post-transition period starts |
| `test_switching_on_follows_the_priorities_not_the_load_numbers` | With load 2 first in priority, load 2 comes ON first |
| `test_the_next_load_waits_for_the_end_of_the_post_transition_period` | After a switch, no other load switches for `POST_TRANSITION_MAX_COUNT` cycles |
| `test_the_previous_load_waits_for_the_end_of_the_post_transition_period` | The same, switching OFF |
| `test_the_upper_threshold_follows_the_bucket_*` / `test_the_*_threshold_stays_within_the_bucket` | During that period, the threshold follows the bucket, within 0 and the capacity |
| `test_low_energy_switches_the_last_load_off` | Below the lower threshold, the last load ON goes OFF |
| `test_the_load_switched_last_may_switch_back_at_once` | The load just switched may switch back during the period |
| `test_the_middle_of_the_bucket_counts_as_the_lower_half` / `test_each_half_resets_the_opposite_threshold` | Each half of the bucket resets the other half's threshold to its default |
| `test_anti_flicker_thresholds_leave_a_dead_band` | In anti-flicker mode, nothing switches between the two thresholds |
| `test_nothing_to_switch_starts_no_post_transition_period` | All ON in a high bucket, or all OFF in a low one: no transition |
| `test_a_surplus_brings_every_load_on_then_its_end_every_load_off` | A rising bucket brings every load ON, one at a time; a falling one, every load OFF |
| `test_rotate_*` | Rotation: the last load becomes the first, each keeps its state; N rotations are back to the start; a single load is left alone (no write outside the array) |
| `test_physical_states_follow_the_load_numbers` | The logical states land on the physical loads they name |
| `test_diversion_off_switches_every_load_off` / `test_an_override_forces_its_load_on` / `test_the_override_is_asked_for_the_physical_load` | Diversion OFF stops the surplus diversion only: a forced load stays ON |
| `test_calibration_mode_keeps_every_load_off` | Calibration mode: every load OFF, forced or not |

## Running

```bash
pio test -e native -f "*test_load_priorities*"
```
