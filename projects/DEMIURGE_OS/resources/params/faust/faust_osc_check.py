#!/usr/bin/env python3
"""Faust selftest driver: Faust speaks its own OSC, not /p, so this sends what the pool's osc
sink would send and waits for the arch's /pout.
  /cutoff 0.5              (alias [osc:/cutoff 0 1]: Faust maps 0..1 -> 40..12000 itself)
                           -> engine reports /test/cutoff = 0.5 (normalized)
  /dparam_example/res 0.6  (default address, native 0..1) -> /test/res = 0.6
"""
import argparse, os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import testpool

ap = argparse.ArgumentParser()
ap.add_argument("--pool-port", type=int, required=True)
ap.add_argument("--engine-port", type=int, required=True)
ap.add_argument("--timeout", type=float, default=12)
a = ap.parse_args()
pool = testpool.TestPool(a.pool_port).start()
want = [("/cutoff", 0.5, "/test/cutoff"), ("/dparam_example/res", 0.6, "/test/res")]
end = time.time() + a.timeout
ok = False
while time.time() < end and not ok:
    for addr, v, path in want:
        pool.sock.sendto(testpool.encode(addr, v), ("127.0.0.1", a.engine_port))
    time.sleep(0.25)
    ok = all(pool.has_pout(path, v, 0.01) for _, v, path in want)
pool.stop()
print("PASS: faust OSC alias + default address round-trip" if ok else
      f"FAIL: faust engine did not report {[w[2] for w in want]} (saw {[(p, round(v, 3)) for _, p, v in pool.pouts][-4:]})")
sys.exit(0 if ok else 1)
