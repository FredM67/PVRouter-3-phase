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

Import("env")
proj_path = env["PROJECT_DIR"]

proj_path = os.path.join(proj_path, "dummy")

macro_value = r"\"" + os.path.split(os.path.dirname(proj_path))[1] + r"\""

tz_dt = datetime.now().replace(microsecond=0).astimezone().isoformat(' ')

env.Append(CPPDEFINES=[
  ("PROJECT_PATH", macro_value),
  ("CURRENT_TIME", "\\\"" + tz_dt + "\\\""),
  ("BRANCH_NAME", r"\"" + get_git_current_branch() + r"\""),
  ("COMMIT_HASH", r"\"" + get_git_revision_short_hash() + r"\"")
])
