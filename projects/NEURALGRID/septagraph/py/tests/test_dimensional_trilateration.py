"""Solve the hand's position from all the rangefinders at once.

Opposing pairs use two sensors per axis and throw the other two away. But each
sensor sits at a KNOWN place on the rim and reports a distance, which
constrains the hand to a circle around it -- three circles intersect at a
point. That is trilateration, and it uses everything that can see.

It also degrades properly: with two sensors it falls back to the pair, with one
it says so in sigma rather than inventing a position.
"""
import pytest

from septagraph.dimensional.trilaterate import SENSOR_AT, solve_xy


def _d(px, py, pos):
    sx, sy = SENSOR_AT[pos]
    return ((px - sx) ** 2 + (py - sy) ** 2) ** 0.5


def _all(px, py, which=("n", "s", "e", "w")):
    return {p: _d(px, py, p) for p in which}


class TestThreeOrMore:
    def test_it_recovers_a_known_point(self):
        (x, y), _ = solve_xy(_all(0.3, 0.7))
        assert (x, y) == pytest.approx((0.3, 0.7), abs=0.02)

    def test_it_recovers_the_centre(self):
        (x, y), _ = solve_xy(_all(0.5, 0.5))
        assert (x, y) == pytest.approx((0.5, 0.5), abs=0.02)

    def test_three_sensors_are_enough(self):
        (x, y), _ = solve_xy(_all(0.25, 0.4, which=("n", "e", "w")))
        assert (x, y) == pytest.approx((0.25, 0.4), abs=0.05)

    def test_a_corner_is_found(self):
        (x, y), _ = solve_xy(_all(0.85, 0.15))
        assert (x, y) == pytest.approx((0.85, 0.15), abs=0.04)

    def test_more_sensors_read_as_more_certain(self):
        _, s4 = solve_xy(_all(0.4, 0.6))
        _, s3 = solve_xy(_all(0.4, 0.6, which=("n", "s", "e")))
        assert s4 < s3

    def test_one_bad_reading_is_attenuated_not_followed(self):
        # least squares spreads the error over every sensor, so the answer
        # moves by roughly HALF the bad reading rather than all of it
        for err in (0.05, 0.1, 0.2):
            d = _all(0.4, 0.6)
            d["e"] += err
            (x, y), _ = solve_xy(d)
            assert max(abs(x - 0.4), abs(y - 0.6)) < err * 0.6


class TestDegrading:
    def test_two_opposing_sensors_still_give_that_axis(self):
        (x, y), sigma = solve_xy({"n": 0.3, "s": 0.7})
        assert y == pytest.approx(0.3, abs=0.08)
        assert sigma > 0.1

    def test_one_sensor_is_reported_as_barely_known(self):
        _, sigma = solve_xy({"n": 0.4})
        assert sigma > 0.4

    def test_nothing_at_all_declines(self):
        assert solve_xy({}) == (None, 1.0)

    def test_the_answer_stays_on_the_plane(self):
        for d in ({"n": 2.0, "s": 2.0, "e": 2.0, "w": 2.0}, {"n": 0.0, "e": 0.0}):
            (x, y), _ = solve_xy(d)
            assert 0.0 <= x <= 1.0 and 0.0 <= y <= 1.0


class TestOneSensorIsNotTheCentre:
    """Regression: a single rangefinder returned dead centre whatever it read,
    so sweeping a hand over one sensor parked the point in the middle. One
    reading is weak, but it is not NO information -- the hand is out in front of
    that sensor, at roughly the distance it says."""

    def test_it_sits_out_in_front_of_the_sensor(self):
        (x, y), _ = solve_xy({"n": 0.25})
        assert x == pytest.approx(0.5, abs=0.02)
        assert y == pytest.approx(0.25, abs=0.05)

    def test_moving_away_moves_the_point_away(self):
        (_, near), _ = solve_xy({"n": 0.2})
        (_, far), _ = solve_xy({"n": 0.7})
        assert far > near + 0.3

    def test_each_sensor_pushes_from_its_own_edge(self):
        assert solve_xy({"s": 0.25})[0][1] == pytest.approx(0.75, abs=0.05)
        assert solve_xy({"w": 0.25})[0][0] == pytest.approx(0.25, abs=0.05)
        assert solve_xy({"e": 0.25})[0][0] == pytest.approx(0.75, abs=0.05)

    def test_the_unknown_axis_stays_centred(self):
        assert solve_xy({"n": 0.3})[0][0] == pytest.approx(0.5, abs=0.02)

    def test_and_it_says_it_barely_knows(self):
        assert solve_xy({"n": 0.3})[1] > 0.4

    def test_two_adjacent_sensors_are_solved_not_centred(self):
        # n and w together DO pin a point; the old fallback centred both axes
        (x, y), _ = solve_xy({"n": _d(0.3, 0.35, "n"), "w": _d(0.3, 0.35, "w")})
        assert (x, y) == pytest.approx((0.3, 0.35), abs=0.12)
