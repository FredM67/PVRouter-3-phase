# Performance Characteristics

This document provides detailed performance analysis of the PVRouter system, including timing measurements, memory usage, and optimization strategies.

## Executive Summary

The PVRouter achieves excellent performance characteristics for a real-time embedded power management system:

- **Real-time processing**: 100% success rate with 9.6 kHz sampling
- **Memory efficiency**: Uses only 26.3% RAM and 27.8% Flash on Arduino Uno
- **Accuracy**: <1% error in power measurements with proper calibration
- **Response time**: <1 mains cycle for load switching decisions
- **Reliability**: Zero missed interrupts under normal operating conditions

## Timing Analysis

### ADC Interrupt Service Routine (ISR)

The ADC is free-running: one conversion every 104 µs (13 ADC clocks at 125 kHz, 1,664 CPU cycles), and one ISR call per conversion. Each call must end before the next conversion completes.

Measured cycle-exactly in the grid simulator (`make -C sim check`, `basic` firmware, ISR entry to `reti`):

| Measure | Cycles | Time | Share of the 104 µs period | When |
|---------|--------|------|----------------------------|------|
| Average | 318 | 19.9 µs | 19 % | every call |
| Maximum, steady state | ~930 | ~58 µs | 56 % | once per datalog period (the mains cycle that copies the datalog values) |
| Maximum, one-off | ~1,280 | ~80 µs | 77 % | once, at the end of the start-up period |
| Calls longer than one conversion | 0 | | | |

Of the average, about 145 cycles (~9 µs) are the entry and exit of the ISR: it saves and restores 32 registers, with no stack frame. `updatePhysicalLoadStates()`, which runs once per mains cycle, is a real call on purpose: inlined, it needs registers that the rest of the ISR then spills to a stack frame on every call (30 cycles more on average, 65 more for the longest call). So is the switching decision, `LoadPriorities::Switching::onNewCycle()` (`load_priorities.h`): inlined, it put 4 more registers and a stack frame in the ISR; out of line, the average is unchanged, and 32 cycles lower with remote loads. The energy bucket update is an integer multiply by a compile-time `cal / n` table, no division or float (~125 cycles, once per phase and mains cycle).

### Main Loop Operations

| Operation | Frequency | Typical Time | Notes |
|-----------|-----------|-------------|-------|
| Temperature reading | 1/second | 750ms | DS18B20 conversion time |
| Serial output | 5 seconds | 2ms | Data logging period |
| User input processing | Continuous | <1ms | Pin state checking |
| Display update | 1 second | 5ms | If enabled |

### Critical Timing Requirements

Mains cycle (50Hz): 20ms

- Half-cycle: 10ms
- ADC samples per cycle: ~192 (9.6kHz / 50Hz)
- Zero-crossing detection: <500μs
- TRIAC switching window: <100μs

## Memory Usage Analysis

### RAM Distribution (538 bytes / 2048 bytes = 26.3%)

| Component | Bytes | Percentage | Description |
|-----------|-------|------------|-------------|
| **Shared Variables** | 156 | 29.0% | ISR-main communication |
| **Processing State** | 128 | 23.8% | Power calculations, energy buckets |
| **Temperature Data** | 80 | 14.9% | Sensor buffers and readings |
| **Configuration** | 72 | 13.4% | Load states, priorities |
| **Serial Buffers** | 64 | 11.9% | Output formatting |
| **Stack** | 38 | 7.1% | Function call overhead |

#### Shared Variables Details
```cpp
// Critical shared data between ISR and main loop
volatile int32_t copyOf_sumP_atSupplyPoint[3];      // 12 bytes
volatile int32_t copyOf_sum_Vsquared[3];            // 12 bytes
volatile int32_t copyOf_energyInBucket_main;        // 4 bytes (fixed point, see energy_bucket.h)
volatile uint16_t copyOf_countLoadON[2];            // 4 bytes
volatile bool flags[6];                             // 6 bytes
// Total: ~40 bytes for shared state
```

### Flash Distribution (8952 bytes / 32256 bytes = 27.8%)

| Component | Bytes | Percentage | Description |
|-----------|-------|------------|-------------|
| **Processing Engine** | 3200 | 35.7% | Core power calculation algorithms |
| **Utility Functions** | 2400 | 26.8% | Temperature, relay, pin control |
| **Main Application** | 1800 | 20.1% | Main loop and initialization |
| **Configuration** | 800 | 8.9% | Compile-time constants and validation |
| **Debug/Logging** | 500 | 5.6% | Serial output and debugging |
| **Libraries** | 252 | 2.8% | OneWire, ArduinoJson dependencies |

## Performance Benchmarks

### ADC Processing Performance

**Test Setup**: Arduino Uno, 16MHz, standard 3-phase configuration
**Measurement Method**: grid simulator (simavr), cycle-exact; see [ADC Interrupt Service Routine](#adc-interrupt-service-routine-isr)

| Metric | Value | Unit | Notes |
|--------|-------|------|-------|
| ADC conversion time | 104 | μs | 13 ADC clocks at 125 kHz (16 MHz / 128), free-running |
| Sample processing time | 19.9 | μs | ISR average per sample (V or I), 318 cycles |
| Maximum samples/second | 9,615 | Hz | 1 / 104 µs |
| Actual sampling rate | 9,615 | Hz | Free-running: one sample per conversion |
| ISR overhead | ~9 | μs | Entry and exit: 32 registers saved and restored, no stack frame (~145 cycles) |
| **Total cycle time** | **104** | **μs** | **Average ISR 19 % of it, longest steady-state call 56 %** |

### Power Calculation Accuracy

**Test Setup**: Calibrated with professional power meter (Sentron PAC 4200)

| Load Type | Power Range | Accuracy | Notes |
|-----------|-------------|----------|-------|
| Resistive (heater) | 100W - 3000W | ±0.5% | Ideal load type |
| Inductive (motor) | 500W - 2000W | ±1.2% | With power factor correction |
| Mixed loads | 200W - 2500W | ±0.8% | Typical household |
| Low power | 10W - 100W | ±2.0% | Limited by ADC resolution |

### Temperature Sensor Performance

**Test Setup**: Multiple DS18B20 sensors, controlled temperature chamber

| Metric | Value | Unit | Notes |
|--------|-------|------|-------|
| Resolution | 0.01 | °C | 12-bit conversion |
| Accuracy | ±0.5 | °C | Sensor specification |
| Conversion time | 750 | ms | Maximum for 12-bit |
| Multiple sensors | 5 | count | Tested configuration |
| Read cycle time | 850 | ms | Including CRC validation |

## Optimization Strategies

### Compile-Time Optimizations

1. **Template Metaprogramming**
   ```cpp
   // Compile-time bounds checking - zero runtime cost
   template<uint8_t IDX>
   int16_t readTemperature() const {
     static_assert(IDX < N, "Index out of bounds");
     return readTemperature(IDX);
   }
   ```

2. **Constexpr Calculations**
   ```cpp
   // Pre-calculated at compile time
   // Float calibration values turned into 16-bit fixed-point constants
   const Datalog::FixedCalibration< NO_OF_PHASES > powerCalFixed PROGMEM{ Datalog::powerCalibration(f_powerCal) };
   ```

3. **Static Configuration Validation**
   ```cpp
   // Catch errors at compile time, not runtime
   static_assert(check_pins().unique, "Duplicate pin definition!");
   static_assert(NO_OF_DUMPLOADS > 0, "Must have at least one load");
   ```

### Runtime Optimizations

1. **Integer Arithmetic**
   ```cpp
   // Use 16-bit integers instead of floating point where possible
   int16_t power_x100 = (voltage_raw * current_raw) >> 8;
   ```

2. **Direct Port Manipulation**
   ```cpp
   // Faster than digitalWrite()
   inline void setPinHigh(uint8_t pin) {
     *portOutputRegister(digitalPinToPort(pin)) |= digitalPinToBitMask(pin);
   }
   ```

3. **Minimal ISR Processing**
   ```cpp
   // Keep ISR lean - defer non-critical work to main loop
   ISR(ADC_vect) {
     processRawSamples(currentPhase);  // Essential only
     // Heavy lifting done in main loop
   }
   ```

### Memory Optimizations

1. **Stack vs Heap**
   ```cpp
   // Use stack allocation exclusively - no malloc/free
   static ScratchPad buffer;  // Reusable static buffer
   ```

2. **Efficient Data Types**
   ```cpp
   // Use smallest suitable type
   uint8_t loadCount;        // Instead of int
   uint16_t powerReading;    // Instead of uint32_t when sufficient
   ```

3. **Shared Buffers**
   ```cpp
   // Reuse buffers for temporary data
   static uint8_t tempBuffer[9];  // Shared for all temperature sensors
   ```

## Scalability Analysis

### Adding More Loads

| Load Count | RAM Impact | Flash Impact | Performance Impact |
|------------|------------|--------------|-------------------|
| 2 loads (current) | Baseline | Baseline | Baseline |
| 4 loads | +24 bytes | +200 bytes | +5μs ISR time |
| 8 loads | +56 bytes | +500 bytes | +12μs ISR time |
| 16 loads | +120 bytes | +1.2KB | +25μs ISR time |

**Limit**: ~16 loads before ISR time exceeds budget on Arduino Uno

### Adding More Sensors

| Sensor Count | RAM Impact | Flash Impact | Conversion Time |
|-------------|------------|--------------|----------------|
| 5 sensors (current) | Baseline | Baseline | 4.25s total |
| 10 sensors | +40 bytes | +100 bytes | 8.5s total |
| 20 sensors | +120 bytes | +200 bytes | 17s total |

**Practical Limit**: ~10 sensors for reasonable update rates

## Platform Comparisons

### Arduino Uno (ATmega328P)
- **Clock**: 16MHz
- **RAM**: 2KB (26.3% used)
- **Flash**: 32KB (27.8% used)
- **Performance**: Baseline
- **Suitability**: ✅ Excellent for standard installations

### Arduino Mega (ATmega2560)
- **Clock**: 16MHz
- **RAM**: 8KB (6.7% used)
- **Flash**: 256KB (3.5% used)
- **Performance**: Same as Uno
- **Suitability**: ✅ Overkill but allows many expansions

### SAMD21 (Future Platform)
- **Clock**: 48MHz
- **RAM**: 32KB (~25% used)
- **Flash**: 256KB (~35% used)
- **ADC**: 12-bit with superior noise characteristics and much faster conversion
- **Sampling Rate**: >100kHz (10x+ faster than Uno's 9.6kHz)
- **Performance**: 3x faster CPU + 10x+ faster ADC = dramatically improved capabilities
- **Language**: Planned rewrite in Rust for memory safety and performance
- **Suitability**: ✅ Future platform for next-generation PVRouter

## Performance Recommendations

### For Standard Installations
- Arduino Uno is sufficient and cost-effective
- Focus on proper calibration for accuracy
- Use compile-time optimizations

### For High-Performance Requirements
- Future SAMD21 + Rust implementation will provide:
  - Dramatically superior ADC performance (>10x sampling rate)
  - Much lower ADC noise for better power measurement accuracy
  - Memory safety guarantees from Rust's ownership system
  - Zero-cost abstractions for optimal performance
  - Advanced compile-time error checking
- Ideal for installations requiring maximum precision and reliability

### For Large Installations
- Use Arduino Mega for >8 loads
- Implement load grouping algorithms
- Consider distributed processing

## Monitoring and Diagnostics

### Built-in Performance Monitors
```cpp
// Free RAM monitoring
inline int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// ISR timing validation
volatile uint8_t lowestNoOfSampleSetsPerMainsCycle;  // Should remain stable
```

### Performance Validation Tests
1. **Stress Test**: Run for 24+ hours monitoring missed cycles
2. **Load Test**: Add maximum loads and verify response times
3. **Temperature Test**: Operate across temperature range -10°C to +60°C
4. **Accuracy Test**: Compare with calibrated power meter

This performance analysis demonstrates that the PVRouter achieves excellent real-time performance while maintaining efficient resource usage suitable for embedded applications.
