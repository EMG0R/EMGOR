"""router.py — the mapping matrix: one conditioned source -> many maps -> targets.

Pure functions. Each Map is evaluated independently by `apply_map`; the
daemon (or a test) is responsible for calling it once per map per frame and,
when several maps share a target, folding the results together with
`combine_values`. Splitting it this way keeps a single map's logic testable
in isolation (the "three maps, one source" example in the spec is really
just three independent apply_map calls that happen to read the same norm).

MAP FIELDS (see ~/demiurge/sensors.json "maps[]"):
    zone_lo/zone_hi  slice of the source's 0..1 norm, rescaled to fill the
                      map's own 0..1 output range. zone=[0.5,1.0] means
                      "the back half of the pedal is a whole knob": input
                      0.5 -> map-output 0.0, input 1.0 -> map-output 1.0.
    outside           behaviour when norm falls outside [zone_lo, zone_hi]:
                        clamp — pin to the nearest zone edge (0 or 1), and
                                keep tracking further excursions at that
                                pinned value (i.e. re-sent every frame).
                        hold  — freeze the map's OUTPUT at whatever it was
                                the instant it left the zone, and stop
                                tracking the source entirely until it
                                re-enters. (Differs from clamp only once the
                                source moves *past* the edge and clamp would
                                have kept re-affirming the same pinned
                                value anyway — hold additionally survives a
                                curve/invert change to the edge value
                                between frames, and is the natural choice
                                paired with combine="add", where a map with
                                nothing new to say should not re-assert 0.)
                        idle  — send nothing at all this frame; the target
                                is left entirely to other maps (or to
                                whatever last held it).
    invert            flip the 0..1 fraction (post zone-rescale, pre-curve).
    curve             lin | exp | log | s  (see _CURVES below)
    out_lo/out_hi     final output range the curved fraction is scaled into.
                      For kind="cc" targets this is expected to already be
                      in 0..127 space; the sink does not rescale further.
    mode              continuous | toggle | gate | trigger | step
    combine           replace | add | max — see combine_values()
    slew_ms           rate limit on the OUTPUT value: the time to sweep the
                      map's full out_lo..out_hi span from a standing start.
                      0 = no limiting.
    steps             (step mode only) number of quantization steps. NOT in
                      the spec's field list — added because "step" mode
                      needs a step count from somewhere; see README design
                      note. Defaults to 8 if omitted.

Threshold/hysteresis for toggle/gate/trigger are fixed at 0.6 (arm-high) /
0.4 (arm-low) on the post-zone, pre-curve fraction — a design choice (not
specified) chosen to match the "heel/toe" pedal-switch feel already used
elsewhere in this codebase (e.g. expr_targets.json's FREEZE LISTEN comment:
"toe (>0.6) opens ... heel (<0.4) closes").
"""

from __future__ import annotations

import math
from typing import NamedTuple, Optional

_HYST_HI = 0.6
_HYST_LO = 0.4


class MapState(NamedTuple):
    """Per-map state carried across frames."""
    initialized: bool = False
    last_output: float = 0.0     # last value this map computed (pre-slew)
    last_sent: float = 0.0       # last value actually sent (post-slew)
    zone_frozen: float = 0.0     # value latched while outside="hold"
    was_inside: bool = True
    toggled: bool = False        # mode=toggle latch
    armed: bool = True           # mode=toggle/trigger hysteresis arm
    gate_open: bool = False      # mode=gate latch


class MapResult(NamedTuple):
    send: bool
    value: float
    new_state: MapState


def _curve_lin(x: float) -> float:
    return x


def _curve_exp(x: float) -> float:
    # More resolution at the low end of travel, less at the top — the usual
    # "exponential feel" for a volume-like control.
    return x * x


def _curve_log(x: float) -> float:
    # Inverse of exp: more resolution at the top of travel.
    return math.sqrt(x)


def _curve_s(x: float) -> float:
    # Smoothstep: gentle near both ends, steep through the middle.
    return x * x * (3.0 - 2.0 * x)


_CURVES = {"lin": _curve_lin, "exp": _curve_exp, "log": _curve_log, "s": _curve_s}


def _zone(norm: float, zone_lo: float, zone_hi: float) -> tuple[float, bool]:
    """Rescale norm through [zone_lo, zone_hi] -> 0..1. Returns (frac, inside)."""
    span = zone_hi - zone_lo
    if span <= 0.0:
        return (0.0, norm >= zone_lo)
    inside = zone_lo <= norm <= zone_hi
    frac = (norm - zone_lo) / span
    return (0.0 if frac < 0.0 else (1.0 if frac > 1.0 else frac), inside)


def _slew(target: float, state: MapState, out_lo: float, out_hi: float,
          slew_ms: float, dt: float) -> float:
    if slew_ms <= 0.0 or dt <= 0.0 or not state.initialized:
        return target
    full_span = abs(out_hi - out_lo) or 1.0
    max_step = full_span * (dt * 1000.0 / slew_ms)
    delta = target - state.last_sent
    if delta > max_step:
        return state.last_sent + max_step
    if delta < -max_step:
        return state.last_sent - max_step
    return target


def apply_map(map_cfg: dict, norm: float, valid: bool, state: MapState,
              dt: float) -> MapResult:
    """Evaluate one map for one frame. `norm`/`valid` come from conditioning."""
    zone_lo = map_cfg.get("zone_lo", 0.0)
    zone_hi = map_cfg.get("zone_hi", 1.0)
    outside = map_cfg.get("outside", "clamp")
    invert = map_cfg.get("invert", False)
    curve = map_cfg.get("curve", "lin")
    out_lo = map_cfg.get("out_lo", 0.0)
    out_hi = map_cfg.get("out_hi", 1.0)
    mode = map_cfg.get("mode", "continuous")
    slew_ms = map_cfg.get("slew_ms", 0.0)
    steps = map_cfg.get("steps", 8)

    frac, inside = _zone(norm, zone_lo, zone_hi)

    if not inside:
        if outside == "idle":
            new_state = state._replace(was_inside=False)
            return MapResult(False, state.last_sent, new_state)
        if outside == "hold":
            frozen = state.zone_frozen if (state.initialized and not state.was_inside) \
                else state.last_output
            new_state = state._replace(was_inside=False, zone_frozen=frozen,
                                        last_output=frozen, initialized=True)
            sent = _slew(frozen, state, out_lo, out_hi, slew_ms, dt)
            return MapResult(True, sent, new_state._replace(last_sent=sent))
        # outside == "clamp" (default): fall through using the already-
        # clamped frac (0.0 below the zone, 1.0 above it) as if inside.

    if invert:
        frac = 1.0 - frac

    fn = _CURVES.get(curve, _curve_lin)
    curved = fn(frac)

    continuous_value = out_lo + curved * (out_hi - out_lo)

    if mode == "continuous":
        sent = _slew(continuous_value, state, out_lo, out_hi, slew_ms, dt)
        new_state = state._replace(initialized=True, was_inside=True,
                                    zone_frozen=continuous_value,
                                    last_output=continuous_value, last_sent=sent)
        return MapResult(True, sent, new_state)

    if mode == "step":
        n = max(1, int(steps))
        q = round(frac * n) / n
        value = out_lo + q * (out_hi - out_lo)
        changed = (not state.initialized) or value != state.last_output
        new_state = state._replace(initialized=True, was_inside=True,
                                    last_output=value,
                                    last_sent=value if changed else state.last_sent)
        return MapResult(changed, value, new_state)

    if mode == "gate":
        open_now = state.gate_open
        if frac >= _HYST_HI:
            open_now = True
        elif frac <= _HYST_LO:
            open_now = False
        value = out_hi if open_now else out_lo
        changed = (not state.initialized) or open_now != state.gate_open
        new_state = state._replace(initialized=True, was_inside=True,
                                    gate_open=open_now, last_output=value,
                                    last_sent=value)
        return MapResult(changed, value, new_state)

    if mode == "toggle":
        toggled = state.toggled
        armed = state.armed
        fired = False
        if valid:
            if frac >= _HYST_HI and armed:
                toggled = not toggled
                armed = False
                fired = True
            elif frac <= _HYST_LO and not armed:
                armed = True
        value = out_hi if toggled else out_lo
        new_state = state._replace(initialized=True, was_inside=True,
                                    toggled=toggled, armed=armed,
                                    last_output=value, last_sent=value)
        return MapResult(fired, value, new_state)

    if mode == "trigger":
        armed = state.armed
        fired = False
        if valid and frac >= _HYST_HI and armed:
            fired = True
            armed = False
        elif frac <= _HYST_LO:
            armed = True
        value = out_hi
        new_state = state._replace(initialized=True, was_inside=True,
                                    armed=armed, last_output=value,
                                    last_sent=value if fired else state.last_sent)
        return MapResult(fired, value, new_state)

    # Unknown mode: behave like continuous rather than silently dropping data.
    sent = _slew(continuous_value, state, out_lo, out_hi, slew_ms, dt)
    new_state = state._replace(initialized=True, was_inside=True,
                                last_output=continuous_value, last_sent=sent)
    return MapResult(True, sent, new_state)


def combine_values(contributions: list[tuple[str, float]]) -> Optional[float]:
    """Fold several maps' outputs that share one target into a single value.

    `contributions` is an ordered list of (combine_mode, value) for maps that
    fired THIS frame, in sensors.json map-list order. Returns None if the
    list is empty (nothing to send this frame for this target).

    replace — last writer wins (later maps in config order override earlier
              ones; this makes config order double as priority order).
    add     — running sum. Not clamped: if the maps' out_lo/out_hi ranges are
              meant to stay within 0..1 (e.g. an "/p" target) that is the
              config author's responsibility, same as it would be patching
              two envelopes into one CV input.
    max     — running max.
    """
    if not contributions:
        return None
    acc: Optional[float] = None
    for combine, value in contributions:
        if combine == "add":
            acc = value if acc is None else acc + value
        elif combine == "max":
            acc = value if acc is None else max(acc, value)
        else:  # "replace" (default/unknown)
            acc = value
    return acc
