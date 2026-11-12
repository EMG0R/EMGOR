"""Engine-link + touch-control tests (2026-08-09 batch).

Covers: the Teensy-identical CC mapping tables, slot tap -> toggle CC (with
engine-authoritative visuals — no local flip), the effect-view power button,
looper press/release passthrough, tab swipes vs list scrolls (axis
dominance), param-slider chase pulses (rate-capped, /last_toggle-gated),
WAVE edge-sweep clamp gestures + bar-stroke interpolation, the sync-request
writer, and the glow premultiply fix (faces discoloration).
"""

import os
import time

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import companion_input
import neptrPhase4_UI as UI
import wavetable


class FakeEngine:
    """Records what the UI asks the engine link to send."""

    def __init__(self):
        self.sent = []          # ("press"|"release"|"rotate"|"sync", arg)

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
        self.sent.append(("cc", control, value))
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


def tap(u, pos):
    u.touch_down(*pos)
    u.touch_up(*pos)


def find_hit(u, kind):
    return [(r, a) for r, a in u.hit if a[0] == kind]


# --------------------------------------------------------------- CC mapping

def test_slot_button_cc_default_convention():
    # row0 = hardware slot4 (logical 72 -> physical 75) ... row4 = slot0
    assert [UI.slot_button_cc(1, r) for r in range(5)] == [75, 71, 72, 73, 70]


def test_slot_button_cc_looper_is_ch_order():
    # LOOPER rows are CH1..CH5 = slot0..slot4
    assert [UI.slot_button_cc(UI.LOOPER_MENU, r) for r in range(5)] == \
        [70, 73, 72, 71, 75]


def test_param_rot_cc_nam_alias_and_gate_ids():
    # 2026-09-01: In<->Out encoders swapped (physical P2/P3 were reversed).
    # OUT gain now rides P2 (logical CC20 -> physical CC26); IN gain rides P3
    # (logical CC23 -> physical CC24). Engine agrees: rot_p2_cc20.orc drives
    # gkNAMOutGain, rot_p3_cc23.orc drives gkNAMInGain (both lastToggle 37).
    assert UI.param_rot_cc("nam3", "out_gain") == (26, 37)   # logical 20 (P2)
    assert UI.param_rot_cc("nam3", "in_gain") == (24, 37)    # logical 23 (P3)
    assert UI.param_rot_cc("ir4", "mix") == (26, 53)         # logical 20 (P2)
    assert UI.param_rot_cc("tape", "amt") == (25, 21)        # logical 22
    assert UI.param_rot_cc("tape", "nosuch") is None


def test_nam_and_ir_params_avoid_menu_shadowed_slot_rotations():
    """Menus 17 (NAM) and 18 (IR VERB) hand every SLOT rotation to their
    variant browse, so the amp gains and the IR dry/wet must ride the two
    menu-encoder rotations (logical CC20/CC23) instead — otherwise the
    slider is dead on the very page that draws it."""
    menu_rots = {20, 23}
    for code, param in (("nam", "in_gain"), ("nam", "out_gain"),
                        ("ir0", "mix"), ("ir1", "mix"), ("ir2", "mix"),
                        ("ir3", "mix"), ("ir4", "mix")):
        logical, toggle = UI.PARAM_CC[(code, param)]
        assert logical in menu_rots, (code, param, logical)
    # every IR row arms on its own toggle id (49 + row)
    for r in range(5):
        assert UI.PARAM_CC[(f"ir{r}", "mix")][1] == 49 + r
    # both NAM gains arm on the single amp-sim toggle
    assert UI.PARAM_CC[("nam", "in_gain")][1] == 37
    assert UI.PARAM_CC[("nam", "out_gain")][1] == 37
    # the two gains do NOT share an encoder
    assert UI.PARAM_CC[("nam", "in_gain")][0] != \
        UI.PARAM_CC[("nam", "out_gain")][0]


def test_nam_gain_slider_ranges_have_unity_at_zero_db():
    for r in range(5):
        params = dict((p[0], p) for p in UI.EFFECT_PARAMS[f"nam{r}"])
        assert params["in_gain"][2] < 0 < params["in_gain"][3]
        assert params["out_gain"][2] < 0 < params["out_gain"][3]


# ------------------------------------------------------------- slot toggles

def test_off_slot_tap_sends_toggle_cc_no_local_flip(ui):
    ui.handle_menu_change(1)                      # VOLUME
    ui.render()
    rect, action = find_hit(ui, "slot")[0]        # row0 = TAPE (off)
    assert action[3] == "tape"
    tap(ui, rect.center)
    assert ui.engine.sent == [("press", 75), ("release", 75)]
    # visual state comes ONLY from the engine's OSC broadcast
    assert ui.bypass_states.get("tape", 1) == 1


def test_preset_slot_tap_always_sends(ui):
    ui.handle_menu_change(0)
    ui.render()
    rect, action = find_hit(ui, "slot")[0]        # row0 = OVERDRIVE
    tap(ui, rect.center)
    assert ("press", 75) in ui.engine.sent


def test_on_slot_tap_toggles_off_symmetric(ui):
    # Symmetric toggle: tapping a lit slot sends the same CC (engine will
    # bypass it); no local navigation, no local state flip.
    ui.handle_menu_change(1)
    ui.bypass_states["tape"] = 0
    ui.render()
    rect, _ = find_hit(ui, "slot")[0]
    tap(ui, rect.center)
    assert ui.engine.sent == [("press", 75), ("release", 75)]
    assert ui.current_view == "menu1"
    assert ui.bypass_states["tape"] == 0          # engine echo flips it


def test_bypass_echo_keeps_effect_view_open(ui):
    # Field-test call: bypassing the viewed effect STAYS on the view
    # (dimmed off-state) instead of snapping back to the menu.
    ui.handle_menu_change(1)
    ui.bypass_states["tape"] = 0
    ui.enter_effect_view("tape")
    ui.handle_state_change("tape", 1)
    assert ui.current_view == "tape"


def test_body_encoder_toggle_off_is_visible_from_an_effect_page(ui):
    """The BODY's encoders must move the screen exactly like a finger does.

    2026-09-15 field report: "when i toggle effects on w the neptr body the
    ui doesn't reflect it ... not just the touchscreen". Reproduced entirely
    through route_osc — i.e. through the engine's own state broadcast, with
    NO touch at all, which is the only difference between an encoder press
    and a screen tap (both land on the same physical CC, the same midiin,
    the same dispatch).

    The trap is that a toggle-ON parks the UI on that effect's param page
    (enter_effect_view) while the ENGINE stays on its menu. From then on the
    UI's "am I looking at this effect's slot?" test is asking a view the
    engine never heard of, so every further encoder toggle-OFF of any OTHER
    effect updated bypass_states and drew nothing whatsoever. A finger can
    never reach that state: a slot tap is only possible from the menu you
    are already looking at."""
    ui.sync_quiet_until = 0
    ui.handle_menu_change(1)                      # engine says: VOLUME menu
    UI.route_osc(ui, "/tape_state", 0)            # encoder: TAPE on
    ui.enter_effect_view(ui.pending_switch_code)  # ...UI parks on the tape page
    assert ui.current_view == "tape"
    UI.route_osc(ui, "/atk_state", 0)             # encoder: ATTACK on
    assert ui.current_view == "atk"

    ui.dirty = False
    UI.route_osc(ui, "/tape_state", 1)            # encoder: TAPE back off
    assert ui.bypass_states["tape"] == 1          # state was always accepted...
    assert ui.dirty                               # ...but must also be DRAWN
    # and drawn somewhere it is actually visible: the engine's own menu, whose
    # slot grid is the one surface that shows every slot's on/off at once.
    assert ui.current_view == f"menu{ui.last_menu_index}" == "menu1"
    assert ui._slot_is_on(1, 0, "tape") is False
    assert ui._slot_is_on(1, 1, "atk") is True


def test_engine_state_change_on_the_hyperguitar_page_does_not_crash(ui):
    """HG_MENU is one PAST MENUS, so the same "is this effect on my view?"
    lookups raised IndexError there — inside route_osc, which the main loop
    calls unguarded. Any encoder toggle with the guitar plugged in killed the
    screen. The hyperGuitar page draws lanes, not effect slots: no effects."""
    ui.sync_quiet_until = 0
    ui.hg_present = True
    ui.handle_menu_change(UI.HG_MENU)
    assert ui.current_view == f"menu{UI.HG_MENU}"
    assert ui._menu_effects(UI.HG_MENU) == []
    UI.route_osc(ui, "/tape_state", 0)            # must not raise
    UI.route_osc(ui, "/tape_state", 1)
    assert ui.bypass_states["tape"] == 1


def test_menu_swipe_pages_menus_via_engine(ui):
    ui.handle_menu_change(2)                      # SPECTRAL
    ui.render()
    ui.touch_down(360, 640)
    ui.touch_move(360 - UI.SWIPE_PX - 10, 646)    # swipe left -> next menu
    assert ui.engine.sent == [("press", 74), ("release", 74)]
    ui.touch_up(200, 646)
    assert ui.current_view == "menu2"             # view waits for /menu echo
    ui.engine.sent.clear()
    ui.touch_down(360, 640)
    ui.touch_move(360 + UI.SWIPE_PX + 10, 646)    # swipe right -> previous
    ui.touch_up(500, 646)
    assert ui.engine.sent == [("press", 76), ("release", 76)]


def test_gorcore_bank_labels_and_lighting(ui):
    # 2026-09-09: the 5 gorcore onboard slots are now USER preset-bank slots
    # (13..17), not baked voicings. Labels still come from GORCORE_PRESETS; a tap
    # now RECALLS the bank slot via ch16 CC106 (value=slot), not a factory-select
    # slot-button press. (Active-slot lighting from /preset is retired for
    # gorcore — a follow-up can light from an engine occupancy/active echo.)
    ui.handle_preset_bank_change(2)
    ui.handle_menu_change(0)
    assert [c for c, _ in ui._menu_effects(0)] == \
        ["gc1", "gc2", "gc3", "gc4", "gc5"]
    ui.render()
    rect, action = find_hit(ui, "slot")[1]        # row1 -> onboard bank slot 14
    tap(ui, rect.center)
    assert ("cc", 106, 14) in ui.engine.sent


def test_effect_view_power_button_taps_home_slot(ui):
    ui.handle_menu_change(1)
    ui.bypass_states["tape"] = 0
    ui.enter_effect_view("tape")
    ui.render()
    rect, action = find_hit(ui, "fx_toggle")[0]
    assert action[1] == "tape"
    tap(ui, rect.center)
    assert ui.engine.sent == [("press", 75), ("release", 75)]


# ------------------------------------------------------------------- looper

def test_looper_row_press_release_passthrough(ui):
    ui.handle_menu_change(UI.LOOPER_MENU)
    ui.render()
    rect, action = find_hit(ui, "lp")[0]          # CH1
    assert action[1] == 70
    ui.touch_down(*rect.center)
    assert ui.engine.sent == [("press", 70)]
    ui.touch_up(*rect.center)
    assert ui.engine.sent == [("press", 70), ("release", 70)]


# ----------------------------------------------------------- swipes/scrolls

def _open_companion(u, screen):
    u.companion.active = True
    u.companion.screen = screen
    u.patches = ["p%d" % i for i in range(12)]
    u.dirty = True
    u.render()


def test_horizontal_swipe_switches_tab(ui):
    _open_companion(ui, 1)
    ui.touch_down(360, 640)
    ui.touch_move(360 - UI.SWIPE_PX - 10, 646)    # swipe left -> next tab
    assert ui.companion.screen == 2
    ui.touch_up(200, 646)
    assert ui.companion.screen == 2


def test_vertical_drag_still_scrolls_not_swipes(ui):
    _open_companion(ui, 1)
    ui.touch_down(360, 800)
    ui.touch_move(365, 800 - 2 * UI.SCROLL_STEP)
    assert ui.companion.screen == 1
    assert ui.list_scroll.get(1, 0) > 0


def test_tap_fires_on_release_not_down(ui):
    # Note-on/note-off model: down only lights the pressed preview; the
    # release fires the action.
    _open_companion(ui, 0)
    rect, action = [h for h in find_hit(ui, "tab") if h[1][1] == 2][0]
    ui.touch_down(*rect.center)
    assert ui.companion.screen == 0               # nothing fired yet
    assert ui._pressed(("tab", 2))                # but the tab lights NOW
    ui.touch_up(*rect.center)
    assert ui.companion.screen == 2               # release commits


def test_swipe_starting_on_slot_pages_without_toggle(ui):
    # A touch that starts on a slot but becomes a swipe must page the menu
    # and NEVER emit the slot's toggle CC.
    ui.handle_menu_change(1)
    ui.render()
    rect, _ = find_hit(ui, "slot")[0]
    x, y = rect.center
    ui.touch_down(x, y)
    assert ui.pressed_hit is not None
    ui.touch_move(x - UI.SWIPE_PX - 10, y + 4)
    ui.touch_up(x - UI.SWIPE_PX - 10, y + 4)
    assert ui.engine.sent == [("press", 74), ("release", 74)]  # menu NEXT only
    assert ui.pressed_hit is None                 # preview cancelled


def test_pressed_preview_flips_slot_and_reverts(ui):
    ui.handle_menu_change(1)
    ui.render()
    rect, action = find_hit(ui, "slot")[0]        # OFF slot
    ui.touch_down(*rect.center)
    assert ui._pressed(action)                    # draws as ON while held
    ui.touch_move(rect.centerx + UI.TAP_SLOP + 8, rect.centery)
    assert ui.pressed_hit is None                 # slid/moved -> fading
    assert ui.pressed_fade is not None


# ------------------------------------------------------------ param sliders

def _enter_armed_effect(u):
    u.handle_menu_change(1)
    u.bypass_states["tape"] = 0
    u.last_toggle = 21                            # TAPE's toggle id
    u.enter_effect_view("tape")
    u.render()


def test_slider_drag_bursts_toward_finger_realtime(ui):
    _enter_armed_effect(ui)
    rect, name, lo, hi = ui._param_hits[0]        # amt, 0..1
    assert name == "amt"
    ui.touch_down(rect.centerx, rect.y + 4)       # target near max
    assert ui.drag_target == ("param", "amt")
    ui.tick_param_chase(100.0)
    # near-full error -> a BURST of detents (real-time tracking), capped
    assert ui.engine.sent == [("rotate", 25, True)] * UI.PARAM_BURST
    ui.tick_param_chase(100.001)                  # inside the tick window
    assert len(ui.engine.sent) == UI.PARAM_BURST
    # small remaining error -> proportional (single) pulse
    ui.param_values["/tape_amt"] = ui.param_chase["target"] - 0.03
    ui.tick_param_chase(100.0 + UI.PARAM_TICK + 0.001)
    assert len(ui.engine.sent) == UI.PARAM_BURST + 2  # ~0.03/0.02 -> 2 detents
    # engine echo lands on target -> no more pulses
    ui.param_values["/tape_amt"] = ui.param_chase["target"]
    n = len(ui.engine.sent)
    ui.tick_param_chase(101.0)
    assert len(ui.engine.sent) == n
    ui.touch_up(rect.centerx, rect.y + 4)


def test_slider_thumb_follows_finger_absolute(ui):
    # The dragged slider draws the FINGER target, live, both directions.
    _enter_armed_effect(ui)
    rect, name, lo, hi = ui._param_hits[0]
    ui.touch_down(rect.centerx, rect.y + 4)
    hi_target = ui.param_chase["target"]
    assert hi_target > 0.9 * (hi - lo) + lo
    ui.touch_move(rect.centerx, rect.bottom - 4)  # drag DOWN in the same drag
    assert ui.param_chase["target"] < 0.1 * (hi - lo) + lo
    ui.touch_move(rect.centerx, rect.y + 4)       # and back UP
    assert ui.param_chase["target"] > 0.9 * (hi - lo) + lo
    ui.touch_up(rect.centerx, rect.y + 4)


def test_nam_gain_slider_sends_menu_encoder_cc_from_the_nam_page(ui):
    """UI -> engine, on the page that used to be dead. Standing in the NAM
    menu, the amp's OUT dB slider must pulse a menu-encoder rotation — NOT a
    slot rotation, which menu 17 spends on the variant browse. 2026-09-01: the
    In<->Out encoders were swapped, so OUT now rides P2 (physical CC26; it was
    P3/CC24 before). rot_p2_cc20.orc drives gkNAMOutGain to match."""
    ui.handle_menu_change(UI.NAM_MENU)
    ui.bypass_states["nam1"] = 0
    ui.last_toggle = 37
    ui.enter_effect_view("nam1")
    ui.render()
    rect, name, lo, hi = ui._param_hits[0]
    assert name == "out_gain"
    ui.touch_down(rect.centerx, rect.y + 4)
    ui.tick_param_chase(100.0)
    assert ui.engine.sent == [("rotate", 26, True)] * UI.PARAM_BURST
    ui.touch_up(rect.centerx, rect.y + 4)


def test_ir_mix_slider_sends_menu_encoder_cc_from_the_ir_page(ui):
    """Same for the IR reverb's dry/wet: P2 rotation (physical CC26)."""
    ui.handle_menu_change(UI.IR_MENU)
    ui.bypass_states["ir2"] = 0
    ui.last_toggle = 51                           # HALL = row 2 = toggle 51
    ui.enter_effect_view("ir2")
    ui.render()
    rect, name, lo, hi = ui._param_hits[0]
    assert name == "mix"
    ui.touch_down(rect.centerx, rect.y + 4)
    ui.tick_param_chase(100.0)
    assert ui.engine.sent == [("rotate", 26, True)] * UI.PARAM_BURST
    ui.touch_up(rect.centerx, rect.y + 4)


def test_engine_broadcast_updates_nam_and_ir_param_mirrors(ui):
    """engine -> UI: the addresses the engine actually publishes land in the
    UI's param cache (/nam_in_gain and /nam_out_gain fan out to all 5 rows;
    /ir<r>_mix goes through the generic code_param branch)."""
    UI.route_osc(ui, '/nam_in_gain', 5.0)
    UI.route_osc(ui, '/nam_out_gain', -3.0)
    UI.route_osc(ui, '/ir0_mix', 0.55)
    for r in range(5):
        assert ui.param_values['/nam%d_in_gain' % r] == 5.0
        assert ui.param_values['/nam%d_out_gain' % r] == -3.0
    assert ui.param_values['/ir0_mix'] == 0.55


def test_slider_gated_when_knobs_armed_elsewhere(ui):
    _enter_armed_effect(ui)
    ui.last_toggle = 15                           # DELAY armed, not TAPE
    rect, name, lo, hi = ui._param_hits[0]
    ui.touch_down(rect.centerx, rect.centery)
    assert ui.drag_target is None
    assert ui.param_hint_until is not None
    ui.tick_param_chase(100.0)
    assert ui.engine.sent == []


def test_slider_downward_chase_direction(ui):
    _enter_armed_effect(ui)
    ui.param_values["/tape_amt"] = 0.9
    rect, name, lo, hi = ui._param_hits[0]
    ui.touch_down(rect.centerx, rect.bottom - 4)  # target near min
    ui.tick_param_chase(100.0)
    assert ui.engine.sent and all(
        m == ("rotate", 25, False) for m in ui.engine.sent)
    assert len(ui.engine.sent) <= UI.PARAM_BURST


# ------------------------------------------------------- WAVE edge gestures

def _open_wave(u):
    """Open the WAVE editor. It is a TOP-LEVEL destination (reached from the
    main screen's top-left button), not a companion tab — so the companion
    menu must be CLOSED for the editor to be on screen at all."""
    u.companion.active = False
    u.enter_wave()
    u.render()
    assert u.on_wave()
    return u._wave_rects


def test_wave_bottom_sweep_zeroes_swept_range(ui):
    wr, _ = _open_wave(ui)
    ui.touch_down(wr.x + 5, wr.centery)
    ui.touch_move(wr.x + 5, wr.bottom + 40)       # exit below, stroke lives on
    ui.touch_move(wr.right - 5, wr.bottom + 40)   # sweep along the bottom
    ui.touch_up(wr.right - 5, wr.bottom + 40)
    n = wavetable.N
    assert all(abs(v) < 1e-9 for v in ui.wave.wave[n // 4: 3 * n // 4])


def test_wave_top_sweep_maxes_swept_range(ui):
    wr, _ = _open_wave(ui)
    ui.touch_down(wr.x + 5, wr.y + 5)             # stroke starts inside
    ui.touch_move(wr.x + 5, wr.y - 30)            # exits above, keeps tracking
    ui.touch_move(wr.right - 5, wr.y - 30)
    ui.touch_up(wr.right - 5, wr.y - 30)
    n = wavetable.N
    assert all(v > 0.999 for v in ui.wave.wave[n // 4: 3 * n // 4])


def test_wave_swift_full_width_drag_reshapes_whole_table(ui):
    # One fast stroke (few motion events) across the full width must
    # reshape the ENTIRE table — the interpolator fills the gaps.
    wr, _ = _open_wave(ui)
    ui.touch_down(wr.x + 1, wr.centery)           # amp ~0 at left edge
    ui.touch_move(wr.centerx, wr.y + 8)           # one event: near max mid
    ui.touch_move(wr.right - 1, wr.centery)       # one event: back to ~0
    ui.touch_up(wr.right - 1, wr.centery)
    w = ui.wave.wave
    n = wavetable.N
    assert w[n // 2] > 0.9                        # peak landed mid-table
    assert abs(w[n // 4] - w[n // 2] / 2) < 0.15  # linear ramp between events
    assert abs(w[0]) < 0.1 and abs(w[-1]) < 0.1


def test_bar_stroke_interpolates_across_bars():
    ws = wavetable.WaveState()
    ws.begin_bar_stroke()
    ws.stroke_magnitude(8, 0.8)
    ws.stroke_magnitude(24, 0.8)                  # fast sweep skips bars
    ws.end_bar_stroke()
    mags = ws.magnitudes()
    assert all(m > 0.05 for m in mags[7:24])


def test_wave_screen_has_no_buttons(ui):
    _open_wave(ui)
    assert not find_hit(ui, "wave_clear") and not find_hit(ui, "wave_undo")


# ------------------------------------------- faces discoloration regression

def test_glow_dot_is_premultiplied():
    pygame.display.init()
    pygame.display.set_mode((1, 1))
    g = UI.glow_dot(100, (86, 24, 98))
    edge = g.get_at((3, 100))                     # near the outer rim
    assert edge.r < 10 and edge.g < 10 and edge.b < 10
    center = g.get_at((100, 100))
    assert center.r > 25                          # falloff, not a flat disc


def test_sleep_faces_identical_before_and_after_wave_visit(ui):
    ui.load_faces()
    ui.enter_sleep()
    ui.render()
    before = pygame.image.tobytes(ui.canvas, "RGB")
    ui.wake_up()
    wr, br = _open_wave(ui)
    ui.touch_down(wr.x + 20, wr.centery)          # draw a bit
    ui.touch_move(wr.x + 260, wr.centery - 80)
    ui.touch_up(wr.x + 260, wr.centery - 80)
    ui.touch_down(br.centerx, br.centery)         # sculpt a bar
    ui.touch_up(br.centerx, br.centery)
    ui._stroke_trail = []
    ui.companion.active = False
    ui.enter_sleep()
    ui.render()
    after = pygame.image.tobytes(ui.canvas, "RGB")
    assert before == after


# -------------------------------------------------- raw evdev touch decoder

import struct

import touch_evdev


def _ev(etype, code, value):
    return struct.pack(touch_evdev.EV_FMT, 0, 0, etype, code, value)


def _frame(*evs):
    return b"".join(evs) + _ev(touch_evdev.EV_SYN, touch_evdev.SYN_REPORT, 0)


def test_decoder_swipe_sequence_from_bytes():
    d = touch_evdev.Decoder()
    stream = _frame(_ev(touch_evdev.EV_KEY, touch_evdev.BTN_TOUCH, 1),
                    _ev(touch_evdev.EV_ABS, touch_evdev.ABS_X, 1000),
                    _ev(touch_evdev.EV_ABS, touch_evdev.ABS_Y, 2000))
    for x in range(1100, 3100, 100):
        stream += _frame(_ev(touch_evdev.EV_ABS, touch_evdev.ABS_X, x))
    stream += _frame(_ev(touch_evdev.EV_KEY, touch_evdev.BTN_TOUCH, 0))
    d.feed_bytes(stream)
    evs = d.take()
    assert evs[0] == ("down", 1000, 2000)
    assert evs[-1][0] == "up"
    moves = [e for e in evs if e[0] == "move"]
    assert len(moves) == 20
    assert moves[-1][1] == 3000                   # X tracked the finger


def test_decoder_pressure_only_frames_emit_nothing():
    d = touch_evdev.Decoder()
    d.feed_bytes(_frame(_ev(touch_evdev.EV_KEY, touch_evdev.BTN_TOUCH, 1),
                        _ev(touch_evdev.EV_ABS, touch_evdev.ABS_X, 500),
                        _ev(touch_evdev.EV_ABS, touch_evdev.ABS_Y, 500)))
    d.take()
    d.feed_bytes(_frame(_ev(touch_evdev.EV_ABS, touch_evdev.ABS_PRESSURE, 90)))
    assert d.take() == []                         # no position change, no move


def test_coalesce_keeps_downs_ups_collapses_moves():
    evs = [("down", 1, 1), ("move", 2, 2), ("move", 3, 3), ("move", 4, 4),
           ("up", 4, 4), ("down", 9, 9), ("move", 10, 10), ("move", 11, 11)]
    assert touch_evdev.coalesce(evs) == [
        ("down", 1, 1), ("move", 4, 4), ("up", 4, 4),
        ("down", 9, 9), ("move", 11, 11)]


def test_mapper_raw_uses_cal_directly():
    pygame.display.init()
    pygame.display.set_mode((1, 1))
    m = UI.TouchMapper(0, (480, 800))
    m.cal = UI.TouchCal(0, 100, 4000, 100, 4000)
    x, y = m.raw(2050, 100, {})                   # mid-X, top-Y raw
    assert abs(x - UI.W // 2) < 12 and y < 8
    m.cal = None                                  # no cal: device ranges
    x2, y2 = m.raw(2048, 4095,
                   {touch_evdev.ABS_X: (0, 4095), touch_evdev.ABS_Y: (0, 4095)})
    assert abs(x2 - UI.W // 2) < 12 and y2 == UI.H - 1
    pygame.display.quit()


# ----------------------------------------------------------------- EngineOut

class _FakePort:
    def __init__(self):
        self.msgs = []

    def send(self, msg):
        self.msgs.append(msg)

    def close(self):
        pass


def test_engineout_sends_cc_and_sync():
    eo = companion_input.EngineOut()
    eo._port = _FakePort()
    assert eo.tap(70)
    assert eo.rotate(20, True) and eo.rotate(20, False)
    assert eo.sync_request()
    m = eo._port.msgs
    assert (m[0].control, m[0].value, m[0].channel) == (70, 127, 0)
    assert (m[1].control, m[1].value) == (70, 0)
    assert (m[2].value, m[3].value) == (127, 0)
    assert (m[4].control, m[4].channel, m[4].value) == (102, 15, 127)


def test_engineout_resolves_through_pool_only():
    eo = companion_input.EngineOut()
    assert eo._resolve(["Teensy MIDI 1", "Midi Through Port-0"]) \
        == "Midi Through Port-0"
    assert eo._resolve(["Teensy MIDI 1"]) is None
    eo2 = companion_input.EngineOut(port_name="IAC")
    assert eo2._resolve(["IAC Driver Bus 1"]) == "IAC Driver Bus 1"


def test_smoother_ema_deadband_and_stale_release():
    s = touch_evdev.Smoother(alpha=0.5, min_px=2)
    assert s.apply("down", 100, 100) == ("down", 100, 100)
    assert s.apply("move", 100, 101) is None      # micro-jitter dropped
    k = s.apply("move", 140, 100)                 # EMA halves the jump
    assert k[0] == "move" and 115 <= k[1] <= 125
    last = s.apply("move", 180, 100)
    up = s.apply("up", 900, 900)                  # stale release coords
    assert up == ("up", last[1], last[2])         # trusts the smoothed track


def test_sync_catchup_states_never_auto_navigate(ui):
    import time as _t
    ui.sync_quiet_until = _t.time() + 2.0
    ui.handle_state_change("les", 0)              # ON echo during sync
    assert ui.current_view == "menu0"             # no teleport
    assert ui.bypass_states["les"] == 0           # state still recorded
    ui.sync_quiet_until = 0.0
    ui.handle_state_change("vib", 0)              # a REAL toggle after sync
    import neptrPhase4_UI as _ui


def test_gap_bridger_bridges_contact_bounce():
    b = touch_evdev.GapBridger(grace=0.07)
    assert b.apply("down", 500, 570, 10.00) == [("down", 500, 570)]
    assert b.apply("move", 470, 568, 10.02) == [("move", 470, 568)]
    assert b.apply("up", 470, 568, 10.03) == []   # held
    # pressure dip: re-press 30ms later mid-swipe -> bridged into a move
    assert b.apply("down", 430, 566, 10.06) == [("move", 430, 566)]
    assert b.apply("move", 380, 564, 10.08) == [("move", 380, 564)]
    assert b.apply("up", 380, 564, 10.09) == []
    assert b.flush(10.12) == []                   # grace not yet over
    assert b.flush(10.17) == [("up", 380, 564)]   # real release
    assert b.flush(10.30) == []


# ------------------------------------------------------ expression slider

def test_expr_slider_present_on_all_menu_views(ui):
    for m in (1, 0, UI.LOOPER_MENU):
        ui.handle_menu_change(m)
        ui.dirty = True
        ui.render()
        assert ui._expr_rect is not None
        assert ui._expr_rect.bottom <= UI.CONTENT_BOTTOM


def test_expr_drag_sends_absolute_cc29(ui):
    ui.handle_menu_change(1)
    ui.render()
    tr = ui._expr_rect
    ui.touch_down(tr.centerx, tr.centery)         # tap = jump to position
    assert ui.drag_target == 'expr'
    assert ui.engine.sent[-1][0] == "cc" and ui.engine.sent[-1][1] == 29
    assert abs(ui.engine.sent[-1][2] - 64) <= 2
    ui.touch_up(tr.right - 1, tr.centery)         # ride to toe, final send
    assert ui.engine.sent[-1] == ("cc", 29, 127)
    assert ui.drag_target is None
    ui.touch_down(tr.x + 1, tr.centery)           # heel
    ui.touch_up(tr.x + 1, tr.centery)
    assert ui.engine.sent[-1] == ("cc", 29, 0)


def test_expr_drag_never_pages_menus(ui):
    ui.handle_menu_change(2)
    ui.render()
    tr = ui._expr_rect
    ui.touch_down(tr.x + 40, tr.centery)
    ui.touch_move(tr.x + 40 + UI.SWIPE_PX + 30, tr.centery)
    ui.touch_up(tr.x + 40 + UI.SWIPE_PX + 30, tr.centery)
    kinds = {m[0] for m in ui.engine.sent}
    assert kinds == {"cc"}                        # only pedal values, no CCs
    assert ui.current_view == "menu2"


# --------------------------------------------------------- late-join sync
# The engine answers ch16 CC102 with a BURST covering every state/param
# address. "Synced" has to mean that burst arrived — the old "any OSC message
# ends the retries" rule was satisfied by a single unrelated broadcast (clock
# telemetry, a looper position), and by a cold-boot engine that came up long
# after the UI had spent its 3 requests.

def test_one_stray_message_is_not_a_snapshot(ui):
    UI.route_osc(ui, "/cb_bpm", 120.0)
    assert ui.osc_seen is True
    assert ui.synced is False           # retries must keep going


def test_a_full_burst_marks_the_ui_synced(ui):
    for i, addr in enumerate(("/menu", "/last_toggle", "/dly_state",
                              "/rev_state", "/les_state", "/vib_state",
                              "/pre_state", "/tgl_state")):
        assert ui.synced is (i >= UI.SYNC_MIN_ADDRS)
        UI.route_osc(ui, addr, 0)
    assert ui.synced is True
    assert ui.sync_retry_at is None     # and the retries are called off


def test_repeats_of_one_address_never_count_as_a_snapshot(ui):
    for _ in range(50):
        UI.route_osc(ui, "/cb_bpm", 120.0)
    assert ui.synced is False


def test_sync_retry_backs_off_and_keeps_asking(ui):
    """A cold-boot engine can be minutes late; the request interval grows but
    never stops before SYNC_RETRY_SECONDS_TOTAL."""
    dt = UI.SYNC_RETRY_SECS
    seen = []
    for _ in range(12):
        dt = min(dt * UI.SYNC_RETRY_BACKOFF, UI.SYNC_RETRY_MAX)
        seen.append(dt)
    assert seen[-1] == UI.SYNC_RETRY_MAX
    assert sum(seen) < UI.SYNC_RETRY_SECONDS_TOTAL


def test_engineout_open_failure_is_retried_not_latched():
    """The UI can be up before the ALSA Through pool exists. A failed open
    must not disable the engine link (and with it the CC102 sync) forever."""
    clock = {"t": 0.0}
    eo = companion_input.EngineOut(clock=lambda: clock["t"])
    attempts = []

    def _open():
        attempts.append(clock["t"])
        raise RuntimeError("no Through-pool MIDI output found")
    eo._open = _open
    assert eo.cc(1, 1) is False
    assert eo.cc(1, 1) is False         # inside the backoff: no new attempt
    assert len(attempts) == 1
    clock["t"] = 100.0
    assert eo.cc(1, 1) is False
    assert len(attempts) == 2           # tried again once the backoff lapsed


def test_engineout_recovers_when_the_port_appears():
    clock = {"t": 0.0}
    eo = companion_input.EngineOut(clock=lambda: clock["t"])
    eo._open = lambda: (_ for _ in ()).throw(RuntimeError("no port"))
    assert eo.sync_request() is False
    port = _FakePort()
    eo._port = port                     # pool showed up
    assert eo.sync_request() is True
    assert port.msgs[0].control == companion_input.CC_UI_SYNC


# ------------------------------------------------------- position restore

def _effect_on_a_menu(ui):
    """First (effect code, menu index) pair that actually appears on a menu —
    EFFECT_PARAMS also holds codes that have no slot of their own."""
    for menu in range(len(UI.MENUS)):
        for code, _name in ui._menu_effects(menu):
            if code in UI.EFFECT_PARAMS:
                return code, menu
    raise AssertionError("no effect with params is reachable from a menu")


def test_restore_menu_view(ui):
    ui.restore_position({"view": "menu5", "menu": 5, "companion": 0,
                         "companion_screen": 0, "companion_sel": 0,
                         "scroll_patches": -1, "scroll_audio": -1})
    assert ui.current_view == "menu5" and ui.last_menu_index == 5


def test_restore_out_of_range_menu_clamps(ui):
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view="menu999", menu=999))
    assert ui.current_view == f"menu{len(UI.MENUS) - 1}"


def test_restore_unknown_view_falls_back_to_menu0(ui):
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view="not-a-view"))
    assert ui.current_view == "menu0"


def test_restored_effect_view_waits_for_the_engine(ui):
    """An effect view is UI-owned position, but whether that effect is ON is
    the ENGINE's fact. The view is only re-entered once the engine confirms
    both the menu and the on-state — never from the persisted file alone."""
    code, menu = _effect_on_a_menu(ui)
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view=code, menu=menu))
    assert ui.current_view == f"menu{menu}"      # menu view for now
    assert ui.restore_effect == code
    ui.handle_state_change(code, 0)             # engine: it is ON
    UI.route_osc(ui, "/menu", menu)             # engine: this menu is current
    assert ui.current_view == code
    assert ui.restore_effect is None


def test_restored_effect_view_is_dropped_if_the_menu_moved(ui):
    code, menu = _effect_on_a_menu(ui)
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view=code, menu=menu))
    other = (menu + 1) % len(UI.MENUS)
    UI.route_osc(ui, "/menu", other)
    assert ui.restore_effect is None
    assert ui.current_view == f"menu{other}"


def test_snapshot_holds_position_only(ui):
    ui.bypass_states["dly"] = 0
    ui.param_values["/dly_time"] = 0.5
    snap = ui.state_snapshot()
    assert set(snap) == set(UI.ui_state.DEFAULTS)
    assert "0.5" not in str(snap)


def test_snapshot_keeps_remembering_a_pending_effect_view(ui):
    code, menu = _effect_on_a_menu(ui)
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view=code, menu=menu))
    # engine has not answered yet: the file must still say "the effect view",
    # otherwise a restart during the sync window silently forgets it.
    assert ui.state_snapshot()["view"] == code


def test_snapshot_roundtrips_through_the_file(ui, tmp_path):
    path = str(tmp_path / "ui-state.conf")
    ui.restore_position(dict(UI.ui_state.DEFAULTS, view="menu4", menu=4))
    UI.ui_state.save(ui.state_snapshot(), path)
    ui.current_view = "menu0"
    ui.restore_position(UI.ui_state.load(path))
    assert ui.current_view == "menu4"


def test_traffic_after_a_long_silence_rearms_the_sync(ui):
    """An engine restart is announced only by a gap in OSC. The re-sync is
    DEFERRED to the next loop pass, so the message that revealed the restart
    is still handled as a normal, navigable event."""
    for addr in ("/menu", "/last_toggle", "/dly_state", "/rev_state",
                 "/les_state", "/vib_state", "/pre_state", "/tgl_state"):
        UI.route_osc(ui, addr, 0)
    assert ui.synced is True and ui.resync_pending is False
    ui.last_osc_time = time.time() - (UI.RESYNC_AFTER_SILENCE + 5)
    UI.route_osc(ui, "/menu", 1)
    assert ui.resync_pending is True
    assert ui.synced is True            # not torn down inline ...
    assert ui.sync_quiet_until <= time.time()   # ... and no quiet window yet


def test_normal_traffic_does_not_rearm_the_sync(ui):
    for addr in ("/menu", "/last_toggle", "/dly_state", "/rev_state",
                 "/les_state", "/vib_state", "/pre_state", "/tgl_state"):
        UI.route_osc(ui, addr, 0)
    ui.last_osc_time = time.time()
    UI.route_osc(ui, "/menu", 1)
    assert ui.resync_pending is False
