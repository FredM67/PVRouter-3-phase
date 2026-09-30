# grid_sim - the router on a simulated grid

`grid_sim` runs the **production firmware** (`firmware.elf`, unmodified) in [simavr](https://github.com/buserror/simavr), an ATmega328P emulator, and connects it to a simulated 3-phase installation:

- the 6 ADC inputs receive 50 Hz voltage and current waves, computed at the exact CPU cycle of each conversion;
- each load pin drives a load of known power on a given phase, through a **zero-crossing triac driver**: a pin change only takes effect at the next zero crossing of the load's own phase;
- the grid current of each phase is the scenario's surplus minus the loads that conduct - which is what the firmware measures and regulates.

No hardware, no sun: regulation, load priorities, ISR timing and switching instants can be checked on any Linux/WSL machine, and in CI.

## What it measures

| Figure | Meaning |
|---|---|
| ADC conversion period | must be 1664 cycles (13 ADC clocks at /128); `grid_sim` fails otherwise |
| ADC ISR average / max | cycle-exact duration of `ISR(ADC_vect)` |
| ISR overruns | ISR calls longer than one conversion (104 µs): the next sample may be taken on the wrong channel |
| misfiled samples | conversions filed under the wrong channel: the ISR writes ADMUX for the conversion after next, so an overrun leaves one conversion on the previous channel. The first two rounds after the ADC starts are ignored |
| min sample sets | as reported by the firmware's own datalog (32 on the real board) |
| per load | switch-ons, time on, energy, and the **switching latency**: delay from the pin change to the zero crossing where the load follows, and whether it was a rising crossing (start of the phase's measurement window) |
| grid | import / export energy, total and per phase |

The firmware's serial output is shown with the simulated time.

## Usage (Linux / WSL)

```bash
# 1. build the firmware
(cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)

# 2. run every scenario, fail on any unmet expectation
make check

# or one scenario, with the serial output
make run SCN=scenarios/clouds.scn
```

simavr comes from the PlatformIO package `platformio/tool-simavr` (installed by `pio test -e uno_sim`, or `pio pkg install -g -t platformio/tool-simavr`). Only the libelf runtime is needed besides gcc and make. Another firmware can be given with `FW=path/to/firmware.elf`.

`make run` and `make check` also write, in `out/`:
- `<scenario>-trace.csv`: one line per mains cycle - surplus and grid power per phase, loads conducting (bit mask);
- `<scenario>-events.csv`: every load pin change, with its time.

## Scenarios

Plain text, one command per line, `#` starts a comment.

| Command | Meaning |
|---|---|
| `include <file>` | read another scenario file (relative path) |
| `duration <s>` | simulated time |
| `mains <Hz> <Vrms>` | grid frequency and voltage |
| `vcal <L1> <L2> <L3>` / `pcal ...` | `f_voltageCal` / `f_powerCal` from `calibration.h` |
| `load <pin> <phase> <W>` | a resistive load on a firmware output pin |
| `input <pin> <0\|1>` | level of an input pin (unconnected inputs read 1, like the pull-ups) |
| `at <t> <L1> <L2> <L3>` | surplus per phase in W (> 0 = export) at time t; linear in between, repeat a time for a step |
| `expect <t> pin <p> on\|off` | state of an output pin at time t |
| `expect <t0> <t1> grid_avg <min> <max>` | average grid power (all phases, import > 0) over a window |
| `expect min_sample_sets <n>` | lowest sample-set count reported by the firmware |
| `expect isr_overruns <n>` | at most n ISR calls longer than one conversion |
| `expect misfiled_samples <n>` | at most n conversions filed under the wrong channel |
| `expect isr_max <cycles>` | ISR duration never above this |

`scenarios/common.scn` describes the hardware of the default `config.h` / `calibration.h`; the other scenarios include it.

## Limits

- **Firmware configuration**: the scenarios match the default `config.h` (3 local loads on D5-D7, override pin D4). Another configuration needs its own `common.scn`.
- **No RF**: remote loads and RF datalogging are not simulated.
- **Ideal signals**: pure sine waves, no ADC noise, no CT phase error, no harmonics.
- **Power stage**: an ideal zero-crossing triac driver, resistive loads.
- **Timing**: simavr executes the real instructions with their datasheet cycle counts; peripherals are modelled, not exact.

## simavr ADC fix

simavr's ADC does not time free-running conversions correctly: with this firmware it converts every ~256 CPU cycles instead of 1664. `avr_adc.c` is a copy of upstream's with that fix; it is linked ahead of `libsimavr.a`, so it replaces the library's ADC. `grid_sim` checks the conversion period on every run.
