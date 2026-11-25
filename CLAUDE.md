# CLAUDE.md

This file gives Claude Code (claude.ai/code) its bearings in this repository. The firmware has its
own, more detailed guide: [`Mk2_3phase_RFdatalog_temp/CLAUDE.md`](Mk2_3phase_RFdatalog_temp/CLAUDE.md).

## What is where

| Folder | Content |
|---|---|
| `Mk2_3phase_RFdatalog_temp/` | The router firmware (ATmega328P, PlatformIO), its native and embedded tests |
| `RemoteLoadReceiver/` | Firmware of the RF remote unit that drives remote loads |
| `sim/` | `grid_sim`: the unmodified firmware in simavr on a simulated grid, with scenarios and CI targets ([README](sim/README.md)) |
| `scripts/ci/` | CI helpers: `compile-fail.sh` (settings that must be refused by a `static_assert`), benchmark reports |
| `scripts/` | Git hooks (pre-commit framework or plain hooks, [README](scripts/README.md)) |
| `configurator/` | Browser configurator that writes `config.h` (plain JS, Node tests) |
| `analysis/`, `dev/` | Measurement analysis and development sketches; not part of the build |
| `dev/CtRecorder/`, `analysis/ct_recorder/` | CT recorder: a sketch recording the ADC chain (own PlatformIO envs and native tests) and the Python tool that finds the CT filter ([README](dev/CtRecorder/README.en.md)); built and tested by the `ct-recorder` CI job |
| `docs/`, `lang/`, `pages/`, `img/`, `schematics/` | Wiring diagrams, translated docs, GitHub Pages, pictures, schematics |
| `obsolete/` | Old code kept for reference; do not update |

## Commands

```bash
# Firmware (from Mk2_3phase_RFdatalog_temp/)
pio run -e basic                 # build
pio test -e native               # native unit tests (needs gcc)

# Settings that must be refused at compile time (from the repo root)
bash scripts/ci/compile-fail.sh

# Grid simulator (Linux/WSL, needs simavr): see sim/README.md
PVR_SIM_BUILD=1 sh sim/configs/sim_config.sh   # before every firmware build for the simulator
make -C sim check                # also check-new, check-new20, check-relays, check-rf

# Configurator
node --test configurator/test/*.test.js
```

On Windows, the native tests, the compile-fail checks and the simulator run in WSL: Git Bash has
no gcc and no simavr.

## Rules

- **Branches:** `main` (releases) and `dev` (integration) are protected by rulesets. Work on a
  topic branch (`feat/…`, `fix/…`, `docs/…`, `ci/…`, …); personal branches go under `users/`.
  Pull requests target `dev`, never `main`.
- **Commits:** conventional commits (`feat:`, `fix:`, `docs:`, `test:`, `ci:`, `refactor:`,
  `chore:`); `!` marks a change that breaks existing settings.
- **Never commit** the local changes to `Mk2_3phase_RFdatalog_temp/version.h` (rewritten by
  every build) or `Mk2_3phase_RFdatalog_temp/.vscode/extensions.json` (rewritten by VS Code).
- **Formatting:** clang-format 18 (`.clang-format`), applied by the pre-commit hook; CI checks it.
  A fresh clone has no hook until `scripts/install-hooks.sh` (or `pre-commit install`) runs:
  without it, run `clang-format -i` (18.1.8, as CI) on the changed files before committing.
- **Real time:** the ADC interrupt has 1664 CPU cycles per call. ADMUX is written last in the
  ISR, for the conversion after next; no busy-wait and no allocation in the ISR. Any
  ISR change is measured in the simulator (`ADC ISR max`, `ADMUX delay`, `misfiled samples`).
  An ISR change must not add flash, lengthen the slowest call or bring the ADMUX write closer
  to its limits (`admux_delay_min 128` stays). Gains on the average call alone are not wanted:
  `loop()` is not short of CPU. Already measured and turned down: a `switch` per phase in the
  ISR, and the rare paths moved out of line (`noinline`).
- **Pins:** no feature on pins 14 and above (A0–A5 are the analog inputs).
- **Warnings:** the build must stay warning-free. Keep the narrow types (`uint8_t`, …); when a
  loop count can be 0, write the loop with `!=` rather than casting to `int`.
- **Settings:** every user setting is checked at compile time in `validation.h`; a new check
  gets a case in `scripts/ci/compile-fail/`. A setting users see also needs its page in the
  documentation repository (FredM67/Mk2PVRouter, French docs), in a PR there or an issue that
  the PR links to.
