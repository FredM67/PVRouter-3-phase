# GPIO Pin Manipulation Tests

Tests for low-level GPIO pin manipulation functions.

## Tests

| Test | Description |
|------|-------------|
| `test_setPinON` | Setting a single pin HIGH |
| `test_setPinOFF` | Setting a single pin LOW |
| `test_togglePin` | Toggling pin state |
| `test_setPinState` | Setting pin to specific state |
| `test_setPinsON` | Setting multiple pins HIGH via bitmask |
| `test_setPinsOFF` | Setting multiple pins LOW via bitmask |

## Running

```bash
pio test -e uno -f "*test_utils_pins*"
```
