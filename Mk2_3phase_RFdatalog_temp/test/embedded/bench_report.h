/**
 * @file bench_report.h
 * @brief Machine-readable lines per benchmark result, for the CI performance report
 *
 * @details Prints `BENCH|<name>|<value>|<unit>` on its own line. CI collects these lines from the
 *          verbose output of the embedded tests (scripts/ci/bench_to_json.py), measures the
 *          target branch of a pull request in the same run, and posts the comparison as a
 *          comment (scripts/ci/pr_report.py). Smaller is better. simavr is cycle-accurate, so
 *          the values are exact and the same from run to run.
 *
 *          The name must stay the same over time: it is the key of the comparison.
 */

#ifndef BENCH_REPORT_H
#define BENCH_REPORT_H

#include <Arduino.h>

/** @brief The start of a BENCH line, up to the separator before the value. */
inline void benchPrefix(const __FlashStringHelper* name, const __FlashStringHelper* suffix)
{
  Serial.println();  // the line may follow output without a newline
  Serial.print(F("BENCH|"));
  Serial.print(name);
  Serial.print(suffix);
  Serial.print('|');
}

/**
 * @brief A result in CPU cycles, reported twice: in microseconds and in cycles.
 *
 * @details `<name> [us]` is cycles / 16 at 16 MHz, exact: the 16ths of a microsecond are
 *          printed as four decimals (1/16 = 0.0625). `<name> [cycles]` is the count itself.
 *          Measured with BenchTimer (bench_timer.h), or with Timer1 directly.
 */
inline void benchCycles(const __FlashStringHelper* name, const uint32_t cycles)
{
  benchPrefix(name, F(" [us]"));
  Serial.print(cycles / 16UL);
  Serial.print('.');
  const uint16_t decimals{ static_cast< uint16_t >((cycles % 16UL) * 625U) };  // 0..9375
  if (decimals < 1000)
  {
    Serial.print('0');
  }
  if (decimals < 100)
  {
    Serial.print('0');
  }
  if (decimals < 10)
  {
    Serial.print('0');
  }
  Serial.print(decimals);
  Serial.println(F("|us"));

  benchPrefix(name, F(" [cycles]"));
  Serial.print(cycles);
  Serial.println(F("|cycles"));
}

#endif  // BENCH_REPORT_H
