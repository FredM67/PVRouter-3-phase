# EWMA Average Tests

Tests for the Exponentially Weighted Moving Average filter used for cloud immunity in power measurements.

## Tests

| Test | Description |
|------|-------------|
| `test_initial_values` | Verifies EWMA initializes to zero |
| `test_single_value_update` | Tests response to first input value |
| `test_multiple_value_updates` | Tests averaging behavior over multiple samples |
| `test_large_value_response` | Tests filter response to sudden large changes |
| `test_convergence_behavior` | Verifies filter converges to steady-state value |
| `test_reset_behavior` | Tests reset functionality clears state |

## Running

```bash
pio test -e native -f "*test_ewma_avg*"
```
