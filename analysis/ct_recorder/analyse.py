"""Analyses a CT recording: the reference, the energy of each burst, the CT filter that offsets the CT, the verdict.

    python -m ct_recorder.analyse RUN.ctr [--compare RUN2.ctr] [--plots DIR]

The "true" current of the resistive load is the voltage scaled by a constant k and shifted by a fraction of a
sample (the divider, the CT and the conversion order shift the phase), fitted on steady ON cycles (20 cycles or
more into a burst). The decisive figure is the energy of each burst, over its ON cycles: the sum of V x I
against the sum of V x k x V shifted, relative. The search runs the firmware's own fixed-point path for each
candidate alpha/gain from the recording's raw samples and starting state, and keeps the pair with the lowest RMS
burst-energy error. With --compare, RUN is the run with the filter off and RUN2 the run with the filter on.
"""

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .firmware_path import PathState, boosted, coefficients_of, current_minus_dc, low_pass_trace
from .recording import read_recording

ALPHA_MIN = 0.00005  # validation.h: alpha from 0.00005 to below 0.0625, gain from 0 to below 64
ALPHA_MAX = 0.0625
GAIN_MAX = 64.0
FIT_CYCLES_INTO_BURST = 20  # the reference is fitted on ON cycles this far into a burst, or further
STARTUP_S = 10.0  # skipped at the start of a recording: a lagging DC removal, the candidate filters settling
WARM_UPTIME_S = 120  # the DC removal settles within this time after power-up (the sketch waits for it)
# the shift of the reference, a time whatever the spacing: 2 ms is 36 deg at 50 Hz. A fast CT leads the current:
# 12 deg (0.67 ms) for a 15 ms CT, 32 deg (1.8 ms) for a 5 ms one, plus up to 208 us of conversion order (CT2)
SHIFT_LIMIT_S = 2.0e-3
LOAD_RATIO_MIN = 3.0  # the current's rms in the ON cycles against its rms late in the OFF periods
# A burst without current (the heater's thermostat cut out, the sketch still switching): its weakest ON cycle,
# from the LEVEL_CYCLES_INTO_BURST-th on, carries less than NO_CURRENT_RATIO of the load's current (the
# strongest burst's median cycle). A resistive load's current only moves by a few % with the mains voltage; the
# load conducts from a zero crossing after the switch, up to a cycle late, hence the first cycles left out.
LEVEL_CYCLES_INTO_BURST = 2
NO_CURRENT_RATIO = 0.5
NO_CURRENT = "no current (thermostat?)"
MIN_USED_BURSTS = 4  # fewer bursts left: too few repeats to judge a filter against the burst-to-burst spread
# The fit leaves rms(i - reference) / rms(reference) on its cycles; beyond this, the current is not k x V shifted
# (not a resistive load, or a load on another phase). Noise and quantisation leave about 1 %; 15 % is a first
# guess, to be tuned on hardware recordings.
PROPORTIONAL_RESIDUAL_MAX = 0.15
TAU_SIGNIFICANCE = 6.0  # the CT's switch-on residual must be this many standard errors above zero
TAU_GRID = (1e-3, 30.0)  # s: the CT time constants tried
TAIL_LIMIT = 0.05  # the CT's tail left at the end of an OFF period, as a fraction of its value at switch-off
ALPHA_GRID = 16
GAIN_GRID = (0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 11.0, 16.0, 22.0, 32.0, 45.0, 63.99)
GOLDEN = (math.sqrt(5.0) - 1.0) / 2.0


class AnalysisError(Exception):
    pass


@dataclass
class Burst:
    """A run of ON cycles and the OFF period after it, up to the next ON cycle. Indices are frame indices."""

    number: int  # 1 for the first complete burst of the recording
    on_cycles: int
    flag_on: int  # the cycle start where the sketch turned the load ON
    flag_off: int  # the cycle start where it turned it OFF
    flag_next: int  # the cycle start of the next burst
    first_cycle: int  # index of the first ON cycle in the list of cycle starts
    off_cycle: int
    excluded: str = ""  # "", "start-up", "overrun" or NO_CURRENT
    start: int = 0  # the load conducts from the zero crossing at start to the one at end
    end: int = 0
    span_end: int = 0  # the next burst's start

    @property
    def used(self):
        return not self.excluded


def _rms(errors):
    return float(np.sqrt(np.mean(np.square(errors)))) if len(errors) else float("nan")


class _Context:
    """What a recording's analysis needs whatever the current: cycles, bursts, windows, the fit's samples."""

    def __init__(self, rec):
        n = len(rec.v_raw)
        self.rec = rec
        self.n = n
        self.dt = float(rec.header.get("spacing_us", 624)) * 1e-6
        self.state = rec.initial_state or PathState()
        cs = np.flatnonzero(rec.cycle_start)
        if len(cs) < 3:
            raise AnalysisError("no mains cycles in the recording (no cycle start)")
        self.cs = cs
        self.period = float(np.median(np.diff(cs)))
        self.omega_dt = 2.0 * math.pi / self.period

        # the voltage centred on each cycle, and its quadrature: V(t + s dt) ~ V cos(s w dt) + Q sin(s w dt)
        v = rec.v_raw.astype(float)
        cycle_of = np.searchsorted(cs, np.arange(n), side="right") - 1  # -1 before the first cycle start
        means = np.add.reduceat(v, cs) / np.diff(np.append(cs, n))
        means = np.concatenate(([means[0]], means))  # the samples before the first cycle start use cycle 0
        self.vc = v - means[cycle_of + 1]
        q = (np.roll(self.vc, -1) - np.roll(self.vc, 1)) / (2.0 * math.sin(self.omega_dt))
        q[0] = q[-1] = 0.0
        self.q = q

        self.skip = int(STARTUP_S / self.dt)
        self.bursts = self._find_bursts()
        self.raw = current_minus_dc(rec.v_raw, rec.i_raw, rec.initial_state)
        self._set_mask()
        self._check_load_current()
        self._exclude_without_current()
        self._set_mask()
        self._set_windows()
        self._v_m = self.vc[self.mask]
        self._q_m = self.q[self.mask]
        self._gram = np.array(
            [[self._v_m @ self._v_m, self._v_m @ self._q_m], [self._v_m @ self._q_m, self._q_m @ self._q_m]]
        )
        cum_vv = np.concatenate(([0.0], np.cumsum(self.vc * self.vc)))
        cum_vq = np.concatenate(([0.0], np.cumsum(self.vc * self.q)))
        self._starts = np.array([b.start for b in self.used])
        self._ends = np.array([b.end for b in self.used])
        self._span_ends = np.array([b.span_end for b in self.used])
        self._e_vv = cum_vv[self._ends] - cum_vv[self._starts]
        self._e_vq = cum_vq[self._ends] - cum_vq[self._starts]
        self._tail = None
        self._check_proportional()

    def _set_mask(self):
        """The used bursts and the fit's samples: their ON cycles FIT_CYCLES_INTO_BURST or more into the burst."""
        self.used = [b for b in self.bursts if b.used]
        self.mask = np.zeros(self.n, dtype=bool)
        for b in self.used:
            for j in range(b.first_cycle + FIT_CYCLES_INTO_BURST, b.off_cycle):
                self.mask[self.cs[j] : self.cs[j + 1]] = True
        if not self.mask.any():
            raise AnalysisError(
                f"no burst of more than {FIT_CYCLES_INTO_BURST} ON cycles after the first {STARTUP_S:g} s:"
                " nothing to fit the reference on (use a longer recording or longer bursts)"
            )

    def _exclude_without_current(self):
        """Excludes the used bursts the load drew no current in, whole or in part (a thermostat cut out), and
        refuses when fewer than MIN_USED_BURSTS are left."""
        levels = {}  # burst number: (weakest cycle, median cycle), current rms per ON cycle
        for b in self.used:
            cycles = [
                float(np.std(self.raw[self.cs[j] : self.cs[j + 1]]))
                for j in range(b.first_cycle + LEVEL_CYCLES_INTO_BURST, b.off_cycle)
            ]
            if cycles:
                levels[b.number] = (min(cycles), float(np.median(cycles)))
        load = max((median for _, median in levels.values()), default=0.0)
        for b in self.used:
            if b.number in levels and levels[b.number][0] < NO_CURRENT_RATIO * load:
                b.excluded = NO_CURRENT
        used = [b for b in self.bursts if b.used]
        if len(used) < MIN_USED_BURSTS:
            without = sum(1 for b in self.bursts if b.excluded == NO_CURRENT)
            raise AnalysisError(
                f"too few bursts to judge a filter: {len(used)} used after the first {STARTUP_S:g} s"
                f" ({without} without current: a thermostat?), {MIN_USED_BURSTS} needed; record longer, with the"
                " load drawing current all along"
            )

    def _check_proportional(self):
        """Refuses a raw current that is not k x V shifted on the fit's cycles: what the fit leaves is above
        PROPORTIONAL_RESIDUAL_MAX of the current.

        A shift at its limit alone does not refuse: the fit then leaves little when the true shift is only a
        little beyond, and the report warns. A shift well beyond the limit (a load on another phase: 120 deg)
        leaves far more than PROPORTIONAL_RESIDUAL_MAX.
        """
        k, _, theta = self.fit(self.raw)
        ref = self.reference(k, theta)[self.mask]
        left = self.raw[self.mask] - ref
        left = left - left.mean()  # the current's DC against the voltage's is not the load's
        ratio = _rms(left) / max(_rms(ref), 1e-9)
        if ratio > PROPORTIONAL_RESIDUAL_MAX:
            raise AnalysisError(
                "current not proportional to the voltage: load not resistive, or on another phase (the fit leaves"
                f" {100 * ratio:.0f} % of the current, {100 * PROPORTIONAL_RESIDUAL_MAX:.0f} % at most); check that"
                " the CT is on the load's phase and that the load is a heater"
            )

    def _find_bursts(self):
        rec, cs = self.rec, self.cs
        on = rec.load_on[cs]
        on_starts = [j for j in range(1, len(cs)) if on[j] and not on[j - 1]]
        off_starts = [j for j in range(1, len(cs)) if on[j - 1] and not on[j]]
        bursts = []
        for number, j_on in enumerate(on_starts):
            j_off = next((j for j in off_starts if j > j_on), None)
            if j_off is None or number + 1 >= len(on_starts):
                break  # the last burst is not complete
            j_next = on_starts[number + 1]
            burst = Burst(number + 1, j_off - j_on, int(cs[j_on]), int(cs[j_off]), int(cs[j_next]), j_on, j_off)
            if burst.flag_on < self.skip:
                burst.excluded = "start-up"
            elif rec.overrun[burst.flag_on : burst.flag_next + 1].any():
                burst.excluded = "overrun"
            bursts.append(burst)
        return bursts

    def _check_load_current(self):
        on = self.raw[self.mask] / 64.0
        late_off = np.zeros(self.n, dtype=bool)
        for b in self.used:
            late_off[(b.flag_off + b.flag_next) // 2 : b.flag_next] = True
        off = self.raw[late_off] / 64.0
        on_rms = float(np.std(on))
        off_rms = float(np.std(off)) if len(off) else 0.0
        if on_rms < LOAD_RATIO_MIN * max(off_rms, 0.5):
            raise AnalysisError(
                f"no load current: the current in the ON cycles (rms {on_rms:.2f} counts) is not above the noise"
                f" of the OFF periods (rms {off_rms:.2f} counts); check the load, the load pin and the CT"
            )

    def _set_windows(self):
        """The load conducts from a zero crossing of the voltage: the one the sketch switched at, or a later one."""
        positive = self.vc >= 0
        rising = np.flatnonzero(~positive[:-1] & positive[1:]) + 1
        falling = np.flatnonzero(positive[:-1] & ~positive[1:]) + 1
        crossings = np.sort(np.concatenate((rising, falling)))
        rising_set = set(rising.tolist())

        def crossing_at(flag):
            """Index in crossings of the rising crossing nearest the flag (within a quarter cycle)."""
            k = int(np.searchsorted(crossings, flag))
            near = [j for j in (k - 2, k - 1, k, k + 1) if 0 <= j < len(crossings)]
            near = [j for j in near if int(crossings[j]) in rising_set]
            near = [j for j in near if abs(int(crossings[j]) - flag) <= self.period / 4]
            if not near:
                return None
            return min(near, key=lambda j: abs(int(crossings[j]) - flag))

        # how many half-cycles after the switching crossing the current starts: the first half-cycle carrying
        # more than half the energy of a steady one, median over the bursts
        cycle_energy = []
        for b in self.used:
            for j in range(b.first_cycle + FIT_CYCLES_INTO_BURST, b.off_cycle):
                s, e = self.cs[j], self.cs[j + 1]
                cycle_energy.append(abs(float(self.vc[s:e] @ self.raw[s:e])))
        half = np.median(cycle_energy) / 2.0
        delays = []
        for b in self.used:
            z = crossing_at(b.flag_on)
            if z is None or z + 5 >= len(crossings):
                continue
            for h in range(4):
                s, e = crossings[z + h], crossings[z + h + 1]
                if abs(float(self.vc[s:e] @ self.raw[s:e])) > 0.5 * half:
                    delays.append(h)
                    break
        self.delay_half_cycles = int(np.median(delays)) if delays else 0

        def window_edge(flag):
            z = crossing_at(flag)
            if z is None or z + self.delay_half_cycles >= len(crossings):
                return None
            return int(crossings[z + self.delay_half_cycles])

        for b in self.bursts:
            edges = (window_edge(b.flag_on), window_edge(b.flag_off), window_edge(b.flag_next))
            if None in edges:
                if b.used:
                    raise AnalysisError(f"burst {b.number}: no zero crossing of the voltage near its switching")
                edges = (b.flag_on, b.flag_off, b.flag_next)
            b.start, b.end, b.span_end = edges

    def fit(self, i):
        """(k, shift in samples, theta) of i ~ k x V shifted, least squares on the fit's samples."""
        i_m = np.asarray(i, dtype=float)[self.mask]
        rhs = np.array([self._v_m @ i_m, self._q_m @ i_m])
        a, b = np.linalg.solve(self._gram, rhs)
        k = math.hypot(a, b)
        if a < 0:
            k, a, b = -k, -a, -b
        theta = math.atan2(b, a)
        limit = SHIFT_LIMIT_S / self.dt  # samples
        if abs(theta / self.omega_dt) >= limit:
            theta = math.copysign(limit * self.omega_dt, theta)
            c, s = math.cos(theta), math.sin(theta)
            g = self._gram
            k = (c * rhs[0] + s * rhs[1]) / (c * c * g[0, 0] + 2 * c * s * g[0, 1] + s * s * g[1, 1])
        return k, theta / self.omega_dt, theta

    def at_shift_limit(self, shift):
        return abs(shift) >= SHIFT_LIMIT_S / self.dt * (1 - 1e-9)

    def reference(self, k, theta):
        return k * (self.vc * math.cos(theta) + self.q * math.sin(theta))

    def errors(self, i, tail=False):
        """Per used burst: (sum of V x I - sum of V x reference) / sum of V x reference over its ON window.

        tail: the measured sum runs on to the end of the OFF period (the CT's tail included).
        """
        i = np.asarray(i, dtype=float)
        k, _, theta = self.fit(i)
        ref = k * (math.cos(theta) * self._e_vv + math.sin(theta) * self._e_vq)
        cum = np.concatenate(([0.0], np.cumsum(self.vc * i)))
        measured = cum[self._span_ends if tail else self._ends] - cum[self._starts]
        return (measured - ref) / ref

    def tail(self):
        """(tau, tau_low, tau_high in s, C, its standard error): a first-order high-pass fitted on the raw residual.

        tau is nan when C is not significant. tau_low to tau_high: the time constants whose fit is within two
        standard errors of the best (a profile of the sum of squares), tau_high inf beyond the grid; a long time
        constant leaves a residual of a fraction of a count, and is only bounded from below.

        The CT's natural mode: -C exp(-t/tau) from each switch-on, +C exp(-t/tau) from each switch-off, with the
        50 Hz reference refitted alongside and a constant (the current's DC against the voltage's).
        """
        if self._tail is not None:
            return self._tail
        region = np.zeros(self.n, dtype=bool)
        conducting = np.zeros(self.n)
        for b in self.used:
            region[b.start : b.span_end] = True
        live = [b for b in self.bursts if b.excluded != NO_CURRENT]  # the load drew current: the CT's events
        for b in live:
            conducting[b.start : b.end] = 1.0
        events = [b.start for b in live] + [b.end for b in live]
        signs = [-1.0] * len(live) + [1.0] * len(live)
        y = self.raw[region].astype(float)
        base = np.column_stack(
            ((self.vc * conducting)[region], (self.q * conducting)[region], np.ones(int(region.sum())))
        )

        def solve(tau_s):
            tau_samples = tau_s / self.dt
            length = min(self.n, int(12 * tau_samples) + 2)
            kernel = np.exp(-np.arange(length) / tau_samples)
            g = np.zeros(self.n)
            for e, sign in zip(events, signs):
                m = min(length, self.n - e)
                g[e : e + m] += sign * kernel[:m]
            x = np.column_stack((g[region], base))
            xtx = x.T @ x
            coef = np.linalg.solve(xtx, x.T @ y)
            sse = float(np.sum((y - x @ coef) ** 2))
            sigma = math.sqrt(sse / max(len(y) - 4, 1) * np.linalg.inv(xtx)[0, 0])
            return sse, coef[0], sigma

        taus = np.geomspace(TAU_GRID[0], TAU_GRID[1], 49)
        sses = np.array([solve(t)[0] for t in taus])
        best = int(np.argmin(sses))
        lo, hi = math.log(taus[max(best - 1, 0)]), math.log(taus[min(best + 1, len(taus) - 1)])
        tau = math.exp(_golden_min(lambda x: solve(math.exp(x))[0], lo, hi, 16))
        sse, c, sigma = solve(tau)
        limit = sse + 4.0 * sse / max(len(y) - 4, 1)  # delta chi-square 4: two standard errors

        def bound(inside, outside):
            """Where the fit crosses the limit between inside (within it) and outside (beyond it)."""
            a, b = math.log(inside), math.log(outside)
            for _ in range(12):
                middle = (a + b) / 2
                if solve(math.exp(middle))[0] <= limit:
                    a = middle
                else:
                    b = middle
            return math.exp(a)

        # every time constant the data allow, even apart from the best one (a long one is barely bounded)
        allowed = [float(t) for t, v in zip(taus, sses) if v <= limit] + [tau]
        inside_low, inside_high = min(allowed), max(allowed)
        outside_low = max((float(t) for t in taus if t < inside_low), default=None)
        outside_high = min((float(t) for t in taus if t > inside_high), default=None)
        tau_low = inside_low if outside_low is None else bound(inside_low, outside_low)
        tau_high = math.inf if outside_high is None else bound(inside_high, outside_high)
        if not abs(c) > TAU_SIGNIFICANCE * sigma:
            tau = tau_low = tau_high = float("nan")
        self._tail = (tau, tau_low, tau_high, c, sigma)
        return self._tail

    def unsettled(self):
        """(bursts, unconfirmed): the used bursts whose OFF period ends with more than TAIL_LIMIT of the CT's tail.

        With an upper bound on the time constant, for the longest one the fit allows. Without one (a slow CT: its
        tail is a fraction of a count), for the shortest: the fit shows the tail left at the end; if even that
        one leaves nothing, the settling cannot be confirmed at 10 bits (unconfirmed True, no bursts), and no OFF
        length could confirm it.
        """
        _, tau_low, tau_high, _, _ = self.tail()
        if math.isnan(tau_high):
            return [], False

        def late(tau):
            return [b for b in self.used if math.exp(-(b.span_end - b.end) * self.dt / tau) > TAIL_LIMIT]

        if not math.isinf(tau_high):
            return late(tau_high), False
        bursts = late(tau_low)
        return bursts, not bursts


def _golden_min(f, lo, hi, iterations):
    """The x in [lo, hi] minimising f (unimodal), by golden-section search."""
    a, b = lo, hi
    c, d = b - GOLDEN * (b - a), a + GOLDEN * (b - a)
    fc, fd = f(c), f(d)
    for _ in range(iterations):
        if fc <= fd:
            b, d, fd = d, c, fc
            c = b - GOLDEN * (b - a)
            fc = f(c)
        else:
            a, c, fc = c, d, fd
            d = a + GOLDEN * (b - a)
            fd = f(d)
    return c if fc <= fd else d


def _context(rec):
    ctx = rec.__dict__.get("_analysis")
    if ctx is None:
        ctx = _Context(rec)
        rec.__dict__["_analysis"] = ctx
    return ctx


def find_bursts(rec):
    """The complete bursts of the recording (list of Burst), the excluded ones with their reason."""
    return _context(rec).bursts


def fit_reference(rec, i=None):
    """(k, shift_samples): I out (or i) ~ k x V shifted by shift_samples, on steady ON cycles.

    The shift is limited to SHIFT_LIMIT_S (2 ms) either way, converted with the recording's spacing.
    """
    k, shift, _ = _context(rec).fit(rec.i_out if i is None else i)
    return k, shift


def burst_energy_errors(rec, i, tail=False):
    """Per used burst, the relative error of sum(V x i) against sum(V x k x V shifted) over its ON cycles."""
    return _context(rec).errors(i, tail)


def ct_tau_ms(rec):
    """The CT's high-pass time constant in ms from the raw switch-on residual, nan when there is none to see."""
    return _context(rec).tail()[0] * 1000.0  # nan stays nan


def search_filter(rec):
    """(alpha, gain, ct_tau_ms): the filter with the lowest RMS burst-energy error, and the CT's time constant.

    For each alpha (log-spaced over the allowed range, then refined) the firmware's low-pass runs once from the
    recording's starting state; every gain is then applied to its state exactly as the firmware does. A gain of
    0 (alpha then meaningless) is the answer when no filter does better than none.
    """
    ctx = _context(rec)
    samples = ctx.raw
    start_x64, start_low = ctx.state.lpf_x64, ctx.state.lpf_low_bits
    best = [_rms(ctx.errors(samples)), 0.0, 0.0]  # rms, alpha, gain

    def best_gain(alpha):
        trace = low_pass_trace(samples, coefficients_of(1.0, alpha), start_x64, start_low)

        def rms_of(gain):
            value = _rms(ctx.errors(boosted(samples, *trace, coefficients_of(gain, alpha))))
            if value < best[0]:
                best[:] = [value, alpha, gain]
            return value

        values = [rms_of(g) for g in GAIN_GRID]
        j = int(np.argmin(values))
        lo = GAIN_GRID[j - 1] if j > 0 else 0.0
        hi = GAIN_GRID[j + 1] if j + 1 < len(GAIN_GRID) else GAIN_GRID[-1]
        gain = round(_golden_min(rms_of, lo, hi, 10) * 256) / 256  # the firmware's gain has 8 fractional bits
        return min(rms_of(gain), values[j]) if 0 < gain < GAIN_MAX else values[j]

    alphas = np.geomspace(ALPHA_MIN, ALPHA_MAX * 0.999, ALPHA_GRID)
    values = [best_gain(a) for a in alphas]
    j = int(np.argmin(values))
    lo, hi = math.log(alphas[max(j - 1, 0)]), math.log(alphas[min(j + 1, len(alphas) - 1)])
    _golden_min(lambda x: best_gain(math.exp(x)), lo, hi, 8)
    _, alpha, gain = best
    return float(alpha), float(gain), ct_tau_ms(rec)


def _spread(errors, groups):
    """The standard deviation of the errors over the repeated bursts: pooled within the bursts of one length."""
    errors = np.asarray(errors, dtype=float)
    if groups is not None:
        groups = np.asarray(groups)
        squares, dof = 0.0, 0
        for g in np.unique(groups):
            e = errors[groups == g]
            if len(e) > 1:
                squares += float(np.sum((e - e.mean()) ** 2))
                dof += len(e) - 1
        if dof:
            return math.sqrt(squares / dof)
    return float(np.std(errors, ddof=1)) if len(errors) > 1 else 0.0


def _shortest(value, same):
    """The shortest decimal text of value for which same(float(text)) holds."""
    for digits in range(1, 10):
        text = np.format_float_positional(float(f"{value:.{digits}g}"), trim="-")
        if same(float(text)):
            return text
    return np.format_float_positional(value, trim="-")


def calibration_lines(alpha, gain, coefficients=None):
    """The calibration.h lines (Mk2_3phase_RFdatalog_temp) giving these coefficients (default: of alpha, gain)."""
    target = tuple(coefficients if coefficients is not None else coefficients_of(gain, alpha))
    gain_text = _shortest(gain, lambda g: coefficients_of(g, alpha)[4:] == target[4:])
    alpha_text = _shortest(alpha, lambda a: tuple(coefficients_of(float(gain_text), a)) == target)
    return (
        f"inline constexpr float lpf_gain{{ {gain_text} }}; /**< setting this to 0 disables this extra processing */\n"
        f"inline constexpr float alpha{{ {alpha_text} }};"
    )


def verdict(errors_off, errors_on, alpha=None, gain=None, groups_off=None, groups_on=None, coefficients=None):
    """ "no filter needed" when the filter's RMS improvement is below the spread over the repeated bursts;
    otherwise the calibration.h lines for alpha and gain."""
    rms_off, rms_on = _rms(errors_off), _rms(errors_on)
    spread = max(_spread(errors_off, groups_off), _spread(errors_on, groups_on))
    improvement = rms_off - rms_on
    figures = f"RMS {100 * rms_off:.3f} % without, {100 * rms_on:.3f} % with, spread {100 * spread:.3f} %"
    if not gain or improvement < spread:
        return f"no filter needed (the filter's improvement is within the spread over the repeated bursts: {figures})"
    return f"filter needed ({figures}); for calibration.h:\n" + calibration_lines(alpha, gain, coefficients)


def _describe(rec, path):
    h = rec.header
    filtered = bool(len(rec.filter_on) and rec.filter_on[0])
    setting = f"filter on (alpha {h.get('alpha')}, gain {h.get('gain')})" if filtered else "filter off"
    seconds = len(rec.v_raw) * float(h.get("spacing_us", 624)) * 1e-6
    return (
        f"Recording {path}: {h.get('router', '?')} router, board {h.get('board', '?')}, spacing"
        f" {h.get('spacing_us', '?')} us, {seconds:.1f} s, pattern {h.get('pattern', '?')}, {setting}"
    )


def _report_header(rec, path, lines):
    ctx = _context(rec)
    lines.append(_describe(rec, path))
    uptime = rec.header.get("uptime_s")
    if uptime is not None and uptime.isdigit() and int(uptime) < WARM_UPTIME_S:
        lines.append(
            f"warning: recorded {uptime} s after power-up: the DC removal settles within {WARM_UPTIME_S} s,"
            " the first bursts may be off"
        )
    overruns = int(rec.overrun.sum())
    lines.append(
        f"Overruns: {overruns} (frames lost to a full ring buffer or skipped bytes: {rec.dropped_bytes} bytes)"
    )
    k, shift, _ = ctx.fit(rec.i_out)
    lines.append(
        f"Reference: k = {k:.5f} (I out per voltage count), shift = {shift:+.3f} samples"
        f" ({shift * ctx.dt * 1e6:+.0f} us)"
    )
    if ctx.at_shift_limit(shift):
        lines.append(
            f"warning: the shift sits at its limit ({SHIFT_LIMIT_S * 1e3:g} ms): the phase between V and I is"
            " larger than the conversion order and a CT explain; the burst errors carry a constant offset"
        )
    startup = [b for b in ctx.bursts if b.excluded == "start-up"]
    excluded = [b for b in ctx.bursts if b.excluded and b.excluded != "start-up"]
    text = f"Bursts: {len(ctx.used)} used of {len(ctx.bursts)} ({len(startup)} in the first {STARTUP_S:g} s"
    for b in excluded:
        text += f"; burst {b.number} (t = {b.flag_on * ctx.dt:.1f} s) excluded: {b.excluded}"
    lines.append(text + ")")
    if ctx.delay_half_cycles:
        lines.append(f"The load conducts {ctx.delay_half_cycles} half-cycle(s) after the sketch switches it")


def _tail_lines(rec, lines):
    ctx = _context(rec)
    tau, tau_low, tau_high, c, sigma = ctx.tail()
    high = "above " + _duration(TAU_GRID[1]) if math.isinf(tau_high) else _duration(tau_high)
    if math.isnan(tau):
        lines.append(f"CT time constant: not measurable (switch-on residual {c / 64:.3f} +- {sigma / 64:.3f} counts)")
    else:
        lines.append(
            f"CT time constant: {_duration(tau)} ({_duration(tau_low)} to {high};"
            f" switch-on residual {c / 64:.2f} +- {sigma / 64:.2f} counts)"
        )
    late, unconfirmed = ctx.unsettled()
    needed = f"{math.log(1 / TAIL_LIMIT):.0f} of them needed"
    if late:
        off_s = min((b.span_end - b.end) * ctx.dt for b in late)
        bound = f"up to {high}" if not math.isinf(tau_high) else f"of {_duration(tau_low)} or more"
        lines.append(
            f"warning: the CT tail is not settled at the end of the OFF period ({off_s:.1f} s, against a CT time"
            f" constant {bound}: {needed}) in bursts {', '.join(str(b.number) for b in late)}:"
            " use longer OFF periods"
        )
    elif unconfirmed:
        lines.append(
            f"note: the settling of the CT tail cannot be confirmed at 10 bits: the CT time constant is"
            f" {_duration(tau_low)} or more, without an upper bound (its tail is a fraction of a count); the OFF"
            f" periods are long enough for {_duration(tau_low)} ({needed})"
        )


def _duration(seconds):
    return f"{seconds * 1000:.1f} ms" if seconds < 1 else f"{seconds:.2f} s"


def _plots(rec, outputs, directory):
    """switch_on.png and switch_off.png: the longest used burst, raw, filtered and reference, in ADC counts."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    ctx = _context(rec)
    burst = max(ctx.used, key=lambda b: b.on_cycles)
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    width = int(4 * ctx.period)
    window = np.zeros(ctx.n)
    window[burst.start : burst.end] = 1.0
    for name, edge in (("switch_on", burst.start), ("switch_off", burst.end)):
        s, e = max(edge - width, 0), min(edge + 2 * width, ctx.n)
        t = (np.arange(s, e) - edge) * ctx.dt * 1000.0
        fig, ax = plt.subplots(figsize=(10, 4))
        for label, current in outputs:
            ax.plot(t, np.asarray(current[s:e], dtype=float) / 64.0, label=label, linewidth=1)
        k, _, theta = ctx.fit(outputs[-1][1])
        ax.plot(t, (ctx.reference(k, theta) * window)[s:e] / 64.0, label="reference (k x V shifted)", linewidth=1)
        ax.set_xlabel(f"ms from the {name.replace('_', '-')} of burst {burst.number}")
        ax.set_ylabel("current minus DC (ADC counts)")
        ax.legend(loc="upper right")
        ax.grid(alpha=0.3)
        fig.tight_layout()
        fig.savefig(directory / f"{name}.png", dpi=100)
        plt.close(fig)


def analyse(path, plots=None):
    rec = read_recording(path)
    ctx = _context(rec)
    lines = []
    _report_header(rec, path, lines)
    alpha, gain, _ = search_filter(rec)
    _tail_lines(rec, lines)
    raw = ctx.raw
    best = firmware_current_from(ctx, alpha, gain)
    errors_off, errors_on = ctx.errors(raw), ctx.errors(best)
    net_off = ctx.errors(raw, tail=True)
    lines.append(f"Filter found: alpha {alpha:.6g}, gain {gain:.4g}" if gain else "Filter found: none (gain 0)")
    lines.append("Burst energy error, relative (ON cycles; 'with tail' runs on to the end of the OFF period):")
    lines.append("  burst      t  ON cycles   no filter   with tail   with the filter")
    for b, off, net, on in zip(ctx.used, errors_off, net_off, errors_on):
        lines.append(
            f"  {b.number:5d} {b.flag_on * ctx.dt:6.1f} {b.on_cycles:10d} {100 * off:10.3f} % {100 * net:9.3f} %"
            f" {100 * on:12.3f} %"
        )
    lines.append(f"RMS: no filter {100 * _rms(errors_off):.3f} %, with the filter {100 * _rms(errors_on):.3f} %")
    groups = [b.on_cycles for b in ctx.used]
    lines.append("Verdict: " + verdict(errors_off, errors_on, alpha, gain, groups, groups))
    outputs = [("raw", raw)]
    if gain:
        outputs.append((f"filtered (alpha {alpha:.4g}, gain {gain:.4g})", best))
    print("\n".join(lines))
    if plots:
        _plots(rec, outputs, plots)
        print(f"Plots written to {plots}")


def firmware_current_from(ctx, alpha, gain):
    """The firmware's output for alpha, gain from the context's raw samples and starting state."""
    if not gain:
        return ctx.raw
    coef = coefficients_of(gain, alpha)
    trace = low_pass_trace(ctx.raw, coef, ctx.state.lpf_x64, ctx.state.lpf_low_bits)
    return boosted(ctx.raw, *trace, coef)


def compare(path_off, path_on, plots=None):
    rec_off, rec_on = read_recording(path_off), read_recording(path_on)
    if bool(rec_off.filter_on.any()) or not bool(rec_on.filter_on.all()):
        raise AnalysisError("--compare takes the run with the filter off first, then the run with the filter on")
    lines = []
    for rec, path in ((rec_off, path_off), (rec_on, path_on)):
        _report_header(rec, path, lines)
    _tail_lines(rec_off, lines)
    ctx_off, ctx_on = _context(rec_off), _context(rec_on)
    errors_off, errors_on = ctx_off.errors(rec_off.i_out), ctx_on.errors(rec_on.i_out)
    lines.append(f"Filter off ({path_off}): RMS burst energy error {100 * _rms(errors_off):.3f} %")
    lines.append(f"Filter on ({path_on}): RMS burst energy error {100 * _rms(errors_on):.3f} %")
    alpha, gain = float(rec_on.header.get("alpha", "nan")), float(rec_on.header.get("gain", "nan"))
    verdict_text = verdict(
        errors_off,
        errors_on,
        alpha,
        gain,
        [b.on_cycles for b in ctx_off.used],
        [b.on_cycles for b in ctx_on.used],
        rec_on.coefficients,
    )
    lines.append("Verdict: " + verdict_text)
    print("\n".join(lines))
    if plots:
        _plots(rec_on, [("raw", ctx_on.raw), ("filtered (as recorded)", rec_on.i_out)], plots)
        print(f"Plots written to {plots}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("recording", help="a recording from ct_recorder.capture (with --compare: the filter off)")
    parser.add_argument("--compare", metavar="RUN2", help="the same load recorded with the filter on")
    parser.add_argument("--plots", metavar="DIR", help="write switch_on.png and switch_off.png to DIR")
    args = parser.parse_args(argv)
    try:
        if args.compare:
            compare(args.recording, args.compare, args.plots)
        else:
            analyse(args.recording, args.plots)
    except (AnalysisError, ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
