# CT recorder: measure the CTs, compute the CT filter, check it

Status: design, for approval. Repository: PVRouter-3-phase. A copy goes to PVRouter-1-phase once
the tool is finished.

## Why

The router can add a CT filter to the current samples: an IIR low-pass of the current, added back
with a gain (`lpf_gain`, `alpha` in `calibration.h`, fixed point in `ct_filter.h`). It offsets the
CT acting as a high-pass filter: after a step of current, which burst-fire switching makes at every
load switch, a current-output CT's output sags and then recovers. Robin Emley tuned it for the YHDC
SCT-013-000 with CT1 sampled every 375 µs (`Mk2_fasterControl_2`: gain 12, alpha 0.002). The single-phase
`Mk2_fasterControl_Full` ships 9 / 0.0011.

Those values don't transfer:
- the time between two samples of the same V/I pair is not Robin's 375 µs: 312 µs on the
  single-phase firmware (3 conversions per set), 624 µs on the 3-phase (6 conversions per set);
- the new board uses voltage-output CTs, which may behave differently, or need no filter at all.

So the filter values must be measured: on the old and the new board, for each CT type, at each
sample spacing. The answer may also be "no filter", if the filter makes no difference.

## Goals

- One recording tool for both routers and both boards: the only difference between single-phase and
  3-phase is the time between two samples of a V/I pair, a setting of the tool.
- The workflow:
  1. a run with the filter off, on real hardware;
  2. a script computes the filter from that recording;
  3. the filter is set in the tool, and a second run is recorded;
  4. the script compares the two runs, and gives the values for `calibration.h`, or concludes that
     no filter is needed.
- The reference needs no instrument: a resistive load (the water heater, as Robin used) draws a
  current that is the voltage times a constant, in phase, sinusoidal, and exactly zero when
  switched off. The recorded voltage is the reference the current is compared with.

## Non-goals

- No change to the router firmware: the tool is a separate sketch.
- No real-time display or GUI: a capture script writes a file, an analysis script reads it.
- Not a calibration tool for `f_powerCal`/`f_voltageCal` (those are compared with a meter, as today).

## Anti-goals

- A filter "with filter" that is not exactly what the firmware computes: the tool must run the
  firmware's own `ct_filter.h`, not a copy or a float approximation.
- A recording whose sample timing differs from the firmware's: the ADC must run as the router runs
  it (free-running, 104 µs per conversion, the same number of conversions per set and reference).

## Approaches (chosen: A)

- **A. A standalone recording sketch plus Python scripts.** Chosen by the user ("I don't want to mess
  up the firmware with that"). Firmware untouched; the timing is reproduced by settings; the filter is
  the firmware's `ct_filter.h`.
- B. A recording mode inside the router firmware: exact timing by construction, but a serial stream
  and lab code in the real-time firmware. Rejected.
- C. Snapshots in RAM printed afterwards (Robin's `RawSamplesTool_6chan`, `dev/RST_375us_dev`): no fast
  link needed, but 2 KB of RAM hold about 8 mains cycles of V+I, and the CT's recovery lasts about
  0.2 s (about 10 cycles, from Robin's alpha), so a window misses most of it. Rejected.

## Decided

- **Where:** a PlatformIO project under `dev/` of PVRouter-3-phase (next to `dev/RST_375us_dev` and
  `dev/RST_3phase_free_dev`, the references), and the scripts under `analysis/` (next to the EWMA
  analysis). Docs in French (default) and English, as the rest of the repository.
- **Sampling:** free-running ADC, 104 µs per conversion (prescaler 128, 13 ADC clocks), as the
  firmware. Settings: conversions per set (3 for the single-phase, 6 for the 3-phase), board
  (`OLD`: AVcc reference; `NEW`: internal 1.1 V), which pair is sent. On the 3-phase all six channels
  are converted to keep the timing; only V1/I1 are sent. On the single-phase V, CT1, CT2 are
  converted; V/CT1 is sent, or V/CT2 as a setting, to look at the diverted CT too.
- **Transfer:** the ISR puts each V/I pair (with the load state) into a temporary array, a ring
  buffer; `loop()` streams it to the serial port (the user's suggestion; see `dev/RST_375us_dev`).
  Rates: one pair every 624 µs (3-phase) or 312 µs (single-phase), a few bytes per pair, at most
  about 16 kB/s: 250 000 baud carries it with room to spare. A buffer overrun must be detected and
  reported in the stream, never silent.
- **Load:** the tool drives the load pin itself, in a fixed burst pattern, switching at mains-cycle
  boundaries (the zero-crossing triac driver follows at the next zero crossing), so that the
  recording holds switch-on steps and switch-off tails with known timing, and OFF periods long enough
  to see the CT's tail die out.
- **Filter in the tool:** off for the first run; for the second, `lpf_gain`/`alpha` set as in
  `calibration.h` and applied with the firmware's `ct_filter.h` (the tool includes it from the
  firmware folder). The stream records both the raw current and the filtered one, so one run also
  shows the raw signal.
- **Header of a recording:** tool version, board, conversions per set, pair sent, sample spacing,
  filter values, burst pattern, so the analysis needs no other input.
- **Analysis (Python):**
  - the capture script reads the serial port into a file (pyserial);
  - the analysis script fits the constant between voltage and current on steady ON cycles, then
    computes the error of each current sample against the scaled voltage (zero in OFF cycles);
  - it searches the `alpha`/`gain` pair that minimises that error for the recording's sample
    spacing, within the range `validation.h` accepts (alpha 0.00005 to 0.0625, gain below 64),
    running the same fixed-point filter as the firmware;
  - it reports, with and without the filter: the error, the energy of each burst against the
    "true" energy (from the scaled voltage), and plots of a switch-on and a switch-off;
  - it compares two recordings (filter off and on) and states the verdict: the values for
    `calibration.h`, or "no filter needed" when the improvement is within the noise.

## Settled after the builder check (my calls, for the user to check)

- **The current path, not the filter alone.** The firmware doesn't filter the raw ADC value: it
  removes the DC offset, left-aligns to x64, filters, saturates (`processCurrentRawSample`). The
  tool runs that same path (the firmware's DC removal and `ct_filter.h`). The stream carries the
  raw 10-bit counts of V and I, plus the filtered current when the filter is on; the Python side
  rebuilds the firmware's path from the raw counts, and a test checks it bit for bit against the
  C++ output in the stream.
- **The filter is set at run time, without reflashing.** The capture script sends `alpha`/`gain`
  to the tool before a run (refused outside the range `validation.h` accepts); the tool turns them
  into fixed point with `ct_filter.h`'s own `coefficientsOf()`, so the values are the firmware's.
  Default at power-up: filter off.
- **Pairing and phase.** The tool sends V and I as converted, without `f_phaseCal`. The analysis
  fits the constant and a time shift between V and I together on steady ON cycles (the divider,
  the CT and the conversion order all shift the phase), and only then measures the sag; without
  the shift, a phase error would pass for a CT effect.
- **Metric and verdict.** The decisive figure is the energy of each burst against the "true"
  energy from the scaled voltage: it is what the energy bucket integrates. The waveform error
  around the switchings is reported for diagnosis. "No filter needed" when the filter's
  improvement of the burst-energy error is below the spread of that error over the repeated
  bursts of the same run. One set of values per combination of CT type and sample spacing (old or
  new board, 312 or 624 µs), since `calibration.h` holds one per board and per firmware.
- **The load.** Only the load of the recorded pair is driven: on the 3-phase, the element on
  phase 1 (pin a setting, default the firmware's first load pin); every other load pin is held
  OFF. A zero-crossing triac driver or SSR (the routers' own outputs), not a relay. Mains-cycle
  boundaries are found as the firmware finds them, from the polarity of the DC-removed voltage.
- **Sample spacing.** Free-running, the ADC converts every 13 ADC clocks whatever the ISR does,
  so the spacing is the nominal one; the tool also measures it over a long count with `micros()`
  and writes it in the header, as a check.
- **Results.** The analysis reports the fitted CT time constant as well as `alpha`/`gain`; a gain
  of 0 is a valid answer (for a CT with no measurable sag).
- **Recovery length.** The burst pattern is a setting; the analysis checks that the current is
  back to zero before the end of each OFF period, and warns when it is not (a longer OFF period is
  then needed).

## Left to the builder

- The record format (binary frames with a sync marker are likely needed at 250 000 baud), the size
  of the ring buffer, how overruns are marked.
- The burst pattern's exact lengths (several ON lengths, OFF periods of a few seconds).
- How the fixed-point filter reaches the Python script (a port of `ct_filter.h`'s arithmetic, checked
  bit for bit against the C++ through a test, or the C++ built as a small host program).
- The fitting method (grid search or optimiser) and the threshold behind "within the noise".
- Tests: the tool's pure parts (frame encoding, ring buffer, burst pattern) natively; the analysis on
  synthetic recordings (a simulated first-order CT high-pass applied to a sine with bursts, whose
  time constant the script must find back); the sketch built in CI for both settings.
