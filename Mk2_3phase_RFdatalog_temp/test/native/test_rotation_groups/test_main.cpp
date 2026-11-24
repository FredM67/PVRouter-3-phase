// SPDX-License-Identifier: GPL-3.0-or-later
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
#include "load_priorities.h"
#include "rotation_groups.h"

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

// ---- from codes to load numbers ----

namespace
{
constexpr uint8_t ON{ loadStateOnBit };

// 3 local loads, then 2 on unit 1, then 1 on unit 2
constexpr uint8_t loadMap[]{ Load::local(5), Load::local(6), Load::local(7), Load::remote(1), Load::remote(1), Load::remote(2) };
constexpr uint8_t prio[]{ 0, 1, 2, 3, 4, 5 };

template< typename Groups >
constexpr bool allChecksPass(const Groups& groups)
{
  return Rotation::targetsAreLoads(groups, loadMap) && Rotation::groupsHaveTwoLoads(groups, loadMap)
         && Rotation::noLoadTwice(groups, loadMap) && Rotation::groupsAreConsistent(groups, loadMap);
}
}  // namespace

void test_loads_of_load_and_local_load(void)
{
  TEST_ASSERT_EQUAL_HEX32(0b10, Rotation::loadsOf(LOAD(1), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0b100, Rotation::loadsOf(LOCAL_LOAD(2), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(LOCAL_LOAD(3), loadMap));  // load 3 is remote
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(LOAD(6), loadMap));        // only 6 loads
}

void test_loads_of_remote_load_follows_map_order(void)
{
  constexpr uint8_t mixed[]{ Load::local(5), Load::remote(1), Load::local(6), Load::remote(2) };
  TEST_ASSERT_EQUAL_HEX32(0b0010, Rotation::loadsOf(REMOTE_LOAD(0), mixed));
  TEST_ASSERT_EQUAL_HEX32(0b1000, Rotation::loadsOf(REMOTE_LOAD(1), mixed));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(REMOTE_LOAD(2), mixed));
}

void test_loads_of_pin_is_the_local_load_on_it(void)
{
  TEST_ASSERT_EQUAL_HEX32(0b10, Rotation::loadsOf(6, loadMap));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(9, loadMap));
}

void test_loads_of_groups_and_relays(void)
{
  TEST_ASSERT_EQUAL_HEX32(0b000111, Rotation::loadsOf(ALL_LOCAL_LOADS(), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0b111000, Rotation::loadsOf(ALL_REMOTE_LOADS(), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0b111111, Rotation::loadsOf(ALL_LOADS(), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(RELAY(0), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(ALL_RELAYS(), loadMap));
  TEST_ASSERT_EQUAL_HEX32(0, Rotation::loadsOf(ALL_LOADS_AND_RELAYS(), loadMap));
}

// ---- the checks ----

void test_checks_pass_for_two_good_groups(void)
{
  constexpr RotationGroups groups{ { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4) } };
  TEST_ASSERT_TRUE(allChecksPass(groups));
}

void test_check_relay_is_not_a_load(void)
{
  constexpr RotationGroups groups{ { LOAD(0), RELAY(0) } };
  TEST_ASSERT_FALSE(Rotation::targetsAreLoads(groups, loadMap));
}

void test_check_group_of_one(void)
{
  constexpr RotationGroups groups{ { LOAD(0) }, { LOAD(1), LOAD(2) } };
  TEST_ASSERT_FALSE(Rotation::groupsHaveTwoLoads(groups, loadMap));
}

void test_check_load_twice(void)
{
  constexpr RotationGroups across{ { LOAD(0), LOAD(1) }, { LOAD(1), LOAD(2) } };
  constexpr RotationGroups within{ { LOAD(0), LOAD(0), LOAD(1) } };
  constexpr RotationGroups byGroup{ { ALL_LOCAL_LOADS(), LOAD(1) } };
  TEST_ASSERT_FALSE(Rotation::noLoadTwice(across, loadMap));
  TEST_ASSERT_FALSE(Rotation::noLoadTwice(within, loadMap));
  TEST_ASSERT_FALSE(Rotation::noLoadTwice(byGroup, loadMap));
}

void test_check_mixed_local_and_remote(void)
{
  constexpr RotationGroups groups{ { LOAD(2), LOAD(3) } };
  TEST_ASSERT_FALSE(Rotation::groupsAreConsistent(groups, loadMap));
}

void test_check_two_remote_units(void)
{
  constexpr RotationGroups groups{ { LOAD(4), LOAD(5) } };
  TEST_ASSERT_FALSE(Rotation::groupsAreConsistent(groups, loadMap));
}

void test_checks_pass_without_groups(void)
{
  constexpr RotationGroups groups{};
  TEST_ASSERT_TRUE(allChecksPass(groups));
}

// ---- the rotation ----

void test_one_group_of_every_load_is_todays_rotation(void)
{
  constexpr uint8_t map4[]{ Load::local(5), Load::local(6), Load::local(7), Load::local(8) };
  constexpr uint8_t prio4[]{ 0, 1, 2, 3 };
  constexpr Rotation::Table table{ RotationGroups{ { ALL_LOADS() } }, map4, prio4 };
  uint8_t a[]{ 0, 1, 2 | ON, 3 };
  uint8_t b[]{ 0, 1, 2 | ON, 3 };
  table.rotate(a);
  LoadPriorities::rotate(b);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(b, a, 4);
}

void test_two_interleaved_groups_rotate_apart(void)
{
  constexpr uint8_t map6[]{ Load::local(2), Load::local(3), Load::local(4), Load::local(5), Load::local(6), Load::local(7) };
  constexpr uint8_t prio6[]{ 0, 3, 1, 4, 2, 5 };
  constexpr Rotation::Table table{ RotationGroups{ { LOAD(0), LOAD(1), LOAD(2) }, { LOAD(3), LOAD(4), LOAD(5) } }, map6, prio6 };
  uint8_t p[]{ 0, 3, 1, 4, 2, 5 };
  table.rotate(p);
  constexpr uint8_t expected[]{ 2, 5, 0, 3, 1, 4 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, p, 6);
}

void test_loads_outside_groups_never_move(void)
{
  constexpr Rotation::Table table{ RotationGroups{ { LOAD(0), LOAD(1), LOAD(2) } }, loadMap, prio };
  uint8_t p[]{ 0, 1, 2, 3, 4, 5 };
  table.rotate(p);
  constexpr uint8_t expected[]{ 2, 0, 1, 3, 4, 5 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, p, 6);
}

void test_on_bits_travel_with_their_load(void)
{
  constexpr uint8_t map3[]{ Load::local(5), Load::local(6), Load::local(7) };
  constexpr uint8_t prio3[]{ 0, 1, 2 };
  constexpr Rotation::Table table{ RotationGroups{ { LOAD(0), LOAD(1) } }, map3, prio3 };
  uint8_t p[]{ 0 | ON, 1, 2 };
  table.rotate(p);
  constexpr uint8_t expected[]{ 1, 0 | ON, 2 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, p, 3);
}

void test_n_rotations_are_back_to_the_start(void)
{
  constexpr Rotation::Table table{ RotationGroups{ { LOAD(3), LOAD(4) }, { ALL_LOCAL_LOADS() } }, loadMap, prio };
  uint8_t p[]{ 0, 1 | ON, 2, 3, 4 | ON, 5 };
  for (uint8_t i = 0; i != 6; ++i)  // 6: a multiple of both group sizes
  {
    table.rotate(p);
  }
  constexpr uint8_t expected[]{ 0, 1 | ON, 2, 3, 4 | ON, 5 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, p, 6);
}

void test_table_without_groups_changes_nothing(void)
{
  constexpr Rotation::Table table{ RotationGroups{}, loadMap, prio };
  uint8_t p[]{ 5, 4, 3, 2, 1, 0 };
  table.rotate(p);
  constexpr uint8_t expected[]{ 5, 4, 3, 2, 1, 0 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, p, 6);
  TEST_ASSERT_EQUAL(0, table.size());
}

void test_table_gives_the_loads_of_each_group(void)
{
  constexpr Rotation::Table table{ RotationGroups{ { LOAD(3), LOAD(4) }, { ALL_LOCAL_LOADS() } }, loadMap, prio };
  TEST_ASSERT_EQUAL(2, table.size());
  TEST_ASSERT_EQUAL_HEX32(0b011000, table.loads(0));
  TEST_ASSERT_EQUAL_HEX32(0b000111, table.loads(1));
}

int main(int /* argc */, char** /* argv */)
{
  UNITY_BEGIN();

  RUN_TEST(test_no_group);
  RUN_TEST(test_one_group_of_every_local_load);
  RUN_TEST(test_two_groups_keep_their_codes);
  RUN_TEST(test_missing_inner_braces_make_one_load_groups);

  RUN_TEST(test_loads_of_load_and_local_load);
  RUN_TEST(test_loads_of_remote_load_follows_map_order);
  RUN_TEST(test_loads_of_pin_is_the_local_load_on_it);
  RUN_TEST(test_loads_of_groups_and_relays);
  RUN_TEST(test_checks_pass_for_two_good_groups);
  RUN_TEST(test_check_relay_is_not_a_load);
  RUN_TEST(test_check_group_of_one);
  RUN_TEST(test_check_load_twice);
  RUN_TEST(test_check_mixed_local_and_remote);
  RUN_TEST(test_check_two_remote_units);
  RUN_TEST(test_checks_pass_without_groups);
  RUN_TEST(test_one_group_of_every_load_is_todays_rotation);
  RUN_TEST(test_two_interleaved_groups_rotate_apart);
  RUN_TEST(test_loads_outside_groups_never_move);
  RUN_TEST(test_on_bits_travel_with_their_load);
  RUN_TEST(test_n_rotations_are_back_to_the_start);
  RUN_TEST(test_table_without_groups_changes_nothing);
  RUN_TEST(test_table_gives_the_loads_of_each_group);

  return UNITY_END();
}
