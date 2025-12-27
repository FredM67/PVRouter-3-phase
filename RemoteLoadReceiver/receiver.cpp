/**
 * @file receiver.cpp
 * @brief Implementation of Remote Load Receiver functions
 * @version 2.0
 * @date 2025-10-26
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025
 */

#include "config.h"

// Global state variables
RfStatus rfStatus{ RF_LOST };
unsigned long lastMessageTime{ 0 };
unsigned long lastWatchdogToggle{ 0 };
bool watchdogState{ false };
uint8_t previousLoadBitmask{ 0xFF };  // Initialize to invalid value to force first print
RemoteLoadPayload receivedData;

// RFM69 radio instance (SS=D10, IRQ=D2, isRFM69HW)
#include <Arduino.h>
#include <RFM69.h>
#include "config.h"

// RFM69 radio instance
RFM69 radio(RFConfig::RF_CS_PIN, RFConfig::RF_IRQ_PIN, RFConfig::IS_RFM69HW);

void initializeReceiver()
{
  // Configure load pins as outputs and set to OFF
  for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
  {
    pinMode(loadPins[i], OUTPUT);
    digitalWrite(loadPins[i], LOW);  // Loads OFF initially
  }

  // Configure status LEDs if present
  if constexpr (STATUS_LEDS_PRESENT)
  {
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(GREEN_LED_PIN, LOW);
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN, LOW);
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

  // Initialize RF module
  if (!radio.initialize(RFConfig::FREQUENCY, RFConfig::REMOTE_NODE_ID, RFConfig::NETWORK_ID))
  {
    Serial.println(F("RFM69 initialization FAILED!"));
    while (1);  // Halt
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

void updateWatchdog()
{
  if ((millis() - lastWatchdogToggle) > WATCHDOG_INTERVAL_MS)
  {
    watchdogState = !watchdogState;
    lastWatchdogToggle = millis();
  }
}

void updateLoads(uint8_t bitmask)
{
  for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
  {
    bool loadOn = (bitmask & (1 << i)) != 0;
    digitalWrite(loadPins[i], loadOn ? HIGH : LOW);
  }
}

void updateStatusLED()
{
  if constexpr (!STATUS_LEDS_PRESENT)
  {
    return;
  }

  // Green LED: 1Hz watchdog blink (500ms on, 500ms off)
  digitalWrite(GREEN_LED_PIN, watchdogState ? HIGH : LOW);

  // Red LED: Fast blink when RF lost (~4Hz = 125ms period)
  if (rfStatus == RF_LOST)
  {
    digitalWrite(RED_LED_PIN, (millis() / 125) % 2);
  }
  else
  {
    digitalWrite(RED_LED_PIN, LOW);  // RF OK - red LED off
  }
}

void processRfMessages()
{
  // Check for incoming RF data
  if (radio.receiveDone())
  {
    // Only process messages from the expected transmitter
    if (radio.SENDERID == RFConfig::ROUTER_NODE_ID)
    {
      // Copy received data
      memcpy(&receivedData, (void *)radio.DATA, sizeof(receivedData));

      // Send ACK if requested
      if (radio.ACKRequested())
      {
        radio.sendACK();
      }

      // Update loads based on received bitmask
      updateLoads(receivedData.loadBitmask);

      // Update RF status
      lastMessageTime = millis();

      if (rfStatus != RF_OK)
      {
        rfStatus = RF_OK;
        Serial.println(F("RF link restored"));
      }

      // Debug output - only print if data has changed
      if (receivedData.loadBitmask != previousLoadBitmask)
      {
        Serial.print(F("Received: 0b"));
        Serial.print(receivedData.loadBitmask, BIN);
        Serial.print(F(" (RSSI: "));
        Serial.print(radio.RSSI);
        Serial.print(F(") - Loads: "));
        for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
        {
          Serial.print(i);
          Serial.print(F(":"));
          Serial.print((receivedData.loadBitmask & (1 << i)) ? F("ON ") : F("OFF "));
        }
        Serial.println();

        previousLoadBitmask = receivedData.loadBitmask;
      }
    }
  }
}

void checkRfTimeout()
{
  // Check for RF timeout
  if ((millis() - lastMessageTime) > RF_TIMEOUT_MS)
  {
    if (rfStatus != RF_LOST)
    {
      rfStatus = RF_LOST;
      Serial.println(F("RF link LOST - turning all loads OFF"));

      // Safety: Turn all loads OFF when RF link is lost
      for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
      {
        digitalWrite(loadPins[i], LOW);
      }

      // Reset previous bitmask so next valid message will be printed
      previousLoadBitmask = 0xFF;
    }
  }
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
  processRfMessages();
  checkRfTimeout();
  updateStatusLED();
  updateWatchdog();
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
