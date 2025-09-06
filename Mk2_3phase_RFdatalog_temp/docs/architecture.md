# Software Architecture

This document describes the software architecture of the PVRouter system, including module organization, data flow, and key design decisions.

## System Overview

The PVRouter is designed as a real-time embedded system with the following core responsibilities:

1. **Real-time power measurement** across three phases
2. **Intelligent load switching** based on surplus energy
3. **Temperature monitoring** for load management
4. **Remote control and monitoring** capabilities
5. **Data logging and telemetry** for analysis

## Architecture Diagram

```
                    PVRouter Software Architecture
                    
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌───────────────┐  ┌──────────────┐  ┌─────────────────────┐   │
│  │   Main Loop   │  │ Temperature  │  │    Telemetry &      │   │
│  │  (main.cpp)   │  │   Sensing    │  │   Data Logging      │   │
│  │               │  │(utils_temp.h)│  │  (various utils)    │   │
│  └───────────────┘  └──────────────┘  └─────────────────────┘   │
│          │                   │                    │             │
│          ▼                   ▼                    ▼             │
├─────────────────────────────────────────────────────────────────┤
│                     Processing Engine Layer                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              Core Processing Engine                      │   │
│  │                (processing.cpp/.h)                       │   │
│  │                                                          │   │
│  │  • Real power calculation (V × I × cos φ)                │   │
│  │  • Energy bucket management                              │   │
│  │  • Load priority algorithms                              │   │
│  │  • Polarity detection & confirmation                     │   │
│  │  • Data aggregation for logging                          │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
├─────────────────────────────────────────────────────────────────┤
│                    Hardware Abstraction Layer                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────┐ │
│  │   ADC ISR   │  │  Pin Control │  │      OneWire Bus        │ │
│  │ (interrupt) │  │(utils_pins.h)│  │   (utils_temp.h)        │ │
│  │             │  │              │  │                         │ │
│  │ • Sample V  │  │ • TRIAC      │  │ • DS18B20 sensors       │ │
│  │ • Sample I  │  │ • Relay      │  │ • Temperature reading   │ │
│  │ • Timing    │  │ • GPIO       │  │ • CRC validation        │ │
│  └─────────────┘  └──────────────┘  └─────────────────────────┘ │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                       Configuration Layer                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────┐ │
│  │   config.h  │  │calibration.h │  │     validation.h        │ │
│  │             │  │              │  │                         │ │
│  │ • Features  │  │ • Cal values │  │ • Compile-time checks   │ │
│  │ • Pin maps  │  │ • Power/V/I  │  │ • Static assertions     │ │
│  │ • Behavior  │  │ • Phase cal  │  │ • Safety validation     │ │
│  └─────────────┘  └──────────────┘  └─────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Key Modules

### 1. Processing Engine (`processing.cpp/.h`)
**Core responsibility**: Real-time power measurement and load control

**Key functions**:
- `processCurrentRawSample()` - Process current sensor ADC samples
- `processVoltageRawSample()` - Process voltage sensor ADC samples  
- `processRawSamples()` - Main ADC sample processing
- `updatePhysicalLoadStates()` - Control TRIAC outputs
- `processDataLogging()` - Aggregate data for logging

**Design patterns**:
- Interrupt Service Routine (ISR) for real-time processing
- Energy bucket algorithm for load switching decisions
- State machine for polarity detection

### 2. Configuration System (`config.h`, `validation.h`)
**Core responsibility**: Compile-time configuration and validation

**Key features**:
- Template-based configuration
- Compile-time validation with `static_assert`
- Type-safe pin assignments
- Feature toggles with zero runtime overhead

**Example validation**:
```cpp
static_assert(check_pins(), "******** Duplicate pin definition ! ********");
static_assert(NO_OF_DUMPLOADS > 0, "Number of dump loads must be greater than 0");
```

### 3. Temperature Sensing (`utils_temp.h`)
**Core responsibility**: DS18B20 temperature sensor management

**Key features**:
- Template-based sensor configuration
- Compile-time bounds checking: `readTemperature<INDEX>()`
- Runtime bounds checking: `readTemperature(index)`
- CRC validation and error handling

### 4. Utility Modules
- **`utils_pins.h`**: Direct port manipulation for performance
- **`utils_relay.h`**: Relay-based load control with timing
- **`utils_dualtariff.h`**: Off-peak period management
- **`utils_rf.h`**: RF communication support

## Data Flow

### 1. ADC Sampling (9.6 kHz)
```
ADC Interrupt → processRawSamples() → Update running calculations
     ↓
V/I samples → Real power calculation → Energy bucket update
     ↓
Load switching decisions → TRIAC control → Physical load changes
```

### 2. Main Loop (Background)
```
Temperature reading → Sensor validation → Data storage
     ↓
User input processing → Control logic → State updates
     ↓  
Data logging → Serial output → Telemetry transmission
```

## Memory Architecture

### Flash Memory Usage (27.8% on Arduino Uno)
- **Code**: ~7.5KB (Processing engine, utility functions)
- **Constants**: ~1.0KB (Calibration, configuration tables)
- **Strings**: ~0.5KB (Debug messages, validation text)

### RAM Usage (26.3% on Arduino Uno)
- **Global variables**: ~300 bytes (Shared state, accumulators)
- **Local variables**: ~150 bytes (Stack usage)
- **Buffers**: ~88 bytes (Temperature scratchpad, arrays)

### Key Memory Optimizations
1. **No dynamic allocation** - All memory statically allocated
2. **Compile-time constants** - `constexpr` eliminates runtime overhead
3. **Efficient data types** - `uint16_t` where `uint32_t` not needed
4. **Shared buffers** - Reuse static buffers for temporary data

## Real-Time Guarantees

### Interrupt Service Routine (ISR)
- **Maximum execution time**: <50μs
- **Frequency**: 9.6 kHz (every 104μs)
- **Priority**: Highest (hardware ADC interrupt)
- **Memory**: Stack-based, no heap allocation

### Critical Timing Requirements
1. **ADC processing**: Must complete within 104μs
2. **TRIAC switching**: Synchronized to mains zero-crossing
3. **Temperature conversion**: 750ms maximum (DS18B20 limit)
4. **Serial communication**: Non-blocking, buffered output

## Error Handling Strategy

### Compile-Time Safety
- Static assertions for configuration validation
- Template bounds checking for array access
- Type safety with strong typing (enums, constexpr)

### Runtime Error Handling
- CRC validation for sensor data
- Range checking for temperature readings
- Polarity confirmation for power calculations
- Watchdog monitoring for system health

### Graceful Degradation
- Continue operation if temperature sensors fail
- Disable loads if voltage/current sensors malfunction
- Maintain basic power measurement if advanced features fail

## Performance Characteristics

### Computational Efficiency
- **Real power calculation**: 16-bit integer arithmetic
- **Phase correction**: Linear interpolation
- **Load switching**: Energy bucket algorithm (O(1) complexity)

### Memory Efficiency
- **Zero-copy data flow**: Direct ADC → calculation → output
- **Minimal buffering**: Only essential data stored
- **Compile-time optimization**: Templates eliminate runtime overhead

### Power Efficiency
- **Minimal sleep usage**: Interrupt-driven design
- **Efficient algorithms**: Optimized for AVR instruction set
- **Low-power modes**: Available for battery applications

## Extension Points

### Adding New Features
1. **New sensor types**: Extend template system in utils_temp.h
2. **Additional loads**: Modify NO_OF_DUMPLOADS and arrays
3. **Communication protocols**: Add new utils_*.h modules
4. **Control algorithms**: Extend processing engine

### Hardware Adaptations  
1. **Different microcontrollers**: Modify pin definitions
2. **Additional phases**: Extend NO_OF_PHASES constant
3. **Different ADC resolution**: Adjust calibration factors
4. **Custom hardware**: Implement new utils modules

This architecture provides a solid foundation for embedded real-time power management while maintaining extensibility and safety.