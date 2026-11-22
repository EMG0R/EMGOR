#!/usr/bin/env python3
"""neuralGrid lidar + softpot streaming smoke test.

Connects to the grid_link_lidar firmware over serial (LidarGridLink — a
GridLink subclass, since GridLink itself silently drops unknown message
types like LIDAR 0x13), sends PING, then prints a live line with the four
lidar ranges (mm; '----' = 0xFFFF invalid/dead/absent), the four raw
softpot ADC values, and (fw6+) the four raw FSR ADC values.

    python3 test_lidar.py                    # auto-detect serial port
    python3 test_lidar.py --port /dev/ttyUSB1
    python3 test_lidar.py --oneline          # rewrite one status line (tty)

Also works against the ORIGINAL grid_link firmware: softpots stream, lidar
columns stay 'no LIDAR frames yet'. Ctrl-C to exit.
"""

from __future__ import annotations

import argparse
import glob
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from neuralgrid import codec, codec_lidar  # noqa: E402
from neuralgrid.link_lidar import LidarGridLink  # noqa: E402


def autodetect_port() -> str:
    """Prefer the stable by-id symlink (survives ttyUSB0->ttyUSB1 renumber),
    then fall back to raw ttyUSB/tty.usbserial nodes."""
    candidates = (
        sorted(glob.glob("/dev/serial/by-id/*"))
        + sorted(glob.glob("/dev/ttyUSB*"))
        + sorted(glob.glob("/dev/tty.usbserial*"))  # macOS
    )
    if not candidates:
        raise SystemExit(
            "no serial port found (looked in /dev/serial/by-id, /dev/ttyUSB*, "
            "/dev/tty.usbserial*) — pass --port explicitly"
        )
    return candidates[0]


class Latest:
    """Thread-safe 'most recent value of everything' snapshot."""

    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.hello = None
        self.analog = None
        self.lidar = None
        self.lidar_frames = 0
        self.analog_frames = 0
        self.frame_errors = 0

    def on_hello(self, h) -> None:
        with self.lock:
            self.hello = h
        print(
            f"\nHELLO: proto={h.proto_ver} grid={h.grid_w}x{h.grid_h} "
            f"analog={h.n_analog} fw={h.fw_ver}",
            flush=True,
        )

    def on_analog(self, a) -> None:
        with self.lock:
            self.analog = a
            self.analog_frames += 1

    def on_lidar(self, l) -> None:
        with self.lock:
            self.lidar = l
            self.lidar_frames += 1

    def on_lidardiag(self, d) -> None:
        with self.lock:
            first = getattr(self, "_diag_seen", False) is False
            self._diag_seen = True
        if first:
            names = codec_lidar.LIDAR_CHANNEL_NAMES
            alone = " ".join(
                f"{names[i]}={'YES' if d.responded_alone[i] else 'no'}"
                for i in range(4)
            )
            print(
                f"\nLIDARDIAG: responds-alone[{alone}] "
                f"zombie={'YES (XSHUT fault!)' if d.zombie else 'no'} "
                f"{'(alone-results unreliable while zombie present) ' if not d.alone_meaningful else ''}"
                f"bus-scan-acks={[hex(a) for a in d.ack_addrs] or 'none'}",
                flush=True,
            )

    def on_key(self, k) -> None:
        print(
            f"\nKEY ({k.x},{k.y}) {'press' if k.pressed else 'release'}",
            flush=True,
        )

    def on_frame_error(self, exc, frame) -> None:
        with self.lock:
            self.frame_errors += 1


def fmt_range(mm: int) -> str:
    return " ----" if mm == codec_lidar.LIDAR_INVALID else f"{mm:5d}"


def main() -> int:
    ap = argparse.ArgumentParser(description="neuralGrid lidar/softpot stream test")
    ap.add_argument("--port", default=None, help="serial port (default: auto-detect)")
    ap.add_argument("--interval", type=float, default=0.2, help="print period, seconds")
    ap.add_argument("--oneline", action="store_true",
                    help="rewrite a single status line instead of scrolling")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    print(f"opening {port} …", flush=True)

    latest = Latest()
    link = LidarGridLink(
        port,
        on_hello=latest.on_hello,
        on_key=latest.on_key,
        on_analog=latest.on_analog,
        on_lidar=latest.on_lidar,
        on_lidardiag=latest.on_lidardiag,
        on_frame_error=latest.on_frame_error,
    )
    link.start()
    link.send_ping()  # firmware answers with HELLO; also proves the down-path

    names = codec_lidar.LIDAR_CHANNEL_NAMES
    try:
        while True:
            time.sleep(args.interval)
            with latest.lock:
                lid, ana = latest.lidar, latest.analog
                lf, af, fe = latest.lidar_frames, latest.analog_frames, latest.frame_errors
            if lid is not None:
                lidar_s = "  ".join(
                    f"{names[i]}{fmt_range(lid.ranges[i])}mm" for i in range(4)
                )
            else:
                lidar_s = "no LIDAR frames yet (old firmware?)"
            if ana is not None:
                ana_s = " ".join(
                    f"{names[i]}={v:4d}" for i, v in enumerate(ana.values[:4])
                )
                # fw6: channels 4-7 are the FSRs, if the firmware sends them
                fsr_vals = ana.values[4:8]
                fsr_s = (
                    " ".join(f"FSR{i}={v:4d}" for i, v in enumerate(fsr_vals))
                    if fsr_vals else "none (fw < 6)"
                )
            else:
                ana_s = "no ANALOG frames yet"
                fsr_s = "no ANALOG frames yet"
            line = (f"lidar[{lidar_s}]  softpot[{ana_s}]  fsr[{fsr_s}]  "
                    f"frames(lidar={lf} analog={af} err={fe})")
            if args.oneline and sys.stdout.isatty():
                print("\r\x1b[2K" + line, end="", flush=True)
            else:
                print(line, flush=True)
    except KeyboardInterrupt:
        print("\nbye")
    finally:
        link.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
