# Contributing

Thank you for your interest in the project. Read the [Code of Conduct](./CODE_OF_CONDUCT.md) first.

The code standards (style, types, real-time rules, tests) are in
[`Mk2_3phase_RFdatalog_temp/docs/contributing.md`](Mk2_3phase_RFdatalog_temp/docs/contributing.md).
This page covers the workflow.

## Branches

| Branch | Purpose |
|--------|---------|
| `main` | Stable releases (default branch) |
| `dev` | Integration of the next release |

Both are protected by GitHub rulesets: nobody pushes to them directly. The `no-commit-to-branch`
pre-commit hook also refuses local commits on `main` and `dev`.

Work on a topic branch made from `dev`, named after the kind of change: `feat/…`, `fix/…`,
`docs/…`, `ci/…`, … Personal branches go under `users/`.

```bash
git checkout dev
git pull origin dev
git checkout -b feat/my-feature
```

## Commits

Use [conventional commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `docs:`,
`test:`, `ci:`, `refactor:`, `chore:`. A `!` (for example `feat!:`) marks a change that breaks
existing settings.

## Pull requests

Pull requests target `dev`, never `main`:

```bash
git push -u origin feat/my-feature
gh pr create --base dev --title "feat: my feature"
```

Fill in the [pull request template](.github/pull_request_template.md) and link the issue you fix,
if any. CI builds the firmware, runs the tests and checks the formatting.

A release is a pull request from `dev` to `main`.

## Pre-commit hooks

The hooks are described in [`scripts/README.md`](scripts/README.md). Install them once per clone
with the [pre-commit](https://pre-commit.com/) framework, configured by `.pre-commit-config.yaml`:

```bash
pip install pre-commit
pre-commit install
```

On each commit they:

- refuse a commit on `main` or `dev`;
- format C/C++ files with clang-format (`.clang-format`);
- refresh the `@date` / `@copyright` fields of the Doxygen file headers
  (`scripts/hooks/update-metadata.sh`), only when the change touches other lines too;
- rebuild the [graphify](scripts/README.md#code-graph-graphify-optional) code graph
  `graphify-out/graph.json` when code changes (skipped when graphify is not installed);
- fix trailing whitespace, final newlines and line endings (LF), check YAML files and refuse files
  above 500 KB.

To run all hooks on all files:

```bash
pre-commit run --all-files
```

If a hook modifies files, add them again and commit again.

Without Python, `./scripts/install-hooks.sh` (Linux, macOS, Git Bash) or `scripts\install-hooks.bat`
(Windows command prompt) installs a plain git hook that only runs clang-format and the
`@date` / `@copyright` update.
