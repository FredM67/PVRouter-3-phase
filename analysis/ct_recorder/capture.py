"""Records from the CT recorder sketch over serial.

    python -m ct_recorder.capture PORT --seconds 20 [--filter ALPHA GAIN] [--pattern 5,5,10,10] [--wait] -o FILE

Sends F (filter) and B (burst pattern) when given, then R <seconds>, and writes everything the sketch answers
(from #OK to the #END line, header lines and binary frames) to FILE, for recording.read_recording().
"""

import argparse
import sys
import time

BAUD = 500000
WARMUP_RETRY_S = 5


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


def capture(ser, seconds, filter_settings=None, pattern=None, wait=False, sleep=time.sleep):
    """Returns the bytes of the recording (from #OK to the #END line)."""
    if filter_settings is not None:
        _command(ser, f"F {filter_settings[0]} {filter_settings[1]}")
    if pattern:
        _command(ser, f"B {pattern}")
    while True:
        ser.write(f"R {seconds}\n".encode("ascii"))
        data = bytearray()
        deadline = time.monotonic() + seconds + 20
        refused = False
        while time.monotonic() < deadline:
            chunk = ser.read(4096)
            if not chunk:
                continue
            data += chunk
            if not data.startswith(b"#OK") and b"\n" in data and data.lstrip().startswith(b"#ERR"):
                refused = True
                break
            end = data.rfind(b"#END")
            if end >= 0 and b"\n" in data[end:]:
                return bytes(data)
        if refused:
            reason = data.split(b"\n", 1)[0].decode("ascii", errors="replace").strip()
            if reason == "#ERR warming up" and wait:
                print("The sketch is warming up (120 s after power-up), retrying...", file=sys.stderr)
                sleep(WARMUP_RETRY_S)
                continue
            if reason == "#ERR warming up":
                raise CaptureError("the sketch is warming up (it refuses recording for 120 s after power-up): wait, or use --wait")
            raise CaptureError(f"R {seconds}: {reason[1:]}")
        raise CaptureError("the recording did not end (no #END line)")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port")
    parser.add_argument("--seconds", type=int, required=True, help="1 to 3600")
    parser.add_argument("--filter", nargs=2, metavar=("ALPHA", "GAIN"), help="CT filter, gain 0 is off")
    parser.add_argument("--pattern", help="burst pattern in mains cycles: ON,OFF[,ON,OFF...]")
    parser.add_argument("--wait", action="store_true", help="when the sketch is warming up, wait and retry")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args(argv)

    import serial  # pyserial

    with serial.Serial(args.port, BAUD, timeout=0.5) as ser:
        time.sleep(2.0)  # the board may reset when the port opens
        ser.reset_input_buffer()
        try:
            data = capture(ser, args.seconds, args.filter, args.pattern, args.wait)
        except CaptureError as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
    with open(args.output, "wb") as out:
        out.write(data)
    print(f"{len(data)} bytes written to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
