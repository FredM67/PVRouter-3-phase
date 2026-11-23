"""Shared fixtures: the host build of golden_main.cpp and the synthetic recording."""

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


def synthetic_recording(seconds=20.0, spacing_us=624.0):
    """10-bit (V, I): 50 Hz sine of 300 counts on 515, load bursts, a CT high-pass (tau 15 ms), LCG noise.

    Seconds 3 to 4 are clipped (raw 0 and 1023 on V and I): (raw | 32) - dcOffset then wraps the int16.
    """
    n = int(seconds * 1e6 / spacing_us)
    dt = spacing_us * 1e-6
    t = np.arange(n) * dt
    state = 1
    noise = np.empty(n)
    for k in range(n):
        state = (1103515245 * state + 12345) % (1 << 31)
        noise[k] = (state >> 16) % 5 - 2  # -2..2
    volts = 515 + 300 * np.sin(2 * np.pi * 50 * t) + noise
    burst = (t % 1.0) < 0.5  # half a second on, half off
    amps = 0.5 * 300 * np.sin(2 * np.pi * 50 * t - 0.2) * burst + 30 * np.sin(2 * np.pi * 50 * t)
    # first-order high-pass of the CT: y[k] = a (y[k-1] + x[k] - x[k-1])
    a = 15e-3 / (15e-3 + dt)
    y = np.empty(n)
    prev_x = prev_y = 0.0
    for k in range(n):
        prev_y = a * (prev_y + amps[k] - prev_x)
        prev_x = amps[k]
        y[k] = prev_y
    cur = 515 + y + noise[::-1]
    clipped = (t >= 3.0) & (t < 4.0)
    volts = np.where(clipped, 515 + 3 * (volts - 515), volts)
    cur = np.where(clipped, 515 + 12 * (cur - 515), cur)
    return (np.clip(np.rint(volts), 0, 1023).astype(np.int64), np.clip(np.rint(cur), 0, 1023).astype(np.int64))
