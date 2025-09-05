# Battery System Configuration Guide

[![fr](https://img.shields.io/badge/lang-fr-blue.svg)](BATTERY_CONFIGURATION_GUIDE.md)

## The Real Problem with Battery Systems

When a PV Router is used with battery systems, customers often experience relays that **never turn OFF**. The root cause is the fundamental physics of how battery systems maintain grid balance.

## Understanding the Issue

### Normal Installation (Grid-Tied Only)
```cpp
// Typical configuration
relayOutput(pin, 1000, 200, 5, 5)
//                ^    ^
//                |    └─ Import threshold: 200W (turn OFF when importing > 200W)
//                └─ Surplus threshold: 1000W (turn ON when surplus > 1000W)
```

**Behavior:**
- ☀️ **Surplus > 1000W** → Relay turns ON
- ☁️ **Import > 200W** → Relay turns OFF
- ✅ **Works perfectly** - clear ON/OFF conditions

### Battery Installation (The Problem)
```cpp
// Customer tries to set tight control
relayOutput(pin, 1000, 0, 5, 5)   // ❌ PROBLEM!
//                ^    ^
//                |    └─ Import threshold: 0W  
//                └─ Surplus threshold: 1000W
```

**What happens:**
- ☀️ **Surplus > 1000W** → Relay turns ON
- 🔋 **Battery compensates** → Grid stays ≈ 0W regardless of what the relay does
- ❌ **Relay needs import > 0W to turn OFF** → But battery prevents this!
- 🚨 **Relay stays ON indefinitely**

**Why increasing import threshold makes it worse:**
```cpp
relayOutput(pin, 1000, 50, 5, 5)  // Even worse!
```
- Relay turns OFF when import > 50W
- Battery immediately discharges to bring grid back to 0W  
- Relay turns ON again
- **Result: Relay chattering!** 

## The Correct Solution: Negative Import Threshold

### Battery-Compatible Configuration

```cpp
// Battery-compatible configuration using negative threshold
relayOutput(pin, 1000, -20, 5, 5)
//                ^    ^
//                |    └─ Negative threshold: turn OFF when surplus < 20W
//                └─ Surplus threshold: 1000W (turn ON when surplus > 1000W)
```

**How it works:**
- ☀️ **Surplus > 1000W** → Relay turns ON
- ☁️ **Surplus drops < 20W** → Relay turns OFF
- ✅ **Battery can't prevent this** - we're monitoring surplus, not import!

### Threshold Selection Guidelines

| Installation Type | Recommended Negative Threshold | Reasoning |
|------------------|-------------------------------|-----------|
| **Small loads** (< 1kW) | `-10W to -30W` | Small margin for measurement noise |
| **Medium loads** (1-3kW) | `-20W to -50W` | Balanced approach |
| **Large loads** (> 3kW) | `-50W to -100W` | Larger margin for bigger systems |
| **Very noisy measurements** | `-100W` | For systems with poor measurement accuracy |

## Example Configurations

### Battery X + Pool Pump (1.5kW)
```cpp
relayOutput(4, 1500, -30, 10, 5)
//          ^   ^    ^   ^   ^
//          |   |    |   |   └─ Min OFF: 5 minutes
//          |   |    |   └─ Min ON: 10 minutes (pump protection)
//          |   |    └─ Turn OFF when surplus < 30W
//          |   └─ Turn ON when surplus > 1500W (pump power)
//          └─ Control pin
```

### Battery Y + Water Heating (2kW)
```cpp
relayOutput(5, 2000, -50, 15, 10)
//          ^   ^    ^    ^   ^
//          |   |    |    |   └─ Min OFF: 10 minutes
//          |   |    |    └─ Min ON: 15 minutes
//          |   |    └─ Turn OFF when surplus < 50W
//          |   └─ Turn ON when surplus > 2000W
//          └─ Control pin
```

### Conservative Configuration (Large Battery System)
```cpp
relayOutput(6, 3000, -100, 5, 5)
//          ^   ^    ^     ^ ^
//          |   |    |     | └─ Standard timing
//          |   |    |     └─ Standard timing  
//          |   |    └─ Turn OFF when surplus < 100W (safe margin)
//          |   └─ Turn ON when surplus > 3000W
//          └─ Control pin
```

## How It Works: Technical Explanation

### The Key Insight
**Battery systems maintain grid balance, but they can't hide surplus changes**

- 🔋 **Battery charging/discharging** keeps grid ≈ 0W
- ☀️ **PV surplus changes** are still detectable by monitoring the "surplus side"
- ✅ **Negative thresholds** monitor surplus drops, not import rises

### Comparison of Approaches

| Approach | Grid Monitoring | Works with Battery | Result |
|----------|----------------|-------------------|---------|
| **Positive threshold** | "Turn OFF when import > X" | ❌ No | Battery prevents import |
| **Zero threshold** | "Turn OFF when import > 0" | ❌ No | Fluctuations around 0W |
| **Negative threshold** | "Turn OFF when surplus < X" | ✅ Yes | Battery can't hide surplus drops |

### Serial Output Examples

**Normal mode (positive threshold):**
```
Import threshold: 200 (import mode)
```

**Battery mode (negative threshold):**
```
Import threshold: -50 (surplus mode: turn OFF when surplus < 50W)
```

## Implementation Details

### Internal Logic
```cpp
if (importThreshold >= 0)
{
  // Normal mode: turn OFF when importing > threshold
  if (currentAvgPower > importThreshold)
    return try_turnOFF();
}
else
{
  // Battery mode: turn OFF when surplus < abs(threshold)
  if (currentAvgPower > importThreshold)  // importThreshold is negative
    return try_turnOFF();
}
```

### Integration with EWMA Filter
- EWMA filtering still works perfectly
- Negative thresholds work with the filtered power values
- Cloud immunity is maintained

## Migration Guide

### From Problematic Configuration
```cpp
// Old (problematic)
relayOutput(pin, 1000, 0, 5, 5)     // Never turns OFF with battery

// New (works with battery)  
relayOutput(pin, 1000, -20, 5, 5)   // Turns OFF when surplus < 20W
```

### Choosing the Right Negative Value
1. **Start conservative:** Use -50W to -100W
2. **Monitor behavior:** Watch for proper ON/OFF cycling
3. **Fine-tune:** Adjust based on your system's noise level
4. **Validate:** Ensure reliable operation over several days

## Troubleshooting

### If relay turns OFF too early
- **Symptom:** Relay turns OFF during good sun with battery system
- **Solution:** Make threshold more negative (e.g., -20W → -50W)

### If relay still doesn't turn OFF
- **Check:** Verify you're using negative threshold
- **Check:** Ensure value is appropriate for your load size
- **Check:** Monitor actual surplus values in your system

### If relay chatters
- **Likely cause:** Threshold too close to noise level
- **Solution:** Make threshold more negative or increase EWMA filtering

## Benefits of This Approach

### ✅ **Works with Battery Physics**
- Monitors surplus changes that batteries can't hide
- No workarounds needed for battery compensation

### ✅ **Simple & Robust**
- Single parameter change solves the problem
- No complex logic or timeouts required

### ✅ **Configurable**
- Easy to tune for different systems and noise levels
- Backward compatible with normal installations

### ✅ **Maintains All Features**
- EWMA cloud immunity still works
- Min ON/OFF times still apply
- Integration with other features unchanged

## Summary

Battery system relay issues are solved with **negative import thresholds**:

1. **Root cause:** Battery systems prevent import detection
2. **Physics reality:** Batteries can't hide surplus changes
3. **Elegant solution:** Monitor surplus drops instead of import rises
4. **Result:** Reliable relay operation with battery systems

This approach works **with** the physics of battery systems rather than trying to work around them.