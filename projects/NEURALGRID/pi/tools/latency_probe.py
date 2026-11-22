#!/usr/bin/env python3
"""neuralGrid latency probe — measures, does not fix.

ADDITIVE tool (pi/tools/latency_probe.py). Owns the serial port directly, so
the monitor service MUST be stopped first:

    sudo systemctl stop neuralgrid-monitor
    cd ~/neuralGrid/pi && python3 tools/latency_probe.py
    sudo systemctl start neuralgrid-monitor

Measures, against whatever firmware is currently flashed:

  (a) link RTT      — PING -> HELLO round trip, N samples, p50/p95/p99/max.
  (b) ANALOG fps     — observed inter-frame interval over the run.
  (c) LIDAR fps      — same, for TYPE_LIDAR frames.
  (d) LED apply (fw5+ only) — sends a burst of LEDS_FULL/LEDS_PARTIAL frames
      and reads back the TYPE_ACK (0x15) the firmware sends after each
      applied (non-coalesced-away) frame: recv->show()-complete latency,
      p50/p95, separately for LEDS_FULL and LEDS_PARTIAL. Silently skipped
      (reported as "not supported") against fw<5, which never sends 0x15 —
      the tool detects this from HELLO.fw_ver and does not hang waiting.

Also importable: run(link, ...) -> dict, used by monitor.py's "latency
test" button (it borrows the ALREADY-OPEN link instead of opening a second
one — see monitor.py's Handler for /latency_test).

Pure stdlib + pyserial (via neuralgrid.link). No side effects beyond
sending PING/LEDS_*/CLEAR frames and restoring a blank grid at the end.
"""

from __future__ import annotations

import argparse
import statistics
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from neuralgrid import codec, codec_lidar  # noqa: E402
from neuralgrid.link_lidar import LidarGridLink  # noqa: E402

DEFAULT_RTT_SAMPLES = 30
DEFAULT_LED_SAMPLES = 40
DEFAULT_STREAM_S = 3.0  # how long to just listen for ANALOG/LIDAR fps


def _percentile(values, p):
    if not values:
        return None
    values = sorted(values)
    k = (len(values) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(values) - 1)
    if f == c:
        return values[f]
    return values[f] + (values[c] - values[f]) * (k - f)


def stats(values_ms):
    """Public alias of _stats — n/min/p50/p95/p99/max over a list of
    millisecond samples. Used by monitor.py for its own rolling windows
    (link RTT, LED ack) so both tools report numbers the same way."""
    return _stats(values_ms)


def _stats(values_ms):
    if not values_ms:
        return {"n": 0, "p50": None, "p95": None, "p99": None, "max": None, "min": None}
    return {
        "n": len(values_ms),
        "min": round(min(values_ms), 2),
        "p50": round(_percentile(values_ms, 50), 2),
        "p95": round(_percentile(values_ms, 95), 2),
        "p99": round(_percentile(values_ms, 99), 2),
        "max": round(max(values_ms), 2),
    }


class _Collector:
    """Short-lived listener attached to an already-running LidarGridLink for
    the duration of one probe run. Chains onto whatever callbacks the link
    already has (e.g. monitor.py's MonitorState) rather than replacing them,
    so running the probe from the monitor UI doesn't blind the live view.
    """

    def __init__(self, link: LidarGridLink):
        self.link = link
        self.lock = threading.Lock()
        self.hello_events: list[float] = []       # monotonic times a HELLO arrived
        self.analog_times: list[float] = []
        self.lidar_times: list[float] = []
        self.ack_full_ms: list[float] = []
        self.ack_partial_ms: list[float] = []
        self.fw_ver = None

        self._prev_hello = link.on_hello
        self._prev_analog = link.on_analog
        self._prev_lidar = link.on_lidar
        self._prev_ack = getattr(link, "on_ack", None)

        link.on_hello = self._on_hello
        link.on_analog = self._on_analog
        link.on_lidar = self._on_lidar
        link.on_ack = self._on_ack

    def detach(self):
        self.link.on_hello = self._prev_hello
        self.link.on_analog = self._prev_analog
        self.link.on_lidar = self._prev_lidar
        self.link.on_ack = self._prev_ack

    def _on_hello(self, h):
        with self.lock:
            self.hello_events.append(time.monotonic())
            self.fw_ver = h.fw_ver
        if self._prev_hello:
            self._prev_hello(h)

    def _on_analog(self, a):
        with self.lock:
            self.analog_times.append(time.monotonic())
        if self._prev_analog:
            self._prev_analog(a)

    def _on_lidar(self, l):
        with self.lock:
            self.lidar_times.append(time.monotonic())
        if self._prev_lidar:
            self._prev_lidar(l)

    def _on_ack(self, ack: codec_lidar.LedAck):
        ms = ack.apply_us / 1000.0
        with self.lock:
            if ack.acked_type == codec.TYPE_LEDS_FULL:
                self.ack_full_ms.append(ms)
            else:
                self.ack_partial_ms.append(ms)
        if self._prev_ack:
            self._prev_ack(ack)


def _fps_from_times(times: list[float]) -> float | None:
    if len(times) < 2:
        return None
    span = times[-1] - times[0]
    if span <= 0:
        return None
    return (len(times) - 1) / span


def run(
    link: LidarGridLink,
    rtt_samples: int = DEFAULT_RTT_SAMPLES,
    led_samples: int = DEFAULT_LED_SAMPLES,
    stream_s: float = DEFAULT_STREAM_S,
    log=lambda msg: None,
) -> dict:
    """Run the full probe against an ALREADY-STARTED LidarGridLink (caller
    owns start()/stop()). Returns a plain-dict report, safe to json.dumps.
    """
    col = _Collector(link)
    try:
        # -- (a) link RTT: PING -> next HELLO ------------------------------
        log(f"RTT: {rtt_samples} PING/HELLO round trips...")
        rtts_ms = []
        for _ in range(rtt_samples):
            with col.lock:
                before = len(col.hello_events)
            t0 = time.monotonic()
            link.send_ping()
            deadline = t0 + 1.0
            got = False
            while time.monotonic() < deadline:
                with col.lock:
                    if len(col.hello_events) > before:
                        t1 = col.hello_events[-1]
                        got = True
                        break
                time.sleep(0.002)
            if got:
                rtts_ms.append((t1 - t0) * 1000.0)
            time.sleep(0.05)  # don't hammer faster than KEEPALIVE-scale traffic

        # -- (b)/(c) stream fps: just listen ------------------------------
        log(f"streaming {stream_s:.1f}s for ANALOG/LIDAR fps...")
        with col.lock:
            col.analog_times.clear()
            col.lidar_times.clear()
        time.sleep(stream_s)
        with col.lock:
            analog_fps = _fps_from_times(col.analog_times)
            lidar_fps = _fps_from_times(col.lidar_times)
            fw_ver = col.fw_ver

        # -- (d) LED apply ack timing (fw5+) --------------------------------
        led_report = {"supported": False, "reason": None}
        if fw_ver is not None and fw_ver >= 5:
            log(f"LED ack: {led_samples} LEDS_FULL + {led_samples} LEDS_PARTIAL frames...")
            with col.lock:
                col.ack_full_ms.clear()
                col.ack_partial_ms.clear()

            off = [(0, 0, 0)] * 64
            on = [(0, 0, 0)] * 64
            for i in range(led_samples):
                on[:] = [(0, 0, 0)] * 64
                on[i % 64] = (255, 255, 255)
                link.send_leds_full(on)
                time.sleep(0.01)
            for i in range(led_samples):
                link.send_leds_partial([(i % 8, (i // 8) % 8, 0, 255, 0)])
                time.sleep(0.01)
            time.sleep(0.1)  # let trailing ACKs land
            link.send_clear()

            with col.lock:
                led_report = {
                    "supported": True,
                    "leds_full_ms": _stats(list(col.ack_full_ms)),
                    "leds_partial_ms": _stats(list(col.ack_partial_ms)),
                }
        else:
            led_report["reason"] = (
                f"fw_ver={fw_ver} < 5 (no TYPE_ACK 0x15)" if fw_ver is not None
                else "no HELLO seen yet (fw_ver unknown)"
            )

        return {
            "fw_ver": fw_ver,
            "link_rtt_ms": _stats(rtts_ms),
            "analog_fps": round(analog_fps, 1) if analog_fps else None,
            "lidar_fps": round(lidar_fps, 1) if lidar_fps else None,
            "led_apply": led_report,
        }
    finally:
        col.detach()


def _print_report(r: dict) -> None:
    print(f"fw_ver: {r['fw_ver']}")
    print(f"link RTT (ms):    {r['link_rtt_ms']}")
    print(f"analog fps:       {r['analog_fps']}")
    print(f"lidar fps:        {r['lidar_fps']}")
    led = r["led_apply"]
    if led.get("supported"):
        print(f"LED apply LEDS_FULL (ms):    {led['leds_full_ms']}")
        print(f"LED apply LEDS_PARTIAL (ms): {led['leds_partial_ms']}")
    else:
        print(f"LED apply: not supported ({led.get('reason')})")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default=None, help="serial port (default: auto-detect)")
    ap.add_argument("--rtt-samples", type=int, default=DEFAULT_RTT_SAMPLES)
    ap.add_argument("--led-samples", type=int, default=DEFAULT_LED_SAMPLES)
    ap.add_argument("--stream-s", type=float, default=DEFAULT_STREAM_S)
    ap.add_argument("--json", action="store_true", help="print JSON instead of text")
    args = ap.parse_args()

    import glob

    port = args.port
    if port is None:
        candidates = (
            sorted(glob.glob("/dev/serial/by-id/usb-Silicon_Labs_CP2102*"))
            + sorted(glob.glob("/dev/ttyUSB*"))
            + sorted(glob.glob("/dev/tty.usbserial*"))
        )
        if not candidates:
            print("no serial port found — pass --port, and make sure "
                  "neuralgrid-monitor is stopped first "
                  "(sudo systemctl stop neuralgrid-monitor)", file=sys.stderr)
            return 1
        port = candidates[0]

    print(f"opening {port} (make sure neuralgrid-monitor is stopped!)...")
    link = LidarGridLink(port)
    link.start()
    link.send_ping()
    time.sleep(1.5)  # let HELLO arrive so fw_ver is known before the probe starts

    try:
        report = run(
            link,
            rtt_samples=args.rtt_samples,
            led_samples=args.led_samples,
            stream_s=args.stream_s,
            log=lambda m: print(f"  {m}"),
        )
    finally:
        link.stop()

    if args.json:
        import json
        print(json.dumps(report, indent=2))
    else:
        _print_report(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
