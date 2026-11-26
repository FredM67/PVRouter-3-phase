# Configuration System

How the settings reach the firmware, and how the build checks them. The settings themselves, one
by one, are described in [Readme.en.md](../Readme.en.md#program-configuration).

## Design

Every setting is a compile-time constant, and every one is checked by the compiler: a wrong
setting stops the build with a message saying what to fix, instead of misbehaving on the board.
The checks take no flash and no RAM.

## Configuration headers

```
config.h              ← The settings, and nothing else (the file users edit, or the configurator writes)
└── config_types.h    ← What config.h is written with: types, helpers, target codes (no setting, no Arduino)

config_derived.h      ← What the code includes, never config.h directly
├── config_system.h   ← System-wide constants
├── config.h
├── types.h, debug.h  ← debug.h reads SERIAL_OUTPUT_TYPE and ENABLE_DEBUG from config.h itself
├── calibration.h     ← Hardware calibration values, per board (PCB_VERSION from config.h)
├── NO_OF_REMOTE_LOADS, NO_OF_REMOTE_UNITS, remoteLoads
├── relayDiverter, overrideTable, boostSchedule, tempSensors
│                     ← built from relays, overridePins, scheduledBoosts, temperatureSensing
└── remote_loads.h

validation.h          ← static_assert on the settings (includes config_derived.h; included by main.cpp)
```

config.h has no `#include` but `config_types.h` and computes nothing, so no line of it depends on a
header included before it, and the configurator writes it whole. `RelayEngine`, `OverridePins`,
`ScheduledBoosts` and `TemperatureSensing` only hold what config.h says; the objects the firmware works
with are built from them in config_derived.h.

`LOAD(n)`, `RELAY(n)` and `ALL_LOADS()` cannot give a pin in config.h: which pin load #1 is, and which
relays exist, is only known once config.h is read. They give a code (`TargetCode` in config_types.h),
and `TargetMap` (utils_override.h) turns each code into its pins against the load map and the relays.
A plain number is a pin, as before. The native test `test_config_h` builds config.h alone on the PC.

A config.h of the former layout (with `#include` lines and derived values) stops the build with an
`#error` that says how to update it (`config_derived.h`, `debug.h`).

## Features are constants, not macros

The features are `inline constexpr` settings, such as `inline constexpr bool DUAL_TARIFF{ false };`
or `inline constexpr bool ENABLE_DEBUG{ true };`. The code tests them with `if constexpr`, and the
code of a feature that is off is not built; `#ifdef` never sees them.

## What the build checks

`validation.h` holds the checks, as `static_assert`s on the settings and on what config_derived.h
builds from them. Among them:

- the system values: `SUPPLY_FREQUENCY` 50 or 60 Hz, `DATALOG_PERIOD_IN_SECONDS` 30 s at most, the
  calibration values (`f_powerCal`, `f_voltageCal`, the CT filter);
- each `..._PRESENT` feature against its pin: a pin set if and only if the feature is on;
- the pins: none used twice, not 0 and 1 (serial), not 14 and above (the analog inputs), not the
  RF module's pins when it is present;
- the load map and priorities: at least one load, a pin in 2..13 for each local load, remote units
  1 to 3, the loads per remote unit, one priority per load;
- the relays: their pins, the minimum times, `RELAY_FILTER_DELAY` spanning at least 5 datalog
  periods;
- the targets of the override pins, the scheduled boosts (each window within the off-peak period)
  and the rotation groups;
- the temperature sensors: `BOOST_TEMP_SENSOR` among them, and their reads within a datalog period;
- the RF node and network IDs.

Each message ends with what to check, e.g.
`******** Duplicate pin definition ! Please check your config ! ********`.

`scripts/ci/compile-fail.sh` builds the router with settings that must be refused and checks each
message; a new check gets a case in `scripts/ci/compile-fail/` (see [testing.md](testing.md)).
