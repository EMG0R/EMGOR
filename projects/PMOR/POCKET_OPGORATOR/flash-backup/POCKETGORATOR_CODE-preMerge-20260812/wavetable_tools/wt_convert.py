#!/usr/bin/env python3
"""
wt_convert.py - convert source WAV files into the Pocket OpGorator .PWT
wavetable format.

.PWT format (16-byte header, little-endian, then int16 LE PCM):
    offset 0  : 4 bytes  magic  = b'PWT1'
    offset 4  : uint16   frameLen   (samples per single-cycle frame, default 2048)
    offset 6  : uint16   numFrames  (1 for a single-cycle table; 64 for a bank)
    offset 8  : uint32   sampleRate (informational; engine reads by phase, not Hz)
    offset 12 : 4 bytes  padding (zero)
    offset 16 : int16 LE samples, numFrames * frameLen total, normalized to +/-1.

Supported sources:
    * AKWF-style single-cycle mono WAVs (e.g. 600 samples) -> 1 frame, resampled.
    * WaveEdit 64x256 banks (a 16384-sample WAV = 64 frames of 256) -> 64 frames.
    * Generic single-cycle WAVs of any length / rate, mono or stereo.

Frame count is inferred from total length: if length is an exact multiple of a
known WaveEdit frame size (256) and >= 2 frames, it is treated as a bank;
otherwise the whole file is one single-cycle frame. Override with --frames.

Usage:
    python wt_convert.py in.wav out.PWT
    python wt_convert.py in.wav out.PWT --frame-len 2048 --frames 64
"""
import argparse, struct, sys
import numpy as np
import soundfile as sf

MAGIC = b'PWT1'

def resample_linear(x, n):
    """Resample 1-D array x to n samples with linear interpolation (wrap-safe
    for single-cycle: index n maps back to sample 0)."""
    src = len(x)
    if src == n:
        return x.astype(np.float64)
    # sample positions 0..src mapped across n output points; wrap at src
    pos = np.arange(n, dtype=np.float64) * (src / n)
    i0 = np.floor(pos).astype(np.int64)
    frac = pos - i0
    i1 = (i0 + 1) % src
    return x[i0 % src] * (1.0 - frac) + x[i1] * frac

def main():
    ap = argparse.ArgumentParser(description="Convert a WAV to Pocket OpGorator .PWT")
    ap.add_argument("infile")
    ap.add_argument("outfile")
    ap.add_argument("--frame-len", type=int, default=2048, help="output samples per frame (default 2048)")
    ap.add_argument("--frames", type=int, default=0, help="force number of frames (0 = auto-detect)")
    ap.add_argument("--src-frame", type=int, default=256, help="WaveEdit source frame size for bank detection (default 256)")
    ap.add_argument("--no-normalize", action="store_true", help="skip peak normalization")
    args = ap.parse_args()

    data, sr = sf.read(args.infile, dtype="float64", always_2d=True)  # (samples, channels)
    mono = data.mean(axis=1)  # downmix stereo/multi to mono
    total = len(mono)
    if total == 0:
        sys.exit("error: empty input file")

    # decide frame count
    nframes = args.frames
    if nframes <= 0:
        if total >= 2 * args.src_frame and total % args.src_frame == 0:
            nframes = total // args.src_frame   # WaveEdit-style bank
        else:
            nframes = 1                          # single-cycle
    if total % nframes != 0:
        print(f"warning: {total} samples not divisible by {nframes} frames; "
              f"truncating to {total - (total % nframes)}", file=sys.stderr)
        mono = mono[: total - (total % nframes)]
        total = len(mono)

    src_len = total // nframes
    frames = mono.reshape(nframes, src_len)

    out = np.empty((nframes, args.frame_len), dtype=np.float64)
    for i in range(nframes):
        out[i] = resample_linear(frames[i], args.frame_len)

    if not args.no_normalize:
        peak = np.max(np.abs(out))
        if peak > 0:
            out *= (0.999 / peak)

    pcm = np.clip(out.reshape(-1) * 32767.0, -32768, 32767).astype("<i2")

    header = MAGIC + struct.pack("<HHI", args.frame_len, nframes, int(sr)) + b"\x00\x00\x00\x00"
    with open(args.outfile, "wb") as f:
        f.write(header)
        f.write(pcm.tobytes())

    print(f"wrote {args.outfile}: {nframes} frame(s) x {args.frame_len} samples "
          f"= {len(header) + pcm.nbytes} bytes (src {total} samples @ {int(sr)} Hz)")

if __name__ == "__main__":
    main()
