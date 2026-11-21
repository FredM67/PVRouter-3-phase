# Override Pin System Tests

Tests for the override pin utilities that allow external control of loads via GPIO pins, including support for remote loads.

## Tests

| Test | Description |
|------|-------------|
| `test_indicesToBitmask_*` | Conversion from pin indices to bitmask |
| `test_are_pins_valid_*` | Pin validation (rejects 0, 1, 14+) |
| `test_PinList_*` | PinList class for storing pin configurations |
| `test_PinList_toRemoteBitmask_*` | Bitmask generation for remote loads (virtual pins >= 128) |
| `test_KeyIndexPair_*` | Override pin/load mapping pairs |
| `test_OverrideTable_*` | OverrideTable class for managing configurations |
| `test_*_mixed` | Mixed local + remote load scenarios |
| `test_TargetMap_*` | The codes of config.h (`LOAD()`, `LOCAL_LOAD()`, `RELAY()`, `ALL_...()`) turned into pins: out of range or without relay diversion, pin 0; groups and single targets in one list |
| `test_OverrideTable_from_the_settings_*` | The `OverridePins` of config.h give the same table as the same targets written as pins |

## Running

```bash
pio test -e native -f "*test_utils_override*"
```
