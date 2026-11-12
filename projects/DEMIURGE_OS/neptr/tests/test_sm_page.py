"""SoundMagic spectral rack page (menu 2) -- registry, layout, sheet, OSC."""
import os

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import neptrPhase4_UI as UI
import sm_page


class FakeEngine:
    def __init__(self):
        self.sent = []

    def press(self, c): self.sent.append(("press", c)); return True
    def release(self, c): self.sent.append(("release", c)); return True
    def tap(self, c): self.press(c); self.release(c); return True
    def rotate(self, c, up): self.sent.append(("rotate", c, bool(up))); return True
    def cc(self, c, v, channel=0): self.sent.append(("cc", c, v, channel)); return True
    def sync_request(self): self.sent.append(("sync",)); return True
    def close(self): pass


@pytest.fixture
def ui():
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    u = UI.NeptrUI()
    u.engine = FakeEngine()
    u.osc = []
    u.sm.sender = lambda a, *args: u.osc.append((a,) + args)
    UI.route_osc(u, "/menu", UI.NORRIS_MENU)
    u._last_rendered_view = u._last_rendered_signature = None
    yield u
    pygame.display.quit()


def test_registry_counts():
    r = sm_page.Registry()
    assert len(r.effects) == 23
    assert r.n_params == 309
    assert len(r.spectral) == 19


def test_heroes_and_pedal_valid():
    r = sm_page.Registry()
    for e in r.effects:
        n = len(e["params"])
        assert 1 <= len(e["heroes"]) <= 3, e["code"]
        assert all(0 <= h < n for h in e["heroes"]), e["code"]
        assert 0 <= e["pedal"] < n, e["code"]


def test_norris_is_menu_20_and_spectral_is_plain():
    assert UI.NORRIS_MENU == 20
    assert len(UI.MENUS[UI.NORRIS_MENU]["effects"]) == 5
    assert UI.MENUS[2]["name"] == "SPECTRAL"
    assert not hasattr(UI, "SPECTRAL_MENU")
    assert UI.HG_MENU == 21


def test_page_renders_tiles_without_legacy_slots(ui):
    assert ui.render() is True
    assert not [a for _r, a in ui.hit if a[0] == 'slot']
    tiles = [a[1] for _r, a in ui.hit if a[0] == 'sm_tile']
    assert tiles[:19] == list(range(19)) or set(range(19)) <= set(tiles)
    assert 'rack' in tiles


def test_tile_hit_test_maps_rect_to_fx(ui):
    lay = ui.sm.layout()
    assert len(lay["tiles"]) == 20
    for rect, tid in lay["tiles"]:
        assert ui.sm.tile_at(rect.centerx, rect.centery) == tid
    assert [t for _r, t in lay["tiles"]][-1] == 'rack'


def test_tap_tile_toggles_and_focuses(ui):
    ui.render()
    rect, tid = next(t for t in ui.sm.layout()["tiles"] if t[1] == 7)
    ui.touch_down(rect.centerx, rect.centery)
    ui.touch_up(rect.centerx, rect.centery)
    assert ("/st", 7, 1) in ui.osc
    assert ui.sm.focus == 7
    assert ui.sm.on[7] == 0          # engine-authoritative: no local flip


def test_long_press_opens_sheet_and_swallows_tap(ui):
    ui.render()
    rect, _ = next(t for t in ui.sm.layout()["tiles"] if t[1] == 3)
    ui.touch_down(rect.centerx, rect.centery)
    t0 = ui.sm.lp[1]
    assert ui.sm.lp_deadline() == pytest.approx(t0 + 0.45)
    assert ui.sm.tick(t0 + 0.5, True, False)
    ui.touch_up(rect.centerx, rect.centery)
    assert ui.sm.sheet == ('fx', 3)
    assert not any(m[0] == "/st" for m in ui.osc)


def test_deep_sheet_rows_equal_params_for_every_effect(ui):
    r = ui.sm.reg
    for e in r.effects:
        ui.sm.open_sheet(e["fx"])
        assert len(ui.sm._rows) == len(e["params"]), e["code"]
    ui.sm.close_sheet()


def test_sheet_renders_every_effect(ui):
    for e in ui.sm.reg.effects:
        ui.sm.open_sheet(e["fx"])
        assert ui.render()
    ui.sm.close_sheet()


def test_sheet_fader_sends_absolute_sp(ui):
    ui.sm.open_sheet(0)
    ui.render()
    g = ui.sm.row_geom(ui.sm._rows[0])
    tr = g['track']
    ui.touch_down(tr.right - 1, tr.centery)
    ui.touch_up(tr.right - 1, tr.centery)
    sp = [m for m in ui.osc if m[0] == "/sp"]
    assert sp and sp[-1][1:3] == (0, 0) and sp[-1][3] == pytest.approx(1000.0)


def test_sheet_bool_enum_and_chip(ui):
    ui.sm.open_sheet(0)
    rows = {r['p']: r for r in ui.sm._rows}
    ui.render()
    g = ui.sm.row_geom(rows[4])['row']          # Randomize phases (bool, default 1)
    ui.touch_down(g.centerx, g.centery); ui.touch_up(g.centerx, g.centery)
    assert ("/sp", 0, 4, 0.0) in ui.osc
    chip = ui.sm.row_geom(rows[5])[('chip', 4)]  # FFT Size chip 8192
    ui.touch_down(chip.centerx, chip.centery); ui.touch_up(chip.centerx, chip.centery)
    assert ("/sp", 0, 5, 4.0) in ui.osc
    assert sm_page.fmt_value(ui.sm.reg.param(0, 5), 4.0) == "8192"


def test_sheet_back_chip_and_swipe_down_close(ui):
    ui.sm.open_sheet(0); ui.render()
    ui.touch_down(60, 50); ui.touch_up(60, 50)
    assert ui.sm.sheet is None
    ui.sm.open_sheet(0); ui.render()
    ui.touch_down(360, 400); ui.touch_move(360, 500); ui.touch_move(360, 620); ui.touch_up(360, 620)
    assert ui.sm.sheet is None


def test_sheet_scrolls(ui):
    ui.sm.open_sheet(4)               # 25 params
    ui.render()
    assert ui.sm.max_scroll() > 0
    ui.touch_down(30, 700); ui.touch_move(30, 600); ui.touch_move(30, 400); ui.touch_up(30, 400)
    assert ui.sm.scroll > 0 and ui.sm.sheet == ('fx', 4)


def test_hero_fader_drag_sends_sp_and_echo_snaps(ui):
    ui.sm.focus = 0
    ui.render()
    rect, fx, p = ui.sm.layout()["faders"][0]
    ui.touch_down(rect.centerx, rect.y)
    assert ui.drag_target == 'sm'
    ui.touch_move(rect.centerx, rect.y)
    ui.touch_up(rect.centerx, rect.y)
    sp = [m for m in ui.osc if m[0] == "/sp"]
    assert sp[-1] == ("/sp", 0, 0, pytest.approx(1000.0))
    UI.route_osc(ui, "/sm_p", (0, 0, 640.0))
    assert ui.sm.get(0, 0) == 640.0


def test_sm_osc_routing(ui):
    UI.route_osc(ui, "/sm_state", (5, 1))
    assert ui.sm.on[5] == 1
    UI.route_osc(ui, "/sm_p", (7, 0, 33.0))
    assert ui.sm.get(7, 0) == 33.0
    UI.route_osc(ui, "/sm_p", (23, 0, 4.0))
    assert ui.sm.get(23, 0) == 4.0


def test_rack_latency_and_default_fft(ui):
    assert ui.sm.latency_s() == 0
    UI.route_osc(ui, "/sm_state", (0, 1))        # AVERAGE @ 2048 default
    assert ui.sm.latency_s() == pytest.approx(2 * 2048 / 4 / 48000)
    UI.route_osc(ui, "/sm_state", (3, 1))        # FREEZE also 2048: distinct sizes only
    assert ui.sm.latency_s() == pytest.approx(2 * 2048 / 4 / 48000)
    ui.sm.open_sheet('rack')
    assert ui.render()
    chip = ui.sm.row_geom(ui.sm._rows[0])[('chip', 1)]
    ui.touch_down(chip.centerx, chip.centery); ui.touch_up(chip.centerx, chip.centery)
    assert ("/sp", 23, 0, 1.0) in ui.osc


def test_sheet_closes_on_menu_change(ui):
    ui.sm.open_sheet(1)
    UI.route_osc(ui, "/menu", 5)
    assert ui.sm.sheet is None


def test_render_test_produces_two_pngs(tmp_path):
    import subprocess, sys
    r = subprocess.run([sys.executable, "scripts/ui_render_test.py", "--out", str(tmp_path)],
                       capture_output=True, text=True,
                       cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    names = os.listdir(tmp_path)
    assert any("NORRIS_soundmagic" in n for n in names), r.stderr[-400:]
    assert any("NORRIS_deep_sheet" in n for n in names)


def test_sm_hero_remaps_focused_strip(ui):
    sm = ui.sm
    fx = sm.focus
    assert sm.heroes_of(fx) == sm.reg.by_fx[fx]["heroes"][:3]
    before = [p for (_r, _f, p) in sm.layout()["faders"]]
    assert before == sm.heroes_of(fx)
    n = len(sm.reg.by_fx[fx]["params"])
    newp = n - 1
    rev = sm.rev
    UI.route_osc(ui, "/sm_hero", (fx, 0, fx * 32 + newp))
    assert sm.rev > rev
    assert sm.heroes_of(fx)[0] == newp
    assert [p for (_r, _f, p) in sm.layout()["faders"]][0] == newp
    ui.render()                                   # strip redraws with the new set
    UI.route_osc(ui, "/sm_hero", (fx, 0, -1))     # clear slot 0 -> next filled slots shift up
    assert newp not in sm.heroes_of(fx)
    UI.route_osc(ui, "/sm_hero", (fx, 5, 999999))  # outside the effect's block: slot emptied
    assert sm.hero_slots[fx][5] == -1
    UI.route_osc(ui, "/sm_hero", (99, 0, 0))       # bad fx ignored
    ui.render()


def test_sm_pedal_tag(ui):
    sm = ui.sm
    fx = sm.focus
    hero = sm.heroes_of(fx)[0]
    UI.route_osc(ui, "/sm_pedal", (fx, fx * 32 + hero))
    assert sm.pedal[fx] == hero
    ui.dirty = False
    UI.route_osc(ui, "/sm_pedal", (fx, fx * 32 + hero))   # unchanged: no redraw churn
    assert not ui.dirty
    UI.route_osc(ui, "/sm_pedal", (fx, -1))
    assert sm.pedal[fx] == -1
    UI.route_osc(ui, "/sm_pedal", (fx, fx * 32 + hero))
    ui.render()
