"""The analysis on synthetic recordings (3-phase spacing, default burst pattern)."""

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
