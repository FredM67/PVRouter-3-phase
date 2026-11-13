#!/bin/sh
# Settings the simulated grid needs, whatever the defaults of config.h:
# - the board: OLD (ADC reference AVCC, 5 V) by default, for scenarios/common.scn and its old-board
#   calibration (0.05, as f_powerCalOldBoard in calibration.h); NEW (internal 1.1 V reference) for
#   'make check-new', which takes the new board's calibration from calibration.h (board_scn.sh);
# - the human-readable output: grid_sim reads its datalog.
#
#   sim/configs/sim_config.sh [OLD|NEW] [config.h]   (default: OLD, Mk2_3phase_RFdatalog_temp/config.h)
set -e
board=OLD
case $1 in OLD | NEW) board=$1; shift ;; esac
cfg=${1:-Mk2_3phase_RFdatalog_temp/config.h}
sed -i -E "s/(PCB_VERSION\{ PcbVersion::)[A-Z]+/\1$board/; s/(SERIAL_OUTPUT_TYPE = SerialOutputType::)[A-Za-z]+/\1HumanReadable/" "$cfg"
grep -q "PCB_VERSION{ PcbVersion::$board }" "$cfg"
grep -q 'SERIAL_OUTPUT_TYPE = SerialOutputType::HumanReadable;' "$cfg"
