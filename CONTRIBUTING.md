# Welcome to GitHub docs contributing guide <!-- omit in toc -->

Thank you for investing your time in contributing to our project! Any contribution you make will be reflected on [docs.github.com](https://docs.github.com/en) :sparkles:.

Read our [Code of Conduct](./CODE_OF_CONDUCT.md) to keep our community approachable and respectable.

In this guide you will get an overview of the contribution workflow from opening an issue, creating a PR, reviewing, and merging the PR.

## New contributor guide

To get an overview of the project, read the [README](README.md). Here are some resources to help you get started with open source contributions:

- [Finding ways to contribute to open source on GitHub](https://docs.github.com/en/get-started/exploring-projects-on-github/finding-ways-to-contribute-to-open-source-on-github)
- [Set up Git](https://docs.github.com/en/get-started/quickstart/set-up-git)
- [GitHub flow](https://docs.github.com/en/get-started/quickstart/github-flow)
- [Collaborating with pull requests](https://docs.github.com/en/github/collaborating-with-pull-requests)


## Branching Strategy

This project uses a two-branch workflow:

| Branch | Purpose | Description |
|--------|---------|-------------|
| `main` | Stable releases | Production-ready code that users download (default branch) |
| `dev` | Development | Active development and testing |

> **Note:** Direct commits to `main` and `dev` are blocked by pre-commit hooks. Always work in feature branches.

### Daily Development Workflow

**Starting a new feature:**

```bash
# Switch to dev and update
git checkout dev
git pull origin dev

# Create feature branch
git checkout -b feature/my-feature-name
```

**Working on your feature:**

```bash
# Make changes, commit as usual
git add .
git commit -m "feat: description of changes"

# Push your feature branch
git push -u origin feature/my-feature-name
```

**Creating a Pull Request:**

```bash
# Create PR targeting dev branch
gh pr create --base dev --title "feat: my feature"

# Or use GitHub UI and change base branch from 'main' to 'dev'
```

### Release Workflow

**When ready to release stable code to users:**

```bash
# Switch to main branch
git checkout main
git pull origin main

# Merge dev into main
git merge dev

# Tag the release
git tag v1.x.x
git push origin main --tags
```

After merging to `main`, users will get the latest stable release when they clone or download the repository.

## Getting started

To navigate our codebase with confidence, see [the introduction to working in the docs repository](/contributing/working-in-docs-repository.md) :confetti_ball:. For more information on how we write our markdown files, see [the GitHub Markdown reference](contributing/content-markup-reference.md).

Check to see what [types of contributions](/contributing/types-of-contributions.md) we accept before making changes. Some of them don't even require writing a single line of code :sparkles:.

### Issues

#### Create a new issue

If you spot a problem with the docs, [search if an issue already exists](https://docs.github.com/en/github/searching-for-information-on-github/searching-on-github/searching-issues-and-pull-requests#search-by-the-title-body-or-comments). If a related issue doesn't exist, you can open a new issue using a relevant [issue form](https://github.com/github/docs/issues/new/choose).

#### Solve an issue

Scan through our [existing issues](https://github.com/github/docs/issues) to find one that interests you. You can narrow down the search using `labels` as filters. See [Labels](/contributing/how-to-use-labels.md) for more information. As a general rule, we don’t assign issues to anyone. If you find an issue to work on, you are welcome to open a PR with a fix.

### Make Changes

#### Make changes in the UI

Click **Make a contribution** at the bottom of any docs page to make small changes such as a typo, sentence fix, or a broken link. This takes you to the `.md` file where you can make your changes and [create a pull request](#pull-request) for a review.

#### Make changes in a codespace

For more information about using a codespace for working on GitHub documentation, see "[Working in a codespace](https://github.com/github/docs/blob/main/contributing/codespace.md)."

#### Make changes locally

1. [Install Git LFS](https://docs.github.com/en/github/managing-large-files/versioning-large-files/installing-git-large-file-storage).

2. Fork the repository.
- Using GitHub Desktop:
  - [Getting started with GitHub Desktop](https://docs.github.com/en/desktop/installing-and-configuring-github-desktop/getting-started-with-github-desktop) will guide you through setting up Desktop.
  - Once Desktop is set up, you can use it to [fork the repo](https://docs.github.com/en/desktop/contributing-and-collaborating-using-github-desktop/cloning-and-forking-repositories-from-github-desktop)!

- Using the command line:
  - [Fork the repo](https://docs.github.com/en/github/getting-started-with-github/fork-a-repo#fork-an-example-repository) so that you can make your changes without affecting the original project until you're ready to merge them.

3. Create a working branch and start with your changes!

4. Set up pre-commit hooks (see below).

### Setting up Pre-commit Hooks

This project uses [pre-commit](https://pre-commit.com/) to ensure code quality before commits. The hooks automatically:

- Format C/C++ code with `clang-format`
- Fix trailing whitespace and end-of-file issues
- Validate YAML syntax
- Prevent accidental commits to the `main` branch

#### Installation

**Linux/macOS:**

```bash
# Using pip
pip install pre-commit

# Or using PlatformIO's Python environment
~/.platformio/penv/bin/python -m pip install pre-commit
```

**Windows:**

```powershell
# Using pip
pip install pre-commit

# Or using PlatformIO's Python environment
%USERPROFILE%\.platformio\penv\Scripts\python -m pip install pre-commit
```

#### Activating the Hooks

After installing pre-commit, navigate to the repository and run:

```bash
pre-commit install
```

This installs the Git hooks. From now on, every `git commit` will automatically run the checks.

#### Running Hooks Manually

To run all hooks on all files (useful for initial setup):

```bash
pre-commit run --all-files
```

To run on staged files only:

```bash
pre-commit run
```

#### Bypassing Hooks (when necessary)

If you need to commit without running hooks (not recommended):

```bash
git commit --no-verify
```

> **Note:** The `no-commit-to-branch` hook prevents direct commits to `main` and `dev`. Always create a feature branch for your changes (typically branching from `dev`).

### Commit your update

Commit the changes once you are happy with them. Don't forget to [self-review](/contributing/self-review.md) to speed up the review process:zap:.

### Pull Request

When you're finished with the changes, create a pull request, also known as a PR.
- Fill the "Ready for review" template so that we can review your PR. This template helps reviewers understand your changes as well as the purpose of your pull request.
- Don't forget to [link PR to issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue) if you are solving one.
- Enable the checkbox to [allow maintainer edits](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/allowing-changes-to-a-pull-request-branch-created-from-a-fork) so the branch can be updated for a merge.
Once you submit your PR, a Docs team member will review your proposal. We may ask questions or request additional information.
- We may ask for changes to be made before a PR can be merged, either using [suggested changes](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/incorporating-feedback-in-your-pull-request) or pull request comments. You can apply suggested changes directly through the UI. You can make any other changes in your fork, then commit them to your branch.
- As you update your PR and apply changes, mark each conversation as [resolved](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/commenting-on-a-pull-request#resolving-conversations).
- If you run into any merge issues, checkout this [git tutorial](https://github.com/skills/resolve-merge-conflicts) to help you resolve merge conflicts and other issues.

### Your PR is merged!

Congratulations :tada::tada: The GitHub team thanks you :sparkles:.

Once your PR is merged, your contributions will be publicly visible on the [GitHub docs](https://docs.github.com/en).

Now that you are part of the GitHub docs community, see how else you can [contribute to the docs](/contributing/types-of-contributions.md).
