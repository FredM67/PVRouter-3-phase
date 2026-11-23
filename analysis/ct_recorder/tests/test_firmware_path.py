import numpy as np
import pytest

from ct_recorder.firmware_path import firmware_current

from .conftest import synthetic_recording

SETTINGS = [(0.0, 0), (0.0011, 9), (0.002, 12), (0.0625 - 1e-6, 63)]


@pytest.mark.parametrize("alpha,gain", SETTINGS)
def test_firmware_path_matches_cpp(golden, alpha, gain):
    v, i = synthetic_recording()
    expected = golden(v, i, alpha, gain)
    got = firmware_current(v, i, alpha, gain)
    assert got.dtype == np.int16
    assert len(got) == len(expected) == len(v)
    assert np.array_equal(got, expected)
    if gain:
        assert not np.array_equal(got, firmware_current(v, i, alpha, 0))  # the filter does something


def test_firmware_path_matches_stream(golden):
    """A recording whose i_out comes from the program (as the sketch streams it)."""
    from ct_recorder.recording import Recording

    v, i = synthetic_recording(seconds=5.0)
    i_out = golden(v, i, 0.002, 12)
    n = len(v)
    flags = np.zeros(n, dtype=bool)
    rec = Recording({}, v, i, i_out.astype(np.int64), flags, flags | True, flags, flags, 0)
    assert np.array_equal(firmware_current(rec.v_raw, rec.i_raw, 0.002, 12), rec.i_out)
