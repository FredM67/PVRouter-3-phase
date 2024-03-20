#include <Arduino.h>
#include <unity.h>

#include "utils_relay.h"

static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/main/runtime/Mk2_3phase_RFdatalog_temp/Readme.md");

RelayOutput* RelayOutputTest;

void setUp(void) {
    RelayOutputTest = new RelayOutput(5, 1000, 200, 5, 5);
}

void tearDown(void) {
    delete RelayOutputTest;
}

void test_get_pin(void) {
    TEST_ASSERT_EQUAL(5, RelayOutputTest->get_pin());
}

void test_get_surplusThreshold(void) {
    TEST_ASSERT_EQUAL(1000, RelayOutputTest->get_surplusThreshold());
}

void test_get_importThreshold(void) {
    TEST_ASSERT_EQUAL(200, RelayOutputTest->get_importThreshold());
}

void test_get_minON(void) {
    TEST_ASSERT_EQUAL(5 * 60, RelayOutputTest->get_minON());
}

void test_get_minOFF(void) {
    TEST_ASSERT_EQUAL(5 * 60, RelayOutputTest->get_minOFF());
}

void test_isRelayON(void) {
    TEST_ASSERT_EQUAL(false, RelayOutputTest->isRelayON());
}

void test_proceed_relay(void) {
    TEST_ASSERT_EQUAL(false, RelayOutputTest->proceed_relay(150));
    TEST_ASSERT_EQUAL(false, RelayOutputTest->isRelayON());

    for(uint16_t _time = 0; _time < RelayOutputTest->get_minOFF(); ++_time) {
        RelayOutputTest->inc_duration();
    }

    TEST_ASSERT_EQUAL(true, RelayOutputTest->proceed_relay(-1500));
    TEST_ASSERT_EQUAL(true, RelayOutputTest->isRelayON());

    for(uint16_t _time = 0; _time < RelayOutputTest->get_minON(); ++_time) {
        RelayOutputTest->inc_duration();
    }

    TEST_ASSERT_EQUAL(true, RelayOutputTest->proceed_relay(250));
    TEST_ASSERT_EQUAL(false, RelayOutputTest->isRelayON());
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_get_pin);
    RUN_TEST(test_get_surplusThreshold);
    RUN_TEST(test_get_importThreshold);
    RUN_TEST(test_get_minON);
    RUN_TEST(test_get_minOFF);
    RUN_TEST(test_isRelayON);
    RUN_TEST(test_proceed_relay);
    UNITY_END();
}

void loop() {
    // Do nothing here
}