#!/bin/bash
#
# Update @date and @copyright fields in C++ source files
#
# This script is called by pre-commit framework with a list of files
# to process. It updates:
# - @date fields to current date (YYYY-MM-DD format)
# - @copyright year to include current year
#

CURRENT_DATE=$(date +%Y-%m-%d)
CURRENT_YEAR=$(date +%Y)

# Exit with success if no files provided
if [ $# -eq 0 ]; then
  exit 0
fi

# Process each file passed as argument
for file in "$@"; do
  if [ -f "$file" ]; then
    # Check if file contains @date or @copyright fields
    if grep -q '@date\|@copyright' "$file"; then

      # Update @date field with current date
      sed -i "s/@date [0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}/@date $CURRENT_DATE/" "$file"

      # Update copyright year
      # Case 1: Single year (e.g., "Copyright (c) 2021") -> add current year as range
      sed -i "s/@copyright Copyright (c) \([0-9]\{4\}\)$/@copyright Copyright (c) \1-$CURRENT_YEAR/" "$file"

      # Case 2: Year range (e.g., "Copyright (c) 2021-2023") -> update end year
      sed -i "s/@copyright Copyright (c) \([0-9]\{4\}\)-[0-9]\{4\}/@copyright Copyright (c) \1-$CURRENT_YEAR/" "$file"
    fi
  fi
done

exit 0
