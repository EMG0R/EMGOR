"""What the soft -> hard -> soft sweep is actually for.

Keeping only the peak of that sweep throws it away: one sample per pad, at the
noisiest possible moment (maximum flex), and no way to tell whether the
sensors' ratio to each other even holds across the force range.

The sweep gives, per pad:
  column      the sensor mixing ratio, FITTED over every sample
  span        how much force range was actually covered
  stability   whether that ratio holds from soft to hard
  hysteresis  how far the release branch differs from the press branch
"""
import pytest

from septagraph.dimensional.sweep import analyze_sweep

# a press at this pad loads the sensors in a fixed 10:5:2:1 ratio
RATIO = (1.0, 0.5, 0.2, 0.1)


def _sweep(peak=1000, steps=6, noise=None, ratio=RATIO):
    """soft -> hard -> soft, as the user actually performs it.

    `noise` perturbs ONE sensor on a given sample -- what a bad reading really
    looks like. Perturbing all four equally would just rescale that sample.
    """
    levels = [peak * k / steps for k in range(1, steps + 1)]
    levels += levels[-2::-1]
    out = []
    for n, lv in enumerate(levels):
        row = [lv * r for r in ratio]
        if noise and n in noise:
            row[0] += noise[n]
        out.append(tuple(int(v) for v in row))
    return out


class TestFittingTheColumn:
    def test_recovers_the_mixing_ratio(self):
        col = analyze_sweep(_sweep()).column
        norm = [c / col[0] for c in col]
        assert norm == pytest.approx(list(RATIO), abs=0.02)

    def test_uses_every_sample_not_just_the_peak(self):
        # one wild sample at the peak must barely move a fitted column, where
        # it would completely define a peak-only one
        clean = analyze_sweep(_sweep()).column
        spiked = analyze_sweep(_sweep(noise={5: 900})).column
        drift = max(abs(a - b) / max(a, 1) for a, b in zip(clean, spiked))
        assert drift < 0.35

    def test_the_column_is_scaled_near_the_top_of_the_sweep(self):
        # force == 1.0 still means "about as hard as you calibrated it" --
        # deliberately a high percentile rather than the single hardest
        # instant, which is also the likeliest sample to be bad.
        r = analyze_sweep(_sweep(peak=1000))
        true_peak = 1000 * sum(RATIO)
        assert 0.7 * true_peak <= sum(r.column) <= 1.05 * true_peak

    def test_one_spike_at_peak_does_not_redefine_the_scale(self):
        clean = sum(analyze_sweep(_sweep()).column)
        spiked = sum(analyze_sweep(_sweep(noise={5: 900})).column)
        assert spiked == pytest.approx(clean, rel=0.1)


class TestQualityOfTheSweep:
    def test_reports_the_force_range_covered(self):
        assert analyze_sweep(_sweep(peak=1000)).span > 500

    def test_a_single_stab_is_not_a_sweep(self):
        stab = [(1000, 500, 200, 100)] * 4
        assert not analyze_sweep(stab).is_sweep

    def test_a_real_sweep_is_recognised(self):
        assert analyze_sweep(_sweep()).is_sweep

    def test_a_stable_ratio_reports_high_stability(self):
        assert analyze_sweep(_sweep()).stability > 0.9

    def test_a_ratio_that_drifts_with_force_is_flagged(self):
        # sensor 1 saturates as force rises -- the ratio is not constant
        rows = []
        for k in list(range(1, 7)) + list(range(5, 0, -1)):
            f = k * 150
            rows.append((int(f), int(min(f * 0.5, 300)), int(f * 0.2), int(f * 0.1)))
        assert analyze_sweep(rows).stability < 0.9


class TestHysteresis:
    def test_a_symmetric_sweep_shows_none(self):
        assert analyze_sweep(_sweep()).hysteresis == pytest.approx(0.0, abs=0.05)

    def test_a_lagging_release_is_measured(self):
        # FSRs read high on the way back down -- real, and worth knowing
        up = [(k * 150, 0, 0, 0) for k in range(1, 7)]
        down = [(int(k * 150 * 1.4), 0, 0, 0) for k in range(5, 0, -1)]
        assert analyze_sweep(up + down).hysteresis > 0.2


class TestDegenerate:
    def test_an_empty_sweep_is_handled(self):
        r = analyze_sweep([])
        assert r.column == (0, 0, 0, 0) and not r.is_sweep

    def test_a_sweep_nothing_felt_is_not_a_sweep(self):
        assert not analyze_sweep([(0, 0, 0, 0)] * 8).is_sweep


class TestWeaklyCoupledPads:
    """Pads far from every FSR produce a small but perfectly real sweep.

    An absolute span threshold rejects them for being quiet, which is a
    different thing from being a stab. Observed on hardware: a contiguous block
    of ten pads all failed together -- a spatial pattern, i.e. coupling, not
    technique.
    """

    def test_a_small_but_full_sweep_is_accepted(self):
        weak = _sweep(peak=90)          # a tenth of a well-coupled pad
        assert analyze_sweep(weak).is_sweep

    def test_a_stab_is_still_rejected_however_hard(self):
        stab = [(2000, 1000, 400, 200)] * 8
        assert not analyze_sweep(stab).is_sweep

    def test_what_makes_a_sweep_is_range_relative_to_its_own_peak(self):
        assert analyze_sweep(_sweep(peak=90)).is_sweep
        assert analyze_sweep(_sweep(peak=5000)).is_sweep

    def test_something_far_too_faint_to_trust_is_still_rejected(self):
        assert not analyze_sweep(_sweep(peak=8)).is_sweep

    def test_the_reason_for_rejection_is_reported(self):
        assert analyze_sweep([(2000, 1000, 400, 200)] * 8).reason == "stab"
        assert analyze_sweep(_sweep(peak=8)).reason == "too_faint"
        assert analyze_sweep(_sweep()).reason is None
