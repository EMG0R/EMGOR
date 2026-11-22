#!/usr/bin/env python3
"""Audio purity analysis: phase-locked comparison of a returned sine
against the sine that was sent.

The premise: every audible artifact -- an xrun, a resync, a dropped or
repeated buffer, a clipped sample, an auto-mute chatter, a gap -- is a
deviation from a sine. Counters measure subsystems; this measures the
audio. It is the only test that fails when the rig is silent or dirty
regardless of which subsystem caused it.

Playback and capture ride the same card clock on a loopback rig, so the
returned sine is sample-locked to the reference: a single global fit of
amplitude, phase and offset is exact, and everything left over in the
residual is an artifact. That is what makes per-sample truth possible
here and it is why this is not a spectral test -- an average would hide
the very events we are hunting.

Used by the demiurge-purity CLI. Import it or run it standalone:

    purity.py REF_HZ RECORDING.wav [--threshold PCT]

Pure numpy, no JACK headers, so unlike rtdelay.c/nullclient.c this half
of the toolchain does run on the Mac.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import wave
from dataclasses import dataclass, field, asdict


# A deviation has to clear this fraction of full scale before it counts
# as an artifact. Below it we are looking at converter noise and dither,
# which are not what anybody means by "crackle".
DEFAULT_THRESHOLD_PCT = 0.5

# Events closer together than this are one artifact, not several -- a
# single click smears across a few hundred samples once it has been
# through two converters.
EVENT_COALESCE_MS = 5.0

# An interval histogram this tight means the artifact is being clocked
# by something, not caused by load.
PERIODIC_TOLERANCE = 0.15


@dataclass
class Event:
    """One artifact: where it was, how long, how far off."""

    start_s: float
    duration_ms: float
    peak_dev_pct: float

    @property
    def peak_dev_db(self) -> float:
        return pct_to_db(self.peak_dev_pct)


@dataclass
class PurityResult:
    """Everything one injection run learned."""

    ok: bool
    verdict: str
    detail: str = ""

    ref_hz: float = 0.0
    duration_s: float = 0.0
    samples: int = 0

    # Phase lock
    latency_samples: int = 0
    latency_ms: float = 0.0
    correlation: float = 0.0

    # Levels
    amplitude_pct: float = 0.0
    peak_pct: float = 0.0
    noise_floor_db: float = -math.inf
    thdn_db: float = -math.inf

    # Artifacts
    deviating_samples: int = 0
    peak_dev_pct: float = 0.0
    peak_dev_db: float = -math.inf
    events: list = field(default_factory=list)
    slips: int = 0
    periodic: bool = False
    period_s: float = 0.0
    period_jitter_pct: float = 0.0

    def as_dict(self) -> dict:
        d = asdict(self)
        d["events"] = [asdict(e) for e in self.events]
        return d


def pct_to_db(pct: float) -> float:
    """Percent of full scale to dBFS."""
    if pct <= 0:
        return -math.inf
    return 20.0 * math.log10(pct / 100.0)


def read_wav(path: str):
    """Read a WAV to float arrays in [-1, 1], one per channel.

    Handles the 16- and 32-bit integer and 32-bit float cases that
    pw-record actually emits; anything else is a caller error worth
    naming rather than silently mangling.
    """
    import numpy as np

    with wave.open(path, "rb") as w:
        nch = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())

    if width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        # pw-record writes f32 for float formats; the WAV header does not
        # distinguish them here, so sniff: real audio in float never
        # exceeds a few units, whereas i32 audio is astronomically large.
        as_f32 = np.frombuffer(raw, dtype="<f4")
        if as_f32.size and np.nanmax(np.abs(as_f32)) <= 8.0:
            data = as_f32.astype(np.float64)
        else:
            data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        packed = (b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16))
        packed = np.where(packed & 0x800000, packed - 0x1000000, packed)
        data = packed.astype(np.float64) / 8388608.0
    else:
        raise ValueError(f"unsupported sample width: {width * 8}-bit")

    if nch < 1:
        raise ValueError("recording has no channels")
    chans = [data[i::nch] for i in range(nch)]
    return chans, rate


def _fit_sine(sig, ref_hz: float, rate: int):
    """Least-squares fit of a sine at a known frequency.

    Returns (amplitude, phase, fitted_signal). Solving for the sin and
    cos coefficients is exact and needs no search, which matters because
    any iterative fit would happily absorb the artifacts we are trying
    to measure.
    """
    import numpy as np

    n = len(sig)
    t = np.arange(n) / rate
    w = 2 * math.pi * ref_hz * t
    s, c = np.sin(w), np.cos(w)

    # Normal equations for a . sin + b . cos
    m = np.array([[np.dot(s, s), np.dot(s, c)], [np.dot(c, s), np.dot(c, c)]])
    v = np.array([np.dot(s, sig), np.dot(c, sig)])
    try:
        a, b = np.linalg.solve(m, v)
    except np.linalg.LinAlgError:
        return 0.0, 0.0, np.zeros(n)

    amp = math.hypot(a, b)
    phase = math.atan2(b, a)
    return amp, phase, a * s + b * c


# Long enough for a stable fit (about 85 cycles at 1 kHz), short enough that a
# single event stays inside one or two blocks instead of poisoning the run.
FIT_BLOCK = 4096

# A block whose phase has moved further than this from its neighbour did not
# drift there -- the stream slipped. Converter clocks on one card do not drift
# at all, so anything above measurement noise is a real discontinuity.
PHASE_SLIP_RAD = 0.35


def _fit_blockwise(sig, ref_hz: float, rate: int):
    """Fit the reference sine in short blocks rather than all at once.

    A single global fit is exact only if the return is one unbroken sine. If
    the stream slips even once -- a dropped buffer, a resync, a resampler
    correction -- every later sample sits at the wrong phase, the global fit
    splits the difference, and the residual balloons across the WHOLE
    recording. That reports one real event as total corruption and hides where
    it happened, which is precisely the wrong direction for a diagnostic.

    Fitting per block keeps each event local, and makes the slip itself
    measurable: the phase step between adjacent blocks is the discontinuity,
    in radians, convertible straight to frames.

    Returns (mean_amplitude, fitted_signal, slip_events).
    """
    import numpy as np

    n = len(sig)
    fitted = np.zeros(n)
    amps, slips = [], []
    prev_phase = None

    for lo in range(0, n, FIT_BLOCK):
        hi = min(lo + FIT_BLOCK, n)
        if hi - lo < 64:
            fitted[lo:hi] = sig[lo:hi]
            continue

        amp, phase, fit = _fit_sine(sig[lo:hi], ref_hz, rate)
        fitted[lo:hi] = fit
        amps.append(amp)

        # Each block is fitted in its own time origin, so a continuous sine
        # advances by a predictable amount between blocks; subtract it and
        # what remains is slip.
        if prev_phase is not None:
            advance = 2 * math.pi * ref_hz * FIT_BLOCK / rate
            delta = (phase - prev_phase - advance + math.pi) % (2 * math.pi) - math.pi
            if abs(delta) > PHASE_SLIP_RAD:
                frames = abs(delta) / (2 * math.pi) * rate / ref_hz
                slips.append(
                    Event(
                        start_s=lo / rate,
                        duration_ms=frames * 1000.0 / rate,
                        peak_dev_pct=amp * 100.0,
                    )
                )
        prev_phase = phase

    return (float(np.mean(amps)) if amps else 0.0), fitted, slips


def _find_tone(rec, rate: int):
    """Locate the injected tone inside the recording by its envelope.

    Capture is armed before playback starts -- that ordering is what
    guarantees we never miss the onset -- so the recording begins with
    however much silence the arming took. Cross-correlation cannot find
    the onset of a *continuous* sine (every cycle correlates equally
    well, so the peak is arbitrary), which is why this works on the
    amplitude envelope instead. A prior attempt at this was fooled by
    exactly that ambiguity.

    Returns (start, end, onset_confidence).
    """
    import numpy as np

    win = max(1, int(rate * 0.005))
    # Envelope by moving RMS, decimated -- we only need to find edges.
    sq = rec**2
    kern = np.ones(win) / win
    env = np.sqrt(np.convolve(sq, kern, mode="same"))

    peak = float(np.max(env)) if len(env) else 0.0
    if peak <= 0:
        return 0, len(rec), 0.0

    # Half the peak envelope is a robust edge: well above any plausible
    # noise floor, well below the tone's steady level.
    loud = env > (peak * 0.5)
    idx = np.flatnonzero(loud)
    if not len(idx):
        return 0, len(rec), 0.0

    start, end = int(idx[0]), int(idx[-1]) + 1

    # Confidence: how cleanly the tone stands above what preceded it.
    if start > win:
        before = float(np.mean(env[:start])) or 1e-12
        conf = min(1.0, float(np.mean(env[start:end])) / (before * 100.0))
    else:
        conf = 0.5

    return start, end, conf


def _coalesce(mask, rate: int, sig_ref):
    """Group deviating samples into events.

    A single click emerges from the converters spread over a few hundred
    samples, so contiguous-ish runs are one event. Reporting them
    per-sample would turn one artifact into a meaningless count.
    """
    import numpy as np

    if not mask.any():
        return []

    gap = max(1, int(rate * EVENT_COALESCE_MS / 1000.0))
    idx = np.flatnonzero(mask)
    splits = np.flatnonzero(np.diff(idx) > gap)
    groups = np.split(idx, splits + 1)

    events = []
    for g in groups:
        lo, hi = int(g[0]), int(g[-1])
        peak = float(np.max(np.abs(sig_ref[lo : hi + 1]))) * 100.0
        events.append(
            Event(
                start_s=lo / rate,
                duration_ms=(hi - lo + 1) * 1000.0 / rate,
                peak_dev_pct=peak,
            )
        )
    return events


def _periodicity(events):
    """Decide whether artifacts are on a clock or just happening.

    A tight interval distribution means something is ticking -- a clock
    broadcast, a message-ring drain, a resync cadence -- and that is a
    completely different bug from load-driven bursts. Telling those two
    apart is most of the diagnostic value here.
    """
    import numpy as np

    if len(events) < 3:
        return False, 0.0, 0.0

    starts = np.array([e.start_s for e in events])
    gaps = np.diff(starts)
    if len(gaps) < 2:
        return False, 0.0, 0.0

    mean = float(np.mean(gaps))
    if mean <= 0:
        return False, 0.0, 0.0
    jitter = float(np.std(gaps) / mean)
    return jitter <= PERIODIC_TOLERANCE, mean, jitter * 100.0


def analyse(
    recording: str,
    ref_hz: float,
    threshold_pct: float = DEFAULT_THRESHOLD_PCT,
    channel: int = 0,
    skip_s: float = 0.25,
) -> PurityResult:
    """Compare a recorded return against the sine that produced it."""
    import numpy as np

    try:
        chans, rate = read_wav(recording)
    except Exception as exc:  # noqa: BLE001 - surfaced to the user as text
        return PurityResult(ok=False, verdict="UNREADABLE", detail=str(exc))

    if channel >= len(chans):
        channel = 0
    rec = chans[channel]

    if len(rec) < rate // 4:
        return PurityResult(
            ok=False,
            verdict="TOO SHORT",
            detail=f"only {len(rec)} samples captured",
        )

    onset, tone_end, quality = _find_tone(rec, rate)

    # Trim both edges: the leading moments carry the DAC's own unmute
    # ramp and the tone's onset transient, and the trailing edge carries
    # the stream teardown. Neither is a fault of the rig under test, and
    # counting them would make every clean run look dirty.
    start = onset + int(rate * skip_s)
    stop = max(start, tone_end - int(rate * skip_s))
    body = rec[start:stop]

    if len(body) < rate // 4:
        return PurityResult(
            ok=False,
            verdict="TOO SHORT",
            detail=(
                "not enough steady tone after the onset and teardown were "
                "trimmed -- inject for longer"
            ),
            ref_hz=ref_hz,
            duration_s=len(rec) / rate,
            samples=len(rec),
        )

    amp, fitted, slips = _fit_blockwise(body, ref_hz, rate)
    peak = float(np.max(np.abs(body)))

    # A fit that found nothing means no tone came back. That is the
    # silent-rig case, and it is the single most important thing this
    # test catches -- every counter reads perfect while nothing reaches
    # the jacks.
    if amp < 0.001:
        return PurityResult(
            ok=False,
            verdict="NO SIGNAL",
            detail=(
                "no tone returned -- the cable is unpatched, or the signal "
                "never reached the jacks"
            ),
            ref_hz=ref_hz,
            duration_s=len(rec) / rate,
            samples=len(rec),
            peak_pct=peak * 100.0,
            correlation=quality,
        )

    residual = body - fitted
    res_rms = float(np.sqrt(np.mean(residual**2)))
    thdn = 20.0 * math.log10(res_rms / amp) if res_rms > 0 else -math.inf
    noise_floor = 20.0 * math.log10(res_rms) if res_rms > 0 else -math.inf

    thr = threshold_pct / 100.0
    mask = np.abs(residual) > thr
    events = _coalesce(mask, rate, residual)

    # A slip that produced no large residual is still a real discontinuity --
    # the stream jumped without a click loud enough to clear the threshold --
    # so merge them in rather than reporting two disjoint lists.
    known = {round(e.start_s, 2) for e in events}
    for sl in slips:
        if round(sl.start_s, 2) not in known:
            events.append(sl)
    events.sort(key=lambda e: e.start_s)

    periodic, period, jitter = _periodicity(events)

    peak_dev = float(np.max(np.abs(residual))) if len(residual) else 0.0

    res = PurityResult(
        ok=True,
        verdict="PURE",
        ref_hz=ref_hz,
        duration_s=len(rec) / rate,
        samples=len(rec),
        latency_samples=onset,
        latency_ms=onset * 1000.0 / rate,
        correlation=quality,
        amplitude_pct=amp * 100.0,
        peak_pct=peak * 100.0,
        noise_floor_db=noise_floor,
        thdn_db=thdn,
        deviating_samples=int(mask.sum()),
        peak_dev_pct=peak_dev * 100.0,
        peak_dev_db=pct_to_db(peak_dev * 100.0),
        events=events,
        slips=len(slips),
        periodic=periodic,
        period_s=period,
        period_jitter_pct=jitter,
    )

    _verdict(res, peak)
    return res


def _verdict(res: PurityResult, peak: float) -> None:
    """Turn the numbers into something a musician can act on.

    The order matters: report the cause that explains the most, because
    a player wants one instruction, not a list of symptoms.
    """
    # Clipping first -- it crackles audibly, logs nothing anywhere, and
    # every other number looks fine while it happens.
    if peak >= 0.999:
        res.ok = False
        res.verdict = "CLIPPING"
        res.detail = (
            "the return is hitting full scale -- reduce the input gain "
            "or the output level"
        )
        return

    if not res.events:
        res.ok = True
        res.verdict = "PURE"
        res.detail = (
            f"no deviations above threshold over {res.duration_s:.1f}s; "
            f"THD+N {res.thdn_db:.1f} dB"
        )
        return

    res.ok = False
    n = len(res.events)

    if res.periodic:
        res.verdict = "PERIODIC ARTIFACTS"
        res.detail = (
            f"{n} artifacts, one every {res.period_s:.2f}s "
            f"(jitter {res.period_jitter_pct:.0f}%) -- a fixed interval "
            f"points at a clock or scheduling source, not load"
        )
    else:
        res.verdict = "ARTIFACTS"
        res.detail = (
            f"{n} artifacts at irregular intervals, peak "
            f"{res.peak_dev_db:.1f} dBFS -- irregular bursts point at "
            f"load or an intermittent fault"
        )


def format_report(res: PurityResult, max_events: int = 12) -> str:
    """Human-readable report, in the house style of the other CLIs."""
    lines = []
    mark = "OK" if res.ok else "!!"
    lines.append(f"[{mark}] {res.verdict}")
    if res.detail:
        lines.append(f"     {res.detail}")
    lines.append("")

    if res.samples:
        lines.append(f"  signal      {res.ref_hz:g} Hz, {res.duration_s:.1f}s captured")
    if res.amplitude_pct:
        lines.append(
            f"  level       {res.amplitude_pct:.1f}% FS returned "
            f"(peak {res.peak_pct:.1f}%)"
        )
    if res.latency_samples:
        lines.append(
            f"  round-trip  {res.latency_samples} samples "
            f"({res.latency_ms:.2f} ms)"
        )
    if res.thdn_db > -math.inf:
        lines.append(f"  THD+N       {res.thdn_db:.1f} dB")
    if res.noise_floor_db > -math.inf:
        lines.append(f"  noise floor {res.noise_floor_db:.1f} dBFS")

    if res.events:
        lines.append("")
        if res.slips:
            lines.append(
                f"  {res.slips} phase slip(s) -- the stream jumped, "
                f"not just distorted"
            )
        lines.append(
            f"  {len(res.events)} artifact(s), "
            f"{res.deviating_samples} deviating samples, "
            f"peak {res.peak_dev_db:.1f} dBFS"
        )
        for e in res.events[:max_events]:
            lines.append(
                f"    {e.start_s:8.3f}s  {e.duration_ms:6.2f} ms  "
                f"{e.peak_dev_db:6.1f} dBFS"
            )
        if len(res.events) > max_events:
            lines.append(f"    ... and {len(res.events) - max_events} more")

    return "\n".join(lines)


def main(argv=None) -> int:
    p = argparse.ArgumentParser(
        description="Compare a returned sine against the sine that was sent."
    )
    p.add_argument("ref_hz", type=float, help="frequency of the injected tone")
    p.add_argument("recording", help="WAV file of the captured return")
    p.add_argument(
        "--threshold",
        type=float,
        default=DEFAULT_THRESHOLD_PCT,
        help=f"deviation threshold, %%FS (default {DEFAULT_THRESHOLD_PCT})",
    )
    p.add_argument("--channel", type=int, default=0, help="channel to analyse")
    p.add_argument("--json", action="store_true", help="machine-readable output")
    args = p.parse_args(argv)

    res = analyse(
        args.recording,
        args.ref_hz,
        threshold_pct=args.threshold,
        channel=args.channel,
    )

    if args.json:
        print(json.dumps(res.as_dict(), indent=2, default=str))
    else:
        print(format_report(res))

    return 0 if res.ok else 1


if __name__ == "__main__":
    sys.exit(main())
