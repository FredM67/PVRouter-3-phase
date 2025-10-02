---
name: Bug report
about: Create a report to help us improve
title: '[BUG] '
labels: 'bug'
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**Hardware Configuration**
- **PVRouter version**: [e.g. basic, rf, emonesp]
- **Firmware version**: [e.g. commit hash or release tag]
- **Load configuration**: [e.g. 3-phase water heater, single-phase loads]
- **RF module**: [if applicable: RFM12B, RFM69CW, etc.]

**Electrical Installation**
- **Grid configuration**: [e.g. 3-phase + neutral, 3-phase no neutral]
- **Grid voltage**: [e.g. 230V/400V, 240V/415V]
- **Grid frequency**: [e.g. 50Hz, 60Hz]
- **Load type**: [e.g. resistive water heater, heat pump, etc.]
- **Load power rating**: [e.g. 3kW, 6kW]

**To Reproduce**
Steps to reproduce the behavior:
1. Set configuration: '...'
2. Apply load: '...'
3. Observe behavior: '...'
4. See error/unexpected behavior

**Expected behavior**
A clear and concise description of what you expected to happen.

**Actual behavior**
What actually happened instead.

**Serial Monitor Output**
If applicable, paste relevant serial monitor output:
```
[Paste serial output here]
```

**Configuration Settings**
Please share relevant configuration from `config.h` or other config files:
```cpp
// Paste relevant configuration here
```

**Measurements/Data**
If applicable, provide:
- Power measurements (grid import/export, PV generation, load consumption)
- Voltage measurements
- Temperature readings
- Any timing-related observations

**Photos/Oscilloscope traces**
If applicable, add photos of:
- Hardware setup
- Oscilloscope traces showing waveforms
- Multimeter readings
- LED behavior patterns

**Additional context**
- Any recent changes to the installation
- Environmental conditions (temperature, weather)
- Any error patterns (e.g. happens only at certain times, under specific conditions)
- Other systems that might interfere (e.g. other smart devices, power optimizers)
