#include <unity.h>
#include <cstdio>

// Mock Arduino types and constants for native testing
typedef uint8_t byte;
#define HIGH 1
#define LOW 0

// Mock pin and config definitions
enum relayState_t : byte
{
  RELAY_ON,
  RELAY_OFF
};
enum outputType_t : byte
{
  DIGITAL_PIN,
  RELAY
};

// Simple mock relay output class for testing
class MockRelayOutput
{
public:
  bool m_relay_state;
  int32_t m_import_threshold;
  int32_t m_surplus_threshold;

  MockRelayOutput(int32_t import_threshold = 20, int32_t surplus_threshold = 20)
    : m_relay_state(false), m_import_threshold(import_threshold), m_surplus_threshold(surplus_threshold) {}

  // Core relay logic for negative threshold testing
  bool proceed_relay(int32_t power_value)
  {
    bool new_state{ m_relay_state };

    if (m_import_threshold < 0)
    {
      // Battery scenario: negative threshold means we want surplus above abs(threshold)
      int32_t required_surplus{ -m_import_threshold };
      if (!m_relay_state && power_value >= required_surplus)
      {
        new_state = true;  // Turn on when surplus exceeds threshold
      }
      else if (m_relay_state && power_value < (required_surplus - m_surplus_threshold))
      {
        new_state = false;  // Turn off when surplus drops below (threshold - hysteresis)
      }
    }
    else
    {
      // Normal scenario: positive threshold means import/export boundary
      if (!m_relay_state && power_value >= m_import_threshold)
      {
        new_state = true;  // Turn on when import exceeds threshold
      }
      else if (m_relay_state && power_value < (m_import_threshold - m_surplus_threshold))
      {
        new_state = false;  // Turn off when import drops below threshold minus hysteresis
      }
    }

    m_relay_state = new_state;
    return m_relay_state;
  }

  void reset_state()
  {
    m_relay_state = false;
  }
};

void setUp(void)
{
  // Set up before each test
}

void tearDown(void)
{
  // Clean up after each test
}

void test_normal_positive_threshold()
{
  MockRelayOutput relay(20, 10);  // 20W import threshold, 10W hysteresis

  // Start with relay off, below threshold
  TEST_ASSERT_FALSE(relay.proceed_relay(10));  // 10W import < 20W threshold
  TEST_ASSERT_FALSE(relay.proceed_relay(15));  // Still below threshold

  // Cross threshold - relay should turn on
  TEST_ASSERT_TRUE(relay.proceed_relay(25));  // 25W import > 20W threshold
  TEST_ASSERT_TRUE(relay.proceed_relay(30));  // Stay on above threshold

  // Drop below threshold but above hysteresis - should stay on
  TEST_ASSERT_TRUE(relay.proceed_relay(15));  // 15W > (20-10)W, stay on

  // Drop below hysteresis - should turn off
  TEST_ASSERT_FALSE(relay.proceed_relay(5));  // 5W < (20-10)W, turn off
}

void test_negative_threshold_battery_scenario()
{
  MockRelayOutput relay(-50, 20);  // -50W threshold (need 50W+ surplus), 20W hysteresis

  // Start with relay off - no surplus or small surplus
  TEST_ASSERT_FALSE(relay.proceed_relay(-10));  // 10W import, no surplus
  TEST_ASSERT_FALSE(relay.proceed_relay(0));    // Zero import/export
  TEST_ASSERT_FALSE(relay.proceed_relay(30));   // 30W surplus < 50W required

  // Reach required surplus - relay should turn on
  TEST_ASSERT_TRUE(relay.proceed_relay(60));   // 60W surplus > 50W required
  TEST_ASSERT_TRUE(relay.proceed_relay(100));  // Stay on with more surplus

  // Drop surplus but stay above hysteresis - should stay on
  TEST_ASSERT_TRUE(relay.proceed_relay(40));  // 40W > (50-20)W, stay on

  // Drop below hysteresis - should turn off
  TEST_ASSERT_FALSE(relay.proceed_relay(25));  // 25W < (50-20)W, turn off
}

void test_negative_threshold_edge_cases()
{
  MockRelayOutput relay(-100, 30);  // -100W threshold (need 100W+ surplus), 30W hysteresis

  // Test exactly at boundaries
  TEST_ASSERT_FALSE(relay.proceed_relay(99));  // Just below threshold
  TEST_ASSERT_TRUE(relay.proceed_relay(100));  // Exactly at threshold
  TEST_ASSERT_TRUE(relay.proceed_relay(101));  // Just above threshold

  // Test hysteresis boundary when relay is on
  TEST_ASSERT_TRUE(relay.proceed_relay(71));   // 71W > (100-30)W, stay on
  TEST_ASSERT_TRUE(relay.proceed_relay(70));   // Exactly at hysteresis boundary
  TEST_ASSERT_FALSE(relay.proceed_relay(69));  // Just below hysteresis
}

void test_negative_threshold_import_scenarios()
{
  MockRelayOutput relay(-30, 15);  // -30W threshold, 15W hysteresis

  // Test behavior during import (negative power values)
  TEST_ASSERT_FALSE(relay.proceed_relay(-50));  // Importing 50W, no surplus
  TEST_ASSERT_FALSE(relay.proceed_relay(-10));  // Importing 10W, no surplus
  TEST_ASSERT_FALSE(relay.proceed_relay(0));    // Zero point

  // Move to surplus and cross threshold
  TEST_ASSERT_FALSE(relay.proceed_relay(20));  // 20W surplus < 30W required
  TEST_ASSERT_TRUE(relay.proceed_relay(35));   // 35W surplus > 30W required

  // Drop back through zero to import - should turn off immediately
  TEST_ASSERT_FALSE(relay.proceed_relay(-5));  // Import 5W < hysteresis boundary (15W)
}

void test_zero_threshold_special_case()
{
  MockRelayOutput relay(0, 10);  // 0W threshold (normal scenario)

  // Should behave like normal positive threshold at zero
  TEST_ASSERT_FALSE(relay.proceed_relay(-10));  // Import, relay off
  TEST_ASSERT_TRUE(relay.proceed_relay(5));     // Export, relay on
  TEST_ASSERT_TRUE(relay.proceed_relay(-5));    // Back to import but above hysteresis
  TEST_ASSERT_FALSE(relay.proceed_relay(-15));  // Below hysteresis, relay off
}

void test_large_negative_threshold()
{
  MockRelayOutput relay(-500, 100);  // Very large negative threshold for high-power battery systems

  // Test with realistic battery power levels
  TEST_ASSERT_FALSE(relay.proceed_relay(-1000));  // Heavy import
  TEST_ASSERT_FALSE(relay.proceed_relay(400));    // 400W surplus < 500W required
  TEST_ASSERT_TRUE(relay.proceed_relay(600));     // 600W surplus > 500W required
  TEST_ASSERT_TRUE(relay.proceed_relay(450));     // 450W > (500-100)W hysteresis
  TEST_ASSERT_FALSE(relay.proceed_relay(350));    // 350W < 400W hysteresis boundary
}

void test_state_transitions_with_debug()
{
  MockRelayOutput relay(-40, 20);  // -40W threshold, 20W hysteresis

  printf("DEBUG: Testing negative threshold state transitions\n");
  printf("DEBUG: Threshold=-40W (need 40W+ surplus), Hysteresis=20W\n");

  // Track state changes
  bool prev_state{ false };
  int32_t test_values[] = { -20, 0, 20, 35, 45, 30, 25, 15 };
  const char* descriptions[] = {
    "Import 20W", "Zero", "Surplus 20W", "Surplus 35W",
    "Surplus 45W (ON)", "Surplus 30W", "Surplus 25W", "Surplus 15W (OFF)"
  };

  for (int i = 0; i < 8; i++)
  {
    bool new_state{ relay.proceed_relay(test_values[i]) };
    printf("DEBUG: Power=%dW, State=%s, Description=%s\n",
           (int)test_values[i], new_state ? "ON" : "OFF", descriptions[i]);

    if (i == 4) TEST_ASSERT_TRUE(new_state);   // Should be on at 45W surplus
    if (i == 7) TEST_ASSERT_FALSE(new_state);  // Should be off at 15W surplus

    prev_state = new_state;
  }
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_normal_positive_threshold);
  RUN_TEST(test_negative_threshold_battery_scenario);
  RUN_TEST(test_negative_threshold_edge_cases);
  RUN_TEST(test_negative_threshold_import_scenarios);
  RUN_TEST(test_zero_threshold_special_case);
  RUN_TEST(test_large_negative_threshold);
  RUN_TEST(test_state_transitions_with_debug);

  return UNITY_END();
}
