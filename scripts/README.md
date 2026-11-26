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

3. **Code graph** ([graphify](https://docs.graphify.com/cookbooks/shared-team-graph))
   - Rebuilds `graphify-out/graph.json`, the code-only graph of the repository, and stages it,
     when the commit changes code (C/C++, Python, JavaScript, shell, YAML, JSON)
   - Skips when graphify is not installed, or with `GRAPHIFY_SKIP_HOOK=1`

## Code graph (graphify, optional)

`graphify-out/graph.json` is a [graphify](https://docs.graphify.com/cookbooks/shared-team-graph) graph of
the code: functions, classes, files and the calls and includes between them, built locally from the
syntax tree (no LLM, no API key). The pre-commit hook keeps it up to date for those who have graphify
installed; everyone else can ignore it. Everything else in `graphify-out/` is local and ignored.

**Setup, once per clone:**

```bash
uv tool install --python 3.12 graphifyy

# the merge driver: a union merge of graph.json instead of a conflict
PY="$(sed -n '1s/^#!//p' "$(command -v graphify)")"
git config merge.graphify.name "graphify graph.json union merge"
git config merge.graphify.driver "\"$PY\" -m graphify merge-driver %O %A %B"
```

On Windows, `graphify.exe` has no shebang line: set `PY` to the `python.exe` of the graphify
environment instead (`uv tool dir` gives its folder, then `graphifyy\Scripts\python.exe`).

Do not run `graphify hook install`: it rebuilds the graph after each commit, which leaves it as an
uncommitted change. Do not run `graphify update .` either: it merges into `graph.json` and adds the
Markdown files to it. The hook rebuilds the graph from scratch (`graph.json` deleted, then
`PYTHONHASHSEED=0 graphify extract . --code-only`). Any rebuild outside a commit changes `graph.json`,
since it records the commit it was built on; `graphify export html` refreshes the local `graph.html`
from the committed graph without touching it.

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
