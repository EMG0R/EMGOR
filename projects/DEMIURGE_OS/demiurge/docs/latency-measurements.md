# Measured latency — DEMIURGE / NEPTR

**The authoritative record of MEASURED round-trip latency on this rig.**

Every number in this file was produced with a physical cable from the audio
interface's output back into its own input. No number in this file is derived
from buffer arithmetic. That distinction is the entire reason the file exists —
see [Measured vs inferred](#measured-vs-inferred), which documents where the
arithmetic this project ran on for months was wrong.

- **Rig:** Raspberry Pi 5, `gorpi7@demiurge.local`
- **Interface:** HiFiBerry DAC+ADC Pro (platform I2S, ALSA card `sndrpihifiberry`,
  PipeWire node `alsa_*.platform-soc_107c000000_sound.stereo-fallback`)
- **First measured:** 2026-08-10

> **All measurements on this page are on the HiFiBerry.** There is no USB
> interface attached to this Pi (`aplay -l` shows only the two HDMI outputs, the
> HiFiBerry, and the snd-aloop virtual card), despite the shipped `live.conf`
> still carrying `audio_interface = hw:CARD=USB,DEV=0`. That line resolves to
> nothing and everything falls through to the HiFiBerry. USB numbers from this
> file do not exist; do not infer them.

---

## Headline

**The rig's round trip is 5.5 ms. The instrument's round trip was 29 ms.
The 23.6 ms difference was one wrong constant in our own wrapper.**

| | frames | ms |
|---|---|---|
| Rig floor — graph → DAC → cable → ADC → graph, no engine | 261.5 | **5.45** |
| Through csound as shipped (`-B 1024`) | 1392.8 | **29.02** |
| Through csound at `-B 128` | 397.2 | **8.28** |

`demiurge-run-csound` floored csound's `-B` (the ring between csound's engine
thread and the JACK callback) at 1024 frames, with a comment claiming csound
refuses anything lower on this Pi. Measured against a cable, that claim is
false, and believing it cost **20.7 ms** — more than every other latency lever
in this project combined.

The engine is not the problem and the instrument is not the problem. A buffer
constant was.

---

## Method

### The tool

`testing/latency/rtdelay.c` — a small JACK client (run through `pw-jack`, so it
attaches to PipeWire) that emits an 8-sample broadband click and counts frames
until the click returns on its input. Each shot is independent and the result is
an **integer sample offset**, so the figure is a count, not an estimate.

```
ssh gorpi7@demiurge.local
cd ~/demiurge/testing/latency && make
./rtdelay.sh 40 250        # 40 shots, 250 ms apart
```

`rtdelay.sh` is out-of-band in the same sense as the rest of `testing/latency/`:
it parks the chain's output link to the DAC for the measurement window, restores
it on exit (including on Ctrl-C), never edits `live.conf`, and never stops
`demiurge.service`. The chain keeps running throughout, so the CPU load under
measurement is the real one.

Onset detection is peak-relative (first sample above 20 % of the shot's own
peak), not a fixed threshold, because the DAC reconstruction filter and ADC
anti-alias filter smear the click into a short ringing burst whose absolute
amplitude depends on analogue gain we do not control. Peak-relative detection
makes the number comparable across gain settings and across cards.

### Why not `jack_iodelay`

`jack_iodelay` is installed on the Pi and was used for the first reading. It
reports a continuously phase-tracked estimate, and on this rig that estimate
**drifts** — about 1.3 frames/s over a 70 s run. A drifting estimate cannot
produce a mean and a spread that mean anything. `rtdelay` fires independent
shots so drift shows up as spread across shots instead of a silently sliding
number.

(`latency.sh internal` remains blocked on `jack_capture`, which is not packaged
for this Pi. `rtdelay` supersedes that path for round-trip work.)

---

## Results

### Confirmed

| Config | sync_layer | quantum | playback ring | capture ring | rate / format | Shots | Round-trip mean | sd | min–max | Input signal |
|---|---|---|---|---|---|---|---|---|---|---|
| **As found, 2026-08-10** | on | 64 | 128 × 4, hr 0 | 1024 × 32, hr 32 (stock) | 48 kHz S32_LE | 20 | **265.4 fr / 5.53 ms** | 4.9 fr / 0.10 ms | 248–271 fr (5.17–5.65 ms) | silence (chain parked off DAC; csound still computing) |

That row is the one to quote. It is the only measurement so far taken on a
system in a known-good state, and its spread is tight (sd 0.1 ms).

### csound's own cost — the `-B` sweep

Measured with `testing/latency/passthrough.csd`, an empty csound program (read
input, write output, no DSP) run out-of-band, so everything it adds is the
engine and its JACK backend rather than any patch. Quantum 64, capture ring
128 × 16, 30 shots each. "csound's cost" is the row minus the 261.5-frame rig
floor measured the same afternoon.

| `-B` | round trip | csound's own cost | sd |
|---|---|---|---|
| 1024 (as shipped) | 1392.8 fr / 29.02 ms | +23.6 ms | 79.3 fr |
| 512 | 789.0 fr / 16.44 ms | +11.0 ms | 11.0 fr |
| 256 | 525.7 fr / 10.95 ms | +5.5 ms | 6.3 fr |
| **128** | **397.2 fr / 8.28 ms** | **+2.83 ms** | **0.9 fr** |
| 64 | refuses | — | — |

Three things worth reading off this table:

1. **The relationship is roughly `-B` frames of latency.** 1024 → 512 saves
   ~12.6 ms; 512 → 256 saves ~5.5 ms. The ring is sitting directly in the
   signal path, not acting as slack.
2. **Lower is also MORE stable, dramatically.** sd falls from 79.3 frames to
   0.9. The jitter this project spent an afternoon attributing to converters,
   to clock drift, and to the ALSA capture ring was substantially csound's
   1024-frame ring.
3. **`-B 64` fails for an unrelated reason** and is not evidence of a ring
   floor: csound derives `-b` from `-B`, lands on `-b 32`, and `-b` must be an
   integer multiple of `ksmps`.

**The real floor is `ksmps`, and it belongs to the patch, not the system.**
`-B` cannot go below `ksmps`, so the orchestra's control block sets the limit.

> **Correction, 2026-08-10:** this paragraph originally said NEPTR is written at
> `ksmps = 128`. It is not. `globals.orc` has been macro-driven since 2026-07-26
> — `#ifndef KSMPS / #define KSMPS #64#` — and defaults to **64**, overridable
> from `live.conf` with `csound_extra = --omacro:KSMPS=<n>` and no patch edit.
> (The literal `ksmps = 128` still in `globals.csd` belongs to a different file
> that the `.csd` does not include.) So `-B 128` was NOT at the patch's floor,
> and quantum 32 is not blocked by the orchestra either.
Lowering it further is a change to the instrument, not to the OS — and at
2.83 ms of engine cost against 5.45 ms of unavoidable converter-and-graph
latency, there is very little left to win.

### quantum 32 — achievable, measured, and deliberately NOT shipped

Measured 2026-08-10 on the HiFiBerry, sync layer on, UI and touch daemon
running, both windows 120 s, both against the SAME (current) patch code.

| Config | quantum | ksmps | period-size × num | csound CPU (1 core) | csound `rtjack: xrun` | pw-top ERR (interface) |
|---|---|---|---|---|---|---|
| shipped | 64 | 64 | 128 × 4 | **36.7 %** | 0 | 3 |
| experiment | 32 | 32 | 64 × 4 | **56.1 %** | 0 | 0 |

**It works.** csound ran a true 32-frame period — verified from the process
cmdline (`-B 64 -b 32` with `--omacro:KSMPS=32`), not from what live.conf asked
for. That distinction matters: at ksmps 64 a requested `-b 32` is silently
rounded UP to 64, so a naive "quantum 32" test measures a 64-frame period and
reports success for the wrong reason. Set ksmps first or you are measuring
nothing.

**The gain is ~1.33 ms round trip** (one quantum on the way in, one on the way
out; the `-B` ring halves with it, 128 → 64).

**The cost is ~19 points of CPU on the one core that must never miss a
deadline** — a 1.5× increase in DSP cost, which is what doubling the k-rate
work of a ~300-stage chain buys you. Both configs showed zero engine xruns over
their windows, so there is no stability signal *yet*; the margin is precisely
what protects against the load spikes that were not measured, and the user can
switch in more effects at any moment.

**Recommendation: stay at 64.** 1.33 ms off an 8.28 ms round trip is a 16 %
reduction in a figure that is already below what the instrument's own attack
transients resolve, and it is bought with a third of the remaining headroom
plus a period-size in the neighbourhood of the one that hard-locked this board
into a watchdog reboot. Trading dropout risk for imperceptible latency is a bad
trade on a live instrument.

The path is left OPEN, not blocked: `demiurge-audio quantum 32` will do the
whole thing (lower the period-size in lockstep, refuse if ksmps is wrong) the
moment someone sets `csound_extra = --omacro:KSMPS=32`. What is blocked is
doing it by accident.

Structural prerequisite, verified across every `pvsanal`/`pvsinit` site in the
patch: the smallest FFT hop is **128** (`onset_pick.orc`, `attack.orc`; every
other site is 256, 512 or 1024). The `hop >= ksmps` invariant that crashed the
patch once therefore holds at ksmps 32 with 4× margin.

### Embedded Csound host — the ring removed, and what that did and did not buy

Measured 2026-08-10, quantum 64, ksmps 64, 48 kHz S32_LE, sync_layer on, cable
loop, 40 shots per row, all four rows in a single window so nothing about the
rig could drift between them (`testing/latency/ab-csound-host.sh`).

`src/demiurge-csound-host.cpp` is a JACK client that embeds Csound via its C API
and calls `csoundPerformKsmps()` from inside the process callback, so the
engine-thread ring (`-B`) does not exist at all. At quantum 64 with ksmps 64 that
is exactly one control period per callback.

| Row | What is in the return path | mean | sd | min–max |
|---|---|---|---|---|
| floor (`rtdelay.sh`) | nothing | 268.4 fr / 5.59 ms | 9.0 fr | 239–280 fr |
| **null client** | a JACK client that only `memcpy`s | **256.2 fr / 5.34 ms** | **0.6 fr** | 255–258 fr |
| **standalone** `-B 128 -b 64` | `csound -+rtaudio=jack` | **385.0 fr / 8.02 ms** | **0.0 fr** | 385–385 fr |
| **embedded, Csound bypassed** | the host's JACK plumbing only | **257.3 fr / 5.36 ms** | **1.2 fr** | 256–262 fr |
| **embedded, Csound computing** | `csoundPerformKsmps()` in the callback | 235–380 fr / 4.9–7.9 ms | 30–83 fr | 193–566 fr |

Read those rows carefully, because three of them are excellent and one is not.

**1. The `-B` model is confirmed exactly.** Standalone minus null client is
385.0 − 256.2 = **128.8 frames**, against a `-B` of 128. The ring is in the
signal path at face value, and the standalone row's sd of **0.0** says it is
also perfectly deterministic — every one of 40 shots returned 385 frames.

**2. The host's JACK plumbing costs nothing, as designed.** With
`DEMIURGE_HOST_BYPASS=1` — same client, same ports, same callback, samples
copied straight through without Csound — the host measures 257.3 fr against the
256.2 fr null-client baseline, sd 1.2. That is zero added latency to within a
frame, and it is the claim the design was making: **the ring can be removed, and
removing it is worth the 128 frames (2.68 ms) the standalone backend spends.**

**3. But with Csound actually computing in the callback, the result is not
stable, and that is a blocker.** Across four runs the mean moved between 235 and
380 frames with sd 30–83, individual shots ranged 193–566, and some shots MISSed
outright. Per-shot traces show a slow monotonic slide (~60 frames/s) punctuated
by jumps back up — a phase that slips rather than a delay that is merely long.
The cause is inside `csoundPerformKsmps`, not in the plumbing: rows 2 and 4
differ only by whether that call happens. Ruled out so far: `--realtime` (drifts
identically without it), stale `spin`/`spout` pointers (re-fetching them every
callback changes nothing), and orphaned engine instances (the numbers reproduce
on a verified-clean graph).

**Conclusion: `csound_host = embedded` is not production-ready, and
`standalone` remains the default and the shipped path.** The prize is real and
now measured — 128 frames, 2.68 ms, with a mechanism proven to deliver it — but
a round trip whose phase slides is worse than one that is 2.68 ms longer and
dead stable.

#### 2026-08-11: the audio path is exact; the failure handling was not

Two later results supersede the "unstable" reading above.

**The host adds exactly zero, and this is now measured without any hardware at
all.** Wire `rtdelay` straight into a client and back — `rtdelay:out ->
client:input1 -> client:output1 -> rtdelay:in` — and there is no DAC, no cable,
no chain to park, so the number is the client and nothing else. 20 shots each:

| client | every shot | sd |
|---|---|---|
| null client (memcpy only) | 64 fr | 0.0 |
| **host, Csound computing** | **64 fr** | **0.0** |
| host, Csound bypassed | 64 fr | 0.0 |

64 frames is one quantum, the floor for any JACK client. Csound computing inside
the callback is bit-for-bit indistinguishable from a `memcpy`. Separately,
`csoundGetCurrentTimeSamples() - hostFrames` held at **+0** across 45,004
callbacks and 2,880,256 frames.

**The earlier "phase wandering" was the rig, not the engine.** Re-run on the
hardware loop with three clients in identical topology, back to back: null
client sd 68.2, host-with-Csound sd 88.7, host-bypassed sd 71.6, bare floor
sd 71.1 — all wandering together over the same 230–465 frame range. A nine-line
`memcpy` client reproduces the effect in full. Earlier the same null client
measured sd 0.6 on that topology, so the measurement path itself changes state
between sessions; treat any single-session spread as suspect and always run a
null-client row in the same window.

**What actually stopped it shipping was fault handling.** Deployed as the live
backend it came up correctly — all four ports registered, linked both
directions, MIDI subscribed to Midi Through in both directions with live CCs
observed, UI and touch up — played, and then during a 120 s soak deregistered
every JACK port while the process stayed alive at 0 % CPU. A silent rig with a
healthy-looking process. Reverted to standalone.

The host now registers `jack_on_shutdown` plus a periodic `jack_port_is_mine`
backstop, so it exits instead of idling invisibly. **That makes the failure
visible, not survivable**, and the distinction matters:

> `demiurge-launcher`'s `Event::ChildExited` marks the stage dead and re-applies
> the graph. **Nothing respawns it.** `demiurge.service`'s `Restart=on-failure`
> restarts the *launcher*, which does not exit when one stage of several dies.
> So any engine death is a silent instrument until someone restarts by hand —
> and this is NOT specific to the embedded host: the standalone backend was
> found alive with no audio path twice the same night.

Stage respawn in the launcher (or re-registration in the host) is the real fix,
and it is worth more than the 2.68 ms this whole exercise is chasing.

#### A measurement trap this work uncovered

`rtdelay.sh` and `rtdelay-through.sh` measure **different graphs**, and the
difference between them is not "what the client costs":

```
rtdelay.sh          rtdelay:out -> DAC -> cable -> ADC -> rtdelay:in
rtdelay-through.sh  rtdelay:out -> DAC -> cable -> ADC -> <client> -> rtdelay:in
```

Inserting any client changes the graph's topological order and with it
PipeWire's scheduling of `rtdelay` relative to the capture node. Subtracting the
first from the second produced an impossible result here — a round trip *through
an engine* that came out ~40 frames **below** the floor, reproducibly, three
times — before the cause was understood.

`testing/latency/nullclient.c` exists to fix this: a JACK client that copies
input to output and nothing else, giving the through-graph its own zero point
(256.2 fr, sd 0.6). **Difference engines against the null client, not against
`rtdelay.sh`.** The `-B` sweep earlier on this page used the floor as its
baseline, so its "csound's own cost" column is inflated by roughly the 12-frame
gap between the two topologies; the *shape* of that table is unaffected and its
conclusion stands.

#### Two operational hazards found the hard way

- **The Pi hard-reset during this work.** Orphaned `demiurge-csound-host`
  processes accumulated (SSH invocations that backgrounded the host without
  detaching its stdio, so the session hung and the kill never ran). Unlike a
  stray standalone Csound — which computes on its own thread behind a ring —
  every instance of the embedded host runs a full orchestra inside a SCHED_FIFO
  callback, so several at once is a genuine RT-livelock risk, and this rig's
  `RuntimeWatchdogUSec` turns that into a board reset. The host now opens its
  JACK client with `JackUseExactName`, so a second instance **refuses to start**
  instead of registering as `csound6-1` and becoming a second RT load.
  `ab-csound-host.sh` refuses to measure through a dirty graph at all.
- **`PIPEWIRE_LATENCY` is not set outside the launcher.** `demiurge-env.sh`
  derives it from `DEMIURGE_QUANTUM`, which only the launcher exports, so any
  hand-launched Csound falls back to 128. Ask for `-b 64` against that and
  rtjack refuses — `*** rtjack: buffer size (-B) is too small` — and exits in
  0.06 s, so the client never registers. It fails loudly in Csound's log and
  silently in any summary built around it.

### Bypass mode — inferred, still not measured

Not measured, and honestly so. In bypass, csound owns the ALSA device
exclusively, so no second client can inject or capture; measuring it needs a
csd that performs its own impulse round trip, which is a separate piece of work.

But the `-B` finding lands on bypass too, and hard: `demiurge-run-csound`'s
bypass branch uses `-B $(( Q * 8 ))` — 512 frames at quantum 64. On the
measured curve above, 512 frames of ring is ~11 ms of csound-side latency.
That is very close to the long-quoted ~19.4 ms bypass figure once the
converters and the ALSA ring are added, which suggests **the bypass number is
dominated by the same class of bug, not by the `bufSamp_HW` source bug it has
been blamed on.** Worth measuring before anyone spends more effort on csound's
ALSA backend. Flagged, not fixed — bypass was explicitly out of scope here.

### Provisional — do not quote

Nothing on this page is provisional any more. The capture-ring rows that were
provisional have been re-measured properly and moved into the section below.

### The capture ring is a STABILITY knob, not a latency lever

An earlier draft of this document raised the alarm that the capture ring was
untuned (`period-size 1024, period-num 32`, a 32768-frame ring) and implied it
was costing latency. **Measured, that alarm was overstated.** All four ring
configurations land within 0.2 ms of each other:

| capture ring | buffer | shots | mean | sd |
|---|---|---|---|---|
| 1024 × 32 (PipeWire stock) | 32768 | 20 | 265.4 fr / 5.53 ms | 4.9 fr |
| 128 × 4 | 512 | 40 | 279.0 fr / 5.81 ms | 97.6 fr |
| **128 × 16** | **2048** | 40 | **261.5 fr / 5.45 ms** | **14.9 fr** |
| 128 × 32 | 4096 | 40 | 270.6 fr / 5.64 ms | 20.0 fr |

The mean barely moves — which is the same conclusion the playback side reached
in Rule 2c ("ring depth is NOT a latency lever"), now confirmed independently
for capture. What the ring *does* change is **stability**: at 128 × 4 the ring
is too shallow, PipeWire's read pointer hunts, and the spread explodes to sd
97.6 frames with a bimodal tail past 500 frames.

**Shipped: 128 × 16.** period-size matches the playback rule so it can never
become the binding floor on quantum, and period-num 16 gives the capture side
enough slack to stop hunting.

The earlier sawtooth in the per-shot data — a fall of ~19 frames per shot for
3–9 shots, then a jump back up — is now explained as the shallow-ring hunting
plus csound's 1024-frame ring, both since fixed. Note also that a separate
agent independently found the SCHED_FIFO audio thread was being blocked by
journald writes on every clock beat (829 xruns/120 s before, zero after), which
would have contributed. After all three fixes the same measurement reads
sd 6.1 frames.

### A measurement artefact worth knowing about

The first 40-shot run at the new ring returned **3 shots and 37 misses**, which
looked exactly like an unplugged cable. It was not. The launcher owns every
hardware playback port: `scrub_hw_playback` deletes any incoming link that is
neither `demiurge-sink-output` nor in its own `desired` set, and a test
client's output link is by definition neither. The measurement link was being
deleted mid-run.

`rtdelay.sh` now re-asserts its links once a second for the duration of the
run. If you write another test client that drives hardware playback directly,
it will need the same treatment.

### Not yet measured

- **Bypass mode** — see the section above for why, and for a strong reason to
  believe the long-quoted figure is misattributed.
- **The real NEPTR patch end-to-end.** It cannot be measured with this method:
  NEPTR emits silence at its output when untriggered, so an impulse injected at
  its input does not come back out. That is correct behaviour for an
  instrument, not a fault. The system-side number (`-B 128`, 8.28 ms) is
  measured; what NEPTR adds on top is the patch's own musical latency.
- **Quantum 32.** NOT blocked by the patch — see the correction above; ksmps
  follows `live.conf`. Note the trap: setting `quantum = 32` while ksmps is
  still 64 does not give you a 32-frame period. Csound rounds `-b` UP to the
  next ksmps multiple and pipewire-jack buffers the mismatch, so you measure a
  padded 64. `csound_extra = --omacro:KSMPS=32` has to come down with it. The
  embedded host refuses this combination outright instead of degrading.
- Any config measured with **real signal** rather than silence at the input.

---

## Measured vs inferred

This project ran for a long time on a device-queue model: read `delay` out of
`/proc/asound/.../status`, add up quanta, report a total. That model produced
the "108 frames ≈ 2.25 ms device-side at quantum 64" figure and the "~19.4 ms
bypass cap".

The cable says the sync-layer figure was **wrong by more than a factor of two,
in the direction that flatters the rig**: 108 frames inferred, 265 frames
measured. The gap is roughly 157 frames ≈ 3.3 ms, and it is not mysterious — it
is everything the device-queue model never counted:

- **The converters themselves.** A delta-sigma DAC and ADC each carry a digital
  interpolation/decimation filter with real group delay. On this class of part
  that is tens of samples *per direction* and it is invisible to every software
  counter, because it happens after the numbers leave the SoC.
- **The capture side of the graph.** The inferred figure was a *playback* delay
  number. Nothing was measuring capture, and — as it turns out — nothing was
  *using* capture either (see below), so the omission went unnoticed.
- **Graph scheduling**, as opposed to device queueing: the engine runs a quantum
  behind its driver by construction.

The lesson is narrow and worth stating plainly: **`delay` from
`/proc/asound` is a device-queue depth, not a latency.** It is a lower bound on
one direction of one half of the path. Quoting it as round-trip understates the
real figure by roughly the cost of the converters, which is precisely the part
you cannot get rid of by tuning.

The honest version of the old claim is: the *software* contribution at quantum
64 is small, and the measured 5.53 ms is mostly converter and graph-scheduling
cost that no buffer setting will remove.

---

## Things found while measuring

Two bugs surfaced during this work, both on the input side, both invisible until
a cable forced the issue.

### 1. The engine had no input at all

In sync mode the graph was:

```
csound6:output1/2  ->  hifiberry playback_FL/FR    (direct — the 2026-06 optimisation)
hifiberry capture_FL/FR  ->  demiurge-source-input (a loopback nothing consumed)
csound6:input1/2  ->  NOTHING                      (zero links)
```

The user's instrument never reached the engine in sync mode. Output had received
a direct-path optimisation; input had never received a path at all. Fixed
2026-08-10 in the launcher (`graph::in_source_ports`,
`aggregate::direct_input_device`) so the selected interface supplies **both**
directions by default.

### 2. The capture ring was never tuned

The WirePlumber ring-tuning rules matched `alsa_output.*` only. The capture side
was running PipeWire stock — `period-size 1024, period-num 32, headroom 32`, a
32768-frame ring — on both the platform and USB buses. It had never mattered,
because nothing was wired into capture. Wiring the input up without fixing this
would have handed the user a tuned output and a stock input.

Rules 2d (platform) and 2e (USB) added 2026-08-10. **The values are not yet
settled** — see the provisional table above.

---

## Reproducing

```bash
ssh gorpi7@demiurge.local
cd ~/demiurge/testing/latency
make                       # build on the Pi; the Mac has no JACK headers
./rtdelay.sh 40 250
```

Patch the interface's output to its own input first. On the HiFiBerry both are
line level, so this is safe. `rtdelay` prints `MISS` for every shot that gets
nothing back, which is what an unpatched or muted card looks like.

To measure with the instrument audible rather than against silence:

```bash
KEEP_CHAIN=1 ./rtdelay.sh 40 250
```

Record which mode you used — the tables above have a column for it because it
changes the noise floor the onset detector works against.

### Counting xruns, correctly

Two different counters, two different failure modes, and conflating them has
already caused one wrong claim on this project:

- `pw-top`'s **ERR** column counts PipeWire *graph* xruns.
- csound's own `rtjack: xrun` lines in the journal count the *engine* missing
  its deadline.

The engine can be missing every deadline while the graph reports zero. Always
say which one you counted.
