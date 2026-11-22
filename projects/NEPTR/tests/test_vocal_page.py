"""VOCAL page (menu 21): UI table mirrors the engine CC map, echo + slider."""
import os
import re

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import neptrPhase4_UI as UI

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSD = os.path.join(ROOT, "csound")


def test_vocal_menu_and_hg_shift():
    assert UI.VOCAL_MENU == 21 and UI.MIXER_MENU == 22 and UI.HG_MENU == 23
    src = open(os.path.join(CSD, "hyperguitar.orc")).read()
    assert re.search(r"#define\s+HG_MENU\s+#23#", src)


def test_ranges_and_defaults_in_spec():
    by = {sp[0]: sp for sp in UI.VOC_PARAMS}
    assert (by["voc_hpf_hz"][3], by["voc_hpf_hz"][4]) == (40, 300)
    assert (by["voc_level"][3], by["voc_level"][4]) == (0, 4)
    for sp in UI.VOC_PARAMS:
        assert sp[3] <= sp[7] <= sp[4]
        assert UI.voc_from_frac(sp, UI.voc_to_frac(sp, sp[7])) - sp[7] < 1e-6 * max(1, abs(sp[7]))
    assert by["voc_level"][7] <= 1.0


def test_cc_map_matches_engine():
    src = open(os.path.join(CSD, "midi_handler.orc")).read()
    for sp in UI.VOC_PARAMS:
        assert re.search(r'kCC == %d\) then\s+chnset [^\n]*"%s"' % (sp[2], sp[0]), src), sp[0]
    assert UI.VOC_ON_CC == 40


def test_state_slots_in_lockstep():
    # voc_pre_db (PREAMP, first row) was added later and lives in slot 342
    ch = ["voc_on"] + [sp[0] for sp in UI.VOC_PARAMS if sp[0] != "voc_pre_db"]
    wr = open(os.path.join(CSD, "state_persist.orc")).read()
    un = open(os.path.join(CSD, "state_unpack.inc.orc")).read()
    for i, c in enumerate(ch):
        n = 319 + i
        assert re.search(r'chnget "%s"' % c, wr) and ("tab:k(%d, giNeptrState)" % n) in wr
        assert ('chnset kStV%d - 100, "%s"' % (i, c)) in un
    assert "tab:k(342, giNeptrState)" in wr and 'chnset kStPre - 100, "voc_pre_db"' in un
