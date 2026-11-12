"""The neuralGrid encoder: raw sensors -> dimensional frames.

Declared geometry (HANDOFF/memory, decided 2026-09-16):
    1x 2D  pad             (8x8)
    1x 3D  lidar volume    (triangulated in the encoder, not downstream)
    1x 1D  force           (4 FSRs summed -- global, NOT per-pad)
    4x 1D  softpot ribbons
"""
import pytest

from septagraph.dimensional import format as fmt
from septagraph.dimensional.encoders.neuralgrid import (
    NeuralGridEncoder, Calibration, LIDAR_SIGMA_WEAK,
)


def enc(**kw):
    return NeuralGridEncoder(**kw)


class TestDeclaredGeometry:
    def test_declares_every_channel_even_when_nothing_is_touched(self):
        f = enc().frame(t_mono=0.0)
        assert {c.id for c in f.channels} == {
            "pad", "lidar", "force",
            "softpot_n", "softpot_s", "softpot_e", "softpot_w",
        }

    def test_pad_is_a_2d_position_space(self):
        ch = enc().frame(t_mono=0.0).channel("pad")
        assert (ch.dims, ch.label) == (2, "position")

    def test_lidar_is_a_3d_volume(self):
        ch = enc().frame(t_mono=0.0).channel("lidar")
        assert (ch.dims, ch.label) == (3, "distance")

    def test_force_is_one_global_1d_channel_not_four(self):
        # The 4 FSRs are unevenly placed and global: the hardware cannot say
        # which of two held pads is pressed harder, so claiming per-pad z
        # would be a lie about the hardware.
        ch = enc().frame(t_mono=0.0).channel("force")
        assert (ch.dims, ch.label) == (1, "force")

    def test_force_is_softly_associated_with_the_pad(self):
        assert enc().frame(t_mono=0.0).channel("force").associated == ("pad",)

    def test_source_names_the_controller(self):
        assert enc().frame(t_mono=0.0).source == "neuralgrid"


class TestPad:
    def test_a_held_pad_becomes_a_normalized_point(self):
        e = enc(calibration=Calibration(grid_rotation=0))
        e.on_key(x=0, y=0, pressed=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.values == pytest.approx((1 / 16, 1 / 16))

    def test_pad_centres_are_used_not_corners(self):
        e = enc(calibration=Calibration(grid_rotation=0))
        e.on_key(x=7, y=7, pressed=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.values == pytest.approx((15 / 16, 15 / 16))

    def test_quantization_shows_up_as_half_a_cell_of_sigma(self):
        e = enc()
        e.on_key(x=3, y=5, pressed=True, t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("pad").points
        assert p.sigma == pytest.approx((1 / 16, 1 / 16))

    def test_several_pads_held_at_once_are_several_points(self):
        e = enc()
        e.on_key(x=0, y=0, pressed=True, t_mono=1.0)
        e.on_key(x=4, y=4, pressed=True, t_mono=1.1)
        assert len(e.frame(t_mono=1.2).channel("pad").points) == 2

    def test_releasing_a_pad_drops_its_point(self):
        e = enc()
        e.on_key(x=2, y=2, pressed=True, t_mono=1.0)
        e.on_key(x=2, y=2, pressed=False, t_mono=1.5)
        assert not e.frame(t_mono=1.6).channel("pad").active


class TestEdges:
    def test_a_press_emits_an_onset_edge_at_its_own_timestamp(self):
        e = enc()
        e.on_key(x=1, y=1, pressed=True, t_mono=9.8765)
        (edge,) = e.frame(t_mono=9.99).edges
        assert (edge.channel, edge.kind, edge.t_mono) == ("pad", "onset", 9.8765)

    def test_a_release_emits_a_release_edge(self):
        e = enc()
        e.on_key(x=1, y=1, pressed=True, t_mono=1.0)
        e.frame(t_mono=1.0)
        e.on_key(x=1, y=1, pressed=False, t_mono=2.0)
        (edge,) = e.frame(t_mono=2.0).edges
        assert edge.kind == "release"

    def test_edges_are_drained_so_each_is_reported_exactly_once(self):
        e = enc()
        e.on_key(x=1, y=1, pressed=True, t_mono=1.0)
        assert len(e.frame(t_mono=1.0).edges) == 1
        assert len(e.frame(t_mono=1.1).edges) == 0


def _held(e, pad=0, t=0.5):
    """Force is gated to pad presses -- aftertouch needs a note held."""
    e.on_key(pad % 8, pad // 8, True, t_mono=t)
    return e


class TestForceFromFsrs:
    def test_untouched_fsrs_leave_the_channel_inactive(self):
        e = enc()  # no pad held: gated shut
        e.on_analog((0, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("force").active

    def test_pressing_any_fsr_activates_the_single_force_channel(self):
        e = _held(enc())
        e.on_analog((0, 0, 0, 0, 2000, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").active

    def test_force_rises_with_harder_pressing(self):
        e = _held(enc())
        e.on_analog((0, 0, 0, 0, 800, 0, 0, 0), t_mono=1.0)
        soft = e.frame(t_mono=1.0).channel("force").points[0].values[0]
        e.on_analog((0, 0, 0, 0, 3500, 0, 0, 0), t_mono=1.1)
        hard = e.frame(t_mono=1.1).channel("force").points[0].values[0]
        assert hard > soft

    def test_force_auto_ranges_so_no_fsr_calibration_file_is_needed(self):
        # User's call: FSRs are processed in the encoder, not calibrated.
        e = _held(enc())
        for t, raw in enumerate((600, 1200, 2400, 3600)):
            e.on_analog((0, 0, 0, 0, raw, 0, 0, 0), t_mono=float(t))
        top = e.frame(t_mono=9.0).channel("force").points[0].values[0]
        assert top == pytest.approx(1.0, abs=1e-6)

    def test_all_four_fsrs_contribute_to_one_reading(self):
        # Rest first: with a single observation the encoder cannot tell a
        # resting level from a press, so both would read the same by design.
        e = _held(enc())
        e.on_analog((0,) * 8, t_mono=0.0)
        e.on_analog((0, 0, 0, 0, 1000, 0, 0, 0), t_mono=1.0)
        one = e.frame(t_mono=1.0).channel("force").points[0].values[0]
        e2 = _held(enc())
        e2.on_analog((0,) * 8, t_mono=0.0)
        e2.on_analog((0, 0, 0, 0, 1000, 1000, 1000, 1000), t_mono=1.0)
        four = e2.frame(t_mono=1.0).channel("force").points[0].values[0]
        assert four > one


class TestSoftpots:
    def test_an_untouched_ribbon_is_inactive(self):
        e = enc()
        e.on_analog((0, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("softpot_n").active

    def test_a_touched_ribbon_reports_position_along_its_travel(self):
        cal = Calibration(softpot={"softpot_n": (500, 3500)})
        e = enc(calibration=cal)
        e.on_analog((2000, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("softpot_n").points
        assert p.values[0] == pytest.approx((2000 - 500) / (3500 - 500))

    def test_calibrated_travel_ends_map_to_zero_and_one(self):
        cal = Calibration(softpot={"softpot_n": (500, 3500)})
        e = enc(calibration=cal)
        e.on_analog((3500, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("softpot_n").points[0].values[0] == pytest.approx(1.0)

    def test_ribbons_are_labelled_as_ribbons(self):
        assert enc().frame(t_mono=0.0).channel("softpot_n").label == "ribbon"


class TestLidarVolume:
    def test_nothing_in_range_leaves_the_volume_inactive(self):
        e = enc()
        e.on_lidar((0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("lidar").active

    def test_opposing_pairs_triangulate_a_position(self):
        # N/S face each other along y, E/W along x. Equal readings = centre.
        e = enc(calibration=Calibration(lidar_max_mm=1000))
        e.on_lidar((500, 500, 500, 500), t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("lidar").points
        assert p.values[0] == pytest.approx(0.5)
        assert p.values[1] == pytest.approx(0.5)

    def test_a_hand_nearer_north_moves_the_y_coordinate(self):
        e = enc(calibration=Calibration(lidar_max_mm=1000))
        e.on_lidar((200, 800, 500, 500), t_mono=1.0)
        near_n = e.frame(t_mono=1.0).channel("lidar").points[0].values[1]
        e.on_lidar((800, 200, 500, 500), t_mono=1.1)
        near_s = e.frame(t_mono=1.1).channel("lidar").points[0].values[1]
        assert near_n != pytest.approx(near_s)

    def test_fewer_sensors_report_higher_sigma(self):
        # Sigma now comes from the SOLVE, which either has enough sensors to
        # pin a point in space or does not -- it is no longer per-axis, because
        # trilateration does not solve the axes separately.
        four = enc(calibration=Calibration(lidar_max_mm=1000))
        four.on_lidar((60, 60, 60, 60), t_mono=1.0)
        s4 = max(four.frame(t_mono=1.0).channel("lidar").points[0].sigma)
        one = enc(calibration=Calibration(lidar_max_mm=1000))
        one.on_lidar((60, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        s1 = max(one.frame(t_mono=1.0).channel("lidar").points[0].sigma)
        assert s1 > s4

    def test_height_is_reported_but_admits_it_is_weakly_determined(self):
        # 4 perimeter sensors cannot really measure z; sigma must be honest.
        e = enc(calibration=Calibration(lidar_max_mm=1000))
        e.on_lidar((500, 500, 500, 500), t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("lidar").points
        assert p.sigma[2] > p.sigma[0]


class TestCalibrationFile:
    def test_round_trips_through_json(self, tmp_path):
        cal = Calibration(softpot={"softpot_n": (100, 4000)}, lidar_max_mm=1234)
        path = tmp_path / "calibration.json"
        cal.save(path)
        assert Calibration.load(path) == cal

    def test_a_missing_file_yields_usable_defaults(self, tmp_path):
        cal = Calibration.load(tmp_path / "nope.json")
        assert cal.lidar_max_mm > 0

    def test_is_loaded_at_runtime_not_compiled_in(self, tmp_path):
        # recalibrating must never require a rebuild or redeploy
        path = tmp_path / "calibration.json"
        Calibration(softpot={"softpot_n": (0, 1000)}).save(path)
        e = enc(calibration_path=path)
        e.on_analog((1000, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("softpot_n").points[0].values[0] == pytest.approx(1.0)

        Calibration(softpot={"softpot_n": (0, 4000)}).save(path)
        e.reload_calibration()
        e.on_analog((1000, 0, 0, 0, 0, 0, 0, 0), t_mono=2.0)
        assert e.frame(t_mono=2.0).channel("softpot_n").points[0].values[0] == pytest.approx(0.25)


class TestForceRestingBaseline:
    """Found on hardware 2026-09-16: force read 0.99 with nothing touching the
    plate. Auto-ranging against the maximum alone means any non-zero resting
    level reads as full scale, which makes the channel useless."""

    def test_a_constant_resting_level_does_not_read_as_full_force(self):
        e = _held(enc())
        for t in range(5):
            e.on_analog((0, 0, 0, 0, 900, 900, 900, 900), t_mono=float(t))
        v = e.frame(t_mono=9.0).channel("force").points[0].values[0]
        assert v < 0.1

    def test_pressing_above_the_resting_level_still_registers(self):
        e = _held(enc())
        for t in range(5):
            e.on_analog((0, 0, 0, 0, 900, 900, 900, 900), t_mono=float(t))
        e.on_analog((0, 0, 0, 0, 3000, 3000, 3000, 3000), t_mono=6.0)
        v = e.frame(t_mono=6.0).channel("force").points[0].values[0]
        assert v > 0.5

    def test_the_baseline_follows_the_resting_level_downward(self):
        # FSRs drift; a baseline that only ever rises would strand the channel.
        e = _held(enc())
        e.on_analog((0, 0, 0, 0, 2000, 0, 0, 0), t_mono=0.0)
        e.on_analog((0, 0, 0, 0, 400, 0, 0, 0), t_mono=1.0)
        e.on_analog((0, 0, 0, 0, 2000, 0, 0, 0), t_mono=2.0)
        assert e.frame(t_mono=2.0).channel("force").points[0].values[0] > 0.5


class TestRawPassthrough:
    """The UI needs raw -> normalized side by side, or you cannot tune by eye."""

    def test_exposes_the_raw_analog_counts(self):
        e = enc()
        e.on_analog((11, 22, 33, 44, 55, 66, 77, 88), t_mono=1.0)
        assert e.raw()["analog"] == (11, 22, 33, 44, 55, 66, 77, 88)

    def test_exposes_the_raw_lidar_ranges(self):
        e = enc()
        e.on_lidar((100, 200, 300, 0xFFFF), t_mono=1.0)
        assert e.raw()["lidar"] == (100, 200, 300, 0xFFFF)

    def test_exposes_the_force_baseline_and_span_it_is_working_from(self):
        e = enc()
        e.on_analog((0, 0, 0, 0, 900, 0, 0, 0), t_mono=1.0)
        r = e.raw()
        assert "force_floor" in r and "force_ceiling" in r


class TestDiscoveredChannelMapping:
    """N/S/E/W in the firmware is only WIRING ORDER (GPIO32/33/34/35,
    XSHUT 4/13/14/27). Which physical strip or sensor that actually is was
    never established anywhere in this repo, so the encoder must not assume
    index order -- calibration discovers it and stores the map."""

    def test_softpot_positions_default_to_wiring_order(self):
        e = enc()
        e.on_analog((3000, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("softpot_n").active

    def test_a_discovered_map_rebinds_a_position_to_another_input(self):
        cal = Calibration(
            softpot={"softpot_n": (0, 4000)},
            softpot_map={"softpot_n": 2},  # N is physically wired to input 2
        )
        e = enc(calibration=cal)
        e.on_analog((0, 0, 3000, 0, 0, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("softpot_n").active

    def test_the_unmapped_input_no_longer_drives_that_position(self):
        cal = Calibration(softpot_map={"softpot_n": 2})
        e = enc(calibration=cal)
        e.on_analog((3000, 0, 0, 0, 0, 0, 0, 0), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("softpot_n").active

    def test_a_discovered_lidar_map_reorders_the_sensors(self):
        # N/S must be an opposing pair for the geometry to mean anything; a
        # wrong map puts the hand on the wrong side.
        cal = Calibration(lidar_max_mm=1000, grid_rotation=0,
                          lidar_map={"n": 3, "s": 2, "e": 1, "w": 0})
        e = enc(calibration=cal)
        e.on_lidar((0xFFFF, 0xFFFF, 800, 200), t_mono=1.0)   # n=idx3=200
        near_top = e.frame(t_mono=1.0).channel("lidar").points[0].values[1]
        e2 = enc(calibration=cal)
        e2.on_lidar((0xFFFF, 0xFFFF, 200, 800), t_mono=1.0)  # n=idx3=800
        assert near_top < e2.frame(t_mono=1.0).channel("lidar").points[0].values[1]


class TestFsrRestingOffsets:
    """The housing pre-loads some FSRs, so they sit well above zero with
    nothing touching them (measured on hardware: 3319 / 1419 / 0 / 0). That is
    a property of the assembly, changes whenever it is opened, and therefore
    belongs in the calibration file."""

    def test_per_sensor_rest_is_subtracted(self):
        cal = Calibration(fsr_rest=(3300, 1400, 0, 0))
        e = _held(enc(calibration=cal))
        e.on_analog((0, 0, 0, 0, 3300, 1400, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").points[0].values[0] == 0.0

    def test_pressing_past_the_housing_load_still_registers(self):
        cal = Calibration(fsr_rest=(3300, 1400, 0, 0))
        e = _held(enc(calibration=cal))
        e.on_analog((0, 0, 0, 0, 3300, 1400, 0, 0), t_mono=1.0)
        e.on_analog((0, 0, 0, 0, 3900, 2600, 0, 0), t_mono=2.0)
        assert e.frame(t_mono=2.0).channel("force").points[0].values[0] > 0.0

    def test_a_sensor_reading_below_its_rest_never_goes_negative(self):
        cal = Calibration(fsr_rest=(3300, 0, 0, 0))
        e = _held(enc(calibration=cal))
        e.on_analog((0, 0, 0, 0, 100, 0, 0, 0), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").points[0].values[0] == 0.0

    def test_an_uncalibrated_rest_falls_back_to_learning_it_live(self):
        e = _held(enc())  # no fsr_rest in calibration
        for t in range(4):
            e.on_analog((0, 0, 0, 0, 900, 900, 900, 900), t_mono=float(t))
        assert e.frame(t_mono=9.0).channel("force").points[0].values[0] < 0.1


class TestCalibrationFileCarriesTheMaps:
    def test_maps_and_rest_round_trip(self, tmp_path):
        cal = Calibration(
            softpot={"softpot_n": (100, 4000)},
            softpot_map={"softpot_n": 2, "softpot_s": 0},
            lidar_map={"n": 3, "s": 2, "e": 1, "w": 0},
            fsr_rest=(3300, 1400, 0, 0),
            lidar_max_mm=1234,
        )
        path = tmp_path / "calibration.json"
        cal.save(path)
        assert Calibration.load(path) == cal

    def test_an_old_file_without_the_new_fields_still_loads(self, tmp_path):
        import json
        path = tmp_path / "calibration.json"
        path.write_text(json.dumps({"v": 1, "softpot": {}, "lidar_max_mm": 900}))
        cal = Calibration.load(path)
        assert cal.lidar_max_mm == 900 and cal.fsr_rest == ()


class TestLidarBoundsAndConfidence:
    """Short-distance mode + four free-running sensors in a shared volume make
    precise triangulation a fantasy. So: calibrated per-sensor bounds reject
    what the sensor cannot actually resolve, and HOW MANY sensors see the hand
    drives sigma, instead of reporting a confident number from one weak return.
    """

    def _cal(self, **kw):
        base = dict(
            lidar_max_mm=1000,
            lidar_bounds={"n": (30, 240), "s": (30, 240), "e": (30, 240), "w": (30, 240)},
        )
        base.update(kw)
        return Calibration(**base)

    def test_a_reading_past_the_calibrated_far_bound_is_rejected(self):
        e = enc(calibration=self._cal())
        e.on_lidar((900, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("lidar").active

    def test_a_reading_inside_the_bounds_is_kept(self):
        e = enc(calibration=self._cal())
        e.on_lidar((150, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("lidar").active

    def test_more_sensors_seeing_the_hand_means_lower_sigma(self):
        e = enc(calibration=self._cal())
        e.on_lidar((150, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        one = e.frame(t_mono=1.0).channel("lidar").points[0].sigma
        e.on_lidar((150, 150, 150, 150), t_mono=2.0)
        four = e.frame(t_mono=2.0).channel("lidar").points[0].sigma
        assert max(four) < max(one)

    def test_the_number_of_sensors_seeing_the_hand_is_reported(self):
        e = enc(calibration=self._cal())
        e.on_lidar((150, 200, 0xFFFF, 0xFFFF), t_mono=1.0)
        assert e.raw()["lidar_seen"] == 2

    def test_a_fold_back_raises_sigma_rather_than_lying_confidently(self):
        # Observed on hardware: raise a hand, the reading climbs then REVERSES
        # as the beam latches onto the forearm. A hand cannot move that fast,
        # so the encoder must not report the new value as trustworthy.
        # y axis = top/bottom = firmware S/W = indices 1/3.
        e = enc(calibration=self._cal())
        e.on_lidar((0xFFFF, 240, 0xFFFF, 240), t_mono=1.00)
        e.on_lidar((0xFFFF, 120, 0xFFFF, 240), t_mono=1.05)  # 2400 mm/s
        (p,) = e.frame(t_mono=1.05).channel("lidar").points
        assert p.sigma[1] >= LIDAR_SIGMA_WEAK

    def test_a_plausible_movement_is_not_penalised(self):
        e = enc(calibration=self._cal())
        e.on_lidar((0xFFFF, 200, 0xFFFF, 200), t_mono=1.00)
        e.on_lidar((0xFFFF, 205, 0xFFFF, 200), t_mono=1.05)  # 100 mm/s
        (p,) = e.frame(t_mono=1.05).channel("lidar").points
        assert p.sigma[1] < LIDAR_SIGMA_WEAK

    def test_uncalibrated_bounds_fall_back_to_accepting_everything(self):
        e = enc(calibration=Calibration(lidar_max_mm=1000))
        e.on_lidar((900, 0xFFFF, 0xFFFF, 0xFFFF), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("lidar").active

    def test_bounds_round_trip_through_the_file(self, tmp_path):
        cal = self._cal()
        path = tmp_path / "c.json"
        cal.save(path)
        assert Calibration.load(path).lidar_bounds == cal.lidar_bounds


class TestKnownPhysicalLayout:
    """User established the physical arrangement directly (2026-09-17):
    firmware N = his LEFT, S = his TOP, E = his RIGHT, W = his BOTTOM.
    So no discovery needed -- and note the old wiring-order default paired
    firmware N/S (left and top, 90 degrees apart) as the y "opposing pair"."""

    def test_the_default_map_matches_the_physical_layout(self):
        from septagraph.dimensional.encoders.neuralgrid import DEFAULT_LIDAR_MAP
        assert DEFAULT_LIDAR_MAP == {"n": 1, "e": 2, "s": 3, "w": 0}

    def test_an_uncalibrated_encoder_already_uses_it(self):
        e = enc()
        assert e.calibration.lidar_index("n") == 1
        assert e.calibration.lidar_index("w") == 0

    def test_the_y_axis_runs_top_to_bottom(self):
        # top = firmware S = index 1; bottom = firmware W = index 3.
        # Trilateration solves the geometry rather than taking a ratio, so the
        # property to hold is the DIRECTION, not a particular number.
        e = enc(calibration=Calibration(lidar_max_mm=1000, grid_rotation=0))
        e.on_lidar((0xFFFF, 200, 0xFFFF, 800), t_mono=1.0)
        near_top = e.frame(t_mono=1.0).channel("lidar").points[0].values[1]
        e2 = enc(calibration=Calibration(lidar_max_mm=1000, grid_rotation=0))
        e2.on_lidar((0xFFFF, 800, 0xFFFF, 200), t_mono=1.0)
        near_bottom = e2.frame(t_mono=1.0).channel("lidar").points[0].values[1]
        assert near_top < near_bottom

    def test_the_x_axis_runs_left_to_right(self):
        # left = firmware N = index 0; right = firmware E = index 2
        e = enc(calibration=Calibration(lidar_max_mm=1000, grid_rotation=0))
        e.on_lidar((300, 0xFFFF, 700, 0xFFFF), t_mono=1.0)
        near_left = e.frame(t_mono=1.0).channel("lidar").points[0].values[0]
        e2 = enc(calibration=Calibration(lidar_max_mm=1000, grid_rotation=0))
        e2.on_lidar((700, 0xFFFF, 300, 0xFFFF), t_mono=1.0)
        near_right = e2.frame(t_mono=1.0).channel("lidar").points[0].values[0]
        assert near_left < near_right

    def test_calibration_can_still_override_it(self):
        e = enc(calibration=Calibration(lidar_map={"n": 0}))
        assert e.calibration.lidar_index("n") == 0
