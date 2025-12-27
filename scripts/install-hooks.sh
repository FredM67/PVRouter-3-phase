#!/bin/bash
#
# Install Git hooks for the PVRouter-3-phase project
#
# This script copies all hooks from scripts/git-hooks/ to .git/hooks/
# and makes them executable.
#
# Usage:
#   ./scripts/install-hooks.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
HOOKS_SOURCE="$SCRIPT_DIR/git-hooks"
HOOKS_DEST="$PROJECT_ROOT/.git/hooks"

# Color codes for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Installing Git hooks..."
echo

# Check if we're in a git repository
if [ ! -d "$PROJECT_ROOT/.git" ]; then
  echo "Error: Not in a Git repository root"
  exit 1
fi

# Check if hooks source directory exists
if [ ! -d "$HOOKS_SOURCE" ]; then
  echo "Error: Hooks source directory not found: $HOOKS_SOURCE"
  exit 1
fi

# Install each hook
INSTALLED_COUNT=0
for hook in "$HOOKS_SOURCE"/*; do
  if [ -f "$hook" ]; then
    hook_name=$(basename "$hook")
    dest_hook="$HOOKS_DEST/$hook_name"

    # Copy the hook
    cp "$hook" "$dest_hook"

    # Make it executable
    chmod +x "$dest_hook"

    echo -e "${GREEN}✓${NC} Installed: $hook_name"
    INSTALLED_COUNT=$((INSTALLED_COUNT + 1))
  fi
done

echo
if [ $INSTALLED_COUNT -eq 0 ]; then
  echo -e "${YELLOW}Warning: No hooks found to install${NC}"
  exit 0
fi

echo -e "${GREEN}Successfully installed $INSTALLED_COUNT hook(s)${NC}"
echo
echo "Hooks installed:"
echo "  • pre-commit: Formats code with clang-format and updates @date/@copyright"
echo
echo "Note: clang-format is optional but recommended for automatic code formatting."
echo "      Install with: pacman -S mingw-w64-x86_64-clang-tools-extra"
