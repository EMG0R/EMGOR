"""A human-readable one-line form of a frame, and the grid's rotation.

JSON is the wire format; it is not something you can read while playing. The
monitor wants one line per frame, dense, only what is actually active.
"""
import pytest

from septagraph.dimensional import format as fmt
from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder


class TestReadableLine:
    def _frame(self, **kw):
        return fmt.DimensionalFrame(source="neuralgrid", t_mono=12.345, **kw)

    def test_an_idle_frame_says_so_rather_than_printing_nothing(self):
        assert "idle" in fmt.frame_to_line(self._frame())

    def test_it_leads_with_the_time(self):
        # right-aligned so the column stays put as a scrolling log
        assert fmt.frame_to_line(self._frame()).lstrip().startswith("12.345")

    def test_an_active_2d_channel_shows_its_coordinates(self):
        f = self._frame(channels=(
            fmt.Channel(id="pad", dims=2, label="position",
                        points=(fmt.Point((0.4375, 0.5625), (0.0625, 0.0625)),)),
        ))
        line = fmt.frame_to_line(f)
        assert "pad" in line and "0.44" in line and "0.56" in line

    def test_inactive_channels_are_left_out(self):
        f = self._frame(channels=(
            fmt.Channel(id="pad", dims=2, label="position"),
            fmt.Channel(id="force", dims=1, label="force",
                        points=(fmt.Point((0.62,), (0.1,)),)),
        ))
        line = fmt.frame_to_line(f)
        assert "force" in line and "pad" not in line

    def test_sigma_is_shown_when_it_is_large_enough_to_matter(self):
        f = self._frame(channels=(
            fmt.Channel(id="lidar", dims=3, label="distance",
                        points=(fmt.Point((0.5, 0.5, 0.5), (0.3, 0.3, 0.3)),)),
        ))
        assert "±" in fmt.frame_to_line(f)

    def test_a_confident_reading_is_not_cluttered_with_sigma(self):
        f = self._frame(channels=(
            fmt.Channel(id="force", dims=1, label="force",
                        points=(fmt.Point((0.62,), (0.01,)),)),
        ))
        assert "±" not in fmt.frame_to_line(f)

    def test_several_simultaneous_points_are_counted(self):
        f = self._frame(channels=(
            fmt.Channel(id="pad", dims=2, label="position", points=(
                fmt.Point((0.1, 0.1), (0.0, 0.0)),
                fmt.Point((0.9, 0.9), (0.0, 0.0)),
            )),
        ))
        assert "x2" in fmt.frame_to_line(f)

    def test_onsets_are_marked(self):
        f = self._frame(edges=(
            fmt.Edge(channel="pad", kind="onset", t_mono=12.3, point=(0.5, 0.5)),
        ))
        assert "onset" in fmt.frame_to_line(f)


class TestGridRotation:
    """The grid is mounted 180 degrees from how it is played."""

    def test_zero_rotation_is_identity(self):
        e = NeuralGridEncoder(calibration=Calibration(grid_rotation=0))
        e.on_key(0, 0, True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.values == pytest.approx((1 / 16, 1 / 16))

    def test_rotating_puts_the_first_pad_in_the_opposite_corner(self):
        e = NeuralGridEncoder(calibration=Calibration(grid_rotation=180))
        e.on_key(0, 0, True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.values == pytest.approx((15 / 16, 15 / 16))

    def test_the_far_corner_comes_back_to_the_origin(self):
        e = NeuralGridEncoder(calibration=Calibration(grid_rotation=180))
        e.on_key(7, 7, True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.values == pytest.approx((1 / 16, 1 / 16))

    def test_onset_edges_are_rotated_too(self):
        e = NeuralGridEncoder(calibration=Calibration(grid_rotation=180))
        e.on_key(0, 0, True, t_mono=1.0)
        (edge,) = e.frame(t_mono=1.0).edges
        assert edge.point == pytest.approx((15 / 16, 15 / 16))

    def test_the_force_profile_still_keys_off_the_PHYSICAL_pad(self):
        # the pad profile was measured against hardware positions; rotating the
        # view must not silently look up a different pad's response
        profile = {0: (500, 200, 60, 40)}
        e = NeuralGridEncoder(calibration=Calibration(
            grid_rotation=180, fsr_rest=(0, 0, 0, 0), fsr_pads=profile))
        e.on_key(0, 0, True, t_mono=1.0)
        # force is aftertouch: settle, then lean in
        for k in range(14):
            e.on_analog((0, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0 + k * 0.024)
        for k in range(14, 34):
            e.on_analog((0, 0, 0, 0, 500, 200, 60, 40), t_mono=1.0 + k * 0.024)
            e.frame(t_mono=1.0 + k * 0.024)
        assert e.frame(t_mono=2.0).channel("force").points[0].values[0] > 0.0

    def test_it_round_trips_in_the_calibration_file(self, tmp_path):
        cal = Calibration(grid_rotation=180)
        p = tmp_path / "c.json"
        cal.save(p)
        assert Calibration.load(p).grid_rotation == 180


class TestServedToTheUi:
    def test_state_carries_the_readable_line(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        assert "idle" in a.state()["line"]

    def test_the_line_reflects_what_is_happening(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        a = DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)
        a.on_key(3, 3, True)
        assert "pad" in a.state()["line"]
