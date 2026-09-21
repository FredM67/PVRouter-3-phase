# Teleinfo Tests

Tests for French Teleinfo protocol parsing (electricity meter data).

## Tests

| Test | Description |
|------|-------------|
| `test_lineSize_calculation` | Verifies line buffer size calculation |
| `test_calcBufferSize_compile_time` | Tests compile-time buffer sizing |
| `test_teleinfo_instantiation` | Tests object creation |
| `test_teleinfo_basic_operations` | Basic read/parse operations |
| `test_teleinfo_edge_values` | Edge case handling |
| `test_teleinfo_multiple_frames` | Multiple frame processing |
| `test_teleinfo_long_sequences` | Long data sequence handling |

## Running

```bash
pio test -e uno -f "*test_teleinfo*"
```
