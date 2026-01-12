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
    - [Sensor configuration (common to both cases above)](#sensor-configuration-common-to-both-cases-above)
  - [Off-peak hours management and scheduled boost (dual tariff)](#off-peak-hours-management-and-scheduled-boost-dual-tariff)
    - [Hardware configuration](#hardware-configuration)
    - [Software configuration](#software-configuration)
    - [Scheduled boost configuration (rg\_ForceLoad)](#scheduled-boost-configuration-rg_forceload)
    - [Visual examples](#visual-examples)
    - [Multiple loads configuration](#multiple-loads-configuration)
    - [Quick reference](#quick-reference)
  - [Priority rotation](#priority-rotation)
  - [Manual boost (pin-triggered)](#manual-boost-pin-triggered)
    - [How it works](#how-it-works)
    - [When to use manual boost](#when-to-use-manual-boost)
    - [Configuration](#configuration)
    - [Targeting loads and relays](#targeting-loads-and-relays)
    - [Configuration examples](#configuration-examples)
    - [Wiring](#wiring)
    - [Practical examples](#practical-examples)
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

Then, you need to assign the corresponding *pins* as well as the priority order at startup.
```cpp
inline constexpr uint8_t physicalLoadPin[NO_OF_DUMPLOADS]{ 5, 7 };
inline constexpr uint8_t loadPrioritiesAtStartup[NO_OF_DUMPLOADS]{ 0, 1 };
```

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
These sensors can serve informational purposes or to control boost mode.

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

## Off-peak hours management and scheduled boost (dual tariff)

This feature allows the router to automatically manage heating during off-peak electricity periods. It's useful for:
- Heating water at night when electricity is cheaper
- Ensuring hot water is available in the morning if solar surplus was insufficient during the day
- Limiting heating duration to avoid overheating (optionally using a temperature sensor)

### Hardware configuration

Disconnect the Day/Night contactor control, which is no longer necessary.
Connect directly a chosen *pin* to the dry contact of the meter (*C1* and *C2* terminals).

> [!WARNING]
> You must connect **directly**, a *pin/ground* pair with the *C1/C2* terminals of the meter.
> There must NOT be 230 V on this circuit!

### Software configuration

**Step 1:** Activate the feature:
```cpp
inline constexpr bool DUAL_TARIFF{ true };
```

**Step 2:** Configure the *pin* connected to the meter:
```cpp
inline constexpr uint8_t dualTariffPin{ 3 };
```

**Step 3:** Set the off-peak period duration in hours (currently, only one period per day is supported):
```cpp
inline constexpr uint8_t ul_OFF_PEAK_DURATION{ 8 };
```

**Step 4:** Configure the scheduled boost timing for each load.

### Scheduled boost configuration (rg_ForceLoad)

The `rg_ForceLoad` array defines **when** and **how long** each load should be boosted during the off-peak period.

```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{ { START_TIME, DURATION } };
```

Each load has two parameters: `{ START_TIME, DURATION }`

#### Understanding the timeline

```
Off-peak period example: 23:00 to 07:00 (8 hours)

        23:00                                           07:00
          |================== OFF-PEAK ==================|
          |                                              |
     START ──────────────────────────────────────────► END
          │                                              │
          │  Positive values                             │
          │  count from here ───►                        │
          │                                              │
          │                        ◄─── Negative values  │
          │                             count from here  │
```

#### Parameter 1: START_TIME (when to start)

| Value | Meaning | Example (off-peak 23:00-07:00) |
|-------|---------|-------------------------------|
| `0` | **Disabled** - no boost for this load | - |
| `1` to `23` | Hours **after** off-peak START | `3` = start at 02:00 (23:00 + 3h) |
| `-1` to `-23` | Hours **before** off-peak END | `-3` = start at 04:00 (07:00 - 3h) |
| `24` or more | Minutes **after** off-peak START | `90` = start at 00:30 (23:00 + 90min) |
| `-24` or less | Minutes **before** off-peak END | `-90` = start at 05:30 (07:00 - 90min) |

> [!NOTE]
> **Why 24?** The value 24 is used as a threshold to distinguish between hours and minutes.
> Values from 1-23 are interpreted as hours, values 24+ are interpreted as minutes.

#### Parameter 2: DURATION (how long to boost)

| Value | Meaning |
|-------|---------|
| `0` | **Disabled** - no boost |
| `1` to `23` | Duration in **hours** |
| `24` or more | Duration in **minutes** |
| `UINT16_MAX` | Until the **end** of off-peak period |

> [!IMPORTANT]
> **Boost always stops when the off-peak period ends**, regardless of the configured duration.
> If you set a duration that would extend past the end of off-peak, the boost will be cut short.

### Visual examples

All examples assume off-peak period from **23:00 to 07:00** (8 hours):

**Example 1:** `{ -3, 2 }` - Start 3 hours before end, run for 2 hours
```
23:00                              04:00    06:00    07:00
  |====================================|======|========|
                                       |BOOST |
                                       └──2h──┘
```
Result: Boost runs from **04:00 to 06:00**

**Example 2:** `{ 2, 3 }` - Start 2 hours after start, run for 3 hours
```
23:00    01:00          04:00                        07:00
  |========|=============|==============================|
           |────BOOST────|
           └─────3h──────┘
```
Result: Boost runs from **01:00 to 04:00**

**Example 3:** `{ -90, 120 }` - Start 90 minutes before end, duration of 120 minutes (but limited)
```
23:00                              05:30    07:00
  |====================================|======|
                                       |BOOST | ← stops here (off-peak ends)
                                       └─90min─┘
```
Result: Boost runs from **05:30 to 07:00** (stops at end of off-peak, not 07:30)

> [!NOTE]
> **Boost always stops at the end of the off-peak period**, even if the configured duration is longer.
> In this example, only 90 minutes of boost occur instead of the configured 120 minutes.

**Example 4:** `{ 1, UINT16_MAX }` - Start 1 hour after start, run until end
```
23:00    00:00                                       07:00
  |========|=========================================|
           |──────────────BOOST──────────────────────|
```
Result: Boost runs from **00:00 to 07:00**

### Multiple loads configuration

Each load can have its own boost schedule. Use `{ 0, 0 }` to disable boost for a specific load.

**Example:** 2 loads, boost only the second one:
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{
    { 0, 0 },      // Load #1: no scheduled boost
    { -3, 2 }      // Load #2: boost 3h before end, for 2h
};
```

**Example:** 3 loads with different schedules:
```cpp
inline constexpr pairForceLoad rg_ForceLoad[NO_OF_DUMPLOADS]{
    { -4, 2 },         // Load #1: 04:00-06:00 (last resort, if water still cold)
    { -2, UINT16_MAX }, // Load #2: 05:00-07:00 (top-up before morning)
    { 0, 0 }           // Load #3: no scheduled boost
};
```

### Quick reference

| Want to... | Use this |
|------------|----------|
| Disable boost | `{ 0, 0 }` |
| Start 2h after off-peak begins, run 3h | `{ 2, 3 }` |
| Start 3h before off-peak ends, run 2h | `{ -3, 2 }` |
| Start 90min after off-peak begins, run 2h | `{ 90, 2 }` |
| Start 90min before off-peak ends, run until end | `{ -90, UINT16_MAX }` |

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

## Manual boost (pin-triggered)

Unlike [scheduled boost](#scheduled-boost-configuration-rg_forceload) which runs automatically during off-peak hours, **manual boost** lets you trigger heating on-demand using physical switches, buttons, timers, or home automation systems.

### How it works

```
┌─────────────────┐
│  Physical pin   │──── When contact closes ───► Loads/relays turn ON at full power
│  (dry contact)  │──── When contact opens  ───► Normal router operation resumes
└─────────────────┘
```

### When to use manual boost

- **Bathroom button**: Quickly heat water before a shower
- **External timer**: Boost all loads for 30 minutes at a specific time
- **Home automation**: Trigger boost via Alexa, Home Assistant, or similar
- **Emergency heating**: Override normal operation when more hot water is needed

### Configuration

**Step 1:** Enable the feature:
```cpp
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };
```

**Step 2:** Define which pin(s) control which load(s):
```cpp
inline constexpr OverridePins overridePins{ { { PIN, TARGETS } } };
```

### Targeting loads and relays

There are **two ways** to specify which loads/relays to activate:

#### Method 1: By index using macros (recommended)

| Macro | Description |
|-------|-------------|
| `LOAD(n)` | Load by index (0 = first load, 1 = second, etc.) |
| `RELAY(n)` | Relay by index (0 = first relay, 1 = second, etc.) |
| `ALL_LOADS()` | All configured loads |
| `ALL_RELAYS()` | All configured relays |
| `ALL_LOADS_AND_RELAYS()` | Everything |

#### Method 2: By physical pin number

You can also use the **physical pin number** directly. This activates whatever load or relay is connected to that pin.

```cpp
{ 3, { 5 } }    // Pin D3 triggers: activates load/relay on pin 5
{ 3, { 5, 7 } } // Pin D3 triggers: activates loads/relays on pins 5 and 7
```

#### Mixing both methods

You can combine both methods in the same configuration:

```cpp
{ 3, { 5, LOAD(1) } }  // Pin D3: activates pin 5 AND load #1
```

### Configuration examples

**Simple:** One button controls everything
```cpp
inline constexpr OverridePins overridePins{ {
    { 11, ALL_LOADS_AND_RELAYS() }    // Pin D11: boost entire system
} };
```

**Using macros:** Target by load/relay index
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { LOAD(0) } },               // Pin D3: boost load #0 (first load)
    { 4, { RELAY(0), RELAY(1) } }     // Pin D4: boost relays #0 and #1
} };
```

**Using pin numbers:** Target by physical pin
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { 5 } },                     // Pin D3: boost whatever is on pin 5
    { 4, { 5, 7 } }                   // Pin D4: boost pins 5 and 7
} };
```

**Mixed approach:** Combining both methods
```cpp
inline constexpr OverridePins overridePins{ {
    { 3, { 5, LOAD(1) } },            // Pin D3: pin 5 + load #1
    { 4, ALL_LOADS() },               // Pin D4: all loads
    { 11, { LOAD(1), LOAD(2) } },     // Pin D11: loads #1 and #2
    { 12, ALL_LOADS_AND_RELAYS() }    // Pin D12: entire system
} };
```

### Wiring

Connect each configured pin to a **dry contact** (voltage-free switch):

```
Arduino pin ────┬──── Switch/Button ────┬──── GND
                │                       │
           (internal pullup)       (closes circuit)
```

- **Contact closed** = boost active (loads at full power)
- **Contact open** = normal router operation

> [!NOTE]
> Pins are configured with internal pull-up resistors. No external resistors needed.

### Practical examples

| Setup | Configuration | Result |
|-------|---------------|--------|
| Bathroom boost button | `{ 3, { LOAD(0) } }` | Press button → water heater runs at 100% |
| 30-min timer | `{ 4, ALL_LOADS() }` | Timer closes contact → all loads boost for timer duration |
| Smart home integration | `{ 11, ALL_LOADS_AND_RELAYS() }` | ESP32/relay module triggers full system boost |

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
inline constexpr bool OVERRIDE_PIN_PRESENT{ true };     // Boost

// Pin configuration according to extension board mapping
inline constexpr uint8_t diversionPin{ 12 };     // D12 - routing stop

// Flexible boost configuration
inline constexpr OverridePins overridePins{ { { 11, ALL_LOADS_AND_RELAYS() } } }; // D11 - boost

// Temperature sensor configuration
// IMPORTANT: Disable temperature management in Mk2PVRouter
// if ESP32 manages probes (TEMP bridge not soldered)
inline constexpr bool TEMP_SENSOR_PRESENT{ false };  // Disabled as managed by ESP32
```

> [!NOTE]
> Configuring serial output to `SerialOutputType::IoT` is not strictly mandatory for router operation. However, it's necessary if you want to exploit router data in Home Assistant (instantaneous power, statistics, etc.). Without this configuration, only control functions (boost, routing stop) will be available in Home Assistant.

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
- Trigger boost remotely
- Monitor temperatures in real time
- Create advanced automation scenarios combining solar production data and temperatures

For more details on ESPHome configuration and Home Assistant integration, consult the [detailed documentation available in this gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). This complete guide explains step by step how to configure your ESP32 with ESPHome to maximize your PVRouter's features in Home Assistant.

# Configuration without extension board

> [!IMPORTANT]
> If you don't have the specific extension board or the appropriate motherboard PCB (these two elements not being available for now), you can still achieve integration by your own means.

In this case:
- No connection is predefined between ESP32 and Mk2PVRouter
- You'll need to create your own wiring according to your needs
- Make sure to configure coherently:
  - The router program (config.h file)
  - ESPHome configuration on ESP32

Ensure particularly that pin numbers used in each configuration correspond exactly to your physical connections. Don't forget to use logic level adapters if necessary between Mk2PVRouter (5 V) and ESP32 (3.3 V).

For temperature probes, you can connect them directly to ESP32 using a `GPIO` pin of your choice, which you'll then configure in ESPHome. **Don't forget to add a 4.7 kΩ pull-up resistor between the data line (DQ) and +3.3 V power supply** to ensure proper 1-Wire bus operation.

> [!NOTE]
> Even without the extension board, all Home Assistant integration features remain accessible, provided your wiring and software configurations are correctly implemented.

For more details on ESPHome configuration and Home Assistant integration, consult the [detailed documentation available in this gist](https://gist.github.com/FredM67/986e1cb0fc020fa6324ccc151006af99). This complete guide explains step by step how to configure your ESP32 with ESPHome to maximize your PVRouter's features in Home Assistant.

# Troubleshooting
- Ensure all required libraries are installed.
- Verify correct configuration of pins and parameters.
- Check serial output for error messages.

# Contributing
Contributions are welcome! Please submit issues, feature requests, and pull requests via GitHub.

*unfinished doc*
