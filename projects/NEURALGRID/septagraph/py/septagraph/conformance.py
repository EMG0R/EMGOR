"""Renderer conformance checker (spec §B obligations).

Sends a fixed stream to a renderer under test and checks its manifest
arrives (and keeps arriving after the fast plane has been exercised).
Returns a list of failed obligation names; empty list = conformant.

CLI: ``python -m septagraph.conformance <host>``.
"""
from __future__ import annotations

import sys
import threading

from pythonosc.osc_message_builder import OscMessageBuilder

from .codec import encode_hit, encode_manifest_request, encode_move
from .net import CTL_PORT, RENDER_PORT, Receiver, Sender
from .types import ZERO, Hit, Manifest, Move, vec
from .axes import K


def _out_of_range_hit_packet() -> bytes:
    """A raw /sept/v1/hit with mu/sigma outside [0,1], bypassing Hit's clamp."""
    b = OscMessageBuilder(address="/sept/v1/hit")
    for a in ("conformance:oor", 0, 1, 1.0):
        b.add_arg(a)
    for _ in range(2 * K):          # mu + sigma
        b.add_arg(2.0, arg_type="f")
    return b.build().dgram


def _unknown_address_packet() -> bytes:
    b = OscMessageBuilder(address="/sept/v1/whatever")
    b.add_arg(1)
    return b.build().dgram


def check_renderer(
    host: str,
    timeout_s: float = 3.0,
    render_port: int = RENDER_PORT,
    ctl_port: int = CTL_PORT,
) -> list[str]:
    failed: list[str] = []
    manifests: list[Manifest] = []
    got_manifest = threading.Event()

    def on_message(obj: object) -> None:
        if isinstance(obj, Manifest):
            manifests.append(obj)
            got_manifest.set()

    receiver = Receiver(ctl_port, on_message)
    receiver.start()
    try:
        sender = Sender([(host, render_port)])
        try:
            sender.send(encode_manifest_request())
            if not got_manifest.wait(timeout_s):
                failed.append("manifest")
                return failed

            # Exercise the fast plane; conformant renderers must not crash
            # or stop answering (obligations #1-3).
            got_manifest.clear()
            voice = "conformance:0"
            sender.send(encode_hit(Hit(voice, 0, True, 0.6, vec(*[0.5] * K), ZERO)))
            sender.send(encode_move(Move(voice, vec(*[0.6] * K))))
            sender.send(encode_hit(Hit(voice, 0, False, 0.0, vec(*[0.5] * K), ZERO)))
            sender.send(_out_of_range_hit_packet())
            sender.send(_unknown_address_packet())

            # Obligation #4: re-announces on manifest/request.
            sender.send(encode_manifest_request())
            if not got_manifest.wait(timeout_s):
                failed.append("manifest_followup")
        finally:
            sender.close()
    finally:
        receiver.stop()

    return failed


def _main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(f"usage: python -m septagraph.conformance <host>", file=sys.stderr)
        return 2
    host = argv[1]
    failed = check_renderer(host)
    if failed:
        print(f"FAIL: {host} failed obligations: {', '.join(failed)}")
        return 1
    print(f"PASS: {host} is conformant")
    return 0


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
