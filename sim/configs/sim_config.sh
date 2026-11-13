#!/bin/sh
# Settings the simulated grid needs, whatever the defaults of config.h:
# - the old board: grid_sim feeds the ADC for a 5 V reference (AVCC), and its scenarios use the
#   old board's calibration (scenarios/common.scn: 0.05, as f_powerCalOldBoard in calibration.h);
# - the human-readable output: grid_sim reads its datalog.
#
#   sim/configs/sim_config.sh [config.h]   (default: Mk2_3phase_RFdatalog_temp/config.h)
set -e
cfg=${1:-Mk2_3phase_RFdatalog_temp/config.h}
sed -i -E 's/(PCB_VERSION\{ PcbVersion::)[A-Z]+/\1OLD/; s/(SERIAL_OUTPUT_TYPE = SerialOutputType::)[A-Za-z]+/\1HumanReadable/' "$cfg"
grep -q 'PCB_VERSION{ PcbVersion::OLD }' "$cfg"
grep -q 'SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;' "$cfg"
