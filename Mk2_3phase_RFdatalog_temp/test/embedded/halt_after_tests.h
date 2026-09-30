/**
 * @file halt_after_tests.h
 * @brief Stops the MCU once an embedded test suite has run
 *
 * @details Call it right after UNITY_END(). The suite then runs exactly once instead of
 *          being repeated by loop(), and the simavr simulator exits: it quits as soon as
 *          the CPU sleeps with interrupts disabled.
 */

#ifndef HALT_AFTER_TESTS_H
#define HALT_AFTER_TESTS_H

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

inline void haltAfterTests()
{
  Serial.flush();  // let the last Unity lines out, transmission is interrupt-driven

  cli();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

#endif  // HALT_AFTER_TESTS_H
