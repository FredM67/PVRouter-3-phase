#!/bin/sh
# The calibration of a board, as grid_sim settings, read from calibration.h: the simulator must
# generate the voltages and powers the firmware reads back with the same values.
#
#   board_scn.sh OLD|NEW [calibration.h] > board.scn      then: grid_sim -b board.scn ...
#
# OLD: vcal = f_voltageCalOldBoard, pcal = f_powerCalOldBoard
# NEW: vcal = f_voltageCalNewBoard, pcal = f_powerCalPerAmp x f_ctRating
set -e
board=$1
cal=${2:-../Mk2_3phase_RFdatalog_temp/calibration.h}

# the 3 values of an array, from its definition line (commented-out lines are skipped)
values() {
  v=$(sed -n "s/^inline constexpr float $1\[NO_OF_PHASES\]{\([^}]*\)}.*/\1/p" "$cal" | tr -d 'F ' | tr ',' ' ')
  [ "$(echo $v | wc -w)" -eq 3 ] || { echo "board_scn.sh: no 3 values for $1 in $cal" >&2; exit 1; }
  echo $v
}

echo "# $board board, from $cal"
case $board in
OLD)
  echo "vcal $(values f_voltageCalOldBoard)"
  echo "pcal $(values f_powerCalOldBoard)"
  ;;
NEW)
  echo "vcal $(values f_voltageCalNewBoard)"
  echo "$(values f_powerCalPerAmp) $(values f_ctRating)" |
    awk '{ printf "pcal %.9g %.9g %.9g\n", $1 * $4, $2 * $5, $3 * $6 }'
  ;;
*)
  echo "usage: board_scn.sh OLD|NEW [calibration.h]" >&2
  exit 2
  ;;
esac
