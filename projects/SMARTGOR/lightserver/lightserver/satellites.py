"""Satellite nodes: small "dot" LED blobs anchored to a position on the
ring map (see ring.py), independently repositionable/addressable but known
to the system as part of the one room-light organism.

Config file `satellites.json` lives at the repo root (next to `ring.json`):

    {"satellites": [
      {"node": "dot1", "anchor": 0,   "mode": "mirror", "color": null},
      {"node": "dot2", "anchor": 960, "mode": "mirror", "color": null}
    ]}

Fields per satellite: node (must exist in nodes.json — that's where its
host/ip/port/pixel_count live), anchor (a ring index 0..ring_len-1 — the
point on the 1920px ring map the dot sits at), mode ("mirror" or
"independent"), color (hex string, used only in "independent" mode).

The satellites file is entirely OPTIONAL and additive: a missing file (or
an empty `satellites` list) means zero satellites and changes nothing
about ring/strip behavior. Anchors are runtime-repositionable (POST
/satellites/{name}) WITHOUT touching the ring's 1920px topology or its 3
strip segments — a dot just re-samples a different window of the same
ring canvas next frame.

Modes (documented explicitly so more can be added later):
  "mirror"      (default) — the dot shows a `pixel_count`-wide window of
                 the ring canvas, centered on `anchor`, wrapping at the
                 0/1919 seam. This was chosen over "flood the anchor pixel
                 across all N LEDs": a real window preserves whatever
                 spatial motion is crossing that point of the ring (the
                 orbiting orb, a wave, a trail) instead of flattening it
                 into a single color, and it's exactly as cheap (a 7-wide
                 numpy fancy-index each frame — see RingEngine._emit).
  "independent" — the dot ignores the ring and shows a flat `color` on
                 all its pixels. Set via POST /satellites/{name}
                 {"mode": "independent", "color": "#rrggbb"}; switch back
                 to "mirror" (with or without a new anchor) to release it.

Persistence: anchor/mode/color/boost are stored under a top-level
"satellites" key in the same state.json used for scene persistence (see
persist.py), so a repositioned/overridden dot survives a restart the same
way a scene does.

Boost (brightness-boost mechanism): the dots are physically weaker than
the ring strips (7px, 3V3-powered) so a dim ambient mirror window can
look anemic on them. `boost` is a per-satellite setting with three forms,
applied only in "mirror" mode (an "independent" mode's flat `color` is
already an explicit, maximal color choice, so boost is a no-op there —
see SatelliteRuntime.render):

  "off" / 1.0  — (default, canonical form "off") current behavior,
                 unchanged mirror window.
  <number>     — a fixed multiplier on the mirror window's RGB, e.g. 2.5.
                 Clipped at 255 by RESCALING (not per-channel
                 saturation): if `window * gain` would blow out a
                 channel past 255, the whole triple is scaled back down
                 by the same factor so hue/ratios are preserved instead
                 of desaturating toward white. See scale_preserve_hue().
  "auto"       — per-frame PEAK NORMALIZATION: the window's RGB is scaled
                 so its brightest channel hits 255 this frame, so the
                 dot is always maximally bright while every color ratio
                 (hue, and the relative structure across the 7 pixels)
                 stays intact. Guard rails (see AUTO_BOOST_FLOOR /
                 AUTO_BOOST_MAX_GAIN below):
                   - a fully black window stays black (gain doesn't
                     matter — 0 * anything is still 0 — so there's no
                     divide-by-zero to guard).
                   - a near-black window (peak < AUTO_BOOST_FLOOR, i.e.
                     dark enough that "full normalization" would mean an
                     enormous gain amplifying sensor-noise-like dimness
                     into a bright flash) is instead capped at
                     AUTO_BOOST_MAX_GAIN.
                 The gain is driven by a per-satellite peak-HOLD envelope
                 (2026-08 bugfix — see BOOST_ENVELOPE_RELEASE_DECAY and
                 the "vivid" bugfix-history paragraph below for the full
                 writeup, which applies identically here): the window's
                 peak channel value INSTANTLY raises the envelope (so a
                 bright orb sweeping in is boosted appropriately the same
                 frame) but the envelope only SLOWLY decays back down
                 (~3s half-life) once the peak drops — so a fading
                 trail/tail left behind by a departing bright object
                 renders through a held-steady gain (an accurate,
                 proportional fade) instead of being chased back up
                 toward max by a rising per-frame target.
  "vivid"      — PEAK-ANCHORED hue-preserving boost: every pixel is
                 normalized relative to the window's OWN brightest pixel
                 (V_peak) this frame, not pushed independently to max.
                 This is the fix for a real bug (2026-08 field report on
                 the yellow-comet scene): the original "vivid" pushed
                 EVERY pixel above the black-guard to V=1.0 independently,
                 which flattened a fading comet trail into a block of
                 flat max-color — "the trails do not work now looks bad".
                 The correct semantics (user spec): the window's brightest
                 pixel should hit max, but dimmer pixels must still read
                 dimmer — "it should still get dimmer when it gets
                 dimmer — brightest for brightest" — so fades/gradients
                 crossing the window survive, same spirit as "auto" but
                 computed in HSV with exact hue/saturation preservation.
                 Implementation (see apply_vivid() / rgb_to_hsv_np() /
                 hsv_to_rgb_np()): convert the window to HSV, leave H
                 exactly as computed (hue-accurate — no hue drift, ever).
                 Find V_peak = max(V) across the window this frame:
                   - V_peak < VIVID_BLACK_GUARD: the whole window is dark
                     — returned completely unchanged, "black stays
                     black" (same philosophy/threshold as AUTO_BOOST_FLOOR).
                   - otherwise, per pixel at/above VIVID_BLACK_GUARD:
                     V_out = (V_in / V_peak) ** vivid_lift — peak-
                     normalized so the brightest pixel in the window
                     lands at V_out == 1.0 while every other pixel keeps
                     its proportional position below it (order-preserving:
                     a dimmer input pixel is never V_out-brighter than a
                     brighter input pixel). The optional per-satellite
                     `vivid_lift` (default 0.7, valid range [0.3, 1.5])
                     shapes that ratio: 1.0 is strictly linear/
                     proportional; <1.0 (the default, 0.7) gives dim
                     trail pixels a mild perceptual lift — punchier, but
                     still clearly dimmer than the peak, never equal to
                     it. Pixels below VIVID_BLACK_GUARD are left
                     completely unchanged regardless of V_peak — "dark
                     stays dark", applied per-pixel same as before.
                 Saturation is controlled by the optional per-satellite
                 `vivid_saturation` (default `None`): with `None`, S is
                 left exactly as computed (hue-and-saturation-accurate,
                 only V changes); set to a float in `[0, 1]`, it acts as
                 a FLOOR — `S_out = max(S_in, floor)` — only raising
                 washed-out pixels toward full saturation, never
                 desaturating a pixel that was already more saturated
                 than the floor.
                 The `1 / V_peak` peak factor is driven by a per-satellite
                 peak-HOLD envelope (2026-08 bugfix, see
                 BOOST_ENVELOPE_RELEASE_DECAY and
                 SatelliteRuntime._apply_boost), NOT the raw per-frame
                 `1/V_peak` and NOT a symmetric EMA of it (that was the
                 previous, buggy design — see "Fade-out bugfix" below).
                 The envelope tracks `E_t = max(V_peak_t, E_{t-1} *
                 BOOST_ENVELOPE_RELEASE_DECAY)`: it jumps up INSTANTLY to
                 a new brighter peak (so the comet head still hits max
                 the very frame it crosses the window) but only decays
                 back down SLOWLY (~3s half-life) once the peak drops —
                 so once a bright object leaves the window, gain (`1/E`)
                 stays held near the head's peak while the window's own
                 content (e.g. a comet's fading trail) is rendered
                 through that ~flat gain: an accurate, monotonically
                 decreasing fade, matching what's actually happening on
                 the strip, not a re-amplified one. Only after several
                 seconds with no bright peak does the envelope relax and
                 dim ambient content get boosted bright again. The lift
                 exponent itself is a static per-satellite config value,
                 not a per-frame signal, so it is NOT run through the
                 envelope. (apply_vivid() also accepts an explicit
                 `peak_gain` for direct/test calls — `None` means "use
                 this window's own instantaneous 1/V_peak", i.e. the
                 fully-settled value with no temporal state to thread
                 through.)

                 Fade-out bugfix (2026-08, field report): vivid's gain
                 previously chased the raw per-frame `1/V_peak` target
                 with a SYMMETRIC exponential smoothing
                 (AUTO_BOOST_SMOOTHING_ALPHA, rises and falls at the same
                 rate). That's wrong for a departing comet: as the head
                 exits the window, V_peak shrinks frame over frame (only
                 the dimmer fading trail is left), so the *target* gain
                 keeps RISING — and the symmetric smoothing dutifully
                 chased it upward, re-amplifying the fading tail back
                 toward max instead of letting it fade, then snapping to
                 black once the trail finally dropped below
                 VIVID_BLACK_GUARD. User report: "the fadeout does not
                 work — it should fade out smoothly; we just wanted bias
                 towards brighter." The peak-hold envelope above fixes
                 this directly: gain no longer rises just because the
                 window got dimmer — only a NEW, brighter peak raises it;
                 a fading trail is held at ~the head's gain and fades
                 accurately, matching the strip.
                 `GET /satellites`' `effective_gain` for "vivid" is the
                 MEAN per-pixel gain (V_out/V_in) across pixels at/above
                 the guard, recomputed fresh every render call — 1.0 if
                 the whole window is at/under the guard.

Both spellings `1.0` and `"off"` are accepted on input and normalized to
the canonical `"off"` on output/persistence (see normalize_boost()).
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from .nodes import NodeConfig
from .protocol import UdpSender

DEFAULT_SATELLITES_PATH = Path(__file__).resolve().parent.parent / "satellites.json"

VALID_MODES = {"mirror", "independent"}

# -- boost tuning constants --------------------------------------------------
# Below this peak channel value (0..255), "auto" mode does not attempt full
# normalization (which would require a huge gain on a near-black window and
# amplify flicker/noise into a false bright flash); instead it applies a
# flat capped gain. Chosen so 255/AUTO_BOOST_FLOOR > AUTO_BOOST_MAX_GAIN,
# i.e. the cap is what actually binds below the floor.
AUTO_BOOST_FLOOR = 8.0
# Max gain "auto" mode will ever apply, whether capped-below-floor or (in
# principle) computed above it. Picked empirically as "enough to make a dim
# ambient window read as lit, not enough to make single-pixel noise flash".
AUTO_BOOST_MAX_GAIN = 12.0
# Exponential smoothing coefficient for the per-frame auto gain:
#   gain_t = gain_{t-1} + ALPHA * (target_gain_t - gain_{t-1})
# At ALPHA=0.25 a step change in target gain is ~90% tracked within about
# 8 frames (~270ms @ 30fps) — fast enough to feel responsive, slow enough
# that a single bright pixel sweeping through the window doesn't strobe.
#
# 2026-08 SUPERSEDED as the sole temporal control for "auto"/"vivid" gain
# (see BOOST_ENVELOPE_RELEASE_DECAY below) — kept only as a historical
# constant/import for anything that still references it; no longer used
# in the gain computation itself. What it did wrong: it smoothed the
# gain symmetrically (both rising AND falling), so on a per-frame
# peak-anchored gain (gain ~= 1/V_peak_this_frame) it could not tell
# "the bright object just left, the window is now a fading trail" apart
# from "the window just got dimmer, boost it more" — a departing comet
# head shrinks V_peak frame over frame, so the *target* gain quietly
# rises, and the EMA dutifully chases it upward, RE-AMPLIFYING the
# fading tail back toward max instead of letting it fade. That was the
# user-reported bug: "the fadeout does not work". A peak-HOLD envelope
# (fast attack, slow release — see below) replaces this: it also fixes
# the resulting "snap to black at the black-guard" artifact, since the
# gain no longer keeps climbing (only render-time V then falls through
# the guard) — the tail now decays smoothly to true zero instead.
AUTO_BOOST_SMOOTHING_ALPHA = 0.25

# -- peak-hold envelope (AGC-with-release) -----------------------------------
# Fixes the "fade-out doesn't fade" bug above by replacing the raw
# per-frame `1/V_peak` (or `255/peak`) target — smoothed symmetrically by
# AUTO_BOOST_SMOOTHING_ALPHA above — with an asymmetric envelope `E`
# tracked per-satellite (see SatelliteRuntime._boost_envelope /
# _vivid_envelope):
#
#     E_t = max(V_peak_t, E_{t-1} * BOOST_ENVELOPE_RELEASE_DECAY)
#     gain_t = <same floor/cap formula as before, but of E_t, not V_peak_t>
#
# - ATTACK is INSTANT: `max(V_peak_t, ...)` means a new, brighter peak
#   (e.g. the comet head crossing the window) raises E — and therefore
#   drops the gain to match — in the very same frame. This is why the
#   old per-frame EMA smoothing (AUTO_BOOST_SMOOTHING_ALPHA) is not
#   layered on top here: that smoothing existed specifically to make the
#   gain "breathe" instead of snap, but breathing the ATTACK is exactly
#   what re-introduces the fade-out bug (a slow-to-rise gain estimate
#   lags behind a departing peak, so it looks like it's still rising
#   just when the trail needs it to hold/fall) and it also delays "the
#   head frame hits max", which the fix explicitly requires to be
#   immediate. The envelope's own asymmetry (instant up / slow down)
#   supplies all the debounce that's needed, without that failure mode.
# - RELEASE is SLOW: while `V_peak_t <= E_{t-1} * decay`, E instead
#   decays geometrically toward the current (lower) peak. Concretely,
#   this means once a comet head passes and leaves a fading trail behind
#   it, E stays pinned near the head's peak brightness for the release
#   window — so `gain = 1/E` stays ~constant near 1.0 too, and the
#   trail's OWN true brightness (already fading on the strip) is what
#   the dot renders, scaled by a ~flat gain — an accurate, proportional,
#   monotonically-decreasing fade, not a re-amplified one. Only after
#   the envelope has fully relaxed down to ambient level (several
#   seconds of quiet) does the gain rise again and boost dim ambient
#   content bright — preserving "auto"/"vivid"'s original bias-toward-
#   brighter intent for content that ISN'T a fading trail.
# - decay picked for a ~3s half-life at the ring's ~30fps render rate:
#   `decay = 0.5 ** (1 / (half_life_s * fps))`. 3s is long enough that
#   any comet-trail-scale fade (a couple hundred ms to ~1-2s in this
#   scene) completes entirely inside the "held near peak" window, but
#   short enough that ambient content is boosted bright again well
#   inside a typical "things went quiet" pause, not tens of seconds
#   later.
BOOST_ENVELOPE_HALF_LIFE_S = 3.0
BOOST_ENVELOPE_ASSUMED_FPS = 30.0
BOOST_ENVELOPE_RELEASE_DECAY = 0.5 ** (1.0 / (BOOST_ENVELOPE_HALF_LIFE_S
                                              * BOOST_ENVELOPE_ASSUMED_FPS))
# == 0.9923279... : each frame E decays to ~99.23% of itself absent a
# higher new peak; compounding over 90 frames (3s @ 30fps) that is 0.5x.

# Below this per-pixel V (0..1, i.e. max(r,g,b)/255), "vivid" leaves a
# pixel completely unchanged instead of boosting it — "dark stays dark",
# same threshold philosophy/value as AUTO_BOOST_FLOOR above (8/255 on the
# 0..255 scale AUTO_BOOST_FLOOR is expressed on). Also gates the whole
# window: if the window's PEAK V is below this, the whole window is left
# unchanged (see apply_vivid()).
VIVID_BLACK_GUARD = 8.0 / 255.0

# Default/valid range for the per-satellite "vivid" lift exponent applied
# to the peak-normalized V ratio: V_out = (V_in / V_peak) ** vivid_lift.
# 1.0 is strictly linear/proportional; VIVID_LIFT_DEFAULT (0.7) gives dim
# trail pixels a mild perceptual lift (punchier, but still clearly
# dimmer than the peak — never pushed to equal it, unlike the old
# per-pixel-independent "vivid"). Range chosen so it can only ever be a
# mild reshaping, never a near-independent-maximization (that would
# reintroduce the "flat block" bug this mode was fixed to avoid).
VIVID_LIFT_DEFAULT = 0.7
VIVID_LIFT_MIN = 0.3
VIVID_LIFT_MAX = 1.5


class SatelliteConfigError(ValueError):
    pass


def normalize_boost(value) -> str | float:
    """Validate + canonicalize a boost setting.

    Accepts: "auto"; "vivid"; "off"; 1.0/1 (both spellings of "off"
    collapse to the canonical "off"); any other positive number
    (int/float, or a numeric string) as a fixed gain. Raises ValueError
    on anything else (non-positive numbers, bools, garbage strings) —
    callers translate that to a 422 (API) or SatelliteConfigError (config
    file load). `None` is treated as "off" (i.e. an omitted config-file
    field)."""
    if value is None:
        return "off"
    if isinstance(value, bool):
        raise ValueError(f"invalid boost: {value!r}")
    if isinstance(value, str):
        v = value.strip().lower()
        if v == "auto":
            return "auto"
        if v == "vivid":
            return "vivid"
        if v == "off":
            return "off"
        try:
            value = float(v)
        except ValueError:
            raise ValueError(f"invalid boost: {value!r}")
    if isinstance(value, (int, float)):
        num = float(value)
        if num <= 0:
            raise ValueError(f"boost gain must be positive: {value!r}")
        return "off" if num == 1.0 else num
    raise ValueError(f"invalid boost: {value!r}")


def validate_vivid_saturation(value) -> float | None:
    """Validate the optional per-satellite `vivid_saturation` saturation
    floor. `None` passes through unchanged (the default: keep each
    pixel's own saturation exactly, see apply_vivid()). Any other value
    must be a number in [0, 1] (a saturation FLOOR — see apply_vivid());
    raises ValueError otherwise, same 422/SatelliteConfigError translation
    pattern as normalize_boost()."""
    if value is None:
        return None
    if isinstance(value, bool):
        raise ValueError(f"invalid vivid_saturation: {value!r}")
    try:
        num = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"invalid vivid_saturation: {value!r}")
    if not (0.0 <= num <= 1.0):
        raise ValueError(f"vivid_saturation must be in [0, 1]: {value!r}")
    return num


def validate_vivid_lift(value) -> float:
    """Validate the optional per-satellite `vivid_lift` exponent applied
    to "vivid" mode's peak-normalized V ratio (see apply_vivid()).
    `None` means "use the default" — returns VIVID_LIFT_DEFAULT (0.7),
    NOT None (unlike validate_vivid_saturation(), where None is itself a
    valid sentinel meaning "no floor"; here None just means "omitted",
    the field always has a concrete numeric effect). Must be within
    [VIVID_LIFT_MIN, VIVID_LIFT_MAX] otherwise, same 422/
    SatelliteConfigError translation pattern as validate_vivid_saturation()."""
    if value is None:
        return VIVID_LIFT_DEFAULT
    if isinstance(value, bool):
        raise ValueError(f"invalid vivid_lift: {value!r}")
    try:
        num = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"invalid vivid_lift: {value!r}")
    if not (VIVID_LIFT_MIN <= num <= VIVID_LIFT_MAX):
        raise ValueError(
            f"vivid_lift must be in [{VIVID_LIFT_MIN}, {VIVID_LIFT_MAX}]: {value!r}")
    return num


def rgb_to_hsv_np(rgb: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Vectorized RGB(uint8 0..255) -> HSV(float 0..1), operating on a
    whole (n, 3) array at once (no per-pixel colorsys loop). Standard
    formula, verified bit-for-bit against colorsys.rgb_to_hsv per pixel.
    Returns (h, s, v), each shape (n,)."""
    f = rgb.astype(np.float64) / 255.0
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    maxc = np.max(f, axis=-1)
    minc = np.min(f, axis=-1)
    v = maxc
    delta = maxc - minc
    safe_max = np.where(maxc > 0, maxc, 1.0)  # avoid 0-division; s forced 0 below
    s = np.where(maxc > 0, delta / safe_max, 0.0)
    safe_delta = np.where(delta > 0, delta, 1.0)  # avoid 0-division; h forced 0 below
    rc = (maxc - r) / safe_delta
    gc = (maxc - g) / safe_delta
    bc = (maxc - b) / safe_delta
    h = np.zeros_like(v)
    h = np.where(maxc == r, bc - gc, h)
    h = np.where(maxc == g, 2.0 + rc - bc, h)
    h = np.where(maxc == b, 4.0 + gc - rc, h)
    h = (h / 6.0) % 1.0
    h = np.where(delta == 0, 0.0, h)
    return h, s, v


def hsv_to_rgb_np(h: np.ndarray, s: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Vectorized HSV(float 0..1) -> RGB(uint8 0..255), the inverse of
    rgb_to_hsv_np(). Standard formula, verified against colorsys."""
    i = np.floor(h * 6.0).astype(np.int64) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p = v * (1.0 - s)
    q = v * (1.0 - f * s)
    t = v * (1.0 - (1.0 - f) * s)
    conditions = [i == k for k in range(6)]
    r = np.select(conditions, [v, q, p, p, t, v])
    g = np.select(conditions, [t, v, v, q, p, p])
    b = np.select(conditions, [p, p, t, v, v, q])
    rgb = np.stack([r, g, b], axis=-1)
    return np.clip(np.round(rgb * 255.0), 0, 255).astype(np.uint8)


def apply_vivid(window: np.ndarray,
                saturation_floor: float | None,
                lift: float = 1.0,
                peak_gain: float | None = None) -> tuple[np.ndarray, float]:
    """PEAK-ANCHORED hue-preserving boost of an (n, 3) uint8 RGB block
    (see the "vivid" paragraph of the module docstring for the full
    rationale/bugfix history). Every pixel is normalized relative to the
    window's OWN brightest pixel this frame (V_peak), not pushed
    independently to max — this is what keeps a fading trail/gradient
    crossing the window looking like a fade instead of a flat block:

      - V_peak (max V across the whole window) < VIVID_BLACK_GUARD: the
        window is entirely dark — returned completely unchanged ("black
        stays black", no divide-by-zero risk since H/S/scaling are never
        touched).
      - Otherwise, per pixel at/above VIVID_BLACK_GUARD:
            V_out = clip(V_in * gain, 0, 1) ** lift
        where `gain` is `peak_gain` if given, else this window's own
        instantaneous `1 / V_peak` (i.e. the fully-settled value — see
        `peak_gain` below). Because `gain` is the SAME value for every
        pixel in the window, `V_in * gain` is a pure proportional
        rescaling — the brightest pixel (V_in == V_peak) always lands at
        exactly `1.0` before the `lift` exponent is applied, and pixel
        ORDER is preserved (a dimmer input pixel can never end up
        V_out-brighter than a brighter one, since x -> x**lift is
        monotonic increasing on [0, 1] for lift > 0). `lift` (see
        VIVID_LIFT_DEFAULT/_MIN/_MAX) reshapes that ratio: 1.0 is
        strictly linear; < 1.0 lifts dim pixels relatively higher
        (punchier) without ever equaling the peak (unless already equal
        to it pre-lift). Pixels below VIVID_BLACK_GUARD are left
        completely unchanged ("dark stays dark"), regardless of V_peak
        or `gain`.
      H is always kept exactly as computed (no hue drift). S is kept
      exactly as computed when `saturation_floor` is None; otherwise
      raised to `max(S, saturation_floor)` (never lowered).

    `peak_gain`: the CALLER's already-temporally-smoothed `1 / V_peak`
    factor (see SatelliteRuntime._apply_boost, which mirrors "auto"'s
    AUTO_BOOST_SMOOTHING_ALPHA smoothing of its own gain, so the dot's
    boost breathes instead of strobing/snapping as bright content enters
    or leaves the window). `None` (the default — convenient for direct/
    test calls that don't care about temporal behavior) means "use this
    window's own instantaneous `1 / V_peak`", i.e. the fully-settled
    value with no smoothing state to thread through — the window's own
    peak pixel then always lands at exactly `V_out == 1.0` (pre-lift).

    Returns (out_window, mean_gain) where mean_gain is the MEAN of
    V_out/V_in across pixels at/above the guard (1.0 if none are, i.e.
    the whole window is at/under the guard) — see GET /satellites'
    `effective_gain` for "vivid" boost."""
    if window.size == 0:
        return window.copy(), 1.0
    h, s, v = rgb_to_hsv_np(window)
    peak = float(v.max())
    if peak < VIVID_BLACK_GUARD:
        return window.copy(), 1.0

    above = v >= VIVID_BLACK_GUARD
    gain = peak_gain if peak_gain is not None else 1.0 / peak
    v_scaled = np.clip(v * gain, 0.0, 1.0)
    v_out = np.where(above, v_scaled ** lift, v)
    if saturation_floor is not None:
        s_out = np.maximum(s, saturation_floor)
    else:
        s_out = s

    shaped = hsv_to_rgb_np(h, s_out, v_out)
    out = np.where(above[:, None], shaped, window).astype(np.uint8)

    if np.any(above):
        # v is in 0..1 here (not 0..255) but the ratio is identical either
        # way; guard divide-by-zero even though `above` already implies
        # v > 0 (v >= VIVID_BLACK_GUARD > 0) — defensive, not load-bearing.
        v_in_above = v[above]
        v_out_above = v_out[above]
        gains = np.where(v_in_above > 0,
                        v_out_above / np.where(v_in_above > 0, v_in_above, 1.0), 1.0)
        mean_gain = float(np.mean(gains))
    else:
        mean_gain = 1.0
    return out, mean_gain


def scale_preserve_hue(window: np.ndarray, gain: float) -> np.ndarray:
    """Multiply an (n, 3) uint8 RGB block by `gain`, clipping at 255 by
    RESCALING the whole block (not per-channel saturation) if the
    brightest resulting channel would blow out — preserves hue/ratios
    instead of desaturating toward white. gain <= 0 or an all-zero window
    both safely produce an all-zero result (no divide-by-zero)."""
    if gain <= 0 or window.size == 0:
        return np.zeros_like(window)
    scaled = window.astype(np.float64) * gain
    peak = float(scaled.max())
    if peak > 255.0:
        scaled *= 255.0 / peak
    # round (not truncate) so a peak that lands at e.g. 254.9999999 due to
    # float rounding in the rescale above still reports as the intended 255.
    return np.clip(np.round(scaled), 0, 255).astype(np.uint8)


def hex_to_uint8(hex_str: str) -> np.ndarray:
    """'#rrggbb' (or 'rrggbb') -> uint8 rgb, no gamma (this is a direct
    output color for independent mode, not a scene-rendered one)."""
    s = hex_str.strip().lstrip("#")
    if len(s) == 3:
        s = "".join(c * 2 for c in s)
    if len(s) != 6:
        raise ValueError(f"bad hex color: {hex_str!r}")
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.uint8)


@dataclass
class SatelliteRuntime:
    """Live, mutable satellite state — anchor/mode/color can change at
    runtime via the API; `sender` is created once by RingEngine."""
    node: str
    pixel_count: int
    anchor: int = 0
    mode: str = "mirror"
    color: str | None = None
    boost: str | float = "off"
    # Saturation floor for "vivid" boost only (None = keep each pixel's
    # own S exactly). Ignored by every other boost mode. See apply_vivid().
    vivid_saturation: float | None = None
    # Lift exponent for "vivid" boost only (V_out = (V_in/V_peak)**lift).
    # Ignored by every other boost mode. See apply_vivid() /
    # VIVID_LIFT_DEFAULT. Persisted like vivid_saturation.
    vivid_lift: float = VIVID_LIFT_DEFAULT
    sender: UdpSender | None = None
    # Peak-HOLD envelope for "auto" gain (0..255 scale, same scale as
    # `window.max()`), carried frame to frame — see BOOST_ENVELOPE_* /
    # _apply_boost(). Starts at 0.0 ("nothing seen yet"), so the very
    # first bright frame's attack is still instant (max(peak, 0*decay)
    # == peak). Not persisted — a per-frame render artifact, not
    # configuration; it re-settles from 0.0 after a restart.
    _boost_envelope: float = field(default=0.0, repr=False, compare=False)
    # Live "auto" gain actually applied last render (derived fresh from
    # _boost_envelope each frame — see _apply_boost()), reported as
    # `effective_gain` in as_status(). Not persisted, same reasoning.
    _boost_gain: float = field(default=1.0, repr=False, compare=False)
    # Peak-HOLD envelope for "vivid" gain (0..1 V scale, same scale as
    # rgb_to_hsv_np()'s v), carried frame to frame — see BOOST_ENVELOPE_*
    # / _apply_boost(). Same "starts at 0.0, instant first attack"
    # reasoning as _boost_envelope. Not persisted.
    _vivid_envelope: float = field(default=0.0, repr=False, compare=False)
    # Live "vivid" 1/E peak-gain factor passed to apply_vivid() each
    # frame (derived from _vivid_envelope — see _apply_boost()). Not
    # persisted, same reasoning as _boost_envelope.
    _vivid_peak_gain: float = field(default=1.0, repr=False, compare=False)
    # Last-computed mean "vivid" per-pixel gain (V_out/V_in, see
    # apply_vivid()), recomputed fresh every render call, reported as
    # `effective_gain` in as_status(). Not persisted, same reasoning as
    # _boost_envelope: a render artifact for status reporting, not config.
    _vivid_gain: float = field(default=1.0, repr=False, compare=False)

    def render(self, ring_frame: np.ndarray, ring_len: int) -> np.ndarray:
        """This satellite's (pixel_count, 3) uint8 frame for this tick."""
        if self.mode == "independent" and self.color:
            rgb = hex_to_uint8(self.color)
            return np.tile(rgb, (self.pixel_count, 1))
        half = self.pixel_count // 2
        idx = [(self.anchor - half + i) % ring_len for i in range(self.pixel_count)]
        window = ring_frame[idx]
        return self._apply_boost(window)

    def _apply_boost(self, window: np.ndarray) -> np.ndarray:
        """Boost only applies to a mirror(-shaped) window — an
        independent-mode flat color is already an explicit choice and
        returns before this is ever called; see render().

        "auto" and "vivid" both drive their gain off a per-satellite
        peak-HOLD envelope (see BOOST_ENVELOPE_RELEASE_DECAY): the
        envelope jumps UP instantly to a new, brighter peak (fast
        attack — the comet head hits max the frame it crosses the
        window) but only decays slowly back down (slow release — ~3s
        half-life), so once a bright object leaves the window, the
        envelope (and therefore the gain) stays pinned near that peak
        while whatever's left in the window fades — rendering the
        window's OWN true, already-fading brightness through a ~flat
        gain, i.e. an accurate, monotonically-decreasing fade, not a
        re-amplified one. This replaces the old symmetric per-frame EMA
        (AUTO_BOOST_SMOOTHING_ALPHA) which chased the RISING per-frame
        target as a departing peak shrank V_peak frame over frame,
        re-amplifying the fading tail back toward max (the
        "fadeout does not work" bug) — see the module docstring and the
        BOOST_ENVELOPE_* comment block above for the full writeup."""
        if self.boost == "off":
            return window
        if self.boost == "vivid":
            # V is just max(r,g,b)/255 by definition — no need for a full
            # HSV conversion just to find the window's peak for the
            # envelope; apply_vivid() does its own HSV conversion below.
            peak = float(window.max()) / 255.0 if window.size else 0.0
            self._vivid_envelope = max(peak, self._vivid_envelope
                                       * BOOST_ENVELOPE_RELEASE_DECAY)
            self._vivid_peak_gain = 1.0 / max(self._vivid_envelope, VIVID_BLACK_GUARD)
            out, self._vivid_gain = apply_vivid(window, self.vivid_saturation,
                                                self.vivid_lift,
                                                self._vivid_peak_gain)
            return out
        if self.boost == "auto":
            peak = float(window.max()) if window.size else 0.0
            self._boost_envelope = max(peak, self._boost_envelope
                                       * BOOST_ENVELOPE_RELEASE_DECAY)
            env = self._boost_envelope
            if env <= 0:
                target = 0.0
            elif env >= AUTO_BOOST_FLOOR:
                target = 255.0 / env
            else:
                target = AUTO_BOOST_MAX_GAIN
            self._boost_gain = target
            return scale_preserve_hue(window, self._boost_gain)
        # numeric fixed gain
        return scale_preserve_hue(window, float(self.boost))

    def as_status(self) -> dict:
        if self.boost == "auto":
            effective_gain = round(self._boost_gain, 3)
        elif self.boost == "vivid":
            effective_gain = round(self._vivid_gain, 3)
        elif isinstance(self.boost, (int, float)):
            effective_gain = float(self.boost)
        else:
            effective_gain = 1.0
        d = {"node": self.node, "anchor": self.anchor, "mode": self.mode,
             "color": self.color, "pixel_count": self.pixel_count,
             "boost": self.boost, "vivid_saturation": self.vivid_saturation,
             "vivid_lift": self.vivid_lift,
             "effective_gain": effective_gain}
        snd = self.sender
        if snd is None:
            d["online"] = False
        else:
            d.update({
                "resolved_ip": snd.resolved_ip,
                "last_known_ip": snd.ip,
                "frames_sent": snd.frames_sent,
                "last_send_ts": snd.last_send_ts,
                "last_error": snd.last_error,
                "online": bool((snd.resolved_ip or snd.ip)
                               and snd.frames_sent > 0 and snd.last_error is None),
            })
        return d


def load_satellites(path: str | Path | None, configs: list[NodeConfig],
                    ring_len: int) -> list[SatelliteRuntime]:
    """Load + validate satellites.json against the node registry + ring
    length. Missing file -> []. Raises SatelliteConfigError on
    inconsistency (unknown node, duplicate node, anchor out of range, bad
    mode) — a broken satellites config should fail loudly at boot, same as
    a broken ring config."""
    p = Path(path) if path else DEFAULT_SATELLITES_PATH
    if not p.is_file():
        return []
    try:
        raw = json.loads(p.read_text())
    except (OSError, json.JSONDecodeError) as e:
        raise SatelliteConfigError(f"cannot read satellites config {p}: {e}")
    sat_list = raw.get("satellites") if isinstance(raw, dict) else None
    if sat_list is None:
        return []
    if not isinstance(sat_list, list):
        raise SatelliteConfigError(f"{p}: 'satellites' must be a list")

    by_name = {c.name: c for c in configs}
    out: list[SatelliteRuntime] = []
    seen: set[str] = set()
    for i, d in enumerate(sat_list):
        if not isinstance(d, dict) or "node" not in d:
            raise SatelliteConfigError(f"satellites[{i}]: must be an object with 'node'")
        node = str(d["node"])
        if node in seen:
            raise SatelliteConfigError(f"satellites[{i}]: duplicate node '{node}'")
        seen.add(node)
        cfg = by_name.get(node)
        if cfg is None:
            raise SatelliteConfigError(f"satellites[{i}]: unknown node '{node}' "
                                       f"(not in nodes.json)")
        try:
            anchor = int(d.get("anchor", 0))
        except (TypeError, ValueError):
            raise SatelliteConfigError(f"satellites[{i}] '{node}': bad anchor")
        if not (0 <= anchor < ring_len):
            raise SatelliteConfigError(
                f"satellites[{i}] '{node}': anchor {anchor} out of range "
                f"0..{ring_len - 1}")
        mode = str(d.get("mode", "mirror"))
        if mode not in VALID_MODES:
            raise SatelliteConfigError(
                f"satellites[{i}] '{node}': invalid mode '{mode}' "
                f"(must be one of {sorted(VALID_MODES)})")
        color = d.get("color")
        if color is not None:
            try:
                hex_to_uint8(str(color))
            except ValueError:
                raise SatelliteConfigError(
                    f"satellites[{i}] '{node}': bad color {color!r}")
        try:
            boost = normalize_boost(d.get("boost", "off"))
        except ValueError as e:
            raise SatelliteConfigError(f"satellites[{i}] '{node}': {e}")
        try:
            vivid_saturation = validate_vivid_saturation(d.get("vivid_saturation"))
        except ValueError as e:
            raise SatelliteConfigError(f"satellites[{i}] '{node}': {e}")
        try:
            vivid_lift = validate_vivid_lift(d.get("vivid_lift"))
        except ValueError as e:
            raise SatelliteConfigError(f"satellites[{i}] '{node}': {e}")
        out.append(SatelliteRuntime(node=node, pixel_count=cfg.pixel_count,
                                    anchor=anchor, mode=mode, color=color,
                                    boost=boost, vivid_saturation=vivid_saturation,
                                    vivid_lift=vivid_lift))
    return out
