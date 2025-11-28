# Scripts

This directory contains utility scripts for the PVRouter-3-phase project.

## Git Hooks

### Installation

After cloning the repository, run the hook installation script:

**Linux/macOS/Git Bash:**
```bash
./scripts/install-hooks.sh
```

**Windows Command Prompt:**
```cmd
scripts\install-hooks.bat
```

### Available Hooks

#### pre-commit

Automatically runs before each commit to:

1. **Format code** with clang-format (if installed)
   - Uses the `.clang-format` configuration in the project root
   - Formats all staged `.cpp` and `.h` files
   - Skips gracefully if clang-format is not available

2. **Update file metadata**
   - Updates `@date` fields to the current date (YYYY-MM-DD format)
   - Updates `@copyright` year ranges to include the current year

### Installing clang-format (Optional)

While optional, clang-format is highly recommended for consistent code formatting.

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

### Manual Hook Updates

If you update hooks in `scripts/git-hooks/`, re-run the installation script to update your local `.git/hooks/` directory.

## Hook Development

To modify or add new hooks:

1. Edit or create hook files in `scripts/git-hooks/`
2. Test the hook by running the install script and making a commit
3. Commit your changes to `scripts/git-hooks/` to share with the team
