"""Grid orientation: all eight, including mirrors.

Rotation alone cannot produce a mirror, so no amount of turning fixes a grid
that is flipped -- which is exactly the state that had us cycling quarters and
never landing. Pressing bottom-left and lighting TOP-left is a y-flip.

And the ribbons get assigned by hand: which edge each input is has never been
established, and permuting them from a verbal description is guesswork.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder

C0, C7 = 1 / 16, 15 / 16


def _pad(x, y, **cal):
    # always explicit: these are about the transform, not about how this
    # particular unit happens to be mounted
    cal.setdefault("grid_rotation", 0)
    e = NeuralGridEncoder(calibration=Calibration(**cal))
    e.on_key(x, y, True, t_mono=1.0)
    return e.frame(t_mono=1.0).channel("pad").points[0].values


class TestMirror:
    def test_flipping_y_mirrors_top_to_bottom(self):
        assert _pad(0, 0, grid_flip_y=True) == pytest.approx((C0, C7))

    def test_flipping_y_leaves_x_alone(self):
        assert _pad(0, 3, grid_flip_y=True)[0] == pytest.approx(C0)

    def test_flipping_x_mirrors_left_to_right(self):
        assert _pad(0, 0, grid_flip_x=True) == pytest.approx((C7, C0))

    def test_both_flips_is_the_same_as_a_half_turn(self):
        assert _pad(2, 5, grid_flip_x=True, grid_flip_y=True) == pytest.approx(
            _pad(2, 5, grid_rotation=180)
        )

    def test_a_flip_applies_after_the_rotation(self):
        # order matters; rotating a mirrored grid is not mirroring a rotated one
        assert _pad(0, 0, grid_rotation=90, grid_flip_y=True) == pytest.approx((C7, C7))

    def test_edges_are_rotated_and_flipped_the_same_way(self):
        e = NeuralGridEncoder(
            calibration=Calibration(grid_rotation=0, grid_flip_y=True))
        e.on_key(0, 0, True, t_mono=1.0)
        assert e.frame(t_mono=1.0).edges[0].point == pytest.approx((C0, C7))

    def test_it_round_trips_in_the_file(self, tmp_path):
        cal = Calibration(grid_flip_y=True, grid_flip_x=True)
        p = tmp_path / "c.json"
        cal.save(p)
        loaded = Calibration.load(p)
        assert loaded.grid_flip_y and loaded.grid_flip_x


class TestFlipIsAControl:
    def test_the_app_can_flip_it_and_it_persists(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        assert a.flip_grid("y") is True
        assert Calibration.load(path).grid_flip_y is True

    def test_flipping_twice_returns(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.flip_grid("y")
        assert a.flip_grid("y") is False

    def test_flipping_keeps_the_force_profile(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        Calibration(fsr_pads={0: (5, 5, 5, 5)}).save(path)
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        a.flip_grid("y")
        assert a.encoder.calibration.fsr_pads == {0: (5, 5, 5, 5)}


class TestAssigningRibbons:
    """Touch a ribbon, say which edge it is. No permutation guessing."""

    def test_the_input_being_touched_is_identified(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_analog((0, 2500, 0, 0, 0, 0, 0, 0))
        assert a.touched_ribbon() == 1

    def test_nothing_touched_identifies_nothing(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_analog((0,) * 8)
        assert a.touched_ribbon() is None

    def test_assigning_binds_the_touched_input_to_that_edge(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = tmp_path / "c.json"
        a = DimensionalApp(calibration_path=path, start_mode=MODE_RUN)
        a.on_analog((0, 2500, 0, 0, 0, 0, 0, 0))
        assert a.assign_ribbon("top") == 1
        assert Calibration.load(path).softpot_map["softpot_n"] == 1

    def test_assigning_frees_whatever_held_that_edge_before(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_analog((2500, 0, 0, 0, 0, 0, 0, 0))
        a.assign_ribbon("top")
        a.on_analog((0, 2500, 0, 0, 0, 0, 0, 0))
        a.assign_ribbon("top")
        assert a.encoder.calibration.softpot_map["softpot_n"] == 1

    def test_one_input_cannot_hold_two_edges(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_analog((0, 2500, 0, 0, 0, 0, 0, 0))
        a.assign_ribbon("top")
        a.assign_ribbon("left")
        m = a.encoder.calibration.softpot_map
        assert m.get("softpot_w") == 1 and "softpot_n" not in m

    def test_assigning_nothing_touched_changes_nothing(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_analog((0,) * 8)
        assert a.assign_ribbon("top") is None
        assert a.encoder.calibration.softpot_map == {}
