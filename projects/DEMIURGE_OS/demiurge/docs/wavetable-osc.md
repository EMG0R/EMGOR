# Wavetable OSC — the NEPTR WAVE screen → your patch

The NEPTR UI's **WAVE** tab (companion mode, 5th tab) is a finger-drawn
single-cycle wavetable editor with a log-spaced 64-harmonic bar view (draw
either view, both stay consistent). On every edit commit (finger lift +
0.25 s debounce, also after CLEAR/UNDO) the UI sends the table over OSC.

**Synthesis itself is patch-side.** The UI only publishes the table — what a
patch does with it (oscillator, LFO shape, waveshaper transfer…) is entirely
up to the receiving orchestra. Nothing in the stock chain consumes these
messages until a patch opts in.

## Target

`live.conf` keys (read once at NEPTR-UI start — restart `neptr-ui` to apply):

```
wavetable_osc_host = 127.0.0.1    # default
wavetable_osc_port = 8001         # default (UI listens on 8000 — keep distinct)
```

Dev override without touching live.conf: `NEPTR_WT_OSC_HOST` / `NEPTR_WT_OSC_PORT`.

## Messages (per commit, in this order)

| Address | Typetag | Payload |
|---|---|---|
| `/neptr/wavetable` | `,b` | One blob: **2048 × float32, big-endian** — the full cycle, −1..1. For blob-capable receivers (liblo C, Max, SC, python-osc with `max_packet_size` raised ≥ 16 k — the default 8192 silently drops the ~8.2 kB datagram). |
| `/neptr/wavetable/chunk` × 128 | `,fffffffffffffffff` (17 f) | `offset, s[offset] .. s[offset+15]` — the same table as 128 small messages. **This is the Csound path**: Csound 6's `OSClisten` has no blob type, and 17 float args is comfortably legal (verified on Csound 6.18). |
| `/neptr/wavetable/end` | `,f` | `2048` — table-complete marker; copy/refresh your working ftable when you see it. |
| `/neptr/wavetable/harmonics` | 129 floats | `64, mag1..mag64, phase1..phase64` — the 64-partial decomposition (phases in radians, `a·cos + b·sin` convention, DC dropped). For spectral receivers / additive resynthesis at any table size. |

Both encodings carry identical data; consume whichever suits the receiver.

## Csound receiver (validated against Csound 6.18)

Paste-ready: fills ftable `100` (2048 points) from the chunk stream and marks
it fresh on `/end`. Tested end-to-end (UI → this snippet): all 2048 samples
land bit-exact.

```csound
giOSCwt OSCinit 8001          ; = wavetable_osc_port
giWT    ftgen   100, 0, 2048, 2, 0     ; landing table, GEN02 zeros

instr WavetableRx
  kOff init 0
  kV0 init 0
  kV1 init 0
  kV2 init 0
  kV3 init 0
  kV4 init 0
  kV5 init 0
  kV6 init 0
  kV7 init 0
  kV8 init 0
  kV9 init 0
  kV10 init 0
  kV11 init 0
  kV12 init 0
  kV13 init 0
  kV14 init 0
  kV15 init 0
 nxtmsg:
  kk OSClisten giOSCwt, "/neptr/wavetable/chunk", "fffffffffffffffff", kOff, kV0, kV1, kV2, kV3, kV4, kV5, kV6, kV7, kV8, kV9, kV10, kV11, kV12, kV13, kV14, kV15
  if kk == 0 goto donemsg
  tablew kV0,  kOff,      giWT
  tablew kV1,  kOff + 1,  giWT
  tablew kV2,  kOff + 2,  giWT
  tablew kV3,  kOff + 3,  giWT
  tablew kV4,  kOff + 4,  giWT
  tablew kV5,  kOff + 5,  giWT
  tablew kV6,  kOff + 6,  giWT
  tablew kV7,  kOff + 7,  giWT
  tablew kV8,  kOff + 8,  giWT
  tablew kV9,  kOff + 9,  giWT
  tablew kV10, kOff + 10, giWT
  tablew kV11, kOff + 11, giWT
  tablew kV12, kOff + 12, giWT
  tablew kV13, kOff + 13, giWT
  tablew kV14, kOff + 14, giWT
  tablew kV15, kOff + 15, giWT
  kgoto nxtmsg                 ; drain every queued chunk this k-cycle
 donemsg:
  kNum init 0
  ke OSClisten giOSCwt, "/neptr/wavetable/end", "f", kNum
  if ke == 0 goto out
  ; --- table is complete & consistent here ---------------------------------
  ; Play it directly (GEN-ready: any oscil/oscili/poscil can read giWT), or
  ; snapshot it into a private table if you resample/normalize first:
  ;   tableicopy giMyOsc, giWT      (i-time)  — or tablecopy at k-time
  printks "wavetable updated (%d samples)\n", 0, kNum
 out:
endin
```

Turn it on with `alwayson "WavetableRx"` (or a score `i` statement). Reading
`giWT` while a burst is mid-flight only matters for the ~ms the 128 chunks
take; keying off `/end` (as above) avoids even that.

Notes
- The drawn cycle is raw finger data, clamped to ±1 but **not normalized**;
  a bar-sculpted cycle is a 64-partial resynthesis. Normalize patch-side if
  levels matter.
- `oscili` on a 2048-point table interpolates fine at audio rate; for a
  band-limited oscillator across the keyboard, rebuild from
  `/neptr/wavetable/harmonics` instead (GEN09/GEN19-style additive per
  octave), which is exactly why the partial list is published.
- The UI listens on port 8000 for engine → UI state; never point
  `wavetable_osc_port` at 8000.
