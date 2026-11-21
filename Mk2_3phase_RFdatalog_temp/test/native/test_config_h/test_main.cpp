/**
 * @file test_main.cpp
 * @brief Native tests for config.h itself
 *
 * @details config.h only holds settings: it builds on the PC, without Arduino and without the
 *          rest of the firmware, and its targets resolve as config_derived.h resolves them.
 *          Whatever the values in config.h, so that an edited config.h passes too.
 */

#include <unity.h>

#include "config.h"            // the settings, and config_types.h: nothing else
#include "utils_dualtariff.h"  // BoostSchedule
#include "utils_override.h"    // TargetMap, OverrideTable

void setUp(void) {}
void tearDown(void) {}

void test_config_h_reads_on_its_own(void)
{
  TEST_ASSERT_EQUAL(NO_OF_DUMPLOADS, sizeof(physicalLoadPin));
  TEST_ASSERT_EQUAL(NO_OF_DUMPLOADS, sizeof(loadPrioritiesAtStartup));
  TEST_ASSERT_TRUE(relays.size() > 0);
  TEST_ASSERT_TRUE(temperatureSensing.size() > 0);
  TEST_ASSERT_TRUE(rotationGroups.size() <= NO_OF_DUMPLOADS / 2);
}

void test_targets_resolve_as_in_config_derived_h(void)
{
  constexpr TargetMap targets{ physicalLoadPin, relays, RELAY_DIVERSION };
  constexpr OverrideTable table{ overridePins, targets };
  constexpr BoostSchedule boosts{ scheduledBoosts, targets };

  TEST_ASSERT_EQUAL(overridePins.size(), table.size());
  for (uint8_t i = 0; i != overridePins.size(); ++i)
  {
    TEST_ASSERT_EQUAL(overridePins.getPin(i), table.getPin(i));
    // a target that resolves to nothing is reported by validation.h; here, the list just resolves
    TEST_ASSERT_TRUE(overridePins.getTargets(i).count > 0);
  }
  TEST_ASSERT_EQUAL(scheduledBoosts.size(), boosts.size());
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_config_h_reads_on_its_own);
  RUN_TEST(test_targets_resolve_as_in_config_derived_h);

  return UNITY_END();
}
