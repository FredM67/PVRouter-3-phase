"""The firmware's current path, bit for bit: sample_path.h (SamplePath) and ct_filter.h (CtFilter::LowPassBoost).

Plain Python ints with the C++ widths made explicit: wrap16/wrap32 for the integer casts, ">>" on negative
ints is the arithmetic shift gcc gives, the float to fixed conversions run in float32 as the C++ does.
"""

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


def coefficients_of(gain, alpha):
    """CtFilter::coefficientsOf(): (alpha, alphaX2, alphaX4, shift, gain, gainHalf, gainQuarter)."""
    f32 = np.float32
    gain = f32(gain)
    alpha = f32(alpha)
    half = f32(0.5)
    shift = MAX_ALPHA_SHIFT
    while shift != 0 and alpha * f32(1 << (16 + shift)) + half >= f32(ALPHA_FIXED_LIMIT):
        shift -= 1
    alpha_fixed = _to_int16(alpha * f32(1 << (16 + shift)) + half)
    return (
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

    def __init__(self):
        self.dc_offset = 511 << 6
        self.filter_dc = (511 << 6) << 15
        self.confirmed = 0  # 0 negative, 1 positive
        self.last_confirmed = 0
        self.count = 0
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

    def __init__(self):
        self.lpf_x64 = 0
        self.lpf_low_bits = 0

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


def firmware_current(v_raw, i_raw, alpha, gain):
    """I out as the sketch streams it, from the 10-bit raw V and I of the frames. Gain 0: filter off."""
    path = SamplePath()
    coef = coefficients_of(gain, alpha) if gain != 0 else None
    low_pass = LowPassBoost()
    out = np.empty(len(v_raw), dtype=np.int16)
    for k, (v, i) in enumerate(zip(np.asarray(v_raw).tolist(), np.asarray(i_raw).tolist())):
        path.on_voltage(v << 6)
        value = path.current_minus_dc(i << 6)
        if coef is not None:
            value = low_pass.apply(value, coef)
        out[k] = value
    return out
