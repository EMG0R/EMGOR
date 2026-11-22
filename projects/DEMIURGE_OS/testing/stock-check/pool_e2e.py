#!/usr/bin/env python3
"""pool_e2e.py -- drive a hook engine through the REAL demiurge-io pool objects.

pool write -> OscSink ("proto":"p": /p <path> <0..1>; or address mode) -> engine -> /pout to the pool
port -> parse_pool_packet -> Pool.publish_out -> Pool.values. Exit 0 iff the pool
recorded the engine's report for every --path (value within 0.01 of what was sent).

    pool_e2e.py --src REPO/src/demiurge-io --stage csound-demo --engine-port 18601 \
        --pool-port 18701 --path /csound-demo/ping [--path ...] [--value 0.75] [--timeout 12]

Re-sends every 0.2 s (engines start slowly, UDP is lossy). Loopback test ports only.
"""
import argparse, socket, sys, time, os

ap = argparse.ArgumentParser()
ap.add_argument("--src", required=True)
ap.add_argument("--stage", required=True)
ap.add_argument("--engine-port", type=int, required=True)
ap.add_argument("--pool-port", type=int, required=True)
ap.add_argument("--path", action="append", required=True)
ap.add_argument("--address-prefix", default="",
                help="address mode (Faust): send <prefix>/<last path component> instead of /p")
ap.add_argument("--value", type=float, default=0.75)
ap.add_argument("--timeout", type=float, default=12)
a = ap.parse_args()
sys.dont_write_bytecode = True
sys.path.insert(0, a.src)
from demiurge_io.pool import ParamRegistry, Pool
from demiurge_io.sinks.osc import OscSink
from demiurge_io.sources.osc_in import parse_pool_packet

if not 18000 <= a.pool_port <= 19999 or not 18000 <= a.engine_port <= 19999:
    sys.exit("refusing: test ports must be 18000-19999")
reg = ParamRegistry("/nonexistent-params-dir")
doc = {"stage": a.stage, "port": a.engine_port,
       "params": [({"path": p, "address": a.address_prefix + "/" + p.rsplit("/", 1)[-1]}
                   if a.address_prefix else {"path": p, "proto": "p"}) for p in a.path]}
n = reg.load_manifest(doc)
if n != len(a.path):
    print(f"FAIL: registry loaded {n}/{len(a.path)} hook params"); sys.exit(1)
pool = Pool(reg)
sink = OscSink()
pool.add_sink("osc", sink)
rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx.bind(("127.0.0.1", a.pool_port)); rx.settimeout(0.1)
got = set(); t_end = time.time() + a.timeout; last = 0.0
while time.time() < t_end and len(got) < len(a.path):
    if time.time() - last > 0.2:
        for p in a.path:
            if p not in got:
                pool.publish(p, a.value)
        sink.flush(); last = time.time()
    try:
        data, _ = rx.recvfrom(4096)
    except socket.timeout:
        sink.flush(); continue
    for path, val in parse_pool_packet(data, "/pout"):
        if path in a.path and abs(val - a.value) < 0.01 and pool.publish_out(path, val):
            got.add(path)
missing = [p for p in a.path if p not in got]
if missing:
    print("FAIL: no /pout echo for " + ", ".join(missing)); sys.exit(1)
print(f"PASS: pool wrote {len(a.path)} param(s) via /p, engine reported /pout, pool recorded "
      + ", ".join(f"{p}={pool.values[p]:.2f}" for p in a.path))
