# Rotation Groups Tests

Tests for the rotation groups: `rotationGroups` in `config.h` lists the loads that take turns at the top of the priority list, as groups written with the target codes of the override pins (`LOAD()`, `REMOTE_LOAD()`, `ALL_LOCAL_LOADS()`...). Each group rotates among the priority places its loads hold; loads in no group never move.

## Tests

| Test | Description |
|------|-------------|
| `test_no_group` | `RotationGroups{}` holds no group |
| `test_one_group_of_every_local_load` | `{ { ALL_LOCAL_LOADS() } }` is one group holding that code |
| `test_two_groups_keep_their_codes` | two groups of three loads keep their codes, in order |
| `test_missing_inner_braces_make_one_load_groups` | `{ LOAD(0), LOAD(1) }` is two groups of one load (refused later: a group needs 2 loads) |
| `test_loads_of_load_and_local_load` | `LOAD(n)` is load n; `LOCAL_LOAD(n)` too, if it is local; nothing beyond the map |
| `test_loads_of_remote_load_follows_map_order` | `REMOTE_LOAD(n)` is the n-th remote load, local loads interleaved |
| `test_loads_of_pin_is_the_local_load_on_it` | a plain number is the local load on that pin, or nothing |
| `test_loads_of_groups_and_relays` | `ALL_LOCAL_LOADS()`, `ALL_REMOTE_LOADS()`, `ALL_LOADS()` give their loads; relay codes give none |
| `test_checks_pass_for_two_good_groups` | the four checks accept a local group and a group on one remote unit |
| `test_check_relay_is_not_a_load` | a relay in a group is refused |
| `test_check_group_of_one` | a group of one load is refused |
| `test_check_load_twice` | a load twice, across groups, within a group, or through a group code, is refused |
| `test_check_mixed_local_and_remote` | a group mixing local and remote loads is refused |
| `test_check_two_remote_units` | a group spanning two remote units is refused |
| `test_checks_pass_without_groups` | no group passes every check |
| `test_one_group_of_every_load_is_todays_rotation` | one group holding every load rotates as `LoadPriorities::rotate()` |
| `test_two_interleaved_groups_rotate_apart` | priorities 0 3 1 4 2 5, two groups: 2 5 0 3 1 4 |
| `test_loads_outside_groups_never_move` | only the group's places change |
| `test_on_bits_travel_with_their_load` | each load keeps its ON state |
| `test_n_rotations_are_back_to_the_start` | rotating as many times as the group sizes need gives the start back |
| `test_table_without_groups_changes_nothing` | no group, no change |
| `test_table_gives_the_loads_of_each_group` | the loads of each group, for the startup summary |

## Running

```bash
pio test -e native -f "native/test_rotation_groups"
```
