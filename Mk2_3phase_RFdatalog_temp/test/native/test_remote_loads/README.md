# Remote Load Tests

Tests for the packed load map (`load_map.h`), the per-unit payload builder (`remote_loads_core.h`) and the map-derived override pins (`utils_override.h`), which spread the dump loads of a router over up to three remote units. No stubs: the only stand-in is a callable that records what the radio would have sent.

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
| `test_overridePinOf_*` / `test_*masks*` | Override pins derived from the map: physical pin or `REMOTE_PIN_BASE` + remote rank, fed through the real `OverridePins` |
| `test_isValidMap_*` | Load map check behind `validation.h`: usable pins only, `unused_pin` sentinel caught |
| `test_node_ids_*` | Node ID check behind `validation.h`: range 1-30, unique, not the router's, table long enough |
| `test_sendPending_*` | Send loop with the radio replaced by a recording callable: unit *n* goes to `REMOTE_NODE_ID[n-1]`, only due units are sent |

## Running

```bash
pio test -e native -f "*test_remote_loads*"
```
