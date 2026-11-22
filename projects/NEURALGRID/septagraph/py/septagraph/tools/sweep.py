"""Axis sweep: the "space validation round-trip" for a renderer under test.

Holds one voice and, for each of the 8 coordinate indices (spec
``AXIS_SEMANTICS_v1.md``), sweeps that index from 0 to 1 over 4 seconds
while every other index sits at 0.5, then releases and moves to the next
index. Point this at any conformant renderer (native ChucK, WebChucK, a
future neural renderer, ...) and listen: each axis should produce an
audibly distinct, continuous change with nothing else moving.

Usage::

    python -m septagraph.tools.sweep [--host 127.0.0.1] [--port 7700]
        [--duration 4.0] [--hz 50] [--axes freq,loud,...]

With no ``--axes``, sweeps all 8 in order (freq, loud, harm, rate, scale,
az, spread, dist).
"""
from __future__ import annotations

import argparse
import time

from ..axes import AXES
from ..codec import encode_hit, encode_move
from ..net import RENDER_PORT, Sender
from ..types import ZERO, Hit, Move, vec
from ..axes import K

VOICE_ID = "sweep:0"


def sweep_axis(sender: Sender, idx: int, duration_s: float, hz: float) -> None:
    base = [0.5] * K
    base[idx] = 0.0
    mu0 = vec(*base)

    sender.send(encode_hit(Hit(VOICE_ID, 0, True, 0.8, mu0, ZERO)))

    step_dt = 1.0 / hz
    t0 = time.monotonic()
    while True:
        elapsed = time.monotonic() - t0
        frac = min(1.0, elapsed / duration_s)
        cur = [0.5] * K
        cur[idx] = frac
        sender.send(encode_move(Move(VOICE_ID, vec(*cur))))
        if frac >= 1.0:
            break
        time.sleep(step_dt)

    sender.send(encode_hit(Hit(VOICE_ID, 0, False, 0.0, vec(*cur), ZERO)))
    time.sleep(0.15)  # let the 80ms release tail finish before the next axis


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=RENDER_PORT)
    p.add_argument("--duration", type=float, default=4.0, help="seconds per axis")
    p.add_argument("--hz", type=float, default=50.0, help="/move update rate")
    p.add_argument(
        "--axes",
        default=",".join(AXES),
        help="comma-separated axis names to sweep, in order",
    )
    args = p.parse_args(argv)

    axis_names = [a.strip() for a in args.axes.split(",") if a.strip()]
    for name in axis_names:
        if name not in AXES:
            p.error(f"unknown axis {name!r}; choices are {', '.join(AXES)}")

    sender = Sender([(args.host, args.port)])
    try:
        for name in axis_names:
            idx = AXES.index(name)
            print(f"[sweep] {name} (idx {idx}) 0 -> 1 over {args.duration}s ...")
            sweep_axis(sender, idx, args.duration, args.hz)
        print("[sweep] done.")
    finally:
        sender.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
