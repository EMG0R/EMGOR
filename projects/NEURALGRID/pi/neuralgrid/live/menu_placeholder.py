"""Placeholder menus for softpot channels 2 (MIDI-fx) and 3 (reserved).

Touching those softpots must never crash the engine even though there's no
real screen behind them yet. Each placeholder renders a static, dim,
per-menu color wash (about the most an 8x8 RGB grid can say without text)
and no-ops every input hook.
"""

from __future__ import annotations

from typing import List

from .menu import GRID_H, GRID_W, Menu, RGB


class MenuPlaceholder(Menu):
    name = "placeholder"

    def __init__(self, engine, color: RGB = (20, 20, 20), label: str = "placeholder"):
        super().__init__(engine)
        self.color = color
        self.label = label

    def render(self, fb: List[RGB]) -> None:
        for i in range(GRID_W * GRID_H):
            fb[i] = self.color


class MenuMidiFxPlaceholder(MenuPlaceholder):
    name = "midi-fx (coming soon)"

    def __init__(self, engine):
        super().__init__(engine, color=(40, 0, 40), label=self.name)


class MenuReservedPlaceholder(MenuPlaceholder):
    name = "reserved"

    def __init__(self, engine):
        super().__init__(engine, color=(15, 15, 15), label=self.name)
