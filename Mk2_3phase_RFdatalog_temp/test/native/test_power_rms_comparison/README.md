# Power and RMS Calculation Comparison Tests

Tests comparing old (right-aligned ADC) vs new (left-aligned ADC) implementations for power and RMS calculations, based on Florian's suggestions in issue #121.

## Purpose

Verify that the new left-aligned ADC implementation produces equivalent results to the old right-aligned implementation, ensuring safe migration.

## Implementations Compared

| Aspect | Old Implementation | New Implementation |
|--------|-------------------|-------------------|
| ADC alignment | Right (0-1023) | Left (0-65472) |
| Voltage scaling | x256 | x64 |
| Current scaling | x256 | x64 |
| Power pre-shift | `>> 2` (x64) | `>> 2` (x16) |
| Power final shift | `>> 12` | `>> 8` |
| V² final shift | `>> 12` | `>> 8` |
| Rounding | None | `\| 32U` (+0.5 LSB) |

## Tests

### Basic Power Calculation (4 tests)

| Test | Description |
|------|-------------|
| `test_instant_power_equivalence` | Single sample V*I calculation equivalence |
| `test_power_unity_power_factor` | Full cycle with PF=1 (V and I in phase) |
| `test_power_reactive_load` | Full cycle with PF=0 (90° phase shift) |
| `test_power_export` | Full cycle with PF=-1 (180° phase shift, export) |

### RMS Voltage Calculation (2 tests)

| Test | Description |
|------|-------------|
| `test_vsquared_accumulation_equivalence` | V² accumulation comparison |
| `test_rms_theoretical_value` | RMS vs theoretical (peak/√2) |

### Full Cycle Comparison (3 tests)

| Test | Description |
|------|-------------|
| `test_full_cycle_equivalence` | 100 cycles with DC filter updates |
| `test_dc_offset_compensation` | Handling of non-centered DC level |
| `test_three_phase_simulation` | 3-phase system (0°, 120°, 240°) |

### Edge Cases and Precision (5 tests)

| Test | Description |
|------|-------------|
| `test_zero_load` | No current (should show zero power) |
| `test_near_adc_limits` | High amplitude near ADC limits |
| `test_no_accumulator_overflow` | Accumulator consistency per cycle |
| `test_mathematical_identity` | **CRITICAL**: Proves <0.1% difference |
| `test_scaling_equivalence` | Mathematical scaling verification |

### Extended Tests with Noise (5 tests)

| Test | Description |
|------|-------------|
| `test_extended_10000_cycles` | 10,000 cycles (~200 sec at 50Hz) |
| `test_with_realistic_noise_2lsb` | ±2 LSB ADC noise (typical) |
| `test_with_higher_noise_5lsb` | ±5 LSB ADC noise (stress) |
| `test_varying_power_factor_with_noise` | PF 0°-90° with noise |
| `test_stress_50000_cycles` | 50,000 cycles (~17 min at 50Hz) |

### Realistic Load Variation Tests (6 tests)

All tests use current amplitudes up to ADC limit (~505 with mid-point at 512).

| Test | Description |
|------|-------------|
| `test_multiple_current_amplitudes` | Tests 10 different current levels (20-505) from light to ADC limit |
| `test_sinusoidal_load_variation` | Current amplitude varies 50-500 sinusoidally over 5,000 cycles |
| `test_random_load_steps` | Random appliance on/off (20-505) over 10,000 cycles |
| `test_cloud_shadow_simulation` | Solar cloud shadow dynamics (80-500) over 10,000 cycles |
| `test_daily_solar_profile` | 24-hour solar pattern with 500 peak over 8,640 cycles |
| `test_import_export_transitions` | Bidirectional power flow with phase changes over 10,000 cycles |

## Key Findings

### Mathematical Identity Proven
- **With matching DC offset**: Implementations are **IDENTICAL** (<0.1% difference)
- **Rounding averages out**: The `| 32` rounding adds ~1.5% per sample but averages to 0% over a cycle
- **Observed differences** (2-3%) come from DC offset choice (511 vs 512), NOT algorithm differences

### Behavioral Differences
- New implementation includes rounding (`| 32U`) for better precision
- New DC filter has no explicit limits (vs old ±100 ADC limit)
- Scaling factors are mathematically equivalent after normalization

### Mathematical Equivalence

**Old (x256 scale):**
```
V_scaled = (rawV - 512) << 8        // x256
I_scaled = (rawI - 512) << 8        // x256
instP = (V_scaled >> 2) * (I_scaled >> 2) >> 12  // x1
```

**New (x64 scale):**
```
V_scaled = (rawV << 6) - (511 << 6)  // x64
I_scaled = (rawI << 6) - (511 << 6)  // x64
instP = (V_scaled >> 2) * (I_scaled >> 2) >> 8   // x1
```

Both produce equivalent results because:
- Old: 256/4 = 64, then 64*64/4096 = 1
- New: 64/4 = 16, then 16*16/256 = 1

## Running

```bash
pio test -e native -f "*test_power_rms*"
```

Or run all native tests:
```bash
pio test -e native
```
