# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PVRouter-3-phase is a real-time embedded system for intelligent 3-phase solar power routing on Arduino Uno (ATmega328P). It diverts excess photovoltaic energy to loads (water heaters, etc.) via TRIAC/relay control. This is **safety-critical power electronics code** with strict real-time constraints.

## Build Commands

```bash
# Build (default configuration)
pio run

# Build specific environment
pio run -e basic           # Production build
pio run -e basic_debug     # Debug build with symbols
pio run -e rf              # RF module support (RFM69)
pio run -e emonesp         # EmonESP integration

# Run tests
pio test -e native         # Cross-platform unit tests
pio test -e uno            # Embedded tests (requires hardware)

# Upload to Arduino
pio run -t upload

# Code formatting check
clang-format --dry-run --Werror *.cpp *.h

# Static analysis
pio check
```

## Architecture

### Layer Structure

1. **Application Layer** (`main.cpp`) - Main loop, load priority management, dual tariff forcing
2. **Processing Engine** (`processing.cpp`) - 9.6 kHz ADC ISR, real power calculation (V × I × cos φ), energy bucket algorithm
3. **Hardware Abstraction** (`utils_*.h`) - Pin control, relay timing, OneWire temperature sensors
4. **Configuration Layer** (`config.h`, `validation.h`) - Compile-time configuration with `static_assert` validation

### Key Files

- `config.h` - User configuration (features, pins, loads) - **primary file users edit**
- `calibration.h` - Power calibration values - **users edit for their hardware**
- `processing.cpp/.h` - Core real-time processing engine
- `validation.h` - ~50+ compile-time safety assertions
- `utils_relay.h` - Relay control with EWMA filtering
- `utils_temp.h` - DS18B20 temperature sensor management

### Real-Time Constraints

- ADC ISR runs at **9.6 kHz** (every 104μs)
- ISR must complete in **<50μs**
- No blocking calls, no dynamic allocation in ISR
- Volatile shared variables for ISR-main loop communication

## Critical Development Rules

### Memory Constraints (Arduino Uno: 32KB Flash, 2KB RAM)

- **No malloc/dynamic allocation** - all memory statically allocated
- Use `constexpr` for compile-time constants
- ArduinoJson must be **v6.x** (v7 too heavy for ATmega328P)

### Code Style

- C++17 with GNU extensions (`-std=gnu++17`)
- `constexpr` over `#define` for constants
- `static_cast<>` over C-style casts
- `enum class` for type-safe enumerations
- No magic numbers - use named constants
- camelCase for functions/variables, UPPER_CASE for constants, PascalCase for types

### Safety Requirements

- All load control must include safety validation
- Never modify power routing logic without understanding the energy bucket algorithm
- Test timing on real hardware with oscilloscope for ISR changes
- Compile-time validation via `static_assert` is preferred over runtime checks

## Git Workflow

- Two-branch model: `main` (stable) + `dev` (development)
- PRs target `dev` branch
- Conventional commits: `fix:`, `feat:`, `docs:`, `chore:`

## Test Structure

```
test/
├── embedded/     # Hardware-dependent tests (run on Arduino)
│   ├── test_teleinfo/
│   ├── test_utils_relay/
│   └── test_utils_pins/
└── native/       # Cross-platform tests (run on host)
    ├── test_ewma_avg/
    ├── test_cloud_patterns/
    └── test_negative_threshold/
```

## Dependencies

- **ArduinoJson 6.x** (NOT v7)
- **OneWire 2.3.8+** (for DS18B20 sensors)
- **JeeLib** (optional, for RF support)
