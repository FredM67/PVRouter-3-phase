# Scripts

This directory contains utility scripts for the PVRouter-3-phase project.

## Git Hooks

This project provides **two methods** for managing git hooks. Choose the one that fits your workflow:

### Method 1: Pre-commit Framework (Recommended)

Uses the industry-standard [pre-commit](https://pre-commit.com/) framework.

**Setup:**
```bash
# Install pre-commit (one-time setup)
pip install pre-commit

# Install the hooks
pre-commit install
```

**Features:**
- Automatic clang-format on staged files
- Automatic @date and @copyright updates
- Managed by `.pre-commit-config.yaml`
- Part of a larger ecosystem of hooks

**Update hooks:**
```bash
pre-commit autoupdate
```

### Method 2: Manual Hook Installation

Simple bash-based hooks without external dependencies.

**Setup:**

**Linux/macOS/Git Bash:**
```bash
./scripts/install-hooks.sh
```

**Windows Command Prompt:**
```cmd
scripts\install-hooks.bat
```

**Features:**
- No Python dependencies
- Self-contained bash scripts
- Same functionality as Method 1

---

## What the Pre-commit Hook Does

Regardless of which method you choose, the pre-commit hook performs:

1. **Code Formatting** (clang-format)
   - Uses the `.clang-format` configuration in the project root
   - Formats all staged `.cpp` and `.h` files
   - Skips gracefully if clang-format is not available

2. **Metadata Updates**
   - Updates `@date` fields to the current date (YYYY-MM-DD format)
   - Updates `@copyright` year ranges to include the current year

## Installing clang-format (Optional but Recommended)

**MSYS2 (recommended for this project):**
```bash
pacman -S mingw-w64-x86_64-clang-tools-extra
```

**Chocolatey (Windows):**
```bash
choco install llvm
```

**Linux (apt):**
```bash
sudo apt install clang-format
```

**macOS (Homebrew):**
```bash
brew install clang-format
```

## Hook Development

### For pre-commit framework:
1. Edit `.pre-commit-config.yaml` in the project root
2. Update local scripts in `scripts/hooks/`

### For manual hooks:
1. Edit hook files in `scripts/git-hooks/`
2. Re-run the installation script to update your local `.git/hooks/`
