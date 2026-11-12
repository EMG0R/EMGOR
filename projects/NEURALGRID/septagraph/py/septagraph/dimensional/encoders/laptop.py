"""Laptop encoder: trackpad + keyboard as 2x 2D spaces.

The portability proof. A format with one implementation is not a format, it is
a data structure with delusions -- so the second encoder is deliberately
nothing like an 8x8 pad with ribbons and rangefinders. It needs no new
hardware, and it covers both kinds of 2D at once:

    trackpad   continuous surface   very small sigma
    keyboard   key grid             half-a-cell sigma

Downstream, a model reads both with the same accessors it uses on the
neuralGrid, and never learns which is which.

Events are injected (``on_trackpad`` / ``on_key``) rather than grabbed from the
OS: the encoder stays pure and testable, and the transport -- browser, evdev,
whatever -- is somebody else's problem.
"""
from __future__ import annotations

from typing import Optional

from .. import format as fmt

#: Physical-ish QWERTY layout. Row 0 is the top. Only the letter/number block
#: is mapped; anything else is ignored rather than guessed at.
KEY_GRID = (
    "1234567890",
    "qwertyuiop",
    "asdfghjkl;",
    "zxcvbnm,./",
)

#: A trackpad resolves position far more finely than a key grid can. Not zero:
#: a fingertip is wide and the surface is noisy.
TRACKPAD_SIGMA = 0.004


def _key_cell(key: str) -> Optional[tuple[int, int]]:
    k = (key or "").lower()
    if len(k) != 1:
        return None
    for row, chars in enumerate(KEY_GRID):
        col = chars.find(k)
        if col >= 0:
            return col, row
    return None


class LaptopEncoder:
    """Stateful, same shape of API as :class:`NeuralGridEncoder`."""

    source = "laptop"

    def __init__(self):
        self._pad: Optional[tuple[float, float]] = None
        self._keys: dict[str, tuple[int, int]] = {}
        self._pending_edges: list[fmt.Edge] = []

    # ---- inputs ----------------------------------------------------------
    def on_trackpad(self, x: float, y: float, down: bool, t_mono: float) -> None:
        point = (fmt.clamp01(x), fmt.clamp01(y))
        was_down = self._pad is not None
        self._pad = point if down else None
        if down and not was_down:
            self._pending_edges.append(
                fmt.Edge(channel="trackpad", kind="onset", t_mono=float(t_mono), point=point)
            )
        elif was_down and not down:
            self._pending_edges.append(
                fmt.Edge(channel="trackpad", kind="release", t_mono=float(t_mono), point=point)
            )

    def on_key(self, key: str, pressed: bool, t_mono: float) -> None:
        cell = _key_cell(key)
        if cell is None:
            return
        k = key.lower()
        point = self._cell_to_point(cell)
        if pressed:
            self._keys[k] = cell
            kind = "onset"
        else:
            self._keys.pop(k, None)
            kind = "release"
        self._pending_edges.append(
            fmt.Edge(channel="keyboard", kind=kind, t_mono=float(t_mono), point=point)
        )

    # ---- output ----------------------------------------------------------
    def frame(self, t_mono: float) -> fmt.DimensionalFrame:
        edges = tuple(self._pending_edges)
        self._pending_edges.clear()
        return fmt.DimensionalFrame(
            source=self.source,
            t_mono=float(t_mono),
            channels=(self._trackpad_channel(), self._keyboard_channel()),
            edges=edges,
        )

    # ---- channels --------------------------------------------------------
    def _trackpad_channel(self) -> fmt.Channel:
        points: tuple[fmt.Point, ...] = ()
        if self._pad is not None:
            points = (
                fmt.Point(values=self._pad, sigma=(TRACKPAD_SIGMA, TRACKPAD_SIGMA)),
            )
        return fmt.Channel(id="trackpad", dims=2, label="position", points=points)

    def _keyboard_channel(self) -> fmt.Channel:
        sx = fmt.sigma_for_steps(len(KEY_GRID[0]))
        sy = fmt.sigma_for_steps(len(KEY_GRID))
        points = tuple(
            fmt.Point(values=self._cell_to_point(cell), sigma=(sx, sy), pid=i)
            for i, (_, cell) in enumerate(sorted(self._keys.items()))
        )
        return fmt.Channel(id="keyboard", dims=2, label="position", points=points)

    @staticmethod
    def _cell_to_point(cell: tuple[int, int]) -> tuple[float, float]:
        col, row = cell
        return (
            (col + 0.5) / len(KEY_GRID[0]),
            (row + 0.5) / len(KEY_GRID),
        )


__all__ = ["LaptopEncoder", "KEY_GRID", "TRACKPAD_SIGMA"]
