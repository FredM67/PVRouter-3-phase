/**
 * @file test_main.cpp
 * @brief Native unit tests for load_map.h and remote_loads_core.h
 *
 * Tests the packed load-map encoding, the per-unit bitmask builder, the override
 * pins derived from the map, the config checks and the send loop.
 * No config.h, no Arduino, no stubs - just the real headers.
 */

#include <unity.h>

#include "load_map.h"           // Real header - pure C++
#include "remote_loads_core.h"  // Real header - pure C++
#include "utils_override.h"     // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Helpers
// ============================================================================

/** @brief Run one mains cycle: feed the states to the core, for the given map. */
template< const auto &Map, uint8_t NumUnits, uint8_t NumLoads >
static void cycle(RemoteLoadCore< NumUnits > &core, const LoadStates (&states)[NumLoads])
{
  core.template updateLoads< Map >(states);
}

/** @brief Claim a unit's payload if a transmission is due, else return -1. */
template< uint8_t NumUnits >
static int drain(RemoteLoadCore< NumUnits > &core, uint8_t unitIdx)
{
  uint8_t payload{ 0 };
  return core.takePending(unitIdx, payload) ? static_cast< int >(payload) : -1;
}

/** @brief Build a LoadStates array from a bit pattern, bit i = load i. */
template< uint8_t NumLoads >
static void setStates(LoadStates (&states)[NumLoads], uint8_t pattern)
{
  for (uint8_t i = 0; i < NumLoads; ++i)
  {
    states[i] = (pattern & (1U << i)) ? LoadStates::LOAD_ON : LoadStates::LOAD_OFF;
  }
}

// ============================================================================
// Tests for the packed encoding
// ============================================================================

void test_local_keeps_unit_zero(void)
{
  TEST_ASSERT_TRUE(Load::isLocal(Load::local(5)));
  TEST_ASSERT_EQUAL(0, Load::unitOf(Load::local(5)));
  TEST_ASSERT_EQUAL(5, Load::pinOf(Load::local(5)));
  TEST_ASSERT_EQUAL(13, Load::pinOf(Load::local(13)));
}

void test_remote_encodes_the_unit_number(void)
{
  TEST_ASSERT_EQUAL_HEX8(0x40, Load::remote(1));
  TEST_ASSERT_EQUAL_HEX8(0x80, Load::remote(2));
  TEST_ASSERT_EQUAL_HEX8(0xC0, Load::remote(3));

  TEST_ASSERT_FALSE(Load::isLocal(Load::remote(1)));
  TEST_ASSERT_EQUAL(1, Load::unitOf(Load::remote(1)));
  TEST_ASSERT_EQUAL(2, Load::unitOf(Load::remote(2)));
  TEST_ASSERT_EQUAL(3, Load::unitOf(Load::remote(3)));
}

void test_remote_without_led_has_pin_zero(void)
{
  TEST_ASSERT_EQUAL(0, Load::pinOf(Load::remote(1)));
  TEST_ASSERT_EQUAL(0, Load::pinOf(Load::remote(3)));
}

void test_remote_led_pin_round_trips(void)
{
  TEST_ASSERT_EQUAL(6, Load::pinOf(Load::remote(1, 6)));
  TEST_ASSERT_EQUAL(2, Load::unitOf(Load::remote(2, 13)));
  TEST_ASSERT_EQUAL(13, Load::pinOf(Load::remote(2, 13)));
}

void test_census_is_constexpr(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1, 6), Load::remote(2, 7), Load::remote(1) };

  static_assert(Load::countUnits(map) == 2, "highest unit is 2");
  static_assert(Load::countRemoteLoads(map) == 3, "three remote loads");
  static_assert(Load::maxLoadsPerUnit(map) == 2, "unit 1 carries two loads");

  TEST_ASSERT_EQUAL(2, Load::countUnits(map));
  TEST_ASSERT_EQUAL(3, Load::countRemoteLoads(map));
  TEST_ASSERT_EQUAL(2, Load::maxLoadsPerUnit(map));
}

void test_census_of_an_all_local_map(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::local(6), Load::local(7) };

  static_assert(Load::countUnits(map) == 0, "no remote unit");
  static_assert(Load::countRemoteLoads(map) == 0, "no remote load");
  static_assert(Load::maxLoadsPerUnit(map) == 0, "no remote load");

  TEST_ASSERT_EQUAL(0, Load::countUnits(map));
}

void test_remoteOrdinal_counts_remote_entries_only(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::local(6), Load::remote(2), Load::remote(1) };

  TEST_ASSERT_EQUAL(0, Load::remoteOrdinal(map, 1));
  TEST_ASSERT_EQUAL(1, Load::remoteOrdinal(map, 3));
  TEST_ASSERT_EQUAL(2, Load::remoteOrdinal(map, 4));
}

void test_bitOf_counts_within_the_same_unit(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(2), Load::remote(1), Load::remote(2), Load::remote(1) };

  TEST_ASSERT_EQUAL(0, Load::bitOf(map, 0));
  TEST_ASSERT_EQUAL(0, Load::bitOf(map, 1));
  TEST_ASSERT_EQUAL(1, Load::bitOf(map, 2));
  TEST_ASSERT_EQUAL(1, Load::bitOf(map, 3));
  TEST_ASSERT_EQUAL(2, Load::bitOf(map, 4));
}

// ============================================================================
// Tests for unit grouping
// ============================================================================

void test_single_unit_payload(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(1), Load::remote(1) };
  LoadStates states[3]{};
  RemoteLoadCore< 1 > core;

  setStates(states, 0b101);
  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0b101, core.payloadOf(0));
}

void test_local_loads_are_ignored(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::local(7) };
  LoadStates states[3]{ LoadStates::LOAD_ON, LoadStates::LOAD_OFF, LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0, core.payloadOf(0));
}

void test_two_units_are_independent(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(2), Load::remote(1), Load::remote(2) };
  LoadStates states[4]{};
  RemoteLoadCore< 2 > core;

  // load 0 (unit 1, bit 0) and load 3 (unit 2, bit 1) are ON
  setStates(states, 0b1001);
  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0b01, core.payloadOf(0));
  TEST_ASSERT_EQUAL_HEX8(0b10, core.payloadOf(1));
}

void test_three_units_one_load_each(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(2), Load::remote(3) };
  LoadStates states[3]{};
  RemoteLoadCore< 3 > core;

  setStates(states, 0b101);
  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(1, core.payloadOf(0));
  TEST_ASSERT_EQUAL_HEX8(0, core.payloadOf(1));
  TEST_ASSERT_EQUAL_HEX8(1, core.payloadOf(2));
}

void test_interleaved_map(void)
{
  // the map used as an example in the documentation
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1, 6), Load::remote(2, 7), Load::remote(1) };
  LoadStates states[4]{};
  RemoteLoadCore< 2 > core;

  // everything ON: unit 1 holds loads 1 and 3, unit 2 holds load 2
  setStates(states, 0b1111);
  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0b11, core.payloadOf(0));
  TEST_ASSERT_EQUAL_HEX8(0b01, core.payloadOf(1));
}

// ============================================================================
// Bit ordering - regression pin for the descending/ascending mismatch
// ============================================================================

void test_first_load_of_a_unit_is_bit_zero(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(1), Load::remote(1) };
  LoadStates states[3]{};
  RemoteLoadCore< 1 > core;

  setStates(states, 0b001);
  cycle< map >(core, states);
  TEST_ASSERT_EQUAL_HEX8(0b001, core.payloadOf(0));

  setStates(states, 0b100);
  cycle< map >(core, states);
  TEST_ASSERT_EQUAL_HEX8(0b100, core.payloadOf(0));
}

void test_eight_loads_on_one_unit(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(1), Load::remote(1), Load::remote(1),
                                  Load::remote(1), Load::remote(1), Load::remote(1), Load::remote(1) };
  LoadStates states[8]{};
  RemoteLoadCore< 1 > core;

  setStates(states, 0x80);
  cycle< map >(core, states);
  TEST_ASSERT_EQUAL_HEX8(0x80, core.payloadOf(0));

  setStates(states, 0xFF);
  cycle< map >(core, states);
  TEST_ASSERT_EQUAL_HEX8(0xFF, core.payloadOf(0));
}

void test_index_alignment_with_a_leading_local_load(void)
{
  // Regression pin: the previous implementation filled a separate remote-state array
  // ascending and consumed it descending, so this map produced 0b10 instead of 0b01.
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::remote(1) };
  LoadStates states[3]{ LoadStates::LOAD_OFF, LoadStates::LOAD_ON, LoadStates::LOAD_OFF };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0b01, core.payloadOf(0));
}

// ============================================================================
// Change detection
// ============================================================================

void test_change_flags_a_transmission(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);

  TEST_ASSERT_TRUE(core.isPending(0));
  TEST_ASSERT_EQUAL(1, drain(core, 0));
}

void test_draining_clears_the_flag(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);

  TEST_ASSERT_EQUAL(1, drain(core, 0));
  TEST_ASSERT_FALSE(core.isPending(0));
  TEST_ASSERT_EQUAL(-1, drain(core, 0));
}

void test_unchanged_state_does_not_flag(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);
  TEST_ASSERT_EQUAL(1, drain(core, 0));

  cycle< map >(core, states);
  TEST_ASSERT_FALSE(core.isPending(0));
}

void test_payload_tracks_the_latest_state_even_if_unsent(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(1) };
  LoadStates states[2]{};
  RemoteLoadCore< 1 > core;

  setStates(states, 0b01);
  cycle< map >(core, states);

  // never drained
  setStates(states, 0b10);
  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(0b10, core.payloadOf(0));
  TEST_ASSERT_EQUAL(0b10, drain(core, 0));
}

void test_a_change_on_one_unit_does_not_flag_the_other(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(2) };
  LoadStates states[2]{};
  RemoteLoadCore< 2 > core;

  cycle< map >(core, states);
  // both start at 0, which matches the initial previousBitmask: nothing to send
  TEST_ASSERT_FALSE(core.isPending(0));
  TEST_ASSERT_FALSE(core.isPending(1));

  setStates(states, 0b01);
  cycle< map >(core, states);

  TEST_ASSERT_TRUE(core.isPending(0));
  TEST_ASSERT_FALSE(core.isPending(1));
}

// ============================================================================
// Keep-alive refresh
// ============================================================================

void test_refresh_fires_on_the_fifth_unchanged_cycle(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);
  TEST_ASSERT_EQUAL(1, drain(core, 0));

  for (uint8_t i = 1; i < REMOTE_REFRESH_CYCLES; ++i)
  {
    cycle< map >(core, states);
    TEST_ASSERT_FALSE(core.isPending(0));
  }

  cycle< map >(core, states);
  TEST_ASSERT_TRUE(core.isPending(0));
  TEST_ASSERT_EQUAL(1, drain(core, 0));
}

void test_refresh_counter_restarts_after_a_refresh(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  for (uint8_t i = 0; i < 2 * REMOTE_REFRESH_CYCLES + 1; ++i)
  {
    cycle< map >(core, states);
    uint8_t payload{ 0 };
    core.takePending(0, payload);
  }

  // two full refresh periods elapsed without the counter running away
  cycle< map >(core, states);
  TEST_ASSERT_FALSE(core.isPending(0));
}

void test_a_change_restarts_the_refresh_counter(void)
{
  static constexpr uint8_t map[]{ Load::remote(1) };
  LoadStates states[1]{ LoadStates::LOAD_ON };
  RemoteLoadCore< 1 > core;

  cycle< map >(core, states);
  TEST_ASSERT_EQUAL(1, drain(core, 0));

  // three quiet cycles
  for (uint8_t i = 1; i < 4; ++i)
  {
    cycle< map >(core, states);
  }

  // a change, which resets the counter
  states[0] = LoadStates::LOAD_OFF;
  cycle< map >(core, states);
  TEST_ASSERT_EQUAL(0, drain(core, 0));

  // the refresh must now be a full period away, not one cycle away
  cycle< map >(core, states);
  TEST_ASSERT_FALSE(core.isPending(0));
}

// ============================================================================
// Edge cases
// ============================================================================

void test_all_off_still_refreshes(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(1) };
  LoadStates states[2]{ LoadStates::LOAD_OFF, LoadStates::LOAD_OFF };
  RemoteLoadCore< 1 > core;

  for (uint8_t i = 0; i < REMOTE_REFRESH_CYCLES; ++i)
  {
    cycle< map >(core, states);
  }

  TEST_ASSERT_TRUE(core.isPending(0));
  TEST_ASSERT_EQUAL(0, drain(core, 0));
}

void test_no_unit_configured_is_a_no_op(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::local(6) };
  LoadStates states[2]{ LoadStates::LOAD_ON, LoadStates::LOAD_ON };
  RemoteLoadCore< 0 > core;

  cycle< map >(core, states);

  TEST_ASSERT_EQUAL(0, RemoteLoadCore< 0 >::size());
  TEST_ASSERT_FALSE(core.isPending(0));
  TEST_ASSERT_EQUAL(-1, drain(core, 0));
}

void test_reset_clears_every_unit(void)
{
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(2) };
  LoadStates states[2]{ LoadStates::LOAD_ON, LoadStates::LOAD_ON };
  RemoteLoadCore< 2 > core;

  cycle< map >(core, states);
  TEST_ASSERT_TRUE(core.isPending(0));

  core.reset();

  TEST_ASSERT_FALSE(core.isPending(0));
  TEST_ASSERT_FALSE(core.isPending(1));
  TEST_ASSERT_EQUAL_HEX8(0, core.payloadOf(0));
  TEST_ASSERT_EQUAL_HEX8(0, core.payloadOf(1));
}

void test_a_unit_beyond_the_core_is_ignored(void)
{
  // unit 3 has no slot in a 2-unit core: it must be skipped, not written past the array
  static constexpr uint8_t map[]{ Load::remote(1), Load::remote(3), Load::remote(2) };
  LoadStates states[3]{ LoadStates::LOAD_ON, LoadStates::LOAD_ON, LoadStates::LOAD_ON };
  RemoteLoadCore< 2 > core;

  cycle< map >(core, states);

  TEST_ASSERT_EQUAL_HEX8(1, core.payloadOf(0));
  TEST_ASSERT_EQUAL_HEX8(1, core.payloadOf(1));
}

void test_out_of_range_accessors_are_safe(void)
{
  RemoteLoadCore< 1 > core;

  TEST_ASSERT_EQUAL_HEX8(0, core.payloadOf(7));
  TEST_ASSERT_FALSE(core.isPending(7));
  TEST_ASSERT_EQUAL(-1, drain(core, 7));
}

// ============================================================================
// Override pins - the map seen through the override subsystem
// ============================================================================

void test_overridePinOf_local_is_the_physical_pin(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::local(7) };

  static_assert(overridePinOf(map, 0) == 5, "a local load keeps its pin");

  TEST_ASSERT_EQUAL_UINT8(5, overridePinOf(map, 0));
  TEST_ASSERT_EQUAL_UINT8(7, overridePinOf(map, 2));
}

void test_overridePinOf_remote_is_a_virtual_pin(void)
{
  // the virtual pin counts remote loads across every unit, in map order
  static constexpr uint8_t map[]{ Load::remote(2), Load::local(5), Load::remote(1), Load::remote(2) };

  TEST_ASSERT_EQUAL_UINT8(REMOTE_PIN_BASE + 0, overridePinOf(map, 0));
  TEST_ASSERT_EQUAL_UINT8(5, overridePinOf(map, 1));
  TEST_ASSERT_EQUAL_UINT8(REMOTE_PIN_BASE + 1, overridePinOf(map, 2));
  TEST_ASSERT_EQUAL_UINT8(REMOTE_PIN_BASE + 2, overridePinOf(map, 3));
}

void test_load_masks_of_a_mixed_map(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::local(7), Load::remote(2) };

  TEST_ASSERT_EQUAL_HEX32((1UL << 5) | (1UL << 7), localLoadsMask(map));
  TEST_ASSERT_EQUAL_HEX32((1UL << 16) | (1UL << 17), remoteLoadsMask(map));
}

void test_load_masks_of_an_all_local_map(void)
{
  static constexpr uint8_t map[]{ Load::local(5), Load::local(6) };

  TEST_ASSERT_EQUAL_HEX32((1UL << 5) | (1UL << 6), localLoadsMask(map));
  TEST_ASSERT_EQUAL_HEX32(0, remoteLoadsMask(map));
}

void test_masks_feed_the_override_config(void)
{
  // what ALL_LOADS() and LOAD(n) hand to OverridePins in config.h
  static constexpr uint8_t map[]{ Load::local(5), Load::remote(1), Load::remote(2), Load::local(6) };
  constexpr OverridePins pins{
    { KeyIndexPair< 8 >{ 4, localLoadsMask(map) | remoteLoadsMask(map) },
      KeyIndexPair< 8 >{ 3, { overridePinOf(map, 2) } } }
  };

  TEST_ASSERT_EQUAL_HEX16((1U << 5) | (1U << 6), pins.getLocalBitmask(0));
  TEST_ASSERT_EQUAL_HEX8(0b11, pins.getRemoteBitmask(0));

  // load 2 is the second remote load, whichever unit it sits on
  TEST_ASSERT_EQUAL_HEX16(0, pins.getLocalBitmask(1));
  TEST_ASSERT_EQUAL_HEX8(0b10, pins.getRemoteBitmask(1));
}

// ============================================================================
// Load map validation (validation.h asserts on these)
// ============================================================================

void test_isValidMap_accepts_a_mixed_map(void)
{
  static constexpr uint8_t map[]{ Load::local(2), Load::local(13), Load::remote(1), Load::remote(3, 9) };

  static_assert(Load::isValidMap(map), "a well-formed map must pass");
  TEST_ASSERT_TRUE(Load::isValidMap(map));
}

void test_isValidMap_rejects_unusable_local_pins(void)
{
  static constexpr uint8_t serialRx[]{ Load::local(0) };
  static constexpr uint8_t serialTx[]{ Load::local(1) };
  static constexpr uint8_t missing[]{ Load::local(14) };

  TEST_ASSERT_FALSE(Load::isValidMap(serialRx));
  TEST_ASSERT_FALSE(Load::isValidMap(serialTx));
  TEST_ASSERT_FALSE(Load::isValidMap(missing));
}

void test_isValidMap_rejects_the_unused_pin_sentinel(void)
{
  // unused_pin is 0xFF, which decodes as unit 3 with a status LED on pin 63
  static constexpr uint8_t map[]{ Load::local(5), 0xFF };

  static_assert(!Load::isValidMap(map), "the unused_pin sentinel must be caught");
  TEST_ASSERT_FALSE(Load::isValidMap(map));
}

void test_isValidMap_checks_a_remote_status_led(void)
{
  static constexpr uint8_t noLed[]{ Load::remote(1) };
  static constexpr uint8_t ledOnSerial[]{ Load::remote(1, 1) };
  static constexpr uint8_t ledOk[]{ Load::remote(1, 8) };

  TEST_ASSERT_TRUE(Load::isValidMap(noLed));
  TEST_ASSERT_FALSE(Load::isValidMap(ledOnSerial));
  TEST_ASSERT_TRUE(Load::isValidMap(ledOk));
}

// ============================================================================
// Node ID validation (validation.h asserts on this)
// ============================================================================

void test_node_ids_accepts_distinct_ids(void)
{
  static constexpr uint8_t ids[]{ 15, 16, 17 };

  static_assert(areValidNodeIds(ids, 3, 10), "distinct in-range IDs must pass");
  TEST_ASSERT_TRUE(areValidNodeIds(ids, 3, 10));
  TEST_ASSERT_TRUE(areValidNodeIds(ids, 0, 10));  // no remote unit: nothing to check
}

void test_node_ids_rejects_a_duplicate(void)
{
  static constexpr uint8_t ids[]{ 15, 15, 17 };

  TEST_ASSERT_FALSE(areValidNodeIds(ids, 2, 10));
  TEST_ASSERT_TRUE(areValidNodeIds(ids, 1, 10));  // entries past the last unit are ignored
}

void test_node_ids_rejects_the_router_id(void)
{
  static constexpr uint8_t ids[]{ 15, 10 };

  TEST_ASSERT_FALSE(areValidNodeIds(ids, 2, 10));
}

void test_node_ids_rejects_out_of_range(void)
{
  static constexpr uint8_t zero[]{ 0 };
  static constexpr uint8_t tooHigh[]{ 31 };
  static constexpr uint8_t bounds[]{ 1, 30 };

  TEST_ASSERT_FALSE(areValidNodeIds(zero, 1, 10));
  TEST_ASSERT_FALSE(areValidNodeIds(tooHigh, 1, 10));
  TEST_ASSERT_TRUE(areValidNodeIds(bounds, 2, 10));
}

void test_node_ids_rejects_a_short_table(void)
{
  static constexpr uint8_t ids[]{ 15 };

  TEST_ASSERT_FALSE(areValidNodeIds(ids, 2, 10));
}

// ============================================================================
// Sending - the radio replaced by a recorder
// ============================================================================

/** @brief Stands in for RFM69::send(): records who would have been sent what. */
struct RecordingRadio
{
  uint8_t nodeId[8]{};
  uint8_t payload[8]{};
  uint8_t count{ 0 };

  void operator()(uint8_t id, uint8_t data)
  {
    if (count < 8)
    {
      nodeId[count] = id;
      payload[count] = data;
    }
    ++count;
  }
};

static constexpr uint8_t sendMap[]{ Load::local(5), Load::remote(1), Load::remote(2), Load::remote(1) };
static constexpr uint8_t sendIds[]{ 15, 16, 17 };

void test_sendPending_addresses_each_unit_by_its_node_id(void)
{
  RemoteLoadCore< 2 > core;
  RecordingRadio radio;
  LoadStates states[4];

  setStates(states, 0b1100);  // unit 1: loads 1 (off) and 3 (on) -> 0b10 ; unit 2: load 2 (on) -> 0b1
  cycle< sendMap >(core, states);
  core.sendPending(sendIds, radio);

  TEST_ASSERT_EQUAL_UINT8(2, radio.count);
  TEST_ASSERT_EQUAL_UINT8(15, radio.nodeId[0]);
  TEST_ASSERT_EQUAL_HEX8(0b10, radio.payload[0]);
  TEST_ASSERT_EQUAL_UINT8(16, radio.nodeId[1]);
  TEST_ASSERT_EQUAL_HEX8(0b1, radio.payload[1]);
}

void test_sendPending_drains_what_it_sends(void)
{
  RemoteLoadCore< 2 > core;
  RecordingRadio radio;
  LoadStates states[4];

  setStates(states, 0b1100);
  cycle< sendMap >(core, states);
  core.sendPending(sendIds, radio);
  core.sendPending(sendIds, radio);  // nothing new since

  TEST_ASSERT_EQUAL_UINT8(2, radio.count);
}

void test_sendPending_only_addresses_the_unit_that_changed(void)
{
  RemoteLoadCore< 2 > core;
  RecordingRadio radio;
  LoadStates states[4];

  setStates(states, 0b1100);
  cycle< sendMap >(core, states);
  core.sendPending(sendIds, RecordingRadio{});  // flush the first cycle

  setStates(states, 0b1000);  // only load 2 (unit 2) switches off
  cycle< sendMap >(core, states);
  core.sendPending(sendIds, radio);

  TEST_ASSERT_EQUAL_UINT8(1, radio.count);
  TEST_ASSERT_EQUAL_UINT8(16, radio.nodeId[0]);
  TEST_ASSERT_EQUAL_HEX8(0, radio.payload[0]);
}

void test_sendPending_refreshes_every_unit(void)
{
  RemoteLoadCore< 2 > core;
  RecordingRadio radio;
  LoadStates states[4];

  setStates(states, 0b1100);
  cycle< sendMap >(core, states);
  core.sendPending(sendIds, RecordingRadio{});

  for (uint8_t i = 0; i < REMOTE_REFRESH_CYCLES; ++i)
  {
    cycle< sendMap >(core, states);
  }
  core.sendPending(sendIds, radio);

  TEST_ASSERT_EQUAL_UINT8(2, radio.count);
  TEST_ASSERT_EQUAL_UINT8(15, radio.nodeId[0]);
  TEST_ASSERT_EQUAL_UINT8(16, radio.nodeId[1]);
}

void test_sendPending_without_units_never_sends(void)
{
  RemoteLoadCore< 0 > core;
  RecordingRadio radio;

  core.sendPending(sendIds, radio);

  TEST_ASSERT_EQUAL_UINT8(0, radio.count);
}

// ============================================================================

int main(int, char **)
{
  UNITY_BEGIN();

  // Packed encoding
  RUN_TEST(test_local_keeps_unit_zero);
  RUN_TEST(test_remote_encodes_the_unit_number);
  RUN_TEST(test_remote_without_led_has_pin_zero);
  RUN_TEST(test_remote_led_pin_round_trips);
  RUN_TEST(test_census_is_constexpr);
  RUN_TEST(test_census_of_an_all_local_map);
  RUN_TEST(test_remoteOrdinal_counts_remote_entries_only);
  RUN_TEST(test_bitOf_counts_within_the_same_unit);

  // Unit grouping
  RUN_TEST(test_single_unit_payload);
  RUN_TEST(test_local_loads_are_ignored);
  RUN_TEST(test_two_units_are_independent);
  RUN_TEST(test_three_units_one_load_each);
  RUN_TEST(test_interleaved_map);

  // Bit ordering
  RUN_TEST(test_first_load_of_a_unit_is_bit_zero);
  RUN_TEST(test_eight_loads_on_one_unit);
  RUN_TEST(test_index_alignment_with_a_leading_local_load);

  // Change detection
  RUN_TEST(test_change_flags_a_transmission);
  RUN_TEST(test_draining_clears_the_flag);
  RUN_TEST(test_unchanged_state_does_not_flag);
  RUN_TEST(test_payload_tracks_the_latest_state_even_if_unsent);
  RUN_TEST(test_a_change_on_one_unit_does_not_flag_the_other);

  // Keep-alive refresh
  RUN_TEST(test_refresh_fires_on_the_fifth_unchanged_cycle);
  RUN_TEST(test_refresh_counter_restarts_after_a_refresh);
  RUN_TEST(test_a_change_restarts_the_refresh_counter);

  // Edge cases
  RUN_TEST(test_all_off_still_refreshes);
  RUN_TEST(test_no_unit_configured_is_a_no_op);
  RUN_TEST(test_reset_clears_every_unit);
  RUN_TEST(test_a_unit_beyond_the_core_is_ignored);
  RUN_TEST(test_out_of_range_accessors_are_safe);

  RUN_TEST(test_overridePinOf_local_is_the_physical_pin);
  RUN_TEST(test_overridePinOf_remote_is_a_virtual_pin);
  RUN_TEST(test_load_masks_of_a_mixed_map);
  RUN_TEST(test_load_masks_of_an_all_local_map);
  RUN_TEST(test_masks_feed_the_override_config);

  RUN_TEST(test_isValidMap_accepts_a_mixed_map);
  RUN_TEST(test_isValidMap_rejects_unusable_local_pins);
  RUN_TEST(test_isValidMap_rejects_the_unused_pin_sentinel);
  RUN_TEST(test_isValidMap_checks_a_remote_status_led);

  RUN_TEST(test_node_ids_accepts_distinct_ids);
  RUN_TEST(test_node_ids_rejects_a_duplicate);
  RUN_TEST(test_node_ids_rejects_the_router_id);
  RUN_TEST(test_node_ids_rejects_out_of_range);
  RUN_TEST(test_node_ids_rejects_a_short_table);

  RUN_TEST(test_sendPending_addresses_each_unit_by_its_node_id);
  RUN_TEST(test_sendPending_drains_what_it_sends);
  RUN_TEST(test_sendPending_only_addresses_the_unit_that_changed);
  RUN_TEST(test_sendPending_refreshes_every_unit);
  RUN_TEST(test_sendPending_without_units_never_sends);

  return UNITY_END();
}
