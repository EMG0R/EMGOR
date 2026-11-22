"""Stdlib HTTP + SSE web UI for the neuralGrid Pi app -- copies
pi/tools/monitor.py's pattern (ThreadingHTTPServer, SSE via `/events`,
no frameworks) but serves static files from septagraph/pi/static/ instead
of one inline PAGE string, since three mode panels + sequencer editor is
a lot more markup than the monitor's single view.

App-agnostic: constructed with a ``get_state`` callable (returns the full
JSON-able state snapshot) and a ``dispatch`` callable (``name, payload ->
result dict``) so app.py owns all the actual behaviour; this module only
knows HTTP/SSE/JSON plumbing.
"""
from __future__ import annotations

import json
import mimetypes
import re
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Callable

UI_FPS = 15.0


#: Only characters a hostname or IPv4 literal can contain. The Host
#: header is attacker-controlled in general, and we are writing it into
#: a Python file we hand someone to execute.
_HOST_OK = re.compile(r"^[A-Za-z0-9._-]+$")

BRIDGE_DEFAULT_HOST = "neuralgrid.local"


def bridge_script_for(host_header: str, device_port: int = 15000) -> str:
    """monome_bridge.py with this grid's address written into it.

    The bridge has to run on the computer that wants a grid (monome apps
    only ever ask their own localhost:12002 -- see monome_bridge.py), so
    the least it can do is arrive already knowing where to call.
    """
    host = (host_header or "").split(":")[0].strip("[]")
    if not _HOST_OK.match(host or ""):
        host = BRIDGE_DEFAULT_HOST
    src = (Path(__file__).parent / "monome_bridge.py").read_text()
    src = src.replace('GRID_HOST = "neuralgrid.local"', f'GRID_HOST = "{host}"')
    src = src.replace("GRID_PORT = 15000", f"GRID_PORT = {int(device_port)}")
    return src


def make_handler(
    get_state: Callable[[], dict],
    dispatch: Callable[[str, dict], dict],
    static_dir: Path,
    get_leds: Callable[[], dict] | None = None,
):
    static_dir = Path(static_dir)

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):  # quiet access log
            pass

        def _send_json(self, obj, status: int = 200) -> None:
            body = json.dumps(obj).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _send_file(self, rel_path: str) -> bool:
            path = (static_dir / rel_path).resolve()
            try:
                path.relative_to(static_dir.resolve())
            except ValueError:
                return False
            if not path.is_file():
                return False
            body = path.read_bytes()
            ctype = mimetypes.guess_type(str(path))[0] or "application/octet-stream"
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return True

        def do_GET(self):
            if self.path == "/" or self.path == "/index.html":
                if self._send_file("index.html"):
                    return
                self._send_json({"error": "index.html missing"}, 500)
                return
            if self.path == "/api/state":
                # one-shot snapshot (same JSON the SSE stream emits) -- handy
                # for `curl http://neuralgrid.local:8767/api/state | jq`
                self._send_json(get_state())
                return
            if self.path == "/api/leds":
                # debug: the last frame actually written to the wire, so the
                # physical grid can be diffed against the UI (data.grid /
                # /api/state) without eyes on the hardware.
                if get_leds is None:
                    self._send_json({"error": "leds debug endpoint not available"}, 404)
                    return
                self._send_json(get_leds())
                return
            if self.path == "/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "keep-alive")
                self.end_headers()
                interval = 1.0 / UI_FPS
                try:
                    while True:
                        snap = get_state()
                        payload = json.dumps(snap, separators=(",", ":"))
                        self.wfile.write(b"data: " + payload.encode("utf-8") + b"\n\n")
                        self.wfile.flush()
                        time.sleep(interval)
                except (BrokenPipeError, ConnectionResetError, OSError):
                    pass
                return
            if self.path == "/monome-bridge":
                body = bridge_script_for(self.headers.get("Host", "")).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/x-python")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if self.path.startswith("/app.js") or self.path.startswith("/app.css"):
                if self._send_file(self.path.lstrip("/")):
                    return
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()

        def do_POST(self):
            if not self.path.startswith("/api/"):
                self.send_response(404)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            action = self.path[len("/api/"):]
            length = int(self.headers.get("Content-Length", 0) or 0)
            raw = self.rfile.read(length) if length else b""
            try:
                payload = json.loads(raw) if raw else {}
            except json.JSONDecodeError:
                self._send_json({"error": "bad JSON"}, 400)
                return
            try:
                result = dispatch(action, payload)
            except Exception as exc:  # noqa: BLE001 -- never crash the server on a bad action
                self._send_json({"error": str(exc)}, 500)
                return
            self._send_json(result if result is not None else {"ok": True})

    return Handler


def serve_forever(
    get_state: Callable[[], dict],
    dispatch: Callable[[str, dict], dict],
    static_dir: Path,
    http_host: str = "0.0.0.0",
    http_port: int = 8767,
    get_leds: Callable[[], dict] | None = None,
) -> None:
    handler = make_handler(get_state, dispatch, static_dir, get_leds=get_leds)
    srv = ThreadingHTTPServer((http_host, http_port), handler)
    print(f"neuralGrid app UI: http://{http_host}:{http_port}/ (Ctrl-C to stop)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        srv.server_close()
