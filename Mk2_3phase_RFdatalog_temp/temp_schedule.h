/**
 * @file temp_schedule.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Which 1-Wire transaction of the temperature sensors runs on which mains cycle
 * @version 0.1
 * @date 2026-11-23
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details A reset of the 1-Wire bus, and each 0 written, keep interrupts off for 65 to 70 µs
 *          (the OneWire library): the ADC interrupt then starts up to ~1100 cycles late, and must
 *          still write ADMUX within the 1664 cycles of its conversion. The heaviest ADC interrupts
 *          are those of the mains cycle's work, run once per cycle shortly after L1's positive
 *          crossing (processing.cpp, processStartNewCycle()): a slot that delays one of them can
 *          make it miss that deadline. The calls at the other crossings (each phase's half cycles)
 *          are lighter.
 *
 *          The main loop therefore runs the bus I/O as short transactions, one per mains cycle at
 *          most, each started as soon as the loop sees the new cycle (Shared::b_newMainsCycle, set
 *          once processStartNewCycle() is done): its long slots end before the negative crossing,
 *          among the ordinary ADC interrupts. A round:
 *          - the conversion of every sensor (reset, Skip ROM, Convert T);
 *          - once the conversion time has passed, for each sensor:
 *            - ADDRESS: reset, Match ROM and the first 4 bytes of its ROM code,
 *            - READ: the last 4 bytes, Read Scratchpad and the 9 bytes of the scratchpad (a read
 *              slot keeps interrupts off ~13 µs only: these may run past the negative crossing),
 *            - CLOSE: the reset that checks the sensor is still there; the reading is decoded.
 *          The bus allows any pause between two time slots: a transaction split over cycles is
 *          still a valid one.
 *
 *          A transaction runs only on a cycle seen on time, the loop idle when it began: after a
 *          long pass (the per-second work, the display, an RF frame), the flag may be several
 *          milliseconds old, and the transaction waits for the next cycle.
 *
 *          One round per datalog period: started when the loop handles the end of the period
 *          (before the datalog output, whose UART interrupts would add to the delay of the ADC
 *          interrupt), or at the next cycle, it ends long before the period does (fitsInPeriod()),
 *          and the datalog sends its values. With sensors built in, the loop handles the datalog
 *          event, raised at L1's crossing, on the pass that sees the new cycle: 1248 us after the
 *          event, and the datalog output of a temperature build is that much later.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test suite.
 *
 * @ingroup TemperatureSensing
 */

#ifndef TEMP_SCHEDULE_H
#define TEMP_SCHEDULE_H

#include <stdint.h>

namespace TempSchedule
{
/** @brief The 1-Wire transaction of a mains cycle. */
enum class Step : uint8_t
{
  NONE,    /**< no bus I/O on this cycle */
  CONVERT, /**< reset, Skip ROM, Convert T: every sensor converts */
  ADDRESS, /**< reset, Match ROM, the first half of the sensor's ROM code */
  READ,    /**< the second half of its ROM code, Read Scratchpad, the 9 bytes */
  CLOSE    /**< reset (the sensor still there?), the reading decoded */
};

/** @brief The transaction to run, and the sensor it reads (ADDRESS, READ, CLOSE). */
struct Action
{
  Step step;
  uint8_t sensor;
};

inline constexpr uint8_t STEPS_PER_SENSOR{ 3 }; /**< ADDRESS, READ, CLOSE */

inline constexpr uint16_t CONVERSION_MS{ 750 }; /**< the longest conversion of a DS18B20, at 12 bits */

/**
 * @brief The mains cycles from the conversion to the first read.
 *
 * @details 750 ms rounded up to whole cycles, plus one: a transaction may start up to a little
 *          after its cycle began (39 cycles at 50 Hz, 780 ms; 46 at 60 Hz, 767 ms).
 *
 * @param supplyFrequency SUPPLY_FREQUENCY
 */
constexpr uint16_t conversionCycles(const uint8_t supplyFrequency)
{
  return static_cast< uint16_t >((static_cast< uint32_t >(CONVERSION_MS) * supplyFrequency + 999) / 1000 + 1);
}

/**
 * @brief The mains cycles of a round on time: from the conversion to the last CLOSE.
 *
 * @param noOfSensors The number of sensors
 * @param supplyFrequency SUPPLY_FREQUENCY
 */
constexpr uint16_t roundCycles(const uint8_t noOfSensors, const uint8_t supplyFrequency)
{
  return static_cast< uint16_t >(conversionCycles(supplyFrequency) + STEPS_PER_SENSOR * noOfSensors);
}

/**
 * @brief Whether a round ends within its datalog period.
 *
 * @details The round starts at the first cycle of the period, or the next one if the first is
 *          late; one cycle per second of the round may be late; and it must be over before the
 *          last cycle of the period, whose datalog copies run in the ADC interrupt at L1's crossing,
 *          before the new cycle is flagged.
 *
 * @param noOfSensors The number of sensors
 * @param supplyFrequency SUPPLY_FREQUENCY
 * @param periodCycles DATALOG_PERIOD_IN_MAINS_CYCLES
 */
constexpr bool fitsInPeriod(const uint8_t noOfSensors, const uint8_t supplyFrequency, const uint16_t periodCycles)
{
  const uint32_t cycles{ roundCycles(noOfSensors, supplyFrequency) };
  return cycles + cycles / supplyFrequency + 2 <= periodCycles;
}

/**
 * @brief The round of the sensors, one transaction per mains cycle.
 *
 * @details Main-loop code. The state is two small counters, the sensor and its transaction,
 *          advanced by compare and reset: no division at run time (the divisions above are
 *          compile-time only).
 *
 * @tparam N The number of sensors
 * @tparam CONVERSION_CYCLES conversionCycles(SUPPLY_FREQUENCY)
 */
template< uint8_t N, uint16_t CONVERSION_CYCLES >
class Scheduler
{
  static_assert(N > 0, "no sensor");

  bool converting{ false };   /**< the round's conversion has started */
  uint16_t sinceConvert{ 0 }; /**< cycles since it started, up to CONVERSION_CYCLES */
  uint8_t sensor{ 0 };        /**< the sensor being read; N: round over */
  uint8_t sub{ 0 };           /**< its next transaction: 0 ADDRESS, 1 READ, 2 CLOSE */

public:
  /**
   * @brief The transaction of this mains cycle: called once per cycle, when the loop sees it.
   *
   * @param onTime Whether the cycle was seen on time (the loop idle when it began)
   * @return The transaction to run now, Step::NONE for none
   */
  Action next(const bool onTime)
  {
    if (!converting)
    {
      if (!onTime)
      {
        return Action{ Step::NONE, 0 };
      }
      converting = true;
      sinceConvert = 0;
      return Action{ Step::CONVERT, 0 };
    }
    if (N == sensor)
    {
      return Action{ Step::NONE, 0 };  // round over
    }
    if (sinceConvert < CONVERSION_CYCLES)
    {
      ++sinceConvert;  // the conversion runs in the sensors, late cycles or not
    }
    if (sinceConvert < CONVERSION_CYCLES || !onTime)
    {
      return Action{ Step::NONE, 0 };
    }
    Action action{ Step::ADDRESS, sensor };
    if (1 == sub)
    {
      action.step = Step::READ;
    }
    else if (2 == sub)
    {
      action.step = Step::CLOSE;
    }
    if (++sub == STEPS_PER_SENSOR)
    {
      sub = 0;
      ++sensor;
    }
    return action;
  }

  /**
   * @brief The sensor did not answer its ADDRESS: its other transactions are skipped.
   *
   * @param idx The sensor of the last ADDRESS (another one is ignored)
   */
  void skipRestOf(const uint8_t idx)
  {
    if (converting && idx == sensor && N != sensor)
    {
      sub = 0;
      ++sensor;
    }
  }

  /**
   * @brief A datalog period begins: a new round, once this one is over.
   *
   * @details An unfinished round (only after many late cycles) goes on: the sensors it has not
   *          read yet keep the values of the period before.
   *
   * @return true if a new round starts: its conversion can run at once, in the cycle that ends
   *         the period (which had no transaction, the round being over)
   */
  bool newPeriod()
  {
    if (!done())
    {
      return false;
    }
    converting = false;
    sensor = 0;
    sub = 0;
    return true;
  }

  /** @brief Whether the round is over. */
  bool done() const
  {
    return converting && N == sensor;
  }
};
}  // namespace TempSchedule

#endif /* TEMP_SCHEDULE_H */
