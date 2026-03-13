# Testing Guide

This guide covers testing methodologies, tools, and procedures for the PVRouter system. Given the safety-critical nature of power electronics, comprehensive testing is essential.

## Testing Philosophy

The PVRouter testing strategy follows a multi-layered approach:

1. **Unit Tests** - Individual component verification
2. **Integration Tests** - Module interaction testing
3. **Hardware-in-the-Loop** - Real hardware validation
4. **Performance Tests** - Timing and resource validation
5. **Safety Tests** - Fault condition and protection testing

## Test Environment Setup

### Software Testing Environment

#### PlatformIO Test Framework
```ini
; platformio.ini
[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_flags =
    -D UNIT_TEST
    -D ARDUINO_ARCH_AVR

[env:embedded]
platform = atmelavr
board = uno
framework = arduino
test_framework = unity
test_port = /dev/ttyUSB0
test_speed = 115200
```

#### Unity Test Framework Setup
```cpp
// test/test_main.cpp
#include <unity.h>
#include <Arduino.h>

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Run test suites
    RUN_TEST(test_power_calculation);
    RUN_TEST(test_load_control);
    RUN_TEST(test_configuration_validation);

    UNITY_END();
    return 0;
}
```

### Hardware Testing Setup

#### Required Equipment
- **Arduino Uno/ESP32** - Target microcontroller
- **3-Phase Test Bench** - Controlled AC voltage/current source
- **Oscilloscope** - 4+ channel, 100MHz+ bandwidth
- **Power Analyzer** - Calibrated reference instrument
- **Multimeter** - True RMS capability
- **Variable Resistive Loads** - For dump load simulation
- **Current Transformers** - Calibrated CTs for testing
- **Safety Equipment** - Isolation transformer, RCD protection

#### Test Bench Configuration
```
AC Source (3-phase) → Current Transformers → PVRouter PCB
                  ↓
              Oscilloscope
                  ↓
             Power Analyzer
                  ↓
            Reference Loads
```

## Unit Testing

### Power Calculation Tests
```cpp
// test/test_power_calculation.cpp
#include <unity.h>
#include "power_calculation.h"

void test_instantaneous_power_calculation() {
    // Test data
    float voltage = 230.0;  // RMS voltage
    float current = 10.0;   // RMS current, in phase
    float expected_power = 2300.0;

    // Calculate power
    float calculated_power = calculateInstantaneousPower(voltage, current);

    // Verify within tolerance (0.1%)
    TEST_ASSERT_FLOAT_WITHIN(2.3, expected_power, calculated_power);
}

void test_rms_calculation() {
    // Generate test sine wave
    const uint16_t samples = 100;
    float sine_samples[samples];
    float amplitude = 325.27;  // Peak for 230V RMS

    for (uint16_t i = 0; i < samples; i++) {
        sine_samples[i] = amplitude * sin(2 * PI * i / samples);
    }

    // Calculate RMS
    float rms = calculateRMS(sine_samples, samples);

    // Should be approximately 230V
    TEST_ASSERT_FLOAT_WITHIN(1.0, 230.0, rms);
}

void test_three_phase_power_sum() {
    float phase_powers[3] = {1000.0, 1100.0, 900.0};
    float expected_total = 3000.0;

    float total = sumThreePhasePower(phase_powers);

    TEST_ASSERT_FLOAT_WITHIN(0.1, expected_total, total);
}
```

### Configuration Validation Tests
```cpp
// test/test_configuration.cpp
#include <unity.h>
#include "config.h"

void test_load_configuration_bounds() {
    // Test that all load indices are valid
    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; i++) {
        TEST_ASSERT_TRUE(loadPriority[i].pin >= 2);
        TEST_ASSERT_TRUE(loadPriority[i].pin <= 13);
        TEST_ASSERT_TRUE(loadPriority[i].nominalPower > 0);
        TEST_ASSERT_TRUE(loadPriority[i].nominalPower <= 10000);
    }
}

void test_pin_conflict_detection() {
    // Verify no pin conflicts exist
    uint8_t used_pins[14] = {0};  // Arduino Uno pins 0-13

    // Mark used pins
    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; i++) {
        uint8_t pin = loadPriority[i].pin;
        TEST_ASSERT_FALSE_MESSAGE(used_pins[pin], "Pin conflict detected");
        used_pins[pin] = 1;
    }
}

void test_memory_constraints() {
    // Verify memory usage is within limits
    size_t estimated_ram = sizeof(powerReadings) +
                          sizeof(loadState) +
                          sizeof(temperatureReadings);

    TEST_ASSERT_LESS_THAN(1000, estimated_ram);  // <1KB for data structures
}
```

### Temperature Sensor Tests
```cpp
// test/test_temperature.cpp
#ifdef TEMP_ENABLED

void test_temperature_bounds_checking() {
    // Test compile-time bounds checking works
    float temp0 = readTemperature<0>();  // Should compile
    // float temp_invalid = readTemperature<99>();  // Should not compile

    TEST_ASSERT_TRUE(temp0 >= -55.0 && temp0 <= 125.0);  // DS18B20 range
}

void test_temperature_sensor_addresses() {
    // Verify sensor addresses are properly configured
    for (uint8_t i = 0; i < TEMPERATURE_SENSORS_COUNT; i++) {
        // Check address is not all zeros
        bool all_zero = true;
        for (uint8_t j = 0; j < 8; j++) {
            if (temperatureSensors[i][j] != 0) {
                all_zero = false;
                break;
            }
        }
        TEST_ASSERT_FALSE_MESSAGE(all_zero, "Temperature sensor address not configured");
    }
}

#endif
```

## Integration Testing

### ADC and Power Calculation Integration
```cpp
// test/test_integration.cpp
void test_adc_to_power_pipeline() {
    // Simulate ADC readings
    uint16_t test_voltage_adc = 512;  // Mid-scale
    uint16_t test_current_adc = 600;  // Some current

    // Process through pipeline
    processAdcReading(VOLTAGE_CHANNEL, test_voltage_adc);
    processAdcReading(CURRENT_CHANNEL, test_current_adc);

    // Verify power calculation is reasonable
    float calculated_power = getLastPowerReading();
    TEST_ASSERT_TRUE(calculated_power > -10000 && calculated_power < 10000);
}

void test_load_control_integration() {
    // Set up conditions for load activation
    setPowerReading(1500);  // 1.5kW excess
    updateLoadControl();

    // Verify load was activated
    TEST_ASSERT_EQUAL(LoadState::ON, getLoadState(0));

    // Set up conditions for load deactivation
    setPowerReading(-500);  // Importing power
    updateLoadControl();

    // Verify load was deactivated
    TEST_ASSERT_EQUAL(LoadState::OFF, getLoadState(0));
}
```

### Serial Communication Tests
```cpp
void test_serial_output_format() {
    // Test human-readable output
    setSerialOutputType(SerialOutputType::HumanReadable);
    String output = captureSerialOutput();

    TEST_ASSERT_TRUE(output.indexOf("Power:") >= 0);
    TEST_ASSERT_TRUE(output.indexOf("Load:") >= 0);

    // Test EmonTX format
    setSerialOutputType(SerialOutputType::EmonTX);
    output = captureSerialOutput();

    // Should be comma-separated values
    TEST_ASSERT_TRUE(output.indexOf(",") >= 0);
}
```

## Hardware-in-the-Loop Testing

### Timing Validation Tests
```cpp
// test/test_timing.cpp
void test_adc_interrupt_timing() {
    // Measure ISR execution time
    volatile uint32_t start_time, end_time;

    // Setup test interrupt
    attachInterrupt(digitalPinToInterrupt(2), []() {
        start_time = micros();
        // Simulate ISR work
        processAdcSample();
        end_time = micros();
    }, RISING);

    // Trigger interrupt
    triggerTestInterrupt();

    // Verify timing constraint
    uint32_t execution_time = end_time - start_time;
    TEST_ASSERT_LESS_THAN(50, execution_time);  // <50μs requirement
}

void test_main_loop_timing() {
    uint32_t loop_start = millis();

    // Run main loop iteration
    loop();

    uint32_t loop_time = millis() - loop_start;
    TEST_ASSERT_LESS_THAN(10, loop_time);  // <10ms per iteration
}
```

### Power Measurement Accuracy Tests
```cpp
void test_power_measurement_accuracy() {
    // Apply known test load
    applyTestLoad(1000);  // 1kW resistive load
    delay(1000);  // Settle time

    // Read calculated power
    float measured_power = getCurrentPower();

    // Verify accuracy within 1%
    TEST_ASSERT_FLOAT_WITHIN(10.0, 1000.0, measured_power);
}

void test_phase_balance_measurement() {
    // Apply balanced 3-phase load
    applyBalancedLoad(3000);  // 1kW per phase
    delay(1000);

    // Verify phase measurements
    for (uint8_t phase = 0; phase < 3; phase++) {
        float phase_power = getPhasePower(phase);
        TEST_ASSERT_FLOAT_WITHIN(50.0, 1000.0, phase_power);
    }
}
```

## Performance Testing

### Memory Usage Tests
```cpp
// test/test_performance.cpp
void test_stack_usage() {
    // Measure available stack
    uint16_t stack_before = getStackSize();

    // Exercise worst-case call path
    worstCaseFunction();

    uint16_t stack_after = getStackSize();
    uint16_t stack_used = stack_before - stack_after;

    // Verify stack usage is reasonable
    TEST_ASSERT_LESS_THAN(200, stack_used);  // <200 bytes
}

void test_heap_usage() {
    // Verify no dynamic allocation
    uint16_t heap_before = freeMemory();

    // Run system for extended period
    for (int i = 0; i < 1000; i++) {
        loop();
        delay(1);
    }

    uint16_t heap_after = freeMemory();

    // Heap should be unchanged (no leaks)
    TEST_ASSERT_EQUAL(heap_before, heap_after);
}
```

### Real-Time Performance Tests
```cpp
void test_interrupt_jitter() {
    const uint16_t samples = 1000;
    uint32_t interrupt_times[samples];

    // Measure interrupt timing
    for (uint16_t i = 0; i < samples; i++) {
        interrupt_times[i] = measureInterruptPeriod();
    }

    // Calculate jitter
    uint32_t min_period = *min_element(interrupt_times, interrupt_times + samples);
    uint32_t max_period = *max_element(interrupt_times, interrupt_times + samples);
    uint32_t jitter = max_period - min_period;

    // Verify jitter is acceptable
    TEST_ASSERT_LESS_THAN(5, jitter);  // <5μs jitter
}
```

## Safety Testing

### Fault Injection Tests
```cpp
// test/test_safety.cpp
void test_adc_fault_handling() {
    // Simulate ADC failure
    simulateAdcFault();

    // System should detect fault and shut down safely
    TEST_ASSERT_TRUE(systemInSafeState());
    TEST_ASSERT_EQUAL(LoadState::OFF, getLoadState(0));
}

void test_overvoltage_protection() {
    // Simulate overvoltage condition
    simulateVoltage(280.0);  // 20% overvoltage

    // System should shut down loads
    updateProtectionSystems();

    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; i++) {
        TEST_ASSERT_EQUAL(LoadState::OFF, getLoadState(i));
    }
}

void test_overcurrent_protection() {
    // Simulate overcurrent on load
    simulateLoadCurrent(0, 50.0);  // 50A overcurrent

    updateProtectionSystems();

    // Affected load should be shut down
    TEST_ASSERT_EQUAL(LoadState::OFF, getLoadState(0));
}
```

### Watchdog Testing
```cpp
void test_watchdog_functionality() {
    // Enable watchdog
    enableWatchdog(1000);  // 1 second timeout

    // Normal operation should reset watchdog
    for (int i = 0; i < 10; i++) {
        loop();
        delay(50);
    }

    // System should still be running
    TEST_ASSERT_TRUE(systemRunning());

    // Simulate hang (don't reset watchdog)
    delay(2000);

    // System should have reset
    TEST_ASSERT_TRUE(wasWatchdogReset());
}
```

## Test Automation

### Continuous Integration Setup
```yaml
# .github/workflows/test.yml
name: Test Suite

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup PlatformIO
        run: pip install platformio
      - name: Run unit tests
        run: pio test -e native

  embedded-tests:
    runs-on: self-hosted
    steps:
      - uses: actions/checkout@v2
      - name: Flash and test
        run: |
          pio run -t upload
          pio test -e embedded
```

### Test Data Collection
```cpp
// Automated test result logging
struct TestResult {
  const char* test_name;
  bool passed;
  float measured_value;
  float expected_value;
  float tolerance;
  uint32_t execution_time_us;
};

class TestLogger {
public:
  void logResult(const TestResult& result) {
    Serial.print("TEST,");
    Serial.print(result.test_name);
    Serial.print(",");
    Serial.print(result.passed ? "PASS" : "FAIL");
    Serial.print(",");
    Serial.print(result.measured_value);
    Serial.print(",");
    Serial.print(result.expected_value);
    Serial.print(",");
    Serial.println(result.execution_time_us);
  }
};
```

## Test Execution Guidelines

### Pre-Test Checklist
- [ ] Hardware properly connected and calibrated
- [ ] Safety equipment active (RCD, isolation)
- [ ] Test environment documented
- [ ] Expected results defined
- [ ] Safety abort procedures ready

### Test Execution Sequence
1. **Power-up tests** - Basic functionality
2. **Calibration verification** - Measurement accuracy
3. **Normal operation tests** - Typical use cases
4. **Stress tests** - Extreme conditions
5. **Safety tests** - Fault conditions
6. **Long-term tests** - Stability and reliability

### Post-Test Analysis
```python
# Python script for test result analysis
import pandas as pd
import matplotlib.pyplot as plt

def analyze_test_results(test_log):
    df = pd.read_csv(test_log)

    # Calculate pass rate
    pass_rate = df['passed'].mean() * 100
    print(f"Overall pass rate: {pass_rate:.1f}%")

    # Plot performance metrics
    plt.figure(figsize=(12, 6))
    plt.subplot(1, 2, 1)
    plt.hist(df['execution_time_us'], bins=20)
    plt.xlabel('Execution Time (μs)')
    plt.title('Test Execution Time Distribution')

    plt.subplot(1, 2, 2)
    accuracy = abs(df['measured_value'] - df['expected_value']) / df['expected_value'] * 100
    plt.hist(accuracy, bins=20)
    plt.xlabel('Accuracy Error (%)')
    plt.title('Measurement Accuracy Distribution')

    plt.tight_layout()
    plt.savefig('test_analysis.png')
```

## Debugging Test Failures

### Common Test Failure Modes
1. **Timing violations** - ISR too slow, jitter too high
2. **Accuracy errors** - Calibration or calculation issues
3. **Memory issues** - Stack overflow, heap corruption
4. **Hardware faults** - Connection problems, component failures

### Debug Tools and Techniques
```cpp
// Debug assertions for test troubleshooting
#ifdef DEBUG_TESTS
  #define TEST_DEBUG(x) Serial.println(x)
  #define TEST_ASSERT_DEBUG(condition, message) \
    if (!(condition)) { \
      Serial.print("ASSERTION FAILED: "); \
      Serial.println(message); \
      while(1); \
    }
#else
  #define TEST_DEBUG(x)
  #define TEST_ASSERT_DEBUG(condition, message)
#endif
```

### Test Result Documentation
Maintain detailed test records including:
- Hardware configuration used
- Environmental conditions
- Software version tested
- Test results and any failures
- Analysis and corrective actions

This comprehensive testing approach ensures the PVRouter system operates safely and reliably in real-world conditions.
