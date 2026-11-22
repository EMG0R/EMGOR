#!/usr/bin/env python3
"""Channel-isolation verdict for nam_channel_isolation.csd captures.

Usage: analyze_isolation.py <dir with iso_dry_r0 iso_wet_r0 iso_wet_r1>
Pure stdlib (no numpy — must run on the Pi as-is). Exit 0 = PASS.
"""
import array
import math
import os
import sys

WINDOW_S = 5
SR = 48000
CROSSTALK_LIMIT_DB = -60.0   # fixed build measures -inf; buggy build ~-4.7 dB
MIN_WET_DRY_DB = -40.0       # wet must differ from dry (else model never loaded)


def read_f32(path):
    a = array.array("f")
    with open(path, "rb") as f:
        a.frombytes(f.read())
    if sys.byteorder == "big":
        a.byteswap()
    return a


def rms(xs):
    return math.sqrt(sum(x * x for x in xs) / len(xs)) if xs else 0.0


def db(x, ref):
    if x <= 0:
        return float("-inf")
    return 20.0 * math.log10(x / ref)


def main():
    d = sys.argv[1]
    dry = read_f32(os.path.join(d, "iso_dry_r0"))
    wa = read_f32(os.path.join(d, "iso_wet_r0"))
    wb = read_f32(os.path.join(d, "iso_wet_r1"))
    n = min(len(dry), len(wa), len(wb))
    if n < 2 * WINDOW_S * SR:
        print("ISO_TEST=FAIL reason=short-render")
        return 1
    w0 = n - WINDOW_S * SR
    dry, wa, wb = dry[w0:n], wa[w0:n], wb[w0:n]

    sig = rms(wa)
    if sig < 1e-6:
        print("ISO_TEST=FAIL reason=silent-output")
        return 1
    wet_dry_db = db(rms([a - b for a, b in zip(wa, dry)]), sig)
    if wet_dry_db < MIN_WET_DRY_DB:
        print("ISO_TEST=FAIL reason=passthrough (model not loaded)")
        return 1
    xtalk_db = db(rms([a - b for a, b in zip(wa, wb)]), sig)
    ok = xtalk_db < CROSSTALK_LIMIT_DB
    print(f"CROSSTALK_DB={xtalk_db:.1f} WET_DRY_DB={wet_dry_db:.1f} "
          f"ISO_TEST={'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
