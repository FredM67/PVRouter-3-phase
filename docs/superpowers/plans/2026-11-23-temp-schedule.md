# 1-Wire Scheduling (3-phase) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The 3-phase firmware runs its 1-Wire bus I/O as short transactions right after the mains-cycle boundary, so sensors combined with remote loads or relays keep the ADC deadline.

**Architecture:** Port of PVRouter-1-phase commit f38a5cd (`D:/Git/IoT/pvr-wt/1phase-parity`, reference files `Mk2_1phase_RFdatalog_temp/temp_schedule.h`, `utils_temp.h`, `main.cpp` (`runTemperatureTransaction`, `processTemperatureData`, `loop`), `validation.h`, `test/native/test_temp_schedule/`). A pure scheduler header decides which transaction runs on which cycle; the loop runs it first thing on the pass that sees `Shared::b_newMainsCycle`.

**Tech Stack:** C++17 / Arduino / PlatformIO (ATmega328P), Unity native tests, simavr grid simulator (`sim/`).

**Spec:** `docs/superpowers/specs/2026-11-23-temp-schedule-design.md`

## Global Constraints

- Values and cadence unchanged: one reading per sensor per datalog period, converted at its start, sent at its end; `DS18B20::PowerOnFilter` and the datalog format untouched.
- No change to the interrupt work (`processing.cpp`) beyond what the port needs (none expected).
- Keep the 3-phase's existing names and style where they differ from the single-phase (e.g. `tempSensors`, `TemperatureSensing`, config names); port behaviour, not identifiers that don't exist here.
- Documentation comments in the repo's Doxygen style, `@ingroup TemperatureSensing`.
- clang-format 18.1.8 (`~/cf18venv/bin/clang-format` in WSL). Conventional commits ending with `Co-Authored-By: Claude <noreply@anthropic.com>`. Never commit `version.h` or `.vscode/extensions.json`. Never `git stash`. New `.sh`: `git update-index --chmod=+x`.
- pio, gcc, simavr only in WSL (`export PATH=$HOME/.platformio/penv/bin:$PATH`); sim builds: `sim/configs/sim_config.sh` first; apply patches in worktrees with `sed -n '/^diff --git/,$p' PATCH | patch -p1`; each firmware its own `PLATFORMIO_BUILD_DIR`.
- French docs (`Readme.md`, sim README if French) take U+00A0 before `:` `;` `!` `?`.

## Review Focus

1. **A loop pass that sees the new cycle late** (after the per-second work, an RF frame, the datalog output): no transaction runs on that cycle. *Test: Task 1 native `test_late_cycle_runs_nothing` (scheduler), Task 3 sim combo scenarios (no misfiled sample).*
2. **A sensor unplugged mid-round** (no answer to ADDRESS's reset): its reading becomes `DEVICE_DISCONNECTED_RAW` and the round moves to the next sensor. *Test: Task 1 native `test_skip_rest_of_sensor`; Task 2 keeps the existing ds18b20 tests green.*
3. **The datalog period too short for a round** (many sensors, 1 s period): refused at compile time. *Test: Task 1 compile-fail case in `test/` or `validation.h` static_assert, checked by the repo's compile checks.*
4. **No sensor built in** (`TEMP_SENSOR_PRESENT` false): the loop and RAM unchanged in effect. *Test: Task 2 builds every env, compares RAM/flash of the default env with dev.*
5. **The datalog event not on the cycle's pass** (3-phase timing differs from the single-phase): the round still starts once per period. *Test: Task 3 sim: readings present in every datalog line of the combo and temp scenarios.*

---

### Task 1: The scheduler header and its native tests

**Files:**
- Create: `Mk2_3phase_RFdatalog_temp/temp_schedule.h` (from the single-phase, comments adapted to the 3-phase: the per-cycle call is `processStartNewCycle()` after L1's positive crossing; no CT1 old-board remark unless true here)
- Create: `Mk2_3phase_RFdatalog_temp/test/native/test_temp_schedule/test_main.cpp` (from the single-phase; add `test_late_cycle_runs_nothing` and `test_skip_rest_of_sensor` if not already covered under other names — say which)
- Modify: `Mk2_3phase_RFdatalog_temp/validation.h`: the period-fits-a-round check, as the single-phase
- Modify: compile-fail tests if the repo has them for validation.h (look for the mechanism used by `make compile-fail` / `compile-fail.sh`) — add the "period too short" case.

**Interfaces:**
- Produces: `TempSchedule::Step`, `TempSchedule::Action{step, sensor}`, `TempSchedule::conversionCycles(freq)`, `TempSchedule::Scheduler<N, C>` with `next(bool onTime)`, `skipRestOf(uint8_t)`, `newPeriod()`, and `fitsInPeriod(...)` — exactly as the single-phase header defines them.

- [ ] Step 1: copy the native test, run `pio test -e native -f test_temp_schedule` → fails (header missing).
- [ ] Step 2: copy and adapt the header; validation.h check; compile-fail case.
- [ ] Step 3: run the native suite → all pass; compile-fail suite → all as expected.
- [ ] Step 4: commit `feat: temp_schedule.h, which 1-Wire transaction runs on which mains cycle`.

### Task 2: The transactions in utils_temp.h and the loop

**Files:**
- Modify: `Mk2_3phase_RFdatalog_temp/utils_temp.h`: `startRead()`, `readScratchpad()`, `endRead()` (and `initTemperatureSensors()` no longer requests a conversion), as the single-phase.
- Modify: `Mk2_3phase_RFdatalog_temp/main.cpp`: `tempSchedule`, `temperatureReadings[]`, `runTemperatureTransaction(bool onTime)`, `processTemperatureData(bool bStartNow)`, and `loop()`: the transaction first on the pass that sees the new cycle, on time when the previous pass was short (single-phase `ON_TIME_US` 1000), the period-end start condition adapted to where the 3-phase raises `b_datalogEventPending` relative to `b_newMainsCycle` (read `processing.cpp`; document the numbers in a comment).
- Modify: any native/embedded test or mock that covers `TempSensors` (e.g. `test_ds18b20`, `utils_temp.h` mock) so it builds and tests the new methods.
- Modify: `Mk2_3phase_RFdatalog_temp/Readme.md` / `Readme.en.md` only if they describe how the sensors are read.

**Interfaces:**
- Consumes: Task 1 `TempSchedule::*`.

- [ ] Step 1: write/extend the native test for whatever pure logic moved (decode path already in `ds18b20.h`); run → fail.
- [ ] Step 2: implement.
- [ ] Step 3: build every env of `Mk2_3phase_RFdatalog_temp/platformio.ini` that CI builds, plus a `TEMP_SENSOR_PRESENT` build (sim_config + temp config); no new warnings; report RAM/flash of the default env against dev. Native suite and embedded (simavr) tests green.
- [ ] Step 4: commit `feat: 1-Wire transactions start right after a mains-cycle boundary`.

### Task 3: Simulator coverage, CI, docs

**Files:**
- Modify: `sim/Makefile`, `sim/ci_targets.sh`, `.github/workflows/build.yml`: targets for the combination build (`configs/temp_rf_relays.patch`, `scenarios/combo/`), old and new boards, 50 and 60 Hz (supply_60hz.patch), in CI; ADMUX ceilings from the measurements, as `check-temp` has.
- Run locally: the RF scenarios (`check-rf`) and the relay scenarios with sensors added (old and new boards), and the existing `check-temp*` targets; record worst ADMUX write, misfiled samples, overruns.
- Modify: `sim/README.md` (replace "remote loads or relays together with sensors do not keep the ADC deadline" with the new figures), `CHANGELOG.md` ([Unreleased]), `Mk2_3phase_RFdatalog_temp/CLAUDE.md` / root `CLAUDE.md` if they map `temp_schedule.h` or the test tree.

- [ ] Step 1: add the targets; run them on the Task 2 firmware → pass, no misfiled sample; run the same targets on the firmware before Task 2 (worktree at the base commit) → they fail, as measured on 2026-11-23 (record both).
- [ ] Step 2: the RF and relay runs with sensors; record the figures.
- [ ] Step 3: docs and CI; YAML parse.
- [ ] Step 4: commit `test(sim): temperature with remote loads, RF datalog and relays in CI`.
