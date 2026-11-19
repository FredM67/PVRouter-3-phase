# Datalog Arithmetic on the Target

Tests of `datalog_math.h` on the ATmega328P, where `isqrt()` is an assembly loop (the native suite [test_datalog_math](../../native/test_datalog_math/) checks the C version of the same algorithm).

## Correctness

| Test | Description |
|------|-------------|
| `test_isqrt_on_avr_every_boundary` | For all 65536 roots r: r² - 1 (from r = 2), r² and r² + r give r, r² + r + 1 gives r + 1. `round(sqrt(x))` changes from r to r + 1 between r² + r and r² + r + 1, so this covers every rounding boundary of the 32-bit range, up to `UINT32_MAX` = 65535² + 65535, and the largest remainders |
| `test_isqrt_on_avr_edge_cases` | 0 to 4, 2^30 (smallest normalised value), 2^31, the boundaries around 32768.5, 65535² and `UINT32_MAX` (saturated to 65535) |
| `test_vrms_on_avr_matches_the_float_code` | `sqrtScaled()` within 0.5 of the exact value, and `vrms()` within 0.01 V of the former float code, from 0 to the largest ADC voltage |

## Benchmark

Timer1 runs at the CPU clock with interrupts disabled: the counts are exact CPU cycles, in simavr as on a real board. Each test fails if the integer code gets slower than the float code it replaced. In simavr, average / worst case:

| | Float code (before) | Integer code |
|---|---|---|
| Square root (with the conversions of the float code) | 573 / 662 | 446 / 513 (`isqrt()` alone: 415 / 421) |
| `Vrms_L_x100` of a phase | 831 / 916 | 569 / 653 |
| `power_L` of a phase | 291 / 334 | 156 / 158 |

`sqrtScaled()` first normalises the value to >= 2^30, so that the root keeps 15 to 16 significant bits (without it, 230 V would only get ~12 bits, ±0.05 V): a whole byte costs ~8 cycles, a pair of bits ~14. The worst case is the smallest value (1: 3 bytes and 3 pairs, ~90 cycles more than `isqrt()`); a mains voltage needs ~60.

## Running

```bash
pio test -e uno_sim -f embedded/test_datalog_math_avr -v   # -v prints the cycle counts
```
