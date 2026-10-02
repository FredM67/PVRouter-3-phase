# Changelog

All notable changes to the router firmware (`Mk2_3phase_RFdatalog_temp`), the remote load
firmware (`RemoteLoadReceiver`) and their tooling. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

Changes on `dev`, not yet on `main`.

### Upgrading from `main`

If you keep your own `config.h`, check these points:

- **`ENABLE_DEBUG`** is now a setting like the others: replace `#define ENABLE_DEBUG` with
  `inline constexpr bool ENABLE_DEBUG{ true };` (or `false`). It only controls the debug
  messages now; the startup configuration is always printed in human-readable mode ([#180]).
- **mk2Wifi / ESPHome:** `EMONESP_CONTROL` and the `emonesp` PlatformIO environment are gone.
  For the mk2Wifi module, set `SERIAL_OUTPUT_TYPE = SerialOutputType::IoT` and choose its pins
  freely in `config.h` ([#180]).
- **Diversion pin:** with the pin LOW, forced loads *and* forced relays and remote loads now stay
  ON (before, only forced local TRIACs did). For an absence where nothing must heat, not even with
  off-peak forcing, use the new router OFF pin ([#179]).
- **RF:** RF datalogging and remote loads use an RFM69 module with the LowPowerLab RFM69 library.
  The RFM12B / JeeLib stack is no longer supported. The RF settings are in `config_rf.h` ([#155]).
- **Calibration:** the `cal_CTx_v_meter` sketch is gone. Calibrate with the router firmware
  itself, with `CALIBRATION_MODE` set (see the README) ([#178]).
- **Arduino IDE:** the ArduinoJson library is no longer needed ([#172]).

No change needed for: the calibration values (`f_powerCal`, `f_voltageCal`, `f_phaseCal`), which
keep their meaning ([#127], [#163]), and load pin lists such as `{ 5, 6, 7 }`, which are still
valid load maps ([#160]).

### Added

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
- The ArduinoJson dependency of the firmware ([#172]).

### Development

- **Grid simulator** (`sim/`, simavr): the production firmware runs on a simulated 3-phase grid,
  with scenarios checked in CI. It covers regulation, ISR timing and switching instants ([#169],
  [#170]), the RFM69 radio and the receiver firmware with frame loss ([#171], [#173]), relay
  diversion ([#176]), and runs the scenarios in parallel ([#174]).
- **Embedded tests in simavr** instead of Wokwi, with no token needed ([#168]); CI without
  duplicate runs, with automatic test discovery ([#167]).
- **CI builds** every environment ([#154]) and the IoT serial output, and reports flash and RAM
  in the job summary ([#166], [#180]).
- **Tests:** native suites for the energy bucket, the remote loads, the overrides, the RMS
  pipeline and the DC filter, and simavr tests for the relays, the serial output and the
  multiplies ([#127], [#160], [#163], [#172], [#180]).
- **Workflow:** `main` (stable) and `dev` (development) branches ([#134], [#135]), pre-commit
  hooks for formatting and file headers ([#153], [#159]).

[Unreleased]: https://github.com/FredM67/PVRouter-3-phase/compare/main...dev
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
