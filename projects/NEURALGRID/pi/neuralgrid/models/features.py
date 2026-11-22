"""Interaction-feature extraction -- Phase 4, spec section 7.1 "Input".

Turns a rolling log of raw KEY (pad hit) + ANALOG (softpot) events into a
fixed-length float feature vector that the Interface-VAE's encoder
consumes. Pure numpy/stdlib, no hardware imports, so this is importable
and testable anywhere (mirrors config.py's boundary discipline).

The feature vector captures:
  - pad-hit rate and spatial centroid/spread of hits (where + how often
    the player is hammering the grid)
  - a velocity proxy (inter-onset-interval derived, since the wire
    protocol's KEY message carries no velocity byte -- see
    docs/PROTOCOL.md -- so "how hard/fast" has to be read off timing)
  - softpot activity, spread-across-channels (entropy), and slide speed
  - a derived INTERACTION-MODE estimate: 0 = direct-trigger (hammering
    pads dominates), 1 = expressive continuum (sliding softpots
    dominates), continuous in between -- exactly the "which sensors are
    active" signal the spec calls for.
  - an overall event-density figure and a cold-start "silence" flag.

`FeatureWindow` is the live-feeding object: engine.py (or any caller) push
events into it via `on_key_event` / `on_analog_event` as they arrive, and
`compute_features()` returns the current fixed-length snapshot -- O(1)
amortized per event (bounded deques), safe to call from the hot path.
"""

from __future__ import annotations

import math
import time
from collections import deque
from typing import Deque, Optional, Sequence, Tuple

import numpy as np

GRID_W = 8
GRID_H = 8
N_SOFTPOTS = 4
ADC_MAX = 4095.0

FEATURE_DIM = 12

# How far back "the rolling window" looks, in wall-clock seconds. A few
# bars at typical tempos -- long enough to characterize a playing style,
# short enough that the brain stays responsive to a player changing modes.
WINDOW_SECONDS = 8.0

# Normalization caps, chosen generously so typical playing saturates well
# under 1.0 (leaves headroom, avoids everything pinning to the rail).
KEY_RATE_CAP_HZ = 8.0        # pad hits/sec that reads as "maximally busy"
EVENT_DENSITY_CAP_HZ = 16.0  # combined key+analog-motion events/sec cap
IOI_FLOOR_SECONDS = 0.08     # inter-onset interval at/below which velocity proxy saturates
ANALOG_MOVE_THRESHOLD = 40.0  # raw ADC delta between consecutive samples counted as "movement"
ANALOG_TOUCH_FLOOR = 80.0     # below this raw value a channel reads as untouched (matches touch.py IN_RANGE_MIN)


class FeatureWindow:
    """Rolling log of KEY + ANALOG events -> fixed-length feature vector."""

    def __init__(self, window_seconds: float = WINDOW_SECONDS):
        self.window_seconds = window_seconds
        # (t, x, y) for each *press* (release edges don't add playing info
        # beyond what press timing already gives us).
        self._key_presses: Deque[Tuple[float, int, int]] = deque()
        # (t, values) per ANALOG frame, one tuple of N_SOFTPOTS raw ADC ints.
        self._analog_frames: Deque[Tuple[float, Tuple[int, ...]]] = deque()
        self._prev_analog: Optional[Tuple[int, ...]] = None

    # -- live feed, called from engine callbacks (or a fake feed in tests) --

    def on_key_event(self, x: int, y: int, pressed: bool, t: Optional[float] = None) -> None:
        if not pressed:
            return  # only onsets carry playing-style information here
        t = t if t is not None else time.monotonic()
        self._key_presses.append((t, int(x), int(y)))
        self._prune(t)

    def on_analog_event(self, values: Sequence[int], t: Optional[float] = None) -> None:
        t = t if t is not None else time.monotonic()
        self._analog_frames.append((t, tuple(int(v) for v in values)))
        self._prune(t)

    def _prune(self, now: float) -> None:
        cutoff = now - self.window_seconds
        while self._key_presses and self._key_presses[0][0] < cutoff:
            self._key_presses.popleft()
        while self._analog_frames and self._analog_frames[0][0] < cutoff:
            self._analog_frames.popleft()

    def reset(self) -> None:
        self._key_presses.clear()
        self._analog_frames.clear()
        self._prev_analog = None

    # -- snapshot -----------------------------------------------------

    def compute_features(self, now: Optional[float] = None) -> np.ndarray:
        """Returns a (FEATURE_DIM,) float32 vector. Never raises, never
        blocks -- safe to call every beat from a stager."""
        now = now if now is not None else time.monotonic()
        self._prune(now)

        presses = list(self._key_presses)
        frames = list(self._analog_frames)

        n_press = len(presses)
        n_frames = len(frames)
        silence = 1.0 if (n_press == 0 and n_frames == 0) else 0.0

        # -- pad-hit rate + spatial centroid/spread ---------------------
        key_hit_rate = _clip01(n_press / self.window_seconds / KEY_RATE_CAP_HZ)
        if n_press > 0:
            xs = np.array([p[1] for p in presses], dtype=np.float64)
            ys = np.array([p[2] for p in presses], dtype=np.float64)
            centroid_x = _clip01(float(xs.mean()) / max(GRID_W - 1, 1))
            centroid_y = _clip01(float(ys.mean()) / max(GRID_H - 1, 1))
            spread_x = _clip01(float(xs.std()) / (GRID_W / 2.0))
            spread_y = _clip01(float(ys.std()) / (GRID_H / 2.0))
        else:
            centroid_x = centroid_y = 0.5  # neutral (grid center), not "corner"
            spread_x = spread_y = 0.0

        # -- velocity proxy: faster inter-onset-interval -> higher proxy --
        if n_press >= 2:
            ts = [p[0] for p in presses]
            iois = [b - a for a, b in zip(ts, ts[1:]) if b > a]
            if iois:
                mean_ioi = sum(iois) / len(iois)
                velocity_proxy = _clip01(1.0 - (mean_ioi - IOI_FLOOR_SECONDS) / 1.0)
            else:
                velocity_proxy = 0.0
        else:
            velocity_proxy = 0.0

        # -- softpot activity / spread-across-channels / slide speed ----
        if n_frames > 0:
            arr = np.array([f[1] for f in frames], dtype=np.float64)  # (n_frames, N_SOFTPOTS)
            if arr.shape[1] < N_SOFTPOTS:  # defensive, shouldn't happen
                pad = np.zeros((arr.shape[0], N_SOFTPOTS - arr.shape[1]))
                arr = np.concatenate([arr, pad], axis=1)

            touched = arr > ANALOG_TOUCH_FLOOR
            softpot_activity = _clip01(float(touched.any(axis=1).mean()))

            # per-channel "how much did this channel matter" mass, for an
            # entropy-style spread-across-channels measure.
            per_channel_mass = touched.sum(axis=0).astype(np.float64)
            total_mass = per_channel_mass.sum()
            if total_mass > 0:
                p = per_channel_mass / total_mass
                nz = p[p > 0]
                entropy = float(-(nz * np.log(nz)).sum())
                softpot_entropy = _clip01(entropy / math.log(N_SOFTPOTS))
            else:
                softpot_entropy = 0.0

            if arr.shape[0] >= 2:
                deltas = np.abs(np.diff(arr, axis=0))
                slide_speed_raw = float(deltas.mean())
                softpot_slide_speed = _clip01(slide_speed_raw / ANALOG_MOVE_THRESHOLD / 4.0)
            else:
                softpot_slide_speed = 0.0
        else:
            softpot_activity = 0.0
            softpot_entropy = 0.0
            softpot_slide_speed = 0.0

        # -- interaction-mode estimate: 0 = direct-trigger (pads dominate),
        # 1 = expressive continuum (softpot slide dominates). Weighs each
        # sensor family's *engagement*, not raw event count, so a single
        # slow touch doesn't outweigh a genuine hammering session just
        # because ANALOG frames arrive faster than KEY events.
        direct_signal = key_hit_rate
        expressive_signal = max(softpot_activity, softpot_slide_speed)
        denom = direct_signal + expressive_signal
        if denom > 1e-9:
            interaction_mode = _clip01(expressive_signal / denom)
        else:
            interaction_mode = 0.5  # no evidence either way -- stay neutral

        # -- overall density -----------------------------------------------
        moving_analog = 0
        if n_frames >= 2:
            arr2 = np.array([f[1] for f in frames], dtype=np.float64)
            deltas2 = np.abs(np.diff(arr2, axis=0))
            moving_analog = int((deltas2.max(axis=1) > ANALOG_MOVE_THRESHOLD).sum())
        overall_density = _clip01((n_press + moving_analog) / self.window_seconds / EVENT_DENSITY_CAP_HZ)

        feat = np.array(
            [
                key_hit_rate,
                centroid_x,
                centroid_y,
                spread_x,
                spread_y,
                velocity_proxy,
                softpot_activity,
                softpot_entropy,
                softpot_slide_speed,
                interaction_mode,
                overall_density,
                silence,
            ],
            dtype=np.float32,
        )
        assert feat.shape == (FEATURE_DIM,), feat.shape
        return feat


def _clip01(v: float) -> float:
    if v != v:  # NaN guard
        return 0.0
    return max(0.0, min(1.0, float(v)))
