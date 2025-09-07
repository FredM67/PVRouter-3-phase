# Memory Layout Analysis

This document provides a detailed analysis of RAM and Flash memory usage in the PVRouter firmware.

## Memory Architecture Overview

The PVRouter firmware is designed to operate efficiently within the constraints of microcontroller memory:

- **Target Platform**: Arduino Uno (ATmega328P)
- **Flash Memory**: 32KB (program storage)
- **SRAM**: 2KB (dynamic variables)
- **EEPROM**: 1KB (not currently used)

## Flash Memory Usage

### Program Size Analysis
Based on compilation with Arduino IDE and PlatformIO:

```
Sketch uses 8,956 bytes (27.8%) of program storage space.
Maximum is 32,256 bytes.
```

### Memory Distribution

| Component | Flash Usage | Percentage | Description |
|-----------|-------------|------------|-------------|
| Core Logic | ~6,500 bytes | 20.1% | Main processing algorithms |
| Libraries | ~1,800 bytes | 5.6% | OneWire, DallasTemperature |
| Configuration | ~400 bytes | 1.2% | Compile-time constants |
| String Literals | ~256 bytes | 0.8% | Debug messages, serial output |

### Optimization Techniques
- **Compile-time constants**: All configuration uses `constexpr`
- **Template metaprogramming**: Bounds checking at compile time
- **Minimal libraries**: Only essential libraries included
- **String optimization**: F() macro for PROGMEM strings

## SRAM Usage

### Dynamic Memory Analysis
```
Global variables use 538 bytes (26.3%) of dynamic memory.
Maximum is 2,048 bytes.
```

### Critical Data Structures

#### ADC Sample Buffers
```cpp
// Raw ADC samples for each phase
int16_t sampleVminusDC[NO_OF_PHASES];     // 6 bytes
int16_t sampleIminusDC[NO_OF_PHASES];     // 6 bytes
long cumVdeltasThisCycle[NO_OF_PHASES];   // 12 bytes
long cumIdeltasThisCycle[NO_OF_PHASES];   // 12 bytes
```

#### Power Calculation Variables
```cpp
// Instantaneous power calculations
long instantPower[NO_OF_PHASES];          // 12 bytes
long sumOfInstPowers;                     // 4 bytes
long requiredExportPerMainsCycle;         // 4 bytes
```

#### Load Control Arrays
```cpp
// Load management state
uint16_t loadPriorityAndState[NO_OF_DUMPLOADS]; // Variable size
float loadPower[NO_OF_DUMPLOADS];              // Variable size
```

### Memory Optimization Strategies

#### Stack Usage Minimization
- Local variables kept minimal in ISR
- Recursive functions avoided
- Large temporary variables avoided

#### Efficient Data Types
- Fixed-width integers (`int16_t`, `uint8_t`) used consistently
- Bitfields for boolean flags where appropriate
- Enums with explicit underlying types

## Memory Safety Features

### Compile-Time Validation
```cpp
static_assert(NO_OF_DUMPLOADS <= 6, "Too many dump loads");
static_assert(NO_OF_PHASES == 3, "Only 3-phase operation supported");
```

### Compile-Time Bounds Checking
Template-based bounds checking for array access:
```cpp
template<uint8_t IDX>
constexpr float readTemperature() {
  static_assert(IDX < TEMPERATURE_SENSORS_COUNT, "Invalid sensor index");
  // Implementation...
}
```

### Stack Overflow Protection
- Watchdog timer enabled in production builds
- ISR kept minimal to prevent stack overflow
- Deep call chains avoided

## Performance Implications

### ADC Interrupt Service Routine
- **Critical path**: Must complete within ~104μs
- **Memory access**: Only essential variables in ISR
- **Stack usage**: Minimal local variables

### Main Loop Efficiency
- **Processing time**: <1ms per main loop iteration
- **Memory allocation**: No dynamic allocation used
- **Cache efficiency**: Sequential memory access patterns

## Memory Monitoring

### Compile-Time Analysis
```bash
# Arduino IDE: Shows memory usage after compilation
# PlatformIO: Use advanced memory analysis
pio run --target size
```

### Runtime Monitoring
```cpp
#ifdef ENABLE_DEBUG
  // Stack usage estimation
  extern uint8_t __heap_start;
  extern void *__brkval;
  int freeMemory() {
    return (int) &freeMemory - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  }
#endif
```

## Configuration Impact on Memory

### Feature Toggles Effect

| Feature | Flash Impact | RAM Impact | Notes |
|---------|-------------|------------|-------|
| Temperature Sensors | +1.2KB | +32 bytes per sensor | OneWire library |
| RF Transmission | +800 bytes | +16 bytes | RFM12B support |
| Dual Tariff | +200 bytes | +8 bytes | Time-based logic |
| Debug Output | +400 bytes | +64 bytes | String literals |

### Scalability Limits

#### Maximum Dump Loads
- **Practical limit**: ~8-10 loads (constrained by available digital pins on Arduino Uno)
- **Memory**: Each load adds ~8 bytes RAM
- **Performance**: Processing time scales linearly

#### Temperature Sensors
- **Practical limit**: 8 sensors
- **Memory per sensor**: ~32 bytes (addresses + readings)
- **Processing overhead**: ~20ms per sensor reading

## Recommendations

### Memory Optimization
1. **Use F() macro** for all debug strings
2. **Minimize global variables** where possible
3. **Use appropriate data types** (uint8_t vs int)
4. **Enable compiler optimizations** (-Os flag)

### Safety Practices
1. **Always validate array bounds** at compile time
2. **Monitor stack usage** during development
3. **Test with all features enabled** to check memory limits
4. **Use static analysis tools** to detect potential issues

### Future Considerations
- **SAMD21 migration**: 32KB RAM, 256KB Flash, superior 12-bit ADC
- **Code modularity**: Prepare for larger memory spaces
- **Dynamic features**: Consider memory pools for expansion