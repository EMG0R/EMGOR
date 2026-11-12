# Sweep PLAN — finding the lowest-latency cell that survives heavy DSP

## Goal

Across the matrix of (PipeWire `clock.force-quantum`) × (Csound `-b`)
combinations, find the cell that gives the **lowest software round-trip
latency** while still producing **clean audio under heavy FX load**.

"Clean" means all four signatures stay green simultaneously:

1. zero PipeWire xrun counter increment
2. zero `pw-top` grazing (BUSY/QUANTUM < 0.85 sustained)
3. zero growth on the probe's glitch fingerprint (dropouts, spikes,
   sample discontinuities, spectral anomalies)
4. zero `journalctl` / `dmesg` xrun/underrun lines in the window

A cell where latency is lower but ANY of the four fails = not a winner.
We want the floor cell that holds all four.

## The matrix

Two hard rules constrain it:

1. **`-b` must be ≥ quantum** (from `[[project_jack_bufsize_quantum_rule]]`):
   csound's `-b` sizes the ring buffer between csound's DSP engine and
   the JACK process-callback thread. JACK delivers chunks of `quantum`
   samples per callback. If `-b < quantum`, csound's ring can't supply
   one full chunk and the JACK callback silently emits zeros — `-inf
   dB`. The cell looks healthy in `jack_lsp` and reports no xrun.
   Audio is just gone. So `-b < quantum` is not a "tighter" cell, it's
   a broken cell.

2. **`-b` larger than quantum adds csound-internal latency.** The
   excess samples have to age through the ring before reaching JACK —
   so `q=128 -b=256` ends up equivalent in total latency to
   `q=256 -b=256`, with no upside. So `-b > quantum` is just a worse
   quantum cell.

Both rules together: **the only useful cells are `-b = quantum`.** That
collapses the per-quantum axis to a single point. Also trim the *top*:
`q=512` is 21.3 ms RTT, known too high for the use case, dropped
without testing.

| #  | quantum | `-b`  | `-B`  | `ksmps` | RTT (2Q/48k) | role                          |
| -- | ------- | ----- | ----- | ------- | ------------ | ----------------------------- |
| 1  | 256     | 256   | 2048  | 64      | 10.7 ms      | safe ceiling / sanity         |
| 2  | 128     | 128   | 1024  | 64      |  5.3 ms      | mid                           |
| 3  |  64     |  64   |  512  | 32      |  2.7 ms      | aggressive                    |

3 cells total. Each cell ≈ 20 s wall time (~3 s warm-up + 15 s probe +
~2 s teardown). Full sweep ≈ 1 minute. The user's glitch fires
instantly on FX engagement, so 15 s × 60 impulses is enough sampling
to surface it.

`-B` = `8 × -b` per cell — headroom against CPU spikes, the source of
`-B 1024` for `-b 128` in `[[project_csound_jack_buffer]]`. `-B` is
distinct from `-b`: it sizes csound's overall I/O buffer (periods of
`-b`-sized chunks) and adds robustness to transient spikes without
adding latency the way `-b` itself does. If a cell fails by grazing
only, varying `-B` (e.g. 4× vs 8× vs 16×) is the v2 second-axis sweep
to do before giving up on the quantum.

`ksmps` divides `-b`. `min(64, -b)` keeps it from getting silly at
`-b 64`.

## What runs in each cell

```
for each (quantum, -b) in matrix:

  1. set runtime quantum:
       pw-metadata -n settings 0 clock.force-quantum <quantum>
     (this is the runtime override; it bypasses the static pin in
     [[project_pipewire_rate_pin]] for the duration of the sweep and
     reverts when we set it back to 0 at sweep end)

  2. stop the running chain csound (the sweep owns the slot for now):
       systemctl --user stop demiurge.service
     OR pkill the running csound — whichever the user prefers; sweep
     runner doesn't enforce this, plan does.

  3. start sweep-controlled csound with these args:
       csound ~/NEPTR_phase4/csound/neptrPhase4.csd \
              -b <b>  -B <B>  --ksmps=<ksmps>  -m 4 \
              -+rtaudio=jack -+rtmidi=alsaseq -Ma
     stderr → <rundir>/csound.log so xrun-watch can scan it

  4. wait for csound to register JACK ports (poll jack_lsp ≤ 5 s)

  5. wait 1 s for the graph to settle, then have the operator (or a
     pre-scripted menu navigation) position the 5 heavy FX in the slots
     IF not already positioned. v1 plan: skip the menu nav, run the
     sweep against whatever FX are currently in slots and document
     in report.

  6. xrun-watch.sh start <rundir>

  7. stress.sh hold 17 &     # 5 slots ON for 17 s (probe is 15 s; pad)

  8. probe.sh 15
       → produces report.txt for this cell

  9. wait for stress.sh to finish (it will toggle slots OFF on exit)

  10. xrun-watch.sh stop <rundir> <rundir>/csound.log
       → produces xrun-report.txt

  11. kill the sweep-controlled csound

  12. append a single row to the sweep summary table:
       quantum, -b, rtt_ms, jitter_ms, dropouts, spikes, discont,
       spec_anom, xrun_count, grazing_nodes, journal_hits, dmesg_hits,
       verdict
```

At end:

```
pw-metadata -n settings 0 clock.force-quantum 0        # revert
systemctl --user start demiurge.service                # hand chain back
```

## Verdict rule (per cell)

`verdict = PASS` if and only if all of:

- `xrun_count == 0`
- `grazing_nodes == 0`
- `journal_hits == 0`
- `dmesg_hits == 0`
- `dropouts == 0`
- `spikes == 0`
- `discontinuities ≤ 5` (probe's "ok" threshold)
- `spectral_anomalies ≤ 5` (probe's "ok" threshold)

Anything else: `WARN` if only one metric is non-zero, `FAIL` if two or
more. PASS-WARN-FAIL is a coarse handle for scanning the summary table.
The numbers themselves are what matters for decisions.

## "Winner" rule — best latency-to-DSP-headroom ratio

Take the **lowest-latency `PASS` cell**. With 3 cells one per quantum,
this is just "lowest quantum that passes." `max_busy_pct` from
`pw-top` is reported alongside as the headroom metric — if it's close
to 85%, the winner is borderline and v2 should re-test with larger
`-B` for more headroom at the same latency.

If no PASS cell exists: the chain cannot run heavy DSP cleanly at any
software latency we tried. Action items go to the Tier-2/Tier-3
recommendations from the earlier latency analysis — PREEMPT_RT, IRQ
pinning, single-process csound on raw ALSA.

## Sweep summary table format

One row per cell. Tab-separated for grep/awk friendliness, with a
header so a human can read it directly:

```
quantum  -b   rtt_ms  jit_ms  drops  spikes  discont  specA  xruns  graz  busy%  jhits  dhits  verdict
256     256   10.92   0.04    0      0       2        2      0      0      48     0      0     PASS
128     128    5.78   0.04    0      0       2        3      0      0      62     0      0     PASS
 64      64    3.18   0.09    0      1       6        5      1      3      88     0      0     WARN
```

(Numbers above are illustrative.) `busy%` is the peak BUSY/QUANTUM
ratio seen on csound's node during the cell, from `pw-top`. Among
PASS cells, lower is better — more headroom for further FX load.

## Below-xrun glitch detection — five independent signals

These are the five we cross-check before calling a cell clean.

| signal                                | where it comes from                                                                                       | catches                                                  |
| ------------------------------------- | --------------------------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| 1. PipeWire `xrun-count` per node     | `pw-dump` JSON, delta before vs after                                                                     | classic xruns the scheduler observed                     |
| 2. PipeWire BUSY/QUANTUM ratio        | `pw-top -b -n 1` ratio column                                                                             | grazing — DSP eating 85+% of the cycle (the "no-xrun glitch") |
| 3. `journalctl --user` log scan       | grep `xrun\|underrun\|overrun\|late\|missed`                                                              | PW/WP-side warnings that didn't increment xrun-count     |
| 4. `dmesg` log scan                   | grep `xhci\|usb_audio\|snd_usb\|underflow`                                                                | USB-Audio class buffer issues — happen below PW awareness |
| 5. csound stderr scan (`-m 4`)        | grep `underrun\|buffer\|late\|missed\|cannot\|fail`                                                       | csound's own RT health complaints                        |
| 6. probe's glitch fingerprint         | RMS dropouts, RMS spikes, sample discontinuities, STFT centroid                                           | audio-data-level corruption that left no log evidence    |

Signal 2 is the load-bearing one for the user's symptom. Classic xruns
require the scheduler to notice a missed deadline; grazing produces no
xrun but corrupts the audio anyway (cache thrashing, IRQ preemption,
DSP returning in 99% of the budget so jitter cascades). `pw-top`'s
BUSY/QUANTUM ratio is the most reliable signal we have for that state.

## Pieces required

| piece                         | exists? | path                                            |
| ----------------------------- | ------- | ----------------------------------------------- |
| signal injector               | ✓       | `testing/probe/inject.csd`                      |
| probe orchestration           | ✓       | `testing/probe/probe.sh`                        |
| probe analyzer                | ✓       | `testing/probe/analyze.py`                      |
| MIDI injector (low-level)     | ✓       | `neptr/scripts/claude_midi.sh`                  |
| stress wrapper                | ✓       | `testing/sweep/stress.sh`                       |
| xrun + grazing watcher        | ✓       | `testing/sweep/xrun-watch.sh`                   |
| **sweep runner**              | ⏳      | `testing/sweep/sweep.sh` — not yet written      |
| **summary table builder**     | ⏳      | inline in sweep.sh or a small Python collector  |

The runner is the only piece missing. It's mechanical given the rest:
loop over the matrix, call into the existing pieces, collect outputs,
write the summary. Reason it's not yet written: I want operator
review of this plan before the script that actually stops and starts
the live chain exists. Stopping `demiurge.service` for ~1 minute is
a real operation; the runner should not be a surprise.

## What the operator does once, before sweep

1. Plug in the Scarlett, confirm chain is up: `systemctl --user status demiurge.service`.
2. Position the 5 heavy FX into the 5 slots manually (granular,
   spectraldelay, pitchshifter, reverb, formant — or whatever the
   current heavy set is). The sweep will toggle them ON/OFF; it does
   not select them.
3. Mute the speakers or use headphones — the sweep emits the probe's
   click train through the chain at -6 dBFS for ~1 minute.
4. Ensure no other audio process is running (browser tabs, etc).

Then: `cd ~/testing/sweep && ./sweep.sh` (once written).

## Operator outputs

After a full sweep:

```
testing/sweep/runs/<timestamp>/
  summary.tsv              ← the table above
  cells/
    q512-b128/
      capture-*.wav        ← from probe
      report.txt           ← from probe
      xrun-report.txt      ← from xrun-watch
      csound.log
      pwtop-{before,after}.txt
      pw-{before,after}.json
    q512-b256/
      ...
    ...
  winner.txt               ← single line: "quantum=128 -b=128 rtt=5.78ms PASS"
```

Per-cell artifacts let us go back and inspect the raw capture and the
raw counts if a verdict looks suspicious. Don't trust the summary
without the artifacts surviving alongside.

## Iteration after sweep

The matrix above is intentionally a single point per quantum because
of the `-b ≥ quantum` constraint. After v1:

- If the winner is at `q=128` and `max_busy_pct` is comfortably under
  ~70%, try `q=96` and `q=80` (PW does support non-power-of-2 quantums)
  in a small follow-up sweep. Each step down from 128 saves ~0.7 ms.
- If the winner is `q=64` borderline (PASS at high `busy%` or
  occasional WARN), sweep `-B` next: 4×, 8×, 16× × `-b`. Larger `-B`
  adds robustness without adding latency.
- If the winner is at `q ≥ 256`, the platform is not heavy-DSP-stable
  below that *with the current FX selection*. Either swap heavy FX out
  or pursue the Tier-2 software fixes (PREEMPT_RT, per-IRQ pin, sched
  deadline).
- If results vary between runs of the same cell, that's a stability
  problem on its own — re-run each cell ≥ 3 times and report median +
  worst-case before trusting the table.

## What the plan deliberately does NOT cover (yet)

- **Total RTT including ADC + USB + DAC.** That needs a physical
  output→input loopback cable plus the alsa_input as the probe
  reference. Software RTT (csound-input → demiurge-sink) is what we're
  sweeping for now; hardware overhead is fixed across cells and
  doesn't change the ranking, only the absolute floor.
- **PREEMPT_RT kernel and per-IRQ pinning.** Those are platform-side
  changes, not sweepable per-cell. They become candidate Tier-2 actions
  if the sweep reveals no cell holds heavy DSP below ~10 ms.
- **Hot-loadable FX changes mid-cell.** Cells are FX-set constant. We
  do not switch which FX is in which slot during a sweep; that's a
  user pre-step.
- **Multiple FX-set presets.** v1 sweeps one preset (the user's
  currently-loaded slots). Future v2: sweep across a small set of
  representative FX combinations (light / medium / heavy) and produce
  one matrix per preset.

## What "no plan" pieces stay user-controlled

- Which FX go in which slot, before the sweep starts.
- Whether to stop `demiurge.service` cleanly or pkill the running
  csound — the runner will accept either via a flag.
- Whether to use 15 s probe windows or longer for stricter cells
  (`probe.sh 30` for a borderline winner re-check, for example).
  Default stays 15 s for the v1 sweep.
- Whether to enable `--realtime` and `--ksmps=…` overrides in csound.
  Defaults from this plan are the v1 starting point; the runner should
  expose them as overridable env vars.
