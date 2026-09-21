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
| `test_OverridePins_*` | OverridePins class for managing configurations |
| `test_*_mixed` | Mixed local + remote load scenarios |

## Running

```bash
pio test -e native -f "*test_utils_override*"
```
