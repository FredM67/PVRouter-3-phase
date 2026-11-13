# Datalog Arithmetic Tests

Tests for the integer datalog arithmetic (`datalog_math.h`), which replaces the float code that computed, at the end of each datalog period, the power and the RMS voltage of each phase (#120):

- `power_L = -(sumP / n) * f_powerCal[phase]`
- `Vrms_L_x100 = factor * f_voltageCal[phase] * sqrt(sum_Vsquared / n)` (factor 100, or 400 when the ISR scales V² down by 16 for datalog periods over 10 s)

The float expressions, evaluated in double precision, are the reference. The calibration constants have at least 16 significant bits, like the root of the normalised square root; together with the final rounding, the power stays within 0.5 W + 2⁻¹⁶ of the value (< 1 W up to 32 kW), and the voltage within 0.012 V at 230 V.

## Tests

| Test | Description |
|------|-------------|
| `test_isqrt_at_every_boundary` | For all 65536 roots r: r² - 1, r² and r² + r give r, r² + r + 1 gives r + 1 (every rounding boundary of the 32-bit range) |
| `test_isqrt_matches_the_reference` | 2 million values over the whole 32-bit range give `round(sqrt())`, in double precision |
| `test_isqrt_edge_cases` / `test_isqrt_of_the_largest_value_saturates` | 0 to 4, 2^30, 2^31, the boundaries around 32768.5, and 65535 for `UINT32_MAX` |
| `test_sqrtScaled_*` | The root of the normalised value keeps at least 15 significant bits; 0 stays 0 |
| `test_power_calibration_is_constexpr` | The fixed-point power calibration is built at compile time, with the shift of the energy bucket |
| `test_watts_matches_the_reference_*` | Watts match the float reference from -2^18 to +2^18, old board, new board (50 A and 20 A CTs), extremes |
| `test_watts_is_sign_symmetric` | Import and export weigh the same |
| `test_watts_saturates_beyond_int16` | Out-of-range powers saturate to ±32767 W instead of wrapping |
| `test_watts_differs_from_the_float_code_by_at_most_1_W` | The former code truncated, the new one rounds |
| `test_voltage_calibration_is_constexpr` | The fixed-point voltage calibration is built at compile time and fills 16 bits |
| `test_vrms_matches_the_reference_*` | Vrms matches the float reference up to the largest ADC voltage, short and long datalog periods |
| `test_vrms_at_230_V` / `test_vrms_of_zero` / `test_vrms_saturates` | A typical value, no voltage, and saturation at 655.35 V |
| `test_valid_voltage_calibration` | Zero, negative or too small voltage calibration values are rejected |

On AVR, `isqrt()` is an assembly loop: [test_datalog_math_avr](../../embedded/test_datalog_math_avr/) runs the same boundary checks on the target (simavr), and measures the cycles against the float code. These native tests check the C version of the same algorithm.

## Running

```bash
pio test -e native -f "*test_datalog_math*"
```
