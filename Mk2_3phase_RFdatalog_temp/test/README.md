# Test Suite

Unit tests for PVRouter-3-phase firmware.

## Structure

```
test/
├── native/           # Run on host PC (no hardware required)
│   ├── test_dc_offset_filter/
│   ├── test_ewma_avg/
│   ├── test_power_rms_comparison/
│   ├── test_remote_loads/
│   └── test_utils_override/
└── embedded/         # Run on Arduino or Wokwi simulator
    ├── test_fastdivision/
    ├── test_mult_asm/
    ├── test_teleinfo/
    ├── test_utils_pins/
    ├── test_utils_relay/
    └── test_voltage_accumulation/
```

## Running Tests

```bash
# All native tests
pio test -e native

# All embedded tests (requires hardware or Wokwi)
pio test -e uno

# Specific test suite
pio test -e native -f "*test_ewma_avg*"
pio test -e uno -f "*test_utils_relay*"
```

## Test Suites

### Native Tests

| Test | Purpose |
|------|---------|
| [test_dc_offset_filter](native/test_dc_offset_filter/) | DC offset removal filter |
| [test_ewma_avg](native/test_ewma_avg/) | EWMA filter for cloud immunity |
| [test_power_rms_comparison](native/test_power_rms_comparison/) | Real power vs. RMS power computation |
| [test_remote_loads](native/test_remote_loads/) | Packed load map and per-unit remote payloads |
| [test_utils_override](native/test_utils_override/) | Override pin system (local + remote loads) |

### Embedded Tests

| Test | Purpose |
|------|---------|
| [test_fastdivision](embedded/test_fastdivision/) | AVR assembly fast division (divu10, divmod10) |
| [test_mult_asm](embedded/test_mult_asm/) | AVR assembly multiplication optimization |
| [test_teleinfo](embedded/test_teleinfo/) | French Teleinfo protocol parsing |
| [test_utils_pins](embedded/test_utils_pins/) | GPIO pin manipulation |
| [test_utils_relay](embedded/test_utils_relay/) | Relay control logic and timing |
| [test_voltage_accumulation](embedded/test_voltage_accumulation/) | Voltage sample accumulation over a mains cycle |

## CI Integration

- **Native tests**: Run on Ubuntu in GitHub Actions
- **Embedded tests**: Run in Wokwi simulator
- See `.github/workflows/build.yml` for configuration
