"""Headless integration test: drive companion mode through the real NeptrUI.

Uses SDL's dummy video driver so it runs with no display/Pi. Exercises the
render paths (which would crash on a bad surface op) and the enter/nav/
activate/idle wiring end-to-end. Side-effecting actions are stubbed so nothing
shells out.
"""

import os

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import companion
import neptrPhase4_UI as UI


@pytest.fixture
def ui(monkeypatch):
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    # Stub the patch list so entering companion mode loads a known set
    # instead of scanning the (absent) patches dir on this dev machine.
    monkeypatch.setattr(
        UI.actions, "list_patches",
        lambda d: ["car_mode", "neptr", "neptr_bass", "neptr_surf", "testing_ground"])
    u = UI.NeptrUI()
    yield u
    pygame.display.quit()


CC = UI.companion_input

# Screen-direction raw values (calibrated for the 270° mount):
# next tab (right) = jy LOW; selection down = jx LOW; centre = 64.
LOWV, CTR = 8, 64


def _feed(u, cc, value):
    u.input_state.feed(cc, value)


def _center(u, t):
    _feed(u, CC.CC_JOY_X, CTR); _feed(u, CC.CC_JOY_Y, CTR)
    u.tick_companion(t)


def _tab_right(u, t):
    _feed(u, CC.CC_JOY_Y, LOWV); u.tick_companion(t); _center(u, t + 0.05)


def _sel_down(u, t):
    _feed(u, CC.CC_JOY_X, LOWV); u.tick_companion(t); _center(u, t + 0.05)


def _hold_enter(u, t0=0.0):
    _feed(u, CC.CC_JOY_BTN, 127)
    u.tick_companion(t0)
    u.tick_companion(t0 + 3.0)            # crosses the hold threshold
    _feed(u, CC.CC_JOY_BTN, 0)
    u.tick_companion(t0 + 3.1)
    return t0 + 3.0


def _tap_arcade(u, t):
    _feed(u, CC.CC_ARCADE, 127); u.tick_companion(t)
    _feed(u, CC.CC_ARCADE, 0); u.tick_companion(t + 0.05)   # release = tap


def test_hold_enters_and_renders_dashboard(ui):
    _hold_enter(ui)
    assert ui.companion.active
    assert ui.companion.screen == 0
    assert ui.render() is True            # dashboard renders without error
    assert ui.canvas.get_size() == (UI.W, UI.H)


def test_navigate_all_three_screens_render(ui):
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    assert ui.companion.screen == 1
    assert ui.render() is True            # patches list renders
    _tab_right(ui, t + 0.3)
    assert ui.companion.screen == 2
    assert ui.render() is True            # power buttons render


def test_patch_selection_moves_with_joystick(ui):
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    assert ui.companion.screen == 1 and ui.companion.sel == 0
    _sel_down(ui, t + 0.3)
    assert ui.companion.sel == 1


def _stub_switch(ui, monkeypatch):
    """Capture patch activation WITHOUT touching the machine. Both halves must
    be stubbed: on a real Pi the power half runs `demiurge-power <level>` for
    real, which is both a side effect a test must never have and a slow one
    (it made the activation assert race the daemon thread)."""
    activated = []
    monkeypatch.setattr(ui.acts.activator, "activate",
                        lambda name: activated.append(name))
    monkeypatch.setattr(ui.acts.power_set, "run", lambda **kw: True)
    monkeypatch.setattr(ui, "_refresh_power_label", lambda: None)
    monkeypatch.setattr(ui, "refresh_active_patch", lambda: ui.active_patch)
    return activated


def _settle(activated, want, timeout=2.0):
    """Wait for the daemon dispatch thread instead of sleeping a guess."""
    import time
    end = time.time() + timeout
    while time.time() < end and len(activated) < want:
        time.sleep(0.01)
    return activated


def test_one_arcade_tap_does_not_switch_patch(ui, monkeypatch):
    """A single tap ARMS the patch; it must not load it. One stray contact on
    the resistive panel used to rewrite live.conf, drop the power governor and
    restart the audio engine — always into car_mode, the row-0 patch."""
    activated = _stub_switch(ui, monkeypatch)
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    _tap_arcade(ui, t + 0.3)
    import time
    time.sleep(0.1)                       # a dispatch would have landed by now
    assert activated == []
    assert ui.companion_msg and "tap again" in ui.companion_msg


def test_second_arcade_tap_dispatches_patch(ui, monkeypatch):
    activated = _stub_switch(ui, monkeypatch)
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    sel = ui.companion.sel
    _tap_arcade(ui, t + 0.3)
    _tap_arcade(ui, t + 0.6)              # confirm, inside the window
    assert _settle(activated, 1) == [ui.patches[sel]]


def test_patch_arming_expires(ui, monkeypatch):
    activated = _stub_switch(ui, monkeypatch)
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    ui._companion_activate(1, 0, t + 0.3)
    ui._companion_activate(1, 0, t + 0.3 + UI.PATCH_CONFIRM_SECS + 0.1)
    import time
    time.sleep(0.05)
    assert activated == []                # window lapsed: re-armed, not fired


def test_active_patch_is_never_reloaded(ui, monkeypatch):
    activated = _stub_switch(ui, monkeypatch)
    ui.patches = ["car_mode", "neptr_surf"]
    ui.active_patch = "car_mode"
    ui._companion_activate(1, 0, 1.0)
    ui._companion_activate(1, 0, 1.2)
    import time
    time.sleep(0.05)
    assert activated == []
    assert ui.companion_msg == "already loaded"


def test_patches_cursor_lands_on_the_active_patch(ui, monkeypatch):
    """Entering PATCHES points the cursor at the patch live.conf actually
    holds, not at row 0 (car_mode)."""
    monkeypatch.setattr(UI.actions, "active_patch", lambda d, live_conf=None:
                        "neptr_surf")
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)
    assert ui.active_patch == "neptr_surf"
    assert ui.companion.sel == ui.patches.index("neptr_surf")
    assert ui.render() is True            # the ● marker renders


def test_power_screen_restart_dispatches(ui, monkeypatch):
    ran = []
    monkeypatch.setattr(ui.acts.restart, "run", lambda **kw: ran.append(True))
    t = _hold_enter(ui)
    _tab_right(ui, t + 0.1)               # -> PATCHES
    _tab_right(ui, t + 0.3)               # -> AUDIO
    _tab_right(ui, t + 0.5)               # -> SYSTEM (power lives here now)
    assert ui.companion.screen == companion.SYSTEM
    # move down to RESTART (index 3) and tap
    for k in range(3):
        _sel_down(ui, t + 0.7 + k * 0.2)
    assert ui.companion.sel == 3
    _tap_arcade(ui, t + 1.6)
    import time
    time.sleep(0.05)
    assert ran == [True]


def test_idle_exits_to_sleep(ui):
    t = _hold_enter(ui)
    ui.tick_companion(t + 15.1)
    assert not ui.companion.active
    assert ui.is_sleeping


def test_second_hold_exits_to_normal(ui):
    t = _hold_enter(ui)
    assert ui.companion.active
    _feed(ui, CC.CC_JOY_BTN, 127)
    ui.tick_companion(t + 5.0)
    ui.tick_companion(t + 8.0)            # 3s hold again
    assert not ui.companion.active
    assert not ui.is_sleeping             # plain exit, not idle-sleep


def test_midi_port_prefers_teensy_source_over_through(monkeypatch):
    # Audio-independence: read the Teensy's own port, not the audio-engine-wired
    # "Midi Through", so sensor streaming works with Csound on OR off.
    import mido
    monkeypatch.delenv("NEPTR_MIDI_PORT", raising=False)
    monkeypatch.setattr(mido, "get_input_names", lambda: [
        "Midi Through:Midi Through Port-0 14:0",
        "Teensy MIDI:Teensy MIDI MIDI 1 16:0",
    ])
    assert "Teensy" in UI.choose_midi_port()


def test_midi_port_falls_back_to_through_when_no_teensy(monkeypatch):
    import mido
    monkeypatch.delenv("NEPTR_MIDI_PORT", raising=False)
    monkeypatch.setattr(mido, "get_input_names",
                        lambda: ["Midi Through:Midi Through Port-0 14:0"])
    assert "Through" in UI.choose_midi_port()


def test_midi_port_env_override_wins(monkeypatch):
    import mido
    monkeypatch.setenv("NEPTR_MIDI_PORT", "Through")
    monkeypatch.setattr(mido, "get_input_names", lambda: [
        "Midi Through:Midi Through Port-0 14:0",
        "Teensy MIDI:Teensy MIDI MIDI 1 16:0",
    ])
    assert "Through" in UI.choose_midi_port()


def test_normal_ui_unaffected_when_companion_idle(ui):
    # No companion interaction: normal menu rendering still works.
    ui.handle_menu_change(2)
    assert ui.render() is True
    assert ui.current_view == "menu2"
    assert not ui.companion.active


# ---------------------------------------------------------------------------
# Preset bank switching
# ---------------------------------------------------------------------------

def test_preset_bank_defaults_to_surf(ui):
    assert ui.preset_bank == 0


def test_bank_switch_to_bass_changes_menu0_effects(ui):
    UI.route_osc(ui, '/menu', 0)
    UI.route_osc(ui, '/preset_bank', 1)
    effects = ui._menu_effects(0)
    codes = [c for c, _ in effects]
    assert codes == ["bpl", "bph", "tgl", "bds", "h90"]


def test_bank_switch_to_surf_restores_surf_effects(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/preset_bank', 0)
    effects = ui._menu_effects(0)
    codes = [c for c, _ in effects]
    assert codes == ["sld", "hvy", "pha", "trm", "amb"]


def test_bank_switch_renders_bass_labels(ui):
    UI.route_osc(ui, '/menu', 0)
    UI.route_osc(ui, '/preset_bank', 1)
    assert ui.render() is True
    names = [n for _, n in ui._menu_effects(0)]
    assert "PRE LIGHT" in names
    assert "H90 WAMMY" in names


def test_bank_switch_triggers_signature_change(ui):
    UI.route_osc(ui, '/menu', 0)
    ui.render()
    sig_surf = ui._signature("menu0")
    UI.route_osc(ui, '/preset_bank', 1)
    sig_bass = ui._signature("menu0")
    assert sig_surf != sig_bass


def test_bass_bpl_slot_lights_up_from_state(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/menu', 0)
    UI.route_osc(ui, '/bpl_state', 0)   # 0 = engaged
    assert ui._slot_is_on(0, 0, "bpl") is True


def test_bass_bpl_slot_dim_when_bypassed(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/bpl_state', 1)   # 1 = bypassed
    assert ui._slot_is_on(0, 0, "bpl") is False


def test_bass_h90_slot_uses_bypass_state(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/h90_state', 0)
    assert ui._slot_is_on(0, 4, "h90") is True


def test_bass_tgl_slot_uses_tgl_state(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/tgl_state', 0)   # 0 = total glitch ON
    assert ui._slot_is_on(0, 2, "tgl") is True


def test_bass_state_on_menu0_marks_dirty(ui):
    UI.route_osc(ui, '/preset_bank', 1)
    UI.route_osc(ui, '/menu', 0)
    ui.render()           # settle — clears dirty
    ui.dirty = False
    UI.route_osc(ui, '/bpl_state', 0)
    assert ui.dirty is True


def test_other_menus_unaffected_by_bank_switch(ui):
    for menu_idx in range(1, 11):
        surf = UI.MENUS[menu_idx]["effects"] if menu_idx < len(UI.MENUS) else []
        UI.route_osc(ui, '/preset_bank', 1)
        assert ui._menu_effects(menu_idx) == surf, f"menu{menu_idx} changed in bass mode"


def test_non_menu0_render_unaffected_by_bank(ui):
    UI.route_osc(ui, '/menu', 4)    # MOD menu
    UI.route_osc(ui, '/preset_bank', 1)
    assert ui.render() is True
    assert ui.current_view == "menu4"


# ---------------------------------------------------------------------------
# PREAMP menu state routing
# ---------------------------------------------------------------------------

def test_preamp_state_routes_to_pre_slot(ui):
    # Csound sends /preamp_state; UI code for MAIN PRE slot is "pre"
    UI.route_osc(ui, '/preamp_state', 0)   # engaged
    assert ui.bypass_states.get('pre') == 0


def test_preamp_state_bypassed_routes_correctly(ui):
    UI.route_osc(ui, '/preamp_state', 0)
    UI.route_osc(ui, '/preamp_state', 1)   # bypassed
    assert ui.bypass_states.get('pre') == 1


def test_preamp_menu_main_pre_slot_lights_on_state(ui):
    UI.route_osc(ui, '/menu', 6)           # PREAMP menu
    UI.route_osc(ui, '/preamp_state', 0)
    assert ui._slot_is_on(6, 0, 'pre') is True


def test_trm_state_routes_to_trm_slot(ui):
    # /trm_state (line 1566) is the correct send; /trem_state (now removed) was stale
    UI.route_osc(ui, '/trm_state', 0)
    assert ui.bypass_states.get('trm') == 0


# ---------------------------------------------------------------------------
# LOOPER menu (menu 14 since the 2026-07-26 delay/verb-expansion reorg;
# handler_looper.orc hardcodes this index): OSC state must actually repaint the screen
# ---------------------------------------------------------------------------

def test_looper_menu_renders(ui):
    UI.route_osc(ui, '/menu', 14)
    assert ui.current_view == 'menu14'
    assert ui.render() is True


def test_looper_state_change_triggers_repaint(ui):
    # The repaint-dedup signature must include lp_states — this render()
    # returned False (repaint swallowed) before the signature fix.
    UI.route_osc(ui, '/menu', 14)
    ui.render()
    UI.route_osc(ui, '/lp1_state', 1)      # CH1 → REC
    assert ui.lp_states[0] == 1
    assert ui.dirty is True
    assert ui.render() is True


def test_looper_all_states_render_without_crash(ui):
    # States 0-4 incl. STOP(4), which used to overflow STATE_LABELS/COLORS.
    UI.route_osc(ui, '/menu', 14)
    for state in (0, 1, 2, 3, 4, 5):
        UI.route_osc(ui, '/lp3_state', state)
        assert ui.render() is True


def test_looper_pos_updates_repaint(ui):
    UI.route_osc(ui, '/menu', 14)
    UI.route_osc(ui, '/lp2_state', 2)      # CH2 playing
    ui.render()
    UI.route_osc(ui, '/lp2_pos', 0.5)
    assert ui.lp_pos[1] == 0.5
    assert ui.render() is True


def test_looper_speed_updates_repaint(ui):
    UI.route_osc(ui, '/menu', 14)
    ui.render()
    UI.route_osc(ui, '/lp4_speed', 7)      # CH4 → +2x
    assert ui.lp_speed[3] == 7
    assert ui.render() is True


def test_looper_osc_wakes_sleeping_ui(ui):
    UI.route_osc(ui, '/menu', 14)
    ui.render()
    ui.enter_sleep()
    UI.route_osc(ui, '/lp1_state', 1)
    assert ui.is_sleeping is False


def test_looper_osc_ignores_bad_channel(ui):
    # /lp9_state must not crash or write out of range
    UI.route_osc(ui, '/lp9_state', 2)
    assert ui.lp_states == [0, 0, 0, 0, 0]


def test_looper_drum_index_triggers_repaint(ui):
    # /lp_drum_* must not be swallowed by the startswith('/lp') branches, and
    # the repaint-dedup signature must include the drum state.
    UI.route_osc(ui, '/menu', 14)
    ui.render()
    UI.route_osc(ui, '/lp_drum_ch', 1)
    UI.route_osc(ui, '/lp_drum_idx', 3)
    assert ui.lp_drum_ch == 1
    assert ui.lp_drum_idx == 3
    assert ui.dirty is True
    assert ui.render() is True


def test_looper_drum_count_routes(ui):
    UI.route_osc(ui, '/lp_drum_n', 7)
    assert ui.lp_drum_count == 7


def test_looper_drum_mode_renders(ui):
    UI.route_osc(ui, '/menu', 14)
    UI.route_osc(ui, '/lp_drum_n', 4)
    UI.route_osc(ui, '/lp_drum_ch', 1)
    UI.route_osc(ui, '/lp_drum_idx', 0)
    UI.route_osc(ui, '/lp1_state', 2)      # DRUMS auto-plays on load
    assert ui.render() is True


def test_looper_drum_styling_follows_channel(ui):
    # The DRUM row must follow /lp_drum_ch — with the drum loop on ch3 the
    # amber outline sits on row 2 and ch1's row stays on the state palette.
    UI.route_osc(ui, '/menu', 14)
    UI.route_osc(ui, '/lp_drum_n', 4)
    UI.route_osc(ui, '/lp_drum_ch', 3)
    UI.route_osc(ui, '/lp_drum_idx', 1)
    assert ui.render() is True
    slot_h = (UI.CONTENT_BOTTOM - UI.CONTENT_TOP - 4 * UI.SLOT_SPACING) / 5

    def outline_at(row):
        y = int(UI.CONTENT_TOP + row * (slot_h + UI.SLOT_SPACING)) + UI.SLOT_INSET
        return tuple(ui.canvas.get_at((UI.PAD_X + UI.SLOT_INSET + 1, y + 1)))[:3]

    assert outline_at(2) == (220, 170, 40)     # ch3 row = amber DRUM outline
    assert outline_at(0) != (220, 170, 40)     # ch1 row untouched


def test_looper_drum_exit_returns_to_live_display(ui):
    UI.route_osc(ui, '/menu', 14)
    UI.route_osc(ui, '/lp_drum_ch', 2)
    UI.route_osc(ui, '/lp_drum_idx', 0)
    ui.render()
    UI.route_osc(ui, '/lp_drum_ch', -1)    # double-tap clear → live mode
    UI.route_osc(ui, '/lp_drum_idx', -1)
    assert ui.lp_drum_ch == -1
    assert ui.lp_drum_idx == -1
    assert ui.render() is True


# ---------------------------------------------------------------------------
# 2026-07-26 delay/verb expansion phase 2 — new menu slots + param pages
# (docs/specs/2026-07-26-delay-verb-expansion.md). BINSCATTER/TAPE DELAY/
# MOVE DELAY/NEBULA/SHIMMER/FREEZE/GRAINVERB.
# ---------------------------------------------------------------------------

def test_digiverb_menu_slot_order(ui):
    # Menu 9 DIGIVERB: NEBULA, SHIMMER, FREEZE, GRAINVERB, REVERB last.
    menu_idx = next(i for i, m in enumerate(UI.MENUS) if m["name"] == "DIGIVERB")
    codes = [c for c, _ in UI.MENUS[menu_idx]["effects"]]
    assert codes == ["neb", "shm", "frz", "gvb", "vrb"]


def test_delay_menu_has_tape_and_move_delay(ui):
    menu_idx = next(i for i, m in enumerate(UI.MENUS) if m["name"] == "DELAY")
    codes = [c for c, _ in UI.MENUS[menu_idx]["effects"]]
    assert "tdl" in codes and "mvd" in codes
    # SPECDLY lives ONLY in DELAY now, no longer dual-homed in SPECTRAL.
    spectral_idx = next(i for i, m in enumerate(UI.MENUS) if m["name"] == "SPECTRAL")
    spectral_codes = [c for c, _ in UI.MENUS[spectral_idx]["effects"]]
    assert "sdl" not in spectral_codes
    assert "bsc" in spectral_codes


def test_binscatter_replaces_specdly_slot_in_spectral(ui):
    menu_idx = next(i for i, m in enumerate(UI.MENUS) if m["name"] == "SPECTRAL")
    assert ("bsc", "BINSCATR") in UI.MENUS[menu_idx]["effects"]


def test_tdl_param_page_renders_with_stepped_head_label(ui):
    UI.route_osc(ui, '/tdl_state', 0)      # engaged
    ui.enter_effect_view('tdl')
    UI.route_osc(ui, '/tdl_mix', 0.35)
    UI.route_osc(ui, '/tdl_time', 300)
    UI.route_osc(ui, '/tdl_fback', 0.35)
    UI.route_osc(ui, '/tdl_center', 1200)
    UI.route_osc(ui, '/tdl_width', 0.7)
    UI.route_osc(ui, '/tdl_head', 4)        # index 4 -> "1+2" (TDL_HEADS)
    assert ui.render() is True
    assert UI.STEP_LABELS[('tdl', 'head')][4] == "1+2"


def test_mvd_param_page_renders_with_v5_sliders(ui):
    UI.route_osc(ui, '/mvd_state', 0)
    ui.enter_effect_view('mvd')
    UI.route_osc(ui, '/mvd_numl', 3)
    UI.route_osc(ui, '/mvd_denl', 4)
    UI.route_osc(ui, '/mvd_numr', 5)
    UI.route_osc(ui, '/mvd_denr', 7)
    UI.route_osc(ui, '/mvd_att', 0.3)
    UI.route_osc(ui, '/mvd_dec', 0.6)
    assert ui.render() is True
    assert [p[0] for p in UI.EFFECT_PARAMS['mvd']] == [
        'numl', 'denl', 'numr', 'denr', 'att', 'dec']
    assert ui.param_values['/mvd_numl'] == 3
    assert ('mvd', 'div') not in UI.STEP_LABELS


def test_new_effects_registered_in_all_effects(ui):
    for code in ("bsc", "tdl", "mvd", "neb", "shm", "frz", "gvb"):
        assert code in UI.ALL_EFFECTS
        assert code in UI.EFFECT_PARAMS
