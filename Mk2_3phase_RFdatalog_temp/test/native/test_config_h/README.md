# config.h Tests

Tests for `config.h` itself. It only holds settings: it includes `config_types.h` and nothing else, so it builds on the PC, without Arduino and without the rest of the firmware. The targets of its override pins and scheduled boosts (`LOAD()`, `RELAY()`, `ALL_LOADS()`...) are codes; they resolve against the load map and the relays as `config_derived.h` resolves them. The tests hold whatever the values in `config.h`, so an edited `config.h` passes too.

## Tests

| Test | Description |
|------|-------------|
| `test_config_h_reads_on_its_own` | `config.h` builds alone: the load map and the startup priorities have one entry per load, relays and sensors are listed |
| `test_targets_resolve_as_in_config_derived_h` | `TargetMap` built from the settings, as in `config_derived.h`: the override table and the boost schedule have one entry per setting, on the same pins |

## Running

```bash
pio test -e native -f "native/test_config_h"
```
