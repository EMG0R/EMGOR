"""Terminal preview: downsample the strip to the terminal width and draw
one line of truecolor ANSI half-blocks, refreshed in place."""
from __future__ import annotations

import os
import shutil
import sys

import numpy as np


class TerminalPreview:
    def __init__(self, num_pixels: int, width: int | None = None):
        self.n = num_pixels
        cols = width or max(20, shutil.get_terminal_size((100, 20)).columns - 2)
        self.width = min(cols, num_pixels)
        # each output cell averages a slice of the strip
        self.edges = np.linspace(0, num_pixels, self.width + 1).astype(int)
        self._tty = sys.stdout.isatty() or os.environ.get("LIGHTSERVER_FORCE_TTY")
        if self._tty:
            sys.stdout.write("\x1b[?25l")  # hide cursor

    def show(self, frame: np.ndarray) -> None:
        cells = []
        for i in range(self.width):
            a, b = self.edges[i], self.edges[i + 1]
            seg = frame[a:b] if b > a else frame[a:a + 1]
            r, g, bl = seg.astype(np.uint16).mean(axis=0).astype(int)
            cells.append(f"\x1b[38;2;{r};{g};{bl}m█")
        line = "".join(cells) + "\x1b[0m"
        try:
            if self._tty:
                sys.stdout.write("\r" + line)
            else:
                sys.stdout.write(line + "\n")
            sys.stdout.flush()
        except BrokenPipeError:  # downstream pipe closed (e.g. | head) — go quiet
            self._tty = False
            sys.stdout = open(os.devnull, "w")

    def close(self) -> None:
        try:
            if self._tty:
                sys.stdout.write("\x1b[0m\x1b[?25h\n")
                sys.stdout.flush()
        except BrokenPipeError:
            pass
