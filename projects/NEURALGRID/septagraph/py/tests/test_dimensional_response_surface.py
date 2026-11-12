"""Smoothing the per-pad profile into a response surface.

A rigid plate on four point sensors has a response that varies SMOOTHLY with
position -- it is a physical field, not 64 independent numbers. But the
measurement comes from 64 hand presses, and a hand is not a consistent
actuator. Fitting a low-order surface through all 64 samples rejects that
press-to-press variation while keeping the real spatial trend, and fills in
pads that were skipped or that no sensor felt.

24 coefficients (6 per sensor) from 256 measurements: heavily overdetermined,
which is exactly why it smooths.
"""
import pytest

from septagraph.dimensional.response_surface import fit_response_surface


def _linear_profile(noise=None):
    """Response falling off linearly with x -- a plausible physical field."""
    prof = {}
    for pad in range(64):
        x, y = pad % 8, pad // 8
        base = 1000 - 100 * x + 50 * y
        bump = (noise or {}).get(pad, 0)
        prof[pad] = (base + bump, base // 2, base // 4, base // 8)
    return prof


class TestFitting:
    def test_returns_a_value_for_every_pad(self):
        assert len(fit_response_surface(_linear_profile())) == 64

    def test_recovers_a_clean_linear_field_almost_exactly(self):
        fitted = fit_response_surface(_linear_profile())
        for pad in (0, 7, 27, 63):
            assert fitted[pad][0] == pytest.approx(_linear_profile()[pad][0], rel=0.02)

    def test_smooths_out_one_badly_hit_pad(self):
        # pad 27 was hit twice as hard as its neighbours; the surface should
        # not follow it there.
        measured = _linear_profile(noise={27: 900})
        fitted = fit_response_surface(measured)
        assert fitted[27][0] < measured[27][0] - 400

    def test_leaves_the_honest_neighbours_alone(self):
        fitted = fit_response_surface(_linear_profile(noise={27: 900}))
        clean = _linear_profile()
        assert fitted[26][0] == pytest.approx(clean[26][0], rel=0.15)

    def test_fills_in_a_pad_that_was_never_measured(self):
        measured = _linear_profile()
        del measured[27]
        fitted = fit_response_surface(measured)
        assert fitted[27][0] == pytest.approx(_linear_profile()[27][0], rel=0.05)

    def test_fills_in_a_whole_missing_row(self):
        measured = {p: v for p, v in _linear_profile().items() if p // 8 != 3}
        fitted = fit_response_surface(measured)
        assert fitted[27][0] == pytest.approx(_linear_profile()[27][0], rel=0.1)

    def test_fits_every_sensor_independently(self):
        fitted = fit_response_surface(_linear_profile())
        assert fitted[0][1] == pytest.approx(_linear_profile()[0][1], rel=0.05)
        assert fitted[0][3] == pytest.approx(_linear_profile()[0][3], rel=0.05)


class TestRobustness:
    def test_never_produces_a_negative_response(self):
        steep = {pad: (2000 - 300 * (pad % 8), 10, 10, 10) for pad in range(64)}
        fitted = fit_response_surface(steep)
        assert all(min(v) >= 0 for v in fitted.values())

    def test_too_few_samples_returns_the_measurements_untouched(self):
        sparse = {0: (500, 200, 60, 40), 9: (400, 180, 55, 38)}
        assert fit_response_surface(sparse) == sparse

    def test_an_empty_profile_is_handled(self):
        assert fit_response_surface({}) == {}

    def test_the_fit_is_deterministic(self):
        p = _linear_profile()
        assert fit_response_surface(p) == fit_response_surface(p)


class TestAppliedOnSave:
    """Saving fits the surface, so your own press-to-press variation does not
    become permanent calibration."""

    def _session(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        a = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        a.start_fsr_calibration()
        for _ in range(10):
            a.on_analog((0, 0, 0, 0, 0, 0, 0, 0))
        return a

    def _hit(self, a, pad, peak):
        """A full soft -> hard -> soft sweep: a stab is no longer accepted,
        because one sample says nothing about the force curve."""
        a.on_key(pad % 8, pad // 8, True)
        levels = [k / 6 for k in range(1, 7)]
        for f in levels + levels[-2::-1]:
            p = int(peak * f)
            a.on_analog((0, 0, 0, 0, p, p // 2, p // 4, p // 8))
        a.on_key(pad % 8, pad // 8, False)

    def test_saving_produces_a_profile_for_every_pad(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        a = self._session(tmp_path)
        for pad in range(64):
            self._hit(a, pad, 1000 - 8 * pad)
        a.finish_fsr_calibration()
        assert len(Calibration.load(tmp_path / "calibration.json").fsr_pads) == 64

    def test_one_wild_press_does_not_survive_into_the_calibration(self, tmp_path):
        a = self._session(tmp_path)
        for pad in range(64):
            self._hit(a, pad, 3000 if pad == 27 else 1000)
        a.finish_fsr_calibration()
        neighbour = a.encoder.calibration.fsr_pads[26][0]
        assert a.encoder.calibration.fsr_pads[27][0] < neighbour * 1.6

    def test_a_pad_you_skipped_is_filled_in_from_its_neighbours(self, tmp_path):
        a = self._session(tmp_path)
        for pad in range(64):
            if pad == 27:
                a.fsr.skip()
            else:
                self._hit(a, pad, 1000)
        a.finish_fsr_calibration()
        assert 27 in a.encoder.calibration.fsr_pads

    def test_the_raw_measurements_are_kept_so_it_can_be_refitted(self, tmp_path):
        a = self._session(tmp_path)
        for pad in range(64):
            self._hit(a, pad, 1000)
        raw = dict(a.fsr.result())
        a.finish_fsr_calibration()
        assert a.encoder.calibration.fsr_pads_raw == raw

    def test_too_few_pads_are_saved_unsmoothed(self, tmp_path):
        a = self._session(tmp_path)
        self._hit(a, 0, 1000)
        a.finish_fsr_calibration()
        assert set(a.encoder.calibration.fsr_pads) == {0}
