# DEMIURGE Global Clock

A single clock source every DEMIURGE program can read from and write to.

## Why there is a clock at all

Every audio language on DEMIURGE has its own sense of time. ChucK has its
own scheduler, Pd has its own dsp graph, SuperCollider has its own routine
thread, Faust has sample-accurate `ma.SR`, Csound has `kr`, Strudel has
`cps`. Left alone, they drift. A global clock lets you pick one BPM and
have all of them line up — ChucK's note rate, Faust's delay time, Csound's
modulation rate, Strudel's pattern playback — without any one of them
being the "master." The daemon is the master; everything else is a peer.

## What the clock is

`demiurge_clock` is a tiny C++ daemon that opens an ALSA seq client named
`demiurge_clock` with one input port and one output port, both subscribed
to the shared `Midi Through` bus. It does two things:

1. **Ticks.** It emits MIDI realtime `0xF8` at 24 PPQN (pulses per quarter
   note) using `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` for jitter
   below one audio block at 48 kHz. `0xFA` (start), `0xFC` (stop), and
   `0xFB` (continue) are emitted on transport changes.

2. **Broadcasts BPM.** Once per beat (every 24 ticks) it sends a control
   change on **MIDI channel 16, CC119** with the current BPM mapped linearly
   into the 0..127 range (`40 + value / 127 * 360`, so 40..400 BPM). This
   is the heartbeat value every other DEMIURGE program reads to stay in
   sync.

All clock control CCs live on **channel 16** (out of the way of musical
CCs on channel 1) at the **top of the CC number space**:

- **`ch16 CC119` — BPM broadcast (READ).** Clock writes this once per beat.
  Every program listens to it and updates its internal tempo. Nothing else
  ever writes to CC119.
- **`ch16 CC118` — BPM steer (WRITE).** The CC right below the broadcast.
  Any program can write CC118 with a new BPM value; the clock retunes
  itself and the next beat's CC119 broadcast reflects the new tempo —
  which every program then picks up. This is how "one program changes
  tempo and the whole chain follows" works.
- **`ch16 CC117` — transport.** `>=64` starts, `<64` stops. Emits `0xFA`/`0xFC`.
- **`ch16 CC116` — role.** `1` = this daemon becomes **follower** (an
  external device, e.g. the Pocket OpGorator, is master); `2` = this
  daemon becomes **master**. See "Follower mode" below.

Because CC119 and CC118 are separated (read-only broadcast vs. write-only
steer), there's no feedback loop — the daemon never has to filter its
own echo. CC116 is broadcast by whichever side changes it, both sides
obey, and the daemon guards against its own re-announcement by ignoring
ALSA-seq events whose source client is itself.

## Protocol summary

| MIDI event | Direction | Meaning |
|---|---|---|
| `0xF8` tick | clock ↔ bus | 24 PPQN metronome (clock → bus as master; bus → clock as follower) |
| `0xFA` start | clock ↔ bus | transport started |
| `0xFB` continue | clock ↔ bus | transport resumed |
| `0xFC` stop | clock ↔ bus | transport stopped |
| `ch16 CC120` + `CC121` | clock ↔ bus | **BPM as a 14-bit number** (MSB/LSB). The canonical tempo. See "Tempo is a number" below. |
| `ch16 CC119` | clock → bus | BPM, legacy 7-bit coarse. **Superseded by CC120/121** — 2.835 BPM per step is far too coarse for float tap tempo. |
| `ch16 CC118` | any → clock | request new BPM (7-bit; a gesture-adjacent CC118 is privileged, see the ladder) |
| `ch16 CC117` | any → clock | transport command / a device announcing its own sounding state |
| `ch16 CC116` | any ↔ clock (~1/2s) | role: 1 = OpGorator is master, 2 = Demiurge is master |
| `ch16 CC113` | NEPTR → clock | **Tap** on the NEPTR arcade button. Value = age in ms since the press (0..127). The daemon analyses it (see "Tap is an event"). |
| `ch16 CC112` | PMOR → clock | **Tap** on the PMOR encoder, sent only while PMOR is SLAVED. Value = age in ms, ISR-stamped on the Daisy. |
| `ch16 CC115` | NEPTR → clock | NEPTR holds a loop (127 = at least one looper channel has audio). Sent UNGATED by clock authority. |
| `ch16 CC114` | clock → bus | tempo source is REAL (127) vs merely the `live.conf` default (0). Gates NEPTR's external-grid latch. |
| `ch16 CC102` | NEPTR UI → engine | UI sync request — the NEPTR csound engine rebroadcasts its full OSC state (see [engine-ui.md](engine-ui.md)) |

(Channel 16 is the shared utility channel: the touch daemon also owns
`ch16 CC80-95` — see the touchscreen docs. `CC102` was allocated to the NEPTR
UI sync request on 2026-08-09; `CC114`/`CC115` on 2026-10-01; `CC120`/`CC121`
for 14-bit tempo on 2026-10-01 (specified then, IMPLEMENTED 2026-10-05: daemon, PMOR, NEPTR csound and
`demiurge-sync` all read/send it now); `CC112`/`CC113` for tap events on 2026-10-05. `CC96-101` and `CC108-111` remain free.)

## Tempo is a number. Phase is a pulse.

Two separate jobs, deliberately carried by different mechanisms, because they
have opposite requirements.

**Tempo is communicated as a NUMBER, never inferred from tick timing.**
A device that recovers tempo by measuring the interval between `0xF8` ticks is
measuring the *sender's scheduling jitter*, not its clock. PMOR is the worked
example: its sequencer grid is a Q32.32 accumulator advanced by its own audio
ISR sample counter — sample-exact, immune to buffering — but it *transmits*
from its main loop, alongside OLED blits and SD writes. So a PLL watching
PMOR's ticks produces a visibly wobbling BPM even though the grid underneath
is rock solid. That wobble is not cosmetic: NEPTR's looper varispeed scales
recorded audio by the tempo ratio, so tempo noise becomes *pitch* noise
(~0.4 % rate error ≈ 7 cents of warble on sustained material).

Hence `CC120`/`CC121`: 14-bit, `(bpm - 40) * 16383 / 360`, covering
40.00–400.00 BPM at 0.022 BPM resolution. As a varispeed ratio that is
0.018 %, about 0.3 cents — below audibility, where 1 cent is roughly the
threshold.

**THE WIRE VALUE IS CANONICAL.** This is the rule that prevents artifacts, and
it applies to the device that originated the tempo too. Tap 123.456 on PMOR
and it encodes, decodes to 123.45, and *PMOR adopts 123.45*, discarding its
own 123.456. Whoever taps quantises to the wire grid immediately and adopts
that — no round-trip wait, and guaranteed agreement because the quantisation
is deterministic on both sides.

Rounding on the wire is harmless. **Divergence is what kills you.** If PMOR
kept 123.456 while the daemon held 123.45, that 0.005 % error is a permanent,
slow varispeed warble and a drift of roughly one sample every four seconds,
forever. Two devices agreeing on a rounded number is strictly better than each
keeping its own precise one.

**Phase is the `0xF8` pulse stream, and it is deliberately asymmetric.**
The daemon owns phase and streams 24 PPQN continuously (see "the clock runs
continuously" in `demiurge-clock.cpp` — gating ticks on transport deadlocked
with PMOR). Followers correct their phase against it; the daemon never
corrects to a follower. The reason is where the correction lands:

- PMOR fires short step events. Nudging a trigger a few samples is inaudible.
- The looper plays sustained audio. Changing its rate changes pitch.

So the correction burden belongs on the sequencer, never on the looper — even
when a tap on PMOR set the tempo. **Tempo authority and phase authority are
different things.** A tap says "the tempo is now X"; it does not say "I am now
the clock."

Pulses remain a *fallback* tempo source for foreign gear that cannot send the
number — that is what the `CLOCK_SRC_EXTERNAL_MIDI` rung of the ladder is for.
For PMOR and NEPTR, the number always outranks it.

### Tap is an event (2026-10-05)

A tap is not a tempo. It is a **timestamped event** that lands on the bus, and the tempo is
worked out **once**, by the one analyzer, in the one place that owns the transport:

- PMOR encoder tap (while PMOR is slaved) -> `ch16 CC112`, value = age in ms.
- NEPTR arcade button (b1, via `serial_bridge.py`) -> `ch16 CC113`, value = age in ms.
- Any other pool source -> OSC `/transport/tap ,if` (source 0 pmor / 1 neptr / 2 other, age ms) to UDP `127.0.0.1:9100`.

`demiurge-clock` reconstructs the press instant as (its receive time - age), so main-loop stalls on
the device are measured and subtracted instead of becoming tempo error. The analyzer is
`src/demiurge-transport.h` -- **the same header the PMOR firmware compiles** for standalone use
(least-squares fit over the last 6 taps, 120 ms bounce gate, 2 s window expiry, 40 % outlier restart).
The result is quantised to the 14-bit wire grid FIRST, adopted, then broadcast at once as
CC119 + CC120/121 (and to the pool as `/transport/tempo`). Rules:

- A tap is rung 0 of the tempo ladder (never blocked) and does **not** move the daemon's phase:
  tempo authority and phase authority stay different things.
- One stray tap does nothing (it only opens a window). A tap *pair* on a device that is not the
  clock master takes master (CC116=2) so the tempo can actually be set; PMOR taps while PMOR is the
  master we follow are ignored (PMOR analysed them itself and its number arrives as CC120/121).
- A follower adopts the master's CC120/121 number as THE tempo; the PLL keeps recovering phase and
  is only the tempo fallback for foreign gear that sends no number (3 s freshness window).

Pool OSC published by the daemon on UDP `127.0.0.1:9101` (loopback only): `/transport/tempo ,f`
(wire-canonical BPM, every beat and on change), `/transport/bar ,i` (bar index since Start),
`/transport/start`, `/transport/stop`, and `/transport/tap ,if` for every tap from any path.
`clock_status.conf` gains `bpm_wire` (the number every display should show), `tempo_from_number`,
`tap_count`, `last_tap_source`, `last_tap_age_s`. JACK transport / BBT is **not** wired yet.

### Tap tempo from either side, and Ableton Link

Tap is symmetric: PMOR has its encoder press, NEPTR has the body's arcade
button. Either may originate a tempo. Both send the number to the daemon; the
daemon is the single authority that decides the canonical value.

Link makes the quantisation step load-bearing. Link carries tempo as a
`double`, so an Ableton peer can hold 123.456 while PMOR can only receive
123.45 — precisely the 0.005 % divergence described above, with Ableton on one
side of it. Therefore the daemon **quantises to the 14-bit wire grid BEFORE
committing to the Link session**, so Ableton, NEPTR and PMOR all hold the
identical number. A tap on either instrument propagates outward to Link; a
Link peer's change propagates inward to both instruments. One value, four
consumers.

Everything travels on the shared `Midi Through` ALSA seq bus (usually
`14:0`), which every DEMIURGE program joins automatically. Faust stages
join via PipeWire's Midi-Bridge (which exposes `Midi Through` as a JACK
MIDI capture port); Pd joins via a small python sidecar
(`bridges/pd_clock_bridge.py`) that forwards CC119/CC117 to Pd as FUDI
over UDP 9997.

## How each language reads it

### ChucK
```chuck
MidiIn min; MidiMsg msg;
min.open(0);
while (true) {
    min => now;
    while (min.recv(msg)) {
        // Status nibble + channel
        msg.data1 & 0xF0 => int status;
        msg.data1 & 0x0F => int chan;
        // ch16 (index 15) CC119 = global BPM
        if (status == 0xB0 && chan == 15 && msg.data2 == 119) {
            40.0 + (msg.data3 / 127.0) * 360.0 => float bpm;
            60.0 / bpm => float beat_s;
            // … use beat_s to schedule events
        }
        // 0xF8 = MIDI realtime Clock tick (24 PPQN)
        if (msg.data1 == 0xF8) { /* beat tick */ }
    }
}
```

### Pd
Pd vanilla's raw ALSA MIDI can't see the shared ALSA seq bus directly,
so DEMIURGE runs a small python sidecar (`pd_clock_bridge.py`) alongside
Pd patches. The sidecar reads ch16 CC119 from `Midi Through` and forwards
the BPM to Pd as FUDI over UDP 9997. The Pd patch:
```
[netreceive 9997 1]
    |
[route bpm]
    |
[float]   — BPM in 40..400
```

### SuperCollider
```supercollider
MIDIClient.init;
MIDIIn.connectAll;
MIDIdef.cc(\clockBPM, { |val, num, chan|
    if (chan == 15 and: { num == 119 }) {
        ~bpm = 40 + (val / 127) * 360;
        TempoClock.default.tempo = ~bpm / 60;
    };
}, 119);
```

### Faust
```faust
bpm_cc = hslider("bpm[midi:ctrl 119]", 0.35, 0, 1, 0.001) : si.smoo;
bpm    = 40.0 + bpm_cc * 360.0;
// Derive delay times, LFO rates, etc. from bpm.
```
**Do NOT specify a channel** in `[midi:ctrl N CH]`. On `faust2jackconsole`
the channel numbering is off by one (so `[midi:ctrl 119 15]` receives
channel 15 not ch16) and `[midi:ctrl 119 16]` silently breaks the
binding entirely. Omit the channel and Faust accepts the CC from any
channel — fine here, because CC119 is reserved for the clock broadcast.
See INSTRUCTIONS.md §12 under "Faust" for the full story.

Build with `faust2jackconsole -midi faust_delay.dsp` — the launcher's
`wire_faust_midi` step connects PipeWire's
`Midi-Bridge:Midi Through Port-0 (capture)` port to the faust binary's
JACK MIDI input at graph-build time.

### Csound
Csound on DEMIURGE must be told to subscribe from `Midi Through` at
client `14:0`, not the default `-M0` which is the useless System Timer
client:
```
<CsOptions>
-+rtmidi=alsaseq
-M14:0
</CsOptions>
```
```csound
instr 1
  ; ctrl7 ichan is 1-indexed, so channel 16 = ichan 16.
  gk_bpm_cc ctrl7 16, 119, 0, 1
  gk_bpm     = 40 + gk_bpm_cc * 360
endin
i 1 0 -1
```

### C++ / JACK (via ALSA seq)
```cpp
if (ev->type == SND_SEQ_EVENT_CONTROLLER
    && ev->data.control.channel == 15      // ch16 == idx 15
    && ev->data.control.param == 119) {
    double bpm = 40.0 + (ev->data.control.value / 127.0) * 360.0;
    g_bpm.store(bpm);
}
```

### Strudel (Node / headless)
```js
import easymidi from 'easymidi';
const inputs = easymidi.getInputs();
const midiIn = new easymidi.Input(inputs.find(o => /midi through/i.test(o)));
midiIn.on('cc', (msg) => {
    if (msg.controller !== 119 || msg.channel !== 15) return;  // ch16
    const bpm = 40 + (msg.value / 127) * 360;
    scheduler.setCps(bpm / 60 / 2);  // 1 cycle = 2 beats
});
midiIn.on('start', () => scheduler.start());
midiIn.on('stop',  () => scheduler.stop());
```
This works in both `strudel-runner.mjs` modes — whether Strudel is
producing audio natively via `node-web-audio-api` (default) or emitting
MIDI to the shared bus (`--midi`), the clock-sync listener is the
same.

## How to steer the clock from any program

Write `ch16 CC118` onto the bus with the desired BPM. The daemon picks
it up immediately and the next `CC119` broadcast propagates the new
tempo to every other program.

One-liner from the shell (aseqsend takes raw MIDI bytes in hex —
0xBF = Control Change on ch16, 0x76 = CC118):
```sh
# ~150 BPM == value 70 (0x46)
aseqsend -p "Midi Through:0" bf 76 46
```

From ChucK:
```chuck
MidiOut mout; mout.open(0);
MidiMsg m;
// 0xB0 | 15 = 0xBF -> CC on channel 16
0xBF => m.data1;  118 => m.data2;  70 => m.data3;
mout.send(m);
```

Because `CC119` (broadcast) and `CC118` (steer) are separate, the
daemon never has to filter its own echo, and any program writing
`CC118` can be sure the whole chain follows on the next beat.

## Follower mode — following an external MIDI clock master

`demiurge-clock` normally free-runs as the master (or takes tempo/transport
from Ableton Link, see below). It can also flip the other way and *follow*
an external MIDI clock master on the bus — the motivating case is the
[Pocket OpGorator](../../docs/superpowers/specs/2026-07-29-pocket-opgorator-integration-design.md),
a hardware groovebox that can itself be clock master.

**Role state:**

- **`master`** (default) — the daemon free-runs at `--bpm` (or Link, or
  CC118 steer from any program). This is the behavior described
  everywhere else in this doc.
- **`follower`** — the daemon locks its tempo and phase to an external
  `0xF8` clock arriving on the bus, then keeps re-emitting its own
  `0xF8` + `CC119` broadcast at the recovered tempo/phase. Every
  downstream consumer (looper grid, csound, chase_bliss, ...) needs zero
  changes — it still just reads `0xF8`/`CC119` off the shared bus.

**Recovering tempo (PLL).** Incoming `0xF8` ticks are never trusted
individually — USB MIDI jitter is on the order of ±1ms, which is 5-10%
of a tick's duration at 120 BPM. Instead the daemon keeps a sliding
window of the last 24 tick-to-tick intervals and combines them; `g_bpm`
only updates once that window has filled, then keeps recomputing every
subsequent tick. The tick-emission thread also snaps its own schedule
to the incoming tick's phase whenever a fresh average lands, so its own
`0xF8` stream stays phase-locked to the external master, not just
tempo-matched. The gate + averaging algorithm lives in
[`src/demiurge-clock-pll.h`](../../src/demiurge-clock-pll.h), a
platform-independent header shared byte-for-byte between the daemon and
the test harness in `testing/clock/` — there is exactly one copy of this
logic.

**Interval plausibility gate.** An incoming interval is only accepted if
it's physically possible for a 24 PPQN clock between `MIN_BPM` (20) and
`MAX_BPM` (400):

```
floor   = 60 / MAX_BPM / PPQN = 60 / 400 / 24 =   6.25 ms
ceiling = 60 / MIN_BPM / PPQN = 60 /  20 / 24 = 125.0  ms
```

This exact formula is duplicated in the firmware
(`firmware-tinyusb/src/clock_engine.c`, `handle_realtime()` around the
`min_iv`/`max_iv` computation) expressed in audio samples instead of
seconds — the two are meant to agree, and as of this fix they do.

The floor exists specifically because of a confirmed PMOR firmware/USB
bug: roughly 3% of `0xF8` bytes are transmitted twice, with the
duplicate landing 0.3–1.5 ms after the real tick. That's comfortably
under the 6.25 ms floor, so duplicates are rejected outright. Just as
importantly, **a rejected sample does not re-anchor** — the daemon does
not move its "last tick" reference to the duplicate's timestamp. The
older version of this code re-anchored unconditionally on every tick
(accepted or not), which meant a duplicate would make the *next real*
interval measure short, and that short interval would itself pass the
old, much looser gate (`dt > 0.5ms`) and get averaged in — a silent ~3%
fast bias in recovered tempo with no error, warning, or symptom besides
"the beat is a little quick." `testing/clock/test_pll.cpp` reproduces
the old gate/behavior side-by-side with the fixed one and shows the
worst-case bias (~9% in the shipped test's synthetic run) versus the
fixed version's <0.1 BPM error under the same input.

An interval *above* the ceiling is a stall (cable unplugged, device
powered off mid-following), not a duplicate — the daemon discards the
current averaging window and re-anchors to the new tick, so the *next*
tick begins a clean lock rather than the gap itself being averaged in as
one enormous fake interval.

**Outlier-resistant averaging.** Once the window has 3+ samples, the
recovered BPM comes from a **trimmed mean**: sort the window, drop the
single lowest and single highest sample, average what's left. A plain
mean over 24 samples lets one surviving bad sample shift recovered tempo
by as much as ~4%; the trim removes that single point of failure.

**Freewheeling.** If no external tick arrives for more than 2 seconds
(cable unplugged, device powered off, etc.), the daemon keeps emitting
its own `0xF8`/`CC119` at the last recovered tempo rather than stalling
the whole bus. It re-locks automatically the moment ticks resume.

**Transport.** While following, incoming `SND_SEQ_EVENT_START` /
`CONTINUE` / `STOP` (the raw `0xFA`/`0xFB`/`0xFC` MIDI-realtime bytes,
not `CC117`) are mapped onto the same play/stop state as a `CC117`
write, and re-broadcast to the bus the normal way. `CC117` itself still
works identically in either role — it's a separate, explicit "any
program can start/stop transport" mechanism.

**Feedback guard.** The daemon captures its own ALSA-seq client id at
startup and ignores clock/transport/CC116 events whose event source is
itself, so its own re-broadcast can never be mistaken for an external
master.

**Ableton Link interaction.** When following, Link tempo/transport
commits are skipped entirely (both from the tick thread's Link poll and
from the CC118-steer handler) — the external MIDI master wins. Link, if
enabled, effectively goes along for the ride via the daemon's own
re-broadcast rather than driving it.

### Tempo-source priority ladder

Four different things can ask the clock to change tempo: the external
MIDI PLL above, `CC118` steer writes, Ableton Link, and the startup
`--bpm` value. Historically these had no explicit ordering — `CC118` in
particular was accepted from anyone unconditionally. The daemon now
enforces an explicit ladder, highest priority first:

1. **`EXTERNAL_MIDI_CLOCK`** — the PLL above (role = follower, driven by
   PMOR or any other external `0xF8` source).
2. **`CC118` steer** — how the NEPTR looper/drum loops set tempo, via
   `chase_bliss_clock.orc`.
3. **Ableton Link** — only ever consulted while `role == master` (as
   before).
4. **live.conf `bpm`** (the startup `--bpm=` flag) — lowest; applied
   once at process start and never re-applied at runtime, so it never
   needs a recency check of its own.

A lower-priority source is **ignored** while a strictly higher-priority
one has been heard within the last **2.0 seconds** — the same recency
window `chase_bliss_clock.orc`'s own local arbiter uses
(`iChaseBlissExtWindow`), so the two systems agree on what "fresh"
means. Concretely: if PMOR is actively driving the clock (ticks arriving
at least every couple seconds), a stray `CC118` write from anywhere else
is dropped; the moment the external clock goes stale (cable pulled,
freewheel window expired), the very next `CC118` takes effect
immediately.

**This is a safety net, not a second arbitration system.**
`chase_bliss_clock.orc` already self-gates: it only writes `CC118` while
its own local arbiter believes the *loop*, not DEMIURGE, is
authoritative, and stays silent whenever DEMIURGE is authoritative (see
the "MASTER CLOCK COORDINATION" block in that file, `NEPTR phase4/csound/
chase_bliss_clock.orc` around lines 444–524). Under normal operation the
ladder should rarely have to actually block anything — the two systems
already agree. It exists so a misbehaving or unaware future CC118 sender
can't yank tempo out from under a live external-clock lock.

The ladder logic (`clock_tempo_source_may_apply()`) lives in
[`src/demiurge-clock-pll.h`](../../src/demiurge-clock-pll.h) alongside
the PLL, for the same reason: one copy, shared with the test harness.
The currently-active source is logged in the periodic status line (see
below) so it's diagnosable without instrumenting the binary.

### Role negotiation — `ch16 CC116`

- `1` — the daemon becomes **follower** (an external device, e.g. the
  OpGorator, is master).
- `2` — the daemon becomes **master**.
- Whichever side's menu/UI flips the role writes `CC116` onto the bus;
  both sides obey. The daemon also re-announces its *current* role on
  `CC116` once every ~2 seconds (from the tick thread, alongside the
  BPM broadcast), so a device that gets plugged in late still converges
  to the right role without a manual nudge.
- The daemon ignores its own re-announcement (see feedback guard above).

### Persistence — `~/demiurge/sync.conf`

Role is stored outside `live.conf`, in `~/demiurge/sync.conf`, so that
`demiurge-set` patch switches never clobber it (same pattern as
`interface.conf`):

```
role = opgorator
```

- `role = opgorator` → daemon starts in **follower** mode (the
  OpGorator is master).
- `role = demiurge` → daemon starts in **master** mode.
- Missing file, or a missing/unrecognized `role` key → **master**. (The
  policy of defaulting to `opgorator` when the device is actually
  attached lives in the `demiurge-sync` CLI/UI layer, which writes this
  file — the daemon itself just reads whatever's there at startup.)

The daemon reads this file once, at startup, before creating its
threads. Runtime role changes always arrive live as `CC116` — nothing
re-reads the file while the daemon is running; that's what
`demiurge-sync opgorator|demiurge` is for (it writes the file *and*
injects `CC116` on the bus in one step, no service restart needed).

**CLI override:**

```sh
demiurge-clock --role=follower   # or --role=master
demiurge-clock --role follower   # space-separated form also accepted
```

An explicit `--role` flag always wins over `sync.conf`, matching the
existing `--link`/`--bpm` flag conventions.

## Running it

The clock and the Pd sidecar are both implicit in `live.conf` — you
never list them. The launcher always supervises `demiurge-clock`, and
adds `pd_clock_bridge.py` automatically whenever any `chain =` stage is
a `.pd` file. Flipping `link = on/off` in `live.conf` and saving
triggers a clock respawn with the new flag; everything else about the
chain stays in place.

Under the hood the launcher treats the clock as a `clock`-typed
`Program` inside the internal session graph — it skips JACK port
resolution and runs the binary in the background. Users never see this;
stop/start/restart the whole session with `sudo systemctl restart demiurge`
and the clock restarts with everything else.

The daemon subscribes itself **both ways** to `Midi Through` (ALSA seq
client `14:0`) at startup:

- `out_port → 14:0`  so `0xF8` ticks and the `CC119` broadcast reach
  every other program on the bus.
- `14:0 → in_port`  so any program writing `CC118` on the shared bus
  reaches the daemon and retunes it.

The IN-side subscription is the easy thing to forget. Without it,
`CC118` steer messages travel happily on Midi Through but the clock
never sees them, and the tempo silently refuses to change. The binary
handles this itself — there's no launcher wire-up step required.

## Rebuilding the daemon and launcher on the Pi

This is the one way (flags identical to `setup-demiurge.sh` Phase 6; the
config-only `setup-script/deploy-clock-config.sh` does NOT build). Run on the
Pi from the repo checkout (`$HOME/_______DEMIURGE`):

```
cd "$HOME/_______DEMIURGE"
g++ -std=c++17 -O2 -Wall -pthread -DDEMIURGE_LINK -DLINK_PLATFORM_LINUX=1 \
    -o /tmp/demiurge-clock src/demiurge-clock.cpp -lasound -lpthread
sudo install -m 0755 /tmp/demiurge-clock /opt/demiurge/bin/demiurge-clock && rm -f /tmp/demiurge-clock

(cd src/demiurge-launcher-rs && cargo build --release -j2)
sudo install -m 0755 src/demiurge-launcher-rs/target/release/demiurge-launcher /opt/demiurge/bin/demiurge-launcher
```

Installing does not restart anything: the new daemon runs after the launcher
next respawns it (flip `link =` in live.conf, or restart per the usual
permission rules); the new launcher runs after the next `demiurge` restart.
Keep `-j2`: `-j4` browned the Pi 5 out.

## Self-echo and tempo authority (2026-10-03)

The daemon sends to Midi Through and listens on it; Midi Through echoes every
event back to the sender with the source stripped. The daemon therefore stamps
every 0xF8/START/CONTINUE/STOP/CC116 it sends and drops the first matching
event received within `SELF_ECHO_WINDOW_S` (1.5 ms) as its own echo. In
`role=master` received 0xF8 never moves tempo; on a follower->master flip the
tempo is re-seeded from the last master-side value (live.conf `--bpm`, Link,
CC118) and logged (`clock: master re-seed bpm ...`). An echo-only tick stream
cannot hold follower mode: it never reaches the PLL, so the 5 s reclaim fires.
`clock_status.conf` gained `ext_clock = yes|no` (real, non-echo 0xF8 stream
heard); see `clock-role-auto.md`.

## Verifying the clock from the shell

```sh
# 1. Watch the broadcast value live
aseqdump -p "Midi Through" | grep "controller 119"

# 2. In another shell, steer to ~197 BPM (value 100)
python3 -c '
import mido
out = mido.open_output("Midi Through:Midi Through Port-0 14:0")
out.send(mido.Message("control_change", channel=15, control=118, value=100))
'

# 3. Confirm in the clock daemon log
tail -f ~/.demiurge/logs/clock.log
# expect:  clock: BPM -> 197.5 (via CC118)
```

If step 3 doesn't show the retune message, the daemon's `in_port`
isn't subscribed from `14:0` — check `aconnect -l` for the
`demiurge_clock` client and look for `Connected From: 14:0` under
its `in` port. If the retune message shows up but the tempo doesn't
actually move, check the periodic status line instead (below) — the
tempo-source ladder may be holding a higher-priority source.

**Periodic status line.** Every ~2 seconds (piggybacking on the CC116
role re-announce), the daemon logs its current state:

```
clock: status bpm=124.0 role=follower source=external-midi
```

`source` is one of `external-midi`, `cc118`, `link`, or `live.conf` —
whichever source last actually won the tempo-source ladder (see
"Tempo-source priority ladder" above). This is the fastest way to tell
*why* a `CC118` write or a Link tempo change didn't take effect: if
`source=external-midi` and PMOR is attached, that's expected — the
external clock outranks everything else while it's fresh.

## Verifying that audio actually flows

`pw-record --target=` and `pw-cat -r --target=` are unreliable on
DEMIURGE — they tend to silently auto-link to the Scarlett mic
instead of the node you asked for, producing all-zero WAVs while
the chain is in fact playing fine. **Use `pw-jack jack_rec` with
the JACK-style port name** (mind the spaces — quote them):

```sh
# Tap the final mix (demiurge-sink monitor) for 4 seconds
pw-jack jack_rec -d 4 -f /tmp/mix.wav \
    "DEMIURGE:monitor_FL" "DEMIURGE:monitor_FR"
sox /tmp/mix.wav -n stats   # look for non -inf RMS

# Tap any single stage by node:port
pw-jack jack_rec -d 3 -f /tmp/ck.wav \
    "ChucK:outport 0" "ChucK:outport 1"
```

## Ableton Link

The clock can optionally participate in an Ableton Link session. Link is
an open network sync protocol — Live, Max/MSP, iOS/Android music apps,
and many hardware boxes speak it. When enabled, Link becomes the outer
authority: tempo and transport decisions from any peer on the local
network propagate into the DEMIURGE clock, and the clock still fans out
the usual `0xF8` / `CC119` / transport broadcasts to every language on
the MIDI bus — so nothing in the session has to know Link exists.

**Enable:**

```sh
# in /boot/firmware/demiurge.conf
link = on
```

Restart the clock (`systemctl --user restart demiurge` or reboot). You
should see:

```
demiurge_clock: Ableton Link enabled (initial BPM=110.0)
```

With Link enabled:

- **Tempo.** The clock polls Link on every tick. If a peer changes
  tempo, DEMIURGE follows within one tick; if a DEMIURGE program writes
  `CC118` to the shared MIDI bus, the clock commits that change back
  into Link, and every peer on the network follows. One tempo, shared
  across every language and every networked app.
- **Transport.** Link's `startStopSync` is enabled. `CC117` on the
  shared bus commits start/stop into Link; Link's session state
  propagates into `g_playing` on every tick and fires `0xFA` / `0xFC`
  out to the MIDI bus. If Ableton Live is on the same network, its
  play button and DEMIURGE's `CC117` control the same transport.

When `link` is unset or off, the clock behaves as a standalone master
exactly as described above — this is the default.

**Building with Link:** requires `ableton-link-dev` (in apt on Bookworm
and later) and a compile-time flag:

```sh
g++ -std=c++17 -O2 -DDEMIURGE_LINK -o demiurge-clock src/demiurge-clock.cpp -lasound -lpthread
```

A binary built without `-DDEMIURGE_LINK` and `link = on` in
`demiurge.conf` will print a warning and run in standalone mode.

## Building from source

```sh
# Standalone build (no Link)
g++ -O2 -o demiurge-clock src/demiurge-clock.cpp -lasound -lpthread

# With Ableton Link
sudo apt install -y ableton-link-dev
g++ -std=c++17 -O2 -DDEMIURGE_LINK -o demiurge-clock src/demiurge-clock.cpp -lasound -lpthread

sudo install -m 0755 demiurge-clock /opt/demiurge/bin/demiurge-clock
```

## Testing — `testing/clock/`

The daemon itself only builds/runs on Linux (real ALSA), so it has no
in-tree runtime tests. What's testable without a Pi is the PLL gate and
the tempo-source ladder — both live in
[`src/demiurge-clock-pll.h`](../../src/demiurge-clock-pll.h), a
platform-independent header with no ALSA, threads, or process-global
state, `#include`d by `demiurge-clock.cpp` itself so there's exactly one
copy of the algorithm.

```sh
testing/clock/run.sh
```

This does two things:

1. Compiles and runs `testing/clock/test_pll.cpp` — unit tests against
   `demiurge-clock-pll.h` directly. Covers: a clean tick train recovers
   exact BPM; a train with 3% duplicate ticks injected at +0.3ms/+1.5ms
   (PMOR's known bug) still recovers correct BPM, and separately proves
   the *old* (pre-fix) gate/behavior would NOT have (regression test);
   realistic USB jitter (±1ms) recovers within 0.5%; a tempo change
   settles within a bounded number of ticks; a stall past the gate's
   ceiling re-anchors cleanly instead of wedging; and the priority
   ladder blocks a lower-priority source while a higher one is fresh,
   then lets it through once that source goes stale.
2. Compiles `src/demiurge-clock.cpp` — the real, unmodified daemon
   source — as a whole, against a minimal stubbed
   `<alsa/asoundlib.h>` in `testing/clock/stub/` (macOS has no real
   ALSA to link against; this is a compile-only sanity check, never
   used to run the daemon) with `-Wall -Wextra`, asserting zero
   warnings and zero errors.

Both steps must pass for the runner to exit 0.

## Design notes

- **Why channel 16 and the top CCs?** Keeps the clock out of the way of
  every musical CC you'd realistically ever use. CC119 is the very last
  7-bit CC in the MIDI spec, so there's no collision with common
  controllers (mod wheel = CC1, volume = CC7, sustain pedal = CC64, etc).
- **Why a separate steer CC?** Splitting `CC119` (read) from `CC118`
  (write) removes the echo-feedback problem entirely. The clock never
  has to filter its own broadcast when processing steer commands, and
  any program can write `CC118` without accidentally triggering retune
  loops.
- **Why MIDI realtime, not a custom protocol?** Every audio language on
  Earth already knows how to read `0xF8` and `CC`. A custom protocol would
  mean writing a new reader per language. MIDI piggybacks on wiring that
  already exists.
- **Why 24 PPQN?** It's the MIDI spec. Downstream programs that want finer
  resolution can interpolate; programs that want coarser can decimate.
- **Why `clock_nanosleep` with `TIMER_ABSTIME`?** Absolute-deadline sleep
  prevents drift: each tick targets `start + n * tick_period` rather than
  `now + tick_period`, so scheduler jitter doesn't accumulate.
- **Why broadcast `CC119` at all?** `0xF8` alone gives you rate but not
  absolute tempo. Programs like Faust that can't easily count ticks still
  need to know what BPM means. The once-per-beat `CC119` is cheap and
  answers the question directly.
