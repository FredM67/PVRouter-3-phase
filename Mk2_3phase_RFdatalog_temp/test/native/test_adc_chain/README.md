# ADC Chain Tests

Tests for the six ADC conversions (`adc_chain.h`): the order V1, I1, V2, I2, V3, I3, the analog input of each (A0 to A5), the ADC reference per board, and the ADMUX pipeline. The ADC runs free: when the ISR of a finished conversion runs, the next conversion has already started, so the ADMUX written now applies to the conversion after next. `processing.cpp` builds its channel table and the start-up ADMUX from this header.

## Tests

| Test | Description |
|------|-------------|
| `test_order_of_the_chain` | The six conversions are numbered 0 to 5 in the order V1, I1, V2, I2, V3, I3 |
| `test_pins` | V1 A0, I1 A1, V2 A2, I2 A3, V3 A4, I3 A5 |
| `test_reference_per_board` | OLD: AVcc (REFS0); NEW: internal 1.1 V (REFS1 and REFS0) |
| `test_admux_of_is_reference_adlar_and_pin` | ADMUX is reference, ADLAR and pin, on both boards |
| `test_after_next_of_every_position` | The channel written after a conversion is two steps ahead, wrap included (V3 gives V1, I3 gives I1) |
| `test_admux_written_after_selects_the_channel_after_next` | `admuxWrittenAfter()` is the ADMUX of the channel after next, on both boards |
| `test_channel_converted_by_round` | Round k converts channel k modulo 6 |
| `test_pipeline_order` | Plays the free-running ADC for 60 rounds: the channel converted is the one the ISR believes it handles |
| `test_channel_table_matches_the_former_literals` | The table of `processing.cpp` has the values of the former hand-written one (V1 to V2, I1 to I2, V2 to V3, I2 to I3, V3 to V1, I3 to I1) and the same start-up ADMUX |
