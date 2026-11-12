"""FSR (force-sensitive resistor) input processing, shared by all three
neuralGrid Pi app modes (Direct Lab, Septagraph, MIDI) and by the web UI's
FSR meters.

Hardware (HANDOFF.md §11, fw6): ANALOG channels 4-7 = FSR0-3 (softpots stay
0-3, unchanged). The response curve below is copied EXACTLY from
``pi/tools/monitor.py`` -- search that file for ``FSR_GAMMA`` -- where the
user already tuned it live on hardware:

    n = clamp((raw - floor) / (4095 - floor), 0, 1)
    pressure = n ** gamma

``gamma``/``floor`` are kept as plain function arguments (not module
constants) so the app can expose them as live-editable ``app_state`` config
(this task's brief), same idea as monitor.py's own ``GET /fsr?gamma=&floor=``
live-tune endpoint -- just plumbed through app_state/dispatch here instead.

Onset/release + velocity capture: this task's brief allows an optional
15ms peak-capture window after the onset crossing ("velocity = peak
pressure within the next 15ms ... or if that adds >15ms latency, send
immediately at the first sample and document"). The analog stream itself
arrives at ~42-50 fps (~20-24ms/sample, HANDOFF §3: I2C dropped to 400kHz),
which already exceeds the 15ms budget on its own -- waiting for even one
more sample can cost more than the whole allowance, and doing so precisely
would need a dedicated timer thread per FSR for a very small gain. So
:class:`FsrChannel` fires the strike **immediately** on the sample that
crosses ``onset threshold``, with velocity = that same sample's pressure.
This is the simpler, documented choice, not the two-phase "wait for the
peak" design.
"""
from __future__ import annotations

from dataclasses import dataclass

N_FSR = 4
FSR_NAMES = ("FSR0", "FSR1", "FSR2", "FSR3")

# Defaults match pi/tools/monitor.py's FSR_GAMMA / FSR_FLOOR startup values
# exactly (same hardware, same tuning session).
DEFAULT_GAMMA = 3.3
DEFAULT_FLOOR = 40

ONSET_THRESHOLD = 0.08   # pressure rising through this = strike
RELEASE_THRESHOLD = 0.04  # pressure falling below this = release


@dataclass
class FsrConfig:
    """Live-tunable FSR response-curve config (app_state-persisted)."""

    gamma: float = DEFAULT_GAMMA
    floor: float = DEFAULT_FLOOR
    onset: float = ONSET_THRESHOLD
    release: float = RELEASE_THRESHOLD
    # Strikes OFF by default: with the FSRs mounted inside the housing, pad
    # presses flex the case and the FSRs cross the onset threshold on their
    # own (observed 2026-09-09: FSR1 -> ganaPati fired on every pad release).
    # Meters still work; re-enable from the UI once the mount is solid.
    enabled: bool = False

    def to_json(self) -> dict:
        return {"gamma": self.gamma, "floor": self.floor, "onset": self.onset,
                "release": self.release, "enabled": bool(self.enabled)}

    def update(self, **kwargs) -> None:
        for k, v in kwargs.items():
            if v is None or not hasattr(self, k):
                continue
            if k == "enabled":
                self.enabled = bool(v) if not isinstance(v, str) else v.lower() in ("1", "true", "on", "yes")
            else:
                setattr(self, k, float(v))


def fsr_pressure(raw: int, gamma: float = DEFAULT_GAMMA, floor: float = DEFAULT_FLOOR) -> float:
    """Apply the tuned gamma curve to one raw FSR ADC sample -> pressure in [0, 1]."""
    span = 4095.0 - floor
    n = 0.0 if span <= 0 else (raw - floor) / span
    n = 0.0 if n < 0.0 else 1.0 if n > 1.0 else n
    return n ** gamma


class FsrChannel:
    """Onset/release edge detector over one FSR's gamma-curved pressure."""

    def __init__(self):
        self.pressed = False
        self.pressure = 0.0

    def update(self, raw: int, cfg: FsrConfig):
        """Feed one raw ADC sample against ``cfg``.

        Returns ``("strike", velocity)`` on a rising edge through
        ``cfg.onset``, ``("release", None)`` on a falling edge through
        ``cfg.release``, or ``(None, pressure)`` otherwise. ``pressure`` is
        also always available afterwards via ``self.pressure`` (used by
        the web UI's live meters).
        """
        p = fsr_pressure(raw, cfg.gamma, cfg.floor)
        self.pressure = p
        if not self.pressed and p >= cfg.onset:
            self.pressed = True
            return "strike", p
        if self.pressed and p < cfg.release:
            self.pressed = False
            return "release", None
        return None, p


def make_channels(n: int = N_FSR) -> list:
    return [FsrChannel() for _ in range(n)]
