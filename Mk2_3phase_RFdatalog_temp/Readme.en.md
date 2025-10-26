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
  - [RF module and remote loads configuration](#rf-module-and-remote-loads-configuration)
    - [Required hardware](#required-hardware)
    - [Software configuration](#software-configuration-1)
    - [Remote receiver configuration](#remote-receiver-configuration)
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

📊 **[Analysis Tools and Technical Documentation](../analysis/README.en.md)** [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](../analysis/README.md)

This section contains advanced analysis tools and technical documentation for:

- **🔄 EWMA/TEMA Filtering** : Cloud immunity analysis and filter optimization
- **📈 Performance Analysis** : Visualization scripts and benchmarks
- **⚙️ Tuning Guide** : Documentation for parameter optimization
- **📊 Technical Charts** : Visual comparisons of filtering algorithms

> **Advanced users:** These tools will help you understand and optimize PV router behavior, especially for installations with solar production variability or battery systems.

# Program configuration

The configuration of a feature generally follows two steps:
- Feature activation
- Feature parameter configuration

Configuration consistency is checked during compilation. For example, if a *pin* is accidentally allocated twice, the compiler will generate an error.

## Serial output type

The serial output type can be configured to suit different needs. Three options are available:

- **HumanReadable** : Human-readable output, ideal for debugging or commissioning.
- **IoT** : Formatted output for IoT platforms like Home Assistant.
- **JSON** : Formatted output for platforms like EmonCMS (JSON).

To configure the serial output type, modify the following constant in the **config.h** file:
```cpp
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;
```
Replace `HumanReadable` with `IoT` or `JSON` according to your needs.

## TRIAC output configuration

The first step is to define the number of TRIAC outputs:

```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 2 };
```

Then, you need to assign the corresponding *pins* **for local loads only** as well as the priority order at startup.
```cpp
// Pins for LOCAL loads only (remote loads are controlled via RF)
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS]{ 5 };

// Optional: Status LEDs for remote loads
inline constexpr uint8_t remoteLoadStatusLED[NO_OF_REMOTE_LOADS]{ unused_pin, unused_pin };

// Priority order at startup (0 = highest priority, applies to ALL loads)
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 };
```

**Important:** 
- `physicalLoadPin` contains only the pins for **local** loads (TRIACs directly connected)
- **Remote** loads have no physical pin on the main controller (they are controlled via RF)
- `remoteLoadStatusLED` optionally allows adding status LEDs to visualize the state of remote loads
- `loadPrioritiesAtStartup` defines the priority order for **all** loads (local + remote). Priorities 0 to (number of local loads - 1) control local loads, subsequent priorities control remote loads.

## On/off relay output configuration
On/off relay outputs allow powering devices that contain electronics (heat pump ...).

You need to activate the feature like this:
```cpp
inline constexpr bool RELAY_DIVERSION{ true };
```

Each relay requires the definition of five parameters:
- the **pin** number to which the relay is connected
- the **surplus threshold** before startup (default **1000 W**)
- the **import threshold** before shutdown (default **200 W**)
- the **minimum operating duration** in minutes (default **5 min**)
- the **minimum shutdown duration** in minutes (default **5 min**).

Example relay configuration:
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 } } };
```
In this example, the relay is connected to *pin* **4**, it will trigger from **1000 W** surplus, stop from **200 W** import, and has a minimum operating and shutdown duration of **10 min**.

To configure multiple relays, simply list the configurations for each relay:
```cpp
inline constexpr RelayEngine relays{ { { 4, 1000, 200, 10, 10 },
                                       { 3, 1500, 250, 5, 15 } } };
```
Relays are activated in list order, and deactivated in reverse order.  
In all cases, minimum operating and shutdown durations are always respected.

### Operating principle
Surplus and import thresholds are calculated using an exponentially weighted moving average (EWMA), in our specific case, it's a modification of a triple exponentially weighted moving average (TEMA).  
By default, this average is calculated over a window of approximately **10 min**. You can adjust this duration to suit your needs.  
It's possible to lengthen it but also to shorten it.  
For Arduino performance reasons, the chosen duration will be rounded to a close duration that allows calculations without impacting router performance.

The time window duration is controlled by the `RELAY_FILTER_DELAY` parameter in the configuration file.

If the user prefers a 15 min window, simply write:
```cpp
inline constexpr RelayEngine relays{ MINUTES(15), { { 3, 1000, 200, 1, 1 } } };
```
___
> [!NOTE]
> The `MINUTES()` macro automatically converts the value to a template parameter. No special suffix is needed!
___

Relays configured in the system are managed by a system similar to a state machine.
Every second, the system increases the duration of the current state of each relay and proceeds with all relays based on the current average power:
- if the current average power is above the import threshold, it tries to turn off some relays.
- if the current average power is above the surplus threshold, it tries to turn on more relays.

Relays are processed in ascending order for surplus and in descending order for import.

For each relay, the transition or state change is managed as follows:
- if the relay is *OFF* and the current average power is below the surplus threshold, the relay tries to switch to *ON* state. This transition is subject to the condition that the relay has been *OFF* for at least the *minOFF* duration.
- if the relay is *ON* and the current average power is above the import threshold, the relay tries to switch to *OFF* state. This transition is subject to the condition that the relay has been *ON* for at least the *minON* duration.

> [!NOTE]
> **Battery installations:** For optimal relay configuration with battery systems, consult the **[Battery Configuration Guide](docs/BATTERY_CONFIGURATION_GUIDE.en.md)** [![fr](https://img.shields.io/badge/lang-fr-blue.svg)](docs/BATTERY_CONFIGURATION_GUIDE.md)

## RF module and remote loads configuration

The router can control remote loads via an RFM69 RF module. This feature allows controlling resistors or relays located in another location, without additional wiring.

### Required hardware

**For transmitter (main router):**
- RFM69W/CW or RFM69HW/HCW module (868 MHz for Europe, 915 MHz for North America)
- Appropriate antenna for chosen frequency
- Standard SPI connection (D10=CS, D2=IRQ)

**For remote receiver:**
- Arduino UNO or compatible
- RFM69 module (same model as transmitter)
- TRIAC or SSR to control loads
- Optional status LEDs (D5=green watchdog, D7=red RF loss)

### Software configuration

**Activating RF features:**

The RF module can be used for two independent features:

1. **RF Telemetry** (`ENABLE_RF_DATALOGGING`): Sends power/voltage data to a gateway
2. **Remote Loads** (`ENABLE_REMOTE_LOADS`): Load control via RF

To activate the RF module with remote load control, uncomment in **config.h**:

```cpp
#define RF_PRESENT                /**< Enable RFM69 module (required for any RF feature) */
#define ENABLE_REMOTE_LOADS       /**< Enable remote load control via RF */
```

**Load configuration:**

Define total number of loads (local + remote):

```cpp
inline constexpr uint8_t NO_OF_DUMPLOADS{ 3 };        // Total: 3 loads
inline constexpr uint8_t NO_OF_REMOTE_LOADS{ 2 };     // Including 2 remote loads
                                                       // Local loads: 3 - 2 = 1

// Pin for the local load (TRIAC)
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS - NO_OF_REMOTE_LOADS]{ 5 };

// Optional LEDs to indicate remote load status
inline constexpr uint8_t remoteLoadStatusLED[NO_OF_REMOTE_LOADS]{ 8, 9 };  // D8 and D9
```

**Priorities:**

Remote loads **always** have lower priority than local loads. In the example above:
- Local load #0 (physicalLoadPin[0]): highest priority
- Remote load #0: medium priority
- Remote load #1: lowest priority

**RF configuration (in utils_rf.h):**

Default parameters are:
- Frequency: 868 MHz (Europe)
- Network ID: 210
- Transmitter ID: 10
- Receiver ID: 15

To modify these parameters, edit **utils_rf.h**:

```cpp
inline constexpr uint8_t THIS_NODE_ID{ 10 };        // ID of this transmitter
inline constexpr uint8_t GATEWAY_ID{ 1 };           // Gateway ID (telemetry)
inline constexpr uint8_t REMOTE_LOAD_ID{ 15 };     // Load receiver ID
inline constexpr uint8_t NETWORK_ID{ 210 };        // Network ID (1-255)
```

### Remote receiver configuration

The **RemoteLoadReceiver** sketch is provided in the `RemoteLoadReceiver/` folder.

**Minimum configuration (in receiver's config.h):**

```cpp
// RF Configuration - must match transmitter
inline constexpr uint8_t TX_NODE_ID{ 10 };          // Transmitter ID
inline constexpr uint8_t MY_NODE_ID{ 15 };          // This receiver's ID  
inline constexpr uint8_t NETWORK_ID{ 210 };         // Network ID

// Load configuration
inline constexpr uint8_t NO_OF_LOADS{ 2 };                    // Number of loads on this receiver
inline constexpr uint8_t loadPins[NO_OF_LOADS]{ 4, 3 };       // TRIAC/SSR output pins

// Status LEDs (optional)
inline constexpr uint8_t GREEN_LED_PIN{ 5 };        // Green LED: 1 Hz watchdog
inline constexpr uint8_t RED_LED_PIN{ 7 };          // Red LED: RF link lost (fast blink)
inline constexpr bool STATUS_LEDS_PRESENT{ true };  // Enable LEDs
```

**Safety:**

The receiver automatically turns OFF **all loads** if no message is received for more than 500 ms. This ensures safety in case of RF link loss.

**Link testing:**

Once configured and uploaded, both Arduinos communicate automatically:
- Transmitter sends load states every ~100 ms (5 mains cycles at 50 Hz)
- Receiver displays received commands on serial port
- Green LED blinks at 1 Hz (system alive)
- Red LED blinks rapidly if RF link is lost

**Diagnostics:**

On the receiver's serial monitor, you should see:
```
Received: 0b01 (RSSI: -45) - Loads: 0:ON 1:OFF
```

An RSSI between -30 and -70 indicates good signal quality. Beyond -80, the link becomes unstable.

## Watchdog configuration
A watchdog is an electronic circuit or software used in digital electronics to ensure that an automaton or computer does not remain stuck at a particular stage of the processing it performs.

This is achieved using an LED that blinks at a frequency of 1 Hz, i.e., every second.  
Thus, the user knows whether their router is powered on, and if this LED stops blinking, it means the Arduino is stuck (a case never encountered yet!).  
A simple press of the *Reset* button will restart the system without unplugging anything.

You need to activate the feature like this:
```cpp
inline constexpr bool WATCHDOG_PIN_PRESENT{ true };
```
and define the *pin* used, in the example pin *9*:
```cpp
inline constexpr uint8_t watchDogPin{ 9 };
```

## Temperature sensor(s) configuration
It's possible to connect one or more Dallas DS18B20 temperature sensors.  
These sensors can serve informational purposes or to control forced operation mode.

To activate this feature, you need to proceed differently depending on whether you use the Arduino IDE or Visual Studio Code with the PlatformIO extension.

By default, output `D3` is used for the temperature sensor output and already has a pull-up.  
If you want to use another pin, you'll need to add a *pull-up* to the used pin.

### Feature activation

To activate this feature, the procedure differs depending on whether you use the Arduino IDE or Visual Studio Code with the PlatformIO extension.

#### With Arduino IDE
Activate the following line by removing the comment:
```cpp
#define TEMP_ENABLED
```

If the *OneWire* library is not installed, install it via the **Tools** => **Manage Libraries...** menu.  
Search for "Onewire" and install "**OneWire** by Jim Studt, ..." version **2.3.7** or newer.

#### With Visual Studio Code and PlatformIO
Select the "**env:temperature (Mk2_3phase_RFdatalog_temp)**" configuration.

### Sensor configuration (common to both cases above)
To configure the sensors, you must enter their addresses.  
Use a program to scan connected sensors.  
You can find such programs on the Internet or among the examples provided with the Arduino IDE.  
It's recommended to stick a label with each sensor's address on its cable.

Enter the addresses as follows:
```cpp
inline constexpr TemperatureSensing temperatureSensing{ 4,
                                                        { { 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 },
                                                          { 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 } } };
```
The number *4* as first parameter is the *pin* that the user will have chosen for the *OneWire* bus.

___
> [!NOTE]
> Multiple sensors can be connected to the same cable.
> On the Internet you'll find all the details regarding the topology usable with this type of sensors.
___

## Off-peak hours management configuration (dual tariff)
It's possible to entrust off-peak hours management to the router.  
This allows, for example, limiting heating in forced mode to avoid overheating the water with the intention of using the surplus the next morning.  
This limit can be in duration or temperature (requires using a Dallas DS18B20 temperature sensor).

### Hardware configuration
Disconnect the Day/Night contactor control, which is no longer necessary.  
Connect directly a chosen *pin* to the dry contact of the meter (*C1* and *C2* terminals).
___
> [!WARNING]
> You must connect **directly**, a *pin/ground* pair with the *C1/C2* terminals of the meter.
> There must NOT be 230 V on this circuit!
___

### Software configuration
Activate the feature as follows:
```cpp
inline constexpr bool DUAL_TARIFF{ true };
```
Configure the *pin* to which the meter is connected:
```cpp
inline constexpr uint8_t dualTariffPin{ 3 };
```

Configure the duration in *hours* of the off-peak period (for now, only one period is supported per day):
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

Finally, define the operating modes during the off-peak period:
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { -3, 2 } };
```
It's possible to define a configuration for each load independently of the others.
The first parameter of *rg_ForceLoad* determines the startup delay relative to the beginning or end of off-peak hours:
- if the number is positive and less than 24, it's the number of hours,
- if the number is negative greater than −24, it's the number of hours relative to the end of off-peak hours
- if the number is positive and greater than 24, it's the number of minutes,
- if the number is negative less than −24, it's the number of minutes relative to the end of off-peak hours

The second parameter determines the forced operation duration:
- if the number is less than 24, it's the number of hours,
- if the number is greater than 24, it's the number of minutes.

Examples for better understanding (with off-peak start at 23:00, until 7:00, i.e., 8 h duration):
- ```{ -3, 2 }``` : startup **3 hours BEFORE** period end (at 4 AM), for a duration of 2 h.
- ```{ 3, 2 }``` : startup **3 hours AFTER** period start (at 2 AM), for a duration of 2 h.
- ```{ -150, 2 }``` : startup **150 minutes BEFORE** period end (at 4:30), for a duration of 2 h.
- ```{ 3, 180 }``` : startup **3 hours AFTER** period start (at 2 AM), for a duration of 180 min.

For *infinite* duration (so until the end of the off-peak period), use ```UINT16_MAX``` as second parameter:
- ```{ -3, UINT16_MAX }``` : startup **3 hours BEFORE** period end (at 4 AM) with forced operation until the end of off-peak period.

If your system consists of 2 outputs (```NO_OF_DUMPLOADS``` will then have a value of 2), and you want forced operation only on the 2nd output, write:
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { 0, 0 },
                                                              { -3, 2 } };
```

## Priority rotation
Priority rotation is useful when powering a three-phase water heater.  
It allows balancing the operating duration of different resistors over an extended period.

But it can also be interesting if you want to swap the priorities of two devices each day (two water heaters, ...).

For once, activating this function has 2 modes:
- **automatic**, you'll then specify
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::AUTO };
```
- **manual**, you'll then write
```cpp
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
```
In **automatic** mode, rotation happens automatically every 24 h.  
In **manual** mode, you must also define the *pin* that will trigger rotation:
```cpp
inline constexpr uint8_t rotationPin{ 10 };
```

## Forced operation configuration (New Flexible System)

The forced operation (*Boost*) can now be triggered via one or more *pins*, with flexible association between each pin and the loads (dump loads) or relays to activate. This feature allows:

- Activating forced operation from multiple locations or devices
- Precisely targeting one or more loads or relays for each pin  
- Grouping multiple loads/relays under the same command

### Feature activation

Activate the feature in your configuration:
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```

### OverridePins definition

The `OverridePins` structure allows associating each pin with one or more loads or relays, or with predefined groups (e.g., "all loads", "all relays", or "entire system").

Each entry in the array corresponds to a pin, followed by a list or special function that activates one or more groups of loads or relays during forced operation.

**Simple configuration:**
```cpp
// Pin 3 activates load #0 and relay #0
// Pin 4 activates all loads
inline constexpr OverridePins overridePins{ { { 3, { LOAD(0), RELAY(0) } },
                                              { 4, ALL_LOADS() } } };
```

**Advanced configuration:**
```cpp
// Flexible configuration with custom groups
inline constexpr OverridePins overridePins{ { { 3, { RELAY(1), LOAD(1) } },     // Pin 3: load #1 + relay #1
                                              { 4, ALL_LOADS() },              // Pin 4: all loads
                                              { 11, { 1, LOAD(1), LOAD(2) } },  // Pin 11: specific loads
                                              { 12, ALL_LOADS_AND_RELAYS() } } }; // Pin 12: entire system
```

**Available macros:**
- `LOAD(n)` : references load number (resistor controlled, 0 → load #1)
- `RELAY(n)` : references relay number (on/off relay output, 0 → relay #1)  
- `ALL_LOADS()` : all loads
- `ALL_RELAYS()` : all relays
- `ALL_LOADS_AND_RELAYS()` : entire system (loads and relays)

### Usage

- Connect each configured pin to a dry contact (switch, timer, automaton, etc.)
- When a contact is closed, all loads/relays associated with that pin enter forced operation
- As soon as all contacts are open, forced operation is deactivated

**Usage examples:**
- A button in the bathroom to force only the water heater
- A timer on another pin to force all relays for 30 minutes
- A home automation system that activates multiple loads according to demand

## Routing stop
It can be convenient to disable routing during a prolonged absence.  
This feature is particularly useful if the control *pin* is connected to a dry contact that can be controlled remotely, for example via an Alexa routine or similar.  
Thus, you can disable routing during your absence and reactivate it one or two days before your return, to have (free) hot water upon your arrival.

To activate this feature, use the following code:
```cpp
inline constexpr bool DIVERSION_PIN_PRESENT{ true };
```
You must also specify the *pin* to which the dry contact is connected:
```cpp
inline constexpr uint8_t diversionPin{ 12 };
```

# Advanced program configuration

These parameters are found in the `config_system.h` file.

## `DIVERSION_START_THRESHOLD_WATTS` parameter
The `DIVERSION_START_THRESHOLD_WATTS` parameter defines a surplus threshold before any routing to loads configured on the router. It's mainly intended for installations with storage batteries.   
By default, this value is set to 0 W.  
By setting this parameter to 50 W for example, the router will only start routing when 50 W of surplus is available. Once routing has started, the entire surplus will be routed.  
This feature allows establishing a clear hierarchy in the use of produced energy, prioritizing energy storage over immediate consumption. You can adjust this value according to the battery charging system's responsiveness and your energy use priorities.

> [!IMPORTANT]
> This parameter only concerns the routing startup condition.
> Once the threshold is reached and routing has started, the **entire** surplus becomes available for loads.

## `REQUIRED_EXPORT_IN_WATTS` parameter
The `REQUIRED_EXPORT_IN_WATTS` parameter determines the minimum amount of energy that the system must reserve for export or import to the electrical grid before diverting surplus to controlled loads.  
Set to 0 W by default, this parameter can be used to guarantee constant export to the grid, for example to comply with electricity resale agreements.  
A negative value will force the router to consume this power from the grid. This can be useful or even necessary for installations configured in *zero injection* to initiate solar production.

> [!IMPORTANT]
> Unlike the first parameter, this one represents a permanent offset that is continuously subtracted from available surplus.
> If set to 20 W for example, the system will **always** reserve 20 W for export, regardless of other conditions.

# Configuration with ESP32 extension board

The ESP32 extension board allows simple and reliable integration between the Mk2PVRouter and an ESP32 for remote control via Home Assistant. This section details how to properly configure the Mk2PVRouter when using this extension board.

## Pin mapping
When using the ESP32 extension board, the connections between the Mk2PVRouter and ESP32 are predefined as follows:

| ESP32  | Mk2PVRouter | Function                              |
| ------ | ----------- | ------------------------------------- |
| GPIO12 | D12         | Digital Input/Output - Free use       |
| GPIO13 | D11         | Digital Input/Output - Free use       |
| GPIO14 | D13         | Digital Input/Output - Free use       |
| GPIO27 | D10         | Digital Input/Output - Free use       |
| GPIO5  | DS18B20     | 1-Wire bus for temperature probes     |

## `TEMP` bridge configuration
**Important** : If you want the ESP32 to control temperature probes (recommended for Home Assistant integration), **the `TEMP` bridge on the router motherboard must not be soldered**.
- **`TEMP` bridge not soldered** : ESP32 controls temperature probes via GPIO5.
- **`TEMP` bridge soldered** : Mk2PVRouter controls temperature probes via D3.

## Recommended configuration
For optimal use with Home Assistant, it's recommended to activate at minimum the following functions:

### Recommended basic configuration
```cpp
// Serial output type for IoT integration
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::IoT;

// Essential recommended functions
inline constexpr bool DIVERSION_PIN_PRESENT{ true };    // Routing stop
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };     // Forced operation

// Pin configuration according to extension board mapping
inline constexpr uint8_t diversionPin{ 12 };     // D12 - routing stop

// Flexible forced operation configuration  
inline constexpr OverridePins overridePins{ { { 11, ALL_LOADS_AND_RELAYS() } } }; // D11 - forced operation

// Temperature sensor configuration
// IMPORTANT: Disable temperature management in Mk2PVRouter
// if ESP32 manages probes (TEMP bridge not soldered)
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  // Disabled as managed by ESP32
```

> [!NOTE]
> Configuring serial output to `SerialOutputType::IoT` is not strictly mandatory for router operation. However, it's necessary if you want to exploit router data in Home Assistant (instantaneous power, statistics, etc.). Without this configuration, only control functions (forced operation, routing stop) will be available in Home Assistant.

### Recommended additional features
For even more complete integration, you can also add these features:
```cpp
// Priority rotation via pin (optional)
inline constexpr RotationModes PRIORITY_ROTATION{ RotationModes::PIN };
inline constexpr uint8_t rotationPin{ 10 };      // D10 - priority rotation
```

### Temperature probe installation
For temperature probe installation:
- Ensure the `TEMP` bridge is **not** soldered on the router motherboard
- Connect your DS18B20 probes directly via the dedicated connectors on the Mk2PVRouter motherboard
- Configure probes in ESPHome (no configuration needed on Mk2PVRouter side)

Using the ESP32 to manage temperature probes has several advantages:
- Temperature visualization directly in Home Assistant
- Ability to create temperature-based automations
- More flexible probe configuration without having to reprogram the Mk2PVRouter

## Home Assistant integration
Once your MkPVRouter is configured with the ESP32 extension board, you'll be able to:
- Remotely control routing activation/deactivation (ideal during absences)
- Trigger forced operation remotely
- Monitor temperatures in real time
- Create advanced automation scenarios combining solar production data and temperatures

For more details on ESPHome configuration and Home Assistant integration, consult the [detailed documentation available in this gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). This complete guide explains step by step how to configure your ESP32 with ESPHome to maximize your PVRouter's features in Home Assistant.

# Configuration without extension board

[!IMPORTANT] If you don't have the specific extension board or the appropriate motherboard PCB (these two elements not being available for now), you can still achieve integration by your own means.

In this case:
- No connection is predefined between ESP32 and Mk2PVRouter
- You'll need to create your own wiring according to your needs
- Make sure to configure coherently:
  - The router program (config.h file)
  - ESPHome configuration on ESP32
  
Ensure particularly that pin numbers used in each configuration correspond exactly to your physical connections. Don't forget to use logic level adapters if necessary between Mk2PVRouter (5 V) and ESP32 (3.3 V).

For temperature probes, you can connect them directly to ESP32 using a `GPIO` pin of your choice, which you'll then configure in ESPHome. **Don't forget to add a 4.7 kΩ pull-up resistor between the data line (DQ) and +3.3 V power supply** to ensure proper 1-Wire bus operation.

[!NOTE] Even without the extension board, all Home Assistant integration features remain accessible, provided your wiring and software configurations are correctly implemented.

For more details on ESPHome configuration and Home Assistant integration, consult the [detailed documentation available in this gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). This complete guide explains step by step how to configure your ESP32 with ESPHome to maximize your PVRouter's features in Home Assistant.

# Troubleshooting
- Ensure all required libraries are installed.
- Verify correct configuration of pins and parameters.
- Check serial output for error messages.

# Contributing
Contributions are welcome! Please submit issues, feature requests, and pull requests via GitHub.

*unfinished doc*
