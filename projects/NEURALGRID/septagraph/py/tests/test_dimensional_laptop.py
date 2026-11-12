"""The laptop encoder: trackpad + keyboard as 2x 2D spaces.

This exists to prove the format is not secretly grid-shaped. It is the cheapest
possible second interface (no new hardware) and it deliberately covers BOTH
kinds of 2D -- a continuous surface and a key grid -- so the same downstream
model reads a trackpad, a keyboard and an 8x8 pad without knowing the
difference.
"""
import pytest

from septagraph.dimensional import format as fmt
from septagraph.dimensional.encoders.laptop import LaptopEncoder, KEY_GRID


class TestDeclaredGeometry:
    def test_declares_exactly_two_2d_spaces(self):
        chans = LaptopEncoder().frame(t_mono=0.0).channels
        assert {c.id for c in chans} == {"trackpad", "keyboard"}
        assert all(c.dims == 2 for c in chans)

    def test_source_names_the_controller(self):
        assert LaptopEncoder().frame(t_mono=0.0).source == "laptop"

    def test_both_spaces_use_the_same_label_as_the_grid_pad(self):
        # A model generalizes across devices only if the vocabulary matches.
        chans = LaptopEncoder().frame(t_mono=0.0).channels
        assert {c.label for c in chans} == {"position"}


class TestTrackpad:
    def test_reports_the_touch_position(self):
        e = LaptopEncoder()
        e.on_trackpad(x=0.25, y=0.75, down=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("trackpad").points
        assert p.values == pytest.approx((0.25, 0.75))

    def test_is_inactive_once_the_finger_lifts(self):
        e = LaptopEncoder()
        e.on_trackpad(x=0.5, y=0.5, down=True, t_mono=1.0)
        e.on_trackpad(x=0.5, y=0.5, down=False, t_mono=1.5)
        assert not e.frame(t_mono=1.6).channel("trackpad").active

    def test_a_continuous_surface_reports_far_finer_sigma_than_a_key_grid(self):
        # Same format, same channel shape -- only the precision differs. This
        # is the whole reason sigma replaced a discrete/continuous flag.
        e = LaptopEncoder()
        e.on_trackpad(x=0.5, y=0.5, down=True, t_mono=1.0)
        e.on_key("g", pressed=True, t_mono=1.0)
        pad_sigma = e.frame(t_mono=1.0).channel("trackpad").points[0].sigma[0]
        key_sigma = e.frame(t_mono=1.0).channel("keyboard").points[0].sigma[0]
        assert pad_sigma < key_sigma

    def test_a_press_emits_an_onset_edge(self):
        e = LaptopEncoder()
        e.on_trackpad(x=0.1, y=0.2, down=True, t_mono=4.25)
        (edge,) = e.frame(t_mono=4.3).edges
        assert (edge.channel, edge.kind, edge.t_mono) == ("trackpad", "onset", 4.25)


class TestKeyboard:
    def test_a_key_becomes_a_position_in_the_key_grid(self):
        e = LaptopEncoder()
        e.on_key("q", pressed=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("keyboard").points
        assert 0.0 <= p.values[0] <= 1.0 and 0.0 <= p.values[1] <= 1.0

    def test_keys_on_the_same_row_share_a_y(self):
        e = LaptopEncoder()
        e.on_key("a", pressed=True, t_mono=1.0)
        e.on_key("l", pressed=True, t_mono=1.0)
        ys = {p.values[1] for p in e.frame(t_mono=1.0).channel("keyboard").points}
        assert len(ys) == 1

    def test_keys_further_right_have_a_larger_x(self):
        e = LaptopEncoder()
        e.on_key("a", pressed=True, t_mono=1.0)
        a_x = e.frame(t_mono=1.0).channel("keyboard").points[0].values[0]
        e.on_key("a", pressed=False, t_mono=1.1)
        e.on_key("l", pressed=True, t_mono=1.2)
        l_x = e.frame(t_mono=1.2).channel("keyboard").points[0].values[0]
        assert l_x > a_x

    def test_several_keys_held_at_once_are_several_points(self):
        e = LaptopEncoder()
        for k in "asd":
            e.on_key(k, pressed=True, t_mono=1.0)
        assert len(e.frame(t_mono=1.0).channel("keyboard").points) == 3

    def test_an_unknown_key_is_ignored_rather_than_crashing(self):
        e = LaptopEncoder()
        e.on_key("F13", pressed=True, t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("keyboard").active

    def test_key_sigma_reflects_the_grid_it_sits_in(self):
        e = LaptopEncoder()
        e.on_key("g", pressed=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("keyboard").points
        assert p.sigma[0] == pytest.approx(fmt.sigma_for_steps(len(KEY_GRID[0])))


class TestInterchangeability:
    def test_its_frames_survive_the_same_json_round_trip(self):
        e = LaptopEncoder()
        e.on_trackpad(x=0.3, y=0.4, down=True, t_mono=1.0)
        e.on_key("j", pressed=True, t_mono=1.0)
        f = e.frame(t_mono=1.0)
        assert fmt.frame_from_dict(fmt.frame_to_dict(f)) == f

    def test_a_consumer_can_read_it_with_no_device_specific_knowledge(self):
        # The point of the whole exercise: same accessor, different hardware.
        for e, cid in ((LaptopEncoder(), "trackpad"),):
            e.on_trackpad(x=0.6, y=0.6, down=True, t_mono=1.0)
            ch = e.frame(t_mono=1.0).channel(cid)
            assert ch.dims == 2 and ch.active and len(ch.points[0].values) == 2
