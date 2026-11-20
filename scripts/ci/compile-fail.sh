#!/usr/bin/env bash
# Builds the router with settings that must be refused, and checks that the compiler refuses
# each one with the expected static_assert message: a check that lets a wrong setting
# through, or rejects a good one, shows up here instead of on someone's water heater.
#
# Each case in compile-fail/*.sh edits the shipped config*.h with 'replace' (and the
# helpers below), and sets 'expect' to a part of the static_assert message. An empty
# 'expect' means the settings are good and the build must succeed.
#
# Usage: compile-fail.sh [case.sh...]   (default: every case)
# PIO can point to PlatformIO.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
PIO=${PIO:-pio}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

src="$work/Mk2_3phase_RFdatalog_temp"
mkdir -p "$src"
(cd "$repo/Mk2_3phase_RFdatalog_temp" && tar --exclude=./.pio -cf - .) | (cd "$src" && tar -xf -)
mkdir -p "$work/shipped"
cp "$src"/config*.h "$work/shipped/"

# Replaces 'old' by 'new' in config*.h; 'old' must appear exactly once, so a case can't go
# stale silently when the shipped config changes.
replace() {
  local old=$1 new=$2 f content rest found=0 target=
  for f in "$src"/config*.h; do
    content=$(<"$f")
    rest=${content//"$old"/}
    found=$((found + (${#content} - ${#rest}) / ${#old}))
    if [ "$content" != "$rest" ]; then target=$f; fi
  done
  if [ "$found" -ne 1 ]; then
    echo "replace: '$old' found $found times in config*.h, expected once"
    stale=1
    return 0
  fi
  content=$(<"$target")
  printf '%s\n' "${content/"$old"/"$new"}" > "$target"
}

dual_tariff_on() {
  replace 'DUAL_TARIFF{ false }' 'DUAL_TARIFF{ true }'
  replace 'dualTariffPin{ unused_pin }' 'dualTariffPin{ 8 }'
}

temperature_on() {
  replace 'TEMP_SENSOR_PRESENT{ false }' 'TEMP_SENSOR_PRESENT{ true }'
  replace 'temperatureSensing{ unused_pin,' 'temperatureSensing{ 3,'
}

cases=("$@")
[ ${#cases[@]} -eq 0 ] && cases=("$here"/compile-fail/*.sh)

failed=0
for case in "${cases[@]}"; do
  name=$(basename "$case" .sh)
  cp "$work/shipped"/config*.h "$src/"
  expect=
  stale=0
  echo "::group::$name"
  # shellcheck source=/dev/null
  source "$case"
  if [ $stale -ne 0 ]; then
    echo "::error::$name: the case no longer applies to the shipped config"
    failed=1
    echo "::endgroup::"
    continue
  fi
  if "$PIO" run -d "$src" -e basic > "$work/log" 2>&1; then
    built=1
  else
    built=0
  fi
  if [ -z "$expect" ]; then
    if [ $built -eq 1 ]; then
      result="ok: builds"
    else
      cat "$work/log"
      result="FAILED: should build"
    fi
  elif [ $built -eq 1 ]; then
    result="FAILED: built, should be refused with '$expect'"
  elif grep -qF -- "$expect" "$work/log"; then
    result="ok: refused"
  else
    grep -E 'error|static assertion' "$work/log" || cat "$work/log"
    result="FAILED: refused, but without '$expect'"
  fi
  echo "::endgroup::"
  echo "$name: $result"
  case $result in FAILED*)
    echo "::error::$name: ${result#FAILED: }"
    failed=1
    ;;
  esac
done
exit $failed
