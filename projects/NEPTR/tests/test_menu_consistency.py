"""Menu / toggle / OSC consistency guarantee (user-mandated).

The UI (neptrPhase4_UI.py) and the Csound handler (csound/handler/*.orc) are
two hand-maintained tables describing the SAME thing: which effect lives in
which menu slot, what its params are called, and what it publishes over OSC.
They drift silently — a new effect wired in Csound but missing from MENUS just
never appears; an OSC address the UI does not route just vanishes.

This module asserts they agree. It is a structural test: it parses the .orc
files as text (no Csound needed) and imports the UI module for its tables.

Checks
  1. every MENUS code has an EFFECT_PARAMS row  (minus the documented exempt
     classes: the "—" placeholder, looper lp*, NAM nam*, IR ir*, preset-only
     codes)
  2. every EFFECT_PARAMS code appears in MENUS  (minus retired legacy codes)
  3. FX_TOGGLE toggle IDs are a bijection with their bypass globals in
     handler_menus.orc — one ID per effect, one effect per ID (an effect MAY
     appear in several menus, e.g. BASS PRE L in both PREAMP and BASS)
  4. every /<code>_state OSC send in handler_osc.orc (plus the effect-local
     allowlist) is routed by the UI — explicitly or via the generic handler
  5. the docs' toggle-ID registry lists every toggle ID the handler fires
  6. MENU_COUNT matches len(MENUS) and the wrap modulus in handler_menus.orc
  7. every STEP_LABELS entry names a real (code, param) pair
"""

import os
import re

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pytest

import neptrPhase4_UI as UI

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HANDLER_MENUS = os.path.join(ROOT, "csound", "handler", "handler_menus.orc")
HANDLER_OSC = os.path.join(ROOT, "csound", "handler", "handler_osc.orc")
# Menu prev/next navigation (and its wrap modulus) live in the split-out
# dispatch/nav.orc since the 2026-08-12 dispatch rearchitecture.
HANDLER_NAV = os.path.join(ROOT, "csound", "handler", "dispatch", "nav.orc")
DOCS = os.path.join(ROOT, "docs", "neptr_full_reference.md")


# --- exemption tables (keep these SMALL and justified) ----------------------

# Menu codes that legitimately have no EFFECT_PARAMS row.
#   lp*   LOOPER channels — a bespoke renderer (_render_looper), not sliders
#   nam*/ir* — row browsers; they DO have rows, but are matched by prefix below
#   tgl   TOTAL GLITCH — a bass-preset bundle button, not a param-bearing effect
NO_PARAMS_OK = {"—", "tgl"}
PREFIX_EXEMPT = ("lp",)

# EFFECT_PARAMS rows kept for retired/legacy preset codes that no longer appear
# in any menu. Do not grow this list — delete the row instead.
LEGACY_PARAM_ROWS = {"cln", "drt"}

# OSC /<code>_state sends whose <code> is not a UI effect code but IS routed
# explicitly in route_osc(). Keep in sync with route_osc's explicit branches.
EXPLICIT_STATE_ROUTES = {
    "preamp",   # -> ui.handle_state_change('pre', ...)
    "tgl",      # -> bass PRESETS slot
    "blp",      # transport state, not a bypass flag
    "rec",      # -> ui.recording (UTILITY page master RECORD indicator)
}

# Non-param OSC sends that live on an EFFECT code's namespace but drive bespoke
# UI machinery, not a param slider (so they legitimately have no EFFECT_PARAMS
# row). Routed explicitly in route_osc(). The POLY DLY (mvd) v5 repeats +
# pitch-interval echoes (touch-only absolute CC110/CC111).
NON_PARAM_OSC = {
    "/mvd_reps", "/mvd_intv",
}

# OSC sends emitted from an effect .orc rather than handler_osc.orc.
# (2026-07-13: none — every effect publishes through handler_osc.orc. If you
# add a local OSC3x_* send inside an effect file, list its address here.)
EFFECT_LOCAL_OSC = set()


def _read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


@pytest.fixture(scope="module")
def menus_src():
    # 2026-09-25: the $FX_TOGGLE / $GLITCH_PRESS call sites moved out of
    # handler_menus.orc into handler/dispatch/toggle_slot*.orc in the
    # 2026-08-12 dispatch split; this fixture never followed, so the toggle
    # tests below had been passing an EMPTY scan into their assertions and
    # failing on "no $FX_TOGGLE found". Scan the handler AND the dispatch dir.
    import glob
    src = _read(HANDLER_MENUS)
    for f in sorted(glob.glob(os.path.join(ROOT, "csound", "handler", "dispatch", "*.orc"))):
        src += "\n" + _read(f)
    return src


@pytest.fixture(scope="module")
def osc_src():
    return _read(HANDLER_OSC)


@pytest.fixture(scope="module")
def docs_src():
    return _read(DOCS)


def _all_menu_codes():
    """Every effect code reachable from a menu slot, including the bass bank."""
    codes = set()
    for menu in UI.MENUS:
        for code, _name in menu["effects"]:
            codes.add(code)
    for code, _name in UI.BASS_PRESETS:
        codes.add(code)
    return codes


def _preset_codes():
    """Menu-0 codes (both banks) — presets, not ordinary effects."""
    codes = {c for c, _ in UI.MENUS[0]["effects"]}
    codes |= {c for c, _ in UI.BASS_PRESETS}
    return codes


# --- 1. MENUS -> EFFECT_PARAMS ---------------------------------------------

def test_every_menu_code_has_effect_params():
    missing = []
    presets = _preset_codes()
    for code in sorted(_all_menu_codes()):
        if code in NO_PARAMS_OK or code.startswith(PREFIX_EXEMPT):
            continue
        if code in presets and code not in UI.EFFECT_PARAMS:
            # Preset-only buttons (state, no params) are allowed to skip a row.
            continue
        if code not in UI.EFFECT_PARAMS:
            missing.append(code)
    assert not missing, (
        f"MENUS codes with no EFFECT_PARAMS row: {missing}. Add a row (the "
        f"effect view would render blank) or exempt it deliberately."
    )


# --- 2. EFFECT_PARAMS -> MENUS ---------------------------------------------

def test_every_effect_params_code_is_reachable_from_a_menu():
    menu_codes = _all_menu_codes()
    orphans = sorted(
        code for code in UI.EFFECT_PARAMS
        if code not in menu_codes and code not in LEGACY_PARAM_ROWS
    )
    assert not orphans, (
        f"EFFECT_PARAMS rows unreachable from any menu slot: {orphans}. "
        f"Either give the effect a menu slot or delete the row."
    )


# --- 3. FX_TOGGLE ID uniqueness --------------------------------------------

FX_TOGGLE_RE = re.compile(r"\$FX_TOGGLE\((gk\w+)'(\d+)\)")
GLITCH_PRESS_RE = re.compile(r"\$GLITCH_PRESS\((gk\w+)'[^']*'[^']*'[^']*'(\d+)\)")


def _toggle_pairs(menus_src):
    """(bypass_global, toggle_id) for every toggle the handler can fire."""
    pairs = FX_TOGGLE_RE.findall(menus_src)
    pairs += GLITCH_PRESS_RE.findall(menus_src)
    return [(g, int(i)) for g, i in pairs]


def test_toggle_ids_are_a_bijection_with_their_globals(menus_src):
    pairs = _toggle_pairs(menus_src)
    assert pairs, "no $FX_TOGGLE found — did handler_menus.orc move?"

    by_id = {}
    by_global = {}
    clashes = []
    for gvar, tid in pairs:
        # An effect may appear in several menus (BASS PRE L is in PREAMP and
        # BASS), so the SAME (global, id) pair repeating is fine. What is never
        # fine is one id used by two globals, or one global using two ids.
        if by_id.setdefault(tid, gvar) != gvar:
            clashes.append(f"toggle {tid} used by both {by_id[tid]} and {gvar}")
        if by_global.setdefault(gvar, tid) != tid:
            clashes.append(f"{gvar} fires both toggle {by_global[gvar]} and {tid}")

    assert not clashes, "toggle-ID collisions in handler_menus.orc:\n  " + "\n  ".join(clashes)


def test_new_batch_toggles_are_present_and_unclaimed_elsewhere(menus_src):
    """The 2026-07-13 ten-effect batch owns 57-65. Guard the assignment."""
    expected = {
        57: "gkCMP_bypass", 58: "gkTEQ_bypass",
        # 59 = TAPESTOP, REMOVED 2026-07-13 (see docs §11); it must stay
        # unclaimed so a future effect cannot silently inherit the slot.
        59: None,
        60: "gkCFB_bypass", 61: "gkTSY_bypass", 62: "gkPLL_bypass",
        63: "gkFBK_bypass", 64: "gkDIF_bypass",
        # 65 = SHIMMER in the 2026-07-13 batch; it has since moved to 71 as
        # gkShmBypass (DIGIVERB menu). 65 must stay unclaimed like 59.
        65: None,
    }
    by_id = dict((tid, g) for g, tid in _toggle_pairs(menus_src))
    for tid, gvar in expected.items():
        if gvar is None:
            assert tid not in by_id, f"toggle {tid} is retired but {by_id.get(tid)} claims it"
            continue
        assert by_id.get(tid) == gvar, (
            f"toggle {tid} should be {gvar}, found {by_id.get(tid)!r}")


# --- 4. OSC state sends are routed by the UI --------------------------------

OSC_STATE_RE = re.compile(r"'(/(\w+)_state)'")


def test_every_osc_state_send_is_routed(osc_src):
    addrs = OSC_STATE_RE.findall(osc_src)
    assert addrs, "no /<code>_state sends found — did handler_osc.orc move?"

    unrouted = []
    for _addr, code in sorted(set(addrs)):
        if code in EXPLICIT_STATE_ROUTES:
            continue
        # Generic route_osc branch: `if param == 'state' and code in ALL_EFFECTS`
        if code in UI.ALL_EFFECTS:
            continue
        unrouted.append(code)
    assert not unrouted, (
        f"/<code>_state sent by Csound but not routed by the UI: {unrouted}. "
        f"Add the code to a MENUS row (generic handling) or give route_osc an "
        f"explicit branch and list it in EXPLICIT_STATE_ROUTES."
    )


def test_every_osc_param_send_has_an_effect_params_row(osc_src):
    """Non-state $OSC_F/$OSC_I addresses should land on a known param, so the
    effect view actually shows them. Addresses for machinery the UI handles by
    hand (menu, preset, looper, nam/ir selectors, ...) are skipped."""
    addrs = re.findall(r"'(/\w+)'", osc_src)
    skip_prefixes = (
        "/menu", "/last_toggle", "/preset", "/nam_", "/ir_", "/lp",
        "/stereo_in", "/glt_", "/preamp_",
    )
    unknown = []
    for addr in sorted(set(addrs)):
        if addr.endswith("_state") or addr.startswith(skip_prefixes):
            continue
        if addr in EFFECT_LOCAL_OSC or addr in NON_PARAM_OSC:
            continue
        code, _, param = addr[1:].partition("_")
        if not param:
            continue
        row = UI.EFFECT_PARAMS.get(code)
        if row is None:
            continue     # not an effect namespace (e.g. bespoke machinery)
        if not any(p[0] == param for p in row):
            unknown.append(addr)
    assert not unknown, (
        f"OSC params sent for a known effect but absent from its EFFECT_PARAMS "
        f"row (the UI drops them silently): {unknown}"
    )


# --- 5. docs registry lists every toggle ------------------------------------

def test_docs_registry_lists_every_toggle(menus_src, docs_src):
    m = re.search(r"\*\*Toggle ID registry:\*\*(.+?)\*\*Next free", docs_src, re.S)
    assert m, "toggle-ID registry block not found in docs/neptr_full_reference.md"
    documented = {int(n) for n in re.findall(r"(?:^|·)\s*(\d+)\s", m.group(1))}

    fired = {tid for _g, tid in _toggle_pairs(menus_src)}
    missing = sorted(fired - documented)
    assert not missing, (
        f"toggles fired by handler_menus.orc but absent from the docs registry "
        f"(§11): {missing}"
    )


# --- 6. menu count agreement ------------------------------------------------

def test_menu_count_matches_handler_wrap():
    """The nav wrap is DYNAMIC as of 2026-09-15.

    It used to be a literal `% 20`. hyperGuitar's menu (21) only exists while
    the instrument is plugged in and answering its heartbeat, so nav.orc now
    wraps at `kMenuN = 21 + gkHgPresent` — 21 normally, 22 with the guitar
    attached. The base still has to agree with the UI's fixed menu list, or
    nav lands on a menu that does not exist.
    """
    # UI's ALWAYS-PRESENT menus: 0-18 effects + 19 = UTILITY + 20 = NORRIS. HYPERGUITAR is
    # appended conditionally and is deliberately not in this count.
    assert UI.MENU_COUNT == len(UI.MENUS) == 23
    nav_src = _read(HANDLER_NAV)

    # 2026-10-04: the wrap is DATA — nav.orc reads gkMenuCycleN (live length
    # of giMenuCycle, generated from menus.json by tools/gen_menus.py and
    # reloadable over OSC). The base count must still agree with the UI.
    assert "kMenuN = gkMenuCycleN" in nav_src, (
        "dispatch/nav.orc must take its wrap from gkMenuCycleN (menus.json)")
    gen_src = _read(os.path.join(ROOT, "csound", "handler", "gen", "menu_cycle.orc"))
    n = re.search(r"giMenuCycleN\s*=\s*(\d+)", gen_src)
    assert n, "giMenuCycleN not found in handler/gen/menu_cycle.orc"
    assert int(n.group(1)) == UI.MENU_COUNT + 1, (
        f"menu_cycle.orc lists {n.group(1)} menus but the UI has {UI.MENU_COUNT} "
        f"always-present menus + HYPERGUITAR — regenerate (tools/gen_menus.py) or fix the UI."
    )

    # 2026-09-25: the foot cycle is TABLE-DRIVEN (giMenuCycle in globals.orc)
    # so the five performance menus sit at the end of the lap. The contract
    # that replaces the old literal-wrap check:
    #   * the table lists every always-present menu 0..MENU_COUNT-1 exactly
    #     once, then HYPERGUITAR (MENU_COUNT) last;
    #   * both nav branches index it modulo kMenuN (the dynamic count), so
    #     HYPERGUITAR is reachable in both directions only while present.
    m = re.search(r"giMenuCycle\s+ftgen\s+0,\s*0,\s*-?\d+,\s*-2,\s*([\d,\s]+)", gen_src)
    assert m, "giMenuCycle table not found in handler/gen/menu_cycle.orc"
    cycle = [int(x) for x in m.group(1).replace(" ", "").split(",") if x][:int(n.group(1))]
    assert sorted(cycle[:UI.MENU_COUNT]) == list(range(UI.MENU_COUNT)), (
        f"giMenuCycle must walk every always-present menu exactly once; got {cycle}"
    )
    assert cycle[UI.MENU_COUNT] == UI.MENU_COUNT, "HYPERGUITAR must be the LAST entry"
    # 2026-10-24 order: PRESETS, UTILITY, then the FX menus 1..13, then
    # NORRIS, IR VERB, NAM, CHASE BLISS, GLITCH, LOOPER, MIXER, VOCAL, HYPERGUITAR.
    assert cycle == [0, 19, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 18, 17,
                     15, 16, 14, 22, 21, 23], f"menu cycle order changed: {cycle}"
    wraps = re.findall(r"tab:k\(\(kCycPos\s*[-+]\s*1(?:\s*\+\s*kMenuN)?\)\s*%\s*(\w+),\s*giMenuCycle\)", nav_src)
    assert len(wraps) == 2 and set(wraps) == {"kMenuN"}, (
        f"both nav branches must index giMenuCycle modulo kMenuN; found {wraps}"
    )


def test_hyperguitar_menu_index_is_appended_not_inserted():
    """Menu 21 must be the LAST index, one past the UI's fixed list.

    handler_looper.orc hardcodes menu 14 and a dozen dispatch branches test
    literal menu numbers, so inserting a menu anywhere but the end silently
    re-points all of them.
    """
    hg_src = _read(os.path.join(ROOT, "csound", "hyperguitar.orc"))
    m = re.search(r"#define\s+HG_MENU\s+#(\d+)#", hg_src)
    assert m, "HG_MENU not defined in csound/hyperguitar.orc"
    assert int(m.group(1)) == UI.MENU_COUNT, (
        f"HG_MENU is {m.group(1)} but the UI has {UI.MENU_COUNT} fixed menus; "
        f"hyperGuitar must be appended at index {UI.MENU_COUNT}."
    )


def test_every_menu_has_five_slots():
    for i, menu in enumerate(UI.MENUS):
        assert len(menu["effects"]) == 5, f"menu {i} ({menu['name']}) has {len(menu['effects'])} slots, expected 5"


# --- 7. STEP_LABELS point at real params ------------------------------------

def test_step_labels_reference_real_params():
    bad = []
    for (code, param), labels in UI.STEP_LABELS.items():
        row = UI.EFFECT_PARAMS.get(code)
        if row is None or not any(p[0] == param for p in row):
            bad.append((code, param))
            continue
        # The renderer indexes labels with int(round(value * scale)); the param's
        # declared max must reach the last label and not overshoot it.
        _n, _lab, _lo, hi = next(p for p in row if p[0] == param)
        scale = UI.STEP_SCALE.get((code, param), 1)
        assert round(hi * scale) == len(labels) - 1, (
            f"STEP_LABELS[{code!r},{param!r}] has {len(labels)} labels but the "
            f"param range tops out at index {round(hi * scale)}")
    assert not bad, f"STEP_LABELS entries with no matching EFFECT_PARAMS param: {bad}"
