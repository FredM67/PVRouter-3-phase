#!/bin/sh
# The same scenario must give the same result on both boards: compares the summaries of
# 'make check' (old board) and 'make check-new' (new board), scenario by scenario.
#
#   compare_boards.sh [out dir]   (default: out)
#
# Compared: grid import and export, in total and per phase, and the energy of each load.
# Allowed difference: 1 % or 0.1 Wh, whichever is larger. The boards differ in resolution only
# (fewer ADC steps per watt and per volt on the new board), which can move a switching
# instant by a half-cycle now and then: a few hundredths of a Wh over a scenario.
out=${1:-out}
status=0
found=0
for new in "$out"/new-*-summary.txt; do
  [ -e "$new" ] || break
  found=1
  name=$(basename "$new" -summary.txt)
  name=${name#new-}
  old="$out/$name-summary.txt"
  if [ ! -e "$old" ]; then
    echo "$name: no old-board summary ($old): run make check"
    status=1
    continue
  fi
  awk -v name="$name" '
    # figures of a summary: grid import/export (total and per phase), energy of each load
    function grab(  i, n, p, v) {
      if ($1 == "Grid") {
        fig["grid import"] = $3; fig["grid export"] = $6
        for (i = 8; i <= NF; i += 2) {      # (L1 5.51/4.81, L2 ...
          p = $i; sub(/^\(/, "", p)
          v = $(i + 1); sub(/[,)]$/, "", v); split(v, n, "/")
          fig[p " import"] = n[1]; fig[p " export"] = n[2]
        }
      } else if ($1 == "Load" || $1 == "Relay") {
        for (i = 3; i < NF; ++i)
          if ($(i + 1) == "Wh,") { fig[$1 " " $2] = $i; break }
      }
    }
    FNR == 1 { file++ }
    { delete fig; grab(); for (k in fig) val[file, k] = fig[k]; if (file == 1) for (k in fig) keys[k] = 1 }
    END {
      bad = 0
      for (k in keys) {
        a = val[1, k]; b = val[2, k]
        if (b == "") { printf "%s: %s missing on the new board\n", name, k; bad = 1; continue }
        d = a - b; if (d < 0) d = -d
        m = a > b ? a : b; tol = m * 0.01; if (tol < 0.1) tol = 0.1
        if (d > tol) { printf "%s: %s %.2f Wh (old) / %.2f Wh (new), off by %.2f Wh\n", name, k, a, b, d; bad = 1 }
      }
      printf "%s: %s\n", name, bad ? "the boards DIFFER" : "same result on both boards"
      exit bad
    }' "$old" "$new" || status=1
done
if [ $found = 0 ]; then
  echo "no new-board summary in $out: run make check-new"
  exit 1
fi
exit $status
