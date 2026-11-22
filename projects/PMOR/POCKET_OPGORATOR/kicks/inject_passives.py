"""
Pocket OpGorator — inject_passives.py
Adds ONLY passive component footprints to the existing board.
NO wipe. NO Phase 1. Existing placement is 100% preserved.

Passives added (all B.Cu, bottom section):
  Left strip  (x≈5):  R_VBAT1 (100k), R_VBAT2 (33k)
  Middle strip (x≈35): R_FSR (10k), C_FSR (100nF), R_XSHUT (10k), C_BULK_5V (10uF)
  + 2 silkscreen zone outlines on B.SilkS

Run:
  /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 inject_passives.py
"""

import re, uuid as uuidlib

LIBS       = '/Applications/KiCad.app/Contents/SharedSupport/footprints'
BOARD_PATH = '/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/prototypes/Pocket-OpGorator/kicks/kicks.kicad_pcb'

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

def make_block(lib_dir, fp_name, x, y, side, ref, value, rot=0):
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

_RES  = ("Resistor_THT",  "R_Axial_DIN0204_L3.6mm_D1.6mm_P5.08mm_Horizontal")
_ELEC = ("Capacitor_THT", "CP_Radial_D5.0mm_P2.50mm")

PASSIVES = [
    # ref          lib    fp              x      y      side  value                  rot
    ("R_VBAT1", *_RES,    5.0,  115.0, "B", "100k VBAT-div top",    0),
    ("R_VBAT2", *_RES,    5.0,  121.0, "B", "33k VBAT-div bot",     0),
    ("R_FSR",   *_RES,   35.0,  116.0, "B", "10k FSR pull-down",    0),
    ("R_XSHUT", *_RES,   35.0,  122.0, "B", "10k ToF SHUT pull-up", 0),
    ("C_BULK_5V",*_ELEC, 35.0,  131.0, "B", "10uF 5V bulk",         0),
]

PASSIVE_ZONES = [
    ( -0.5, 112.5,  10.0, 130.0, "B.SilkS"),  # VBAT divider cluster
    ( 29.5, 113.5,  41.5, 140.5, "B.SilkS"),  # FSR / XSHUT / bulk cap cluster
]

with open(BOARD_PATH) as f:
    board_text = f.read()

blocks = []
ok = fail = 0

for ref, lib_dir, fp_name, x, y, side, value, rot in PASSIVES:
    try:
        b = make_block(lib_dir, fp_name, x, y, side, ref, value, rot)
        blocks.append(b)
        ok += 1
        print(f"  ✓ {ref:20s} ({x:6.1f},{y:6.1f}) {side}  {fp_name}")
    except Exception as e:
        fail += 1
        print(f"  ✗ {ref:20s} FAILED: {e}")

for x1, y1, x2, y2, layer in PASSIVE_ZONES:
    blocks.append(make_gr_rect(x1, y1, x2, y2, layer))

insertion = '\n'.join(blocks) + '\n'
new_board = board_text.replace(
    '\n\t(embedded_fonts no)\n)',
    '\n' + insertion + '\t(embedded_fonts no)\n)',
    1
)

if new_board == board_text:
    print("⚠ Injection marker not found — board file may have wrong structure.")
else:
    with open(BOARD_PATH, 'w') as f:
        f.write(new_board)
    print(f"\n✓ {ok} passives injected ({fail} failed), {len(PASSIVE_ZONES)} zone outlines")
    print(f"✓ Nothing else touched. Existing layout preserved.")
    print(f"✓ Saved: {BOARD_PATH}")
    print("\nIn KiCad: File → Revert Board to see the result.")
