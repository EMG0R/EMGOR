"""telemetry.py — the normalized input bus. Fans conditioned input out to any
number of consumers.

    /sensor <id:string> <norm:float>          continuous, decimated to ~20 Hz
    /cc     <ch:int> <num:int> <val:int>       discrete, every event

This is a SEPARATE stream from the sensor->router->csound path. That path is
about NEPTR's parameters; this one is about the input itself, already
conditioned, with nothing audio-specific left in it. Consumers so far:
demiurge_web.py's browser UI (live meters) and the video synth engine.

THE CONTRACT, AND WHY IT IS WORTH KEEPING NARROW
------------------------------------------------
`norm` is a float 0..1 produced by conditioning.py — calibration, dropout
policy, deadzone and smoothing already applied. A consumer must NOT re-scale
it, apply its own calibration, or reach for raw units. sensors.json is the one
place that behaviour is configured; a second calibration somewhere downstream
means two definitions of what "hand at 200 mm" means, and they drift.

If a consumer wants a DIFFERENT response to the same sensor, that is a second
map in router.py, not a second calibration.

`/cc` rides the same fan-out because discrete events cannot be expressed as a
normalized level. A mode selector or a menu is driven by button presses, and a
0..1 stream cannot say "the user pressed A". Note that `/p` deliberately does
NOT fan out: expression-target ids are csound parameter ids and are meaningless
to anything else.

WHY AN EXPLICIT DESTINATION LIST
--------------------------------
UDP unicast reaches exactly one socket, so a single hardcoded destination made
this bus single-consumer in practice — whoever bound the port first won and
everyone else silently received nothing.

DO NOT "fix" that with SO_REUSEPORT. On Linux the kernel LOAD-BALANCES
datagrams across sockets bound to the same port rather than duplicating them,
so two listeners would each receive roughly half the readings and both would
look subtly, intermittently broken. It is the tempting wrong answer.

Multicast would work, but for a handful of localhost consumers an explicit
list is simpler to reason about and visible to tcpdump. Adding a consumer is
one more --telemetry flag.

Never blocks: sends are fire-and-forget, and the decimation check is an O(1)
dict lookup before a packet is even built. A dropped meter update is invisible;
a stalled daemon is not.
"""

from __future__ import annotations

import socket

from .osc import encode_osc

HOST_DEFAULT = "127.0.0.1"
PORT_DEFAULT = 8002
DECIMATE_HZ = 20.0
MIN_INTERVAL = 1.0 / DECIMATE_HZ


def parse_destination(text: str) -> tuple[str, int]:
    """'host:port' or bare 'port' -> (host, port). Bare port means localhost,
    which is what every consumer is today."""
    s = text.strip()
    if ":" in s:
        host, _, port = s.rpartition(":")
        host = host.strip() or HOST_DEFAULT
    else:
        host, port = HOST_DEFAULT, s
    try:
        n = int(port)
    except ValueError:
        raise ValueError("bad telemetry destination %r (want host:port)" % text)
    if not (1 <= n <= 65535):
        raise ValueError("telemetry port out of range in %r" % text)
    return (host, n)


class TelemetryBroadcaster:
    def __init__(self, destinations=None, host: str = HOST_DEFAULT,
                 port: int = PORT_DEFAULT):
        """destinations: iterable of (host, port). Falls back to the single
        host/port pair so existing callers and tests keep working."""
        if destinations is None:
            destinations = [(host, port)]
        self.destinations = [tuple(d) for d in destinations]
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._last_sent: dict[str, float] = {}
        self._last_param: dict[str, float] = {}

    # Kept for callers that only ever had one address.
    @property
    def addr(self):
        return self.destinations[0] if self.destinations else None

    def _send(self, packet: bytes) -> None:
        for dest in self.destinations:
            try:
                self.sock.sendto(packet, dest)
            except OSError:
                # One unreachable consumer must not stop the others, and must
                # never take the daemon down.
                continue

    def publish(self, source_id: str, norm: float, now: float) -> bool:
        """Send /sensor <id> <norm> to every destination, if this source has
        not been sent in the last 1/20 s. Returns whether it actually sent."""
        last = self._last_sent.get(source_id)
        if last is not None and (now - last) < MIN_INTERVAL:
            return False
        self._last_sent[source_id] = now
        if self.destinations:
            self._send(encode_osc("/sensor", source_id, float(norm)))
        return True

    def publish_cc(self, channel: int, cc: int, value: int) -> None:
        """Send /cc <ch> <num> <val> to every destination.

        NOT decimated: these are discrete, human-timescale events and dropping
        one loses a button press rather than blurring a curve. This is what
        lets a consumer with no audio involvement — a boot-time mode selector,
        say — be driven by the arcade buttons and joystick.
        """
        if self.destinations:
            self._send(encode_osc("/cc", int(channel), int(cc), int(value)))

    def publish_param(self, path: str, value: float, now: float) -> bool:
        """Send /param <path:string> <val:float> (parameter-pool echo for UIs),
        decimated per path to the same ~20 Hz as /sensor. Opt-in at the daemon
        (--telemetry-params): existing consumers on 8002/8004 never asked for
        this verb."""
        last = self._last_param.get(path)
        if last is not None and (now - last) < MIN_INTERVAL:
            return False
        self._last_param[path] = now
        if self.destinations:
            self._send(encode_osc("/param", path, float(value)))
        return True
