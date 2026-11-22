"""Grid orientation, all four turns, settable live.

Which way the grid is mounted has now been guessed wrong twice, and changing
the projection changed the apparent answer again. It is one number; it should
be a control, not a belief.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder


def _pad(rot, x, y):
    e = NeuralGridEncoder(calibration=Calibration(grid_rotation=rot))
    e.on_key(x, y, True, t_mono=1.0)
    return e.frame(t_mono=1.0).channel("pad").points[0].values


C0, C7 = 1 / 16, 15 / 16


class TestQuarterTurns:
    def test_zero_is_identity(self):
        assert _pad(0, 0, 0) == pytest.approx((C0, C0))

    def test_one_eighty_is_the_opposite_corner(self):
        assert _pad(180, 0, 0) == pytest.approx((C7, C7))

    def test_ninety_turns_it_a_quarter(self):
        assert _pad(90, 0, 0) == pytest.approx((C7, C0))

    def test_two_seventy_turns_it_the_other_way(self):
        assert _pad(270, 0, 0) == pytest.approx((C0, C7))

    def test_four_quarter_turns_come_back_round(self):
        seen = {rot: _pad(rot, 2, 5) for rot in (0, 90, 180, 270)}
        assert len(set(seen.values())) == 4

    def test_an_unknown_angle_falls_back_to_no_rotation(self):
        assert _pad(45, 0, 0) == pytest.approx((C0, C0))


class TestItIsAControl:
    def test_the_app_can_turn_it_and_it_persists(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        Calibration(grid_rotation=0).save(path)
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        a.rotate_grid()
        assert a.encoder.calibration.grid_rotation == 90
        assert Calibration.load(path).grid_rotation == 90

    def test_it_cycles_all_the_way_round(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        Calibration(grid_rotation=0).save(tmp_path / "c.json")
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        assert [a.rotate_grid() for _ in range(4)] == [90, 180, 270, 0]

    def test_turning_it_keeps_the_force_profile(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        Calibration(fsr_pads={0: (5, 5, 5, 5)}, fsr_rest=(1, 1, 1, 1)).save(path)
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        a.rotate_grid()
        assert a.encoder.calibration.fsr_pads == {0: (5, 5, 5, 5)}


class TestTheInstrumentsOwnDefault:
    """The NeoTrellis is mounted a half turn from how it is played. That is a
    physical fact about this unit, so a fresh file must come up right without
    anyone rediscovering it with the buttons."""

    def test_a_fresh_calibration_is_already_half_turned(self):
        assert Calibration().grid_rotation == 180

    def test_an_uncalibrated_encoder_maps_the_corner_across(self):
        e = NeuralGridEncoder()
        e.on_key(0, 0, True, t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("pad").points[0].values == pytest.approx(
            (15 / 16, 15 / 16)
        )
