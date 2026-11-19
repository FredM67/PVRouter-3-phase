from datetime import datetime
import os
import subprocess

Import("env")


def git(*args) -> str:
  """The output of a git command run in the project, or "N/A" (no git, not a repository, empty).

  No shell: with shell=True, a list of arguments only runs its first element on Linux - a bare
  'git', which fails - so Linux builds always said N/A.
  """
  try:
    output = subprocess.check_output(["git", *args], cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL)
  except (OSError, subprocess.CalledProcessError):
    return "N/A"
  return output.decode("utf-8").strip() or "N/A"


def get_git_revision_short_hash() -> str:
  # a fixed length: plain --short grows when an abbreviation becomes ambiguous, and the size of
  # the firmware would move with it (the PR report compares sizes to the byte)
  return git("rev-parse", "--short=8", "HEAD")


def get_git_current_branch() -> str:
  # empty, hence N/A, on a detached HEAD (CI checkouts)
  return git("branch", "--show-current")


def write_version_h(file_path, project_path, current_time, branch_name, commit_hash, build_env):
    content = f"""\
#ifndef VERSION_H
#define VERSION_H

#define PROJECT_PATH "{project_path}"
#define CURRENT_TIME "{current_time}"
#define BRANCH_NAME "{branch_name}"
#define COMMIT_HASH "{commit_hash}"
#define BUILD_ENV "{build_env}"

#endif // VERSION_H
"""
    # Only write to the file if the content has changed
    if not os.path.exists(file_path) or open(file_path).read() != content:
        with open(file_path, "w") as f:
            f.write(content)

# Get project directory and other values
proj_path = os.path.basename(env["PROJECT_DIR"])
current_time = datetime.now().replace(microsecond=0).astimezone().isoformat(' ')
branch_name = get_git_current_branch()
commit_hash = get_git_revision_short_hash()
build_env = env["PIOENV"]

# Path to the version.h file
version_h_path = os.path.join(env["PROJECT_DIR"], "version.h")

# Write the values to version.h
write_version_h(version_h_path, proj_path, current_time, branch_name, commit_hash, build_env)
