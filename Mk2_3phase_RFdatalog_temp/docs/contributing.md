# Contributing Guide

Welcome to the PVRouter project! This guide will help you contribute effectively to the codebase while maintaining the high standards of safety, reliability, and performance that are essential for power electronics applications.

## Project Overview

The PVRouter is a safety-critical embedded system that controls electrical power routing in real-time. Contributions must prioritize:

1. **Safety** - No changes that could cause electrical hazards
2. **Reliability** - Code must work consistently in production environments
3. **Performance** - Real-time constraints must be maintained
4. **Maintainability** - Code should be readable and well-documented

## Getting Started

### Development Environment Setup

#### Option 1: PlatformIO (Recommended)
```bash
# Install PlatformIO Core
pip install platformio

# Clone repository
git clone https://github.com/FredM67/PVRouter-3-phase.git
cd PVRouter-3-phase/Mk2_3phase_RFdatalog_temp

# Build and test
pio run
pio test
```

#### Option 2: Arduino IDE
1. Install Arduino IDE 1.8.19 or newer
2. Install required libraries:
   - OneWire (Paul Stoffregen)
   - DallasTemperature (Miles Burton)
3. Open `Mk2_3phase_RFdatalog_temp.ino`

### Hardware Requirements for Testing
- Arduino Uno or compatible board
- 3-phase PVRouter shield (for full testing)
- Oscilloscope (for timing verification)
- Multimeter (for power measurements)

## Code Standards

### C++ Guidelines

#### Modern C++ Features (C++17)
```cpp
// ✅ Good: Use constexpr for compile-time constants
constexpr uint8_t MAX_LOADS = 6;

// ✅ Good: Use auto for type deduction
auto sampleRate = calculateSampleRate();

// ✅ Good: Use range-based for loops
for (const auto& load : loadConfigs) {
  processLoad(load);
}

// ❌ Avoid: C-style casts
float value = (float)intValue;  // Bad
float value = static_cast<float>(intValue);  // Good
```

#### Memory Management
```cpp
// ✅ Good: Stack allocation preferred
int16_t samples[BUFFER_SIZE];

// ❌ Avoid: Dynamic allocation in embedded code
int16_t* samples = malloc(sizeof(int16_t) * size);  // Forbidden

// ✅ Good: Use fixed-size containers
std::array<float, NO_OF_PHASES> powerReadings;
```

#### Type Safety
```cpp
// ✅ Good: Use enum class for type safety
enum class LoadState : uint8_t {
  OFF = 0,
  ON = 1,
  TRANSITIONING = 2
};

// ✅ Good: Use explicit constructors
class PowerReading {
  explicit PowerReading(float watts) : watts_(watts) {}
private:
  float watts_;
};

// ❌ Avoid: Magic numbers
delay(1000);  // Bad
delay(STARTUP_DELAY_MS);  // Good
```

### Coding Style

#### Naming Conventions
```cpp
// Variables and functions: camelCase
int sampleCount;
void processAdcReading();

// Constants: UPPER_CASE
constexpr uint16_t MAX_ADC_VALUE = 1023;

// Classes and types: PascalCase
class PowerCalculator;
enum class OperationMode;

// Private members: trailing underscore
class MyClass {
private:
  int privateMember_;
};
```

#### File Organization
```cpp
// File header template
/**
 * @file filename.h
 * @brief Brief description
 * @author Your Name (your.email@domain.com)
 * @date YYYY-MM-DD
 * 
 * Detailed description of the file's purpose and functionality.
 */

#ifndef CONTRIBUTING_H
#define CONTRIBUTING_H

// Include order:
// 1. Standard library
#include <stdint.h>

// 2. Arduino/platform headers
#include <Arduino.h>

// 3. Project headers
#include "config.h"
#include "types.h"

// Implementation...

#endif /* CONTRIBUTING_H */
```

## Safety Requirements

### Critical Code Sections

#### Interrupt Service Routines
```cpp
// ISR must be:
// - Minimal in duration (<50μs)
// - Atomic in operation
// - Free of blocking calls

ISR(ADC_vect) {
  // ✅ Good: Essential operations only
  rawSample = ADC;
  processRawSample(rawSample);
  
  // ❌ Forbidden in ISR:
  // Serial.print()    - Blocking I/O
  // delay()           - Blocking delay
  // malloc()          - Dynamic allocation
  // digitalWrite()    - Unless absolutely necessary
}
```

#### Load Control Logic
```cpp
// All load control must include safety checks
void activateLoad(uint8_t loadIndex) {
  // Validate input parameters
  if (loadIndex >= NO_OF_DUMPLOADS) {
    logError("Invalid load index");
    return;
  }
  
  // Check system state
  if (systemInFaultState()) {
    logError("Cannot activate load - system fault");
    return;
  }
  
  // Check power limits
  if (getCurrentPower() > MAX_SAFE_POWER) {
    logError("Power limit exceeded");
    return;
  }
  
  // Proceed with activation
  setLoadState(loadIndex, LoadState::ON);
}
```

### Testing Requirements

#### Unit Tests (Required for Safety-Critical Code)
```cpp
// Example test for power calculation
void test_powerCalculation() {
  // Setup known conditions
  float voltage = 230.0;  // RMS voltage
  float current = 10.0;   // RMS current
  float expectedPower = 2300.0;  // Expected power
  
  // Test calculation
  float calculatedPower = calculatePower(voltage, current);
  
  // Verify result within tolerance
  assert(abs(calculatedPower - expectedPower) < 0.1);
}
```

#### Hardware-in-the-Loop Testing
- All GPIO control code must be tested on actual hardware
- Power calculations must be verified with calibrated instruments
- Timing requirements must be validated with oscilloscope

## Contribution Workflow

### 1. Issue Creation
Before starting work, create or comment on an issue describing:
- Problem statement or feature request
- Proposed solution approach
- Potential safety implications
- Testing strategy

### 2. Branch Strategy
```bash
# Create feature branch
git checkout -b feature/description-of-change

# Keep commits focused and atomic
git commit -m "Add bounds checking for temperature sensors"
git commit -m "Update documentation for new safety feature"
```

### 3. Development Process

#### Code Changes
1. **Implement** the change following coding standards
2. **Add tests** for new functionality
3. **Update documentation** for any API changes
4. **Verify safety** implications have been addressed

#### Compilation Requirements
```bash
# Must compile without warnings
pio run

# Must pass all tests
pio test

# Must fit memory constraints
# Check memory usage after compilation
```

### 4. Pull Request Guidelines

#### PR Description Template
```markdown
## Description
Brief description of the change and why it's needed.

## Type of Change
- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Breaking change (fix or feature that changes existing behavior)
- [ ] Documentation update

## Safety Impact Assessment
- [ ] No safety implications
- [ ] Safety implications reviewed and mitigated
- [ ] Requires safety review by maintainer

## Testing
- [ ] Unit tests added/updated
- [ ] Hardware testing completed
- [ ] Performance impact measured
- [ ] Documentation updated

## Hardware Tested
- [ ] Arduino Uno
- [ ] ESP32 (if applicable)
- [ ] Full 3-phase hardware setup

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex logic
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Memory usage verified
```

### 5. Review Process

#### Automated Checks
- Code compilation (all platforms)
- Static analysis (cppcheck, clang-tidy)
- Memory usage verification
- Performance regression tests

#### Human Review Focus Areas
1. **Safety**: Could this change cause electrical hazards?
2. **Performance**: Does this maintain real-time requirements?
3. **Correctness**: Is the logic sound and properly tested?
4. **Style**: Does it follow project conventions?
5. **Documentation**: Are changes properly documented?

## Specific Areas for Contribution

### High-Priority Areas
1. **Test Coverage** - Expand unit test suite
2. **Documentation** - Improve user guides and technical docs
3. **Hardware Support** - Support for new MCU platforms
4. **Performance** - Optimization of critical paths
5. **Safety Features** - Additional fault detection and protection

### Configuration Examples
Help create configuration examples for:
- Different hardware setups
- Various load types (heat pumps, battery chargers)
- Home automation integration
- Industrial applications

### Feature Requests
Current areas of active development:
- **Web interface** for remote monitoring
- **WiFi configuration** for ESP32 platforms
- **Advanced algorithms** for load prediction
- **Integration** with home energy management systems

## Code Review Checklist

### For Contributors (Self-Review)
- [ ] Code compiles without warnings on all target platforms
- [ ] All new functions have appropriate documentation
- [ ] Error handling is comprehensive and appropriate
- [ ] Performance impact has been considered
- [ ] Memory usage is within acceptable limits
- [ ] Safety implications have been analyzed

### For Reviewers
- [ ] Code follows project style guidelines
- [ ] Safety-critical sections are properly implemented
- [ ] Test coverage is adequate for the change
- [ ] Documentation is clear and complete
- [ ] Change is backward compatible (if required)
- [ ] Performance requirements are maintained

## Release Process

### Version Numbering
We follow semantic versioning (MAJOR.MINOR.PATCH):
- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality, backward compatible
- **PATCH**: Bug fixes, backward compatible

### Release Checklist
1. All tests pass on target hardware
2. Documentation is updated
3. Example configurations are verified
4. Performance benchmarks are acceptable
5. Safety review is completed (for safety-critical changes)

## Getting Help

### Communication Channels
- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and ideas
- **Email**: For security-related issues

### Documentation Resources
- **README.md**: Quick start guide
- **docs/**: Technical documentation
- **examples/**: Configuration examples
- **Gallery.md**: Hardware setup photos

### Common Gotchas
1. **Timing sensitivity**: ADC ISR must complete within 104μs
2. **Memory constraints**: Arduino Uno has only 2KB RAM
3. **Power safety**: All load control must include safety checks
4. **Calibration**: Hardware must be properly calibrated before use

Thank you for contributing to the PVRouter project! Your contributions help make sustainable energy systems more accessible and reliable.