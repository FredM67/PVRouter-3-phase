# Interrupt System Design

This document details the interrupt-driven architecture of the PVRouter, focusing on ADC processing and ISR design principles.

## Overview

The PVRouter uses a sophisticated interrupt system to achieve real-time power measurement and control with microsecond precision. The system is built around the ADC (Analog-to-Digital Converter) interrupt that provides the heartbeat for all timing-critical operations.

## Core Interrupt Architecture

### ADC Interrupt Service Routine (ISR)
The primary interrupt that drives the entire system:

```cpp
ISR(ADC_vect) {
  static uint8_t sample_index = 0;
  
  // Read ADC value
  int rawSample = ADC;
  
  // Process voltage/current samples
  processAdcSample(sample_index, rawSample);
  
  // Increment sample index
  sample_index++;
  if (sample_index >= (NO_OF_PHASES * 2)) {
    sample_index = 0;
  }
  
  // Setup next ADC conversion
  setupNextAdcConversion(sample_index);
}
```

### Timing Characteristics

| Parameter | Value | Notes |
|-----------|--------|-------|
| **ADC Conversion Time** | ~13μs | At 125kHz ADC clock |
| **ISR Execution Time** | <50μs | Critical timing requirement |
| **Sample Rate** | 9.6 kHz | 6 channels × 1.6 kHz each |
| **Cycle Period** | 104.17μs | 6 samples per interrupt cycle |

## Sample Processing Pipeline

### Phase 1: ADC Sample Acquisition
```
ADC Channel Sequence:
V1 → I1 → V2 → I2 → V3 → I3 → [repeat]
```

Each channel is sampled in round-robin fashion to maintain phase relationships.

### Phase 2: Signal Conditioning
```cpp
// Remove DC offset
sampleVminusDC[phase] = rawSample - DCoffset_V[phase];
sampleIminusDC[phase] = rawSample - DCoffset_I[phase];

// Apply calibration
sampleVminusDC[phase] = (sampleVminusDC[phase] * voltageCal[phase]) >> 8;
sampleIminusDC[phase] = (sampleIminusDC[phase] * currentCal[phase]) >> 8;
```

### Phase 3: Power Calculation
```cpp
// Instantaneous power per phase
instantPower[phase] = sampleVminusDC[phase] * sampleIminusDC[phase];

// Accumulate for cycle average
cumVdeltasThisCycle[phase] += sampleVminusDC[phase];
cumIdeltasThisCycle[phase] += sampleIminusDC[phase];
```

## Interrupt Priority and Nesting

### Priority Levels
1. **ADC Interrupt** (Highest) - Timer-critical power measurement
2. **Timer Interrupts** (Medium) - Load control PWM
3. **External Interrupts** (Low) - User input, sensors

### Critical Section Management
```cpp
// Atomic operations for shared variables
ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
  // Critical code that must not be interrupted
  sharedVariable = newValue;
}
```

## Real-Time Constraints

### Hard Real-Time Requirements
- **ADC ISR completion**: Must finish before next conversion (~104μs)
- **Zero missed samples**: System failure if timing is violated
- **Deterministic execution**: No dynamic memory allocation in ISR

### Soft Real-Time Requirements
- **Load switching**: Within 1 mains cycle (20ms @ 50Hz)
- **Temperature reading**: Within 1 second
- **Serial communication**: Best effort, can be delayed

## ISR Design Principles

### Minimal Processing Rule
```cpp
ISR(ADC_vect) {
  // ✅ GOOD: Essential processing only
  rawSample = ADC;
  processRawSample(rawSample);
  
  // ❌ BAD: Avoid in ISR
  // Serial.print("Sample: ");
  // digitalWrite(LED_PIN, HIGH);
  // float result = sqrt(sample);
}
```

### Data Sharing Between ISR and Main Loop
```cpp
// ISR writes, main loop reads
volatile bool dataReady = false;
volatile long powerReadings[NO_OF_PHASES];

ISR(ADC_vect) {
  // Update readings
  powerReadings[phase] = newReading;
  dataReady = true;  // Signal main loop
}

void loop() {
  if (dataReady) {
    dataReady = false;
    // Process readings safely
    processReadings();
  }
}
```

## Advanced Interrupt Features

### Timer-Based Load Control
```cpp
// Timer1 for TRIAC phase control
ISR(TIMER1_COMPA_vect) {
  // Precise timing for TRIAC firing
  if (shouldFireTriac) {
    PORTD |= (1 << triacPin);
    // Schedule turn-off
    OCR1B = OCR1A + TRIAC_PULSE_WIDTH;
  }
}

ISR(TIMER1_COMPB_vect) {
  // Turn off TRIAC pulse
  PORTD &= ~(1 << triacPin);
}
```

### Watchdog Timer Integration
```cpp
ISR(WDT_vect) {
  // System health check
  if (systemHealthy) {
    wdt_reset();  // Reset watchdog
  } else {
    // Allow system reset
    while(1);
  }
}
```

## Performance Optimization

### Assembly-Level Optimizations
```cpp
// Critical path optimizations
inline void __attribute__((always_inline)) fastSample() {
  asm volatile (
    "in %0, %1"     "\n\t"
    : "=r" (sample)
    : "I" (_SFR_IO_ADDR(ADCH))
  );
}
```

### Register Usage Optimization
```cpp
// Use register variables for hot paths
register uint8_t phase asm("r24");  // Force specific register
```

### Branch Prediction Hints
```cpp
// Help compiler optimize common cases
if (__builtin_expect(normalCase, 1)) {
  // Likely path
} else {
  // Unlikely path
}
```

## Error Handling and Recovery

### ISR Overrun Detection
```cpp
volatile uint32_t isrOverruns = 0;

ISR(ADC_vect) {
  static bool isrActive = false;
  
  if (isrActive) {
    isrOverruns++;  // Count overruns
    return;         // Skip this cycle
  }
  
  isrActive = true;
  // Normal processing
  isrActive = false;
}
```

### System State Recovery
```cpp
// Recovery from timing violations
if (isrOverruns > MAX_OVERRUNS) {
  // Reduce load or reset system
  emergencyShutdown();
}
```

## Debugging Interrupt Systems

### Timing Analysis
```cpp
#ifdef DEBUG_TIMING
ISR(ADC_vect) {
  PORTB |= (1 << DEBUG_PIN);  // Set pin high
  
  // ISR processing here
  
  PORTB &= ~(1 << DEBUG_PIN); // Set pin low
}
// Measure pulse width with oscilloscope
#endif
```

### ISR Execution Profiling
```cpp
// Count cycles in ISR
volatile uint32_t isrCycles = 0;

ISR(ADC_vect) {
  uint32_t start = micros();
  
  // ISR processing
  
  isrCycles = micros() - start;
}
```

## Safety Considerations

### Stack Overflow Prevention
- **Minimal local variables** in ISR
- **No recursive calls** from ISR
- **Stack monitoring** in debug builds

### Data Corruption Prevention
- **Atomic access** to multi-byte variables
- **Volatile qualification** for ISR-shared data
- **Memory barriers** where needed

### System Stability
- **Watchdog integration** for fault recovery
- **Graceful degradation** under overload
- **Emergency shutdown** capability

## Future Enhancements

### Higher Resolution ADC
- **12-bit external ADC** for improved accuracy
- **Differential inputs** for better noise immunity
- **Higher sample rates** for better harmonic analysis

### SAMD21 + Rust Migration
- **SAMD21 microcontroller** with superior 12-bit ADC (low noise)
- **Rust implementation** for memory safety and zero-cost abstractions
- **Higher processing power** for advanced algorithms
- **More memory** for complex signal processing
- **Better real-time performance** with dedicated ADC subsystem
- **Compile-time guarantees** for interrupt safety and data races prevention

### Advanced Signal Processing
- **FFT analysis** for harmonic content
- **Digital filtering** for noise reduction
- **Predictive algorithms** for load control

This interrupt system design ensures reliable, real-time operation while maintaining the flexibility to add new features and optimizations.