from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
from subprocess import check_output, CalledProcessError

def get_git_revision_short_hash() -> str:
  try:
    return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'],shell=True).decode('utf-8').strip()
  except CalledProcessError:
    return "N/A"

def get_git_current_branch() -> str:
  try:  
    return subprocess.check_output(['git', 'branch', '--show-current'],shell=True).decode('utf-8').strip()
  except CalledProcessError:
    return "N/A"

def write_version_h(file_path, project_path, current_time, branch_name, commit_hash):
    content = f"""\
#ifndef VERSION_H
#define VERSION_H

#define PROJECT_PATH "{project_path}"
#define CURRENT_TIME "{current_time}"
#define BRANCH_NAME "{branch_name}"
#define COMMIT_HASH "{commit_hash}"

#endif // VERSION_H
"""
    # Only write to the file if the content has changed
    if not os.path.exists(file_path) or open(file_path).read() != content:
        with open(file_path, "w") as f:
            f.write(content)

Import("env")

# Get project directory and other values
proj_path = os.path.basename(env["PROJECT_DIR"])
current_time = datetime.now().replace(microsecond=0).astimezone().isoformat(' ')
branch_name = get_git_current_branch()
commit_hash = get_git_revision_short_hash()

# Path to the version.h file
version_h_path = os.path.join(env["PROJECT_DIR"], "version.h")

# Write the values to version.h
write_version_h(version_h_path, proj_path, current_time, branch_name, commit_hash)
