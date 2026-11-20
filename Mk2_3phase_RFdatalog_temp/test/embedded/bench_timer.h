/**
 * @file bench_timer.h
 * @brief Exact CPU cycle count of a benchmark loop, whatever its length
 *
 * @details Timer1 counts every CPU cycle; its overflow interrupt extends it to 32 bits, so a loop
 *          of up to 268 s can be timed. During the measurement, the millis() interrupt of Timer0
 *          is paused and pending serial output has been sent: the only interrupt left is the
 *          Timer1 overflow, which happens a fixed number of times for a given loop. The count is
 *          then exact and the same from run to run, wherever the loop starts - unlike micros(),
 *          which advances in steps of 4 us and includes every millis() interrupt in between.
 *
 *          The cycles of start() and stop() themselves are measured once and subtracted.
 *
 *          Usage:
 *          @code
 *          BenchTimer::start();
 *          for (...) { ... }
 *          const uint32_t cycles{ BenchTimer::stop() };
 *          @endcode
 *
 * @note Defines the Timer1 overflow interrupt: include it in one source file of a test only.
 */

#ifndef BENCH_TIMER_H
#define BENCH_TIMER_H

#include <Arduino.h>

namespace BenchTimer
{
inline volatile uint16_t overflows{ 0 }; /**< Timer1 overflows since start() */
inline uint8_t savedTimsk0{ 0 };         /**< the Timer0 interrupts to restore in stop() */
inline uint32_t overhead{ 0 };           /**< cycles of an empty start()/stop() */
inline bool calibrated{ false };

/** @brief Starts counting: from the last instruction on, one count per CPU cycle. */
inline void begin()
{
  Serial.flush();  // no transmit interrupt during the measurement
  savedTimsk0 = TIMSK0;
  TIMSK0 = 0;  // millis() paused
  TCCR1B = 0;
  TCCR1A = 0;
  TCNT1 = 0;
  overflows = 0;
  TIFR1 = _BV(TOV1);  // no stale overflow
  TIMSK1 = _BV(TOIE1);
  TCCR1B = _BV(CS10);  // CPU clock, no prescaler
}

/** @brief The cycles since begin(), its own instructions included. */
inline uint32_t end()
{
  const uint8_t sreg{ SREG };
  cli();
  // read while counting: once stopped, simavr reads TCNT1 as 0
  const uint16_t low{ TCNT1 };
  uint16_t high{ overflows };
  if ((TIFR1 & _BV(TOV1)) && low < 0x8000U)
  {
    ++high;  // overflow between the last interrupt and the read, not served yet
  }
  TCCR1B = 0;
  TIMSK1 = 0;
  TIFR1 = _BV(TOV1);
  TIMSK0 = savedTimsk0;
  SREG = sreg;
  return (static_cast< uint32_t >(high) << 16) | low;
}

/** @brief Starts a measurement (the first one also measures the overhead). */
inline void start()
{
  if (!calibrated)
  {
    calibrated = true;
    begin();
    overhead = end();
  }
  begin();
}

/** @brief The CPU cycles since start(), without those of start() and stop(). */
inline uint32_t stop()
{
  return end() - overhead;
}
}  // namespace BenchTimer

ISR(TIMER1_OVF_vect)
{
  ++BenchTimer::overflows;
}

#endif  // BENCH_TIMER_H
