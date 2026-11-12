"""Axis semantics v2: space becomes a real point on a sphere.

v1 could not express a point in 3D. `az` spanned only -90..+90, so the
back half of the room was unreachable, and there was no elevation at all
-- space was a flat half-plane. Ambisonics/VBAP need azimuth, elevation
and distance.

`el` is appended as the LAST index rather than inserted next to `az`, so
indices 0..7 keep their meaning and existing catalog data stays valid.
"""
import math

import pytest

from septagraph import axes
from septagraph.axes import AXES, K, from_physical, to_physical


class TestTheAxisSet:
    def test_there_are_nine_wire_values(self):
        assert K == 9

    def test_elevation_exists(self):
        assert "el" in AXES

    def test_it_is_last_so_older_indices_keep_their_meaning(self):
        assert AXES.index("el") == 8
        assert AXES[:8] == ("freq", "loud", "harm", "rate", "scale", "az", "spread", "dist")


class TestAzimuthGoesAllTheWayRound:
    def test_it_spans_a_full_circle(self):
        assert to_physical(5, 0.0) == pytest.approx(-180.0)
        assert to_physical(5, 1.0) == pytest.approx(180.0)

    def test_the_middle_is_still_straight_ahead(self):
        assert to_physical(5, 0.5) == pytest.approx(0.0)

    def test_behind_the_listener_is_now_reachable(self):
        # the whole point: v1 could not say "behind me"
        assert to_physical(5, 0.125) == pytest.approx(-135.0)
        assert to_physical(5, 0.875) == pytest.approx(135.0)

    def test_quarter_turns(self):
        assert to_physical(5, 0.25) == pytest.approx(-90.0)   # hard left
        assert to_physical(5, 0.75) == pytest.approx(90.0)    # hard right

    def test_it_round_trips(self):
        for deg in (-180.0, -90.0, -45.0, 0.0, 90.0, 180.0):
            assert to_physical(5, from_physical(5, deg)) == pytest.approx(deg)


class TestElevation:
    def test_zero_is_below_one_is_above(self):
        assert to_physical(8, 0.0) == pytest.approx(-90.0)
        assert to_physical(8, 1.0) == pytest.approx(90.0)

    def test_the_middle_is_ear_level(self):
        assert to_physical(8, 0.5) == pytest.approx(0.0)

    def test_it_round_trips(self):
        for deg in (-90.0, -30.0, 0.0, 45.0, 90.0):
            assert to_physical(8, from_physical(8, deg)) == pytest.approx(deg)

    def test_out_of_range_is_clamped(self):
        assert from_physical(8, -999.0) == 0.0
        assert from_physical(8, 999.0) == 1.0


class TestAPointInSpaceIsNowExpressible:
    def test_directly_overhead(self):
        az, el, dist = from_physical(5, 0.0), from_physical(8, 90.0), 0.2
        assert to_physical(8, el) == pytest.approx(90.0)
        assert 0.0 <= az <= 1.0

    def test_behind_and_below(self):
        az = from_physical(5, 170.0)
        el = from_physical(8, -40.0)
        assert to_physical(5, az) == pytest.approx(170.0)
        assert to_physical(8, el) == pytest.approx(-40.0)

    def test_the_other_axes_are_untouched(self):
        # a v2 change must not move pitch, level, texture or distance
        assert to_physical(0, 0.5) == pytest.approx(20.0 * 1000.0 ** 0.5)
        assert to_physical(1, 0.5) == pytest.approx(-30.0)
        assert to_physical(3, 0.5) == pytest.approx(0.5 * 128.0 ** 0.5)
        assert to_physical(4, 0.5) == pytest.approx(0.25 * 32.0 ** 0.5)
        assert to_physical(7, 0.3) == pytest.approx(0.3)   # dist unchanged


class TestVectorsFollowK:
    def test_half_and_zero_widen_with_the_axis_set(self):
        from septagraph.types import HALF, ONE, ZERO
        assert len(HALF) == K and len(ZERO) == K and len(ONE) == K
