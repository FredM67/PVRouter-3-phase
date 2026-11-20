/**
 * @file test_main.cpp
 * @brief Native unit tests for pin_edges.h
 *
 * The edges of the active-LOW input pins: pinEdges(), trackPinEdges<pin>() and
 * isRotationRequested(). Each pin keeps its previous level in a static of its own
 * trackPinEdges<pin>(), for the whole run: each test uses pins of its own, so that it starts
 * from the startup state (released).
 */

#include <unity.h>

#include "pin_edges.h"  // Real header - pure C++

void setUp(void) {}
void tearDown(void) {}

inline constexpr bool LOW_{ true };   /**< active: the pin is LOW */
inline constexpr bool HIGH_{ false }; /**< released: the pin is HIGH */

// ============================================================================
// PinEdges and pinEdges()
// ============================================================================

void test_pinEdges_from_the_two_levels(void)
{
  constexpr PinEdges noneHigh{ pinEdges(HIGH_, HIGH_) };
  constexpr PinEdges press{ pinEdges(HIGH_, LOW_) };
  constexpr PinEdges held{ pinEdges(LOW_, LOW_) };
  constexpr PinEdges release{ pinEdges(LOW_, HIGH_) };

  TEST_ASSERT_FALSE(noneHigh.active);
  TEST_ASSERT_FALSE(noneHigh.changed);
  TEST_ASSERT_FALSE(noneHigh.pressed());
  TEST_ASSERT_FALSE(noneHigh.released());

  TEST_ASSERT_TRUE(press.active);
  TEST_ASSERT_TRUE(press.changed);
  TEST_ASSERT_TRUE(press.pressed());
  TEST_ASSERT_FALSE(press.released());

  TEST_ASSERT_TRUE(held.active);
  TEST_ASSERT_FALSE(held.changed);
  TEST_ASSERT_FALSE(held.pressed());
  TEST_ASSERT_FALSE(held.released());

  TEST_ASSERT_FALSE(release.active);
  TEST_ASSERT_TRUE(release.changed);
  TEST_ASSERT_FALSE(release.pressed());
  TEST_ASSERT_TRUE(release.released());
}

void test_changed_is_either_edge_active_is_the_level(void)
{
  // changed: on both edges; active: the level, whether it just changed or not
  TEST_ASSERT_EQUAL(pinEdges(HIGH_, LOW_).changed, pinEdges(LOW_, HIGH_).changed);
  TEST_ASSERT_EQUAL(pinEdges(HIGH_, LOW_).active, pinEdges(LOW_, LOW_).active);
  TEST_ASSERT_EQUAL(pinEdges(LOW_, HIGH_).active, pinEdges(HIGH_, HIGH_).active);
}

// ============================================================================
// trackPinEdges<pin>()
// ============================================================================

void test_startup_is_released(void)
{
  // HIGH at the first reading: no edge, the previous level is "released" at startup
  const PinEdges first{ trackPinEdges< 2 >(HIGH_) };

  TEST_ASSERT_FALSE(first.active);
  TEST_ASSERT_FALSE(first.changed);
  TEST_ASSERT_FALSE(first.released());
}

void test_low_at_startup_is_a_press(void)
{
  const PinEdges first{ trackPinEdges< 3 >(LOW_) };

  TEST_ASSERT_TRUE(first.active);
  TEST_ASSERT_TRUE(first.pressed());
}

void test_press_once_then_held_is_no_edge(void)
{
  trackPinEdges< 4 >(HIGH_);

  const PinEdges press{ trackPinEdges< 4 >(LOW_) };
  TEST_ASSERT_TRUE(press.pressed());

  for (uint8_t second = 0; second != 5; ++second)
  {
    const PinEdges held{ trackPinEdges< 4 >(LOW_) };

    TEST_ASSERT_TRUE(held.active);
    TEST_ASSERT_FALSE(held.changed);
    TEST_ASSERT_FALSE(held.pressed());
    TEST_ASSERT_FALSE(held.released());
  }
}

void test_release_once_then_no_edge(void)
{
  trackPinEdges< 5 >(LOW_);
  trackPinEdges< 5 >(LOW_);

  const PinEdges release{ trackPinEdges< 5 >(HIGH_) };
  TEST_ASSERT_FALSE(release.active);
  TEST_ASSERT_TRUE(release.changed);
  TEST_ASSERT_TRUE(release.released());
  TEST_ASSERT_FALSE(release.pressed());

  const PinEdges after{ trackPinEdges< 5 >(HIGH_) };
  TEST_ASSERT_FALSE(after.changed);
  TEST_ASSERT_FALSE(after.released());
}

void test_each_pin_keeps_its_own_level(void)
{
  // pin 6 pressed, pin 7 untouched: pin 7 still starts released
  trackPinEdges< 6 >(LOW_);
  TEST_ASSERT_FALSE(trackPinEdges< 6 >(LOW_).changed);

  const PinEdges other{ trackPinEdges< 7 >(HIGH_) };
  TEST_ASSERT_FALSE(other.changed);

  // pin 7 pressed now: pin 6, still held, sees no edge
  TEST_ASSERT_TRUE(trackPinEdges< 7 >(LOW_).pressed());
  TEST_ASSERT_FALSE(trackPinEdges< 6 >(LOW_).changed);

  // pin 6 released: pin 7, still held, sees no edge
  TEST_ASSERT_TRUE(trackPinEdges< 6 >(HIGH_).released());
  TEST_ASSERT_FALSE(trackPinEdges< 7 >(LOW_).changed);
}

void test_press_release_press(void)
{
  constexpr bool levels[]{ HIGH_, LOW_, LOW_, HIGH_, HIGH_, LOW_, HIGH_ };
  constexpr bool pressed[]{ false, true, false, false, false, true, false };
  constexpr bool released[]{ false, false, false, true, false, false, true };

  for (uint8_t i = 0; i != sizeof(levels); ++i)
  {
    const PinEdges edges{ trackPinEdges< 8 >(levels[i]) };

    TEST_ASSERT_EQUAL_MESSAGE(levels[i], edges.active, "active");
    TEST_ASSERT_EQUAL_MESSAGE(pressed[i], edges.pressed(), "pressed");
    TEST_ASSERT_EQUAL_MESSAGE(released[i], edges.released(), "released");
  }
}

// ============================================================================
// isRotationRequested()
// ============================================================================

void test_rotation_on_a_press_when_nothing_is_forced(void)
{
  TEST_ASSERT_TRUE(isRotationRequested(pinEdges(HIGH_, LOW_), false));
  TEST_ASSERT_FALSE(isRotationRequested(pinEdges(LOW_, LOW_), false));
  TEST_ASSERT_FALSE(isRotationRequested(pinEdges(LOW_, HIGH_), false));
  TEST_ASSERT_FALSE(isRotationRequested(pinEdges(HIGH_, HIGH_), false));
}

void test_no_rotation_while_forced(void)
{
  TEST_ASSERT_FALSE(isRotationRequested(pinEdges(HIGH_, LOW_), true));
}

void test_a_press_while_forced_is_dropped_not_carried_over(void)
{
  // the rotation pin is read every second, forced or not
  TEST_ASSERT_FALSE(isRotationRequested(trackPinEdges< 12 >(HIGH_), false));

  // pressed during forcing: no rotation
  TEST_ASSERT_FALSE(isRotationRequested(trackPinEdges< 12 >(LOW_), true));
  TEST_ASSERT_FALSE(isRotationRequested(trackPinEdges< 12 >(LOW_), true));

  // forcing ends, the pin still held: no edge any more, no rotation
  TEST_ASSERT_FALSE(isRotationRequested(trackPinEdges< 12 >(LOW_), false));
  TEST_ASSERT_FALSE(isRotationRequested(trackPinEdges< 12 >(HIGH_), false));

  // a new press after forcing rotates
  TEST_ASSERT_TRUE(isRotationRequested(trackPinEdges< 12 >(LOW_), false));
}

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_pinEdges_from_the_two_levels);
  RUN_TEST(test_changed_is_either_edge_active_is_the_level);

  RUN_TEST(test_startup_is_released);
  RUN_TEST(test_low_at_startup_is_a_press);
  RUN_TEST(test_press_once_then_held_is_no_edge);
  RUN_TEST(test_release_once_then_no_edge);
  RUN_TEST(test_each_pin_keeps_its_own_level);
  RUN_TEST(test_press_release_press);

  RUN_TEST(test_rotation_on_a_press_when_nothing_is_forced);
  RUN_TEST(test_no_rotation_while_forced);
  RUN_TEST(test_a_press_while_forced_is_dropped_not_carried_over);

  return UNITY_END();
}
