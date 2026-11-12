"""Tests for the Machine Lab renderer (plan Task 3.2).

Uses a small hand-rolled fixture map (never the real
``renderers/lab_map.json``, which is owned by a separate task) and a fake
UDP "lab" receiver on loopback in place of the real Mac mini.
"""
from __future__ import annotations

import copy
import json
import random
import socket
import time

import pytest
from pythonosc.osc_message import OscMessage

from pythonosc.osc_message_builder import OscMessageBuilder

from septagraph.codec import Decoder
from septagraph.net import Sender
from septagraph.renderers.lab import LabRenderer, lab_osc, load_lab_map
from septagraph.types import Hit, ManifestRequest, Move, ZERO, vec
from septagraph.axes import K

BREAK_KICK_MU = vec(0.30, 0.7, 0.15, 0.02, 0.6, 0.35, 0.05, 0.4, 0.5)
BREAK_SNARE_MU = vec(0.32, 0.7, 0.15, 0.02, 0.6, 0.35, 0.05, 0.4, 0.5)  # close to kick (idx0 differs by 0.02)

FIXTURE = {
    "renderer_id": "lab",
    "name": "CalArts Machine Lab (fixture)",
    "kind": "catalog",
    "target": "127.0.0.1:9",
    "points": [
        {
            "point_id": 0, "instrument": "breakBot", "element": "kick",
            "osc": {"address": "/breakBot", "args": ["i:note=36", "i:vel"]},
            "vel_range": [1, 127],
            "mu": list(BREAK_KICK_MU), "sigma": [0.0] * K,
            "max_rate_hz": 12, "vel_min": 0.15, "vel_max": 1.0,
            "source": "estimated", "enabled": True,
        },
        {
            "point_id": 1, "instrument": "breakBot", "element": "snare",
            "osc": {"address": "/breakBot", "args": ["i:note=38", "i:vel"]},
            "vel_range": [1, 127],
            "mu": list(BREAK_SNARE_MU), "sigma": [0.0] * K,
            "max_rate_hz": 12, "vel_min": 0.15, "vel_max": 1.0,
            "source": "estimated", "enabled": True,
        },
        {
            "point_id": 2, "instrument": "tammy", "element": "hit",
            "osc": {"address": "/tammy", "args": ["f:vel"]},
            "vel_range": [0, 1],
            "mu": [0.8, 0.5, 0.9, 0.02, 0.3, 0.5, 0.0, 0.2, 0.5], "sigma": [0.0] * K,
            "max_rate_hz": 8, "vel_min": 0.0, "vel_max": 1.0,
            "source": "estimated", "enabled": True,
        },
        {
            # Disabled: must never be chosen, struck, or announced, even
            # though its mu coincides exactly with a hit used in tests.
            "point_id": 3, "instrument": "rattleTron", "element": "hit",
            "osc": {"address": "/rattleTron", "args": ["i:note=40", "i:vel"]},
            "vel_range": [1, 127],
            "mu": [0.1, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5], "sigma": [0.0] * K,
            "max_rate_hz": 4, "vel_min": 0.0, "vel_max": 1.0,
            "source": "estimated", "enabled": False,
        },
    ],
}


def _write_map(tmp_path, points=None):
    data = copy.deepcopy(FIXTURE)
    if points is not None:
        data["points"] = points
    path = tmp_path / "lab_map_fixture.json"
    path.write_text(json.dumps(data))
    return path


class _FakeLab:
    """Loopback UDP receiver standing in for the Machine Lab's :8001 listener."""

    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))

    @property
    def address(self):
        return self.sock.getsockname()

    def recv_one(self, timeout=1.0) -> OscMessage:
        self.sock.settimeout(timeout)
        data, _ = self.sock.recvfrom(65536)
        return OscMessage(data)

    def expect_silence(self, timeout=0.2):
        self.sock.settimeout(timeout)
        with pytest.raises(socket.timeout):
            self.sock.recvfrom(65536)

    def close(self):
        self.sock.close()


@pytest.fixture()
def fake_lab():
    lab = _FakeLab()
    yield lab
    lab.close()


def _renderer(map_path, lab, rng=random.Random, ctl_targets=None, vel_mode="hit", vel_random=(70, 110)):
    # vel_mode defaults to "hit" here (not LabRenderer's own "random" default)
    # so every pre-existing test in this file -- which asserts exact scaled
    # velocity numbers -- keeps working unchanged; tests that want the real
    # default/random behaviour pass vel_mode="random" (or construct
    # LabRenderer directly) explicitly.
    sender = Sender([lab.address])
    return LabRenderer(map_path, sender, ctl_targets or [("127.0.0.1", 1)], rng=rng, vel_mode=vel_mode, vel_random=vel_random)


# --- lab_osc -----------------------------------------------------------

def test_lab_osc_exact_message_for_sigma_zero(tmp_path):
    point = FIXTURE["points"][0]
    hit = Hit("v:1", 0, True, 1.0, BREAK_KICK_MU, ZERO)
    msg = OscMessage(lab_osc(point, hit))
    assert msg.address == "/breakBot"
    # note const 36; velocity 1.0 -> scaled into [0.15,1.0] -> mapped into [1,127] -> 127
    assert msg.params == [36, 127]


def test_lab_osc_velocity_zero_uses_vel_min_floor():
    point = FIXTURE["points"][0]
    hit = Hit("v:1", 0, True, 0.0, BREAK_KICK_MU, ZERO)
    msg = OscMessage(lab_osc(point, hit))
    # scaled = 0.15 -> 1 + 0.15*126 = 19.9 -> round 20
    assert msg.params == [36, 20]


def test_lab_osc_float_vel_token():
    point = FIXTURE["points"][2]
    hit = Hit("v:1", 0, True, 0.5, vec(*point["mu"]), ZERO)
    msg = OscMessage(lab_osc(point, hit))
    assert msg.address == "/tammy"
    assert msg.params == pytest.approx([0.5], abs=1e-5)


# --- load_lab_map --------------------------------------------------------

def test_load_lab_map_excludes_disabled_points(tmp_path):
    map_path = _write_map(tmp_path)
    manifest, raw = load_lab_map(map_path)
    assert manifest.kind == "catalog"
    assert {p.point_id for p in manifest.points} == {0, 1, 2}
    assert 3 in raw  # raw dict keeps the disabled point for lookup, just excluded from selection
    assert raw[3]["instrument"] == "rattleTron"


# --- LabRenderer.on_message / choose -------------------------------------

def test_exact_hit_sigma_zero_sends_exact_lab_message(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    hit = Hit("v:1", 0, True, 1.0, BREAK_KICK_MU, ZERO)
    renderer.on_message(hit)
    msg = fake_lab.recv_one()
    assert msg.address == "/breakBot"
    assert msg.params == [36, 127]


def test_sigma_choice_selects_either_near_point_over_many_trials(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    chosen_ids = set()
    for seed in range(50):
        renderer = _renderer(map_path, fake_lab, rng=random.Random(seed))
        hit = Hit("v:1", 0, True, 0.5, BREAK_KICK_MU, vec(*[0.1] * K))
        point = renderer.choose(hit)
        assert point is not None
        chosen_ids.add(point.point_id)
    assert chosen_ids == {0, 1}


def test_rate_limit_drops_second_hit_20ms_later(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    hit = Hit("v:1", 0, True, 1.0, BREAK_KICK_MU, ZERO)  # max_rate_hz=12 -> ~83ms min interval
    renderer.on_message(hit)
    first = fake_lab.recv_one()
    assert first.params == [36, 127]

    time.sleep(0.02)  # 20 ms later, well under 1/12 s
    renderer.on_message(hit)
    fake_lab.expect_silence(timeout=0.2)  # second hit was rate-limited, nothing sent


def test_release_onset_zero_sends_nothing(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    hit = Hit("v:1", 0, False, 1.0, BREAK_KICK_MU, ZERO)
    renderer.on_message(hit)
    fake_lab.expect_silence()


def test_move_is_ignored(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    renderer.on_message(Move("v:1", BREAK_KICK_MU))
    fake_lab.expect_silence()


def test_disabled_point_never_chosen(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    # sigma=1 on every axis: within_sigma covers every point, including the
    # disabled one whose mu is identical to this hit's mu.
    hit = Hit("v:1", 0, True, 0.5, vec(0.1, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5), vec(*[1.0] * K))
    chosen_ids = set()
    for seed in range(20):
        renderer = _renderer(map_path, fake_lab, rng=random.Random(seed))
        point = renderer.choose(hit)
        assert point is not None
        chosen_ids.add(point.point_id)
    assert 3 not in chosen_ids


def test_nearest_used_when_no_candidate_within_sigma(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    # far from everything, sigma=0 -> no within_sigma candidates -> nearest
    hit = Hit("v:1", 0, True, 1.0, vec(*[0.9] * K), ZERO)
    point = renderer.choose(hit)
    assert point is not None
    assert point.point_id == 2  # tammy's mu is the closest of the enabled points


# --- manifest / announce --------------------------------------------------

def test_manifest_request_yields_complete_catalog_on_ctl_port(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    ctl_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ctl_sock.bind(("127.0.0.1", 0))
    ctl_sock.settimeout(1.0)
    ctl_addr = ctl_sock.getsockname()

    renderer = _renderer(map_path, fake_lab, ctl_targets=[ctl_addr])
    renderer.on_message(ManifestRequest())

    data, _ = ctl_sock.recvfrom(65536)
    (manifest,) = Decoder().feed(data)
    assert manifest.kind == "catalog"
    assert manifest.renderer_id == "lab"
    assert {p.point_id for p in manifest.points} == {0, 1, 2}  # disabled point 3 excluded
    ctl_sock.close()


def test_announce_excludes_disabled_points(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    ctl_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ctl_sock.bind(("127.0.0.1", 0))
    ctl_sock.settimeout(1.0)
    ctl_addr = ctl_sock.getsockname()

    renderer = _renderer(map_path, fake_lab, ctl_targets=[ctl_addr])
    renderer.announce()

    data, _ = ctl_sock.recvfrom(65536)
    (manifest,) = Decoder().feed(data)
    instruments = {p.instrument for p in manifest.points}
    assert instruments == {"breakBot", "tammy"}
    assert "rattleTron" not in instruments
    ctl_sock.close()


# --- direct_strike ---------------------------------------------------------

def test_direct_strike_sends_and_rate_limits(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)

    assert renderer.direct_strike(0, 1.0) is True
    msg = fake_lab.recv_one()
    assert msg.address == "/breakBot" and msg.params == [36, 127]

    assert renderer.direct_strike(0, 1.0) is False  # rate-limited
    fake_lab.expect_silence(timeout=0.2)


def test_direct_strike_rejects_disabled_and_unknown_points(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    assert renderer.direct_strike(3, 1.0) is False  # disabled
    assert renderer.direct_strike(99, 1.0) is False  # unknown
    fake_lab.expect_silence(timeout=0.2)


# --- vel_mode="random" ------------------------------------------------------

def test_lab_renderer_default_vel_mode_is_random(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    sender = Sender([fake_lab.address])
    renderer = LabRenderer(map_path, sender, [("127.0.0.1", 1)])
    assert renderer.vel_mode == "random"
    assert renderer.vel_random == (70, 110)


def test_lab_osc_random_vel_mode_ignores_hit_velocity_and_vel_range():
    # point 0's vel_range is [1,127] and vel_min/vel_max floor to [0.15,1.0];
    # random mode must ignore all of that and only use vel_random.
    point = FIXTURE["points"][0]
    rng = random.Random(7)
    for hit_velocity in (0.0, 0.5, 1.0):
        hit = Hit("v:1", 0, True, hit_velocity, BREAK_KICK_MU, ZERO)
        msg = OscMessage(lab_osc(point, hit, vel_mode="random", vel_random=(70, 110), rng=rng))
        note, vel = msg.params
        assert note == 36
        assert 70 <= vel <= 110


def test_on_message_random_vel_mode_all_strikes_in_range_and_vary(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    hit = Hit("v:1", 0, True, 1.0, BREAK_KICK_MU, ZERO)
    velocities = set()
    for seed in range(50):
        renderer = _renderer(map_path, fake_lab, rng=random.Random(seed), vel_mode="random")
        renderer.on_message(hit)
        msg = fake_lab.recv_one()
        note, vel = msg.params
        assert note == 36
        assert 70 <= vel <= 110
        velocities.add(vel)
    assert len(velocities) > 1


def test_direct_strike_random_vel_mode_all_strikes_in_range_and_vary(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    velocities = set()
    for seed in range(50):
        renderer = _renderer(map_path, fake_lab, rng=random.Random(seed), vel_mode="random")
        assert renderer.direct_strike(0, 1.0) is True
        msg = fake_lab.recv_one()
        note, vel = msg.params
        assert note == 36
        assert 70 <= vel <= 110
        velocities.add(vel)
    assert len(velocities) > 1


def test_vel_mode_hit_restores_old_scaled_behaviour(tmp_path, fake_lab):
    # explicit vel_mode="hit" reproduces the pre-random-mode exact numbers.
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab, vel_mode="hit")
    hit = Hit("v:1", 0, True, 1.0, BREAK_KICK_MU, ZERO)
    renderer.on_message(hit)
    msg = fake_lab.recv_one()
    assert msg.params == [36, 127]


# --- liveness (GOR python-server contract: /ping -> /pong gor_server <uptime>) --

def _pong_bytes(uptime: int) -> bytes:
    builder = OscMessageBuilder(address="/pong")
    builder.add_arg("gor_server", arg_type="s")
    builder.add_arg(int(uptime), arg_type="i")
    return builder.build().dgram


def test_lab_reachable_false_before_any_pong(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    assert renderer.lab_reachable is False
    assert renderer.lab_uptime is None
    assert renderer.last_pong_age_s is None


def test_handle_pong_sets_reachable_and_uptime(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    renderer._handle_pong(_pong_bytes(4242))
    assert renderer.lab_reachable is True
    assert renderer.lab_uptime == 4242
    assert renderer.last_pong_age_s is not None
    assert renderer.last_pong_age_s < 1.0


def test_handle_pong_ignores_wrong_address_and_malformed_args(tmp_path, fake_lab):
    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)

    other = OscMessageBuilder(address="/notpong")
    other.add_arg("gor_server", arg_type="s")
    other.add_arg(1, arg_type="i")
    renderer._handle_pong(other.build().dgram)
    assert renderer.lab_reachable is False

    wrong_tag = OscMessageBuilder(address="/pong")
    wrong_tag.add_arg("not_gor_server", arg_type="s")
    wrong_tag.add_arg(1, arg_type="i")
    renderer._handle_pong(wrong_tag.build().dgram)
    assert renderer.lab_reachable is False

    renderer._handle_pong(b"not even osc")
    assert renderer.lab_reachable is False


def test_lab_reachable_becomes_false_after_timeout(tmp_path, fake_lab, monkeypatch):
    from septagraph.renderers import lab as lab_module

    map_path = _write_map(tmp_path)
    renderer = _renderer(map_path, fake_lab)
    renderer._handle_pong(_pong_bytes(1))
    assert renderer.lab_reachable is True

    fake_now = [renderer._clock()]
    monkeypatch.setattr(renderer, "_clock", lambda: fake_now[0])
    fake_now[0] += lab_module.PONG_TIMEOUT_S + 0.1
    assert renderer.lab_reachable is False
    assert renderer.last_pong_age_s >= lab_module.PONG_TIMEOUT_S


def test_start_liveness_noop_without_try_recv(tmp_path, fake_lab):
    # _DryRunSender-shaped sender has no try_recv -- start_liveness must not
    # spin up a thread (and therefore never touches a real socket).
    map_path = _write_map(tmp_path)

    class _NoRecvSender:
        def __init__(self, target):
            self.target = target

        def send(self, packet: bytes) -> None:
            pass

    renderer = LabRenderer(map_path, _NoRecvSender(fake_lab.address), [("127.0.0.1", 1)])
    renderer.start_liveness()
    assert renderer._liveness_thread is None
    renderer.stop_liveness()  # must not raise even though nothing was started


def test_start_liveness_full_round_trip(tmp_path, monkeypatch):
    from septagraph.renderers import lab as lab_module

    monkeypatch.setattr(lab_module, "PING_INTERVAL_S", 0.02)
    monkeypatch.setattr(lab_module, "_LIVENESS_POLL_S", 0.005)

    lab_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    lab_sock.bind(("127.0.0.1", 0))
    lab_sock.settimeout(1.0)
    lab_addr = lab_sock.getsockname()

    map_path = _write_map(tmp_path)
    sender = Sender([lab_addr])
    renderer = LabRenderer(map_path, sender, [("127.0.0.1", 1)])
    renderer.start_liveness()
    try:
        data, sender_addr = lab_sock.recvfrom(65536)
        msg = OscMessage(data)
        assert msg.address == "/ping"

        lab_sock.sendto(_pong_bytes(777), sender_addr)

        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline and not renderer.lab_reachable:
            time.sleep(0.005)
        assert renderer.lab_reachable is True
        assert renderer.lab_uptime == 777
    finally:
        renderer.stop_liveness()
        lab_sock.close()


# --- real renderers/lab_map.json (GOR python-server contract) --------------

import math
from pathlib import Path

REAL_MAP = Path(__file__).resolve().parents[1] / "renderers" / "lab_map.json"

MARIMBA_SCL = {
    45, 47, 48, 50, 52, 53, 54, 55, 57, 59, 60, 62, 64, 65, 66, 67, 69, 71, 72, 74,
    76, 77, 78, 79, 81, 83, 84, 86, 88, 89, 90, 91, 93, 95, 96, 97,
}

EXPECTED_MAX_RATE_HZ = {
    "breakBot": 16.7,
    "ganaPati": 7.1,
    "tammy": 20,
    "modulettes": 20,
    "rattleTron": 20,
    "marimbot": 20,
    "lydia": 20,
}

EXPECTED_VEL_RANGE = {
    "breakBot": [65, 127],
    "ganaPati": [65, 127],
    "tammy": [1, 127],
    "modulettes": [0, 127],
    "rattleTron": [0, 127],
    "marimbot": [0, 127],
}


def _real_raw_points() -> list[dict]:
    with open(REAL_MAP) as f:
        return json.load(f)["points"]


def _note_of_raw(p: dict) -> int:
    for tok in p["osc"]["args"]:
        if tok.startswith("i:note="):
            return int(tok.split("=", 1)[1])
    raise AssertionError(f"no note token on {p!r}")


def test_real_map_marimbot_enabled_notes_equal_scl():
    points = _real_raw_points()
    enabled_notes = {
        _note_of_raw(p) for p in points
        if p["instrument"] == "marimbot" and p.get("enabled", True)
    }
    assert enabled_notes == MARIMBA_SCL


def test_real_map_marimbot_out_of_scl_notes_disabled_not_deleted():
    points = _real_raw_points()
    marimbot_notes = {_note_of_raw(p): p for p in points if p["instrument"] == "marimbot"}
    for note in (73, 92, 94):
        assert note in marimbot_notes, f"note {note} should still be present, just disabled"
        assert marimbot_notes[note]["enabled"] is False


def test_real_map_broken_beaters_stay_disabled():
    points = _real_raw_points()
    by_instr_note = {(p["instrument"], _note_of_raw(p)): p for p in points}
    # breakBot 0 is the KICK the user confirmed on-site 2026-09-09 (trigger
    # list's 'X' was stale); 7 stays off. Only kick/snare/cymbal are enabled.
    assert by_instr_note[("breakBot", 0)]["enabled"] is True
    assert by_instr_note[("breakBot", 7)]["enabled"] is False
    enabled_bb = sorted(n for (i, n), p in by_instr_note.items() if i == "breakBot" and p["enabled"])
    assert enabled_bb == [0, 5, 11]
    assert by_instr_note[("breakBot", 11)]["note_cycle"] == [11, 12]
    # ganaPati notes 0/4/5 have no corresponding point at all (never wired
    # by the mini's own route/comment survey) -- nothing to assert there.
    assert ("ganaPati", 0) not in by_instr_note
    assert ("ganaPati", 4) not in by_instr_note
    assert ("ganaPati", 5) not in by_instr_note


def test_real_map_max_rate_hz_matches_server_floor_per_instrument():
    points = _real_raw_points()
    for p in points:
        instr = p["instrument"]
        assert p["max_rate_hz"] == EXPECTED_MAX_RATE_HZ[instr], (instr, p["point_id"])


def test_real_map_vel_range_matches_contract_clamp():
    points = _real_raw_points()
    for p in points:
        instr = p["instrument"]
        if instr in EXPECTED_VEL_RANGE:
            assert p["vel_range"] == EXPECTED_VEL_RANGE[instr], (instr, p["point_id"])


def test_real_map_note_ranges_match_contract():
    points = _real_raw_points()
    by_instr_notes: dict[str, set[int]] = {}
    for p in points:
        by_instr_notes.setdefault(p["instrument"], set()).add(_note_of_raw(p))
    assert by_instr_notes["breakBot"] == set(range(0, 13))
    assert by_instr_notes["ganaPati"] >= {1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14}
    assert by_instr_notes["tammy"] == set(range(0, 26))
    assert by_instr_notes["lydia"] == set(range(60, 80))


def test_real_map_all_instruments_enabled_no_server_switch():
    # GOR python-server contract (2026-09-08): the grid enables every
    # instrument the lab can route (subject only to each point's own
    # "enabled") -- there is no "servers" gate or chuck/python mode switch.
    manifest, raw = load_lab_map(REAL_MAP)
    d = json.loads(REAL_MAP.read_text())
    assert "servers" not in d
    enabled_instruments = {p.instrument for p in manifest.points}
    assert enabled_instruments == {
        "breakBot", "ganaPati", "tammy", "lydia", "marimbot", "modulettes", "rattleTron",
    }


def test_real_map_rate_limiter_never_exceeds_server_floor_per_point(tmp_path):
    # LabRenderer._rate_ok must never let two hits reach the same point_id
    # sooner than 1/max_rate_hz -- exercised against ganaPati's real
    # 140ms floor (the tightest one) using the real map + a fake lab.
    lab = _FakeLab()
    try:
        renderer = LabRenderer(REAL_MAP, Sender([lab.address]), [("127.0.0.1", 1)], vel_mode="hit")
        gana_point = next(p for p in renderer.manifest.points if p.instrument == "ganaPati")
        interval = 1.0 / gana_point.max_rate_hz
        hit = Hit("v:1", 0, True, 1.0, gana_point.mu, ZERO)

        assert renderer.direct_strike(gana_point.point_id, 1.0) is True
        lab.recv_one()

        time.sleep(min(interval * 0.5, 0.05))
        assert renderer.direct_strike(gana_point.point_id, 1.0) is False
        lab.expect_silence(timeout=0.1)

        time.sleep(interval + 0.02)
        assert renderer.direct_strike(gana_point.point_id, 1.0) is True
        lab.recv_one()
    finally:
        lab.close()
