# Debugging Guide

This guide provides comprehensive troubleshooting techniques, diagnostic tools, and debugging strategies for the PVRouter system. Given the real-time and safety-critical nature of the system, effective debugging is essential.

## Debug Configuration

### Enabling Debug Mode
```cpp
// config.h - Enable debug features
#define ENABLE_DEBUG        // Enable debug output and assertions
#define DEBUG_TIMING        // Enable timing measurements
#define DEBUG_POWER_CALC    // Enable power calculation debug
#define DEBUG_LOAD_CONTROL  // Enable load control debug
#define DEBUG_MEMORY        // Enable memory monitoring

// Debug output levels
enum class DebugLevel : uint8_t {
  NONE = 0,
  ERROR = 1,
  WARNING = 2,
  INFO = 3,
  VERBOSE = 4
};

constexpr DebugLevel DEBUG_LEVEL = DebugLevel::INFO;
```

### Debug Macros
```cpp
// debug.h - Debug utility macros
#ifdef ENABLE_DEBUG
  #define DEBUG_PRINT(level, msg) \
    if (level <= DEBUG_LEVEL) { \
      Serial.print("["); Serial.print(millis()); Serial.print("] "); \
      Serial.println(msg); \
    }

  #define DEBUG_PRINTF(level, fmt, ...) \
    if (level <= DEBUG_LEVEL) { \
      char buf[128]; \
      snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
      Serial.print("["); Serial.print(millis()); Serial.print("] "); \
      Serial.println(buf); \
    }

  #define DEBUG_ASSERT(condition, msg) \
    if (!(condition)) { \
      Serial.print("ASSERTION FAILED: "); \
      Serial.print(__FILE__); Serial.print(":"); Serial.print(__LINE__); \
      Serial.print(" - "); Serial.println(msg); \
      while(1); \
    }
#else
  #define DEBUG_PRINT(level, msg)
  #define DEBUG_PRINTF(level, fmt, ...)
  #define DEBUG_ASSERT(condition, msg)
#endif
```

## System Diagnostics

### Health Monitoring System
```cpp
// System health monitoring
struct SystemHealth {
  uint32_t adc_overruns;           // ADC ISR timing violations
  uint32_t main_loop_overruns;     // Main loop timing violations
  uint16_t free_memory;            // Available RAM
  uint16_t stack_usage;            // Estimated stack usage
  float max_isr_time;              // Maximum ISR execution time
  bool voltage_fault[NO_OF_PHASES]; // Voltage out of range
  bool current_fault[NO_OF_PHASES]; // Current out of range
  uint32_t last_update;            // Last health update timestamp
};

SystemHealth system_health;

void updateSystemHealth() {
  system_health.free_memory = freeMemory();
  system_health.stack_usage = estimateStackUsage();
  system_health.last_update = millis();

  // Check voltage ranges
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    float voltage = getRMSVoltage(phase);
    system_health.voltage_fault[phase] =
      (voltage < MIN_VOLTAGE) || (voltage > MAX_VOLTAGE);
  }

  DEBUG_PRINTF(DebugLevel::VERBOSE,
               "Health: RAM=%d, Stack=%d, ADC_OR=%lu",
               system_health.free_memory,
               system_health.stack_usage,
               system_health.adc_overruns);
}
```

### Diagnostic Commands
```cpp
// Interactive diagnostic system
void processDiagnosticCommand(String command) {
  if (command == "health") {
    printSystemHealth();
  } else if (command == "power") {
    printPowerReadings();
  } else if (command == "loads") {
    printLoadStatus();
  } else if (command == "memory") {
    printMemoryUsage();
  } else if (command == "timing") {
    printTimingStats();
  } else if (command.startsWith("test ")) {
    String test_name = command.substring(5);
    runDiagnosticTest(test_name);
  } else {
    Serial.println("Available commands:");
    Serial.println("  health  - System health status");
    Serial.println("  power   - Power measurements");
    Serial.println("  loads   - Load status");
    Serial.println("  memory  - Memory usage");
    Serial.println("  timing  - Timing statistics");
    Serial.println("  test <name> - Run diagnostic test");
  }
}
```

## Power Measurement Debugging

### ADC Signal Analysis
```cpp
// ADC raw data capture for analysis
#ifdef DEBUG_POWER_CALC
struct AdcSample {
  uint32_t timestamp;
  uint8_t channel;
  uint16_t raw_value;
  int16_t calibrated_value;
};

constexpr uint16_t ADC_BUFFER_SIZE = 1000;
AdcSample adc_buffer[ADC_BUFFER_SIZE];
volatile uint16_t adc_buffer_index = 0;

void captureAdcSample(uint8_t channel, uint16_t raw_value, int16_t calibrated) {
  if (adc_buffer_index < ADC_BUFFER_SIZE) {
    adc_buffer[adc_buffer_index] = {
      .timestamp = micros(),
      .channel = channel,
      .raw_value = raw_value,
      .calibrated_value = calibrated
    };
    adc_buffer_index++;
  }
}

void dumpAdcBuffer() {
  Serial.println("ADC Sample Dump:");
  Serial.println("Time(us),Channel,Raw,Calibrated");

  for (uint16_t i = 0; i < adc_buffer_index; ++i) {
    const auto& sample = adc_buffer[i];
    Serial.print(sample.timestamp);
    Serial.print(",");
    Serial.print(sample.channel);
    Serial.print(",");
    Serial.print(sample.raw_value);
    Serial.print(",");
    Serial.println(sample.calibrated_value);
  }

  adc_buffer_index = 0;  // Reset buffer
}
#endif
```

### Power Calculation Validation
```cpp
// Real-time power calculation debugging
void debugPowerCalculation() {
  static uint32_t last_debug = 0;
  if (millis() - last_debug > 1000) {  // Once per second

    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
      float voltage_rms = getRMSVoltage(phase);
      float current_rms = getRMSCurrent(phase);
      float real_power = getRealPower(phase);
      float apparent_power = voltage_rms * current_rms;
      float power_factor = (apparent_power > 0) ? real_power / apparent_power : 0;

      DEBUG_PRINTF(DebugLevel::INFO,
                   "P%d: V=%.1fV, I=%.2fA, P=%.0fW, PF=%.3f",
                   phase + 1, voltage_rms, current_rms, real_power, power_factor);
    }

    float total_power = getTotalPower();
    DEBUG_PRINTF(DebugLevel::INFO, "Total Power: %.0fW", total_power);

    last_debug = millis();
  }
}
```

### Waveform Analysis
```cpp
// Capture voltage/current waveforms for analysis
void captureWaveforms() {
  const uint16_t samples_per_cycle = 50;
  float voltage_waveform[NO_OF_PHASES][samples_per_cycle];
  float current_waveform[NO_OF_PHASES][samples_per_cycle];

  // Capture one complete cycle
  for (uint16_t sample = 0; sample < samples_per_cycle; ++sample) {
    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
      voltage_waveform[phase][sample] = getInstantaneousVoltage(phase);
      current_waveform[phase][sample] = getInstantaneousCurrent(phase);
    }
    delayMicroseconds(400);  // 20ms cycle / 50 samples
  }

  // Output captured waveforms
  Serial.println("Waveform Data:");
  Serial.print("Sample");
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    Serial.print(",V"); Serial.print(phase + 1);
    Serial.print(",I"); Serial.print(phase + 1);
  }
  Serial.println();

  for (uint16_t sample = 0; sample < samples_per_cycle; ++sample) {
    Serial.print(sample);
    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
      Serial.print(","); Serial.print(voltage_waveform[phase][sample]);
      Serial.print(","); Serial.print(current_waveform[phase][sample]);
    }
    Serial.println();
  }
}
```

## Timing Analysis

### ISR Performance Monitoring
```cpp
// Measure ISR execution time
#ifdef DEBUG_TIMING
volatile uint32_t isr_start_time;
volatile uint32_t isr_end_time;
volatile uint32_t max_isr_time = 0;
volatile uint32_t isr_call_count = 0;

ISR(ADC_vect) {
  isr_start_time = micros();

  // Normal ISR processing here
  processAdcInterrupt();

  isr_end_time = micros();

  uint32_t execution_time = isr_end_time - isr_start_time;
  if (execution_time > max_isr_time) {
    max_isr_time = execution_time;
  }
  isr_call_count++;

  // Check for timing violation
  if (execution_time > 50) {  // 50μs limit
    system_health.adc_overruns++;
  }
}

void printTimingStats() {
  Serial.println("Timing Statistics:");
  Serial.print("ISR calls: "); Serial.println(isr_call_count);
  Serial.print("Max ISR time: "); Serial.print(max_isr_time); Serial.println("μs");
  Serial.print("ADC overruns: "); Serial.println(system_health.adc_overruns);
  Serial.print("Overrun rate: ");
  Serial.print((float)system_health.adc_overruns / isr_call_count * 100);
  Serial.println("%");
}
#endif
```

### Main Loop Performance
```cpp
// Monitor main loop timing
#ifdef DEBUG_TIMING
uint32_t loop_start_time;
uint32_t max_loop_time = 0;
uint32_t loop_count = 0;

void loop() {
  loop_start_time = millis();

  // Normal loop processing
  mainLoopProcessing();

  uint32_t loop_time = millis() - loop_start_time;
  if (loop_time > max_loop_time) {
    max_loop_time = loop_time;
  }
  loop_count++;

  // Warn if loop is taking too long
  if (loop_time > 10) {  // 10ms warning threshold
    DEBUG_PRINTF(DebugLevel::WARNING, "Slow loop: %lums", loop_time);
  }
}

void printLoopStats() {
  Serial.println("Main Loop Statistics:");
  Serial.print("Loop iterations: "); Serial.println(loop_count);
  Serial.print("Max loop time: "); Serial.print(max_loop_time); Serial.println("ms");
  Serial.print("Average loop rate: ");
  Serial.print(loop_count * 1000.0 / millis());
  Serial.println(" Hz");
}
#endif
```

## Memory Debugging

### Stack Usage Monitoring
```cpp
// Estimate stack usage
extern uint8_t __heap_start;
extern void *__brkval;

uint16_t freeMemory() {
  return (int) &freeMemory - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

uint16_t estimateStackUsage() {
  // This is an approximation - for accurate measurement use specific tools
  static uint16_t min_free_memory = 2048;  // Start with max possible
  uint16_t current_free = freeMemory();

  if (current_free < min_free_memory) {
    min_free_memory = current_free;
  }

  return 2048 - min_free_memory;  // Approximate stack usage
}

void printMemoryUsage() {
  uint16_t free_mem = freeMemory();
  uint16_t stack_usage = estimateStackUsage();

  Serial.println("Memory Usage:");
  Serial.print("Free RAM: "); Serial.print(free_mem); Serial.println(" bytes");
  Serial.print("Est. Stack: "); Serial.print(stack_usage); Serial.println(" bytes");
  Serial.print("Used RAM: "); Serial.print(2048 - free_mem); Serial.println(" bytes");

  if (free_mem < 200) {
    DEBUG_PRINT(DebugLevel::ERROR, "WARNING: Low memory!");
  }
}
```

### Memory Leak Detection
```cpp
// Simple memory leak detection
class MemoryTracker {
private:
  uint16_t baseline_memory;
  uint32_t last_check;

public:
  void setBaseline() {
    baseline_memory = freeMemory();
    last_check = millis();
  }

  void checkForLeaks() {
    uint16_t current_memory = freeMemory();
    uint32_t current_time = millis();

    if (current_memory < baseline_memory - 10) {  // 10 byte tolerance
      DEBUG_PRINTF(DebugLevel::WARNING,
                   "Possible memory leak: %d bytes lost in %lums",
                   baseline_memory - current_memory,
                   current_time - last_check);
    }

    last_check = current_time;
  }
};

MemoryTracker memory_tracker;
```

## Load Control Debugging

### Load State Monitoring
```cpp
// Track load state changes
struct LoadStateHistory {
  uint32_t timestamp;
  uint8_t load_index;
  LoadState old_state;
  LoadState new_state;
  float power_reading;
};

constexpr uint8_t LOAD_HISTORY_SIZE = 50;
LoadStateHistory load_history[LOAD_HISTORY_SIZE];
uint8_t load_history_index = 0;

void logLoadStateChange(uint8_t load_index, LoadState old_state, LoadState new_state) {
  load_history[load_history_index] = {
    .timestamp = millis(),
    .load_index = load_index,
    .old_state = old_state,
    .new_state = new_state,
    .power_reading = getCurrentPower()
  };

  load_history_index = (load_history_index + 1) % LOAD_HISTORY_SIZE;

  DEBUG_PRINTF(DebugLevel::INFO,
               "Load %d: %s -> %s (Power: %.0fW)",
               load_index,
               stateToString(old_state),
               stateToString(new_state),
               getCurrentPower());
}

void dumpLoadHistory() {
  Serial.println("Load State History:");
  Serial.println("Time,Load,Old_State,New_State,Power");

  for (uint8_t i = 0; i < LOAD_HISTORY_SIZE; ++i) {
    uint8_t idx = (load_history_index + i) % LOAD_HISTORY_SIZE;
    const auto& entry = load_history[idx];

    if (entry.timestamp > 0) {  // Valid entry
      Serial.print(entry.timestamp);
      Serial.print(",");
      Serial.print(entry.load_index);
      Serial.print(",");
      Serial.print(stateToString(entry.old_state));
      Serial.print(",");
      Serial.print(stateToString(entry.new_state));
      Serial.print(",");
      Serial.println(entry.power_reading);
    }
  }
}
```

## Hardware Debugging

### Pin State Monitoring
```cpp
// Monitor digital pin states
void debugPinStates() {
  Serial.println("Pin States:");

  for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i) {
    uint8_t pin = loadPriority[i].pin;
    int state = digitalRead(pin);
    Serial.print("Load "); Serial.print(i);
    Serial.print(" (Pin "); Serial.print(pin); Serial.print("): ");
    Serial.println(state ? "HIGH" : "LOW");
  }

#ifdef TEMP_ENABLED
  Serial.print("Temp sensor pin "); Serial.print(PIN_TEMP_SENSOR);
  Serial.print(": "); Serial.println(digitalRead(PIN_TEMP_SENSOR));
#endif

#ifdef DUAL_TARIFF
  Serial.print("Dual tariff pin "); Serial.print(PIN_DUAL_TARIFF);
  Serial.print(": "); Serial.println(digitalRead(PIN_DUAL_TARIFF));
#endif
}
```

### ADC Channel Testing
```cpp
// Test ADC functionality
void testAdcChannels() {
  Serial.println("ADC Channel Test:");

  // Test all voltage channels
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    uint8_t channel = getVoltageChannel(phase);
    uint16_t reading = analogRead(channel);

    Serial.print("V"); Serial.print(phase + 1);
    Serial.print(" (A"); Serial.print(channel); Serial.print("): ");
    Serial.print(reading);
    Serial.print(" ("); Serial.print(reading * 5.0 / 1023.0); Serial.println("V)");
  }

  // Test all current channels
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    uint8_t channel = getCurrentChannel(phase);
    uint16_t reading = analogRead(channel);

    Serial.print("I"); Serial.print(phase + 1);
    Serial.print(" (A"); Serial.print(channel); Serial.print("): ");
    Serial.print(reading);
    Serial.print(" ("); Serial.print(reading * 5.0 / 1023.0); Serial.println("V)");
  }
}
```

## Communication Debugging

### Serial Output Analysis
```cpp
// Monitor serial communication
void debugSerialOutput() {
  static uint32_t last_output = 0;
  static uint32_t output_count = 0;

  uint32_t current_time = millis();

  if (current_time - last_output > 1000) {  // Every second
    output_count++;

    DEBUG_PRINTF(DebugLevel::VERBOSE,
                 "Serial output #%lu, free buffer: %d",
                 output_count, Serial.availableForWrite());

    last_output = current_time;
  }
}
```

## Common Issues and Solutions

### Issue: ADC Timing Violations
**Symptoms:** `adc_overruns` counter increasing
**Diagnosis:**
```cpp
if (system_health.adc_overruns > 0) {
  DEBUG_PRINT(DebugLevel::ERROR, "ADC timing violations detected");
  printTimingStats();
}
```
**Solutions:**
- Reduce ISR complexity
- Optimize critical code paths
- Check for blocking operations in ISR

### Issue: Memory Exhaustion
**Symptoms:** System resets, erratic behavior
**Diagnosis:**
```cpp
if (freeMemory() < 100) {
  DEBUG_PRINT(DebugLevel::ERROR, "Critical memory shortage");
  printMemoryUsage();
}
```
**Solutions:**
- Reduce buffer sizes
- Optimize data structures
- Check for memory leaks

### Issue: Power Measurement Inaccuracy
**Symptoms:** Incorrect power readings
**Diagnosis:**
```cpp
// Check calibration values
for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
  if (voltageCal[phase] == 0 || currentCal[phase] == 0) {
    DEBUG_PRINTF(DebugLevel::ERROR, "Phase %d not calibrated", phase);
  }
}
```
**Solutions:**
- Recalibrate hardware
- Check sensor connections
- Verify calculation algorithms

### Issue: Load Control Malfunctions
**Symptoms:** Loads not switching properly
**Diagnosis:**
```cpp
// Test load outputs
for (uint8_t i = 0; i < NO_OF_DUMPLOADS; ++i) {
  testLoadOutput(i);
  dumpLoadHistory();
}
```
**Solutions:**
- Check pin configurations
- Verify load ratings
- Test TRIAC/relay drivers

## Remote Debugging

### WiFi Debug Interface (ESP32)
```cpp
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>

WebServer debug_server(80);

void setupRemoteDebug() {
  debug_server.on("/health", []() {
    String response = generateHealthJSON();
    debug_server.send(200, "application/json", response);
  });

  debug_server.on("/power", []() {
    String response = generatePowerJSON();
    debug_server.send(200, "application/json", response);
  });

  debug_server.begin();
}

String generateHealthJSON() {
  return String("{") +
    "\"free_memory\":" + system_health.free_memory + "," +
    "\"adc_overruns\":" + system_health.adc_overruns + "," +
    "\"max_isr_time\":" + system_health.max_isr_time + "," +
    "\"uptime\":" + millis() +
    "}";
}
#endif
```

This debugging guide provides comprehensive tools and techniques for troubleshooting the PVRouter system. Remember that safety is paramount - always use proper electrical safety procedures when debugging power electronics.
