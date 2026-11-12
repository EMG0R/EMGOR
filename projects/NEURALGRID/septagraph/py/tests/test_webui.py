"""Tests for septagraph.pi.webui: stdlib HTTP+SSE plumbing.

Runs a real ThreadingHTTPServer on a loopback port so we exercise the
actual socket path, but keeps everything else fake/deterministic.
"""
from __future__ import annotations

import http.client
import json
import threading
import time

import pytest

from septagraph.pi.webui import make_handler
from http.server import ThreadingHTTPServer


@pytest.fixture
def server(tmp_path):
    static_dir = tmp_path / "static"
    static_dir.mkdir()
    (static_dir / "index.html").write_text("<html>hello</html>")
    (static_dir / "app.js").write_text("console.log(1)")

    calls = []

    def get_state():
        return {"mode": "direct_lab", "t": 1}

    def dispatch(name, payload):
        calls.append((name, payload))
        if name == "boom":
            raise ValueError("nope")
        return {"handled": name, "payload": payload}

    handler = make_handler(get_state, dispatch, static_dir)
    srv = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    port = srv.server_address[1]
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    yield port, calls
    srv.shutdown()
    srv.server_close()


def _get(port, path):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    conn.request("GET", path)
    resp = conn.getresponse()
    body = resp.read()
    conn.close()
    return resp.status, body


def _post(port, path, obj):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    body = json.dumps(obj).encode()
    conn.request("POST", path, body=body, headers={"Content-Type": "application/json"})
    resp = conn.getresponse()
    out = resp.read()
    conn.close()
    return resp.status, out


def test_index_served(server):
    port, _ = server
    status, body = _get(port, "/")
    assert status == 200
    assert b"hello" in body


def test_static_js_served(server):
    port, _ = server
    status, body = _get(port, "/app.js")
    assert status == 200
    assert b"console.log" in body


def test_events_sse_stream(server):
    port, _ = server
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    conn.request("GET", "/events")
    resp = conn.getresponse()
    assert resp.status == 200
    assert resp.getheader("Content-Type") == "text/event-stream"
    chunk = resp.fp.readline()
    assert chunk.startswith(b"data: ")
    data = json.loads(chunk[len(b"data: "):])
    assert data["mode"] == "direct_lab"
    conn.close()


def test_get_api_state_returns_full_snapshot(server):
    port, _ = server
    status, body = _get(port, "/api/state")
    assert status == 200
    assert json.loads(body) == {"mode": "direct_lab", "t": 1}


def test_post_action_dispatches(server):
    port, calls = server
    status, body = _post(port, "/api/direct/strike", {"point_id": 3})
    assert status == 200
    result = json.loads(body)
    assert result == {"handled": "direct/strike", "payload": {"point_id": 3}}
    assert calls == [("direct/strike", {"point_id": 3})]


def test_post_action_error_returns_500_json(server):
    port, _ = server
    status, body = _post(port, "/api/boom", {})
    assert status == 500
    assert "nope" in json.loads(body)["error"]


def test_unknown_get_is_404(server):
    port, _ = server
    status, _ = _get(port, "/nope")
    assert status == 404


def test_api_leds_missing_get_leds_is_404(server):
    port, _ = server
    status, body = _get(port, "/api/leds")
    assert status == 404


def test_api_leds_returns_frame_hex(tmp_path):
    static_dir = tmp_path / "static2"
    static_dir.mkdir()
    (static_dir / "index.html").write_text("<html></html>")

    def get_leds():
        frame = [[1, 2, 3]] * 64
        return {"n": 64, "hex": "010203" * 64, "frame": frame}

    handler = make_handler(lambda: {}, lambda n, p: {}, static_dir, get_leds=get_leds)
    srv = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    port = srv.server_address[1]
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    try:
        status, body = _get(port, "/api/leds")
        assert status == 200
        data = json.loads(body)
        assert data["n"] == 64
        assert len(data["frame"]) == 64
        assert data["hex"] == "010203" * 64
    finally:
        srv.shutdown()
        srv.server_close()
