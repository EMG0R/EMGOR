"""Smooth the HANDOVER between rangefinders, not the steady state.

Two separate things get called "smoothing" and they cost different amounts:

  steady tracking  -- slowing this is what reads as LATENCY. Left fast.
  handover         -- when the set of contributing sensors changes, the answer
                      genuinely jumps because it is now solved from different
                      information. Easing across that costs nothing, because
                      while it happens the hand has not moved much anyway.

So: fast normally, cross-faded across a change. The target is always the NEW
answer -- this is not holding the old position, which was a separate bug.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import (
    Calibration, DEFAULT_LIDAR_MAP, GRID_MM, NeuralGridEncoder,
)
from septagraph.dimensional.trilaterate import SENSOR_AT


def _hand(x, y, which=("n", "s", "e", "w")):
    out = [0xFFFF] * 4
    for pos in which:
        sx, sy = SENSOR_AT[pos]
        d = ((x - sx) ** 2 + (y - sy) ** 2) ** 0.5
        out[DEFAULT_LIDAR_MAP[pos]] = int(d * GRID_MM)
    return tuple(out)


def _enc():
    return NeuralGridEncoder(calibration=Calibration(lidar_min_mm=20, grid_rotation=0))


def _pt(e, t):
    ch = e.frame(t_mono=t).channel("lidar")
    return ch.points[0].values if ch.points else None


def _hold(e, frame, n, t):
    for _ in range(n):
        t += 0.05
        e.on_lidar(frame, t_mono=t)
    return t


class TestSteadyTrackingStaysFast:
    def test_an_ordinary_move_is_followed_in_a_couple_of_frames(self):
        e = _enc()
        t = _hold(e, _hand(0.5, 0.25), 20, 0.0)
        start = _pt(e, t)[1]
        t = _hold(e, _hand(0.5, 0.75), 3, t)
        moved = abs(_pt(e, t)[1] - start)
        assert moved > 0.25, "steady tracking must not feel laggy"

    def test_it_arrives_fully_soon_after(self):
        e = _enc()
        t = _hold(e, _hand(0.5, 0.25), 20, 0.0)
        t = _hold(e, _hand(0.5, 0.75), 10, t)
        assert _pt(e, t)[1] == pytest.approx(0.75, abs=0.1)


class TestHandoverIsEased:
    def test_a_sensor_joining_does_not_step(self):
        e = _enc()
        t = _hold(e, _hand(0.75, 0.4, which=("n", "s")), 25, 0.0)
        before = _pt(e, t)[0]
        t = _hold(e, _hand(0.75, 0.4), 1, t)
        assert abs(_pt(e, t)[0] - before) < 0.12

    def test_but_it_arrives_within_a_few_frames(self):
        e = _enc()
        t = _hold(e, _hand(0.75, 0.4, which=("n", "s")), 25, 0.0)
        t = _hold(e, _hand(0.75, 0.4), 12, t)
        assert _pt(e, t)[0] == pytest.approx(0.75, abs=0.12)

    def test_a_sensor_dropping_out_does_not_step_either(self):
        e = _enc()
        t = _hold(e, _hand(0.75, 0.4), 25, 0.0)
        before = _pt(e, t)[0]
        t = _hold(e, _hand(0.75, 0.4, which=("n", "s")), 1, t)
        assert abs(_pt(e, t)[0] - before) < 0.12

    def test_the_ease_targets_the_NEW_answer_not_the_old_position(self):
        # the distinction that matters: easing toward the new solve is smooth,
        # holding the old one is just wrong for those frames
        e = _enc()
        t = _hold(e, _hand(0.2, 0.4, which=("n", "s")), 25, 0.0)
        t = _hold(e, _hand(0.8, 0.4), 20, t)
        assert _pt(e, t)[0] > 0.65

    def test_a_hand_that_leaves_still_stops_immediately(self):
        e = _enc()
        t = _hold(e, _hand(0.5, 0.4), 20, 0.0)
        t += 0.05
        e.on_lidar((0xFFFF,) * 4, t_mono=t)
        assert not e.frame(t_mono=t).channel("lidar").active
