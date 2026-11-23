"""Synthetic recordings for the tests: what the sketch would stream from a board driving a resistive load.

make_recording() writes a recording file (header, frames, #END) as capture.py does: the mains voltage, the
load switched by the sketch's own burst pattern at its own cycle starts, the triac conducting from the next
zero crossing, a CT acting as a first-order high-pass, noise, and I out from the firmware's path.
"""

from pathlib import Path

import numpy as np

from .firmware_path import PathState, SamplePath, coefficients_of, firmware_current
from .recording import FLAG_CYCLE_START, FLAG_FILTER_ON, FLAG_LOAD_ON, FLAG_OVERRUN, SYNC

DEFAULT_PATTERN = ((5, 150), (10, 150), (25, 150), (50, 150))  # burst_pattern.h
CONVERSION_US = 104.0


def lcg_noise(n, seed=1, spread=5, continuous=False):
    """Noise from the C library's LCG (reproducible anywhere): integers from -(spread // 2) to spread // 2, or
    continuous: uniform from -spread / 2 to spread / 2 (analog noise, which dithers the ADC's rounding)."""
    state = seed
    noise = np.empty(n)
    for k in range(n):
        state = (1103515245 * state + 12345) % (1 << 31)
        if continuous:
            noise[k] = ((state >> 7) / float(1 << 24) - 0.5) * spread
        else:
            noise[k] = (state >> 16) % spread - spread // 2
    return noise


def ct_highpass(x, tau_s, dt):
    """First-order high-pass of the CT: y[k] = a (y[k-1] + x[k] - x[k-1]); tau_s None: an ideal CT."""
    if tau_s is None:
        return np.asarray(x, dtype=float)
    a = tau_s / (tau_s + dt)
    y = np.empty(len(x))
    prev_x = prev_y = 0.0
    for k, value in enumerate(np.asarray(x, dtype=float).tolist()):
        prev_y = a * (prev_y + value - prev_x)
        prev_x = value
        y[k] = prev_y
    return y


def synthetic_recording(seconds=20.0, spacing_us=624.0):
    """10-bit (V, I): 50 Hz sine of 300 counts on 515, load bursts, a CT high-pass (tau 15 ms), LCG noise.

    Seconds 3 to 4 are clipped (raw 0 and 1023 on V and I): (raw | 32) - dcOffset then wraps the int16.
    For the bit-exact tests of the firmware path (no frames, no flags).
    """
    n = int(seconds * 1e6 / spacing_us)
    dt = spacing_us * 1e-6
    t = np.arange(n) * dt
    noise = lcg_noise(n)
    volts = 515 + 300 * np.sin(2 * np.pi * 50 * t) + noise
    burst = (t % 1.0) < 0.5  # half a second on, half off
    amps = 0.5 * 300 * np.sin(2 * np.pi * 50 * t - 0.2) * burst + 30 * np.sin(2 * np.pi * 50 * t)
    cur = 515 + ct_highpass(amps, 15e-3, dt) + noise[::-1]
    clipped = (t >= 3.0) & (t < 4.0)
    volts = np.where(clipped, 515 + 3 * (volts - 515), volts)
    cur = np.where(clipped, 515 + 12 * (cur - 515), cur)
    return (np.clip(np.rint(volts), 0, 1023).astype(np.int64), np.clip(np.rint(cur), 0, 1023).astype(np.int64))


def _pattern_on(pattern, cycle):
    position = cycle % sum(on + off for on, off in pattern)
    for on, off in pattern:
        if position < on:
            return True
        position -= on
        if position < off:
            return False
        position -= off
    return False


def make_recording(
    path,
    seconds=60.0,
    spacing_us=624,
    i_delay_us=CONVERSION_US,
    i_phase_deg=0.0,
    tau_s=0.015,
    pattern=DEFAULT_PATTERN,
    i_amplitude=150.0,
    ct_filter=None,
    dc_step=0,
    dc_step_s=5.0,
    overrun_at=(),
    gap_frames=10,
    uptime_s=600,
    seed=1,
    current_off_s=None,
):
    """Writes a recording to path and returns path: 3-phase (V1/I1) at 624 us, single-phase at another spacing.

    i_delay_us: the current converted this long after the voltage. i_phase_deg: extra phase between V and I.

    tau_s: the CT's high-pass time constant (None: ideal CT). i_amplitude: the load current's peak in counts
    (0: no load current, noise only). ct_filter: (alpha, gain) the sketch applies (None: filter off).
    dc_step: counts added to V and I for the first dc_step_s seconds. overrun_at: times (s) where gap_frames
    frames are lost, the next frame flagged as an overrun. current_off_s: the load stops drawing current from
    this time on (a thermostat cuts out), while the sketch keeps switching it.
    """
    dt = spacing_us * 1e-6
    n = int(seconds / dt)
    t = 0.3e-3 + np.arange(n) * dt  # the voltage's sampling times
    # the current is converted i_delay_us after the voltage (one conversion for V1/I1, two for CT2); i_phase_deg:
    # the phase the voltage divider and the CT add between V and I, in the same direction
    t_i = t + i_delay_us * 1e-6 + i_phase_deg / 360.0 / 50.0
    omega = 2 * np.pi * 50.0
    v_true = 300 * np.sin(omega * t)
    step = np.where(t < dc_step_s, dc_step, 0)
    v_raw = np.clip(np.rint(515 + v_true + step + lcg_noise(n, seed, 4, continuous=True)), 0, 1023).astype(np.int64)

    # the board: cycle starts as the firmware finds them, the load switched by the pattern at each one
    state = PathState(dc_offset=515 << 6, filter_dc=(515 << 6) << 15)
    sample_path = SamplePath(state)
    cycle_start = np.zeros(n, dtype=bool)
    load_on = np.zeros(n, dtype=bool)
    on = False
    cycle = 0
    for k, v in enumerate(v_raw.tolist()):
        sample_path.on_voltage(v << 6)
        if sample_path.cycle_started:
            cycle_start[k] = True
            on = _pattern_on(pattern, cycle)
            cycle += 1
        load_on[k] = on

    # the triac conducts from the first zero crossing (either way) after the pin changes: the state at the last
    # crossing before each current sample
    last_crossing = np.floor(omega * t_i / np.pi) * np.pi / omega
    driving = np.searchsorted(t, last_crossing, side="right") - 1
    conducting = np.where(driving >= 0, load_on[np.maximum(driving, 0)], False)
    if current_off_s is not None:
        conducting &= t_i < current_off_s
    amps = i_amplitude * np.sin(omega * t_i) * conducting
    i_float = 512 + ct_highpass(amps, tau_s, dt) + step + lcg_noise(n, seed + 1, 4, continuous=True)
    i_raw = np.clip(np.rint(i_float), 0, 1023).astype(np.int64)

    alpha, gain = ct_filter if ct_filter is not None else (0.0, 0.0)
    coef = coefficients_of(gain, alpha) if gain else coefficients_of(0.0, 0.0011)
    i_out = firmware_current(v_raw, i_raw, coefficients=coef if gain else None, state=state)

    flags = load_on * FLAG_LOAD_ON | cycle_start * FLAG_CYCLE_START | (FLAG_FILTER_ON if gain else 0)
    flags = np.asarray(flags, dtype=np.int64)
    keep = np.ones(n, dtype=bool)
    for when in overrun_at:
        k = int(when / dt)
        keep[k - gap_frames : k] = False
        flags[k] |= FLAG_OVERRUN

    frames = np.zeros((n, 8), dtype=np.uint8)
    frames[:, 0] = SYNC
    frames[:, 1] = flags
    frames[:, 2:4] = v_raw.astype("<u2").view(np.uint8).reshape(n, 2)
    frames[:, 4:6] = i_raw.astype("<u2").view(np.uint8).reshape(n, 2)
    frames[:, 6:8] = i_out.astype("<i2").view(np.uint8).reshape(n, 2)
    frames = frames[keep]

    header = {
        "version": "1.0",
        "router": "3-phase" if spacing_us == 624 else "single-phase",
        "board": "OLD",
        "pair": "1",
        "conversions": 6 if spacing_us == 624 else 3,
        "spacing_us": spacing_us,
        "measured_spacing_us": spacing_us,
        "alpha": f"{alpha:.6f}",
        "gain": f"{gain:.4f}",
        "pattern": ",".join(f"{on},{off}" for on, off in pattern),
        "load_pin": 5,
        "dc_offset": state.dc_offset,
        "uptime_s": uptime_s,
        "alpha_fixed": coef.alpha,
        "alpha_shift": coef.shift,
        "gain_fixed": coef.gain,
        "gain_half": coef.gain_half,
        "gain_quarter": coef.gain_quarter,
        "state_dc_offset": state.dc_offset,
        "state_integrator": state.filter_dc,
        "state_confirmed": state.confirmed,
        "state_last_confirmed": state.last_confirmed,
        "state_count": state.count,
        "state_lpf_x64": state.lpf_x64,
        "state_lpf_low_bits": state.lpf_low_bits,
    }
    # the lines end with CR LF, as Serial.println writes them
    text = "#OK\r\n" + "".join(f"#{key} {value}\r\n" for key, value in header.items()) + "#BEGIN\r\n"
    end = f"#END frames={len(frames)} overruns={len(overrun_at)}\r\n"
    path = Path(path)
    path.write_bytes(text.encode("ascii") + frames.tobytes() + end.encode("ascii"))
    return path
