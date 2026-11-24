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
pio test -e native         # Cross-platform unit tests (needs gcc: WSL on Windows)
pio test -e uno_sim        # Embedded tests in simavr, no hardware
pio test -e uno            # Embedded tests on a real board

# Settings that must be refused (from the repo root)
bash ../scripts/ci/compile-fail.sh

# Grid simulator: regulation and ISR timing on a simulated grid (see ../sim/README.md)
make -C ../sim check

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
- `ct_filter.h` - The CT filter (`lpf_gain`, `alpha` of `calibration.h`) in fixed point, Arduino-free (native tests); compiled out with `lpf_gain` 0
- `processing.cpp/.h` - Core real-time processing engine
- `adc_chain.h` - The six ADC conversions (V1, I1, V2, I2, V3, I3), their pins and ADMUX values, and the rule that the ADMUX written in an ISR call applies to the conversion after next; Arduino-free, native tests. `processing.cpp` builds its channel table from it
- `validation.h` - ~50+ compile-time safety assertions
- `utils_relay.h` - Relay control: pins and printing; the decisions, with EWMA filtering, are in `relay_engine.h` (Arduino-free, native tests)
- `utils_temp.h` - DS18B20 temperature sensor management: the bus I/O in short transactions (`startRead()`, `readScratchpad()`, `endRead()`)
- `temp_schedule.h` - Which 1-Wire transaction runs on which mains cycle (Arduino-free, native tests): one per cycle, right after the cycle's heavy ADC interrupt call, so the ADC deadline holds with remote loads and relays

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
- Measure every ISR change in the grid simulator (`ADC ISR max`, `ADMUX delay`, `misfiled samples`); confirm on real hardware when the change is risky
- Compile-time validation via `static_assert` is preferred over runtime checks

## Git Workflow

- `main` (stable) and `dev` (development), both protected by rulesets; topic branches (`feat/…`, `fix/…`, `docs/…`, …), personal branches under `users/`
- PRs target `dev` branch
- Conventional commits: `fix:`, `feat:`, `docs:`, `chore:`
- **Port every change to the single-phase firmware** (PVRouter-1-phase, `Mk2_1phase_RFdatalog_temp`), when it applies there: the two firmwares share their settings, names, files and interfaces. A fix, feature or refactor here gets a matching PR there, unless it touches what only this firmware has (the three phases and their per-phase arrays, `RemoteLoadReceiver`). Each PR says whether it was ported, with a link to the other PR, or why not.

## Test Structure

```
test/
├── embedded/     # Run on the AVR: in simavr (uno_sim) or on a board (uno)
│   ├── test_datalog_math_avr/  test_fastdivision/  test_fastdivision_perf/
│   ├── test_mult_asm/  test_mult_asm_perf/  test_serial_output/
│   └── test_teleinfo/  test_utils_pins/  test_utils_relay/  test_voltage_accumulation/
└── native/       # Run on the host
    ├── test_adc_chain/  test_config_h/  test_ct_filter/  test_datalog_math/  test_dc_offset_filter/
    ├── test_ds18b20/  test_dual_tariff/  test_energy_bucket/  test_ewma_avg/  test_load_priorities/
    ├── test_onewire_search/  test_override_masks/  test_pin_edges/  test_power_rms_comparison/
    └── test_relay_engine/  test_remote_loads/  test_rotation_groups/  test_teleinfo/  test_temp_schedule/  test_utils_override/
```

## Dependencies

- **ArduinoJson 6.x** (NOT v7): only for the embedded test `test_serial_output`, the firmware writes its JSON itself
- **OneWire 2.3.8+** (for DS18B20 sensors)
- **RFM69** by LowPowerLab (`lowpowerlab/RFM69`, for the RF datalog and remote loads)
