[![en](https://img.shields.io/badge/lang-en-red.svg)](Readme.en.md)
[![fr](https://img.shields.io/badge/lang-fr-blue.svg)](Readme.md)

This program is designed to be used with the Arduino IDE and/or other development IDEs like VSCode + PlatformIO.

- [Use with Visual Studio Code (recommended)](#use-with-visual-studio-code-recommended)
- [Use with Arduino IDE](#use-with-arduino-ide)
- [Quick overview of files](#quick-overview-of-files)
- [Development documentation](#development-documentation)
- [Router calibration](#router-calibration)
- [Analysis documentation and tools](#analysis-documentation-and-tools)
- [Program configuration](#program-configuration)
  - [Serial output type](#serial-output-type)
  - [TRIAC output configuration](#triac-output-configuration)
  - [On/off relay output configuration](#onoff-relay-output-configuration)
    - [Operating principle](#operating-principle)
  - [Watchdog configuration](#watchdog-configuration)
  - [Temperature sensor(s) configuration](#temperature-sensors-configuration)
    - [Feature activation](#feature-activation)
      - [With Arduino IDE](#with-arduino-ide)
      - [With Visual Studio Code and PlatformIO](#with-visual-studio-code-and-platformio)
    - [Sensor configuration (common to both cases above)](#sensor-configuration-common-to-both-cases-above)
  - [Off-peak hours management configuration (dual tariff)](#off-peak-hours-management-configuration-dual-tariff)
    - [Hardware configuration](#hardware-configuration)
    - [Software configuration](#software-configuration)
  - [Priority rotation](#priority-rotation)
  - [Forced operation configuration](#forced-operation-configuration)
  - [Routing stop](#routing-stop)
- [Advanced program configuration](#advanced-program-configuration)
  - [`DIVERSION_START_THRESHOLD_WATTS` parameter](#diversion_start_threshold_watts-parameter)
  - [`REQUIRED_EXPORT_IN_WATTS` parameter](#required_export_in_watts-parameter)
- [Configuration with ESP32 extension board](#configuration-with-esp32-extension-board)
  - [Pin mapping](#pin-mapping)
  - [`TEMP` bridge configuration](#temp-bridge-configuration)
  - [Recommended configuration](#recommended-configuration)
    - [Recommended basic configuration](#recommended-basic-configuration)
    - [Recommended additional features](#recommended-additional-features)
    - [Temperature probe installation](#temperature-probe-installation)
  - [Home Assistant integration](#home-assistant-integration)
- [Configuration without extension board](#configuration-without-extension-board)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

# Use with Visual Studio Code (recommended)

You'll need to install additional extensions. The most popular and used extensions for this job are '*Platform IO*' and '*Arduino*'.  
The entire project has been designed to be used optimally with *Platform IO*.

# Use with Arduino IDE

To use this program with the Arduino IDE, you must download and install the latest version of the Arduino IDE. Choose the "standard" version, NOT the version from the Microsoft Store. Opt for the "Win 10 and newer, 64 bits" or "MSI installer" version.

Since the code is optimized with one of the latest C++ standards, you need to edit a configuration file to activate C++17. You'll find the '**platform.txt**' file in the Arduino IDE installation path.

For **Windows**, you'll typically find the file in '**C:\Program Files (x86)\Arduino\hardware\arduino\avr**' and/or in '**%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\x.y.z**' where **'x.y.z**' is the version of the Arduino AVR Boards package.

You can also run this command in Powershell: `Get-Childitem –Path C:\ -Include platform.txt -Recurse -ErrorAction SilentlyContinue`. This may take a few seconds/minutes until the file is found.

For **Linux**, if using the AppImage package, you'll find this file in '~/.arduino15/packages/arduino/hardware/avr/1.8.6'. You can run `find / -name platform.txt 2>/dev/null` in case the location has changed.

For **MacOSX**, this file is located in '/Users/[user]/Library/Arduino15/packages/arduino/hardware/avr/1.8.6'.

Open the file in any text editor (you'll need administrator rights) and replace the parameter '**-std=gnu++11**' with '**-std=gnu++17**'. That's it!

If your Arduino IDE was open, please close all instances and reopen it.
___
> [!WARNING]
> When using the **ArduinoJson** library, you must install a version **6.x**.
> Version 7.x, although more recent, has become too heavy for an Atmega328P.
___

# Quick overview of files

- **Mk2_3phase_RFdatalog_temp.ino** : This file is needed for Arduino IDE
- **calibration.h** : contains the calibration parameters
- **config.h** : the user's preferences are stored here (pin assignments, features ...)
- **config_system.h** : rarely modified system constants
- **constants.h** : some constants — *do not edit*
- **debug.h** : Some macros for serial output and debugging
- **dualtariff.h** : dual tariff function definitions
- **ewma_avg.h** : EWMA averaging calculation functions
- **main.cpp** : main source code
- **movingAvg.h** : sliding window average source code
- **processing.cpp** : processing engine source code
- **processing.h** : processing engine function prototypes
- **Readme.en.md** : this file
- **teleinfo.h**: *IoT Telemetry* feature source code
- **types.h** : type definitions ...
- **type_traits.h** : some STL stuff not yet available in the avr package
- **type_traits** : contains missing STL templates
- **utils_dualtariff.h** : *Off-peak hours management* feature source code
- **utils_pins.h** : some functions for direct access to microcontroller inputs/outputs
- **utils_relay.h** : *relay diversion* feature source code
- **utils_rf.h** : *RF* function source code
- **utils_temp.h** : *Temperature* feature source code
- **utils.h** : helper functions and miscellaneous stuff
- **validation.h** : parameter validation, this code is executed at compile time only!
- **platformio.ini** : PlatformIO settings
- **inject_sketch_name.py** : helper script for PlatformIO
- **Doxyfile** : parameter for Doxygen (code documentation)

The end-user should ONLY edit the **calibration.h** and **config.h** files.

# Development documentation

You can start reading the documentation here [3-phase router](https://fredm67.github.io/Mk2PVRouter-3-phase/) (in English).

# Router calibration
Calibration values are found in the **calibration.h** file.
This is the line:
```cpp
inline constexpr float f_powerCal[NO_OF_PHASES]{ 0.05000F, 0.05000F, 0.05000F };
```

These default values must be determined to ensure optimal router operation.

# Analysis documentation and tools

📊 **[Analysis Tools and Technical Documentation](analysis/README.en.md)** [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](analysis/README.md)

This section contains advanced analysis tools and technical documentation for:

- **🔄 EWMA/TEMA Filtering** : Cloud immunity analysis and filter optimization
- **📈 Performance Analysis** : Visualization scripts and benchmarks
- **⚙️ Tuning Guide** : Documentation for parameter optimization
- **📊 Technical Charts** : Visual comparisons of filtering algorithms

> **Advanced users:** These tools will help you understand and optimize PV router behavior, especially for installations with solar production variability or battery systems.

# Program configuration

All preferences are found in the **config.h** file.

## Serial output type

The program can output different types of information on the serial link. It's recommended to start with the default choice ('SERIALOUT_NORMAL') and only change it if the need arises.

```cpp
#define SERIALOUT_NORMAL
//#define SERIALOUT_DIVERSION
//#define SERIALOUT_SUMONLY
//#define SERIALOUT_NONE
```

To change the output type, comment (by adding '//' at the beginning of the line) the current line and uncomment the desired line (by removing '//' at the beginning of the line).

## TRIAC output configuration

By default, the program is configured to work with 3-phase loads using triacs. This is the most common usage.

```cpp
constexpr load_t loads[NO_OF_DUMPLOADS]{
  { /*capacity*/ 3000, /*load*/ outputPins.physicalLoadPin(1) },
  { /*capacity*/ 2000, /*load*/ outputPins.physicalLoadPin(2) },
  { /*capacity*/ 1000, /*load*/ outputPins.physicalLoadPin(3) } };
```

## On/off relay output configuration

The program can also work with on/off relays. For this configuration, you need to replace the `loads` variable definition with a `relaysConfig` definition:

```cpp
constexpr RelayEngine relaysConfig{
  // Relay delay in 100ms units:
  12,  // 1.2s delay
  // Relay configurations:
  Relay{ 1000, outputPins.relayPin(1) },  // water heater 1000W
  Relay{ 2000, outputPins.relayPin(2) },  // secondary load 2000W
  Relay{ 3000, outputPins.relayPin(3) }   // additional load 3000W
};
```

The first parameter is the delay between each relay activation/deactivation. This delay is expressed in units of 100 milliseconds (12 = 1.2 seconds).

Then, for each relay, you specify its power and the pin it's connected to.

> **Battery installations:** For optimal relay configuration with battery systems, consult the **[Battery System Configuration Guide](BATTERY_CONFIGURATION_GUIDE.en.md)** [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](BATTERY_CONFIGURATION_GUIDE.md)

### Operating principle

With this system, the router will only start relays when the available excess power reaches exactly the relay power. The router tries to reach the relay power by accumulating the excess power. Once the relay is started, the power consumed by the relay is subtracted from the calculations.

For example, with the configuration above:
- If the available power is between 1000W and 2999W → only the first relay (1000W) will be started
- If the available power is between 3000W and 4999W → the first relay (1000W) and the second relay (2000W) will be started
- If the available power is 6000W or more → all three relays will be started

To minimize relay wear due to frequent switching, the relay system integrates available energy. A relay is only started when the accumulated energy is sufficient to run the relay for a defined duration.

The delay is an additional security measure to prevent relays from switching too quickly when the available power is close to the relay power threshold.

This system optimizes energy usage by starting the most appropriate relays according to available excess power.

## Watchdog configuration

By default, the watchdog is enabled and configured for 8 seconds. If the program crashes or gets stuck, the system will automatically restart after 8 seconds.

```cpp
#define ENABLE_WATCHDOG ///< Enable the internal watchdog
```

To disable the watchdog, comment this line:

```cpp
//#define ENABLE_WATCHDOG ///< Enable the internal watchdog
```

> [!WARNING]
> Disabling the watchdog may cause the router to remain blocked indefinitely in case of a software problem. It's recommended to keep it enabled unless you have specific reasons to disable it.

## Temperature sensor(s) configuration

The router can monitor temperature and stop routing to protect against overheating.

The functionality is disabled by default and must be explicitly enabled.

### Feature activation

#### With Arduino IDE

To activate the temperature functionality, uncomment the corresponding line in the **config.h** file:

```cpp
#define TEMP_ENABLED
```

#### With Visual Studio Code and PlatformIO

With Visual Studio Code and PlatformIO, you also need to add the necessary library in the **platformio.ini** file in the `lib_deps` section.

### Sensor configuration (common to both cases above)

Once the functionality is activated, you need to configure the sensor(s) in the **config.h** file:

```cpp
inline constexpr Temp_sensor temp_sensors[MAX_SENSORS]{
  { "Dissipator", DS18B20, 0x28, 0x0D, 0x01, 0x75, 0x91, 0x0D, 0x02, 0xC4, 50.0 }  // ,
  //  { "Outdoor", DS18B20, 0x28, 0x0D, 0x01, 0x75, 0x91, 0x0D, 0x02, 0xC4, 60.0 }
};
```

For each sensor, you need to specify:
- A name (in quotes)
- The sensor type (DS18B20)
- The unique address of the sensor (8 bytes in hexadecimal)
- The trigger temperature in degrees Celsius

To find the unique address of your sensor(s), you can use a simple sketch to scan and display available sensors.

## Off-peak hours management configuration (dual tariff)

The router can be configured to work with a dual tariff system, prioritizing certain loads during off-peak hours.

This feature allows modifying load priorities or activating specific loads only during off-peak periods.

### Hardware configuration

You need to connect a signal from your meter (or a contact indicating off-peak hours) to one of the digital pins of the Arduino.

### Software configuration

In the **config.h** file, enable the functionality and configure the pin:

```cpp
#define DUAL_TARIFF ///< Enable dual tariff functionality

inline constexpr uint8_t dualTariffPin{ 2 }; ///< Arduino pin for dual tariff signal
```

Then you can define specific behaviors for each tariff period in your load or relay configuration.

## Priority rotation

The router can automatically rotate load priorities to balance wear on different devices.

To activate this feature:

```cpp
#define PRIORITY_ROTATION ///< Enable load priority rotation
inline constexpr uint8_t PRIORITY_ROTATION_DURATION{ 1 }; ///< Duration in hours
```

## Forced operation configuration

The router can be configured to allow manual forcing of certain loads.

```cpp
#define FORCE_PIN_DUMP_LOAD ///< Enable manual forcing of loads
inline constexpr uint8_t forcePin{ 2 }; ///< Arduino pin for forced operation
```

When the pin is activated (connected to ground), the first load will be forced on regardless of available power.

## Routing stop

You can configure a pin to completely stop routing:

```cpp
#define STOP_PIN ///< Enable routing stop functionality
inline constexpr uint8_t stopPin{ 3 }; ///< Arduino pin to stop routing
```

When this pin is activated, the router will stop all diversion until the pin is deactivated.

# Advanced program configuration

## `DIVERSION_START_THRESHOLD_WATTS` parameter

This parameter defines the minimum power threshold before the router starts diverting energy.

```cpp
inline constexpr int16_t DIVERSION_START_THRESHOLD_WATTS{ 20 };
```

## `REQUIRED_EXPORT_IN_WATTS` parameter

This parameter defines the minimum export power that the router tries to maintain toward the grid.

```cpp
inline constexpr auto REQUIRED_EXPORT_IN_WATTS{ 0 };
```

A positive value means the router will try to maintain an export to the grid.
A negative value means the router will accept some import from the grid before stopping diversion.

# Configuration with ESP32 extension board

The router can be used with an ESP32 extension board for enhanced connectivity features.

## Pin mapping

The pin correspondence between Arduino and ESP32 is defined in the **config.h** file:

```cpp
inline constexpr OutputPins outputPins{
  // Arduino pins
  .triac_1 = 5,
  .triac_2 = 6,
  .triac_3 = 3,
  .relay_1 = 7,
  .relay_2 = 8,
  .relay_3 = 9,
  // ESP32 pins (when used)
  .esp32_relay_1 = 16,
  .esp32_relay_2 = 17,
  .esp32_relay_3 = 18
};
```

## `TEMP` bridge configuration

On the ESP32 board, you'll find a bridge marked 'TEMP'. This bridge must be configured according to your needs:
- **Open** (default): DS18B20 temperature sensors are powered by the ESP32
- **Closed**: DS18B20 temperature sensors are powered by the Arduino

## Recommended configuration

### Recommended basic configuration

For optimal use with the ESP32 extension board:

```cpp
#define ESP32_PORTAL         ///< Enable ESP32 web portal
#define ENABLE_DEBUG         ///< Enable debugging for development
```

### Recommended additional features

```cpp
#define TEMP_ENABLED         ///< Enable temperature monitoring
#define DUAL_TARIFF          ///< Enable dual tariff if needed
```

### Temperature probe installation

When using DS18B20 probes with the ESP32 board:
1. Keep the 'TEMP' bridge **open** (default position)
2. Connect the probes to the dedicated connector on the ESP32 board
3. The probes will be powered by the ESP32's 3.3V

## Home Assistant integration

The ESP32 extension enables integration with Home Assistant for monitoring and control.

Detailed configuration will be available in dedicated documentation.

# Configuration without extension board

The router can operate without the ESP32 extension board, using only the Arduino's capabilities.

In this case, all relay controls are done directly by the Arduino's digital pins.

# Troubleshooting

Common problems and their solutions:

1. **The router doesn't divert**: Check calibration values in **calibration.h**
2. **Erratic operation**: Verify power supply stability and grounding
3. **Relays don't activate**: Check relay configuration and pin assignments
4. **Temperature sensors not detected**: Verify wiring and sensor addresses

For more detailed troubleshooting, consult the [online documentation](https://fredm67.github.io/Mk2PVRouter-3-phase/).

# Contributing

Contributions are welcome! Please read the [CONTRIBUTING.md](../CONTRIBUTING.md) file for guidelines on how to contribute to this project.

For questions or discussions, you can:
- Open an issue on GitHub
- Participate in discussions on the project forum
- Consult the [development documentation](https://fredm67.github.io/Mk2PVRouter-3-phase/)
