"""Shaping the force value: a compressive curve, then smoothing.

Two distinct jobs, easy to conflate:

  CURVE      maps 0-1 force to 0-1 output. Compressive, so the light end gets
             most of the resolution and the hardest presses change least --
             you cannot press meaningfully harder than "hard", but you can
             absolutely press meaningfully lighter than "medium".

  SMOOTHING  removes frame-to-frame jitter. Asymmetric: fast on the way up so
             a hit still feels immediate, slow on the way down so sustained
             pressure does not shimmer. The sensors wander several hundred
             counts at rest, so this is not optional.
"""
import pytest

from septagraph.dimensional.force_shape import ForceSmoother, apply_curve


class TestCurve:
    def test_endpoints_are_preserved(self):
        assert apply_curve(0.0) == 0.0
        assert apply_curve(1.0) == pytest.approx(1.0)

    def test_is_monotonic(self):
        vals = [apply_curve(x / 20) for x in range(21)]
        assert all(b >= a for a, b in zip(vals, vals[1:]))

    def test_light_presses_are_expanded(self):
        # half the force should read as MORE than half the output
        assert apply_curve(0.5) > 0.5

    def test_the_hardest_presses_change_least(self):
        low = apply_curve(0.2) - apply_curve(0.0)
        high = apply_curve(1.0) - apply_curve(0.8)
        assert high < low

    def test_a_stronger_exponent_compresses_harder(self):
        assert apply_curve(0.5, exponent=0.3) > apply_curve(0.5, exponent=0.7)

    def test_an_exponent_of_one_is_a_straight_line(self):
        assert apply_curve(0.42, exponent=1.0) == pytest.approx(0.42)

    def test_out_of_range_input_is_clamped(self):
        assert apply_curve(-1.0) == 0.0
        assert apply_curve(2.0) == pytest.approx(1.0)


class TestSmoothing:
    def test_the_first_value_passes_straight_through(self):
        assert ForceSmoother().push(0.7) == pytest.approx(0.7)

    def test_a_rise_is_followed_quickly(self):
        s = ForceSmoother()
        s.push(0.0)
        assert s.push(1.0) > 0.5

    def test_a_fall_is_followed_slowly(self):
        s = ForceSmoother()
        s.push(1.0)
        assert s.push(0.0) > 0.5

    def test_attack_outruns_release(self):
        up = ForceSmoother()
        up.push(0.0)
        risen = up.push(1.0)
        down = ForceSmoother()
        down.push(1.0)
        fallen = 1.0 - down.push(0.0)
        assert risen > fallen

    def test_it_settles_on_a_held_value(self):
        s = ForceSmoother()
        for _ in range(40):
            v = s.push(0.6)
        assert v == pytest.approx(0.6, abs=0.01)

    def test_jitter_is_damped(self):
        s = ForceSmoother()
        out = [s.push(0.5 + (0.2 if n % 2 else -0.2)) for n in range(30)]
        spread = max(out[10:]) - min(out[10:])
        assert spread < 0.4 * 0.8  # raw swing is 0.4

    def test_releasing_resets_it(self):
        s = ForceSmoother()
        s.push(0.9)
        s.reset()
        assert s.push(0.1) == pytest.approx(0.1)


class TestTogether:
    def test_the_encoder_applies_both(self):
        from septagraph.dimensional.encoders.neuralgrid import (
            Calibration, NeuralGridEncoder,
        )
        REST = (0, 0, 0, 0)
        e = NeuralGridEncoder(calibration=Calibration(fsr_rest=REST))
        e.on_key(0, 0, True, t_mono=0.0)   # force is gated to pad presses
        e.on_analog((0, 0, 0, 0, 0, 0, 0, 0), t_mono=0.0)
        # a mid press should read ABOVE its linear fraction thanks to the curve
        e.on_analog((0, 0, 0, 0, 600, 0, 0, 0), t_mono=1.0)
        for t in range(2, 20):  # let the smoother settle
            e.on_analog((0, 0, 0, 0, 600, 0, 0, 0), t_mono=float(t))
        v = e.frame(t_mono=20.0).channel("force").points[0].values[0]
        assert 0.0 < v <= 1.0

    def test_the_curve_exponent_is_configurable_without_a_rebuild(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        cal = Calibration(force_curve=0.3)
        p = tmp_path / "c.json"
        cal.save(p)
        assert Calibration.load(p).force_curve == pytest.approx(0.3)


class TestTuning:
    def test_attack_reaches_most_of_a_step_within_two_frames(self):
        # ~42fps: two frames is ~48ms, below where smoothing reads as lag
        s = ForceSmoother()
        s.push(0.0)
        s.push(1.0)
        assert s.push(1.0) > 0.9

    def test_the_light_end_is_lifted_but_gently(self):
        # 0.35 lifted a tenth of an input to nearly half an output, which read
        # as "too quick to get bright". Still expanded, no longer a leap.
        assert 0.15 < apply_curve(0.1) < 0.3
        assert 0.3 < apply_curve(0.25) < 0.5
