#!/bin/sh
# The make targets of one CI job, when the scenarios of a set are spread over several jobs.
#
#   ci_targets.sh grid <k>/<n>     scenarios/*.scn, each on the three boards (check-, check-new-,
#                                  check-new20-): the boards of a scenario stay in the same job,
#                                  which can then compare them
#   ci_targets.sh relays <k>/<n>   scenarios/relays/*.scn
#
# Job k of n takes every n-th scenario, in alphabetical order, starting from the k-th. The
# scenarios are found here, as the Makefile finds them: a new one is always run by some job.
set -eu

here=$(dirname "$0")
set_name=$1
k=${2%/*}
n=${2#*/}

case $set_name in
  grid)   dir=$here/scenarios ;;
  relays) dir=$here/scenarios/relays ;;
  *) echo "ci_targets.sh: unknown set '$set_name'" >&2; exit 1 ;;
esac

names=$(for f in "$dir"/*.scn; do basename "$f" .scn; done | grep -vx common | awk -v k="$k" -v n="$n" '(NR - k) % n == 0')
if [ -z "$names" ]; then
  echo "ci_targets.sh: no scenario for job $k of $n in $dir" >&2
  exit 1
fi

for name in $names; do
  case $set_name in
    grid)   printf 'check-%s check-new-%s check-new20-%s\n' "$name" "$name" "$name" ;;
    relays) printf 'check-relays-%s\n' "$name" ;;
  esac
done | tr '\n' ' '
echo
