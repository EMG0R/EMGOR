"""neuralGrid encoder: raw ESP32 sensor traffic -> dimensional frames.

Declared geometry:

    1x 2D  pad             8x8 NeoTrellis          label "position"
    1x 3D  lidar           4x VL53L1X, triangulated label "distance"
    1x 1D  force           4x FSR summed, GLOBAL    label "force"  (assoc: pad)
    4x 1D  softpot_n/s/e/w ribbons                  label "ribbon"

Two deliberate choices worth not undoing:

* **Force is one global channel, not per-pad z.** The four FSRs are unevenly
  placed under the plate and read the whole surface. The hardware genuinely
  cannot say which of two simultaneously-held pads is pressed harder, so
  declaring a per-point z would be a lie about the hardware. A Linnstrument or
  Push *would* have true per-point z; the format supports both shapes, and the
  soft ``associated`` link tells a model to use this force as pad velocity.

* **Lidar triangulation happens HERE, not downstream.** Device-specific math
  belongs in the encoder; meaning-assignment belongs in the model. Declaring a
  portable 3D volume means a webcam or Leap could later fill the same channel.
  Where the geometry is weakly determined -- a half-seen axis, or z, which four
  perimeter sensors cannot really measure -- the encoder says so in sigma
  rather than inventing a confident number.

FSRs are deliberately NOT calibrated from a file: they auto-range here as you
play (user's call, 2026-09-16). Softpot travel and lidar range DO come from
``calibration.json``, loaded at runtime so recalibrating never needs a rebuild.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, field, replace
from typing import Optional

from .. import format as fmt
from ..fsr_calibrate import force_from_profile
from ..pad_forces import overall_force, solve_pad_forces
from ..trilaterate import in_plane, solve_xy
from ..onset_force import OnsetForce
from ..force_shape import DEFAULT_CURVE, HEADROOM, ForceSmoother, apply_curve

GRID_W = 8
GRID_H = 8
N_SOFTPOT = 4
N_FSR = 4
SOFTPOT_IDS = ("softpot_n", "softpot_s", "softpot_e", "softpot_w")
LIDAR_INVALID = 0xFFFF

#: Compass positions are POSITIONS, not wiring order. The firmware's N/S/E/W is
#: just the order things happen to be soldered (softpots GPIO32/33/34/35, lidar
#: XSHUT 4/13/14/27); which physical strip or sensor that is was never
#: established. Calibration discovers the real mapping; these are only the
#: fallback when nothing has been discovered yet.
LIDAR_POSITIONS = ("n", "s", "e", "w")

#: The PHYSICAL layout, established directly by the user 2026-09-17:
#: firmware N = his LEFT, S = his TOP, E = his RIGHT, W = his BOTTOM.
#: Keys here are playing positions (n = top, s = bottom, e = right, w = left)
#: and values are hardware indices in firmware order (N, S, E, W).
#:
#: This matters beyond labelling: the previous wiring-order default paired
#: firmware N against S for the y axis -- the user's LEFT against his TOP,
#: which are 90 degrees apart, not opposing. Triangulation was geometrically
#: wrong, not merely mislabelled.
DEFAULT_LIDAR_MAP = {"n": 1, "e": 2, "s": 3, "w": 0}

#: The compass keys above are internal (they name the OPPOSING PAIRS that make
#: triangulation work). Anything a human reads should use the edge they can
#: actually see, or "n" looks like it means the sensor labelled N in the
#: firmware -- which is a different sensor entirely.
POSITION_NAME = {"n": "top", "e": "right", "s": "bottom", "w": "left"}

#: Below this raw ADC count a channel is considered untouched. With the fitted
#: 82-100k pulldowns an untouched strip reads a clean 0 (HANDOFF §3), so this
#: only has to clear the noise floor.
TOUCH_FLOOR = 120

#: FSR response is roughly logarithmic in force; raw counts lie. Same cubic
#: shaping proven on PMOR (`_fsr_scale.h`).
FSR_EXPONENT = 3.0

#: Summed force below this above rest is drift, not playing. The rest point on
#: this instrument wanders hundreds of counts between captures, so without a
#: floor "above rest" is never actually zero -- and the compressive curve then
#: multiplies that residue into real-looking force.
FSR_DEADZONE = 90

#: Auto-ranging ceiling for the summed FSR reading: the total the encoder
#: assumes is already "full" before it has ever seen you press hard. Without it
#: the observed maximum chases the current reading and every press reads 1.0.
FSR_NOMINAL_FULL = 1200.0

#: The auto-ranged force scale is the hardest press seen in this window. It has
#: to fall as well as rise: the sensors drifted until a press produced a tenth
#: of what the profile was calibrated against, and a scale that only grows can
#: never come back down to meet that. The profile's RATIOS survive drift; its
#: magnitudes do not, so the projection supplies position-independence and this
#: supplies level.
FORCE_SCALE_WINDOW_S = 20.0

#: Never divide by less than this -- a near-zero scale turns noise into a
#: full-scale press.
FORCE_SCALE_FLOOR = 0.02

#: How fast the scale chases that window peak. SLOWLY, and this is the whole
#: trick: snapping to each new peak meant every press instantly became its own
#: full scale, so pressing harder read exactly the same as pressing softly --
#: dynamics gone. Creeping over ~a second lets a harder press ride above the
#: scale while it catches up, so playing dynamics survive on the timescale of a
#: gesture and drift is still tracked on the timescale of minutes.
FORCE_SCALE_RATE = 0.02

#: How far back the rolling rest floor looks. These FSRs drift hundreds of
#: counts, so a one-shot rest snapshot goes stale within minutes and strands
#: the sensor below its own zero -- measured at 537 counts under. Whatever a
#: sensor has actually reached in this window is, by definition, at least as
#: low as resting. Long enough that a sustained press cannot erode it.
FSR_FLOOR_WINDOW_S = 12.0

#: Sigma floor for a well-determined lidar axis, and the penalty applied when
#: an axis is half-seen or (always) for z.
LIDAR_SIGMA_GOOD = 0.08
LIDAR_SIGMA_WEAK = 0.45

#: Fastest a hand plausibly moves, mm per second. A jump beyond this between
#: consecutive frames is the sensor changing its mind about WHAT it is looking
#: at -- observed on hardware as a reading climbing to ~240mm and then
#: REVERSING as the beam latches onto the forearm. The value is not wrong
#: exactly; it is a measurement of something else, so it is reported at low
#: confidence rather than as a trustworthy number.
LIDAR_MAX_SPEED_MM_S = 2000.0

#: EMA coefficient for the fused hand position. Fast enough that the lag is not
#: playable-through (~13ms group delay at the 20Hz lidar rate) but enough to
#: take the edge off a jittering triangulation. The point is meant to be blurry
#: in SIGMA, not unsteady in position.
LIDAR_SMOOTH = 0.65

#: How fast the height estimate moves. SLOW on purpose: a hand does not change
#: altitude quickly, and height is the noisiest thing these four coplanar
#: sensors measure. Smoothing it before it is subtracted keeps that noise out
#: of x and y entirely.
LIDAR_HEIGHT_RATE = 0.08

#: Frames spent easing across a change in which sensors are contributing. The
#: answer genuinely jumps there -- it is solved from different information --
#: and easing costs nothing, because the hand has barely moved while it
#: happens. Steady tracking stays fast; slowing THAT is what reads as latency.
LIDAR_HANDOVER_FRAMES = 10

#: How fast a rangefinder's contribution fades in and out. A sensor acquiring
#: or losing the hand should blend rather than pop -- the geometry it implies is
#: real either way, it just arrives gradually.
LIDAR_PRESENCE_RATE = 0.12

#: Nothing is held over when the contributing set changes. Easing through it
#: looked smoother but carried a stale position while the hand was already
#: somewhere else, which is worse than a visible step: the reading was simply
#: wrong for those frames.

#: The volume the hand actually moves in, learned from what the sensors return
#: rather than taken from Calibration.lidar_max_mm. The firmware runs the parts
#: in SHORT distance mode and skin is a poor 940nm reflector, so the real
#: working range is a few centimetres -- normalizing height against 1200mm put
#: the hand at maximum height permanently. Starts as a plausible guess and only
#: ever widens.
LIDAR_SPAN_NEAR = 40.0
LIDAR_SPAN_FAR = 220.0

#: How wide the playing surface is, in millimetres. Trilateration needs real
#: distances in the same units as the sensor positions, so this is a physical
#: fact about the instrument -- four 4x4 NeoTrellis boards, roughly 120mm
#: across. Inferring it from the readings instead was circular: the geometry
#: then depended on whatever had been measured so far.
GRID_MM = 120.0

#: Anything nearer than this is the housing, not a hand -- the walls on this
#: instrument sit at 15-40mm. A plain distance cutoff, deliberately replacing
#: an earlier stillness heuristic that was wrong in the case that matters most:
#: a hand HELD STILL is not moving, so it got classified as background and
#: vanished. Holding a position is playing.
LIDAR_MIN_MM = 50


def _cell_centre(i: int, n: int) -> float:
    """Centre of cell ``i`` of ``n`` on a 0-1 axis -- not its corner."""
    return (i + 0.5) / n


@dataclass(frozen=True)
class Calibration:
    """Runtime calibration. Absent entries fall back to sane defaults.

    Note there is no FSR section: force auto-ranges in the encoder.
    """

    softpot: dict = field(default_factory=dict)  # id -> (raw_min, raw_max)
    lidar_max_mm: int = 1200
    #: position -> hardware input index, discovered by calibration.
    softpot_map: dict = field(default_factory=dict)
    lidar_map: dict = field(default_factory=dict)
    #: position -> (near_mm, far_mm) the sensor can actually resolve. Anything
    #: outside is not a measurement of the hand. Empty = accept everything.
    lidar_bounds: dict = field(default_factory=dict)
    #: per-sensor resting counts. The housing pre-loads some FSRs, so they sit
    #: well above zero untouched; that is a property of the assembly and changes
    #: every time it is opened, which is exactly what a calibration file is for.
    fsr_rest: tuple = ()
    #: pad index -> the 4-sensor response recorded for a press AT that pad.
    #: The FSRs are unevenly placed, so the same force at two corners gives
    #: wildly different raw totals; this is what makes force position-
    #: independent. Re-measured every time the housing is opened.
    fsr_pads: dict = field(default_factory=dict)
    #: the raw per-pad measurements behind fsr_pads, kept so the surface can be
    #: refitted (different order, more pads) without re-hitting all 64.
    fsr_pads_raw: dict = field(default_factory=dict)
    #: pad -> every sample of its calibration sweep. Kept so the response can
    #: be plotted, a rejected pad diagnosed, or a better model fitted later
    #: without re-hitting all 64.
    fsr_curves: dict = field(default_factory=dict)
    #: Compression exponent for the force response. 1.0 = linear; lower puts
    #: more resolution in the light end and flattens the hardest presses.
    #: A by-feel number -- tune it by playing, not by argument.
    force_curve: float = DEFAULT_CURVE
    #: Degrees the grid is mounted relative to how it is played. Applied to the
    #: pad channel and to LED output ONLY -- never to the force profile, which
    #: is keyed to physical hardware positions and must keep pointing at the
    #: pad that was actually measured.
    #: The NeoTrellis is mounted a half turn from how it is played, so the UI
    #: has to be 180 from the hardware. Established by playing it, 2026-09-17 --
    #: this is the instrument's physical fact, not a preference, so it is the
    #: default rather than something to rediscover with the buttons.
    grid_rotation: int = 180
    #: (near_mm, far_mm) the hand actually moves through. Learned live, then
    #: frozen here -- a normalization that keeps shifting means the same
    #: gesture maps to different numbers on different days, which is poison for
    #: anything trained on recorded sessions.
    lidar_span: tuple = ()
    #: Near cutoff in mm; anything closer is housing, not hand.
    lidar_min_mm: int = LIDAR_MIN_MM
    #: Width of the playing surface in mm -- the scale trilateration works in.
    grid_mm: float = GRID_MM
    #: Mirrors, applied AFTER the rotation. Rotation alone cannot produce a
    #: mirror, so a flipped grid can never be fixed by turning it -- which is
    #: exactly why cycling quarters never landed.
    grid_flip_x: bool = False
    grid_flip_y: bool = False
    #: Ribbons whose travel runs the other way. Which end reads zero is a
    #: wiring detail -- a strip can be mounted either way round -- so this is
    #: per channel, not global.
    softpot_invert: tuple = ()

    def softpot_range(self, channel_id: str) -> tuple[int, int]:
        lo, hi = self.softpot.get(channel_id, (TOUCH_FLOOR, 4095))
        return int(lo), int(hi)

    def softpot_index(self, channel_id: str) -> int:
        """Hardware input feeding this position; wiring order if undiscovered."""
        if channel_id in self.softpot_map:
            return int(self.softpot_map[channel_id])
        return SOFTPOT_IDS.index(channel_id)

    def lidar_in_bounds(self, position: str, mm: int) -> bool:
        b = self.lidar_bounds.get(position)
        if not b:
            return True
        return int(b[0]) <= mm <= int(b[1])

    def lidar_index(self, position: str) -> int:
        if position in self.lidar_map:
            return int(self.lidar_map[position])
        return DEFAULT_LIDAR_MAP[position]

    def view_xy(self, x: int, y: int) -> tuple[int, int]:
        """Hardware pad -> the position the player sees it at."""
        x, y = int(x), int(y)
        mx, my = GRID_W - 1, GRID_H - 1
        if self.grid_rotation == 90:
            x, y = (mx - y, x)
        elif self.grid_rotation == 180:
            x, y = (mx - x, my - y)
        elif self.grid_rotation == 270:
            x, y = (y, my - x)
        if self.grid_flip_x:
            x = mx - x
        if self.grid_flip_y:
            y = my - y
        return (x, y)

    def view_point(self, x: float, y: float) -> tuple[float, float]:
        """Continuous position -> what the player sees.

        The same orientation the pad channel gets. Rotating the pads but not
        the lidar left them a half turn apart: both individually right, and
        left reading as right.
        """
        if self.grid_rotation == 90:
            x, y = (1.0 - y, x)
        elif self.grid_rotation == 180:
            x, y = (1.0 - x, 1.0 - y)
        elif self.grid_rotation == 270:
            x, y = (y, 1.0 - x)
        if self.grid_flip_x:
            x = 1.0 - x
        if self.grid_flip_y:
            y = 1.0 - y
        return (x, y)

    def fsr_rest_at(self, i: int) -> int:
        return int(self.fsr_rest[i]) if i < len(self.fsr_rest) else 0

    def to_dict(self) -> dict:
        return {
            "v": 1,
            "softpot": {k: list(v) for k, v in self.softpot.items()},
            "lidar_max_mm": self.lidar_max_mm,
            "softpot_map": dict(self.softpot_map),
            "lidar_map": dict(self.lidar_map),
            "lidar_bounds": {k: list(v) for k, v in self.lidar_bounds.items()},
            "fsr_rest": list(self.fsr_rest),
            "fsr_pads": {str(k): list(v) for k, v in self.fsr_pads.items()},
            "fsr_pads_raw": {str(k): list(v) for k, v in self.fsr_pads_raw.items()},
            "fsr_curves": {
                str(k): [list(r) for r in v] for k, v in self.fsr_curves.items()
            },
            "force_curve": self.force_curve,
            "grid_rotation": self.grid_rotation,
            "lidar_span": list(self.lidar_span),
            "lidar_min_mm": self.lidar_min_mm,
            "grid_mm": self.grid_mm,
            "grid_flip_x": bool(self.grid_flip_x),
            "grid_flip_y": bool(self.grid_flip_y),
            "softpot_invert": list(self.softpot_invert),
        }

    def save(self, path) -> None:
        """Atomic write, keeping the previous good copy alongside.

        This file is expensive: sixty-four pad sweeps is twenty minutes. One
        bad write, or one program overwriting another's fields, should never
        be unrecoverable.
        """
        path = str(path)
        backup = path.replace(".json", ".backup.json")
        if backup != path and os.path.exists(path):
            try:
                with open(path, "rb") as src, open(backup, "wb") as dst:
                    dst.write(src.read())
            except OSError:
                pass  # a failed backup must never block the save itself
        tmp = path + ".tmp"
        with open(tmp, "w") as fh:
            json.dump(self.to_dict(), fh, indent=2)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, path)

    @property
    def is_calibrated(self) -> bool:
        """True once this instrument has been set up at all."""
        return bool(self.fsr_pads or self.softpot or self.fsr_rest)

    @classmethod
    def load(cls, path) -> "Calibration":
        try:
            with open(str(path)) as fh:
                d = json.load(fh)
        except (OSError, ValueError):
            return cls()
        return cls(
            softpot={k: (int(v[0]), int(v[1])) for k, v in d.get("softpot", {}).items()},
            lidar_max_mm=int(d.get("lidar_max_mm", 1200)),
            softpot_map={k: int(v) for k, v in d.get("softpot_map", {}).items()},
            lidar_map={k: int(v) for k, v in d.get("lidar_map", {}).items()},
            lidar_bounds={
                k: (int(v[0]), int(v[1])) for k, v in d.get("lidar_bounds", {}).items()
            },
            fsr_rest=tuple(int(v) for v in d.get("fsr_rest", ())),
            fsr_pads={
                int(k): tuple(int(x) for x in v)
                for k, v in d.get("fsr_pads", {}).items()
            },
            fsr_pads_raw={
                int(k): tuple(int(x) for x in v)
                for k, v in d.get("fsr_pads_raw", {}).items()
            },
            fsr_curves={
                int(k): [tuple(int(x) for x in row) for row in v]
                for k, v in d.get("fsr_curves", {}).items()
            },
            force_curve=float(d.get("force_curve", DEFAULT_CURVE)),
            grid_rotation=int(d.get("grid_rotation", 0)),
            lidar_span=tuple(float(v) for v in d.get("lidar_span", ())),
            lidar_min_mm=int(d.get("lidar_min_mm", LIDAR_MIN_MM)),
            grid_mm=float(d.get("grid_mm", GRID_MM)),
            grid_flip_x=bool(d.get("grid_flip_x", False)),
            grid_flip_y=bool(d.get("grid_flip_y", False)),
            softpot_invert=tuple(d.get("softpot_invert", ())),
        )


class NeuralGridEncoder:
    """Stateful: fed by the link's callbacks, sampled by :meth:`frame`."""

    source = "neuralgrid"

    def __init__(
        self,
        calibration: Optional[Calibration] = None,
        calibration_path=None,
    ):
        self._calibration_path = calibration_path
        if calibration is not None:
            self._cal = calibration
        elif calibration_path is not None:
            self._cal = Calibration.load(calibration_path)
        else:
            self._cal = Calibration()
        self._held: dict[tuple[int, int], float] = {}
        self._pending_edges: list[fmt.Edge] = []
        self._analog: tuple[int, ...] = (0,) * (N_SOFTPOT + N_FSR)
        self._lidar: tuple[int, ...] = (LIDAR_INVALID,) * 4
        # Auto-ranging window for the summed FSR reading. The FLOOR is the
        # resting level: found on hardware 2026-09-16 that the plate rests well
        # above zero, and ranging against the ceiling alone pinned force at
        # 0.99 with nothing touching it. Both ends track observation, so no
        # calibration file is involved -- the FSRs are processed here.
        self._prev_lidar: dict[str, tuple[float, int]] = {}  # position -> (t, mm)
        self._lidar_point: Optional[tuple[float, float, float]] = None
        self._lidar_contributing: tuple[str, ...] = ()
        self._lidar_presence = {p: 0.0 for p in LIDAR_POSITIONS}
        self._lidar_handover = 0
        self._lidar_height = 0.0
        self._lidar_sigma: tuple[float, float, float] = (1.0, 1.0, 1.0)
        span = self._cal.lidar_span
        self._lidar_near = float(span[0]) if len(span) == 2 else LIDAR_SPAN_NEAR
        self._lidar_far = float(span[1]) if len(span) == 2 else LIDAR_SPAN_FAR
        self._lidar_span_frozen = len(span) == 2
        self._lidar_suspect: set[str] = set()
        self._lidar_seen = 0
        self._pad_forces: dict[int, float] = {}
        self._force_smoother = ForceSmoother()
        self._fsr_floor: Optional[float] = None
        self._fsr_seen: list = [[] for _ in range(N_FSR)]   # (t, raw) per sensor
        self._onset_force = OnsetForce()
        self._force_peaks: list = []      # (t, projection) while playing
        self._force_scale = FORCE_SCALE_FLOOR
        self._now = 0.0
        self._fsr_ceiling = FSR_NOMINAL_FULL

    # ---- calibration -----------------------------------------------------
    @property
    def calibration(self) -> Calibration:
        return self._cal

    def reload_calibration(self) -> None:
        """Re-read the calibration file. Never requires a restart."""
        if self._calibration_path is not None:
            self._cal = Calibration.load(self._calibration_path)
            span = self._cal.lidar_span
            if len(span) == 2:
                self._lidar_near, self._lidar_far = float(span[0]), float(span[1])
                self._lidar_span_frozen = True

    @property
    def learned_lidar_span(self) -> tuple[float, float]:
        """What the encoder has observed -- freeze this into calibration."""
        return (self._lidar_near, self._lidar_far)

    # ---- inputs ----------------------------------------------------------
    def on_key(self, x: int, y: int, pressed: bool, t_mono: float) -> None:
        # _held stays in HARDWARE coordinates: the force profile is keyed to
        # physical pads and must keep pointing at the one actually measured.
        # Rotation is a view transform, applied on the way out.
        key = (int(x), int(y))
        vx, vy = self._cal.view_xy(*key)
        point = (_cell_centre(vx, GRID_W), _cell_centre(vy, GRID_H))
        if pressed:
            self._held[key] = float(t_mono)
            kind = "onset"
            if len(self._held) == 1:
                # first note down: this press owns the reference
                self._onset_force.begin(float(t_mono))
        else:
            self._held.pop(key, None)
            kind = "release"
            if not self._held:
                self._onset_force.end()
        self._pending_edges.append(
            fmt.Edge(channel="pad", kind=kind, t_mono=float(t_mono), point=point)
        )

    def drain_onsets(self) -> tuple:
        """Edges produced so far, without emitting a frame. For anything that
        only cares about note starts."""
        return self._take_edges()

    def _take_edges(self) -> tuple:
        out = tuple(self._pending_edges)
        self._pending_edges.clear()
        return out

    def drain_onsets(self) -> tuple:
        """Edges produced so far, without emitting a frame. For anything that
        only cares about note starts."""
        return self._take_edges()

    def _track_floor(self, t: float) -> None:
        """Roll each sensor's observed minimum forward."""
        raws = self._analog[N_SOFTPOT : N_SOFTPOT + N_FSR]
        for i, raw in enumerate(raws):
            hist = self._fsr_seen[i]
            hist.append((t, int(raw)))
            cutoff = t - FSR_FLOOR_WINDOW_S
            while hist and hist[0][0] < cutoff:
                hist.pop(0)

    def _floor_at(self, i: int) -> int:
        """Effective zero for one sensor: the calibrated rest, or lower if the
        sensor has actually been lower. Never higher -- a long press must not
        drag the zero up and erase the sensor's range."""
        rest = self._cal.fsr_rest_at(i)
        hist = self._fsr_seen[i] if i < len(self._fsr_seen) else []
        return min(rest, min((v for _, v in hist), default=rest))

    def on_analog(self, values, t_mono: float = 0.0) -> None:
        self._analog = tuple(int(v) for v in values)
        self._now = float(t_mono)
        self._track_floor(float(t_mono))
        self._onset_force.observe(
            self._analog[N_SOFTPOT : N_SOFTPOT + N_FSR], float(t_mono)
        )
        total = float(self._fsr_total())
        if self._fsr_floor is None or total < self._fsr_floor:
            self._fsr_floor = total
        if total > self._fsr_ceiling:
            self._fsr_ceiling = total

    def on_lidar(self, ranges, t_mono: float = 0.0) -> None:
        self._lidar = tuple(int(v) for v in ranges)
        self._lidar_suspect.clear()
        seen = 0
        for pos in LIDAR_POSITIONS:
            mm = self._lidar_at(pos)
            if mm is None:
                self._prev_lidar.pop(pos, None)
                continue
            seen += 1
            prev = self._prev_lidar.get(pos)
            if prev is not None:
                dt = max(t_mono - prev[0], 1e-6)
                if abs(mm - prev[1]) / dt > LIDAR_MAX_SPEED_MM_S:
                    self._lidar_suspect.add(pos)
            self._prev_lidar[pos] = (float(t_mono), mm)
        self._lidar_seen = seen
        self._fuse_lidar()

    # ---- output ----------------------------------------------------------
    def _raw_lidar_at(self, position: str) -> Optional[int]:
        """Reading for a position with no background judgement applied."""
        i = self._cal.lidar_index(position)
        if not (0 <= i < len(self._lidar)):
            return None
        v = self._lidar[i]
        if v == LIDAR_INVALID:
            return None
        if v < self._cal.lidar_min_mm:
            return None  # the housing, not a hand
        if not self._cal.lidar_in_bounds(position, v):
            return None  # outside what this sensor can actually resolve
        return v

    def _lidar_at(self, position: str) -> Optional[int]:
        """Reading for a position, or None if it is not a measurement of a hand."""
        return self._raw_lidar_at(position)

    def held_pads(self) -> tuple:
        """Held pads in HARDWARE coordinates -- for anything talking to the
        device itself, which knows nothing about the view transform."""
        return tuple(sorted(self._held))

    def raw(self) -> dict:
        """Raw counts behind the normalized frame.

        The UI shows raw and normalized side by side: you cannot tune a sensor
        by eye without seeing both, and you cannot tell a dead channel from a
        badly-scaled one from the normalized value alone.
        """
        return {
            "analog": self._analog,
            "lidar": self._lidar,
            "fsr_total": self._fsr_total(),
            "force_floor": self._fsr_floor,
            "fsr_floor_now": [self._floor_at(i) for i in range(N_FSR)],
            "force_scale": round(self.force_scale, 4),
            "force_ceiling": self._fsr_ceiling,
            "touch_floor": TOUCH_FLOOR,
            "lidar_seen": self._lidar_seen,
            "pad_forces": dict(self._pad_forces),
            "lidar_suspect": sorted(self._lidar_suspect),
            "lidar_span_mm": (round(self._lidar_near), round(self._lidar_far)),
            "lidar_presence": {
                POSITION_NAME[p]: round(v, 3)
                for p, v in self._lidar_presence.items()
            },
            "lidar_contributing": [
                POSITION_NAME[p] for p in self._lidar_contributing
            ],
            "lidar_no_target": sorted(
                POSITION_NAME[p] for p in LIDAR_POSITIONS
                if self._raw_lidar_at(p) is None
            ),
        }

    def frame(self, t_mono: float) -> fmt.DimensionalFrame:
        self._now = float(t_mono)
        edges = self._take_edges()
        return fmt.DimensionalFrame(
            source=self.source,
            t_mono=float(t_mono),
            channels=(
                self._pad_channel(),
                self._lidar_channel(),
                self._force_channel(),
                *self._softpot_channels(),
            ),
            edges=edges,
        )

    # ---- channels --------------------------------------------------------
    def _pad_channel(self) -> fmt.Channel:
        sx = fmt.sigma_for_steps(GRID_W)
        sy = fmt.sigma_for_steps(GRID_H)
        points = tuple(
            fmt.Point(
                values=(
                    _cell_centre(self._cal.view_xy(x, y)[0], GRID_W),
                    _cell_centre(self._cal.view_xy(x, y)[1], GRID_H),
                ),
                sigma=(sx, sy),
                pid=y * GRID_W + x,   # hardware index, so the profile matches
            )
            for (x, y) in sorted(self._held)
        )
        return fmt.Channel(id="pad", dims=2, label="position", points=points)

    def _fsr_total(self) -> int:
        """Summed force above each sensor's own resting level.

        Calibrated rest wins; with none, the learned live floor in
        ``_force_channel`` covers the same ground more crudely.
        """
        raws = self._analog[N_SOFTPOT : N_SOFTPOT + N_FSR]
        if self._cal.fsr_rest:
            return sum(max(0, r - self._floor_at(i)) for i, r in enumerate(raws))
        return sum(r for r in raws if r > TOUCH_FLOOR)

    def _fsr_above_rest(self) -> tuple[int, ...]:
        raws = self._analog[N_SOFTPOT : N_SOFTPOT + N_FSR]
        return tuple(max(0, r - self._floor_at(i)) for i, r in enumerate(raws))

    def _update_force_scale(self, projection: float) -> float:
        """Full-scale reference: the recent window's hardest press, approached
        slowly so a harder press can still ride above it."""
        t = self._now
        self._force_peaks.append((t, projection))
        cutoff = t - FORCE_SCALE_WINDOW_S
        while self._force_peaks and self._force_peaks[0][0] < cutoff:
            self._force_peaks.pop(0)
        target = max(FORCE_SCALE_FLOOR, max(v for _, v in self._force_peaks))
        self._force_scale += (target - self._force_scale) * FORCE_SCALE_RATE
        return max(FORCE_SCALE_FLOOR, self._force_scale)

    @property
    def force_scale(self) -> float:
        return max(FORCE_SCALE_FLOOR, self._force_scale)

    def _shape(self, raw: float) -> float:
        """Smooth in the linear force domain, THEN curve.

        Curving first would make the smoother's time constant depend on how
        hard you happen to be pressing.
        """
        return apply_curve(self._force_smoother.push(raw), self._cal.force_curve)

    def _silent_force(self) -> fmt.Channel:
        self._force_smoother.reset()
        self._pad_forces = {}
        return fmt.Channel(
            id="force", dims=1, label="force", points=(), associated=("pad",)
        )

    def _force_channel(self) -> fmt.Channel:
        # Gated to pad presses. This channel is monophonic AFTERTOUCH, and
        # aftertouch with no note held is not a thing -- reading the resting
        # sensors as if they were being played is what put force at 0.3 with
        # nobody touching the instrument.
        if not self._held:
            return self._silent_force()

        # ONE force value, whatever is held. The per-pad decomposition below is
        # internal machinery, not output: solving each held pad's own force is
        # what makes the single number independent of WHERE and HOW MANY pads
        # are pressed, which a raw sum can never be (the four FSRs are unevenly
        # placed, so the same press at two corners gives different totals).
        #
        # Combined with a MAX (user's call 2026-09-17, by feel): the value
        # tracks the HARDEST contact, so resting a finger on a pad cannot
        # dilute a hard hit elsewhere. A sum would conflate contact count with
        # intensity; a mean would let a light contact drag a hard one down.
        if self._cal.fsr_pads and self._held:
            held_pads = [y * GRID_W + x for (x, y) in sorted(self._held)]
            # Aftertouch measured from THIS note's onset. Drift cannot reach a
            # reference captured milliseconds ago, and whatever the plate was
            # already carrying is cancelled with it.
            above = self._onset_force.aftertouch(
                self._analog[N_SOFTPOT : N_SOFTPOT + N_FSR]
            ) or self._fsr_above_rest()
            # ONE overall value, projected onto the sum of the held pads'
            # calibrated columns. Per-pad decomposition needs as many good
            # sensors as pads, which a four-pad chord does not have on this
            # hardware; the projection is one unknown against four equations
            # and works with any number of pads down. Per-pad forces are still
            # solved for raw()/debugging, but they are not the output.
            self._pad_forces = solve_pad_forces(above, held_pads, self._cal.fsr_pads)[0]
            combined, sigma = overall_force(above, held_pads, self._cal.fsr_pads)
            if combined is not None:
                # Auto-range the projection: keep WHERE from the profile, learn
                # HOW MUCH from what the sensors are doing now.
                combined = combined / self._update_force_scale(combined)
                return fmt.Channel(
                    id="force", dims=1, label="force",
                    points=(
                        fmt.Point(values=(self._shape(combined * HEADROOM),),
                                  sigma=(sigma,)),
                    ),
                    associated=("pad",),
                )
        else:
            self._pad_forces = {}

        total = self._fsr_total()

        # A calibrated rest IS the zero point, so the floor is zero here. The
        # auto-learned floor exists only for the uncalibrated case; left in, it
        # re-zeroed itself on the first sample of every press, so a genuine
        # press read 0.0.
        floor = (
            0.0 if self._cal.fsr_rest
            else (self._fsr_floor if self._fsr_floor is not None else 0.0)
        )
        # Deadzone FIRST, then normalize what is left, so the curve maps the
        # usable range instead of starting its steep region inside the noise --
        # which is what made light presses feel twitchy.
        usable = max(0.0, total - floor - FSR_DEADZONE)
        span = self._fsr_ceiling - floor - FSR_DEADZONE
        frac = 0.0 if span <= 0 else fmt.clamp01(usable / span)

        # A held pad always reports pressure, even when that pressure is zero:
        # the note is down, the aftertouch value is simply at its bottom.
        return fmt.Channel(
            id="force", dims=1, label="force",
            points=(fmt.Point(values=(self._shape(frac),), sigma=(0.1,)),),
            associated=("pad",),
        )

    def _softpot_channels(self) -> tuple[fmt.Channel, ...]:
        out = []
        for cid in SOFTPOT_IDS:
            i = self._cal.softpot_index(cid)
            raw = self._analog[i] if 0 <= i < len(self._analog) else 0
            points: tuple[fmt.Point, ...] = ()
            if raw > TOUCH_FLOOR:
                lo, hi = self._cal.softpot_range(cid)
                span = max(hi - lo, 1)
                pos = fmt.clamp01((raw - lo) / span)
                if cid in self._cal.softpot_invert:
                    pos = 1.0 - pos
                points = (fmt.Point(values=(pos,), sigma=(0.02,)),)
            out.append(fmt.Channel(id=cid, dims=1, label="ribbon", points=points))
        return tuple(out)

    def _fuse_lidar(self) -> None:
        """Triangulate one point from whichever rangefinders can see, then
        smooth it. Runs on every sample."""
        reading = {p: self._lidar_at(p) for p in LIDAR_POSITIONS}
        n, s_, e, w = (reading[p] for p in LIDAR_POSITIONS)

        # Ramp each sensor's presence toward what it is doing, so one acquiring
        # or losing the hand blends instead of popping.
        for p in LIDAR_POSITIONS:
            target = 1.0 if reading[p] is not None else 0.0
            cur = self._lidar_presence[p]
            self._lidar_presence[p] = cur + (target - cur) * LIDAR_PRESENCE_RATE

        if all(v is None for v in (n, s_, e, w)):
            # Nothing in the volume: forget the point so regaining a hand starts
            # there instead of sliding across. Fading is for a sensor changing
            # its mind, not for a hand that has gone.
            self._lidar_point = None
            self._lidar_contributing = ()
            for p in LIDAR_POSITIONS:
                self._lidar_presence[p] = 0.0
            return

        seen = [v for v in (n, s_, e, w) if v is not None]

        # Height first, slowly, then TAKE IT OUT of the distances and solve
        # the plane with what is left.
        #
        # A rangefinder on the rim measures the hypotenuse to the hand; height
        # is one leg. Solving the plane with the hypotenuse gave a rising hand
        # nowhere to go but sideways -- that is why moving up on the right
        # walked left. Solving x, y and z together instead spends the extra
        # freedom absorbing noise, which was steadier on paper and worse to
        # play. Removing a slowly-estimated leg does neither.
        mm_per_unit = max(self._cal.grid_mm, 1.0)
        raw_units = {
            p: mm / mm_per_unit for p, mm in reading.items() if mm is not None
        }

        # how far the hand is overall -> height, damped hard before use
        dist = sum(float(v) for v in seen) / len(seen)
        span = max(self._lidar_far - self._lidar_near, 1.0)
        z_target = fmt.clamp01((dist - self._lidar_near) / span)
        self._lidar_height += (z_target - self._lidar_height) * LIDAR_HEIGHT_RATE
        z = self._lidar_height

        solved, s_tri = solve_xy(in_plane(raw_units, z * (dist / mm_per_unit)))
        if solved is not None:
            (x, y), sx, sy = solved, s_tri, s_tri
        else:
            cap = float(max(self._cal.lidar_max_mm, 1))
            x, sx = self._axis_from_pair(w, e, cap)
            y, sy = self._axis_from_pair(n, s_, cap)
        sz = min(1.0, max(sx, sy) * 2.2)

        if self._lidar_suspect & {"w", "e"}:
            sx = max(sx, LIDAR_SIGMA_WEAK)
        if self._lidar_suspect & {"n", "s"}:
            sy = max(sy, LIDAR_SIGMA_WEAK)

        # Learn the volume actually used; widen only, and stop once frozen so
        # recorded sessions stay comparable.
        if not self._lidar_span_frozen:
            self._lidar_near = min(self._lidar_near, dist)
            self._lidar_far = max(self._lidar_far, dist)

        # Same frame as the pads. Everything the player sees is oriented once.
        x, y = self._cal.view_point(x, y)

        if self._lidar_suspect & {"w", "e"}:
            sx = max(sx, LIDAR_SIGMA_WEAK)
        if self._lidar_suspect & {"n", "s"}:
            sy = max(sy, LIDAR_SIGMA_WEAK)
        self._lidar_sigma = (sx, sy, sz)

        contributing = tuple(
            p for p, v in zip(LIDAR_POSITIONS, (n, s_, e, w)) if v is not None
        )
        if contributing != self._lidar_contributing:
            self._lidar_contributing = contributing
            self._lidar_handover = LIDAR_HANDOVER_FRAMES

        if self._lidar_point is None:
            self._lidar_point = (x, y, z)      # never ease in from nowhere
            return

        if self._lidar_handover > 0:
            self._lidar_handover -= 1
            # Toward the NEW answer, gently. Not holding the old position --
            # that was a separate bug and it made the reading simply wrong for
            # those frames.
            a = 1.0 / (self._lidar_handover + 2)
        else:
            a = LIDAR_SMOOTH
        px, py, pz = self._lidar_point
        self._lidar_point = (
            px + (x - px) * a, py + (y - py) * a, pz + (z - pz) * a
        )

    def _lidar_channel(self) -> fmt.Channel:
        """Report the fused point. All the work happens in on_lidar, because
        smoothing has to advance once per SAMPLE, not once per frame -- the two
        rates are independent and tying them together made the filter's time
        constant depend on how often anyone happened to look."""
        if self._lidar_point is None:
            return fmt.Channel(id="lidar", dims=3, label="distance", points=())
        return fmt.Channel(
            id="lidar", dims=3, label="distance",
            points=(fmt.Point(values=self._lidar_point, sigma=self._lidar_sigma),),
        )

    @staticmethod
    def _axis_from_pair(a: Optional[int], b: Optional[int], cap: float):
        """Position along an axis from an opposing sensor pair.

        Both seen -> a real ratio. One seen -> a guess from that sensor alone,
        reported at high sigma. Neither -> centre at maximum sigma.
        """
        if a is not None and b is not None:
            total = a + b
            pos = 0.5 if total <= 0 else a / total
            return fmt.clamp01(pos), LIDAR_SIGMA_GOOD
        if a is not None:
            return fmt.clamp01(min(a / cap, 1.0)), LIDAR_SIGMA_WEAK
        if b is not None:
            return fmt.clamp01(1.0 - min(b / cap, 1.0)), LIDAR_SIGMA_WEAK
        return 0.5, fmt.SIGMA_UNKNOWN


__all__ = [
    "NeuralGridEncoder", "Calibration", "GRID_W", "GRID_H",
    "SOFTPOT_IDS", "LIDAR_POSITIONS", "DEFAULT_LIDAR_MAP", "N_SOFTPOT", "N_FSR",
]
