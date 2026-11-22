"""The lidar and the pad must live in the same frame.

The pad channel is rotated to match how the instrument is mounted; the lidar
position was not. So with the grid at 180 they were a half turn apart -- left
read as right and up as down -- even though both were individually "correct".

One orientation, applied to everything the player sees.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import (
    Calibration, DEFAULT_LIDAR_MAP, GRID_MM, NeuralGridEncoder,
)
from septagraph.dimensional.trilaterate import SENSOR_AT


def _hand(x, y, z=0.0):
    out = [0xFFFF] * 4
    for pos in ("n", "s", "e", "w"):
        sx, sy = SENSOR_AT[pos]
        d = ((x - sx) ** 2 + (y - sy) ** 2 + z * z) ** 0.5
        out[DEFAULT_LIDAR_MAP[pos]] = int(d * GRID_MM)
    return tuple(out)


def _xy(rot, x, y, **kw):
    e = NeuralGridEncoder(calibration=Calibration(grid_rotation=rot, **kw))
    for k in range(40):
        e.on_lidar(_hand(x, y), t_mono=k * 0.05)
    p = e.frame(t_mono=2.0).channel("lidar").points[0]
    return p.values[0], p.values[1]


class TestViewPoint:
    def test_unrotated_passes_through(self):
        assert _xy(0, 0.8, 0.3) == pytest.approx((0.8, 0.3), abs=0.08)

    def test_a_half_turn_mirrors_both_axes(self):
        assert _xy(180, 0.8, 0.3) == pytest.approx((0.2, 0.7), abs=0.08)

    def test_a_quarter_turn_swaps_them(self):
        x, y = _xy(90, 0.8, 0.3)
        assert (x, y) == pytest.approx((0.7, 0.8), abs=0.08)

    def test_a_mirror_applies_too(self):
        assert _xy(0, 0.8, 0.3, grid_flip_x=True)[0] == pytest.approx(0.2, abs=0.08)


class TestItAgreesWithThePad:
    def test_both_land_on_the_same_side(self):
        # a hand and a pad press on the same side of the instrument must read
        # as the same side -- that is the whole point of one frame
        for rot in (0, 90, 180, 270):
            cal = Calibration(grid_rotation=rot)
            e = NeuralGridEncoder(calibration=cal)
            e.on_key(7, 7, True, t_mono=0.0)          # hardware far corner
            for k in range(40):
                e.on_lidar(_hand(15 / 16, 15 / 16), t_mono=k * 0.05)
            f = e.frame(t_mono=2.0)
            pad = f.channel("pad").points[0].values
            lidar = f.channel("lidar").points[0].values[:2]
            assert lidar == pytest.approx(pad, abs=0.1), f"frames disagree at {rot}"
