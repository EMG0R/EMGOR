"""Headless tests for the reorganised settings area, the WAVE promotion and
the Wi-Fi keyboard.

Everything that would shell out to a demiurge CLI is stubbed, so these run on
a dev machine with no Pi and no audio hardware.
"""

import os

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import actions
import companion
import neptrPhase4_UI as UI


AUDIO_FIXTURE = {
    "mode": {"value": "bypass", "default": "sync",
             "options": ["sync", "bypass"],
             "effect": "bypass: the chain program owns the interface directly"},
    "quantum": {"value": "128", "default": "128",
                "options": ["32", "64", "128", "256"],
                "effect": "CPU headroom only — the audio backend pins a "
                          "1024-frame ring and ignores this setting"},
    "bits": {"value": "32", "default": "32",
             "options": ["16", "32", "float"],
             "effect": "sample format sent to the device"},
    "rate": {"value": "48000", "default": "48000",
             "options": ["44100", "48000"],
             "effect": "sample rate of the whole graph"},
}

NETS = [
    {"ssid": "HomeNet", "signal": 80, "security": "wpa",
     "active": True, "saved": True},
    {"ssid": "Backstage", "signal": 60, "security": "wpa",
     "active": False, "saved": True},
    {"ssid": "Venue Guest", "signal": 44, "security": "open",
     "active": False, "saved": False},
    {"ssid": "CoffeeShop", "signal": 30, "security": "wpa",
     "active": False, "saved": False},
]


@pytest.fixture
def ui(monkeypatch):
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))
    monkeypatch.setattr(UI.actions, "list_patches", lambda d: ["neptr"])
    monkeypatch.setattr(UI.actions, "list_interfaces",
                        lambda **k: [{"name": "Volt 2", "id": "V2",
                                      "active": True, "chosen": True,
                                      "inuse": False}])
    u = UI.NeptrUI()
    u.interfaces = UI.actions.list_interfaces()
    u.audio_settings = dict(AUDIO_FIXTURE)
    u.wifi = {"state": "connected", "ssid": "HomeNet", "signal": 80,
              "security": "wpa", "ip": "192.168.1.7", "iface": "wlan0"}
    u.wifi_nets = [dict(n) for n in NETS]
    yield u
    pygame.display.quit()


def _actions_of(ui, kind):
    return [a for _r, a in ui.hit if a[0] == kind]


def _rect_for(ui, action):
    for r, a in ui.hit:
        if a == action:
            return r
    raise AssertionError("no hit region for %r" % (action,))


def _tap(ui, action):
    """Press and release inside a registered hit region — the real gesture,
    so press-preview/commit-on-release is exercised, not bypassed."""
    r = _rect_for(ui, action)
    ui.touch_down(r.centerx, r.centery)
    ui.touch_up(r.centerx, r.centery)


def _open(ui, screen):
    ui.companion.active = True
    ui.companion.screen = screen
    ui.dirty = True
    ui.render()


# ───────────────────────── tab structure ─────────────────────────

def test_tabs_are_grouped_by_concept_and_wave_is_not_one():
    assert UI.COMPANION_TABS == ["DASH", "PATCH", "AUDIO", "SYS", "NET"]
    assert "WAVE" not in UI.COMPANION_TABS
    assert companion.SCREENS == ("DASHBOARD", "PATCHES", "AUDIO",
                                 "SYSTEM", "NETWORK")


def test_every_tab_renders_and_registers_its_tab_bar(ui):
    for screen in range(len(companion.SCREENS)):
        _open(ui, screen)
        assert len(_actions_of(ui, 'tab')) == len(companion.SCREENS)


# ───────────────────────── WAVE promotion ─────────────────────────

def test_wave_button_is_on_the_main_screen_not_in_settings(ui):
    ui.companion.active = False
    ui.current_view = 'menu0'
    ui.dirty = True
    ui.render()
    assert _actions_of(ui, 'wave_open') == [('wave_open',)]


def test_wave_button_opens_the_wave_destination(ui):
    ui.companion.active = False
    ui.current_view = 'menu0'
    ui.dirty = True
    ui.render()
    _tap(ui, ('wave_open',))
    assert ui.current_view == UI.WAVE_VIEW
    assert ui.on_wave()


def test_wave_back_button_returns_where_it_was_opened_from(ui):
    ui.companion.active = False
    ui.current_view = 'menu2'
    ui.last_menu_index = 2
    ui.dirty = True
    ui.render()
    _tap(ui, ('wave_open',))
    ui.render()
    assert _actions_of(ui, 'back') == [('back',)]
    _tap(ui, ('back',))
    assert ui.current_view == 'menu2'
    assert not ui.on_wave()


def test_wave_button_and_back_never_share_the_top_left_corner(ui):
    """The two top-left owners must be mutually exclusive, or a tap there is
    ambiguous."""
    ui.companion.active = False
    for view in ('menu0', UI.WAVE_VIEW):
        ui.current_view = view
        ui.dirty = True
        ui.render()
        both = _actions_of(ui, 'wave_open') and _actions_of(ui, 'back')
        assert not both, "both owners present on %s" % view


def test_wave_editor_still_draws_and_sends_on_finger_lift(ui):
    ui.companion.active = False
    ui.current_view = 'menu0'
    ui.dirty = True
    ui.render()
    _tap(ui, ('wave_open',))
    ui.render()
    wr, br = ui._wave_rects
    ui.touch_down(wr.x + 10, wr.centery - 40)
    ui.touch_move(wr.right - 10, wr.centery + 40)
    ui.touch_up(wr.right - 10, wr.centery + 40)
    # a finger-lift arms the debounced OSC send
    assert ui.wt_send_at is not None
    assert br.height > 0


def test_wave_touch_is_ignored_while_the_settings_menu_is_open(ui):
    """on_wave() is the single gate — with the menu open the editor is not on
    screen, so its rects must not steal the gesture."""
    ui.companion.active = False
    ui.current_view = UI.WAVE_VIEW
    ui.dirty = True
    ui.render()
    ui.companion.active = True
    assert not ui.on_wave()


# ───────────────────────── AUDIO screen ─────────────────────────

def test_audio_screen_shows_interfaces_and_engine_rows_together(ui):
    _open(ui, companion.AUDIO)
    items = _actions_of(ui, 'item')
    # SIGNAL PATH + one interface + four engine rows, all on the AUDIO screen
    assert len(items) == 1 + 1 + len(actions.AUDIO_ROWS)
    assert all(a[1] == companion.AUDIO for a in items)


def test_audio_item_count_covers_both_sections(ui):
    counts = ui._companion_item_counts()
    assert counts[companion.AUDIO] == 1 + 1 + len(actions.AUDIO_ROWS)


def test_tapping_an_engine_row_opens_its_picker(ui):
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 2))     # after SIGNAL PATH + 1 iface
    assert ui.picker is not None
    assert ui.picker["key"] == actions.AUDIO_ROWS[0][0]


def test_picker_offers_exactly_the_cli_options(ui):
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 2 + 2))   # the bits row
    assert ui.picker["key"] == "bits"
    ui.render()
    assert set(_actions_of(ui, 'pick')) == {
        ('pick', o) for o in AUDIO_FIXTURE["bits"]["options"]}


def test_picker_never_offers_24_bit(ui):
    """24 is not a legal sample format for csound's realtime ALSA. The CLI is
    the only place that knows the legal set, so the UI must simply not have a
    way to invent one."""
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 4))
    ui.render()
    assert not any(a[1] == "24" for a in _actions_of(ui, 'pick'))


def test_picker_shows_the_cli_effect_text_verbatim(ui):
    """The bypass-mode caveat must reach the screen as the CLI wrote it."""
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 3))       # quantum
    assert ui.picker["effect"] == AUDIO_FIXTURE["quantum"]["effect"]
    assert "CPU headroom only" in ui.picker["effect"]


def test_choosing_a_safe_setting_calls_the_cli_immediately(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "set_audio",
                        lambda k, v, **kw: calls.append((k, v)) or True)
    monkeypatch.setattr(UI.actions, "audio_settings", lambda **kw: AUDIO_FIXTURE)
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 4))       # bits
    ui.render()
    _tap(ui, ('pick', "16"))
    for _ in range(200):
        if calls:
            break
        pygame.time.wait(5)
    assert calls == [("bits", "16")]


def test_a_restarting_setting_needs_a_second_tap(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "set_audio",
                        lambda k, v, **kw: calls.append((k, v)) or True)
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 2))       # mode — restarts audio
    ui.render()
    _tap(ui, ('pick', "sync"))
    assert calls == [], "mode changed on a single tap"
    assert ui.setting_armed == ("mode", "sync")
    assert ui.picker is not None                  # still open, still asking


def test_mode_and_rate_are_the_confirmed_ones():
    assert actions.AUDIO_CONFIRM == frozenset(("mode", "rate"))


# ───────────────────────── SYSTEM screen ─────────────────────────

def test_system_screen_carries_the_power_items(ui):
    _open(ui, companion.SYSTEM)
    items = _actions_of(ui, 'item')
    assert len(items) == len(UI.POWER_ITEMS)
    assert all(a[1] == companion.SYSTEM for a in items)


# ───────────────────────── NETWORK screen ─────────────────────────

def test_network_screen_lists_every_visible_network(ui):
    _open(ui, companion.NETWORK)
    assert len(_actions_of(ui, 'item')) == len(NETS)
    assert _actions_of(ui, 'net_rescan') == [('net_rescan',)]


def test_joining_the_current_network_is_a_no_op(ui):
    _open(ui, companion.NETWORK)
    _tap(ui, ('item', companion.NETWORK, 0))     # HomeNet, already active
    assert ui.keyboard is None
    assert ui.setting_armed is None


def test_a_saved_network_arms_before_it_switches(ui):
    _open(ui, companion.NETWORK)
    _tap(ui, ('item', companion.NETWORK, 1))     # Backstage, saved
    assert ui.keyboard is None
    assert ui.setting_armed == ('wifi', "Backstage")


def test_an_open_network_also_arms_rather_than_switching_instantly(ui):
    _open(ui, companion.NETWORK)
    _tap(ui, ('item', companion.NETWORK, 2))     # Venue Guest, open
    assert ui.keyboard is None
    assert ui.setting_armed == ('wifi', "Venue Guest")


def test_a_secured_unknown_network_opens_the_keyboard(ui):
    _open(ui, companion.NETWORK)
    _tap(ui, ('item', companion.NETWORK, 3))     # CoffeeShop, needs a password
    assert ui.keyboard is not None
    assert ui.keyboard["ssid"] == "CoffeeShop"


# ───────────────────────── keyboard ─────────────────────────

def _kb(ui, ssid="CoffeeShop"):
    ui.companion.active = True
    ui.companion.screen = companion.NETWORK
    ui._open_keyboard(ssid)
    ui.render()


def test_keyboard_keys_are_big_enough_for_a_finger(ui):
    """The panel is resistive and single-touch; a 10-wide QWERTY row would be
    ~48 physical px per key. Every key must clear a real finger target once
    the 720x1280 canvas is scaled down to the 480x800 panel."""
    _kb(ui)
    scale_x, scale_y = 480 / UI.W, 800 / UI.H
    keys = [r for r, a in ui.hit if a[0] == 'kb']
    assert keys
    for r in keys:
        assert r.width * scale_x >= 48
        assert r.height * scale_y >= 48


def test_keyboard_covers_the_whole_alphabet_and_digits(ui):
    _kb(ui)
    letters = {a[1] for _r, a in ui.hit if a[0] == 'kb' and len(a[1]) == 1}
    assert set("abcdefghijklmnopqrstuvwxyz") <= letters | {" "}
    _tap(ui, ('kb', '@page'))
    ui.render()
    digits = {a[1] for _r, a in ui.hit if a[0] == 'kb'}
    assert set("0123456789") <= digits


def test_typing_builds_the_password(ui):
    _kb(ui)
    for ch in "abc":
        _tap(ui, ('kb', ch))
        ui.render()
    assert ui.keyboard["text"] == "abc"


def test_shift_is_one_shot_then_releases(ui):
    _kb(ui)
    _tap(ui, ('kb', '@shift'))
    ui.render()
    _tap(ui, ('kb', 'a'))
    ui.render()
    _tap(ui, ('kb', 'b'))
    assert ui.keyboard["text"] == "Ab"


def test_double_tapping_shift_locks_caps(ui):
    _kb(ui)
    _tap(ui, ('kb', '@shift'))
    ui.render()
    _tap(ui, ('kb', '@shift'))
    ui.render()
    assert ui.keyboard["caps"] is True
    for ch in "ab":
        _tap(ui, ('kb', ch))
        ui.render()
    assert ui.keyboard["text"] == "AB"


def test_backspace_deletes(ui):
    _kb(ui)
    for ch in "ab":
        _tap(ui, ('kb', ch))
        ui.render()
    _tap(ui, ('kb', '@back'))
    assert ui.keyboard["text"] == "a"


def test_password_is_masked_by_default_and_show_reveals_it(ui):
    _kb(ui)
    assert ui.keyboard["reveal"] is False
    _tap(ui, ('kb', '@reveal'))
    assert ui.keyboard["reveal"] is True


def test_short_password_is_refused_without_calling_the_cli(ui, monkeypatch):
    joins = []
    monkeypatch.setattr(UI.actions, "wifi_join",
                        lambda s, p=None, **kw: joins.append((s, p)) or (True, "ok"))
    _kb(ui)
    for ch in "abc":
        _tap(ui, ('kb', ch))
        ui.render()
    _tap(ui, ('kb', '@ok'))
    assert joins == []
    assert ui.keyboard is not None          # still open so it can be fixed


def test_committing_sends_the_password_to_the_cli_and_forgets_it(ui, monkeypatch):
    joins = []
    monkeypatch.setattr(UI.actions, "wifi_join",
                        lambda s, p=None, **kw: joins.append((s, p)) or (True, "joined"))
    monkeypatch.setattr(UI.actions, "wifi_status", lambda **kw: None)
    monkeypatch.setattr(UI.actions, "wifi_list", lambda **kw: [])
    _kb(ui)
    for ch in "abcdefgh":
        _tap(ui, ('kb', ch))
        ui.render()
    _tap(ui, ('kb', '@ok'))
    for _ in range(200):
        if joins:
            break
        pygame.time.wait(5)
    assert joins == [("CoffeeShop", "abcdefgh")]
    # the entry view is gone, so the UI no longer holds the plaintext
    assert ui.keyboard is None


def test_cancelling_drops_the_typed_password(ui):
    _kb(ui)
    for ch in "abcdefgh":
        _tap(ui, ('kb', ch))
        ui.render()
    _tap(ui, ('kb', '@cancel'))
    assert ui.keyboard is None


def test_keyboard_is_modal_nothing_behind_it_is_tappable(ui):
    _kb(ui)
    kinds = {a[0] for _r, a in ui.hit}
    assert kinds == {'kb'}, "non-keyboard hit regions leaked through: %s" % kinds


def test_picker_is_modal_too(ui):
    _open(ui, companion.AUDIO)
    _tap(ui, ('item', companion.AUDIO, 2))
    ui.render()
    kinds = {a[0] for _r, a in ui.hit}
    assert kinds <= {'pick', 'pick_cancel'}
