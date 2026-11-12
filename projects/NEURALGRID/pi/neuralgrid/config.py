"""neuralGrid CONFIG BOUNDARY -- Phase 3.

The single source of truth for the schema-versioned parameter vector
described in docs/CONFIG_SCHEMA.md. This is the boundary between the LIVE
LAYER (Phase 2, reads the active config, never blocks) and the MODEL LAYER
(Phase 4+, authors new configs). For now (Phase 3) nothing authors configs
but hand-written code (see live/stager_manual.py) and this selftest -- the
point of Phase 3 is to prove the codec + atomic swap mechanism works before
any model exists.

No hardware imports on purpose: this module must be importable and
testable on a machine with no serial/MIDI backend at all (just numpy,
already a project dependency).
"""

from __future__ import annotations

from typing import Dict, List

import numpy as np

SCHEMA_VERSION = 1

# ---------------------------------------------------------------------------
# Enum tables
# ---------------------------------------------------------------------------

MENUS = ["instrument", "sequencer", "midi_fx", "reserved"]
N_MENUS = len(MENUS)

NOTE_LAYOUTS = ["chromatic", "rows_in_fourths", "scale_degrees", "isomorphic"]
N_LAYOUTS = len(NOTE_LAYOUTS)

# name -> semitone intervals from root. Order here IS the scale_id enum order.
SCALES = [
    {"name": "chromatic", "intervals": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]},
    {"name": "major", "intervals": [0, 2, 4, 5, 7, 9, 11]},
    {"name": "natural_minor", "intervals": [0, 2, 3, 5, 7, 8, 10]},
    {"name": "dorian", "intervals": [0, 2, 3, 5, 7, 9, 10]},
    {"name": "mixolydian", "intervals": [0, 2, 4, 5, 7, 9, 10]},
    {"name": "major_pentatonic", "intervals": [0, 2, 4, 7, 9]},
    {"name": "minor_pentatonic", "intervals": [0, 3, 5, 7, 10]},
    {"name": "harmonic_minor", "intervals": [0, 2, 3, 5, 7, 8, 11]},
]
N_SCALES = len(SCALES)

N_ROOT_NOTES = 12
N_OCTAVES = 8

VELOCITY_CURVES = ["fixed", "row", "col", "accent"]
N_VEL_CURVES = len(VELOCITY_CURVES)

ARP_MODES = ["off", "up", "down", "updown", "random"]
N_ARP_MODES = len(ARP_MODES)

LED_PALETTES = ["rainbow", "mono_blue", "fire", "ice", "purple", "grayscale"]
N_PALETTES = len(LED_PALETTES)

N_CHANNELS = 16

SOFTPOT_FUNCTIONS = ["menu_select", "cc", "pitch_bend", "none"]
N_SOFTPOT_FUNCS = len(SOFTPOT_FUNCTIONS)
N_SOFTPOTS = 4

# ---------------------------------------------------------------------------
# Vector packing order + widths (see docs/CONFIG_SCHEMA.md "Packing order")
# ---------------------------------------------------------------------------

VECTOR_LEN = (
    N_MENUS  # active_menu
    + N_LAYOUTS  # note_layout
    + N_SCALES  # scale_id
    + N_ROOT_NOTES  # root_note
    + N_OCTAVES  # octave
    + 1  # density
    + N_VEL_CURVES  # velocity_curve
    + 1  # velocity_fixed
    + 1  # swing
    + N_ARP_MODES  # arp_mode
    + 1  # echo_send
    + 1  # quantize_strength
    + N_PALETTES  # led_palette
    + 1  # led_brightness
    + N_CHANNELS  # channel
    + N_SOFTPOTS * N_SOFTPOT_FUNCS  # softpot_fn[4]
)
assert VECTOR_LEN == 89, VECTOR_LEN  # pinned by docs/CONFIG_SCHEMA.md


# ---------------------------------------------------------------------------
# default / validate
# ---------------------------------------------------------------------------


def default_config() -> Dict:
    """The config the live layer boots with. Chosen to reproduce Phase 2's
    original hard-coded behavior exactly (rows-in-fourths layout, fixed
    velocity 100/127, MIDI channel 0, full-brightness rainbow palette) --
    Phase 3 is a pure superset of Phase 2, never a regression."""
    return {
        "schema_version": SCHEMA_VERSION,
        "active_menu": 0,  # instrument
        "note_layout": 1,  # rows_in_fourths (Phase 2's original formula)
        "scale_id": 1,  # major
        "root_note": 0,  # C
        "octave": 2,  # -> base note 36 (C2), matching Phase 2's BASE_NOTE
        "density": 1.0,
        "velocity_curve": 0,  # fixed
        "velocity_fixed": 100.0 / 127.0,  # matches Phase 2's VELOCITY = 100
        "swing": 0.0,
        "arp_mode": 0,  # off
        "echo_send": 0.0,
        "quantize_strength": 0.0,
        "led_palette": 0,  # rainbow
        "led_brightness": 1.0,
        "channel": 0,
        "softpot_fn": [0, 0, 0, 0],  # all menu_select, matching Phase 2 touch behavior
    }


def validate(cfg: Dict) -> bool:
    """Raises ValueError describing the first problem found; returns True
    if `cfg` is a fully valid config dict."""

    def _err(msg: str):
        raise ValueError(f"invalid config: {msg}")

    if not isinstance(cfg, dict):
        _err(f"cfg must be a dict, got {type(cfg)!r}")

    sv = cfg.get("schema_version", SCHEMA_VERSION)
    if sv != SCHEMA_VERSION:
        _err(f"schema_version {sv!r} != {SCHEMA_VERSION}")

    def _enum(name: str, n: int):
        v = cfg.get(name)
        if isinstance(v, bool) or not isinstance(v, int) or not (0 <= v < n):
            _err(f"{name} must be an int in [0,{n}), got {v!r}")

    def _scalar(name: str):
        v = cfg.get(name)
        if isinstance(v, bool) or not isinstance(v, (int, float)):
            _err(f"{name} must be a number, got {v!r}")
        if not (0.0 <= float(v) <= 1.0):
            _err(f"{name} must be in [0,1], got {v!r}")

    _enum("active_menu", N_MENUS)
    _enum("note_layout", N_LAYOUTS)
    _enum("scale_id", N_SCALES)
    _enum("root_note", N_ROOT_NOTES)
    _enum("octave", N_OCTAVES)
    _scalar("density")
    _enum("velocity_curve", N_VEL_CURVES)
    _scalar("velocity_fixed")
    _scalar("swing")
    _enum("arp_mode", N_ARP_MODES)
    _scalar("echo_send")
    _scalar("quantize_strength")
    _enum("led_palette", N_PALETTES)
    _scalar("led_brightness")
    _enum("channel", N_CHANNELS)

    sp = cfg.get("softpot_fn")
    if not isinstance(sp, (list, tuple)) or len(sp) != N_SOFTPOTS:
        _err(f"softpot_fn must be a length-{N_SOFTPOTS} list, got {sp!r}")
    for i, v in enumerate(sp):
        if isinstance(v, bool) or not isinstance(v, int) or not (0 <= v < N_SOFTPOT_FUNCS):
            _err(f"softpot_fn[{i}] must be an int in [0,{N_SOFTPOT_FUNCS}), got {v!r}")

    return True


# ---------------------------------------------------------------------------
# codec: dict <-> float32 vector
# ---------------------------------------------------------------------------


def _one_hot(index: int, n: int) -> np.ndarray:
    v = np.zeros(n, dtype=np.float32)
    v[index] = 1.0
    return v


def _scalar_vec(value: float) -> np.ndarray:
    return np.array([max(0.0, min(1.0, float(value)))], dtype=np.float32)


def to_vector(cfg: Dict) -> np.ndarray:
    """Deterministic dict -> float32 vector, in the field order documented
    in docs/CONFIG_SCHEMA.md. Validates `cfg` first."""
    validate(cfg)

    parts: List[np.ndarray] = [
        _one_hot(cfg["active_menu"], N_MENUS),
        _one_hot(cfg["note_layout"], N_LAYOUTS),
        _one_hot(cfg["scale_id"], N_SCALES),
        _one_hot(cfg["root_note"], N_ROOT_NOTES),
        _one_hot(cfg["octave"], N_OCTAVES),
        _scalar_vec(cfg["density"]),
        _one_hot(cfg["velocity_curve"], N_VEL_CURVES),
        _scalar_vec(cfg["velocity_fixed"]),
        _scalar_vec(cfg["swing"]),
        _one_hot(cfg["arp_mode"], N_ARP_MODES),
        _scalar_vec(cfg["echo_send"]),
        _scalar_vec(cfg["quantize_strength"]),
        _one_hot(cfg["led_palette"], N_PALETTES),
        _scalar_vec(cfg["led_brightness"]),
        _one_hot(cfg["channel"], N_CHANNELS),
    ]
    for fn in cfg["softpot_fn"]:
        parts.append(_one_hot(fn, N_SOFTPOT_FUNCS))

    vec = np.concatenate(parts).astype(np.float32)
    assert vec.shape == (VECTOR_LEN,), vec.shape
    return vec


def from_vector(vec) -> Dict:
    """float32 vector -> dict. Raises ValueError if `vec` isn't shaped
    (VECTOR_LEN,). Otherwise always succeeds -- enum blocks decode via
    argmax (robust to soft/continuous model logits, not just hard one-hot)
    and scalars clamp to [0,1], so a model's raw output can never produce
    an invalid config at this boundary."""
    vec = np.asarray(vec, dtype=np.float32)
    if vec.shape != (VECTOR_LEN,):
        raise ValueError(f"from_vector: expected shape ({VECTOR_LEN},), got {vec.shape}")

    i = 0

    def take_onehot(n: int) -> int:
        nonlocal i
        seg = vec[i : i + n]
        i += n
        return int(np.argmax(seg))

    def take_scalar() -> float:
        nonlocal i
        v = float(vec[i])
        i += 1
        return max(0.0, min(1.0, v))

    cfg: Dict = {"schema_version": SCHEMA_VERSION}
    cfg["active_menu"] = take_onehot(N_MENUS)
    cfg["note_layout"] = take_onehot(N_LAYOUTS)
    cfg["scale_id"] = take_onehot(N_SCALES)
    cfg["root_note"] = take_onehot(N_ROOT_NOTES)
    cfg["octave"] = take_onehot(N_OCTAVES)
    cfg["density"] = take_scalar()
    cfg["velocity_curve"] = take_onehot(N_VEL_CURVES)
    cfg["velocity_fixed"] = take_scalar()
    cfg["swing"] = take_scalar()
    cfg["arp_mode"] = take_onehot(N_ARP_MODES)
    cfg["echo_send"] = take_scalar()
    cfg["quantize_strength"] = take_scalar()
    cfg["led_palette"] = take_onehot(N_PALETTES)
    cfg["led_brightness"] = take_scalar()
    cfg["channel"] = take_onehot(N_CHANNELS)
    cfg["softpot_fn"] = [take_onehot(N_SOFTPOT_FUNCS) for _ in range(N_SOFTPOTS)]

    assert i == VECTOR_LEN, (i, VECTOR_LEN)
    return cfg


# ---------------------------------------------------------------------------
# LED palette rendering helper (shared by menu_instrument / menu_sequencer)
# ---------------------------------------------------------------------------

import colorsys  # noqa: E402  (kept near its one use, after the codec)

RGB = tuple


def palette_color(palette_id: int, t: float, value: float = 1.0):
    """Map a palette id + position t in [0,1] + brightness value in [0,1]
    to an (r,g,b) 0..255 tuple. Used by menus for led_palette/led_brightness
    driven coloring so the config boundary actually controls what a human
    sees, not just what a model could in principle write."""
    t = max(0.0, min(1.0, float(t)))
    value = max(0.0, min(1.0, float(value)))
    name = LED_PALETTES[palette_id % N_PALETTES]

    if name == "rainbow":
        r, g, b = colorsys.hsv_to_rgb(t, 1.0, value)
    elif name == "mono_blue":
        r, g, b = colorsys.hsv_to_rgb(0.6, 1.0, value * (0.3 + 0.7 * t))
    elif name == "purple":
        r, g, b = colorsys.hsv_to_rgb(0.78, 1.0, value * (0.3 + 0.7 * t))
    elif name == "fire":
        r, g, b = colorsys.hsv_to_rgb(0.15 * t, 1.0, value)
    elif name == "ice":
        r, g, b = colorsys.hsv_to_rgb(0.5 + 0.16 * t, 1.0, value)
    elif name == "grayscale":
        r = g = b = value * t
    else:  # pragma: no cover -- unreachable given the modulo above
        r = g = b = 0.0

    return (int(r * 255), int(g * 255), int(b * 255))
