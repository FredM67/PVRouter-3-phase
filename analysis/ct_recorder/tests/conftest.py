"""Shared fixtures: the host build of golden_main.cpp (the synthetic recording is in ct_recorder.synthetic)."""

import shutil
import subprocess
from pathlib import Path

import numpy as np
import pytest

REPO = Path(__file__).resolve().parents[3]
GOLDEN_SRC = REPO / "dev" / "CtRecorder" / "test" / "host" / "golden_main.cpp"


@pytest.fixture(scope="session")
def golden(tmp_path_factory):
    """run(v_raw, i_raw, alpha, gain, warm=0) -> (coefficients, state, i_out) from the C++ program.

    coefficients: the 7 ints of CtFilter::Coefficients; state: the 7 ints of the state before sample `warm`
    (None when warm is 0); i_out: int16 array of the samples from `warm` on.
    """
    if shutil.which("g++") is None:
        pytest.skip("g++ is not installed")
    exe = tmp_path_factory.mktemp("golden") / "golden"
    cmd = [
        "g++", "-std=c++17", "-O1", "-I", str(REPO / "dev" / "CtRecorder"),
        "-I", str(REPO / "Mk2_3phase_RFdatalog_temp"), str(GOLDEN_SRC), "-o", str(exe),
    ]
    done = subprocess.run(cmd, capture_output=True, text=True)
    assert done.returncode == 0, done.stderr

    def run(v_raw, i_raw, alpha, gain, warm=0):
        lines = [f"{alpha!r} {gain!r}"] + [f"{v} {i}" for v, i in zip(v_raw, i_raw)]
        args = [str(exe)] + (["--warm", str(warm)] if warm else [])
        out = subprocess.run(args, input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        coef = state = None
        values = []
        for line in out.stdout.splitlines():
            if line.startswith("COEF"):
                coef = tuple(int(x) for x in line.split()[1:])
            elif line.startswith("STATE"):
                state = tuple(int(x) for x in line.split()[1:])
            else:
                values.append(int(line))
        return coef, state, np.array(values, dtype=np.int16)

    return run
