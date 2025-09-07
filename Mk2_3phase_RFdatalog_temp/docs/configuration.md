# Configuration System

This document describes the compile-time validation and type safety mechanisms implemented in the PVRouter configuration system.

## Design Philosophy

The PVRouter configuration system is built on the principle of "fail fast, fail at compile time." Rather than discovering configuration errors at runtime (potentially causing system damage), the system uses modern C++ features to validate all configuration parameters during compilation.

## Core Configuration Architecture

### Configuration Headers Structure
```
config.h              ← User-specific configuration (main file)
├── config_system.h   ← System-wide constants and types
├── calibration.h     ← Hardware calibration values
├── types.h          ← Type definitions and enums
└── utils_*.h        ← Feature-specific utilities
```

### Type-Safe Configuration
```cpp
// Strong typing prevents configuration errors
enum class SerialOutputType : uint8_t {
  HumanReadable = 0,
  EmonTX = 1,
  IntegralOnly = 2,
  PVOnly = 3
};

enum class RotationModes : uint8_t {
  OFF = 0,
  AUTO = 1,
  PIN = 2
};

// Configuration with explicit types
inline constexpr SerialOutputType SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;
inline constexpr RotationModes PRIORITY_ROTATION = RotationModes::OFF;
```

## Compile-Time Validation System

### Static Assertions for Safety
```cpp
// Validate fundamental constraints
static_assert(NO_OF_DUMPLOADS > 0, "At least one dump load must be configured");
static_assert(NO_OF_DUMPLOADS <= 6, "Maximum 6 dump loads supported");
static_assert(NO_OF_PHASES == 3, "Only 3-phase operation supported");

// Validate pin assignments
static_assert(VOLTAGE_SENSOR_PIN != CURRENT_SENSOR_PIN, 
              "Voltage and current pins must be different");

// Validate timing constraints
static_assert(DATALOG_PERIOD_IN_SECONDS >= 5, 
              "Datalog period must be at least 5 seconds");
static_assert(DATALOG_PERIOD_IN_SECONDS <= 300, 
              "Datalog period must not exceed 5 minutes");
```

### Template-Based Bounds Checking
```cpp
// Compile-time bounds checking for array access
template<uint8_t INDEX>
constexpr auto getLoadConfig() {
  static_assert(INDEX < NO_OF_DUMPLOADS, "Load index out of bounds");
  return loadPriority[INDEX];
}

// Type-safe sensor access
template<uint8_t SENSOR_ID>
constexpr float readTemperature() {
  static_assert(SENSOR_ID < TEMPERATURE_SENSORS_COUNT, 
                "Temperature sensor index out of bounds");
  // Implementation...
}
```

### Conditional Compilation Guards
```cpp
// Feature-dependent validation
#ifdef TEMP_ENABLED
  static_assert(TEMPERATURE_SENSORS_COUNT > 0, 
                "Temperature feature enabled but no sensors configured");
  static_assert(TEMPERATURE_SENSORS_COUNT <= MAX_TEMPERATURE_SENSORS,
                "Too many temperature sensors configured");
  
  // Validate sensor addresses
  static_assert(sizeof(temperatureSensors) == 
                TEMPERATURE_SENSORS_COUNT * sizeof(DeviceAddress),
                "Temperature sensor array size mismatch");
#endif

#ifdef DUAL_TARIFF
  static_assert(PIN_DUAL_TARIFF != PIN_TEMP_SENSOR,
                "Dual tariff pin conflicts with temperature sensor pin");
#endif
```

## Configuration Validation Framework

### Pin Conflict Detection
```cpp
// Comprehensive pin conflict checking
template<uint8_t PIN>
constexpr bool isPinUsed() {
  bool used = false;
  
  // Check against all configured pins
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i) {
    if (loadPriority[i].pin == PIN) used = true;
  }
  
#ifdef TEMP_ENABLED
  if (PIN_TEMP_SENSOR == PIN) used = true;
#endif

#ifdef DUAL_TARIFF
  if (PIN_DUAL_TARIFF == PIN) used = true;
#endif

  return used;
}

// Validate no pin conflicts at compile time
static_assert(!isPinUsed<PIN_VOLTAGE_SENSOR_1>(), "Pin conflict detected");
```

### Load Configuration Validation
```cpp
// Validate load configuration
template<uint8_t LOAD_INDEX>
constexpr bool validateLoadConfig() {
  constexpr auto config = loadPriority[LOAD_INDEX];
  
  // Check pin validity
  static_assert(config.pin >= 2 && config.pin <= 13, "Invalid pin number");
  
  // Check power rating
  static_assert(config.nominalPower > 0, "Load power must be positive");
  static_assert(config.nominalPower <= 10000, "Load power too high (>10kW)");
  
  // Check load type
  static_assert(config.type == LoadType::TRIAC || config.type == LoadType::RELAY,
                "Invalid load type");
  
  return true;
}

// Validate all loads
template<uint8_t... INDICES>
constexpr bool validateAllLoads(std::index_sequence<INDICES...>) {
  return (validateLoadConfig<INDICES>() && ...);
}

static_assert(validateAllLoads(std::make_index_sequence<NO_OF_DUMPLOADS>{}),
              "Load configuration validation failed");
```

### Memory Layout Validation
```cpp
// Ensure critical structures fit in memory
static_assert(sizeof(powerReadings) <= 100, "Power readings structure too large");
static_assert(sizeof(loadState) <= 50, "Load state structure too large");

// Validate stack usage estimates
constexpr size_t ESTIMATED_STACK_USAGE = 
  sizeof(int) * 10 +           // Local variables
  sizeof(void*) * 5 +          // Function calls
  64;                          // Safety margin

static_assert(ESTIMATED_STACK_USAGE < 200, "Stack usage too high");
```

## Type Safety Mechanisms

### Strong Typing for Units
```cpp
// Type-safe units to prevent unit confusion
struct Watts {
  int16_t value;
  constexpr explicit Watts(int16_t w) : value(w) {}
};

struct Millivolts {
  int16_t value;
  constexpr explicit Millivolts(int16_t mv) : value(mv) {}
};

// Prevents accidental unit mixing
constexpr Watts loadPower(1000);              // ✅ Clear units
// constexpr Watts bad = 1000;                // ❌ Compilation error
```

### Enum Class for Type Safety
```cpp
// Prevent implicit conversions and magic numbers
enum class LoadState : uint8_t {
  OFF = 0,
  ON = 1,
  TRANSITIONING = 2
};

// Type-safe state management
void setLoadState(uint8_t loadIndex, LoadState state) {
  // Implementation...
}

// Usage
setLoadState(0, LoadState::ON);              // ✅ Clear intent
// setLoadState(0, 1);                       // ❌ Compilation error
```

### Constexpr Configuration Functions
```cpp
// Compile-time configuration calculations
constexpr uint32_t calculateSampleRate() {
  return SAMPLES_PER_CYCLE * MAINS_FREQUENCY * NO_OF_PHASES * 2;
}

constexpr uint16_t calculateBufferSize() {
  return calculateSampleRate() / 10;  // 100ms buffer
}

// Computed at compile time
constexpr uint32_t SAMPLE_RATE = calculateSampleRate();
constexpr uint16_t BUFFER_SIZE = calculateBufferSize();
```

## Configuration Validation Tools

### Custom Static Assert Messages
```cpp
#define CONFIG_ASSERT(condition, message) \
  static_assert(condition, "Configuration Error: " message)

// Usage with clear error messages
CONFIG_ASSERT(MAINS_FREQUENCY == 50 || MAINS_FREQUENCY == 60,
              "Mains frequency must be 50Hz or 60Hz");

CONFIG_ASSERT(POWERVAL_DIVIDER > 0,
              "Power divider must be positive non-zero value");
```

### Compile-Time Configuration Report
```cpp
// Generate configuration summary at compile time
constexpr const char* generateConfigSummary() {
  return 
    "Configuration Summary:\n"
    "- Dump Loads: " STR(NO_OF_DUMPLOADS) "\n"
    "- Temperature Sensors: " STR(TEMPERATURE_SENSORS_COUNT) "\n"
    "- Sample Rate: " STR(SAMPLE_RATE) " Hz\n"
    "- Memory Usage: " STR(ESTIMATED_MEMORY_USAGE) " bytes\n";
}

#pragma message(generateConfigSummary())
```

## Feature Flag Management

### Hierarchical Feature Dependencies
```cpp
// Define feature dependencies
#ifdef EMONESP_CONTROL
  #ifndef SERIALOUT_ON
    #error "EmonESP control requires serial output to be enabled"
  #endif
  
  #if SERIAL_OUTPUT_TYPE != SerialOutputType::EmonTX
    #error "EmonESP control requires EmonTX serial output format"
  #endif
#endif

#ifdef PRIORITY_ROTATION
  #if PRIORITY_ROTATION == RotationModes::PIN
    #ifndef PRIORITY_ROTATION_PIN
      #error "Pin-based priority rotation requires PRIORITY_ROTATION_PIN"
    #endif
  #endif
#endif
```

### Conditional Configuration Blocks
```cpp
// Configuration blocks that adapt based on features
#ifdef TEMP_ENABLED
  inline constexpr uint8_t ACTIVE_FEATURES = FEATURE_TEMP;
#else
  inline constexpr uint8_t ACTIVE_FEATURES = 0;
#endif

#ifdef DUAL_TARIFF
  inline constexpr uint8_t ACTIVE_FEATURES |= FEATURE_DUAL_TARIFF;
#endif

// Validate feature combinations
static_assert(!(ACTIVE_FEATURES & FEATURE_TEMP) || (PIN_TEMP_SENSOR != 0),
              "Temperature feature enabled but no pin configured");
```

## Runtime Configuration Validation

### Startup Configuration Check
```cpp
bool validateRuntimeConfig() {
  bool valid = true;
  
  // Check hardware presence
  if (!checkADCFunctionality()) {
    Serial.println("ERROR: ADC not functioning");
    valid = false;
  }
  
  // Validate calibration values
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    if (voltageCal[phase] == 0 || currentCal[phase] == 0) {
      Serial.println("ERROR: Calibration values not set");
      valid = false;
    }
  }
  
  // Check pin availability
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i) {
    if (!isPinAvailable(loadPriority[i].pin)) {
      Serial.println("ERROR: Load pin not available");
      valid = false;
    }
  }
  
  return valid;
}
```

### Configuration Self-Test
```cpp
void performConfigurationSelfTest() {
  Serial.println("Configuration Self-Test:");
  
  // Test all configured loads
  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i) {
    Serial.print("Testing load ");
    Serial.print(i);
    testLoad(i);
    Serial.println(" - OK");
  }
  
  // Test temperature sensors
#ifdef TEMP_ENABLED
  for (uint8_t i = 0; i < TEMPERATURE_SENSORS_COUNT; ++i) {
    if (testTemperatureSensor(i)) {
      Serial.print("Temperature sensor ");
      Serial.print(i);
      Serial.println(" - OK");
    }
  }
#endif
  
  Serial.println("Configuration test complete");
}
```

## Best Practices

### Configuration Guidelines
1. **Use constexpr** for all compile-time constants
2. **Apply static_assert** for all validation rules
3. **Use enum class** instead of plain enums
4. **Validate pin assignments** to prevent conflicts
5. **Check memory constraints** at compile time

### Error Prevention Strategies
1. **Strong typing** prevents unit confusion
2. **Template bounds checking** prevents array overruns
3. **Feature dependency validation** prevents incompatible combinations
4. **Comprehensive pin conflict detection**
5. **Memory usage validation**

### Debugging Configuration Issues
```cpp
#ifdef DEBUG_CONFIG
  // Dump configuration at startup
  void dumpConfiguration() {
    Serial.println("=== Configuration Dump ===");
    Serial.print("Dump loads: "); Serial.println(NO_OF_DUMPLOADS);
    Serial.print("Temperature sensors: "); Serial.println(TEMPERATURE_SENSORS_COUNT);
    Serial.print("Sample rate: "); Serial.println(SAMPLE_RATE);
    // ... more debug output
  }
#endif
```

This configuration system ensures that the vast majority of configuration errors are caught at compile time, greatly improving system reliability and reducing the risk of runtime failures that could damage equipment or cause unsafe operation.