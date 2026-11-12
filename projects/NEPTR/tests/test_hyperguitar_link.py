"""hyperGuitar <-> NEPTR link: keep the three copies of the lane model in step.

The lane model is stated in THREE places, in three languages, because none of
them can read the others at runtime:

  1. bela/hyperGuitar6chPan/hg_lanes.cpp — gHgLane[], the authority. Entry
     voicings live here because the Bela is what applies them.
  2. csound/hyperguitar.orc — giHgLaneLen[], so a rotation can wrap without a
     round trip to the guitar.
  3. neptrPhase4_UI.py — HG_LANES, so the page renders labels immediately
     instead of blank until the guitar answers.

Duplication is the deliberate choice (streaming strings over OSC would mean a
blank page on every menu entry, and csound has no good way to hold a string
table). These tests are the price: they parse all three and assert they agree,
so the duplication cannot rot silently.

A mismatch is not cosmetic. If csound thinks a lane has 7 entries and the Bela
has 6, the encoder sends an index the guitar clamps, and the lane appears to
stick at the end of its list for no visible reason.
"""
import os
import re

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pytest

import neptrPhase4_UI as UI

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HG_ORC = os.path.join(ROOT, "csound", "hyperguitar.orc")
HG_LINK_ORC = os.path.join(ROOT, "csound", "hyperguitar_link.orc")

# The Bela tree is a sibling of DEMIURGE_OS, not inside it.
_REPO_PARENT = os.path.dirname(os.path.dirname(ROOT))
HG_LANES_CPP = os.path.join(_REPO_PARENT, "hyperGuitar", "bela",
                            "hyperGuitar6chPan", "hg_lanes.cpp")

pytestmark = pytest.mark.skipif(
    not os.path.exists(HG_LANES_CPP),
    reason="hyperGuitar tree not checked out beside DEMIURGE_OS",
)


def _read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def _cpp_lane_labels():
    """[(lane_name, [entry labels])] parsed from hg_lanes.cpp, in lane order.

    Parsed rather than hand-listed on purpose: a test that restates the table
    is just a fourth copy to drift.
    """
    src = _read(HG_LANES_CPP)

    # static const HgEntry gXxxEntries[] = { { FX, p1, p2, "LABEL" }, ... };
    tables = {}
    for m in re.finditer(r"static const HgEntry (\w+)\[\]\s*=\s*\{(.*?)\n\};",
                         src, re.S):
        name, body = m.group(1), m.group(2)
        tables[name] = re.findall(r'"([^"]+)"\s*\}', body)

    # const HgLane gHgLane[HG_NLANES] = { { "NAME", KIND, gXxxEntries, ... }, };
    block = re.search(r"const HgLane gHgLane\[HG_NLANES\]\s*=\s*\{(.*?)\n\};",
                      src, re.S)
    assert block, "gHgLane[] table not found in hg_lanes.cpp"
    lanes = re.findall(r'\{\s*"([^"]+)"\s*,\s*\w+\s*,\s*(\w+)\s*,', block.group(1))
    assert lanes, "no lane rows parsed from gHgLane[]"
    return [(nm, tables[tbl]) for nm, tbl in lanes]


def test_five_lanes_everywhere():
    cpp = _cpp_lane_labels()
    assert len(cpp) == 5, "five NEPTR encoders, five lanes"
    assert len(UI.HG_LANES) == 5


def test_ui_labels_match_the_bela_exactly():
    cpp = _cpp_lane_labels()
    for i, (name, entries) in enumerate(cpp):
        ui_lane = UI.HG_LANES[i]
        assert ui_lane["name"] == name, (
            f"lane {i}: UI calls it {ui_lane['name']!r}, hg_lanes.cpp calls it "
            f"{name!r}"
        )
        assert ui_lane["entries"] == entries, (
            f"lane {i} ({name}) entry labels differ:\n"
            f"  UI  : {ui_lane['entries']}\n"
            f"  Bela: {entries}"
        )


def test_engine_lane_lengths_match_the_bela():
    cpp = _cpp_lane_labels()
    src = _read(HG_ORC)
    m = re.search(r"giHgLaneLen\[\]\s*fillarray\s*([0-9,\s]+)", src)
    assert m, "giHgLaneLen[] not found in csound/hyperguitar.orc"
    lens = [int(n) for n in m.group(1).split(",")]
    assert lens == [len(e) for _, e in cpp], (
        f"csound giHgLaneLen{lens} disagrees with hg_lanes.cpp "
        f"{[len(e) for _, e in cpp]} — a rotation would wrap at the wrong "
        f"point and the lane would look stuck."
    )


def test_menu_index_agrees_between_engine_and_ui():
    src = _read(HG_ORC)
    m = re.search(r"#define\s+HG_MENU\s+#(\d+)#", src)
    assert m, "HG_MENU not defined in csound/hyperguitar.orc"
    assert int(m.group(1)) == UI.HG_MENU


def test_lane_to_cc_mapping_is_the_uis_own_row_mapping():
    """Lane N must use the CCs the UI already assigns to row N.

    The engine's menu-21 branches were inserted into specific dispatch files by
    hand; this pins that hand-work against the UI's existing row->CC tables,
    which are independently derived. Lane 0 is the LEFTMOST encoder, which is
    hardware slot 4 — the two orderings are reversed and that is exactly the
    kind of thing that gets wired backwards.
    """
    assert UI._ROW_BTN_LOGICAL == [72, 71, 76, 75, 74]
    assert UI._ROW_ROT_LOGICAL == [22, 21, 26, 25, 24]

    dispatch = os.path.join(ROOT, "csound", "handler", "dispatch")
    tog_files = ["toggle_slot4.orc", "toggle_slot3.orc", "toggle_slot2.orc",
                 "toggle_slot1.orc", "toggle_slot0.orc"]
    rot_files = ["rot_slot4_cc22.orc", "rot_slot3_cc21.orc",
                 "rot_slot2_cc26.orc", "rot_slot1_cc25.orc",
                 "rot_slot0_cc24.orc"]

    # REVERSED 2026-09-15: lane N is hardware slot N on this page (identity),
    # not 4-N as every other menu uses. The player reported the encoder order
    # came out backwards. UI.HG_ROW_SLOT carries the same reversal and both are
    # checked here so they cannot drift apart.
    assert UI.HG_ROW_SLOT == [4, 3, 2, 1, 0]

    tog_by_lane = ["toggle_slot0.orc", "toggle_slot1.orc", "toggle_slot2.orc",
                   "toggle_slot3.orc", "toggle_slot4.orc"]
    rot_by_lane = ["rot_slot0_cc24.orc", "rot_slot1_cc25.orc",
                   "rot_slot2_cc26.orc", "rot_slot3_cc21.orc",
                   "rot_slot4_cc22.orc"]

    for lane, fn in enumerate(tog_by_lane):
        src = _read(os.path.join(dispatch, fn))
        assert f"$HG_TOGGLE({lane})" in src, (
            f"lane {lane} toggle branch missing from {fn}")

    # lane 0 is the pedal-driven glide and has NO encoder behaviour
    src0 = _read(os.path.join(dispatch, rot_by_lane[0]))
    assert "$HG_ROTATE(0'" not in src0, (
        "lane 0 (PITCH EXP) must have no rotation behaviour — it is driven by "
        "the expression pedal and its encoder is deliberately inert")
    for lane, fn in enumerate(rot_by_lane):
        if lane == 0:
            continue
        src = _read(os.path.join(dispatch, fn))
        assert f"$HG_ROTATE({lane}'" in src, (
            f"lane {lane} rotation branch missing from {fn}")


def test_every_lane_branch_is_gated_on_presence():
    """No menu-21 branch may send without checking gkHgPresent.

    With no guitar attached the engine would otherwise fire UDP at
    192.168.7.2 on every encoder move. Harmless in effect, but it is work on
    the audio thread for an instrument that is not there.
    """
    src = _read(HG_ORC)
    for macro in ("HG_TOGGLE", "HG_ROTATE"):
        body = re.search(r"#define\s+" + macro + r"\([^)]*\)\s*#(.*?)\n#",
                         src, re.S)
        assert body, f"{macro} macro not found"
        assert "gkHgPresent == 1" in body.group(1), (
            f"{macro} sends without a presence check")


def test_presence_instrument_exists_and_is_always_on():
    src = _read(HG_LINK_ORC)
    assert "instr 98" in src
    assert "schedule 98, 0, -1" in src, (
        "instr 98 must be always-on: presence is evaluated every k-cycle, not "
        "only when a CC arrives."
    )


def test_presence_is_rebroadcast_periodically_not_only_on_change():
    """A late-joining UI must still learn the guitar is there.

    Measured failure, 2026-09-15: engine up 18:07:22, heartbeat ~1 s later,
    gkHgPresent 0->1 fired its single change-triggered /hg_present — and the UI
    did not start until 18:10:57. It missed the edge, never set hg_present, and
    clamped every menu-21 echo back to IR VERB. Reported as "not seeing my
    hyperguitar menu even tho it is plugged in", with the link itself perfectly
    healthy end to end.

    Engine and UI restart independently, so any startup ordering has to
    converge. Presence is re-stated on a timer as well as on change.
    """
    src = _read(HG_LINK_ORC)
    assert "$OSC_I(gkHgPresent'/hg_present'hgPres)" in src, (
        "the change-triggered presence send should stay — it reports a real "
        "transition immediately instead of up to half a second late")
    assert re.search(r'OSC3x_i\s+"/hg_present"', src), (
        "presence must ALSO be re-broadcast on a timer, or a UI that starts "
        "after the engine never learns the guitar is present")
    assert re.search(r"kPresTick\s*>=\s*kr\s*/\s*2", src), (
        "the presence re-broadcast tick is missing or no longer derived from "
        "kr (it must stay rate-independent)")


def test_sync_request_also_resends_lane_state():
    """Presence alone is not enough for a late-joining UI: it needs the lanes."""
    src = _read(HG_LINK_ORC)
    i_refresh = src.index("gkOscRefresh == 1")
    tail = src[i_refresh:]
    assert '"/hg_lane"' in tail, (
        "a gkOscRefresh sync must re-state the lane mirror to the UI")
    assert "HgSendSync" in tail, (
        "a sync must also ask the guitar to re-send, so the mirror itself "
        "cannot be the stale thing")


def test_dynamics_lane_order_autowah_first_swell_last():
    """User's explicit ordering for the DYNAMICS lane."""
    cpp = _cpp_lane_labels()
    dyn = cpp[3][1]
    assert dyn[0].startswith("WAH"), f"autowah must be first, got {dyn[0]}"
    assert dyn[-1] == "POG SWELL", f"the swell must be last, got {dyn[-1]}"


def test_lane_roles_match_the_spec():
    cpp = _cpp_lane_labels()
    names = [n for n, _ in cpp]
    assert names == ["PITCH EXP", "PS STEPS", "FORMANT", "DYNAMICS", "SYNTH"]


def test_dispatch_branches_use_hg_menu_macro_not_a_literal():
    """Menu 20 is NORRIS (no engine branches); hyperGuitar is $HG_MENU (21).
    A literal `gkCurrentMenu == 20` in dispatch would re-point at NORRIS."""
    dispatch = os.path.join(ROOT, "csound", "handler", "dispatch")
    for fn in sorted(os.listdir(dispatch)):
        if fn.endswith(".orc"):
            assert not re.search(r"gkCurrentMenu\s*==\s*20\b", _read(os.path.join(dispatch, fn))), fn
