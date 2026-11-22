"""conditioning.py — per-source raw -> normalized-float pipeline.

Pure functions only: every call takes the previous state explicitly and
returns the new state explicitly, so this module needs no hardware, no
threads, and no mocks to test — feed it numbers, check what comes back.

Pipeline, in order:
    1. dropout check      (raw == the source's known-bad sentinel?)
    2. linear calibration  (raw -> 0..1 using in_lo/in_hi, REAL units)
    3. deadzone            (clip near 0 and near 1 to exactly 0 / 1)
    4. one-pole smoothing   (time-constant filter, so `dt` matters)

Dropout policy is deliberately narrow: only "hold" is implemented, because
that is the one that matters. A LiDAR reading a timeout must NOT snap the
mapped parameter to 0 (or whatever raw=-1 would linearly scale to) — it
should freeze at the last good value until the beam reacquires. "hold" is
also the only sane default for any future dropout-capable source, so it is
what happens even if a source config omits the field.
"""

from __future__ import annotations

import math
from typing import NamedTuple, Optional

from .model import SourceDescriptor


class ConditionerState(NamedTuple):
    """Carried between calls for one (source_id) across frames.

    `held_norm` is the last norm value considered good (i.e. produced from a
    non-dropout sample) — this is what "hold" freezes onto. It starts at
    None so the very first sample, even if it happens to be a dropout, has
    something sane to fall back to (0.0) rather than an undefined value.
    `smoothed` is the filter's running output; kept distinct from held_norm
    so the filter keeps warming up even while frozen at a plateau (it is
    already equal to held_norm once settled, this just avoids a special
    case in the smoothing step below).
    """
    held_norm: float = 0.0
    smoothed: float = 0.0
    initialized: bool = False
    valid: bool = True


def _clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else (1.0 if x > 1.0 else x)


def condition(raw: float, source_cfg: dict, descriptor: Optional[SourceDescriptor],
              state: ConditionerState, dt: float) -> tuple[float, bool, ConditionerState]:
    """Run one raw sample through the pipeline.

    source_cfg: the sensors.json "sources[]" entry for this source (dict
                with in_lo, in_hi, deadzone, smooth_ms, dropout — all
                optional, see DEFAULTS below).
    descriptor: the driver's SourceDescriptor, used only to learn the raw
                dropout sentinel. May be None (source config wins if it
                sets in_lo/in_hi explicitly; dropout detection is then
                skipped since there is nothing to compare against).
    dt:         seconds since the previous call for this source. Pass 0.0
                on the very first sample.

    Returns (norm, valid, new_state).
    """
    in_lo = source_cfg.get("in_lo", 0.0)
    in_hi = source_cfg.get("in_hi", 1.0)
    deadzone = source_cfg.get("deadzone", 0.0)
    smooth_ms = source_cfg.get("smooth_ms", 0.0)
    dropout_policy = source_cfg.get("dropout", "hold")

    dropout_value = descriptor.dropout_value if descriptor is not None else None
    is_dropout = dropout_value is not None and raw == dropout_value

    if is_dropout:
        # "hold": freeze at the last good normalized value. Nothing else is
        # implemented (see module docstring) so any other policy string
        # also holds — that is the safe direction to fail in.
        del dropout_policy  # reserved for future policies; "hold" always wins today
        norm = state.held_norm if state.initialized else 0.0
        new_state = ConditionerState(held_norm=norm, smoothed=norm,
                                      initialized=True, valid=False)
        return norm, False, new_state

    # 1. linear calibration, REAL units -> 0..1
    span = (in_hi - in_lo)
    lin = (raw - in_lo) / span if span else 0.0
    lin = _clamp01(lin)

    # 2. deadzone — clip near-edge noise to a hard 0 or 1. This does NOT
    #    rescale the remaining span; a deadzone is meant to kill jitter at
    #    rest, not eat into the usable travel.
    if deadzone > 0.0:
        if lin < deadzone:
            lin = 0.0
        elif lin > 1.0 - deadzone:
            lin = 1.0

    # 3. one-pole smoothing. tau in seconds; alpha->1 (no smoothing) as
    #    smooth_ms->0, alpha->0 (frozen) as smooth_ms->inf. Standard
    #    exponential-filter alpha for a fixed sample interval dt.
    tau = smooth_ms / 1000.0
    if tau <= 0.0 or dt <= 0.0 or not state.initialized:
        smoothed = lin
    else:
        alpha = 1.0 - math.exp(-dt / tau)
        smoothed = state.smoothed + alpha * (lin - state.smoothed)

    new_state = ConditionerState(held_norm=smoothed, smoothed=smoothed,
                                  initialized=True, valid=True)
    return smoothed, True, new_state
