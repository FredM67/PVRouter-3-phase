import struct

import numpy as np
import pytest

from ct_recorder.firmware_path import Coefficients, PathState, coefficients_of, firmware_current
from ct_recorder.recording import read_recording

from .conftest import synthetic_recording

SETTINGS = [(0.0, 0), (0.0011, 9), (0.002, 12), (0.0625 - 1e-6, 63), (0.01, 9), (0.00005, 12)]


def test_synthetic_recording_is_clipped():
    v, i = synthetic_recording()
    assert 0 in v and 1023 in v and 0 in i and 1023 in i


@pytest.mark.parametrize("alpha,gain", SETTINGS)
def test_firmware_path_matches_cpp(golden, alpha, gain):
    v, i = synthetic_recording()
    coef, _, expected = golden(v, i, alpha, gain)
    if gain:
        assert tuple(coefficients_of(gain, alpha)) == coef
    got = firmware_current(v, i, alpha, gain)
    assert got.dtype == np.int16
    assert len(got) == len(expected) == len(v)
    assert np.array_equal(got, expected)
    if gain:
        assert not np.array_equal(got, firmware_current(v, i, alpha, 0))  # the filter does something


def test_clipping_wraps_the_int16():
    """The clipped segment drives (raw | 32) - dcOffset beyond the int16 range, as the C++ does (the match above)."""
    v, i = synthetic_recording()
    out = firmware_current(v, i, 0.0, 0)
    assert np.abs(out).max() > 30000


@pytest.mark.parametrize("alpha,gain", [(0.0, 0), (0.002, 12)])
def test_firmware_path_matches_stream(golden, alpha, gain):
    """The board runs warm before the recording: the snapshot of the state starts the Python run."""
    v, i = synthetic_recording(seconds=10.0)
    warm = 9000
    _, state_values, expected = golden(v, i, alpha, gain, warm=warm)
    state = PathState(*state_values)
    tail = slice(warm, None)
    from_snapshot = firmware_current(v[tail], i[tail], alpha, gain, state=state)
    assert np.array_equal(from_snapshot, expected)
    assert len(expected) == len(v) - warm
    # from the power-up state the same samples differ: the test can fail
    assert not np.array_equal(firmware_current(v[tail], i[tail], alpha, gain), expected)


def _header(coef, state):
    names = ["alpha_fixed", "alpha_shift", "gain_fixed", "gain_half", "gain_quarter"]
    values = [coef[0], coef[3], coef[4], coef[5], coef[6]]
    state_names = ["dc_offset", "integrator", "confirmed", "last_confirmed", "count", "lpf_x64", "lpf_low_bits"]
    lines = ["#alpha 0.001099", "#gain 9.0000"]
    lines += [f"#{k} {v}" for k, v in zip(names, values)]
    lines += [f"#state_{k} {v}" for k, v in zip(state_names, state)]
    return ("\n".join(lines) + "\n#BEGIN\n").encode()


def test_header_coefficients_rebuild_i_out_where_the_printed_alpha_does_not(golden, tmp_path):
    """F 0.0010993 9 gives alpha 1153; the 6-decimal 0.001099 printed in the header would give 1152."""
    alpha, gain, warm = 0.0010993, 9, 6000
    assert coefficients_of(gain, alpha).alpha == 1153
    assert coefficients_of(gain, 0.001099).alpha == 1152
    v, i = synthetic_recording(seconds=10.0)
    coef, state, expected = golden(v, i, alpha, gain, warm=warm)
    v, i = v[warm:], i[warm:]
    body = b"".join(struct.pack("<BBHHh", 0xA5, 2, a, b, c) for a, b, c in zip(v, i, expected))
    path = tmp_path / "r.bin"
    path.write_bytes(_header(coef, state) + body + b"#END frames=%d overruns=0\n" % len(v))
    rec = read_recording(path)
    assert rec.coefficients == Coefficients(*coef)
    assert rec.initial_state == PathState(*state)
    assert np.array_equal(rec.replay(), rec.i_out)
    assert np.array_equal(
        firmware_current(rec.v_raw, rec.i_raw, coefficients=rec.coefficients, state=rec.initial_state), rec.i_out
    )
    # the 6-decimal alpha of the header is one step off: the output differs
    assert not np.array_equal(firmware_current(rec.v_raw, rec.i_raw, 0.001099, 9, state=rec.initial_state), rec.i_out)
