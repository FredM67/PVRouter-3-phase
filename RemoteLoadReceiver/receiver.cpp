/**
 * @file receiver.cpp
 * @brief Implementation of Remote Load Receiver functions
 * @version 2.0
 * @date 2026-01-30
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 *
 * @copyright Copyright (c) 2025-2026
 */

#include "config.h"
#include "utils_pins.h"  // Fast direct port manipulation

// Global state variables
RfStatus rfStatus{ RfStatus::LOST };
unsigned long lastMessageTime{ 0 };
unsigned long lastRedLedToggle{ 0 };
uint8_t previousLoadBitmask{ 0xFF };  // Initialize to invalid value to force first print
RemoteLoadPayload receivedData;

// RFM69 radio instance (SS=D10, IRQ=D2, isRFM69HW)
#include <Arduino.h>
#include <RFM69.h>
#include "config.h"

// RFM69 radio instance
RFM69 radio(RFConfig::RF_CS_PIN, RFConfig::RF_IRQ_PIN, RFConfig::IS_RFM69HW);

/**
 * @brief Timer1 Compare Match ISR for watchdog LED
 * @details Toggles green LED at 1Hz (every 1 second)
 *          Using Timer1 in CTC mode with prescaler 1024
 *          OCR1A = 15624 for 1 second interval @ 16MHz
 */
ISR(TIMER1_COMPA_vect)
{
  if constexpr (STATUS_LEDS_PRESENT)
  {
    togglePin(GREEN_LED_PIN);
  }
}

/**
 * @brief Initialize Timer1 for watchdog LED toggle
 * @details CTC mode, prescaler 1024, 1 second interval
 */
void initializeWatchdogTimer()
{
  // Timer1 CTC mode, prescaler 1024
  // For 1 second @ 16MHz: 16000000 / 1024 = 15625 ticks per second
  // OCR1A = 15625 - 1 = 15624
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);  // CTC mode, prescaler 1024
  OCR1A = 15624;                                      // 1 second interval
  TIMSK1 = (1 << OCIE1A);                             // Enable compare match interrupt
}

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

  // Initialize Timer1 for watchdog LED toggle (1Hz)
  initializeWatchdogTimer();

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

void updateLoads(uint8_t bitmask)
{
  // Build pin masks from load bitmask (same approach as main program)
  uint16_t pinsON{ 0 };
  uint16_t pinsOFF{ 0 };

  uint8_t i{ NO_OF_LOADS };
  do
  {
    --i;
    if (bitmask & (1 << i))
    {
      pinsON |= bit(loadPins[i]);
    }
    else
    {
      pinsOFF |= bit(loadPins[i]);
    }
  } while (i);

  // Single port write for all pins - fastest possible update
  setPinsOFF(pinsOFF);
  setPinsON(pinsON);
}

void updateStatusLED()
{
  if constexpr (!STATUS_LEDS_PRESENT)
  {
    return;
  }

  // Green LED is handled by Timer1 ISR

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

  // Only process messages from the expected transmitter
  if (radio.SENDERID != RFConfig::ROUTER_NODE_ID)
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

  // Safety: Turn all loads OFF when RF link is lost (fast direct port manipulation)
  uint16_t pinsOFF{ 0 };
  uint8_t i{ NO_OF_LOADS };
  do
  {
    --i;
    pinsOFF |= bit(loadPins[i]);
  } while (i);
  setPinsOFF(pinsOFF);

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
