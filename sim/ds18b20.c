/*
 * ds18b20 - DS18B20 temperature sensors on a 1-Wire bus of the simulated AVR (ds18b20.h)
 *
 * Copyright (c) 2026 Frédéric Metrich
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ds18b20.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "avr_ioport.h"
#include "sim_cycle_timers.h"

#define US(x) ((avr_cycle_count_t)((x) * (bus->avr->frequency / 1000000)))

#define RESET_MIN_US 480  /* a low this long or longer resets the sensors */
#define WRITE_1_MAX_US 15 /* the longest low of a 1 written */
#define WRITE_0_MIN_US 60 /* a 0 written: 60 to 120 us */
#define WRITE_0_MAX_US 120
#define PRESENCE_WAIT_US 30 /* from the master's release to the presence pulse */
#define PRESENCE_US 120     /* the presence pulse */
#define READ_0_US 30        /* a 0 sent: the bus held low from the master's falling edge */
#define RESET_SAMPLE_US 70  /* the OneWire library samples the presence 70 us after its release */
#define CONVERSION_MS 750   /* the longest conversion, at 12 bits */
#define BURST_GAP_MS 2      /* bus activity less than this apart: the same burst */
#define POWER_ON_REG 0x0550 /* 85.00 °C */

enum
{
  IDLE,     /* until the next reset */
  ROM,      /* the ROM command */
  MATCH,    /* the 64 bits of Match ROM */
  FUNCTION, /* the function command */
  READ,     /* Read Scratchpad: the sensor sends */
};

static uint8_t crc8(const uint8_t *data, int len)
{
  uint8_t crc = 0;
  while (len--)
  {
    uint8_t in = *data++;
    for (int i = 0; i < 8; ++i)
    {
      const int mix = (crc ^ in) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      in >>= 1;
    }
  }
  return crc;
}

static void set_pulling(ds18b20_bus_t *bus, int low)
{
  if (low == bus->pulling)
    return;
  bus->pulling = low;
  bus->set_level(low, bus->param);
}

static avr_cycle_count_t release(avr_t *avr, avr_cycle_count_t when, void *param)
{
  (void)avr;
  (void)when;
  set_pulling((ds18b20_bus_t *)param, 0);
  return 0;
}

static avr_cycle_count_t presence(avr_t *avr, avr_cycle_count_t when, void *param)
{
  ds18b20_bus_t *bus = (ds18b20_bus_t *)param;
  (void)when;
  set_pulling(bus, 1);
  ++bus->presences;
  avr_cycle_timer_register(avr, US(PRESENCE_US), release, bus);
  return 0;
}

static double angle_of(const ds18b20_bus_t *bus, avr_cycle_count_t cycle)
{
  const double t = (double)cycle / bus->avr->frequency;
  return 360.0 * (bus->frequency * t - floor(bus->frequency * t));
}

/* interrupts kept off from <from> to <to>: the angles of the mains cycle it covers */
static void long_slot(ds18b20_bus_t *bus, avr_cycle_count_t from, avr_cycle_count_t to)
{
  ++bus->long_slots;
  const double a0 = angle_of(bus, from), a1 = angle_of(bus, to);
  int b0 = (int)(a0 * DS18B20_ANGLE_BINS / 360.0), b1 = (int)(a1 * DS18B20_ANGLE_BINS / 360.0);
  for (int b = b0;; b = (b + 1) % DS18B20_ANGLE_BINS)
  {
    ++bus->angle_bins[b % DS18B20_ANGLE_BINS];
    if (b % DS18B20_ANGLE_BINS == b1 % DS18B20_ANGLE_BINS)
      break;
  }
}

/* the temperature register, once the conversion is over */
static void update(ds18b20_bus_t *bus, ds18b20_t *d)
{
  if (d->converting && bus->avr->cycle >= d->conv_end)
  {
    d->converting = 0;
    d->reg = (int16_t)lround(d->celsius * 16.0);
  }
}

static void start_read(ds18b20_bus_t *bus)
{
  /* the wired-AND of the addressed sensors: one, as the firmware addresses them */
  memset(bus->tx, 0xff, sizeof bus->tx);
  int any = 0;
  for (int i = 0; i < bus->no_of_devs; ++i)
  {
    ds18b20_t *d = &bus->dev[i];
    if (!d->selected)
      continue;
    any = 1;
    update(bus, d);
    ++d->reads;
    if (d->converting)
      ++d->early_reads;
    uint8_t pad[9] = { (uint8_t)(d->reg & 0xff), (uint8_t)((uint16_t)d->reg >> 8), 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0 };
    pad[8] = crc8(pad, 8);
    for (int k = 0; k < 9; ++k)
      bus->tx[k] &= pad[k];
  }
  bus->tx_bit = 0;
  bus->state = any ? READ : IDLE;
}

/* a bit written by the master */
static void bit_written(ds18b20_bus_t *bus, int bit)
{
  switch (bus->state)
  {
    case ROM:
    case FUNCTION:
      bus->byte = (uint8_t)((bus->byte >> 1) | (bit ? 0x80 : 0));
      if (++bus->bits < 8)
        return;
      bus->bits = 0;
      if (bus->state == ROM)
      {
        if (bus->byte == 0xCC) /* Skip ROM */
        {
          for (int i = 0; i < bus->no_of_devs; ++i)
            bus->dev[i].selected = 1;
          bus->state = FUNCTION;
        }
        else if (bus->byte == 0x55) /* Match ROM */
        {
          for (int i = 0; i < bus->no_of_devs; ++i)
            bus->dev[i].selected = 1;
          bus->state = MATCH;
        }
        else
          bus->state = IDLE;
      }
      else if (bus->byte == 0x44) /* Convert T */
      {
        for (int i = 0; i < bus->no_of_devs; ++i)
        {
          ds18b20_t *d = &bus->dev[i];
          if (!d->selected)
            continue;
          update(bus, d);
          ++d->conversions;
          if (!d->converting)
          {
            d->converting = 1;
            d->conv_end = bus->avr->cycle + US(CONVERSION_MS * 1000.0);
          }
        }
        bus->state = IDLE;
      }
      else if (bus->byte == 0xBE) /* Read Scratchpad */
        start_read(bus);
      else
        bus->state = IDLE;
      break;
    case MATCH:
      for (int i = 0; i < bus->no_of_devs; ++i)
      {
        ds18b20_t *d = &bus->dev[i];
        if (((d->rom[bus->bits / 8] >> (bus->bits % 8)) & 1) != bit)
          d->selected = 0;
      }
      if (++bus->bits == 64)
      {
        bus->bits = 0;
        bus->byte = 0;
        bus->state = FUNCTION;
      }
      break;
    default:
      break;
  }
}

static void master_edge(ds18b20_bus_t *bus, int low)
{
  if (low == bus->master_low)
    return;
  bus->master_low = low;
  const avr_cycle_count_t now = bus->avr->cycle;

  if (low)
  {
    if (!bus->bursts || now - bus->last_activity > US(BURST_GAP_MS * 1000.0))
    {
      ++bus->bursts;
      bus->burst_start = now;
    }
    bus->fell_at = now;
    /* a read slot: the sensor sends its bit from the falling edge */
    if (bus->state == READ)
    {
      const int bit = (bus->tx_bit < 72) ? (bus->tx[bus->tx_bit / 8] >> (bus->tx_bit % 8)) & 1 : 1;
      ++bus->tx_bit;
      if (!bit)
      {
        set_pulling(bus, 1);
        avr_cycle_timer_register(bus->avr, US(READ_0_US), release, bus);
      }
    }
    return;
  }

  /* the master releases the bus: how long it held it low */
  const avr_cycle_count_t held = now - bus->fell_at;
  bus->last_activity = now;
  if (now - bus->burst_start > bus->longest_burst)
  {
    bus->longest_burst = now - bus->burst_start;
    bus->longest_burst_at = (double)bus->burst_start / bus->avr->frequency;
  }
  if (held >= US(RESET_MIN_US))
  {
    ++bus->resets;
    long_slot(bus, now, now + US(RESET_SAMPLE_US));
    bus->state = ROM;
    bus->bits = 0;
    bus->byte = 0;
    if (bus->no_of_devs)
      avr_cycle_timer_register(bus->avr, US(PRESENCE_WAIT_US), presence, bus);
    return;
  }
  if (bus->state == READ)
    return;
  if (held <= US(WRITE_1_MAX_US))
    bit_written(bus, 1);
  else if (held >= US(WRITE_0_MIN_US) && held <= US(WRITE_0_MAX_US))
  {
    long_slot(bus, bus->fell_at, now);
    bit_written(bus, 0);
  }
  else
  {
    ++bus->slot_errors;
    bus->state = IDLE;
  }
}

/* DDR written (raised before the register takes its value) */
static void ddr_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  ds18b20_bus_t *bus = (ds18b20_bus_t *)param;
  (void)irq;
  avr_ioport_state_t st;
  avr_ioctl(bus->avr, AVR_IOCTL_IOPORT_GETSTATE(bus->port), &st);
  master_edge(bus, ((value >> bus->bit) & 1) && !((st.port >> bus->bit) & 1));
}

/* PORT written */
static void port_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  ds18b20_bus_t *bus = (ds18b20_bus_t *)param;
  (void)irq;
  avr_ioport_state_t st;
  avr_ioctl(bus->avr, AVR_IOCTL_IOPORT_GETSTATE(bus->port), &st);
  master_edge(bus, ((st.ddr >> bus->bit) & 1) && !((value >> bus->bit) & 1));
}

void ds18b20_init(ds18b20_bus_t *bus, avr_t *avr, char port, int bit, double frequency, ds18b20_level_cb_t set_level,
                  void *param)
{
  memset(bus, 0, sizeof *bus);
  bus->avr = avr;
  bus->port = port;
  bus->bit = bit;
  bus->frequency = frequency;
  bus->set_level = set_level;
  bus->param = param;
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port), IOPORT_IRQ_DIRECTION_ALL), ddr_hook, bus);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port), IOPORT_IRQ_REG_PORT), port_hook, bus);
}

void ds18b20_add(ds18b20_bus_t *bus, double celsius, const uint8_t rom[8])
{
  ds18b20_t *d = &bus->dev[bus->no_of_devs++];
  memset(d, 0, sizeof *d);
  d->celsius = celsius;
  memcpy(d->rom, rom, 8);
  d->reg = POWER_ON_REG;
}

void ds18b20_report(ds18b20_bus_t *bus)
{
  if (!bus->avr || !bus->no_of_devs)
    return;
  printf("1-Wire    D%d: %u resets, %u presence pulses, %u slot errors; %u bursts, longest %.2f ms at %.3f s\n",
         bus->port == 'B' ? bus->bit + 8 : bus->bit, bus->resets, bus->presences, bus->slot_errors, bus->bursts,
         1e3 * (double)bus->longest_burst / bus->avr->frequency, bus->longest_burst_at);
  /* the angles where interrupts were kept off for long, as ranges of 5.625 deg bins */
  printf("1-Wire    %u slots with interrupts off for long (resets, 0s written), in the mains cycle at", bus->long_slots);
  int first = -1, printed = 0;
  /* start the ranges at an empty bin, so that none is split by the wrap */
  for (first = 0; first < DS18B20_ANGLE_BINS && bus->angle_bins[first]; ++first);
  if (first == DS18B20_ANGLE_BINS)
    printf(" every angle");
  else
    for (int k = 1, start = -1; k <= DS18B20_ANGLE_BINS; ++k)
    {
      const int b = (first + k) % DS18B20_ANGLE_BINS;
      if (bus->angle_bins[b] && start < 0)
        start = b;
      if (!bus->angle_bins[b] && start >= 0)
      {
        printf("%s %.0f-%.0f", printed++ ? "," : "", start * 360.0 / DS18B20_ANGLE_BINS, b * 360.0 / DS18B20_ANGLE_BINS);
        start = -1;
      }
    }
  printf(" deg\n");
  for (int i = 0; i < bus->no_of_devs; ++i)
  {
    const ds18b20_t *d = &bus->dev[i];
    printf("DS18B20   %d (%02X %02X %02X %02X %02X %02X %02X %02X) at %.2f C: %u conversions, %u reads, %u before the end of "
           "a conversion\n",
           i + 1, d->rom[0], d->rom[1], d->rom[2], d->rom[3], d->rom[4], d->rom[5], d->rom[6], d->rom[7], d->celsius,
           d->conversions, d->reads, d->early_reads);
  }
}
