# Memory Layout

The router runs on an ATmega328P (`board = uno` in every AVR environment of `platformio.ini`):
32 KB of flash, of which 32,256 bytes are left by the bootloader, 2 KB of SRAM, 1 KB of EEPROM. The
firmware does not use the EEPROM.

## Current size

`pio run -e basic` with the shipped `config.h` (three local loads, the override pin, IoT output;
no temperature sensing, relays or RF), on `dev` d63dc5ca, sizes from `avr-size`:

| | Bytes | Share |
|---|---|---|
| Flash (`.text` + `.data`) | 7,946 | 24.6 % of 32,256 |
| RAM, static (`.data` + `.bss`) | 414 | 20.2 % of 2,048 |

The rest of the RAM is the stack. The figures depend on the configuration: each feature that is
off is not built at all (`if constexpr`). CI builds several configurations and reports their flash
and RAM on every pull request, against the target branch; the history is on
[GitHub Pages](https://fredm67.github.io/PVRouter-3-phase/bench/) (see
[Benchmark history](../test/README.md#benchmark-history)).

To see the sizes of your own configuration:

```bash
pio run -e basic                              # prints RAM and Flash use
~/.platformio/packages/toolchain-atmelavr/bin/avr-size -C --mcu=atmega328p .pio/build/basic/firmware.elf
```

## Where the RAM goes

The largest static object in this build is the Arduino `Serial` object (157 bytes, with its
transmit and receive buffers). The sums of the ADC interrupt are a few bytes per phase:
`l_sumP`, `l_sumP_atSupplyPoint`, `l_sum_Vsquared` and `l_filterDC_V` (`int32_t` or `uint32_t`
per phase), `i_sampleVminusDC` and `i_DCoffset_V` (16 bits per phase), in `processing.cpp`.
The ISR hands its results to the main loop through the `copyOf_*` variables of `shared_var.h`.
List them for your build with:

```bash
~/.platformio/packages/toolchain-atmelavr/bin/avr-nm -C --size-sort -S -t d .pio/build/basic/firmware.elf
```

With `ENABLE_DEBUG`, the router prints its free RAM at startup (`freeRam()`, `utils.h`).

## Rules

- No dynamic allocation.
- Constant data in flash: texts with `F("...")`, tables with `PROGMEM`.
- Narrow types (`uint8_t`, `int16_t`) wherever the range allows.
