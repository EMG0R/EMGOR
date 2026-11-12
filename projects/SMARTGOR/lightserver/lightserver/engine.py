"""The render engine: fixed-timestep sim -> linear HDR -> tone-map ->
gamma 2.2 -> uint8 -> output (UDP / terminal preview / headless).

Scene swaps crossfade in linear light. Thread-safe enough for the API:
scene swaps go through a lock; the render thread owns everything else.
"""
from __future__ import annotations

import threading
import time

import numpy as np

from . import DEFAULT_FPS, DEFAULT_IP, DEFAULT_PORT, NUM_PIXELS
from .color import linear_frame_to_uint8
from .protocol import UdpSender
from .scene import SceneInstance, validate_scene

DEFAULT_CROSSFADE = 0.75  # seconds

BLACK_SCENE = {"name": "black", "layers": [{"type": "solid", "color": "#000000"}]}


class Engine:
    def __init__(self, num_pixels: int = NUM_PIXELS, fps: int = DEFAULT_FPS,
                 mode: str = "headless", ip: str = DEFAULT_IP, port: int = DEFAULT_PORT,
                 seed: int | None = None, host: str | None = None,
                 name: str = "fcob-strip"):
        self.name = name
        self.host = host
        self.n = num_pixels
        self.fps = fps
        self.dt = 1.0 / fps
        self.mode = mode
        self.seed = seed
        self._lock = threading.Lock()
        self._scene = SceneInstance(BLACK_SCENE, num_pixels, seed=seed)
        self._old_scene: SceneInstance | None = None
        self._xfade_t = 0.0
        self._xfade_dur = DEFAULT_CROSSFADE
        self.last_hdr = np.zeros((num_pixels, 3), dtype=np.float32)
        self.last_frame = np.zeros((num_pixels, 3), dtype=np.uint8)
        self.frame_count = 0
        self.actual_fps = 0.0
        self.streaming = False
        self._user_stopped = False  # explicit /stop; survives run() (re)start
        self._stop_evt = threading.Event()
        self._thread: threading.Thread | None = None

        self.sender: UdpSender | None = None
        self.preview = None
        if mode == "udp":
            self.sender = UdpSender(ip, port, host=host)
        elif mode == "preview":
            from .preview import TerminalPreview
            self.preview = TerminalPreview(num_pixels)

    # -- scene control -----------------------------------------------------
    def set_scene(self, scene: dict, crossfade: float = DEFAULT_CROSSFADE) -> None:
        """Validate + swap in a new scene with a crossfade. Raises on invalid."""
        validate_scene(scene)
        new = SceneInstance(scene, self.n, seed=self.seed)
        with self._lock:
            if crossfade > 0 and self.frame_count > 0:
                self._old_scene = self._scene
                self._xfade_t = 0.0
                self._xfade_dur = crossfade
            else:
                self._old_scene = None
            self._scene = new
            self._user_stopped = False
            self.streaming = True

    def set_pixels(self, mapping: dict, base: str = "black",
                   crossfade: float = 0.15) -> dict:
        """Direct per-pixel set. base='current' merges onto an existing paint
        scene's pixels; base='black' starts fresh. Returns the scene applied."""
        pixels = {str(int(k)): str(v) for k, v in mapping.items()}
        with self._lock:
            cur = self._scene.scene
        if base == "current" and cur.get("layers"):
            top = cur["layers"][-1]
            if top.get("type") == "paint":
                merged = dict(top.get("pixels") or {})
                merged.update(pixels)
                pixels = merged
        scene = {
            "name": "paint",
            "layers": [{"type": "paint", "blend": "alpha",
                        "background": "#000000", "pixels": pixels}],
        }
        self.set_scene(scene, crossfade=crossfade)
        return scene

    @property
    def current_scene(self) -> dict:
        with self._lock:
            return self._scene.scene

    @property
    def current_scene_name(self) -> str:
        with self._lock:
            return self._scene.name

    # -- rendering ---------------------------------------------------------
    def render_frame(self) -> np.ndarray:
        """Advance one fixed timestep and return the uint8 frame (n,3)."""
        with self._lock:
            scene, old = self._scene, self._old_scene
        hdr = scene.render(self.dt)
        tone_map = scene.tone_map
        if old is not None:
            old_hdr = old.render(self.dt)
            self._xfade_t += self.dt
            mix = min(1.0, self._xfade_t / max(1e-6, self._xfade_dur))
            hdr = old_hdr * (1.0 - mix) + hdr * mix
            if mix >= 1.0:
                with self._lock:
                    self._old_scene = None
        self.last_hdr = hdr
        self.last_frame = linear_frame_to_uint8(hdr, tone_map=tone_map)
        self.frame_count += 1
        return self.last_frame

    def _emit(self, frame: np.ndarray) -> None:
        if self.sender is not None and self.streaming:
            self.sender.send_frame(frame.tobytes())
        if self.preview is not None:
            self.preview.show(frame)

    def step(self) -> np.ndarray:
        """One tick: render + emit. Used by tests and the run loop."""
        frame = self.render_frame()
        self._emit(frame)
        return frame

    # -- run loop ----------------------------------------------------------
    def run(self, duration: float | None = None) -> None:
        """Blocking fixed-timestep loop with drift-free scheduling."""
        self.streaming = self.mode == "udp" and not self._user_stopped
        self._stop_evt.clear()
        next_t = time.perf_counter()
        t0 = next_t
        fps_win_start, fps_win_frames = next_t, 0
        while not self._stop_evt.is_set():
            self.step()
            fps_win_frames += 1
            now = time.perf_counter()
            if now - fps_win_start >= 1.0:
                self.actual_fps = fps_win_frames / (now - fps_win_start)
                fps_win_start, fps_win_frames = now, 0
            if duration is not None and now - t0 >= duration:
                break
            next_t += self.dt
            sleep = next_t - time.perf_counter()
            if sleep > 0:
                time.sleep(sleep)
            else:  # fell behind; resync rather than spiral
                next_t = time.perf_counter()
        if self.preview is not None:
            self.preview.close()

    def start_background(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self.run, daemon=True,
                                        name=f"lightserver-render-{self.name}")
        self._thread.start()

    def stop_streaming(self) -> None:
        """Stop sending frames; firmware falls back to onboard effect after 5s."""
        self._user_stopped = True
        self.streaming = False

    def resume_streaming(self) -> None:
        self._user_stopped = False
        if self.mode == "udp":
            self.streaming = True

    @property
    def stopped(self) -> bool:
        """True after an explicit stop_streaming until scene/resume clears it."""
        return self._user_stopped

    def shutdown(self) -> None:
        self._stop_evt.set()
        if self._thread:
            self._thread.join(timeout=2.0)
        if self.sender:
            self.sender.close()

    # -- stats -------------------------------------------------------------
    def stats(self) -> dict:
        s = {
            "scene": self.current_scene_name,
            "mode": self.mode,
            "streaming": bool(self.streaming and self.mode == "udp"),
            "num_pixels": self.n,
            "fps_target": self.fps,
            "fps_actual": round(self.actual_fps, 2),
            "frame_count": self.frame_count,
        }
        if self.sender:
            s["udp"] = {
                "target": f"{self.sender.ip}:{self.sender.port}",
                "host": self.sender.host,
                "resolved_ip": self.sender.resolved_ip,
                "frames_sent": self.sender.frames_sent,
                "packets_sent": self.sender.packets_sent,
                "bytes_sent": self.sender.bytes_sent,
                "last_send_ts": self.sender.last_send_ts,
                "last_error": self.sender.last_error,
            }
        return s
