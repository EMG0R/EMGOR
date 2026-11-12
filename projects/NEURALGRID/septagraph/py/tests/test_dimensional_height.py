"""Height has to actually vary across the range the hand can be in.

z was normalized against Calibration.lidar_max_mm (1200mm), but the firmware
runs the sensors in SHORT distance mode and skin is a poor reflector, so the
real working range is a few centimetres. Everything in range therefore mapped
to ~1.0 -- the hand sat at maximum height permanently.

The usable span is learned from what the sensors actually return, the same way
force auto-ranges, so it fits the instrument instead of the datasheet.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import (
    Calibration, DEFAULT_LIDAR_MAP, NeuralGridEncoder,
)


def _ranges(top=None, bottom=None, left=None, right=None):
    out = [0xFFFF] * 4
    for key, mm in (("n", top), ("s", bottom), ("w", left), ("e", right)):
        if mm is not None:
            out[DEFAULT_LIDAR_MAP[key]] = mm
    return tuple(out)


def _enc():
    return NeuralGridEncoder(calibration=Calibration(lidar_max_mm=1200))


def _h(e, t):
    ch = e.frame(t_mono=t).channel("lidar")
    return ch.points[0].values[2] if ch.points else None


def _at(e, mm, t, n=1):
    """Hand at a given distance from the rig -- all sensors together, which is
    what moving AWAY looks like. An opposing pair moving in opposite directions
    is a sideways move and must not change this axis at all."""
    for _ in range(n):
        t += 0.05
        e.on_lidar(_ranges(top=mm, bottom=mm, left=mm, right=mm), t_mono=t)
    return t


def _sweep(e, lo, hi, t0=0.0, steps=24):
    """A hand moving out through the volume, so the range gets learned."""
    t = t0
    for k in range(steps):
        t = _at(e, int(lo + (hi - lo) * (k / (steps - 1))), t)
    return t


class TestHeightUsesTheRealRange:
    def test_a_close_hand_is_not_pinned_at_the_top(self):
        e = _enc()
        t = _sweep(e, 70, 240)
        t = _at(e, 70, t, n=90)         # close in
        assert _h(e, t) < 0.5

    def test_height_moves_across_the_range_actually_seen(self):
        e = _enc()
        t = _sweep(e, 70, 240)
        # height is damped hard on purpose, so give it time to arrive
        seen = []
        for mm in (70, 150, 240):
            t = _at(e, mm, t, n=90)
            seen.append(_h(e, t))
        # further reads higher
        assert seen[0] < seen[1] < seen[2]
        assert seen[2] - seen[0] > 0.4

    def test_furthest_is_highest(self):
        e = _enc()
        t = _sweep(e, 70, 240)
        t = _at(e, 70, t, n=90)
        near = _h(e, t)
        t = _at(e, 240, t, n=90)
        assert _h(e, t) > near

    def test_the_learned_span_is_reported(self):
        e = _enc()
        _sweep(e, 70, 240)
        lo, hi = e.raw()["lidar_span_mm"]
        assert lo <= 75 and hi >= 235

    def test_a_single_sensor_reports_height_at_low_confidence(self):
        # Height is how far the hand is overall, so one reading still says
        # something -- just not much, and sigma carries that.
        e = _enc()
        e.on_lidar(_ranges(top=100), t_mono=0.0)
        ch = e.frame(t_mono=0.0).channel("lidar")
        assert ch.points[0].sigma[2] > 0.4

    def test_the_span_only_ever_widens(self):
        e = _enc()
        _sweep(e, 70, 240)
        lo1, hi1 = e.raw()["lidar_span_mm"]
        _sweep(e, 100, 150, t0=5.0)
        lo2, hi2 = e.raw()["lidar_span_mm"]
        assert lo2 <= lo1 and hi2 >= hi1


class TestSidewaysIsNotHeight:
    def test_a_purely_lateral_move_leaves_the_third_axis_alone(self):
        # the bug this replaced: min(distances) tracked lateral position, so
        # sliding sideways read as rising
        e = _enc()
        t = _sweep(e, 70, 240)
        for _ in range(10):
            t += 0.05
            e.on_lidar(_ranges(left=80, right=220), t_mono=t)
        a = _h(e, t)
        for _ in range(10):
            t += 0.05
            e.on_lidar(_ranges(left=220, right=80), t_mono=t)
        assert _h(e, t) == pytest.approx(a, abs=0.06)


class TestSpanCanBeFrozen:
    """A normalization that keeps moving means the same gesture maps to
    different numbers on different days -- unusable as training data."""

    def test_a_frozen_span_is_used_instead_of_learning(self):
        e = NeuralGridEncoder(calibration=Calibration(lidar_span=(50.0, 250.0)))
        assert e.raw()["lidar_span_mm"] == (50, 250)

    def test_a_frozen_span_does_not_widen(self):
        e = NeuralGridEncoder(calibration=Calibration(lidar_span=(50.0, 250.0)))
        for k in range(20):
            e.on_lidar(_ranges(top=900, bottom=900, left=900, right=900), t_mono=k * 0.05)
        assert e.raw()["lidar_span_mm"] == (50, 250)

    def test_the_same_reading_maps_the_same_way_every_run(self):
        cal = Calibration(lidar_span=(50.0, 250.0))
        out = []
        for _ in range(2):
            e = NeuralGridEncoder(calibration=cal)
            t = 0.0
            for _ in range(6):
                t += 0.05
                e.on_lidar(_ranges(top=150, bottom=150, left=150, right=150), t_mono=t)
            out.append(_h(e, t))
        assert out[0] == pytest.approx(out[1])

    def test_what_was_learned_is_available_to_freeze(self):
        e = _enc()
        _sweep(e, 70, 240)
        lo, hi = e.learned_lidar_span
        assert lo <= 75 and hi >= 235

    def test_it_round_trips_in_the_file(self, tmp_path):
        cal = Calibration(lidar_span=(50.0, 250.0))
        p = tmp_path / "c.json"
        cal.save(p)
        assert Calibration.load(p).lidar_span == (50.0, 250.0)
