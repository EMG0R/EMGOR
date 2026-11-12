#!/usr/bin/env python3
"""wavstat.py FILE.wav [--min-rms R] -> prints 'rms=.. peak=.. clip=..' ; exit 0 iff non-silent and not clipping.
Tolerates unfinalized RIFF sizes (ChucK's WvOut under Machine.crash). 16-bit PCM or 32-bit PCM/float."""
import struct, sys, math
f = sys.argv[1]; min_rms = float(sys.argv[sys.argv.index("--min-rms") + 1]) if "--min-rms" in sys.argv else 0.01
b = open(f, "rb").read()
i = b.find(b"data")
if i < 0 or len(b) < 64:
    print("no audio data"); sys.exit(1)
fmt = struct.unpack("<H", b[20:22])[0]; bits = struct.unpack("<H", b[34:36])[0]; d = b[i + 8:]
if bits == 16:
    n = len(d) // 2; s = struct.unpack("<%dh" % n, d[:n * 2]); sc = 32768.0
elif bits == 32 and fmt == 3:
    n = len(d) // 4; s = struct.unpack("<%df" % n, d[:n * 4]); sc = 1.0
else:
    n = len(d) // 4; s = struct.unpack("<%di" % n, d[:n * 4]); sc = 2147483648.0
if not s:
    print("empty"); sys.exit(1)
peak = max(abs(x) for x in s) / sc
rms = math.sqrt(sum(x * x for x in s) / len(s)) / sc
clip = sum(1 for x in s if abs(x) / sc >= 0.999) / len(s)
print("rms=%.4f peak=%.3f clip=%.1e" % (rms, peak, clip))
sys.exit(0 if (rms > min_rms and clip < 1e-5 and peak <= 1.0) else 1)
