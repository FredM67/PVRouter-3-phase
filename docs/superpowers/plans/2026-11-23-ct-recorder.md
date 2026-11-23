# CT Recorder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A recording sketch and Python scripts that measure a router's CT on real hardware, compute the CT filter (`lpf_gain`, `alpha`) for that board and sample spacing, and compare a run with the filter against one without, or conclude no filter is needed.

**Architecture:** `dev/CtRecorder/` is a standalone PlatformIO project. Its ISR runs the ADC free-running exactly as the routers do, and puts each recorded V/I pair, with the load state, in a ring buffer. `loop()` streams the buffer as binary frames and drives the load in a burst pattern. Its pure headers (chain layout, sample path, frames, ring buffer, burst pattern) are tested natively. `analysis/ct_recorder/` is a Python package: it captures the serial stream into a file, rebuilds the firmware's sample path bit for bit, fits the CT, searches the filter and compares runs.

**Tech Stack:** C++17 / Arduino / PlatformIO (ATmega328P), Unity native tests; Python 3 with numpy, matplotlib and pyserial, pytest.

**Spec:** `docs/superpowers/specs/2026-11-23-ct-recorder-design.md`

## Global Constraints

- The router firmware (`Mk2_3phase_RFdatalog_temp/`) is not modified. The tool includes `ct_filter.h` and `mult_asm.h` from it (`-I ../../Mk2_3phase_RFdatalog_temp`) and copies nothing from them.
- "The whole ADC chain as the firmware runs it, without calibration": free-running ADC, prescaler 128, 13 ADC clocks per conversion (104 µs), ADMUX written last in the ISR for the conversion after next. Same reference and left-aligned result (ADLAR). No `f_powerCal`, `f_voltageCal` or `f_phaseCal`: everything stays in ADC units.
- Sample path, as `processing.cpp`:
  - voltage minus DC is `(raw | 32) - dcOffset`;
  - `dcOffset` starts at `511 << 6` (32704), the integrator at `32704 << 15`;
  - the integrator adds every voltage sample minus DC;
  - `dcOffset = (integrator << 1) >> 16` at each confirmed change to negative polarity;
  - polarity is confirmed when it has differed for more than `PERSISTENCE_FOR_POLARITY_CHANGE` = 1 sample;
  - current minus DC is `(raw | 32) - dcOffset` (the voltage's offset), then `CtFilter::LowPassBoost::apply` when the filter is on.
- Settings:

  | Router | Board | Conversions per set | V pin | Current pins | Reference |
  |---|---|---|---|---|---|
  | 3-phase | `OLD`, `NEW` | 6, pins 0..5 in order V1, I1, V2, I2, V3, I3 | 0 | I1 = 1 | |
  | single-phase | `OLD_SEG` | 3, in order V, CT1, CT2 | 3 | CT1 5, CT2 4 | |
  | single-phase | `OLD_OLED`, `NEW` | 3, in order V, CT1, CT2 | 0 | CT1 1, CT2 3 | |

  `OLD*` boards use AVcc (REFS0), `NEW` the internal 1.1 V (REFS1|REFS0). The recorded pair is V1/I1 (3-phase), V/CT1 or V/CT2 (single-phase).
- Filter settings are accepted only in the range `validation.h` accepts: alpha from 0.00005 to below 0.0625, gain from 0 to below 64. A gain of 0 means the filter is off. At power-up the filter is off.
- The load pin defaults to 5 (the first load of both shipped configs) and is driven only at a confirmed positive crossing (cycle start). Every other load pin (6, 7) is held OFF. ON is the level the firmware drives for `LoadStates::LOAD_ON`.
- Serial: 500 000 baud (exact at 16 MHz with U2X).
- Docs in French (`README.md`, default) and English (`README.en.md`). French text has a no-break space (U+00A0) before `:` `;` `!` `?`.
- clang-format 18.1.8 (`~/cf18venv/bin/clang-format` in WSL). Conventional commits ending with `Co-Authored-By: Claude <noreply@anthropic.com>`. Never commit `version.h` or `.vscode/extensions.json`. Any new `.sh`: `git update-index --chmod=+x`.
- Environment: pio, gcc and simavr only in WSL (`export PATH=$HOME/.platformio/penv/bin:$PATH` in scripts). git from Git Bash in worktrees.

## Review Focus

1. **A burst that starts while the DC offset is still settling** (the first seconds after power-up): the analysis must skip the start-up, not fit on it. *Test: Task 4 `test_skips_startup`.*
2. **A recording cut mid-frame or with garbage before `#BEGIN`** (the capture started late, or a USB hiccup): the reader resyncs on the next valid frame and counts what it dropped, and doesn't fail. *Test: Task 3 `test_reader_resyncs_after_garbage`.*
3. **Ring buffer overruns** (a slow USB adapter): they must show in the recording and in the analysis report, and the affected bursts must be left out of the fit. *Tests: Task 2 `test_overrun_flag_on_next_frame`, Task 4 `test_bursts_with_overrun_excluded`.*
4. **A load that isn't resistive or isn't connected** (current near 0 in ON cycles, or not proportional to V): the fit must refuse with a clear message, not output a filter. *Test: Task 4 `test_refuses_without_load_current`.*
5. **An OFF period shorter than the CT's tail:** a warning names the burst, as the spec requires. *Test: Task 4 `test_warns_when_tail_not_settled`.*

---

### Task 1: Chain layout and sample path (pure C++, native tests)

**Files:**
- Create: `dev/CtRecorder/platformio.ini`: envs `three_phase_old`, `three_phase_new`, `single_phase_old_seg`, `single_phase_old_oled`, `single_phase_new` (each sets `-D CTR_ROUTER=… -D CTR_BOARD=… -D CTR_PAIR=…`), plus `native` (`test_build_src = no`, `-I ../../Mk2_3phase_RFdatalog_temp`). Model it on `dev/RST_3phase_free_dev/platformio.ini`.
- Create: `dev/CtRecorder/chain_layout.h`, `dev/CtRecorder/sample_path.h`
- Test: `dev/CtRecorder/test/test_chain_layout/test_main.cpp`, `dev/CtRecorder/test/test_sample_path/test_main.cpp`

**Interfaces:**
- Produces:
  - `enum class Router : uint8_t { SINGLE_PHASE, THREE_PHASE }`, `enum class Board : uint8_t { OLD_SEG, OLD_OLED, OLD, NEW }` (OLD is the 3-phase old board), `enum class Pair : uint8_t { GRID, DIVERTED }`.
  - `struct ChainLayout { uint8_t conversions; uint8_t pins[6]; uint8_t vIndex; uint8_t iIndex; uint8_t refBits; }`.
  - `constexpr ChainLayout layoutOf(Router, Board, Pair)`, `constexpr uint8_t admuxOf(const ChainLayout&, uint8_t index)` (refBits | ADLAR | pin), `constexpr uint8_t indexAfterNext(const ChainLayout&, uint8_t index)`, `constexpr uint16_t sampleSpacingUs(const ChainLayout&)` = conversions × 104.
  - `struct SamplePath` with `int16_t onVoltage(uint16_t raw)` (returns V minus DC, updates polarity, integrator and offset); `bool cycleStarted() const` (true right after a confirmed change to positive); `int16_t currentMinusDc(uint16_t raw) const`; `uint16_t dcOffset() const`.

- [ ] **Step 1: Write the failing tests**

  `test_chain_layout`:
  - `test_three_phase_pins_in_order`: `layoutOf(THREE_PHASE, OLD, GRID)` gives conversions 6, pins `{0,1,2,3,4,5}`, vIndex 0, iIndex 1.
  - `test_single_phase_old_seg_pins`: conversions 3, pins `{3,5,4}`; GRID → iIndex 1; DIVERTED → iIndex 2.
  - `test_single_phase_new_pins`: pins `{0,1,3}`, refBits `0xC0`; OLD_OLED refBits `0x40`.
  - `test_after_next_wraps`: for 6 conversions, `indexAfterNext(l, 4) == 0` and `indexAfterNext(l, 5) == 1`; for 3, `indexAfterNext(l, 2) == 1`.
  - `test_sample_spacing`: 624 for 3-phase, 312 for single-phase.

  `test_sample_path`:
  - `test_initial_offset_is_nominal`: `dcOffset() == 32704`.
  - `test_voltage_minus_dc_rounds_half`: raw `512 << 6` gives `(32768 | 32) - 32704 = 96`.
  - `test_offset_tracks_a_shifted_sine`: a 50 Hz sine of amplitude 300 counts centred on 530, at the 3-phase spacing for 10 s. Final `dcOffset()` is within 64 of `530 << 6`, and it only changes at confirmed negative crossings (assert no change between two of them).
  - `test_persistence_needs_two_samples`: a single sample of opposite sign does not flip the polarity; two do.
  - `test_cycle_started_once_per_cycle`: over 1 s of a 50 Hz sine, `cycleStarted()` is true exactly 50 times (±1).
  - `test_current_uses_voltage_offset`: after the offset moves, `currentMinusDc(raw) == (raw | 32) - dcOffset()`.

- [ ] **Step 2: Run them, see them fail**

  Run in WSL: `cd dev/CtRecorder && pio test -e native`. Expected: build errors (headers missing).

- [ ] **Step 3: Implement `chain_layout.h` and `sample_path.h`**

  Pure C++, no Arduino. ADMUX bit numbers as constants, checked against `avr/io.h` by a `static_assert` in the sketch (Task 2), as the firmware does. Each value and rule from the Global Constraints sample path, with a comment naming the `processing.cpp` function it mirrors (`processPolarity`, `confirmPolarity`, `processVoltage`, `processMinusHalfCycle`, `processCurrentRawSample`).

- [ ] **Step 4: Run them, see them pass.** Same command. Expected: all PASS.

- [ ] **Step 5: Commit:** `feat(ct-recorder): chain layouts and the firmware's sample path, with native tests`

### Task 2: Frames, ring buffer, burst pattern, and the sketch

**Files:**
- Create: `dev/CtRecorder/frame.h`, `dev/CtRecorder/ring_buffer.h`, `dev/CtRecorder/burst_pattern.h`, `dev/CtRecorder/CtRecorder.ino` (or `main.cpp`, as `RST_3phase_free_dev`)
- Test: `dev/CtRecorder/test/test_frame/`, `test_ring_buffer/`, `test_burst_pattern/`

**Interfaces:**
- Consumes: Task 1 `layoutOf`, `admuxOf`, `indexAfterNext`, `SamplePath`; firmware `CtFilter::coefficientsOf(float gain, float alpha)`, `CtFilter::LowPassBoost::apply(int16_t, const Coefficients&)`.
- Produces:
  - **The frame.** 8 bytes:
    - `0xA5`;
    - flags: bit0 load ON, bit1 filter ON, bit2 cycle start, bit3 overrun before this frame, bits 4–7 zero;
    - V raw as uint16 LE, 10-bit right-aligned;
    - I raw as uint16 LE, 10-bit;
    - I out as int16 LE: the x64 current minus DC, filtered when the filter is on.

    `void encodeFrame(uint8_t out[8], const FrameData&)`, `bool decodeFrame(const uint8_t in[8], FrameData&)`. Decode rejects a bad sync byte, non-zero high flag bits, or raw ≥ 1024.
  - **The ring buffer.** `template<uint8_t N> struct RingBuffer` (N a power of two, 128 in the sketch) with `bool push(const FrameData&)` (false and overrun latched when full), `bool pop(FrameData&)` (sets the overrun flag on the next popped frame) and `uint32_t overruns() const`.
  - **The burst pattern.** `struct BurstPattern { bool onForCycle(uint32_t cycle) const; }`, built from a list of `{onCycles, offCycles}`. Default `{5,150},{10,150},{25,150},{50,150}`, repeated.
  - **The serial protocol** (lines, at 500 000 baud):
    - Commands:
      - `F <alpha> <gain>` sets the filter (`#OK` or `#ERR <reason>`);
      - `B <on>,<off>[,<on>,<off>…]` sets the pattern;
      - `R <seconds>` records.
    - A recording sends:
      - header lines starting with `#`: `version`, `router`, `board`, `pair`, `conversions`, `spacing_us`, `measured_spacing_us`, `alpha`, `gain`, `pattern`, `load_pin`;
      - then `#BEGIN`, the binary frames, and `#END frames=<n> overruns=<n>`.

- [ ] **Step 1: Write the failing tests**
  - `test_frame_roundtrip`: encode then decode gives the same data, for V 0 and 1023, I out −32768 and 32767, every flag.
  - `test_decode_rejects_bad_sync_flags_range`.
  - `test_ring_buffer_fifo_order`; `test_overrun_flag_on_next_frame`: push N+1 without pop. Then `overruns() == 1` and the next popped frame has the overrun flag; the one after doesn't.
  - `test_burst_default_pattern`: cycles 0–4 ON, 5–154 OFF, 155–164 ON; wraps after the last pair.

- [ ] **Step 2: Run them, see them fail.** `pio test -e native`.

- [ ] **Step 3: Implement the three headers.**

- [ ] **Step 4: Run them, see them pass.**

- [ ] **Step 5: Write the sketch.**
  - The ISR (ADC_vect) reads ADCW, runs the sample path on V. On the recorded I it computes I minus DC, then the filter when on, and pushes one frame per set, using V and I of the same set. ADMUX is written last, for `indexAfterNext`.
  - At a cycle start the load pin takes the pattern's state.
  - `loop()` handles commands and streams the frames. Before `#BEGIN` it measures the set spacing over 1 s with `micros()`.
  - The other load pins are held OFF.
  - `static_assert`s: the ADMUX bit numbers, and the ring buffer size a power of two.

- [ ] **Step 6: Build every env.** Run: `pio run` in `dev/CtRecorder` (all five envs). Expected: SUCCESS, no new warnings, RAM below 1500 B.

- [ ] **Step 7: Commit:** `feat(ct-recorder): the recording sketch, binary frames over a ring buffer`

### Task 3: Python capture, reader and bit-exact sample path

**Files:**
- Create:
  - `analysis/ct_recorder/__init__.py`
  - `analysis/ct_recorder/capture.py` (CLI: `python -m ct_recorder.capture PORT --seconds S [--filter ALPHA GAIN] [--pattern …] -o FILE`)
  - `analysis/ct_recorder/recording.py`
  - `analysis/ct_recorder/firmware_path.py`
  - `analysis/ct_recorder/requirements.txt`: numpy, matplotlib, pyserial, pytest
- Create: `dev/CtRecorder/test/host/golden_main.cpp`. A host program: it reads raw (V, I) pairs and filter settings from stdin and writes I out, using Task 1's `SamplePath` and the firmware's `ct_filter.h`.
- Test: `analysis/ct_recorder/tests/test_recording.py`, `analysis/ct_recorder/tests/test_firmware_path.py`

**Interfaces:**
- Consumes: Task 2 frame and header format.
- Produces:
  - `Recording` (dataclass): `header: dict`, `v_raw`, `i_raw`, `i_out` (numpy int arrays), `load_on`, `filter_on`, `cycle_start`, `overrun` (bool arrays), `dropped_bytes: int`.
  - `read_recording(path) -> Recording`.
  - `firmware_current(v_raw, i_raw, alpha, gain) -> np.ndarray` (int16 I out, bit for bit the C++).

- [ ] **Step 1: Write the failing tests**
  - `test_reads_header_and_frames`: a file built in the test (header lines, `#BEGIN`, 3 frames, `#END`) gives the header dict and arrays of length 3.
  - `test_reader_resyncs_after_garbage`: 5 random bytes before the first frame and a truncated last frame. 3 frames are read and `dropped_bytes == 5 + truncated length`.
  - `test_firmware_path_matches_cpp`: builds `golden_main.cpp` with `g++ -std=c++17 -I dev/CtRecorder -I Mk2_3phase_RFdatalog_temp` (skip if g++ is missing). It feeds 20 s of a synthetic recording: a 50 Hz sine of 300 counts on 515, bursts, a first-order high-pass of τ 15 ms on I, and LCG noise `x = (1103515245·x + 12345) mod 2³¹`. It runs alpha/gain (0, 0), (0.0011, 9), (0.002, 12) and (0.0625 − 1e-6, 63). Asserts `firmware_current` equals the program's output element for element.
  - `test_firmware_path_matches_stream`: on a recording whose `i_out` comes from `golden_main` for alpha 0.002 and gain 12, `firmware_current(v_raw, i_raw, …)` equals `i_out`.

- [ ] **Step 2: Run them, see them fail.** `cd analysis && python -m pytest ct_recorder/tests -q`.

- [ ] **Step 3: Implement** `recording.py`, `firmware_path.py` (a port of `SamplePath` and `LowPassBoost::apply` / `coefficientsOf` with Python ints: truncation towards zero, int16 saturation), `golden_main.cpp`, and `capture.py` (sends the commands, writes everything from the first `#` line to `#END` into the file).

- [ ] **Step 4: Run them, see them pass.**

- [ ] **Step 5: Commit:** `feat(ct-recorder): capture and reader, the firmware's current path bit for bit in Python`

### Task 4: Analysis: fit, filter search, comparison, verdict

**Files:**
- Create: `analysis/ct_recorder/analyse.py` (CLI: `python -m ct_recorder.analyse RUN.ctr [--compare RUN2.ctr] [--plots DIR]`), `analysis/ct_recorder/synthetic.py` (synthetic recordings for tests)
- Test: `analysis/ct_recorder/tests/test_analyse.py`

**Interfaces:**
- Consumes: Task 3 `read_recording`, `firmware_current`.
- Produces:
  - `fit_reference(rec) -> (k, shift_samples)`: least squares of I out ≈ k × V shifted by a fractional `shift_samples` in [−1, 1]. Fitted on ON cycles at least 20 cycles into a burst, after start-up.
  - `burst_energy_errors(rec, i) -> np.ndarray`: per burst, Σ(V×I) against Σ(V × k × V_shifted), relative.
  - `search_filter(rec) -> (alpha, gain, ct_tau_ms)`: minimises the RMS of the burst energy errors, recomputed with `firmware_current`. The search grid is log-spaced in alpha over the allowed range, gain 0 to 63; then refined. `ct_tau_ms` comes from a first-order high-pass fit of the raw switch-on residual.
  - `verdict(errors_off, errors_on) -> str`: "no filter needed" when the RMS improvement is below the standard deviation of the errors over the repeated bursts; otherwise the `calibration.h` lines.
  - Reports on stdout: k, shift, ct_tau_ms, overruns, bursts used, energy errors, the verdict. Plots of one switch-on and one switch-off, raw vs filtered vs reference.

- [ ] **Step 1: Write the failing tests** (synthetic recordings with `synthetic.py`: 3-phase spacing, default pattern, 60 s)
  - `test_finds_filter_for_highpass_ct`: CT high-pass τ 15 ms. The searched filter cuts the burst-energy RMS error by more than half, and `ct_tau_ms` is within 20 % of 15.
  - `test_no_filter_for_ideal_ct`: no high-pass, noise only. The verdict is "no filter needed".
  - `test_compare_reports_improvement`: two recordings (off, and on with the found filter). `--compare` prints both RMS errors and the verdict.
  - `test_skips_startup`: the first 5 s carry a DC offset step. The fit is unchanged (k within 0.5 %) compared with the same recording without it.
  - `test_bursts_with_overrun_excluded`: overrun flags inside one burst. That burst is left out and reported.
  - `test_refuses_without_load_current`: I is noise only. It raises/exits with "no load current".
  - `test_warns_when_tail_not_settled`: τ 2 s with OFF 150 cycles. The output contains a warning naming the bursts.

- [ ] **Step 2: Run them, see them fail.**

- [ ] **Step 3: Implement** `synthetic.py` and `analyse.py`.

- [ ] **Step 4: Run them, see them pass.** `cd analysis && python -m pytest ct_recorder/tests -q`

- [ ] **Step 5: Commit:** `feat(ct-recorder): the analysis, filter search and verdict`

### Task 5: Docs and CI

**Files:**
- Create: `dev/CtRecorder/README.md` (French) and `README.en.md`. Cover: wiring (only the recorded load connected; other loads off), choosing the env, the four-step workflow with exact commands, reading the report, and what to put in `calibration.h`.
- Modify: `analysis/README.md` and `README.en.md` (a section pointing to the tool), `CHANGELOG.md` ([Unreleased], Development), `CLAUDE.md` (root map: `dev/CtRecorder`, `analysis/ct_recorder`).
- Modify: `.github/workflows/build.yml`. A job `ct-recorder`:
  - `pio run` all envs and `pio test -e native` in `dev/CtRecorder`;
  - `pip install -r analysis/ct_recorder/requirements.txt` then `python -m pytest analysis/ct_recorder/tests` (with g++ available).
  - Keep readable `@vX` action tags.

- [ ] **Step 1: Write the docs.** French first.
- [ ] **Step 2: Add the CI job.** Validate with actionlint if available, else a YAML parse.
- [ ] **Step 3: Run everything locally:** the five builds, `pio test -e native` in `dev/CtRecorder`, pytest. Expected: all pass.
- [ ] **Step 4: Commit:** `docs(ct-recorder): how to record and analyse; ci: build and test the tool`
