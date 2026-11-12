"""CLI entrypoint.

Examples:
  python -m lightserver --preview --scene scenes/aurora.json
  python -m lightserver --headless --scene scenes/comet.json --duration 5
  python -m lightserver --api                       # UDP -> strip + HTTP on :8800
  python -m lightserver --api --preview             # terminal preview + HTTP
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import DEFAULT_FPS, DEFAULT_IP, DEFAULT_PORT, NUM_PIXELS
from .engine import Engine
from .nodes import DEFAULT_NODES_PATH, NodeManager, load_node_configs
from .persist import DEFAULT_STATE_PATH, StateStore
from .ring import (DEFAULT_RING_PATH, RingConfigError, RingManager,
                   load_ring_segments, ring_length)
from .satellites import (DEFAULT_SATELLITES_PATH, SatelliteConfigError,
                         load_satellites)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="lightserver",
                                 description="Real-time LED render engine for the FCOB strip")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--preview", action="store_true", help="render to the terminal")
    mode.add_argument("--headless", action="store_true", help="no output (tests/bench)")
    ap.add_argument("--ip", default=DEFAULT_IP, help=f"strip IP (default {DEFAULT_IP})")
    ap.add_argument("--nodes", type=Path, default=None,
                    help=f"node registry JSON (default {DEFAULT_NODES_PATH} if present; "
                         "in udp mode each enabled node gets its own engine)")
    ap.add_argument("--ring", type=Path, default=None,
                    help=f"ring topology JSON (default {DEFAULT_RING_PATH} if "
                         "present; ONE engine renders the whole closed loop and "
                         "slices per strip — the ring is the only control target)")
    ap.add_argument("--no-ring", action="store_true",
                    help="ignore ring.json and run per-node engines")
    ap.add_argument("--satellites", type=Path, default=None,
                    help=f"satellite 'dot' node registry JSON (default "
                         f"{DEFAULT_SATELLITES_PATH} if present; ring mode "
                         "only; missing file/entry list -> no satellites)")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--fps", type=int, default=DEFAULT_FPS)
    ap.add_argument("--pixels", type=int, default=NUM_PIXELS)
    ap.add_argument("--scene", type=Path, help="scene JSON to load at start "
                    "(boot default; persisted per-node state overrides it)")
    ap.add_argument("--state", type=Path, default=DEFAULT_STATE_PATH,
                    help=f"per-node scene persistence file (API mode; "
                         f"default {DEFAULT_STATE_PATH})")
    ap.add_argument("--no-state", action="store_true",
                    help="disable scene persistence (API mode)")
    ap.add_argument("--prompt", help="natural-language vibe to generate the starting scene")
    ap.add_argument("--duration", type=float, help="run for N seconds then exit")
    ap.add_argument("--api", action="store_true", help="also serve the HTTP API")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--api-port", type=int, default=8800)
    ap.add_argument("--seed", type=int, help="deterministic RNG seed")
    args = ap.parse_args(argv)

    out_mode = "preview" if args.preview else ("headless" if args.headless else "udp")

    nodes_path = args.nodes or DEFAULT_NODES_PATH
    ring_path = args.ring or DEFAULT_RING_PATH
    if (not args.no_ring and out_mode == "udp" and Path(ring_path).is_file()
            and Path(nodes_path).is_file()):
        # Ring topology: one engine at ring resolution, sliced per strip.
        configs = load_node_configs(nodes_path)
        try:
            segments = load_ring_segments(ring_path, configs)
        except RingConfigError as e:
            print(f"[ring] INVALID {ring_path}: {e}", file=sys.stderr)
            return 2
        sat_path = args.satellites or DEFAULT_SATELLITES_PATH
        try:
            satellites = load_satellites(sat_path, configs, ring_length(segments))
        except SatelliteConfigError as e:
            print(f"[satellites] INVALID {sat_path}: {e}", file=sys.stderr)
            return 2
        manager = RingManager.from_configs(configs, segments, fps=args.fps,
                                           mode="udp", seed=args.seed,
                                           satellites=satellites)
        total = manager.default.n
        order = " -> ".join(f"{s.node}[{s.ring_offset}..{s.ring_offset + s.length - 1}]"
                            for s in segments)
        print(f"[ring] {ring_path}: {total}px closed loop: {order}", file=sys.stderr)
        if satellites:
            sats = ", ".join(f"{s.node}@{s.anchor}({s.mode})" for s in satellites)
            print(f"[satellites] {sat_path}: {sats}", file=sys.stderr)
    elif out_mode == "udp" and Path(nodes_path).is_file():
        # Multi-node: one engine per enabled node from the registry.
        configs = load_node_configs(nodes_path)
        manager = NodeManager.from_configs(configs, fps=args.fps, mode="udp",
                                           seed=args.seed)
        names = ", ".join(f"{c.name}({c.pixel_count}px)" for c in configs if c.enabled)
        print(f"[nodes] {nodes_path}: {names}", file=sys.stderr)
    else:
        # Single-node path, exactly as before (preview/headless, or no nodes.json).
        engine = Engine(num_pixels=args.pixels, fps=args.fps, mode=out_mode,
                        ip=args.ip, port=args.port, seed=args.seed)
        manager = NodeManager.single(engine)
    engine = manager.default

    if args.scene:
        manager.set_scene_all(json.loads(args.scene.read_text()), crossfade=0)
    elif args.prompt:
        from .nl import generate_scene
        scene, source = generate_scene(args.prompt)
        print(f"[nl] scene '{scene.get('name')}' via {source}", file=sys.stderr)
        manager.set_scene_all(scene, crossfade=0)

    if args.api:
        import uvicorn
        from .api import create_app
        if not args.no_state:
            # Persisted per-node scenes win over the boot default above;
            # nodes without a persisted entry keep the default.
            store = StateStore(args.state)
            manager.restore(store)
            print(f"[state] {store.path} "
                  f"({'restored' if store.load() else 'empty; using defaults'})",
                  file=sys.stderr)
        manager.start_all()
        app = create_app(engine, manager)
        try:
            uvicorn.run(app, host=args.host, port=args.api_port, log_level="info")
        finally:
            manager.shutdown()
        return 0

    try:
        manager.start_others_background()
        engine.run(duration=args.duration)
    except KeyboardInterrupt:
        pass
    finally:
        manager.shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
