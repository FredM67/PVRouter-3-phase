# EWMA Filter Tuning Guide for Cloud Immunity

[![Français](https://img.shields.io/badge/🇫🇷%20Langue-Français-blue?style=for-the-badge)](EWMA_TUNING_GUIDE.md) [![English](https://img.shields.io/badge/🌍%20Language-English-red?style=for-the-badge)](EWMA_TUNING_GUIDE.en.md)

---

## Overview

The EWMA (Exponentially Weighted Moving Average) filter in the PV Router prevents relay chattering caused by clouds passing over solar panels. This guide explains how to tune the filter for optimal performance.

## Recent Improvements

### 1. Enhanced Filter Algorithm
- **Changed from EMA to TEMA**: The relay control now uses Triple EMA (`getAverageT()`) instead of single EMA
- **Better Cloud Immunity**: TEMA provides superior immunity to brief cloud shadows while maintaining good responsiveness to genuine changes
- **Optimal Performance**: TEMA offers the best balance between stability and responsiveness

### 2. Battery System Understanding 🔋 **NEW**
- **Configuration Solution**: Proper import threshold configuration for battery systems
- **Physics-Based Approach**: Account for measurement fluctuations around zero
- **Simple & Robust**: No complex workarounds needed, just proper configuration

### 3. Easy Configuration
New configuration parameters have been added to `config.h`:

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 };  /**< EWMA filter delay in minutes */
```

For battery systems, see `../../Mk2_3phase_RFdatalog_temp/BATTERY_CONFIGURATION_GUIDE.en.md` for proper threshold configuration.

## Tuning Guidelines

### Filter Time Constants

| Region Type | Recommended Delay | Characteristics |
|-------------|------------------|-----------------|
| **Clear Sky** | 1 minute | Fast response, minimal clouds |
| **Mixed Conditions** | 2 minutes | Balanced performance (default) |
| **Very Cloudy** | 3+ minutes | Maximum stability, frequent clouds |

### How to Tune

1. **Edit `config.h`**: Change `RELAY_FILTER_DELAY_MINUTES` value
2. **Recompile and upload** the firmware
3. **Monitor performance** for a few days
4. **Adjust if needed**:
   - If relays chatter during clouds → Increase delay
   - If response is too slow → Decrease delay

### Technical Details

#### Current Implementation
- **Sample Rate**: 5 seconds (configurable via `DATALOG_PERIOD_IN_SECONDS`)
- **Filter Type**: Triple EMA (TEMA) for optimal cloud immunity
- **Time Constant**: `RELAY_FILTER_DELAY_MINUTES * 60 / DATALOG_PERIOD_IN_SECONDS`

#### Filter Behavior
- **Cloud Shadows (5-60 seconds)**: Filtered out, no relay switching
- **Genuine Load Changes**: Quick response (within 1-2 filter time constants)
- **Solar Production Changes**: Smooth response without oscillation

## Example Configurations

### For Clear Sky Regions (minimal clouds)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 1 }; // Fast, responsive
```

### For Mixed Conditions (default)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 2 }; // Balanced
```

### For Very Cloudy Regions
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 3 }; // Stable, slow
```

### For Extreme Conditions (very frequent clouds)
```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 5 }; // Maximum stability
```

### For Battery Systems 🔋 **NEW**
See `../../Mk2_3phase_RFdatalog_temp/BATTERY_CONFIGURATION_GUIDE.en.md` for detailed configuration guidance.
```cpp
// Example: Proper import threshold for battery systems
relayOutput(pin, 1000, 50, 5, 5)  // 50W import threshold instead of 0W
```

## Benefits of TEMA Filter

1. **Relay Longevity**: Prevents wear from frequent switching
2. **Load Stability**: Consistent power delivery to heating elements
3. **System Efficiency**: Reduces energy waste from switching losses
4. **Grid Stability**: Smoother power flow, less grid disturbance
5. **Optimal Response**: Best balance of stability and responsiveness

## Monitoring Performance

Watch for these indicators:

### Good Tuning
- Relays switch smoothly during genuine power changes
- No chattering during brief cloud shadows
- System responds appropriately to load changes

### Need to Increase Delay
- Relays turn on/off rapidly during clouds
- System seems "nervous" or unstable
- Frequent switching sounds from relay contacts

### Need to Decrease Delay
- System responds slowly to load changes
- Takes too long to start diversion after solar increase
- Sluggish response to consumption changes

## Advanced Configuration

For users who want to experiment with different filter types, the relay system can be modified to use:

- **EMA**: `ewma_average.getAverageS()` - Most responsive, least filtering
- **DEMA**: `ewma_average.getAverageD()` - Good balance
- **TEMA**: `ewma_average.getAverageT()` - Best cloud immunity (current default)

The filter selection is in `utils_relay.h` in the `get_average()` and `proceed_relays()` methods.

### Visual Filter Comparison

For detailed analysis of filter performance, see the comprehensive analysis in `TEMA_ANALYSIS_README.md` which includes:

![Filter Comparison](../plots/ema_dema_tema_comparison_extended.png)

This analysis demonstrates why TEMA is the optimal choice for PV router applications.

## Troubleshooting

### Common Issues

1. **Still getting relay chatter**: Increase `RELAY_FILTER_DELAY_MINUTES`
2. **Too slow response**: Decrease `RELAY_FILTER_DELAY_MINUTES`
3. **Inconsistent behavior**: Check that `DATALOG_PERIOD_IN_SECONDS` is appropriate for your installation

### Advanced Debugging

Monitor the EWMA filter values through serial output or telemetry to understand filter behavior:
- Raw power values vs filtered values
- Filter response time to step changes
- Stability during cloud events

---

*This tuning guide helps optimize your PV Router for your specific weather conditions and installation requirements.*
