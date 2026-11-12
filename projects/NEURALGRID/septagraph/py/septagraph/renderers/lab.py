"""Machine Lab renderer (plan Task 3.2).

A **catalog** renderer that turns septagraph ``/sept/v1/hit`` messages into
the CalArts Machine Lab's own OSC vocabulary and sends them to the lab's
listener on the MTIID Mac mini (default ``192.168.0.103:8001``).

**Read-only towards the Lab.** This module only *sends* OSC packets to the
lab's already-running listener, exactly as a composer would from a laptop —
never SSHes into the mini, never restarts lab processes, never touches
firmware. See ``neuralGrid/septagraph/py/renderers/LAB_OSC_CONTRACT.md`` for
the underlying instrument argument grammar (ground truth, read-only survey)
and ``neuralGrid/septagraph/py/renderers/lab_map.json`` for the catalog data
this module loads at runtime — both files are owned by a separate task and
are read here, never written.

Data flow:

- ``/sept/v1/hit`` onset=1  -> :meth:`LabRenderer.choose` picks a catalog
  point -> :func:`lab_osc` builds the lab's own message -> ``sender.send``.
- ``/sept/v1/hit`` onset=0  -> nothing (robots have no sustain in v1).
- ``/sept/v1/move``         -> ignored (robots have no continuous control).
- ``/sept/v1/manifest/request`` -> :meth:`LabRenderer.announce`.
"""
from __future__ import annotations

import argparse
import json
import random
import threading
import time
from typing import Callable, Optional, Sequence

from pythonosc.osc_message import OscMessage
from pythonosc.osc_message_builder import OscMessageBuilder

from ..axes import clamp01
from ..codec import encode_manifest
from ..manifest import ManifestStore
from ..net import CTL_PORT, RENDER_PORT, Receiver, Sender
from ..types import CatalogPoint, Hit, Manifest, ManifestRequest, Move, ZERO, vec

__all__ = ["LabRenderer", "lab_osc", "load_lab_map", "main"]

DEFAULT_LAB_PORT = 8001

# -- liveness (GOR python-server contract: /ping -> /pong gor_server <uptime>) --
PING_INTERVAL_S = 2.0   # how often LabRenderer sends /ping while liveness is running
PONG_TIMEOUT_S = 5.0    # lab_reachable is False once the last /pong is older than this
_LIVENESS_POLL_S = 0.05  # tick rate of the liveness thread's recv-poll loop


def _build_ping() -> bytes:
    return OscMessageBuilder(address="/ping").build().dgram


def _parse_arg_token(token: str) -> tuple[str, str, Optional[str]]:
    """Split one arg-template token into ``(kind, name, const)``.

    ``"i:note=36"`` -> ``("i", "note", "36")``; ``"i:vel"`` -> ``("i", "vel", None)``.
    """
    kind, rest = token.split(":", 1)
    if "=" in rest:
        name, const = rest.split("=", 1)
    else:
        name, const = rest, None
    return kind, name, const


def _scale_velocity_int(velocity: float, vel_min: float, vel_max: float, vel_range: Sequence[float]) -> int:
    """Scale a 0-1 hit velocity into ``[vel_min, vel_max]`` then linearly into ``vel_range``, cast to int."""
    v = clamp01(velocity)
    scaled = vel_min + v * (vel_max - vel_min)
    lo, hi = vel_range
    return int(round(lo + scaled * (hi - lo)))


def _scale_velocity_float(velocity: float, vel_min: float, vel_max: float) -> float:
    """Scale a 0-1 hit velocity into ``[vel_min, vel_max]`` as a float (no vel_range mapping)."""
    v = clamp01(velocity)
    return vel_min + v * (vel_max - vel_min)


def lab_osc(
    point: dict,
    hit: Hit,
    vel_mode: str = "hit",
    vel_random: Sequence[int] = (70, 110),
    rng: Optional[random.Random] = None,
) -> bytes:
    """Build the Lab's own OSC message for ``point`` (a raw ``lab_map.json`` point dict) and ``hit``.

    This is NOT a septagraph message: it speaks whatever grammar
    ``point["osc"]["address"]`` / ``point["osc"]["args"]`` declares (see
    ``LAB_OSC_CONTRACT.md``). Arg tokens: ``"i:name=CONST"`` / ``"f:name=CONST"``
    / ``"s:name=CONST"`` are constants baked into the template; ``"f:vel"`` is
    always the hit's velocity, scaled per ``vel_min``/``vel_max``.

    ``"i:vel"`` depends on ``vel_mode``: ``"hit"`` (the default here, used by
    direct callers/tests) scales the hit's velocity per ``vel_min``/``vel_max``
    then maps it into ``vel_range`` and casts to int (the original behaviour).
    ``"random"`` ignores the hit's velocity and every point's
    ``vel_range``/``vel_min``/``vel_max`` entirely, sending
    ``rng.randint(*vel_random)`` instead (``rng`` defaults to the module-level
    :mod:`random` when not given) -- this is :class:`LabRenderer`'s own
    default, since real per-hit velocity data isn't available yet.
    """
    osc = point["osc"]
    address = osc["address"]
    args = osc.get("args", [])
    vel_min = float(point.get("vel_min", 0.0))
    vel_max = float(point.get("vel_max", 1.0))
    vel_range = point.get("vel_range", [0, 127])
    rnd = rng if rng is not None else random

    builder = OscMessageBuilder(address=address)
    # Per-point note cycling (e.g. breakBot cymbal alternates 11/12 on every
    # hit): "note_cycle": [n1, n2, ...] replaces the constant "note" arg,
    # advancing one step per strike. State lives on the point dict itself.
    cycle = point.get("note_cycle")
    cycled_note = None
    if cycle:
        k = int(point.get("_cycle_pos", 0)) % len(cycle)
        cycled_note = int(cycle[k])
        point["_cycle_pos"] = (k + 1) % len(cycle)
    for token in args:
        kind, name, const = _parse_arg_token(token)
        if name == "note" and cycled_note is not None and kind == "i":
            builder.add_arg(cycled_note, arg_type="i")
            continue
        if name == "vel" and const is None:
            if kind == "i":
                if point.get("vel_fixed") is not None:
                    # per-point override (e.g. breakBot always at max) --
                    # beats every velocity mode
                    builder.add_arg(int(point["vel_fixed"]), arg_type="i")
                elif vel_mode == "random":
                    lo, hi = vel_random
                    builder.add_arg(rnd.randint(lo, hi), arg_type="i")
                else:
                    builder.add_arg(_scale_velocity_int(hit.velocity, vel_min, vel_max, vel_range), arg_type="i")
            elif kind == "f":
                builder.add_arg(_scale_velocity_float(hit.velocity, vel_min, vel_max), arg_type="f")
            else:
                raise ValueError(f"unsupported vel arg kind {kind!r} in {token!r}")
        elif kind == "i":
            builder.add_arg(int(const), arg_type="i")
        elif kind == "f":
            builder.add_arg(float(const), arg_type="f")
        elif kind == "s":
            builder.add_arg(str(const), arg_type="s")
        else:
            raise ValueError(f"unsupported arg token {token!r}")
    return builder.build().dgram


def load_lab_map(path) -> tuple[Manifest, dict[int, dict]]:
    """Load a ``lab_map.json``-shaped file into ``(Manifest, {point_id: raw dict})``.

    Only points with ``"enabled": true`` (the default when the key is
    absent) appear in the returned :class:`~septagraph.types.Manifest` —
    disabled points are excluded from selection and from the announced
    catalog, per plan Task 3.2. The raw dict keeps every point (enabled or
    not) so callers can still look up the OSC template for a point_id;
    :class:`LabRenderer` itself only ever selects/strikes enabled points.
    """
    with open(path) as f:
        data = json.load(f)

    raw_by_id: dict[int, dict] = {}
    enabled_points: list[CatalogPoint] = []
    for p in data["points"]:
        point_id = int(p["point_id"])
        raw_by_id[point_id] = p
        if not p.get("enabled", True):
            continue
        enabled_points.append(CatalogPoint(
            point_id=point_id,
            mu=vec(*p["mu"]),
            sigma=vec(*p["sigma"]),
            instrument=p["instrument"],
            max_rate_hz=float(p["max_rate_hz"]),
            vel_min=float(p.get("vel_min", 0.0)),
            vel_max=float(p.get("vel_max", 1.0)),
        ))

    manifest = Manifest(
        renderer_id=data["renderer_id"],
        kind=data.get("kind", "catalog"),
        name=data.get("name", data["renderer_id"]),
        lo=None,
        hi=None,
        points=tuple(enabled_points),
    )
    return manifest, raw_by_id


class LabRenderer:
    """Catalog renderer: `/sept/v1/hit` -> a Machine Lab robot trigger over UDP."""

    def __init__(
        self,
        map_path,
        sender: Sender,
        ctl_targets: list[tuple[str, int]],
        rng=random.Random,
        vel_mode: str = "random",
        vel_random: Sequence[int] = (70, 110),
    ) -> None:
        self.map_path = map_path
        self.manifest, self._raw_by_id = load_lab_map(map_path)
        self.sender = sender
        self.ctl_targets = list(ctl_targets)
        self.rng = rng() if isinstance(rng, type) else rng
        self.vel_mode = vel_mode
        self.vel_random = tuple(vel_random)
        self._store = ManifestStore()
        self._store.update(self.manifest)
        self._ctl_sender = Sender(self.ctl_targets) if self.ctl_targets else None
        self._last_strike: dict[int, float] = {}
        self._clock: Callable[[], float] = time.monotonic

        # -- liveness (/ping -> /pong gor_server <uptime>), off until
        # start_liveness() is called -- the real app (and the CLI's main())
        # call it unconditionally; tests keep it off by simply never calling
        # it, so constructing a LabRenderer never spins a background thread
        # on its own.
        self.lab_uptime: Optional[int] = None
        self._last_pong_monotonic: Optional[float] = None
        self._liveness_thread: Optional[threading.Thread] = None
        self._liveness_stop = threading.Event()

    @property
    def last_pong_age_s(self) -> Optional[float]:
        """Seconds since the last ``/pong`` was received, or ``None`` if none ever arrived."""
        if self._last_pong_monotonic is None:
            return None
        return self._clock() - self._last_pong_monotonic

    @property
    def lab_reachable(self) -> bool:
        """``True`` iff a ``/pong`` arrived within :data:`PONG_TIMEOUT_S`."""
        age = self.last_pong_age_s
        return age is not None and age < PONG_TIMEOUT_S

    def start_liveness(self) -> None:
        """Start sending ``/ping`` to the lab target every :data:`PING_INTERVAL_S`
        and parsing ``/pong gor_server <uptime>`` replies on the same (send-side)
        UDP socket. No-op if the sender can't both send and receive (e.g. a
        dry-run sender) or if liveness is already running."""
        if self._liveness_thread is not None:
            return
        if not hasattr(self.sender, "try_recv"):
            return
        self._liveness_stop.clear()
        self._liveness_thread = threading.Thread(target=self._liveness_loop, daemon=True)
        self._liveness_thread.start()

    def stop_liveness(self) -> None:
        self._liveness_stop.set()
        if self._liveness_thread is not None:
            self._liveness_thread.join(timeout=1.0)
            self._liveness_thread = None

    def _liveness_loop(self) -> None:
        last_ping = self._clock()  # defer the first /ping by one full interval
        ping_bytes = _build_ping()
        while not self._liveness_stop.is_set():
            now = self._clock()
            if now - last_ping >= PING_INTERVAL_S:
                try:
                    self.sender.send(ping_bytes)
                except Exception:
                    pass
                last_ping = now
            msg = self.sender.try_recv()
            if msg is not None:
                self._handle_pong(msg[0])
            else:
                self._liveness_stop.wait(_LIVENESS_POLL_S)

    def _handle_pong(self, data: bytes) -> None:
        """Parse one incoming datagram; sets :attr:`lab_uptime` and refreshes
        :attr:`last_pong_age_s` on a well-formed ``/pong gor_server <int>``."""
        try:
            msg = OscMessage(data)
        except Exception:
            return
        if msg.address != "/pong":
            return
        params = list(msg.params)
        if len(params) >= 2 and params[0] == "gor_server":
            try:
                self.lab_uptime = int(params[1])
            except (TypeError, ValueError):
                return
            self._last_pong_monotonic = self._clock()

    def _rate_ok(self, point: CatalogPoint) -> bool:
        last = self._last_strike.get(point.point_id)
        if last is None:
            return True
        min_interval = (1.0 / point.max_rate_hz) if point.max_rate_hz > 0 else 0.0
        return (self._clock() - last) >= min_interval

    def choose(self, hit: Hit) -> Optional[CatalogPoint]:
        """Pick a catalog point for ``hit``, or ``None`` (nothing reachable / rate-limited)."""
        points = self.manifest.points
        candidates = self._store.within_sigma(hit.mu, hit.sigma, points=points)
        if candidates:
            point = self.rng.choice(candidates)
        else:
            point = self._store.nearest(hit.mu, points=points)
        if point is None:
            return None
        if not self._rate_ok(point):
            return None
        return point

    def _strike(self, point: CatalogPoint, hit: Hit) -> None:
        raw = self._raw_by_id[point.point_id]
        self.sender.send(lab_osc(raw, hit, vel_mode=self.vel_mode, vel_random=self.vel_random, rng=self.rng))
        self._last_strike[point.point_id] = self._clock()

    def on_message(self, obj: object) -> None:
        if isinstance(obj, Hit):
            if not obj.onset:
                return  # release: nothing in v1 (robots have no sustain)
            point = self.choose(obj)
            if point is None:
                return
            self._strike(point, obj)
        elif isinstance(obj, Move):
            return  # ignored in v1: robots have no continuous control
        elif isinstance(obj, ManifestRequest):
            self.announce()
        # else: unknown message type -> ignore silently

    def announce(self) -> None:
        """Encode the enabled-points-only catalog manifest and send it to every ctl target."""
        if self._ctl_sender is None:
            return
        self._ctl_sender.send(encode_manifest(self.manifest))

    def direct_strike(self, point_id: int, velocity: float) -> bool:
        """Strike ``point_id`` directly (no coordinate math) for Direct mode. Honors the rate limit."""
        point = next((p for p in self.manifest.points if p.point_id == point_id), None)
        if point is None:
            return False
        if not self._rate_ok(point):
            return False
        hit = Hit(voice_id="direct", layer=0, onset=True, velocity=velocity, mu=point.mu, sigma=ZERO)
        self._strike(point, hit)
        return True


def _parse_host_port(s: str, default_port: int) -> tuple[str, int]:
    if ":" in s:
        host, port_s = s.rsplit(":", 1)
        return host, int(port_s)
    return s, default_port


class _DryRunSender:
    """Drop-in for :class:`~septagraph.net.Sender` that prints instead of sending."""

    def __init__(self, target: tuple[str, int]) -> None:
        self.target = target

    def send(self, packet: bytes) -> None:
        try:
            msg = OscMessage(packet)
            print(f"[dry-run] -> {self.target[0]}:{self.target[1]} {msg.address} {list(msg.params)}")
        except Exception:
            print(f"[dry-run] -> {self.target[0]}:{self.target[1]} {packet!r}")


def _print_startup_summary(manifest: Manifest) -> None:
    by_instrument: dict[str, int] = {}
    for p in manifest.points:
        by_instrument[p.instrument] = by_instrument.get(p.instrument, 0) + 1
    print(f"[lab] {manifest.name!r} ({manifest.renderer_id}): {len(manifest.points)} enabled points")
    for instrument in sorted(by_instrument):
        print(f"  {instrument}: {by_instrument[instrument]}")


def main(argv: Optional[list[str]] = None) -> None:
    parser = argparse.ArgumentParser(description="Septagraph Machine Lab renderer")
    parser.add_argument("--map", required=True, help="path to lab_map.json")
    parser.add_argument("--lab", required=True, help="HOST:PORT of the lab's OSC listener (e.g. 192.168.0.103:8001)")
    parser.add_argument("--listen", type=int, default=RENDER_PORT, help="UDP port to receive /sept/v1/* on")
    parser.add_argument(
        "--ctl", action="append", default=[],
        help=f"HOST:PORT controller to announce manifests to (repeatable, default 127.0.0.1:{CTL_PORT})",
    )
    parser.add_argument("--dry-run", action="store_true", help="print would-be lab messages instead of sending")
    parser.add_argument(
        "--vel-mode", choices=["random", "hit"], default="random",
        help="velocity policy: 'random' (default) ignores hit velocity, sends rng.randint(*--vel-random); "
             "'hit' scales the hit's own velocity (old behaviour)",
    )
    parser.add_argument(
        "--vel-random", type=int, nargs=2, default=(70, 110), metavar=("LO", "HI"),
        help="inclusive randint range for --vel-mode random (default: 70 110)",
    )
    args = parser.parse_args(argv)

    lab_target = _parse_host_port(args.lab, DEFAULT_LAB_PORT)
    ctl_targets = [_parse_host_port(c, CTL_PORT) for c in args.ctl] or [("127.0.0.1", CTL_PORT)]

    sender = _DryRunSender(lab_target) if args.dry_run else Sender([lab_target])
    renderer = LabRenderer(args.map, sender, ctl_targets, vel_mode=args.vel_mode, vel_random=tuple(args.vel_random))
    _print_startup_summary(renderer.manifest)
    renderer.announce()
    renderer.start_liveness()  # always on -- GOR python-server contract, no mode switch

    receiver = Receiver(args.listen, renderer.on_message)
    receiver.start()
    print(f"[lab] listening on :{args.listen}, target {lab_target[0]}:{lab_target[1]}"
          f"{' (dry-run)' if args.dry_run else ''}")
    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        renderer.stop_liveness()
        receiver.stop()


if __name__ == "__main__":
    main()
