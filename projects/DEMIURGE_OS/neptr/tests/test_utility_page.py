"""UTILITY page (menu 19) + POLY DLY (mvd) v5 repeats/interval controls.

Two additions to the touchscreen UI, both matching an already-deployed engine
contract:

  * A custom UTILITY page (menu index 19, no slot grid) hosting three GLOBAL
    controls — master output volume (ch16 CC103 / OSC /master_out), master BPM
    (ch16 CC104 / OSC /master_bpm, with the live /clock_bpm shown prominently),
    and a master RECORD toggle that shells out to `demiurge-record`.

  * POLY DLY (mvd) v5: an 8-chip REPEATS row (ch1 CC110 absolute / OSC
    /mvd_reps), a signed-semitone INTERVAL [-]/[+] stepper (ch1 CC111
    absolute / OSC /mvd_intv), and an L/R ratio readout line.

Follows tests/test_variant_browse.py's FakeEngine + fixture pattern.
"""

import os
import time

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import neptrPhase4_UI as UI
import companion_input


class FakeEngine:
    def __init__(self):
        self.sent = []

    def press(self, control):
        self.sent.append(("press", control))
        return True

    def release(self, control):
        self.sent.append(("release", control))
        return True

    def tap(self, control):
        self.press(control)
        self.release(control)
        return True

    def rotate(self, control, up):
        self.sent.append(("rotate", control, bool(up)))
        return True

    def cc(self, control, value, channel=0):
        self.sent.append(("cc", control, value, channel))
        return True

    def sync_request(self):
        self.sent.append(("sync",))
        return True

    def close(self):
        pass


@pytest.fixture
def ui():
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    u = UI.NeptrUI()
    u.engine = FakeEngine()
    yield u
    pygame.display.quit()


def _ccs(ui):
    return [s for s in ui.engine.sent if s[0] == "cc"]


# --------------------------------------------------------------- menu wiring

def test_utility_menu_exists_and_is_last():
    assert UI.MENU_COUNT == len(UI.MENUS) == 21
    assert UI.UTILITY_MENU == 19
    assert UI.MENUS[UI.UTILITY_MENU]["name"] == "UTILITY"


def test_menu_nav_reaches_utility(ui):
    # /menu 19 navigates to the utility page (CC70/73 nav on the engine side
    # echoes /menu, which route_osc turns into handle_menu_change).
    UI.route_osc(ui, '/menu', 19)
    assert ui.current_view == 'menu19'
    assert ui.title_text == "UTILITY"


def test_utility_page_renders_custom(ui):
    ui.current_view = 'menu19'
    ui._last_rendered_view = ui._last_rendered_signature = None
    assert ui.render() is True
    # RECORD button hit region is present; no slot toggles.
    assert any(a[0] == 'rec_toggle' for _r, a in ui.hit)
    assert not any(a[0] == 'slot' for _r, a in ui.hit)


# --------------------------------------------------------------- master volume

def test_master_volume_drag_sends_cc103_ch16(ui):
    ui.current_view = 'menu19'
    ui.render()
    tr = ui._mvol_rect
    assert tr is not None
    # Drag to the far right = unity*1.5 = full 127.
    ui.touch_down(tr.right - 2, tr.centery)
    ui.touch_up(tr.right - 2, tr.centery)
    ccs = _ccs(ui)
    assert ccs, "no CC sent for master volume drag"
    ctrl, val, chan = ccs[-1][1], ccs[-1][2], ccs[-1][3]
    assert ctrl == UI.MASTER_VOL_CC == 103
    assert chan == companion_input.UTILITY_CHANNEL      # ch16
    assert val == 127


def test_master_out_osc_echo_updates_slider(ui):
    UI.route_osc(ui, '/master_out', 0.75)
    assert abs(ui.master_out - 0.75) < 1e-6


# --------------------------------------------------------------- master BPM

def test_master_bpm_drag_sends_cc104_ch16(ui):
    ui.current_view = 'menu19'
    ui.render()
    tr = ui._mbpm_rect
    ui.touch_down(tr.x + 2, tr.centery)     # far left = min BPM (40) = 0
    ui.touch_up(tr.x + 2, tr.centery)
    ccs = _ccs(ui)
    ctrl, val, chan = ccs[-1][1], ccs[-1][2], ccs[-1][3]
    assert ctrl == UI.MASTER_BPM_CC == 104
    assert chan == companion_input.UTILITY_CHANNEL
    assert val == 0


def test_master_and_clock_bpm_tracked_separately(ui):
    UI.route_osc(ui, '/master_bpm', 120.0)
    UI.route_osc(ui, '/clock_bpm', 124.0)
    assert ui.master_bpm == 120.0
    assert ui.clock_bpm == 124.0


# --------------------------------------------------------------- RECORD button

def test_record_toggle_runs_wrapper(ui, monkeypatch):
    calls = []

    class _FakePopen:
        def __init__(self, argv, **kw):
            calls.append(argv)

    monkeypatch.setattr(UI.subprocess, "Popen", _FakePopen)
    ui._toggle_record(time.time())
    assert calls and calls[0][0] == UI.RECORD_CMD and calls[0][1] == "toggle"
    assert ui.recording is True          # optimistic flip


def test_record_toggle_survives_missing_binary(ui, monkeypatch):
    def _boom(argv, **kw):
        raise OSError("no such file")
    monkeypatch.setattr(UI.subprocess, "Popen", _boom)
    ui._toggle_record(time.time())       # must not raise
    assert ui.recording is False


def test_record_status_poll_sets_state(ui, monkeypatch):
    class _R:
        returncode = 0
    monkeypatch.setattr(UI.subprocess, "run", lambda *a, **k: _R())
    ui._poll_record_status()
    assert ui.recording is True
    _R.returncode = 1
    ui._poll_record_status()
    assert ui.recording is False


# --------------------------------------------------------------- POLY DLY v5

def test_mvd_effect_view_shows_reps_and_interval(ui):
    ui.bypass_states['mvd'] = 0
    ui.enter_effect_view('mvd')
    assert ui.current_view == 'mvd'
    ui.render()
    reps = sorted(a[1] for _r, a in ui.hit if a[0] == 'mvd_reps')
    assert reps == list(range(1, 9))
    assert sorted(a[1] for _r, a in ui.hit if a[0] == 'mvd_intv') == [-1, 1]


def test_mvd_reps_pick_sends_cc110_ch1_absolute(ui):
    ui._mvd_reps_pick(8, time.time())    # last chip -> full 127
    ccs = _ccs(ui)
    assert ccs and ccs[-1][1] == UI.MVD_REPS_CC == 110
    assert ccs[-1][2] == 127
    assert ccs[-1][3] == 0               # ch1
    ui._mvd_reps_pick(1, time.time())
    assert _ccs(ui)[-1][2] == 0
    ui._mvd_reps_pick(4, time.time())
    assert _ccs(ui)[-1][2] == round(3 / 7 * 127)


def test_mvd_reps_tap_never_writes_locally(ui):
    before = ui.mvd_reps
    ui._mvd_reps_pick(7, time.time())
    assert ui.mvd_reps == before


def test_mvd_intv_step_sends_cc111_ch1_absolute(ui):
    UI.route_osc(ui, '/mvd_intv', 7)
    ui._mvd_intv_step(1, time.time())    # +8
    ccs = _ccs(ui)
    assert ccs[-1][1] == UI.MVD_INTV_CC == 111
    assert ccs[-1][2] == round((8 + 12) / 24 * 127)
    assert ccs[-1][3] == 0
    assert ui.mvd_intv == 7              # engine-authoritative: no local write


def test_mvd_intv_clamps_at_both_ends(ui):
    UI.route_osc(ui, '/mvd_intv', 12)
    ui._mvd_intv_step(1, time.time())
    assert _ccs(ui)[-1][2] == 127
    UI.route_osc(ui, '/mvd_intv', -12)
    ui._mvd_intv_step(-1, time.time())
    assert _ccs(ui)[-1][2] == 0


def test_mvd_reps_osc_echo_highlights(ui):
    UI.route_osc(ui, '/mvd_reps', 6)
    assert ui.mvd_reps == 6


def test_mvd_intv_osc_echo_signed(ui):
    UI.route_osc(ui, '/mvd_intv', -5)
    assert ui.mvd_intv == -5


def test_mvd_actions_fire(ui):
    ui._fire(('mvd_reps', 2), time.time())
    assert _ccs(ui)[-1][1] == 110
    ui._fire(('mvd_intv', -1), time.time())
    assert _ccs(ui)[-1][1] == 111


def test_mvd_v5_cc_map(ui):
    assert {p: UI.PARAM_CC[('mvd', p)] for p in
            ('numl', 'denl', 'numr', 'denr', 'att', 'dec')} == {
        'numl': (20, 69), 'denl': (23, 69), 'numr': (24, 69),
        'denr': (26, 69), 'att': (21, 69), 'dec': (22, 69)}
