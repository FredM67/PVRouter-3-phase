/*
 * ds18b20 - DS18B20 temperature sensors on a 1-Wire bus of the simulated AVR
 *
 * Just enough of the sensor for the firmware's bus I/O (OneWire library, utils_temp.h):
 * - reset and presence pulse (the master's low of 480 us or more, then the sensors pull the bus
 *   low 30 us after it is released, for 120 us);
 * - the ROM commands Skip ROM (0xCC, every sensor) and Match ROM (0x55 and the 64-bit code);
 * - the function commands Convert T (0x44, the register takes the temperature 750 ms later, the
 *   longest conversion at 12 bits) and Read Scratchpad (0xBE: the 9 bytes and their CRC, then 1s).
 * A bit written is decoded from how long the master holds the bus low (up to 15 us: 1, 60 to
 * 120 us: 0, anything else is a slot error); a sensor sending a 0 holds the bus low for 30 us from
 * the master's falling edge. Until its first conversion, a sensor holds the power-on value 85.00 °C.
 * Search ROM and the other commands are not modelled: the sensors wait for the next reset.
 *
 * The master drives the bus low when its pin is an output at 0: the model follows DDR and PORT.
 * The sensors pull it low through a callback, which sets the level the pin reads as an input.
 *
 * Timing figures, for the ADC interrupt's deadline: the time slots during which the OneWire library
 * keeps interrupts off for long (a reset, from its release to its presence sample, 70 us; a 0
 * written, its 65 us low), where they fall in the mains cycle, and the longest burst of bus
 * activity (slots less than 2 ms apart).
 *
 * Copyright (c) 2026 Frédéric Metrich
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DS18B20_MODEL_H
#define DS18B20_MODEL_H

#include <stdint.h>

#include "sim_avr.h"

#define DS18B20_MAX 8
#define DS18B20_ANGLE_BINS 64 /* 5.625 deg each */

typedef struct
{
  double celsius; /* its temperature */
  uint8_t rom[8]; /* its ROM code */
  int selected;   /* addressed by the current transaction */
  int16_t reg;    /* the temperature register, x16 */
  int converting; /* a conversion is running, until conv_end */
  avr_cycle_count_t conv_end;
  unsigned conversions, reads, early_reads; /* early: read before the end of a conversion */
} ds18b20_t;

/* the sensors pull the bus low (low != 0) or release it */
typedef void (*ds18b20_level_cb_t)(int low, void *param);

typedef struct
{
  avr_t *avr;
  int port, bit; /* 'D', 3 */
  double frequency;
  ds18b20_t dev[DS18B20_MAX];
  int no_of_devs;
  ds18b20_level_cb_t set_level;
  void *param;
  /* bus */
  int master_low;
  avr_cycle_count_t fell_at;
  int pulling; /* the sensors pull the bus low */
  /* protocol */
  int state, bits;
  uint8_t byte;
  int tx_bit; /* Read Scratchpad: next bit to send */
  uint8_t tx[9];
  /* statistics */
  unsigned resets, presences, slot_errors, long_slots;
  unsigned bursts;
  avr_cycle_count_t burst_start, last_activity, longest_burst;
  double longest_burst_at;
  unsigned long angle_bins[DS18B20_ANGLE_BINS]; /* long slots per angle of the mains cycle */
} ds18b20_bus_t;

/* the bus on pin <port><bit>; the sensors are added with ds18b20_add() before the run */
void ds18b20_init(ds18b20_bus_t *bus, avr_t *avr, char port, int bit, double frequency, ds18b20_level_cb_t set_level,
                  void *param);
void ds18b20_add(ds18b20_bus_t *bus, double celsius, const uint8_t rom[8]);
/* prints the summary lines */
void ds18b20_report(ds18b20_bus_t *bus);

#endif
