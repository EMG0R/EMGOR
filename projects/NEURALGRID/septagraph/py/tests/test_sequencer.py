"""Tests for septagraph.pi.sequencer.Sequencer.

Drives Sequencer.tick(now) directly with synthetic timestamps -- no real
thread/sleep needed, so this is fast and deterministic. The background
thread (Sequencer.start/stop) is exercised once, lightly, at the end.
"""
from __future__ import annotations

import time

import pytest

from septagraph.pi.sequencer import Sequencer, LEVEL_VELOCITY


def test_default_shape():
    seq = Sequencer()
    assert seq.n_tracks == 4
    assert seq.n_steps == 8
    assert len(seq.pattern) == 4
    assert all(len(row) == 8 for row in seq.pattern)
    assert seq.tempo_bpm == 120
    assert seq.playing is False


def test_toggle_and_cycle_step():
    seq = Sequencer()
    assert seq.pattern[0][0] == 0
    seq.toggle_step(0, 0)
    assert seq.pattern[0][0] == 1
    seq.toggle_step(0, 0)
    assert seq.pattern[0][0] == 0

    seq.cycle_step_velocity(1, 2)
    assert seq.pattern[1][2] == 1
    seq.cycle_step_velocity(1, 2)
    assert seq.pattern[1][2] == 2
    seq.cycle_step_velocity(1, 2)
    assert seq.pattern[1][2] == 3
    seq.cycle_step_velocity(1, 2)
    assert seq.pattern[1][2] == 0


def test_play_advances_steps_in_order_and_fires_armed_steps():
    hits = []
    playheads = []
    seq = Sequencer(
        n_tracks=2, n_steps=4, tempo_bpm=120, swing_pct=0, steps_per_beat=1,
        on_step=lambda tr, st, vel: hits.append((tr, st, round(vel, 3))),
        on_playhead=lambda st: playheads.append(st),
    )
    seq.toggle_step(0, 0)
    seq.toggle_step(1, 2)
    # step_duration at 120bpm, steps_per_beat=1 -> 60/120 = 0.5s per step
    seq.play(now=0.0)
    # step 0 fires immediately at play()
    assert playheads == [0]
    assert hits == [(0, 0, LEVEL_VELOCITY[1])]

    seq.tick(now=0.5)  # step 1: nothing armed
    assert playheads == [0, 1]

    seq.tick(now=1.0)  # step 2: track 1 armed
    assert playheads == [0, 1, 2]
    assert hits[-1] == (1, 2, LEVEL_VELOCITY[1])

    seq.tick(now=1.5)  # step 3
    seq.tick(now=2.0)  # wraps to step 0 again -> re-fires track0 step0
    assert playheads == [0, 1, 2, 3, 0]
    assert hits[-1] == (0, 0, LEVEL_VELOCITY[1])


def test_stop_halts_playhead():
    playheads = []
    seq = Sequencer(n_tracks=1, n_steps=4, tempo_bpm=120, on_playhead=lambda st: playheads.append(st))
    seq.play(now=0.0)
    seq.tick(now=0.5)
    seq.stop_transport()
    seq.tick(now=1.0)
    seq.tick(now=1.5)
    assert playheads == [0, 1]  # no further advance once stopped
    assert seq.playing is False


def test_swing_delays_odd_steps():
    playheads = []
    times = []
    seq = Sequencer(
        n_tracks=1, n_steps=4, tempo_bpm=120, swing_pct=50, steps_per_beat=1,
        on_playhead=lambda st: (playheads.append(st), times.append(seq._next_time)),
    )
    seq.play(now=0.0)  # fires step 0 at t=0, schedules step1 at 0.5 (even->odd uses swing)
    # with 50% swing, the gap before an odd step lengthens by 50% of a step
    assert seq._next_time == pytest.approx(0.75)


def test_max_bpm_and_min_bpm_clamped():
    seq = Sequencer(tempo_bpm=1000)
    assert seq.tempo_bpm == 240
    seq.tempo_bpm = 1
    assert seq.tempo_bpm == 40


def test_resize_preserves_existing_cells():
    seq = Sequencer(n_tracks=4, n_steps=8)
    seq.toggle_step(0, 0)
    seq.resize(n_tracks=8, n_steps=8)
    assert seq.n_tracks == 8
    assert seq.pattern[0][0] == 1
    assert len(seq.pattern) == 8


def test_pattern_json_roundtrip():
    seq = Sequencer(n_tracks=2, n_steps=4)
    seq.toggle_step(0, 1)
    seq.cycle_step_velocity(1, 3)
    data = seq.to_json()
    seq2 = Sequencer(n_tracks=2, n_steps=4)
    seq2.from_json(data)
    assert seq2.pattern == seq.pattern
    assert seq2.tempo_bpm == seq.tempo_bpm


def test_rate_limit_skip_counter_via_wrapper():
    # The sequencer itself doesn't know about catalog rate limits; modes.py
    # wraps on_step to enforce them. Verify the hook composition works by
    # emulating what DirectLabMode does.
    skips = []
    fires = []
    last_fire = {}

    def make_gated(min_interval):
        def gated(tr, st, vel, now):
            key = tr
            t = last_fire.get(key)
            if t is not None and (now - t) < min_interval:
                skips.append((tr, st))
                return
            last_fire[key] = now
            fires.append((tr, st))
        return gated

    gated = make_gated(1.0)
    seq = Sequencer(n_tracks=1, n_steps=2, tempo_bpm=120, steps_per_beat=1)
    seq.toggle_step(0, 0)
    seq.toggle_step(0, 1)
    seq.on_step = lambda tr, st, vel: gated(tr, st, vel, seq._now)
    seq._now = 0.0
    seq.play(now=0.0)
    seq._now = 0.5
    seq.tick(now=0.5)
    assert fires == [(0, 0)]
    assert skips == [(0, 1)]


def test_background_thread_runs_and_can_stop():
    hits = []
    seq = Sequencer(n_tracks=1, n_steps=2, tempo_bpm=240, on_step=lambda tr, st, vel: hits.append(st))
    seq.toggle_step(0, 0)
    seq.start()
    seq.play()
    time.sleep(0.3)
    seq.stop_transport()
    seq.stop()
    assert len(hits) >= 1
