# CT recorder

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](README.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](README.en.md)

---

A standalone sketch that records the router's own ADC chain (one voltage and one current, no calibration), and a Python tool (`analysis/ct_recorder/`) that finds the CT filter (`lpf_gain` and `alpha` in `calibration.h`) for your board. It may also conclude that no filter is needed.

> **Not yet tested on a real board.** The sketch and the tool are checked by the native tests, by the simulator and by synthetic recordings, not on a real router.

## Wiring

- Only the load of the recorded pair is connected and driven. The other loads are off.
- The load is driven by pin 5 (the first load of both shipped configurations), through the router's zero-crossing triac or SSR output, not a relay. Pins 6 and 7 are held OFF.
- A resistive load (water heater) is the reference.
- The recorded pair is V1/I1 on the 3-phase router, V/CT1 on the single-phase one (`CTR_PAIR=GRID`). For V/CT2, set `CTR_PAIR=DIVERTED` in `platformio.ini`.
- The recorded CT must see the load's current only. I1 and CT1 are grid CTs: they see the whole house and the PV export. Clamp the CT around the water heater's supply cable only. Otherwise, record with everything else off and no PV (at night, for instance). CT2 (`DIVERTED`) normally sees only the diverted load.

## Choosing the environment

One environment per router and board type (folder `dev/CtRecorder`):

| Environment | Router | Board | Sample spacing |
|---|---|---|---|
| `three_phase_old` | 3-phase | old | 624 µs |
| `three_phase_new` | 3-phase | new | 624 µs |
| `single_phase_old_seg` | single-phase | old, 7-segment display | 312 µs |
| `single_phase_old_oled` | single-phase | old, OLED | 312 µs |
| `single_phase_new` | single-phase | new | 312 µs |

You need one set of values per CT type and sample spacing.

## Workflow

From the repository root. Install the tool (once):

```bash
pip install -r analysis/ct_recorder/requirements.txt
```

1. **Upload the sketch** (change the environment):

   ```bash
   cd dev/CtRecorder
   pio run -e three_phase_old -t upload
   cd ../..
   ```

2. **Record with the filter off**, at least 3 minutes (180 s) with the default burst pattern:

   ```bash
   cd analysis
   python -m ct_recorder.capture COM3 --seconds 180 -o filter_off.ctr
   ```

   `COM3` is the serial port (500 000 baud). Opening the port may reset the board. The sketch then refuses to record for 120 s, and the capture waits. So expect about 2 minutes more per capture. With `--no-wait`, the capture stops with an error instead of waiting. Change the pattern with `--pattern ON,OFF[,ON,OFF...]` (in mains cycles, up to 8 pairs).

3. **Analyse**:

   ```bash
   python -m ct_recorder.analyse filter_off.ctr --plots plots
   ```

   The report gives the filter found and the verdict. With `--plots`, `switch_on.png` and `switch_off.png` show the raw current, the filtered one and the reference for the longest burst.

4. **Check with the filter** (values from the verdict), then compare:

   ```bash
   python -m ct_recorder.capture COM3 --seconds 180 --filter 0.004626 9.344 -o filter_on.ctr
   python -m ct_recorder.analyse filter_off.ctr --compare filter_on.ctr
   ```

   `0.004626` and `9.344` are example values, from a synthetic recording. Use those your analysis prints. `--filter ALPHA GAIN` is accepted for alpha from 0.00005 to below 0.0625 and a gain from 0 to below 64. A gain of 0 turns the filter off (the state at power-up).

## Rules

- The sketch refuses to record (`#ERR warming up`) during the first 120 s after power-up or a reset. Opening the serial port may reset the board (depending on the board and the driver). The firmware's DC-offset filter settles with a time constant of about 20 s on the 3-phase spacing (about 10 s single-phase). The capture waits and retries, except with `--no-wait`.
- Record at least 3 minutes with the default pattern (at least about 10 bursts). With fewer, the CT time-constant estimate is unreliable.
- Commands are not read while a recording runs (64-byte serial input buffer).

## Reading the result

The analysis measures the energy error over the ON cycles of each burst. For a linear CT, the switch-off tail mirrors the switch-on sag, so over a burst plus its tail the error cancels (the "with tail" column, near 0). The router's energy bucket integrates every cycle, so a CT's sag does not bias the long-run energy. The filter puts the energy in the right cycles, which matters for the router's cycle-by-cycle decisions.

- **"no filter needed"**: the improvement is within the burst-to-burst spread. Leave `lpf_gain` at 0.
- **"filter needed"**: the verdict prints the two lines to paste into `Mk2_3phase_RFdatalog_temp/calibration.h`:

  ```cpp
  inline constexpr float lpf_gain{ 9.344 }; /**< setting this to 0 disables this extra processing */
  inline constexpr float alpha{ 0.004626 };
  ```

  These are example values, from a synthetic recording. Use those your analysis prints.

- The fitted V/I shift is limited to ±1 ms, with a warning at the limit.
- If the current is not proportional to the voltage (the load is not resistive, or the CT is on another phase), the analysis refuses and gives no filter.
- A note says when the end of the CT tail cannot be confirmed at 10 bits (tail too short against the resolution).
- Bursts in the first 10 s, with a buffer overrun, or without current (the water heater's thermostat cut out), are left out and listed. With fewer than 4 bursts left, the analysis refuses.

## Files

- `main.cpp`: the sketch (commands `F` filter, `B` pattern, `R` duration, header lines, `#BEGIN`/`#END`).
- `sample_path.h`, `chain_layout.h`, `burst_pattern.h`, `frame.h`, `ring_buffer.h`: the sample path as the firmware runs it, the channel layouts, the pattern, the frame format, the buffer.
- `test/`: native tests (`pio test -e native`).
- The firmware is not modified: the sketch includes `ct_filter.h` and `mult_asm.h` from `Mk2_3phase_RFdatalog_temp/`.
