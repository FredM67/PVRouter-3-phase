#!/usr/bin/env bash
#
# Rebuild the shared code graph, graphify-out/graph.json, and stage it with the commit.
#
# The graph is code-only (local AST, no LLM, no API key), so any machine rebuilds it the
# same way; PYTHONHASHSEED=0 keeps the community ids stable across machines. The hook does
# nothing when graphify is not installed, when GRAPHIFY_SKIP_HOOK=1, or when the clone has
# no graph yet: a contributor without graphify commits as before, and the graph catches up
# on the next commit made with it. See scripts/README.md.

[ "${GRAPHIFY_SKIP_HOOK:-0}" = 1 ] && exit 0
command -v graphify >/dev/null 2>&1 || exit 0
[ -f graphify-out/graph.json ] || exit 0

if ! PYTHONHASHSEED=0 graphify update . >/dev/null 2>&1; then
  echo "graphify update failed: run 'graphify update .' to see why, or commit with GRAPHIFY_SKIP_HOOK=1" >&2
  exit 1
fi
git add graphify-out/graph.json
