"""The dimensional app: one service, two modes.

Boots into CALIBRATE, hands off to RUN the moment calibration is saved, and can
be sent back to CALIBRATE from RUN. Sensor traffic is fed in from the link; the
app owns which of the two consumes it.
"""
import pytest

from septagraph.dimensional.app import DimensionalApp, MODE_CALIBRATE, MODE_RUN
from septagraph.axes import K


def app(tmp_path, **kw):
    return DimensionalApp(calibration_path=tmp_path / "calibration.json", **kw)


def _rest(a):
    """Step 1 wants hands off everything."""
    for _ in range(10):
        a.on_analog((0,) * 8)
    a.calibration.advance()


def _sweep(a, position="top", hw_index=0, lo=500, hi=3500):
    """Sweep the ribbon on `position`; the session discovers `hw_index`."""
    a.calibration.goto(f"softpot_{position}")
    for raw in (lo, hi):
        vals = [0] * K
        vals[hw_index] = raw
        a.on_analog(tuple(vals))


class TestModes:
    def test_boots_into_calibration(self, tmp_path):
        assert app(tmp_path).mode == MODE_CALIBRATE

    def test_can_boot_straight_into_run_when_asked(self, tmp_path):
        assert app(tmp_path, start_mode=MODE_RUN).mode == MODE_RUN

    def test_finishing_calibration_hands_off_to_the_encoder(self, tmp_path):
        a = app(tmp_path)
        a.finish_calibration()
        assert a.mode == MODE_RUN

    def test_finishing_calibration_writes_the_file(self, tmp_path):
        a = app(tmp_path)
        _rest(a)
        _sweep(a)
        a.finish_calibration()
        assert (tmp_path / "calibration.json").exists()

    def test_the_encoder_picks_up_the_calibration_just_written(self, tmp_path):
        a = app(tmp_path)
        _rest(a)
        # both ends above TOUCH_FLOOR: a sweep is only real where the ribbon
        # was actually being touched.
        _sweep(a, position="top", hw_index=0, lo=200, hi=3400)
        a.finish_calibration()
        a.on_analog((1000, 0, 0, 0, 0, 0, 0, 0))
        # "top" became softpot_n, bound to the input that answered (0)
        v = a.frame().channel("softpot_n").points[0].values[0]
        assert v == pytest.approx((1000 - 200) / (3400 - 200))

    def test_calibration_can_be_relaunched_from_run(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        a.start_calibration()
        assert a.mode == MODE_CALIBRATE

    def test_relaunching_calibration_starts_it_fresh(self, tmp_path):
        a = app(tmp_path)
        _rest(a)
        _sweep(a)
        a.finish_calibration()
        a.start_calibration()
        assert a.calibration.current.kind == "fsr_rest"
        assert a.calibration.current.samples == []


class TestRouting:
    def test_sensor_traffic_feeds_calibration_while_calibrating(self, tmp_path):
        a = app(tmp_path)
        _rest(a)
        a.on_analog((900, 0, 0, 0, 0, 0, 0, 0))
        assert a.calibration.current.lo == 900

    def test_sensor_traffic_feeds_the_encoder_while_running(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        a.on_key(x=1, y=2, pressed=True, t_mono=1.0)
        assert a.frame().channel("pad").active

    def test_calibration_does_not_disturb_the_encoder_state(self, tmp_path):
        a = app(tmp_path)
        _rest(a)
        a.on_analog((900, 0, 0, 0, 0, 0, 0, 0))
        a.finish_calibration()
        assert not a.frame().channel("softpot_n").active

    def test_lidar_traffic_reaches_whichever_mode_is_active(self, tmp_path):
        a = app(tmp_path)
        a.calibration.goto_kind("lidar_arc")
        a.on_lidar((1400, 0xFFFF, 0xFFFF, 0xFFFF))
        assert a.calibration.arcs.samples["top"] == [(1400, 0xFFFF, 0xFFFF, 0xFFFF)]


class TestLeds:
    def test_calibration_cues_drive_the_grid_while_calibrating(self, tmp_path):
        a = app(tmp_path)
        assert any(px != (0, 0, 0) for px in a.led_frame())

    def test_running_lights_the_pad_that_was_actually_pressed(self, tmp_path):
        # The LED frame goes to the HARDWARE, so it must be in hardware
        # coordinates. Building it from the rotated view lit the physically
        # opposite pad -- press top-left, watch bottom-right come on.
        a = app(tmp_path, start_mode=MODE_RUN)
        a.on_key(x=0, y=0, pressed=True, t_mono=1.0)
        frame = a.led_frame()
        assert frame[0] != (0, 0, 0)
        assert all(px == (0, 0, 0) for px in frame[1:])

    def test_the_lit_led_does_not_move_when_the_view_is_rotated(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        lit = []
        for rot in (0, 90, 180, 270):
            Calibration(grid_rotation=rot).save(tmp_path / "calibration.json")
            a = app(tmp_path, start_mode=MODE_RUN)
            a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
            lit.append([i for i, px in enumerate(a.led_frame()) if px != (0, 0, 0)])
        assert lit == [[5 * 8 + 2]] * 4

    def test_an_untouched_grid_is_dark_while_running(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        assert all(px == (0, 0, 0) for px in a.led_frame())


class TestStateForTheUi:
    def test_reports_its_mode(self, tmp_path):
        assert app(tmp_path).state()["mode"] == MODE_CALIBRATE

    def test_exposes_calibration_progress_while_calibrating(self, tmp_path):
        # 1 resting-offset + 4 softpot positions + 4 lidar positions
        assert app(tmp_path).state()["calibration"]["total"] == 9

    def test_exposes_the_live_frame_while_running(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        a.on_key(x=3, y=3, pressed=True, t_mono=1.0)
        chans = a.state()["frame"]["channels"]
        pad = next(c for c in chans if c["id"] == "pad")
        assert pad["points"]

    def test_the_frame_in_state_is_the_wire_format_not_a_bespoke_shape(self, tmp_path):
        from septagraph.dimensional import format as fmt
        a = app(tmp_path, start_mode=MODE_RUN)
        assert fmt.frame_from_dict(a.state()["frame"]).source == "neuralgrid"


class TestRawInState:
    """Tuning by eye needs raw next to normalized, in both modes."""

    def test_exposes_raw_counts_while_running(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        a.on_analog((5, 6, 7, 8, 9, 10, 11, 12))
        assert a.state()["raw"]["analog"] == [5, 6, 7, 8, 9, 10, 11, 12]

    def test_exposes_raw_counts_while_calibrating(self, tmp_path):
        a = app(tmp_path)
        a.on_analog((900, 0, 0, 0, 0, 0, 0, 0))
        assert a.state()["raw"]["analog"][0] == 900

    def test_raw_lidar_is_visible_too(self, tmp_path):
        a = app(tmp_path, start_mode=MODE_RUN)
        a.on_lidar((111, 222, 333, 0xFFFF))
        assert a.state()["raw"]["lidar"] == [111, 222, 333, 0xFFFF]


class TestFsrMode:
    """The per-pad program is its own mode: run it every housing open."""

    def _in_fsr(self, tmp_path):
        from septagraph.dimensional.app import MODE_FSR
        a = app(tmp_path)
        _rest(a)                      # capture resting offsets first
        a.start_fsr_calibration()
        return a

    def test_can_be_launched_on_its_own(self, tmp_path):
        from septagraph.dimensional.app import MODE_FSR
        assert self._in_fsr(tmp_path).mode == MODE_FSR

    def test_it_inherits_the_resting_offsets_just_measured(self, tmp_path):
        a = app(tmp_path)
        for _ in range(10):
            a.on_analog((0, 0, 0, 0, 3300, 1400, 0, 0))
        a.start_fsr_calibration()
        assert a.fsr.rest == (3300, 1400, 0, 0)

    def test_lights_the_pad_being_calibrated(self, tmp_path):
        a = self._in_fsr(tmp_path)
        assert a.led_frame()[0] != (0, 0, 0)

    def _sweep_pad(self, a, pad=0, peak=800):
        """soft -> hard -> soft. A stab no longer calibrates a pad."""
        a.on_key(pad % 8, pad // 8, True)
        levels = [k / 6 for k in range(1, 7)]
        for f in levels + levels[-2::-1]:
            p = int(peak * f)
            a.on_analog((0, 0, 0, 0, p, p // 4, p // 8, p // 16))
        a.on_key(pad % 8, pad // 8, False)

    def test_a_press_is_recorded_and_moves_to_the_next_pad(self, tmp_path):
        a = self._in_fsr(tmp_path)
        self._sweep_pad(a)
        assert a.fsr.current_pad == 1 and 0 in a.fsr.result()

    def test_finishing_writes_the_profile_into_the_calibration(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        a = self._in_fsr(tmp_path)
        self._sweep_pad(a)
        a.finish_fsr_calibration()
        assert 0 in Calibration.load(tmp_path / "calibration.json").fsr_pads

    def test_finishing_hands_off_to_the_encoder(self, tmp_path):
        from septagraph.dimensional.app import MODE_RUN
        a = self._in_fsr(tmp_path)
        a.finish_fsr_calibration()
        assert a.mode == MODE_RUN

    def test_the_encoder_picks_the_profile_up_immediately(self, tmp_path):
        a = self._in_fsr(tmp_path)
        self._sweep_pad(a)
        measured = a.fsr.result()[0]
        a.finish_fsr_calibration()
        assert a.encoder.calibration.fsr_pads_raw[0] == measured

    def test_sensor_calibration_is_untouched_by_the_fsr_pass(self, tmp_path):
        a = self._in_fsr(tmp_path)
        self._sweep_pad(a)
        a.finish_fsr_calibration()
        # the pad pass must not wipe the map the one-time program established
        assert a.encoder.calibration.fsr_rest == (0, 0, 0, 0)

    def test_state_exposes_which_pad_to_hit(self, tmp_path):
        a = self._in_fsr(tmp_path)
        assert a.state()["fsr"]["pad"] == 0


class TestFsrAutosave:
    """A restart must never cost a calibration session."""

    def _sweep(self, a, pad, peak=800):
        a.on_key(pad % 8, pad // 8, True)
        levels = [k / 6 for k in range(1, 7)]
        for f in levels + levels[-2::-1]:
            p = int(peak * f)
            a.on_analog((0, 0, 0, 0, p, p // 4, p // 8, p // 16))
        a.on_key(pad % 8, pad // 8, False)

    def _started(self, tmp_path):
        a = app(tmp_path)
        a.start_fsr_calibration()
        for _ in range(10):
            a.on_analog((0,) * 8)
        return a

    def test_progress_is_written_as_pads_are_done(self, tmp_path):
        a = self._started(tmp_path)
        self._sweep(a, 0)
        assert (tmp_path / "fsr_progress.json").exists()

    def test_an_interrupted_session_resumes(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_FSR
        a = self._started(tmp_path)
        for pad in range(5):
            self._sweep(a, pad)
        # service restarts
        b = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        assert b.resume_fsr_calibration()
        assert b.mode == MODE_FSR and b.fsr.done == 5

    def test_it_resumes_at_the_next_uncalibrated_pad(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        a = self._started(tmp_path)
        for pad in range(5):
            self._sweep(a, pad)
        b = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        b.resume_fsr_calibration()
        assert b.fsr.current_pad == 5

    def test_resuming_keeps_the_curves(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        a = self._started(tmp_path)
        self._sweep(a, 0)
        b = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        b.resume_fsr_calibration()
        assert len(b.fsr.curves[0]) > 5

    def test_nothing_to_resume_is_reported_honestly(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        b = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        assert b.resume_fsr_calibration() is False


class TestRemeasureRest:
    """The resting load drifts; re-zeroing must not cost the pad profile."""

    def _with_profile(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        cal = Calibration(
            fsr_rest=(80, 2155, 0, 232),
            fsr_pads={0: (500, 200, 60, 40)},
            fsr_pads_raw={0: (500, 200, 60, 40)},
        )
        cal.save(tmp_path / "calibration.json")
        return app(tmp_path, start_mode=MODE_RUN)

    def test_starting_it_enters_a_rest_capture(self, tmp_path):
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        assert a.capturing_rest

    def test_it_settles_after_enough_steady_samples(self, tmp_path):
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        for _ in range(20):
            a.on_analog((0, 0, 0, 0, 120, 2300, 45, 300))
        assert not a.capturing_rest

    def test_the_new_zero_point_is_taken(self, tmp_path):
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        for _ in range(20):
            a.on_analog((0, 0, 0, 0, 120, 2300, 45, 300))
        assert a.encoder.calibration.fsr_rest == (120, 2300, 45, 300)

    def test_the_pad_profile_survives(self, tmp_path):
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        for _ in range(20):
            a.on_analog((0, 0, 0, 0, 120, 2300, 45, 300))
        assert a.encoder.calibration.fsr_pads[0] == (500, 200, 60, 40)

    def test_it_is_written_to_disk(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        for _ in range(20):
            a.on_analog((0, 0, 0, 0, 120, 2300, 45, 300))
        assert Calibration.load(tmp_path / "calibration.json").fsr_rest == (120, 2300, 45, 300)

    def test_a_stray_sample_cannot_skew_it(self, tmp_path):
        a = self._with_profile(tmp_path)
        a.start_rest_capture()
        for _ in range(19):
            a.on_analog((0, 0, 0, 0, 120, 2300, 45, 300))
        a.on_analog((0, 0, 0, 0, 3900, 2300, 45, 300))
        assert a.encoder.calibration.fsr_rest[0] == 120


class TestCalibrationProgramsDoNotClobberEachOther:
    """Two programs write one file. The sensor map is run ONCE; the pad profile
    is twenty minutes of work re-done every housing open. Finishing either must
    never destroy the other -- it did, and it cost a full pad session."""

    def _with_pads(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        Calibration(
            fsr_rest=(80, 2155, 0, 232),
            fsr_pads={0: (500, 200, 60, 40)},
            fsr_pads_raw={0: (500, 200, 60, 40)},
            fsr_curves={0: [(10, 5, 2, 1), (500, 200, 60, 40)]},
            softpot_map={"softpot_n": 2},
            force_curve=0.42,
        ).save(tmp_path / "calibration.json")
        return app(tmp_path)

    def test_finishing_the_sensor_map_keeps_the_pad_profile(self, tmp_path):
        a = self._with_pads(tmp_path)
        a.finish_calibration()
        assert a.encoder.calibration.fsr_pads == {0: (500, 200, 60, 40)}

    def test_it_keeps_the_raw_measurements_and_curves(self, tmp_path):
        a = self._with_pads(tmp_path)
        a.finish_calibration()
        cal = a.encoder.calibration
        assert cal.fsr_pads_raw and cal.fsr_curves

    def test_it_keeps_a_tuned_force_curve(self, tmp_path):
        a = self._with_pads(tmp_path)
        a.finish_calibration()
        assert a.encoder.calibration.force_curve == pytest.approx(0.42)

    def test_it_still_writes_what_it_actually_measured(self, tmp_path):
        a = self._with_pads(tmp_path)
        a.start_calibration()   # it boots into RUN now that a file exists
        _rest(a)
        _sweep(a, position="top", hw_index=1, lo=200, hi=3400)
        a.finish_calibration()
        assert a.encoder.calibration.softpot_map["softpot_n"] == 1

    def test_finishing_the_pad_pass_keeps_the_sensor_map(self, tmp_path):
        a = self._with_pads(tmp_path)
        a.start_fsr_calibration()
        a.finish_fsr_calibration()
        assert a.encoder.calibration.softpot_map == {"softpot_n": 2}

    def test_the_file_on_disk_carries_both(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        a = self._with_pads(tmp_path)
        a.finish_calibration()
        cal = Calibration.load(tmp_path / "calibration.json")
        assert cal.fsr_pads and cal.softpot_map


class TestSurvivesAReboot:
    """Calibration is a file on the Pi. Once it exists the instrument should
    come up PLAYING, not asking to be calibrated again."""

    def _calibrated(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        Calibration(
            fsr_rest=(80, 2155, 0, 232),
            fsr_pads={p: (500, 200, 60, 40) for p in range(64)},
            softpot={"softpot_n": (200, 3400)},
        ).save(tmp_path / "calibration.json")
        return tmp_path / "calibration.json"

    def test_a_fresh_install_asks_to_be_calibrated(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_CALIBRATE
        a = DimensionalApp(calibration_path=tmp_path / "calibration.json")
        assert a.mode == MODE_CALIBRATE

    def test_a_calibrated_instrument_comes_up_playing(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_RUN
        path = self._calibrated(tmp_path)
        assert DimensionalApp(calibration_path=path).mode == MODE_RUN

    def test_the_profile_is_loaded_at_boot(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        path = self._calibrated(tmp_path)
        a = DimensionalApp(calibration_path=path)
        assert len(a.encoder.calibration.fsr_pads) == 64

    def test_an_explicit_start_mode_still_wins(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_CALIBRATE
        path = self._calibrated(tmp_path)
        a = DimensionalApp(calibration_path=path, start_mode=MODE_CALIBRATE)
        assert a.mode == MODE_CALIBRATE

    def test_a_corrupt_file_does_not_brick_the_boot(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp, MODE_CALIBRATE
        p = tmp_path / "calibration.json"
        p.write_text("{ this is not json")
        assert DimensionalApp(calibration_path=p).mode == MODE_CALIBRATE

    def test_every_save_leaves_a_backup(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        path = self._calibrated(tmp_path)
        Calibration(fsr_pads={0: (1, 2, 3, 4)}).save(path)
        assert (tmp_path / "calibration.backup.json").exists()

    def test_the_backup_holds_the_PREVIOUS_good_copy(self, tmp_path):
        from septagraph.dimensional.encoders.neuralgrid import Calibration
        path = self._calibrated(tmp_path)
        Calibration(fsr_pads={0: (1, 2, 3, 4)}).save(path)
        assert len(Calibration.load(tmp_path / "calibration.backup.json").fsr_pads) == 64
