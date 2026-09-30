#!/bin/bash
# Builds the RemoteLoadReceiver firmware once per remote unit, for grid_sim -r.
#
#   build_receivers.sh <out dir> <node ID>...
#
# REMOTE_NODE_ID is a compile-time constant of config_rf.h: it is set for each
# build, and config_rf.h is restored afterwards.
set -euo pipefail

out=$(realpath -m "$1")
shift
here=$(dirname "$(realpath "$0")")
receiver="$here/../RemoteLoadReceiver"
config="$receiver/config_rf.h"

mkdir -p "$out"
backup=$(mktemp)
cp "$config" "$backup"
trap 'cp "$backup" "$config"; rm -f "$backup"' EXIT

for node in "$@"; do
  sed -i "s/REMOTE_NODE_ID{ [0-9]* }/REMOTE_NODE_ID{ $node }/" "$config"
  grep -q "REMOTE_NODE_ID{ $node }" "$config" || { echo "cannot set REMOTE_NODE_ID in $config" >&2; exit 1; }
  PLATFORMIO_BUILD_DIR="$receiver/.pio/sim-$node" pio run --silent --project-dir "$receiver" --environment uno
  cp "$receiver/.pio/sim-$node/uno/firmware.elf" "$out/receiver-$node.elf"
  echo "receiver for node $node: $out/receiver-$node.elf"
done
