"""Touch detection state machine for the SoftPot analog channels.

The ESP32 sends completely raw, ungated ADC samples (0..4095) at ~50 Hz
(docs/PROTOCOL.md, ANALOG message). It does zero filtering — telling
"finger on the strip" apart from "wiper floating / picking up noise" is
explicitly the Pi's job (PROTOCOL.md "Touch detection is NOT here").

REALITY (measured on hardware, UNTOUCHED, no external pulldowns):
    ch0(N): 0..2676 mean 1364 std 972
    ch1(S): 0..1853 mean  867 std 653
    ch2(E): 0..1577 mean  811 std 536
    ch3(W): 0..1346 mean  596 std 471
i.e. an untouched channel is NOT pinned near 0 — it reads violent wideband
noise across most of the ADC range, jumping hundreds of counts per sample.
So absolute value tells you nothing. The discriminating signal is STABILITY:

  * Untouched: a short window's spread (max-min) is huge (~1000+ counts).
  * Touched (held or even sliding): consecutive samples are slew-limited, so
    the window spread is small (a fingertip can't teleport between 20 ms
    samples; even a fast slide moves < ~200 counts/sample).

This detector is therefore STABILITY-based and SELF-CALIBRATING: it tracks a
per-channel running estimate of the untouched noise spread (the "floor") and
flags a touch when the current window spread drops well below that floor (and
under an absolute cap). Onset/release are debounced so a lucky-quiet or
single-noisy sample can't flicker the decision. No external pulldown is
assumed; adding one only makes the untouched floor tighter, which this
handles automatically.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from typing import Deque, List, Optional, Sequence

# ---------------------------------------------------------------------------
# Tunables (raw ADC units, 0..4095, unless noted)
# ---------------------------------------------------------------------------

# ANALOG arrives ~50 Hz (20 ms/tick). A 6-sample window is ~120 ms: long
# enough that noise shows its full spread, short enough that a held/sliding
# finger stays inside a small spread.
WINDOW_SIZE = 6

# The discriminating metric is the MAX consecutive sample-to-sample jump in
# the window, NOT the total spread. Why: a fast finger SLIDE (used to scroll a
# menu) moves smoothly — small, same-direction steps (~90 counts/sample) whose
# total spread over 120 ms can look noise-sized, but whose per-sample jump
# stays small. Floating noise instead jumps ~700..1400 counts EVERY sample.
# So max-consecutive-jump separates BOTH a held finger AND a sliding finger
# from the noise, where total-spread cannot.
#
# A window is a touch candidate when its max jump is below BOTH:
#   - an ABSOLUTE cap (a real hold or slide rarely jumps this much/sample), and
#   - a RELATIVE fraction of the learned untouched jump floor for that channel
#     (robust to strips quieter or noisier than the defaults).
STABLE_JUMP_ABS = 200.0
STABLE_JUMP_REL = 0.30

# Running estimate of each channel's UNTOUCHED max-jump (the noise floor),
# updated only while untouched via an EMA. Initialised high so nothing reads
# as touched until the floor has been observed. Measured floors are ~700..1400.
NOISE_FLOOR_INIT = 1000.0
NOISE_FLOOR_ALPHA = 0.05  # slow adaptation; ~20 ticks to move meaningfully

# Consecutive ticks required in the new state before flipping. Onset is a bit
# stricter than release so noise can't punch in a spurious touch, but a real
# release (noise resumes) is recognised quickly.
DEBOUNCE_ONSET_TICKS = 3
DEBOUNCE_RELEASE_TICKS = 2

# Guard rails on value: a touch can sit anywhere on the strip, but a channel
# jammed at an ADC rail for a whole window is a wiring fault / open input, not
# a finger — allow the full range but exclude only the hard extremes.
VALUE_MIN = 10
VALUE_MAX = 4085

ADC_MAX = 4095.0

# EMA smoothing for the reported position while touched (0..1 scale input).
POSITION_ALPHA = 0.4

# Minimum change in smoothed position (0..1) while touched to count as
# "moved", for last_touched_channel (menu-select UX: whichever softpot you
# most recently touched-and-moved owns the selection; a fresh onset also
# claims it).
MOVEMENT_EPSILON = 0.01


@dataclass
class _ChannelState:
    window: Deque[float] = field(default_factory=lambda: deque(maxlen=WINDOW_SIZE))
    touched: bool = False
    candidate_state: bool = False
    candidate_count: int = 0
    noise_floor: float = NOISE_FLOOR_INIT
    smoothed_position: float = 0.0
    last_smoothed_position: Optional[float] = None


@dataclass(frozen=True)
class ChannelReading:
    touched: bool
    position: float  # smoothed, normalized 0..1 (meaningful only if touched)


def _max_jump(values: Sequence[float]) -> float:
    """Largest absolute step between consecutive samples in the window."""
    return max(
        (abs(values[i] - values[i - 1]) for i in range(1, len(values))),
        default=0.0,
    )


def _mean(values: Sequence[float]) -> float:
    return sum(values) / len(values)


class TouchDetector:
    """Per-channel touch/position state machine (stability-based, self-calibrating).

    Usage:
        det = TouchDetector(n_channels=4)
        readings = det.update(raw_values)   # -> List[ChannelReading]
        det.last_touched_channel            # -> Optional[int]
    """

    def __init__(self, n_channels: int):
        if n_channels < 1:
            raise ValueError("n_channels must be >= 1")
        self.n_channels = n_channels
        self._channels: List[_ChannelState] = [_ChannelState() for _ in range(n_channels)]
        self.last_touched_channel: Optional[int] = None

    def update(self, raw_values: Sequence[int]) -> List[ChannelReading]:
        if len(raw_values) != self.n_channels:
            raise ValueError(
                f"expected {self.n_channels} raw values, got {len(raw_values)}"
            )
        return [
            self._update_channel(self._channels[i], i, raw)
            for i, raw in enumerate(raw_values)
        ]

    def _update_channel(self, state: _ChannelState, ch_idx: int, raw: int) -> ChannelReading:
        state.window.append(float(raw))

        if len(state.window) < WINDOW_SIZE:
            # Not enough history: hold untouched, keep filling.
            return ChannelReading(touched=state.touched, position=state.smoothed_position)

        jump = _max_jump(state.window)
        mean_val = _mean(state.window)

        # Learn the untouched noise floor only while NOT touched, so a long
        # hold (small jump) can't drag the floor down and lock us in.
        if not state.touched:
            state.noise_floor += NOISE_FLOOR_ALPHA * (jump - state.noise_floor)

        stable_threshold = min(STABLE_JUMP_ABS, STABLE_JUMP_REL * state.noise_floor)
        in_range = VALUE_MIN <= mean_val <= VALUE_MAX
        candidate = (jump <= stable_threshold) and in_range

        # Debounce onset/release.
        if candidate == state.candidate_state:
            state.candidate_count += 1
        else:
            state.candidate_state = candidate
            state.candidate_count = 1

        required = DEBOUNCE_ONSET_TICKS if candidate else DEBOUNCE_RELEASE_TICKS
        new_touched = candidate if state.candidate_count >= required else state.touched

        became_touched = new_touched and not state.touched
        state.touched = new_touched

        if state.touched:
            target = min(max(mean_val / ADC_MAX, 0.0), 1.0)
            if became_touched:
                state.smoothed_position = target
            else:
                state.smoothed_position += POSITION_ALPHA * (target - state.smoothed_position)
            position = state.smoothed_position

            moved = (
                state.last_smoothed_position is None
                or abs(position - state.last_smoothed_position) >= MOVEMENT_EPSILON
            )
            if became_touched or moved:
                self.last_touched_channel = ch_idx
            state.last_smoothed_position = position
        else:
            state.last_smoothed_position = None

        return ChannelReading(touched=state.touched, position=state.smoothed_position)
