"""PRESETS menu (0): five USER preset slots, gesture timed in the engine."""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSD = os.path.join(ROOT, "csound")


def _read(*p):
    with open(os.path.join(CSD, *p)) as fh:
        return fh.read()


def test_gesture_block_thresholds_and_slot_map():
    src = _read("handler", "handler_menus.orc")
    assert "kVal == 127 && gkCurrentMenu == 0" in src and "kVal == 0 && gkPgHeld == kPgSlot" in src
    assert "kPgDur >= 8" in src and "kPgDur >= 5" in src
    # logical CC72/71/76/75/74 = slots 1..5 (same row order as the UI)
    assert re.search(r"kCC == 72 \? 1 : \(kCC == 71 \? 2 : \(kCC == 76 \? 3 : \(kCC == 75 \? 4 : 5\)", src)
    ops = re.search(r"kPgDur >= 8\) then\s+gkPresetOp = 3\s+elseif \(kPgDur >= 5\) then\s+gkPresetOp = 1\s+else\s+gkPresetOp = 2", src)
    assert ops, "release<5 recall(2), 5..8 capture(1), >=8 delete(3)"


def test_old_onboard_presets_gone_from_menu0_dispatch():
    for i in range(5):
        src = _read("handler", "dispatch", "toggle_slot%d.orc" % i)
        assert "gkPresetSlot = 1" not in src.split("gkCurrentMenu == $MX_MENU")[0]
        assert "PRESET: AMBIENT" not in src


def test_osc_echoes_present():
    osc = _read("handler", "handler_osc.orc")
    assert "'/pocc'" in osc and "'/ploaded'" in osc
    assert '"/phold"' in _read("midi_handler.orc")
    assert "gkPrOccMask" in _read("state_persist.orc")
