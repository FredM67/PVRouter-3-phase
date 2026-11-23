import pytest

from ct_recorder.capture import CaptureError, capture
from ct_recorder.recording import read_recording


class FakeSerial:
    """Answers each command from a script: a list of replies, one per write."""

    def __init__(self, replies):
        self.replies = list(replies)
        self.written = []
        self.pending = b""

    def write(self, data):
        self.written.append(data)
        self.pending += self.replies.pop(0)

    def read(self, size=1):
        chunk, self.pending = self.pending[:size], self.pending[size:]
        return chunk


RECORDING = b"#OK\n#version 1\n#BEGIN\n" + bytes([0xA5, 0, 1, 0, 2, 0, 3, 0]) + b"#END frames=1 overruns=0\n"


def test_capture_sends_commands_and_returns_the_recording(tmp_path):
    ser = FakeSerial([b"#OK\n", b"#OK\n", RECORDING])
    data = capture(ser, 1, filter_settings=("0.002", "12"), pattern="5,5")
    assert ser.written == [b"F 0.002 12\n", b"B 5,5\n", b"R 1\n"]
    path = tmp_path / "r.bin"
    path.write_bytes(data)
    assert list(read_recording(path).i_out) == [3]


def test_warming_up_raises_without_wait():
    with pytest.raises(CaptureError, match="warming up"):
        capture(FakeSerial([b"#ERR warming up\n"]), 1)


def test_warming_up_retries_with_wait():
    ser = FakeSerial([b"#ERR warming up\n", RECORDING])
    assert capture(ser, 1, wait=True, sleep=lambda s: None) == RECORDING
    assert ser.written == [b"R 1\n", b"R 1\n"]
