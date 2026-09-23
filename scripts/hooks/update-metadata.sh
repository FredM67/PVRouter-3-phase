#!/usr/bin/env bash
#
# Keep the @date / @copyright fields of Doxygen file headers up to date.
#
# Run as a pre-commit hook over the staged C/C++ files. For each file:
#
#   - if the staged change touches anything other than the @date / @copyright
#     lines, stamp @date with today's date and extend @copyright to this year;
#   - if the staged change consists of nothing BUT those lines, restore them to
#     what HEAD already has and leave the file alone.
#
# The second rule is the point of this script. Stamping unconditionally means a
# file that is merely opened and re-saved, or picked up by a `git add -A`, lands
# in the commit as a one-line date bump - noise that pads diffs and produces
# conflicts between long-lived branches for no benefit.
#
# Newly added files have no HEAD version to compare against and are always
# stamped.

set -euo pipefail

CURRENT_DATE=$(date +%Y-%m-%d)
CURRENT_YEAR=$(date +%Y)

# Lines in a unified diff that only adjust the metadata we manage here.
readonly METADATA_LINE_RE='^[+-][[:space:]]*\*[[:space:]]*@(date [0-9]{4}-[0-9]{2}-[0-9]{2}|copyright Copyright \(c\) [0-9]{4}(-[0-9]{4})?)[[:space:]]*$'

# Rewrite the metadata fields of a file in place.
stamp_metadata() {
  local file=$1

  sed -i -E "s/@date [0-9]{4}-[0-9]{2}-[0-9]{2}/@date $CURRENT_DATE/" "$file"
  # A bare year becomes a range; an existing range gets its end year moved up.
  sed -i -E "s/@copyright Copyright \(c\) ([0-9]{4})\$/@copyright Copyright (c) \1-$CURRENT_YEAR/" "$file"
  sed -i -E "s/@copyright Copyright \(c\) ([0-9]{4})-[0-9]{4}/@copyright Copyright (c) \1-$CURRENT_YEAR/" "$file"
}

# True when the staged diff contains at least one change that is not a metadata
# line, i.e. the file was edited for a real reason.
has_substantive_change() {
  local file=$1

  git diff --cached --unified=0 -- "$file" \
    | grep -E '^[+-]' \
    | grep -vE '^(\+\+\+|---)' \
    | grep -qvE "$METADATA_LINE_RE"
}

# Put the file's metadata lines back to the values recorded in HEAD.
restore_metadata_from_head() {
  local file=$1
  local head_date head_copyright

  head_date=$(git show "HEAD:$file" | grep -m1 -oE '@date [0-9]{4}-[0-9]{2}-[0-9]{2}' || true)
  head_copyright=$(git show "HEAD:$file" | grep -m1 -oE '@copyright Copyright \(c\) [0-9]{4}(-[0-9]{4})?' || true)

  [ -n "$head_date" ] && sed -i -E "s/@date [0-9]{4}-[0-9]{2}-[0-9]{2}/$head_date/" "$file"
  [ -n "$head_copyright" ] && sed -i -E "s/@copyright Copyright \(c\) [0-9]{4}(-[0-9]{4})?/$head_copyright/" "$file"

  return 0
}

changed=0

for file in "$@"; do
  [ -f "$file" ] || continue
  grep -qE '@date|@copyright' "$file" || continue

  if git cat-file -e "HEAD:$file" 2>/dev/null && ! has_substantive_change "$file"; then
    # Metadata-only change: drop it rather than commit a pure date bump.
    restore_metadata_from_head "$file"
  else
    stamp_metadata "$file"
  fi

  if ! git diff --quiet -- "$file"; then
    echo "  metadata updated: $file"
    changed=1
  fi
done

# Like clang-format, we rewrite files rather than staging them ourselves -
# `git add` from inside a hook interferes with pre-commit's stash handling.
# Failing here lets the user look at the result and commit again.
if [ "$changed" -ne 0 ]; then
  echo "Headers were rewritten. Review, re-stage and commit again."
  exit 1
fi

exit 0
