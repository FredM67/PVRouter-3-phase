
#include <unity.h>
#include "ewma_avg.hpp"  // Include the EWMA_average class

// 120 should be for around 10 minutes of data (period in minutes * 60 / datalog period in seconds)
void test_initial_values()
{
  EWMA_average< 120 > avg;                        // Initialize with default smoothing factor
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageS());  // EMA should start at 0
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageD());  // DEMA should start at 0
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageT());  // TEMA should start at 0
}

void test_single_value_update()
{
  EWMA_average< 120 > avg;
  avg.addValue(100);                                // Add a single value
  TEST_ASSERT_EQUAL_INT32(100, avg.getAverageS());  // EMA should match the input
  TEST_ASSERT_EQUAL_INT32(100, avg.getAverageD());  // DEMA should match the input
  TEST_ASSERT_EQUAL_INT32(100, avg.getAverageT());  // TEMA should match the input
}

void test_multiple_value_updates()
{
  EWMA_average< 120 > avg;
  avg.addValue(100);                                 // Add first value
  avg.addValue(200);                                 // Add second value
  TEST_ASSERT_GREATER_THAN(100, avg.getAverageS());  // EMA should be between 100 and 200
  TEST_ASSERT_LESS_THAN(200, avg.getAverageS());     // EMA should be less than 200
  TEST_ASSERT_GREATER_THAN(100, avg.getAverageD());  // DEMA should be between 100 and 200
  TEST_ASSERT_LESS_THAN(200, avg.getAverageD());     // DEMA should be less than 200
  TEST_ASSERT_GREATER_THAN(100, avg.getAverageT());  // TEMA should be between 100 and 200
  TEST_ASSERT_LESS_THAN(200, avg.getAverageT());     // TEMA should be less than 200
}

void test_peak_response()
{
  EWMA_average< 120 > avg;
  avg.addValue(100);
  avg.addValue(1000);                                              // Add a peak value
  TEST_ASSERT_GREATER_THAN(100, avg.getAverageS());                // EMA should increase
  TEST_ASSERT_GREATER_THAN(avg.getAverageS(), avg.getAverageD());  // EMA > DEMA
  TEST_ASSERT_GREATER_THAN(avg.getAverageD(), avg.getAverageT());  // DEMA > TEMA
}

void test_reset_behavior()
{
  EWMA_average< 120 > avg;
  avg.addValue(100);
  avg.addValue(200);
  avg = EWMA_average< 120 >();                    // Reset by reinitializing
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageS());  // EMA should reset to 0
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageD());  // DEMA should reset to 0
  TEST_ASSERT_EQUAL_INT32(0, avg.getAverageT());  // TEMA should reset to 0
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_initial_values);
  RUN_TEST(test_single_value_update);
  RUN_TEST(test_multiple_value_updates);
  RUN_TEST(test_peak_response);
  RUN_TEST(test_reset_behavior);

  return UNITY_END();
}