/**
 * @file receiver.cpp
 * @brief Implementation of Remote Load Receiver functions
 * @version 2.0
 * @date 2026-10-01
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025-2026
 */

#include <avr/wdt.h>

#include "config.h"
#include "utils_pins.h"  // Fast direct port manipulation

// Global state variables
RfStatus rfStatus{ RfStatus::LOST };
unsigned long lastMessageTime{ 0 };
unsigned long lastRedLedToggle{ 0 };
unsigned long lastGreenLedToggle{ 0 };
uint8_t previousLoadBitmask{ 0xFF };  // Initialize to invalid value to force first print
RemoteLoadPayload receivedData;

// RFM69 radio instance (SS=D10, IRQ=D2, isRFM69HW)
#include <Arduino.h>
#include <RFM69.h>
#include "config.h"

/**
 * @brief Stops the watchdog first thing at boot
 * @details After a watchdog reset, the watchdog stays enabled with its shortest timeout (15 ms):
 *          without a bootloader that clears it (Optiboot does), the board would reset forever.
 */
void disableWatchdogAtBoot() __attribute__((naked, used, section(".init3")));
void disableWatchdogAtBoot()
{
  MCUSR = 0;
  wdt_disable();
}

// RFM69 radio instance
RFM69 radio(RFConfig::RF_CS_PIN, RFConfig::RF_IRQ_PIN, RFConfig::IS_RFM69HW);

void initializeReceiver()
{
  // Build bitmask of all load pins for fast initialization
  uint16_t loadPinMask{ 0 };
  uint8_t i{ NO_OF_LOADS };
  do
  {
    --i;
    loadPinMask |= bit(loadPins[i]);
  } while (i);

  // Configure load pins as outputs and set to OFF (fast direct port manipulation)
  setPinsAsOutput(loadPinMask);
  setPinsOFF(loadPinMask);

  // Configure status LEDs if present
  if constexpr (STATUS_LEDS_PRESENT)
  {
    constexpr uint16_t ledPinMask = bit(GREEN_LED_PIN) | bit(RED_LED_PIN);
    setPinsAsOutput(ledPinMask);
    setPinsOFF(ledPinMask);
  }

  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("======================================="));
  Serial.println(F("Remote Load Receiver v2.0 (RFM69)"));
  Serial.println(F("Based on remoteUnit_fasterControl_1"));
  Serial.println(F("======================================="));
  Serial.print(F("Listening to Router ID: "));
  Serial.println(RFConfig::ROUTER_NODE_ID);
  Serial.print(F("My Node ID: "));
  Serial.println(RFConfig::REMOTE_NODE_ID);
  Serial.print(F("Network ID: "));
  Serial.println(RFConfig::NETWORK_ID);
  Serial.print(F("Number of loads: "));
  Serial.println(NO_OF_LOADS);
  Serial.println(F("---------------------------------------"));

  // Resets the board, loads OFF, if loop() stops running
  wdt_enable(WDTO_1S);

  // Initialize RF module
  if (!radio.initialize(RFConfig::FREQUENCY, RFConfig::REMOTE_NODE_ID, RFConfig::NETWORK_ID))
  {
    Serial.println(F("RFM69 initialization FAILED! Retrying after a reset..."));
    while (true)
    {
      // wait for the watchdog reset
    }
  }

  // Optional: set high power mode for RFM69HW
  if (IS_RFM69HW)
  {
    radio.setHighPower();
  }

  // Optional: enable encryption (must match transmitter)
  // radio.encrypt("sampleEncryptKey");

  Serial.println(F("RF module initialized"));
  Serial.println(F("Waiting for commands..."));
  Serial.println();

  lastMessageTime = millis();
}

void updateLoads(uint8_t bitmask)
{
  uint8_t i{ NO_OF_LOADS };
  do
  {
    --i;
    setPinState(loadPins[i], bitmask & (1 << i));
  } while (i);
}

void updateStatusLED()
{
  if constexpr (!STATUS_LEDS_PRESENT)
  {
    return;
  }

  // Toggled from loop(), not from a timer ISR: it stops blinking if the loop hangs
  if ((millis() - lastGreenLedToggle) >= GREEN_LED_INTERVAL_MS)
  {
    togglePin(GREEN_LED_PIN);
    lastGreenLedToggle += GREEN_LED_INTERVAL_MS;
  }

  if (rfStatus != RfStatus::LOST)
  {
    setPinOFF(RED_LED_PIN);
    return;
  }

  if ((millis() - lastRedLedToggle) <= RED_LED_INTERVAL_MS)
  {
    return;
  }

  togglePin(RED_LED_PIN);
  lastRedLedToggle = millis();
}

void processRfMessages()
{
  // Check for incoming RF data
  if (!radio.receiveDone())
  {
    return;
  }

  // Only process well-formed messages from the expected transmitter
  if (radio.SENDERID != RFConfig::ROUTER_NODE_ID || radio.DATALEN != sizeof(RemoteLoadPayload))
  {
    return;
  }

  // Copy received data (single byte, direct assignment is faster than memcpy)
  receivedData.loadBitmask = radio.DATA[0];

  // Note: ACK not used - transmitter sends with requestACK=false for faster, non-blocking operation

  // Update loads based on received bitmask
  updateLoads(receivedData.loadBitmask);

  // Update RF status
  lastMessageTime = millis();

  if (rfStatus != RfStatus::OK)
  {
    rfStatus = RfStatus::OK;
    Serial.println(F("RF link restored"));
  }

  // Debug output - only print if data has changed
  // if (receivedData.loadBitmask != previousLoadBitmask)
  // {
  //   Serial.print(F("Received: 0b"));
  //   Serial.print(receivedData.loadBitmask, BIN);
  //   Serial.print(F(" (RSSI: "));
  //   Serial.print(radio.RSSI);
  //   Serial.print(F(") - Loads: "));
  //   for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
  //   {
  //     Serial.print(i);
  //     Serial.print(F(":"));
  //     Serial.print((receivedData.loadBitmask & (1 << i)) ? F("ON ") : F("OFF "));
  //   }
  //   Serial.println();

  //   previousLoadBitmask = receivedData.loadBitmask;
  // }
}

void checkRfTimeout()
{
  // Check for RF timeout
  if ((millis() - lastMessageTime) <= RF_TIMEOUT_MS)
  {
    return;
  }

  if (rfStatus == RfStatus::LOST)
  {
    return;
  }

  rfStatus = RfStatus::LOST;
  Serial.println(F("RF link LOST - turning all loads OFF"));

  // Safety: Turn all loads OFF when RF link is lost
  updateLoads(0);

  // Reset previous bitmask so next valid message will be printed
  previousLoadBitmask = 0xFF;
}

/**
 * @brief Called once during startup.
 */
void setup()
{
  initializeReceiver();
}

/**
 * @brief Main processor loop.
 */
void loop()
{
  wdt_reset();
  processRfMessages();
  checkRfTimeout();
  updateStatusLED();
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
