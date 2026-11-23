"""The analysis on synthetic recordings (3-phase spacing, default burst pattern)."""

import math
import re

import numpy as np
import pytest

from ct_recorder import analyse
from ct_recorder.analyse import AnalysisError, burst_energy_errors, find_bursts, fit_reference, search_filter
from ct_recorder.firmware_path import firmware_current
from ct_recorder.recording import read_recording
from ct_recorder.synthetic import make_recording

SECONDS = 60.0


def rms(errors):
    return float(np.sqrt(np.mean(np.square(errors))))


def raw_current(rec):
    return firmware_current(rec.v_raw, rec.i_raw, state=rec.initial_state)


@pytest.fixture(scope="module")
def highpass(tmp_path_factory):
    """A CT with a 15 ms high-pass, filter off, and the filter the search finds for it."""
    path = make_recording(tmp_path_factory.mktemp("hp") / "off.ctr", seconds=SECONDS, tau_s=0.015)
    rec = read_recording(path)
    return path, rec, search_filter(rec)


def test_finds_filter_for_highpass_ct(highpass):
    _, rec, (alpha, gain, ct_tau_ms) = highpass
    off = burst_energy_errors(rec, raw_current(rec))
    on = burst_energy_errors(rec, firmware_current(rec.v_raw, rec.i_raw, alpha, gain, state=rec.initial_state))
    assert len(off) == len(on) >= 8
    assert gain > 0
    assert rms(on) < 0.5 * rms(off)
    assert abs(ct_tau_ms - 15.0) < 0.2 * 15.0


def test_no_filter_for_ideal_ct(tmp_path, capsys):
    path = make_recording(tmp_path / "ideal.ctr", seconds=SECONDS, tau_s=None)
    plots = tmp_path / "plots"
    assert analyse.main([str(path), "--plots", str(plots)]) == 0
    out = capsys.readouterr().out
    assert "no filter needed" in out
    assert "warning" not in out.lower()
    assert (plots / "switch_on.png").stat().st_size > 0 and (plots / "switch_off.png").stat().st_size > 0


def test_compare_reports_improvement(highpass, tmp_path, capsys):
    path_off, rec_off, (alpha, gain, _) = highpass
    path_on = make_recording(tmp_path / "on.ctr", seconds=SECONDS, tau_s=0.015, ct_filter=(alpha, gain))
    rec_on = read_recording(path_on)
    assert rec_on.filter_on.all()
    rms_off = rms(burst_energy_errors(rec_off, rec_off.i_out))
    rms_on = rms(burst_energy_errors(rec_on, rec_on.i_out))
    assert rms_on < 0.5 * rms_off
    assert analyse.main([str(path_off), "--compare", str(path_on)]) == 0
    out = capsys.readouterr().out
    assert f"{100 * rms_off:.3f} %" in out
    assert f"{100 * rms_on:.3f} %" in out
    assert "inline constexpr float lpf_gain{" in out and "inline constexpr float alpha{" in out


def test_skips_startup(tmp_path):
    """The first 5 s sit 250 counts higher: V clips at 1023 and the DC removal lags behind.

    50-cycle bursts every 3 s, so that steady ON cycles fall inside the step (the default pattern has none in the
    first 7 s). Without the skip, k moves by 0.3 %: the brief's 0.5 % would not see it, hence 0.1 %.
    """
    pattern = ((50, 100),)
    clean = read_recording(make_recording(tmp_path / "clean.ctr", seconds=30.0, tau_s=0.015, pattern=pattern))
    stepped = read_recording(
        make_recording(tmp_path / "step.ctr", seconds=30.0, tau_s=0.015, pattern=pattern, dc_step=250)
    )
    assert (stepped.v_raw[:100] == 1023).any()
    assert stepped.load_on[: int(5.0 / 624e-6)].any()  # the sketch still switches during the step
    k_clean, shift_clean = fit_reference(clean)
    k_stepped, shift_stepped = fit_reference(stepped)
    assert abs(k_stepped / k_clean - 1) < 0.001
    assert abs(shift_stepped - shift_clean) < 0.01


def test_bursts_with_overrun_excluded(tmp_path, capsys):
    clean = read_recording(make_recording(tmp_path / "clean.ctr", seconds=SECONDS, tau_s=0.015))
    path = make_recording(tmp_path / "gap.ctr", seconds=SECONDS, tau_s=0.015, overrun_at=(30.0,), uptime_s=60)
    rec = read_recording(path)
    assert rec.overrun.sum() == 1
    excluded = [b for b in find_bursts(rec) if b.excluded == "overrun"]
    assert len(excluded) == 1
    gap = int(np.flatnonzero(rec.overrun)[0])
    assert excluded[0].flag_on <= gap <= excluded[0].flag_next
    assert len(burst_energy_errors(rec, rec.i_out)) == len(burst_energy_errors(clean, clean.i_out)) - 1
    assert analyse.main([str(path)]) == 0
    out = capsys.readouterr().out
    assert f"burst {excluded[0].number} " in out and "overrun" in out
    assert "warning: recorded 60 s after power-up" in out  # the DC removal may not have settled


def test_refuses_without_load_current(tmp_path, capsys):
    path = make_recording(tmp_path / "noload.ctr", seconds=30.0, tau_s=None, i_amplitude=0)
    with pytest.raises(AnalysisError, match="no load current"):
        fit_reference(read_recording(path))
    assert analyse.main([str(path)]) != 0
    assert "no load current" in capsys.readouterr().err


def test_warns_when_tail_not_settled(tmp_path, capsys):
    path = make_recording(tmp_path / "slow.ctr", seconds=SECONDS, tau_s=2.0)
    rec = read_recording(path)
    assert rec.header["pattern"] == "5,150,10,150,25,150,50,150"
    assert analyse.main([str(path)]) == 0
    out = capsys.readouterr().out
    warnings = [line for line in out.splitlines() if line.lower().startswith("warning") and "tail" in line]
    assert len(warnings) == 1
    used = [b.number for b in find_bursts(rec) if not b.excluded]
    listed = re.search(r"bursts ([0-9, ]+)", warnings[0]).group(1)
    assert used and set(used) <= {int(x) for x in listed.replace(",", " ").split()}


def test_no_filter_at_single_phase_spacing_with_phase(tmp_path, capsys):
    """312 us, CT2 converted 208 us after V, 2 deg more from the divider and the CT: 1.02 samples of shift.

    The shift's limit is a time (1 ms), not a sample: the fit is not clamped, no warning, no filter needed.
    """
    path = make_recording(
        tmp_path / "ct2.ctr", seconds=40.0, spacing_us=312, tau_s=None, i_delay_us=208, i_phase_deg=2.0
    )
    rec = read_recording(path)
    expected = (208e-6 + 2.0 / 360 / 50) / 312e-6
    _, shift = fit_reference(rec)
    assert expected > 1.0 and abs(shift - expected) < 0.02
    assert analyse.main([str(path)]) == 0
    out = capsys.readouterr().out
    assert "no filter needed" in out
    assert "warning" not in out.lower()


def test_unbounded_tail_says_settling_cannot_be_confirmed(tmp_path):
    """A slow CT: its time constant has no upper bound at 10 bits. The OFF periods (3 s) cover the lower bound:
    a note that the settling cannot be confirmed, not "use longer OFF periods"; a lower bound beyond them warns."""
    rec = read_recording(make_recording(tmp_path / "slow.ctr", seconds=30.0, tau_s=None))
    ctx = analyse._context(rec)
    ctx._tail = (20.0, 0.8, math.inf, -13.0, 1.0)  # tau, tau_low, tau_high (unbounded), C, its standard error
    lines = []
    analyse._tail_lines(rec, lines)
    assert not any(line.startswith("warning") for line in lines)
    note = [line for line in lines if line.startswith("note:")]
    assert len(note) == 1 and "cannot be confirmed at 10 bits" in note[0] and "800.0 ms or more" in note[0]
    assert "longer OFF" not in note[0]
    ctx._tail = (20.0, 2.0, math.inf, -13.0, 1.0)
    lines = []
    analyse._tail_lines(rec, lines)
    warnings = [line for line in lines if line.startswith("warning")]
    assert len(warnings) == 1 and "not settled" in warnings[0] and "use longer OFF periods" in warnings[0]


def test_refuses_a_current_not_proportional_to_the_voltage(tmp_path, capsys):
    """The load on another phase: the current 120 deg from V. An ideal CT, so nothing else to blame: no filter
    may come out of it."""
    path = make_recording(tmp_path / "phase.ctr", seconds=SECONDS, tau_s=None, i_phase_deg=120.0)
    with pytest.raises(AnalysisError, match="current not proportional to the voltage"):
        fit_reference(read_recording(path))
    assert analyse.main([str(path)]) != 0
    captured = capsys.readouterr()
    assert "not resistive, or on another phase" in captured.err
    assert "filter needed" not in captured.out and "lpf_gain" not in captured.out


def test_thermostat_cut_out_bursts_excluded(tmp_path, capsys):
    """The heater's thermostat cuts out at 50 s of 90 s: the sketch still switches, no current flows. The bursts
    without current are left out and listed; the filter and the CT time constant come from the others."""
    path = make_recording(tmp_path / "cut.ctr", seconds=90.0, tau_s=0.015, current_off_s=50.0)
    rec = read_recording(path)
    bursts = find_bursts(rec)
    cut = [b for b in bursts if b.excluded == "no current (thermostat?)"]
    dt = 624e-6
    assert cut and all(b.flag_off * dt > 50.0 for b in cut)  # their ON cycles reach past the cut-out
    assert all(b.flag_off * dt < 50.0 for b in bursts if b.used)
    alpha, gain, tau_ms = search_filter(rec)
    assert gain > 0 and abs(tau_ms - 15.0) < 0.2 * 15.0
    assert analyse.main([str(path)]) == 0
    out = capsys.readouterr().out
    assert "filter needed" in out
    for b in cut:
        assert f"burst {b.number} " in out
    assert "no current (thermostat?)" in out


def test_refuses_when_too_few_bursts_carry_current(tmp_path):
    """The thermostat cuts out at 24 s: three bursts with current are left after the start-up, too few to judge
    a filter."""
    path = make_recording(tmp_path / "cut.ctr", seconds=60.0, tau_s=0.015, current_off_s=24.0)
    with pytest.raises(AnalysisError, match="too few bursts"):
        fit_reference(read_recording(path))


def test_shift_at_limit_with_a_small_residual_warns_without_refusing(tmp_path, capsys):
    """20 deg more than the conversion order: the shift sits at its 1 ms limit, the fit still leaves only a few %
    of the current. A warning, not a refusal (a 15 ms CT on CT2 of the single-phase router is close to the limit
    too)."""
    path = make_recording(tmp_path / "ph20.ctr", seconds=SECONDS, tau_s=None, i_phase_deg=20.0)
    assert analyse.main([str(path)]) == 0
    assert "warning: the shift sits at its limit" in capsys.readouterr().out


def test_fast_ct_on_single_phase_ct2_is_not_refused(tmp_path):
    """312 us, CT2 208 us after V, 2 deg, a 15 ms CT: the shift is close to its limit, the current proportional."""
    path = make_recording(
        tmp_path / "ct2.ctr", seconds=40.0, spacing_us=312, tau_s=0.015, i_delay_us=208, i_phase_deg=2.0
    )
    _, shift = fit_reference(read_recording(path))
    assert 0.9e-3 < shift * 312e-6 < analyse.SHIFT_LIMIT_S
