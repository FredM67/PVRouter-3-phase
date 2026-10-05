#!/usr/bin/env bash
# Runs 'esphome config' on the YAML generated for each preset with the mk2Wifi module.
#
# Usage: check-yaml.sh [preset.json...]   (default: every preset)
# NODE and ESPHOME can point to the tools.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
NODE=${NODE:-node}
ESPHOME=${ESPHOME:-esphome}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

presets=("$@")
[ ${#presets[@]} -eq 0 ] && presets=("$here"/presets/*.json)

failed=0
for preset in "${presets[@]}"; do
  name=$(basename "$preset" .json)
  out="$work/$name"
  "$NODE" "$here/generate-preset.js" "$preset" "$out" > /dev/null
  for yaml in "$out"/*.yaml; do
    [ -f "$yaml" ] || continue
    # dummy secrets: only the structure is checked
    cat > "$out/secrets.yaml" << EOF
api_encryption_key: "$(head -c 32 /dev/urandom | base64)"
esphome_ota_password: "ota-password"
wifi_ssid: "ssid"
wifi_password: "wifi-password"
wifi_fallback_password: "fallback-password"
esphome_web_server_username: "admin"
esphome_web_server_password: "web-password"
EOF
    echo "::group::$name: $(basename "$yaml")"
    if ! "$ESPHOME" config "$yaml" > "$out/esphome.log" 2>&1; then
      cat "$out/esphome.log"
      echo "$name: $(basename "$yaml") is not a valid ESPHome configuration"
      failed=1
    else
      grep -E '^(INFO|WARNING)' "$out/esphome.log" || true
    fi
    echo "::endgroup::"
  done
done
exit $failed
