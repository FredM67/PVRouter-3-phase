# EWMA Filter Tuning Guide for Cloud Immunity

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](EWMA_TUNING_GUIDE.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](EWMA_TUNING_GUIDE.en.md)

---

## Overview

The relays do not switch on the instant power, but on an average of it, so that a cloud passing over the panels does not make them chatter. The average is a triple exponential moving average (TEMA, `getAverageT()` in `ewma_avg.hpp`), used by `relay_engine.h`.

## The setting

The length of the average, in minutes, is `RELAY_FILTER_DELAY` in `config.h`, given to the relays with `MINUTES()`. As shipped:

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY{ 2 }; /**< EWMA filter delay in minutes for relay control */

inline constexpr RelayEngine relays{ MINUTES(RELAY_FILTER_DELAY),
                                     { { unused_pin, 100, 200, 1, 1 },
                                       { unused_pin, 300, 400, 1, 1 },
                                       { unused_pin, 500, 600, 1, 1 } } };
```

The quick reference of `config.h`:

| Region type | `RELAY_FILTER_DELAY` |
|-------------|----------------------|
| Clear sky (desert, dry) | 1 minute: fast response |
| Mixed conditions (most locations) | 2 minutes: recommended default |
| Frequently cloudy (coastal) | 3 minutes: more stability |
| Very cloudy (mountain, tropical) | 4 minutes: maximum stability |

The build checks that the average spans at least 5 datalog periods
(`RELAY_FILTER_DELAY * 60 / DATALOG_PERIOD_IN_SECONDS >= 5`, in `validation.h`), and, with relays,
that `DATALOG_PERIOD_IN_SECONDS` divides 60: 1 minute needs a datalog period of 12 s or less.

## How to tune

1. Change `RELAY_FILTER_DELAY` in `config.h`.
2. Build and upload the firmware.
3. Watch the relays for a few days:
   - relays switching on and off during clouds: increase it;
   - diversion starting too late after the sun comes back, or slow response to consumption: decrease it.

With relay diversion, the datalog prints the average the relays use: `R` in the IoT and JSON
outputs, after the total power (`P:<power>/<average>`) in the human-readable output.

## Technical details

- One value per datalog period (`DATALOG_PERIOD_IN_SECONDS`, 5 s by default).
- The template parameter of the average is `RELAY_FILTER_DELAY * 60 / DATALOG_PERIOD_IN_SECONDS`
  (`relay_engine.h`); its smoothing factors are rounded to powers of two, so the arithmetic is
  shifts only (`ewma_avg.hpp`).

## Battery systems

With a battery, the import threshold of a relay is negative: the relay turns OFF when the surplus
drops below its absolute value. From the comments of `config.h`:

```cpp
//   Battery installation: { pin, 1000, -50, 5, 5 }   // Turn OFF when surplus < 50W
```

See the [battery configuration guide](../../Mk2_3phase_RFdatalog_temp/docs/BATTERY_CONFIGURATION_GUIDE.en.md).

## Other filters

`ewma_avg.hpp` also gives a single EMA (`getAverageS()`, the most responsive) and a double EMA
(`getAverageD()`). The relays use the TEMA; the call is in `get_average()` and `proceed_relays()`
of `relay_engine.h`.

For a comparison of the three filters, see the [TEMA analysis](TEMA_ANALYSIS_README.en.md):

![Filter Comparison](../plots/ema_dema_tema_comparison_extended.png)
