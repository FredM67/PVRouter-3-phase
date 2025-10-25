/**
 * @file RemoteLoadReceiver.ino
 * @brief Receiver sketch for remotely controlled loads via RF using RFM69
 * @version 2.0
 * @date 2025-10-25
 * @author Based on remoteUnit_fasterControl_1 by Robin Emley
 * 
 * @details This sketch receives RF commands from the main PV Router and controls
 *          local loads (TRIACs or SSRs) accordingly. Uses RFM69 library (maintained)
 *          instead of deprecated JeeLib.
 * 
 *          Supports up to 8 loads via a bitmask protocol that minimizes data transmission.
 * 
 *          Features:
 *          - Receives compact bitmask commands (1 byte for up to 8 loads)
 *          - Detects RF link failures (no messages for > 500ms)
 *          - Optional LED indicators for RF status and load states
 *          - Watchdog to detect system lockups
 * 
 *          RF Status Indication:
 *          - Green LED: RF link OK
 *          - Red LED: RF link failed/lost
 * 
 * Hardware:
 * - Arduino UNO or compatible
 * - RFM69W/CW or RFM69HW/HCW RF module
 * - TRIAC/SSR outputs for loads
 * - Optional: Status LEDs
 * 
 *      www.Mk2PVrouter.co.uk
 */

#include <Arduino.h>
#include <RFM69.h>
#include <SPI.h>

// RF Configuration - must match transmitter
#define FREQUENCY RF69_433MHZ  // RF69_433MHZ, RF69_868MHZ, or RF69_915MHZ
#define IS_RFM69HW false       // true for RFM69HW/HCW, false for RFM69W/CW

const uint8_t TX_NODE_ID = 10;      // Node ID of transmitter
const uint8_t MY_NODE_ID = 15;      // This receiver's node ID
const uint8_t NETWORK_ID = 100;     // Network ID (1-255, must match transmitter)

// Load Configuration
const uint8_t NO_OF_LOADS = 2;  // Number of loads controlled by this unit
const uint8_t loadPins[NO_OF_LOADS] = { 5, 6 };  // Output pins for loads (active HIGH)

// Status LED Configuration (optional)
const uint8_t STATUS_LED_PIN = 4;  // Combined status LED (or split to two separate LEDs)
const bool STATUS_LED_PRESENT = true;

// Timing Configuration
const unsigned long RF_TIMEOUT_MS = 500;  // Lost RF link after this many milliseconds
const unsigned long WATCHDOG_INTERVAL_MS = 1000;  // Toggle watchdog this often

// Data structure for received commands (must match transmitter)
struct RemoteLoadPayload
{
  uint8_t loadBitmask;  // Bit 0 = Load 0, Bit 1 = Load 1, etc.
};

RemoteLoadPayload receivedData;

// RFM69 radio instance (SS=D10, IRQ=D2, isRFM69HW)
RFM69 radio(10, 2, IS_RFM69HW);

// State tracking
enum RfStatus
{
  RF_OK,
  RF_LOST
};

RfStatus rfStatus = RF_LOST;
unsigned long lastMessageTime = 0;
unsigned long lastWatchdogToggle = 0;
bool watchdogState = false;

/**
 * @brief Initialize hardware and RF module
 */
void setup()
{
  // Configure load pins as outputs and set to OFF
  for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
  {
    pinMode(loadPins[i], OUTPUT);
    digitalWrite(loadPins[i], LOW);  // Loads OFF initially
  }
  
  // Configure status LED if present
  if (STATUS_LED_PRESENT)
  {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);  // Start with LED off (RF lost)
  }
  
  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("======================================="));
  Serial.println(F("Remote Load Receiver v2.0 (RFM69)"));
  Serial.println(F("Based on remoteUnit_fasterControl_1"));
  Serial.println(F("======================================="));
  Serial.print(F("Listening to Node ID: "));
  Serial.println(TX_NODE_ID);
  Serial.print(F("My Node ID: "));
  Serial.println(MY_NODE_ID);
  Serial.print(F("Network ID: "));
  Serial.println(NETWORK_ID);
  Serial.print(F("Number of loads: "));
  Serial.println(NO_OF_LOADS);
  Serial.println(F("---------------------------------------"));
  
  // Initialize RF module
  if (!radio.initialize(FREQUENCY, MY_NODE_ID, NETWORK_ID))
  {
    Serial.println(F("RFM69 initialization FAILED!"));
    while (1); // Halt
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

/**
 * @brief Main loop - check for RF messages and update loads
 */
void loop()
{
  // Check for incoming RF data
  if (radio.receiveDone())
  {
    // Valid packet received - check if it's from the expected transmitter and correct size
    if (radio.SENDERID == TX_NODE_ID && radio.DATALEN == sizeof(receivedData))
    {
      // Copy received data
      memcpy(&receivedData, (void*)radio.DATA, sizeof(receivedData));
      
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
      
      // Debug output
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
    }
  }
  
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
    }
  }
  
  // Update status LED
  updateStatusLED();
  
  // Toggle watchdog
  if ((millis() - lastWatchdogToggle) > WATCHDOG_INTERVAL_MS)
  {
    watchdogState = !watchdogState;
    lastWatchdogToggle = millis();
  }
}

/**
 * @brief Update physical load states based on received bitmask
 * @param bitmask Bitmask of load states (bit N = load N)
 */
void updateLoads(uint8_t bitmask)
{
  for (uint8_t i = 0; i < NO_OF_LOADS; ++i)
  {
    bool loadOn = (bitmask & (1 << i)) != 0;
    digitalWrite(loadPins[i], loadOn ? HIGH : LOW);
  }
}

/**
 * @brief Update status LED based on RF link status
 * @details Can be expanded to support separate red/green LEDs
 */
void updateStatusLED()
{
  if (!STATUS_LED_PRESENT) return;
  
  if (rfStatus == RF_OK)
  {
    // RF OK - LED on steady (or green if using bi-color LED)
    digitalWrite(STATUS_LED_PIN, HIGH);
  }
  else
  {
    // RF lost - LED blinks (or red if using bi-color LED)
    digitalWrite(STATUS_LED_PIN, (millis() / 250) % 2);
  }
}

/**
 * @brief Get free RAM for diagnostics
 * @return Available RAM in bytes
 */
int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
