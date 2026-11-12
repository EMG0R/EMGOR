"""ManualStager -- a hand-authored config stager, standing in for the
model layer (Phase 4) before it exists.

Phase 3's whole point is to prove the config boundary mechanism -- stage a
config, watch it swap in exactly on the downbeat, never before -- with no
neural model involved yet. This module cycles through a small
hand-authored sequence of configs (walking note_layout / scale_id /
led_palette through a few contrasting combinations) and stages the next
one in the sequence either on a wall-clock timer (`tick()`, wired into
run_live.py via `--stage-demo`) or on demand (`stage_next()`), so a human
can watch/hear the live layer reconfigure itself on the beat.

This talks to LiveEngine only through the public `stage_config()` method --
exactly the same interface the model layer will use later -- so nothing
here is special-cased inside the engine.
"""

from __future__ import annotations

import logging
import time
from typing import Dict, List, Optional

from .. import config as config_mod

log = logging.getLogger("neuralgrid.live.stager_manual")

SOURCE = "manual-demo"


def _demo_sequence() -> List[Dict]:
    """A handful of contrasting configs so each downbeat swap is obviously
    audible/visible: layout, scale, and LED palette all change together."""
    base = config_mod.default_config()
    combos = [
        # (note_layout, scale_id, led_palette)
        (1, 1, 0),  # rows_in_fourths, major,            rainbow
        (0, 5, 2),  # chromatic,       major_pentatonic,  fire
        (2, 6, 3),  # scale_degrees,   minor_pentatonic,  ice
        (3, 2, 4),  # isomorphic,      natural_minor,     purple
        (2, 7, 5),  # scale_degrees,   harmonic_minor,    grayscale
    ]
    sequence = []
    for layout, scale, palette in combos:
        cfg = dict(base)
        cfg["note_layout"] = layout
        cfg["scale_id"] = scale
        cfg["led_palette"] = palette
        sequence.append(cfg)
    return sequence


class ManualStager:
    def __init__(
        self,
        engine,
        interval_seconds: float = 4.0,
        sequence: Optional[List[Dict]] = None,
    ):
        self.engine = engine
        self.interval_seconds = interval_seconds
        self.sequence = sequence if sequence is not None else _demo_sequence()
        self._idx = 0
        self._next_at = time.monotonic() + interval_seconds

    def stage_next(self) -> bool:
        """Stage the next config in the sequence right now (cycles). Returns
        whether the engine accepted it (it always should -- the demo
        sequence is built from validated configs)."""
        cfg = dict(self.sequence[self._idx % len(self.sequence)])
        self._idx += 1
        ok = self.engine.stage_config(cfg, source=SOURCE)
        log.info(
            "manual stager: staged demo config %d/%d (layout=%s scale=%s palette=%s) accepted=%s",
            self._idx, len(self.sequence),
            cfg["note_layout"], cfg["scale_id"], cfg["led_palette"], ok,
        )
        return ok

    def tick(self, now: Optional[float] = None) -> None:
        """Call once per engine loop iteration. Stages the next config
        every `interval_seconds` of wall-clock time. The actual swap still
        only happens on the next downbeat -- this just decides *when a new
        config becomes available to be swapped in*."""
        now = now if now is not None else time.monotonic()
        if now >= self._next_at:
            self.stage_next()
            self._next_at = now + self.interval_seconds
