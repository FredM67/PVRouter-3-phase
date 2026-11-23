"""Records from the CT recorder sketch over serial.

    python -m ct_recorder.capture PORT --seconds 20 [--filter ALPHA GAIN] [--pattern 5,5,10,10] [--no-wait] -o FILE

Sends F (filter) and B (burst pattern) when given, then R <seconds>, and writes everything the sketch answers
(from #OK to the #END line, header lines and binary frames) to FILE, for recording.read_recording().

Opening the port may reset the board: the sketch then refuses to record for 120 s (#ERR warming up). The capture
waits and retries by default; --no-wait stops with an error instead.
"""

import argparse
import sys
import time

BAUD = 500000
WARMUP_RETRY_S = 5
WARMUP_WAIT_S = 180  # the sketch's warm-up lasts 120 s after power-up or a reset; longer means something is wrong
RESET_SETTLE_S = 2.0  # after a reset, the bootloader runs for up to about 2 s before the sketch starts


class CaptureError(Exception):
    pass


def _read_line(ser, timeout_s):
    """One line (without the end of line), or None on timeout."""
    deadline = time.monotonic() + timeout_s
    line = bytearray()
    while time.monotonic() < deadline:
        byte = ser.read(1)
        if not byte:
            continue
        if byte == b"\n":
            return bytes(line).decode("ascii", errors="replace").strip()
        line += byte
    return None


def _command(ser, text, timeout_s=3.0):
    """Sends one command, returns the reply once it is #OK; raises CaptureError on #ERR or silence."""
    ser.write(text.encode("ascii") + b"\n")
    while True:
        line = _read_line(ser, timeout_s)
        if line is None:
            raise CaptureError(f"no answer to '{text}'")
        if line.startswith("#ERR"):
            raise CaptureError(f"'{text}': {line[1:]}")
        if line == "#OK":
            return


def _refusal(data):
    """The #ERR line the sketch sent before #BEGIN (in place of #OK, or after #OK and part of the header, as
    "#ERR no ADC conversions"), or None. Only complete lines count."""
    text = bytes(data).split(b"#BEGIN", 1)[0]
    for line in text.split(b"\n")[:-1]:
        line = line.strip()
        if line.startswith(b"#ERR"):
            return line.decode("ascii", errors="replace")
    return None


def capture(ser, seconds, filter_settings=None, pattern=None, wait=True, sleep=time.sleep):
    """Returns the bytes of the recording (from #OK to the #END line).

    wait: while the sketch is warming up, retry every WARMUP_RETRY_S, for up to WARMUP_WAIT_S.
    """
    if filter_settings is not None:
        _command(ser, f"F {filter_settings[0]} {filter_settings[1]}")
    if pattern:
        _command(ser, f"B {pattern}")
    retries = 0
    while True:
        ser.write(f"R {seconds}\n".encode("ascii"))
        data = bytearray()
        deadline = time.monotonic() + seconds + 20
        begun = False
        refusal = None
        end = -1
        searched = 0
        while time.monotonic() < deadline:
            chunk = ser.read(4096)
            if not chunk:
                continue
            data += chunk
            if not begun:
                refusal = _refusal(data)
                if refusal is not None:
                    break
                begun = b"#BEGIN" in data
            if end < 0:
                end = data.find(b"#END", max(searched - 3, 0))  # only the new bytes (and a marker cut in two)
                searched = len(data)
            if end >= 0 and data.find(b"\n", end) >= 0:
                return bytes(data)
        if refusal is None:
            raise CaptureError("the recording did not end (no #END line)")
        if refusal != "#ERR warming up":
            raise CaptureError(f"R {seconds}: {refusal[1:]}")
        if not wait:
            raise CaptureError(
                "the sketch is warming up (it refuses to record for 120 s after power-up or a reset):"
                " wait, or leave out --no-wait"
            )
        if retries * WARMUP_RETRY_S >= WARMUP_WAIT_S:
            raise CaptureError(
                f"the sketch is still warming up after {WARMUP_WAIT_S} s: does the board keep resetting?"
            )
        if retries == 0:
            print("The sketch is warming up (120 s after power-up or a reset), waiting...", file=sys.stderr)
        retries += 1
        sleep(WARMUP_RETRY_S)


def open_port(port, serial_module=None):
    """The serial port at BAUD, opened with DTR and RTS low: on an auto-reset board (Uno, Nano), DTR going high
    resets it. pyserial applies both as it opens the port; some drivers still pulse DTR on open, so the board may
    reset all the same."""
    if serial_module is None:
        import serial as serial_module  # pyserial

    ser = serial_module.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 0.5
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port")
    parser.add_argument("--seconds", type=int, required=True, help="1 to 3600")
    parser.add_argument("--filter", nargs=2, metavar=("ALPHA", "GAIN"), help="CT filter, gain 0 is off")
    parser.add_argument("--pattern", help="burst pattern in mains cycles: ON,OFF[,ON,OFF...]")
    parser.add_argument(
        "--no-wait", action="store_true", help="when the sketch is warming up, stop with an error (default: wait)"
    )
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args(argv)

    with open_port(args.port) as ser:
        time.sleep(RESET_SETTLE_S)  # in case the board reset as the port opened
        ser.reset_input_buffer()
        try:
            data = capture(ser, args.seconds, args.filter, args.pattern, wait=not args.no_wait, sleep=time.sleep)
        except CaptureError as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
    with open(args.output, "wb") as out:
        out.write(data)
    print(f"{len(data)} bytes written to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
