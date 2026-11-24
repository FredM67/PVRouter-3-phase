# Sample Set Counter Tests

Tests for `sample_sets.h`: the counter of the sample sets of a datalog period, the divisor of the datalog power and voltage. With 6 conversions per sample set (a V and an I per phase), a mains cycle holds up to `N_MAX` sets (36 at 50 Hz, 30 at 60 Hz). The counter is chosen at compile time, as the V² scaling: `uint16_t` while the largest count of the period fits (up to 36 s, so the default 5 s keeps it), `uint32_t` above, up to the 40 s allowed.

## Tests

| Test | Description |
|------|-------------|
| `test_largest_count_of_a_period` | Cycles of the period × `N_MAX`: 64800 at 36 s, 72000 at 40 s (50 and 60 Hz) |
| `test_16_bits_up_to_36_s` | 5 s and 36 s (50 and 60 Hz) and `UINT16_MAX` give `uint16_t`, which holds the largest count |
| `test_32_bits_from_37_s_to_40_s` | 37 s and 40 s (50 and 60 Hz) give `uint32_t`, which holds the largest count |
| `test_datalog_sums_hold_40_s` | At 40 s, the 64-bit power sum and the 32-bit V² sum (scaled by 16) hold the largest count at full scale |

## Running

```bash
pio test -e native -f "native/test_sample_sets"
```
