"""NAM/IR variant-list overlay tests (2026-08-21).

Menu 17 (NAM) and 18 (IR VERB) each expose 5 physical slots, but every slot's
row actually has up to 5 selectable variants, reachable in hardware only by
turning that row's encoder (see rot_slot{0,1,2,3,4}_cc{24,25,26,21,22}.orc).
The effect view for a nam<r>/ir<r> slot now shows a BROWSE affordance
(neptrPhase4_UI.py _render_effect) that opens a full-screen scrollable list
of that row's variants (self.variant_list + _render_variant_list); tapping
one drives the SAME relative-rotation CC the physical encoder uses
(slot_rotate_cc + _variant_pick), and the highlighted/selected item follows
the engine's own /nam_sel<r>·/ir_sel<r> echo, matching how hardware-driven
changes are reflected everywhere else in this UI (engine-authoritative, no
local flicker).

This implementation was pulled from the Pi's live, already-deployed copy
(neptr/SYNC.md's documented pull direction) rather than written independently
here — it predates this test file. NAM_ROWS/IR_ROWS come from scanning
NEPTR_NAM_DIR/NEPTR_IR_DIR at import time (empty on a dev machine without
profile files), so these tests monkeypatch the module dicts directly instead
of fighting import order / real files.
"""

import os
import time

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import neptrPhase4_UI as UI


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
        self.sent.append(("cc", control, value))
        return True

    def sync_request(self):
        self.sent.append(("sync",))
        return True

    def close(self):
        pass


def _fake_rows():
    return {r: [f"R{r}V{v}" for v in range(5)] for r in range(5)}


@pytest.fixture
def ui(monkeypatch):
    monkeypatch.setattr(UI, "NAM_ROWS", _fake_rows())
    monkeypatch.setattr(UI, "IR_ROWS", _fake_rows())
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    u = UI.NeptrUI()
    u.engine = FakeEngine()
    yield u
    pygame.display.quit()


def wait_for_thread(u, timeout=0.5):
    """_variant_pick fires its rotate burst (<=4 pulses, 4ms apart) on a
    daemon thread — give it a moment to land before asserting on
    ui.engine.sent."""
    time.sleep(min(timeout, 0.2))


# --------------------------------------------------------------- row/CC mapping

def test_nam_row_cc_matches_state_row_order():
    # NAM's UI rows are drawn in state-row order (nam0..nam4 straight
    # through), so _ui_row_for_state_row is the identity for NAM.
    for r in range(5):
        assert UI._ui_row_for_state_row(UI.NAM_MENU, r) == r


def test_ir_row_cc_uses_reversed_menu_order():
    # MENUS lists IR VERB as ir4..ir0 (reversed) — state row 0 (SPRING) is
    # drawn in UI slot 4, state row 4 (WOOD RM) in UI slot 0.
    assert UI._ui_row_for_state_row(UI.IR_MENU, 0) == 4
    assert UI._ui_row_for_state_row(UI.IR_MENU, 4) == 0
    assert UI._ui_row_for_state_row(UI.IR_MENU, 2) == 2   # HALL sits in the middle


def test_slot_rotate_cc_is_a_real_physical_cc():
    for row in range(5):
        cc = UI.slot_rotate_cc(UI.NAM_MENU, row)
        assert cc in UI._PHYS_FOR_ROT.values()


# --------------------------------------------------------------- BROWSE affordance

def test_effect_view_shows_browse_button_when_variants_exist(ui):
    ui.bypass_states['nam2'] = 0          # must be ON for enter_effect_view
    ui.enter_effect_view('nam2')
    assert ui.current_view == 'nam2'
    ui.render()
    assert any(a[0] == 'vlist_open' for _r, a in ui.hit)
    _, menu_idx, row = next(a for _r, a in ui.hit if a[0] == 'vlist_open')
    assert menu_idx == UI.NAM_MENU and row == 2


def test_effect_view_no_browse_button_without_scanned_rows(ui, monkeypatch):
    monkeypatch.setattr(UI, "NAM_ROWS", {})
    ui.bypass_states['nam2'] = 0
    ui.enter_effect_view('nam2')
    ui.render()
    assert not any(a[0] == 'vlist_open' for _r, a in ui.hit)


def test_non_nam_ir_effect_view_has_no_browse_button(ui):
    # sanity: an ordinary effect (not nam*/ir*) never gets a BROWSE row
    code = next(c for c in UI.EFFECT_PARAMS
               if not (c.startswith('nam') and c[3:].isdigit())
               and not (c.startswith('ir') and c[2:].isdigit()))
    ui.bypass_states[code] = 0
    ui.enter_effect_view(code)
    assert ui.current_view == code
    ui.render()
    assert not any(a[0] == 'vlist_open' for _r, a in ui.hit)


# --------------------------------------------------------------- opening/closing

def test_vlist_open_sets_state_and_clears_scroll(ui):
    ui._fire(('vlist_open', UI.NAM_MENU, 3), time.time())
    assert ui.variant_list == {'menu': UI.NAM_MENU, 'row': 3}
    assert 'vlist' not in ui.list_scroll


def test_vlist_close_clears_state(ui):
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 3}
    ui._fire(('vlist_close',), time.time())
    assert ui.variant_list is None


def test_variant_list_render_owns_the_whole_screen(ui):
    # Contract (per _render_variant_list's docstring): the CALLER clears
    # self.hit first (render() does this for every modal), so a stale hit
    # from behind the overlay can never be tapped through it.
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    ui.hit = []
    ui._render_variant_list(ui.canvas)
    assert any(a[0] == 'vlist_pick' for _r, a in ui.hit)
    assert any(a[0] == 'vlist_close' for _r, a in ui.hit)


def test_variant_list_highlights_current_selection(ui):
    ui.nam_sel[1] = 3
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 1}
    ui.hit = []
    ui._render_variant_list(ui.canvas)
    picks = {a[1]: r for r, a in ui.hit if a[0] == 'vlist_pick'}
    assert 3 in picks


def test_render_dispatches_to_variant_list_when_open(ui):
    ui.current_view = 'nam0'
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    ui._last_rendered_view = None
    ui._last_rendered_signature = None
    assert ui.render() is True
    assert any(a[0] == 'vlist_pick' for _r, a in ui.hit)


# --------------------------------------------------------------- picking a variant

def test_variant_pick_sends_shorter_wrap_direction(ui):
    ui.nam_sel[0] = 0
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    cc = UI.slot_rotate_cc(UI.NAM_MENU, UI._ui_row_for_state_row(UI.NAM_MENU, 0))
    ui._variant_pick(3, time.time())
    wait_for_thread(ui)
    rotates = [s for s in ui.engine.sent if s[0] == 'rotate']
    # 0 -> 3 over 5 variants: forward=3, backward=2 -> shorter is backward (down)
    assert len(rotates) == 2
    assert all(r[1] == cc and r[2] is False for r in rotates)


def test_variant_pick_forward_when_shorter(ui):
    ui.nam_sel[1] = 0
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 1}
    cc = UI.slot_rotate_cc(UI.NAM_MENU, UI._ui_row_for_state_row(UI.NAM_MENU, 1))
    ui._variant_pick(1, time.time())
    wait_for_thread(ui)
    rotates = [s for s in ui.engine.sent if s[0] == 'rotate']
    assert len(rotates) == 1
    assert rotates[0] == ('rotate', cc, True)


def test_variant_pick_current_selection_is_noop(ui):
    ui.nam_sel[2] = 2
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 2}
    ui._variant_pick(2, time.time())
    wait_for_thread(ui, timeout=0.2)
    assert ui.engine.sent == []


def test_variant_pick_ir_uses_reversed_row_cc(ui):
    ui.ir_sel[0] = 0
    ui.variant_list = {'menu': UI.IR_MENU, 'row': 0}
    cc = UI.slot_rotate_cc(UI.IR_MENU, UI._ui_row_for_state_row(UI.IR_MENU, 0))
    ui._variant_pick(1, time.time())
    wait_for_thread(ui)
    rotates = [s for s in ui.engine.sent if s[0] == 'rotate']
    assert rotates and rotates[0][1] == cc


def test_tap_a_variant_row_end_to_end(ui):
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    ui.nam_sel[0] = 0
    ui.hit = []
    ui._render_variant_list(ui.canvas)
    rect, action = next((r, a) for r, a in ui.hit
                        if a[0] == 'vlist_pick' and a[1] != ui.nam_sel[0])
    x, y = rect.center
    ui.touch_down(x, y)
    ui.touch_up(x, y)
    wait_for_thread(ui)
    assert any(s[0] == 'rotate' for s in ui.engine.sent)


# --------------------------------------------------------------- hardware -> UI sync

def test_osc_echo_updates_selection_while_variant_list_open(ui):
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 4}
    UI.route_osc(ui, '/nam_sel4', 2)
    assert ui.nam_sel[4] == 2


def test_osc_echo_still_updates_when_only_menu_view_showing(ui):
    ui.current_view = f'menu{UI.NAM_MENU}'
    UI.route_osc(ui, '/nam_sel0', 3)
    assert ui.nam_sel[0] == 3


def test_ir_osc_echo_updates_selection(ui):
    ui.variant_list = {'menu': UI.IR_MENU, 'row': 2}
    UI.route_osc(ui, '/ir_sel2', 4)
    assert ui.ir_sel[2] == 4


def test_variant_list_reflects_osc_echo_without_reopening(ui):
    """The requirement's other half: hardware picks a variant -> the open
    browse list's highlight follows it, without the user having to close
    and reopen the list."""
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    UI.route_osc(ui, '/nam_sel0', 4)
    assert ui.nam_sel[0] == 4
    ui.hit = []
    ui._render_variant_list(ui.canvas)
    # _render_variant_list computes `sel` fresh from ui.nam_sel each call —
    # the highlighted row (drawn "lit") is whichever index equals ui.nam_sel,
    # so it tracks the OSC-confirmed selection with no manual re-sync step.
    assert any(a[0] == 'vlist_pick' and a[1] == 4 for _r, a in ui.hit)


# --------------------------------------------------------------- touch scroll isolation

def test_variant_list_drag_scrolls_its_own_key_not_patcher(ui):
    ui.variant_list = {'menu': UI.NAM_MENU, 'row': 0}
    ui.render()
    x, y = UI.W // 2, UI.CONTENT_TOP + 40
    ui.touch_down(x, y)
    assert ui.drag_target == ('gscroll', 'vlist')
    ui.touch_move(x, y - UI.SCROLL_STEP * 2)
    assert 'patcher' not in ui.list_scroll or ui.list_scroll.get('patcher', 0) == 0
