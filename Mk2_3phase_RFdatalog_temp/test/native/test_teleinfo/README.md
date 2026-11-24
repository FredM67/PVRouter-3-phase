# Teleinfo Tests

Tests for `teleinfo.h`, the telemetry frame of the IoT output. The frame is streamed: `TeleInfo::writeFrameStep()` writes one line per step of the serial output (STX before the first line, ETX after the last), each line with its own checksum, from a data source given as a template parameter. The test sources cover the feature sets of the firmware: one or three phases, relays, loads, temperature sensors, dual tariff.

The reference is the buffered `TeleInfo` class of the former firmware, its code kept in the test (`utoa()` replaced by `snprintf()`), and its `sendTelemetryData()`, with `N` and `V` sent unsigned: the streamed frame must be the same, byte for byte.

## Tests

| Test | Description |
|------|-------------|
| `test_line` | Single lines: tag and index, sign, 16-bit values, the checksum, the longest line |
| `test_golden_frame` | A frame written by hand, checksums computed by hand |
| `test_streamed_frame_equals_buffered` | The streamed frame equals the buffered one, for every feature set, typical and extreme values, invalid temperatures |
| `test_step_fits_serial_output` | A step never writes more than `MAX_STEP_LENGTH`, at most the serial output's 32 characters |
| `test_one_line_per_step` | One step per line |
| `test_every_tag` | Every tag of a frame with every feature: its place, its value, no line for an invalid temperature |
| `test_optional_features` | Relays, temperatures, dual tariff and the per-phase lines: there with the feature on, absent with it off |
| `test_absence_count_unsigned` | `N` (seconds without diverted energy) is sent unsigned: 40000 and 65535, not negative |
| `test_voltage_unsigned` | `V1` to `V3` are sent unsigned: 46000 (was -19536), 32768 and 65535 |
| `test_sample_count_unsigned` | `S` above 32767 is sent unsigned |

## Running

```bash
pio test -e native -f "native/test_teleinfo"
```
