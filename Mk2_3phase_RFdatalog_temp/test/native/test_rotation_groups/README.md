# Rotation Groups Tests

Tests for the rotation groups: `rotationGroups` in `config.h` lists the loads that take turns at the top of the priority list, as groups written with the target codes of the override pins (`LOAD()`, `REMOTE_LOAD()`, `ALL_LOCAL_LOADS()`...). Each group rotates among the priority places its loads hold; loads in no group never move.

## Tests

| Test | Description |
|------|-------------|
| `test_no_group` | `RotationGroups{}` holds no group |
| `test_one_group_of_every_local_load` | `{ { ALL_LOCAL_LOADS() } }` is one group holding that code |
| `test_two_groups_keep_their_codes` | two groups of three loads keep their codes, in order |
| `test_missing_inner_braces_make_one_load_groups` | `{ LOAD(0), LOAD(1) }` is two groups of one load (refused later: a group needs 2 loads) |

## Running

```bash
pio test -e native -f "native/test_rotation_groups"
```
