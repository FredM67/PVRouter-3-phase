/*
 * rfm69 - model of an RFM69 radio module on the simulated AVR's SPI bus
 *
 * Just enough of the chip for the LowPowerLab RFM69 library in packet mode:
 * register file, operating modes, and a FIFO whose content goes on air when the
 * chip enters TX mode. Each frame is handed to a callback once it has been sent,
 * after the air time given by the programmed bit rate. The channel always looks
 * free (RSSI -114 dBm), so the library's carrier sense never waits.
 *
 * Copyright (c) 2026 Frédéric Metrich
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RFM69_MODEL_H
#define RFM69_MODEL_H

#include <stdint.h>

#include "sim_avr.h"

#define RFM69_FIFO_SIZE 66

typedef struct
{
  avr_cycle_count_t start, end; /* on air, in CPU cycles */
  uint16_t target, sender;      /* 10-bit node IDs, as the library encodes them */
  uint8_t ctl;                  /* control byte: ACK flags and address bits 8-9 */
  uint8_t len;                  /* payload length */
  uint8_t payload[RFM69_FIFO_SIZE];
} rfm69_frame_t;

typedef void (*rfm69_frame_cb_t)(const rfm69_frame_t *frame, void *param);

typedef struct
{
  avr_t *avr;
  uint8_t regs[0x80];
  int mode; /* OPMODE bits 4-2 */
  /* current SPI transaction: CS low, address byte, then data bytes */
  int selected, index;
  uint8_t addr;
  /* FIFO */
  uint8_t fifo[RFM69_FIFO_SIZE];
  int fifo_len, fifo_pos;
  /* frame on air */
  int transmitting;
  rfm69_frame_t frame;
  rfm69_frame_cb_t on_frame;
  void *param;
  /* statistics */
  unsigned long frames, bad_frames;
} rfm69_t;

/* connects the chip to the SPI port (unnamed on the ATmega328P), with its chip select on the given pin of the AVR */
void rfm69_init(rfm69_t *rf, avr_t *avr, char cs_port, int cs_bit, rfm69_frame_cb_t on_frame, void *param);

#endif
