"""
Pocket OpGorator — pcbnew setup script
Runs 3 steps: outline, audit, place

Usage:
  From terminal:  /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 setup_board.py
  From KiCad console: exec(open(p).read())
"""
import sys, os

KICAD_PYTHON_SITE = '/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages'
BOARD_PATH = '/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/prototypes/Pocket-OpGorator/kicks/kicks.kicad_pcb'

if KICAD_PYTHON_SITE not in sys.path:
    sys.path.insert(0, KICAD_PYTHON_SITE)

import pcbnew

# Load board: from file when run from terminal, from open session inside KiCad
try:
    board = pcbnew.GetBoard()
    if not board.GetFileName():
        raise Exception("no open board")
except Exception:
    board = pcbnew.LoadBoard(BOARD_PATH)

def mm(v):    return pcbnew.FromMM(v)
def v2i(x,y): return pcbnew.VECTOR2I(mm(x), mm(y))

# ── 1. BOARD OUTLINE + M2 MOUNTING HOLES ────────────────────────────────────

def setup_outline():
    # Clear existing Edge.Cuts
    for d in list(board.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            board.Delete(d)

    # 75 × 135 mm rectangle
    rect = pcbnew.PCB_SHAPE(board)
    rect.SetShape(pcbnew.SHAPE_T_RECT)
    rect.SetLayer(pcbnew.Edge_Cuts)
    rect.SetStart(v2i(0, 0))
    rect.SetEnd(v2i(75, 135))
    rect.SetWidth(mm(0.05))
    board.Add(rect)

    # M2 NPTH mounting holes: 2.2mm drill, 5mm copper-free keepout
    HOLES = [("H1",3,3),("H2",72,3),("H3",3,132),("H4",72,132)]
    for ref, x, y in HOLES:
        # Remove existing hole with same ref
        for fp in list(board.GetFootprints()):
            if fp.GetReference() == ref:
                board.Delete(fp)

        fp = pcbnew.FOOTPRINT(board)
        fp.SetReference(ref)
        fp.SetValue("M2")
        fp.SetPosition(v2i(x, y))

        pad = pcbnew.PAD(fp)
        pad.SetNumber("")
        pad.SetSize(v2i(2.2, 2.2))
        pad.SetDrillSize(v2i(2.2, 2.2))
        pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        layers = pcbnew.LSET()
        layers.AddLayer(pcbnew.F_Cu)
        layers.AddLayer(pcbnew.B_Cu)
        pad.SetLayerSet(layers)
        fp.Add(pad)
        board.Add(fp)

    print("✓ Outline: 75×135mm  |  M2 holes: (3,3) (72,3) (3,132) (72,132)")

# ── 2. PLACEMENT SPEC ───────────────────────────────────────────────────────
# (reference, x_mm, y_mm, side "F"/"B", rotation_deg)

SPEC = [
    # Top side (F.Cu) ─────────────────────────────────────────────────────
    ("J_IN",           18.75,   3.0,  "F",   0),   # PJ-307 audio in, top edge
    ("J_OUT",          56.25,   3.0,  "F",   0),   # PJ-307 audio out, top edge
    ("SW1",            56.25,   5.0,  "F",   0),   # SS12D10-G5 power switch
    ("J_OLED",          3.0,   17.62, "F",   0),   # 7-pin header, pin1@y=10, center@y=17.62
    ("ENC1",           18.75,  62.0,  "F",   0),   # Left encoder
    ("ENC2",           56.25,  62.0,  "F",   0),   # Right encoder
    ("J_TOP_SENSOR",   37.5,   62.0,  "F",   0),   # VL53L1X header
    ("J_SWITCH",       56.25,   7.0,  "F",   0),   # Switch header (alt wiring)
    ("J_AUX",          65.0,   90.0,  "F",   0),   # Spare GPIO header

    # Bottom side (B.Cu) ──────────────────────────────────────────────────
    ("J_DAISY_L",      29.88,  47.48, "B",   0),   # Left Daisy socket row
    ("J_DAISY_R",      45.12,  47.48, "B",   0),   # Right Daisy socket row
    ("J_TRELLIS",       4.0,   82.0,  "B",   0),   # NeoTrellis 4-pin header
    ("J_FSR",          71.0,   82.0,  "B",   0),   # FSR 3-pin header
    ("J_MPU9250",      37.5,   64.0,  "B",   0),   # IMU 10-pin header
    ("J_MAX4466",      18.75,   5.0,  "B",   0),   # Mic module header
    # Power zone (B.Cu, y=80–130mm)
    ("J_TP4056",       15.0,  100.0,  "B",   0),   # TP4056 charger header
    ("J_XL3608_IN",    55.0,   95.0,  "B",   0),   # Boost in
    ("J_XL3608_OUT",   55.0,  103.0,  "B",   0),   # Boost out
    ("J_PAM8403_L",    12.0,  115.0,  "B",   0),   # Amp left
    ("J_PAM8403_R",    32.0,  115.0,  "B",   0),   # Amp right
    ("J_SDCARD",       55.0,  115.0,  "B",   0),   # SD module
]

# ── 3. AUDIT ─────────────────────────────────────────────────────────────────

def audit():
    fps   = {fp.GetReference(): fp for fp in board.GetFootprints()}
    srefs = {s[0] for s in SPEC}

    print("\n── AUDIT ──────────────────────────────────────────────────────")
    print(f"  Board has {len(fps)} footprints  |  Spec defines {len(SPEC)} positions\n")

    ok = miss = extra = 0
    for ref, x, y, side, rot in SPEC:
        if ref in fps:
            fp  = fps[ref]
            cx  = pcbnew.ToMM(fp.GetX())
            cy  = pcbnew.ToMM(fp.GetY())
            lay = "B" if fp.IsFlipped() else "F"
            dx  = abs(cx - x); dy = abs(cy - y)
            tag = "✓" if dx < 0.1 and dy < 0.1 and lay == side else "→"
            print(f"  {tag} {ref:20s}  spec({x:6.2f},{y:6.2f},{side}) | now({cx:6.2f},{cy:6.2f},{lay})")
            ok += 1
        else:
            print(f"  ✗ {ref:20s}  MISSING — not yet in board")
            miss += 1

    if extra_refs := [r for r in fps if r not in srefs and not r.startswith("H")]:
        print(f"\n  Footprints on board not in spec:")
        for ref in extra_refs:
            fp = fps[ref]; cx = pcbnew.ToMM(fp.GetX()); cy = pcbnew.ToMM(fp.GetY())
            print(f"    {ref:20s}  at ({cx:.2f},{cy:.2f})")
            extra += 1

    print(f"\n  Summary: {ok} matched | {miss} missing | {extra} unrecognised")

# ── 4. PLACE ─────────────────────────────────────────────────────────────────

def place():
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
    placed = skipped = 0

    for ref, x, y, side, rot in SPEC:
        if ref not in fps:
            skipped += 1
            continue
        fp = fps[ref]
        fp.SetPosition(v2i(x, y))
        fp.SetOrientationDegrees(rot)
        # Flip to correct side if needed
        is_back   = fp.IsFlipped()
        want_back = (side == "B")
        if is_back != want_back:
            fp.Flip(fp.GetPosition(), False)
        placed += 1

    print(f"\n✓ Placed {placed}  |  {skipped} skipped (missing from board)")
    if skipped > 0:
        print("  → Run after: File > Import > Netlist to add footprints first")
    pcbnew.Refresh()

# ── RUN ALL ──────────────────────────────────────────────────────────────────

setup_outline()
audit()
place()
board.Save(board.GetFileName())
print(f"\n✓ Saved: {board.GetFileName()}")
