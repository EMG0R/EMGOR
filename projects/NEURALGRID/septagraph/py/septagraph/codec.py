"""OSC encode/decode for the septagraph wire grammar (spec §B).

Every address begins ``/sept/v1/``. Type tags: ``s`` string, ``i`` int32,
``f`` float32, arrays of 8 floats as 8 consecutive ``f``. Encoding uses
``pythonosc.osc_message_builder`` / ``osc_bundle_builder``; decoding uses
``pythonosc.osc_packet.OscPacket`` so bundles flatten transparently.

Manifests are multi-message (begin, bounds|point..., end) and only
resolve to a single :class:`~septagraph.types.Manifest` once ``end``
arrives — see :class:`Decoder`. The stateless :func:`decode` uses a
scratch ``Decoder`` per call, so a manifest split across *separate*
packets will not resolve via ``decode`` alone; use ``Decoder.feed``
across packets for that (see ``manifest/request`` handling and
``test_manifest_across_packets_and_begin_invalidates``).
"""
from __future__ import annotations

from typing import Union

from pythonosc.osc_bundle_builder import IMMEDIATELY, OscBundleBuilder
from pythonosc.osc_message_builder import OscMessageBuilder
from pythonosc.osc_packet import OscPacket

from .axes import K, clamp01
from .types import (
    CatalogPoint, Hit, Knot, Manifest, ManifestRequest, Move, Septagraph, vec,
)

ADDR_HIT = "/sept/v1/hit"
ADDR_MOVE = "/sept/v1/move"
ADDR_SHAPE = "/sept/v1/shape"
ADDR_MANIFEST_BEGIN = "/sept/v1/manifest/begin"
ADDR_MANIFEST_BOUNDS = "/sept/v1/manifest/bounds"
ADDR_MANIFEST_POINT = "/sept/v1/manifest/point"
ADDR_MANIFEST_END = "/sept/v1/manifest/end"
ADDR_MANIFEST_REQUEST = "/sept/v1/manifest/request"

Decoded = Union[Hit, Move, Septagraph, Manifest, ManifestRequest]


def _msg(address: str, *args) -> OscMessageBuilder:
    b = OscMessageBuilder(address=address)
    for a in args:
        if isinstance(a, tuple):
            b.add_arg(a[1], arg_type=a[0])
        else:
            b.add_arg(a)
    return b


def _add_vec8(builder: OscMessageBuilder, v) -> None:
    for x in v:
        builder.add_arg(float(x), arg_type="f")


def encode_hit(h: Hit) -> bytes:
    b = _msg(ADDR_HIT, h.voice_id, int(h.layer), 1 if h.onset else 0, ("f", float(h.velocity)))
    _add_vec8(b, h.mu)
    _add_vec8(b, h.sigma)
    return b.build().dgram


def encode_move(m: Move) -> bytes:
    b = _msg(ADDR_MOVE, m.voice_id)
    _add_vec8(b, m.mu)
    return b.build().dgram


def encode_shape(s: Septagraph) -> bytes:
    b = _msg(ADDR_SHAPE, s.shape_id, int(s.layer), ("f", float(s.duration_s)), len(s.knots))
    for knot in s.knots:
        b.add_arg(float(knot.t), arg_type="f")
        _add_vec8(b, knot.mu)
        _add_vec8(b, knot.sigma)
    return b.build().dgram


def encode_manifest_request() -> bytes:
    return _msg(ADDR_MANIFEST_REQUEST).build().dgram


def encode_manifest(m: Manifest) -> bytes:
    """One OSC bundle: begin, bounds|points..., end (spec §B)."""
    bundle = OscBundleBuilder(IMMEDIATELY)

    n_points = len(m.points)
    begin = _msg(ADDR_MANIFEST_BEGIN, m.renderer_id, m.kind, n_points, m.name)
    bundle.add_content(begin.build())

    if m.kind == "continuous":
        bounds = _msg(ADDR_MANIFEST_BOUNDS, m.renderer_id)
        _add_vec8(bounds, m.lo if m.lo is not None else [0.0] * K)
        _add_vec8(bounds, m.hi if m.hi is not None else [1.0] * K)
        bundle.add_content(bounds.build())
    else:
        for p in m.points:
            pt = _msg(ADDR_MANIFEST_POINT, m.renderer_id, int(p.point_id))
            _add_vec8(pt, p.mu)
            _add_vec8(pt, p.sigma)
            pt.add_arg(p.instrument)
            pt.add_arg(float(p.max_rate_hz), arg_type="f")
            pt.add_arg(float(p.vel_min), arg_type="f")
            pt.add_arg(float(p.vel_max), arg_type="f")
            bundle.add_content(pt.build())

    end = _msg(ADDR_MANIFEST_END, m.renderer_id)
    bundle.add_content(end.build())

    return bundle.build().dgram


def _read_vec8(args, i: int):
    return vec(*[clamp01(float(x)) for x in args[i:i + K]]), i + K


class Decoder:
    """Stateful decoder: accumulates manifest begin/point/end across packets.

    A new ``begin`` for a ``renderer_id`` discards any in-progress partial
    manifest for that id (spec §B: "a `begin` invalidates that
    `renderer_id`'s previous manifest").
    """

    def __init__(self) -> None:
        self._pending: dict[str, dict] = {}

    def _decode_message(self, msg) -> list[Decoded]:
        addr = msg.address
        args = msg.params
        out: list[Decoded] = []

        try:
            if addr == ADDR_HIT:
                voice_id, layer, onset, velocity = args[0], args[1], args[2], args[3]
                mu, i = _read_vec8(args, 4)
                sigma, i = _read_vec8(args, i)
                out.append(Hit(str(voice_id), int(layer), bool(onset), float(velocity), mu, sigma))

            elif addr == ADDR_MOVE:
                voice_id = args[0]
                mu, _ = _read_vec8(args, 1)
                out.append(Move(str(voice_id), mu))

            elif addr == ADDR_SHAPE:
                shape_id, layer, duration_s, n_knots = args[0], args[1], args[2], args[3]
                knots = []
                i = 4
                for _ in range(int(n_knots)):
                    t = args[i]
                    i += 1
                    mu, i = _read_vec8(args, i)
                    sigma, i = _read_vec8(args, i)
                    knots.append(Knot(float(t), mu, sigma))
                out.append(Septagraph(str(shape_id), int(layer), float(duration_s), tuple(knots)))

            elif addr == ADDR_MANIFEST_REQUEST:
                out.append(ManifestRequest())

            elif addr == ADDR_MANIFEST_BEGIN:
                renderer_id, kind, n_points, name = args[0], args[1], args[2], args[3]
                self._pending[str(renderer_id)] = {
                    "kind": str(kind), "name": str(name), "n_points": int(n_points),
                    "lo": None, "hi": None, "points": [],
                }

            elif addr == ADDR_MANIFEST_BOUNDS:
                renderer_id = str(args[0])
                st = self._pending.get(renderer_id)
                if st is not None:
                    lo, i = _read_vec8(args, 1)
                    hi, _ = _read_vec8(args, i)
                    st["lo"] = lo
                    st["hi"] = hi

            elif addr == ADDR_MANIFEST_POINT:
                renderer_id = str(args[0])
                st = self._pending.get(renderer_id)
                if st is not None:
                    point_id = int(args[1])
                    mu, i = _read_vec8(args, 2)
                    sigma, i = _read_vec8(args, i)
                    instrument = str(args[i]); i += 1
                    max_rate_hz = float(args[i]); i += 1
                    vel_min = float(args[i]); i += 1
                    vel_max = float(args[i]); i += 1
                    st["points"].append(
                        CatalogPoint(point_id, mu, sigma, instrument, max_rate_hz, vel_min, vel_max)
                    )

            elif addr == ADDR_MANIFEST_END:
                renderer_id = str(args[0])
                st = self._pending.pop(renderer_id, None)
                if st is not None:
                    out.append(Manifest(
                        renderer_id, st["kind"], st["name"], st["lo"], st["hi"],
                        tuple(st["points"]),
                    ))

            # else: unknown /sept/v1/* address -> ignore silently (conformance #1)

        except (IndexError, TypeError, ValueError):
            # Malformed message for a known address: ignore rather than crash.
            return []

        return out

    def feed(self, packet: bytes) -> list[Decoded]:
        out: list[Decoded] = []
        try:
            osc_packet = OscPacket(packet)
        except Exception:
            return out
        for timed_msg in osc_packet.messages:
            msg = timed_msg.message
            if not msg.address.startswith("/sept/v1/"):
                continue
            out.extend(self._decode_message(msg))
        return out


def decode(packet: bytes) -> list[Decoded]:
    """Stateless decode of a single packet (bundle or message).

    A manifest bundle (begin+bounds/points+end all in one packet, as
    produced by :func:`encode_manifest`) resolves in one call. A manifest
    split across separate packets requires a persistent :class:`Decoder`.
    """
    return Decoder().feed(packet)
