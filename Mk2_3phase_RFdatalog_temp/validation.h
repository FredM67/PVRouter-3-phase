/**
 * @file validation.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Compile-time validations
 * @version 0.1
 * @date 2023-02-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _VALIDATION_H
#define _VALIDATION_H

#include "utils.h"

/**
 * @note All these checks are done by the compiler.
 * It does not take ANY space in the Arduino.
 * 
 */

static_assert(DATALOG_PERIOD_IN_SECONDS <= 40, "**** Data log duration is too long and will lead to overflow ! ****");

static_assert(TEMP_SENSOR_PRESENT ^ (tempSensorPin == 0xff), "******** Wrong pin value for temperature sensor(s). Please check your config.h ! ********");
static_assert(DIVERSION_PIN_PRESENT ^ (diversionPin == 0xff), "******** Wrong pin value for diversion command. Please check your config.h ! ********");
static_assert(PRIORITY_ROTATION ^ (rotationPin == 0xff), "******** Wrong pin value for rotation command. Please check your config.h ! ********");
static_assert(OVERRIDE_PIN_PRESENT ^ (forcePin == 0xff), "******** Wrong pin value for override command. Please check your config.h ! ********");
static_assert(WATCHDOG_PIN_PRESENT ^ (watchDogPin == 0xff), "******** Wrong pin value for watchdog. Please check your config.h ! ********");

static_assert(DUAL_TARIFF ^ (dualTariffPin == 0xff), "******** Wrong pin value for dual tariff. Please check your config.h ! ********");
static_assert(!DUAL_TARIFF | (ul_OFF_PEAK_DURATION == 0), "******** Off-peak duration cannot be zero. Please check your config.h ! ********");
static_assert(!(DUAL_TARIFF & (ul_OFF_PEAK_DURATION > 12)), "******** Off-peak duration cannot last more than 12 hours. Please check your config.h ! ********");

static_assert(!EMONESP_CONTROL || (DIVERSION_PIN_PRESENT && DIVERSION_PIN_PRESENT && PRIORITY_ROTATION && OVERRIDE_PIN_PRESENT), "******** Wrong configuration. Please check your config.h ! ********");

constexpr uint16_t check_pins()
{
  uint16_t used_pins{ 0 };

  if (tempSensorPin != 0xff)
    bit_set(used_pins, tempSensorPin);

  if (diversionPin != 0xff)
  {
    if (bit_read(used_pins, diversionPin))
      return 0;

    bit_set(used_pins, diversionPin);
  }

  if (rotationPin != 0xff)
  {
    if (bit_read(used_pins, rotationPin))
      return 0;

    bit_set(used_pins, rotationPin);
  }

  if (forcePin != 0xff)
  {
    if (bit_read(used_pins, forcePin))
      return 0;

    bit_set(used_pins, forcePin);
  }

  if (watchDogPin != 0xff)
  {
    if (bit_read(used_pins, watchDogPin))
      return 0;

    bit_set(used_pins, watchDogPin);
  }

  //physicalLoadPin
  for (const auto &loadPin : physicalLoadPin)
  {
    if (loadPin == 0xff)
      return 0;

    if (bitRead(used_pins, loadPin))
      return 0;

    bit_set(used_pins, loadPin);
  }

  return used_pins;
}

static_assert(check_pins(), "******** Duplicate pin definition ! Please check your config ! ********");
static_assert((check_pins() & B00000011) == 0, "******** Pins 0 & 1 are reserved for RX/TX ! Please check your config ! ********");
static_assert((check_pins() & 0xC000) == 0, "******** Pins 14 and/or 15 do not exist ! Please check your config ! ********");
static_assert(!(RF_CHIP_PRESENT && ((check_pins() & 0x3C04) != 0)), "******** Pins from RF chip are reserved ! Please check your config ! ********");

#endif
