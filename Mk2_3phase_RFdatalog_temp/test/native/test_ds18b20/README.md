# DS18B20 Reading Tests

Tests for the checks of a temperature reading (`ds18b20.h`): what the firmware does with the 9 bytes of a DS18B20 scratchpad once read, and with the reading once converted. The bus I/O stays in `utils_temp.h`; no stub is needed. The CRC is the firmware's own copy of the Dallas/Maxim CRC-8 (the OneWire library's is no longer used), so the CRC tested is the one that runs.

## Tests

| Test | Description |
|------|-------------|
| `test_crc_of_*` | CRC-8 against known values: the ROM code of Maxim application note 27, the power-on scratchpad of the datasheet, two other scratchpads |
| `test_crc_catches_any_single_bit_error` / `test_*_crc_*` | A good CRC gives the temperature; any flipped bit of the 9 bytes gives `DEVICE_DISCONNECTED_RAW` |
| `test_zero` / `test_positive` / `test_negative` | Register (x16, two's complement) to hundredths of °C, with the examples of the datasheet |
| `test_fractions_of_a_degree` | 1/16 °C steps: x6.25, rounded down (+0.0625 °C gives 6, -0.0625 °C gives -7) |
| `test_every_register_value_of_the_range` | Every register value from -54.9375 to +124.9375 °C against the exact value rounded down |
| `test_just_inside_the_range` / `test_the_bounds_*` / `test_beyond_the_range` | Range check: -55.00 and +125.00 °C are valid readings, beyond them `OUTOFRANGE_TEMPERATURE` |
| `test_all_ones_is_a_disconnected_probe` | Nothing on the bus (all 0xFF): CRC error, `DEVICE_DISCONNECTED_RAW` |
| `test_all_zeros_passes_the_crc` | All zeros pass the CRC and read 0.00 °C (a bus held low fails its reset before) |
| `test_power_on_scratchpad_reads_85` / `test_85_*` | The power-on value: 85.00 °C more than 5.00 °C away from the previous reading (or after an invalid one) is skipped; within 5.00 °C, kept |
| `test_a_first_reading_of_85_*` / `test_a_steady_85_*` / `test_85_after_an_invalid_reading_far_*` / `test_a_skipped_or_out_of_range_*` | `PowerOnFilter`, reading after reading: 85.00 °C is compared with the last valid reading, so a CRC error does not drop a steady 85.00 °C for good; a first 85.00 °C is skipped |
| `test_other_readings_are_kept_whatever_the_jump` | Only exactly 85.00 °C is concerned |
| `test_*_sent` / `test_the_invalid_values_cannot_be_temperatures` | `isValidTemperature()`: the two markers are not sent, and no reading in the range can be taken for one |

## Running

```bash
pio test -e native -f "*test_ds18b20*"
```
