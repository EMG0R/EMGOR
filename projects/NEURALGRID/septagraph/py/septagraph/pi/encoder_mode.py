"""The dimensional encoder as a mode of the one grid app.

It shipped as its own systemd service, which was a mistake made visible
on 2026-09-17: both it and this app held /dev/ttyUSB0 open, so every byte
the ESP32 sent went to whichever process read first. One process owns the
link. The encoder keeps its own web UI and its own DimensionalApp -- only
the serial port stops being duplicated.
"""
from __future__ import annotations

from typing import Optional

from . import leds
from .modes import Mode

class EncoderMode(Mode):
    name = "encoder"

    def __init__(self, dim_app, http_port: Optional[int] = None):
        self.dim = dim_app
        self.http_port = http_port

    def on_key(self, x: int, y: int, edge: int) -> None:
        self.dim.on_key(int(x), int(y), bool(edge))

    def on_analog(self, raw) -> None:
        self.dim.on_analog(tuple(raw))

    def on_lidar(self, mm4) -> None:
        self.dim.on_lidar(tuple(mm4))

    def led_frame(self):
        try:
            frame = list(self.dim.led_frame())
        except Exception:
            # The encoder is a lot of arithmetic over noisy sensors; if it
            # throws, the instrument should still be lit and switchable.
            frame = leds.blank()
        if len(frame) != leds.N_LEDS:
            frame = (frame + leds.blank())[:leds.N_LEDS]
        # What the encoder draws is what you see. An earlier version lit a
        # corner whenever the frame came back entirely black, so that pad
        # blinked in and out as the encoder drew -- a light flickering at
        # the edge of vision reads as a fault, which is worse than the
        # dark grid it was there to explain.
        return frame

    def panic(self) -> None:
        return

    def state(self) -> dict:
        d = {"http_port": self.http_port}
        try:
            d["encoder_mode"] = self.dim.mode
        except Exception:
            pass
        return d


__all__ = ["EncoderMode"]
