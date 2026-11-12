"""The dimensional app: calibration and encoder in one service.

Boots into CALIBRATE with guidance on the grid, hands off to RUN the instant
calibration is saved, and can be sent back to CALIBRATE at any time from the
UI. One process owns the serial link, so there is never a second process
fighting for the port.

The app is deliberately transport-free: ``on_key`` / ``on_analog`` / ``on_lidar``
are fed by whatever is driving it (the real link, or a test), and ``led_frame``
/ ``state`` are pulled out. That keeps the whole mode machine testable with no
hardware attached.
"""
from __future__ import annotations

import os
import threading
import time
from typing import Optional

from . import format as fmt
from .calibrate import CalibrationSession
from .fsr_calibrate import FsrPadSession
from .leds import clamp_frame
from .response_surface import fit_response_surface
from dataclasses import replace

from .encoders.neuralgrid import GRID_H, GRID_W, N_FSR, NeuralGridEncoder

MODE_CALIBRATE = "calibrate"   # one-time sensor map: which strip is which
MODE_FSR = "fsr"               # per-pad force profile: EVERY housing open
MODE_RUN = "run"

#: How long a pushed LED frame holds the grid without being refreshed. Long
#: enough for a lazy driver, short enough that a crashed one hands back fast.
LED_PUSH_TIMEOUT_S = 2.0

#: Lit pad colour while running -- brightness follows the live force channel so
#: the grid shows what the encoder is actually reporting, not a fixed colour.
RUN_BASE = (20, 60, 90)
RUN_PEAK = (120, 200, 255)


class DimensionalApp:
    def __init__(self, calibration_path, start_mode: Optional[str] = None):
        self._path = calibration_path
        self._lock = threading.RLock()
        self._encoder = NeuralGridEncoder(calibration_path=calibration_path)
        self._calibration = CalibrationSession(path=calibration_path)
        # Calibration is a file on the Pi, so once it exists the instrument
        # comes up PLAYING rather than asking to be set up again. A corrupt or
        # missing file loads as an empty Calibration, which lands here as
        # "not calibrated" and boots into setup -- never a hard failure.
        if start_mode is None:
            start_mode = (
                MODE_RUN if self._encoder.calibration.is_calibrated else MODE_CALIBRATE
            )
        self._mode = start_mode
        self._t0 = time.monotonic()
        # Raw snapshot kept by the app, not the encoder: the UI must show raw
        # counts in BOTH modes, but during calibration the encoder must stay
        # untouched so it does not report channels from calibration sweeps.
        self._raw_analog: tuple[int, ...] = ()
        self._raw_lidar: tuple[int, ...] = ()
        self._fsr: Optional[FsrPadSession] = None
        self._led_source = None
        self._led_source_error = ""
        self._pushed_leds = None
        self._pushed_at = 0.0
        self._rest_wanted = 0
        self._rest_samples: list = []
        self._autosave_path = (
            os.path.join(os.path.dirname(os.path.abspath(str(calibration_path))),
                         "fsr_progress.json")
            if calibration_path else None
        )

    # ---- mode ------------------------------------------------------------
    @property
    def mode(self) -> str:
        return self._mode

    @property
    def calibration(self) -> CalibrationSession:
        return self._calibration

    @property
    def encoder(self) -> NeuralGridEncoder:
        return self._encoder

    @property
    def fsr(self) -> Optional[FsrPadSession]:
        return self._fsr

    # ---- re-zero -----------------------------------------------------
    def rotate_grid(self) -> int:
        """Turn the grid a quarter and persist it. Which way it is mounted has
        been guessed wrong twice; it is one number and belongs under the
        player's thumb, not in an assumption."""
        with self._lock:
            cal = self._encoder.calibration
            turned = (int(cal.grid_rotation) + 90) % 360
            replace(cal, grid_rotation=turned).save(self._path)
            self._encoder.reload_calibration()
            return turned

    def flip_grid(self, axis: str) -> bool:
        """Mirror the grid. A flipped grid cannot be fixed by rotating it."""
        with self._lock:
            cal = self._encoder.calibration
            field = "grid_flip_x" if axis == "x" else "grid_flip_y"
            value = not getattr(cal, field)
            replace(cal, **{field: value}).save(self._path)
            self._encoder.reload_calibration()
            return value

    def invert_ribbon(self, position: str) -> Optional[bool]:
        """Reverse one ribbon's travel. Per channel: a strip can be mounted
        either way round, and they are not all the same way on this unit."""
        from .calibrate import POSITION_TO_SOFTPOT
        cid = POSITION_TO_SOFTPOT.get(position)
        if cid is None:
            return None
        with self._lock:
            cal = self._encoder.calibration
            cur = set(cal.softpot_invert)
            on = cid not in cur
            cur.symmetric_difference_update({cid})
            replace(cal, softpot_invert=tuple(sorted(cur))).save(self._path)
            self._encoder.reload_calibration()
            return on

    def touched_ribbon(self) -> Optional[int]:
        """Which ribbon INPUT is being touched right now, if any."""
        from .encoders.neuralgrid import N_SOFTPOT, TOUCH_FLOOR
        best, best_raw = None, TOUCH_FLOOR
        for i in range(min(N_SOFTPOT, len(self._raw_analog))):
            if self._raw_analog[i] > best_raw:
                best, best_raw = i, self._raw_analog[i]
        return best

    def assign_ribbon(self, position: str) -> Optional[int]:
        """Bind the ribbon being touched to an edge you can see.

        Which edge each input is has never been established, and permuting them
        from a verbal description is guesswork. Touch one, name it, done.
        """
        from .calibrate import POSITION_TO_SOFTPOT
        with self._lock:
            idx = self.touched_ribbon()
            cid = POSITION_TO_SOFTPOT.get(position)
            if idx is None or cid is None:
                return None
            cal = self._encoder.calibration
            # one input cannot hold two edges, and naming an edge replaces
            # whatever held it before
            m = {k: v for k, v in cal.softpot_map.items() if v != idx and k != cid}
            m[cid] = idx
            replace(cal, softpot_map=m).save(self._path)
            self._encoder.reload_calibration()
            return idx

    def start_rest_capture(self, samples: int = 16) -> None:
        """Re-measure the housing's resting load WITHOUT touching the pad
        profile. The rest drifts (observed swinging by over a thousand counts
        in a session); the profile costs twenty minutes to rebuild."""
        with self._lock:
            self._rest_wanted = int(samples)
            self._rest_samples = []

    @property
    def capturing_rest(self) -> bool:
        return self._rest_wanted > 0

    def _feed_rest_capture(self, values) -> None:
        fsr = tuple(int(v) for v in list(values)[-N_FSR:])
        if len(fsr) < N_FSR:
            return
        self._rest_samples.append(fsr)
        if len(self._rest_samples) < self._rest_wanted:
            return
        rest = tuple(
            sorted(s[i] for s in self._rest_samples)[len(self._rest_samples) // 2]
            for i in range(N_FSR)
        )
        cal = replace(self._encoder.calibration, fsr_rest=rest)
        cal.save(self._path)
        self._encoder.reload_calibration()
        self._led_source = None
        self._led_source_error = ""
        self._pushed_leds = None
        self._pushed_at = 0.0
        self._rest_wanted = 0
        self._rest_samples = []

    def _autosave_fsr(self) -> None:
        """Persist pad progress as it happens.

        Learned the hard way: the session lived only in memory until the user
        pressed Save, and this service gets restarted constantly during
        development. Sixty-four sweeps is twenty minutes of work and must not
        depend on nobody bouncing the unit.
        """
        if self._fsr is None or self._autosave_path is None:
            return
        try:
            self._fsr.save(self._autosave_path)
        except OSError:
            pass  # never let a disk hiccup interrupt a calibration in progress

    def resume_fsr_calibration(self) -> bool:
        """Pick an interrupted pad session back up. True if anything resumed."""
        with self._lock:
            if self._autosave_path is None:
                return False
            profile = FsrPadSession.load_profile(self._autosave_path)
            if not profile:
                return False
            rest = FsrPadSession.load_rest(self._autosave_path)
            s = FsrPadSession(rest=rest)
            s._profile = dict(profile)
            s.curves = FsrPadSession.load_curves(self._autosave_path)
            done = set(profile)
            while s.current_pad is not None and s.current_pad in done:
                s._i += 1
            self._fsr = s
            self._mode = MODE_FSR
            return True

    def start_fsr_calibration(self) -> None:
        """Per-pad force profile. Seeded with whatever resting offsets are
        known -- from this session's rest step if it ran, else the file."""
        with self._lock:
            rest = self._calibration.steps[0].rest or self._encoder.calibration.fsr_rest
            self._fsr = FsrPadSession(rest=rest)
            self._mode = MODE_FSR

    def finish_fsr_calibration(self) -> None:
        """Merge the pad profile into calibration.json WITHOUT disturbing the
        sensor map -- that is the run-once program's territory."""
        with self._lock:
            if self._fsr is not None:
                cal = self._encoder.calibration
                measured = self._fsr.result()
                # Fit a smooth surface through the measurements: a hand is not
                # a consistent actuator, and without this your press-to-press
                # variation becomes permanent per-pad calibration.
                merged = replace(
                    cal,
                    fsr_rest=tuple(self._fsr.rest) or cal.fsr_rest,
                    fsr_pads=fit_response_surface(measured),
                    fsr_pads_raw=measured,
                    fsr_curves={k: [list(r) for r in v]
                                for k, v in self._fsr.curves.items()},
                )
                merged.save(self._path)
                self._encoder.reload_calibration()
            self._mode = MODE_RUN

    def start_calibration(self) -> None:
        """Always a fresh session: a stale half-sweep would poison the range."""
        with self._lock:
            self._calibration = CalibrationSession(path=self._path)
            self._mode = MODE_CALIBRATE

    def finish_calibration(self) -> None:
        """Write ONLY what the sensor-map program measured.

        Two programs share one file. The sensor map is run once; the pad
        profile is twenty minutes of work re-done every time the housing is
        opened. Building a fresh Calibration from this session's result wiped
        the pad profile on every finish -- merge, never overwrite.
        """
        with self._lock:
            measured = self._calibration.result()
            current = self._encoder.calibration
            # Only fields this session actually measured. An empty result means
            # "I did not do that step", not "erase what is there" -- finishing
            # a half-done pass must not blank a map from a previous one.
            updates = {
                name: value
                for name, value in (
                    ("softpot", measured.softpot),
                    ("softpot_map", measured.softpot_map),
                    ("lidar_map", measured.lidar_map),
                    ("lidar_bounds", measured.lidar_bounds),
                    ("fsr_rest", measured.fsr_rest),
                )
                if value
            }
            if measured.lidar_bounds:
                updates["lidar_max_mm"] = measured.lidar_max_mm
            merged = replace(current, **updates)
            merged.save(self._path)
            self._encoder.reload_calibration()
            self._mode = MODE_RUN

    # ---- clock -----------------------------------------------------------
    def now(self) -> float:
        return time.monotonic() - self._t0

    # ---- inputs ----------------------------------------------------------
    def on_key(self, x: int, y: int, pressed: bool, t_mono: Optional[float] = None) -> None:
        with self._lock:
            if self._mode == MODE_FSR and self._fsr is not None:
                before = self._fsr.done
                self._fsr.on_key(x, y, pressed)
                if self._fsr.done != before or (not pressed and self._fsr.unfelt):
                    self._autosave_fsr()
            elif self._mode == MODE_RUN:
                self._encoder.on_key(x, y, pressed, self.now() if t_mono is None else t_mono)

    def on_analog(self, values, t_mono: Optional[float] = None) -> None:
        with self._lock:
            self._raw_analog = tuple(int(v) for v in values)
            if self._rest_wanted:
                self._feed_rest_capture(values)
            if self._mode == MODE_CALIBRATE:
                self._calibration.observe_analog(values)
            elif self._mode == MODE_FSR and self._fsr is not None:
                self._fsr.on_analog(values)
            else:
                self._encoder.on_analog(values, self.now() if t_mono is None else t_mono)

    def on_lidar(self, ranges, t_mono: Optional[float] = None) -> None:
        with self._lock:
            self._raw_lidar = tuple(int(v) for v in ranges)
            if self._mode == MODE_CALIBRATE:
                self._calibration.observe_lidar(ranges)
            else:
                self._encoder.on_lidar(ranges, self.now() if t_mono is None else t_mono)

    # ---- outputs ---------------------------------------------------------
    def frame(self, t_mono: Optional[float] = None) -> fmt.DimensionalFrame:
        with self._lock:
            return self._encoder.frame(self.now() if t_mono is None else t_mono)

    def set_led_source(self, fn) -> None:
        """Hand the grid to something else -- a model, a renderer, anything
        that can produce 64 (r,g,b). Pass None to take it back."""
        with self._lock:
            self._led_source = fn
            self._led_source_error = ""

    def push_leds(self, frame, now: Optional[float] = None) -> bool:
        """Hand the grid to an outside program. Any language, any process --
        the contract is 64 (r,g,b), the same thing the ESP32 passes through."""
        with self._lock:
            clamped = clamp_frame(frame)
            if clamped is None:
                self._led_source_error = "frame rejected: needs 64 (r,g,b)"
                return False
            self._pushed_leds = clamped
            self._pushed_at = time.monotonic() if now is None else now
            self._led_source_error = ""
            return True

    def release_leds(self) -> None:
        with self._lock:
            self._pushed_leds = None

    def led_frame(self, now: Optional[float] = None) -> list[tuple[int, int, int]]:
        with self._lock:
            # Guidance cues are never overridable: during calibration the grid
            # is how you know what to do next.
            if self._mode == MODE_CALIBRATE:
                return self._calibration.led_frame()
            if self._mode == MODE_FSR and self._fsr is not None:
                return self._fsr.led_frame()
            if self._led_source is not None:
                frame = self._safe_source_frame()
                if frame is not None:
                    return frame
            # A pushed frame holds the grid only while the driver keeps
            # pushing. If it dies the instrument takes back over rather than
            # freezing on whatever was last sent -- a stuck grid looks exactly
            # like dead hardware.
            if self._pushed_leds is not None:
                t = time.monotonic() if now is None else now
                if t - self._pushed_at <= LED_PUSH_TIMEOUT_S:
                    return self._pushed_leds
                self._pushed_leds = None
            return self._run_led_frame()

    def _safe_source_frame(self):
        """Never let a driver take the instrument down. A throw, a wrong
        length or a junk value falls back to the built-in view and is
        surfaced -- a silently dark grid is indistinguishable from dead
        hardware, which has already cost debugging time here."""
        try:
            frame = clamp_frame(self._led_source())
        except Exception as exc:
            self._led_source_error = f"{type(exc).__name__}: {exc}"
            return None
        if frame is None:
            self._led_source_error = "frame rejected: needs 64 (r,g,b)"
            return None
        self._led_source_error = ""
        return frame

    def _run_led_frame(self) -> list[tuple[int, int, int]]:
        """LEDs in HARDWARE coordinates.

        The pad channel is the VIEW -- rotated and mirrored to match how the
        instrument is played -- but this frame goes straight to the NeoTrellis,
        which knows nothing about that. Building it from the view lit the
        physically opposite pad on a half-turned grid.
        """
        frame = [(0, 0, 0)] * (GRID_W * GRID_H)
        force_ch = self._encoder._force_channel()
        f = force_ch.points[0].values[0] if force_ch.active else 0.0
        colour = tuple(
            int(base + (peak - base) * f) for base, peak in zip(RUN_BASE, RUN_PEAK)
        )
        for (x, y) in self._encoder.held_pads():
            frame[y * GRID_W + x] = colour
        return frame

    def _frame_views(self) -> dict:
        f = self._encoder.frame(self.now())
        return {"frame": fmt.frame_to_dict(f), "line": fmt.frame_to_line(f)}

    def _raw_state(self) -> dict:
        """Everything the encoder exposes, plus the app's own snapshot.

        Passes the encoder's raw dict through WHOLESALE. Cherry-picking keys
        here meant every new diagnostic silently failed to reach the UI -- it
        happened three separate times -- and the UI is where these are read.
        """
        out = dict(self._encoder.raw())
        out["analog"] = list(self._raw_analog)
        out["lidar"] = list(self._raw_lidar)
        return out

    def state(self) -> dict:
        with self._lock:
            return {
                "mode": self._mode,
                "calibration": self._calibration.state(),
                "fsr": self._fsr.state() if self._fsr else None,
                # ONE frame: frame() drains pending edges, so building it twice
                # would let the line eat onsets the visualization never sees.
                **self._frame_views(),
                "raw": self._raw_state(),
                "capturing_rest": self.capturing_rest,
                "led_source": self._led_source is not None,
                "led_pushed": self._pushed_leds is not None,
                "led_source_error": self._led_source_error,
                "grid_rotation": self._encoder.calibration.grid_rotation,
                "grid_flip_x": self._encoder.calibration.grid_flip_x,
                "grid_flip_y": self._encoder.calibration.grid_flip_y,
                "softpot_map": dict(self._encoder.calibration.softpot_map),
                "softpot_invert": list(self._encoder.calibration.softpot_invert),
                "touched_ribbon": self.touched_ribbon(),
                "labels": list(fmt.LABELS),
                # Physical layout, so the UI labels sensors by where they ARE
                # rather than by the firmware's wiring order. Getting this
                # wrong is what made "lidar n" point at the left sensor.
                "lidar_layout": {
                    pos: self._encoder.calibration.lidar_index(key)
                    for pos, key in (("top", "n"), ("right", "e"),
                                     ("bottom", "s"), ("left", "w"))
                },
                "format_version": fmt.FORMAT_VERSION,
            }


__all__ = ["DimensionalApp", "MODE_CALIBRATE", "MODE_FSR", "MODE_RUN"]
