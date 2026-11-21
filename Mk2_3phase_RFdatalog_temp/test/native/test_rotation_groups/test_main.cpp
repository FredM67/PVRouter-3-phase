/**
 * @file test_main.cpp
 * @brief Native tests for the rotation groups: the setting, its resolution and the rotation
 *
 * @details rotationGroups in config.h lists the loads that take turns, as groups written with the
 *          target codes of the override pins. Each group rotates among the priority places its
 *          loads hold; loads in no group never move.
 */

#include <unity.h>

#include "config_types.h"
#include "load_map.h"

void setUp(void) {}
void tearDown(void) {}

// ---- the setting ----

void test_no_group(void)
{
  constexpr RotationGroups groups{};
  TEST_ASSERT_EQUAL(0, groups.size());
}

void test_one_group_of_every_local_load(void)
{
  constexpr RotationGroups groups{ { ALL_LOCAL_LOADS() } };
  TEST_ASSERT_EQUAL(1, groups.size());
  TEST_ASSERT_EQUAL(1, groups.getGroup(0).count);
  TEST_ASSERT_EQUAL(ALL_LOCAL_LOADS(), groups.getGroup(0).pins[0]);
}

void test_two_groups_keep_their_codes(void)
{
  constexpr RotationGroups groups{ { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4), LOAD(5) } };
  TEST_ASSERT_EQUAL(2, groups.size());
  TEST_ASSERT_EQUAL(3, groups.getGroup(1).count);
  TEST_ASSERT_EQUAL(LOAD(4), groups.getGroup(1).pins[1]);
}

void test_missing_inner_braces_make_one_load_groups(void)
{
  constexpr RotationGroups groups{ LOAD(0), LOAD(1) };
  TEST_ASSERT_EQUAL(2, groups.size());
  TEST_ASSERT_EQUAL(1, groups.getGroup(0).count);
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_no_group);
  RUN_TEST(test_one_group_of_every_local_load);
  RUN_TEST(test_two_groups_keep_their_codes);
  RUN_TEST(test_missing_inner_braces_make_one_load_groups);

  return UNITY_END();
}
