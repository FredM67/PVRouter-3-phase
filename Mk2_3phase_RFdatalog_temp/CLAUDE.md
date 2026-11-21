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

- `config.h` - User configuration (features, pins, loads) - **primary file users edit**. Settings only: it includes `config_types.h` and nothing else
- `config_types.h` - What config.h is written with: settings types, `LOAD()`/`RELAY()`/`ALL_...()` target codes, the boost windows. No setting, no Arduino
- `config_derived.h` - What the code includes (never config.h): derived constants, `remoteLoads`, and the objects built from the settings (`relayDiverter`, `overrideTable`, `boostSchedule`, `tempSensors`)
- `calibration.h` - Power calibration values - **users edit for their hardware**
- `processing.cpp/.h` - Core real-time processing engine
- `validation.h` - ~50+ compile-time safety assertions
- `utils_relay.h` - Relay control: pins and printing; the decisions, with EWMA filtering, are in `relay_engine.h` (Arduino-free, native tests)
- `utils_temp.h` - DS18B20 temperature sensor management

### Real-Time Constraints

- ADC ISR runs at **9.6 kHz** (every 104μs, 1,664 CPU cycles)
- Every ISR call must end within one ADC conversion (104μs); measured in the grid simulator: **318 cycles (~20μs) on average**, ~930 cycles (~58μs) for the longest steady-state call (once per datalog period), ~1,280 cycles once at the end of start-up (docs/performance.md)
- No blocking calls, no dynamic allocation in ISR
- Volatile shared variables for ISR-main loop communication

## Critical Development Rules

### Memory Constraints (Arduino Uno: 32KB Flash, 2KB RAM)

- **No malloc/dynamic allocation** - all memory statically allocated
- Use `constexpr` for compile-time constants

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
    ├── test_dual_tariff/
    ├── test_ewma_avg/
    ├── test_load_priorities/
    ├── test_remote_loads/
    └── test_utils_override/
```

## Dependencies

- **ArduinoJson 6.x** (NOT v7): only for the embedded test `test_serial_output`, the firmware writes its JSON itself
- **OneWire 2.3.8+** (for DS18B20 sensors)
- **JeeLib** (optional, for RF support)
