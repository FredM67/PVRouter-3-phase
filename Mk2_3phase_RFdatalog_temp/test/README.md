# Test Suite

Unit tests for PVRouter-3-phase firmware.

## Structure

```
test/
├── native/           # Run on host PC (no hardware required)
│   ├── test_datalog_math/
│   ├── test_dc_offset_filter/
│   ├── test_ds18b20/
│   ├── test_dual_tariff/
│   ├── test_energy_bucket/
│   ├── test_ewma_avg/
│   ├── test_load_priorities/
│   ├── test_power_rms_comparison/
│   ├── test_relay_engine/
│   ├── test_remote_loads/
│   └── test_utils_override/
└── embedded/         # Run on Arduino or Wokwi simulator
    ├── test_datalog_math_avr/
    ├── test_fastdivision/
    ├── test_fastdivision_perf/
    ├── test_mult_asm/
    ├── test_mult_asm_perf/
    ├── test_serial_output/
    ├── test_teleinfo/
    ├── test_utils_pins/
    ├── test_utils_relay/
    └── test_voltage_accumulation/
```

## Running Tests

```bash
# All native tests
pio test -e native

# All embedded tests in the simavr simulator (Linux/WSL, no hardware needed)
pio test -e uno_sim

# All embedded tests on a connected Arduino Uno
pio test -e uno

# Specific test suite
pio test -e native -f "*test_ewma_avg*"
pio test -e uno -f "*test_utils_relay*"
```

## Test Suites

### Native Tests

| Test | Purpose |
|------|---------|
| [test_datalog_math](native/test_datalog_math/) | Integer power and RMS voltage of the datalog, checked against the float references |
| [test_dc_offset_filter](native/test_dc_offset_filter/) | DC offset removal filter |
| [test_ds18b20](native/test_ds18b20/) | Checks of a DS18B20 reading: CRC-8, register to hundredths of °C (1/16 °C steps, negatives), range, disconnected probe, the 85 °C power-on value skipped after a jump, invalid readings not sent |
| [test_dual_tariff](native/test_dual_tariff/) | Scheduled boosts of the dual tariff: the windows (`FIRST`, `LAST`, `FROM_START`, `BEFORE_END`, `FOR`), the masks of the targets, rejected windows (starting or ending outside the off-peak period, or of 0 min) |
| [test_energy_bucket](native/test_energy_bucket/) | Integer energy bucket, checked against the float and exact references |
| [test_ewma_avg](native/test_ewma_avg/) | EWMA filter for cloud immunity |
| [test_load_priorities](native/test_load_priorities/) | Load priority state machine: switching order, post-transition period, rotation, overrides |
| [test_override_masks](native/test_override_masks/) | Forced outputs: the targets of the override pins held LOW and of the scheduled boosts in their window (off-peak, temperature threshold, dead probe), joined |
| [test_pin_edges](native/test_pin_edges/) | Edges of the active-LOW input pins: released at startup, one edge per press or release, one previous level per pin, a rotation press during forcing dropped |
| [test_power_rms_comparison](native/test_power_rms_comparison/) | Real power vs. RMS power computation |
| [test_relay_engine](native/test_relay_engine/) | Relay diversion decisions: strict thresholds, minimum ON/OFF times, settle time after any change, order in and out, overrides, diversion OFF, decision on the average |
| [test_remote_loads](native/test_remote_loads/) | Packed load map and per-unit remote payloads |
| [test_utils_override](native/test_utils_override/) | Override pin system (local + remote loads) |

### Embedded Tests

| Test | Purpose |
|------|---------|
| [test_datalog_math_avr](embedded/test_datalog_math_avr/) | Integer datalog arithmetic on the target: the assembly square root at every rounding boundary, and cycle counts against the float code |
| [test_fastdivision](embedded/test_fastdivision/) | AVR assembly fast division (divu10, divmod10) |
| [test_fastdivision_perf](embedded/test_fastdivision_perf/) | Benchmark: fast division against the C `/` and `%` operators |
| [test_mult_asm](embedded/test_mult_asm/) | AVR assembly multiplication optimization |
| [test_mult_asm_perf](embedded/test_mult_asm_perf/) | Benchmark: assembly multiplications, and their cost in a simulated ISR |
| [test_serial_output](embedded/test_serial_output/) | Non-blocking serial output: waits for room in the transmit buffer, decimal printing, Teleinfo frame written in parts |
| [test_teleinfo](embedded/test_teleinfo/) | French Teleinfo protocol parsing |
| [test_utils_pins](embedded/test_utils_pins/) | GPIO pin manipulation |
| [test_utils_relay](embedded/test_utils_relay/) | Relay control logic and timing |
| [test_voltage_accumulation](embedded/test_voltage_accumulation/) | Voltage sample accumulation over a mains cycle |

## CI Integration

- **Native tests**: Run on Ubuntu in GitHub Actions
- **Embedded tests**: Run in the [simavr](https://github.com/buserror/simavr) simulator (`uno_sim` environment), benchmarks included
- **Compile checks**: `scripts/ci/compile-fail.sh` builds the router with settings that `validation.h` must refuse, each with its expected message, and a few it must accept (all loads remote, a wrong boost with dual tariff off). One case per file in `scripts/ci/compile-fail/`: it edits the shipped `config*.h` with `replace 'old' 'new'` and sets `expect` to part of the message, or leaves it empty for a build that must succeed. Runs in about 30 seconds, also locally (Linux/WSL)
- See `.github/workflows/build.yml` for configuration

## Benchmark history

Every benchmark counts CPU cycles, and reports each result twice as machine-readable lines, with `benchCycles()` from [`embedded/bench_report.h`](embedded/bench_report.h): in microseconds (cycles / 16, exact to 1/16 µs) and in cycles.

```
BENCH|datalog: isqrt avg [us]|25.8750|us
BENCH|datalog: isqrt avg [cycles]|414|cycles
```

A loop reports one operation, its cycles divided by the operations, loop overhead included: `benchCycles(name, cycles, ops)`, with a name saying what one operation is (`... per op`, `... per call`). The cycles then have two decimals, the microseconds stay at four.

```
BENCH|mult_asm: multS16x16_to32 per op [us]|2.7864|us
BENCH|mult_asm: multS16x16_to32 per op [cycles]|44.58|cycles
```

Loops of any length are timed with `BenchTimer` from [`embedded/bench_timer.h`](embedded/bench_timer.h): Timer1 at the CPU clock, extended to 32 bits by its overflow interrupt, with the `millis()` interrupt paused and the serial output sent before the measurement. Short code is timed with Timer1 directly, interrupts off (`test_datalog_math_avr`).

CI collects these lines from the verbose output of the embedded tests (`scripts/ci/bench_to_json.py`), together with the flash and RAM of every build: they feed the performance report of the pull requests (below). They are also tracked on GitHub Pages, [fredm67.github.io/PVRouter-3-phase/bench](https://fredm67.github.io/PVRouter-3-phase/bench/): the flash and RAM in one chart each with every build configuration, and one small chart per benchmark, in cycles. Each push to `dev` adds a point (`scripts/ci/pages_update.py`). Smaller is better everywhere.

Every pull request gets a **performance report** as a comment, updated at each push (as ESPHome does for memory): its target branch is built and measured in the same run (`scripts/ci/measure.sh`), and compared with the PR (`scripts/ci/pr_report.py`). 📈 🚨 marks a value more than **2%** worse, 📈 🔸 a smaller regression, 📉 ✅ / 🎉 an improvement. PRs from forks get the report in the job summary only.

simavr is cycle-accurate, so the values are exact and identical from run to run: only a change of the measured code moves them, not where it starts (`micros()`, used before, counted in steps of 4 µs and included the `millis()` interrupts). The name of a result is its key in the report - renaming it shows it as new.

Every embedded test must call `haltAfterTests()` (from [`embedded/halt_after_tests.h`](embedded/halt_after_tests.h)) right after `UNITY_END()`:
the suite then runs once, and simavr exits instead of running forever.
