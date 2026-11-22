#!/usr/bin/env python3
"""
hgmon_fake.py -- fake hyperGuitar telemetry transmitter, for testing hgmon.py
without the real Bela.

Emits HGMON lines in exactly the documented wire format to 127.0.0.1:9000 at
~20 Hz, with plausible motion: drifting input levels, plucked-string transients
on the hex channels, a tilting accelerometer, a sweeping ToF, a circling
joystick and slowly walking encoders.

Useful flags for exercising hgmon's edge cases:

  --omit tof,gyr      never send those fields   -> hgmon must show "no data",
                                                   not 0.00
  --tof-unplugged     send tof:-1               -> "no reading", a third state
  --flaky             random 1-3 s transmission dropouts -> staleness banner
  --burst             occasional loud transients -> peak-hold markers
  --rate 5            slower packet rate         -> pkt/s readout
  --garbage           occasionally send a malformed line -> parser robustness

Stdlib only.  Ctrl-C to stop.
"""

from __future__ import annotations

import argparse
import math
import random
import socket
import sys
import time

FIELDS = ["in", "out", "acc", "gyr", "tof", "exp", "jx", "jy", "jsw",
          "pot", "enc1", "enc2"]

IN_BASE = [-35.0, -37.0, -62.0, -61.0, -73.0, -65.0, -64.0, -70.0, -68.0, -75.0]


def build(t, state, args):
    """Return one HGMON line (no trailing newline) for time t."""
    omit = state["omit"]
    parts = ["HGMON"]

    # --- audio inputs ------------------------------------------------------
    if "in" not in omit:
        ins = []
        for i, base in enumerate(IN_BASE):
            v = base + 4.0 * math.sin(t * (0.7 + 0.13 * i) + i)
            v += random.uniform(-1.2, 1.2)
            # plucked-string transient decay on hex channels
            env = state["pluck"][i]
            if env > 0.001:
                v += 40.0 * env
                state["pluck"][i] = env * 0.72
            ins.append(min(0.5, v))
        parts.append("in:" + ",".join("%.1f" % v for v in ins))

    # --- audio outputs -----------------------------------------------------
    if "out" not in omit:
        base = -22.0 + 3.0 * math.sin(t * 0.9)
        peak = max(state["pluck"]) if state["pluck"] else 0.0
        outs = [base + 30.0 * peak + random.uniform(-0.6, 0.6),
                base + 30.0 * peak + random.uniform(-0.6, 0.6)]
        parts.append("out:" + ",".join("%.1f" % min(0.0, v) for v in outs))

    # --- IMU ---------------------------------------------------------------
    if "acc" not in omit:
        ax = 0.15 * math.sin(t * 0.6) + random.uniform(-0.01, 0.01)
        ay = 0.15 * math.cos(t * 0.43) + random.uniform(-0.01, 0.01)
        az = 0.98 + 0.03 * math.sin(t * 1.1)
        parts.append("acc:%.3f,%.3f,%.3f" % (ax, ay, az))
    if "gyr" not in omit:
        gx = 30.0 * math.sin(t * 1.7) + random.uniform(-1.0, 1.0)
        gy = 18.0 * math.sin(t * 2.3 + 1.0) + random.uniform(-1.0, 1.0)
        gz = 25.0 * math.cos(t * 1.1) + random.uniform(-1.0, 1.0)
        parts.append("gyr:%.2f,%.2f,%.2f" % (gx, gy, gz))

    # --- ToF ---------------------------------------------------------------
    if "tof" not in omit:
        if args.tof_unplugged:
            parts.append("tof:-1")
        else:
            # sweeps 80..1400 mm, with occasional dropouts to -1
            d = 740 + 660 * math.sin(t * 0.5)
            if random.random() < 0.03:
                parts.append("tof:-1")
            else:
                parts.append("tof:%d" % int(d))

    # --- pedals / pots / stick --------------------------------------------
    if "exp" not in omit:
        parts.append("exp:%.3f" % (0.5 + 0.5 * math.sin(t * 0.37)))
    if "jx" not in omit:
        parts.append("jx:%.3f" % (0.5 + 0.42 * math.sin(t * 1.3)))
    if "jy" not in omit:
        parts.append("jy:%.3f" % (0.5 + 0.42 * math.cos(t * 1.3)))
    if "jsw" not in omit:
        parts.append("jsw:%d" % (1 if (int(t) % 5) == 0 else 0))
    if "pot" not in omit:
        parts.append("pot:%.3f" % (0.5 + 0.48 * math.sin(t * 0.21 + 2.0)))

    # --- encoders ----------------------------------------------------------
    if random.random() < 0.15:
        state["enc1"] += random.choice((-1, 1))
    if random.random() < 0.10:
        state["enc2"] += random.choice((-1, 1))
    if "enc1" not in omit:
        parts.append("enc1:%d" % state["enc1"])
    if "enc2" not in omit:
        parts.append("enc2:%d" % state["enc2"])

    return " ".join(parts)


def main(argv=None):
    ap = argparse.ArgumentParser(description="fake hyperGuitar telemetry source")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--rate", type=float, default=20.0, help="packets/sec")
    ap.add_argument("--omit", default="",
                    help="comma list of fields to never send, e.g. tof,gyr,pot "
                         "(valid: %s)" % ",".join(FIELDS))
    ap.add_argument("--tof-unplugged", action="store_true",
                    help="always report tof:-1 (sensor present, no reading)")
    ap.add_argument("--flaky", action="store_true",
                    help="random 1-3 s transmission dropouts")
    ap.add_argument("--burst", action="store_true",
                    help="fire loud string transients, to exercise peak hold")
    ap.add_argument("--garbage", action="store_true",
                    help="occasionally emit a malformed line")
    ap.add_argument("--count", type=int, default=0,
                    help="stop after N packets (0 = run forever)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    omit = {f.strip().lower() for f in args.omit.split(",") if f.strip()}
    unknown = omit - set(FIELDS)
    if unknown:
        sys.stderr.write("hgmon_fake: unknown field(s) in --omit: %s\n"
                         % ",".join(sorted(unknown)))
        return 2

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.host, args.port)
    state = {"omit": omit, "pluck": [0.0] * 10, "enc1": 0, "enc2": 0}

    period = 1.0 / max(0.1, args.rate)
    t0 = time.monotonic()
    sent = 0
    next_send = t0
    dropout_until = 0.0
    next_dropout = t0 + random.uniform(4.0, 9.0)

    if not args.quiet:
        print("hgmon_fake -> %s:%d at %.1f Hz%s%s"
              % (args.host, args.port, args.rate,
                 ("  omitting: " + ",".join(sorted(omit))) if omit else "",
                 "  (flaky)" if args.flaky else ""))
        print("Ctrl-C to stop.")

    try:
        while True:
            now = time.monotonic()
            if now < next_send:
                time.sleep(min(0.01, next_send - now))
                continue
            next_send += period
            if next_send < now - period:      # we fell behind; resync
                next_send = now + period
            t = now - t0

            if args.flaky:
                if now >= dropout_until and now >= next_dropout:
                    dropout_until = now + random.uniform(1.2, 3.0)
                    next_dropout = dropout_until + random.uniform(4.0, 9.0)
                    if not args.quiet:
                        print("  [dropout for %.1fs]"
                              % (dropout_until - now))
                if now < dropout_until:
                    continue

            if args.burst and random.random() < 0.04:
                state["pluck"][random.randrange(4, 10)] = random.uniform(0.6, 1.0)

            line = build(t, state, args)
            if args.garbage and random.random() < 0.05:
                line = random.choice([
                    "NOTHGMON garbage here",
                    "HGMON in:not,a,number out:x acc:",
                    "HGMON tof:",
                ])
            try:
                sock.sendto((line + "\n").encode("ascii"), dest)
            except OSError as e:
                sys.stderr.write("send failed: %s\n" % e)
                time.sleep(0.5)
                continue
            sent += 1
            if args.count and sent >= args.count:
                break
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    if not args.quiet:
        print("\nsent %d packets" % sent)
    return 0


if __name__ == "__main__":
    sys.exit(main())
