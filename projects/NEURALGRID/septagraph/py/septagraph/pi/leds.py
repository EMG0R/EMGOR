"""LED language helpers for the neuralGrid Pi app.

Frame representation used throughout the app: a flat list of 64 (r,g,b)
tuples, index i -> x = i % 8, y = i // 8 (matches
neuralgrid.codec.leds_full's convention exactly).

Implements the shared vocabulary from SEPTAGRAPH_PLAN.txt §7-8 / HANDOFF
§2 "LED language":
  - warm white   = physical catalog point (direct-play pad, MIDI-mapped pad)
  - blue-green   = continuum / unassigned cell
  - violet pulse = prediction ghost (septagraph mode, model-driven)
  - amber        = loop / sequencer playhead
  - ~150ms crossfade whenever the active mode or window swaps, so a mode
    change never pops.
"""
from __future__ import annotations

import colorsys
import time
from typing import Dict, List, Tuple

N_LEDS = 64
RGB = Tuple[int, int, int]

# Transport tuning (app.py's LED send path): send LEDS_PARTIAL instead of
# LEDS_FULL when at most this many pixels differ from the last frame
# actually written to the wire -- keeps a single key-press/crossfade-tick
# update small instead of always re-shipping all 64 pixels.
PARTIAL_MAX_PIXELS = 24

WARM_WHITE: RGB = (255, 190, 110)
CONTINUUM_BASE_HUE = 0.42
VIOLET: RGB = (150, 60, 220)
AMBER: RGB = (255, 150, 0)
OFF: RGB = (0, 0, 0)

CROSSFADE_S = 0.15

# Direct Lab: one distinguishable hue per Machine Lab instrument so the
# FULL 8x8 layout reads at a glance (which rows are marimbot, where the
# breakBot|ganaPati split on row 7 is, ...). Unknown instruments -> WARM_WHITE.
INSTRUMENT_COLORS: Dict[str, RGB] = {
    # user-chosen palette 2026-09-09
    "marimbot": (255, 212, 0),    # yellow
    "lydia": (170, 60, 255),      # purple
    "tammy": (60, 255, 90),       # green
    "breakBot": (0, 200, 255),    # blue (lydia's old colour)
    "ganaPati": (255, 120, 0),    # orange
    "modulettes": (255, 70, 0),   # orange-red (assignable rows only)
    "rattleTron": (255, 40, 60),  # red (assignable rows only)
}


def instrument_color(name: str) -> RGB:
    return INSTRUMENT_COLORS.get(name, WARM_WHITE)


# ---------------------------------------------------------------------------
# Direct Lab per-note colour: base hue per instrument, shaded by note index
# ---------------------------------------------------------------------------
#
# Direct mode deliberately does NOT use WARM_WHITE for catalog pads (that
# meaning is reserved for Septagraph mode per SEPTAGRAPH_PLAN.txt Sec8 "LED
# language: warm white = catalog (physical) points" -- Direct mode instead
# gives every instrument its own hue so the FULL 8x8 layout reads at a
# glance, and shades each instrument's own notes so adjacent pads are still
# visibly distinct).

GAMMA = 2.2  # NeoTrellis gamma-correction curve applied just before send,
             # so low-value shades don't wash out toward white.

FLASH_WHITE: RGB = (255, 255, 255)
FLASH_S = 0.12  # pressed-pad feedback flash, <=120ms (task spec)

NOTE_HUE_DRIFT_DEG = 12.0   # total hue sweep across an instrument's note range
NOTE_VALUE_LO = 0.55        # HSV value at the instrument's lowest note
NOTE_VALUE_HI = 1.0         # HSV value at the instrument's highest note

# Brightness cap (5V-rail protection): sum of R+G+B across all 64 pads must
# not exceed 60% of the theoretical max (64 pads * 255 * 3 = 48,960) =
# 29,376. The FULL default layout's *previous* flat per-instrument frame
# (before per-note shading) already summed to ~25,425 (marimbot rows
# 12,000 + lydia 6,825 + tammy 3,240 + breakBot 1,420 + ganaPati 1,940) --
# comfortably under this cap -- so ordinary Direct Lab use is never dimmed
# by it; the cap only engages if per-note shading (which can push brighter
# notes to full value) plus a busy frame pushes the total higher.
BRIGHTNESS_CAP_FRACTION = 0.6
BRIGHTNESS_CAP_SUM = int(N_LEDS * 255 * 3 * BRIGHTNESS_CAP_FRACTION)  # 29376


def _gamma_channel(c: int, gamma: float = GAMMA) -> int:
    c = max(0, min(255, int(round(c))))
    return int(round(255.0 * ((c / 255.0) ** gamma)))


def apply_gamma(rgb: RGB, gamma: float = GAMMA) -> RGB:
    return tuple(_gamma_channel(c, gamma) for c in rgb)  # type: ignore[return-value]


def apply_gamma_frame(frame: List[RGB], gamma: float = GAMMA) -> List[RGB]:
    return [apply_gamma(c, gamma) for c in frame]


def cap_brightness(frame: List[RGB], cap_sum: int = BRIGHTNESS_CAP_SUM) -> List[RGB]:
    """Uniformly scale a frame down (never up) so its total channel sum
    never exceeds ``cap_sum`` -- keeps 64 simultaneously-lit pads from
    dipping the 5V rail no matter how bright per-note shading gets."""
    total = sum(c for rgb in frame for c in rgb)
    if total <= cap_sum or total == 0:
        return frame
    factor = cap_sum / total
    return [tuple(int(c * factor) for c in rgb) for rgb in frame]  # type: ignore[misc]


def instrument_hsv(name: str) -> Tuple[float, float, float]:
    r, g, b = instrument_color(name)
    return colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)


def note_color(instrument: str, note_index: int, n_notes: int) -> RGB:
    """One instrument's base hue, shaded by this note's position within the
    instrument's ``n_notes`` enabled points (0 = lowest note, n_notes-1 =
    highest): HSV value rises NOTE_VALUE_LO -> NOTE_VALUE_HI and hue drifts
    +/-(NOTE_HUE_DRIFT_DEG/2) low->high, so adjacent pads of the same
    instrument are visibly different while the instrument stays
    recognisable. Unknown instruments fall back to WARM_WHITE (undrifted)."""
    if instrument not in INSTRUMENT_COLORS:
        return WARM_WHITE
    h, s, v = instrument_hsv(instrument)
    t = 0.0 if n_notes <= 1 else max(0.0, min(1.0, note_index / (n_notes - 1)))
    drift = (NOTE_HUE_DRIFT_DEG / 360.0) * (t - 0.5)
    h2 = (h + drift) % 1.0
    v2 = NOTE_VALUE_LO + (NOTE_VALUE_HI - NOTE_VALUE_LO) * t
    r, g, b = colorsys.hsv_to_rgb(h2, s, v2)
    return (round(r * 255), round(g * 255), round(b * 255))


def to_hex(rgb: RGB) -> str:
    r, g, b = rgb
    return f"#{r:02x}{g:02x}{b:02x}"


def blank() -> List[RGB]:
    return [OFF] * N_LEDS


def hue_for_row(y: int, base: float = CONTINUUM_BASE_HUE, spread: float = 0.15) -> RGB:
    """blue-green family, varying gently with row y (0..7)."""
    hue = base + spread * (y / 7.0)
    r, g, b = colorsys.hsv_to_rgb(hue, 0.65, 0.55)
    return (int(r * 255), int(g * 255), int(b * 255))


def dim(rgb: RGB, factor: float) -> RGB:
    factor = max(0.0, min(1.0, factor))
    return tuple(int(c * factor) for c in rgb)  # type: ignore[return-value]


def idx(x: int, y: int) -> int:
    return y * 8 + x


def diff(old: List[RGB], new: List[RGB]) -> List[Tuple[int, int, int, int, int]]:
    """(x, y, r, g, b) for every index where new != old. Used by app.py's LED
    send path to decide LEDS_PARTIAL vs LEDS_FULL and to skip a send
    entirely when nothing changed (empty list)."""
    out: List[Tuple[int, int, int, int, int]] = []
    for i, (o, n) in enumerate(zip(old, new)):
        if o != n:
            out.append((i % 8, i // 8, n[0], n[1], n[2]))
    return out


class Crossfader:
    """Blends a frame transition over CROSSFADE_S seconds.

    Usage: on a mode/window swap, call ``start(old_frame, new_frame)``;
    call ``current(now)`` every render tick until it reports done, then
    keep rendering ``new_frame`` directly.
    """

    def __init__(self, duration_s: float = CROSSFADE_S):
        self.duration_s = duration_s
        self._from: List[RGB] = blank()
        self._to: List[RGB] = blank()
        self._t0: float = 0.0
        self._active = False

    def start(self, from_frame: List[RGB], to_frame: List[RGB], now: float | None = None) -> None:
        self._from = list(from_frame)
        self._to = list(to_frame)
        self._t0 = now if now is not None else time.monotonic()
        self._active = True

    @property
    def active(self) -> bool:
        return self._active

    def current(self, now: float | None = None) -> List[RGB]:
        if not self._active:
            return self._to
        now = now if now is not None else time.monotonic()
        f = (now - self._t0) / self.duration_s if self.duration_s > 0 else 1.0
        if f >= 1.0:
            self._active = False
            return self._to
        out = []
        for (r0, g0, b0), (r1, g1, b1) in zip(self._from, self._to):
            out.append((
                int(r0 + (r1 - r0) * f),
                int(g0 + (g1 - g0) * f),
                int(b0 + (b1 - b0) * f),
            ))
        return out
