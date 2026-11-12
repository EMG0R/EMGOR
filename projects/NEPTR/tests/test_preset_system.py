"""Tests for the IN-ENGINE NEPTR preset system (2026-09-09).

The preset bank now lives entirely in the Csound engine (state_persist.orc:
giPresetBank, 17 slots x 512 = giNeptrState snapshots, per-context persistence).
The old python daemon (src/duodecimus) and demiurge-preset OSC CLI are RETIRED.

What's still host-side and testable here:
  * preset_gesture — the 5s-capture / 8s-delete hold classifier the UI uses to
    turn a touch-hold on the PRESETS page into a ch16 bank CC.
  * demiurge-preset-wipe — remove a context's bank file (backs up, keeps surf).

The engine bank ops (capture/recall/delete/persist/occupancy) are validated by
the csound --syntax-check-only pass; recall behaviour is confirmed on the Pi.
"""
import os
import subprocess
import sys

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NEPTR = os.path.join(ROOT, "neptr")
sys.path.insert(0, NEPTR)

import preset_gesture as pg  # noqa: E402


# ------------------------------------------------------------ gesture machine
def test_hold_select_on_short_tap():
    h = pg.PresetHold()
    h.start(3, 100.0)
    assert h.update(100.5) is None
    assert h.release(101.0) == "select"       # <5s


def test_hold_capture_between_5_and_8():
    h = pg.PresetHold()
    h.start(3, 0.0)
    assert h.update(4.9) is None
    assert h.release(5.5) == "capture"        # 5..8s


def test_hold_delete_at_8_and_no_capture_after():
    h = pg.PresetHold()
    h.start(7, 0.0)
    assert h.update(5.0) is None
    assert h.update(8.01) == "delete"         # terminal at 8s
    assert h.update(9.0) is None              # only once
    assert h.release(9.5) is None             # delete already fired


def test_hold_progress_stages():
    h = pg.PresetHold()
    h.start(1, 0.0)
    assert h.progress(2.5)[0] == "arming"
    assert h.progress(6.5)[0] == "capture_ready"
    assert h.progress(8.5)[0] == "delete_ready"
    assert pg.PresetHold().progress(0.0) == ("idle", 0.0)


def test_classify_release_time():
    assert pg.classify(0.1) == "select"
    assert pg.classify(4.99) == "select"
    assert pg.classify(5.0) == "capture"
    assert pg.classify(7.99) == "capture"
    assert pg.classify(8.0) == "delete"
    assert pg.classify(20.0) == "delete"


def test_hold_cancel():
    h = pg.PresetHold()
    h.start(1, 0.0)
    h.cancel()
    assert not h.active
    assert h.release(10.0) is None


# ---------------------------------------------------- preset exclusions (blooper+glitch)
import re

CSD = os.path.join(ROOT, "neptr", "csound")
EXCL_SAVE = os.path.join(CSD, "state_preset_exclude_save.inc.orc")
EXCL_REST = os.path.join(CSD, "state_preset_exclude_restore.inc.orc")


def _excl_pairs(path):
    """(global, slot) pairs from an exclusion include's `... ; slot N` lines."""
    pairs = []
    for ln in open(path):
        m = re.search(r'(gk\w+|voc_\w+).*?; (?:\w+ )?slot (\d+)', ln)
        if m:
            pairs.append((m.group(1), int(m.group(2))))
    return pairs


def test_excluded_slots_are_exactly_blooper_habit_glitch_menu_and_expr_owner():
    # BLOOPER 225-229 + HABIT 238-242 (2026-09-25: generative, sits before the
    # looper so it still records into loops) + GLITCH menu (IMPULSE/BBCUT/
    # REPEAT/CHOP/RVRS) 249-271, plus 302 = gkCurrentMenu (2026-09-25: a recall
    # must not move the rig off the menu the player is on) and 304 =
    # gkLastToggle29, the expression pedal's owner. 2026-10-03: plus 296 =
    # gkMasterVol, 305 = gkMasterOut, 306 = gkManualBpm (a recall never changes
    # loudness or tempo).
    save = _excl_pairs(EXCL_SAVE)
    slots = {s for _, s in save}
    assert slots == (set(range(225, 230)) | set(range(238, 243))
                     | set(range(249, 272)) | {296, 302, 304, 305, 306}
                     | set(range(319, 331))   # 2026-10-22 VOCAL chain voc_*
                     | set(range(272, 281)))   # 2026-11-01 AMP layer (NAM row/bypass/gains/variants)
    # NOT excluded: ONWARD (230-237) and MOOD II (243-248) must be absent
    assert not (slots & (set(range(230, 238)) | set(range(243, 249))))


def test_expr_pedal_owner_is_excluded_from_recall():
    """A preset recall must NOT re-point the expression pedal.

    gkLastToggle29 (slot 304) is the pedal's sticky owner. The documented
    ownership rule in expr_cc29.orc is LAST-TOGGLED-WINS and explicitly
    survives an effect being switched off — i.e. it is live performance
    state, not preset state.

    The 2026-09-09 full-state bank broke that rule by restoring whatever
    owner happened to be live at capture time. Measured on the user's rig
    2026-09-15: every occupied slot in gorcore's bank carried index 304 =
    2.0, i.e. target 1 (VOLUME PEDAL), so recalling ANY foot preset silently
    stole the pedal away from the menu-0 presets' declared target 30
    (gkMasterVol, the reverb-swell stage) — reported as "the expression
    pedal isn't working to my reverb preset".
    """
    save = dict((g, s) for g, s in _excl_pairs(EXCL_SAVE))
    rest = dict((g, s) for g, s in _excl_pairs(EXCL_REST))
    # 2026-11-01: still saved/restored, but ONLY for onboard slots 13..17 (the
    # restore is guarded by `kRSlot > 12`); DUODECIMUS slots 1..12 hand the
    # pedal to the preset's stored owner (VOLUME for the show presets).
    assert save.get("gkLastToggle29") == 304
    assert rest.get("gkLastToggle29") == 304
    assert "kRSlot > 12" in open(EXCL_REST).read()


def test_save_and_restore_cover_identical_globals():
    save = _excl_pairs(EXCL_SAVE)
    rest = _excl_pairs(EXCL_REST)
    assert save == rest                       # lockstep
    globals_ = [g for g, _ in save]
    assert any(g.startswith("gkBLP_") for g in globals_)      # blooper present
    assert any(g.startswith("gkIMP_") for g in globals_)      # glitch present
    assert len(globals_) == len(set(globals_)) == 50   # 38 + 12 voc_*


def test_recall_path_wraps_unpack_with_save_restore():
    sp = open(os.path.join(CSD, "state_persist.orc")).read()
    # in the recall block the save include precedes the unpack, restore follows
    i_save = sp.index("state_preset_exclude_save.inc.orc")
    i_unpack = sp.index("state_unpack.inc.orc", i_save)
    i_rest = sp.index("state_preset_exclude_restore.inc.orc", i_unpack)
    assert i_save < i_unpack < i_rest


# ------------------------------------------------------------ wipe script
WIPE = os.path.join(ROOT, "src", "wrappers", "demiurge-preset-wipe")


def _mkbank(root, ctx):
    d = os.path.join(root, ctx)
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "bank.ftab"), "wb").write(b"BANK")
    open(os.path.join(d, "bank.b.ftab"), "wb").write(b"BANKB")
    return d


def _run_wipe(root, *args):
    env = dict(os.environ, DEMIURGE_PRESET_DIR=root)
    return subprocess.run(["bash", WIPE, *args], env=env,
                          capture_output=True, text=True)


def test_wipe_gorcore_backs_up_and_keeps_surf(tmp_path):
    root = str(tmp_path)
    g = _mkbank(root, "gorcore")
    s = _mkbank(root, "surf")
    r = _run_wipe(root, "gorcore")
    assert r.returncode == 0, r.stderr
    assert not os.path.exists(os.path.join(g, "bank.ftab"))    # gorcore wiped
    assert not os.path.exists(os.path.join(g, "bank.b.ftab"))
    assert os.path.exists(os.path.join(s, "bank.ftab"))        # surf kept
    backups = os.path.join(root, "_backups")
    assert any(f.endswith(".tar.gz") for f in os.listdir(backups))


def test_wipe_surf_refused_without_force(tmp_path):
    root = str(tmp_path)
    s = _mkbank(root, "surf")
    r = _run_wipe(root, "surf")
    assert r.returncode == 2
    assert os.path.exists(os.path.join(s, "bank.ftab"))        # untouched


def test_wipe_surf_allowed_with_force(tmp_path):
    root = str(tmp_path)
    s = _mkbank(root, "surf")
    r = _run_wipe(root, "surf", "--force-surf")
    assert r.returncode == 0, r.stderr
    assert not os.path.exists(os.path.join(s, "bank.ftab"))


def test_wipe_missing_context_is_noop(tmp_path):
    r = _run_wipe(str(tmp_path), "bass")
    assert r.returncode == 0
    assert "nothing to wipe" in r.stdout


def test_preset_bank_path_is_independent_of_state_dir():
    """2026-09-25: STATE_DIR moved to tmpfs for real-time safety; the preset
    bank must NOT follow it (it did, via STATE_DIR/../presets, and every reboot
    started with an empty bank). SPreDir must come from PRESET_DIR only."""
    sp = open(os.path.join(CSD, "state_persist.orc")).read()
    m = re.search(r'SPreDir\s+sprintf\s+"([^"]*)",\s*(.*)', sp)
    assert m, "SPreDir definition not found"
    assert "PRESET_DIR" in m.group(2) and "SDir" not in m.group(2), m.group(0)
    assert re.search(r"#define PRESET_DIR #/home/gorpi7/demiurge/presets#", sp)
