# PV Router Cloud Pattern Analysis Tools

This directory contains tools to help you optimize the `RELAY_FILTER_DELAY_MINUTES` parameter for your PV Router installation.

## 📊 Available Tools

### 1. Complete Analysis Script
```bash
./scripts/tune_cloud_immunity.sh
```
**What it does:** Runs comprehensive analysis and provides personalized recommendations based on your climate type.

### 2. Visual Analysis (Recommended)
```bash
./scripts/visualize_cloud_patterns_demo.py
```
**What it does:** Creates beautiful graphs showing:
- Raw power measurements vs TEMA filtered values
- Relay state changes for different delay settings
- Switching frequency analysis
- Clear visual recommendations

**Output files:**
- `cloud_pattern_analysis.png` - Main analysis charts
- `relay_switching_summary.png` - Switching frequency comparison

### 3. Advanced Live Data Visualization
```bash
./scripts/visualize_cloud_patterns.py
```
**What it does:** Parses real test data and creates visualizations (requires working cloud pattern tests).

## 🎯 Quick Start

1. **For beginners:** Run the visual analysis
   ```bash
   ./scripts/visualize_cloud_patterns_demo.py
   ```

2. **For complete analysis:** Use the main script
   ```bash
   ./scripts/tune_cloud_immunity.sh
   ```

3. **View the graphs:** Open the generated PNG files in any image viewer

## 📖 Understanding the Results

### Main Analysis Charts
- **Gray line:** Raw power measurements (what your panels actually produce)
- **Colored lines:** TEMA filtered values for different delay settings (1-5 minutes)
- **Shaded areas:** When the relay would be ON/OFF
- **Red dashed line:** Relay activation threshold (1000W default)
- **Orange areas:** Cloud event periods

### Switching Frequency Chart
- **Lower bars = better:** Fewer relay switches mean more stable operation
- **Choose the setting:** That gives good stability without being too slow to respond

### Climate-Based Recommendations
- **Clear sky regions (1-2 min):** Fast response, minimal clouds
- **Mixed conditions (2-3 min):** Balanced stability and responsiveness  
- **Cloudy regions (3-4 min):** Enhanced stability during frequent clouds
- **Very unstable (4-5 min):** Maximum stability for challenging conditions

## ⚙️ Applying Your Settings

Copy the recommended configuration to your `config.h` file:

```cpp
// Example: For mixed conditions
inline constexpr uint8_t RELAY_FILTER_DELAY_MINUTES{ 3 };
```

For battery systems, also consider negative import thresholds:
```cpp
// Example: Battery system (relay OFF when surplus < 100W)
inline constexpr RelayEngine< 1, RELAY_FILTER_DELAY_MINUTES > relays{
  { { unused_pin, 1000, -100, 1, 1 } }
};
```

## 🔧 Requirements

- **Python 3** with matplotlib and numpy (automatically installed if missing)
- **PlatformIO** (for live data analysis)
- Run from the `Mk2_3phase_RFdatalog_temp` directory

## 📚 More Information

See `docs/Cloud_Pattern_Tuning_Guide.md` for complete documentation.