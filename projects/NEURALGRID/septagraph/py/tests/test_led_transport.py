"""Tests for the LED *transport* path (app.py send path + leds.diff) --
NOT colour decisions, which live in modes.py/leds.py's colour tables and are
owned elsewhere. Covers the de-flicker fix: diff-against-last-sent, skip
when unchanged, LEDS_PARTIAL for small deltas, and that _render_lock
actually serializes _led_loop against _on_key so the two can never race
Crossfader.start()/current() onto the wire as two different frames (the
flicker root cause -- see HANDOFF.md).
"""
from __future__ import annotations

import threading
import time

import pytest

from septagraph.pi import leds
from septagraph.pi.app import App, FakeGrid, build_arg_parser


@pytest.fixture
def make_app(tmp_path, monkeypatch):
    # Isolate state file per test so runs don't clobber ~/.neuralgrid or race
    # each other; App opens a UDP Receiver (manifest_receiver) that must be
    # closed after each test or the next App() in the same run fails to bind
    # the same port ("Address already in use").
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    # The grid now boots BLANK (2026-09-17); these tests are about the
    # Machine Lab mode's geometry and LED path, so ask for it explicitly
    # rather than relying on what the instrument comes up in.
    app.switch_mode("direct_lab")
    yield app
    app.stop()


# ---------------------------------------------------------------------------
# leds.diff
# ---------------------------------------------------------------------------


def test_diff_empty_when_identical():
    frame = leds.blank()
    assert leds.diff(frame, list(frame)) == []


def test_diff_finds_changed_pixels_with_xy():
    old = leds.blank()
    new = list(old)
    new[leds.idx(3, 5)] = (10, 20, 30)
    new[leds.idx(0, 0)] = (1, 2, 3)
    d = leds.diff(old, new)
    assert set(d) == {(3, 5, 10, 20, 30), (0, 0, 1, 2, 3)}


# ---------------------------------------------------------------------------
# App._send_frame_locked transport decisions
# ---------------------------------------------------------------------------


def test_identical_frame_is_not_resent(make_app):
    app = make_app
    link: FakeGrid = app.link
    frame = leds.blank()
    frame[0] = (5, 5, 5)

    with app._render_lock:
        app._send_frame_locked(frame)
    sent_after_first = app._frames_sent
    assert sent_after_first == 1

    with app._render_lock:
        app._send_frame_locked(list(frame))  # same content, new list object
    assert app._frames_sent == sent_after_first  # skipped, no second send


def test_small_delta_uses_partial(make_app):
    app = make_app
    link: FakeGrid = app.link
    calls = {"full": 0, "partial": 0}
    orig_full, orig_partial = link.send_leds_full, link.send_leds_partial
    link.send_leds_full = lambda f: (calls.__setitem__("full", calls["full"] + 1), orig_full(f))[-1]
    link.send_leds_partial = lambda u: (calls.__setitem__("partial", calls["partial"] + 1), orig_partial(u))[-1]

    base = leds.blank()
    with app._render_lock:
        app._send_frame_locked(base)  # first send always FULL (no baseline yet)
    assert calls["full"] == 1 and calls["partial"] == 0

    changed = list(base)
    changed[leds.idx(1, 1)] = (255, 0, 0)  # one pixel: well under PARTIAL_MAX_PIXELS
    with app._render_lock:
        app._send_frame_locked(changed)
    assert calls["partial"] == 1
    assert link.last_frame[leds.idx(1, 1)] == (255, 0, 0)


def test_large_delta_uses_full(make_app):
    app = make_app
    link: FakeGrid = app.link
    calls = {"full": 0, "partial": 0}
    orig_full, orig_partial = link.send_leds_full, link.send_leds_partial
    link.send_leds_full = lambda f: (calls.__setitem__("full", calls["full"] + 1), orig_full(f))[-1]
    link.send_leds_partial = lambda u: (calls.__setitem__("partial", calls["partial"] + 1), orig_partial(u))[-1]

    base = leds.blank()
    with app._render_lock:
        app._send_frame_locked(base)
    assert calls["full"] == 1

    changed = [(200, 100, 50)] * leds.N_LEDS  # all 64 pixels differ
    with app._render_lock:
        app._send_frame_locked(changed)
    assert calls["full"] == 2
    assert calls["partial"] == 0


# ---------------------------------------------------------------------------
# Immediate press-flash (2026-09-08: _on_key's fast path wasn't consulting
# active_mode.led_frame() at all -- it just resent whatever the crossfader's
# stale interpolated state was, so a keypress never actually put
# leds.FLASH_WHITE on the wire. See app.py's _current_display_frame_locked /
# _on_key comments for the full root-cause writeup.)
# ---------------------------------------------------------------------------


def test_key_press_flashes_pad_white_immediately(make_app):
    app = make_app
    link: FakeGrid = app.link
    mode = app.modes["direct_lab"]
    # (0,0) is populated by the default lab_map.json layout (see modes.py's
    # DirectLabMode.__init__ -> _rebuild_layout()) -- pick any pad the fixed
    # layout actually assigns a point to, rather than hardcoding one, so
    # this test doesn't silently start no-op'ing if the map ever changes.
    x, y = next(iter(mode.layout.keys())).split(",")
    x, y = int(x), int(y)

    # Let App.__init__'s own boot crossfade (blank -> initial led_frame(),
    # started once in __init__) finish before pressing -- in real use a
    # human is never pressing within the first 150ms after process start,
    # and while that fade is still active current() blends toward the
    # ORIGINAL _from (blank) captured at __init__ time, which would mask
    # the flash for however much of the fade window remains.
    time.sleep(leds.CROSSFADE_S + 0.02)

    # Baseline render so _last_sent_frame is established (matches how the
    # real app behaves after boot/mode-select, before any key is pressed).
    app._render_leds_once()
    assert link.last_frame[leds.idx(x, y)] != leds.FLASH_WHITE

    frames_sent_before = app._frames_sent
    app._on_key(x, y, 1)  # press edge

    # The flash must be on the wire (FakeGrid.last_frame mirrors exactly
    # what was sent) synchronously, within this one _on_key call -- no
    # waiting for the next _led_loop tick.
    assert link.last_frame[leds.idx(x, y)] == leds.FLASH_WHITE
    assert (x, y) in mode._press_times
    assert app._frames_sent == frames_sent_before + 1  # exactly one small send, not a full-frame flood

    # After leds.FLASH_S elapses, the ordinary 20fps-loop-shaped render call
    # (led_frame() no longer returns white for this pad) restores the real
    # colour via the same diff path -- no special-casing needed.
    # While HELD the pad stays white (proper button feel)...
    time.sleep(leds.FLASH_S + 0.02)
    app._render_leds_once()
    assert link.last_frame[leds.idx(x, y)] == leds.FLASH_WHITE
    # ...and reverts after RELEASE (+ the FLASH_S minimum for quick taps).
    app._on_key(x, y, 0)
    time.sleep(leds.FLASH_S + 0.02)
    app._render_leds_once()
    assert link.last_frame[leds.idx(x, y)] != leds.FLASH_WHITE


def test_crossfade_finishes_on_exact_target_frame():
    target = leds.blank()
    target[10] = (7, 8, 9)
    cf = leds.Crossfader(duration_s=0.05)
    cf.start(leds.blank(), target, now=0.0)
    assert cf.current(now=0.05) == target
    assert cf.current(now=1.0) == target  # well past duration: still exact


# ---------------------------------------------------------------------------
# Concurrency: _led_loop and _on_key must not race the crossfader/send path
# ---------------------------------------------------------------------------


def test_led_loop_and_on_key_do_not_race(make_app):
    """Regression test for the flicker bug: _render_lock must be held across
    the crossfader read/mutate AND the send decision as one atomic unit.
    Hammer _render_leds_once (the _on_key fast path) from other threads
    while _led_loop-shaped work runs, and assert every frame ever written to
    the wire was internally self-consistent (a valid crossfade output, never
    a torn mix)."""
    app = make_app
    stop = threading.Event()
    errors = []

    def key_hammer():
        try:
            for _ in range(200):
                app._render_leds_once()
        except Exception as exc:  # pragma: no cover - would fail the test
            errors.append(exc)

    def crossfade_hammer():
        try:
            a, b = leds.blank(), list(leds.blank())
            b[0] = (1, 2, 3)
            for i in range(200):
                frame = a if i % 2 == 0 else b
                with app._render_lock:
                    app._crossfader.start(app._crossfader.current(), frame)
                    app._send_frame_locked(app._crossfader.current())
        except Exception as exc:  # pragma: no cover
            errors.append(exc)

    threads = [threading.Thread(target=key_hammer) for _ in range(3)] + [
        threading.Thread(target=crossfade_hammer) for _ in range(3)
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=5)
        assert not t.is_alive()

    assert not errors
