"""Headless tests for the PATCHER — the audio-graph view on the AUDIO tab.

Why it exists, so a future reader knows what these are protecting: on
2026-08-09 the engine's inputs were connected to nothing at all. Sound went
in, nothing came through, and not one UI could show why. Every assertion here
is about that being visible and fixable from the device.

demiurge-graph is stubbed throughout — these run on a dev machine with no Pi,
no PipeWire and no audio hardware.
"""

import os

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame
import pytest

import actions
import companion
import neptrPhase4_UI as UI


def _port(name, node, d, kind, disp, links):
    return {"name": name, "node": node, "dir": d, "kind": kind,
            "display": disp, "links": links}


NODES = [
    {"name": "csound6", "kind": "engine", "display": "csound6"},
    {"name": "alsa_input.plat", "kind": "interface", "display": "Interface in"},
    {"name": "alsa_output.plat", "kind": "interface", "display": "Interface out"},
]

# Both directions straight to the card — the shape the device should be in.
HEALTHY = {
    "mode": "sync",
    "nodes": NODES,
    "ports": [
        _port("csound6:output1", "csound6", "out", "engine", "OUT 1", 1),
        _port("csound6:output2", "csound6", "out", "engine", "OUT 2", 1),
        _port("csound6:input1", "csound6", "in", "engine", "IN 1", 1),
        _port("csound6:input2", "csound6", "in", "engine", "IN 2", 1),
        _port("alsa_input.plat:capture_FL", "alsa_input.plat", "out",
              "interface", "CAP L", 1),
        _port("alsa_output.plat:playback_FL", "alsa_output.plat", "in",
              "interface", "PLAY L", 1),
    ],
    "links": [
        {"src": "csound6:output1", "dst": "alsa_output.plat:playback_FL",
         "path": "direct", "flags": {"engine-out", "managed"}},
        {"src": "alsa_input.plat:capture_FL", "dst": "csound6:input1",
         "path": "direct", "flags": {"engine-in", "managed"}},
    ],
    "warnings": [],
    "in_path": "direct",
    "out_path": "direct",
}

# The 2026-08-09 fault: outputs fine, inputs wired to nothing.
BROKEN = {
    "mode": "sync",
    "nodes": NODES,
    "ports": [
        _port("csound6:output1", "csound6", "out", "engine", "OUT 1", 1),
        _port("csound6:input1", "csound6", "in", "engine", "IN 1", 0),
        _port("csound6:input2", "csound6", "in", "engine", "IN 2", 0),
        _port("alsa_input.plat:capture_FL", "alsa_input.plat", "out",
              "interface", "CAP L", 0),
        _port("alsa_output.plat:playback_FL", "alsa_output.plat", "in",
              "interface", "PLAY L", 1),
    ],
    "links": [
        {"src": "csound6:output1", "dst": "alsa_output.plat:playback_FL",
         "path": "direct", "flags": {"engine-out", "managed"}},
    ],
    "warnings": [
        {"code": "no-input",
         "text": "The engine has no input — nothing you play reaches it."},
        {"code": "dangling",
         "text": "csound6:input1 is connected to nothing (engine input)."},
        {"code": "dangling",
         "text": "csound6:input2 is connected to nothing (engine input)."},
    ],
    "in_path": "none",
    "out_path": "direct",
}

# Output through the loopback: the aggregate / Bluetooth / aloop shape, one
# quantum slower than direct, and previously invisible from any UI.
VIA_LOOPBACK = dict(HEALTHY, out_path="loopback")

BYPASS = {"mode": "bypass", "nodes": [], "ports": [], "links": [],
          "warnings": [], "in_path": None, "out_path": None}


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
    # No refresh thread in tests: every test sets ui.graph itself, so the
    # rendered state is exactly the fixture and nothing races it.
    monkeypatch.setattr(UI.NeptrUI, "_refresh_graph", lambda self, *a, **k: None)
    u = UI.NeptrUI()
    u.interfaces = UI.actions.list_interfaces()
    u.graph = dict(HEALTHY)
    yield u
    pygame.display.quit()


def _actions_of(ui, kind):
    return [a for _r, a in ui.hit if a[0] == kind]


def _rect_for(ui, action):
    for r, a in ui.hit:
        if a == action:
            return r
    raise AssertionError("no hit region for %r (have %s)"
                         % (action, sorted({a[0] for _r, a in ui.hit})))


def _tap(ui, action):
    r = _rect_for(ui, action)
    ui.touch_down(r.centerx, r.centery)
    ui.touch_up(r.centerx, r.centery)


def _audio(ui):
    ui.companion.active = True
    ui.companion.screen = companion.AUDIO
    ui.dirty = True
    ui.render()


def _patcher(ui, graph=None):
    if graph is not None:
        ui.graph = dict(graph)
    _audio(ui)
    _tap(ui, ('item', companion.AUDIO, 0))
    ui.render()


def _settle(pred, tries=200):
    for _ in range(tries):
        if pred():
            return True
        pygame.time.wait(5)
    return False


# ───────────────────────── where it lives ─────────────────────────

def test_signal_path_is_the_first_row_on_the_audio_tab(ui):
    """The graph is a SOUND question, so it lives on AUDIO — and above the
    buffer size, because a buffer size is worth nothing if the engine's input
    is wired to nothing."""
    _audio(ui)
    assert ('item', companion.AUDIO, 0) in _actions_of(ui, 'item')


def test_audio_row_count_includes_the_signal_path_row(ui):
    counts = ui._companion_item_counts()
    assert counts[companion.AUDIO] == 1 + len(ui.interfaces) + len(ui._audio_rows())


def test_tapping_signal_path_opens_the_patcher(ui):
    _audio(ui)
    _tap(ui, ('item', companion.AUDIO, 0))
    assert ui.patcher is True


def test_patcher_is_modal_nothing_behind_it_is_tappable(ui):
    _patcher(ui)
    kinds = {a[0] for _r, a in ui.hit}
    assert kinds <= {'gport', 'glink', 'greset', 'gclose'}, kinds


def test_close_returns_to_the_audio_tab(ui):
    _patcher(ui)
    _tap(ui, ('gclose',))
    assert ui.patcher is False
    ui.render()
    assert ('item', companion.AUDIO, 0) in _actions_of(ui, 'item')


# ───────────────────────── direct vs loopback ─────────────────────────

def test_summary_names_both_directions(ui):
    assert ui._graph_summary() == "DIRECT·DIRECT"


def test_loopback_output_is_not_reported_as_direct(ui):
    ui.graph = dict(VIA_LOOPBACK)
    assert ui._graph_summary() == "DIRECT·LOOPBACK"


def test_a_healthy_graph_is_not_flagged(ui):
    assert ui._graph_broken() is False


# ───────────────────────── the fault this exists for ─────────────────────────

def test_a_dangling_engine_input_is_flagged(ui):
    ui.graph = dict(BROKEN)
    assert ui._graph_broken() is True
    assert ui._graph_summary() == "!! NO INPUT"


def test_the_audio_tab_row_shows_the_fault_without_opening_anything(ui):
    """The whole point: the fault must be visible on the settings screen, not
    only to someone who thinks to go looking for a patchbay."""
    ui.graph = dict(BROKEN)
    _audio(ui)
    assert ui._graph_summary().startswith("!!")


def test_dangling_ports_render_in_the_patcher(ui):
    _patcher(ui, BROKEN)
    ports = {a[1] for a in _actions_of(ui, 'gport')}
    assert "csound6:input1" in ports
    assert "csound6:input2" in ports


def test_bypass_mode_explains_itself_instead_of_showing_an_empty_patchbay(ui):
    _patcher(ui, BYPASS)
    # No ports, no links, no reset — and crucially no blank panel that would
    # read as "everything is disconnected".
    assert _actions_of(ui, 'gport') == []
    assert _actions_of(ui, 'greset') == []
    assert ('gclose',) in _actions_of(ui, 'gclose')


# ───────────────────────── connecting by touch ─────────────────────────

def test_first_tap_picks_an_endpoint_and_does_not_connect(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_connect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    assert ui.graph_pick["name"] == "alsa_input.plat:capture_FL"
    assert calls == []


def test_tapping_the_same_port_twice_cancels_the_pick(ui):
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    ui.render()
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    assert ui.graph_pick is None


def test_source_then_destination_connects_through_the_cli(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_connect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    monkeypatch.setattr(UI.actions, "graph_state", lambda **k: dict(HEALTHY))
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    ui.render()
    _tap(ui, ('gport', "csound6:input1"))
    assert _settle(lambda: calls)
    assert calls == [("alsa_input.plat:capture_FL", "csound6:input1")]
    assert ui.graph_pick is None


def test_direction_is_normalised_so_either_order_works(ui, monkeypatch):
    """Tapping the destination first must still produce out→in, not in→out —
    the CLI would reject the reversed pair and the user would just see a
    refusal they did nothing to deserve."""
    calls = []
    monkeypatch.setattr(UI.actions, "graph_connect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    monkeypatch.setattr(UI.actions, "graph_state", lambda **k: dict(HEALTHY))
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "csound6:input1"))
    ui.render()
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    assert _settle(lambda: calls)
    assert calls == [("alsa_input.plat:capture_FL", "csound6:input1")]


def test_two_ports_of_the_same_direction_do_not_connect(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_connect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "csound6:input1"))
    ui.render()
    _tap(ui, ('gport', "csound6:input2"))
    assert calls == []
    assert ui.graph_pick["name"] == "csound6:input1"   # pick survives


def test_the_ui_never_calls_pw_link_itself(ui, monkeypatch):
    """One write path. Every graph change goes through demiurge-graph."""
    ran = []
    monkeypatch.setattr(UI.actions, "default_runner",
                        lambda cmd, **k: ran.append(cmd) or (0, ""))
    monkeypatch.setattr(UI.actions, "graph_state", lambda **k: dict(HEALTHY))
    _patcher(ui, BROKEN)
    _tap(ui, ('gport', "alsa_input.plat:capture_FL"))
    ui.render()
    _tap(ui, ('gport', "csound6:input1"))
    _settle(lambda: ran)
    assert not any("pw-link" in " ".join(c) for c in ran)


# ───────────────────────── disconnecting is armed ─────────────────────────

def test_cutting_a_link_needs_a_second_tap(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_disconnect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    _patcher(ui)
    token = ('glink', "csound6:output1", "alsa_output.plat:playback_FL")
    _tap(ui, token)
    assert calls == [], "one tap cut the engine's output"
    assert ui.setting_armed == token


def test_the_arming_message_says_it_can_be_silence(ui):
    _patcher(ui)
    _tap(ui, ('glink', "csound6:output1", "alsa_output.plat:playback_FL"))
    assert "silence" in (ui.companion_msg or "").lower()


def test_the_second_tap_cuts_it(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_disconnect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    monkeypatch.setattr(UI.actions, "graph_state", lambda **k: dict(BROKEN))
    _patcher(ui)
    token = ('glink', "csound6:output1", "alsa_output.plat:playback_FL")
    _tap(ui, token)
    ui.render()
    _tap(ui, token)
    assert _settle(lambda: calls)
    assert calls == [("csound6:output1", "alsa_output.plat:playback_FL")]


def test_the_arm_expires(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_disconnect",
                        lambda s, d, **k: calls.append((s, d)) or (True, ""))
    _patcher(ui)
    token = ('glink', "csound6:output1", "alsa_output.plat:playback_FL")
    _tap(ui, token)
    ui.setting_armed_until = 0.0            # the window closed
    ui.render()
    _tap(ui, token)
    assert calls == [], "a stale arm still cut the link"
    assert ui.setting_armed == token         # re-armed instead


# ───────────────────────── reset ─────────────────────────

def test_reset_is_always_on_screen(ui):
    """A user who has patched themselves into silence must be able to get back
    without ssh, and without scrolling to find the button."""
    _patcher(ui)
    assert ('greset',) in _actions_of(ui, 'greset')


def test_reset_is_armed_before_it_fires(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_reset",
                        lambda **k: calls.append(1) or (True, ""))
    _patcher(ui)
    _tap(ui, ('greset',))
    assert calls == []
    assert ui.setting_armed == ('greset',)


def test_reset_fires_on_the_confirming_tap(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_reset",
                        lambda **k: calls.append(1) or (True, ""))
    monkeypatch.setattr(UI.actions, "graph_state", lambda **k: dict(HEALTHY))
    _patcher(ui)
    _tap(ui, ('greset',))
    ui.render()
    _tap(ui, ('greset',))
    assert _settle(lambda: calls)
    assert calls == [1]


def test_reset_is_reachable_even_with_nothing_connected(ui, monkeypatch):
    calls = []
    monkeypatch.setattr(UI.actions, "graph_reset",
                        lambda **k: calls.append(1) or (True, ""))
    empty = dict(BROKEN, links=[], ports=[
        _port("csound6:input1", "csound6", "in", "engine", "IN 1", 0)])
    _patcher(ui, empty)
    _tap(ui, ('greset',))
    ui.render()
    _tap(ui, ('greset',))
    assert _settle(lambda: calls)
    assert calls == [1]


# ───────────────────────── gesture grammar ─────────────────────────

def test_press_previews_and_release_commits(ui):
    _patcher(ui, BROKEN)
    r = _rect_for(ui, ('gport', "alsa_input.plat:capture_FL"))
    ui.touch_down(r.centerx, r.centery)
    assert ui.graph_pick is None                     # nothing fired yet
    assert ui._pressed(('gport', "alsa_input.plat:capture_FL"))
    ui.touch_up(r.centerx, r.centery)
    assert ui.graph_pick is not None                 # release commits


def test_movement_past_the_threshold_is_a_scroll_not_a_tap(ui):
    _patcher(ui, BROKEN)
    r = _rect_for(ui, ('gport', "alsa_input.plat:capture_FL"))
    ui.touch_down(r.centerx, r.centery)
    ui.touch_move(r.centerx + 4, r.centery - 2 * UI.SCROLL_STEP)
    ui.touch_up(r.centerx + 4, r.centery - 2 * UI.SCROLL_STEP)
    assert ui.graph_pick is None                     # no accidental pick
    assert ui.list_scroll.get('patcher', 0) > 0


def test_a_drag_in_the_patcher_never_switches_the_tab_behind_it(ui):
    _patcher(ui)
    before = ui.companion.screen
    ui.touch_down(360, 640)
    ui.touch_move(360 - UI.SWIPE_PX - 40, 646)
    ui.touch_up(200, 646)
    assert ui.companion.screen == before


def test_leaving_the_audio_tab_closes_the_patcher(ui):
    _patcher(ui)
    ui._apply_companion_events(["nav"], 1.0)
    assert ui.patcher is False


# ───────────────────────── the CLI contract ─────────────────────────

def test_graph_state_parses_the_cli_machine_lines():
    out = "\n".join([
        "mode|sync",
        "node|csound6|engine|csound6",
        "node|alsa_input.plat|interface|Interface in",
        "port|csound6:input1|csound6|in|engine|IN 1|0",
        "port|alsa_input.plat:capture_FL|alsa_input.plat|out|interface|CAP L|0",
        "link|csound6:output1|alsa_output.plat:playback_FL|direct|engine-out,managed",
        "warn|no-input|The engine has no input — nothing you play reaches it.",
        "path|none|direct",
    ])
    g = actions.graph_state(runner=lambda cmd, **k: (0, out))
    assert g["mode"] == "sync"
    assert g["in_path"] == "none" and g["out_path"] == "direct"
    assert g["ports"][0]["links"] == 0
    assert g["links"][0]["flags"] == {"engine-out", "managed"}
    assert g["warnings"][0]["code"] == "no-input"


def test_graph_state_treats_exit_3_as_a_state_not_a_failure():
    """demiurge-graph exits 3 for "there is no graph" (bypass / no PipeWire).
    Parsing it as a failure would show "unavailable" where the honest answer
    is "the sync layer is off"."""
    g = actions.graph_state(runner=lambda cmd, **k: (3, "mode|bypass\n"))
    assert g["mode"] == "bypass"


def test_graph_state_survives_a_missing_cli():
    g = actions.graph_state(runner=lambda cmd, **k: (127, "not found"))
    assert g["mode"] == "unavailable" and g["ports"] == []


def test_graph_state_skips_malformed_lines():
    out = "mode|sync\nport|too|few\nnonsense\nport|a:b|a|in|engine|IN 1|zz\n"
    g = actions.graph_state(runner=lambda cmd, **k: (0, out))
    assert g["mode"] == "sync" and g["ports"] == []


def test_write_helpers_shell_out_to_demiurge_graph():
    seen = []

    def runner(cmd, **k):
        seen.append(cmd)
        return 0, "ok"

    actions.graph_connect("a:out", "b:in", runner=runner)
    actions.graph_disconnect("a:out", "b:in", runner=runner)
    actions.graph_reset(runner=runner)
    assert all(c[0].endswith("demiurge-graph") for c in seen)
    assert [c[1:] for c in seen] == [
        ["connect", "a:out", "b:in"],
        ["disconnect", "a:out", "b:in"],
        ["reset"],
    ]
