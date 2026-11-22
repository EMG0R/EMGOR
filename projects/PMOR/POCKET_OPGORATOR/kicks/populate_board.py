"""
Pocket OpGorator — populate board with footprints from KiCad libraries
Run from terminal or KiCad console (exec(open(p).read()))
"""
import sys

KICAD_SITE = '/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages'
LIBS       = '/Applications/KiCad.app/Contents/SharedSupport/footprints'
BOARD_PATH = '/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/prototypes/Pocket-OpGorator/kicks/kicks.kicad_pcb'

if KICAD_SITE not in sys.path:
    sys.path.insert(0, KICAD_SITE)

import pcbnew

try:
    board = pcbnew.GetBoard()
    if not board.GetFileName():
        raise Exception()
except Exception:
    board = pcbnew.LoadBoard(BOARD_PATH)

def mm(v):     return pcbnew.FromMM(v)
def v2i(x, y): return pcbnew.VECTOR2I(mm(x), mm(y))

L = LIBS  # shorthand

# ── COMPONENT TABLE ──────────────────────────────────────────────────────────
# (ref, lib_dir.pretty, footprint_name, x_mm, y_mm, side, rot_deg, value)
COMPONENTS = [
    # ── Front side (F.Cu) ────────────────────────────────────────────────────
    ("J_IN",         "Connector_Audio",          "Jack_3.5mm_PJ311_Horizontal",                  18.75,  3.0,  "F",  0, "PJ-307 Audio IN"),
    ("J_OUT",        "Connector_Audio",          "Jack_3.5mm_PJ311_Horizontal",                  56.25,  3.0,  "F",  0, "PJ-307 Audio OUT"),
    ("SW1",          "Button_Switch_THT",        "SW_Slide_SPDT_Straight_CK_OS102011MS2Q",       56.25,  5.0,  "F",  0, "SS12D10-G5"),
    ("J_OLED",       "Connector_PinSocket_2.54mm","PinSocket_1x07_P2.54mm_Vertical",              3.0,  17.62, "F",  0, "SSD1309 OLED"),
    ("ENC1",         "Rotary_Encoder",           "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",18.75, 62.0,  "F",  0, "CYT1100 L"),
    ("ENC2",         "Rotary_Encoder",           "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",56.25, 62.0,  "F",  0, "CYT1100 R"),
    ("J_TOP_SENSOR", "Connector_PinSocket_2.54mm","PinSocket_1x06_P2.54mm_Vertical",             37.5,  62.0,  "F",  0, "VL53L1X"),
    ("J_SWITCH",     "Connector_PinSocket_2.54mm","PinSocket_1x02_P2.54mm_Vertical",             56.25,  7.0,  "F",  0, "SW Header"),
    ("J_AUX",        "Connector_PinSocket_2.54mm","PinSocket_1x04_P2.54mm_Vertical",             65.0,  90.0,  "F",  0, "AUX GPIO"),

    # ── Back side (B.Cu) ─────────────────────────────────────────────────────
    ("J_DAISY_L",    "Connector_PinSocket_2.54mm","PinSocket_1x20_P2.54mm_Vertical",             29.88, 47.48, "B",  0, "Daisy Seed L"),
    ("J_DAISY_R",    "Connector_PinSocket_2.54mm","PinSocket_1x20_P2.54mm_Vertical",             45.12, 47.48, "B",  0, "Daisy Seed R"),
    ("J_TRELLIS",    "Connector_PinSocket_2.54mm","PinSocket_1x04_P2.54mm_Vertical",              4.0,  82.0,  "B",  0, "NeoTrellis"),
    ("J_FSR",        "Connector_PinSocket_2.54mm","PinSocket_1x03_P2.54mm_Vertical",             71.0,  82.0,  "B",  0, "FSR"),
    ("J_MPU9250",    "Connector_PinSocket_2.54mm","PinSocket_1x10_P2.54mm_Vertical",             37.5,  64.0,  "B",  0, "MPU9250"),
    ("J_MAX4466",    "Connector_PinSocket_2.54mm","PinSocket_1x03_P2.54mm_Vertical",             18.75,  5.0,  "B",  0, "MAX4466"),
    ("J_TP4056",     "Connector_PinSocket_2.54mm","PinSocket_1x04_P2.54mm_Vertical",             15.0, 100.0,  "B",  0, "TP4056"),
    ("J_XL3608_IN",  "Connector_PinSocket_2.54mm","PinSocket_1x02_P2.54mm_Vertical",             55.0,  95.0,  "B",  0, "XL3608 IN"),
    ("J_XL3608_OUT", "Connector_PinSocket_2.54mm","PinSocket_1x02_P2.54mm_Vertical",             55.0, 103.0,  "B",  0, "XL3608 OUT"),
    ("J_PAM8403_L",  "Connector_PinSocket_2.54mm","PinSocket_1x05_P2.54mm_Vertical",             12.0, 115.0,  "B",  0, "PAM8403 L"),
    ("J_PAM8403_R",  "Connector_PinSocket_2.54mm","PinSocket_1x05_P2.54mm_Vertical",             32.0, 115.0,  "B",  0, "PAM8403 R"),
    ("J_SDCARD",     "Connector_PinSocket_2.54mm","PinSocket_1x06_P2.54mm_Vertical",             55.0, 115.0,  "B",  0, "SD Card"),
]

# ── REMOVE OLD NON-HOLE FOOTPRINTS ───────────────────────────────────────────
keep_refs = {f"H{i}" for i in range(1, 9)}  # keep mounting holes H1-H8
removed = 0
for fp in list(board.GetFootprints()):
    if fp.GetReference() not in keep_refs:
        board.Delete(fp)
        removed += 1
if removed:
    print(f"  Cleared {removed} existing component footprints")

# ── LOAD AND PLACE ───────────────────────────────────────────────────────────
placed = failed = 0

for ref, lib_dir, fp_name, x, y, side, rot, value in COMPONENTS:
    lib_path = f"{L}/{lib_dir}.pretty"
    try:
        fp = pcbnew.FootprintLoad(lib_path, fp_name)
        if fp is None:
            raise ValueError(f"FootprintLoad returned None")
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(v2i(x, y))
        fp.SetOrientationDegrees(rot)
        # Flip to back side if needed (default is front)
        if side == "B":
            fp.Flip(fp.GetPosition(), False)
        board.Add(fp)
        placed += 1
        print(f"  ✓ {ref:20s}  {lib_dir}:{fp_name}  →  ({x},{y}) {side}")
    except Exception as e:
        failed += 1
        print(f"  ✗ {ref:20s}  FAILED: {e}")

print(f"\n── Result: {placed} placed  |  {failed} failed")

# ── SAVE ─────────────────────────────────────────────────────────────────────
pcbnew.Refresh() if hasattr(pcbnew, 'Refresh') else None
board.Save(board.GetFileName())
print(f"✓ Saved: {board.GetFileName()}")
print(f"  Total footprints on board: {len(board.GetFootprints())}")
