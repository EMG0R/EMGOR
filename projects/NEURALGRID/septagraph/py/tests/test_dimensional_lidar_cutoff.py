"""Reject the walls by DISTANCE, not by stillness.

The stillness filter was wrong in the one case that matters: a hand held still
is not moving, so it got classified as background and vanished. Holding a
position is playing, not noise.

The walls on this instrument sit at 15-40mm. A hand does not. So: a simple near
cutoff, and nothing is held over when a sensor drops out -- carrying the last
point made the reading lie about where the hand actually was.
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


def _enc(**kw):
    return NeuralGridEncoder(calibration=Calibration(lidar_min_mm=50, grid_rotation=0, **kw))


def _active(e, t):
    return e.frame(t_mono=t).channel("lidar").active


class TestNearCutoff:
    def test_a_wall_reading_is_rejected(self):
        e = _enc()
        e.on_lidar(_ranges(bottom=31), t_mono=0.0)
        assert not _active(e, 0.0)

    def test_a_hand_further_out_is_kept(self):
        e = _enc()
        e.on_lidar(_ranges(bottom=180), t_mono=0.0)
        assert _active(e, 0.0)

    def test_the_cutoff_is_configurable(self):
        e = NeuralGridEncoder(calibration=Calibration(lidar_min_mm=200))
        e.on_lidar(_ranges(bottom=180), t_mono=0.0)
        assert not _active(e, 0.0)

    def test_it_round_trips_in_the_file(self, tmp_path):
        p = tmp_path / "c.json"
        Calibration(lidar_min_mm=55).save(p)
        assert Calibration.load(p).lidar_min_mm == 55


class TestAStillHandSurvives:
    def test_holding_a_position_does_not_make_it_vanish(self):
        e = _enc()
        t = 0.0
        for _ in range(200):          # ten seconds of holding still
            t += 0.05
            e.on_lidar(_ranges(top=190, bottom=190), t_mono=t)
        assert _active(e, t)

    def test_the_held_position_stays_where_it_is(self):
        # holding still must not drift; the exact value is the solved geometry,
        # so what is asserted is that it SETTLES
        e = _enc()
        t = 0.0
        for _ in range(200):
            t += 0.05
            e.on_lidar(_ranges(top=120, bottom=260), t_mono=t)
        early = e.frame(t_mono=t).channel("lidar").points[0].values[1]
        for _ in range(60):
            t += 0.05
            e.on_lidar(_ranges(top=120, bottom=260), t_mono=t)
        assert e.frame(t_mono=t).channel("lidar").points[0].values[1] == pytest.approx(
            early, abs=0.03
        )


class TestNothingIsHeldOver:
    def test_losing_every_sensor_drops_the_point_at_once(self):
        e = _enc()
        t = 0.0
        for _ in range(20):
            t += 0.05
            e.on_lidar(_ranges(top=150, bottom=150), t_mono=t)
        t += 0.05
        e.on_lidar((0xFFFF,) * 4, t_mono=t)
        assert not _active(e, t)

    def test_the_point_follows_the_hand_to_a_new_spot_promptly(self):
        e = _enc()
        t = 0.0
        for _ in range(20):
            t += 0.05
            e.on_lidar(_ranges(top=100, bottom=300), t_mono=t)
        for _ in range(4):
            t += 0.05
            e.on_lidar(_ranges(top=300, bottom=100), t_mono=t)
        y = e.frame(t_mono=t).channel("lidar").points[0].values[1]
        assert y > 0.6

    def test_a_sensor_dropping_out_does_not_freeze_the_reading(self):
        # It EASES across the handover rather than stepping, so give it the
        # handover window -- what must not happen is getting stuck.
        e = _enc()
        t = 0.0
        for _ in range(20):
            t += 0.05
            e.on_lidar(_ranges(top=100, bottom=300, left=200, right=200), t_mono=t)
        for _ in range(14):
            t += 0.05
            e.on_lidar(_ranges(top=300, bottom=100), t_mono=t)   # left/right gone
        y = e.frame(t_mono=t).channel("lidar").points[0].values[1]
        assert y > 0.6
