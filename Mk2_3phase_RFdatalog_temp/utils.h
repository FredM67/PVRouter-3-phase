/**
 * @file utils.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions
 * @version 0.1
 * @date 2021-10-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "config.h"
#include "processing.h"
#include "constants.h"
#include "dualtariff.h"

inline void togglePin(const uint8_t pin) __attribute__((always_inline));

inline void setPinON(const uint8_t pin) __attribute__((always_inline));
inline void setPinsON(const uint16_t pins) __attribute__((always_inline));

inline void setPinOFF(const uint8_t pin) __attribute__((always_inline));
inline void setPinsOFF(const uint16_t pins) __attribute__((always_inline));

/**
 * @brief Toggle the specified pin
 * 
 */
void togglePin(const uint8_t pin)
{
    if (pin < 8)
        PIND |= bit(pin);
    else
        PINB |= bit(pin - 8);
}

/**
 * @brief Set the Pin state to ON for the specified pin
 * 
 * @param pin pin to change [2..13]
 */
void setPinON(const uint8_t pin)
{
    if (pin < 8)
        PORTD |= bit(pin);
    else
        PORTB |= bit(pin - 8);
}

/**
 * @brief Set the Pins state to ON
 * 
 * @param pins 
 */
void setPinsON(const uint16_t pins)
{
    PORTD |= lowByte(pins);
    PORTB |= highByte(pins);
}

/**
 * @brief Set the Pin state to OFF for the specified pin
 * 
 * @param pin pin to change [2..13]
 */
void setPinOFF(const uint8_t pin)
{
    if (pin < 8)
        PORTD &= ~bit(pin);
    else
        PORTB &= ~bit(pin - 8);
}

/**
 * @brief Set the Pins state to OFF
 * 
 * @param pins 
 */
void setPinsOFF(const uint16_t pins)
{
    PORTD &= ~lowByte(pins);
    PORTB &= ~highByte(pins);
}

/**
 * @brief Print the configuration during start
 * 
 */
inline void printConfiguration()
{
    Serial.println();
    Serial.println();
    Serial.println(F("----------------------------------"));
    Serial.print(F("Sketch ID: "));
    Serial.println(__FILE__);
    Serial.print(F("Build on "));
    Serial.print(__DATE__);
    Serial.print(F(" "));
    Serial.println(__TIME__);

    Serial.println(F("ADC mode:       free-running"));

    Serial.println(F("Electrical settings"));
    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        Serial.print(F("\tf_powerCal for L"));
        Serial.print(phase + 1);
        Serial.print(F(" =    "));
        Serial.println(f_powerCal[phase], 5);

        Serial.print(F("\tf_voltageCal, for Vrms_L"));
        Serial.print(phase + 1);
        Serial.print(F(" =    "));
        Serial.println(f_voltageCal[phase], 5);
    }

    Serial.print(F("\tf_phaseCal for all phases =     "));
    Serial.println(f_phaseCal);

    Serial.print(F("\tExport rate (Watts) = "));
    Serial.println(REQUIRED_EXPORT_IN_WATTS);

    Serial.print(F("\tzero-crossing persistence (sample sets) = "));
    Serial.println(PERSISTENCE_FOR_POLARITY_CHANGE);

    printParamsForSelectedOutputMode();

    Serial.print("Temperature capability ");
#ifdef TEMP_SENSOR
    Serial.println(F("is present"));
#else
    Serial.println(F("is NOT present"));
#endif

    Serial.print("Dual-tariff capability ");
    if constexpr (DUAL_TARIFF)
    {
        Serial.println(F("is present"));
        printDualTariffConfiguration();
    }
    else
        Serial.println(F("is NOT present"));

    Serial.print("Load rotation feature ");
    if constexpr (PRIORITY_ROTATION)
        Serial.println(F("is present"));
    else
        Serial.println(F("is NOT present"));

    Serial.print("RF capability ");
#ifdef RF_PRESENT
    Serial.print(F("IS present, Freq = "));
    if (FREQ == RF12_433MHZ)
        Serial.println(F("433 MHz"));
    else if (FREQ == RF12_868MHZ)
        Serial.println(F("868 MHz"));
    rf12_initialize(nodeID, FREQ, networkGroup); // initialize RF
#else
    Serial.println(F("is NOT present"));
#endif

    Serial.print("Datalogging capability ");
#ifdef SERIALPRINT
    Serial.println(F("is present"));
#else
    Serial.println(F("is NOT present"));
#endif
}

/**
 * @brief Prints data logs to the Serial output in text or json format
 * 
 * @param bOffPeak true if off-peak tariff is active
 */
inline void sendResults(bool bOffPeak)
{
    uint8_t phase;

#ifdef RF_PRESENT
    send_rf_data(); // *SEND RF DATA*
#endif

#if defined SERIALOUT && !defined EMONESP
    Serial.print(copyOf_energyInBucket_main / SUPPLY_FREQUENCY);
    Serial.print(F(", P:"));
    Serial.print(tx_data.power);

    for (phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        Serial.print(F(", P"));
        Serial.print(phase + 1);
        Serial.print(F(":"));
        Serial.print(tx_data.power_L[phase]);
    }
    for (phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        Serial.print(F(", V"));
        Serial.print(phase + 1);
        Serial.print(F(":"));
        Serial.print((float)tx_data.Vrms_L_x100[phase] / 100);
    }

#ifdef TEMP_SENSOR
    Serial.print(", temperature ");
    Serial.print((float)tx_data.temperature_x100 / 100);
#endif
    Serial.println(F(")"));
#endif // if defined SERIALOUT && !defined EMONESP

#if defined EMONESP && !defined SERIALOUT
    StaticJsonDocument<200> doc;
    char strPhase[]{"L0"};
    char strLoad[]{"LOAD_0"};

    for (phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        doc[strPhase] = tx_data.power_L[phase];
        ++strPhase[1];
    }

    for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i)
    {
        doc[strLoad] = (100 * copyOf_countLoadON[i]) / DATALOG_PERIOD_IN_MAINS_CYCLES;
        ++strLoad[5];
    }

#ifdef DUAL_TARIFF
    doc["DUAL_TARIFF"] = bOffPeak ? true : false;
#endif

    // Generate the minified JSON and send it to the Serial port.
    //
    serializeJson(doc, Serial);

    // Start a new line
    Serial.println();
    delay(50);
#endif // if defined EMONESP && !defined SERIALOUT

#if defined SERIALPRINT && !defined EMONESP
    Serial.print(copyOf_energyInBucket_main / SUPPLY_FREQUENCY);
    Serial.print(F(", P:"));
    Serial.print(tx_data.power);

    for (phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        Serial.print(F(", P"));
        Serial.print(phase + 1);
        Serial.print(F(":"));
        Serial.print(tx_data.power_L[phase]);
    }
    for (phase = 0; phase < NO_OF_PHASES; ++phase)
    {
        Serial.print(F(", V"));
        Serial.print(phase + 1);
        Serial.print(F(":"));
        Serial.print((float)tx_data.Vrms_L_x100[phase] / 100);
    }

#ifdef TEMP_SENSOR
    Serial.print(", temperature ");
    Serial.print((float)tx_data.temperature_x100 / 100);
#endif // TEMP_SENSOR
    Serial.print(F(", (minSampleSets/MC "));
    Serial.print(copyOf_lowestNoOfSampleSetsPerMainsCycle);
    Serial.print(F(", #ofSampleSets "));
    Serial.print(copyOf_sampleSetsDuringThisDatalogPeriod);
#ifndef DUAL_TARIFF
#ifdef PRIORITY_ROTATION
    Serial.print(F(", NoED "));
    Serial.print(absenceOfDivertedEnergyCount);
#endif // PRIORITY_ROTATION
#endif // DUAL_TARIFF
    Serial.println(F(")"));
#endif // if defined SERIALPRINT && !defined EMONESP
}

/**
 * @brief Prints the load priorities to the Serial output.
 * 
 */
inline void logLoadPriorities()
{
#ifdef DEBUGGING
    Serial.println(F("Load Priorities: "));
    for (const auto loadPrioAndState : loadPrioritiesAndState)
    {
        Serial.print(F("\tload "));
        Serial.println(loadPrioAndState);
    }
#endif
}

#ifdef TEMP_SENSOR_PRESENT

#include <OneWire.h>                   // for temperature sensing
inline OneWire oneWire(tempSensorPin); /**< For temperature sensing */

/**
 * @brief Convert the internal value read from the sensor to a value in °C.
 * 
 */
inline void convertTemperature()
{
    oneWire.reset();
    oneWire.write(SKIP_ROM);
    oneWire.write(CONVERT_TEMPERATURE);
}

/**
 * @brief Read the temperature.
 * 
 * @return The temperature in °C (x100). 
 */
inline int16_t readTemperature()
{
    uint8_t buf[9];

    if (oneWire.reset())
    {
        oneWire.reset();
        oneWire.write(SKIP_ROM);
        oneWire.write(READ_SCRATCHPAD);
        for (auto &buf_elem : buf)
            buf_elem = oneWire.read();

        if (oneWire.crc8(buf, 8) != buf[8])
            return BAD_TEMPERATURE;

        // result is temperature x16, multiply by 6.25 to convert to temperature x100
        int16_t result = (buf[1] << 8) | buf[0];
        result = (result * 6) + (result >> 2);
        if (result <= TEMP_RANGE_LOW || result >= TEMP_RANGE_HIGH)
            return OUTOFRANGE_TEMPERATURE; // return value ('Out of range')

        return result;
    }
    return BAD_TEMPERATURE;
}
#else
void convertTemperature();
int16_t readTemperature();
#endif // #ifdef TEMP_SENSOR_PRESENT

#ifdef RF_PRESENT
/**
 * @brief Send the logging data through RF.
 * @details For better performance, the RFM12B needs to remain in its
 *          active state rather than being periodically put to sleep.
 * 
 */
inline void send_rf_data()
{
    // check whether it's ready to send, and an exit route if it gets stuck
    uint32_t i = 0;
    while (!rf12_canSend() && i < 10)
    {
        rf12_recvDone();
        ++i;
    }
    rf12_sendNow(0, &tx_data, sizeof tx_data);
}
#endif // #ifdef RF_PRESENT

/**
 * @brief Get the available RAM during setup
 * 
 * @return int The amount of free RAM
 */
inline int freeRam()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#endif // __UTILS_H__