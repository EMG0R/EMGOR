import threading
import time

import pytest

from septagraph.codec import encode_hit, encode_manifest, encode_manifest_request
from septagraph.conformance import check_renderer
from septagraph.net import CTL_PORT, RENDER_PORT, Receiver, Sender
from septagraph.types import ZERO, Hit, Manifest, vec
from septagraph.axes import K


def _free_port() -> int:
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def test_ports_are_spec_defaults():
    assert RENDER_PORT == 7700
    assert CTL_PORT == 7701


def test_loopback_receiver_gets_hit_within_100ms():
    port = _free_port()
    received = []
    ev = threading.Event()

    def on_message(obj):
        received.append(obj)
        ev.set()

    r = Receiver(port, on_message)
    r.start()
    try:
        time.sleep(0.05)  # let the receiver thread bind
        s = Sender([("127.0.0.1", port)])
        h = Hit("grid:1", 0, True, 0.5, vec(*[0.5] * K), ZERO)
        s.send(encode_hit(h))
        assert ev.wait(0.1)
        assert len(received) == 1
        assert isinstance(received[0], Hit)
        assert received[0].voice_id == "grid:1"
    finally:
        r.stop()


class _FakeRenderer:
    """A Receiver that answers manifest/request with a manifest."""

    def __init__(self, listen_port: int, reply_port: int, answer: bool = True):
        self.answer = answer
        self.requests_seen = 0
        self.reply_port = reply_port
        self._sender = None
        self.receiver = Receiver(listen_port, self._on_message)

    def _on_message(self, obj):
        from septagraph.types import ManifestRequest
        if isinstance(obj, ManifestRequest):
            self.requests_seen += 1
            if self.answer:
                s = Sender([("127.0.0.1", self.reply_port)])
                m = Manifest("fake", "continuous", "fake-renderer", ZERO, vec(*[1.0] * K), ())
                s.send(encode_manifest(m))

    def start(self):
        self.receiver.start()

    def stop(self):
        self.receiver.stop()


def test_check_renderer_passes_against_answering_fake():
    render_port = _free_port()
    ctl_port = _free_port()
    fake = _FakeRenderer(render_port, ctl_port, answer=True)
    fake.start()
    try:
        time.sleep(0.05)
        failed = check_renderer(
            "127.0.0.1", timeout_s=1.0, render_port=render_port, ctl_port=ctl_port
        )
        assert failed == []
    finally:
        fake.stop()


def test_check_renderer_fails_against_silent_fake():
    render_port = _free_port()
    ctl_port = _free_port()
    fake = _FakeRenderer(render_port, ctl_port, answer=False)
    fake.start()
    try:
        time.sleep(0.05)
        failed = check_renderer(
            "127.0.0.1", timeout_s=0.3, render_port=render_port, ctl_port=ctl_port
        )
        assert "manifest" in failed
    finally:
        fake.stop()
