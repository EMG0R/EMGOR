"""Latency probe -> paper table (plan Task 3.4).

Three stages, 200 trials each by default, all times in milliseconds:

- ``controller`` -- fake key event -> ``GridController.on_key`` ->
  ``Sender.send``. Timed with a :class:`_TimingSender` subclass that
  timestamps at the ``sendto`` call itself (not before/after the whole
  method), isolating the fast path the plan's Global Constraints call out
  ("dict lookup + encode + sendto", target < 10 ms).
- ``wire`` -- ``Sender`` -> loopback ``Receiver`` -> decoded ``Hit``
  reaching the callback. Exercises :mod:`septagraph.codec` end to end
  over a real (loopback) UDP socket.
- ``renderer`` -- ``Sender`` -> the native ChucK reference renderer
  (``sept_render_core.ck`` + ``sept_render_osc.ck`` +
  ``sept_manifest.ck``), which prints one traced line per onset when run
  with ``sept_render_osc.ck:--trace`` (see ``septagraph/chuck/README.md``,
  section "``--trace`` gate print"). The probe timestamps its own arrival
  of that stdout line, which the ChucK README documents as carrying
  roughly +-1 ms of stdout-pipe jitter on top of the true DSP-gate
  instant. Skipped (not failed) when ``chuck`` is not on ``PATH``, or when
  ``--no-chuck`` is passed.

This file intentionally re-measures nothing from ``neuralGrid/pi`` --
the existing hardware numbers (serial link RTT, LED apply latency) live
in ``neuralGrid/pi/tools/README.md`` and ``HANDOFF.md`` §10 and are cited
as-is in ``neuralGrid/paper/data/LATENCY_TABLE.md``; no model sits on any
of the three stages measured here (Global Constraint: "the fast plane
never has a model in it"), so these numbers are unaffected by whether a
model is "on" elsewhere in the system.

CLI::

    python -m septagraph.tools.latency_probe --out <csv> [--no-chuck] [--trials 200]

Run from ``septagraph/py`` (matches ``sweep.py`` / ``sept_bridge.py``).
"""
from __future__ import annotations

import argparse
import csv
import math
import queue
import shutil
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Optional

from ..codec import encode_hit
from ..manifest import ManifestStore
from ..net import RENDER_PORT, Receiver, Sender
from ..types import Hit, vec
from ..window import Window

# controllers.grid_ctl is a sibling top-level package (septagraph/py/controllers),
# importable when septagraph/py is on sys.path -- true when this tool is run as
# `python -m septagraph.tools.latency_probe` from septagraph/py (cwd on sys.path),
# and true under pytest via septagraph/py/conftest.py. Fall back to inserting it
# explicitly so `python path/to/latency_probe.py` also works.
_PY_ROOT = Path(__file__).resolve().parents[2]  # .../septagraph/py
if str(_PY_ROOT) not in sys.path:
    sys.path.insert(0, str(_PY_ROOT))

from controllers.grid_ctl import GRID_CHANNEL, GridController  # noqa: E402
from ..axes import K

N_TRIALS_DEFAULT = 200
STAGES = ("controller", "wire", "renderer")

CHUCK_DIR = Path(__file__).resolve().parents[3] / "chuck"  # .../septagraph/chuck
CHUCK_FILES = ["sept_render_core.ck", "sept_render_osc.ck:--trace", "sept_manifest.ck"]

# Ports distinct from the ones used by septagraph/py/tests fixtures, to avoid
# collisions with any test suite run concurrently on the same host.
_CONTROLLER_SENDTO_PORT = 27799  # nobody needs to listen; sendto still costs real syscall time
_WIRE_LOOPBACK_PORT = 27800

CSV_FIELDS = ["stage", "p50_ms", "p95_ms", "max_ms", "n", "host"]


def _percentile(sorted_samples: list[float], p: float) -> float:
    if not sorted_samples:
        return float("nan")
    if len(sorted_samples) == 1:
        return sorted_samples[0]
    k = (len(sorted_samples) - 1) * p
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return sorted_samples[int(k)]
    return sorted_samples[f] + (sorted_samples[c] - sorted_samples[f]) * (k - f)


def summarize(samples_ms: list[float]) -> dict:
    s = sorted(samples_ms)
    return {
        "p50_ms": _percentile(s, 0.50),
        "p95_ms": _percentile(s, 0.95),
        "max_ms": s[-1] if s else float("nan"),
        "n": len(s),
    }


class _TimingSender(Sender):
    """``Sender`` that timestamps every packet at the ``sendto`` call itself."""

    def __init__(self, targets: list[tuple[str, int]]) -> None:
        super().__init__(targets)
        self.send_times_ns: list[int] = []

    def send(self, packet: bytes) -> None:
        self.send_times_ns.append(time.perf_counter_ns())
        super().send(packet)


class _FakeLink:
    """Stand-in for LidarGridLink: GridController assigns its callbacks here."""

    on_key = None
    on_analog = None
    on_lidar = None


def _make_window() -> Window:
    return Window(
        center=vec(*([0.5] * K)),
        extent=vec(*([0.5] * K)),
        assign={"grid": (0, 4)},
    )


def measure_controller(n: int = N_TRIALS_DEFAULT) -> list[float]:
    """Stage (a): fake key event -> GridController.on_key -> Sender.send.

    No catalog manifest is loaded (empty ManifestStore) -- the point is to
    time the dict-lookup + project + encode + sendto path itself, not any
    particular snapping outcome; on_key's cost does not depend on whether
    the cell happened to snap.
    """
    sender = _TimingSender(targets=[("127.0.0.1", _CONTROLLER_SENDTO_PORT)])
    leds_calls: list[bytes] = []
    link = _FakeLink()
    ctl = GridController(
        link=link,
        sender=sender,
        window=_make_window(),
        manifests=ManifestStore(),
        leds=lambda frame: leds_calls.append(frame),
    )
    assert GRID_CHANNEL.size == (8, 8)

    samples_ms: list[float] = []
    try:
        for i in range(n):
            x, y = i % 8, (i // 8) % 8
            t0 = time.perf_counter_ns()
            ctl.on_key(x, y, 1)
            t1 = sender.send_times_ns[-1]
            samples_ms.append((t1 - t0) / 1e6)
            ctl.on_key(x, y, 0)  # release: reset held-state, untimed
    finally:
        sender.close()
    return samples_ms


def measure_wire(n: int = N_TRIALS_DEFAULT) -> list[float]:
    """Stage (b): Sender -> loopback Receiver -> decoded Hit callback."""
    arrivals: "queue.Queue[int]" = queue.Queue()

    def on_message(obj) -> None:
        if isinstance(obj, Hit):
            arrivals.put(time.perf_counter_ns())

    recv = Receiver(port=_WIRE_LOOPBACK_PORT, on_message=on_message)
    recv.start()
    time.sleep(0.05)  # let the receiver thread reach recvfrom()
    sender = Sender(targets=[("127.0.0.1", _WIRE_LOOPBACK_PORT)])
    samples_ms: list[float] = []
    try:
        for i in range(n):
            hit = Hit(
                voice_id=f"probe:{i}", layer=0, onset=True, velocity=0.7,
                mu=vec(*([0.5] * K)), sigma=vec(*([0.0] * K)),
            )
            packet = encode_hit(hit)
            t0 = time.perf_counter_ns()
            sender.send(packet)
            t1 = arrivals.get(timeout=1.0)
            samples_ms.append((t1 - t0) / 1e6)
    finally:
        sender.close()
        recv.stop()
    return samples_ms


def chuck_available() -> bool:
    return shutil.which("chuck") is not None


def measure_renderer(n: int = N_TRIALS_DEFAULT, timeout_s: float = 2.0) -> Optional[list[float]]:
    """Stage (c): Sender -> native ChucK renderer, gate print on stdout.

    Returns ``None`` (not an error) when ``chuck`` is absent from ``PATH``.
    """
    if not chuck_available():
        return None

    for name in CHUCK_FILES:
        base = name.split(":", 1)[0]
        if not (CHUCK_DIR / base).exists():
            raise FileNotFoundError(f"missing chuck file: {CHUCK_DIR / base}")

    proc = subprocess.Popen(
        ["chuck", "--loop", "--silent", *CHUCK_FILES],
        cwd=CHUCK_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    lines: "queue.Queue[tuple[int, str]]" = queue.Queue()

    def _reader() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            lines.put((time.perf_counter_ns(), line))

    reader_thread = threading.Thread(target=_reader, daemon=True)
    reader_thread.start()

    time.sleep(1.5)  # let the VM compile + spork + announce (matches test_chuck_renderer.py)
    if proc.poll() is not None:
        leftover = []
        while not lines.empty():
            leftover.append(lines.get_nowait()[1])
        raise RuntimeError(
            "chuck exited before probe start; stdout:\n" + "".join(leftover)
        )

    sender = Sender(targets=[("127.0.0.1", RENDER_PORT)])
    samples_ms: list[float] = []
    try:
        for i in range(n):
            hit = Hit(
                voice_id=f"probe:{i % 16}", layer=0, onset=True, velocity=0.7,
                mu=vec(*([0.5] * K)), sigma=vec(*([0.0] * K)),
            )
            t0 = time.perf_counter_ns()
            sender.send(encode_hit(hit))

            deadline = time.monotonic() + timeout_s
            t1 = None
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                try:
                    ts, line = lines.get(timeout=remaining)
                except queue.Empty:
                    break
                if "sept_gate" in line:
                    t1 = ts
                    break
            if t1 is None:
                raise RuntimeError(f"no 'sept_gate' trace line observed for trial {i}")
            samples_ms.append((t1 - t0) / 1e6)
            time.sleep(0.01)  # ~100 Hz cap, matches test_survives_hit_blast pacing
    finally:
        sender.close()
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=3.0)
    return samples_ms


def write_csv(path, rows: list[tuple]) -> None:
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(CSV_FIELDS)
        for row in rows:
            w.writerow(row)


def run(n: int = N_TRIALS_DEFAULT, include_chuck: bool = True) -> list[tuple]:
    host = socket.gethostname()
    rows: list[tuple] = []

    a = summarize(measure_controller(n))
    rows.append(("controller", a["p50_ms"], a["p95_ms"], a["max_ms"], a["n"], host))

    b = summarize(measure_wire(n))
    rows.append(("wire", b["p50_ms"], b["p95_ms"], b["max_ms"], b["n"], host))

    if include_chuck:
        if chuck_available():
            c_samples = measure_renderer(n)
            if c_samples is not None:
                c = summarize(c_samples)
                rows.append(("renderer", c["p50_ms"], c["p95_ms"], c["max_ms"], c["n"], host))
        else:
            print("latency_probe: 'chuck' not found on PATH; skipping renderer stage", file=sys.stderr)

    return rows


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Septagraph latency probe (plan Task 3.4)")
    parser.add_argument("--out", required=True, help="CSV output path")
    parser.add_argument("--no-chuck", action="store_true", help="skip the renderer (ChucK) stage")
    parser.add_argument("--trials", type=int, default=N_TRIALS_DEFAULT, help="trials per stage (default 200)")
    args = parser.parse_args(argv)

    rows = run(n=args.trials, include_chuck=not args.no_chuck)
    write_csv(args.out, rows)

    print(f"{'stage':<12}{'p50_ms':>10}{'p95_ms':>10}{'max_ms':>10}{'n':>6}  host")
    for stage, p50, p95, mx, n, host in rows:
        print(f"{stage:<12}{p50:>10.4f}{p95:>10.4f}{mx:>10.4f}{n:>6}  {host}")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
