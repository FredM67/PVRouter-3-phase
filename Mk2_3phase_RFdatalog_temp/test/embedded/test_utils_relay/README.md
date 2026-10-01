# Relay Control Tests

Tests for relay control logic including timing constraints and EWMA filtering.

## Tests

| Test | Description |
|------|-------------|
| `test_relay_initialization*` | Relay object initialization |
| `test_get_*` | Getters (pin, thresholds, minON, minOFF) |
| `test_isRelayON` | Relay state query |
| `test_relay_turnON/OFF` | Relay activation/deactivation |
| `test_relay_override_*` | Override behavior |
| `test_proceed_relay` | Relay state machine progression |
| `test_settle_change_*` | Settling time behavior |
| `test_relay_ordering_*` | Turn ON/OFF order (ascending/descending) |
| `test_duration_overflow_*` | Duration counter overflow protection |
| `test_forced_only_*` | Diversion disabled: a relay is ON only while forced |
| `test_diversion_disabled_*` | Diversion control (diversionEnabled parameter) |

## Running

```bash
pio test -e uno -f "*test_utils_relay*"
```
