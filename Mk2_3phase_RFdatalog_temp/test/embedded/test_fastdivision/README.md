# Fast Division Tests

Tests for AVR assembly-optimized integer division routines used in ISR for performance.

## Tests

| Test | Description |
|------|-------------|
| `test_divu10_*` | Division by 10 (basic, edge cases, large values, random) |
| `test_divmod10_*` | Division by 10 with remainder |
| `test_divu8` | Division by 8 |
| `test_divu4` | Division by 4 |
| `test_divu2` | Division by 2 |
| `test_divu1` | Division by 1 (identity) |

## Running

```bash
pio test -e uno -f "*test_fastdivision*"
```

## Why Fast Division?

Standard 32-bit division on AVR can take 400+ cycles. The ISR runs at 9.6 kHz (every 104us) and must complete quickly. Assembly-optimized division provides ~4-5x speedup.
