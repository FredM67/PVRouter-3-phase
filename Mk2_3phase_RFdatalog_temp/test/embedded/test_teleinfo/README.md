# Teleinfo Tests

Tests for `teleinfo.h`, the telemetry frame of the IoT output, on the ATmega328P (16-bit `int`): a frame is written one line per step of the serial output, STX before the first line, ETX after the last. The expected text is built independently, each checksum summed by the test. The native tests (`test/native/test_teleinfo`) check the frame byte for byte against the buffered frame of the former firmware.

## Tests

| Test | Description |
|------|-------------|
| `test_lineSize_calculation` | The size of a line (5 + tag + value), and a step fits the serial output's 32 characters |
| `test_teleinfo_line_contents` | Single lines: a 5-character temperature, a voltage and a sample count above 32767 (unsigned), an indexed relay state, -32768, 0 |
| `test_frame_three_phases` | A whole frame: `P`, then `Pn` and `Vn` of each phase (last phase first), relays, loads, an invalid temperature skipped, `N`, `TA`, `S`, `S_MC` |
| `test_frame_single_phase` | The same frame with one phase: no per-phase lines |

## Running

```bash
pio test -e uno_sim -f "*test_teleinfo*"
```
