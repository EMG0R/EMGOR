"""The dimensional format: geometry only, no semantics.

The encoder says *what shape the hardware is* -- 1D/2D/3D channels normalized
to 0-1 -- and nothing about what any of it means musically. Meaning is the
model's job. See docs/DIMENSIONAL_FORMAT_v1.md.
"""
import math

import pytest

from septagraph.dimensional import format as fmt


class TestPoint:
    def test_holds_one_value_and_sigma_per_dimension(self):
        p = fmt.Point(values=(0.25, 0.5), sigma=(0.01, 0.01))
        assert p.values == (0.25, 0.5)
        assert p.sigma == (0.01, 0.01)

    def test_clamps_values_into_unit_range(self):
        p = fmt.Point(values=(-3.0, 9.0), sigma=(0.0, 0.0))
        assert p.values == (0.0, 1.0)

    def test_rejects_sigma_of_a_different_length_than_values(self):
        with pytest.raises(ValueError, match="sigma"):
            fmt.Point(values=(0.5, 0.5), sigma=(0.1,))


class TestChannel:
    def test_declares_its_dimensionality_and_label(self):
        ch = fmt.Channel(id="pad", dims=2, label="position", points=())
        assert ch.dims == 2
        assert ch.label == "position"

    def test_is_inactive_when_it_holds_no_points(self):
        assert not fmt.Channel(id="pad", dims=2, label="position", points=()).active

    def test_is_active_when_it_holds_a_point(self):
        ch = fmt.Channel(
            id="pad", dims=2, label="position",
            points=(fmt.Point((0.5, 0.5), (0.0, 0.0)),),
        )
        assert ch.active

    def test_holds_several_simultaneous_points(self):
        ch = fmt.Channel(
            id="pad", dims=2, label="position",
            points=(fmt.Point((0.1, 0.1), (0.0, 0.0)), fmt.Point((0.9, 0.9), (0.0, 0.0))),
        )
        assert len(ch.points) == 2

    def test_rejects_a_point_whose_width_disagrees_with_dims(self):
        with pytest.raises(ValueError, match="dims"):
            fmt.Channel(
                id="pad", dims=2, label="position",
                points=(fmt.Point((0.5,), (0.0,)),),
            )

    def test_rejects_a_label_outside_the_controlled_vocabulary(self):
        # Free-text labels would make geometry portable but semantics not,
        # which defeats the whole format.
        with pytest.raises(ValueError, match="label"):
            fmt.Channel(id="pad", dims=2, label="fsr_under_pad", points=())

    def test_carries_soft_associations_to_other_channels(self):
        ch = fmt.Channel(
            id="fsr", dims=1, label="force", points=(), associated=("pad",),
        )
        assert ch.associated == ("pad",)


class TestSigmaCarriesResolution:
    """Sigma replaces any discrete/continuous flag: everything is continuous,
    only precision differs."""

    def test_quantized_axis_reports_half_a_cell_of_sigma(self):
        # an 8-step axis knows the finger is in a cell, not where in it
        assert fmt.sigma_for_steps(8) == pytest.approx(1.0 / 16.0)

    def test_finer_quantization_reports_smaller_sigma(self):
        assert fmt.sigma_for_steps(64) < fmt.sigma_for_steps(8)

    def test_a_dimension_the_device_cannot_sense_is_maximum_sigma(self):
        assert fmt.SIGMA_UNKNOWN == 1.0


class TestFrame:
    def test_carries_its_source_and_channels(self):
        f = fmt.DimensionalFrame(
            source="neuralgrid", t_mono=1.5,
            channels=(fmt.Channel(id="pad", dims=2, label="position", points=()),),
        )
        assert f.source == "neuralgrid"
        assert f.t_mono == 1.5

    def test_looks_a_channel_up_by_id(self):
        ch = fmt.Channel(id="pad", dims=2, label="position", points=())
        f = fmt.DimensionalFrame(source="x", t_mono=0.0, channels=(ch,))
        assert f.channel("pad") is ch
        assert f.channel("nope") is None

    def test_rejects_two_channels_sharing_an_id(self):
        ch = fmt.Channel(id="pad", dims=2, label="position", points=())
        with pytest.raises(ValueError, match="duplicate"):
            fmt.DimensionalFrame(source="x", t_mono=0.0, channels=(ch, ch))

    def test_carries_edges_with_their_own_precise_timestamps(self):
        # a ~42fps stream only resolves 24ms; the ESP32 knows a press to sub-ms,
        # and the fast path depends on not throwing that away.
        e = fmt.Edge(channel="pad", kind="onset", t_mono=1.2345, point=(0.5, 0.5))
        f = fmt.DimensionalFrame(source="x", t_mono=1.25, channels=(), edges=(e,))
        assert f.edges[0].t_mono == 1.2345
        assert f.edges[0].kind == "onset"

    def test_rejects_an_edge_kind_outside_onset_and_release(self):
        with pytest.raises(ValueError, match="kind"):
            fmt.Edge(channel="pad", kind="wiggle", t_mono=0.0, point=(0.0,))


class TestRoundTrip:
    def test_a_frame_survives_a_trip_through_json(self):
        f = fmt.DimensionalFrame(
            source="neuralgrid", t_mono=2.0,
            channels=(
                fmt.Channel(
                    id="pad", dims=2, label="position",
                    points=(fmt.Point((0.25, 0.75), (0.0625, 0.0625)),),
                ),
                fmt.Channel(id="fsr", dims=1, label="force", points=(), associated=("pad",)),
            ),
            edges=(fmt.Edge(channel="pad", kind="onset", t_mono=1.99, point=(0.25, 0.75)),),
        )
        assert fmt.frame_from_dict(fmt.frame_to_dict(f)) == f

    def test_declares_its_version(self):
        f = fmt.DimensionalFrame(source="x", t_mono=0.0, channels=())
        assert fmt.frame_to_dict(f)["v"] == fmt.FORMAT_VERSION


class TestReadableJson:
    """json.dumps(indent=...) puts every single number on its own line, so
    a frame becomes ~90 lines and you cannot see a channel at a glance.
    One line per channel is the readable unit."""

    def _frame(self):
        return fmt.DimensionalFrame(
            source="neuralgrid", t_mono=12.345,
            channels=(
                fmt.Channel(id="pad", dims=2, label="position",
                            points=(fmt.Point(values=(0.44, 0.56), sigma=(0.0625, 0.0625)),)),
                fmt.Channel(id="force", dims=1, label="force", associated=("pad",),
                            points=(fmt.Point(values=(0.62,), sigma=(0.06,)),)),
                fmt.Channel(id="softpot_s", dims=1, label="ribbon", points=()),
                fmt.Channel(id="lidar", dims=3, label="distance",
                            points=(fmt.Point(values=(0.58, 0.97, 0.41),
                                              sigma=(0.29, 0.29, 0.64)),)),
            ),
            edges=(fmt.Edge(channel="pad", kind="onset", t_mono=12.3401,
                            point=(0.44, 0.56)),),
        )

    def test_it_is_still_valid_json(self):
        import json
        assert json.loads(fmt.frame_to_json(self._frame())) == fmt.frame_to_dict(self._frame())

    def test_one_line_per_channel(self):
        out = fmt.frame_to_json(self._frame())
        chan_lines = [l for l in out.splitlines() if '"id":' in l]
        assert len(chan_lines) == 4

    def test_a_channels_values_stay_on_that_line(self):
        out = fmt.frame_to_json(self._frame())
        pad = [l for l in out.splitlines() if '"pad"' in l and '"id":' in l][0]
        assert "0.44" in pad and "0.56" in pad and "0.0625" in pad

    def test_one_line_per_edge(self):
        out = fmt.frame_to_json(self._frame())
        assert len([l for l in out.splitlines() if '"kind":' in l]) == 1

    def test_an_empty_channel_is_still_one_readable_line(self):
        out = fmt.frame_to_json(self._frame())
        sp = [l for l in out.splitlines() if '"softpot_s"' in l][0]
        assert '"points": []' in sp

    def test_it_is_far_shorter_than_naive_pretty_printing(self):
        import json
        naive = json.dumps(fmt.frame_to_dict(self._frame()), indent=1)
        assert len(fmt.frame_to_json(self._frame()).splitlines()) < len(naive.splitlines()) / 3
