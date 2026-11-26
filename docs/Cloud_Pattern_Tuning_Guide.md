# Cloud Pattern Analysis Guide

## Overview

This guide helps you choose `RELAY_FILTER_DELAY` in `config.h` for your climate. The relays do
not decide on the instant grid power but on its average: a triple EMA (TEMA, `getAverageT()` in
`ewma_avg.hpp`) of the power of each datalog line, over `RELAY_FILTER_DELAY` minutes
(`relay_engine.h`). A longer delay rides through clouds; a shorter one follows real changes
sooner.

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY{ 2 }; /**< EWMA filter delay in minutes for relay control */
```

The shipped `config.h` uses 2 minutes. The build refuses a delay that spans fewer than 5 datalog
periods (`RELAY_FILTER_DELAY * 60 / DATALOG_PERIOD_IN_SECONDS >= 5`, `validation.h`): with the
default 5 s period, 1 minute is allowed. It also refuses a delay over 255 datalog periods
(21 minutes at 5 s): the filter takes the count as an 8-bit value.

## Configuration guidelines

The same table as in `config.h`:

| Climate | `RELAY_FILTER_DELAY` |
|---|---|
| Clear sky (desert, dry) | 1 minute: fast response, few clouds |
| Mixed conditions (most locations) | 2 minutes: the default |
| Frequently cloudy (coastal) | 3 minutes: more stability |
| Very cloudy (mountain, tropical) | 4 minutes: the most stability |

If a relay chatters on partly cloudy days, lengthen the delay. If it follows real changes too
slowly, shorten it. The minimum ON and OFF times of each relay, and the 60 s the engine waits
after any relay change, also limit how often a relay switches.

## Visualizing the filter

`Mk2_3phase_RFdatalog_temp/scripts/visualize_cloud_patterns_demo.py` plots the TEMA of delays of
1 to 5 minutes on simulated clouds, and the relay switches they give with a simplified relay
(see [its README](../Mk2_3phase_RFdatalog_temp/scripts/README.md)):

```bash
cd Mk2_3phase_RFdatalog_temp
python3 scripts/visualize_cloud_patterns_demo.py
```

## Checking a setting with the grid simulator

The grid simulator runs the unmodified firmware on a simulated grid, relay engine included: the
TEMA, the minimum ON and OFF times and the settle time
([`sim/README.md`](../sim/README.md)). The scenarios of `sim/scenarios/relays` need a firmware
built with `sim/configs/relays.patch` (two relays, a 1-minute filter); `sim/scenarios/clouds.scn`
runs a cloudy afternoon on the triac loads. To try your own relays and delay, build the firmware
with them and write a scenario: `at` lines for the surplus, `relay` lines for the loads on the
relays, and `expect switches <pin> <min> <max>` to count the switch-ons (see the
[Relays](../sim/README.md#relays) and [Scenarios](../sim/README.md#scenarios) sections).

## Battery systems

With a home battery, use a negative import threshold: the relay then turns OFF when the surplus
falls below the threshold's absolute value (`-100`: OFF below 100 W of surplus), instead of
waiting for an import that the battery hides. The surplus threshold must then be at least the
load's power + |import threshold|, or the relay turns itself OFF. Details in the
[battery guide](../Mk2_3phase_RFdatalog_temp/docs/BATTERY_CONFIGURATION_GUIDE.en.md).

```cpp
inline constexpr bool RELAY_DIVERSION{ true };

inline constexpr uint8_t RELAY_FILTER_DELAY{ 2 };

// 1000 W load: ON above 1100 W of surplus, OFF below 100 W of surplus
inline constexpr RelayEngine relays{ MINUTES(RELAY_FILTER_DELAY), { { 8, 1100, -100, 5, 5 } } };
```
