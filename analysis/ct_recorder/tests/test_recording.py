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


def test_resync_rejects_a_sync_byte_inside_a_broken_frame(tmp_path):
    first = frame(0, 1, 2, 3)
    broken = bytes([0xA5, 0x00, 0x07, 0x00, 0x08])  # a frame cut short
    garbage = bytes([0x00, 0xA5, 0x00, 0x01, 0x00, 0x02, 0x00])  # holds a 0xA5 that looks like a frame start
    body = first + broken + garbage + frame(1, 4, 5, 6) + frame(0, 7, 8, 9)
    path = tmp_path / "r.bin"
    path.write_bytes(HEADER + body + b"#END frames=3 overruns=0\n")
    rec = read_recording(path)
    assert list(rec.v_raw) == [1, 4, 7]
    assert list(rec.i_out) == [3, 6, 9]
    assert rec.dropped_bytes == len(broken) + len(garbage)
    assert rec.resync_indices == [1]
    assert list(rec.overrun) == [False, True, False]  # the gap is marked on the first frame after it


def test_clean_recording_has_no_resync(tmp_path):
    path = tmp_path / "r.bin"
    path.write_bytes(HEADER + frame(0, 1, 2, 3) + frame(0, 4, 5, 6) + b"#END frames=2 overruns=0\n")
    rec = read_recording(path)
    assert rec.resync_indices == [] and not rec.overrun.any() and rec.dropped_bytes == 0
    assert rec.coefficients is None and rec.initial_state is None


def test_reads_crlf_lines_as_the_arduino_core_writes_them(tmp_path):
    """Serial.println ends lines with CR LF: the header, #BEGIN and #END lines all arrive that way."""
    header = HEADER.replace(b"\n", b"\r\n")
    path = tmp_path / "r.bin"
    body = frame(1, 500, 520, -5) + frame(0, 7, 8, 9)
    path.write_bytes(b"#OK\r\n" + header + body + b"#END frames=2 overruns=0\r\n")
    rec = read_recording(path)
    assert rec.header["router"] == "3-phase"
    assert rec.header["gain"] == "12.0000"
    assert rec.header["END"] == "frames=2 overruns=0"
    assert list(rec.v_raw) == [500, 7] and list(rec.i_out) == [-5, 9]
    assert rec.dropped_bytes == 0 and rec.resync_indices == []
