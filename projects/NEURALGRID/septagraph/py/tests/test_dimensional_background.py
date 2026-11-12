"""Superseded: see test_dimensional_lidar_cutoff.py.

This file tested a stillness filter -- a rangefinder whose reading had not
moved for two seconds was treated as staring at the housing. It rejected the
walls correctly and was wrong in the case that matters most: a hand HELD STILL
is not moving either, so it vanished. Holding a position is playing.

Replaced by a plain near-distance cutoff. What survives here is the rule the
old file was right about: nothing may be silently dropped.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import (
    Calibration, DEFAULT_LIDAR_MAP, NeuralGridEncoder,
)


def _ranges(**kw):
    out = [0xFFFF] * 4
    for key, mm in (("n", kw.get("top")), ("s", kw.get("bottom")),
                    ("w", kw.get("left")), ("e", kw.get("right"))):
        if mm is not None:
            out[DEFAULT_LIDAR_MAP[key]] = mm
    return tuple(out)


class TestNothingIsEverLeftOut:
    def test_the_raw_ranges_always_carry_all_four(self):
        e = NeuralGridEncoder()
        e.on_lidar(_ranges(bottom=33), t_mono=0.0)
        assert len(e.raw()["lidar"]) == 4

    def test_a_sensor_seeing_nothing_now_still_works_later(self):
        e = NeuralGridEncoder()
        for k in range(20):
            e.on_lidar(_ranges(bottom=200), t_mono=k * 0.05)
        e.on_lidar(_ranges(top=200, bottom=200), t_mono=1.1)
        assert "top" not in e.raw()["lidar_no_target"]

    def test_every_encoder_diagnostic_reaches_the_app_state(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_lidar(_ranges(bottom=200))
        a.on_analog((0,) * 8)
        missing = set(a.encoder.raw()) - set(a.state()["raw"])
        assert not missing, f"dropped on the way to the UI: {sorted(missing)}"
