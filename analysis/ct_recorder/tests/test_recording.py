import struct

import numpy as np

from ct_recorder.recording import read_recording

HEADER = b"#version 1\n#router 3-phase\n#alpha 0.002000\n#gain 12.0000\n#BEGIN\n"


def frame(flags, v, i, out):
    return struct.pack("<BBHHh", 0xA5, flags, v, i, out)


def test_reads_header_and_frames(tmp_path):
    data = HEADER + frame(0b0011, 500, 520, -5) + frame(0b0100, 1023, 0, 32767) + frame(0b1000, 1, 2, -32768) + b"#END frames=3 overruns=1\n"
    path = tmp_path / "r.bin"
    path.write_bytes(b"#OK\n" + data)
    rec = read_recording(path)
    assert rec.header["router"] == "3-phase"
    assert rec.header["gain"] == "12.0000"
    assert rec.header["END"] == "frames=3 overruns=1"
    assert rec.dropped_bytes == 0
    assert list(rec.v_raw) == [500, 1023, 1]
    assert list(rec.i_raw) == [520, 0, 2]
    assert list(rec.i_out) == [-5, 32767, -32768]
    assert list(rec.load_on) == [True, False, False]
    assert list(rec.filter_on) == [True, False, False]
    assert list(rec.cycle_start) == [False, True, False]
    assert list(rec.overrun) == [False, False, True]
    assert len(rec.i_out) == 3


def test_reader_resyncs_after_garbage(tmp_path):
    truncated = frame(0, 7, 8, 9)[:5]
    garbage = bytes([0x12, 0x34, 0xFF, 0x00, 0x77])
    body = garbage + frame(0, 1, 2, 3) + frame(1, 4, 5, 6) + frame(0, 7, 8, 9) + truncated
    path = tmp_path / "r.bin"
    path.write_bytes(HEADER + body + b"#END frames=4 overruns=0\n")
    rec = read_recording(path)
    assert list(rec.v_raw) == [1, 4, 7]
    assert rec.dropped_bytes == 5 + len(truncated)
