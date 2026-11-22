"""Lidar calibration by arcs.

The sensors stare at the housing until a hand comes in. So the wall reading is
the REFERENCE, not noise: a hand is whatever departs from it.

Two slow arcs over each perceived edge. While you arc over one edge the other
three sensors see only wall, so every arc is a control for the others and the
baseline falls out for free. One motion yields three results per sensor:

    baseline  what it reads with no hand (the housing)
    owner     which sensor actually responded to that edge
    bounds    the usable range of hand readings on that sensor
"""
import pytest

from septagraph.dimensional.lidar_arcs import ArcRecorder, analyze_arcs

WALL = {0: 41, 1: 65535, 2: 16, 3: 30}  # measured on hardware 2026-09-16


def _wall_sample(overrides=None):
    s = dict(WALL)
    s.update(overrides or {})
    return tuple(s[i] for i in range(4))


def _record(rec, position, owner=None, hand_values=()):
    """Arc over `position`; `owner` dips as the hand passes in front of it."""
    rec.begin(position)
    for _ in range(12):
        rec.observe(_wall_sample())
    for v in hand_values:
        rec.observe(_wall_sample({owner: v} if owner is not None else None))
    for _ in range(12):
        rec.observe(_wall_sample())
    rec.end()


def _full_session():
    rec = ArcRecorder()
    _record(rec, "top", owner=0, hand_values=(30, 24, 20, 24, 30))
    _record(rec, "right", owner=2, hand_values=(12, 9, 7, 9, 12))
    _record(rec, "bottom", owner=3, hand_values=(22, 18, 15, 18, 22))
    _record(rec, "left")  # sensor 1 sees nothing at all -- no hand response
    return rec


class TestRecording:
    def test_keeps_samples_per_edge(self):
        rec = ArcRecorder()
        _record(rec, "top", owner=0, hand_values=(30, 20))
        assert len(rec.samples["top"]) == 26

    def test_ignores_samples_outside_a_recording_window(self):
        rec = ArcRecorder()
        rec.observe(_wall_sample())
        assert rec.samples == {}

    def test_a_second_arc_on_the_same_edge_appends(self):
        rec = ArcRecorder()
        _record(rec, "top", owner=0, hand_values=(30,))
        _record(rec, "top", owner=0, hand_values=(30,))
        assert len(rec.samples["top"]) == 50

    def test_reports_how_much_an_edge_has_been_moved_over(self):
        rec = ArcRecorder()
        _record(rec, "top", owner=0, hand_values=(30, 24, 20))
        assert rec.response("top") > 0

    def test_an_edge_with_no_hand_response_reports_nothing(self):
        rec = ArcRecorder()
        _record(rec, "left")
        assert rec.response("left") == 0


class TestBaseline:
    def test_learns_what_each_sensor_reads_with_no_hand(self):
        r = analyze_arcs(_full_session())
        assert r.baseline[0] == pytest.approx(41, abs=2)
        assert r.baseline[2] == pytest.approx(16, abs=2)

    def test_a_sensor_that_never_returns_anything_has_no_baseline(self):
        assert 1 not in analyze_arcs(_full_session()).baseline

    def test_the_hand_does_not_drag_the_baseline_down(self):
        # the owner is only obscured during its own arc: a minority of samples
        r = analyze_arcs(_full_session())
        assert r.baseline[0] > 35


class TestOwnership:
    def test_binds_each_edge_to_the_sensor_that_responded(self):
        r = analyze_arcs(_full_session())
        assert r.owner == {"top": 0, "right": 2, "bottom": 3}

    def test_an_edge_no_sensor_answered_is_left_unbound(self):
        assert "left" not in analyze_arcs(_full_session()).owner

    def test_one_sensor_cannot_own_two_edges(self):
        rec = ArcRecorder()
        _record(rec, "top", owner=0, hand_values=(30, 20, 30))
        _record(rec, "right", owner=0, hand_values=(28, 18, 28))
        owners = analyze_arcs(rec).owner
        assert len(set(owners.values())) == len(owners)


class TestBounds:
    def test_bounds_span_the_hand_readings_not_the_wall(self):
        near, far = analyze_arcs(_full_session()).bounds["top"]
        assert near == pytest.approx(20, abs=2)
        assert far < 41  # the wall is excluded

    def test_the_usable_depth_is_reported_so_a_bad_mount_is_visible(self):
        # If a sensor only has a couple of centimetres between wall and hand,
        # that is a mounting problem and the numbers should say so plainly.
        r = analyze_arcs(_full_session())
        assert r.usable_mm["top"] == pytest.approx(r.bounds["top"][1] - r.bounds["top"][0], abs=1)

    def test_flags_an_edge_whose_working_volume_is_too_shallow_to_play(self):
        # Clearly detected (well past DEPART_MM from the 41mm wall) but with
        # almost no travel: the hand has ~2mm of room. That is a mount problem,
        # not a detection problem -- distinct from a hand nobody can see at all.
        rec = ArcRecorder()
        _record(rec, "top", owner=0, hand_values=(30, 29, 28, 29, 30))
        r = analyze_arcs(rec)
        assert r.owner["top"] == 0
        assert r.usable_mm["top"] < 15
        assert "top" in r.too_shallow


class TestHandoffToTheEncoder:
    def test_produces_a_calibration_the_encoder_understands(self):
        cal = analyze_arcs(_full_session()).to_calibration()
        assert cal.lidar_map["n"] == 0
        assert cal.lidar_bounds["n"][1] < 41

    def test_emits_hardcodable_constants(self):
        # user will run this once and bake the result in
        src = analyze_arcs(_full_session()).as_python()
        assert "LIDAR_MAP" in src and "LIDAR_BOUNDS" in src and "LIDAR_BASELINE" in src

    def test_the_emitted_constants_are_valid_python(self):
        ns = {}
        exec(analyze_arcs(_full_session()).as_python(), ns)
        assert ns["LIDAR_MAP"]["n"] == 0


class TestSessionIntegration:
    """The calibration session drives the recorder, so the lidar steps stop
    claiming 'nearest valid return' -- which on this hardware claims the
    housing wall, not the hand."""

    def _session_through_softpots(self):
        from septagraph.dimensional.calibrate import CalibrationSession
        s = CalibrationSession()
        s.goto_kind("lidar_arc")
        return s

    def test_lidar_steps_are_arc_steps(self):
        s = self._session_through_softpots()
        assert s.current.kind == "lidar_arc"
        assert s.current.position == "top"

    def test_wall_readings_alone_do_not_satisfy_a_step(self):
        s = self._session_through_softpots()
        for _ in range(20):
            s.observe_lidar(_wall_sample())
        assert not s.current.satisfied

    def test_a_hand_passing_through_satisfies_the_step(self):
        s = self._session_through_softpots()
        for _ in range(10):
            s.observe_lidar(_wall_sample())
        for v in (30, 24, 20, 24, 30):
            s.observe_lidar(_wall_sample({0: v}))
        assert s.current.satisfied

    def test_the_result_carries_the_discovered_map_and_bounds(self):
        s = self._session_through_softpots()
        for position, owner, vals in (
            ("top", 0, (30, 24, 20)),
            ("right", 2, (12, 9, 7)),
            ("bottom", 3, (22, 18, 15)),
        ):
            for _ in range(10):
                s.observe_lidar(_wall_sample())
            for v in vals:
                s.observe_lidar(_wall_sample({owner: v}))
            s.advance()
        cal = s.result()
        assert cal.lidar_map == {"n": 0, "e": 2, "s": 3}
        assert cal.lidar_bounds["n"][1] < 41  # the wall is excluded

    def test_redoing_an_arc_discards_what_it_recorded(self):
        s = self._session_through_softpots()
        for v in (30, 24, 20):
            s.observe_lidar(_wall_sample({0: v}))
        s.goto(s.current.key)
        assert not s.current.satisfied
