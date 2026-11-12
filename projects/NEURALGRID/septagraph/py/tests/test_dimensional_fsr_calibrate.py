"""Per-pad FSR calibration -- the one that gets re-run every housing open.

Lights one pad, you hit it, it records how all four force sensors answered,
and it moves to the next by itself. The result is a 64x4 response profile:
what a press at THAT spot does to each sensor. With it, force stops depending
on where you hit.

This is the measurement the user asked for from the start. It is not the
resting-offset step (that is the zero point) and not the sensor map (run once);
it is the one thing that genuinely changes every time the housing is opened.
"""
import pytest

from septagraph.dimensional.fsr_calibrate import FsrPadSession

REST = (3300, 1400, 0, 0)  # measured on hardware


def _press(s, pad=None, peaks=(500, 200, 60, 40), steps=6):
    """Sweep the lit pad: soft -> hard -> soft, as the user performs it.

    Not a stab: the whole point of the sweep is that the mixing ratio is fitted
    across the range rather than read off one sample.
    """
    pad = s.current_pad if pad is None else pad
    s.on_key(pad % 8, pad // 8, True)
    levels = [k / steps for k in range(1, steps + 1)]
    for f in levels + levels[-2::-1]:
        s.on_analog((0, 0, 0, 0) + tuple(REST[i] + int(peaks[i] * f) for i in range(4)))
    s.on_key(pad % 8, pad // 8, False)


class TestWalkingTheGrid:
    def test_starts_on_the_first_pad(self):
        assert FsrPadSession(rest=REST).current_pad == 0

    def test_lights_only_the_pad_being_calibrated(self):
        s = FsrPadSession(rest=REST)
        frame = s.led_frame()
        assert frame[0] != (0, 0, 0)
        assert all(px == (0, 0, 0) for px in frame[1:])

    def test_the_lit_pad_follows_the_session(self):
        s = FsrPadSession(rest=REST)
        s.skip()
        frame = s.led_frame()
        assert frame[0] == (0, 0, 0) and frame[1] != (0, 0, 0)

    def test_covers_every_pad(self):
        assert FsrPadSession(rest=REST).total == 64

    def test_reports_progress(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        assert s.done == 1

    def test_is_complete_after_the_last_pad(self):
        s = FsrPadSession(rest=REST)
        for _ in range(64):
            s.skip()
        assert s.complete and s.current_pad is None

    def test_a_complete_session_lights_nothing(self):
        s = FsrPadSession(rest=REST)
        for _ in range(64):
            s.skip()
        assert all(px == (0, 0, 0) for px in s.led_frame())


class TestSweepQuality:
    def test_a_stab_is_not_accepted_as_a_sweep(self):
        # no range covered -- it says nothing about the force curve
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        for _ in range(5):
            s.on_analog((0, 0, 0, 0) + tuple(REST[i] + (600, 300, 100, 50)[i] for i in range(4)))
        s.on_key(0, 0, False)
        assert 0 not in s.result() and 0 in s.unfelt

    def test_the_sweep_progress_is_visible_while_holding(self):
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        s.on_analog((0, 0, 0, 0) + tuple(REST[i] + (100, 40, 12, 8)[i] for i in range(4)))
        s.on_analog((0, 0, 0, 0) + tuple(REST[i] + (900, 360, 108, 72)[i] for i in range(4)))
        assert s.state()["span"] > 500

    def test_quality_is_reported_per_pad(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        q = s.quality[0]
        assert q["stability"] > 0.8 and "hysteresis" in q and q["span"] > 0


class TestRecordingAPress:
    def test_records_the_ratio_every_sensor_answered_in(self):
        s = FsrPadSession(rest=REST)
        _press(s, peaks=(500, 200, 60, 40))
        col = s.result()[0]
        norm = [c / col[0] for c in col]
        assert norm == pytest.approx([1.0, 0.4, 0.12, 0.08], abs=0.03)

    def test_subtracts_the_resting_load_the_housing_applies(self):
        s = FsrPadSession(rest=REST)
        _press(s, peaks=(500, 0, 0, 0))
        assert s.result()[0][0] < 600  # not ~3800

    def test_one_bad_sample_does_not_define_the_pad(self):
        # the reason for sweeping: a spike at peak flex would otherwise BE the
        # calibration for that pad
        clean = FsrPadSession(rest=REST)
        _press(clean, peaks=(500, 200, 60, 40))
        spiked = FsrPadSession(rest=REST)
        spiked.on_key(0, 0, True)
        levels = [k / 6 for k in range(1, 7)]
        for n, f in enumerate(levels + levels[-2::-1]):
            vals = [REST[i] + int((500, 200, 60, 40)[i] * f) for i in range(4)]
            if n == 5:
                vals[0] += 900
            spiked.on_analog((0, 0, 0, 0) + tuple(vals))
        spiked.on_key(0, 0, False)
        a, b = clean.result()[0], spiked.result()[0]
        assert max(abs(x - y) / max(x, 1) for x, y in zip(a, b)) < 0.2

    def test_advances_by_itself_when_the_pad_is_released(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        assert s.current_pad == 1

    def test_ignores_a_pad_that_is_not_the_one_lit(self):
        s = FsrPadSession(rest=REST)
        _press(s, pad=40)
        assert s.current_pad == 0 and s.result() == {}

    def test_a_press_that_moved_nothing_is_not_recorded(self):
        # a dead pad, or one no sensor can feel
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        s.on_analog(tuple(REST))
        s.on_key(0, 0, False)
        assert 0 not in s.result()

    def test_a_pad_no_sensor_felt_is_reported_so_it_can_be_chased(self):
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        s.on_analog(tuple(REST))
        s.on_key(0, 0, False)
        assert 0 in s.unfelt


class TestCorrections:
    def test_skipping_moves_on_without_recording(self):
        s = FsrPadSession(rest=REST)
        s.skip()
        assert s.current_pad == 1 and s.result() == {}

    def test_can_go_back_to_a_pad(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        s.goto(0)
        assert s.current_pad == 0

    def test_going_back_clears_that_pads_reading(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        s.goto(0)
        assert 0 not in s.result()

    def test_re_pressing_replaces_the_old_reading(self):
        s = FsrPadSession(rest=REST)
        _press(s, peaks=(500, 0, 0, 0))
        first = s.result()[0][0]
        s.goto(0)
        _press(s, peaks=(1400, 0, 0, 0))
        assert s.result()[0][0] > first * 1.5


class TestUsingTheProfile:
    def test_a_press_is_normalized_against_that_pads_own_profile(self):
        # the whole point: the same force at two spots should read the same
        from septagraph.dimensional.fsr_calibrate import force_from_profile
        profile = {0: (500, 200, 60, 40), 63: (40, 60, 200, 500)}
        near = force_from_profile((500, 200, 60, 40), 0, profile)
        far = force_from_profile((40, 60, 200, 500), 63, profile)
        assert near == pytest.approx(far)

    def test_pressing_harder_than_calibrated_reads_above_the_reference(self):
        from septagraph.dimensional.fsr_calibrate import force_from_profile
        profile = {0: (500, 200, 60, 40)}
        assert force_from_profile((1000, 400, 120, 80), 0, profile) > 1.0

    def test_an_uncalibrated_pad_falls_back_to_the_plain_sum(self):
        from septagraph.dimensional.fsr_calibrate import force_from_profile
        assert force_from_profile((100, 100, 0, 0), 7, {}) is None


class TestPersistence:
    def test_round_trips_through_json(self, tmp_path):
        s = FsrPadSession(rest=REST)
        _press(s, peaks=(500, 200, 60, 40))
        path = tmp_path / "fsr_pads.json"
        s.save(path)
        assert FsrPadSession.load_profile(path) == s.result()

    def test_a_partial_session_still_saves(self, tmp_path):
        s = FsrPadSession(rest=REST)
        _press(s)
        s.skip()
        path = tmp_path / "fsr_pads.json"
        s.save(path)
        assert len(FsrPadSession.load_profile(path)) == 1

    def test_the_resting_offsets_are_saved_alongside(self, tmp_path):
        s = FsrPadSession(rest=REST)
        _press(s)
        path = tmp_path / "fsr_pads.json"
        s.save(path)
        assert FsrPadSession.load_rest(path) == REST


def _wire(peaks):
    """Full 8-channel ANALOG frame: 4 softpots (untouched) + 4 FSRs at rest+peak."""
    return (0, 0, 0, 0) + tuple(REST[i] + peaks[i] for i in range(4))


class TestEncoderUsesTheProfile:
    """Force stops depending on where you hit."""

    def _enc(self, profile):
        from septagraph.dimensional.encoders.neuralgrid import (
            Calibration, NeuralGridEncoder,
        )
        cal = Calibration(fsr_rest=REST, fsr_pads=profile)
        return NeuralGridEncoder(calibration=cal)

    def test_the_same_force_at_two_corners_reads_the_same(self):
        profile = {0: (500, 200, 60, 40), 63: (40, 60, 200, 500)}
        e = self._enc(profile)
        e.on_key(0, 0, True, t_mono=1.0)
        e.on_analog(_wire((500, 200, 60, 40)), t_mono=1.0)
        near = e.frame(t_mono=1.0).channel("force").points[0].values[0]

        e2 = self._enc(profile)
        e2.on_key(7, 7, True, t_mono=1.0)
        e2.on_analog(_wire((40, 60, 200, 500)), t_mono=1.0)
        far = e2.frame(t_mono=1.0).channel("force").points[0].values[0]

        assert near == pytest.approx(far, abs=0.02)

    def test_without_a_profile_the_raw_totals_disagree_across_the_grid(self):
        # Proves the test above measures the profile, not a coincidence.
        # Rest first: with a single sample the encoder cannot tell a resting
        # level from a press, so both would normalize to zero by design.
        e = self._enc({})
        e.on_key(0, 0, True, t_mono=0.0)
        e.on_analog(_wire((0, 0, 0, 0)), t_mono=0.0)
        e.on_analog(_wire((500, 200, 60, 40)), t_mono=1.0)
        a = e.frame(t_mono=1.0).channel("force").points[0].values[0]
        e2 = self._enc({})
        e2.on_key(0, 0, True, t_mono=0.0)
        e2.on_analog(_wire((0, 0, 0, 0)), t_mono=0.0)
        e2.on_analog(_wire((40, 6, 20, 50)), t_mono=1.0)
        b = e2.frame(t_mono=1.0).channel("force").points[0].values[0]
        assert a != pytest.approx(b, abs=0.02)

    def test_an_uncalibrated_pad_still_produces_force(self):
        e = self._enc({0: (500, 200, 60, 40)})
        e.on_key(3, 3, True, t_mono=1.0)  # pad 27, no profile
        e.on_analog(_wire((300, 300, 300, 300)), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").active

    def test_the_profile_round_trips_in_the_calibration_file(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        cal = Calibration(fsr_rest=REST, fsr_pads={0: (500, 200, 60, 40)})
        p = tmp_path / "c.json"
        cal.save(p)
        assert Calibration.load(p).fsr_pads[0] == (500, 200, 60, 40)


class TestItMeasuresItsOwnRest:
    """The resting load belongs to this program too -- it changes with the
    housing exactly like the pad profile does. Starting without it would fold
    the housing's pre-load into every pad reading."""

    def test_starts_by_watching_the_resting_load(self):
        s = FsrPadSession()
        assert s.phase == "rest"

    def test_does_not_light_a_pad_while_measuring_rest(self):
        s = FsrPadSession()
        assert all(px == (0, 0, 0) for px in s.led_frame())

    def test_moves_on_to_the_pads_once_rest_has_settled(self):
        s = FsrPadSession()
        for _ in range(12):
            s.on_analog((0, 0, 0, 0) + REST)
        assert s.phase == "pads" and s.current_pad == 0

    def test_it_learns_the_resting_load(self):
        s = FsrPadSession()
        for _ in range(12):
            s.on_analog((0, 0, 0, 0) + REST)
        assert s.rest == REST

    def test_a_known_rest_skips_the_phase(self):
        assert FsrPadSession(rest=REST).phase == "pads"

    def test_presses_are_ignored_until_rest_is_known(self):
        s = FsrPadSession()
        s.on_key(0, 0, True)
        s.on_analog((0, 0, 0, 0) + REST)
        s.on_key(0, 0, False)
        assert s.result() == {}


class TestCurvesAreKept:
    """The raw sweep is the evidence. Collapsing it to one vector and throwing
    the samples away means a rejected pad cannot be diagnosed, and the response
    can never be plotted or refitted with a better model."""

    def test_every_sample_of_the_sweep_is_kept(self):
        s = FsrPadSession(rest=REST)
        _press(s, steps=6)
        assert len(s.curves[0]) == 11        # 6 up + 5 down

    def test_a_curve_is_four_sensors_wide(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        assert all(len(row) == 4 for row in s.curves[0])

    def test_the_curve_rises_and_falls(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        totals = [sum(r) for r in s.curves[0]]
        assert totals[0] < max(totals) > totals[-1]

    def test_a_rejected_pad_keeps_its_curve_so_it_can_be_diagnosed(self):
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        for _ in range(5):
            s.on_analog((0, 0, 0, 0) + tuple(REST[i] + (600, 300, 100, 50)[i] for i in range(4)))
        s.on_key(0, 0, False)
        assert 0 in s.curves and 0 in s.unfelt

    def test_why_a_pad_was_rejected_is_recorded(self):
        s = FsrPadSession(rest=REST)
        s.on_key(0, 0, True)
        for _ in range(5):
            s.on_analog((0, 0, 0, 0) + tuple(REST[i] + (600, 300, 100, 50)[i] for i in range(4)))
        s.on_key(0, 0, False)
        assert s.quality[0]["reason"] == "stab"

    def test_curves_survive_the_save(self, tmp_path):
        s = FsrPadSession(rest=REST)
        _press(s)
        p = tmp_path / "fsr_pads.json"
        s.save(p)
        assert FsrPadSession.load_curves(p)[0] == s.curves[0]

    def test_redoing_a_pad_drops_its_old_curve(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        s.goto(0)
        assert 0 not in s.curves


class TestQualityReachesTheUi:
    def test_state_carries_quality_for_every_recorded_pad(self):
        s = FsrPadSession(rest=REST)
        _press(s)
        assert 0 in s.state()["quality_all"]
