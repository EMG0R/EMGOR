"""
Pocket OpGorator — inject_footprints.py v3.3
Board: 84x150mm portrait, all positions per flux-ai-prompt.md v0.6
Focus: all through-hole connectors/headers (headers, jacks, encoders, switch)
       + all passive components (resistors, caps) on B.Cu lower section

Changes from v3.2 (passives):
  - 7 passive components added on B.Cu, bottom section, two clusters:
      Left strip  (x≈5,  y=115-127): R_VBAT1, R_VBAT2, C_VBAT (VBAT divider)
      Middle strip (x≈35, y=116-137): R_FSR, C_FSR, R_XSHUT, C_BULK_5V
  - Passive clusters have silkscreen zone outlines on B.SilkS
  - Footprints: DIN0204 P5.08mm horizontal resistors, C_Disc P5mm, CP_Radial P2.5mm

Changes from v3.1 (v0.6 spec):
  - J_IN, J_OUT: moved to B.Cu with custom PJ-307 footprint (4+1 pin layout)
    PJ-307 not in KiCad standard library; SJ1-3515N had wrong pin geometry
    B.Cu + rot=270 keeps barrel pointing out through top-edge housing wall
    fp_y=1.8 unchanged; barrel at board_y=-3.5mm (3.5mm past board edge) ✓
  - J_MPU9250: y=63.57 → y=90.0 (v0.6 repositioning, avoids TPS61088 overlap)
  - J_AMP_OUT (1x6) → J_AMP_IN (1x5): PAM8403 input edge (+,L,R,5V,-)
  - J_AMP_PWR (1x2) → J_AMP_OUT (1x4): PAM8403 output edge (L+,L-,R-,R+)
  - 7 board-level module silkscreen gr_rect outlines (F.SilkS / B.SilkS)

B.Cu+rot=270 coordinate transform: board(x,y) = (fp_x + local_y, fp_y + local_x)
  Derivation: B.Cu mirror (-lx,ly) then rot=270 CCW: (ly, lx) → translate by fp

Phase 1 (pcbnew): wipe board, draw 84x150 outline, add 4x M2 NPTH holes
Phase 2 (text):   inject all footprints and module silk outlines

Run:
  /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 inject_footprints.py
"""

import sys, re, uuid as uuidlib, warnings
warnings.filterwarnings('ignore', category=DeprecationWarning)

KICAD_SITE = '/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages'
LIBS       = '/Applications/KiCad.app/Contents/SharedSupport/footprints'
BOARD_PATH = '/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/prototypes/Pocket-OpGorator/kicks/kicks.kicad_pcb'

if KICAD_SITE not in sys.path:
    sys.path.insert(0, KICAD_SITE)

import pcbnew

# ── HELPERS ──────────────────────────────────────────────────────────────────

def mm(v):      return pcbnew.FromMM(v)
def v2i(x, y): return pcbnew.VECTOR2I(mm(x), mm(y))


# ── PHASE 1: clean board, set outline, add M2 holes via pcbnew ───────────────

print("Phase 1: wiping board and setting up 84x150mm outline...")

board = pcbnew.LoadBoard(BOARD_PATH)

removed = 0
for fp in list(board.GetFootprints()):
    board.Delete(fp)
    removed += 1

for d in list(board.GetDrawings()):
    board.Delete(d)

print(f"  Cleared {removed} footprints + all drawings")

rect = pcbnew.PCB_SHAPE(board)
rect.SetShape(pcbnew.SHAPE_T_RECT)
rect.SetLayer(pcbnew.Edge_Cuts)
rect.SetStart(v2i(0, 0))
rect.SetEnd(v2i(84, 150))
rect.SetWidth(mm(0.05))
board.Add(rect)
print("  Board outline: 84x150mm")

for ref, x, y in [("H1",4,4),("H2",80,4),("H3",4,146),("H4",80,146)]:
    fp  = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("M2_NPTH")
    fp.SetPosition(v2i(x, y))
    pad = pcbnew.PAD(fp)
    pad.SetNumber("")
    pad.SetSize(v2i(2.2, 2.2))
    pad.SetDrillSize(v2i(2.2, 2.2))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu)
    ls.AddLayer(pcbnew.B_Cu)
    pad.SetLayerSet(ls)
    fp.Add(pad)
    board.Add(fp)

board.Save(BOARD_PATH)
print("  M2 holes: (4,4) (80,4) (4,146) (80,146) — saved")


# ── LAYER SWAP TABLE (for B.Cu library footprints) ───────────────────────────

_SWAP = {
    'F.Cu':'B.Cu',     'B.Cu':'F.Cu',
    'F.SilkS':'B.SilkS', 'B.SilkS':'F.SilkS',
    'F.Fab':'B.Fab',   'B.Fab':'F.Fab',
    'F.Mask':'B.Mask', 'B.Mask':'F.Mask',
    'F.Paste':'B.Paste','B.Paste':'F.Paste',
    'F.CrtYd':'B.CrtYd','B.CrtYd':'F.CrtYd',
    'F.Adhes':'B.Adhes','B.Adhes':'F.Adhes',
}

def swap_layers(text):
    return re.sub(r'"((?:F|B)\.[A-Za-z]+)"',
                  lambda m: '"' + _SWAP.get(m.group(1), m.group(1)) + '"',
                  text)


# ── FOOTPRINT BLOCK BUILDER (library footprints) ─────────────────────────────

def make_block(lib_dir, fp_name, x, y, side, ref, value, rot=0):
    """
    Read .kicad_mod from standard library, inject board-level position header,
    patch Reference/Value, swap layers for B.Cu.

    rot: 0=pins+y  270=pins+x  90=pins-x  180=pins-y
    """
    path = f"{LIBS}/{lib_dir}.pretty/{fp_name}.kicad_mod"
    with open(path) as f:
        lines = f.read().strip().split('\n')

    lines[0] = f'(footprint "{lib_dir}:{fp_name}"'

    i = 1
    while i < min(12, len(lines)):
        s = lines[i].strip()
        if (re.match(r'\(layer\s+"', s)
                or re.match(r'\(version\s+', s)
                or re.match(r'\(generator\b', s)
                or re.match(r'\(generator_version\s+', s)):
            lines.pop(i)
        else:
            i += 1

    layer_str = "B.Cu" if side == "B" else "F.Cu"
    at_str    = f'{x} {y}' if rot == 0 else f'{x} {y} {rot}'
    for i, ins in enumerate([
        f'\t(layer "{layer_str}")',
        f'\t(uuid "{uuidlib.uuid4()}")',
        f'\t(at {at_str})',
    ]):
        lines.insert(1 + i, ins)

    block = '\n'.join(lines)
    block = re.sub(r'(\(property "Reference" ")[^"]*(")', rf'\g<1>{ref}\2',  block, count=1)
    block = re.sub(r'(\(property "Value" ")[^"]*(")',     rf'\g<1>{value}\2', block, count=1)

    if side == "B":
        hdr  = block.split('\n')[:4]
        body = swap_layers('\n'.join(block.split('\n')[4:]))
        block = '\n'.join(hdr) + '\n' + body

    return '\t' + block.replace('\n', '\n\t')


# ── CUSTOM SS12F44H BLOCK ─────────────────────────────────────────────────────
# 3A SPDT slide switch, 2.54mm pitch. Not in KiCad library at correct pitch.
# Pins in +x: pin1(VBAT_RAW) at (x,y), pin2(COM) at (x+2.54,y), pin3(NC) at (x+5.08,y).

def make_ss12f44h_block(x, y, side, ref, value):
    layer = "B.Cu" if side == "B" else "F.Cu"
    silk  = "B.SilkS" if side == "B" else "F.SilkS"
    fab   = "B.Fab"   if side == "B" else "F.Fab"
    ctyd  = "B.CrtYd" if side == "B" else "F.CrtYd"
    def uid(): return str(uuidlib.uuid4())
    block = (
        f'(footprint "custom:SS12F44H_SPDT"\n'
        f'\t(layer "{layer}")\n'
        f'\t(uuid "{uid()}")\n'
        f'\t(at {x} {y})\n'
        f'\t(property "Reference" "{ref}"\n'
        f'\t\t(at 2.54 -2.5)\n'
        f'\t\t(layer "{silk}")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(property "Value" "{value}"\n'
        f'\t\t(at 2.54 2.5)\n'
        f'\t\t(layer "{fab}")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(attr through_hole)\n'
        f'\t(fp_rect (start -1.9 -2.0) (end 7.0 2.0)\n'
        f'\t\t(stroke (width 0.05) (type default)) (fill no) (layer "{ctyd}") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(fp_rect (start -1.7 -1.75) (end 6.78 1.75)\n'
        f'\t\t(stroke (width 0.1) (type default)) (fill no) (layer "{fab}") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(fp_rect (start -1.7 -1.75) (end 6.78 1.75)\n'
        f'\t\t(stroke (width 0.12) (type default)) (fill no) (layer "{silk}") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "1" thru_hole rect (at 0 0) (size 1.5 1.5) (drill 0.8)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "2" thru_hole circle (at 2.54 0) (size 1.5 1.5) (drill 0.8)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "3" thru_hole circle (at 5.08 0) (size 1.5 1.5) (drill 0.8)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(embedded_fonts no)\n'
        f')'
    )
    return '\t' + block.replace('\n', '\n\t')


# ── CUSTOM PTH PAD BLOCK ──────────────────────────────────────────────────────
# Single large PTH pad for TPS61088 corner connections. 5mm pad / 2.5mm drill.

def make_pth_pad_block(x, y, side, ref, value, drill_mm=2.5, pad_mm=5.0):
    layer = "B.Cu" if side == "B" else "F.Cu"
    silk  = "B.SilkS" if side == "B" else "F.SilkS"
    fab   = "B.Fab"   if side == "B" else "F.Fab"
    ctyd  = "B.CrtYd" if side == "B" else "F.CrtYd"
    r = pad_mm / 2.0
    def uid(): return str(uuidlib.uuid4())
    block = (
        f'(footprint "custom:{ref}"\n'
        f'\t(layer "{layer}")\n'
        f'\t(uuid "{uid()}")\n'
        f'\t(at {x} {y})\n'
        f'\t(property "Reference" "{ref}"\n'
        f'\t\t(at 0 {-(r + 1.5)})\n'
        f'\t\t(layer "{silk}")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(property "Value" "{value}"\n'
        f'\t\t(at 0 {r + 1.5})\n'
        f'\t\t(layer "{fab}")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(attr through_hole)\n'
        f'\t(fp_circle (center 0 0) (end {r + 0.25} 0)\n'
        f'\t\t(stroke (width 0.05) (type default)) (fill no) (layer "{ctyd}") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "1" thru_hole circle (at 0 0) (size {pad_mm} {pad_mm}) (drill {drill_mm})\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(embedded_fonts no)\n'
        f')'
    )
    return '\t' + block.replace('\n', '\n\t')


# ── CUSTOM PJ307 BLOCK ────────────────────────────────────────────────────────
# PJ-307 horizontal 3.5mm TRS jack. Not in KiCad standard library.
# Always B.Cu at rot=270. Barrel exits through top-edge housing wall.
#
# B.Cu+rot=270 transform: board(x,y) = (fp_x + local_y, fp_y + local_x)
#   Derivation: mirror(-lx, ly) then rot=270 CCW → (ly, lx) → + fp origin
#
# Local coords (KiCad applies B.Cu mirror + rot=270 from (at x y 270)):
#   Pin 1 T    (Tip):     (6.0, -3.75) │ 4-pin row, 2.5mm pitch
#   Pin 2 R    (Ring):    (6.0, -1.25) │ → board row at y = fp_y + 6.0
#   Pin 3 S    (Sleeve):  (6.0, +1.25) │
#   Pin 4 SW_R (NC-Ring): (6.0, +3.75) │
#   Pin 5 SW_T (NC-Tip):  (3.8,  0.0)  — middle pin, 1 row toward barrel
#   GND tab 1: (0.0, -5.0)  — left mounting tab at board-edge region
#   GND tab 2: (0.0, +5.0)  — right mounting tab
#   Barrel:    (-5.3, 0)    → board_y = fp_y - 5.3 = 1.8 - 5.3 = -3.5mm ✓
#
# At J_IN/J_OUT fp_y=1.8: 4-pin row board_y=7.8, middle pin board_y=5.6 ✓
# Signal: 1.2mm drill / 1.8mm pad.  GND tabs: 1.4mm drill / 1.8×2.0mm oval.

def make_pj307_block(x, y, ref, value):
    def uid(): return str(uuidlib.uuid4())
    block = (
        f'(footprint "custom:PJ307_THT_Horizontal"\n'
        f'\t(layer "B.Cu")\n'
        f'\t(uuid "{uid()}")\n'
        f'\t(at {x} {y} 270)\n'
        f'\t(property "Reference" "{ref}"\n'
        f'\t\t(at 5.0 -7.5)\n'
        f'\t\t(layer "B.SilkS")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(property "Value" "{value}"\n'
        f'\t\t(at 5.0 7.5)\n'
        f'\t\t(layer "B.Fab")\n'
        f'\t\t(uuid "{uid()}")\n'
        f'\t\t(effects (font (size 1 1) (thickness 0.15)))\n'
        f'\t)\n'
        f'\t(attr through_hole)\n'
        # Courtyard: barrel clearance (-5.8) through back of pins (+10.2), full width (±6.25)
        f'\t(fp_rect (start -5.8 -6.25) (end 10.2 6.25)\n'
        f'\t\t(stroke (width 0.05) (type default)) (fill no) (layer "B.CrtYd") (uuid "{uid()}")\n'
        f'\t)\n'
        # Body outline on Fab and Silk (excludes barrel)
        f'\t(fp_rect (start 0.0 -5.5) (end 9.5 5.5)\n'
        f'\t\t(stroke (width 0.1) (type default)) (fill no) (layer "B.Fab") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(fp_rect (start 0.0 -5.5) (end 9.5 5.5)\n'
        f'\t\t(stroke (width 0.12) (type default)) (fill no) (layer "B.SilkS") (uuid "{uid()}")\n'
        f'\t)\n'
        # Pin-1 polarity marker (thick tick at pin-1 corner of body)
        f'\t(fp_line (start 0.0 -5.5) (end 1.5 -5.5)\n'
        f'\t\t(stroke (width 0.25) (type default)) (layer "B.SilkS") (uuid "{uid()}")\n'
        f'\t)\n'
        # 4-pin row: T, R, S, SW_R at local x=6.0, y=±1.25/±3.75 (pitch 2.5mm)
        f'\t(pad "1" thru_hole rect   (at 6.0 -3.75) (size 1.8 1.8) (drill 1.2)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "2" thru_hole circle (at 6.0 -1.25) (size 1.8 1.8) (drill 1.2)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "3" thru_hole circle (at 6.0  1.25) (size 1.8 1.8) (drill 1.2)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "4" thru_hole circle (at 6.0  3.75) (size 1.8 1.8) (drill 1.2)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        # Middle pin SW_T at local (3.8, 0) — one step toward barrel, centered
        f'\t(pad "5" thru_hole circle (at 3.8  0.0)  (size 1.8 1.8) (drill 1.2)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        # GND mounting tabs at local (0, ±5.0)
        f'\t(pad "G" thru_hole oval   (at 0.0 -5.0)  (size 1.8 2.0) (drill 1.4)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(pad "G" thru_hole oval   (at 0.0  5.0)  (size 1.8 2.0) (drill 1.4)\n'
        f'\t\t(layers "*.Cu" "*.Mask") (uuid "{uid()}")\n'
        f'\t)\n'
        f'\t(embedded_fonts no)\n'
        f')'
    )
    return '\t' + block.replace('\n', '\n\t')


# ── MODULE SILKSCREEN OUTLINE HELPER ─────────────────────────────────────────

def make_gr_rect(x1, y1, x2, y2, layer, width=0.12):
    return (
        f'\t(gr_rect\n'
        f'\t\t(start {x1} {y1})\n'
        f'\t\t(end {x2} {y2})\n'
        f'\t\t(stroke (width {width}) (type solid))\n'
        f'\t\t(fill no)\n'
        f'\t\t(layer "{layer}")\n'
        f'\t\t(uuid "{uuidlib.uuid4()}")\n'
        f'\t)'
    )


# ── COMPONENT TABLE ───────────────────────────────────────────────────────────
# Positions per flux-ai-prompt.md v0.6.
# J_IN/J_OUT: custom PJ307 blocks below (not in this table).
# SW1: custom SS12F44H block below.
# TPS61088: custom PTH pads below.
#
# EC11E origin offset: shaft at local (7.5, 2.5) → fp at (shaft_x-7.5, shaft_y-2.5)
#   ENC1 shaft (21,82) → fp (13.5, 79.5)
#   ENC2 shaft (63,82) → fp (55.5, 79.5)
#
# J_OLED  7-pin center x=42: pin1_x = 42 - 3*2.54 = 34.38
# J_TOP   6-pin center x=42: pin1_x = 42 - 2.5*2.54 = 35.65
# Daisy row spacing: 25.4mm centerline-to-centerline, rows at x=29.3 and x=54.7

COMPONENTS = [

    # ── TOP SIDE (F.Cu) ───────────────────────────────────────────────────────

    ("J_MAX4466",    "Connector_PinSocket_2.54mm", "PinSocket_1x03_P2.54mm_Vertical",
     63.0,  22.0,  "F", "MAX4466 Mic",       270),

    ("J_OLED",       "Connector_PinSocket_2.54mm", "PinSocket_1x07_P2.54mm_Vertical",
     34.38, 31.5,  "F", "SSD1309 OLED",      270),

    ("ENC1",         "Rotary_Encoder",             "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",
     13.5,  79.5,  "F", "CYT1100 ENC L",       0),

    ("J_TOP_SENSOR", "Connector_PinSocket_2.54mm", "PinSocket_1x06_P2.54mm_Vertical",
     35.65, 82.0,  "F", "VL53L1X ToF",        270),

    ("ENC2",         "Rotary_Encoder",             "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",
     55.5,  79.5,  "F", "CYT1100 ENC R",       0),

    # ── BOTTOM SIDE (B.Cu) ───────────────────────────────────────────────────

    ("J_DAISY_L",    "Connector_PinSocket_2.54mm", "PinSocket_1x20_P2.54mm_Vertical",
     29.3,  25.35, "B", "Daisy Seed L",         0),
    ("J_DAISY_R",    "Connector_PinSocket_2.54mm", "PinSocket_1x20_P2.54mm_Vertical",
     54.7,  25.35, "B", "Daisy Seed R",         0),

    # v0.6: y moved from 63.57 → 90.0 (user spec revision; clears TPS61088 area)
    ("J_MPU9250",    "Connector_PinSocket_2.54mm", "PinSocket_1x10_P2.54mm_Vertical",
     49.5,  90.0,  "B", "MPU9250 IMU",           0),

    ("J_TRELLIS",    "Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical",
     4.0,   95.0,  "B", "NeoTrellis",             0),

    ("J_FSR",        "Connector_PinSocket_2.54mm", "PinSocket_1x03_P2.54mm_Vertical",
     80.0,  95.0,  "B", "SF45-65 FSR",            0),

    ("J_TP4056",     "Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical",
     15.0,  95.0,  "B", "TP4056 Charger",         0),

    # PAM8403: v0.6 renames J_AMP_OUT(1x6)→J_AMP_IN(1x5) and J_AMP_PWR(1x2)→J_AMP_OUT(1x4)
    # Input edge: +, L, R, 5V, GND  |  Output edge: L+, L-, R-, R+
    ("J_AMP_IN",     "Connector_PinSocket_2.54mm", "PinSocket_1x05_P2.54mm_Vertical",
     15.0,  122.0, "B", "PAM8403 Audio In",    270),

    ("J_AMP_OUT",    "Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical",
     15.0,  128.0, "B", "PAM8403 Audio Out",   270),

    ("J_SDCARD",     "Connector_PinSocket_2.54mm", "PinSocket_1x06_P2.54mm_Vertical",
     55.0,  122.0, "B", "SD Card SPI",         270),

    # J_AUX removed — no planned use for spare Daisy AUX pins
]


# ── PASSIVE COMPONENTS ────────────────────────────────────────────────────────
# All B.Cu, bottom section.  Two clusters:
#
#   LEFT STRIP  (x≈5, y=115-127)  — VBAT voltage-divider for battery monitor
#     R_VBAT1  100kΩ  VBAT_RAW → divider midpoint  (pad1 at x=2.46, pad2 at x=7.54)
#     R_VBAT2   33kΩ  midpoint → GND               (same x footprint)
#     C_VBAT   100nF  midpoint → GND (filter)       (pad1 at x=2.5,  pad2 at x=7.5)
#     All clear of TP4056 outline (ends y=112) and PAM8403 (starts x=9.5) ✓
#
#   MIDDLE STRIP (x≈35, y=116-137) — FSR divider, ToF SHUT pull-up, 5V bulk
#     R_FSR    10kΩ   FSR pin2 → GND  (pull-down leg of divider)
#     C_FSR   100nF   FSR pin2 → GND  (decoupling, parallel with R_FSR)
#     R_XSHUT  10kΩ   3V3 → VL53L1X SHUT (pull-up, always-on enable)
#     C_BULK_5V 10µF  5V rail → GND  (bulk decoupling near BOOST_VOUT_pad)
#     All clear of PAM8403 right edge (x=30.5) and SD card left edge (x=40.4) ✓
#
# Footprints:
#   Resistors:  Resistor_THT  DIN0204_L3.6mm_D1.6mm_P5.08mm_Horizontal  (1/8W compact)
#   Disc caps:  Capacitor_THT C_Disc_D4.7mm_W2.5mm_P5.00mm
#   Electrolytic: Capacitor_THT CP_Radial_D5.0mm_P2.50mm

_RES  = ("Resistor_THT",   "R_Axial_DIN0204_L3.6mm_D1.6mm_P5.08mm_Horizontal")
_DISC = ("Capacitor_THT",  "C_Disc_D4.7mm_W2.5mm_P5.00mm")
_ELEC = ("Capacitor_THT",  "CP_Radial_D5.0mm_P2.50mm")

PASSIVES = [
    # ref          lib              fp              x      y      side  value                 rot
    # ── left strip: VBAT divider ─────────────────────────────────────────────────────────────
    # C_VBAT removed — battery % is read slowly, software averaging handles noise instead
    ("R_VBAT1", *_RES,             5.0,  115.0, "B", "100k VBAT-div top",   0),
    ("R_VBAT2", *_RES,             5.0,  121.0, "B", "33k VBAT-div bot",    0),
    # ── middle strip: FSR / XSHUT / 5V ──────────────────────────────────────────────────────
    ("R_FSR",   *_RES,            35.0,  116.0, "B", "10k FSR pull-down",   0),
    ("C_FSR",   *_DISC,           35.0,  122.0, "B", "100nF FSR decoup",    0),
    ("R_XSHUT", *_RES,            35.0,  128.0, "B", "10k ToF SHUT pull-up",0),
    ("C_BULK_5V",*_ELEC,          35.0,  137.0, "B", "10uF 5V bulk",        0),
]


# ── PHASE 2: text-inject footprints and module outlines ───────────────────────

print("\nPhase 2: injecting footprints and module outlines...")

with open(BOARD_PATH) as f:
    board_text = f.read()

blocks = []
ok = fail = 0

for ref, lib_dir, fp_name, x, y, side, value, rot in COMPONENTS:
    try:
        b = make_block(lib_dir, fp_name, x, y, side, ref, value, rot)
        blocks.append(b)
        ok += 1
        print(f"  ✓ {ref:20s} ({x:7.3f},{y:7.3f}) {side} rot={rot:3d}  {fp_name}")
    except Exception as e:
        fail += 1
        print(f"  ✗ {ref:20s} FAILED: {e}")

for ref, lib_dir, fp_name, x, y, side, value, rot in PASSIVES:
    try:
        b = make_block(lib_dir, fp_name, x, y, side, ref, value, rot)
        blocks.append(b)
        ok += 1
        print(f"  ✓ {ref:20s} ({x:7.3f},{y:7.3f}) {side} rot={rot:3d}  {fp_name}")
    except Exception as e:
        fail += 1
        print(f"  ✗ {ref:20s} FAILED: {e}")

# SS12F44H power slide switch (custom — no KiCad lib footprint at 2.54mm pitch)
sw1 = make_ss12f44h_block(21.0, 22.0, "F", "SW1", "SS12F44H")
blocks.append(sw1)
ok += 1
print(f"  ✓ {'SW1':20s} ( 21.000, 22.000) F rot=  0  custom:SS12F44H_SPDT")

# PJ-307 audio jacks (custom — not in KiCad library; always B.Cu rot=270 baked in)
# fp_y=1.8: Cmts.User board-edge marker at local x=-1.8 aligns y=0; barrel at board_y=-3.5
for jack_ref, jack_x, jack_val in [
    ("J_IN",  21.0, "PJ-307 Audio IN"),
    ("J_OUT", 63.0, "PJ-307 Audio OUT"),
]:
    b = make_pj307_block(jack_x, 1.8, jack_ref, jack_val)
    blocks.append(b)
    ok += 1
    print(f"  ✓ {jack_ref:20s} ({jack_x:7.3f},  1.800) B rot=270  custom:PJ307_THT_Horizontal")

# TPS61088 boost module corner PTH pads (5mm pad / 2.5mm drill, B.Cu)
BOOST_PADS = [
    ("BOOST_VOUT_pad",     "VOUT", 36.5,  86.5),
    ("BOOST_VIN_pad",      "VIN",  73.5,  86.5),
    ("BOOST_GND_VOUT_pad", "GND",  36.5, 103.5),
    ("BOOST_GND_VIN_pad",  "GND",  73.5, 103.5),
]
for ref, val, bx, by in BOOST_PADS:
    b = make_pth_pad_block(bx, by, "B", ref, val)
    blocks.append(b)
    ok += 1
    print(f"  ✓ {ref:20s} ({bx:7.3f},{by:7.3f}) B rot=  0  custom:PTH_pad")

# Module silkscreen outlines (board-level gr_rect, shows actual module PCB footprint)
# Outlines extend outside board edge where module overhangs (e.g. TP4056 USB-C, audio barrel)
OUTLINES = [
    # (x1,    y1,     x2,    y2,    layer      )  — module + description
    (  6.5,  30.0,  77.5,  73.5, "F.SilkS"),   # SSD1309 OLED   71×43.5mm
    ( 58.5,  22.0,  72.5,  43.0, "F.SilkS"),   # MAX4466 mic    ~20.8×13.8mm (capsule -y toward OLED)
    ( 34.1,  88.7,  49.5, 114.2, "B.SilkS"),   # MPU9250 IMU    25.5×15.4mm (header right edge)
    (-13.0,  95.0,  15.0, 112.0, "B.SilkS"),   # TP4056 charger 28×17mm (USB-C overhangs left edge)
    (  9.5, 116.0,  30.5, 134.0, "B.SilkS"),   # PAM8403 amp    21×18mm (spans both headers)
    ( 35.0,  84.0,  75.0, 106.0, "B.SilkS"),   # TPS61088 boost 40×22mm (4 corner pads)
    ( 40.4, 122.0,  82.4, 146.0, "B.SilkS"),   # SD card module 42×24mm (card slot at y≈146)
    # Passive component cluster zones (dashed zone outlines on B.SilkS)
    ( -0.5, 112.5,  10.0, 130.0, "B.SilkS"),   # VBAT-divider cluster  (R_VBAT1/2, C_VBAT)
    ( 29.5, 113.5,  41.5, 140.5, "B.SilkS"),   # Passives cluster      (R_FSR, C_FSR, R_XSHUT, C_BULK_5V)
]
for x1, y1, x2, y2, layer in OUTLINES:
    blocks.append(make_gr_rect(x1, y1, x2, y2, layer))

# Inject all blocks + outlines before board closing marker
insertion = '\n'.join(blocks) + '\n'
new_board = board_text.replace(
    '\n\t(embedded_fonts no)\n)',
    '\n' + insertion + '\t(embedded_fonts no)\n)',
    1
)

if new_board == board_text:
    print("\n  ⚠ Injection marker not found — board may have wrong structure.")
    print("  Expected: \\n\\t(embedded_fonts no)\\n) at end of file.")
else:
    with open(BOARD_PATH, 'w') as f:
        f.write(new_board)
    print(f"\n✓ {ok} footprints injected ({fail} failed), {len(OUTLINES)} silk outlines")
    print(f"✓ Saved: {BOARD_PATH}")
    print("\nIn KiCad: File → Revert Board (or close/reopen) to see the result.")
    print("\nNext step: open in KiCad, verify passive placement, then route traces.")
