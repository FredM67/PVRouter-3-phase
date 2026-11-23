"""Reads what capture.py wrote: "#" header lines, "#BEGIN", 8-byte binary frames, "#END ..."."""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import numpy as np

from .firmware_path import Coefficients, PathState, firmware_current

FRAME_SIZE = 8
SYNC = 0xA5
FLAG_LOAD_ON = 1
FLAG_FILTER_ON = 2
FLAG_CYCLE_START = 4
FLAG_OVERRUN = 8
FLAGS_MASK = 0x0F
RAW_LIMIT = 1024


@dataclass
class Recording:
    """One recording. header holds the "#key value" lines as strings ("END" holds the #END line's text)."""

    header: dict
    v_raw: np.ndarray  # 10-bit
    i_raw: np.ndarray  # 10-bit
    i_out: np.ndarray  # the firmware's current minus DC (its x64 scale), filtered when the filter is on
    load_on: np.ndarray
    filter_on: np.ndarray
    cycle_start: np.ndarray
    overrun: np.ndarray  # frames were lost before this one (the sketch's flag), or bytes were skipped before it
    dropped_bytes: int
    resync_indices: list = field(default_factory=list)  # frames that follow skipped bytes (also marked in overrun)
    coefficients: Optional[Coefficients] = None  # the fixed-point coefficients the sketch used, ints
    initial_state: Optional[PathState] = None  # the filters' state before the first frame

    def replay(self):
        """I out recomputed by the firmware's path from v_raw and i_raw, from the header's state and coefficients."""
        filtered = bool(len(self.filter_on) and self.filter_on[0])
        return firmware_current(
            self.v_raw, self.i_raw, coefficients=self.coefficients if filtered else None, state=self.initial_state
        )


def _valid(frame):
    return (
        frame[0] == SYNC
        and (frame[1] & ~FLAGS_MASK) == 0
        and (frame[2] | frame[3] << 8) < RAW_LIMIT
        and (frame[4] | frame[5] << 8) < RAW_LIMIT
    )


def parse_frames(body):
    """Returns (list of 8-byte frames, dropped byte count, indices of the frames that follow skipped bytes).

    Bytes that are not a valid frame are skipped one by one. A candidate frame is also required to be followed
    by a sync byte (or the end of the body): a 0xA5 inside a broken frame cannot pass for a frame start.
    """
    frames = []
    resyncs = []
    dropped = 0
    skipped = False
    pos = 0
    size = len(body)
    while pos < size:
        end = pos + FRAME_SIZE
        if end <= size and _valid(body[pos:end]) and (end == size or body[end] == SYNC):
            if skipped:
                resyncs.append(len(frames))
                skipped = False
            frames.append(body[pos:end])
            pos = end
        else:
            dropped += 1
            skipped = True
            pos += 1
    return frames, dropped, resyncs


def read_recording(path):
    data = Path(path).read_bytes()
    marker = b"#BEGIN\n"
    begin = data.find(marker)
    if begin < 0:
        raise ValueError(f"{path}: no #BEGIN line")
    header = {}
    for line in data[:begin].decode("ascii", errors="replace").splitlines():
        if not line.startswith("#"):
            continue
        key, _, value = line[1:].partition(" ")
        if key not in ("OK", "ERR"):
            header[key] = value
    start = begin + len(marker)
    end = data.rfind(b"#END", start)
    if end >= 0:
        tail = data[end:].split(b"\n", 1)[0].decode("ascii", errors="replace")
        header["END"] = tail[len("#END") :].strip()
    else:
        end = len(data)
    frames, dropped, resyncs = parse_frames(data[start:end])
    if frames:
        raw = np.frombuffer(b"".join(frames), dtype=np.uint8).reshape(-1, FRAME_SIZE)
    else:
        raw = np.zeros((0, FRAME_SIZE), dtype=np.uint8)
    flags = raw[:, 1]
    overrun = (flags & FLAG_OVERRUN) != 0
    overrun[resyncs] = True  # bytes were skipped before these frames: a gap of unknown length
    v = raw[:, 2].astype(np.int64) | (raw[:, 3].astype(np.int64) << 8)
    i = raw[:, 4].astype(np.int64) | (raw[:, 5].astype(np.int64) << 8)
    out = (raw[:, 6].astype(np.uint16) | (raw[:, 7].astype(np.uint16) << 8)).view(np.int16).astype(np.int64)
    return Recording(
        header=header,
        v_raw=v,
        i_raw=i,
        i_out=out,
        load_on=(flags & FLAG_LOAD_ON) != 0,
        filter_on=(flags & FLAG_FILTER_ON) != 0,
        cycle_start=(flags & FLAG_CYCLE_START) != 0,
        overrun=overrun,
        dropped_bytes=dropped,
        resync_indices=resyncs,
        coefficients=_coefficients(header),
        initial_state=_state(header),
    )


def _ints(header, keys):
    """The header values of keys as ints, or None when one is missing or not a number."""
    try:
        return [int(header[key]) for key in keys]
    except (KeyError, ValueError):
        return None


def _coefficients(header):
    values = _ints(header, ("alpha_fixed", "alpha_shift", "gain_fixed", "gain_half", "gain_quarter"))
    return None if values is None else Coefficients.from_fixed(*values)


def _state(header):
    values = _ints(
        header,
        (
            "state_dc_offset",
            "state_integrator",
            "state_confirmed",
            "state_last_confirmed",
            "state_count",
            "state_lpf_x64",
            "state_lpf_low_bits",
        ),
    )
    return None if values is None else PathState(*values)
