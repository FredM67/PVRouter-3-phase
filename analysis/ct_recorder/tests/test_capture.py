import pytest

from ct_recorder import capture as capture_module
from ct_recorder.capture import CaptureError, capture, open_port
from ct_recorder.recording import read_recording


class FakeSerial:
    """Answers each command from a script: a list of replies, one per write. Lines end with CR LF, as
    Serial.println writes them."""

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

    def reset_input_buffer(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False


RECORDING = (
    b"#OK\r\n#version 1\r\n#BEGIN\r\n" + bytes([0xA5, 0, 1, 0, 2, 0, 3, 0]) + b"#END frames=1 overruns=0\r\n"
)


def test_capture_sends_commands_and_returns_the_recording(tmp_path):
    ser = FakeSerial([b"#OK\r\n", b"#OK\r\n", RECORDING])
    data = capture(ser, 1, filter_settings=("0.002", "12"), pattern="5,5")
    assert ser.written == [b"F 0.002 12\n", b"B 5,5\n", b"R 1\n"]
    path = tmp_path / "r.bin"
    path.write_bytes(data)
    rec = read_recording(path)
    assert list(rec.i_out) == [3]
    assert rec.header["version"] == "1" and rec.header["END"] == "frames=1 overruns=0"


def test_warming_up_raises_without_wait():
    with pytest.raises(CaptureError, match="warming up"):
        capture(FakeSerial([b"#ERR warming up\r\n"]), 1, wait=False)


def test_warming_up_retries_with_wait():
    ser = FakeSerial([b"#ERR warming up\r\n", RECORDING])
    assert capture(ser, 1, wait=True, sleep=lambda s: None) == RECORDING
    assert ser.written == [b"R 1\n", b"R 1\n"]


def test_waiting_is_the_default():
    """Opening the port may reset the board: the capture then meets the 120 s warm-up, and waits through it."""
    ser = FakeSerial([b"#ERR warming up\r\n", b"#ERR warming up\r\n", RECORDING])
    assert capture(ser, 1, sleep=lambda s: None) == RECORDING
    assert len(ser.written) == 3


def test_error_after_ok_is_reported():
    """The sketch answers #OK, prints part of the header, then gives up: "#ERR no ADC conversions"."""
    ser = FakeSerial([b"#OK\r\n#version 1\r\n#alpha 0.000000\r\n#ERR no ADC conversions\r\n"])
    with pytest.raises(CaptureError, match="no ADC conversions"):
        capture(ser, 1)


def test_main_waits_unless_told_not_to(tmp_path, monkeypatch):
    monkeypatch.setattr(capture_module.time, "sleep", lambda s: None)
    opened = []

    def fake_open(port):
        opened.append(port)
        return FakeSerial([b"#ERR warming up\r\n", RECORDING])

    monkeypatch.setattr(capture_module, "open_port", fake_open)
    out = tmp_path / "r.ctr"
    assert capture_module.main(["COM9", "--seconds", "1", "-o", str(out)]) == 0
    assert out.read_bytes() == RECORDING and opened == ["COM9"]
    assert capture_module.main(["COM9", "--seconds", "1", "--no-wait", "-o", str(tmp_path / "x.ctr")]) == 1


def test_open_port_deasserts_dtr_and_rts_before_opening():
    """DTR (and RTS on some adapters) resets an auto-reset board: both are set low before the port opens."""
    calls = []

    class Port:
        def __init__(self):
            self.is_open = False

        def __setattr__(self, name, value):
            if name in ("port", "baudrate", "timeout", "dtr", "rts"):
                calls.append((name, value))
            object.__setattr__(self, name, value)

        def open(self):
            calls.append(("open", None))
            self.is_open = True

    class Module:
        Serial = Port

    port = open_port("COM9", serial_module=Module)
    assert port.is_open
    assert calls[-1] == ("open", None)
    before = calls[:-1]
    assert ("dtr", False) in before and ("rts", False) in before
    assert ("port", "COM9") in before and ("baudrate", capture_module.BAUD) in before


def test_waiting_gives_up_after_the_warm_up_and_a_margin():
    retries = capture_module.WARMUP_WAIT_S // capture_module.WARMUP_RETRY_S
    ser = FakeSerial([b"#ERR warming up\r\n"] * (retries + 1))
    with pytest.raises(CaptureError, match="still warming up"):
        capture(ser, 1, sleep=lambda s: None)
    assert len(ser.written) == retries + 1
