# Assembly Multiplication Functions Tests

This directory contains Unity-based unit tests for the assembly-optimized multiplication functions.

## Running the Tests

To run the multiplication function tests on embedded hardware (AVR):

```bash
# Run all embedded tests
pio test --environment uno

# Run only the multiplication tests
pio test --environment uno --filter test_mult_asm

# Run with verbose output
pio test --environment uno --filter test_mult_asm --verbose
```

## Test Coverage

The tests cover:

1. **Basic Functionality**
   - `mult16x16_to32`: 16×16→32 signed multiplication
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

## Test Structure

The tests follow the Unity testing framework pattern used throughout the project:
- `setUp()` and `tearDown()` for test initialization/cleanup
- Individual test functions with descriptive names
- `TEST_ASSERT_*` macros for assertions
- Arduino-style `setup()` and `loop()` structure

## Hardware Requirements

These tests require AVR hardware (Arduino Uno/compatible) since they test AVR-specific assembly optimizations.