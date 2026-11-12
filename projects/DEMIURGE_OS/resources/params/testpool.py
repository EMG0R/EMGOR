#!/usr/bin/env python3
"""testpool.py -- a fake parameter pool for self-tests and the stock checker.

Pure stdlib. Speaks the same wire as the real pool (docs/parameters.md §11):
  engine -> pool : /pout <path:s> <val:f>   (recorded here, never echoed back)
  pool -> engine : /p    <path:s> <val:f>   (sent from here to the engine port)

Library use:
    pool = TestPool(19100); pool.start()
    pool.send_p(19200, "/test/ping", 0.75)
    pool.wait_pout("/test/ping", 0.75, timeout=5)

CLI (what every language's selftest.sh calls):
    testpool.py selftest --pool-port 19100 --engine-port 19200 \
        [--path /test/ping] [--value 0.75] [--timeout 12]
  Re-sends /p every 0.2 s (engines start slowly, UDP is lossy) until the
  engine reports the same value back with /pout. Exit 0 = PASS, 1 = no echo.

    testpool.py listen --pool-port 19100 [--seconds 10]   # dump /pout traffic
    testpool.py send --engine-port 19200 --path /x/y --value 0.5

Never use ports outside 19000-19999 for tests on a live rig (9102 is the real pool).
"""
import argparse
import math
import socket
import struct
import sys
import threading
import time


def _pad(b: bytes) -> bytes:
    return b + b"\0" * (4 - len(b) % 4)


def encode(addr: str, *args) -> bytes:
    tags, body = ",", b""
    for a in args:
        if isinstance(a, str):
            tags += "s"; body += _pad(a.encode())
        elif isinstance(a, int) and not isinstance(a, bool):
            tags += "i"; body += struct.pack(">i", a)
        else:
            tags += "f"; body += struct.pack(">f", float(a))
    return _pad(addr.encode()) + _pad(tags.encode()) + body


def _rd_str(d: bytes, i: int):
    j = d.index(b"\0", i)
    return d[i:j].decode(errors="replace"), (j + 4) & ~3


def decode(d: bytes):
    """-> (address, [args]) ; bundles are flattened by the caller (rare here)."""
    if d.startswith(b"#bundle"):
        out, i = [], 16
        while i + 4 <= len(d):
            n = struct.unpack(">i", d[i:i + 4])[0]
            out.append(decode(d[i + 4:i + 4 + n])); i += 4 + n
        return out[0] if out else ("", [])
    addr, i = _rd_str(d, 0)
    tags, i = _rd_str(d, i)
    args = []
    for t in tags[1:]:
        if t == "s":
            s, i = _rd_str(d, i); args.append(s)
        elif t == "f":
            args.append(struct.unpack(">f", d[i:i + 4])[0]); i += 4
        elif t == "d":
            args.append(struct.unpack(">d", d[i:i + 8])[0]); i += 8
        elif t == "i":
            args.append(struct.unpack(">i", d[i:i + 4])[0]); i += 4
        elif t == "h":
            args.append(struct.unpack(">q", d[i:i + 8])[0]); i += 8
        elif t in "TF":
            args.append(t == "T")
        else:
            break
    return addr, args


class TestPool:
    def __init__(self, port: int, host: str = "127.0.0.1"):
        self.port, self.host = port, host
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((host, port))
        self.sock.settimeout(0.1)
        self.pouts = []          # (time, path, value)
        self.other = []          # anything that is not /pout (a bug if the engine sends /p here)
        self._lock = threading.Lock()
        self._stop = False
        self._t = None

    def start(self):
        self._t = threading.Thread(target=self._run, daemon=True)
        self._t.start()
        return self

    def stop(self):
        self._stop = True
        if self._t:
            self._t.join(1)
        self.sock.close()

    def _run(self):
        while not self._stop:
            try:
                d, _ = self.sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                addr, args = decode(d)
            except Exception:
                continue
            with self._lock:
                if addr == "/pout" and len(args) == 2 and isinstance(args[0], str):
                    self.pouts.append((time.time(), args[0], float(args[1])))
                else:
                    self.other.append((addr, args))

    def send_p(self, engine_port: int, path: str, value: float, host: str = "127.0.0.1"):
        self.sock.sendto(encode("/p", path, float(value)), (host, engine_port))

    def wait_pout(self, path: str, value=None, timeout: float = 5.0, tol: float = 1e-3):
        end = time.time() + timeout
        while time.time() < end:
            if self.has_pout(path, value, tol):
                return True
            time.sleep(0.02)
        return False

    def has_pout(self, path, value=None, tol=1e-3):
        with self._lock:
            return any(p == path and (value is None or math.isclose(v, value, abs_tol=tol))
                       for _, p, v in self.pouts)


def cmd_selftest(a) -> int:
    pool = TestPool(a.pool_port).start()
    end = time.time() + a.timeout
    ok = False
    try:
        while time.time() < end and not ok:
            pool.send_p(a.engine_port, a.path, a.value)
            ok = pool.wait_pout(a.path, a.value, timeout=0.2)
    finally:
        seen = list(pool.pouts)[-3:]
        pool.stop()
    if ok:
        print(f"PASS: sent /p {a.path} {a.value}, engine answered /pout")
        return 0
    print(f"FAIL: no matching /pout for {a.path}={a.value} within {a.timeout}s "
          f"(last pouts: {[(p, round(v, 4)) for _, p, v in seen]})")
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("selftest")
    s.add_argument("--pool-port", type=int, required=True)
    s.add_argument("--engine-port", type=int, required=True)
    s.add_argument("--path", default="/test/ping")
    s.add_argument("--value", type=float, default=0.75)
    s.add_argument("--timeout", type=float, default=12.0)
    l = sp.add_parser("listen")
    l.add_argument("--pool-port", type=int, required=True)
    l.add_argument("--seconds", type=float, default=10.0)
    n = sp.add_parser("send")
    n.add_argument("--engine-port", type=int, required=True)
    n.add_argument("--path", required=True)
    n.add_argument("--value", type=float, required=True)
    a = ap.parse_args()
    if a.cmd == "selftest":
        sys.exit(cmd_selftest(a))
    if a.cmd == "listen":
        p = TestPool(a.pool_port).start()
        time.sleep(a.seconds)
        for t, path, v in p.pouts:
            print(f"/pout {path} {v}")
        p.stop()
        return
    socket.socket(socket.AF_INET, socket.SOCK_DGRAM).sendto(
        encode("/p", a.path, a.value), ("127.0.0.1", a.engine_port))


if __name__ == "__main__":
    main()
