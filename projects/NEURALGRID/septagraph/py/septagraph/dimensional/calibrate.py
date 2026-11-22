"""Guided calibration for the neuralGrid encoder.

Nine short steps. In order:

1. **Resting offsets** -- hands off everything while it watches the FSRs. The
   housing pre-loads some of them, so they sit well above zero untouched
   (measured on hardware: 3319 / 1419 / 0 / 0). That is a property of the
   assembly, it changes whenever the housing is opened, and it is precisely
   what a calibration file is for.
2-5. **Each softpot, by position** -- top, right, bottom, left.
6-9. **Each lidar, by position** -- same four.

Steps 2-9 do two things at once. The firmware's N/S/E/W is nothing but wiring
order (softpots GPIO32/33/34/35, lidar XSHUT 4/13/14/27) and *which physical
strip or sensor that is was never established anywhere in this project*. So a
step names a position a human can see, waits to find out which hardware input
answers, claims it, and records that input's travel in the same sweep. An input
already claimed by an earlier position cannot be claimed again, so a mistake
shows up as an unclaimed step rather than a silently doubled binding.

Writes ``calibration.json``, loaded by the encoder at runtime -- recalibrating
never needs a rebuild, which matters because this gets re-run every time the
housing comes open.

Everything here is a pure state machine fed by ``observe_*``, and the LED cues
are a pure function of state, so the whole thing tests with no hardware.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .lidar_arcs import (
    DEPART_MM, POSITION_TO_LIDAR, POSITIONS, ArcRecorder, analyze_arcs,
)
from .encoders.neuralgrid import (
    LIDAR_INVALID,
    LIDAR_POSITIONS,
    N_FSR,
    N_SOFTPOT,
    SOFTPOT_IDS,
    TOUCH_FLOOR,
    Calibration,
)

#: A softpot step only counts once it has seen this much raw span -- proof the
#: ribbon was swept rather than poked once.
MIN_TRAVEL = 800

#: How many steady samples the resting-offset step watches before it believes
#: what it is seeing.
REST_SAMPLES = 8

#: POSITIONS / POSITION_TO_LIDAR live in lidar_arcs (imported above).
POSITION_TO_SOFTPOT = {"top": "softpot_n", "right": "softpot_e",
                       "bottom": "softpot_s", "left": "softpot_w"}

#: Which grid edge to light for each position.
EDGE_CELLS = {
    "top": [i for i in range(8)],
    "bottom": [56 + i for i in range(8)],
    "left": [r * 8 for r in range(8)],
    "right": [r * 8 + 7 for r in range(8)],
}

CUE_PENDING = (70, 45, 0)
CUE_SATISFIED = (0, 95, 35)
CUE_REST = (50, 0, 70)


def _median(xs: list[int]) -> int:
    ordered = sorted(xs)
    return ordered[len(ordered) // 2]


@dataclass
class Step:
    key: str
    kind: str                      # "fsr_rest" | "softpot" | "lidar"
    instruction: str
    position: Optional[str] = None
    hw_index: Optional[int] = None  # discovered, never assumed
    lo: Optional[int] = None
    hi: Optional[int] = None
    samples: list = field(default_factory=list)
    #: live "has anything answered yet" for an arc step, filled by the session
    arc_response: float = 0.0

    def reset(self) -> None:
        self.hw_index = None
        self.lo = None
        self.hi = None
        self.samples = []
        self.arc_response = 0.0

    def observe_range(self, raw: int) -> None:
        self.lo = raw if self.lo is None else min(self.lo, raw)
        self.hi = raw if self.hi is None else max(self.hi, raw)

    @property
    def rest(self) -> tuple[int, ...]:
        if self.kind != "fsr_rest" or not self.samples:
            return ()
        return tuple(
            _median([s[i] for s in self.samples]) for i in range(len(self.samples[0]))
        )

    @property
    def satisfied(self) -> bool:
        if self.kind == "fsr_rest":
            return len(self.samples) >= REST_SAMPLES
        if self.kind == "lidar_arc":
            # No hw_index here by design: which sensor answered is decided
            # afterwards, from the recording, against each sensor's baseline.
            return self.arc_response >= DEPART_MM
        if self.hw_index is None:
            return False
        return self.lo is not None and self.hi is not None and (self.hi - self.lo) >= MIN_TRAVEL


def _default_steps() -> list[Step]:
    steps = [
        Step(
            key="fsr_rest",
            kind="fsr_rest",
            instruction="Hands off the instrument. Reading the resting load the "
                        "housing puts on the force sensors.",
        )
    ]
    for pos in POSITIONS:
        steps.append(
            Step(
                key=f"softpot_{pos}",
                kind="softpot",
                position=pos,
                instruction=f"Sweep the ribbon on the {pos.upper()} edge, end to "
                            f"end, twice. (Which input that is gets discovered.)",
            )
        )
    for pos in POSITIONS:
        steps.append(
            Step(
                key=f"lidar_{pos}",
                kind="lidar_arc",
                position=pos,
                instruction=f"Two slow arcs with your hand over the {pos.upper()} "
                            f"edge. The sensors stare at the housing until a hand "
                            f"comes in -- the departure from that is the signal.",
            )
        )
    return steps


class CalibrationSession:
    def __init__(self, path=None):
        self.steps: list[Step] = _default_steps()
        self._i = 0
        self._path = path
        self.arcs = ArcRecorder()

    # ---- position --------------------------------------------------------
    @property
    def current(self) -> Optional[Step]:
        return self.steps[self._i] if self._i < len(self.steps) else None

    @property
    def complete(self) -> bool:
        return self._i >= len(self.steps)

    @property
    def progress(self) -> tuple[int, int]:
        return (min(self._i, len(self.steps)), len(self.steps))

    def _sync_arc(self) -> None:
        step = self.current
        if step is not None and step.kind == "lidar_arc":
            step.arc_response = self.arcs.response(step.position)

    def advance(self) -> None:
        self._sync_arc()
        self.arcs.end()
        self._i = min(self._i + 1, len(self.steps))

    def goto(self, key: str) -> None:
        """Jump to a step, clearing it. Re-doing must start from scratch: a bad
        first sweep would otherwise poison the range, and a wrong claim would
        hold an input hostage."""
        for i, st in enumerate(self.steps):
            if st.key == key:
                st.reset()
                if st.kind == "lidar_arc":
                    self.arcs.samples.pop(st.position, None)
                    self.arcs.end()
                self._i = i
                return
        raise KeyError(key)

    def goto_kind(self, kind: str) -> None:
        for i, st in enumerate(self.steps):
            if st.kind == kind:
                self._i = i
                return
        raise KeyError(kind)

    # ---- claiming --------------------------------------------------------
    def _claimed(self, kind: str) -> set[int]:
        return {
            st.hw_index
            for st in self.steps
            if st.kind == kind and st.hw_index is not None and st is not self.current
        }

    # ---- observations ----------------------------------------------------
    def observe_analog(self, values) -> None:
        step = self.current
        if step is None:
            return

        if step.kind == "fsr_rest":
            fsr = tuple(int(v) for v in values[N_SOFTPOT : N_SOFTPOT + N_FSR])
            if len(fsr) == N_FSR:
                step.samples.append(fsr)
            return

        if step.kind != "softpot":
            return

        if step.hw_index is None:
            taken = self._claimed("softpot")
            best, best_raw = None, TOUCH_FLOOR
            for i in range(min(N_SOFTPOT, len(values))):
                raw = int(values[i])
                if i not in taken and raw > best_raw:
                    best, best_raw = i, raw
            if best is None:
                return
            step.hw_index = best

        raw = int(values[step.hw_index]) if step.hw_index < len(values) else 0
        if raw > TOUCH_FLOOR:
            step.observe_range(raw)

    def observe_lidar(self, ranges) -> None:
        """Record the whole four-sensor frame for this edge's arcs.

        Deliberately does NOT try to pick a winner live. The old rule -- claim
        the nearest valid return -- claims the HOUSING on this hardware, since
        three of four sensors sit pinned on a wall 15-42mm away. Ownership is
        decided afterwards by which sensor departed from its own baseline, and
        every arc doubles as a control for the other three.
        """
        step = self.current
        if step is None or step.kind != "lidar_arc":
            return
        if self.arcs.position != step.position:
            self.arcs.begin(step.position)
        self.arcs.observe(ranges)
        step.arc_response = self.arcs.response(step.position)

    # ---- result ----------------------------------------------------------
    def result(self) -> Calibration:
        softpot, softpot_map, lidar_map, lidar_bounds = {}, {}, {}, {}
        lidar_far = 0
        rest: tuple[int, ...] = ()

        for st in self.steps:
            if st.kind == "fsr_rest":
                rest = st.rest
            elif st.kind == "softpot" and st.hw_index is not None:
                cid = POSITION_TO_SOFTPOT[st.position]
                softpot_map[cid] = st.hw_index
                if st.lo is not None and st.hi is not None:
                    softpot[cid] = (st.lo, st.hi)

        arc = analyze_arcs(self.arcs)
        for position, idx in arc.owner.items():
            key = POSITION_TO_LIDAR[position]
            lidar_map[key] = idx
            if position in arc.bounds:
                lidar_bounds[key] = arc.bounds[position]
                lidar_far = max(lidar_far, arc.bounds[position][1])

        kwargs = {
            "softpot": softpot,
            "softpot_map": softpot_map,
            "lidar_map": lidar_map,
            "lidar_bounds": lidar_bounds,
            "fsr_rest": rest,
        }
        if lidar_far:
            kwargs["lidar_max_mm"] = lidar_far
        return Calibration(**kwargs)

    def arc_result(self):
        """Full arc analysis -- baselines, usable depth, mounting warnings."""
        return analyze_arcs(self.arcs)

    def save(self, path=None) -> str:
        target = path or self._path
        if target is None:
            raise ValueError("no calibration path configured")
        self.result().save(target)
        return str(target)

    # ---- guidance --------------------------------------------------------
    def led_frame(self) -> list[tuple[int, int, int]]:
        """Light the edge being asked about, so the grid itself tells you which
        strip to touch -- the whole point, since the labels were never real."""
        frame = [(0, 0, 0)] * 64
        step = self.current
        if step is None:
            return frame
        if step.kind == "fsr_rest":
            for i in range(64):
                frame[i] = CUE_REST if (i // 8 + i % 8) % 2 == 0 else (0, 0, 0)
            return frame
        colour = CUE_SATISFIED if step.satisfied else CUE_PENDING
        for cell in EDGE_CELLS.get(step.position, []):
            frame[cell] = colour
        return frame

    def state(self) -> dict:
        step = self.current
        done, total = self.progress
        return {
            "complete": self.complete,
            "done": done,
            "total": total,
            "key": step.key if step else None,
            "kind": step.kind if step else None,
            "position": step.position if step else None,
            "channel": step.key if step else None,
            "instruction": step.instruction if step else "Calibration complete.",
            "hw_index": step.hw_index if step else None,
            "lo": step.lo if step else None,
            "hi": step.hi if step else None,
            "rest": list(step.rest) if step and step.kind == "fsr_rest" else None,
            "rest_samples": len(step.samples) if step and step.kind == "fsr_rest" else None,
            "rest_needed": REST_SAMPLES,
            "arc_response": round(step.arc_response, 1) if step else None,
            "satisfied": step.satisfied if step else True,
            "steps": [
                {
                    "key": s.key, "kind": s.kind, "position": s.position,
                    "satisfied": s.satisfied, "hw_index": s.hw_index,
                    "lo": s.lo, "hi": s.hi,
                }
                for s in self.steps
            ],
        }


__all__ = [
    "CalibrationSession", "Step", "MIN_TRAVEL", "REST_SAMPLES",
    "POSITIONS", "POSITION_TO_SOFTPOT", "POSITION_TO_LIDAR",
]
