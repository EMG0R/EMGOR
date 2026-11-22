"""MIXER page (menu 22): UI table mirrors the engine, dispatch wiring, render, echo."""
import os
import re

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import neptrPhase4_UI as UI

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSD = os.path.join(ROOT, "csound")
DISP = os.path.join(CSD, "handler", "dispatch")


def _read(*p):
    with open(os.path.join(*p)) as fh:
        return fh.read()


class FakeEngine:
    def __init__(self):
        self.sent = []

    def tap(self, control):
        self.sent.append(("tap", control))
        return True

    def cc(self, control, value, channel=0):
        self.sent.append(("cc", control, value, channel))
        return True

    def __getattr__(self, _n):
        return lambda *a, **k: True


@pytest.fixture
def ui():
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    u = UI.NeptrUI()
    u.engine = FakeEngine()
    yield u
    pygame.display.quit()


def test_menu_ids_and_cycle():
    assert UI.MIXER_MENU == 22 and UI.HG_MENU == 23
    assert re.search(r"#define\s+MX_MENU\s+#22#", _read(CSD, "mixer_page.orc"))
    assert re.search(r"#define\s+HG_MENU\s+#23#", _read(CSD, "hyperguitar.orc"))


def test_ui_channels_exist_in_engine():
    src = _read(CSD, "mixer_page.orc") + _read(CSD, "effects", "vocal_chain.orc")
    for _n, _i, vkey, pa, pb in UI.MX_CHANNELS:
        for key in (vkey, pa[1], pb[1]):
            assert re.search(r'chn_k "%s"' % key, src), key
    # defaults: guitar/vocal/PMOR unity, trumpet 0; nothing above unity
    for k, v in UI.MX_VOL_DEFAULTS.items():
        assert re.search(r'chn_k "%s",\s+3, 2, %s,' % (k, ("%g" % v)), src), k
        assert v <= 1.0


def test_dispatch_branches_present():
    # slot 4..1 rotation + button -> channels 0..3; slot 0 is an explicit no-op
    for slot, cc, ch in ((4, 22, 0), (3, 21, 1), (2, 26, 2), (1, 25, 3)):
        assert "$MX_VOL_ROT(%d)" % ch in _read(DISP, "rot_slot%d_cc%d.orc" % (slot, cc))
        assert "$MX_SELECT(%d)" % ch in _read(DISP, "toggle_slot%d.orc" % slot)
    assert "$MX_MENU" in _read(DISP, "rot_slot0_cc24.orc")
    assert "$MX_A_ROT" in _read(DISP, "rot_p2_cc20.orc")
    assert "$MX_B_ROT" in _read(DISP, "rot_p3_cc23.orc")
    # slot_button_cc rows 0..3 are those same slots
    assert [UI._ROW_BTN_LOGICAL[r] for r in range(4)] == [72, 71, 76, 75]


def test_vocal_secondaries_are_voc_channels():
    src = _read(CSD, "mixer_page.orc")
    assert "'voc_dly_wet'" in src and "'voc_rev_wet'" in src and "'voc_level'" in src


def test_state_slots_lockstep():
    ch = ["mx_gtr_vol", "mx_gtr_pan", "mx_gtr_wid", "mx_tpt_vol", "mx_tpt_pan",
          "mx_tpt_wid", "mx_pmr_vol", "mx_pmr_pan", "mx_pmr_wid"]
    wr0 = _read(CSD, "state_persist.orc")
    assert 'chnget "mx_lp_vol"' in wr0 and "tab:k(341, giNeptrState)" in wr0
    wr = _read(CSD, "state_persist.orc")
    un = _read(CSD, "state_unpack.inc.orc")
    sv = _read(CSD, "state_preset_exclude_save.inc.orc")
    rs = _read(CSD, "state_preset_exclude_restore.inc.orc")
    for i, c in enumerate(ch):
        n = 331 + i
        assert re.search(r'chnget "%s"' % c, wr) and ("tab:k(%d, giNeptrState)" % n) in wr
        assert ('chnset kStM%d - 100, "%s"' % (i, c)) in un
        assert c in sv and c in rs


def test_pan_and_width_fmt():
    assert UI.mx_fmt('pan', 0) == "C"
    assert UI.mx_fmt('pan', -0.3) == "L30" and UI.mx_fmt('pan', 1) == "R100"
    assert UI.mx_fmt('wid', 1) == "100%"


def test_mixer_page_renders_and_taps_select(ui):
    ui.current_view = f'menu{UI.MIXER_MENU}'
    ui._last_rendered_view = ui._last_rendered_signature = None
    assert ui.render() is True
    sel = sorted(a[1] for _r, a in ui.hit if a[0] == 'mx_sel')
    assert sel == [0, 1, 2, 3]
    assert not any(a[0] == 'slot' for _r, a in ui.hit)
    ui._fire(('mx_sel', 2), 0.0)
    assert ui.engine.sent[-1] == ("tap", UI.slot_button_cc(UI.MIXER_MENU, 2))


def test_echo_updates_state_and_signature(ui):
    ui.current_view = f'menu{UI.MIXER_MENU}'
    sig0 = ui._signature(ui.current_view)
    UI.route_osc(ui, '/mx_sel', 2)
    UI.route_osc(ui, '/mx_gtr_vol', 0.5)
    UI.route_osc(ui, '/voc_dly_wet', 0.7)       # shared with the VOCAL page
    assert ui.mx_sel == 2
    assert ui.mx_value("mx_gtr_vol") == 0.5 and ui.mx_value("voc_dly_wet") == 0.7
    assert ui._signature(ui.current_view) != sig0
    ui.render()


def test_vocal_page_no_longer_duplicates_mixer_controls(ui):
    ui.current_view = f'menu{UI.VOCAL_MENU}'
    ui._last_rendered_view = ui._last_rendered_signature = None
    assert ui.render() is True
    drawn = {UI.VOC_PARAMS[vi][0] for _r, vi in ui._voc_rects}
    assert not (set(UI.MX_OWNED_VOC) & drawn)
    assert UI.MX_OWNED_VOC == ("voc_level",)
    assert {"voc_dly_wet", "voc_rev_wet", "voc_hpf_hz"} <= drawn


def test_looper_row_and_vocal_meter(ui):
    assert UI.MX_LOOPER_ROW[2] == "mx_lp_vol" and UI.MX_VOL_DEFAULTS["mx_lp_vol"] == 0.85
    assert UI.MX_VOL_MAX_BY_KEY["voc_level"] == 4.0
    UI.route_osc(ui, '/mx_lp_vol', 0.4)
    assert ui.mx["mx_lp_vol"] == 0.4
    UI.route_osc(ui, '/voc_meter', [0.0, 0.0, 1.0, 0.5])
    assert ui.voc_out_db == 0.0 and ui.voc_in_db == -90.0
    ln, g, st = UI.voc_meter_text(-90.0, 0.0, 1.5)
    assert "no signal" in ln and "CLIP" in ln and st == 'clip'
    assert g == "PRE +0  LEVEL 1.50 x7 = +20.4 dB total"
    assert UI.voc_meter_text(-12.0, -3.0, 4.0)[1] == "PRE +0  LEVEL 4.00 x7 = +28.9 dB total"
    assert UI.voc_meter_text(-12.0, -3.0, 1.5, 24)[1] == "PRE +24  LEVEL 1.50 x7 = +44.4 dB total"
    for v in (f'menu{UI.MIXER_MENU}', f'menu{UI.VOCAL_MENU}'):
        ui.current_view = v
        ui._last_rendered_view = ui._last_rendered_signature = None
        assert ui.render() is True
    # looper master: slot 0 rotation + engine channel
    assert "mx_lp_vol" in _read(DISP, "rot_slot0_cc24.orc")
