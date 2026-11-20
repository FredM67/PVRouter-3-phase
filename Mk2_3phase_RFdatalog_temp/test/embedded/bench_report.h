/**
 * @file bench_report.h
 * @brief Machine-readable lines per benchmark result, for the CI performance report
 *
 * @details Prints `BENCH|<name>|<value>|<unit>` on its own line. CI collects these lines from the
 *          verbose output of the embedded tests (scripts/ci/bench_to_json.py), measures the
 *          target branch of a pull request in the same run, and posts the comparison as a
 *          comment (scripts/ci/pr_report.py); each push to dev adds them to the history on
 *          GitHub Pages (scripts/ci/pages_update.py). Smaller is better. simavr is
 *          cycle-accurate, so the values are exact and the same from run to run.
 *
 *          The name must stay the same over time: it is the key of the comparison.
 */

#ifndef BENCH_REPORT_H
#define BENCH_REPORT_H

#include <Arduino.h>

/** @brief Prints value / 10^decimals with that many decimals, zero-padded: 27326, 4 -> 2.7326. */
inline void printDecimal(const uint32_t value, const uint8_t decimals)
{
  uint32_t scale{ 1 };
  for (uint8_t i = 0; i != decimals; ++i)
  {
    scale *= 10;
  }
  Serial.print(value / scale);
  if (decimals == 0)
  {
    return;
  }
  Serial.print('.');
  const uint32_t fraction{ value % scale };
  for (uint32_t digit = scale / 10; digit > 1 && fraction < digit; digit /= 10)
  {
    Serial.print('0');
  }
  Serial.print(fraction);
}

/** @brief cycles / ops in hundredths of a cycle, rounded. */
inline uint32_t centiCycles(const uint32_t cycles, const uint32_t ops)
{
  return static_cast< uint32_t >((static_cast< uint64_t >(cycles) * 100U + ops / 2) / ops);
}

/** @brief cycles / ops in 1/10000 us at 16 MHz, rounded (exact for ops = 1: 1 cycle = 625). */
inline uint32_t deciMicros(const uint32_t cycles, const uint32_t ops)
{
  return static_cast< uint32_t >((static_cast< uint64_t >(cycles) * 625U + ops / 2) / ops);
}

/**
 * @brief Prints "<cycles> cycles = <us> us" for one operation, for the human-readable output.
 *
 * @param cycles the cycles of the whole loop
 * @param ops the operations in the loop: 1 for a single call
 */
inline void printPerOp(const uint32_t cycles, const uint32_t ops = 1)
{
  if (ops == 1)
  {
    Serial.print(cycles);
  }
  else
  {
    printDecimal(centiCycles(cycles, ops), 2);
  }
  Serial.print(F(" cycles = "));
  printDecimal(deciMicros(cycles, ops), 4);
  Serial.print(F(" us"));
}

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
 * @details `<name> [us]` is cycles / 16 at 16 MHz, with four decimals (exact for a single call:
 *          1/16 = 0.0625). `<name> [cycles]` is the count itself, with two decimals for a loop.
 *          Measured with BenchTimer (bench_timer.h), or with Timer1 directly.
 *
 * @param name the name, saying what one operation is for a loop ("... per op")
 * @param cycles the cycles of the whole loop
 * @param ops the operations in the loop, to report one of them: 1 for a single call
 */
inline void benchCycles(const __FlashStringHelper* name, const uint32_t cycles, const uint32_t ops = 1)
{
  benchPrefix(name, F(" [us]"));
  printDecimal(deciMicros(cycles, ops), 4);
  Serial.println(F("|us"));

  benchPrefix(name, F(" [cycles]"));
  if (ops == 1)
  {
    Serial.print(cycles);
  }
  else
  {
    printDecimal(centiCycles(cycles, ops), 2);
  }
  Serial.println(F("|cycles"));
}

#endif  // BENCH_REPORT_H
