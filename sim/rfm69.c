/*
 * rfm69 - model of an RFM69 radio module on the simulated AVR's SPI bus
 *
 * Copyright (c) 2026 Frédéric Metrich
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rfm69.h"

#include <string.h>

#include "avr_ioport.h"
#include "avr_spi.h"

/* registers and bits used by the LowPowerLab library (RFM69registers.h) */
#define REG_FIFO 0x00
#define REG_OPMODE 0x01
#define REG_BITRATEMSB 0x03
#define REG_BITRATELSB 0x04
#define REG_RSSICONFIG 0x23
#define REG_RSSIVALUE 0x24
#define REG_IRQFLAGS1 0x27
#define REG_IRQFLAGS2 0x28
#define REG_PREAMBLEMSB 0x2C
#define REG_PREAMBLELSB 0x2D
#define REG_SYNCCONFIG 0x2E

#define MODE_TX 3 /* OPMODE bits 4-2 */

#define RSSI_START 0x01
#define RSSI_DONE 0x02
#define IRQFLAGS1_MODEREADY 0x80
#define IRQFLAGS2_FIFONOTEMPTY 0x40
#define IRQFLAGS2_FIFOOVERRUN 0x10
#define IRQFLAGS2_PACKETSENT 0x08

#define RSSI_IDLE_CHANNEL 0xE4 /* -114 dBm: below the library's -90 dBm carrier sense limit */
#define CRC_BYTES 2
#define FXOSC 32000000.0

static void fifo_clear(rfm69_t *rf)
{
  rf->fifo_len = rf->fifo_pos = 0;
}

static avr_cycle_count_t tx_done(avr_t *avr, avr_cycle_count_t when, void *param)
{
  (void)when;
  rfm69_t *rf = param;
  rf->transmitting = 0;
  rf->regs[REG_IRQFLAGS2] |= IRQFLAGS2_PACKETSENT;
  rf->frame.end = avr->cycle;
  ++rf->frames;
  if (rf->on_frame)
    rf->on_frame(&rf->frame, rf->param);
  return 0;
}

/* variable length packet: the FIFO holds [length, target, sender, CTL, payload...] */
static void tx_start(rfm69_t *rf)
{
  const int len = rf->fifo[0];
  if (len < 3 || len + 1 > rf->fifo_len)
  {
    ++rf->bad_frames;
    fifo_clear(rf);
    return;
  }

  rfm69_frame_t *f = &rf->frame;
  f->start = rf->avr->cycle;
  f->ctl = rf->fifo[3];
  f->target = rf->fifo[1] | (uint16_t)((f->ctl & 0x0C) << 6);
  f->sender = rf->fifo[2] | (uint16_t)((f->ctl & 0x03) << 8);
  f->len = (uint8_t)(len - 3);
  memcpy(f->payload, rf->fifo + 4, f->len);
  fifo_clear(rf);

  /* preamble, sync word, length byte, packet and CRC at the programmed bit rate */
  const unsigned preamble = (rf->regs[REG_PREAMBLEMSB] << 8) | rf->regs[REG_PREAMBLELSB];
  const unsigned sync = ((rf->regs[REG_SYNCCONFIG] >> 3) & 7) + 1;
  const unsigned bits = 8 * (preamble + sync + 1 + len + CRC_BYTES);
  const unsigned divider = (rf->regs[REG_BITRATEMSB] << 8) | rf->regs[REG_BITRATELSB];
  const double bitrate = FXOSC / (divider ? divider : 1);

  rf->transmitting = 1;
  avr_cycle_timer_register(rf->avr, (avr_cycle_count_t)(bits * rf->avr->frequency / bitrate) + 1, tx_done, rf);
}

static void set_mode(rfm69_t *rf, int mode)
{
  const int old = rf->mode;
  rf->mode = mode;
  if (old == MODE_TX && mode != MODE_TX)
  {
    /* leaving TX before PacketSent drops the frame */
    if (rf->transmitting)
    {
      avr_cycle_timer_cancel(rf->avr, tx_done, rf);
      rf->transmitting = 0;
      ++rf->bad_frames;
    }
    rf->regs[REG_IRQFLAGS2] &= ~IRQFLAGS2_PACKETSENT;
  }
  else if (mode == MODE_TX && old != MODE_TX && rf->fifo_len)
    tx_start(rf);
}

static uint8_t reg_read(rfm69_t *rf, uint8_t addr)
{
  switch (addr)
  {
    case REG_FIFO:
      return (rf->fifo_pos < rf->fifo_len) ? rf->fifo[rf->fifo_pos++] : 0;
    case REG_RSSICONFIG:
      return rf->regs[addr] | RSSI_DONE;
    case REG_RSSIVALUE:
      return RSSI_IDLE_CHANNEL;
    case REG_IRQFLAGS1:
      return IRQFLAGS1_MODEREADY;
    case REG_IRQFLAGS2:
      return rf->regs[addr] | ((rf->fifo_pos < rf->fifo_len) ? IRQFLAGS2_FIFONOTEMPTY : 0);
    default:
      return rf->regs[addr];
  }
}

static void reg_write(rfm69_t *rf, uint8_t addr, uint8_t value)
{
  switch (addr)
  {
    case REG_FIFO:
      if (rf->fifo_len < RFM69_FIFO_SIZE)
        rf->fifo[rf->fifo_len++] = value;
      break;
    case REG_OPMODE:
      rf->regs[addr] = value;
      set_mode(rf, (value >> 2) & 7);
      break;
    case REG_RSSICONFIG:
      rf->regs[addr] = value & ~RSSI_START;
      break;
    case REG_IRQFLAGS1:
      break;
    case REG_IRQFLAGS2:
      if (value & IRQFLAGS2_FIFOOVERRUN)
        fifo_clear(rf);
      break;
    default:
      rf->regs[addr] = value;
      break;
  }
}

/* a byte clocked out by the AVR (SPI master): answer with the byte clocked in */
static void spi_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  rfm69_t *rf = param;
  if (!rf->selected)
    return;

  uint8_t reply = 0;
  if (rf->index++ == 0)
    rf->addr = (uint8_t)value; /* bit 7: write */
  else
  {
    const uint8_t addr = rf->addr & 0x7F;
    if (rf->addr & 0x80)
      reg_write(rf, addr, (uint8_t)value);
    else
      reply = reg_read(rf, addr);
    /* burst access: the address increments, except on the FIFO */
    if (addr != REG_FIFO)
      rf->addr = (uint8_t)((rf->addr & 0x80) | ((addr + 1) & 0x7F));
  }
  avr_raise_irq(avr_io_getirq(rf->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT), reply);
}

static void cs_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  rfm69_t *rf = param;
  rf->selected = !value;
  rf->index = 0;
}

void rfm69_init(rfm69_t *rf, avr_t *avr, char cs_port, int cs_bit, rfm69_frame_cb_t on_frame, void *param)
{
  memset(rf, 0, sizeof *rf);
  rf->avr = avr;
  rf->on_frame = on_frame;
  rf->param = param;

  /* reset values from the datasheet, for the registers the model relies on */
  rf->regs[REG_OPMODE] = 0x04; /* standby */
  rf->mode = 1;
  rf->regs[REG_BITRATEMSB] = 0x1A; /* 4.8 kb/s */
  rf->regs[REG_BITRATELSB] = 0x0B;
  rf->regs[REG_PREAMBLELSB] = 0x03;
  rf->regs[REG_SYNCCONFIG] = 0x98;

  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), spi_hook, rf);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(cs_port), cs_bit), cs_hook, rf);
}
