# neuralGrid encoder: raw ESP32 sensor traffic -> dimensional frames

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
SOFTPOT_IDS = ('softpot_n', 'softpot_s', 'softpot_e', 'softpot_w')
LIDAR_INVALID = 65535
LIDAR_POSITIONS = ('n', 's', 'e', 'w')
DEFAULT_LIDAR_MAP = {'n': 1, 'e': 2, 's': 3, 'w': 0}
POSITION_NAME = {'n': 'top', 'e': 'right', 's': 'bottom', 'w': 'left'}
TOUCH_FLOOR = 120
FSR_EXPONENT = 3.0
FSR_DEADZONE = 90
FSR_NOMINAL_FULL = 1200.0
FORCE_SCALE_WINDOW_S = 20.0
FORCE_SCALE_FLOOR = 0.02
FORCE_SCALE_RATE = 0.02
FSR_FLOOR_WINDOW_S = 12.0
LIDAR_SIGMA_GOOD = 0.08
LIDAR_SIGMA_WEAK = 0.45
LIDAR_MAX_SPEED_MM_S = 2000.0
LIDAR_SMOOTH = 0.65
LIDAR_HEIGHT_RATE = 0.08
LIDAR_HANDOVER_FRAMES = 10
LIDAR_PRESENCE_RATE = 0.12
LIDAR_SPAN_NEAR = 40.0
LIDAR_SPAN_FAR = 220.0
GRID_MM = 120.0
LIDAR_MIN_MM = 50

# centre of cell ``i`` of ``n`` on a 0-1 axis -- not its corner
def _cell_centre(i: int, n: int) -> float:
    return (i + 0.5) / n

# runtime calibration
@dataclass(frozen=True)
class Calibration:
    softpot: dict = field(default_factory=dict)
    lidar_max_mm: int = 1200
    softpot_map: dict = field(default_factory=dict)
    lidar_map: dict = field(default_factory=dict)
    lidar_bounds: dict = field(default_factory=dict)
    fsr_rest: tuple = ()
    fsr_pads: dict = field(default_factory=dict)
    fsr_pads_raw: dict = field(default_factory=dict)
    fsr_curves: dict = field(default_factory=dict)
    force_curve: float = DEFAULT_CURVE
    grid_rotation: int = 180
    lidar_span: tuple = ()
    lidar_min_mm: int = LIDAR_MIN_MM
    grid_mm: float = GRID_MM
    grid_flip_x: bool = False
    grid_flip_y: bool = False
    softpot_invert: tuple = ()

    def softpot_range(self, channel_id: str) -> tuple[int, int]:
        lo, hi = self.softpot.get(channel_id, (TOUCH_FLOOR, 4095))
        return (int(lo), int(hi))

    # hardware input feeding this position; wiring order if undiscovered
    def softpot_index(self, channel_id: str) -> int:
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

    # hardware pad -> the position the player sees it at
    def view_xy(self, x: int, y: int) -> tuple[int, int]:
        x, y = (int(x), int(y))
        mx, my = (GRID_W - 1, GRID_H - 1)
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

    # continuous position -> what the player sees
    def view_point(self, x: float, y: float) -> tuple[float, float]:
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
        return {'v': 1, 'softpot': {k: list(v) for k, v in self.softpot.items()}, 'lidar_max_mm': self.lidar_max_mm, 'softpot_map': dict(self.softpot_map), 'lidar_map': dict(self.lidar_map), 'lidar_bounds': {k: list(v) for k, v in self.lidar_bounds.items()}, 'fsr_rest': list(self.fsr_rest), 'fsr_pads': {str(k): list(v) for k, v in self.fsr_pads.items()}, 'fsr_pads_raw': {str(k): list(v) for k, v in self.fsr_pads_raw.items()}, 'fsr_curves': {str(k): [list(r) for r in v] for k, v in self.fsr_curves.items()}, 'force_curve': self.force_curve, 'grid_rotation': self.grid_rotation, 'lidar_span': list(self.lidar_span), 'lidar_min_mm': self.lidar_min_mm, 'grid_mm': self.grid_mm, 'grid_flip_x': bool(self.grid_flip_x), 'grid_flip_y': bool(self.grid_flip_y), 'softpot_invert': list(self.softpot_invert)}

    # atomic write, keeping the previous good copy alongside
    def save(self, path) -> None:
        path = str(path)
        backup = path.replace('.json', '.backup.json')
        if backup != path and os.path.exists(path):
            try:
                with open(path, 'rb') as src, open(backup, 'wb') as dst:
                    dst.write(src.read())
            except OSError:
                pass
        tmp = path + '.tmp'
        with open(tmp, 'w') as fh:
            json.dump(self.to_dict(), fh, indent=2)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, path)

    # true once this instrument has been set up at all
    @property
    def is_calibrated(self) -> bool:
        return bool(self.fsr_pads or self.softpot or self.fsr_rest)

    @classmethod
    def load(cls, path) -> 'Calibration':
        try:
            with open(str(path)) as fh:
                d = json.load(fh)
        except (OSError, ValueError):
            return cls()
        return cls(softpot={k: (int(v[0]), int(v[1])) for k, v in d.get('softpot', {}).items()}, lidar_max_mm=int(d.get('lidar_max_mm', 1200)), softpot_map={k: int(v) for k, v in d.get('softpot_map', {}).items()}, lidar_map={k: int(v) for k, v in d.get('lidar_map', {}).items()}, lidar_bounds={k: (int(v[0]), int(v[1])) for k, v in d.get('lidar_bounds', {}).items()}, fsr_rest=tuple((int(v) for v in d.get('fsr_rest', ()))), fsr_pads={int(k): tuple((int(x) for x in v)) for k, v in d.get('fsr_pads', {}).items()}, fsr_pads_raw={int(k): tuple((int(x) for x in v)) for k, v in d.get('fsr_pads_raw', {}).items()}, fsr_curves={int(k): [tuple((int(x) for x in row)) for row in v] for k, v in d.get('fsr_curves', {}).items()}, force_curve=float(d.get('force_curve', DEFAULT_CURVE)), grid_rotation=int(d.get('grid_rotation', 0)), lidar_span=tuple((float(v) for v in d.get('lidar_span', ()))), lidar_min_mm=int(d.get('lidar_min_mm', LIDAR_MIN_MM)), grid_mm=float(d.get('grid_mm', GRID_MM)), grid_flip_x=bool(d.get('grid_flip_x', False)), grid_flip_y=bool(d.get('grid_flip_y', False)), softpot_invert=tuple(d.get('softpot_invert', ())))

# stateful: fed by the link's callbacks, sampled by :meth:`frame`
class NeuralGridEncoder:
    source = 'neuralgrid'

    def __init__(self, calibration: Optional[Calibration]=None, calibration_path=None):
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
        self._prev_lidar: dict[str, tuple[float, int]] = {}
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
        self._fsr_seen: list = [[] for _ in range(N_FSR)]
        self._onset_force = OnsetForce()
        self._force_peaks: list = []
        self._force_scale = FORCE_SCALE_FLOOR
        self._now = 0.0
        self._fsr_ceiling = FSR_NOMINAL_FULL

    @property
    def calibration(self) -> Calibration:
        return self._cal

    # re-read the calibration file
    def reload_calibration(self) -> None:
        if self._calibration_path is not None:
            self._cal = Calibration.load(self._calibration_path)
            span = self._cal.lidar_span
            if len(span) == 2:
                self._lidar_near, self._lidar_far = (float(span[0]), float(span[1]))
                self._lidar_span_frozen = True

    # what the encoder has observed -- freeze this into calibration
    @property
    def learned_lidar_span(self) -> tuple[float, float]:
        return (self._lidar_near, self._lidar_far)

    def on_key(self, x: int, y: int, pressed: bool, t_mono: float) -> None:
        key = (int(x), int(y))
        vx, vy = self._cal.view_xy(*key)
        point = (_cell_centre(vx, GRID_W), _cell_centre(vy, GRID_H))
        if pressed:
            self._held[key] = float(t_mono)
            kind = 'onset'
            if len(self._held) == 1:
                self._onset_force.begin(float(t_mono))
        else:
            self._held.pop(key, None)
            kind = 'release'
            if not self._held:
                self._onset_force.end()
        self._pending_edges.append(fmt.Edge(channel='pad', kind=kind, t_mono=float(t_mono), point=point))

    # edges produced so far, without emitting a frame
    def drain_onsets(self) -> tuple:
        return self._take_edges()

    def _take_edges(self) -> tuple:
        out = tuple(self._pending_edges)
        self._pending_edges.clear()
        return out

    def drain_onsets(self) -> tuple:
        return self._take_edges()

    # roll each sensor's observed minimum forward
    def _track_floor(self, t: float) -> None:
        raws = self._analog[N_SOFTPOT:N_SOFTPOT + N_FSR]
        for i, raw in enumerate(raws):
            hist = self._fsr_seen[i]
            hist.append((t, int(raw)))
            cutoff = t - FSR_FLOOR_WINDOW_S
            while hist and hist[0][0] < cutoff:
                hist.pop(0)

    # effective zero for one sensor: the calibrated rest, or lower if the
    # sensor has actually been lower
    def _floor_at(self, i: int) -> int:
        rest = self._cal.fsr_rest_at(i)
        hist = self._fsr_seen[i] if i < len(self._fsr_seen) else []
        return min(rest, min((v for _, v in hist), default=rest))

    def on_analog(self, values, t_mono: float=0.0) -> None:
        self._analog = tuple((int(v) for v in values))
        self._now = float(t_mono)
        self._track_floor(float(t_mono))
        self._onset_force.observe(self._analog[N_SOFTPOT:N_SOFTPOT + N_FSR], float(t_mono))
        total = float(self._fsr_total())
        if self._fsr_floor is None or total < self._fsr_floor:
            self._fsr_floor = total
        if total > self._fsr_ceiling:
            self._fsr_ceiling = total

    def on_lidar(self, ranges, t_mono: float=0.0) -> None:
        self._lidar = tuple((int(v) for v in ranges))
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
                dt = max(t_mono - prev[0], 1e-06)
                if abs(mm - prev[1]) / dt > LIDAR_MAX_SPEED_MM_S:
                    self._lidar_suspect.add(pos)
            self._prev_lidar[pos] = (float(t_mono), mm)
        self._lidar_seen = seen
        self._fuse_lidar()

    # reading for a position with no background judgement applied
    def _raw_lidar_at(self, position: str) -> Optional[int]:
        i = self._cal.lidar_index(position)
        if not 0 <= i < len(self._lidar):
            return None
        v = self._lidar[i]
        if v == LIDAR_INVALID:
            return None
        if v < self._cal.lidar_min_mm:
            return None
        if not self._cal.lidar_in_bounds(position, v):
            return None
        return v

    # reading for a position, or None if it is not a measurement of a hand
    def _lidar_at(self, position: str) -> Optional[int]:
        return self._raw_lidar_at(position)

    # held pads in HARDWARE coordinates -- for anything talking to the
    # device itself, which knows nothing about the view transform
    def held_pads(self) -> tuple:
        return tuple(sorted(self._held))

    # raw counts behind the normalized frame
    def raw(self) -> dict:
        return {'analog': self._analog, 'lidar': self._lidar, 'fsr_total': self._fsr_total(), 'force_floor': self._fsr_floor, 'fsr_floor_now': [self._floor_at(i) for i in range(N_FSR)], 'force_scale': round(self.force_scale, 4), 'force_ceiling': self._fsr_ceiling, 'touch_floor': TOUCH_FLOOR, 'lidar_seen': self._lidar_seen, 'pad_forces': dict(self._pad_forces), 'lidar_suspect': sorted(self._lidar_suspect), 'lidar_span_mm': (round(self._lidar_near), round(self._lidar_far)), 'lidar_presence': {POSITION_NAME[p]: round(v, 3) for p, v in self._lidar_presence.items()}, 'lidar_contributing': [POSITION_NAME[p] for p in self._lidar_contributing], 'lidar_no_target': sorted((POSITION_NAME[p] for p in LIDAR_POSITIONS if self._raw_lidar_at(p) is None))}

    def frame(self, t_mono: float) -> fmt.DimensionalFrame:
        self._now = float(t_mono)
        edges = self._take_edges()
        return fmt.DimensionalFrame(source=self.source, t_mono=float(t_mono), channels=(self._pad_channel(), self._lidar_channel(), self._force_channel(), *self._softpot_channels()), edges=edges)

    def _pad_channel(self) -> fmt.Channel:
        sx = fmt.sigma_for_steps(GRID_W)
        sy = fmt.sigma_for_steps(GRID_H)
        points = tuple((fmt.Point(values=(_cell_centre(self._cal.view_xy(x, y)[0], GRID_W), _cell_centre(self._cal.view_xy(x, y)[1], GRID_H)), sigma=(sx, sy), pid=y * GRID_W + x) for x, y in sorted(self._held)))
        return fmt.Channel(id='pad', dims=2, label='position', points=points)

    # summed force above each sensor's own resting level
    def _fsr_total(self) -> int:
        raws = self._analog[N_SOFTPOT:N_SOFTPOT + N_FSR]
        if self._cal.fsr_rest:
            return sum((max(0, r - self._floor_at(i)) for i, r in enumerate(raws)))
        return sum((r for r in raws if r > TOUCH_FLOOR))

    def _fsr_above_rest(self) -> tuple[int, ...]:
        raws = self._analog[N_SOFTPOT:N_SOFTPOT + N_FSR]
        return tuple((max(0, r - self._floor_at(i)) for i, r in enumerate(raws)))

    # full-scale reference: the recent window's hardest press, approached
    # slowly so a harder press can still ride above it
    def _update_force_scale(self, projection: float) -> float:
        t = self._now
        self._force_peaks.append((t, projection))
        cutoff = t - FORCE_SCALE_WINDOW_S
        while self._force_peaks and self._force_peaks[0][0] < cutoff:
            self._force_peaks.pop(0)
        target = max(FORCE_SCALE_FLOOR, max((v for _, v in self._force_peaks)))
        self._force_scale += (target - self._force_scale) * FORCE_SCALE_RATE
        return max(FORCE_SCALE_FLOOR, self._force_scale)

    @property
    def force_scale(self) -> float:
        return max(FORCE_SCALE_FLOOR, self._force_scale)

    # smooth in the linear force domain, THEN curve
    def _shape(self, raw: float) -> float:
        return apply_curve(self._force_smoother.push(raw), self._cal.force_curve)

    def _silent_force(self) -> fmt.Channel:
        self._force_smoother.reset()
        self._pad_forces = {}
        return fmt.Channel(id='force', dims=1, label='force', points=(), associated=('pad',))

    def _force_channel(self) -> fmt.Channel:
        if not self._held:
            return self._silent_force()
        if self._cal.fsr_pads and self._held:
            held_pads = [y * GRID_W + x for x, y in sorted(self._held)]
            above = self._onset_force.aftertouch(self._analog[N_SOFTPOT:N_SOFTPOT + N_FSR]) or self._fsr_above_rest()
            self._pad_forces = solve_pad_forces(above, held_pads, self._cal.fsr_pads)[0]
            combined, sigma = overall_force(above, held_pads, self._cal.fsr_pads)
            if combined is not None:
                combined = combined / self._update_force_scale(combined)
                return fmt.Channel(id='force', dims=1, label='force', points=(fmt.Point(values=(self._shape(combined * HEADROOM),), sigma=(sigma,)),), associated=('pad',))
        else:
            self._pad_forces = {}
        total = self._fsr_total()
        floor = 0.0 if self._cal.fsr_rest else self._fsr_floor if self._fsr_floor is not None else 0.0
        usable = max(0.0, total - floor - FSR_DEADZONE)
        span = self._fsr_ceiling - floor - FSR_DEADZONE
        frac = 0.0 if span <= 0 else fmt.clamp01(usable / span)
        return fmt.Channel(id='force', dims=1, label='force', points=(fmt.Point(values=(self._shape(frac),), sigma=(0.1,)),), associated=('pad',))

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
            out.append(fmt.Channel(id=cid, dims=1, label='ribbon', points=points))
        return tuple(out)

    # triangulate one point from whichever rangefinders can see, then
    # smooth it
    def _fuse_lidar(self) -> None:
        reading = {p: self._lidar_at(p) for p in LIDAR_POSITIONS}
        n, s_, e, w = (reading[p] for p in LIDAR_POSITIONS)
        for p in LIDAR_POSITIONS:
            target = 1.0 if reading[p] is not None else 0.0
            cur = self._lidar_presence[p]
            self._lidar_presence[p] = cur + (target - cur) * LIDAR_PRESENCE_RATE
        if all((v is None for v in (n, s_, e, w))):
            self._lidar_point = None
            self._lidar_contributing = ()
            for p in LIDAR_POSITIONS:
                self._lidar_presence[p] = 0.0
            return
        seen = [v for v in (n, s_, e, w) if v is not None]
        mm_per_unit = max(self._cal.grid_mm, 1.0)
        raw_units = {p: mm / mm_per_unit for p, mm in reading.items() if mm is not None}
        dist = sum((float(v) for v in seen)) / len(seen)
        span = max(self._lidar_far - self._lidar_near, 1.0)
        z_target = fmt.clamp01((dist - self._lidar_near) / span)
        self._lidar_height += (z_target - self._lidar_height) * LIDAR_HEIGHT_RATE
        z = self._lidar_height
        solved, s_tri = solve_xy(in_plane(raw_units, z * (dist / mm_per_unit)))
        if solved is not None:
            (x, y), sx, sy = (solved, s_tri, s_tri)
        else:
            cap = float(max(self._cal.lidar_max_mm, 1))
            x, sx = self._axis_from_pair(w, e, cap)
            y, sy = self._axis_from_pair(n, s_, cap)
        sz = min(1.0, max(sx, sy) * 2.2)
        if self._lidar_suspect & {'w', 'e'}:
            sx = max(sx, LIDAR_SIGMA_WEAK)
        if self._lidar_suspect & {'n', 's'}:
            sy = max(sy, LIDAR_SIGMA_WEAK)
        if not self._lidar_span_frozen:
            self._lidar_near = min(self._lidar_near, dist)
            self._lidar_far = max(self._lidar_far, dist)
        x, y = self._cal.view_point(x, y)
        if self._lidar_suspect & {'w', 'e'}:
            sx = max(sx, LIDAR_SIGMA_WEAK)
        if self._lidar_suspect & {'n', 's'}:
            sy = max(sy, LIDAR_SIGMA_WEAK)
        self._lidar_sigma = (sx, sy, sz)
        contributing = tuple((p for p, v in zip(LIDAR_POSITIONS, (n, s_, e, w)) if v is not None))
        if contributing != self._lidar_contributing:
            self._lidar_contributing = contributing
            self._lidar_handover = LIDAR_HANDOVER_FRAMES
        if self._lidar_point is None:
            self._lidar_point = (x, y, z)
            return
        if self._lidar_handover > 0:
            self._lidar_handover -= 1
            a = 1.0 / (self._lidar_handover + 2)
        else:
            a = LIDAR_SMOOTH
        px, py, pz = self._lidar_point
        self._lidar_point = (px + (x - px) * a, py + (y - py) * a, pz + (z - pz) * a)

    # report the fused point
    def _lidar_channel(self) -> fmt.Channel:
        if self._lidar_point is None:
            return fmt.Channel(id='lidar', dims=3, label='distance', points=())
        return fmt.Channel(id='lidar', dims=3, label='distance', points=(fmt.Point(values=self._lidar_point, sigma=self._lidar_sigma),))

    # position along an axis from an opposing sensor pair
    @staticmethod
    def _axis_from_pair(a: Optional[int], b: Optional[int], cap: float):
        if a is not None and b is not None:
            total = a + b
            pos = 0.5 if total <= 0 else a / total
            return (fmt.clamp01(pos), LIDAR_SIGMA_GOOD)
        if a is not None:
            return (fmt.clamp01(min(a / cap, 1.0)), LIDAR_SIGMA_WEAK)
        if b is not None:
            return (fmt.clamp01(1.0 - min(b / cap, 1.0)), LIDAR_SIGMA_WEAK)
        return (0.5, fmt.SIGMA_UNKNOWN)
__all__ = ['NeuralGridEncoder', 'Calibration', 'GRID_W', 'GRID_H', 'SOFTPOT_IDS', 'LIDAR_POSITIONS', 'DEFAULT_LIDAR_MAP', 'N_SOFTPOT', 'N_FSR']
