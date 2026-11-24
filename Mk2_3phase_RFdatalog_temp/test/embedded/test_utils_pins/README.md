# GPIO Pin Manipulation Tests

Tests for low-level GPIO pin manipulation functions.

## Tests

| Test | Description |
|------|-------------|
| `test_setPinON` | Setting a single pin HIGH |
| `test_setPinOFF` | Setting a single pin LOW |
| `test_togglePin` | Toggling pin state |
| `test_setPinState` | Setting pin to specific state |
| `test_setPinsState` | Setting multiple pins HIGH and LOW via bitmasks, one write per port |
| `test_setPinsState_skipsUnusedPort` | A port with none of the used pins is not written |

## Running

```bash
pio test -e uno -f "*test_utils_pins*"
```
