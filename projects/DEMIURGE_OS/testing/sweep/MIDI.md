# Remote MIDI control for the sweep

The sweep needs to drive the chain into heavy DSP load while the probe
measures, so the report reflects *under-load* behaviour, not idle. We
do this with MIDI. The MIDI surface already exists — we don't build a
new one, we just call into it.

## Canonical injector

`neptr/scripts/claude_midi.sh` is the existing SSH-friendly MIDI
injector. It writes raw MIDI bytes via `aseqsend` to the kernel
"Midi Through" ALSA seq port (typically `14:0`). The live csound
engine and ChucK are both subscribed to that port via `aconnect`,
so one send fans out to everything that listens.

Single source of truth. Do not duplicate. The script's location on
disk:

| where    | path                                       |
| -------- | ------------------------------------------ |
| local    | `neptr/scripts/claude_midi.sh`             |
| on Pi    | `~/NEPTR_phase4/scripts/claude_midi.sh`    |

Subcommands:

| cmd                             | what                                                    |
| ------------------------------- | ------------------------------------------------------- |
| `cc <num> <val> [ch]`           | send a CC (default ch1)                                 |
| `on <note> <vel> [ch]`          | note on                                                 |
| `off <note> [ch]`               | note off                                                |
| `tap <cc> [ch]`                 | press + release (50 ms hold) — the FX-toggle pattern    |
| `raw <hex bytes ...>`           | arbitrary MIDI                                          |
| `ports`                         | list ALSA seq ports                                     |
| `map`                           | NEPTR Phase 4 CC cheat-sheet                            |

## NEPTR Phase 4 CC map (relevant to the sweep)

From `claude_midi.sh map` and `neptr/csound/midi_handler.orc`:

```
FX slot toggles (button "tap"), channel 1:
  CC74  Enc5  slot 0
  CC75  Enc6  slot 1
  CC76  Enc7  slot 2
  CC71  Enc2  slot 3
  CC72  Enc3  slot 4

Slot parameter rotation, channel 1 (0..127 absolute):
  CC24  Enc5  slot 0 param
  CC25  Enc6  slot 1 param
  CC26  Enc7  slot 2 param
  CC21  Enc2  slot 3 param
  CC22  Enc3  slot 4 param

Menu navigation, channel 1 (127 = step CW, 0 = step CCW):
  CC20  Enc1  menu prev
  CC23  Enc4  menu next

Arcade button (note trigger + ChucK clock), channel 1:
  CC68  press = 127, release = 0

Master clock, channel 16:
  CC119 BPM broadcast (read-only — clock writes this)
  CC118 BPM steer (write to nudge demiurge-clock; 0..127 → 40..400 BPM)
```

## Heavy-DSP FX (effects that move the load needle)

From `neptr/csound/effects/`. The ones likely to push CPU 3 hardest:

| FX             | why heavy                                  |
| -------------- | ------------------------------------------ |
| `granular.orc` | granular synthesis, lots of grain bookkeeping per k-period |
| `spectraldelay.orc` | full-band FFT delay, dominated by transform cost |
| `pitchshifter.orc`  | phase vocoder FFT + resynthesis |
| `reverb.orc`        | feedback network or convolution |
| `formant.orc`       | parallel BPF banks, modulated |
| `mood.orc`          | typically moog-ladder + extra modulation |

For a stress pattern, pre-position the menu so these sit in the 5 FX
slots, then toggle all five slots ON. The `stress.sh` wrapper does the
"toggle all" half; setting which FX is in which slot is operator work
(navigate the menu manually before launching the sweep).

## stress.sh — the wrapper

`testing/sweep/stress.sh` is a thin layer over `claude_midi.sh`. It
exists so the sweep runner has one consistent "now load the chain"
verb regardless of which FX are selected.

```
stress.sh hold <sec>     # toggle 5 slots ON, sleep <sec>, OFF on exit
stress.sh on             # toggle 5 slots ON, return (caller must `off`)
stress.sh off            # toggle 5 slots OFF
stress.sh churn <sec>    # like hold, but also rotate menu+params during
```

`hold` and `churn` install an EXIT trap that re-taps every slot,
returning the chain to "all-off" even if the script is killed mid-run.

## Adding sweep-relevant CCs to the chain

If a future sweep wants to vary a parameter we don't currently expose
via MIDI (e.g. a specific FX's wet/dry, a per-slot bypass), add the
mapping in `neptr/csound/midi_handler.orc` first — that file is the
authoritative table for the live chain. Once a CC reads into a gk_ var
there, the sweep can drive it via `claude_midi.sh cc <num> <val>` with
no further plumbing.

That's also the only place where we extend the MIDI surface: never in
the test harness, never as a parallel pathway. The test harness is a
*consumer* of MIDI, not a definer of it.
