# Power Calculation Engine

This document details the algorithms and mathematical foundations used for real power measurement in the PVRouter system.

## Overview

The PVRouter implements a sophisticated real power measurement system that accurately calculates instantaneous and average power across three phases of an AC electrical system. The engine is designed for high accuracy, real-time operation, and minimal computational overhead.

## Mathematical Foundation

### Real Power Calculation
Real power (active power) is calculated using the instantaneous product of voltage and current:

```
P(t) = V(t) × I(t)
```

For periodic AC signals, the average real power over one complete cycle is:

```
P_avg = (1/T) ∫[0,T] V(t) × I(t) dt
```

In discrete form (digital sampling):

```
P_avg = (1/N) × Σ[n=0,N-1] V[n] × I[n]
```

Where:
- `N` = number of samples per mains cycle
- `V[n]` = voltage sample at time n
- `I[n]` = current sample at time n

## ADC Sampling Strategy

### Sample Rate Calculation
```cpp
// Target: ~50 samples per mains cycle (50Hz)
constexpr uint16_t SAMPLES_PER_MAIN_CYCLE = 50;
constexpr uint32_t SAMPLE_RATE = SAMPLES_PER_MAIN_CYCLE * MAINS_FREQUENCY; // 2.5 kHz

// For 3-phase system with V+I per phase:
constexpr uint32_t ADC_RATE = SAMPLE_RATE * NO_OF_PHASES * 2; // 15 kHz
```

### Channel Sequencing
The ADC samples channels in a specific sequence to maintain phase relationships:

```
Sample Sequence: V1 → I1 → V2 → I2 → V3 → I3 → [repeat]
Timing:         0μs  17μs  33μs  50μs  67μs  83μs
```

## Signal Processing Pipeline

### Phase 1: DC Offset Removal
```cpp
// Remove DC bias from ADC readings
int16_t sampleVminusDC = rawAdcReading - dcOffset_V;
int16_t sampleIminusDC = rawAdcReading - dcOffset_I;
```

DC offsets are calibrated during startup by averaging samples with no AC signal present.

### Phase 2: Calibration and Scaling
```cpp
// Apply calibration factors
long calibratedV = (long)sampleVminusDC * voltageCal;
long calibratedI = (long)sampleIminusDC * currentCal;

// Scale to maintain precision
calibratedV >>= 8;  // Divide by 256
calibratedI >>= 8;
```

Calibration factors convert ADC counts to engineering units (volts, amperes).

### Phase 3: Instantaneous Power Calculation
```cpp
// Calculate instantaneous power
long instantaneousPower = calibratedV * calibratedI;

// Accumulate for cycle average
sumOfInstantaneousPowers += instantaneousPower;
sampleCount++;
```

## Multi-Phase Power Summation

### Three-Phase Total Power
```cpp
// Calculate power for each phase
for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
  long phasePower = voltageV[phase] * currentI[phase];
  instantPower[phase] = phasePower;
}

// Sum all phases for total power
long totalInstantaneousPower = instantPower[0] + instantPower[1] + instantPower[2];
```

### Phase Balance Monitoring
```cpp
// Calculate power balance between phases
long maxPhasePower = max({instantPower[0], instantPower[1], instantPower[2]});
long minPhasePower = min({instantPower[0], instantPower[1], instantPower[2]});
long phaseImbalance = maxPhasePower - minPhasePower;
```

## Advanced Power Calculations

### RMS Voltage and Current
```cpp
// RMS calculation using sum of squares
for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
  sumVsquared[phase] += (long)voltage[phase] * voltage[phase];
  sumIsquared[phase] += (long)current[phase] * current[phase];
}

// At end of cycle
float rmsVoltage = sqrt(sumVsquared[phase] / samplesPerCycle);
float rmsCurrent = sqrt(sumIsquared[phase] / samplesPerCycle);
```

### Apparent Power and Power Factor
```cpp
// Apparent power
float apparentPower = rmsVoltage * rmsCurrent;

// Power factor
float powerFactor = realPower / apparentPower;
```

### Energy Accumulation
```cpp
// Energy = Power × Time
long energyThisCycle = realPowerThisCycle * (CYCLE_PERIOD_US / 1000000.0);
totalEnergyWh += energyThisCycle / 3600.0;  // Convert to Wh
```

## Filtering and Smoothing

### Exponentially Weighted Moving Average (EWMA)
```cpp
class PowerFilter {
private:
  float alpha;        // Smoothing factor (0 < alpha < 1)
  float filteredValue;

public:
  float update(float newSample) {
    filteredValue = alpha * newSample + (1 - alpha) * filteredValue;
    return filteredValue;
  }
};

// Usage
PowerFilter powerSmooth(0.1);  // 10% weight to new samples
float smoothedPower = powerSmooth.update(instantaneousPower);
```

### Double Exponential Moving Average (DEMA)
```cpp
class DEMAFilter {
private:
  float alpha;
  float ema1, ema2;

public:
  float update(float newSample) {
    ema1 = alpha * newSample + (1 - alpha) * ema1;
    ema2 = alpha * ema1 + (1 - alpha) * ema2;
    return 2 * ema1 - ema2;  // DEMA formula
  }
};
```

### Triple Exponential Moving Average (TEMA)
```cpp
class TEMAFilter {
private:
  float alpha;
  float ema1, ema2, ema3;

public:
  float update(float newSample) {
    ema1 = alpha * newSample + (1 - alpha) * ema1;
    ema2 = alpha * ema1 + (1 - alpha) * ema2;
    ema3 = alpha * ema2 + (1 - alpha) * ema3;
    return 3 * ema1 - 3 * ema2 + ema3;  // TEMA formula
  }
};
```

## Calibration Algorithms

### Automatic DC Offset Calibration
```cpp
void calibrateDCOffsets() {
  const uint16_t CALIBRATION_SAMPLES = 1000;
  long sumV[NO_OF_PHASES] = {0};
  long sumI[NO_OF_PHASES] = {0};

  // Collect samples with loads off
  for (uint16_t i = 0; i < CALIBRATION_SAMPLES; ++i) {
    for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
      sumV[phase] += readADC(voltagePin[phase]);
      sumI[phase] += readADC(currentPin[phase]);
    }
    delay(1);
  }

  // Calculate averages
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    dcOffset_V[phase] = sumV[phase] / CALIBRATION_SAMPLES;
    dcOffset_I[phase] = sumI[phase] / CALIBRATION_SAMPLES;
  }
}
```

### Phase Calibration
```cpp
// Determine phase relationship between V and I channels
void calibratePhase() {
  // Apply known resistive load
  // Measure phase difference
  // Calculate correction factor

  float phaseError = calculatePhaseError();
  phaseCorrectionFactor = cos(phaseError);
}
```

## Accuracy and Error Analysis

### Sources of Error

1. **ADC Quantization Error**
   - Error: ±0.5 LSB
   - Impact: ~0.1% at full scale

2. **CT Phase Error**
   - Typical: ±1° phase shift
   - Impact: ~1.5% power error

3. **Temperature Drift**
   - ADC: ±50ppm/°C
   - Impact: ±0.2% over 40°C range

4. **Timing Jitter**
   - ISR timing variation: ±1μs
   - Impact: <0.01% power error

### Error Compensation
```cpp
// Temperature compensation
float tempCompFactor = 1.0 + TEMP_COEFF * (currentTemp - REFERENCE_TEMP);
calibratedValue = rawValue * calibrationFactor * tempCompFactor;

// Phase compensation
float phaseCorrectedI = current * cos(phaseError) +
                       current_delayed * sin(phaseError);
```

## Performance Optimization

### Fixed-Point Arithmetic
```cpp
// Use fixed-point for speed
typedef int32_t fixed_t;
constexpr uint8_t FIXED_SHIFT = 16;

fixed_t floatToFixed(float f) {
  return (fixed_t)(f * (1 << FIXED_SHIFT));
}

float fixedToFloat(fixed_t f) {
  return (float)f / (1 << FIXED_SHIFT);
}

// Fixed-point multiplication
fixed_t fixedMul(fixed_t a, fixed_t b) {
  return ((int64_t)a * b) >> FIXED_SHIFT;
}
```

### Lookup Tables
```cpp
// Pre-calculated sine table for harmonic analysis
constexpr uint16_t SINE_TABLE_SIZE = 256;
const int16_t SINE_TABLE[SINE_TABLE_SIZE] PROGMEM = {
  0, 804, 1608, 2410, 3212, 4011, 4808, 5602, // ...
};

int16_t fastSin(uint8_t angle) {
  return pgm_read_word(&SINE_TABLE[angle]);
}
```

## Quality Assurance

### Self-Test Procedures
```cpp
bool powerEngineSystemTest() {
  // Inject known test signal
  // Verify calculated power matches expected
  // Check all phases
  // Validate RMS calculations
  return allTestsPassed;
}
```

### Continuous Monitoring
```cpp
// Monitor calculation health
struct PowerHealth {
  bool voltageInRange[NO_OF_PHASES];
  bool currentInRange[NO_OF_PHASES];
  bool powerFactorValid[NO_OF_PHASES];
  uint32_t calculationErrors;
};

void updatePowerHealth() {
  for (uint8_t phase = 0; phase < NO_OF_PHASES; ++phase) {
    powerHealth.voltageInRange[phase] =
      (rmsVoltage[phase] > MIN_VOLTAGE) && (rmsVoltage[phase] < MAX_VOLTAGE);

    powerHealth.powerFactorValid[phase] =
      (powerFactor[phase] >= -1.0) && (powerFactor[phase] <= 1.0);
  }
}
```

## Future Enhancements

### Harmonic Analysis
- **FFT implementation** for harmonic content analysis
- **THD calculation** for power quality monitoring
- **Frequency tracking** for grid stability assessment

### Advanced Filtering
- **Kalman filters** for state estimation
- **Adaptive filters** for changing conditions
- **Notch filters** for specific frequency rejection

### Machine Learning Integration
- **Load prediction** algorithms
- **Anomaly detection** for fault diagnosis
- **Optimization** of diverter control

This power calculation engine provides the foundation for accurate, real-time power measurement and control in the PVRouter system.
