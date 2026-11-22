"""Canvas taps: self-expiring UDP subscriptions that stream the ring canvas
to an external consumer (first consumer: GORMAPPER, a projection-mapping
app that wants a live copy of ring[offset..offset+count] to reproject).

A tap is NOT a strip/satellite node — it has no entry in nodes.json, no
mDNS, no firmware. It is a lightweight, presence-based subscription:

  POST /taps/{name}   {port, fps_divisor?, ttl?, offset?, count?}
                       create (port required) or keepalive (port optional
                       once created) + refresh TTL. The destination HOST is
                       ALWAYS derived from the HTTP request's source IP
                       (never from the body) -- see api.py.
  DELETE /taps/{name}  (or POST {"enabled": false}) immediate disable.
  GET /taps            list every configured tap (enabled or not).

TTL/presence: each tap has a `ttl` seconds (default DEFAULT_TTL). Every
keepalive POST refreshes `expires_at = now + ttl`. If no keepalive arrives
in time, the tap self-disables the next time the engine checks (cheap: a
monotonic-clock comparison in TapManager.emit(), no background timer) and
the engine skips all slice/send work for it from then on.

Persistence: only CONFIG (port/fps_divisor/ttl/offset/count, keyed by
name) is persisted, via StateStore's own top-level "taps" key (see
persist.py) -- never `enabled` or `host`. After a restart every
previously-configured tap exists again but starts disabled; a live
keepalive re-enables it and re-derives `host` from that request.

Wire protocol: Frame Protocol v1 (see protocol.py), chunked to
TAP_CHUNK_PIXELS (<=480) pixels per datagram so a full 1920px ring frame
is 4 datagrams of ~1446 bytes each (under a typical WiFi MTU). Each chunk
carries its own correct absolute ring offset/count; the SHOW flag (bit0)
is set ONLY on the last chunk of each frame, matching the strip firmware's
"latch on SHOW" semantics.

Semantic side-channel (optional, see semantic.py / docs/TAPS_DESIGN.md):
a tap that also sets `semantic_port` gets ONE compact JSON UDP datagram
(scene stats + top-8 particle tracks + the scene's verbatim "projector"
block) at 10Hz -- every SEMANTIC_FRAME_DIVISOR-th engine frame -- sent to
(the same derived host, semantic_port), with the same TTL/presence rules
as the pixel stream. `semantic_port` is validated (1..65535) and
persisted alongside port/fps_divisor/ttl/offset/count. Building the
packet is skipped entirely on frames that don't need it and when no
enabled tap has a semantic_port set.
"""
from __future__ import annotations

import json
import socket
import time
from dataclasses import dataclass, field

from . import semantic
from .protocol import build_packet

DEFAULT_TTL = 15            # seconds
DEFAULT_FPS_DIVISOR = 1
TAP_CHUNK_PIXELS = 480      # <=480px per datagram (spec: WiFi-MTU-safe)
SEMANTIC_FRAME_DIVISOR = 3  # 30fps engine -> ~10Hz semantic packets

MIN_PORT, MAX_PORT = 1, 65535
MIN_TTL, MAX_TTL = 1, 3600


@dataclass
class TapRuntime:
    name: str
    port: int
    fps_divisor: int = DEFAULT_FPS_DIVISOR
    ttl: int = DEFAULT_TTL
    offset: int = 0
    count: int = 1920
    semantic_port: int | None = None  # optional: enables the semantic side-channel
    host: str | None = None       # never persisted; re-derived per keepalive
    enabled: bool = False
    expires_at: float = 0.0       # monotonic seconds; only meaningful if enabled
    frames_sent: int = 0
    frame_counter: int = 0        # counts emit() calls while enabled, for fps_divisor

    def as_status(self) -> dict:
        remaining = max(0.0, self.expires_at - time.monotonic()) if self.enabled else 0.0
        return {
            "name": self.name, "host": self.host, "port": self.port,
            "offset": self.offset, "count": self.count,
            "fps_divisor": self.fps_divisor, "ttl": self.ttl,
            "semantic_port": self.semantic_port,
            "enabled": self.enabled,
            "seconds_until_expiry": round(remaining, 1),
            "frames_sent": self.frames_sent,
        }


class TapManager:
    """Owns every configured tap for one RingEngine and does the per-frame
    slice/chunk/send in `emit()`. A single shared UDP socket is used for
    all taps (raw IPs, no mDNS needed -- a plain sendto() is enough)."""

    def __init__(self, ring_len: int):
        self.ring_len = ring_len
        self.taps: dict[str, TapRuntime] = {}
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._frame_counter = 0   # counts every emit() call, for the semantic cadence
        self._semantic_seq = 0    # monotonic "seq" field in semantic packets

    # -- validation ----------------------------------------------------
    def _validate(self, port: int, fps_divisor: int, ttl: int, offset: int,
                  count: int, semantic_port: int | None = None) -> None:
        if not (MIN_PORT <= port <= MAX_PORT):
            raise ValueError(f"port {port} out of range {MIN_PORT}..{MAX_PORT}")
        if fps_divisor < 1:
            raise ValueError(f"fps_divisor {fps_divisor} must be >= 1")
        if not (MIN_TTL <= ttl <= MAX_TTL):
            raise ValueError(f"ttl {ttl} must be {MIN_TTL}..{MAX_TTL}")
        if count < 1:
            raise ValueError(f"count {count} must be >= 1")
        if offset < 0 or offset + count > self.ring_len:
            raise ValueError(f"offset {offset} + count {count} exceeds ring "
                             f"length {self.ring_len}")
        if semantic_port is not None and not (MIN_PORT <= semantic_port <= MAX_PORT):
            raise ValueError(f"semantic_port {semantic_port} out of range "
                             f"{MIN_PORT}..{MAX_PORT}")

    # -- create / keepalive / disable -----------------------------------
    def set_tap(self, name: str, host: str, port: int | None = None,
               fps_divisor: int | None = None, ttl: int | None = None,
               offset: int | None = None, count: int | None = None,
               semantic_port: int | None = None) -> dict:
        """Create (port required) or keepalive (port optional, reuses the
        prior config) a tap, refresh its TTL, and set `host` from the
        caller-supplied value (always the HTTP request's source IP --
        api.py's job, not this method's). Raises ValueError on bad params
        or a missing port on first create."""
        existing = self.taps.get(name)
        if existing is None and port is None:
            raise ValueError("port is required to create a new tap")

        port_v = port if port is not None else existing.port
        fps_v = fps_divisor if fps_divisor is not None else (
            existing.fps_divisor if existing else DEFAULT_FPS_DIVISOR)
        ttl_v = ttl if ttl is not None else (existing.ttl if existing else DEFAULT_TTL)
        offset_v = offset if offset is not None else (existing.offset if existing else 0)
        count_v = count if count is not None else (
            existing.count if existing else self.ring_len)
        semantic_v = semantic_port if semantic_port is not None else (
            existing.semantic_port if existing else None)

        self._validate(port_v, fps_v, ttl_v, offset_v, count_v, semantic_v)

        tap = existing or TapRuntime(name=name, port=port_v)
        tap.port, tap.fps_divisor, tap.ttl = port_v, fps_v, ttl_v
        tap.offset, tap.count = offset_v, count_v
        tap.semantic_port = semantic_v
        tap.host = host
        tap.enabled = True
        tap.expires_at = time.monotonic() + ttl_v
        self.taps[name] = tap
        return tap.as_status()

    def disable(self, name: str) -> None:
        tap = self.taps.get(name)
        if tap is None:
            raise KeyError(name)
        tap.enabled = False

    # -- status ----------------------------------------------------------
    def status_list(self) -> list[dict]:
        return [t.as_status() for t in self.taps.values()]

    def status_one(self, name: str) -> dict | None:
        tap = self.taps.get(name)
        return tap.as_status() if tap else None

    # -- per-frame emit ----------------------------------------------------
    def emit(self, frame, scene=None, output_delay_ms: int = 0) -> None:
        """Called once per rendered ring frame. Cheap: an expired/disabled
        tap costs one dict lookup + one comparison, no slice/send work.

        `scene` (the live SceneInstance, optional) is only needed for the
        semantic side-channel -- callers that only care about the raw
        pixel stream (and every pre-existing test) can omit it, and no
        semantic packet is ever built/sent without it. `frame` here is
        always the CURRENT (undelayed) ring frame -- taps are exempt from
        output-delay AV sync (see sync.py) so GORMAPPER always sees the
        freshest canvas. `output_delay_ms` is passed through into the
        semantic packet so GORMAPPER knows how far behind the LEDs are."""
        if not self.taps:
            return
        now = time.monotonic()
        self._frame_counter += 1
        semantic_due = (self._frame_counter % SEMANTIC_FRAME_DIVISOR == 0)
        semantic_targets: list[TapRuntime] = []
        for tap in self.taps.values():
            if not tap.enabled:
                continue
            if now >= tap.expires_at:
                tap.enabled = False
                continue
            tap.frame_counter += 1
            if tap.frame_counter % tap.fps_divisor == 0:
                self._send(tap, frame)
            if semantic_due and tap.semantic_port:
                semantic_targets.append(tap)
        if semantic_targets and scene is not None:
            self._send_semantic(semantic_targets, scene, frame, output_delay_ms)

    def _send_semantic(self, targets: list[TapRuntime], scene, frame,
                       output_delay_ms: int = 0) -> None:
        packet = semantic.build_semantic_packet(self._semantic_seq, scene, frame,
                                                output_delay_ms=output_delay_ms)
        self._semantic_seq += 1
        data = json.dumps(packet, separators=semantic.SEPARATORS).encode()
        for tap in targets:
            try:
                self._sock.sendto(data, (tap.host, tap.semantic_port))
            except OSError:
                pass  # best-effort UDP: same tolerance as the pixel stream

    def _send(self, tap: TapRuntime, frame) -> None:
        sub = frame[tap.offset:tap.offset + tap.count]
        data = sub.tobytes()
        n = tap.count
        off = 0
        try:
            while off < n:
                cnt = min(TAP_CHUNK_PIXELS, n - off)
                last = off + cnt >= n
                pkt = build_packet(tap.offset + off, data[off * 3:(off + cnt) * 3],
                                   show=last)
                self._sock.sendto(pkt, (tap.host, tap.port))
                off += cnt
            tap.frames_sent += 1
        except OSError:
            pass  # best-effort UDP: never let a bad destination kill the render loop

    # -- persistence: config only, never enabled/host -----------------------
    def config_for_persist(self) -> dict[str, dict]:
        out: dict[str, dict] = {}
        for name, t in self.taps.items():
            d = {"port": t.port, "fps_divisor": t.fps_divisor,
                "ttl": t.ttl, "offset": t.offset, "count": t.count}
            if t.semantic_port is not None:
                d["semantic_port"] = t.semantic_port
            out[name] = d
        return out

    def restore_config(self, saved: dict[str, dict]) -> None:
        """Re-create tap CONFIGS from persisted state, always disabled
        (host unknown until a live keepalive re-asserts presence). Bad/
        stale entries are skipped rather than blocking startup."""
        for name, d in saved.items():
            try:
                port = int(d["port"])
                fps_divisor = int(d.get("fps_divisor", DEFAULT_FPS_DIVISOR))
                ttl = int(d.get("ttl", DEFAULT_TTL))
                offset = int(d.get("offset", 0))
                count = int(d.get("count", self.ring_len))
                raw_sem = d.get("semantic_port")
                semantic_port = int(raw_sem) if raw_sem is not None else None
                self._validate(port, fps_divisor, ttl, offset, count, semantic_port)
            except (KeyError, TypeError, ValueError):
                continue
            self.taps[name] = TapRuntime(name=name, port=port, fps_divisor=fps_divisor,
                                         ttl=ttl, offset=offset, count=count,
                                         semantic_port=semantic_port,
                                         host=None, enabled=False)

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass
