"""sinks/csound.py — OSC to the engine's control-input port (127.0.0.1:7770).

Two verbs, both already implemented engine-side in midi_handler.orc's
"CONTROL INPUT TRANSPORT" section (instr 99):

    /cc <ch:int> <cc:int> <val:int>   — injected into the same kCh/kCC/kVal
        a real MIDI CC would produce; indistinguishable from the Teensy's
        own USB-MIDI mirror once it reaches handler_remap/handler_looper/
        handler_menus. Used for the Teensy's encoder/button bypass path
        (see sources/teensy.py) and for any map with target.kind == "cc".

    /p <id:int> <val:float>   — absolute write to one of the 35 expression
        destinations in handler/expr_targets.json, at full float resolution,
        via the generated ExprSet UDO. This is what makes full-resolution
        LiDAR/accelerometer/etc. data meaningful: `/cc` would still round
        through 0..127 like the pedal always has, `/p` does not.

Plain UDP, fire-and-forget — csound drains its OSC queue every k-block (see
midi_handler.orc's while loops), so there is no handshake and nothing to
block on here. That is also why this needs no dependency: it is 20-odd
lines of struct.pack (osc.py), not a client/server protocol.
"""

from __future__ import annotations

import socket

from ..osc import encode_osc

HOST_DEFAULT = "127.0.0.1"
PORT_DEFAULT = 7770


class CsoundSink:
    def __init__(self, host: str = HOST_DEFAULT, port: int = PORT_DEFAULT):
        self.addr = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send_cc(self, ch: int, cc: int, val: int) -> None:
        val = max(0, min(127, int(round(val))))
        self.sock.sendto(encode_osc("/cc", int(ch), int(cc), val), self.addr)

    def send_p(self, id_: int, val: float) -> None:
        self.sock.sendto(encode_osc("/p", int(id_), float(val)), self.addr)

    def send(self, target: dict, value: float) -> None:
        kind = target.get("kind")
        if kind == "expr":
            self.send_p(target["id"], value)
        elif kind == "cc":
            # "norm": the target is a registered POOL parameter, so `value` is the
            # pool's 0..1 and this last-inch adapter makes it the 0..127 the engine's
            # /cc expects. Legacy {"sink":"csound","kind":"cc"} targets (no "norm")
            # keep their own 0..127 range, unchanged.
            if target.get("norm"):
                value = value * 127.0
            self.send_cc(target.get("ch", 1), target["cc"], value)
        else:
            raise ValueError(f"unknown target kind: {kind!r}")


class DryRunSink:
    """Stand-in for --dry-run: prints instead of sending. Same interface, so
    daemon.py wires it in exactly where a real sink would go."""

    def __init__(self, label: str = "csound"):
        self.label = label

    def send(self, target: dict, value: float) -> None:
        print(f"[dry-run:{self.label}] {target} <- {value!r}", flush=True)

    # Mirrors CsoundSink's extra methods so the control-CC bypass path in
    # daemon.py can call send_cc() uniformly regardless of --dry-run.
    def send_cc(self, ch: int, cc: int, val: int) -> None:
        print(f"[dry-run:{self.label}] /cc ch={ch} cc={cc} val={val}", flush=True)

    def send_p(self, id_: int, val: float) -> None:
        print(f"[dry-run:{self.label}] /p id={id_} val={val}", flush=True)
