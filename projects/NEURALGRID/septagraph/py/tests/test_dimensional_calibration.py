"""The calibration program.

Three jobs, in this order:

1. **FSR resting offsets.** The housing pre-loads some sensors, so they sit well
   above zero untouched (measured: 3319 / 1419 / 0 / 0). Changes every time the
   housing is opened, which is what this file is for.
2. **Which strip/sensor is which.** The firmware's N/S/E/W is only wiring
   order; the physical arrangement was never established. Each step names a
   POSITION and discovers which hardware input answers.
3. **Travel and range** for whatever answered, captured in the same sweep.
"""
import pytest

from septagraph.dimensional.calibrate import CalibrationSession, Step
from septagraph.dimensional.encoders.neuralgrid import Calibration
from septagraph.axes import K


class TestStepList:
    def test_asks_for_resting_offsets_before_anything_is_touched(self):
        assert CalibrationSession().steps[0].kind == "fsr_rest"

    def test_covers_four_softpot_positions_and_four_lidar_positions(self):
        s = CalibrationSession()
        assert [st.kind for st in s.steps].count("softpot") == 4
        assert [st.kind for st in s.steps].count("lidar_arc") == 4

    def test_every_step_names_the_position_a_human_can_see(self):
        for st in CalibrationSession().steps:
            assert st.instruction
            if st.kind != "fsr_rest":
                assert st.position in ("top", "right", "bottom", "left")

    def test_no_step_claims_to_know_which_strip_is_north_in_advance(self):
        # the point of the rewrite: positions are discovered, not assumed
        for st in CalibrationSession().steps:
            assert st.hw_index is None


class TestFsrRest:
    def test_collects_a_resting_level_per_sensor(self):
        s = CalibrationSession()
        for _ in range(10):
            s.observe_analog((0, 0, 0, 0, 3300, 1400, 0, 0))
        assert s.current.rest == (3300, 1400, 0, 0)

    def test_is_unsatisfied_until_enough_samples_have_settled(self):
        s = CalibrationSession()
        s.observe_analog((0, 0, 0, 0, 3300, 1400, 0, 0))
        assert not s.current.satisfied

    def test_is_satisfied_once_it_has_watched_a_steady_rest(self):
        s = CalibrationSession()
        for _ in range(10):
            s.observe_analog((0, 0, 0, 0, 3300, 1400, 0, 0))
        assert s.current.satisfied

    def test_takes_the_median_so_one_stray_sample_cannot_skew_it(self):
        s = CalibrationSession()
        for _ in range(9):
            s.observe_analog((0, 0, 0, 0, 3300, 0, 0, 0))
        s.observe_analog((0, 0, 0, 0, 9, 0, 0, 0))
        assert s.current.rest[0] == 3300


class TestSoftpotDiscovery:
    def _rest(self, s):
        for _ in range(10):
            s.observe_analog((0,) * 8)
        s.advance()

    def test_claims_whichever_input_actually_moved(self, ):
        s = CalibrationSession()
        self._rest(s)
        s.observe_analog((0, 0, 2500, 0, 0, 0, 0, 0))  # input 2 answered
        assert s.current.hw_index == 2

    def test_records_travel_for_the_input_it_claimed(self):
        s = CalibrationSession()
        self._rest(s)
        for raw in (600, 3400):
            s.observe_analog((0, 0, raw, 0, 0, 0, 0, 0))
        assert (s.current.lo, s.current.hi) == (600, 3400)

    def test_prefers_the_input_that_moved_most_when_several_twitch(self):
        s = CalibrationSession()
        self._rest(s)
        s.observe_analog((200, 0, 2500, 0, 0, 0, 0, 0))
        assert s.current.hw_index == 2

    def test_will_not_claim_an_input_another_position_already_owns(self):
        s = CalibrationSession()
        self._rest(s)
        for raw in (600, 3400):
            s.observe_analog((0, 0, raw, 0, 0, 0, 0, 0))
        s.advance()
        s.observe_analog((0, 0, 3000, 0, 0, 0, 0, 0))  # same strip again
        assert s.current.hw_index is None

    def test_is_satisfied_only_after_a_full_sweep_of_a_claimed_input(self):
        s = CalibrationSession()
        self._rest(s)
        s.observe_analog((0, 0, 1000, 0, 0, 0, 0, 0))
        assert not s.current.satisfied
        s.observe_analog((0, 0, 3400, 0, 0, 0, 0, 0))
        assert s.current.satisfied


# NOTE: TestLidarDiscovery removed 2026-09-17. It asserted "claim the nearest
# valid return", which on this hardware claims the HOUSING -- three of four
# sensors sit pinned on a wall 15-42mm away. Ownership is now decided from arc
# recordings against each sensor's own baseline; see
# tests/test_dimensional_lidar_arcs.py.


class TestResult:
    def _full(self):
        s = CalibrationSession()
        for _ in range(10):
            s.observe_analog((0, 0, 0, 0, 3300, 1400, 0, 0))
        s.advance()
        # positions answered by inputs 2, 0, 3, 1 in prompt order
        for idx in (2, 0, 3, 1):
            for raw in (500, 3500):
                vals = [0] * K
                vals[idx] = raw
                s.observe_analog(tuple(vals))
            s.advance()
        for idx in (1, 3, 0, 2):
            near = [0xFFFF] * 4
            near[idx] = 150
            s.observe_lidar(tuple(near))
            far = [0xFFFF] * 4
            far[idx] = 1400
            s.observe_lidar(tuple(far))
            s.advance()
        return s

    def test_maps_each_position_to_the_input_that_answered(self):
        cal = self._full().result()
        assert cal.softpot_map == {
            "softpot_n": 2, "softpot_e": 0, "softpot_s": 3, "softpot_w": 1,
        }

    def test_maps_each_lidar_position_too(self):
        assert self._full().result().lidar_map == {"n": 1, "e": 3, "s": 0, "w": 2}

    def test_stores_travel_against_the_position_not_the_raw_input(self):
        assert self._full().result().softpot["softpot_n"] == (500, 3500)

    def test_carries_the_fsr_resting_offsets(self):
        assert self._full().result().fsr_rest == (3300, 1400, 0, 0)

    def test_takes_the_furthest_lidar_range_seen_anywhere(self):
        assert self._full().result().lidar_max_mm == 1400

    def test_unanswered_positions_simply_fall_back_to_wiring_order(self):
        cal = CalibrationSession().result()
        assert cal.softpot_map == {} and isinstance(cal, Calibration)

    def test_saves_where_the_encoder_will_load_it(self, tmp_path):
        path = tmp_path / "calibration.json"
        s = self._full()
        s._path = path
        s.save()
        assert Calibration.load(path).softpot_map["softpot_n"] == 2


class TestAdvancing:
    def test_is_complete_after_the_last_step(self):
        s = CalibrationSession()
        for _ in range(len(s.steps)):
            s.advance()
        assert s.complete and s.current is None

    def test_redoing_a_step_releases_the_input_it_had_claimed(self):
        s = CalibrationSession()
        for _ in range(10):
            s.observe_analog((0,) * 8)
        s.advance()
        s.observe_analog((0, 0, 2500, 0, 0, 0, 0, 0))
        s.goto(s.current.key)
        assert s.current.hw_index is None
        s.observe_analog((0, 0, 2500, 0, 0, 0, 0, 0))
        assert s.current.hw_index == 2  # free to be claimed again


class TestLedGuidance:
    def test_lights_a_cue_for_the_current_step(self):
        frame = CalibrationSession().led_frame()
        assert len(frame) == 64 and any(px != (0, 0, 0) for px in frame)

    def test_the_cue_marks_the_edge_being_asked_about(self):
        s = CalibrationSession()
        s.goto_kind("softpot")  # first softpot step = top
        top = s.led_frame()
        s.advance()             # right
        assert s.led_frame() != top

    def test_goes_dark_when_complete(self):
        s = CalibrationSession()
        for _ in range(len(s.steps)):
            s.advance()
        assert all(px == (0, 0, 0) for px in s.led_frame())
