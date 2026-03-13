# Cloud Pattern Analysis Guide

## Overview

This guide helps you tune the `RELAY_FILTER_DELAY_MINUTES` parameter in your PV Router configuration based on your local weather conditions. The parameter controls how aggressively the EWMA filter smooths power measurements before making relay decisions.

## How to Use This Guide

### 1. Run the Cloud Pattern Analysis

**Option A: Automated Analysis (Recommended)**
```bash
cd Mk2_3phase_RFdatalog_temp
./scripts/tune_cloud_immunity.sh
```

**Option B: Direct Test Execution**
```bash
cd Mk2_3phase_RFdatalog_temp
pio test -e native --filter="test_cloud_patterns" -v
```

**Option C: Visual Analysis (Beautiful Graphs)**
```bash
cd Mk2_3phase_RFdatalog_temp
./scripts/visualize_cloud_patterns_demo.py
```

This will show you how different filter delay settings perform with various cloud patterns typical of different climates.

### 2. Analyze the Results

The tests show:
- **Sample Data**: Realistic power measurements during different weather conditions
- **Filtered Values**: How different delay settings smooth the data (1min, 2min, 3min, 4min, 5min)
- **Relay States**: When the relay would turn on/off (1=on, 0=off)
- **State Changes**: Total relay switching events (lower = better stability)

### 3. Choose Your Setting

Based on the analysis results and your local climate:

## Configuration Guidelines

### 🌤️ Clear Sky Regions (Desert, Dry Climates)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 1 };
```
- **Characteristics**: Minimal cloud cover, stable solar production
- **Benefits**: Fast response to actual power changes
- **Examples**: Arizona, Nevada, parts of California, Australian Outback

### ⛅ Mixed Conditions (Most Installations)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 };
```
- **Characteristics**: Occasional clouds, typical suburban/rural locations
- **Benefits**: Good balance of responsiveness and stability
- **Examples**: Most of continental Europe, central US states, temperate regions
- **Recommended default setting**

### ☁️ Frequently Cloudy (Coastal, Temperate)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 3 };
```
- **Characteristics**: Regular cloud cover, maritime climates
- **Benefits**: Enhanced stability during frequent cloud events
- **Examples**: UK, coastal regions, Pacific Northwest, Northern Europe

### 🌧️ Very Cloudy (Mountain, Tropical, Marine)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 4 };
```
- **Characteristics**: Challenging conditions with frequent/dramatic cloud changes
- **Benefits**: Maximum stability, reduced relay wear
- **Examples**: Mountain regions, tropical areas, marine environments

## Test Pattern Interpretations

### Scattered Clouds Pattern
- **Scenario**: Light, intermittent cloud cover
- **Typical Result**: 1-2 minute delays work well
- **Climate Match**: Spring/fall conditions in temperate regions

### Heavy Cloud Bank Pattern
- **Scenario**: Dense cloud formation passing over
- **Typical Result**: 2-3 minute delays recommended
- **Climate Match**: Weather fronts, storm systems

### Intermittent Clouds Pattern
- **Scenario**: Rapid cloud/sun alternation
- **Typical Result**: Higher delays reduce chatter significantly
- **Climate Match**: Unstable weather, cumulus cloud days

### Morning Fog Clearing Pattern
- **Scenario**: Gradual power increase as fog lifts
- **Typical Result**: Most delay settings work well (gradual change)
- **Climate Match**: Coastal areas, valleys with morning fog

### Storm Approach Pattern
- **Scenario**: Clear conditions deteriorating to storms
- **Typical Result**: 3+ minute delays prevent premature relay shutoff
- **Climate Match**: Thunderstorm development, weather fronts

### Battery System Pattern
- **Scenario**: Grid-tied battery system with surplus-based control
- **Special Configuration**: Use negative import threshold
- **Example**: `-100` means relay turns on when surplus > 100W

## Complete Configuration Examples

### Standard Installation (Grid-Tie Only)
```cpp
// config.h
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 }; // Adjust based on your climate

// Relay turns OFF when importing > 200W (normal behavior)
inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{
  { { unused_pin, 1000, 200, 1, 1 } }
};
```

### Battery System Installation
```cpp
// config.h
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 }; // 2-3 minutes recommended

// Relay turns OFF when surplus < 100W (battery-friendly)
inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{
  { { unused_pin, 1000, -100, 1, 1 } }
};
```

## Performance Impact

The EWMA filter performance is excellent across all delay settings:
- **1-2 minutes**: ~28-30 ns per operation
- **3-5 minutes**: ~30-32 ns per operation
- **Overhead**: Minimal impact on microcontroller performance

## Validation

Run your chosen configuration in actual conditions and monitor:
1. **Relay cycling frequency** during partly cloudy days
2. **Response time** to actual power changes
3. **System stability** during challenging weather

If you see excessive relay chatter during cloudy periods, increase the delay setting. If the system responds too slowly to genuine power changes, decrease it.

## Advanced Tuning

For locations with very specific weather patterns, you can:
1. Log actual power data during typical cloud events
2. Modify the test patterns to match your specific conditions
3. Run custom analysis to find optimal settings

The cloud pattern tests provide a scientific basis for configuration rather than guesswork, leading to optimal PV router performance for your specific installation and climate.
