"""Runs the dimensional app against real hardware and serves its UI.

    python3 -m septagraph.dimensional.service            # calibrate, then run
    python3 -m septagraph.dimensional.service --run      # skip to the encoder
    python3 -m septagraph.dimensional.service --fake     # no hardware at all

One process owns the serial port, so it must not run alongside
``neuralgrid-monitor`` or the older app. Stdlib only, same as monitor.py.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from .app import MODE_CALIBRATE, MODE_FSR, MODE_RUN, DimensionalApp
from .leds import led_diff

HERE = os.path.dirname(os.path.abspath(__file__))
STATIC = os.path.join(HERE, "static")
DEFAULT_PORT = 8770
DEFAULT_CALIBRATION = os.path.expanduser("~/.neuralgrid/calibration.json")
PORT_GLOB = "/dev/serial/by-id/usb-Silicon_Labs_CP2102*"
LED_HZ = 20.0


def find_port() -> str | None:
    hits = sorted(glob.glob(PORT_GLOB)) or sorted(glob.glob("/dev/ttyUSB*"))
    return hits[0] if hits else None


class Link:
    """Owns the serial link and pumps it into the app.

    Reconnects on its own, the same way monitor.py does: the ESP32 streams
    nothing until it is pinged, and a replug must not need a human.
    """

    def __init__(self, app: DimensionalApp, port: str | None = None):
        self.app = app
        self.port = port
        self._link = None
        self._stop = threading.Event()
        self._hello = None
        self._last_hello_t = 0.0
        self._reconnects = 0

    @property
    def connected(self) -> bool:
        return self._link is not None and (time.monotonic() - self._last_hello_t) < 12.0

    def start(self) -> None:
        threading.Thread(target=self._supervise, daemon=True).start()
        threading.Thread(target=self._led_loop, daemon=True).start()

    def stop(self) -> None:
        self._stop.set()
        self._close()

    def _close(self) -> None:
        if self._link is not None:
            try:
                self._link.stop()
            except Exception:
                pass
            self._link = None

    def _supervise(self) -> None:
        while not self._stop.is_set():
            if self._link is None:
                self._try_open()
            elif not self.connected:
                self._reconnects += 1
                self._close()
            else:
                try:
                    self._link.send_ping()
                except Exception:
                    self._close()
            self._stop.wait(2.0)

    def _try_open(self) -> None:
        port = self.port or find_port()
        if not port:
            return
        try:
            sys.path.insert(0, os.path.expanduser("~/neuralGrid/pi"))
            from neuralgrid.link_lidar import LidarGridLink  # type: ignore

            link = LidarGridLink(
                port,
                on_hello=self._on_hello,
                on_key=lambda k: self.app.on_key(k.x, k.y, bool(k.edge)),
                on_analog=lambda a: self.app.on_analog(a.values),
                on_lidar=lambda l: self.app.on_lidar(l.ranges),
            )
            link.start()
            # Hard reset into run mode, then ping: rescues an ESP32 stranded
            # in ROM download mode (same trick monitor.py uses).
            try:
                link._serial.setDTR(False)
                link._serial.setRTS(False)
            except Exception:
                pass
            time.sleep(0.2)
            link.send_ping()
            self._link = link
            self._last_hello_t = time.monotonic()
        except Exception as exc:  # pragma: no cover - hardware path
            print(f"[link] open failed: {exc}", file=sys.stderr)
            self._close()

    def _on_hello(self, hello) -> None:
        self._hello = hello
        self._last_hello_t = time.monotonic()

    def _led_loop(self) -> None:
        last = None
        while not self._stop.is_set():
            self._stop.wait(1.0 / LED_HZ)
            link = self._link
            if link is None:
                continue
            frame = self.app.led_frame()
            if frame == last:
                continue  # never resend an identical frame (HANDOFF §12)
            try:
                # small change -> list the pixels; big one -> whole frame.
                # A driver animating one pad should not cost 64 I2C writes.
                changed = led_diff(last, frame)
                if changed is None:
                    link.send_leds_full(frame)
                elif changed:
                    link.send_leds_partial(changed)
                last = frame
            except Exception:
                self._close()

    def state(self) -> dict:
        h = self._hello
        return {
            "connected": self.connected,
            "port": self.port or find_port(),
            "reconnects": self._reconnects,
            "fw_ver": getattr(h, "fw_ver", None),
            "n_analog": getattr(h, "n_analog", None),
        }


def make_handler(app: DimensionalApp, link: Link | None):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt_, *args):
            pass

        def _json(self, obj, status=200):
            body = json.dumps(obj).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _file(self, rel):
            path = os.path.join(STATIC, rel)
            if not os.path.isfile(path):
                self._json({"error": "not found"}, 404)
                return
            ctype = {
                ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
            }.get(os.path.splitext(path)[1], "application/octet-stream")
            with open(path, "rb") as fh:
                body = fh.read()
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = self.path.split("?")[0]
            if path == "/":
                return self._file("viz.html")
            if path in ("/setup", "/calibrate"):
                return self._file("index.html")
            if path == "/api/state":
                st = app.state()
                st["link"] = link.state() if link else {"connected": False, "port": None}
                return self._json(st)
            if path.startswith("/static/"):
                return self._file(path[len("/static/"):])
            return self._json({"error": "not found"}, 404)

        def do_POST(self):
            path = self.path.split("?")[0]
            length = int(self.headers.get("Content-Length") or 0)
            raw = self.rfile.read(length) if length else b"{}"
            try:
                body = json.loads(raw or b"{}")
            except ValueError:
                body = {}

            if path == "/api/calibrate/start":
                app.start_calibration()
            elif path == "/api/calibrate/next":
                app.calibration.advance()
                if app.calibration.complete:
                    app.finish_calibration()
            elif path == "/api/calibrate/redo":
                ch = body.get("channel")
                if ch:
                    app.calibration.goto(ch)
            elif path == "/api/calibrate/finish":
                app.finish_calibration()
            elif path == "/api/fsr/start":
                app.start_fsr_calibration()
            elif path == "/api/fsr/skip":
                if app.fsr:
                    app.fsr.skip()
            elif path == "/api/fsr/goto":
                if app.fsr and body.get("pad") is not None:
                    app.fsr.goto(int(body["pad"]))
            elif path == "/api/fsr/finish":
                app.finish_fsr_calibration()
            elif path == "/api/fsr/resume":
                app.resume_fsr_calibration()
            elif path == "/api/fsr/rest":
                app.start_rest_capture()
            elif path == "/api/leds":
                if body.get("release"):
                    app.release_leds()
                    return self._json({"ok": True, "holding": False})
                ok = app.push_leds(body.get("frame") or [])
                return self._json({"ok": ok, "holding": ok}, 200 if ok else 400)
            elif path == "/api/rotate":
                app.rotate_grid()
            elif path == "/api/flip":
                app.flip_grid(body.get("axis", "y"))
            elif path == "/api/ribbon":
                app.assign_ribbon(body.get("position", ""))
            elif path == "/api/ribbon/invert":
                app.invert_ribbon(body.get("position", ""))
            else:
                return self._json({"error": "not found"}, 404)
            return self._json({"ok": True, "mode": app.mode})

    return Handler


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--http-port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--calibration", default=DEFAULT_CALIBRATION)
    ap.add_argument("--serial-port", default=None)
    ap.add_argument("--run", action="store_true", help="boot straight into the encoder")
    ap.add_argument("--calibrate", action="store_true",
                    help="force the sensor-map program even if calibrated")
    ap.add_argument("--fsr", action="store_true", help="boot into per-pad FSR calibration")
    ap.add_argument("--fake", action="store_true", help="no serial link at all")
    args = ap.parse_args(argv)

    os.makedirs(os.path.dirname(os.path.abspath(args.calibration)), exist_ok=True)
    # No flag: let the app decide from whether a calibration file exists.
    start_mode = MODE_RUN if args.run else (MODE_CALIBRATE if args.calibrate else None)
    app = DimensionalApp(calibration_path=args.calibration, start_mode=start_mode)
    if args.fsr:
        app.start_fsr_calibration()

    link = None
    if not args.fake:
        link = Link(app, port=args.serial_port)
        link.start()

    httpd = ThreadingHTTPServer(("0.0.0.0", args.http_port), make_handler(app, link))
    print(f"[dimensional] mode={app.mode} http://0.0.0.0:{args.http_port}/", flush=True)
    print(f"[dimensional] calibration={args.calibration}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if link:
            link.stop()
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
