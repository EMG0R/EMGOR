#!/usr/bin/env python3
"""DEMIURGE latency CLI — render reports, analyze real captures, self-test.

Subcommands:
  selftest            Testing-ground mode: synthesize captures with KNOWN
                      latency/gain/colouring/glitches and prove the engine
                      recovers them. No hardware needed. Exit 0 = good.
  analyze --capture W Analyze a real capture WAV (ch0 = reference sweep,
                      ch1+ = chain output), print the report, optionally
                      write state.json for the UIs.

The shell orchestrator (latency.sh) feeds real captures to `analyze`.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np

import engine

SR_DEFAULT = 48000

# Spec-based hardware (analog + converter) latency estimates, milliseconds,
# used when an interface has not been physically calibrated. These are the
# "add an estimate based on the interface specs" path — round-trip ADC+DAC
# figures from typical datasheets. Calibration always overrides these.
INTERFACE_SPECS = {
    "onboard":   1.5,    # generic onboard codec
    "hdmi":     35.0,    # HDMI audio sinks buffer heavily
    "bluetooth": 150.0,  # A2DP — large, variable
    "usb":       4.0,    # generic class-compliant USB interface
    "virtual":   0.0,    # virtual/loopback clock — no physical converters
    "unknown":   3.0,
}


# Internal (software) latency = quantum buffering × hops for the mode.
# Calibrated against the measured ladder in incidents/2026-05-13 + 2026-05-14:
#   bypass          ≈ 2 × quantum   csound straight to ALSA (in + out)
#   synclayer-direct≈ 4 × quantum   capture(1) + csound(1) + alsa_output ring(2);
#                                   single USB interface, chain wired straight to
#                                   the device — demiurge-sink loopback bypassed
#   synclayer       ≈ 5 × quantum   + demiurge-sink loopback hop (1×Q), used when
#                                   aggregating 2+ interfaces (the mixing point)
# @ 256/48k that's bypass 10.7 ms / direct 21.3 ms / loopback 26.7 ms (software).
MODE_HOPS = {"bypass": 2.0, "synclayer-direct": 4.0, "synclayer": 5.0}


def compute_internal_ms(quantum_ms: float, mode: str) -> float:
    return MODE_HOPS.get(mode, MODE_HOPS["bypass"]) * quantum_ms


def choose_internal(measured_typ, measured_worst, confidence: float,
                    computed: float) -> tuple[tuple[float, float], str]:
    """Prefer a real measured latency; fall back to the computed estimate.

    A virtual sink tap (no device attached) measures ~0 ms — not a real
    internal latency — so anything below 0.5 ms or low confidence falls back
    to the quantum-based computation. Returns ((typical, worst), source).
    """
    if (confidence >= 0.5 and measured_typ is not None
            and measured_typ > 0.5):
        worst = measured_worst if measured_worst is not None else measured_typ
        return (float(measured_typ), float(worst)), "measured"
    return (float(computed), float(computed)), "computed"


def resolve_offset(store: dict, interface: str, mode: str) -> tuple[float, str]:
    """Return (offset_ms, source). Calibrated value wins; else spec estimate.

    The hardware offset is the physical ADC+DAC+USB time — identical whether the
    chain runs direct or through the loopback — so `synclayer-direct` reuses a
    `synclayer` calibration (and vice-versa via default) rather than falling back
    to the spec guess.
    """
    keys = [f"{interface}/{mode}"]
    if mode == "synclayer-direct":
        keys.append(f"{interface}/synclayer")
    elif mode == "synclayer":
        keys.append(f"{interface}/synclayer-direct")
    keys.append(f"{interface}/default")
    for k in keys:
        entry = store.get(k)
        if entry and "hw_offset_ms" in entry:
            return float(entry["hw_offset_ms"]), "calibrated"
    return float(INTERFACE_SPECS.get(interface, INTERFACE_SPECS["unknown"])), "spec"


# ── reporting ───────────────────────────────────────────────────────────────

def _health(n: int, warn: int, bad: int) -> str:
    if n >= bad:
        return "BAD"
    if n >= warn:
        return "WARN"
    return "ok"


def _internal(agg: engine.LatencyAggregate, res: engine.ShotResult,
              quantum_ms: float, mode: str) -> tuple[float, float, str]:
    """(internal_typical, internal_worst, source) for the current run."""
    computed = compute_internal_ms(quantum_ms, mode)
    typ = None if np.isnan(agg.typical_ms) else agg.typical_ms
    wrst = None if np.isnan(agg.worst_ms) else agg.worst_ms
    (it, iw), src = choose_internal(typ, wrst, res.latency.confidence, computed)
    return it, iw, src


def render_report(res: engine.ShotResult, agg: engine.LatencyAggregate,
                  hw_offset_ms: float, interface: str,
                  offset_source: str = "calibrated",
                  quantum_ms: float = 5.333, mode: str = "synclayer") -> str:
    pt = "OK" if res.passthrough else f"FAIL ({res.passthrough_reason})"
    fr = res.freq_response
    gl = res.glitches

    it, iw, isrc = _internal(agg, res, quantum_ms, mode)
    total_typ = it + hw_offset_ms
    total_guar = iw + hw_offset_ms
    # Jitter/confidence only mean something when latency actually LOCKED
    # (measured). On the computed fallback the per-segment spread is noise.
    if isrc == "measured" and not np.isnan(agg.jitter_ms):
        lockline = f"  jitter (σ)   : {agg.jitter_ms:.3f} ms   confidence {res.latency.confidence:.3f} over {agg.n} seg\n"
    else:
        lockline = "  (latency computed from quantum × mode — needs an interface to clock-lock a measurement)\n"

    return (
        f"interface      : {interface}   mode: {mode}   quantum: {quantum_ms:.3f} ms\n"
        f"passthrough    : {pt}\n"
        f"\n"
        f"-- LATENCY --\n"
        f"  internal     : {it:7.3f} ms   [{isrc}]   ({mode}, {quantum_ms:.3f} ms quantum)\n"
        f"  interface    : {hw_offset_ms:7.3f} ms   [{offset_source}]   ({interface})\n"
        f"  TOTAL        : {total_typ:7.3f} ms   (typical input→output)\n"
        f"  GUARANTEED   : {total_guar:7.3f} ms   (worst-case bound)\n"
        f"{lockline}"
        f"\n"
        f"-- SIGNAL-CHAIN FINGERPRINT --\n"
        f"  overall gain : {res.gain_db:+6.2f} dB\n"
        f"  response     : low {fr.low_db:+5.1f}  mid {fr.mid_db:+5.1f}  high {fr.high_db:+5.1f} dB\n"
        f"  noise floor  : {gl.noise_floor_db:7.1f} dBFS\n"
        f"  clicks       : {gl.clicks:5d}  {_health(gl.clicks, 1, 5)}\n"
        f"  dropouts     : {gl.dropout_windows:5d}  {_health(gl.dropout_windows, 1, 5)}\n"
    )


def state_dict(res: engine.ShotResult, agg: engine.LatencyAggregate,
               hw_offset_ms: float, interface: str, mode: str,
               offset_source: str = "calibrated",
               quantum_ms: float = 5.333) -> dict:
    it, iw, isrc = _internal(agg, res, quantum_ms, mode)
    return {
        "ts": time.time(),
        "mode": mode,
        "interface": interface,
        "quantum_ms": round(quantum_ms, 3),
        "passthrough": {"ok": res.passthrough, "reason": res.passthrough_reason},
        "latency": {
            "internal_ms": round(it, 3),
            "internal_worst_ms": round(iw, 3),
            "internal_source": isrc,
            "interface_offset_ms": round(hw_offset_ms, 3),
            "interface_offset_source": offset_source,
            "total_ms": round(it + hw_offset_ms, 3),
            "total_guaranteed_ms": round(iw + hw_offset_ms, 3),
            "jitter_ms": (round(agg.jitter_ms, 3) if isrc == "measured"
                          and not np.isnan(agg.jitter_ms) else None),
            "confidence": round(res.latency.confidence, 3) if isrc == "measured" else None,
            "segments": agg.n,
        },
        "gain_db": round(res.gain_db, 2),
        "freq_response": {k: round(v, 2) for k, v in asdict(res.freq_response).items()},
        "glitches": asdict(res.glitches),
    }


def write_state(path: Path, payload: dict) -> None:
    """Atomic publish so a UI never reads a half-written file."""
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, indent=2))
    tmp.replace(path)


# ── analyze a real capture ──────────────────────────────────────────────────

def analyze_capture(capture: Path, sr_hint: int, hw_offset_ms: float,
                    interface: str, n_segments: int, mode: str = "synclayer",
                    offset_source: str = "calibrated",
                    quantum_ms: float = 5.333) -> tuple[str, dict]:
    import soundfile as sf
    data, sr = sf.read(str(capture), always_2d=True)
    data = data.astype(np.float64)
    if data.shape[1] < 2:
        raise SystemExit(f"[latency] capture needs >=2 channels, got {data.shape[1]}")
    ref = data[:, 0]
    out = data[:, 1:].mean(axis=1)

    res = engine.analyze_shot(ref, out, sr)
    series = engine.measure_latency_series(ref, out, sr, n_segments=n_segments)
    agg = engine.aggregate_latency(series)
    text = render_report(res, agg, hw_offset_ms, interface, offset_source,
                         quantum_ms, mode)
    payload = state_dict(res, agg, hw_offset_ms, interface, mode, offset_source,
                         quantum_ms)
    return text, payload


# ── testing-ground self-test ────────────────────────────────────────────────

def run_selftest() -> int:
    """Synthesize known-truth captures, confirm the engine recovers them."""
    sr = SR_DEFAULT
    ok = True
    print("== DEMIURGE latency engine — testing-ground self-test ==\n")

    def check(name, expected, got, tol, unit=""):
        nonlocal ok
        passed = abs(got - expected) <= tol
        ok = ok and passed
        verdict = "PASS" if passed else "FAIL"
        print(f"  [{verdict}] {name:28s} injected {expected:8.3f}{unit}  "
              f"recovered {got:8.3f}{unit}  (tol {tol}{unit})")
        return passed

    from scipy import signal as sg

    # Scenario 1: known latency + gain
    ref = engine.make_sweep(sr, dur=2.0)
    out = np.concatenate([np.zeros(480), ref])[: len(ref)] * (10 ** (-6.0 / 20))
    r = engine.analyze_shot(ref, out, sr)
    print("Scenario 1 — 10.000 ms delay, -6 dB gain, clean:")
    check("latency", 10.0, r.latency.latency_ms, 0.05, " ms")
    check("gain", -6.0, r.gain_db, 0.5, " dB")
    print(f"  [{'PASS' if r.passthrough else 'FAIL'}] passthrough                 -> "
          f"{r.passthrough_reason}")
    ok = ok and r.passthrough

    # Scenario 2: low-pass colouring detected
    out2 = sg.sosfiltfilt(sg.butter(6, 1500.0, fs=sr, output="sos"),
                          np.concatenate([np.zeros(480), ref])[: len(ref)])
    r2 = engine.analyze_shot(ref, out2, sr)
    print("\nScenario 2 — 1.5 kHz low-pass colouring:")
    hp = r2.freq_response.high_db < r2.freq_response.low_db - 12.0
    ok = ok and hp
    print(f"  [{'PASS' if hp else 'FAIL'}] HF rolloff seen             "
          f"low {r2.freq_response.low_db:+.1f}  high {r2.freq_response.high_db:+.1f} dB")

    # Scenario 3: injected clicks caught, clean stays clean
    clean = np.concatenate([np.zeros(480), ref])[: len(ref)].copy()
    dirty = clean.copy()
    for pos in (10000, 25000, 40000, 60000, 80000):
        dirty[pos] += 0.9
    rc = engine.analyze_shot(ref, clean, sr)
    rd = engine.analyze_shot(ref, dirty, sr)
    print("\nScenario 3 — glitch detection:")
    c1 = rc.glitches.clicks == 0
    c2 = rd.glitches.clicks >= 4
    ok = ok and c1 and c2
    print(f"  [{'PASS' if c1 else 'FAIL'}] clean capture clicks        {rc.glitches.clicks} (want 0)")
    print(f"  [{'PASS' if c2 else 'FAIL'}] 5 injected clicks caught    {rd.glitches.clicks} (want >=4)")

    # Scenario 4: worst-case aggregation
    agg = engine.aggregate_latency([10.0, 10.2, 9.8, 10.1, 12.0])
    print("\nScenario 4 — worst-case (guaranteed) aggregation:")
    check("worst-case", 12.0, agg.worst_ms, 0.01, " ms")
    check("typical (median)", 10.1, agg.typical_ms, 0.2, " ms")

    print("\n" + ("== ALL SCENARIOS PASS ==" if ok else "== SELF-TEST FAILED =="))
    return 0 if ok else 1


# ── entrypoint ──────────────────────────────────────────────────────────────

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="latency")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("selftest", help="testing-ground self-test (no hardware)")

    g = sub.add_parser("gensweep", help="write the reference sweep WAV")
    g.add_argument("--out", required=True, type=Path)
    g.add_argument("--dur", type=float, default=1.0)
    g.add_argument("--sr", type=int, default=SR_DEFAULT)
    g.add_argument("--loops", type=int, default=1,
                   help="tile the sweep N times for a longer capture")

    c = sub.add_parser("calib", help="read/write per-interface hardware offset")
    c.add_argument("action", choices=["get", "set", "list", "offset"])
    c.add_argument("--store", type=Path, required=True)
    c.add_argument("--interface")
    c.add_argument("--mode", default="default", help="bypass | synclayer | default")
    c.add_argument("--ms", type=float)

    a = sub.add_parser("analyze", help="analyze a real capture WAV")
    a.add_argument("--capture", required=True, type=Path)
    a.add_argument("--interface", default="unknown")
    a.add_argument("--hw-offset-ms", type=float, default=0.0)
    a.add_argument("--offset-source", default="calibrated")
    a.add_argument("--mode", default="synclayer", help="bypass | synclayer")
    a.add_argument("--quantum-ms", type=float, default=5.333)
    a.add_argument("--segments", type=int, default=4)
    a.add_argument("--state", type=Path, help="write state.json here")
    a.add_argument("--sr", type=int, default=SR_DEFAULT)

    args = ap.parse_args(argv)

    if args.cmd == "selftest":
        return run_selftest()

    if args.cmd == "gensweep":
        import soundfile as sf
        one = engine.make_sweep(args.sr, dur=args.dur)
        sig = np.tile(one, max(1, args.loops))
        stereo = np.stack([sig, sig], axis=1)
        sf.write(str(args.out), stereo, args.sr, subtype="FLOAT")
        print(f"[latency] sweep written: {args.out} "
              f"({args.dur}s x{args.loops} @ {args.sr} Hz)")
        return 0

    if args.cmd == "calib":
        store = args.store
        data = json.loads(store.read_text()) if store.exists() else {}
        key = f"{args.interface}/{args.mode}"
        if args.action == "list":
            print(json.dumps(data, indent=2))
            return 0
        if args.action == "get":
            entry = data.get(key, {})
            print(entry.get("hw_offset_ms", 0.0))
            return 0
        if args.action == "offset":
            # Resolve to calibrated-or-spec; print "<ms>\t<source>".
            ms, source = resolve_offset(data, args.interface or "unknown", args.mode)
            print(f"{ms}\t{source}")
            return 0
        if args.action == "set":
            if args.interface is None or args.ms is None:
                raise SystemExit("[latency] calib set needs --interface and --ms")
            data[key] = {"hw_offset_ms": round(args.ms, 3), "ts": time.time()}
            write_state(store, data)
            print(f"[latency] calibrated {key}: {args.ms:.3f} ms")
            return 0

    if args.cmd == "analyze":
        text, payload = analyze_capture(
            args.capture, args.sr, args.hw_offset_ms,
            args.interface, args.segments, args.mode, args.offset_source,
            args.quantum_ms)
        print(text)
        if args.state:
            write_state(args.state, payload)
            print(f"[latency] state written: {args.state}")
        return 0

    return 2


if __name__ == "__main__":
    sys.exit(main())
