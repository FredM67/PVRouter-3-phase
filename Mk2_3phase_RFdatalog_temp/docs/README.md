# Architecture Documentation

This directory contains technical documentation about the PVRouter's software architecture, performance characteristics, and design decisions.

## Documents

### Core Architecture
- **[Software Architecture](architecture.md)** - Overall system design and module relationships
- **[Memory Layout](memory-layout.md)** - RAM and Flash usage analysis
- **[Performance Characteristics](performance.md)** - Timing analysis and benchmarks

### Technical Details
- **[Interrupt System](interrupts.md)** - ADC processing and ISR design
- **[Power Calculation Engine](power-calculation.md)** - Real power measurement algorithms
- **[Configuration System](configuration.md)** - Compile-time validation and type safety

### Development Guides
- **[Contributing Guide](contributing.md)** - How to contribute to the project
- **[Testing Guide](testing.md)** - Testing methodologies and tools
- **[Debugging Guide](debugging.md)** - Troubleshooting and diagnostic techniques

## Quick Reference

### System Overview
```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   ADC Inputs    │───▶│  Processing      │───▶│  Load Control   │
│  (V/I sensors)  │    │     Engine       │    │   (TRIACs)      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐
                       │   Data Logging   │
                       │  & Telemetry     │
                       └──────────────────┘
```

### Key Performance Metrics
- **ADC Sampling**: ~104μs per cycle (9.6 kHz)
- **Memory Usage**: 26.3% RAM, 27.8% Flash (Arduino Uno)
- **Real-time Processing**: Zero missed cycles
- **Temperature Resolution**: 0.01°C
- **Power Accuracy**: <1% with proper calibration

### Safety Features
- ✅ Compile-time configuration validation
- ✅ Bounds checking for all array accesses
- ✅ Watchdog monitoring
- ✅ Polarity detection and confirmation
- ✅ Temperature range validation
- ✅ CRC verification for sensor data

For detailed information, see the individual documentation files.