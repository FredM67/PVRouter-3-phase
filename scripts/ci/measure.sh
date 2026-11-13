#!/usr/bin/env bash
# The figures of the PR report (scripts/ci/pr_report.py), measured on a source tree: the flash and
# RAM of the builds, and the BENCH| results of the embedded tests (simavr). Used for the target
# branch of a pull request; the PR's own figures come from the build and test-embedded jobs.
#
#   scripts/ci/measure.sh <source tree> <output dir>
#
# Writes <output dir>/size.json and <output dir>/speed.json (customSmallerIsBetter format). A
# tree without BENCH| results (older than the benchmark history) gives an empty speed.json.
#
# The builds must stay those of the build matrix of .github/workflows/build.yml, with the same
# labels: the report pairs them by name.
set -euo pipefail

tree=$(realpath "$1")
out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
project="$tree/Mk2_3phase_RFdatalog_temp"
avr_size="$HOME/.platformio/packages/toolchain-atmelavr/bin/avr-size"
mkdir -p "$out"

# one build: its flash and RAM as two JSON results
measure() {
  local env=$1 label=$2
  pio run --project-dir "$project" -e "$env" -s
  local text data bss
  read -r text data bss _ < <("$avr_size" "$project/.pio/build/$env/firmware.elf" | tail -n 1)
  printf '{"name": "flash: %s", "value": %d, "unit": "bytes"}\n' "$label" $((text + data))
  printf '{"name": "RAM: %s", "value": %d, "unit": "bytes"}\n' "$label" $((data + bss))
}

{
  measure basic basic
  measure rf rf
  cp "$project/config.h" "$out/config.h.orig"
  for serial in IoT HumanReadable; do
    sed -i -E "s/SERIAL_OUTPUT_TYPE = SerialOutputType::[A-Za-z]+;/SERIAL_OUTPUT_TYPE = SerialOutputType::$serial;/" "$project/config.h"
    measure basic "basic, $serial"
    cp "$out/config.h.orig" "$project/config.h"
  done
} | jq -s . > "$out/size.json"

if ! pio test --project-dir "$project" -e uno_sim -v > "$out/embedded-tests.log" 2>&1; then
  echo "::warning::embedded tests of $tree failed, its speed figures may be incomplete"
fi
if ! python3 "$here/bench_to_json.py" "$out/embedded-tests.log" > "$out/speed.json"; then
  echo "[]" > "$out/speed.json"
fi
