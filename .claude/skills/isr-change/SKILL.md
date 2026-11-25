---
name: isr-change
description: Use when changing anything the ADC interrupt runs (ISR(ADC_vect) in processing.cpp, the functions it calls, adc_chain.h, ADMUX handling), or when proposing a speed-up of the sampling path, before writing the change.
---

# Changing the ADC interrupt

## The bar a change must clear

- The ISR has 1664 cycles per call. ADMUX is written **last**, for the conversion after next, and
  at least one ADC clock (128 cycles) after its conversion started: `expect admux_delay_min 128`
  in `sim/scenarios/common.scn` stays.
- A change must not add flash, lengthen the **slowest** call (`ADC ISR max`), or bring the ADMUX
  write closer to 128 or to the 1664 deadline. Gains on the average call alone are not wanted:
  `loop()` is not short of CPU.
- Already measured and turned down (2026-11): a `switch` with a constant phase per case (-38 %
  average, +912 B flash, ADMUX 27 cycles above 128); the rare paths `noinline` (-11 % average,
  slowest call +40 cycles, +64 B). Do not propose them again.
- The simulated ADC locks the channel at once: an ADMUX write that is too early shows only in
  the `ADMUX delay` figure, never as misfiled samples.

## Measure: baseline first, same procedure for the change

Keep dev and the change in two trees. For each, in WSL (Git Bash has no simavr; put the steps in
a script file, Git Bash expands `$VARS` first; run it with `MSYS_NO_PATHCONV=1 wsl bash <file>`):

```bash
export PVR_SIM_BUILD=1                 # the banner's length shifts the run: same name everywhere
sh sim/configs/sim_config.sh           # before EVERY sim build: old-board grid, readable datalog
(cd Mk2_3phase_RFdatalog_temp && pio run -e basic)
make -C sim -j"$(nproc)" check         # then check-new, check-new20
```

Other builds: copy `config.h` and `version.h` aside, run `sim_config.sh`, apply the patch
(`git apply --unidiff-zero sim/configs/relays.patch`; in a git worktree under WSL git does not
work: `sed -n '/^diff --git/,$p' <patch> | patch -p1`), build with
`PLATFORMIO_BUILD_DIR=.pio/build-<name>`, restore the two files, run the target. Without
`sim_config.sh` the scenarios fail with "min sample sets ... -1": that is the setup, not your
change.

| Targets | Why |
|---|---|
| `check`, `check-new`, `check-new20` | both boards |
| `check-relays`, `check-rf` | longer calls each mains cycle |
| `check-temp`, `check-new-temp`, `check-combo`, `check-new-combo` (and the `60` variants) | thinnest ADMUX margin, after a 1-Wire slot |

Run the baseline twice: simavr is deterministic, two runs of the same firmware give the same
figures, or nothing can be compared.

## Report and decide

Table per target, dev against the change: ISR average and max, ADMUX delay min and max, misfiled
samples, overruns, flash and RAM (`avr-size`), scenario results. Drop the change when any of the
bar above fails, any scenario that passed fails, or a warning appears. Then also
`pio test -e native`, `pio test -e uno_sim` (WSL), and clang-format 18.1.8. Update the figures in
`Mk2_3phase_RFdatalog_temp/docs/performance.md` if they moved. Port to PVRouter-1-phase only after
measuring there: its ISR is different.
