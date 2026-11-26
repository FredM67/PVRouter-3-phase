# Performance Characteristics

Timing of the firmware on the ATmega328P at 16 MHz, as measured in the grid simulator
([sim/README.md](../../sim/README.md)). For the flash and RAM, see [memory-layout.md](memory-layout.md).

## Timing Analysis

### ADC Interrupt Service Routine (ISR)

The ADC is free-running: one conversion every 104 µs (13 ADC clocks at 125 kHz, 1,664 CPU cycles), and one ISR call per conversion. Each call must end before the next conversion completes.

Measured cycle-exactly in the grid simulator (`make -C sim check`, `basic` firmware, ISR entry to `reti`):

| Measure | Cycles | Time | Share of the 104 µs period | When |
|---------|--------|------|----------------------------|------|
| Average | 318 | 19.9 µs | 19 % | every call |
| Maximum, steady state | ~930 | ~58 µs | 56 % | once per datalog period (the mains cycle that copies the datalog values) |
| Maximum, one-off | ~1,280 | ~80 µs | 77 % | once, at the end of the start-up period |
| Calls longer than one conversion | 0 | | | |

Of the average, about 145 cycles (~9 µs) are the entry and exit of the ISR: it saves and restores 32 registers, with no stack frame. `updatePhysicalLoadStates()`, which runs once per mains cycle, is a real call on purpose: inlined, it needs registers that the rest of the ISR then spills to a stack frame on every call (30 cycles more on average, 65 more for the longest call). So is the switching decision, `LoadPriorities::Switching::onNewCycle()` (`load_priorities.h`): inlined, it put 4 more registers and a stack frame in the ISR; out of line, the average is unchanged, and 32 cycles lower with remote loads. The energy bucket update is an integer multiply by a compile-time `cal / n` table, no division or float (~125 cycles, once per phase and mains cycle).

### The ADMUX deadline

The ISR writes ADMUX last, and the value applies to the conversion after next: when the interrupt
of a finished conversion runs, the next conversion has already started (`adc_chain.h`). The write
must come at least one ADC clock (128 CPU cycles) after the start of the running conversion, as
the datasheet requires (earlier, that conversion may take the new channel), and before that
conversion ends, 1664 cycles after its start (later, the channels land one conversion late). The
simulator measures this as `ADMUX delay`, with `misfiled samples` and `ISR overruns`.

The ISR alone stays well inside this window. What brings the write close to the deadline is an
interrupt that starts late: the OneWire library keeps interrupts off for 65 to 70 µs for a bus
reset and for each 0 written. With temperature sensors, the worst ADMUX write measured is 1505
cycles of the 1664, with no misfiled sample (1431 and 1434 cycles together with remote loads or
relays): see *Temperature sensing* in [sim/README.md](../../sim/README.md#temperature-sensing).
The loop schedules the 1-Wire transactions for that reason (below).

### Main loop

The main loop (`loop()` in `main.cpp`) does what is not time-critical, triggered by flags the ISR
sets (`shared_var.h`):

- once per mains cycle (`b_newMainsCycle`): with temperature sensing, one short 1-Wire
  transaction first (`temp_schedule.h`), so that its slots fall among the light ISR calls; one
  round per datalog period: the conversion of every sensor (750 ms), then the reads; the build
  refuses more sensors than fit in the period (`validation.h`);
- once per second: the dual tariff, the override pins and scheduled boosts, the relays, the load
  priorities and their rotation;
- once per datalog period (`b_datalogEventPending`, every `DATALOG_PERIOD_IN_SECONDS`, 5 s by
  default, 30 s at most): power and voltage per phase, the relays' average, the temperatures, and
  the output;
- on every pass: the serial output in progress, as far as the transmit buffer allows (it never
  waits, `serial_output.h`), and the RF frames to the remote units.

## Benchmarks

The embedded `*_perf` suites and `test_datalog_math_avr` count the CPU cycles of the arithmetic
helpers in simavr; CI compares them, with the flash and RAM, on every pull request. See
[Benchmark history](../test/README.md#benchmark-history).
