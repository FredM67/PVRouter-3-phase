# Cloud pattern visualization

`visualize_cloud_patterns_demo.py` helps to choose `RELAY_FILTER_DELAY` in `config.h`: on two
simulated cloud scenarios, it plots the TEMA average the relays decide on, for delays of 1 to 5
minutes, and counts the relay switches.

```bash
cd Mk2_3phase_RFdatalog_temp
python3 scripts/visualize_cloud_patterns_demo.py
```

It needs Python 3 with matplotlib and numpy, and writes two files in the current directory:

- `cloud_pattern_analysis.png`: the simulated power (gray), the TEMA of each delay, the relay
  state and the cloud periods;
- `relay_switching_summary.png`: the number of relay switches for each delay.

The TEMA follows `ewma_avg.hpp`, with one value per 5 s datalog line. The relay is simplified: ON
from 1100 W, OFF from 900 W down, with no minimum ON/OFF time and no settle time. To check a
setting against the real firmware, use the relay scenarios of the grid simulator
([`sim/README.md`](../../sim/README.md#relays)).

Then set the delay in `config.h`:

```cpp
inline constexpr uint8_t RELAY_FILTER_DELAY{ 3 };
```

See [`docs/Cloud_Pattern_Tuning_Guide.md`](../../docs/Cloud_Pattern_Tuning_Guide.md) for the
choice by climate.
