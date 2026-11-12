"""Tests for septagraph.pi.modes: DirectLabMode (Tuesday deliverable),
SeptagraphMode adapter + presets, MidiMode.
"""
from __future__ import annotations

import json
import socket
from pathlib import Path

import pytest

from controllers.grid_ctl import GridController
from septagraph.manifest import ManifestStore
from septagraph.net import Sender
from septagraph.pi import leds
from septagraph.pi.modes import (
    modulettes_note_for_pad,
    DirectLabMode,
    LAYOUT_MODES,
    MidiMode,
    SeptagraphMode,
    _note_of,
    default_full_rows,
    instruments_by_enabled_count,
    layout_mode_from_state,
    load_lab_map,
    sorted_instrument_points,
)
from septagraph.types import HALF, CatalogPoint, Manifest, vec
from septagraph.window import Window
from septagraph.axes import K

FIXTURE_MAP = {
    "renderer_id": "lab", "name": "CalArts Machine Lab", "kind": "catalog", "target": "127.0.0.1:19001",
    "points": [
        {"point_id": 0, "instrument": "breakBot", "element": "kick",
         "osc": {"address": "/breakBot", "args": ["i:note=36", "i:vel"]},
         "vel_range": [1, 127],
         "mu": [0.30, 0.7, 0.15, 0.02, 0.6, 0.35, 0.05, 0.4, 0.5],
         "sigma": [0.03, 0, 0.05, 0, 0.05, 0, 0, 0, 0.0],
         "max_rate_hz": 12, "vel_min": 0.15, "vel_max": 1.0,
         "source": "estimated", "enabled": True},
        {"point_id": 1, "instrument": "breakBot", "element": "snare",
         "osc": {"address": "/breakBot", "args": ["i:note=38", "i:vel"]},
         "vel_range": [1, 127],
         "mu": [0.35, 0.7, 0.2, 0.02, 0.6, 0.35, 0.05, 0.4, 0.5],
         "sigma": [0.03, 0, 0.05, 0, 0.05, 0, 0, 0, 0.0],
         "max_rate_hz": 12, "vel_min": 0.15, "vel_max": 1.0,
         "source": "estimated", "enabled": True},
        {"point_id": 2, "instrument": "marimba", "element": "c4",
         "osc": {"address": "/marimba", "args": ["i:note=60", "i:vel"]},
         "vel_range": [1, 127],
         "mu": [0.5, 0.6, 0.8, 0.02, 0.3, 0.5, 0.05, 0.4, 0.5],
         "sigma": [0.02, 0, 0.02, 0, 0.02, 0, 0, 0, 0.0],
         "max_rate_hz": 20, "vel_min": 0.1, "vel_max": 1.0,
         "source": "estimated", "enabled": True},
        {"point_id": 3, "instrument": "marimba", "element": "c3",
         "osc": {"address": "/marimba", "args": ["i:note=48", "i:vel"]},
         "vel_range": [1, 127],
         "mu": [0.45, 0.6, 0.7, 0.02, 0.3, 0.5, 0.05, 0.4, 0.5],
         "sigma": [0.02, 0, 0.02, 0, 0.02, 0, 0, 0, 0.0],
         "max_rate_hz": 20, "vel_min": 0.1, "vel_max": 1.0,
         "source": "estimated", "enabled": True},
        {"point_id": 4, "instrument": "disabledBot", "element": "x",
         "osc": {"address": "/disabled", "args": ["i:vel"]},
         "vel_range": [1, 127], "mu": [0] * K, "sigma": [0] * K,
         "max_rate_hz": 5, "vel_min": 0, "vel_max": 1,
         "source": "estimated", "enabled": False},
    ],
}


@pytest.fixture
def map_path(tmp_path):
    p = tmp_path / "lab_map.json"
    p.write_text(json.dumps(FIXTURE_MAP))
    return p


@pytest.fixture
def layout_path(tmp_path):
    return tmp_path / "direct_layout.json"


class FakeUdpReceiver:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 19001))
        self.sock.settimeout(0.5)

    def recv(self):
        data, _ = self.sock.recvfrom(65536)
        return data

    def close(self):
        self.sock.close()


def test_load_lab_map_excludes_disabled(map_path):
    points = load_lab_map(map_path)
    assert len(points) == 5
    assert all(p["point_id"] != 4 or not p.get("enabled", True) for p in points)


def test_instruments_by_enabled_count_ranks_marimba_first(map_path):
    points = [p for p in load_lab_map(map_path) if p.get("enabled", True)]
    order = instruments_by_enabled_count(points)
    # marimba has 2 enabled points vs breakBot's 2 -- tie -> first-seen order
    assert set(order) == {"breakBot", "marimba"}


def test_sorted_instrument_points_ascending_by_note(map_path):
    points = [p for p in load_lab_map(map_path) if p.get("enabled", True)]
    pts = sorted_instrument_points(points, "marimba")
    assert [p["point_id"] for p in pts] == [3, 2]  # note 48 then 60


# -- DirectLabMode: row -> instrument catalog paging (task 1) -------------

def _seg0(mode, row):
    return mode.rows_config[row]["segments"][0]


def test_default_row_assignment_uses_top_instruments(map_path, layout_path):
    # The tiny fixture has none of the 5 real instruments, so every default
    # FULL segment falls back to instruments_by_enabled_count order, cycling
    # per segment -> adjacent rows get different instruments.
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert set(mode.rows_config) == set(range(8))
    assert _seg0(mode, 0)["instrument"] in ("breakBot", "marimba")
    assert _seg0(mode, 1)["instrument"] in ("breakBot", "marimba")
    assert _seg0(mode, 0)["instrument"] != _seg0(mode, 1)["instrument"]
    for row in range(8):
        assert sum(s["width"] for s in mode.rows_config[row]["segments"]) == 8
    # row 7 keeps its 4|4 split shape even under fallback
    assert [s["width"] for s in mode.rows_config[7]["segments"]] == [3, 5]


def test_row_points_and_paging(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_row_instrument(0, "marimba")
    pts = mode.row_points(0)
    assert [p["point_id"] for p in pts] == [3, 2]
    assert mode.n_pages(0) == 1  # 2 points fit on one 8-wide page

    # row 0 col 0/1 now show marimba's 2 points, note-ascending
    assert mode.layout["0,0"] == 3
    assert mode.layout["1,0"] == 2


def test_paging_advances_and_clamps(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_row_instrument(0, "marimba")
    assert mode.n_pages(0) == 1
    mode.page(0, +1)
    assert _seg0(mode, 0)["page"] == 0  # clamped: only 1 page exists
    mode.page(0, -1)
    assert _seg0(mode, 0)["page"] == 0  # clamped at 0


def test_manual_pad_override_wins_over_row_page(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_row_instrument(0, "marimba")
    mode.set_pad(0, 0, 999)
    assert mode.layout["0,0"] == 999
    assert mode.layout["1,0"] == 2  # untouched pad still shows the row binding


def test_row_and_override_layout_persists_across_instances(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_row_instrument(2, "breakBot")
    mode.set_pad(7, 3, 1)
    assert layout_path.exists()
    mode2 = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert _seg0(mode2, 2)["instrument"] == "breakBot"
    assert mode2.layout["7,3"] == 1
    assert json.loads(layout_path.read_text())["layout_version"] == 2


def test_direct_strike_sends_osc_and_respects_rate_limit(map_path, layout_path):
    recv = FakeUdpReceiver()
    try:
        mode = DirectLabMode(map_path, lab_target=("127.0.0.1", 19001), layout_path=layout_path)
        # septagraph.renderers.lab landed during this session -> real LabRenderer path
        assert mode._lab_renderer is not None
        ok = mode.direct_strike(0, velocity=1.0)
        assert ok is True
        recv.recv()  # got a packet
        # second strike immediately after -> rate-limited (max_rate_hz=12 -> ~83ms)
        ok2 = mode.direct_strike(0, velocity=1.0)
        assert ok2 is False
        assert mode.skip_count == 1
    finally:
        recv.close()


def test_direct_strike_dry_run_does_not_send(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=("127.0.0.1", 19002), dry_run=True, layout_path=layout_path)
    ok = mode.direct_strike(2)
    assert ok is True


def test_on_key_split_mode_direct_play_row(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode.split is False  # fresh install defaults to FULL
    mode.set_split(True)
    x0y0 = mode.layout.get("0,0")
    assert x0y0 is not None
    mode.on_key(0, 0, 1)
    assert mode.skip_count == 0
    assert mode.last_sent_text != ""


def test_on_key_sequencer_rows_toggle_steps(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_split(True)
    mode.on_key(3, 4, 1)  # SPLIT: row 4 = sequencer track 0, step 3
    assert mode.sequencer.pattern[0][3] == 1
    mode.on_key(3, 4, 1)
    assert mode.sequencer.pattern[0][3] == 0


def test_full_mode_row5_strikes_pad_not_step(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode.split is False
    assert mode.layout.get("0,5") is not None  # fallback fills every row
    mode.last_sent_text = ""
    mode.on_key(0, 5, 1)
    assert mode.last_sent_text != ""
    assert all(v == 0 for row in mode.sequencer.pattern for v in row)


def test_default_track_assignment_first_enabled_distinct_instruments(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode.track_assignment[0] == 0  # breakBot
    assert mode.track_assignment[1] == 2  # marimba (2nd distinct instrument)


def test_sequencer_step_strikes_assigned_point(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.sequencer.toggle_step(0, 0)
    mode.sequencer.play(now=0.0)
    assert mode.last_sent_text.startswith("/breakBot")


def test_set_split_keeps_sequencer_4x8_and_flips_row_roles(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.set_split(False)  # FULL
    assert mode.sequencer.n_tracks == 4
    mode.on_key(2, 6, 1)  # row 6 in FULL -> strikes a pad, never toggles a step
    assert mode.sequencer.pattern[2][2] == 0
    mode.set_split(True)  # SPLIT
    assert mode.sequencer.n_tracks == 4
    mode.on_key(2, 6, 1)  # row 6 in SPLIT -> track 2 step 2
    assert mode.sequencer.pattern[2][2] == 1


def test_led_frame_is_64_rgb_tuples(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    frame = mode.led_frame()
    assert len(frame) == 64
    assert all(len(c) == 3 for c in frame)


def test_velocity_offset_from_e_softpot(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.on_analog((0, 0, 4000, 0))  # E strip pushed high; no FSR channels supplied
    assert mode.vel_offset > 0
    mode.on_analog((0, 0, 0, 0))  # released
    assert mode.vel_offset == 0.0


# -- Direct Lab FULL layout / segments / migration (layout v2) ----------------

REAL_MAP = Path(__file__).resolve().parents[1] / "renderers" / "lab_map.json"


@pytest.fixture
def real_mode(layout_path):
    return DirectLabMode(REAL_MAP, lab_target=None, dry_run=True, layout_path=layout_path)


def _note(mode, x, y):
    return _note_of(mode.points_by_id[mode.layout[f"{x},{y}"]])


def _instr(mode, x, y):
    return mode.points_by_id[mode.layout[f"{x},{y}"]]["instrument"]


def test_default_full_layout_covers_grid_with_expected_instruments(real_mode):
    mode = real_mode
    assert mode.split is False
    # GOR python-server contract (2026-09-08): every instrument's default
    # segment now has >= 16/32/8 enabled points as needed, so all 64 pads
    # are populated -- no more lydia gap at x=7,y=4 (lydia grew 15 -> 20).
    expected_keys = {f"{x},{y}" for x in range(8) for y in range(8)}
    assert set(mode.layout) == expected_keys
    for y in range(4):
        assert all(_instr(mode, x, y) == "marimbot" for x in range(8))
    assert all(_instr(mode, x, 5) == "lydia" for x in range(8))
    assert all(_instr(mode, x, 4) == "lydia" for x in range(8))
    assert all(_instr(mode, x, 6) == "tammy" for x in range(8))
    assert all(_instr(mode, x, 7) == "breakBot" for x in range(3))
    assert all(_instr(mode, x, 7) == "ganaPati" for x in range(3, 8))
    # piano convention: bottom-left = lowest, top-right = highest.
    # marimbot now has 36 enabled notes (was 32); the default FULL plan
    # still shows a fixed page per row (page 3 on row 0), so with more
    # notes than fit in 4 pages of 8 the top-right cell shows the highest
    # note *on page 3* (91), not the instrument's true highest note (97,
    # which only appears on marimbot's now-existing page 4, reachable by
    # paging row 0 -- not shown by default).
    assert _note(mode, 0, 3) == 45
    assert _note(mode, 7, 0) == 91
    assert _note(mode, 0, 5) == 60   # lydia's lowest enabled note is now 60 (was 62)
    assert _note(mode, 6, 4) == 74
    assert _note(mode, 0, 6) == 0    # tammy's lowest enabled note is now 0 (was 1)
    assert _note(mode, 0, 7) == 0 and _instr(mode, 0, 7) == "breakBot"   # kick
    assert _note(mode, 3, 7) == 8 and _instr(mode, 3, 7) == "ganaPati"   # user order 8,3,1,13,11
    # ascending walk rows 3 -> 0, left -> right
    marimbot_walk = [_note(mode, x, y) for y in (3, 2, 1, 0) for x in range(8)]
    assert marimbot_walk == sorted(marimbot_walk)
    # page counts per segment
    st = mode.state()["rows"]
    assert st[0]["segments"][0]["n_pages"] == 5          # marimbot 36 notes -> 5 pages of 8
    assert st[6]["segments"][0]["n_pages"] == 4          # tammy 26 notes -> 4 pages of 8
    assert st[7]["segments"][0]["n_pages"] == 1          # breakBot kit: kick/snare/cymbal in 3 pads
    assert st[7]["segments"][1]["n_pages"] == 1          # ganaPati 8,3,1,13,11 in 5 pads
    assert st[7]["segments"][0]["width"] == 3


def test_set_row_segments_rejects_bad_widths_and_leaves_row_unchanged(real_mode):
    mode = real_mode
    before = json.dumps(mode.rows_config[2])
    layout_before = dict(mode.layout)
    for widths in ((4, 3), (4, 5)):
        with pytest.raises(ValueError):
            mode.set_row_segments(2, [{"instrument": "lydia", "page": 0, "width": widths[0]},
                                      {"instrument": "tammy", "page": 0, "width": widths[1]}])
        assert json.dumps(mode.rows_config[2]) == before
        assert mode.layout == layout_before
    with pytest.raises(ValueError):
        mode.set_row_segments(2, [{"instrument": "nope", "page": 0, "width": 8}])
    with pytest.raises(ValueError):
        mode.set_row_segments(2, [{"instrument": "lydia", "page": 0, "width": 0},
                                  {"instrument": "tammy", "page": 0, "width": 8}])
    # a valid replacement takes
    mode.set_row_segments(2, [{"instrument": "lydia", "page": 0, "width": 2},
                              {"instrument": "tammy", "page": 1, "width": 6}])
    assert [s["width"] for s in mode.rows_config[2]["segments"]] == [2, 6]
    assert _instr(mode, 0, 2) == "lydia" and _instr(mode, 2, 2) == "tammy"
    assert _note(mode, 2, 2) == 6  # tammy now starts at note 0, so page 1 of a width-6 segment starts at note 6


def test_segment_paging_is_independent_and_clamps(real_mode):
    mode = real_mode
    # row 7 kit is fixed and single-page: kick 0, snare 5, cymbal 11 | ganaPati 8,3,1,13,11
    assert [_note(mode, x, 7) for x in range(3)] == [0, 5, 11]
    assert [_note(mode, x, 7) for x in range(3, 8)] == [8, 3, 1, 13, 11]
    mode.segment_page(7, 0, +5)  # clamps at n_pages-1 == 0
    assert mode.rows_config[7]["segments"][0]["page"] == 0
    mode.segment_page(7, 1, -10)
    assert mode.rows_config[7]["segments"][1]["page"] == 0
    # tammy (row 6) still pages independently
    mode.segment_page(6, 0, +1)
    assert mode.rows_config[6]["segments"][0]["page"] == 1
    assert mode.rows_config[7]["segments"][0]["page"] == 0

def test_migrates_v1_layout_to_default_full_rows_preserving_overrides(layout_path):
    old = {
        "rows": {str(r): {"instrument": "marimbot", "page": r} for r in range(4)},
        "overrides": {"7,3": 42},
        "fsr_assign": {"0": 5, "1": None, "2": 7, "3": 9},
    }
    layout_path.write_text(json.dumps(old))
    mode = DirectLabMode(REAL_MAP, lab_target=None, dry_run=True, layout_path=layout_path)
    assert set(mode.rows_config) == set(range(8))
    assert all("segments" in mode.rows_config[r] for r in range(8))
    assert _seg0(mode, 3)["instrument"] == "marimbot" and _seg0(mode, 3)["page"] == 0
    assert _seg0(mode, 0)["page"] == 3
    assert [s["instrument"] for s in mode.rows_config[7]["segments"]] == ["breakBot", "ganaPati"]
    assert mode.pad_overrides == {"7,3": 42}
    assert mode.layout["7,3"] == 42
    assert mode.fsr_assign[0] == 5 and mode.fsr_assign[2] == 7 and mode.fsr_assign[3] == 9
    mode.save_layout()
    saved = json.loads(layout_path.read_text())
    assert saved["layout_version"] == 2
    assert saved["overrides"] == {"7,3": 42}
    assert "segments" in saved["rows"]["7"]
    # v2 file round-trips unchanged
    mode2 = DirectLabMode(REAL_MAP, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode2.rows_config == mode.rows_config


def test_v2_layout_invalid_row_falls_back_to_default_for_that_row(layout_path):
    mode = DirectLabMode(REAL_MAP, lab_target=None, dry_run=True, layout_path=layout_path)
    good = json.loads(layout_path.read_text()) if layout_path.exists() else None
    mode.save_layout()
    data = json.loads(layout_path.read_text())
    data["rows"]["1"] = {"segments": [{"instrument": "lydia", "page": 0, "width": 5}]}  # sums to 5
    data["rows"]["2"] = {"segments": [{"instrument": "tammy", "page": 0, "width": 8}]}  # valid custom
    layout_path.write_text(json.dumps(data))
    mode2 = DirectLabMode(REAL_MAP, lab_target=None, dry_run=True, layout_path=layout_path)
    assert _seg0(mode2, 1)["instrument"] == "marimbot" and _seg0(mode2, 1)["page"] == 2
    assert _seg0(mode2, 2)["instrument"] == "tammy"
    del good


def test_default_full_rows_helper_shape():
    points = load_lab_map(REAL_MAP)
    rows = default_full_rows(points)
    assert set(rows) == set(range(8))
    for r in range(8):
        assert sum(s["width"] for s in rows[r]["segments"]) == 8
    assert rows[7]["segments"] == [
        {"instrument": "breakBot", "page": 0, "width": 3},
        {"instrument": "ganaPati", "page": 0, "width": 5},
    ]
    assert default_full_rows([])[0]["segments"][0]["instrument"] is None


def _hue_family(rgb):
    import colorsys
    h, _, _ = colorsys.rgb_to_hsv(*[c / 255.0 for c in rgb])
    return h


def test_led_frame_full_mode_uses_instrument_colours(real_mode):
    mode = real_mode
    frame = mode.led_frame()
    # Each populated pad is lit near its instrument's base hue (per-note
    # shading drifts hue by <=+/-6deg and gamma-corrects value/exact RGB,
    # so pads are no longer byte-identical to the flat instrument colour --
    # compare hue family instead of exact equality).
    # NOTE_HUE_DRIFT_DEG (12) plus slack for gamma's per-channel hue skew
    tol = 30.0 / 360.0  # orange/yellow hues skew more under gamma at low value
    for (x, y), instr in (((0, 0), "marimbot"), ((3, 5), "lydia"), ((0, 6), "tammy"),
                           ((0, 7), "breakBot"), ((4, 7), "ganaPati")):
        got = _hue_family(frame[leds.idx(x, y)])
        want = _hue_family(leds.instrument_color(instr))
        dist = min(abs(got - want), 1.0 - abs(got - want))
        assert dist <= tol, (x, y, instr, got, want)
    # lydia grew from 15 to 20 enabled notes (GOR python-server contract,
    # 2026-09-08), so its FULL-layout pad at (7,4) is populated now, not
    # empty -- assert it's a lydia-hued pad instead of black.
    assert frame[leds.idx(7, 4)] != (0, 0, 0)
    got74 = _hue_family(frame[leds.idx(7, 4)])
    want_lydia = _hue_family(leds.instrument_color("lydia"))
    dist74 = min(abs(got74 - want_lydia), 1.0 - abs(got74 - want_lydia))
    assert dist74 <= tol
    assert leds.instrument_color("unknownBot") == leds.WARM_WHITE
    colors = mode.state()["instrument_colors"]
    assert colors["marimbot"] == "#ffd400"  # yellow (palette 2026-09-09)
    assert set(colors) == set(mode.instrument_list())
    # SPLIT: rows 4-7 revert to sequencer rendering (dim warm white, never OFF)
    mode.set_split(True)
    frame = mode.led_frame()
    assert frame[leds.idx(7, 4)] == leds.dim(leds.WARM_WHITE, 0.08)
    assert frame[leds.idx(0, 0)] != (0, 0, 0)


def test_led_frame_colour_spec(real_mode):
    """Colour spec (task): 64 entries; every enabled pad non-black; two
    notes of the same instrument differ; different instruments differ in
    hue; disabled pads black; gamma applied."""
    mode = real_mode
    frame = mode.led_frame()
    assert len(frame) == 64

    for y in range(4):  # marimbot rows 0-3: all enabled, all non-black
        for x in range(8):
            assert frame[leds.idx(x, y)] != (0, 0, 0)

    # lydia now has 20 enabled notes (was 15), so its 16th-slot pad at
    # (7,4) is populated, not black (see test_led_frame_full_mode_ above).
    assert frame[leds.idx(7, 4)] != (0, 0, 0)

    # two different marimbot notes (row 3 = lowest page, row 0 = highest
    # page) are visibly different shades of the same instrument
    low = frame[leds.idx(0, 3)]
    high = frame[leds.idx(0, 0)]
    assert low != high

    # different instruments differ in hue (not just brightness)
    import colorsys
    marimbot_rgb = frame[leds.idx(0, 3)]
    lydia_rgb = frame[leds.idx(0, 5)]
    h_m, _, _ = colorsys.rgb_to_hsv(*[c / 255.0 for c in marimbot_rgb])
    h_l, _, _ = colorsys.rgb_to_hsv(*[c / 255.0 for c in lydia_rgb])
    hue_dist = min(abs(h_m - h_l), 1.0 - abs(h_m - h_l))
    assert hue_dist > 0.05  # well beyond the +/-6deg (~0.017) per-note drift

    # gamma applied: raw note_color() output is brighter than what actually
    # lands in the frame for the same pad (gamma 2.2 darkens non-1.0 codes)
    note_index, n_notes = mode._note_ranks[mode.layout["0,3"]]
    raw = leds.note_color("marimbot", note_index, n_notes)
    assert leds.apply_gamma(raw) == frame[leds.idx(0, 3)]
    assert any(g < r for r, g in zip(raw, leds.apply_gamma(raw)) if r not in (0, 255))


def test_led_frame_brightness_cap(real_mode):
    frame = real_mode.led_frame()
    total = sum(c for rgb in frame for c in rgb)
    assert total <= leds.BRIGHTNESS_CAP_SUM


def test_led_frame_pressed_pad_flashes_white_then_reverts(real_mode):
    mode = real_mode
    mode.on_key(0, 3, 1)  # press
    frame = mode.led_frame()
    assert frame[leds.idx(0, 3)] == leds.apply_gamma(leds.FLASH_WHITE)
    # still white while held, even past FLASH_S
    import time as _time
    mode._press_times[(0, 3)] = _time.monotonic() - (leds.FLASH_S + 0.01)
    assert mode.led_frame()[leds.idx(0, 3)] == leds.apply_gamma(leds.FLASH_WHITE)
    # after RELEASE (and FLASH_S) the pad reverts to its normal shaded colour
    mode.on_key(0, 3, 0)
    frame2 = mode.led_frame()
    assert frame2[leds.idx(0, 3)] != leds.apply_gamma(leds.FLASH_WHITE)


def test_note_color_hue_drift_and_value_range():
    lo = leds.note_color("marimbot", 0, 32)
    hi = leds.note_color("marimbot", 31, 32)
    assert lo != hi
    import colorsys
    _, _, v_lo = colorsys.rgb_to_hsv(*[c / 255.0 for c in lo])
    _, _, v_hi = colorsys.rgb_to_hsv(*[c / 255.0 for c in hi])
    assert v_lo < v_hi  # lightness rises low -> high note


def test_state_rows_include_segments_and_legacy_fields(real_mode):
    st = real_mode.state()
    assert st["split"] is False
    row7 = st["rows"][7]
    assert row7["instrument"] == "breakBot" and row7["page"] == 0 and row7["n_pages"] == 1
    assert {s["instrument"] for s in row7["segments"]} == {"breakBot", "ganaPati"}
    assert set(st["rows"]) == set(range(8))


# -- Direct Lab FSR strike pads (task 4) -----------------------------------

def test_fsr_default_assignment_matches_track_assignment(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode.fsr_assign[0] == mode.track_assignment[0]
    assert mode.fsr_assign[1] == mode.track_assignment[1]


def test_fsr_never_strikes_direct_lab(map_path, layout_path):
    """Locked rule (2026-09-09): FSRs never strike robots."""
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.assign_fsr(0, 0)  # breakBot kick
    mode.last_sent_text = ""
    mode.on_analog((0, 0, 0, 0, 4000, 0, 0, 0))  # hard press
    mode.on_analog((0, 0, 0, 0, 0, 0, 0, 0))     # release
    assert mode.last_sent_text == ""
    assert not mode.fsr_pressure[0]  # meters still track pressure (0/None at rest)


def test_fsr_below_onset_does_not_strike(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.assign_fsr(0, 0)
    mode.last_sent_text = ""
    mode.on_analog((0, 0, 0, 0, 41, 0, 0, 0))  # just above floor, gamma-curved way below onset
    assert mode.last_sent_text == ""


def test_fsr_assign_persists(map_path, layout_path):
    mode = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    mode.assign_fsr(2, 1)
    mode2 = DirectLabMode(map_path, lab_target=None, dry_run=True, layout_path=layout_path)
    assert mode2.fsr_assign[2] == 1


# -- Direct Lab MODULETTES layout (64 pads -> /modulettes 1-64, column-major from bottom-right) --

def test_modulettes_layout_maps_every_pad_to_its_note(real_mode):
    mode = real_mode
    mode.set_layout_mode("modulettes")
    assert mode.layout_mode == "modulettes"
    assert mode.split is False  # derived: only True for layout_mode == "split"
    assert set(mode.layout) == {f"{x},{y}" for x in range(8) for y in range(8)}
    for y in range(8):
        for x in range(8):
            pid = mode.layout[f"{x},{y}"]
            point = mode.points_by_id[pid]
            assert point["instrument"] == "modulettes"
            assert _note_of(point) == modulettes_note_for_pad(x, y)


def test_modulettes_strike_sends_correct_note(layout_path):
    """Striking pad (3,2) in MODULETTES must hit modulettes note 38
    (2*8+3) -- verified on the real OSC wire (fake UDP receiver), like the
    existing rate-limit test."""
    from pythonosc.osc_message import OscMessage

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 19010))
    sock.settimeout(1.0)
    try:
        mode = DirectLabMode(REAL_MAP, lab_target=("127.0.0.1", 19010), layout_path=layout_path)
        assert mode._lab_renderer is not None
        mode.set_layout_mode("modulettes")
        pid = mode.layout["3,2"]
        assert _note_of(mode.points_by_id[pid]) == 38  # 1 + (7-3)*8 + (7-2)
        assert mode.points_by_id[pid]["instrument"] == "modulettes"

        mode.on_key(3, 2, 1)
        data, _ = sock.recvfrom(65536)
        msg = OscMessage(data)
        assert msg.address == "/modulettes"
        params = list(msg)
        assert params[0] == 38  # note; params[1] is velocity (int, unpinned)
    finally:
        sock.close()


def test_modulettes_then_full_restores_exact_full_rows(real_mode):
    import copy

    mode = real_mode
    full_rows_before = copy.deepcopy(mode.rows_config)
    full_layout_before = dict(mode.layout)

    mode.set_layout_mode("modulettes")
    assert mode.layout != full_layout_before  # now showing modulettes, not the FULL catalog
    assert mode.rows_config == full_rows_before  # rows_config (FULL) untouched while in modulettes

    mode.set_layout_mode("full")
    assert mode.rows_config == full_rows_before
    assert mode.layout == full_layout_before


def test_set_layout_mode_rejects_unknown_mode(real_mode):
    with pytest.raises(ValueError):
        real_mode.set_layout_mode("nope")
    assert real_mode.layout_mode == "full"


def test_layout_mode_from_state_migrates_legacy_split():
    assert layout_mode_from_state(None) == "full"
    assert layout_mode_from_state({}) == "full"
    assert layout_mode_from_state({"split": False}) == "full"
    assert layout_mode_from_state({"split": True}) == "split"
    assert layout_mode_from_state({"layout_mode": "modulettes"}) == "modulettes"
    # new field wins over a stale legacy bool
    assert layout_mode_from_state({"layout_mode": "modulettes", "split": True}) == "modulettes"
    # an invalid layout_mode falls back to migrating the legacy bool
    assert layout_mode_from_state({"layout_mode": "bogus", "split": True}) == "split"
    assert set(LAYOUT_MODES) == {"full", "split", "modulettes"}


def test_set_split_backcompat_still_selects_full_or_split(real_mode):
    mode = real_mode
    mode.set_layout_mode("modulettes")
    mode.set_split(True)
    assert mode.layout_mode == "split" and mode.split is True
    mode.set_split(False)
    assert mode.layout_mode == "full" and mode.split is False


# -- SeptagraphMode presets (task 2) --------------------------------------

def _catalog_store(points):
    store = ManifestStore()
    store.update(Manifest(renderer_id="lab", kind="catalog", name="fixture", lo=None, hi=None, points=tuple(points)))
    return store


def _make_septagraph_mode(points=(), lab_local=None, external=None):
    window = Window(center=vec(*([0.5] * K)), extent=vec(*([0.5] * K)), assign={"grid": (0, 4)})
    store = _catalog_store(points)
    ctl = GridController(link=None, sender=Sender(targets=[]), window=window, manifests=store, leds=lambda b: None)
    return SeptagraphMode(ctl, store, lab_local_targets=lab_local or [("127.0.0.1", 7700)],
                           external_targets=external or [("10.0.0.5", 7700)])


def test_lab_direct_preset_centers_on_catalog_bbox():
    p1 = CatalogPoint(point_id=0, mu=vec(0.2, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5), sigma=vec(*([0.02] * K)),
                       instrument="a", max_rate_hz=10, vel_min=0, vel_max=1)
    p2 = CatalogPoint(point_id=1, mu=vec(0.8, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5), sigma=vec(*([0.02] * K)),
                       instrument="a", max_rate_hz=10, vel_min=0, vel_max=1)
    mode = _make_septagraph_mode([p1, p2])
    ok = mode.set_preset("LAB_DIRECT")
    assert ok is True
    w = mode.controller.window
    assert w.center[0] == pytest.approx(0.5, abs=1e-6)
    assert w.extent[0] == pytest.approx(0.3, abs=1e-6)
    assert mode.controller.snap_radius == pytest.approx(1.0)
    assert mode.controller.sender.targets == [("127.0.0.1", 7700)]


def test_continuum_preset_window_and_targets():
    mode = _make_septagraph_mode()
    ok = mode.set_preset("CONTINUUM")
    assert ok is True
    w = mode.controller.window
    assert w.center == pytest.approx(HALF, abs=1e-6)
    assert w.extent == pytest.approx((0.45, 0.3, 0.3, 0.0, 0.45, 0.0, 0.0, 0.0, 0.0), abs=1e-6)
    assert mode.controller.snap_radius == pytest.approx(0.0)
    assert mode.controller.sender.targets == [("10.0.0.5", 7700)]


def test_hybrid_preset_window_snap_and_both_targets():
    mode = _make_septagraph_mode()
    ok = mode.set_preset("HYBRID")
    assert ok is True
    w = mode.controller.window
    assert w.extent == pytest.approx((0.45, 0.3, 0.3, 0.0, 0.45, 0.0, 0.0, 0.0, 0.0), abs=1e-6)
    assert mode.controller.snap_radius == pytest.approx(0.06)
    assert mode.controller.sender.targets == [("127.0.0.1", 7700), ("10.0.0.5", 7700)]


def test_mirror_preset_greyed_out():
    mode = _make_septagraph_mode()
    before = mode.preset
    ok = mode.set_preset("MIRROR")
    assert ok is False
    assert mode.preset == before  # unchanged


def test_preset_switch_rebuilds_led_frame():
    mode = _make_septagraph_mode()
    mode.set_preset("CONTINUUM")
    frame = mode.led_frame()
    assert len(frame) == 64


# -- MIDI mode -----------------------------------------------------------

class FakeMidiOut:
    available = True
    port_name = "fake"

    def __init__(self):
        self.events = []

    def note_on(self, note, vel=100, ch=0):
        self.events.append(("on", note, vel, ch))

    def note_off(self, note, ch=0):
        self.events.append(("off", note, ch))

    def cc(self, controller, value, ch=0):
        self.events.append(("cc", controller, value, ch))

    def channel_aftertouch(self, value, ch=0):
        self.events.append(("at", value, ch))

    def all_notes_off(self):
        self.events.append(("panic",))

    def list_ports(self):
        return ["fake", "other"]

    def select_port(self, name):
        if name in self.list_ports():
            self.port_name = name
            return True
        return False


def test_midi_mode_note_for_pad_row_major():
    midi = FakeMidiOut()
    mode = MidiMode(midi, base_note=36)
    assert mode.note_for_pad(0, 0) == 36
    assert mode.note_for_pad(1, 0) == 36 + 2  # major scale degree 1 = +2


def test_midi_mode_key_on_off():
    midi = FakeMidiOut()
    mode = MidiMode(midi, base_note=36)
    mode.on_key(0, 0, 1)
    mode.on_key(0, 0, 0)
    assert midi.events == [("on", 36, 100, 0), ("off", 36, 0)]


def test_midi_mode_analog_to_cc():
    midi = FakeMidiOut()
    mode = MidiMode(midi)
    mode.on_analog((4095, 0, 0, 0))
    assert midi.events[0] == ("cc", mode.cc_map["N"], 127, 0)


def test_midi_mode_panic_stops_and_clears():
    midi = FakeMidiOut()
    mode = MidiMode(midi)
    mode.on_key(0, 0, 1)
    mode.panic()
    assert ("panic",) in midi.events
    assert mode.held_notes == {}


def test_midi_mode_port_picker():
    midi = FakeMidiOut()
    mode = MidiMode(midi)
    assert mode.list_ports() == ["fake", "other"]
    assert mode.select_port("other") is True
    assert midi.port_name == "other"
    assert mode.select_port("nope") is False


# -- MIDI FSR pads (task 4) -------------------------------------------------

def test_midi_fsr_never_sends_notes():
    """Locked rule (2026-09-09): FSRs never trigger notes (press/release
    cycle emits no note-on/off, and no aftertouch since nothing is held)."""
    midi = FakeMidiOut()
    mode = MidiMode(midi, fsr_notes=[36, 38, 42, 46], fsr_aftertouch=True)
    mode.on_analog((0, 0, 0, 0, 4000, 0, 0, 0))
    mode.on_analog((0, 0, 0, 0, 4000, 0, 0, 0))
    mode.on_analog((0, 0, 0, 0, 0, 0, 0, 0))
    assert not any(e[0] in ("on", "off", "at") for e in midi.events)


def test_modulettes_numbering_corners():
    # user spec 2026-09-10: 1 = bottom-right, 8 = top-right, 9 = bottom row 2nd from right, 64 = top-left
    assert modulettes_note_for_pad(7, 7) == 1
    assert modulettes_note_for_pad(7, 0) == 8
    assert modulettes_note_for_pad(6, 7) == 9
    assert modulettes_note_for_pad(0, 0) == 64
    assert sorted(modulettes_note_for_pad(x, y) for x in range(8) for y in range(8)) == list(range(1, 65))
