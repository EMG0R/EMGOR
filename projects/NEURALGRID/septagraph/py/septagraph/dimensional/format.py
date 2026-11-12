"""The dimensional format v1 -- the stable contract in the middle.

    any interface -> encoder -> [ DIMENSIONAL FRAME ] -> model -> audio | text
                                 <- stable contract ->

A frame describes *what shape the hardware is*, and nothing else. Channels are
typed by their geometric dimensionality (1D / 2D / 3D) with every value
normalized to 0-1. No musical or perceptual meaning is assigned here -- that is
the whole point. An encoder author declares their geometry and is done; they
never have to decide what "trackpad-Y means in frequency terms". Assigning
meaning is the model's job, and there are two models (audio, text) reading the
same frames.

Everything is continuous. There is deliberately no discrete/continuous flag:
resolution differences are carried by ``sigma``, which does triple duty --

  1. model uncertainty      "I'm not sure"
  2. device capability      "I can't sense this at all"  (SIGMA_UNKNOWN)
  3. quantization           "I sense this, but only this precisely"

An 8-step pad axis reports sigma = 1/16 (half a cell: the finger is somewhere
in that cell and that is all you know); a trackpad reports far less.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

FORMAT_VERSION = 1

# Controlled vocabulary. Free-text labels would leave the geometry portable but
# the semantics not, which defeats the format: a model that sees "force" on the
# grid and "force" on a laptop can generalize; one that sees "fsr_under_pad" and
# "trackpad_force" cannot. Add to this list deliberately, never ad hoc.
LABELS = (
    "position",   # where something is, within a space
    "force",      # how hard
    "distance",   # how far away
    "ribbon",     # position along a 1D strip
    "key",        # a named/indexed discrete selector
    "tilt",       # orientation
)

EDGE_KINDS = ("onset", "release")

#: A dimension the device cannot sense at all -- maximum spread, no information.
SIGMA_UNKNOWN = 1.0


def clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else 1.0 if x > 1.0 else float(x)


def sigma_for_steps(n_steps: int) -> float:
    """Sigma implied by quantizing a 0-1 axis into ``n_steps`` cells.

    Half a cell: you know which cell the finger was in, not where in it.
    """
    n = int(n_steps)
    if n <= 0:
        raise ValueError("n_steps must be positive")
    return 1.0 / (2.0 * n)


@dataclass(frozen=True)
class Point:
    """One simultaneous contact/reading within a channel."""

    values: tuple[float, ...]
    sigma: tuple[float, ...]
    pid: int = 0

    def __post_init__(self):
        values = tuple(clamp01(float(v)) for v in self.values)
        sigma = tuple(clamp01(float(s)) for s in self.sigma)
        if len(sigma) != len(values):
            raise ValueError(
                f"sigma must have one entry per value ({len(values)}), got {len(sigma)}"
            )
        object.__setattr__(self, "values", values)
        object.__setattr__(self, "sigma", sigma)
        object.__setattr__(self, "pid", int(self.pid))


@dataclass(frozen=True)
class Channel:
    """One geometric channel of an interface.

    ``dims`` is 1, 2 or 3. ``points`` may hold several simultaneous contacts
    (multi-touch); empty means nothing is active right now. ``associated`` is a
    soft hint -- e.g. a global force channel naming the 2D space it sits under
    -- that a model may use or ignore. It is deliberately NOT structural
    nesting: asserting the relationship in the format would be exactly the kind
    of pre-decided meaning this layer exists to avoid.
    """

    id: str
    dims: int
    label: str
    points: tuple[Point, ...] = ()
    associated: tuple[str, ...] = ()

    def __post_init__(self):
        dims = int(self.dims)
        if dims not in (1, 2, 3):
            raise ValueError(f"dims must be 1, 2 or 3, got {dims}")
        if self.label not in LABELS:
            raise ValueError(
                f"label {self.label!r} is not in the controlled vocabulary {LABELS}"
            )
        points = tuple(self.points)
        for p in points:
            if len(p.values) != dims:
                raise ValueError(
                    f"channel {self.id!r} has dims={dims} but a point of width {len(p.values)}"
                )
        object.__setattr__(self, "dims", dims)
        object.__setattr__(self, "points", points)
        object.__setattr__(self, "associated", tuple(self.associated))

    @property
    def active(self) -> bool:
        return bool(self.points)


@dataclass(frozen=True)
class Edge:
    """A precise onset/release timestamp.

    The sampled stream runs at ~42fps (24ms of resolution); the device knows a
    press to sub-millisecond. Edges exist so that precision is not thrown away
    by the act of sampling -- the sub-10ms fast path depends on it.
    """

    channel: str
    kind: str
    t_mono: float
    point: tuple[float, ...]

    def __post_init__(self):
        if self.kind not in EDGE_KINDS:
            raise ValueError(f"edge kind must be one of {EDGE_KINDS}, got {self.kind!r}")
        object.__setattr__(self, "t_mono", float(self.t_mono))
        object.__setattr__(self, "point", tuple(clamp01(float(v)) for v in self.point))


@dataclass(frozen=True)
class DimensionalFrame:
    source: str
    t_mono: float
    channels: tuple[Channel, ...] = ()
    edges: tuple[Edge, ...] = ()

    def __post_init__(self):
        channels = tuple(self.channels)
        ids = [c.id for c in channels]
        dupes = {i for i in ids if ids.count(i) > 1}
        if dupes:
            raise ValueError(f"duplicate channel ids: {sorted(dupes)}")
        object.__setattr__(self, "channels", channels)
        object.__setattr__(self, "edges", tuple(self.edges))
        object.__setattr__(self, "t_mono", float(self.t_mono))

    def channel(self, channel_id: str) -> Optional[Channel]:
        for c in self.channels:
            if c.id == channel_id:
                return c
        return None


def frame_to_dict(f: DimensionalFrame) -> dict:
    return {
        "v": FORMAT_VERSION,
        "source": f.source,
        "t": f.t_mono,
        "channels": [
            {
                "id": c.id,
                "dims": c.dims,
                "label": c.label,
                "points": [
                    {"v": list(p.values), "s": list(p.sigma), "pid": p.pid}
                    for p in c.points
                ],
                "assoc": list(c.associated),
            }
            for c in f.channels
        ],
        "edges": [
            {"ch": e.channel, "kind": e.kind, "t": e.t_mono, "p": list(e.point)}
            for e in f.edges
        ],
    }


def frame_from_dict(d: dict) -> DimensionalFrame:
    v = d.get("v")
    if v != FORMAT_VERSION:
        raise ValueError(f"unsupported dimensional format version {v!r}")
    return DimensionalFrame(
        source=d["source"],
        t_mono=d["t"],
        channels=tuple(
            Channel(
                id=c["id"],
                dims=c["dims"],
                label=c["label"],
                points=tuple(
                    Point(values=tuple(p["v"]), sigma=tuple(p["s"]), pid=p.get("pid", 0))
                    for p in c.get("points", ())
                ),
                associated=tuple(c.get("assoc", ())),
            )
            for c in d.get("channels", ())
        ),
        edges=tuple(
            Edge(channel=e["ch"], kind=e["kind"], t_mono=e["t"], point=tuple(e["p"]))
            for e in d.get("edges", ())
        ),
    )


def frame_to_json(f: DimensionalFrame, indent: str = " ") -> str:
    """The wire format, with a line break per channel and per edge.

    ``json.dumps(..., indent=n)`` puts every single number on its own
    line, which turns one frame into ~90 lines: you cannot see a channel
    at a glance, let alone compare two frames. The readable unit is the
    CHANNEL -- one per line, its values inline -- because that is the
    thing you reason about. Still ordinary JSON; it round-trips.
    """
    import json as _json

    def one(obj) -> str:
        return _json.dumps(obj, separators=(", ", ": "))

    d = frame_to_dict(f)
    i = indent
    lines = [
        "{",
        f'{i}"v": {d["v"]}, "source": {one(d["source"])}, "t": {d["t"]},',
        f'{i}"channels": [',
    ]
    chans = d["channels"]
    for n, c in enumerate(chans):
        lines.append(f"{i}{i}{one(c)}" + ("," if n < len(chans) - 1 else ""))
    lines.append(f"{i}],")
    edges = d["edges"]
    if edges:
        lines.append(f'{i}"edges": [')
        for n, e in enumerate(edges):
            lines.append(f"{i}{i}{one(e)}" + ("," if n < len(edges) - 1 else ""))
        lines.append(f"{i}]")
    else:
        lines.append(f'{i}"edges": []')
    lines.append("}")
    return "\n".join(lines)


#: Below this, sigma is not worth the ink on a readout line.
SIGMA_WORTH_SHOWING = 0.05


def frame_to_line(f: DimensionalFrame) -> str:
    """One dense, readable line per frame -- what the monitor prints.

    JSON is the wire format; it is not something anyone can read while playing.
    Only active channels appear, sigma only when it is large enough to matter,
    and onsets are called out because they are the thing you are listening for.
    """
    parts = []
    for c in f.channels:
        if not c.points:
            continue
        p = c.points[0]
        coords = ",".join(f"{v:.2f}" for v in p.values)
        seg = f"{c.id} {coords}"
        worst = max(p.sigma) if p.sigma else 0.0
        if worst >= SIGMA_WORTH_SHOWING:
            seg += f" ±{worst:.2f}"
        if len(c.points) > 1:
            seg += f" x{len(c.points)}"
        parts.append(seg)

    onsets = sum(1 for e in f.edges if e.kind == "onset")
    if onsets:
        parts.append(f"onset x{onsets}" if onsets > 1 else "onset")

    body = "   ".join(parts) if parts else "idle"
    return f"{f.t_mono:8.3f}  {body}"


__all__ = [
    "FORMAT_VERSION", "LABELS", "EDGE_KINDS", "SIGMA_UNKNOWN",
    "clamp01", "sigma_for_steps",
    "Point", "Channel", "Edge", "DimensionalFrame",
    "frame_to_dict", "frame_from_dict", "frame_to_line", "frame_to_json",
    "SIGMA_WORTH_SHOWING",
]
