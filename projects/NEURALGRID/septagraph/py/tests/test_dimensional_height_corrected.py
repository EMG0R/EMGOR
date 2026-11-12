"""Take height OUT of the distances, then solve flat.

Two attempts failed for opposite reasons:

  flat solve   -- height has nowhere to go, so lifting a hand grows every
                  distance and the only available explanation is "moved away
                  across the floor". A hand rising on the right walks LEFT.
  full 3D      -- solving x, y and z together gives the fit an extra degree of
                  freedom, and it spends it absorbing noise. Steadier on paper,
                  worse to play.

The geometry gives a third way. A rangefinder on the rim measures the
hypotenuse; height is one leg. So estimate height separately and SLOWLY (a hand
does not change altitude quickly), subtract it, and solve the plane with the
in-plane distances that are left:

    d_plane = sqrt(d^2 - z^2)

x and y then come from the same steady flat solve as before, and height cannot
leak into them.
"""
import pytest

from septagraph.dimensional.trilaterate import SENSOR_AT, in_plane, solve_xy


def _d(x, y, z, pos):
    sx, sy = SENSOR_AT[pos]
    return ((x - sx) ** 2 + (y - sy) ** 2 + z * z) ** 0.5


def _all(x, y, z, which=("n", "s", "e", "w")):
    return {p: _d(x, y, z, p) for p in which}


class TestInPlane:
    def test_it_removes_the_height_leg(self):
        assert in_plane({"n": 0.5}, 0.3)["n"] == pytest.approx((0.25 - 0.09) ** 0.5)

    def test_zero_height_changes_nothing(self):
        assert in_plane({"n": 0.5}, 0.0)["n"] == pytest.approx(0.5)

    def test_a_height_larger_than_the_distance_does_not_go_imaginary(self):
        assert in_plane({"n": 0.2}, 0.9)["n"] >= 0.0

    def test_every_sensor_is_corrected(self):
        out = in_plane({"n": 0.5, "e": 0.5}, 0.3)
        assert len(out) == 2 and all(v < 0.5 for v in out.values())


class TestHeightNoLongerLeaks:
    def test_a_hand_on_the_right_stays_right_as_it_rises(self):
        for z in (0.0, 0.2, 0.4, 0.6):
            d = in_plane(_all(0.85, 0.5, z), z)
            (x, _), _ = solve_xy(d)
            assert x > 0.72, f"walked inward at height {z}: x={x:.3f}"

    def test_the_position_barely_moves_across_a_lift(self):
        low = solve_xy(in_plane(_all(0.8, 0.35, 0.05), 0.05))[0]
        high = solve_xy(in_plane(_all(0.8, 0.35, 0.55), 0.55))[0]
        assert high[0] == pytest.approx(low[0], abs=0.05)
        assert high[1] == pytest.approx(low[1], abs=0.05)

    def test_without_the_correction_it_does_walk_inward(self):
        # the bug, demonstrated: same readings, no height removed
        raw = _all(0.85, 0.5, 0.55)
        assert solve_xy(raw)[0][0] < solve_xy(in_plane(raw, 0.55))[0][0] - 0.06

    def test_a_real_lateral_move_still_moves(self):
        left = solve_xy(in_plane(_all(0.25, 0.5, 0.3), 0.3))[0]
        right = solve_xy(in_plane(_all(0.75, 0.5, 0.3), 0.3))[0]
        assert right[0] - left[0] > 0.35
