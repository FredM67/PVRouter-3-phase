# Remote Load Tests

Tests for the packed load map (`load_map.h`) and the per-unit payload builder (`remote_loads_core.h`), which spread the dump loads of a router over up to three remote units.

## Tests

| Test | Description |
|------|-------------|
| `test_local_*` / `test_remote_*` | Packed encoding: unit number in bits 6-7, pin or status LED in bits 0-5 |
| `test_census_*` | `constexpr` census of a map: unit count, remote load count, loads per unit |
| `test_remoteOrdinal_*` | Position of a load among the remote ones (the `REMOTE_LOAD(n)` space) |
| `test_bitOf_*` | Position of a load within its own unit's payload |
| `test_*_unit*` / `test_interleaved_map` | Grouping loads by unit, locals ignored, units independent |
| `test_first_load_of_a_unit_is_bit_zero` | Bit ordering: ascending map order, first load of a unit is bit 0 |
| `test_index_alignment_*` | Regression pin: load *k* must be paired with state *k*, not with state *N-1-k* |
| `test_*_flag*` / `test_payload_tracks_*` | Change detection and draining of a pending transmission |
| `test_refresh_*` | Keep-alive: a payload is re-sent every `REMOTE_REFRESH_CYCLES` mains cycles |
| `test_*_no_unit_*` / `test_reset_*` / `test_*_beyond_the_core_*` | Edge cases: no remote unit, reset, a unit the router does not talk to |

## Running

```bash
pio test -e native -f "*test_remote_loads*"
```
