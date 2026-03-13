# DC Offset Filter Tests

Tests for the DC offset filter with left-aligned ADC (PR #127).

## Tests

### Basic Functionality (6 tests)

| Test | Description |
|------|-------------|
| `test_initialization_values` | Verifies nominal filter values |
| `test_left_aligned_adc` | Validates left-aligned ADC conversion |
| `test_rounding_behavior` | Checks `\| 32U` rounding behavior |
| `test_sample_minus_dc_basic` | DC offset subtraction for various ADC values |
| `test_int16_overflow_edge_case` | Documents int16_t overflow at ADC max |
| `test_safe_operating_range` | Verifies safe ADC range (0-1022) |

### Filter Tracking (3 tests)

| Test | Description |
|------|-------------|
| `test_filter_tracks_positive_offset` | Filter tracks upward DC drift |
| `test_filter_tracks_negative_offset` | Filter tracks downward DC drift |
| `test_filter_stability_centered_waveform` | Filter stability with AC waveform |

### Edge Cases (4 tests)

| Test | Description |
|------|-------------|
| `test_adc_stuck_at_zero` | Fault condition: ADC stuck low |
| `test_adc_stuck_at_max` | Fault condition: ADC stuck high (overflow) |
| `test_accumulator_no_wrap_normal_operation` | No accumulator wrap in normal use |
| `test_large_step_change_recovery` | Filter recovery after DC step |

### Numerical Precision (2 tests)

| Test | Description |
|------|-------------|
| `test_filter_time_constant` | Filter convergence rate |
| `test_q15_precision` | Q15 fixed-point extraction |

### Old vs New Filter Comparison (5 tests)

| Test | Description |
|------|-------------|
| `test_compare_filters_same_dc_level` | Both track to same DC with equivalent input |
| `test_compare_filters_ac_waveform` | Both stable with sinusoidal AC |
| `test_compare_filters_step_response` | Step change tracking comparison |
| `test_compare_sample_minus_dc_equivalence` | Sample-DC calculation equivalence |
| `test_compare_filter_limits_behavior` | New filter behavior vs old limits |

## Key Findings

### Old vs New Filter Equivalence
- **Same DC Level**: Both filters track to the same offset (within 5 ADC counts)
- **AC Waveform**: Both maintain stability with sinusoidal input
- **Sample Calculation**: Mathematically equivalent (accounting for scaling)

### Behavioral Differences

| Aspect | Old Filter | New Filter |
|--------|------------|------------|
| ADC alignment | Right (0-1023) | Left (0-65472) |
| Scaling | x256 | x64 |
| Update method | Accumulate & reset | Continuous integration |
| Time constant | Faster (~200 cycles) | Slower (~400 cycles) |
| DC limits | Explicit (412-612) | No limits |
| Noise immunity | Good | Better (slower response) |

### int16_t Overflow at ADC Max
When ADC is at maximum (1023), the sample-minus-DC calculation overflows:
- `(65472 | 32) - 32704 = 32800`
- 32800 wraps to -32736 as int16_t

**Safe operating range: ADC 0-1022**

In practice, 230V systems use ADC ~112-912, well within safe bounds.

### Filter Limits Comparison
- Old filter clamps to 412-612 ADC (±100 from 512)
- New filter has no explicit limits but tracks the actual DC level
- This allows the new filter to handle miscalibrated systems better

## Running

```bash
pio test -e native -f "*test_dc_offset_filter*"
```
