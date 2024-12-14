from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
from subprocess import check_output, CalledProcessError

def get_git_info(command: str) -> str:
    try:
        return subprocess.check_output(command, shell=True).decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return "N/A"
      
# Get the project path
Import("env")
proj_path = env["PROJECT_DIR"]

proj_path = os.path.join(proj_path, "dummy")

macro_value = r"\"" + os.path.split(os.path.dirname(proj_path))[1] + r"\""

tz_dt = datetime.now().replace(microsecond=0).astimezone().isoformat(' ')

env.Append(CPPDEFINES=[
  ("PROJECT_PATH", macro_value),
  ("CURRENT_TIME", "\\\"" + tz_dt + "\\\""),
  ("BRANCH_NAME", r"\"" + get_git_info('git branch --show-current') + r"\""),
  ("COMMIT_HASH", r"\"" + get_git_info('git rev-parse --short HEAD') + r"\"")
])
