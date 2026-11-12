"""Solve the hand in THREE dimensions, not on the floor.

Reported: raising a hand on the right side walked the point left toward the
middle. That is what a 2D solve must do -- the rangefinders sit on the rim and
measure straight-line distance, so lifting the hand makes every distance grow,
and a solver with nowhere to put height can only explain it as moving away
across the floor.

Four distances, three unknowns. Height belongs in the solve.
"""
import pytest

from septagraph.dimensional.trilaterate import SENSOR_AT, solve_xyz


def _d(p, pos):
    sx, sy = SENSOR_AT[pos]
    x, y, z = p
    return ((x - sx) ** 2 + (y - sy) ** 2 + z * z) ** 0.5


def _all(p, which=("n", "s", "e", "w")):
    return {q: _d(p, q) for q in which}


class TestHeightGoesToHeight:
    def test_lifting_a_hand_does_not_move_it_sideways(self):
        low = solve_xyz(_all((0.8, 0.5, 0.05)))[0]
        high = solve_xyz(_all((0.8, 0.5, 0.45)))[0]
        assert high[0] == pytest.approx(low[0], abs=0.06)
        assert high[1] == pytest.approx(low[1], abs=0.06)

    def test_lifting_a_hand_raises_the_height(self):
        low = solve_xyz(_all((0.8, 0.5, 0.05)))[0]
        high = solve_xyz(_all((0.8, 0.5, 0.45)))[0]
        assert high[2] > low[2] + 0.25

    def test_it_recovers_a_known_point_in_space(self):
        (x, y, z), _ = solve_xyz(_all((0.3, 0.7, 0.25)))
        assert (x, y, z) == pytest.approx((0.3, 0.7, 0.25), abs=0.05)

    def test_a_hand_on_the_right_stays_on_the_right_as_it_rises(self):
        for h in (0.05, 0.2, 0.4, 0.6):
            (x, _, _), _ = solve_xyz(_all((0.85, 0.5, h)))
            assert x > 0.65, f"drifted inward at height {h}"

    def test_height_is_never_negative(self):
        assert solve_xyz(_all((0.5, 0.5, 0.0)))[0][2] >= 0.0


class TestDegrading:
    def test_three_sensors_still_solve(self):
        (x, y, z), _ = solve_xyz(_all((0.4, 0.45, 0.3), which=("n", "e", "w")))
        assert (x, y) == pytest.approx((0.4, 0.45), abs=0.12)

    def test_two_sensors_report_low_confidence(self):
        _, sigma = solve_xyz(_all((0.4, 0.5, 0.2), which=("e", "w")))
        assert sigma > 0.2

    def test_one_sensor_is_out_in_front_of_it(self):
        (x, y, _), sigma = solve_xyz({"e": 0.3})
        assert x == pytest.approx(0.7, abs=0.06)
        assert sigma > 0.4

    def test_nothing_declines(self):
        assert solve_xyz({}) == (None, 1.0)

    def test_everything_stays_in_the_box(self):
        for d in ({"n": 3.0, "s": 3.0, "e": 3.0, "w": 3.0}, {"n": 0.0, "e": 0.0}):
            (x, y, z), _ = solve_xyz(d)
            assert 0.0 <= x <= 1.0 and 0.0 <= y <= 1.0 and 0.0 <= z <= 1.0

    def test_more_sensors_read_as_more_certain(self):
        _, s4 = solve_xyz(_all((0.4, 0.6, 0.2)))
        _, s3 = solve_xyz(_all((0.4, 0.6, 0.2), which=("n", "s", "e")))
        assert s4 <= s3
