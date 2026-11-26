# Testing Guide

The firmware is checked in five ways. All of them run in CI (`.github/workflows/build.yml`), and
locally on Linux or WSL: on Windows, Git Bash has no gcc and no simavr.

| Kind | What it checks | Where |
|---|---|---|
| Native tests | The Arduino-free headers, on the PC (Unity) | `test/native/` |
| Embedded tests | Code that must run on the AVR: assembly, serial output, pins; benchmarks | `test/embedded/` |
| Compile-fail checks | Settings that `validation.h` must refuse, each with its message | `scripts/ci/compile-fail/` |
| Grid simulator | The unmodified firmware on a simulated 3-phase grid: regulation, loads, relays, RF, temperature sensors, ISR timing | `sim/` |
| Benchmarks | Flash, RAM and CPU cycles, compared on every pull request | `test/embedded/*_perf`, `scripts/ci/` |

## Native tests

```bash
cd Mk2_3phase_RFdatalog_temp
pio test -e native                          # every suite (needs gcc)
pio test -e native -f "*test_relay_engine*" # one suite
```

One directory per suite, `test/native/test_*/test_main.cpp`. The suites and what each covers:
[test/README.md](../test/README.md).

## Embedded tests

```bash
pio test -e uno_sim     # in the simavr simulator, no hardware (Linux/WSL)
pio test -e uno         # on a connected Arduino Uno
```

The `uno_sim` environment (`platformio.ini`) runs each suite of `test/embedded/` in simavr, a
cycle-exact ATmega328P at 16 MHz. Every embedded suite calls `haltAfterTests()`
(`test/embedded/halt_after_tests.h`) right after `UNITY_END()`, or simavr never exits.

## Compile-fail checks

```bash
bash scripts/ci/compile-fail.sh                               # from the repository root, every case
bash scripts/ci/compile-fail.sh scripts/ci/compile-fail/40-duplicate-pin.sh   # one case
```

Each case edits the shipped `config*.h` and sets `expect` to part of the `static_assert` message
the build must stop on; an empty `expect` is a build that must succeed. A new check in
`validation.h` gets a case here. Details in the header of `scripts/ci/compile-fail.sh`.

## Grid simulator

```bash
PVR_SIM_BUILD=1 sh sim/configs/sim_config.sh   # before every firmware build for the simulator
(cd Mk2_3phase_RFdatalog_temp && pio run -e basic)
make -C sim check                              # also check-new, check-new20, check-relays, check-rf...
```

`grid_sim` runs `firmware.elf` in simavr against scenarios (surplus over time, loads, relays,
remote units, DS18B20 sensors) and fails on any unmet expectation. It also measures the ADC
interrupt: `ADC ISR max`, `ADMUX delay`, `misfiled samples`; any change to the interrupt is
measured there. The targets, the firmware patches each needs and the scenario format:
[sim/README.md](../../sim/README.md).

## Benchmarks

The `*_perf` suites and `test_datalog_math_avr` print `BENCH|name|value|unit` lines with cycle
counts from simavr. CI collects them with the flash and RAM of every build, posts a performance
report on each pull request (a value more than 2 % worse is flagged) and tracks them on
[GitHub Pages](https://fredm67.github.io/PVRouter-3-phase/bench/). See
[Benchmark history](../test/README.md#benchmark-history).

## Other tests

- Configurator: `node --test configurator/test/*.test.js`
- CT recorder: `pio test --project-dir dev/CtRecorder -e native`, and the Python tool's tests
  (`python -m pytest ct_recorder/tests` from `analysis/`); see
  [dev/CtRecorder](../../dev/CtRecorder/README.en.md).
