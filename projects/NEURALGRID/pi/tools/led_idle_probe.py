#!/usr/bin/env python3
"""neuralGrid LED idle probe — is the firmware re-applying/showing LED
frames with no new frame from the Pi?

Context (HANDOFF.md §12): the Pi-side de-flicker fix (diff-against-last-
sent + a real _render_lock) is deployed and verified — the Pi itself sends
0 LED frames/sec at idle. The grid STILL flickers. That means either (a)
the firmware is doing LED work on its own with no incoming LEDS_FULL/
LEDS_PARTIAL, or (b) something else periodic (the 5s dead-lidar re-init
round-robin is the prime suspect, since 3 of 4 channels are currently
dead) is glitching the shared I2C bus mid-refresh.

This script is a passive, single-purpose diagnostic: open the link, PING,
send exactly ONE full frame (a known baseline so applyPendingLed() has
something real to have already applied), then send NOTHING downstream for
--idle-s seconds while logging every upstream TYPE_ACK (0x15) and
TYPE_TIMING (0x16) frame with a timestamp, plus periodic TYPE_LIDAR (0x13)
snapshots so re-init cadence is visible alongside any led_apply spike.

2026-09-08 correction: the reported flicker is a fast, subtle SHIMMER
visible only on LIT pads (dark pads look fine), not a periodic blink —
the classic seesaw/NeoTrellis symptom of show() re-latching a board's
WS2812 stream with unchanged data (each latch is a real current-draw
burst, invisible on an all-zero board, perceptible on a lit one). That is
exactly why the baseline frame below is a real lit, non-blank pattern
(not leds.blank()/all-off) rather than a blank frame — an idle test with
nothing lit could never surface this symptom even if the bug were real.

Firmware behaves correctly (bug is NOT here) iff, over the whole idle
window:
    - zero TYPE_ACK frames arrive (an ACK is only ever sent from
      applyPendingLed(), which only runs when pendingLedType != 0 — i.e.
      only in response to an actual LEDS_FULL/LEDS_PARTIAL from the Pi)
    - every TYPE_TIMING sample's led_apply slot reads 0 (a phase slot only
      goes non-zero when that phase actually ran that window — see
      grid_link_lidar.ino's tickTiming() comment)

    python3 pi/tools/led_idle_probe.py --idle-s 30
    python3 pi/tools/led_idle_probe.py --port /dev/ttyUSB0 --idle-s 30 --json out.json

MUST be run with neuralgrid-app-live / neuralgrid-monitor stopped first
(single-owner serial port) — see HANDOFF.md's flash/measurement procedure.
Restart whichever service owns the port afterward.
"""
from __future__ import annotations

import argparse
import glob
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # neuralGrid/pi -> neuralgrid package

from neuralgrid import codec_lidar  # noqa: E402
from neuralgrid.link_lidar import LidarGridLink  # noqa: E402


def autodetect_port() -> str:
    candidates = (
        sorted(glob.glob("/dev/serial/by-id/*"))
        + sorted(glob.glob("/dev/ttyUSB*"))
        + sorted(glob.glob("/dev/tty.usbserial*"))  # macOS
    )
    if not candidates:
        raise SystemExit("no serial port found — pass --port explicitly")
    return candidates[0]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None)
    ap.add_argument("--idle-s", type=float, default=30.0, help="seconds to send NOTHING and just listen")
    ap.add_argument("--json", default=None, help="also write the raw event log to this path")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    t_start = time.monotonic()
    events: list[dict] = []

    def log(kind: str, **fields) -> None:
        rec = {"t": round(time.monotonic() - t_start, 4), "kind": kind, **fields}
        events.append(rec)
        extra = " ".join(f"{k}={v}" for k, v in fields.items())
        print(f"[{rec['t']:8.3f}s] {kind:9s} {extra}")

    hello_box = {}

    def on_hello(h):
        hello_box["fw_ver"] = getattr(h, "fw_ver", None)
        log("HELLO", fw_ver=hello_box["fw_ver"], n_analog=getattr(h, "n_analog", None))

    def on_ack(ack: codec_lidar.LedAck) -> None:
        log("ACK", acked_type=hex(ack.acked_type), apply_us=ack.apply_us)

    def on_timing(t: codec_lidar.Timing) -> None:
        d = t.as_dict()
        log("TIMING", **d)

    def on_lidar(lidar: codec_lidar.Lidar) -> None:
        # Cheap liveness snapshot, not every frame (they arrive every 50ms) —
        # printed once every ~1s so the re-init cadence (5s period) is
        # visible without flooding the log.
        now = time.monotonic() - t_start
        last = on_lidar._last  # type: ignore[attr-defined]
        if now - last < 1.0:
            return
        on_lidar._last = now  # type: ignore[attr-defined]
        ranges = [None if r == codec_lidar.LIDAR_INVALID else r for r in lidar.ranges]
        log("LIDAR", N=ranges[0], S=ranges[1], E=ranges[2], W=ranges[3])

    on_lidar._last = -999.0  # type: ignore[attr-defined]

    def on_lidardiag(d) -> None:
        log("LIDARDIAG", responded_alone=list(d.responded_alone), zombie=d.zombie,
            fail_stage=list(d.fail_stage) if d.fail_stage is not None else None)

    link = LidarGridLink(
        port, on_hello=on_hello, on_ack=on_ack, on_timing=on_timing,
        on_lidar=on_lidar, on_lidardiag=on_lidardiag,
    )
    link.start()
    print(f"# opened {port}, PINGing...")
    link.send_ping()
    time.sleep(1.0)
    if "fw_ver" not in hello_box:
        print("# WARNING: no HELLO received within 1s — retrying PING once")
        link.send_ping()
        time.sleep(1.0)

    baseline = [(255, 0, 0)] * 32 + [(0, 255, 0)] * 32  # any deterministic, non-blank frame
    print("# sending ONE baseline LEDS_FULL frame, then going silent...")
    link.send_leds_full(baseline)
    time.sleep(0.5)  # let the ACK for the baseline frame land before the idle window starts

    n_events_before_idle = len(events)
    print(f"# --- idle window starts now: sending NOTHING for {args.idle_s:.1f}s ---")
    idle_t0 = time.monotonic()
    while time.monotonic() - idle_t0 < args.idle_s:
        time.sleep(0.05)
    print("# --- idle window ends ---")

    idle_events = events[n_events_before_idle:]
    acks = [e for e in idle_events if e["kind"] == "ACK"]
    timings = [e for e in idle_events if e["kind"] == "TIMING"]
    led_apply_nonzero = [e for e in timings if e.get("led_apply", 0) not in (0, "0")]

    print()
    print("# ===== SUMMARY (idle window only) =====")
    print(f"# ACK frames received while idle (expect 0): {len(acks)}")
    print(f"# TIMING samples during idle: {len(timings)}")
    print(f"# TIMING samples with led_apply != 0 (expect 0): {len(led_apply_nonzero)}")
    if led_apply_nonzero:
        for e in led_apply_nonzero:
            print(f"#   t={e['t']:.3f}s led_apply={e['led_apply']}us (full timing: {e})")
    if acks:
        for e in acks:
            print(f"#   t={e['t']:.3f}s ACK acked_type={e['acked_type']} apply_us={e['apply_us']}")
    verdict = "FIRMWARE IS RE-APPLYING LED FRAMES WITH NO NEW DATA" if (acks or led_apply_nonzero) else \
              "firmware idle-clean: no ACKs, no nonzero led_apply while idle"
    print(f"# VERDICT: {verdict}")

    link.stop()

    if args.json:
        Path(args.json).write_text(json.dumps({"events": events}, indent=2))
        print(f"# wrote {args.json}")


if __name__ == "__main__":
    main()
