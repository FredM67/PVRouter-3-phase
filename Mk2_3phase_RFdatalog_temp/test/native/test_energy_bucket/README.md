# Energy Bucket Tests

Tests for the integer energy bucket (`energy_bucket.h`), which replaces the float arithmetic `bucket += (l_sumP / n) * f_powerCal[phase]` in the ADC ISR (#120). The float expression, evaluated in double precision, is the reference: the integer version must match it over the full input range, without overflow and without drifting over time.

## Tests

| Test | Description |
|------|-------------|
| `test_shift_*` | The fixed-point shift is derived from the largest calibration value, shared by all phases |
| `test_largest_value_fills_16_bits` | The largest calibration value uses the full 16 bits |
| `test_toFixed_rounds_to_nearest` | Calibration values are rounded, not truncated |
| `test_*quantisation_error*` | The quantised calibration stays within 0.002% of the float value, from 0.005 to 0.5 |
| `test_contribution_matches_the_reference_*` | One cycle's contribution matches the float reference within one unit, from -2^18 to +2^18 |
| `test_contribution_at_full_scale_does_not_overflow` | The ~35-bit product does not wrap |
| `test_contribution_is_sign_symmetric` | Import and export weigh the same |
| `test_no_drift_over_one_minute` | 3000 accumulated cycles stay within bounds of the float sum |
| `test_fromWatts_*` / `test_bucket_capacity_*` | Watts to bucket units; the largest bucket fits in 32 bits |

## Running

```bash
pio test -e native -f "*test_energy_bucket*"
```
