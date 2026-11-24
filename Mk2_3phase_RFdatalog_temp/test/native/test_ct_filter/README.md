# CT Filter Tests

Tests for `ct_filter.h`, the filter that offsets the high-pass behaviour of the CTs (the grid CT of the single-phase router): a low-pass of the current, added back with a gain (`lpf_gain`, `alpha` in `calibration.h`), one state per phase. It is Robin's filter of `Mk2_fasterControl_Full`, at his resolution (x256), in fixed point instead of his float maths (the same code and the same tests in both firmwares): `coefficientsOf()` turns the float settings into fixed point at compile time, the update truncates towards zero as his float to int32 conversion does, and the output is back at x64, saturated to the int16 range.

The reference is Robin's code, verbatim, in float. The tolerance is **one ADC count (64 at the x64 scale of the samples) at the output**, for a gain up to 20. Measured worst cases (`maxOutputError`): 5 (x64 units) on a step, 25 on a sine and 12 on noise with gain 9 and alpha 0.0011; 55 on a sine with gain 20. The error grows with the gain: the state is kept at x256 and a one-unit difference of the state becomes gain / 4 units of x64 at the output.

## Tests

| Test | Description |
|------|-------------|
| `test_gain_zero_is_identity` | With a gain of 0 the sample comes back unchanged, full scale included |
| `test_saturates_at_the_int16_limits` | After a DC residual of 6 counts, a full-scale peak saturates to `INT16_MAX` / `INT16_MIN`, sign kept, instead of wrapping |
| `test_a_small_steady_input_moves_the_state` | 4 ADC counts move the low-pass state by one x256 unit, as Robin's: the dead zone is no wider than his (at x64 it was 14 counts) |
| `test_time_constant_within_2_percent_of_robins` | After a step, the low-pass reaches 1 - 1/e of it within 2 % of -1 / ln(1 - alpha) samples, for alpha 0.0011 and 0.002 |
| `test_old_board_settings_track_robins_float` | Gain 9, alpha 0.0011 (the single-phase old boards): step, sine and noise stay within the tolerance of Robin's float output at every sample |
| `test_shipped_alpha_tracks_robins_float` | The same with alpha 0.002 (`calibration.h`) |
| `test_other_gains_track_robins_float` | Gains 0.5, 1, 4.5 and 20 with both alphas: step, sine and noise |
| `test_state_settles_on_the_dc_residual_as_robins` | After a second of a 50 Hz current with a DC residual, the state is within one count of his |
| `test_valid_range_of_the_settings` | `isAccurate()` accepts the usual settings and refuses alpha 0, 0.0625 and above, a too small alpha, a gain of 64 and above, and a negative gain |

## Running

```bash
pio test -e native -f "native/test_ct_filter"
```
