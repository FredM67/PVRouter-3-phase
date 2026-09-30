# grid_sim - the router on a simulated grid

`grid_sim` runs the **production firmware** (`firmware.elf`, unmodified) in [simavr](https://github.com/buserror/simavr), an ATmega328P emulator, and connects it to a simulated 3-phase installation:

- the 6 ADC inputs receive 50 Hz voltage and current waves, computed at the exact CPU cycle of each conversion;
- each load pin drives a load of known power on a given phase, through a **zero-crossing triac driver**: a pin change only takes effect at the next zero crossing of the load's own phase;
- the grid current of each phase is the scenario's surplus minus the loads that conduct - which is what the firmware measures and regulates;
- an **RFM69 radio model** sits on the SPI bus (chip select D10). Remote loads follow the frames the router sends them, received by the **`RemoteLoadReceiver` firmware** running in more simulated AVRs (or by an ideal receiver). Frames can be lost on purpose.

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
| RF, per node | frames the router sent to it, the longest gap between two of them, air time per frame, frames lost, times its receiver lost the link |
| receiver, per remote unit | frames received, frames missed because the radio was not listening |
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
- `<scenario>-events.csv`: every load change, RF frame (`RF<node>` and its payload in hex) and lost RF link, with its time.

### Remote loads

The scenarios in `scenarios/rf` need a firmware with remote loads: the `rf` environment, with `configs/remote_loads.patch` applied to `config.h` (the third load moves to remote unit 15).

```bash
git apply --unidiff-zero sim/configs/remote_loads.patch
(cd Mk2_3phase_RFdatalog_temp && pio run -e rf)
make -C sim check-rf                 # with the receiver firmware (built by build_receivers.sh)
make -C sim check-rf RX_NODES=       # with ideal receivers
git checkout Mk2_3phase_RFdatalog_temp/config.h
```

The radio model implements what the LowPowerLab library uses in packet mode:
- the register file;
- the operating modes;
- a FIFO that goes on air when the chip enters TX mode.

The PacketSent flag rises after the frame's real air time at the programmed bit rate, so `send()` blocks the main loop as long as it does on the board. The channel always looks free.

A frame that has been sent reaches every other radio. A radio that is listening (RX mode) on the same network gets it in its FIFO, with PayloadReady set and DIO0 (D2, INT0) raised, so the receiving side of the library runs unchanged. A radio that is not listening misses it, as a real one would.

Each remote unit is either:
- **the `RemoteLoadReceiver` firmware** (`grid_sim -r <node>=<receiver.elf>`), in its own simulated AVR, running in step with the router on the same clock. Its load pins (`receiver_pins`) drive its remote loads, and its serial output is shown, prefixed with `R<node>|`. `build_receivers.sh` builds it once per node ID, since `REMOTE_NODE_ID` is a constant of its `config_rf.h`. `make check-rf` does this for the nodes in `RX_NODES`;
- **an ideal receiver**, when no firmware is given for its node: its loads take their bit from each frame once it has been sent, and switch off when no frame has come for `rf_timeout` seconds (500 ms, like the firmware).

Frames get lost in `rf_drop` windows, and at random with `rf_loss`, from a fixed seed, so the same frames are lost on every run.

## Scenarios

Plain text, one command per line, `#` starts a comment.

| Command | Meaning |
|---|---|
| `include <file>` | read another scenario file (relative path) |
| `duration <s>` | simulated time |
| `mains <Hz> <Vrms>` | grid frequency and voltage |
| `vcal <L1> <L2> <L3>` / `pcal ...` | `f_voltageCal` / `f_powerCal` from `calibration.h` |
| `load <pin> <phase> <W>` | a resistive load on a firmware output pin |
| `remote <node> <bit> <phase> <W>` | a resistive load on a remote unit, driven by bit `bit` of the frames sent to RF node `node` |
| `receiver_pins <pin>...` | receiver firmware: the output pin of each payload bit, bit 0 first (`loadPins` in its `config.h`) |
| `rf_timeout <s>` | ideal receivers: link timeout (default 0.5 s) |
| `rf_drop <t0> <t1> [<node>]` | frames sent in this window (to this node, or to all) are lost |
| `rf_loss <percent>` | share of frames lost at random, over the whole run |
| `input <pin> <0\|1>` | level of an input pin (unconnected inputs read 1, like the pull-ups) |
| `at <t> <L1> <L2> <L3>` | surplus per phase in W (> 0 = export) at time t; linear in between, repeat a time for a step |
| `expect <t> pin <p> on\|off` | state of an output pin at time t |
| `expect <t> remote <node> <bit> on\|off` | state of a remote load at time t |
| `expect rf_max_gap <node> <s>` | frames to the node never further apart than this, from its first frame to the end |
| `expect rf_links_lost <node> <min> <max>` | times the node's receiver lost the RF link (its loads switched off) |
| `expect <t0> <t1> grid_avg <min> <max>` | average grid power (all phases, import > 0) over a window |
| `expect min_sample_sets <n>` | lowest sample-set count reported by the firmware |
| `expect isr_overruns <n>` | at most n ISR calls longer than one conversion |
| `expect misfiled_samples <n>` | at most n conversions filed under the wrong channel |
| `expect isr_max <cycles>` | ISR duration never above this |

`scenarios/common.scn` describes the hardware of the default `config.h` / `calibration.h`; the other scenarios include it. `scenarios/rf/common.scn` does the same for the remote-loads configuration.

## Limits

- **Firmware configuration**: the scenarios match the default `config.h` (3 local loads on D5-D7, override pin D4), and `scenarios/rf` matches `configs/remote_loads.patch`. Another configuration needs its own `common.scn`.
- **RF**: a frame either arrives intact or is lost; no range, interference, collisions or corrupted frames. The channel always looks free to carrier sense.
- **Ideal signals**: pure sine waves, no ADC noise, no CT phase error, no harmonics.
- **Power stage**: an ideal zero-crossing triac driver, resistive loads.
- **Timing**: simavr executes the real instructions with their datasheet cycle counts; peripherals are modelled, not exact.

## simavr ADC fix

simavr's ADC does not time free-running conversions correctly: with this firmware it converts every ~256 CPU cycles instead of 1664. `avr_adc.c` is a copy of upstream's with that fix; it is linked ahead of `libsimavr.a`, so it replaces the library's ADC. `grid_sim` checks the conversion period on every run.
