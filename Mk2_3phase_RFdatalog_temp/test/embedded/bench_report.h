/**
 * @file bench_report.h
 * @brief One machine-readable line per benchmark result, for the CI history
 *
 * @details Prints `BENCH|<name>|<value>|<unit>` on its own line. CI collects these lines from the
 *          verbose output of the embedded tests (scripts/ci/bench_to_json.py), and tracks every
 *          value on GitHub Pages: a pull request that makes one worse by more than 2% gets a
 *          comment. Smaller is better. simavr is cycle-accurate, so the values are exact and
 *          the same from run to run.
 *
 *          The name must stay the same over time: it is the key of the history.
 */

#ifndef BENCH_REPORT_H
#define BENCH_REPORT_H

#include <Arduino.h>

inline void benchReport(const __FlashStringHelper* name, const uint32_t value, const __FlashStringHelper* unit)
{
  Serial.println();  // the line may follow output without a newline
  Serial.print(F("BENCH|"));
  Serial.print(name);
  Serial.print('|');
  Serial.print(value);
  Serial.print('|');
  Serial.println(unit);
}

#endif  // BENCH_REPORT_H
