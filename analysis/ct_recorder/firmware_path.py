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


class LowPassBoost:
    """CtFilter::LowPassBoost, state starting at zero."""

    def __init__(self, state=None):
        state = state or PathState()
        self.lpf_x64 = state.lpf_x64
        self.lpf_low_bits = state.lpf_low_bits

    def apply(self, sample, coef):
        alpha, alpha_x2, alpha_x4, shift, gain, gain_half, gain_quarter = coef
        increment = sample * alpha_x4 - self.lpf_x64 * alpha_x4
        if self.lpf_low_bits & 1:
            increment -= alpha
        if self.lpf_low_bits & 2:
            increment -= alpha_x2
        increment = wrap32(increment)

        increment_high = wrap16(increment >> 16)
        fraction_dropped = ((increment & 0xFFFF) != 0) or ((increment_high & ((1 << shift) - 1)) != 0)
        total = wrap16(self.lpf_low_bits + (increment_high >> shift))
        self.lpf_x64 = wrap16(self.lpf_x64 + (total >> 2))
        self.lpf_low_bits = total & 3
        if self.lpf_x64 < 0 and fraction_dropped:
            self.lpf_low_bits += 1
            if self.lpf_low_bits == 4:
                self.lpf_low_bits = 0
                self.lpf_x64 = wrap16(self.lpf_x64 + 1)

        boost = wrap32(self.lpf_x64 * gain)
        if self.lpf_low_bits & 1:
            boost = wrap32(boost + gain_quarter)
        if self.lpf_low_bits & 2:
            boost = wrap32(boost + gain_half)
        out = sample + (boost >> 8)
        return max(INT16_MIN, min(INT16_MAX, out))


def firmware_current(v_raw, i_raw, alpha=None, gain=None, coefficients=None, state=None):
    """I out as the sketch streams it, from the 10-bit raw V and I of the frames.

    The filter is given by coefficients (a Coefficients, as the recording header carries them) or, as a
    convenience, by alpha and gain; neither, or a gain of 0: filter off. state is a PathState (the recording's
    snapshot); without it the run starts from the power-up state.
    """
    path = SamplePath(state)
    if coefficients is not None:
        coef = coefficients
    elif gain:
        coef = coefficients_of(gain, alpha)
    else:
        coef = None
    low_pass = LowPassBoost(state)
    out = np.empty(len(v_raw), dtype=np.int16)
    for k, (v, i) in enumerate(zip(np.asarray(v_raw).tolist(), np.asarray(i_raw).tolist())):
        path.on_voltage(v << 6)
        value = path.current_minus_dc(i << 6)
        if coef is not None:
            value = low_pass.apply(value, coef)
        out[k] = value
    return out
