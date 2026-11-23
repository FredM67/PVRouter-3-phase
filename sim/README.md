# grid_sim - the router on a simulated grid

`grid_sim` runs the **production firmware** (`firmware.elf`, unmodified) in [simavr](https://github.com/buserror/simavr), an ATmega328P emulator, and connects it to a simulated 3-phase installation:

- the 6 ADC inputs receive 50 Hz voltage and current waves, computed at the exact CPU cycle of each conversion;
- each load pin drives a load of known power on a given phase, through a **zero-crossing triac driver**: a pin change only takes effect at the next zero crossing of the load's own phase;
- the grid current of each phase is the scenario's surplus minus the loads that conduct - which is what the firmware measures and regulates;
- an **RFM69 radio model** sits on the SPI bus (chip select D10). Remote loads follow the frames the router sends them, received by the **`RemoteLoadReceiver` firmware** running in more simulated AVRs (or by an ideal receiver). Frames can be lost on purpose.
- **DS18B20 temperature sensors** on a 1-Wire bus (`ds18b20.c`): the firmware reads them as on the board, and the timing of its bus I/O against the mains cycle is measured.

No hardware, no sun: regulation, load priorities, ISR timing and switching instants can be checked on any Linux/WSL machine, and in CI.

## What it measures

| Figure | Meaning |
|---|---|
| ADC reference | AVCC 5 V (old board) or internal 1.1 V (new board), as the firmware selects it in `ADMUX`: the analog levels follow it |
| ADC conversion period | must be 1664 cycles (13 ADC clocks at /128); `grid_sim` fails otherwise |
| ADC ISR average / max | cycle-exact duration of `ISR(ADC_vect)` |
| ISR overruns | ISR calls longer than one conversion (104 µs): the next sample may be taken on the wrong channel |
| misfiled samples | conversions filed under the wrong channel: the ISR writes ADMUX for the conversion after next, so an overrun leaves one conversion on the previous channel. The first two rounds after the ADC starts are ignored |
| ADMUX delay | cycles from the start of a conversion to the ISR's ADMUX write. The datasheet only allows a change one ADC clock (128 cycles) or more after the conversion started: earlier, that conversion may take the new channel. The simulated ADC locks the channel at once, so misfiled samples cannot show it; this figure does |
| 1-Wire | resets, presence pulses, slot errors (a low neither a 1, a 0 nor a reset); the bursts of bus activity (slots less than 2 ms apart) and the longest; where in the mains cycle (phase L1) the slots that keep interrupts off for long fall: a reset (70 µs from its release to its presence sample) and a 0 written (65 µs low) |
| per DS18B20 | its conversions and scratchpad reads, and the reads made before the end of a conversion (750 ms) |
| temperatures | each `T<n>` of the datalog: in how many lines, lowest and highest |
| min sample sets | as reported by the firmware's own datalog (32 on the real board) |
| per load | switch-ons, time on, energy, and the **switching latency**: delay from the pin change to the zero crossing where the load follows, and whether it was a rising crossing (start of the phase's measurement window) |
| per relay | switch-ons, time on, energy, and the shortest complete ON and OFF periods |
| RF, per node | frames the router sent to it, the longest gap between two of them, air time per frame, frames lost, times its receiver lost the link |
| receiver, per remote unit | frames received, frames missed because the radio was not listening |
| grid | import / export energy, total and per phase |

The firmware's serial output is shown with the simulated time.

## Usage (Linux / WSL)

By default, the simulated grid is the old board's (ADC reference AVCC, 5 V, and the calibration of
`scenarios/common.scn`), and `grid_sim` reads the human-readable datalog: `configs/sim_config.sh`
sets both in `config.h`, whatever its defaults. Run it before every firmware build below.

Build with `PVR_SIM_BUILD` set, as CI does. The firmware prints its banner at 9600 baud before it
starts sampling, so the banner's length shifts the whole run against the scenario. A different
branch name alone moves the grid import of `surplus_step` by 0.05 Wh (7.15 against 7.20 Wh). With
`PVR_SIM_BUILD`, the build writes the branch name a CI checkout gives (`N/A`), and the same code
gives the same result everywhere.

```bash
# 1. build the firmware
export PVR_SIM_BUILD=1
(cd .. && sim/configs/sim_config.sh)
(cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)

# 2. run every scenario, fail on any unmet expectation
make check
make -j$(nproc) check     # the same, scenarios in parallel

# or one scenario, with the serial output
make run SCN=scenarios/clouds.scn
```

simavr comes from the PlatformIO package `platformio/tool-simavr` (installed by `pio test -e uno_sim`, or `pio pkg install -g -t platformio/tool-simavr`). Only the libelf runtime is needed besides gcc and make. Another firmware can be given with `FW=path/to/firmware.elf`.

`make run` and `make check` also write, in `out/`:
- `<scenario>-trace.csv`: one line per mains cycle - surplus and grid power per phase, loads conducting (bit mask);
- `<scenario>-events.csv`: every load change, RF frame (`RF<node>` and its payload in hex) and lost RF link, with its time.

### Remote loads

The same scenarios run on the **new board** (internal 1.1 V ADC reference) with `make check-new`: `grid_sim` follows the reference the firmware selects in `ADMUX`, and `board_scn.sh` takes the new board's calibration from `calibration.h` (`f_voltageCalNewBoard`, `f_powerCalPerAmp` × `f_ctRating`), read after the scenario with `grid_sim -b`. The firmware is built in a build directory of its own:

```bash
sim/configs/sim_config.sh NEW
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-new pio run -e basic)
git checkout Mk2_3phase_RFdatalog_temp/config.h
make -C sim check-new
make -C sim compare-boards   # after check and check-new
```

`compare-boards` checks that each scenario gives the same result on both boards: grid import and export, in total and per phase, and the energy of each load, within 1 % or 0.1 Wh. The boards only differ in resolution: with the 50 A CTs of `calibration.h`, the new board has 0.161 A per ADC step against 0.061 A on the old board, which can move a switching instant by a half-cycle now and then.

`check-new20` runs the same scenarios with 20 A CTs (0.065 A per step, about the old board's resolution), and `compare-boards-20` compares them within 0.5 % or 0.1 Wh: a change of ISR timing alone shifts the switching instants and moves the result by up to 0.08 Wh (surplus_step on the old board, same arithmetic). The firmware must be built with that rating, which `sim_config.sh` writes into `f_ctRating`:

```bash
sim/configs/sim_config.sh NEW 20
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-new20 pio run -e basic)
git checkout Mk2_3phase_RFdatalog_temp/config.h Mk2_3phase_RFdatalog_temp/calibration.h
make -C sim check-new20 compare-boards-20
make -C sim compare-ct    # 20 A against 50 A CTs, after check-new and check-new20
```

The scenarios in `scenarios/rf` need a firmware with remote loads: `configs/remote_loads.patch` applied to `config.h`, built in a build directory of its own (a fourth load is added: loads 3 and 4 go to remote units 1 and 2, RF nodes 15 and 16).

```bash
sim/configs/sim_config.sh
git apply --unidiff-zero sim/configs/remote_loads.patch
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-rf pio run -e basic)
make -C sim check-rf                 # with the receiver firmware (built by build_receivers.sh)
make -C sim check-rf RX_NODES=       # with ideal receivers
make -C sim -j$(nproc) check check-rf    # both sets in parallel
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

Frames get lost in `rf_drop` windows, and at random with `rf_loss`, from a fixed seed, so the same frames are lost on every run of the same firmware. Which frames the sequence falls on still depends on the firmware's timing; a limit on the losses in a row keeps the longest gap independent of it.

### Relays

The scenarios in `scenarios/relays` need a firmware with relay diversion: `configs/relays.patch` puts two relays on D8 and D9 (surplus thresholds 1000 and 2000 W, import threshold 200 W, 2 minutes minimum ON and OFF) with a 1-minute filter. It is built in a build directory of its own, so the `basic` firmware of `make check` stays.

```bash
sim/configs/sim_config.sh
git apply --unidiff-zero sim/configs/relays.patch
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-relays pio run -e basic)
git checkout Mk2_3phase_RFdatalog_temp/config.h
make -C sim check-relays
```

`make check-relays60` runs the same scenarios at 60 Hz: `configs/supply_60hz.patch` after `configs/relays.patch`, built in `.pio/build-relays60` (`git checkout Mk2_3phase_RFdatalog_temp/config.h Mk2_3phase_RFdatalog_temp/config_system.h` after the build); `out/scn60/relays` holds the scenarios with `mains 60`, derived as for `check-temp60`.

A relay's contacts follow its pin at once. The firmware decides on the grid power of each datalog line (every 5 s) through its TEMA filter, waits 1 minute after any relay change, and holds each relay for its minimum ON or OFF time - from power-up for the first switch-on. A relay scenario therefore lasts several minutes; the minimum times are longer than the settle time, so that a firmware ignoring them fails.

### Temperature sensing

`make check-temp` (old board) and `make check-new-temp` (new board) run the scenarios of `make check` against a firmware with temperature sensing on D3 (`configs/temperature.patch`, built in a build directory of its own), with the two DS18B20 of `configs/ds18b20.scn` on the bus, at 21.50 and -3.25 °C: the datalog must report both, read after their conversion (`ds18b20_early_reads 0`). The OneWire library keeps interrupts off for a reset and for each 0 written (70 and 65 µs, about 1100 cycles), and the ADC interrupt waits meanwhile; the common expectations (no misfiled sample, ADMUX written 128 cycles or more after its conversion started, no overrun) apply unchanged.

```bash
sim/configs/sim_config.sh NEW        # or without NEW, for check-temp
git apply --unidiff-zero sim/configs/temperature.patch
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-new-temp pio run -e basic)   # build-temp for the old board
git checkout Mk2_3phase_RFdatalog_temp/config.h
make -C sim check-new-temp
```

Measured on the three scenarios, 50 Hz, and over mains frequencies from 49.5 to 51 Hz (the 1-Wire bursts then fall at other angles against the ADC conversions): the ISR itself does not change (max ~1260-1280 cycles, the datalog call), only the ADMUX write is pushed back when an ISR call waits for a 1-Wire time slot. The ADMUX write of an ISR call is at most 1664 cycles after its pending flag: the call may wait for a time slot (up to 1036 cycles for a 0 written, 1113 for a reset) and still has to write ADMUX within what is left. The ordinary calls write ADMUX within 373 cycles after their start, the call of each mains cycle within ~620 (bits of the load update), the datalog call within ~850-1190, once per 5 s: the firmware starts its 1-Wire burst after that call, so a window cannot delay it, but it delays the next calls. Worst ADMUX delay seen: 1505 cycles on the old board, 1445 on the new one, against 1664 (about 1190 without sensors); no misfiled sample, no overrun, no scratchpad read early. That margin is thin: a call that writes ADMUX 620 cycles after its start fails when it is pending in the first 70 cycles of a reset slot.

`make check-temp60` and `make check-new-temp60` run the same scenarios at 60 Hz (`configs/supply_60hz.patch` after `configs/temperature.patch`, `SUPPLY_FREQUENCY` 60): `out/scn60` holds the scenarios with `mains 60` and the sample sets per mains cycle scaled (26.7 instead of 32 per cycle: `min_sample_sets` 25 instead of 30). Worst ADMUX delay 1423 cycles on the old board, 1442 on the new one, no misfiled sample (1558 and 1408 before the 1-Wire transactions were scheduled).

```bash
sim/configs/sim_config.sh NEW        # or without NEW, for check-temp60
git apply --unidiff-zero sim/configs/temperature.patch sim/configs/supply_60hz.patch
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-new-temp60 pio run -e basic)   # build-temp60 for the old board
git checkout Mk2_3phase_RFdatalog_temp/config.h Mk2_3phase_RFdatalog_temp/config_system.h
make -C sim check-new-temp60
```

#### Temperature with remote loads, RF datalog and relays

Temperature sensing together with remote loads (`configs/remote_loads.patch`, four loads), with relays (`configs/relays.patch`, the sensors on D13: the relay patch has D3 for the diversion pin), or with both is the heaviest case for the ADC deadline: a 1-Wire slot keeps interrupts off 1036-1113 cycles, the ISR call of each mains cycle (`processStartNewCycle()`) writes ADMUX ~740-780 cycles after its start with four loads (~620 with three), and a call of the datalog period ~850: a slot that begins before such a call is pending delays it past the 1664 cycles. Before the firmware scheduled its 1-Wire transactions, these builds misfiled samples and wrote ADMUX after the next conversion had started in some runs (the combination build below: 5 misfiled samples on the old board, 35 on the new one, 7 and 11 at 60 Hz).

The firmware now runs the bus I/O as short transactions, at most one per mains cycle, started when the loop sees that `processStartNewCycle()` is done (`Shared::b_newMainsCycle`, set at its end; `temp_schedule.h`), so the long slots fall among the ordinary ADC calls, from 34 to 158 degrees after L1's positive crossing. With sensors, the datalog output is handled on the pass that sees the new cycle, 1248 µs after its event, so that the conversion of the next round starts right after the cycle's work.

`configs/temp_rf_relays.patch` builds the combination (two local loads, two remote loads, RF datalog, two relays on D8 and D9, the override pin and the two DS18B20 on D3) and `scenarios/combo/surplus_ladder.scn` runs every feature at once (the surplus climbs through the four loads and both relays, then falls to an import) in 260 s: the two relays have 1-minute minimum ON and OFF times, which with the 60 s settle time between relays sets the length (relay 1 in at 66 s, relay 2 at 126 s, relay 2 out at 186 s, relay 1 at 246 s). `make check-combo`, `check-new-combo`, `check-combo60` and `check-new-combo60` run it on the old and the new board, at 50 and 60 Hz, with the receiver firmware on each remote unit; they are in CI (the *RF, relays, temperature* job). `scenarios/combo/common.scn` also has a ceiling, `admux_delay_max 1600`, a little above the worst write measured.

| Build | Misfiled before / now | Worst ADMUX write now (cycles, deadline 1664) | Overruns |
|---|---|---|---|
| combination, old board, 50 Hz | 5 / 0 | 1418 | 0 |
| combination, new board, 50 Hz | 35 / 0 | 1398 | 0 |
| combination, old board, 60 Hz | 7 / 0 | 1529 | 0 |
| combination, new board, 60 Hz | 11 / 0 | 1545 | 0 |

The 'before' runs (firmware with the 1-Wire bus read in one burst per datalog period) report ADMUX writes of 2 to 64 cycles, flagged TOO EARLY: a write later than 1664 cycles shows as a small delay from the *next* conversion, so a late write always comes with misfiled samples. The scenario has 26 expectations: 23 passed before, all pass now. The four temperature targets (sensors only) go from 1558 to 1442 cycles at worst.

The same firmware without the combination: `scenarios/rf` with remote loads and sensors on D3, and `scenarios/relays` with relays and sensors on D13, on both boards at 50 and 60 Hz (56 runs, run by hand with `grid_sim -b` and a board file of `board_scn.sh` and `configs/ds18b20.scn`): 0 misfiled samples and 0 overruns in every run; worst ADMUX write 1434 cycles (relays, new board, 60 Hz) and 1431 (remote loads, old board, 60 Hz); before, the diagnostic runs of 2026-11-23 found writes at 1666 cycles (relays) and 1718 (remote loads), past the deadline. One run failed on its own scenario, as without sensors: `relays/rotation_pin_off_peak` at 60 Hz on the old board (`expect 28 pin 6 on`: D6 takes the surplus left by D5 and modulates, 7 cycles ON of 9, so a check at one instant fell in an OFF gap at 60 Hz and in an ON stretch at 50 Hz by luck). The scenario no longer checks D6 there, and `scenarios/relays` now runs at 60 Hz in CI (below).

```bash
sim/configs/sim_config.sh                # NEW for the new board, build-new-combo
git apply --unidiff-zero sim/configs/temp_rf_relays.patch
(cd Mk2_3phase_RFdatalog_temp && PLATFORMIO_BUILD_DIR=.pio/build-combo pio run -e basic)
git checkout Mk2_3phase_RFdatalog_temp/config.h
make -C sim check-combo                  # at 60 Hz: also configs/supply_60hz.patch, build-combo60, check-combo60
```

### In CI

The scenarios are spread over parallel jobs: three for the grid scenarios, one for the RF ones (`build.yml`), three for the relay ones, each at 50 and 60 Hz (`relay-sim.yml`: both firmware builds in every job, `relays` and `relays60` targets). `ci_targets.sh` gives the make targets of a job, from the scenarios it finds: job k of n takes every n-th scenario, starting from the k-th. A grid scenario runs on its three boards and on the four temperature builds (50 and 60 Hz) in the same job; the job then compares the three boards. The combination scenarios (`scenarios/combo`) have two jobs of their own: their two 50 Hz builds in one, their two 60 Hz builds in the other.

```bash
sim/ci_targets.sh grid 1/3      # check-clouds check-new-clouds check-new20-clouds check-temp-clouds check-new-temp-clouds check-temp60-clouds check-new-temp60-clouds
sim/ci_targets.sh relays 2/3    # check-relays-relay_import_first check-relays-relay_steps check-relays-rotation_pin_off_peak
sim/ci_targets.sh relays60 2/3  # check-relays60-relay_import_first check-relays60-relay_steps check-relays60-rotation_pin_off_peak
sim/ci_targets.sh combo 1/1     # check-combo-surplus_ladder check-new-combo-surplus_ladder
sim/ci_targets.sh combo60 1/1   # check-combo60-surplus_ladder check-new-combo60-surplus_ladder
```

With more scenarios than jobs, a job runs several of them in parallel. A new scenario needs no change to the workflows; more jobs, only their `part` list.

## Scenarios

Plain text, one command per line, `#` starts a comment.

| Command | Meaning |
|---|---|
| `include <file>` | read another scenario file (relative path) |
| `duration <s>` | simulated time |
| `mains <Hz> <Vrms>` | grid frequency and voltage |
| `vcal <L1> <L2> <L3>` / `pcal ...` | `f_voltageCal` / `f_powerCal` from `calibration.h` |
| `load <pin> <phase> <W>` | a resistive load on a firmware output pin |
| `relay <pin> <phase> <W>` | a resistive load on a relay: it follows the pin at once, with no zero-crossing latching |
| `remote <node> <bit> <phase> <W>` | a resistive load on a remote unit, driven by bit `bit` of the frames sent to RF node `node` |
| `receiver_pins <pin>...` | receiver firmware: the output pin of each payload bit, bit 0 first (`loadPins` in its `config.h`) |
| `rf_timeout <s>` | ideal receivers: link timeout (default 0.5 s) |
| `rf_drop <t0> <t1> [<node>]` | frames sent in this window (to this node, or to all) are lost |
| `rf_loss <percent> [<max>]` | share of frames lost at random, over the whole run; at most `<max>` in a row per node |
| `rf_absent` | the router's RFM69 does not answer (missing, unpowered or badly wired: every SPI read gives 0) |
| `ds18b20 <pin> <°C> <ROM code>` | a DS18B20 on the 1-Wire bus of the pin, at that temperature; its ROM code in 8 hex bytes (`28 BE 41 6B 09 00 00 A4`). Until its first conversion, it holds the power-on value 85.00 °C |
| `input <pin> <0\|1> [<t>]` | level of an input pin from time t on (default 0): the latest line at or before the current time wins; unconnected inputs read 1, like the pull-ups |
| `at <t> <L1> <L2> <L3>` | surplus per phase in W (> 0 = export) at time t; linear in between, repeat a time for a step |
| `expect <t> pin <p> on\|off` | state of an output pin at time t |
| `expect <t> remote <node> <bit> on\|off` | state of a remote load at time t |
| `expect rf_max_gap <node> <s>` | frames to the node never further apart than this, from its first frame to the end |
| `expect rf_links_lost <node> <min> <max>` | times the node's receiver lost the RF link (its loads switched off) |
| `expect switches <pin> <min> <max>` | switch-ons of a load or relay over the run |
| `expect min_on <pin> <s>` / `expect min_off <pin> <s>` | shortest complete ON period / OFF period between two ON ones, at least |
| `expect <t0> <t1> grid_avg <min> <max>` | average grid power (all phases, import > 0) over a window |
| `expect min_sample_sets <n>` | lowest sample-set count reported by the firmware |
| `expect ds18b20_early_reads <n>` | at most n scratchpads read before the end of their conversion |
| `expect temperature <n> <min> <max>` | every `T<n>` the datalog reports lies within [min, max], and at least one line reports it |
| `expect isr_overruns <n>` | at most n ISR calls longer than one conversion |
| `expect misfiled_samples <n>` | at most n conversions filed under the wrong channel |
| `expect isr_max <cycles>` | ISR duration never above this |
| `expect admux_delay_min <cycles>` | the ISR never writes ADMUX sooner than this after its conversion started |
| `expect admux_delay_max <cycles>` | the ISR never writes ADMUX later than this after its conversion started: a ceiling under the 1664-cycle deadline (a write later than that shows up as a small delay from the next conversion, with misfiled samples) |

`scenarios/common.scn` describes the hardware of the default `config.h` / `calibration.h`; the other scenarios include it. `scenarios/rf/common.scn` and `scenarios/relays/common.scn` do the same for the remote-loads and relay configurations.

## Limits

- **Firmware configuration**: the scenarios match the default `config.h` (3 local loads on D5-D7, override pin D4), `scenarios/rf` matches `configs/remote_loads.patch` and `scenarios/relays` matches `configs/relays.patch` and `scenarios/combo` matches `configs/temp_rf_relays.patch`. Another configuration needs its own `common.scn`.
- **RF**: a frame either arrives intact or is lost; no range, interference, collisions or corrupted frames. The channel always looks free to carrier sense.
- **Ideal signals**: pure sine waves, no ADC noise, no CT phase error, no harmonics.
- **Power stage**: an ideal zero-crossing triac driver, resistive loads; relays switch at once, with no bounce.
- **DS18B20**: Skip ROM, Match ROM, Convert T and Read Scratchpad only, with a fixed temperature, a 750 ms conversion and no parasite power. The bus is ideal: no noise, no rise time. Search ROM (the calibration mode lists the sensors with it) is not modelled.
- **Timing**: simavr executes the real instructions with their datasheet cycle counts; peripherals are modelled, not exact.

## simavr ADC fix

simavr's ADC does not time free-running conversions correctly: with this firmware it converts every ~256 CPU cycles instead of 1664. `avr_adc.c` is a copy of upstream's with that fix; it is linked ahead of `libsimavr.a`, so it replaces the library's ADC. `grid_sim` checks the conversion period on every run.
