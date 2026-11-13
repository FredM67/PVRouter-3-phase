/**
 * @file test_main.cpp
 * @brief The integer datalog arithmetic on the target: exact results, and cycle counts against
 *        the float code it replaces
 * @version 0.1
 * @date 2026-11-13
 *
 * @details datalog_math.h replaced, in the datalog (#120):
 *          - power_L     = (int16_t)(sumP / n * f_powerCal[phase])
 *          - Vrms_L_x100 = (uint32_t)(100 * f_voltageCal[phase] * sqrt(sum_Vsquared / n))
 *          The assembly square root is checked at every rounding boundary of the 32-bit range,
 *          and the integer version must not be slower. Timer1 runs at the CPU clock (prescaler 1)
 *          with interrupts disabled, so the counts are exact CPU cycles, in simavr as on a real
 *          board. The overhead of the measurement itself is subtracted.
 */

#include <Arduino.h>
#include <unity.h>
#include <math.h>

#include "../halt_after_tests.h"

#include "datalog_math.h"

void setUp(void) {}
void tearDown(void) {}

// calibration values of the new board (50 A CTs), as runtime arrays like f_powerCal/f_voltageCal
static volatile float powerCal[]{ 0.1432025F, 0.1432025F, 0.1432025F };
static volatile float voltageCal[]{ 0.888065256F, 0.888065256F, 0.888065256F };

static constexpr float powerCalConst[]{ 0.1432025F, 0.1432025F, 0.1432025F };
static constexpr float voltageCalConst[]{ 0.888065256F, 0.888065256F, 0.888065256F };
static constexpr auto powerFixed{ Datalog::powerCalibration(powerCalConst) };
static constexpr auto voltageFixed{ Datalog::voltageCalibration(100, voltageCalConst) };

// inputs: mean squares from 0 V to ~450 V (ADC steps squared), average powers from -2^18 to +2^18
static const uint32_t meanSquares[]{ 0UL, 1UL, 100UL, 5000UL, 45000UL, 67076UL, 67081UL, 70000UL, 150000UL, 262144UL };
static const int32_t averagePowers[]{ 0L, 1L, -7L, 350L, -2000L, 8380L, -15000L, 60000L, -120000L, 228000L };
static constexpr uint8_t N_INPUTS{ 10 };

static volatile uint32_t inMeanSquare;
static volatile int32_t inAveragePower;
static volatile uint8_t inPhase;
static volatile uint32_t out32;
static volatile int16_t out16;

/** @brief Cycle statistics of one variant over all inputs. */
struct Cycles
{
  uint16_t min{ UINT16_MAX };
  uint16_t max{ 0 };
  uint32_t sum{ 0 };

  void add(uint16_t cycles)
  {
    if (cycles < min)
    {
      min = cycles;
    }
    if (cycles > max)
    {
      max = cycles;
    }
    sum += cycles;
  }
  uint16_t average() const
  {
    return static_cast< uint16_t >(sum / N_INPUTS);
  }
};

// Timer1 at the CPU clock: one count per cycle, overflows after 4 ms
static void startTimer()
{
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B = _BV(CS10);
}

// The code between the two reads of TCNT1, with the overhead of the reads subtracted
#define MEASURE(stats, overhead, code) \
  do \
  { \
    cli(); \
    startTimer(); \
    const uint16_t t0{ TCNT1 }; \
    code; \
    const uint16_t t1{ TCNT1 }; \
    sei(); \
    (stats).add(static_cast< uint16_t >(t1 - t0 - (overhead))); \
  } while (0)

static uint16_t overhead()
{
  Cycles empty;
  for (uint8_t i = 0; i < N_INPUTS; ++i)
  {
    MEASURE(empty, 0, {});
  }
  return empty.min;
}

static void report(const __FlashStringHelper* name, const Cycles& stats)
{
  Serial.print(name);
  Serial.print(F(": min "));
  Serial.print(stats.min);
  Serial.print(F(", avg "));
  Serial.print(stats.average());
  Serial.print(F(", max "));
  Serial.print(stats.max);
  Serial.println(F(" cycles"));
}

// ============================================================================
// Correctness on the target: the assembly square root, sqrtScaled() and vrms()
// ============================================================================

/**
 * @brief Every rounding boundary of every root, on the target: the assembly loop is exact.
 *
 * @details round(sqrt(x)) changes from r to r + 1 between r^2 + r and r^2 + r + 1, since
 *          (r + 0.5)^2 = r^2 + r + 0.25. Checking r^2 - 1 (from r = 2), r^2, r^2 + r and r^2 + r + 1 for all
 *          65536 roots covers every boundary of the 32-bit range (up to 65535^2 + 65535 =
 *          UINT32_MAX), and with them the largest
 *          remainders (worst cases of the 3-byte remainder) and the carries of each byte.
 */
void test_isqrt_on_avr_every_boundary(void)
{
  uint32_t errors{ 0 };
  uint32_t firstError{ 0 };
  for (uint32_t root = 0; root <= UINT16_MAX; ++root)
  {
    const uint32_t square{ root * root };
    const auto r{ static_cast< uint16_t >(root) };
    const uint16_t expected[]{ r, r, r, static_cast< uint16_t >(root < UINT16_MAX ? root + 1 : UINT16_MAX) };
    const uint32_t values[]{ root > 1 ? square - 1 : square, square, square + root, square + root + 1 };  // sqrt(1 - 1) = 0
    const uint8_t count{ static_cast< uint8_t >(root < UINT16_MAX ? 4 : 3) };                             // 65535^2 + 65536 = 2^32 wraps
    for (uint8_t i = 0; i < count; ++i)
    {
      if ((Datalog::isqrt(values[i]) != expected[i]) && !errors++)
      {
        firstError = values[i];
      }
    }
  }
  if (errors)
  {
    Serial.print(F("first wrong value: "));
    Serial.println(firstError);
  }
  TEST_ASSERT_EQUAL_UINT32(0, errors);
}

void test_isqrt_on_avr_edge_cases(void)
{
  TEST_ASSERT_EQUAL_UINT16(0, Datalog::isqrt(0));
  TEST_ASSERT_EQUAL_UINT16(1, Datalog::isqrt(1));
  TEST_ASSERT_EQUAL_UINT16(1, Datalog::isqrt(2));  // 1.414
  TEST_ASSERT_EQUAL_UINT16(2, Datalog::isqrt(3));  // 1.732
  TEST_ASSERT_EQUAL_UINT16(2, Datalog::isqrt(4));
  TEST_ASSERT_EQUAL_UINT16(46341, Datalog::isqrt(1UL << 31));     // 46340.95
  TEST_ASSERT_EQUAL_UINT16(32768, Datalog::isqrt(1UL << 30));     // smallest normalised value
  TEST_ASSERT_EQUAL_UINT16(32768, Datalog::isqrt(0x40008000UL));  // 32768.4999962: rounds down
  TEST_ASSERT_EQUAL_UINT16(32769, Datalog::isqrt(0x40008001UL));  // 32768.5000114: rounds up
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, Datalog::isqrt(65535UL * 65535UL));
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, Datalog::isqrt(UINT32_MAX));  // 65535.99999: saturated
}

/** @brief sqrtScaled() and vrms() on the target, against the float code, over the whole voltage range. */
void test_vrms_on_avr_matches_the_float_code(void)
{
  uint8_t bits;
  for (uint32_t meanSquare = 1; meanSquare <= 512UL * 512UL; meanSquare = meanSquare * 9 / 8 + 1)
  {
    // sqrtScaled: sqrt(x) x 2^bits within 0.5 of the exact value (+ the float's own rounding)
    const uint16_t root{ Datalog::sqrtScaled(meanSquare, bits) };
    const float exact{ sqrtf(static_cast< float >(meanSquare)) * static_cast< float >(1UL << bits) };
    TEST_ASSERT_FLOAT_WITHIN(0.5F + exact * 1.2e-7F, exact, root);

    // vrms: within 1 hundredth of the float code (which truncated)
    inMeanSquare = meanSquare;
    const auto reference{ static_cast< uint32_t >(100U * voltageCal[0] * sqrt(inMeanSquare)) };
    TEST_ASSERT_UINT32_WITHIN(1, reference, Datalog::vrms(inMeanSquare, voltageFixed.value[0], voltageFixed.shift));
  }
}

// ============================================================================
// Square root alone
// ============================================================================

void test_isqrt_is_faster_than_float_sqrt(void)
{
  const uint16_t ovh{ overhead() };
  Cycles floatSqrt, intSqrt, scaledSqrt;

  for (uint8_t i = 0; i < N_INPUTS; ++i)
  {
    inMeanSquare = meanSquares[i];
    MEASURE(floatSqrt, ovh, out32 = static_cast< uint32_t >(sqrt(inMeanSquare)));
    MEASURE(intSqrt, ovh, out32 = Datalog::isqrt(inMeanSquare));
    MEASURE(scaledSqrt, ovh, {
      uint8_t bits;
      out32 = Datalog::sqrtScaled(inMeanSquare, bits);
      out16 = bits;
    });
  }

  Serial.println(F("--- square root ---"));
  report(F("float sqrt()        "), floatSqrt);
  report(F("Datalog::isqrt()    "), intSqrt);
  report(F("Datalog::sqrtScaled "), scaledSqrt);

  TEST_ASSERT_LESS_OR_EQUAL_UINT16(floatSqrt.max, scaledSqrt.max);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(floatSqrt.sum, scaledSqrt.sum);
}

// ============================================================================
// The datalog expressions, as they were and as they are
// ============================================================================

void test_vrms_is_faster_than_the_float_code(void)
{
  const uint16_t ovh{ overhead() };
  Cycles floatVrms, intVrms;

  for (uint8_t i = 0; i < N_INPUTS; ++i)
  {
    inMeanSquare = meanSquares[i];
    inPhase = i % 3;
    MEASURE(floatVrms, ovh, out32 = static_cast< uint32_t >(100U * voltageCal[inPhase] * sqrt(inMeanSquare)));
    const uint32_t reference{ out32 };
    MEASURE(intVrms, ovh, out32 = Datalog::vrms(inMeanSquare, voltageFixed.value[inPhase], voltageFixed.shift));

    TEST_ASSERT_UINT32_WITHIN(1, reference, out32);  // the float code truncates, vrms() rounds
  }

  Serial.println(F("--- Vrms_L_x100 ---"));
  report(F("float (as before)   "), floatVrms);
  report(F("Datalog::vrms()     "), intVrms);

  TEST_ASSERT_LESS_OR_EQUAL_UINT16(floatVrms.max, intVrms.max);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(floatVrms.sum, intVrms.sum);
}

void test_watts_is_faster_than_the_float_code(void)
{
  const uint16_t ovh{ overhead() };
  Cycles floatWatts, intWatts;

  for (uint8_t i = 0; i < N_INPUTS; ++i)
  {
    inAveragePower = averagePowers[i];
    inPhase = i % 3;
    MEASURE(floatWatts, ovh, {
      int16_t power = inAveragePower * powerCal[inPhase];
      power *= -1;
      out16 = power;
    });
    MEASURE(intWatts, ovh, out16 = Datalog::watts(-inAveragePower, powerFixed.value[inPhase], powerFixed.shift));
  }

  Serial.println(F("--- power_L ---"));
  report(F("float (as before)   "), floatWatts);
  report(F("Datalog::watts()    "), intWatts);

  TEST_ASSERT_LESS_OR_EQUAL_UINT16(floatWatts.max, intWatts.max);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(floatWatts.sum, intWatts.sum);
}

void setup()
{
  delay(1000);

  UNITY_BEGIN();

  RUN_TEST(test_isqrt_on_avr_every_boundary);
  RUN_TEST(test_isqrt_on_avr_edge_cases);
  RUN_TEST(test_vrms_on_avr_matches_the_float_code);
  RUN_TEST(test_isqrt_is_faster_than_float_sqrt);
  RUN_TEST(test_vrms_is_faster_than_the_float_code);
  RUN_TEST(test_watts_is_faster_than_the_float_code);

  UNITY_END();
  haltAfterTests();
}

void loop() {}
