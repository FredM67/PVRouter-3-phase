# Assembly Multiplication Functions Tests

This directory contains Unity-based unit tests for the assembly-optimized multiplication functions.

**Note:** Performance benchmarks are in a separate directory (`test_mult_asm_perf/`). Both suites run in CI in the simavr simulator (`uno_sim` environment), which is cycle-accurate: the benchmark timings are exact, and tracked in the CI benchmark history.

## Running the Tests

To run the multiplication function tests on a connected board (AVR), or in simavr with `--environment uno_sim`:

```bash
# Run functional tests
pio test --environment uno --filter test_mult_asm

# Run performance benchmarks
pio test --environment uno --filter test_mult_asm_perf

# Run all multiplication tests including benchmarks
pio test --environment uno --filter "test_mult_asm*"

# Run with verbose output
pio test --environment uno --filter test_mult_asm --verbose
```

## Test Coverage

The tests cover:

1. **Basic Functionality**
   - `multS16x16_to32`: 16×16→32 signed multiplication
   - `multU16x16_to32`: 16×16→32 unsigned multiplication
   - `mult16x8_q8`: 16×8 Q8 fractional multiplication with rounding

2. **Edge Cases**
   - Zero values
   - Maximum/minimum values
   - Overflow scenarios

3. **Q8 Format Testing**
   - Conversion helpers (`float_to_q8`, `q8_to_float`)
   - Rounding behavior
   - Fractional arithmetic accuracy

4. **Assembly vs Standard Comparison**
   - Verifies assembly results match standard C multiplication
   - Tests across multiple value ranges

5. **Performance Benchmarks**
   - Measures execution time of assembly vs standard implementations
   - ISR simulation to demonstrate real-world impact

## Benchmark Results (Arduino Uno @ 16MHz)

Results measured on real Arduino Uno hardware:

| Function | Operations | Assembly Time | Standard Time | Speedup |
|----------|------------|---------------|---------------|---------|
| `multS16x16_to32` | 36,000 | 99,376μs | 146,744μs | **1.48x** |
| `multU16x16_to32` | 49,000 | 127,636μs | 146,496μs | **1.15x** |
| `mult16x8_q8` | 480,000 | 792,828μs | 1,867,324μs | **2.36x** |

**ISR Simulation (1,000 samples):**

| Metric | Assembly | Standard | Improvement |
|--------|----------|----------|-------------|
| Total time | 8,364μs | 12,768μs | **1.53x faster** |
| Time per sample | 8.36μs | 12.77μs | **4.40μs saved** |

**Why This Matters:**

The ADC ISR runs at 9.6 kHz (every 104μs). Saving 4.40μs per ISR iteration:

- Frees ~4.2% of each 104μs ISR period
- Provides more headroom for other processing
- Critical for maintaining real-time performance

## Test Structure

The tests follow the Unity testing framework pattern used throughout the project:
- `setUp()` and `tearDown()` for test initialization/cleanup
- Individual test functions with descriptive names
- `TEST_ASSERT_*` macros for assertions
- Arduino-style `setup()` and `loop()` structure

## Hardware Requirements

These tests need an AVR (Arduino Uno/compatible) since they test AVR-specific assembly optimizations: a real board, or the simavr simulator (`uno_sim` environment, Linux/WSL).
