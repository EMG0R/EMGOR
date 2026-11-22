#!/usr/bin/env python3
"""
control_bus.py -- the arcade pad's ear on the DEMIURGE control bus.

WHAT THIS IS FOR

The arcade console (`~/demiurge/games/arcade_supervisor.py`) has exactly one
input today: a Teensy on /dev/ttyACM0. With no board attached there is no pad,
no exit gesture, and GAMES mode refuses to start. That is the whole problem
this file solves -- and it solves it by CONSUMING the control layer DEMIURGE
already has, not by pretending a board is plugged in. There is no pseudo-
terminal here, no fake device node, and no serial byte is synthesised.

THE LAYER IT CONSUMES

`demiurge-io` conditions every input to a float 0..1 and fans it out to a list
of UDP consumers (`telemetry.py`, `--telemetry HOST:PORT`, repeatable):

    /sensor <id:string> <norm:float>     full resolution, ~20 Hz, already
                                         calibrated by sensors.json
    /cc     <ch:i> <num:i> <val:i>       7-bit, legacy, for csound

`/sensor` is the system-level bus: hardware-agnostic, float throughout, one
place (sensors.json) where response is configured. This reader is built on
`/sensor`. It accepts `/cc` as well, because that is what the shipped daemon
emits for discrete controls TODAY (see PATCH A in deploy/README.md), but `/cc`
is a fallback, never the contract -- a 7-bit ladder is not the thing to build
a new consumer on.

A BUTTON IS A SENSOR THAT LIVES NEAR 0 OR 1

The bus carries levels; a pad needs edges. Thresholding therefore happens HERE,
at the consumer, because only the consumer knows it wants a discrete event --
the bus must not quantise on anyone's behalf. The hysteresis band (0.6 up /
0.4 down) is deliberately the same one `demiurge-io`'s router.py uses for its
gate/toggle modes, so "pressed" means the same thing on both sides of the fence.

WHAT IT HANDS THE ARCADE

`PadState.frame_line()` renders the latched state as one `NEPTR2 key=value ...`
line -- the arcade supervisor's own frame vocabulary, the format its
`parse_frame()` is written against. That is a shim between two in-process
objects, not an emulated device: nothing here opens a tty, claims a vendor id,
or is visible outside this Python process. Every downstream semantic (button
roles, the per-game profiles in lib/gamepad.py, freeze/thaw, launch codes, the
jb-hold exit gesture) is left exactly where it already lives and is not
reimplemented, reinterpreted, or bypassed.

TICKING

The Teensy sends 100 frames a second whether or not anything moved, and the
arcade leans on that: the jb-hold exit gesture is a TIMER evaluated on frame
arrival, so a supervisor that only ever sees a frame when a control changes can
never notice that a held button has been held long enough. So this reader ticks
on a fixed period and re-emits the latched state, which is what the hardware
does anyway. Without that, holding the exit button on a virtual panel would do
nothing at all -- the exact "mode you cannot leave" failure mode-games.sh
exists to prevent.

Pure logic (OSC decode, thresholding, frame rendering) is separated from the
socket so it can be tested on any machine; see tests/test_control_bus.py. The
socket half needs nothing but UDP and is exercised by that suite too.
"""

from __future__ import annotations

import os
import select
import socket
import struct
import sys
import time

# ── the control fields the arcade reads out of a frame ──────────────────────
#
# Names are the firmware's (TEENSYY.ino), which is also what demiurge-io names
# its sources and what BUTTONS.md documents. Nothing is renamed on the way
# through: a control has ONE name in this system.
BUTTON_FIELDS = tuple(f"b{i}" for i in range(1, 10))    # b1..b9
ENCBTN_FIELDS = tuple(f"eb{i}" for i in range(1, 8))    # eb1..eb7
DISCRETE_FIELDS = BUTTON_FIELDS + ENCBTN_FIELDS + ("jb",)
AXIS_FIELDS = ("jx", "jy")

# `/cc` fallback: the inverse of demiurge-io's sources/teensy.py tables. Kept
# here rather than imported because this process must run with nothing from
# the demiurge-io tree importable (it is a separate deployment).
CC_TO_FIELD = {}
CC_TO_FIELD.update({60 + i: f"b{i + 1}" for i in range(9)})    # 60..68
CC_TO_FIELD.update({70 + i: f"eb{i + 1}" for i in range(7)})   # 70..76
CC_TO_FIELD[77] = "jb"
# The joystick's own CC pair, for the same fallback. 7-bit and therefore lossy
# -- which is precisely why /sensor is the primary path.
CC_AXIS = {27: "jx", 28: "jy"}

# Hysteresis on the 0..1 level -> pressed/released. Same band as router.py.
PRESS_HI = 0.6
PRESS_LO = 0.4

# The joystick's raw span, reconstructed from the normalized bus value so the
# arcade's own deadzone/rotation/inversion maths (lib/gamepad.py, driven by
# games.conf) keeps working untouched. See PadState.set_axis_norm for the
# calibration caveat.
AXIS_MAX = 4095
AXIS_CENTER = 2048

DEFAULT_PORT = int(os.environ.get("NV_GAMES_OSC_PORT", "8005"))
DEFAULT_TICK_HZ = 60.0


def log(msg: str) -> None:
    print(f"[control-bus] {msg}", file=sys.stderr, flush=True)


# ── OSC decode ──────────────────────────────────────────────────────────────

def _read_string(data: bytes, i: int):
    """OSC string at `i` -> (value, index just past its 4-byte padding).

    The padding is measured from the START of the packet, not from `i`: OSC
    aligns every field on an absolute 4-byte boundary, and padding relative to
    the field's own start is the classic off-by-a-few that decodes the first
    string fine and then walks off the end of the second one.
    """
    end = data.find(b"\x00", i)
    if end < 0:
        return None, len(data)
    s = data[i:end].decode("ascii", errors="replace")
    nxt = end + 1
    return s, nxt + (-nxt % 4)


def osc_parse(data: bytes):
    """Decode one OSC packet into [(address, [args]), ...].

    Handles the `i`, `f` and `s` tags -- the only three anything on this bus
    sends -- plus `#bundle`, flattened. Returns [] for anything malformed
    rather than raising: this is fed straight off a socket, and one bad
    datagram from a stray sender must not take the pad down mid-game.
    """
    try:
        return _osc_parse(data)
    except (struct.error, UnicodeDecodeError, IndexError, ValueError):
        return []


def _osc_parse(data: bytes):
    if data.startswith(b"#bundle\x00"):
        out = []
        i = 16  # 8 for "#bundle\0", 8 for the time tag
        while i + 4 <= len(data):
            (size,) = struct.unpack_from(">i", data, i)
            i += 4
            if size < 0 or i + size > len(data):
                break
            out.extend(_osc_parse(data[i:i + size]))
            i += size
        return out

    address, i = _read_string(data, 0)
    if address is None or not address.startswith("/"):
        return []
    tags, i = _read_string(data, i)
    if tags is None or not tags.startswith(","):
        return []
    args = []
    for t in tags[1:]:
        if t == "i":
            (v,) = struct.unpack_from(">i", data, i)
            args.append(v)
            i += 4
        elif t == "f":
            (v,) = struct.unpack_from(">f", data, i)
            args.append(float(v))
            i += 4
        elif t == "s":
            v, i = _read_string(data, i)
            if v is None:
                return []
            args.append(v)
        else:
            # An unknown tag makes every later argument's offset a guess.
            return []
    return [(address, args)]


# ── the latched pad state ───────────────────────────────────────────────────

class PadState:
    """Everything the arcade needs from the bus, latched, as one frame.

    Deliberately holds nothing else. It does not know what b1 MEANS -- that is
    BUTTONS.md and lib/gamepad.py's per-game profiles -- and it does not decide
    anything about games, freeze, or the exit gesture.
    """

    def __init__(self, press_hi: float = PRESS_HI, press_lo: float = PRESS_LO):
        self.press_hi = press_hi
        self.press_lo = press_lo
        # THREE levels per button, not one, because of the bug below.
        #   _level   the truth as of the last datagram
        #   _down    what frame() reports
        #   _emitted what the last frame ACTUALLY handed to the arcade
        self._level = {f: 0 for f in DISCRETE_FIELDS}
        self._down = {f: 0 for f in DISCRETE_FIELDS}
        self._emitted = {f: 0 for f in DISCRETE_FIELDS}
        self._hold = {f: False for f in DISCRETE_FIELDS}
        # Centred until told otherwise: a pad whose stick starts pinned in a
        # corner walks the menu on its own before anyone touches anything.
        self._axis = {f: AXIS_CENTER for f in AXIS_FIELDS}
        self.seen_any = False

    # -- inputs ------------------------------------------------------------
    def set_discrete_norm(self, field: str, norm: float) -> bool:
        """Threshold one 0..1 level into pressed/released. Returns True on an
        actual edge. Hysteresis, so a level parked on the boundary (a smoothed
        source, a noisy transducer) cannot chatter the button.

        AN EDGE MUST SURVIVE TO A FRAME. The bus is event-driven and this
        reader is not: several datagrams can land between two ticks, and a
        press followed by its release in that gap would net out to "nothing
        happened" and the button would never be seen pressed at all. That is
        not theoretical -- it is what a fast tap on the phone's panel does,
        and it was observed swallowing every button in the end-to-end run.

        The Teensy does not have this problem for free either; it just hides
        it, by re-sending its whole state 100 times a second so nothing is
        ever inferred from a gap. A bus consumer has to handle it, so an edge
        that has not yet been shown to the arcade is HELD for one frame before
        the newer value replaces it. One tick of latency in the rare case,
        none in the common one, and no press is ever silently dropped.
        """
        if field not in self._level:
            return False
        cur = self._level[field]
        if norm >= self.press_hi:
            new = 1
        elif norm <= self.press_lo:
            new = 0
        else:
            new = cur
        if new == cur:
            return False
        self._level[field] = new
        if self._down[field] != new:
            if self._emitted[field] == new and self._down[field] != self._emitted[field]:
                # `_down` is carrying an edge the arcade has not seen yet and
                # this datagram would erase it. Keep it for one frame instead.
                self._hold[field] = True
            else:
                self._down[field] = new
        return True

    def tick(self) -> None:
        """Advance one emitted frame. Call once per frame, before reading it.

        This is where a held edge is released and where `_emitted` catches up.
        Separated from frame() so the frame itself stays a pure read -- a
        getter with a side effect is how the held edge would get consumed
        twice by a caller that merely wanted to look.
        """
        for f in DISCRETE_FIELDS:
            if self._hold[f]:
                self._hold[f] = False        # this frame shows the held edge
            elif self._down[f] != self._level[f]:
                self._down[f] = self._level[f]
            self._emitted[f] = self._down[f]

    def set_axis_norm(self, field: str, norm: float) -> bool:
        """Reconstruct the raw 12-bit reading the arcade's own deadzone and
        rotation maths expect.

        This assumes the source's sensors.json calibration is symmetric about
        the stick's mechanical centre -- i.e. that norm 0.5 IS centre. That is
        true of the shipped default (in_lo=0, in_hi=4095) and of any sane
        hand-calibration, and an asymmetric one shows up immediately as a
        stick that drifts one way, not as something subtle.
        """
        if field not in self._axis:
            return False
        v = int(round(max(0.0, min(1.0, norm)) * AXIS_MAX))
        changed = v != self._axis[field]
        self._axis[field] = v
        return changed

    def apply_sensor(self, source_id: str, norm: float) -> bool:
        if source_id in self._axis:
            self.seen_any = True
            return self.set_axis_norm(source_id, norm)
        if source_id in self._level:
            self.seen_any = True
            return self.set_discrete_norm(source_id, norm)
        return False   # a LiDAR is not a pad control; ignore, do not guess

    def apply_cc(self, cc: int, val: int) -> bool:
        """FALLBACK ONLY -- see the module header. 7-bit, so it is decoded the
        one way a 0..127 ladder can be: /127 back to a level, then through the
        same threshold as everything else. Channel is not checked: the bus
        carries exactly one control surface's CCs today, and dropping traffic
        on a channel mismatch would be a silent, unexplainable dead pad."""
        field = CC_TO_FIELD.get(cc)
        if field is not None:
            self.seen_any = True
            return self.set_discrete_norm(field, val / 127.0)
        field = CC_AXIS.get(cc)
        if field is not None:
            self.seen_any = True
            return self.set_axis_norm(field, val / 127.0)
        return False

    def apply_message(self, address: str, args) -> bool:
        """One decoded OSC message -> state. Unknown addresses are ignored."""
        if address == "/sensor" and len(args) >= 2 \
                and isinstance(args[0], str) and isinstance(args[1], (int, float)):
            return self.apply_sensor(args[0], float(args[1]))
        if address == "/cc" and len(args) >= 3 \
                and all(isinstance(a, int) for a in args[:3]):
            return self.apply_cc(args[1], args[2])
        return False

    def apply_datagram(self, data: bytes) -> bool:
        changed = False
        for address, args in osc_parse(data):
            changed |= self.apply_message(address, args)
        return changed

    # -- output ------------------------------------------------------------
    def frame(self) -> dict:
        """The latched state as the dict `arcade_supervisor.parse_frame()`
        produces. Every field is always present: the supervisor's launch-code
        detector and lib/gamepad.py both read `frame.get(field, 0)`, and a
        field that comes and goes would make a release indistinguishable from
        a field that was simply not in this frame."""
        out = dict(self._axis)
        out.update(self._down)
        return out

    def frame_line(self) -> str:
        f = self.frame()
        return "NEPTR2 " + " ".join(f"{k}={v}" for k, v in f.items())


# ── the socket half ─────────────────────────────────────────────────────────

class BusReader:
    """A drop-in for the arcade's `SerialReader`: same `.lines()` contract,
    fed by UDP instead of a tty.

    The arcade constructs its reader in `__init__` and only calls `.lines()`
    in `run()`, so swapping the attribute (see games_console.py) is the entire
    integration -- no serial device is opened, and arcade_supervisor.py is not
    edited.
    """

    def __init__(self, port: int = DEFAULT_PORT, host: str = "127.0.0.1",
                 tick_hz: float = DEFAULT_TICK_HZ, state: PadState = None):
        self.port = int(port)
        self.host = host
        self.period = 1.0 / float(tick_hz)
        self.state = state or PadState()
        self.sock = None

    def open(self) -> None:
        if self.sock is not None:
            return
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # No SO_REUSEPORT. On Linux the kernel LOAD-BALANCES across sockets
        # bound to one port rather than duplicating, so a second consumer
        # would silently steal half the pad's input and the stick would feel
        # broken in a way nothing logs. demiurge-io's telemetry.py makes the
        # same point at length; a second consumer gets its own --telemetry
        # destination, not a shared port.
        s.bind((self.host, self.port))
        s.setblocking(False)
        self.sock = s
        log(f"listening for /sensor + /cc on {self.host}:{self.port}")

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def drain(self) -> bool:
        """Read every datagram waiting right now. Returns True if anything
        changed. Never blocks."""
        changed = False
        while True:
            try:
                data, _addr = self.sock.recvfrom(4096)
            except (BlockingIOError, InterruptedError):
                return changed
            except OSError:
                return changed
            changed |= self.state.apply_datagram(data)

    def lines(self):
        """Yield one frame line per tick, forever. See the module header for
        why this ticks rather than yielding only on change."""
        self.open()
        next_tick = time.monotonic()
        while True:
            now = time.monotonic()
            timeout = max(0.0, next_tick - now)
            try:
                select.select([self.sock], [], [], timeout)
            except (OSError, ValueError):
                # The socket was closed under us (shutdown); stop cleanly
                # rather than spinning on a dead fd.
                return
            self.drain()
            now = time.monotonic()
            if now >= next_tick:
                # Catch up rather than accumulate debt if we were descheduled.
                next_tick = now + self.period
                # tick() first: it is what releases an edge that arrived and
                # reversed since the last frame. See set_discrete_norm.
                self.state.tick()
                yield self.state.frame_line()


def main(argv=None) -> int:
    """Standalone monitor: print the frame line whenever the pad state moves.

    This is the thing to run when a virtual button "does nothing" -- it says
    whether the press is reaching this side of the bus at all, which splits
    the problem cleanly in two (panel/demiurge-io vs arcade)."""
    argv = list(sys.argv[1:] if argv is None else argv)
    port = int(argv[0]) if argv else DEFAULT_PORT
    reader = BusReader(port=port)
    prev = None
    try:
        for line in reader.lines():
            if line != prev:
                print(line, flush=True)
                prev = line
    except KeyboardInterrupt:
        pass
    finally:
        reader.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
