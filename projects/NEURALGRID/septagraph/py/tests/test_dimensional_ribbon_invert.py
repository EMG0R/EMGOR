"""Per-ribbon direction.

Which end of a softpot reads as zero is a wiring detail -- the strip can be
mounted either way round, and two of the four on this instrument run backwards
relative to the other two. That is per-channel, not global.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder
from septagraph.axes import K

CAL = {"softpot": {f"softpot_{k}": (0, 4000) for k in "nsew"}}


def _pos(channel, raw, invert=()):
    idx = {"softpot_n": 0, "softpot_s": 1, "softpot_e": 2, "softpot_w": 3}[channel]
    e = NeuralGridEncoder(calibration=Calibration(softpot_invert=tuple(invert), **CAL))
    vals = [0] * K
    vals[idx] = raw
    e.on_analog(tuple(vals), t_mono=1.0)
    return e.frame(t_mono=1.0).channel(channel).points[0].values[0]


class TestInvert:
    def test_uninverted_runs_low_to_high(self):
        assert _pos("softpot_n", 1000) == pytest.approx(0.25)

    def test_inverting_reverses_it(self):
        assert _pos("softpot_n", 1000, invert=("softpot_n",)) == pytest.approx(0.75)

    def test_the_ends_swap(self):
        assert _pos("softpot_s", 4000, invert=("softpot_s",)) == pytest.approx(0.0)
        assert _pos("softpot_s", 0 + 200, invert=("softpot_s",)) > 0.9

    def test_it_is_per_channel(self):
        inv = ("softpot_w", "softpot_s")
        assert _pos("softpot_w", 1000, invert=inv) == pytest.approx(0.75)
        assert _pos("softpot_n", 1000, invert=inv) == pytest.approx(0.25)

    def test_the_midpoint_is_unmoved(self):
        assert _pos("softpot_e", 2000, invert=("softpot_e",)) == pytest.approx(0.5)

    def test_it_round_trips_in_the_file(self, tmp_path):
        cal = Calibration(softpot_invert=("softpot_w", "softpot_s"))
        p = tmp_path / "c.json"
        cal.save(p)
        assert set(Calibration.load(p).softpot_invert) == {"softpot_w", "softpot_s"}


class TestItIsAControl:
    def test_the_app_can_flip_one_and_it_persists(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        assert a.invert_ribbon("left") is True
        assert "softpot_w" in Calibration.load(path).softpot_invert

    def test_flipping_twice_returns(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.invert_ribbon("left")
        assert a.invert_ribbon("left") is False

    def test_it_keeps_the_force_profile(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        Calibration(fsr_pads={0: (5, 5, 5, 5)}).save(path)
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        a.invert_ribbon("bottom")
        assert a.encoder.calibration.fsr_pads == {0: (5, 5, 5, 5)}
