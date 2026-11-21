# Changelog

All notable changes to the router firmware (`Mk2_3phase_RFdatalog_temp`), the remote load
firmware (`RemoteLoadReceiver`) and their tooling. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

Changes on `dev`, not yet on `main`.

### Upgrading from `main`

If you keep your own `config.h`, check these points:

- **`config.h` holds the settings only (breaking):** its `#include` lines, the values derived
  from the load map and `remoteLoads` move to `config_derived.h`, which the code includes
  instead. The settings lines themselves do not change. A `config.h` of the former layout stops
  the build with *This config.h has the former layout*. Create it again with the configurator,
  or edit it by hand:
  - replace `#ifndef CONFIG_H`, `#define CONFIG_H`, `#include "config_system.h"` and
    `#include "types.h"` with `#pragma once` and `#include "config_types.h"`;
  - delete the other `#include` lines (`debug.h`, `remote_loads_core.h`, `utils_dualtariff.h`,
    `utils_relay.h`, `utils_temp.h`, `utils_override_helpers.h`, `remote_loads.h`) and the
    final `#endif`;
  - delete `NO_OF_REMOTE_LOADS`, `NO_OF_REMOTE_UNITS`, `REMOTE_LOADS_PRESENT` and `remoteLoads`.

  `LOAD()`, `RELAY()` and `ALL_LOADS()` are written as before; a group and single targets can
  now share a list: `{ ALL_LOCAL_LOADS(), RELAY(0) }` ([#238]).
- **Motherboard:** add `inline constexpr PcbVersion PCB_VERSION{ PcbVersion::OLD };` after
  `#include "config_types.h"`, or `NEW` for the universal 3phaseDiverter board, then recalibrate
  if you choose `NEW`.
- **Shipped `config.h`:** it now targets the new board with the mk2Wifi module
  (`PcbVersion::NEW`, `SerialOutputType::IoT`), as does the configurator by default. With the
  old board, set `PcbVersion::OLD`; for the serial monitor, `SerialOutputType::HumanReadable`.
- **`ENABLE_DEBUG`** is now a setting like the others: replace `#define ENABLE_DEBUG` with
  `inline constexpr bool ENABLE_DEBUG{ true };` (or `false`). It only controls the debug
  messages now; the startup configuration is always printed in human-readable mode ([#180]).
- **mk2Wifi / ESPHome:** `EMONESP_CONTROL` and the `emonesp` PlatformIO environment are gone.
  For the mk2Wifi module, set `SERIAL_OUTPUT_TYPE = SerialOutputType::IoT` and choose its pins
  freely in `config.h` ([#180]).
- **mk2Wifi YAML:** the `mk2pvrouter` component is now built into ESPHome (2026.10.0 and
  later): remove the `external_components` block, write `tag:` instead of `tag_name:` in the
  sensors, and remove `update_interval` under `mk2pvrouter:`. The configurator writes it so.
- **Diversion pin:** with the pin LOW, forced loads *and* forced relays and remote loads now stay
  ON (before, only forced local TRIACs did). For an absence where nothing must heat, not even with
  off-peak forcing, use the new router OFF pin ([#179]).
- **RF:** RF datalogging and remote loads use an RFM69 module with the LowPowerLab RFM69 library.
  The RFM12B / JeeLib stack is no longer supported. The RF settings are in `config_rf.h` ([#155]).
- **Calibration:** the `cal_CTx_v_meter` sketch is gone. Calibrate with the router firmware
  itself, with `CALIBRATION_MODE` set (see the README) ([#178]).
- **Arduino IDE:** the ArduinoJson library is no longer needed ([#172]).
- **Calibration:** `calibration.h` has a set of values per board, chosen by `PCB_VERSION`.
  Old board: copy your former `f_powerCal` and `f_voltageCal` values into `f_powerCalOldBoard`
  and `f_voltageCalOldBoard`.
- **Dual tariff, scheduled boosts (breaking):** `rg_ForceLoad` (with `pairForceLoad` and
  `rg_OffsetForce`) is replaced by `scheduledBoosts`; there is no compatibility layer. Rewrite each
  load's pair as a boost, with explicit units (`HOURS()`, `MINUTES()`) and its target:

  | Before | After |
  |--------|-------|
  | `{ -3, 2 }` | `{ BEFORE_END(HOURS(3)), FOR(HOURS(2)), { LOAD(n) } }` |
  | `{ 2 }` or `{ 2, UINT16_MAX }` | `{ FROM_START(HOURS(2)), { LOAD(n) } }` (no `FOR`: until the end) |
  | `{ -3, UINT16_MAX }` | `{ LAST(HOURS(3)), { LOAD(n) } }` |
  | `{ -150, 90 }` | `{ BEFORE_END(MINUTES(150)), FOR(MINUTES(90)), { LOAD(n) } }` |
  | `{ 0, 0 }` (never boosted) | leave the load out of `scheduledBoosts` |

  A load left out of `rg_ForceLoad` used to be boosted all night; what is not listed in
  `scheduledBoosts` is never boosted. To keep the whole-night boost, list it with
  `{ FROM_START(MINUTES(0)), { LOAD(n) } }`. The shipped `config.h` now boosts only load #1.
  `ul_OFF_PEAK_DURATION` must match the meter's off-peak period: it places `LAST` and
  `BEFORE_END`. The configurator writes the new form (see "Changed"). Also add
  `inline constexpr uint8_t BOOST_TEMP_SENSOR{ 0 };` after `iTemperatureThreshold`: the sensor
  checked against the threshold (0, the first, as before).
- **`rotationGroups` (breaking):** every `config.h` needs this line after
  `loadPrioritiesAtStartup`, or the build stops with *'rotationGroups' was not declared*. Without
  rotation (`PRIORITY_ROTATION` is `OFF`), write `inline constexpr RotationGroups rotationGroups{};`.
  With rotation, list the loads that take turns:
  `inline constexpr RotationGroups rotationGroups{ { ALL_LOCAL_LOADS() } };` for one water heater
  on every local load (what rotation did before, when every load is local). An empty list with
  rotation on stops the build with *Priority rotation is on but rotationGroups is empty*.

No change needed for: `f_phaseCal`, which keeps its meaning ([#127], [#163]),
and load pin lists such as `{ 5, 6, 7 }`, which are still
valid load maps ([#160]).

### Added

- **Configurator:** a web page that writes the router's `config.h`, `config_system.h` and
  `config_rf.h`, the remote units' files and the ESPHome YAML of the mk2Wifi module, and checks
  the choices as the compiler would (plus the mk2Wifi wiring). English and French, works offline.
  Its mk2Wifi controls are fail-safe: a broken wire or a rebooting module leaves the router
  routing, without boost. With Chrome or Edge it saves straight into the firmware folder, with a
  backup and an undo. In calibration mode, it computes the power and voltage values of the chosen
  board from the router's and a reference meter's readings, and writes them into your `calibration.h`.
  <https://fredm67.github.io/Mk2PVRouter/configurateur/>
- **New motherboard:** `PCB_VERSION` in `config.h` selects the board. `NEW` (universal
  3phaseDiverter board, rev. 6.0 and later) switches the ADC to the internal 1.1 V reference the
  board is designed for; `OLD` keeps AVCC. The startup summary shows the choice.
- **Power calibration per amp of CT rating, new board:** `f_powerCal` = `f_powerCalPerAmp` ×
  `f_ctRating`, so changing a CT only means entering its rating. The default values suit the new board with
  333 mV voltage-output CTs; they come from its design (`docs/calibration.md`).
- **Remote loads over RF:** up to 8 loads on up to 3 remote units, each an Arduino with an RFM69
  running the new `RemoteLoadReceiver` firmware ([#155], [#160]). Local and remote loads are
  declared in one load map (`Load::local(pin)`, `Load::remote(unit)`), in any order, and share
  the same priorities and rotation. A unit that misses a frame catches up within 5 mains cycles.
- **Override pins** can force local loads, remote loads and relays: `LOAD(n)`, `RELAY(n)`,
  `ALL_LOADS()`, `ALL_RELAYS()`, `ALL_LOADS_AND_RELAYS()` ([#155], [#160]).
- **Router OFF pin** (`ROUTER_OFF_PIN_PRESENT` / `routerOffPin`, active LOW): every load and relay
  is OFF, forcing included, while the router keeps measuring and logging ([#179]).
- **Calibration mode** (`CALIBRATION_MODE` in `config.h`): the router measures and logs as usual
  but never switches anything, so it can be calibrated with its loads connected. The README
  describes the procedure ([#178]).
- **mk2Wifi documentation:** the ESP32-C6 module for Home Assistant / ESPHome replaces the old
  ESP32 extension board in the READMEs ([#164]).

### Changed

- **Priority rotation** rotates the loads of `rotationGroups` only, any number of groups, each
  among the priority places its loads hold at startup: two 3-phase water heaters each take turns
  on their own, and loads in no group keep their place. A group holds at least 2 loads, local
  loads only or the loads of one remote unit, and a load is in one group at most; the compiler
  and the configurator check it. The configurator has a checkbox per load in each group, and the
  startup summary prints the groups.
- **`config.h`** only holds settings: it includes `config_types.h` and nothing else, and no line
  depends on a header included before it. `config_derived.h` builds the rest from it: the derived
  values, `remoteLoads`, the relay engine, the override and boost bitmasks, the temperature
  sensors. `LOAD()`, `RELAY()` and `ALL_...()` give codes that it turns into pins once the load map
  and the relays are known, so a list may mix a group and single targets. The firmware is the same,
  instruction for instruction ([#238]).
- **Scheduled boosts** replace the per-load forcing of the dual tariff: a list of `{ window,
  { targets } }` entries, written with `FIRST()`, `LAST()`, `FROM_START()`, `BEFORE_END()`, `FOR()`
  and `HOURS()` / `MINUTES()`, and the same targets as the override pins. Remote loads and relays
  can now be boosted, a target may be in several boosts, and a boost that never runs, or that
  starts or ends outside the off-peak period (`ul_OFF_PEAK_DURATION`), is rejected at compile
  time. Every boost stops when the meter switches back to peak. The configurator has a
  "Scheduled boosts" list in place of the per-load forcing fields, and the startup summary prints
  one line per boost.
- **Temperature threshold of the boosts:** `BOOST_TEMP_SENSOR` chooses which sensor is checked
  against `iTemperatureThreshold` (it was always the first). In the configurator, the threshold
  is hidden without temperature sensing on the router, and a sensor can be chosen when there
  are several.
- **Configurator, temperature probes:** read by the router on the new board, they are on `D3`,
  wired with its pull-up on the board, so the pin is no longer a choice there. On the old board,
  the pin is still chosen, with a reminder to wire the pull-up and the probes' power.
- **Configurator, probes read by the mk2Wifi module:** the addresses are optional, ESPHome finds
  the probes on the bus. With all of them, the YAML keeps each name on its probe (`address`);
  otherwise it tells several probes apart by `index`.
- **`RELAY(n)`** with relay diversion off, or beyond the configured relays, now fails to compile,
  in the override pins and in the boosts, and so does `LOAD(n)` beyond the configured loads, with
  the message of `validation.h` instead of an array-bounds error. `REMOTE_LOAD(n)` from 8 on fails
  too: the target masks hold 8 remote loads, and it used to force nothing. `MINUTES()` is one macro shared
  by the relays and the boosts; `HOURS()` is built on it, so too many hours fail to compile
  instead of wrapping around.
- **One meaning for "diversion OFF":** the diversion pin stops the surplus diversion on TRIACs,
  relays and remote loads alike; forcing (override pins, dual tariff) still works ([#179]).
- **Control inputs at power-up:** the diversion and router OFF pins are read once in `setup()`,
  so a pin held at power-up is honoured from the first load decision ([#179]).
- **Relay forcing** goes through the relay engine and its minimum ON/OFF times: forcing several
  relays at once takes one settle time per relay ([#179]).
- **Text output:** informational messages (startup configuration, status changes) are printed
  whenever the output is human-readable; debug messages also need `ENABLE_DEBUG`. In IoT and
  JSON modes, nothing but the data is sent ([#180]).
- **Non-blocking datalog:** the serial output is written a piece at a time when the UART has
  room, so the main loop no longer waits ~130 ms for each line. A datalog is skipped if the
  previous one is still being sent ([#172]).
- **JSON output** is written without ArduinoJson; temperatures no longer show float noise
  (`-9.97` instead of `-9.969999`) ([#172]).

### Fixed

- **Relays in battery mode:** on a small surplus, they went out first relay first; they now go
  out in decreasing order, as on an import. A minimum ON or OFF time beyond 1092 minutes wrapped
  silently (1093 minutes gave 44 seconds); it is now refused at compile time ([#237]).
- **Temperature:** -55.00 and +125.00 °C were reported out of range, and after one CRC error a
  steady 85.00 °C was dropped every second as the sensor's power-on value. It is now compared
  with the sensor's last valid reading ([#234]).
- **Remote load receiver with the Arduino IDE:** it did not build (`utils_pins.h: No such
  file or directory`), since it took `utils_pins.h` and `utils_bits.h` from the router's folder,
  which only PlatformIO could see. The receiver now has its own copies, checked against the
  router's by CI, which also builds the receiver with the Arduino CLI. Its README explains the
  Arduino IDE setup and the one-copy-per-unit layout ([#185]).
- **Stuck relay:** a relay forced by an override pin could stay ON for good after the override
  was released ([#179]).
- **Dual tariff:** automatic forcing never triggered, and the temperature threshold was ignored
  ([#166]).
- **Dual tariff with remote loads:** out-of-bounds read; remote loads always datalogged 0 ([#160]).
- **Relay pins** on the loads' port (D2-D7) could briefly write back an old load state ([#175]).
- **Temperature sensing** read nothing: the firmware always used a mock sensor bus, and without
  the README's `#define TEMP_ENABLED` the readings were written past the end of their array.
  `TEMP_SENSOR_PRESENT{ true }` is now all it takes.
- **Dual tariff boost of exactly 255 minutes** never ran (its window was 0.25 s); "until the end"
  boosts now print as such in the startup summary.
- **IoT output:**
  - relay states were all sent as `R`, the tag of the relays' average power: now `R1`..`Rn`;
  - a temperature of 100 °C or more, or -10 °C or less, overflowed the frame buffer;
  - the sample count `S` wrapped to a negative value above 32767 (datalog periods of about 21 s
    or more), overflowing the buffer too.
- **`dualTariffPin`** was not checked against the other pins at compile time.
- **`rf` build** failed on a missing library ([#154]).
- **`RemoteLoadReceiver`:** a hang could leave a load stuck ON (now a 1 s hardware watchdog), the
  status LED kept blinking on a hung board, and malformed frames were not rejected ([#177]).

### Performance

- **ADC ISR:** left-aligned conversions and a circular channel list instead of a `switch`
  (suggested by @florentbr in [#121]) ([#127]).
- **Integer energy bucket:** no float and no division left in the ADC ISR; the contribution at
  each zero crossing drops from ~1000 to ~125 cycles. Hand-written 16x16 and 24x16 multiplies
  ([#163]).
- **No float code left:** the datalog (power and voltage per phase) and the load ON times of the
  IoT output use integer arithmetic too, with an assembly integer square root; the float library
  is no longer linked: about 365 bytes of flash and 24 bytes of RAM less. The voltage takes ~570
  cycles per phase instead of ~830, the power ~155 instead of ~290 ([#120]).
- **Flash:**
  - the RFM69 driver is only linked when RF is used (+2.9 kB avoided without RF) ([#155]);
  - the relay code is 396 bytes smaller ([#175]);
  - the text output no longer uses float printing: 758 bytes less ([#180]);
  - in IoT mode, the firmware is about 2.5 kB smaller than in human-readable mode ([#180]).

### Removed

- The `cal_CTx_v_meter` calibration sketch ([#178]).
- `EMONESP_CONTROL`, the `emonesp` environment and its SoftwareSerial debug port ([#180]).
- The RFM12B / JeeLib RF stack, and the `remote_loads` and `rf_all` environments, which built the
  same firmware as `rf` ([#155]).
- The `rf` environment, which built the same firmware as `basic`: RF telemetry and remote loads
  are chosen in `config.h` ([#226]).
- The ArduinoJson dependency of the firmware ([#172]).

### Development

- **GitHub Pages** ([fredm67.github.io/PVRouter-3-phase](https://fredm67.github.io/PVRouter-3-phase/)):
  a landing page to the code documentation (now in `doc/`) and the **firmware size history**,
  the flash and RAM of every build configuration on two charts, recorded on each push to `dev`.
- **Performance report** on every pull request, as a comment updated at each push: the target
  branch is measured in the same run and compared with the PR, 🚨 beyond 2% worse.
- **Configurator CI:** its default choices must regenerate the shipped config files byte for
  byte; presets are built with PlatformIO, and their YAML checked with `esphome config`.
- **Grid simulator** (`sim/`, simavr): the production firmware runs on a simulated 3-phase grid,
  with scenarios checked in CI. It covers regulation, ISR timing and switching instants ([#169],
  [#170]), the RFM69 radio and the receiver firmware with frame loss ([#171], [#173]), relay
  diversion ([#176]), and runs the scenarios in parallel ([#174]), over several CI jobs.
- **Embedded tests in simavr** instead of Wokwi, with no token needed ([#168]); CI without
  duplicate runs, with automatic test discovery ([#167]).
- **CI builds** every environment ([#154]) and the IoT serial output, and reports flash and RAM
  in the job summary ([#166], [#180]).
- **Tests:** native suites for the energy bucket, the datalog arithmetic, the remote loads, the
  overrides, the RMS pipeline and the DC filter, and simavr tests for the relays, the serial output and the
  multiplies ([#120], [#127], [#160], [#163], [#172], [#180]).
- **Workflow:** `main` (stable) and `dev` (development) branches ([#134], [#135]), pre-commit
  hooks for formatting and file headers ([#153], [#159]).

[Unreleased]: https://github.com/FredM67/PVRouter-3-phase/compare/main...dev
[#120]: https://github.com/FredM67/PVRouter-3-phase/issues/120
[#121]: https://github.com/FredM67/PVRouter-3-phase/issues/121
[#127]: https://github.com/FredM67/PVRouter-3-phase/pull/127
[#134]: https://github.com/FredM67/PVRouter-3-phase/pull/134
[#135]: https://github.com/FredM67/PVRouter-3-phase/pull/135
[#153]: https://github.com/FredM67/PVRouter-3-phase/pull/153
[#154]: https://github.com/FredM67/PVRouter-3-phase/pull/154
[#155]: https://github.com/FredM67/PVRouter-3-phase/pull/155
[#159]: https://github.com/FredM67/PVRouter-3-phase/pull/159
[#160]: https://github.com/FredM67/PVRouter-3-phase/pull/160
[#163]: https://github.com/FredM67/PVRouter-3-phase/pull/163
[#164]: https://github.com/FredM67/PVRouter-3-phase/pull/164
[#166]: https://github.com/FredM67/PVRouter-3-phase/pull/166
[#167]: https://github.com/FredM67/PVRouter-3-phase/pull/167
[#168]: https://github.com/FredM67/PVRouter-3-phase/pull/168
[#169]: https://github.com/FredM67/PVRouter-3-phase/pull/169
[#170]: https://github.com/FredM67/PVRouter-3-phase/pull/170
[#171]: https://github.com/FredM67/PVRouter-3-phase/pull/171
[#172]: https://github.com/FredM67/PVRouter-3-phase/pull/172
[#173]: https://github.com/FredM67/PVRouter-3-phase/pull/173
[#174]: https://github.com/FredM67/PVRouter-3-phase/pull/174
[#175]: https://github.com/FredM67/PVRouter-3-phase/pull/175
[#176]: https://github.com/FredM67/PVRouter-3-phase/pull/176
[#177]: https://github.com/FredM67/PVRouter-3-phase/pull/177
[#178]: https://github.com/FredM67/PVRouter-3-phase/pull/178
[#179]: https://github.com/FredM67/PVRouter-3-phase/pull/179
[#180]: https://github.com/FredM67/PVRouter-3-phase/pull/180
[#185]: https://github.com/FredM67/PVRouter-3-phase/pull/185
[#226]: https://github.com/FredM67/PVRouter-3-phase/pull/226
[#234]: https://github.com/FredM67/PVRouter-3-phase/pull/234
[#237]: https://github.com/FredM67/PVRouter-3-phase/pull/237
[#238]: https://github.com/FredM67/PVRouter-3-phase/pull/238
