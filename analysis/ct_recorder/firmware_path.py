"""The firmware's current path, bit for bit: sample_path.h (SamplePath) and ct_filter.h (CtFilter::LowPassBoost).

Plain Python ints with the C++ widths made explicit: wrap16/wrap32 for the integer casts, ">>" on negative
ints is the arithmetic shift gcc gives, the float to fixed conversions run in float32 as the C++ does.
"""

from dataclasses import dataclass
from typing import NamedTuple

import numpy as np

PERSISTENCE_FOR_POLARITY_CHANGE = 1
MAX_ALPHA_SHIFT = 4
ALPHA_FIXED_LIMIT = 4096
INT16_MAX = 32767
INT16_MIN = -32768


def wrap16(x):
    return ((x + 0x8000) & 0xFFFF) - 0x8000


def wrap32(x):
    return ((x + 0x80000000) & 0xFFFFFFFF) - 0x80000000


def _to_int16(f):
    """static_cast<int16_t>(float): truncation towards zero (the values here are in range)."""
    return wrap16(int(f))


class Coefficients(NamedTuple):
    """CtFilter::Coefficients (fixed point, ints)."""

    alpha: int
    alpha_x2: int
    alpha_x4: int
    shift: int
    gain: int
    gain_half: int
    gain_quarter: int

    @classmethod
    def from_fixed(cls, alpha, shift, gain, gain_half, gain_quarter):
        """From what the recording header carries (alpha_x2 and alpha_x4 are multiples of alpha)."""
        return cls(alpha, wrap16(2 * alpha), wrap16(4 * alpha), shift, gain, gain_half, gain_quarter)


@dataclass
class PathState:
    """The state of SamplePath and LowPassBoost at the start of a run (default: the power-up state)."""

    dc_offset: int = 511 << 6
    filter_dc: int = (511 << 6) << 15
    confirmed: int = 0
    last_confirmed: int = 0
    count: int = 0
    lpf_x64: int = 0
    lpf_low_bits: int = 0


def coefficients_of(gain, alpha):
    """CtFilter::coefficientsOf(gain, alpha), in float32."""
    f32 = np.float32
    gain = f32(gain)
    alpha = f32(alpha)
    half = f32(0.5)
    shift = MAX_ALPHA_SHIFT
    while shift != 0 and alpha * f32(1 << (16 + shift)) + half >= f32(ALPHA_FIXED_LIMIT):
        shift -= 1
    alpha_fixed = _to_int16(alpha * f32(1 << (16 + shift)) + half)
    return Coefficients(
        alpha_fixed,
        wrap16(2 * alpha_fixed),
        wrap16(4 * alpha_fixed),
        shift,
        _to_int16(gain * f32(256.0) + half),
        _to_int16(gain * f32(128.0) + half),
        _to_int16(gain * f32(64.0) + half),
    )


class SamplePath:
    """sample_path.h. Samples are left-aligned (10-bit << 6)."""

    def __init__(self, state=None):
        state = state or PathState()
        self.dc_offset = state.dc_offset
        self.filter_dc = state.filter_dc
        self.confirmed = state.confirmed  # 0 negative, 1 positive
        self.last_confirmed = state.last_confirmed
        self.count = state.count
        self.cycle_started = False

    def on_voltage(self, raw):
        v_minus_dc = wrap16((raw | 32) - self.dc_offset)
        most_recent = 1 if v_minus_dc > 0 else 0
        if most_recent == self.last_confirmed:
            self.count = 0
        else:
            self.count += 1
            if self.count > PERSISTENCE_FOR_POLARITY_CHANGE:
                self.count = 0
                self.confirmed = most_recent
        self.cycle_started = False
        if self.confirmed == 1:
            self.cycle_started = self.last_confirmed != 1
        elif self.last_confirmed != 0:
            self.dc_offset = (((self.filter_dc << 1) & 0xFFFFFFFF) >> 16) & 0xFFFF
        self.filter_dc = (self.filter_dc + v_minus_dc) & 0xFFFFFFFF
        self.last_confirmed = self.confirmed
        return v_minus_dc

    def current_minus_dc(self, raw):
        return wrap16((raw | 32) - self.dc_offset)


def low_pass_trace(samples, coef, lpf_x64=0, lpf_low_bits=0):
    """The state of CtFilter::LowPassBoost after each sample: (lpf_x64, lpf_low_bits), two int64 arrays.

    The state depends on alpha only (the gain is applied after it, boosted()): the filter search runs this loop
    once per alpha and tries every gain on its result.
    """
    alpha, alpha_x2, alpha_x4, shift = coef[0], coef[1], coef[2], coef[3]
    shift_mask = (1 << shift) - 1
    n = len(samples)
    trace_x64 = np.empty(n, dtype=np.int64)
    trace_low = np.empty(n, dtype=np.int64)
    x64 = lpf_x64
    low = lpf_low_bits
    for k, sample in enumerate(np.asarray(samples).tolist()):
        increment = sample * alpha_x4 - x64 * alpha_x4
        if low & 1:
            increment -= alpha
        if low & 2:
            increment -= alpha_x2
        increment = ((increment + 0x80000000) & 0xFFFFFFFF) - 0x80000000  # int32
        high = increment >> 16  # an int32 >> 16 is in the int16 range
        fraction_dropped = (increment & 0xFFFF) != 0 or (high & shift_mask) != 0
        total = ((low + (high >> shift) + 0x8000) & 0xFFFF) - 0x8000  # int16
        x64 = ((x64 + (total >> 2) + 0x8000) & 0xFFFF) - 0x8000
        low = total & 3
        if x64 < 0 and fraction_dropped:
            low += 1
            if low == 4:
                low = 0
                x64 = ((x64 + 1 + 0x8000) & 0xFFFF) - 0x8000
        trace_x64[k] = x64
        trace_low[k] = low
    return trace_x64, trace_low


def boosted(samples, trace_x64, trace_low, coef):
    """CtFilter::LowPassBoost's output from its state after each sample (low_pass_trace()), any gain, numpy."""
    gain, gain_half, gain_quarter = coef[4], coef[5], coef[6]
    boost = trace_x64 * gain
    boost = boost + np.where(trace_low & 1, gain_quarter, 0) + np.where(trace_low & 2, gain_half, 0)
    boost = ((boost + 0x80000000) & 0xFFFFFFFF) - 0x80000000  # int32
    out = np.asarray(samples, dtype=np.int64) + (boost >> 8)
    return np.clip(out, INT16_MIN, INT16_MAX).astype(np.int16)


def current_minus_dc(v_raw, i_raw, state=None):
    """The firmware's current minus DC before the CT filter (int64 array), from the 10-bit raw V and I."""
    path = SamplePath(state)
    out = np.empty(len(v_raw), dtype=np.int64)
    for k, (v, i) in enumerate(zip(np.asarray(v_raw).tolist(), np.asarray(i_raw).tolist())):
        path.on_voltage(v << 6)
        out[k] = path.current_minus_dc(i << 6)
    return out


def firmware_current(v_raw, i_raw, alpha=None, gain=None, coefficients=None, state=None):
    """I out as the sketch streams it, from the 10-bit raw V and I of the frames.

    The filter is given by coefficients (a Coefficients, as the recording header carries them) or, as a
    convenience, by alpha and gain; neither, or a gain of 0: filter off. state is a PathState (the recording's
    snapshot); without it the run starts from the power-up state.
    """
    samples = current_minus_dc(v_raw, i_raw, state)
    if coefficients is not None:
        coef = coefficients
    elif gain:
        coef = coefficients_of(gain, alpha)
    else:
        return samples.astype(np.int16)  # int16 wrap, as the C++ (the path is int16 already)
    state = state or PathState()
    trace_x64, trace_low = low_pass_trace(samples, coef, state.lpf_x64, state.lpf_low_bits)
    return boosted(samples, trace_x64, trace_low, coef)
