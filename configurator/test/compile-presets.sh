#!/usr/bin/env bash
# Generates each preset into a copy of the firmware and builds it: the compiler's
# static_asserts are the ground truth for what the configurator accepts.
#
# Usage: compile-presets.sh [preset.json...]   (default: every preset)
# NODE, PIO and CLANG_FORMAT can point to the tools; without clang-format, the format check
# is skipped.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
NODE=${NODE:-node}
PIO=${PIO:-pio}
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$here/../../.clang-format" "$work/"  # for the receivers, which have none of their own

presets=("$@")
[ ${#presets[@]} -eq 0 ] && presets=("$here"/presets/*.json)

failed=0
for preset in "${presets[@]}"; do
  name=$(basename "$preset" .json)
  out="$work/$name"
  echo "::group::$name"
  if ! "$NODE" "$here/generate-preset.js" "$preset" "$out"; then
    echo "$name: rejected by the configurator"
    failed=1
    echo "::endgroup::"
    continue
  fi
  # the generated files must already be formatted
  if command -v "$CLANG_FORMAT" > /dev/null; then
    for f in "$out"/Mk2_3phase_RFdatalog_temp/config*.h "$out"/RemoteLoadReceiver-unit*/config*.h; do
      [ -f "$f" ] || continue
      if ! (cd "$(dirname "$f")" && "$CLANG_FORMAT" --dry-run --Werror "$(basename "$f")"); then
        echo "$name: $(basename "$(dirname "$f")")/$(basename "$f") is not clang-format clean"
        failed=1
      fi
    done
  fi
  "$PIO" run -d "$out/Mk2_3phase_RFdatalog_temp" -e basic || { echo "$name: router build failed"; failed=1; }
  for unit in "$out"/RemoteLoadReceiver-unit*; do
    [ -d "$unit" ] || continue
    "$PIO" run -d "$unit" || { echo "$name: $(basename "$unit") build failed"; failed=1; }
  done
  echo "::endgroup::"
done
exit $failed
