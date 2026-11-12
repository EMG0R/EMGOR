# Editing `.kicad_pcb` Files Directly

A reference for programmatically injecting footprints and board elements into KiCad 9 `.kicad_pcb` files without needing a running KiCad GUI.

---

## Why direct text editing?

KiCad exposes a Python API (`pcbnew`) that works headlessly for *reading* and *deleting* items, but `FootprintLoad()` segfaults outside of a wxApp GUI context. The workaround is a two-phase approach:

- **Phase 1** — use `pcbnew.LoadBoard()` headlessly to delete unwanted footprints (this is safe because it does not instantiate footprint graphics)
- **Phase 2** — inject new footprints by reading `.kicad_mod` library files and splicing the text into the board file

This is reliable because `.kicad_pcb` is a plain S-expression text file that KiCad 9 re-parses completely on load.

---

## The `.kicad_pcb` S-expression format

A KiCad PCB file is nested parenthesised expressions (like Lisp). Every token is either a symbol, a string (double-quoted), or a number.

```
(kicad_pcb
  (version 20241229)
  (generator "pcbnew")
  (generator_version "9.0")
  (general ...)
  (paper "A4")
  (layers ...)
  (setup ...)
  (net 0 "")
  (footprint "LibName:FpName" ...)
  (footprint "LibName:FpName" ...)
  ...
  (gr_rect ...)          ← board outline
  (embedded_fonts no)
)
```

The final two lines of every `.kicad_pcb` file are always:

```
	(embedded_fonts no)
)
```

This is the injection point — insert new footprint blocks immediately before `\t(embedded_fonts no)\n)`.

---

## Layer numbering and names

KiCad 9 layer table as it appears in the `(layers ...)` section:

| ID | Name | Type | Alias |
|----|------|------|-------|
| 0 | `F.Cu` | signal | — |
| 2 | `B.Cu` | signal | — |
| 5 | `F.SilkS` | user | F.Silkscreen |
| 7 | `B.SilkS` | user | B.Silkscreen |
| 1 | `F.Mask` | user | — |
| 3 | `B.Mask` | user | — |
| 13 | `F.Paste` | user | — |
| 15 | `B.Paste` | user | — |
| 31 | `F.CrtYd` | user | F.Courtyard |
| 29 | `B.CrtYd` | user | B.Courtyard |
| 35 | `F.Fab` | user | — |
| 33 | `B.Fab` | user | — |
| 9 | `F.Adhes` | user | F.Adhesive |
| 11 | `B.Adhes` | user | B.Adhesive |
| 25 | `Edge.Cuts` | user | — |
| 27 | `Margin` | user | — |
| 17 | `Dwgs.User` | user | User.Drawings |

The wildcard `*.Cu` means both `F.Cu` and `B.Cu`. It appears in pad `(layers ...)` lists for through-hole pads. Similarly `*.Mask` and `*.Paste` apply to both sides.

---

## Anatomy of an embedded footprint block

This is the canonical structure that pcbnew produces when it normalises a front-side footprint:

```
(footprint "LibName:FpName"
    (layer "F.Cu")
    (uuid "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
    (at X Y)
    (descr "...")
    (tags "...")
    (property "Reference" "REF"
        (at relX relY rot)
        (layer "F.SilkS")
        (uuid "...")
        (effects (font (size 1 1) (thickness 0.15)))
    )
    (property "Value" "VALUE"
        (at relX relY rot)
        (layer "F.Fab")
        (uuid "...")
        (effects (font (size 1 1) (thickness 0.15)))
    )
    (property "Datasheet" ""
        (at 0 0 0)
        (layer "F.Fab")
        (hide yes)
        (uuid "...")
        (effects (font (size 1.27 1.27) (thickness 0.15)))
    )
    (property "Description" ""
        (at 0 0 0)
        (layer "F.Fab")
        (hide yes)
        (uuid "...")
        (effects (font (size 1.27 1.27) (thickness 0.15)))
    )
    (attr through_hole)     ← or (attr smd)
    (fp_line ...)
    (fp_circle ...)
    (pad "1" thru_hole rect
        (at 0 0)
        (size 1.7 1.7)
        (drill 1)
        (layers "*.Cu" "*.Mask")
        (uuid "...")
    )
    (pad "2" thru_hole circle ...)
    (embedded_fonts no)
    (model "..." ...)
)
```

**Key rules:**
- The footprint block starts with `(footprint "LibName:FpName"` — the library prefix is required in a board file (differs from standalone `.kicad_mod` where it is just the footprint name)
- Lines 2–4 are always `(layer ...)`, `(uuid ...)`, `(at X Y)` in that order
- `(at X Y)` is in millimetres; no unit suffix
- Every sub-item (line, circle, pad) needs its own `(uuid "...")` — pcbnew generates these on load if missing, so placeholder UUIDs or even omitting them works on injection, but KiCad will fill them in on save
- `(embedded_fonts no)` appears both at footprint level and at board level

---

## Difference between `.kicad_mod` and embedded footprint format

`.kicad_mod` (library file) contains extra metadata that must be stripped before embedding in a `.kicad_pcb`:

| Line in `.kicad_mod` | In embedded footprint |
|---|---|
| `(footprint "FpName"` | Rename to `(footprint "LibName:FpName"` |
| `(version 20241229)` | **Remove** |
| `(generator "kicad-footprint-generator")` | **Remove** |
| `(generator_version "9.0")` | **Remove** |
| `(layer "F.Cu")` | **Remove** — will be re-inserted manually as line 2 |

---

## Two-phase injection script

### Phase 1 — headless deletion via pcbnew

```python
import sys, pcbnew

BOARD_PATH = '/path/to/your.kicad_pcb'

board = pcbnew.LoadBoard(BOARD_PATH)
KEEP_REFS = {'H1', 'H2', 'H3', 'H4'}   # mounting holes to preserve

removed = 0
for fp in list(board.GetFootprints()):
    if fp.GetReference() not in KEEP_REFS:
        board.Delete(fp)
        removed += 1

board.Save(BOARD_PATH)
```

`pcbnew.LoadBoard()` works from a plain terminal Python session — no KiCad GUI or wxApp needed for deletion. `FootprintLoad()` however segfaults without a wxApp; that is why text injection is used for addition.

### Phase 2 — text injection

```python
import re, uuid as uuidlib

LIBS = '/Applications/KiCad.app/Contents/SharedSupport/footprints'

_LAYER_SWAP = {
    'F.Cu':'B.Cu', 'B.Cu':'F.Cu',
    'F.SilkS':'B.SilkS', 'B.SilkS':'F.SilkS',
    'F.Fab':'B.Fab', 'B.Fab':'F.Fab',
    'F.Mask':'B.Mask', 'B.Mask':'F.Mask',
    'F.Paste':'B.Paste', 'B.Paste':'F.Paste',
    'F.CrtYd':'B.CrtYd', 'B.CrtYd':'F.CrtYd',
    'F.Adhes':'B.Adhes', 'B.Adhes':'F.Adhes',
}

def swap_layers(text):
    return re.sub(r'"((?:F|B)\.[A-Za-z]+)"',
                  lambda m: '"' + _LAYER_SWAP.get(m.group(1), m.group(1)) + '"',
                  text)

def make_block(lib_dir, fp_name, x, y, side, ref, value):
    with open(f"{LIBS}/{lib_dir}.pretty/{fp_name}.kicad_mod") as f:
        lines = f.read().strip().split('\n')

    # 1. Add library prefix to opening line
    lines[0] = f'(footprint "{lib_dir}:{fp_name}"'

    # 2. Strip .kicad_mod-only header lines (version, generator, layer)
    i = 1
    while i < min(10, len(lines)):
        s = lines[i].strip()
        if (re.match(r'\(layer\s+"', s) or
                re.match(r'\(version\s+', s) or
                re.match(r'\(generator\s+', s) or
                re.match(r'\(generator_version\s+', s)):
            lines.pop(i)
        else:
            i += 1

    # 3. Insert board-level header: layer, uuid, position
    layer_str = "B.Cu" if side == "B" else "F.Cu"
    for i, ins in enumerate([
        f'\t(layer "{layer_str}")',
        f'\t(uuid "{uuidlib.uuid4()}")',
        f'\t(at {x} {y})',
    ]):
        lines.insert(1 + i, ins)

    block = '\n'.join(lines)

    # 4. Patch Reference and Value text
    block = re.sub(r'(\(property "Reference" ")[^"]*(")', rf'\g<1>{ref}\2', block, count=1)
    block = re.sub(r'(\(property "Value" ")[^"]*(")',     rf'\g<1>{value}\2', block, count=1)

    # 5. For back-side footprints: swap all F.*/B.* layer names in the body
    #    The 4-line header (footprint, layer, uuid, at) is left unchanged.
    if side == "B":
        header = '\n'.join(block.split('\n')[:4])
        body   = swap_layers('\n'.join(block.split('\n')[4:]))
        block  = header + '\n' + body

    # 6. Indent one level for the board file
    return '\t' + block.replace('\n', '\n\t')


# Inject into board file
with open(BOARD_PATH) as f:
    board_text = f.read()

blocks = '\n'.join(make_block(*args) for args in COMPONENTS) + '\n'

new_board = board_text.replace(
    '\n\t(embedded_fonts no)\n)',   # board-level closing
    '\n' + blocks + '\t(embedded_fonts no)\n)',
    1   # only the first (board-level) occurrence
)

with open(BOARD_PATH, 'w') as f:
    f.write(new_board)
```

---

## Back-side footprint rules

When a footprint lives on B.Cu (soldered from the back of the board):

1. **`(layer "B.Cu")`** — the footprint's primary layer declaration
2. **Layer swap in body** — every `F.*` layer name in silk, fab, courtyard, paste, mask sub-items becomes the corresponding `B.*` name (see `_LAYER_SWAP` table above)
3. **Coordinates are NOT mirrored** — `(at X Y)` uses the same board coordinate system. KiCad renders the mirroring visually when displaying B.Cu; the numbers in the file are always in the "top-down board view" frame
4. **`*.Cu` and `*.Mask` pad layers stay unchanged** — these wildcards expand correctly for whichever side the footprint is on
5. **`Datasheet` and `Description` properties stay on `F.Fab`** — this is what pcbnew writes after normalisation; they are hidden properties that KiCad does not render, so the layer mismatch is harmless

Verified from `kicks_normalised.kicad_pcb`: `J_SDCARD` (B.Cu, pin 1 at (55,115)) has `B.SilkS`, `B.CrtYd`, `B.Fab` on all body items, pad layers `*.Cu` and `*.Mask` unchanged, and `Datasheet`/`Description` still on `F.Fab`.

---

## Coordinate system and footprint origins

All coordinates in `.kicad_pcb` are in **millimetres**, measured from the **top-left corner of the board** (positive X right, positive Y down — screen coordinates, not cartesian).

The `(at X Y)` in a footprint positions the footprint's **local origin**. Every sub-item position is relative to that origin.

### Pin socket footprints (all `PinSocket_1xNN_P2.54mm_Vertical`)

- Origin = **pin 1** (pad "1" is at local (0,0))
- Subsequent pins are offset along +Y by 2.54mm each
- Example: `PinSocket_1x06` — pin 1 at (0,0), pin 6 at (0, 12.7)
- To place pin 1 at board position (X, Y): use `(at X Y)` directly

### EC11E rotary encoder (`RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm`)

- Origin = **pad A** at local (0, 0)
- Shaft centre circle is at local **(7.5, 2.5)** (confirmed from F.SilkS and F.Fab `fp_circle`)
- To place the shaft at board position (SX, SY): `at = (SX - 7.5, SY - 2.5)`
- Example: shaft wanted at (18.75, 62) → `(at 11.25 59.5)`

### PJ311 audio jack (`Jack_3.5mm_PJ311_Horizontal`)

- This is an **SMD footprint** (`(attr smd)`), not through-hole
- Origin is at the SMD pad cluster centre
- Courtyard extends from y=-9.31 to y=+5.94; placing at y=0 puts the jack face flush with the board top edge
- The SMD pads at local (~±3.5, -1.3 to +4.7) are the electrical connections

### Deducing an unknown footprint's origin

```python
import re

fp_file = f"{LIBS}/{lib_dir}.pretty/{fp_name}.kicad_mod"
text = open(fp_file).read()

# Find all pad positions
for m in re.finditer(r'\(pad "([^"]+)".*?\(at ([\d.-]+) ([\d.-]+)\)', text, re.DOTALL):
    print(f"  pad {m.group(1)}: ({m.group(2)}, {m.group(3)})")

# Find shaft/reference circles on F.Fab
for m in re.finditer(r'\(fp_circle\s+\(center ([\d.-]+) ([\d.-]+)\)', text):
    print(f"  circle centre: ({m.group(1)}, {m.group(2)})")
```

---

## Board outline and drawings

Board outline is a `gr_rect` (or `gr_line` segments) on `Edge.Cuts`:

```
(gr_rect
    (start 0 0)
    (end 75 135)
    (stroke
        (width 0.05)
        (type default)
    )
    (fill no)
    (layer "Edge.Cuts")
    (uuid "...")
)
```

Use pcbnew to add the outline cleanly:

```python
rect = pcbnew.PCB_SHAPE(board)
rect.SetShape(pcbnew.SHAPE_T_RECT)
rect.SetLayer(pcbnew.Edge_Cuts)
rect.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(0), pcbnew.FromMM(0)))
rect.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(75), pcbnew.FromMM(135)))
rect.SetWidth(pcbnew.FromMM(0.05))
board.Add(rect)
```

---

## NPTH mounting holes via pcbnew

Non-plated through holes (for M2 screws) are footprints with a single NPTH pad:

```python
fp = pcbnew.FOOTPRINT(board)
fp.SetReference("H1")
fp.SetValue("M2")
fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(3), pcbnew.FromMM(3)))

pad = pcbnew.PAD(fp)
pad.SetNumber("")
pad.SetSize(pcbnew.VECTOR2I(pcbnew.FromMM(2.2), pcbnew.FromMM(2.2)))
pad.SetDrillSize(pcbnew.VECTOR2I(pcbnew.FromMM(2.2), pcbnew.FromMM(2.2)))
pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
layers = pcbnew.LSET()
layers.AddLayer(pcbnew.F_Cu)
layers.AddLayer(pcbnew.B_Cu)
pad.SetLayerSet(layers)
fp.Add(pad)
board.Add(fp)
```

After adding via pcbnew, the serialised form in the board file is:

```
(footprint ""
    (layer "F.Cu")
    (uuid "...")
    (at 3 3)
    (property "Reference" "H1" ...)
    (property "Value" "M2" ...)
    (pad "" np_thru_hole circle
        (at 0 0)
        (size 2.2 2.2)
        (drill 2.2)
        (layers "*.Cu")
        (uuid "...")
    )
    (embedded_fonts no)
)
```

---

## pcbnew headless usage (KiCad 9, macOS)

```python
KICAD_SITE = '/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages'
import sys
sys.path.insert(0, KICAD_SITE)
import pcbnew

board = pcbnew.LoadBoard('/path/to/board.kicad_pcb')
# ... modify ...
board.Save(board.GetFileName())   # or board.Save('/path/to/output.kicad_pcb')
```

Run with the KiCad-bundled Python, not the system Python:

```
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 script.py
```

### What works headlessly

| Operation | Works headlessly |
|---|---|
| `LoadBoard()` | Yes |
| `board.GetFootprints()` | Yes |
| `board.Delete(fp)` | Yes |
| `board.Save()` | Yes |
| `pcbnew.FromMM()` / `pcbnew.ToMM()` | Yes |
| `FOOTPRINT()`, `PAD()`, `PCB_SHAPE()` | Yes |
| `FootprintLoad()` | **No — segfaults** without wxApp |
| `Refresh()` | No — GUI only |

### Running from KiCad scripting console

Inside KiCad (Tools → Scripting Console):

```python
p = '/path/to/your_script.py'
exec(open(p).read())
```

Use a variable for the path to avoid line-wrap issues with long paths. The console rejects lines that wrap visually because it treats the visual break as a newline character. Inside the console, `pcbnew.GetBoard()` returns the currently open board instead of needing `LoadBoard()`.

---

## Normalising the board file

After text injection, the file format may not match exactly what pcbnew would write. To get a clean canonical version and verify KiCad accepts the file:

```python
board = pcbnew.LoadBoard(BOARD_PATH)
board.Save('/tmp/normalised.kicad_pcb')
```

If this succeeds without error, the injected footprints are syntactically valid. The normalised file will have:
- Sub-item UUIDs filled in for any items that were missing them
- `(remove_unused_layers no)` added to through-hole pads where applicable
- Property `(effects ...)` blocks canonicalised
- 3D model paths preserved verbatim

Diff the original injected file against the normalised one to check what pcbnew silently changed.

---

## Common pitfalls

**Wrong footprint origin.** Each footprint type has a different local origin. Pin sockets use pin 1 as origin; complex footprints (encoders, jacks) may have arbitrary origins. Always check `(pad "1" ... (at ...))` in the `.kicad_mod` — if pad 1 is at (0,0) the origin is pin 1. If not, derive the offset from the fab layer geometry.

**Forgetting to strip version/generator lines.** `.kicad_mod` files have `(version ...)`, `(generator ...)`, `(generator_version ...)` and a top-level `(layer "F.Cu")`. These must all be removed before embedding. KiCad will reject or corrupt the board if they are present inside a `(footprint ...)` block within the board file.

**Missing library prefix.** In the board file, footprint names must be `"LibName:FpName"`. In standalone `.kicad_mod` files, the name is just `"FpName"`. Rename the opening line when injecting.

**Mirroring back-side coordinates.** Do NOT mirror X coordinates for B.Cu footprints. The `(at X Y)` is always in board coordinates (top-left origin). KiCad handles the visual flip.

**Injection point ambiguity.** `(embedded_fonts no)` appears both at the footprint level and at the board level. The injection uses `replace(..., 1)` to hit only the first occurrence — which should be the board-level one. Verify by checking that the pattern `\n\t(embedded_fonts no)\n)` appears only once in the base board file before any footprints are injected.

**Layer swap hitting Datasheet/Description.** These hidden properties stay on `F.Fab` even for B.Cu footprints (that is what pcbnew writes). If the layer swap regex runs over them, they get assigned to `B.Fab`. KiCad normalises this silently on load, so it is not fatal, but the canonical form keeps them on `F.Fab`. Guard by only swapping the body (lines after the 4-line header).

**pcbnew deprecation warning.** KiCad 9 emits `DeprecationWarning: pcbnew module will be removed in a future release...`. This is the SWIG-based Python API being phased out in favour of the new IPC/gRPC API. It still works in KiCad 9. The warning can be filtered with `warnings.filterwarnings('ignore', category=DeprecationWarning)`.
