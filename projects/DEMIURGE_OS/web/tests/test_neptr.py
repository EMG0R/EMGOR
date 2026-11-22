"""The NEPTR half of demiurge-web: the instrument model lifted from the
pygame UI, the OSC mirror, the MIDI write path, and the fader control loop.

The load-bearing claims under test are the ones that would silently make the
phone edit the wrong pedal:

  * the model still matches the pygame UI it was generated from;
  * every CC this server can emit is one the panel also emits;
  * a fader refuses when the engine's knobs are armed elsewhere;
  * nothing here ever writes engine state on the strength of its own send.
"""
import sys
import json
import struct
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import neptr_model as nm                                    # noqa: E402
from demiurge_web import (osc_decode, osc_decode_any, NeptrState, EngineIn,  # noqa: E402
                          ParamChase, Actions, App, MockProbes, collect,
                          make_server)

FX = pathlib.Path(__file__).parent / "fixtures"
NEPTR_SRC = (pathlib.Path(__file__).resolve().parents[2]
             / "neptr" / "neptrPhase4_UI.py")


# ───────────────────────── the generated model ─────────────────────────

def test_model_shape_is_the_instrument():
    assert len(nm.MENUS) == nm.MENU_COUNT == 21
    for menu in nm.MENUS:
        assert len(menu["effects"]) == 5
    assert nm.MENUS[nm.LOOPER_MENU]["name"] == "LOOPER"
    assert nm.MENUS[nm.PRESET_MENU]["name"] == "PRESETS"
    assert nm.NAM_MENU > 0 and nm.IR_MENU > 0


def test_model_is_still_in_lockstep_with_the_pygame_ui():
    """Regenerating must be a no-op. If this fails, the panel gained or moved
    an effect and `python3 web/gen_neptr_model.py > web/neptr_model.py` has
    not been re-run — which is exactly the drift the generator exists to
    prevent."""
    if not NEPTR_SRC.exists():
        return                                  # not a checkout with the panel
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
    from gen_neptr_model import lift
    live = lift(str(NEPTR_SRC))
    assert live["MENUS"] == nm.MENUS
    assert live["PARAM_CC"] == nm.PARAM_CC
    assert live["EFFECT_PARAMS"] == nm.EFFECT_PARAMS
    assert live["_PHYS_FOR_BTN"] == nm._PHYS_FOR_BTN
    assert live["_PHYS_FOR_ROT"] == nm._PHYS_FOR_ROT


def test_row_zero_is_hardware_slot_four_everywhere_but_the_looper():
    # The panel draws leftmost-first and the hardware slots run right-to-left,
    # so UI row 0 is slot 4 — except the LOOPER, whose CH1..CH5 are slot 0..4.
    assert nm.slot_button_cc(6, 0) == nm._PHYS_FOR_BTN[72]
    assert nm.hw_slot(6, 0) == 4 and nm.hw_slot(6, 4) == 0
    assert nm.hw_slot(nm.LOOPER_MENU, 0) == 0
    assert nm.slot_button_cc(nm.LOOPER_MENU, 0) == nm._PHYS_FOR_BTN[74]


def test_every_emittable_cc_is_one_the_panel_emits():
    for mi in range(len(nm.MENUS)):
        for row in range(5):
            assert nm.slot_button_cc(mi, row) in nm.ALLOWED_PHYS_CC
    for code, params in nm.EFFECT_PARAMS.items():
        for entry in params:
            hit = nm.param_rot_cc(code, entry[0])
            if hit:
                assert hit[0] in nm.ALLOWED_PHYS_CC
    assert nm.EXPR_CC in nm.ALLOWED_PHYS_CC
    assert nm.MENU_PREV_CC in nm.ALLOWED_PHYS_CC


def test_nam_rows_share_one_pair_of_gain_params():
    # Menus 17/18 spend all five slot rotations on the variant browse, so the
    # NAM gains live on the two menu-encoder rotations no menu shadows.
    for r in range(5):
        assert nm.param_rot_cc("nam%d" % r, "in_gain") == \
               nm.param_rot_cc("nam", "in_gain")
    assert nm.param_rot_cc("nam0", "in_gain")[1] == 37


def test_params_without_an_encoder_path_are_named_not_guessed():
    missing = nm.unavailable_params()
    # These are real holes in the engine, not gaps in this table: a fader for
    # them must render as unavailable rather than silently do nothing.
    assert ("tdl", "head") not in missing        # generated via a local alias
    assert ("bds", "blend") in missing
    assert ("hab", "scan") in missing
    for code, param in missing:
        assert nm.param_rot_cc(code, param) is None


def test_describe_is_json_ready_and_complete():
    d = nm.describe()
    json.dumps(d)
    assert len(d["menus"]) == 21
    slot = d["menus"][6]["slots"][4]             # DRIVE / TUBESCREAM
    assert slot["code"] == "od" and slot["hw"] == 0
    drive = [p for p in slot["params"] if p["name"] == "drive"][0]
    assert drive["cc"] == nm._PHYS_FOR_ROT[24] and drive["toggle"] == 31
    head = [p for p in d["menus"][10]["slots"][0]["params"]
            if p["name"] == "head"][0]
    assert head["steps"] == nm.TDL_HEADS


def test_stepped_params_read_as_words():
    assert nm.step_labels("tdl", "head")[6] == "1+2+3"
    assert nm.step_labels("od", "drive") is None
    assert nm.value_format(-12, 12) == 0         # semitones are whole
    assert nm.value_format(0, 1) == 2


# ───────────────────────── OSC decode ─────────────────────────

def _msg(addr, tag, payload):
    a = addr.encode() + b"\0"
    a += b"\0" * ((4 - len(a) % 4) % 4)
    return a + tag + payload


def test_osc_decode_int_and_float():
    assert osc_decode(_msg("/menu", b",i\0\0", struct.pack(">i", 6))) \
        == ("/menu", 6)
    addr, val = osc_decode(_msg("/od_drive", b",f\0\0",
                                struct.pack(">f", 0.5)))
    assert addr == "/od_drive" and abs(val - 0.5) < 1e-6


def test_osc_decode_rejects_junk():
    assert osc_decode(b"") is None
    assert osc_decode(b"no-slash\0\0\0\0,i\0\0\0\0\0\1") is None
    assert osc_decode(_msg("/x", b",s\0\0", b"abcd")) is None   # unsupported
    assert osc_decode(_msg("/x", b",i\0\0", b"ab")) is None     # truncated


# ───────────────────────── the state mirror ─────────────────────────

def test_state_is_only_ever_written_by_the_engine():
    st = NeptrState()
    st.apply("/od_state", 0)
    st.apply("/mar_state", 1)
    assert st.states == {"od": 0, "mar": 1}
    st.apply("/od_drive", 0.62)
    assert st.value("/od_drive") == 0.62


def test_engine_side_names_are_translated_to_ui_codes():
    st = NeptrState()
    st.apply("/preamp_state", 0)          # engine says preamp, the UI says pre
    st.apply("/tgl_state", 1)
    assert st.states["pre"] == 0 and st.states["tgl"] == 1


def test_one_nam_gain_lands_on_all_five_rows():
    st = NeptrState()
    st.apply("/nam_in_gain", 3.0)
    assert [st.value("/nam%d_in_gain" % r) for r in range(5)] == [3.0] * 5


def test_looper_and_selection_fan_out_by_channel():
    st = NeptrState()
    st.apply("/lp3_state", 2)
    st.apply("/lp3_pos", 0.4)
    st.apply("/nam_sel2", 1)
    st.apply("/ir_sel4", 3)
    snap = st.snapshot()
    assert snap["looper"]["states"][2] == 2
    assert abs(snap["looper"]["pos"][2] - 0.4) < 1e-6
    assert snap["nam_sel"][2] == 1 and snap["ir_sel"][4] == 3


def test_unknown_addresses_do_not_grow_the_state():
    st = NeptrState()
    st.apply("/not_a_thing", 1)
    st.apply("/od_notaparam", 1)
    assert st.params == {} and st.states == {}


def test_synced_needs_a_real_snapshot_not_one_stray_message():
    # Ordinary change traffic carries one or two addresses; the CC102 refresh
    # burst carries dozens. Eight separates them with a wide margin.
    st = NeptrState()
    st.apply("/cb_bpm", 120.0)
    assert st.snapshot()["synced"] is False
    for i, code in enumerate(["od", "mar", "vrb", "dly", "cho", "pha",
                              "fla", "vib"]):
        st.apply("/%s_state" % code, i % 2)
    assert st.snapshot()["synced"] is True


def test_change_callback_fires_only_for_messages_we_keep():
    seen = []
    st = NeptrState(on_change=lambda: seen.append(1))
    st.apply("/od_state", 0)
    st.apply("/garbage", 1)
    assert len(seen) == 1


# ───────────────────────── the contested port ─────────────────────────

def test_listener_reports_port_busy_instead_of_stealing_it():
    """The whole point: with the panel holding 8000 this must degrade to
    'control works, state unknown' — never take a share of the datagrams and
    half-blind the surface the user plays with their hands."""
    import socket
    holder = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    holder.bind(("127.0.0.1", 0))
    port = holder.getsockname()[1]
    try:
        inp = EngineIn(NeptrState(), port=port)
        try:
            inp._bind()
            assert False, "bound a port that was already taken"
        except OSError:
            pass
        assert inp.info()["port"] == port
    finally:
        holder.close()


def test_listener_binds_when_the_port_is_free():
    inp = EngineIn(NeptrState(), port=0)
    s = inp._bind()
    try:
        assert s.getsockname()[0] == "127.0.0.1"
    finally:
        s.close()


# ───────────────────────── the fader control loop ─────────────────────────

class FakeOut:
    def __init__(self, ok=True):
        self.sent = []
        self.detail = "fake"
        self.ok = ok

    def cc(self, control, value, channel=0):
        self.sent.append((channel, control, value))
        return self.ok

    def press(self, c):
        return self.cc(c, 127)

    def release(self, c):
        return self.cc(c, 0)

    def tap(self, c):
        self.press(c)
        self.release(c)
        return self.ok

    def rotate(self, c, up):
        return self.cc(c, 127 if up else 0)

    def absolute(self, c, v):
        return self.cc(c, int(v))

    def sync_request(self):
        return self.cc(nm.CC_UI_SYNC, 127, channel=nm.UTILITY_CHANNEL)

    def info(self):
        return {"ok": self.ok, "detail": self.detail}


def _chase(state=None):
    out = FakeOut()
    return out, ParamChase(out, state or NeptrState())


def test_chase_burst_is_capped_however_far_it_has_to_go():
    st = NeptrState()
    st.apply("/od_drive", 0.0)
    out, ch = _chase(st)
    # A full-span jump: without the cap this would be ~50 messages in one
    # tick, and 300 msg/s has been observed to stall the engine's MIDI input.
    ch.step({"code": "od", "param": "drive", "phys": 20,
             "lo": 0, "hi": 1, "target": 1.0})
    assert len(out.sent) == nm.PARAM_BURST


def test_chase_stops_inside_the_deadband():
    st = NeptrState()
    st.apply("/od_drive", 0.5)
    out, ch = _chase(st)
    ch.step({"code": "od", "param": "drive", "phys": 20,
             "lo": 0, "hi": 1, "target": 0.51})
    assert out.sent == []


def test_chase_turns_the_right_way():
    st = NeptrState()
    st.apply("/od_drive", 0.8)
    out, ch = _chase(st)
    ch.step({"code": "od", "param": "drive", "phys": 20,
             "lo": 0, "hi": 1, "target": 0.2})
    assert all(v == 0 for _c, _n, v in out.sent)      # CCW


def test_chase_refuses_to_guess_before_the_first_echo():
    out, ch = _chase()                                 # no value known
    assert ch.step({"code": "od", "param": "drive", "phys": 20,
                    "lo": 0, "hi": 1, "target": 1.0}) is False
    assert out.sent == []


# ───────────────────────── actions ─────────────────────────

class FakeEngine:
    def __init__(self, live=True, state=None):
        self.state = state or NeptrState()
        self.out = FakeOut()
        self.chase = None
        self._live = live
        self.targets = []

        class Chase:
            def set_target(_s, *a):
                self.targets.append(a)
        self.chase = Chase()

    def osc_live(self):
        return self._live


def _actions(tmp_path, engine):
    return Actions(lambda cmd, stdin=None: (0, ""), lambda: [],
                   tmp_path / "log", engine=engine)


def test_slot_tap_sends_the_panels_own_button_cc(tmp_path):
    eng = FakeEngine()
    status, _ = _actions(tmp_path, eng).neptr_slot(6, 4)
    assert status == 200
    cc = nm.slot_button_cc(6, 4)
    assert eng.out.sent == [(0, cc, 127), (0, cc, 0)]


def test_slot_out_of_range_rejected_and_nothing_sent(tmp_path):
    eng = FakeEngine()
    a = _actions(tmp_path, eng)
    for bad in [(99, 0), (0, 9), ("6", 0), (6, None)]:
        assert a.neptr_slot(*bad)[0] == 400
    assert eng.out.sent == []


def test_looper_rows_are_held_not_tapped(tmp_path):
    eng = FakeEngine()
    a = _actions(tmp_path, eng)
    a.neptr_hold(nm.LOOPER_MENU, 0, True)
    a.neptr_hold(nm.LOOPER_MENU, 0, False)
    cc = nm.slot_button_cc(nm.LOOPER_MENU, 0)
    assert eng.out.sent == [(0, cc, 127), (0, cc, 0)]
    assert a.neptr_hold(nm.LOOPER_MENU, 0, "yes")[0] == 400


def test_menu_paging_uses_the_engines_own_nav_buttons(tmp_path):
    eng = FakeEngine()
    a = _actions(tmp_path, eng)
    a.neptr_menu("next")
    a.neptr_menu("prev")
    assert [c for _ch, c, v in eng.out.sent if v == 127] == \
        [nm.MENU_NEXT_CC, nm.MENU_PREV_CC]
    assert a.neptr_menu("sideways")[0] == 400


def test_a_fader_is_refused_when_the_knobs_are_armed_elsewhere(tmp_path):
    st = NeptrState()
    st.apply("/last_toggle", 99)               # some other effect
    st.apply("/od_drive", 0.2)
    eng = FakeEngine(state=st)
    status, res = _actions(tmp_path, eng).neptr_param("od", "drive", 0.8)
    assert status == 409 and "armed elsewhere" in res["output"]
    assert eng.targets == []


def test_a_fader_runs_when_its_own_effect_is_armed(tmp_path):
    st = NeptrState()
    st.apply("/last_toggle", 31)               # TUBESCREAM's toggle
    st.apply("/od_drive", 0.2)
    eng = FakeEngine(state=st)
    status, _ = _actions(tmp_path, eng).neptr_param("od", "drive", 0.8)
    assert status == 200 and len(eng.targets) == 1
    assert eng.targets[0][0:2] == ("od", "drive")


def test_without_the_state_feed_a_fader_refuses_but_single_steps_do_not(
        tmp_path):
    # No echo means no loop to close; a burst would be an open-loop guess.
    # One detent is still exactly as truthful as one click of the knob.
    eng = FakeEngine(live=False)
    a = _actions(tmp_path, eng)
    assert a.neptr_param("od", "drive", 0.8)[0] == 409
    status, res = a.neptr_step("od", "drive", "up")
    assert status == 200 and "arming unknown" in res["output"]
    assert eng.out.sent == [(0, nm.param_rot_cc("od", "drive")[0], 127)]


def test_param_values_outside_the_engines_range_are_refused(tmp_path):
    st = NeptrState()
    st.apply("/last_toggle", 31)
    eng = FakeEngine(state=st)
    a = _actions(tmp_path, eng)
    assert a.neptr_param("od", "drive", 4.0)[0] == 400
    assert a.neptr_param("od", "drive", "loud")[0] == 400
    assert a.neptr_param("od", "nosuchparam", 0.5)[0] == 400
    assert a.neptr_param("nosucheffect", "drive", 0.5)[0] == 400


def test_a_param_with_no_encoder_path_is_refused_not_faked(tmp_path):
    st = NeptrState()
    st.apply("/last_toggle", 1)
    eng = FakeEngine(state=st)
    status, res = _actions(tmp_path, eng).neptr_param("bds", "blend", 3)
    assert status == 400 and "no encoder path" in res["output"]
    assert eng.out.sent == []


def test_expression_pedal_is_absolute_and_range_checked(tmp_path):
    eng = FakeEngine()
    a = _actions(tmp_path, eng)
    assert a.neptr_expr(100)[0] == 200
    assert eng.out.sent == [(0, nm.EXPR_CC, 100)]
    assert a.neptr_expr(200)[0] == 400
    assert a.neptr_expr(True)[0] == 400


def test_sync_request_goes_out_on_the_utility_channel(tmp_path):
    eng = FakeEngine()
    _actions(tmp_path, eng).neptr_sync()
    assert eng.out.sent[0] == (nm.UTILITY_CHANNEL, nm.CC_UI_SYNC, 127)


def test_midi_unavailable_is_a_503_not_a_crash(tmp_path):
    eng = FakeEngine()
    eng.out.ok = False
    status, res = _actions(tmp_path, eng).neptr_slot(6, 4)
    assert status == 503 and res["ok"] is False


def test_actions_without_an_engine_reject_cleanly(tmp_path):
    a = Actions(lambda cmd, stdin=None: (0, ""), lambda: [],
                tmp_path / "log", engine=None)
    assert a.neptr_slot(6, 4)[0] == 400
    assert a.neptr_expr(10)[0] == 400


# ───────────────────────── http surface ─────────────────────────

def _server(engine=None):
    import threading
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static, actions=None, engine=engine)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return app, srv, srv.server_address[1]


def test_model_endpoint_serves_the_whole_instrument():
    import http.client
    _app, srv, port = _server()
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/api/neptr/model")
        r = c.getresponse()
        assert r.status == 200
        d = json.loads(r.read())
        assert len(d["menus"]) == 21
        assert d["expr_cc"] == nm.EXPR_CC
    finally:
        srv.shutdown()


def test_neptr_endpoint_without_an_engine_says_so():
    import http.client
    _app, srv, port = _server(engine=None)
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/api/neptr")
        d = json.loads(c.getresponse().read())
        assert d["live"] is False and d["link"]["status"] == "off"
    finally:
        srv.shutdown()


def test_neptr_push_channel_coalesces_through_the_app():
    app = App()
    q = app.subscribe_neptr()
    app.publish_neptr({"hello": 1})
    assert q.get(timeout=1) == {"hello": 1}
    app.unsubscribe_neptr(q)
    app.publish_neptr({"hello": 2})
    assert q.empty()


# ───────────────────────── menu order (menus.json) ─────────────────────────

def _menus_dir(tmp_path):
    import shutil
    root = pathlib.Path(__file__).resolve().parents[2] / "neptr"
    d = tmp_path / "neptr"
    (d / "csound" / "tools").mkdir(parents=True)
    shutil.copy(root / "menus.json", d / "menus.json")
    shutil.copy(root / "csound" / "tools" / "gen_menus.py",
                d / "csound" / "tools" / "gen_menus.py")
    return d


def test_menus_save_rejects_bad_bodies(tmp_path):
    import demiurge_web as w
    d = _menus_dir(tmp_path)
    cur = w.menus_load(d / "menus.json")
    ids = [m["id"] for m in cur["menus"]]
    ok = [{"id": i, "hidden": False} for i in ids]
    sent = []
    push = lambda doc: sent.append(doc) or True
    for bad in (
        {},                                           # no list
        {"menus": ok[:-1]},                           # a known id missing
        {"menus": ok + [{"id": ids[0]}]},             # duplicate
        {"menus": ok + [{"id": 999}]},                # unknown
        {"menus": [dict(e, hidden=True) for e in ok]},  # nothing visible
        {"menus": list(reversed(ok))},                # conditional HG not last
    ):
        st, res = w.menus_save(bad, d, push)
        assert st == 400 and not res["ok"], bad
    assert sent == []


def test_menus_save_roundtrip_regenerates_and_pushes(tmp_path):
    import demiurge_web as w
    d = _menus_dir(tmp_path)
    cur = w.menus_load(d / "menus.json")
    ids = [m["id"] for m in cur["menus"]]
    new = [{"id": i, "hidden": i == 3} for i in [1, 0] + ids[2:]]
    new = [e for e in new if e["id"] != 3] + [{"id": 3, "hidden": True}]
    # keep HYPERGUITAR last among the visible ones
    new.sort(key=lambda e: e["hidden"])
    hg = [e for e in new if e["id"] == 21][0]
    new.remove(hg); new.insert(sum(1 for e in new if not e["hidden"]), hg)
    sent = []
    st, res = w.menus_save({"menus": new}, d, lambda doc: sent.append(doc) or True)
    assert st == 200 and res["ok"] and res["pushed"] and res["generated"]
    assert res["cycle"][:2] == [1, 0] and 3 not in res["cycle"]
    assert res["cycle"][-1] == 21
    gen = (d / "csound" / "handler" / "gen" / "menu_cycle.orc").read_text()
    assert "giMenuCycle ftgen 0, 0, -32, -2, 1, 0, 2, 4" in gen
    assert "giMenuCycleN = %d" % len(res["cycle"]) in gen
    assert w.menus_load(d / "menus.json")["menus"][0]["id"] == 1
    # names/slots are never taken from the client
    assert all(m["slots"] for m in w.menus_load(d / "menus.json")["menus"][:5])


def test_menus_push_encodes_mc_messages():
    import socket as so
    import demiurge_web as w
    rx = so.socket(so.AF_INET, so.SOCK_DGRAM)
    rx.bind(("127.0.0.1", 0)); rx.settimeout(2)
    doc = {"menus": [{"id": 5}, {"id": 2, "hidden": True}, {"id": 7}]}
    assert w.menus_push(doc, port=rx.getsockname()[1])
    msgs = [osc_decode_any(rx.recv(256)) for _ in range(3)]
    assert msgs[0] == [("/mc", [0, 5])] and msgs[1] == [("/mc", [1, 7])]
    assert msgs[2] == [("/mc_n", [2])]


def test_menus_checked_in_matches_generated_include():
    import subprocess
    gen = (pathlib.Path(__file__).resolve().parents[2] / "neptr" / "csound"
           / "tools" / "gen_menus.py")
    r = subprocess.run([sys.executable, str(gen), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr


# ───────────────────────── SoundMagic spectral rack ─────────────────────────

import shutil                                               # noqa: E402
from demiurge_web import SmRack, osc_encode, osc_decode_multi   # noqa: E402

REPO_NEPTR = pathlib.Path(__file__).resolve().parents[2] / "neptr"


def _sm_tree(tmp_path):
    """A throwaway neptr/ with the registry, generator and catalog, so the
    heroes save can really rewrite and regenerate without touching the repo."""
    d = tmp_path / "neptr"
    shutil.copytree(REPO_NEPTR / "csound" / "spectral", d / "csound" / "spectral",
                    ignore=shutil.ignore_patterns("*.orc", "__pycache__"))
    (d / "docs" / "research").mkdir(parents=True)
    shutil.copy(REPO_NEPTR / "docs" / "research" / "soundmagic_spectral_catalog.md",
                d / "docs" / "research")
    return d


def _sm(tmp_path):
    sent = []
    sm = SmRack(directory=_sm_tree(tmp_path),
                sender=lambda a, *r: sent.append((a,) + r) or True)
    return sm, sent


def test_sm_param_clamps_and_sends_sp(tmp_path):
    sm, sent = _sm(tmp_path)
    reg = sm.registry()
    q = reg["effects"][0]["params"][0]                 # Number of frames 2..1000
    st, res = sm.set_param(0, 0, 99999)
    assert st == 200 and res["val"] == q["max"]
    assert sent[-1] == ("/sp", 0, 0, q["max"])
    st, res = sm.set_param(0, 0, -5)
    assert res["val"] == q["min"]
    st, res = sm.set_param(0, 4, 0.7)                  # bool snaps
    assert res["val"] == 1.0
    assert sm.snapshot()["vals"][str(q["idx"])]["confirmed"] is False


def test_sm_param_rejects_bad_input(tmp_path):
    sm, sent = _sm(tmp_path)
    for fx, p, v in [(99, 0, 1), (-1, 0, 1), (True, 0, 1), (0, 99, 1),
                     (0, -1, 1), (0, True, 1), (0, 0, "x"), (0, 0, None),
                     (0, 0, float("nan")), (23, 5, 1), (0, 0, True)]:
        assert sm.set_param(fx, p, v)[0] == 400, (fx, p, v)
    assert sent == []
    assert sm.set_param(23, 0, 3)[0] == 200            # rack default FFT slot


def test_sm_state_and_reset(tmp_path):
    sm, sent = _sm(tmp_path)
    assert sm.set_state(3, 1)[0] == 200 and sent[-1] == ("/st", 3, 1)
    assert sm.set_state(3, 2)[0] == 400
    assert sm.set_state(23, 1)[0] == 400
    sent.clear()
    st, res = sm.reset(0)
    e = sm.registry()["effects"][0]
    assert st == 200 and len(sent) == len(e["params"])
    assert [s[3] for s in sent] == [q["default"] for q in e["params"]]


def test_sm_mirror_confirms_values(tmp_path):
    sm, _ = _sm(tmp_path)
    sm.set_param(0, 1, 40)
    sm.apply("/sm_p", [0, 1, 40.0])
    sm.apply("/sm_state", [0, 1])
    snap = sm.snapshot()
    assert snap["vals"]["1"] == {"v": 40.0, "confirmed": True}
    assert snap["on"]["0"] == {"on": 1, "confirmed": True}
    assert snap["mirror"]["live"] is True


def test_sm_osc_encoding_of_the_four_verbs():
    for addr, args, tags in [("/sp", (3, 2, 0.5), b"iif"), ("/st", (3, 1), b"ii"),
                             ("/sh", (3, 0, 97), b"iii"), ("/shp", (3, 96), b"ii")]:
        raw = osc_encode(addr, *args)
        got = osc_decode_multi(raw)
        assert got[0] == addr and got[1][:2] == list(args[:2])
        assert b"," + tags in raw


def test_sm_heroes_validation(tmp_path):
    sm, sent = _sm(tmp_path)
    before = (sm.spectral_dir() / "sm_params.json").read_text()
    bad = [{"fx": 0, "heroes": list(range(8)), "pedal": 0},         # > 7
           {"fx": 0, "heroes": [1, 1], "pedal": 0},                 # dup
           {"fx": 0, "heroes": [99], "pedal": 0},
           {"fx": 0, "heroes": [0], "pedal": 99},
           {"fx": 77, "heroes": [0], "pedal": 0},
           {"fx": 0, "heroes": "x", "pedal": 0}]
    for b in bad:
        assert sm.heroes_save(b)[0] == 400, b
    assert sent == []
    assert (sm.spectral_dir() / "sm_params.json").read_text() == before


def test_sm_heroes_save_writes_regenerates_and_pushes(tmp_path):
    sm, sent = _sm(tmp_path)
    st, res = sm.heroes_save({"fx": 0, "heroes": [None, 2, 1], "pedal": 1})
    assert st == 200 and res["generated"] and res["pushed"]
    sd = sm.spectral_dir()
    picks = json.loads((sd / "sm_heroes.json").read_text())
    assert picks["AVERAGE"]["heroes"] == [None, "Lo bin cutoff", "Brightness"]
    assert picks["AVERAGE"]["pedal"] == "Brightness"
    assert picks["BLUR"]["heroes"]                      # other effects kept
    reg = sm.registry()
    assert reg["effects"][0]["heroes"] == [-1, 2, 1] and reg["effects"][0]["pedal"] == 1
    assert "giSMhero" in (sd / "sm_params.orc").read_text()
    assert sent == [("/sh", 0, 0, -1), ("/sh", 0, 1, 2), ("/sh", 0, 2, 1),
                    ("/sh", 0, 3, -1), ("/sh", 0, 4, -1), ("/sh", 0, 5, -1),
                    ("/sh", 0, 6, -1), ("/shp", 0, 1)]
    # the generator's own --check agrees with what the save left behind
    import subprocess
    r = subprocess.run([sys.executable, str(sd / "tools" / "gen_sm_params.py"),
                        "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_sm_http_routes(tmp_path):
    import http.client, threading
    sm, sent = _sm(tmp_path)
    app = App()
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static, actions=None, sm=sm)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    port = srv.server_address[1]
    try:
        def call(method, path, body=None):
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
            c.request(method, path, json.dumps(body) if body is not None else None,
                      {"Content-Type": "application/json"})
            r = c.getresponse()
            return r.status, json.loads(r.read())
        assert call("GET", "/api/neptr/sm/registry")[1]["n_fx"] == 24
        assert call("POST", "/api/neptr/sm/param", {"fx": 0, "p": 0, "val": 50})[0] == 200
        assert call("POST", "/api/neptr/sm/param", {"fx": 0, "p": 99, "val": 1})[0] == 400
        assert call("POST", "/api/neptr/sm/state", {"fx": 0, "on": 1})[0] == 200
        assert call("PUT", "/api/neptr/sm/heroes", {"fx": 0, "heroes": [0], "pedal": 0})[0] == 200
        assert call("GET", "/api/neptr/sm/state")[1]["vals"]
    finally:
        srv.shutdown()


def test_committed_model_matches_generator_output():
    if not NEPTR_SRC.exists():
        return
    from gen_neptr_model import lift
    assert lift(str(NEPTR_SRC))["MENUS"] == nm.MENUS
    assert nm.MENU_COUNT == len(nm.MENUS)
