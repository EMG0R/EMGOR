"""The DUODECIMUS expression pedal works. Keep it that way.

2026-09-15 was the FIRST time this pedal behaved correctly. Three separate
things have to stay true for it to keep working, and each has already broken
once. This file pins all three.

  1. THE FUNNEL. midi_handler.orc rewrites the foot controller's ch7 CC14/15
     into ch1 CC29 BEFORE the dispatch gate, so the treadle drives the same
     expression path as the onboard pedal with identical scaling and the same
     soft-takeover. (Nothing was ever wrong with the firmware; a stale note
     claiming a reflash was needed sent an earlier session down a dead end.)

  2. CC29 HAS ONE OWNER. The onboard pedal must never also write ch1 CC29.
     expr_cc29.orc's soft-takeover disarms on every owner change and re-arms
     only after real movement, so two writers produce a pedal that
     intermittently ignores the player — a symptom that reads as flaky
     hardware.

  3. PRESET RECALL MUST NOT RE-POINT IT. gkLastToggle29 (slot 304) is the
     pedal's sticky owner and is excluded from recall. This was the actual
     bug: every occupied bank slot carried target 1 (VOLUME PEDAL) instead of
     30 (gkMasterVol, the reverb-swell stage), so recalling any foot preset
     silently stole the pedal.
"""
import os
import re
import sys

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSOUND = os.path.join(ROOT, "csound")
MIDI_HANDLER = os.path.join(CSOUND, "midi_handler.orc")
EXPR_CC29 = os.path.join(CSOUND, "handler", "dispatch", "expr_cc29.orc")
EXCL_SAVE = os.path.join(CSOUND, "state_preset_exclude_save.inc.orc")
EXCL_REST = os.path.join(CSOUND, "state_preset_exclude_restore.inc.orc")

# demiurge-io lives outside the neptr tree
_DEMIURGE = os.path.dirname(ROOT)
TEENSY_SRC = os.path.join(_DEMIURGE, "src", "demiurge-io",
                          "demiurge_io", "sources", "teensy.py")


def _read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


# ------------------------------------------------------- 1. the funnel

def test_duodecimus_treadle_is_funnelled_to_ch1_cc29():
    src = _read(MIDI_HANDLER)
    # ch7 CC14/15 -> ch1 CC29, as a single rewrite before the gate
    assert re.search(r"kCh\s*==\s*7\s*&&\s*\(kCC\s*==\s*14\s*\|\|\s*kCC\s*==\s*15\)",
                     src), "the ch7 CC14/15 expression funnel is gone from midi_handler.orc"
    assert re.search(r"kCh\s*=\s*1", src)
    assert re.search(r"kCC\s*=\s*29", src)


def test_funnel_runs_before_the_channel_7_gate():
    """Order matters: the gate drops every remaining ch7 message.

    If the rewrite ever moves below `kCh != 7`, the treadle is discarded and
    the pedal goes completely dead.
    """
    src = _read(MIDI_HANDLER)
    i_funnel = src.index("kCC == 14 || kCC == 15")
    i_gate = src.index("kCh != 7")
    assert i_funnel < i_gate, (
        "the ch7->ch1 expression funnel must come BEFORE the `kCh != 7` gate, "
        "or the treadle is dropped before it is ever rewritten."
    )


# ------------------------------------------- 2. CC29 has exactly one owner

def test_onboard_pedal_is_not_allowed_on_cc29():
    """The onboard pedal's destination constant must not be 29."""
    if not os.path.exists(TEENSY_SRC):
        pytest.skip("demiurge-io not present")
    src = _read(TEENSY_SRC)
    m = re.search(r"^ONBOARD_EXPR_CC\s*=\s*(\d+)", src, re.M)
    assert m, "ONBOARD_EXPR_CC is not declared in sources/teensy.py"
    assert int(m.group(1)) != 29, (
        "ch1 CC29 belongs to the DUODECIMUS treadle. Two writers on CC29 fight "
        "through expr_cc29.orc's soft-takeover and the pedal starts ignoring "
        "the player intermittently."
    )
    assert int(m.group(1)) == 30


def test_teensy_source_never_emits_cc29():
    """No CC-emitting table in the serial source may contain 29.

    The onboard pedal arrives over SERIAL (the NEPTR2 frame), not MIDI, so this
    is the only place it could acquire a CC.
    """
    if not os.path.exists(TEENSY_SRC):
        pytest.skip("demiurge-io not present")
    src = _read(TEENSY_SRC)
    for name, lo, count in (("ENCODER_CCS", 20, 7),
                            ("BUTTON_CCS", 60, 9),
                            ("ENC_BTN_CCS", 70, 7)):
        assert 29 not in range(lo, lo + count), (
            f"{name} spans CC29, which belongs to the DUODECIMUS treadle")


# --------------------------------- 3. preset recall must not re-point it

def test_preset_recall_does_not_steal_the_pedal():
    for path in (EXCL_SAVE, EXCL_REST):
        src = _read(path)
        assert "gkLastToggle29" in src, (
            f"gkLastToggle29 must stay excluded from preset recall ({path}); "
            f"without it every foot-preset recall re-points the expression "
            f"pedal to whatever was live when the preset was captured."
        )
        assert "; slot 304" in src


def test_soft_takeover_still_guards_the_pedal():
    """The stale-slam guard must survive: it is what stops an owner change
    writing the pedal's resting position into the new target."""
    src = _read(EXPR_CC29)
    assert "kExprArmed" in src
    assert "gkLastToggle29 != kExprOwnPrv" in src, (
        "the soft-takeover owner-change check is gone from expr_cc29.orc")


# --------------------------- 4. the onboard pedal cannot contend for CC29

def test_onboard_midi_pedal_is_demoted_to_cc30():
    """The NEPTR body's own Teensy sends ch1 CC29 over USB-MIDI.

    demiurge-midi-connect.sh fans every physical port into Midi Through 14:0,
    which is the port this engine opens, so once the body is plugged in its
    pedal and the funnelled DUODECIMUS treadle are the same message from two
    sources. Reported live 2026-09-15: the treadle worked perfectly while the
    body was unplugged and started "tweaking again" the moment it was plugged
    in. The onboard one is parked on CC30, which no handler reads.
    """
    src = _read(MIDI_HANDLER)
    assert re.search(r"kFromOsc\s*==\s*0\s*&&.*kCh\s*==\s*1\s*&&\s*kCC\s*==\s*29",
                     src), "the onboard-pedal demotion is missing from midi_handler.orc"
    assert "kCC = 30" in src


def test_demotion_runs_before_the_duodecimus_funnel():
    """ORDER IS LOAD-BEARING.

    The funnel rewrites the treadle TO ch1 CC29. If the demotion ran after it,
    it would immediately demote the treadle too and kill the very pedal this
    whole mechanism exists to protect.
    """
    src = _read(MIDI_HANDLER)
    i_demote = src.index("kCC = 30")
    i_funnel = src.index("kCC == 14 || kCC == 15")
    assert i_demote < i_funnel, (
        "the onboard-pedal demotion must run BEFORE the ch7->ch1 CC29 funnel, "
        "or it demotes the DUODECIMUS treadle as well and both pedals go dead."
    )


def test_osc_cc_path_is_exempt_from_demotion():
    """The web panel's virtual NEPTR and demiurge-io push ch1 CC29 as /cc.

    Those are deliberate software writes with no second physical writer behind
    them, and testing-with-no-hardware (the normal case on a bare Pi) depends
    on them still reaching the expression dispatch.
    """
    src = _read(MIDI_HANDLER)
    assert "kFromOsc = 1" in src, "the OSC path no longer flags itself"
    assert re.search(r"kFromOsc\s*==\s*0", src), (
        "the demotion must be gated on kFromOsc == 0 so OSC writes are exempt")


def test_cc30_has_no_handler():
    """CC30 must stay unread, or 'parked' silently becomes 'does something'."""
    import glob
    hits = []
    for path in glob.glob(os.path.join(CSOUND, "**", "*.orc"), recursive=True):
        body = _read(path)
        if re.search(r"kCC\s*==\s*30\b", body):
            hits.append(os.path.relpath(path, CSOUND))
    assert not hits, f"something now handles CC30: {hits}"


# ------------------- 5. no effect may silently steal the pedal by gesture

def test_freeze_does_not_claim_the_expression_pedal():
    """FREEZE engages by GESTURE, which makes a silent pedal claim indefensible.

    It used to set gkLastToggle29 = 72 on engage. The claim is sticky and never
    released, and nothing on screen shows who owns the pedal — so a single
    FREEZE engage at any point in a session permanently re-points the treadle
    at gkFrzExpr, and every later push past the 0.6 Schmitt threshold in
    freezeverb.orc opens the listen send and stacks live playing into the
    frozen bed. Reported 2026-09-15: the pedal "randomly activating the freeze
    effect ... whenever i push it down".
    """
    src = _read(os.path.join(CSOUND, "handler", "dispatch", "toggle_slot2.orc"))
    assert "gkLastToggle29 = 72" not in src, (
        "FREEZE must not claim the expression pedal: the claim is sticky, "
        "invisible, and survives the effect being switched off."
    )


GENERATIVE_IDS = {20, 23, 24, 25, 48, 56}   # BBCUT REPEAT CHOP RVRS IMPULSE BLOOPER


def test_generative_effects_never_claim_the_pedal_unguarded():
    """BLOOPER and the GLITCH-menu effects must not take the expression pedal
    on toggle-ON (2026-09-25). Every `gkLastToggle29 = <id>` for those ids in
    the dispatch files has to sit inside a `giPedalClaimGenerative == 1` guard,
    and that global must default to 0."""
    import glob
    for f in glob.glob(os.path.join(CSOUND, "handler", "dispatch", "toggle_slot*.orc")):
        lines = open(f).read().split("\n")
        for i, ln in enumerate(lines):
            m = re.match(r"^\s*gkLastToggle29\s*=\s*(\d+)\b", ln)
            if m and int(m.group(1)) in GENERATIVE_IDS:
                window = "\n".join(lines[max(0, i - 2):i])
                assert "giPedalClaimGenerative == 1" in window, (
                    f"{os.path.basename(f)}:{i+1} claims the pedal for generative id "
                    f"{m.group(1)} without the giPedalClaimGenerative guard")
    g = open(os.path.join(CSOUND, "globals.orc")).read()
    assert re.search(r"^giPedalClaimGenerative\s+init\s+0\s*$", g, re.M), "guard must default to 0"
