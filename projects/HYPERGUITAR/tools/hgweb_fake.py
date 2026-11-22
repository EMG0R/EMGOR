#!/usr/bin/env python3
"""
hgweb_fake.py -- stand-in for the Bela, so hgweb.py can be driven and
verified with no hardware attached.

It does both halves of the link:
  * transmits HGMON telemetry lines at ~20 Hz to 127.0.0.1:9000, in exactly
    the captured wire format (see REFERENCE below), and
  * listens on 127.0.0.1:9001 for "HGSET <name> <float>", prints every
    datagram it receives, and FOLDS THE NEW VALUE INTO the vol: field it
    transmits -- so a slider move in the browser has to make the round trip
    to show up, exactly as it would on the real instrument.

Reference packet this replays the shape of:

  HGMON in:-29.8,-42.6,-52.7,-52.9,-54.9,-54.8,-54.4,-42.9,-54.9,-54.1
  out:-43.1,-40.7 acc:-0.626,-0.736,-0.142 gyr:-3.42,-1.82,0.66 menu:0
  armed:2 slots:1,1,1,1,1,1,1,0,0,0,0,0 vol:1.00,1.00,1.00,1.00,1.00,1.00,
  2.50,0.10,1.00,1.00                                   (all one line)

Flags:
  --static            send the reference line verbatim, frozen (easiest to
                      eyeball against the UI)
  --cycle-menu N      change menu: every N seconds (exercises the name swap)
  --drop-after S      stop transmitting after S seconds (staleness banner)
  --flaky             random 2-4 s dropouts
  --omit acc,gyr      never send those fields (must render as dashes, not 0)
  --garbage           occasionally send a malformed line

Stdlib only.  Ctrl-C to stop.
"""

from __future__ import annotations

import argparse
import math
import random
import socket
import sys
import threading
import time

REFERENCE = ("HGMON in:-29.8,-42.6,-52.7,-52.9,-54.9,-54.8,-54.4,-42.9,"
             "-54.9,-54.1 out:-43.1,-40.7 acc:-0.626,-0.736,-0.142 "
             "gyr:-3.42,-1.82,0.66 menu:0 armed:2 "
             "slots:1,1,1,1,1,1,1,0,0,0,0,0 "
             "vol:1.00,1.00,1.00,1.00,1.00,1.00,2.50,0.10,1.00,1.00")

VOL_NAMES = ["sub0", "sub1", "sub2", "sub3", "sub4", "sub5",
             "piezo", "trs", "outA", "outB"]

IN_BASE = [-29.8, -42.6, -52.7, -52.9, -54.9, -54.8, -54.4, -42.9, -54.9, -54.1]

# per-menu (effect-on) slot patterns, chosen so each page looks different
SLOT_PATTERNS = [
    [1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0],   # SPECTRAL: formant group on
    [0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0],   # PITCH: OCT UP
    [1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1],   # MOD: vib group + sag group
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],   # FILTER: nothing on
]
ARMED = [2, 0, 4, -1]      # PAD_FORMANT, PAD_PITCH, PAD_VIB, none


class Fake:
    def __init__(self, args):
        self.args = args
        self.vol = [1.0] * 6 + [2.5, 0.1, 1.0, 1.0]
        self.lock = threading.Lock()
        self.menu = args.menu
        self.omit = set(x.strip() for x in (args.omit or "").split(",") if x.strip())
        # the real rig has the ToF sensor OFF, so we only send it on request
        self.send_tof = bool(args.tof or args.tof_unplugged) and "tof" not in self.omit

    # ---- control listener -------------------------------------------------
    def listen(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind((self.args.listen_host, self.args.ctl_port))
        except OSError as exc:
            print(f"!! cannot bind control port {self.args.ctl_port}: {exc}",
                  file=sys.stderr)
            return
        print(f"   listening for HGSET on udp://{self.args.listen_host}:"
              f"{self.args.ctl_port}")
        while True:
            data, addr = s.recvfrom(4096)
            for line in data.decode("utf-8", "replace").splitlines():
                line = line.strip()
                if not line:
                    continue
                print(f"<< from {addr[0]}:{addr[1]}: {line!r}", flush=True)
                parts = line.split()
                if len(parts) != 3 or parts[0] != "HGSET":
                    print("   !! malformed, ignoring", flush=True)
                    continue
                name, val = parts[1], parts[2]
                if name not in VOL_NAMES:
                    print(f"   !! unknown control {name!r}", flush=True)
                    continue
                try:
                    v = float(val)
                except ValueError:
                    print(f"   !! not a float: {val!r}", flush=True)
                    continue
                v = max(0.0, min(4.0, v))          # firmware clamps
                with self.lock:
                    self.vol[VOL_NAMES.index(name)] = v
                print(f"   -> vol[{name}] = {v:.2f}", flush=True)

    # ---- telemetry --------------------------------------------------------
    def line(self, t):
        a = self.args
        if a.static:
            with self.lock:
                vol = ",".join(f"{v:.2f}" for v in self.vol)
            return REFERENCE.rsplit(" vol:", 1)[0] + " vol:" + vol

        parts = ["HGMON"]
        if "in" not in self.omit:
            ins = []
            for i, b in enumerate(IN_BASE):
                v = b + 6.0 * math.sin(t * (0.6 + 0.13 * i) + i)
                if i >= 4 and random.random() < 0.02:
                    v += 30.0                       # a pluck
                ins.append(max(-80.0, min(0.0, v)))
            parts.append("in:" + ",".join(f"{v:.1f}" for v in ins))
        if "out" not in self.omit:
            o = [-43.1 + 8 * math.sin(t * 0.9), -40.7 + 8 * math.sin(t * 0.9 + 0.4)]
            parts.append("out:" + ",".join(f"{v:.1f}" for v in o))
        if "acc" not in self.omit:
            parts.append("acc:%.3f,%.3f,%.3f" % (
                -0.626 + 0.2 * math.sin(t * 0.7),
                -0.736 + 0.2 * math.cos(t * 0.5), -0.142))
        if "gyr" not in self.omit:
            parts.append("gyr:%.2f,%.2f,%.2f" % (
                -3.42 + 20 * math.sin(t * 1.7), -1.82 + 20 * math.cos(t * 1.3),
                0.66))
        if self.send_tof:
            parts.append("tof:%d" % (-1 if a.tof_unplugged
                                     else int(400 + 300 * math.sin(t))))
        menu = self.menu
        if a.cycle_menu:
            menu = int(t / a.cycle_menu) % 4
        parts.append("menu:%d" % menu)
        parts.append("armed:%d" % ARMED[menu])
        parts.append("slots:" + ",".join(str(v) for v in SLOT_PATTERNS[menu]))
        with self.lock:
            parts.append("vol:" + ",".join(f"{v:.2f}" for v in self.vol))
        return " ".join(parts)

    def transmit(self):
        a = self.args
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        addr = (a.host, a.port)
        print(f"   transmitting HGMON -> udp://{a.host}:{a.port} at {a.rate} Hz")
        t0 = time.monotonic()
        period = 1.0 / a.rate
        dead_until = 0.0
        n = 0
        while True:
            now = time.monotonic()
            t = now - t0
            if a.drop_after and t > a.drop_after:
                if n >= 0:
                    print(f"   --drop-after {a.drop_after}s reached: "
                          "transmission stopped, UI must go stale", flush=True)
                    n = -1
                time.sleep(0.25)
                continue
            if a.flaky:
                if now < dead_until:
                    time.sleep(period)
                    continue
                if random.random() < 0.004:
                    dead_until = now + random.uniform(2.0, 4.0)
                    print("   (flaky) dropping out for "
                          f"{dead_until - now:.1f}s", flush=True)
                    continue
            if a.garbage and random.random() < 0.01:
                s.sendto(b"HGMON in:wat out: menu:zzz\n", addr)
            else:
                s.sendto((self.line(t) + "\n").encode("ascii"), addr)
            n += 1
            if n == 1:
                print("   first packet: " + self.line(t), flush=True)
            time.sleep(period)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1", help="telemetry destination")
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--listen-host", default="127.0.0.1")
    ap.add_argument("--ctl-port", type=int, default=9001)
    ap.add_argument("--rate", type=float, default=20.0)
    ap.add_argument("--static", action="store_true",
                    help="replay the captured reference line verbatim")
    ap.add_argument("--menu", type=int, default=0, choices=[0, 1, 2, 3])
    ap.add_argument("--cycle-menu", type=float, default=0.0, metavar="SEC")
    ap.add_argument("--drop-after", type=float, default=0.0, metavar="SEC")
    ap.add_argument("--flaky", action="store_true")
    ap.add_argument("--garbage", action="store_true")
    ap.add_argument("--omit", default="", metavar="acc,gyr")
    ap.add_argument("--tof", action="store_true",
                    help="also send tof: (the real rig has it OFF)")
    ap.add_argument("--tof-unplugged", action="store_true")
    args = ap.parse_args()

    f = Fake(args)
    threading.Thread(target=f.listen, daemon=True).start()
    try:
        f.transmit()
    except KeyboardInterrupt:
        print("\n   bye")


if __name__ == "__main__":
    main()
