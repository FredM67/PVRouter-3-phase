# 1-Wire transactions after the mains-cycle boundary (3-phase)

Status: approved by the user on 2026-11-23. A port of the single-phase design (PVRouter-1-phase,
commit f38a5cd, `temp_schedule.h`).

## Why

A 1-Wire reset, and each 0 written, keep interrupts off for 65 to 70 µs (the OneWire library). The ADC
interrupt then starts up to ~1100 cycles late, and must still write ADMUX within the 1664 cycles of its
conversion. Today the 3-phase reads every sensor at once at the end of each datalog period, and its
slots can fall on any interrupt call.

Measured in the grid simulator (2026-11-23):
- sensors alone pass at 50 and 60 Hz (worst ADMUX write 1558 cycles);
- sensors with remote loads or relays don't: the per-cycle call (`processStartNewCycle()`, after L1's
  positive crossing) writes ADMUX 660-780 cycles in with 4 loads, and a slot on it makes it miss.
  Old board: all 5 RF scenarios fail (up to 17 misfiled samples), 5 of 9 relay runs; new board: 4 of 6
  frequency-sweep runs, 1 of 9 relay runs; the combination of everything misfiles 5 to 35 samples.

## Goals

- No 1-Wire slot on the per-cycle call, whatever the features built in.
- The values and their cadence unchanged: one reading per sensor and per datalog period, converted at
  its start, sent at its end.

## Non-goals

- No change to the interrupt work itself.
- No change to the sensors' configuration, the power-on filter, or the datalog format.

## Anti-goals

- Disabling sensors when remote loads or relays are built in: it drops a supported feature.

## Approaches (chosen: A)

- **A. The single-phase scheduling, ported.** The bus I/O split into short transactions, at most one
  per mains cycle, each started at the top of the loop pass that sees `Shared::b_newMainsCycle`: its
  slots fall long before the next per-cycle call, 20 ms later. Proven on the single-phase.
- B. Shorter per-cycle interrupt work: the slot still lands on it. Rejected.
- C. Sensors disabled with RF or relays: anti-goal. Rejected.

## Decided

- `temp_schedule.h` copied from PVRouter-1-phase (no Arduino dependency) with its native tests:
  CONVERT (every sensor), then for each sensor ADDRESS, READ, CLOSE, one transaction per cycle.
- A transaction runs only on a cycle the loop sees on time (the previous pass short); otherwise it
  waits for the next cycle.
- One round per datalog period, started when the loop handles the end of the period on time (before
  the datalog output, whose UART interrupts add to the delay), or with the next cycle.
- `utils_temp.h` gains `startRead()`, `readScratchpad()`, `endRead()`; `main.cpp` gains
  `runTemperatureTransaction()` and the scheduled `processTemperatureData()`. `validation.h` refuses
  a datalog period too short for a round, as on the single-phase.
- Tests: the combination build (`sim/configs/temp_rf_relays.patch`, `sim/scenarios/combo/`) goes into
  the simulator's CI targets, old and new boards, 50 and 60 Hz; the RF and relay scenarios are rerun
  with sensors. The sim README and the CHANGELOG get the new figures.

## Left to the builder

- Where the 3-phase raises the datalog event relative to the new cycle (the single-phase's "on time"
  thresholds assume 2 sample sets; check and adapt).
- The ADMUX ceilings of the new CI targets, from the measurements.
