# Contributing Guide

How to work on the router firmware. The workflow of the repository (branches, pre-commit hooks)
is also in [CONTRIBUTING.md](../../CONTRIBUTING.md); the rules below come from
[CLAUDE.md](../../CLAUDE.md), which the maintainers follow too.

## Setup

### PlatformIO (recommended)

```bash
git clone https://github.com/FredM67/PVRouter-3-phase.git
cd PVRouter-3-phase/Mk2_3phase_RFdatalog_temp
pio run -e basic          # build
pio test -e native        # native unit tests (needs gcc)
```

PlatformIO fetches the libraries itself (`platformio.ini`). On Windows, the native tests, the
simavr tests, the compile-fail checks and the grid simulator run in WSL: Git Bash has no gcc and
no simavr.

### Arduino IDE

Install **OneWire** and **RFM69** (LowPowerLab) with the Library Manager, then open
`Mk2_3phase_RFdatalog_temp.ino`: see
[Required libraries for Arduino IDE](../Readme.en.md#required-libraries-for-arduino-ide).

## Code rules

- C++17, `constexpr`, `enum class`, `static_cast`; camelCase for variables and functions,
  UPPER_CASE for constants, PascalCase for types.
- Include `config_derived.h`, never `config.h`: `config.h` holds the settings only
  (see [configuration.md](configuration.md)).
- No dynamic allocation, and no float arithmetic at run time: the float calibration values are
  turned into fixed-point constants at compile time; the energy bucket, the datalog and the CT
  filter are integer code.
- Keep the narrow types (`uint8_t`, ...). The build must stay warning-free; when a loop count can
  be 0, write the loop with `!=` rather than casting to `int`.
- No feature on pins 14 and above (A0-A5 are the analog inputs).
- Formatting: clang-format (`.clang-format`), applied by the pre-commit hook and checked by CI.
- A new file starts with the header the others have (`@file`, `@author`, `@brief`, `@version`,
  `@date`, `@copyright` and `SPDX-License-Identifier: GPL-3.0-or-later`); the pre-commit hook
  updates `@date` and `@copyright`.

### The ADC interrupt

`ISR(ADC_vect)` (`processing.cpp`) has 1664 CPU cycles per call, one ADC conversion (104 µs).

- ADMUX is written last in the ISR, for the conversion after next (`adc_chain.h`).
- No busy-wait, no allocation, no serial output in the ISR.
- Any ISR change is measured in the grid simulator (`ADC ISR max`, `ADMUX delay`,
  `misfiled samples`; [sim/README.md](../../sim/README.md)), with the procedure of
  [.claude/skills/isr-change](../../.claude/skills/isr-change/SKILL.md). The current figures are in
  [performance.md](performance.md).

### Settings

Every user setting is checked at compile time in `validation.h`, with a `static_assert` and a
message that says what to fix. A new check gets a case in `scripts/ci/compile-fail/` (see
[testing.md](testing.md)).

## Before a pull request

Run the tests your change touches ([testing.md](testing.md)):

```bash
pio test -e native                  # from Mk2_3phase_RFdatalog_temp/
pio test -e uno_sim                 # embedded tests in simavr (Linux/WSL)
bash scripts/ci/compile-fail.sh     # from the repository root (Linux/WSL)
make -C sim check                   # grid simulator, see sim/README.md
```

Never commit the local changes to `version.h` (rewritten by every build) or
`.vscode/extensions.json` (rewritten by VS Code).

## Branches, commits, pull requests

- `main` (releases) and `dev` (integration) are protected. Work on a topic branch: `feat/...`,
  `fix/...`, `docs/...`, `ci/...`; personal branches under `users/`.
- Conventional commits: `feat:`, `fix:`, `docs:`, `test:`, `ci:`, `refactor:`, `chore:`; `!`
  marks a change that breaks existing settings.
- Pull requests target `dev`, never `main`, with the
  [pull request template](../../.github/pull_request_template.md).
- A change that applies to the single-phase firmware (PVRouter-1-phase) gets a matching pull
  request there; each pull request says whether it was ported, or why not.

CI (`.github/workflows/build.yml`) builds the firmware in several configurations and runs the
native tests, the embedded tests in simavr, the compile-fail checks, the grid simulator scenarios,
the CT recorder and configurator tests, and posts a performance report (flash, RAM and benchmark
cycles against the target branch) on the pull request. Other workflows check the formatting and
the spelling, and run CodeQL.
