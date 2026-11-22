"""Semantic side-channel: builds the compact JSON telemetry packet a canvas
tap sends alongside (never instead of) its raw pixel stream, so a consumer
(GORMAPPER) gets ground-truth particle tracks + scene stats without having
to image-process the RGB feed itself. See docs/TAPS_DESIGN.md.

Cadence/gating/socket-send live in taps.py (10Hz = every 3rd 30fps engine
frame, only when at least one enabled tap has `semantic_port` set); this
module only knows how to build ONE packet dict from a live scene + its
just-rendered uint8 canvas.

Packet shape (kept <= ~1400 bytes, WiFi-MTU-safe as ONE UDP datagram):

    {"v":1,"seq":<int>,"ts":<float, Pi unix time at packet build>,
     "scene":"<name>","output_delay_ms":<int, 0-500>,
     "stats":{"energy":<0-1>,"dominant":["#rrggbb",...],
              "strobe_phase":<0-1|null>},
     "tracks":[{"id":<int>,"layer":<int>,"pos":<0-1920>,"vel":<px/s>,
                "color":"#rrggbb","size":<px>,"brightness":<0-1>}, ...],
     "projector":<verbatim scene "projector" block, or null>}

tracks: ground truth from PARTICLE layers only (field layers -- wave/
noise/plasma/gradient/solid/paint -- contribute none), top-MAX_TRACKS
across every particle layer by brightness, to bound the packet size
regardless of how many particles are actually alive.
"""
from __future__ import annotations

import json
import time

import numpy as np

from .color import linear_frame_to_uint8
from .layers import ParticleLayer

MAX_TRACKS = 8               # top-N by brightness, across all particle layers
MAX_BYTES = 1400              # safety cap: ~WiFi-MTU-safe single datagram
SEPARATORS = (",", ":")       # compact json.dumps separators (no spaces)
NEAR_BLACK = 8                 # uint8 luminance floor excluded from "dominant"
QUANT_BITS = 4                 # dominant-color histogram: 4 bits/channel (16 levels)
FADE_EXPONENT = 1.5            # matches physics.ParticleSystem.render()'s fade curve


def _luma(frame: np.ndarray) -> np.ndarray:
    """(n,3) uint8 -> (n,) luminance 0..255 (Rec.601-ish weights)."""
    f = frame.astype(np.float64)
    return f[:, 0] * 0.299 + f[:, 1] * 0.587 + f[:, 2] * 0.114


def _energy(frame: np.ndarray) -> float:
    """Mean canvas luminance, 0..1."""
    if frame.size == 0:
        return 0.0
    return round(float(np.clip(_luma(frame).mean() / 255.0, 0.0, 1.0)), 3)


def _dominant(frame: np.ndarray) -> list[str]:
    """Top-2 colors by coarse quantized histogram, excluding near-black
    pixels so a dark scene reports its actual accent colors. All-black
    (or empty) canvas -> ["#000000"]."""
    if frame.size == 0:
        return ["#000000"]
    luma = _luma(frame)
    bright = frame[luma >= NEAR_BLACK]
    if bright.size == 0:
        return ["#000000"]
    shift = 8 - QUANT_BITS
    q = (bright.astype(np.uint16) >> shift)  # 0..15 per channel
    # Pixels that pass the luminance floor can still quantize to the all-
    # zero bucket (e.g. a dim fading-trail pixel like (14,14,14)) -- drop
    # those from the histogram so a big dim trail can't out-vote the
    # scene's actual bright accent color and get reported as "#000000",
    # which is reserved for the genuinely-all-black case above.
    nonzero = q.max(axis=1) > 0
    if nonzero.any():
        q = q[nonzero]
    keys = (q[:, 0].astype(np.int32) << (2 * QUANT_BITS)) | (q[:, 1] << QUANT_BITS) | q[:, 2]
    uniq, counts = np.unique(keys, return_counts=True)
    order = np.argsort(-counts)[:2]
    levels = (1 << QUANT_BITS) - 1
    scale = 255.0 / levels
    out = []
    for idx in order:
        k = int(uniq[idx])
        r = (k >> (2 * QUANT_BITS)) & levels
        g = (k >> QUANT_BITS) & levels
        b = k & levels
        out.append("#%02x%02x%02x" % (round(r * scale), round(g * scale), round(b * scale)))
    return out


def _strobe_phase(scene) -> float | None:
    """0..1 phase within the current strobe cycle, or None when the
    scene's master strobe isn't active."""
    if getattr(scene, "strobe_mode", "none") == "none":
        return None
    return round(float((scene.t * scene.strobe_rate) % 1.0), 3)


def _particle_tracks(scene) -> list[dict]:
    """Ground-truth particle tracks from every ParticleLayer in `scene`,
    pooled and truncated to the top-MAX_TRACKS by brightness."""
    rows: list[tuple[float, dict]] = []
    for li, layer in enumerate(scene.layers):
        if not isinstance(layer, ParticleLayer):
            continue
        system = layer.system
        n = system.count
        if not n:
            continue
        colors = system.resolved_colors(scene.t)  # (n,3) linear
        u8 = linear_frame_to_uint8(colors)         # (n,3) uint8, gamma-correct
        if layer.fade_out:
            life = system.life
            with np.errstate(invalid="ignore", divide="ignore"):
                frac = np.where(np.isfinite(life) & (life > 0),
                                1.0 - system.age / np.maximum(life, 1e-6), 1.0)
            gain = np.clip(frac, 0.0, None) ** FADE_EXPONENT
        else:
            gain = np.ones(n)
        brightness = np.clip(colors.max(axis=1) * gain, 0.0, 1.0)
        pos = system.pos % system.n
        for i in range(n):
            r, g, b = (int(c) for c in u8[i])
            rows.append((float(brightness[i]), {
                "id": int(system.pid[i]),
                "layer": li,
                "pos": round(float(pos[i]), 1),
                "vel": round(float(system.vel[i]), 1),
                "color": "#%02x%02x%02x" % (r, g, b),
                "size": round(float(system.size[i]), 2),
                "brightness": round(float(brightness[i]), 3),
            }))
    rows.sort(key=lambda row: row[0], reverse=True)
    return [row[1] for row in rows[:MAX_TRACKS]]


def build_semantic_packet(seq: int, scene, frame: np.ndarray,
                          output_delay_ms: int = 0) -> dict:
    """Build one semantic packet dict for `scene` (a live SceneInstance)
    whose just-rendered output is `frame` (uint8 (n,3)). `seq` is a
    caller-owned monotonic counter (one per sent semantic packet).
    `output_delay_ms` (see sync.py) is the current output-delay AV sync
    setting: how many milliseconds behind the strips/satellites are
    relative to this (always-current) semantic/pixel feed. GORMAPPER
    should treat this as "the LEDs are delayed -- don't also apply your
    own trim" (their side, not ours; we just expose the value).

    Tracks are already top-MAX_TRACKS by brightness; as a final safety
    net (arbitrarily long scene names/projector blocks aside) this also
    drops trailing tracks if the compact-JSON encoding would still
    exceed MAX_BYTES, so the caller can always send the returned dict
    as one UDP datagram."""
    packet = {
        "v": 1,
        "seq": seq,
        "ts": time.time(),
        "scene": scene.name,
        "output_delay_ms": output_delay_ms,
        "stats": {
            "energy": _energy(frame),
            "dominant": _dominant(frame),
            "strobe_phase": _strobe_phase(scene),
        },
        "tracks": _particle_tracks(scene),
        "projector": scene.scene.get("projector"),
    }
    while packet["tracks"] and _encoded_size(packet) > MAX_BYTES:
        packet["tracks"].pop()
    return packet


def _encoded_size(packet: dict) -> int:
    return len(json.dumps(packet, separators=SEPARATORS).encode())
