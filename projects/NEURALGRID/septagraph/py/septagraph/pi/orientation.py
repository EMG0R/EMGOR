"""Orientation layer for the neuralGrid Pi app -- lets the physical 8x8
control surface be logically "rotated" so any of its four edges can be
addressed as the top by the modes, without any mode ever knowing about it.

Definitions
-----------
- Hardware (hw) coordinates: (x, y) as wired -- what leds.idx()/the ESP32
  protocol/every pre-existing test already assumes, x,y in 0..7, index =
  y*8+x, hw y=0 is the physical top row (README.md's own "bottom-left =
  lowest, top-right = highest" keyboard convention).
- Logical coordinates: what App hands to `active_mode.on_key`/receives
  from `active_mode.led_frame()` -- the "as if unrotated" frame the modes
  are written against.
- `rot`: the clockwise rotation, in degrees (one of 0/90/180/270), that
  physically-rotating the hardware by `rot` would produce the logical
  frame. Equivalently: hw_to_logical(x, y, rot) is the standard
  rotate-an-8x8-grid-clockwise-by-`rot` transform.

Hardware edge names (fixed, wiring-level, never change regardless of
`rot`): N = top (hw y=0), S = bottom (hw y=7), W = left (hw x=0), E =
right (hw x=7) -- same convention the softpot channels (raw[0:4]) and
lidar channels (mm4) already use elsewhere in this app (see
App.get_sensors_state: raw[0]=N, raw[1]=S, raw[2]=E, raw[3]=W).

Gesture contract (App owns the timing/hold-detection; this module only
supplies the pure geometry): holding both hw corners of one edge for
~0.8s makes that edge the new logical top, i.e. its two held corners end
up as logical (0,0)/(7,0) -- the logical TOP row's corners:

    hw edge held      -> rot that makes it logical top
    N (top)              -> rot=0    (identity)
    W (left)              -> rot=90
    S (bottom)            -> rot=180
    E (right)              -> rot=270

(Read `_rotate_xy` + the module-level test suite if you want to re-derive
this table -- it falls straight out of the standard image-rotate-N-degrees
formula plus "which hw edge's y=0/x=0 constraint lands on logical y=0".)

FSR note: FSR0-3's physical placement around the ring is NOT
independently confirmed by this task (only the softpot/lidar N/S/E/W
wiring is documented, HANDOFF Sec11) -- `analog_hw_to_logical` ASSUMES
FSR0..3 = N,S,E,W in the same order as the softpots/lidars, and rotates
them identically. If that assumption is wrong, only the FSR *identity*
after a rotation is affected (which FSR now reads as which logical
edge); raw sensor values themselves are untouched either way, and the
web UI's SENSORS panel always shows hardware-truth (unrotated) FSR/
softpot/lidar readings regardless of `rot` -- only what's handed to
`active_mode.on_analog`/`on_lidar` is reordered.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Sequence, Tuple

N_SIDE = 8
VALID_ROTS = (0, 90, 180, 270)

# Matches the fixed raw[0:4] / mm4 index order used everywhere else in
# this app (app.py's get_sensors_state, its docstrings, and every test
# that drives FakeGrid.simulate_analog/simulate_lidar).
HW_EDGE_ORDER = ("N", "S", "E", "W")
HW_INDEX = {name: i for i, name in enumerate(HW_EDGE_ORDER)}

# hw (x, y) corner pairs per hardware edge name -- the two pads the
# rotation gesture watches (App._update_orientation_gesture).
EDGE_CORNERS: Dict[str, Tuple[Tuple[int, int], Tuple[int, int]]] = {
    "N": ((0, 0), (7, 0)),
    "S": ((0, 7), (7, 7)),
    "W": ((0, 0), (0, 7)),
    "E": ((7, 0), (7, 7)),
}
CORNER_COORDS = frozenset(c for pair in EDGE_CORNERS.values() for c in pair)

# hw edge held -> rot that makes it logical top (see module docstring).
EDGE_TO_ROT: Dict[str, int] = {"N": 0, "W": 90, "S": 180, "E": 270}
ROT_TO_EDGE: Dict[int, str] = {v: k for k, v in EDGE_TO_ROT.items()}

_CLOCKWISE = ("N", "E", "S", "W")  # physical clockwise order of the 4 edges


def _rotate_xy(x: int, y: int, rot: int) -> Tuple[int, int]:
    """Standard clockwise rotation of an (x, y) cell in an N_SIDE x N_SIDE
    row-major grid (y increases downward) by `rot` degrees."""
    if rot not in VALID_ROTS:
        raise ValueError(f"rot must be one of {VALID_ROTS}, got {rot!r}")
    n1 = N_SIDE - 1
    if rot == 0:
        return x, y
    if rot == 90:
        return n1 - y, x
    if rot == 180:
        return n1 - x, n1 - y
    return y, n1 - x  # rot == 270


def hw_to_logical(x: int, y: int, rot: int) -> Tuple[int, int]:
    """Hardware cell (x, y) -> logical cell, for the given `rot`."""
    return _rotate_xy(x, y, rot)


def logical_to_hw(x: int, y: int, rot: int) -> Tuple[int, int]:
    """Inverse of hw_to_logical: logical cell (x, y) -> hardware cell."""
    return _rotate_xy(x, y, (360 - rot) % 360)


def rotate_frame(frame64: Sequence, rot: int) -> List:
    """LOGICAL-ordered 64-cell frame -> HARDWARE-ordered 64-cell frame
    (index i -> x=i%8, y=i//8, same convention leds.py uses everywhere)."""
    if len(frame64) != N_SIDE * N_SIDE:
        raise ValueError(f"frame64 must have {N_SIDE * N_SIDE} cells, got {len(frame64)}")
    out = [None] * (N_SIDE * N_SIDE)
    for hy in range(N_SIDE):
        for hx in range(N_SIDE):
            lx, ly = hw_to_logical(hx, hy, rot)
            out[hy * N_SIDE + hx] = frame64[ly * N_SIDE + lx]
    return out


def rotate_partial(updates: Sequence[Tuple[int, int, int, int, int]], rot: int) -> List[Tuple[int, int, int, int, int]]:
    """(x, y, r, g, b) updates in LOGICAL coords -> the same updates with
    (x, y) converted to HARDWARE coords, colour untouched."""
    out = []
    for x, y, r, g, b in updates:
        hx, hy = logical_to_hw(x, y, rot)
        out.append((hx, hy, r, g, b))
    return out


def edge_map(rot: int) -> Dict[str, str]:
    """logical edge name (N/S/E/W -- "which side of the CURRENT logical
    frame") -> hardware edge name (N/S/E/W -- "which physical ribbon/edge
    is actually there right now"). Identity at rot=0."""
    if rot not in VALID_ROTS:
        raise ValueError(f"rot must be one of {VALID_ROTS}, got {rot!r}")
    top_hw = ROT_TO_EDGE[rot]  # hw edge now sitting at logical N
    start = _CLOCKWISE.index(top_hw)
    return {_CLOCKWISE[i]: _CLOCKWISE[(start + i) % 4] for i in range(4)}


def analog_hw_to_logical(raw8: Sequence[float], rot: int) -> Tuple:
    """Reorder an 8-channel ANALOG sample (softpots raw[0:4] = hw N,S,E,W;
    FSR raw[4:8] -- ASSUMED FSR0..3 = hw N,S,E,W too, see module
    docstring) into LOGICAL N,S,E,W order for both groups, via
    `edge_map(rot)`. Channels beyond index 8 (none in the current
    protocol) pass through unchanged; a too-short input degrades
    gracefully (returned unrotated) rather than raising."""
    raw = list(raw8)
    emap = edge_map(rot)
    logical_order = ("N", "S", "E", "W")

    def _reorder(group):
        if len(group) < 4:
            return tuple(group)
        return tuple(group[HW_INDEX[emap[le]]] for le in logical_order)

    softpots = _reorder(raw[0:4])
    fsrs = _reorder(raw[4:8])
    rest = tuple(raw[8:])
    return softpots + fsrs + rest


def lidar_hw_to_logical(mm4: Sequence[int], rot: int) -> Tuple:
    """Reorder a 4-channel LIDAR sample (hw N,S,E,W) into LOGICAL N,S,E,W
    order, via `edge_map(rot)`."""
    mm = list(mm4)
    if len(mm) < 4:
        return tuple(mm)
    emap = edge_map(rot)
    logical_order = ("N", "S", "E", "W")
    return tuple(mm[HW_INDEX[emap[le]]] for le in logical_order)


@dataclass
class Orientation:
    """Mutable holder for the current rotation. App owns the gesture
    timing/threading/persistence; this dataclass just tracks `rot` and
    offers the derived `top_edge` `/api/state` and the web UI compass
    want (which HARDWARE edge is currently logical top)."""
    rot: int = 0

    def __post_init__(self) -> None:
        if self.rot not in VALID_ROTS:
            raise ValueError(f"rot must be one of {VALID_ROTS}, got {self.rot!r}")

    @property
    def top_edge(self) -> str:
        return ROT_TO_EDGE[self.rot]
