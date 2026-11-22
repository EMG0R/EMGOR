# 2026-05-14 — Bypass mode collapses sync-layer latency

## Symptom

After Phase 1 perf work (`incidents/2026-05-13`), the chain ran clean at
quantum=256 but dropping to quantum=128 reintroduced a consistent
glitch the moment any FX engaged — same "bad sample-rate" sound the
2026-05-13 session had largely fixed. `pw-top` reported zero xruns and
~1% BUSY/QUANTUM, so the sync layer's own deadline-detector was
oblivious. The probe (multi-tone + impulse + 6 glitch detectors)
also missed it in 15 s of capture.

Net: PipeWire reported the chain was healthy while the user's ears
heard continuous corruption on every FX engagement.

## Hypothesis

The 5-quantum software latency budget at Q=256 is:

| stage                                | latency       |
|--------------------------------------|---------------|
| USB ADC capture quantum              | 1 × Q         |
| csound process tick                  | 1 × Q         |
| `demiurge-sink` loopback hop         | 1 × Q         |
| `alsa_output` ring (period-num=4)    | 2 × Q         |
| **Total software**                   | **5 × Q**     |

The chain's per-FX work fits comfortably inside one 2.67 ms cycle at
Q=128 (csound BUSY ≈ 27 µs), but *something across the boundary
between csound's output and the alsa_output ring* doesn't survive the
tighter timing — likely buffer alignment / partial fill events that
don't trigger PW's xrun counter but corrupt the data heading into the
ALSA ring. The deeper structural answer: **cut the loopback hop and
the alsa_output ring entirely**, run csound straight to the ALSA hw
device. If the glitch goes away, that confirms the sync layer is the
fault.

## Implementation — bypass mode

`testing/bypass/run-bypass.sh` + `neptr/live.conf` fields:

```
audio_interface = hw:CARD=USB,DEV=0     # by-name handle to Scarlett
bypass = on                              # script refuses to launch otherwise
```

The script:

1. Stops `demiurge.service`.
2. Stops `pipewire` + `wireplumber` (user services) — PW's ALSA monitor
   holds an open handle on the device even when no streams are routed.
   Without this, csound errors with `Device or resource busy`.
3. Launches csound with:
   `chrt --fifo 74 csound -+rtaudio=alsa -odac:plughw:CARD=USB,DEV=0
    -iadc:plughw:CARD=USB,DEV=0 -r 48000 -B 8×period -b period --realtime`
   The `plughw:` prefix is load-bearing — csound's ALSA backend asks
   for FLOAT_LE but the Scarlett USB Audio Class device wants S32_LE;
   ALSA's plug layer converts in-kernel for ~zero added latency.
4. Traps Ctrl-C / TERM to restart pipewire → wireplumber → demiurge in
   that order on exit.

## Verification

Tested period sequence:

| period | software RTT calc       | observation                              |
|--------|-------------------------|------------------------------------------|
| 128    | 2 × 128/48k = 5.3 ms    | clean under FX; glitch gone vs Q=128 sync |
| 64     | 2 × 64/48k = 2.7 ms     | clean under FX; running stable             |

Adding ~1.7 ms for USB-out + DAC group delay puts total RTT at **~7 ms
@ period=128** and **~4 ms @ period=64**. The Q=256 sync chain was
~28 ms RTT. Bypass at period=64 is 4× faster.

## Tradeoffs accepted in bypass mode

Per `[[project_phase4_must_use_hub]]`:

- **No replug recovery.** Unplugging the Scarlett mid-session drops audio.
- **No rate enforcement at the WP layer.** Rate is whatever csound
  negotiates (we pass `-r 48000` explicitly, so it's pinned).
- **No multi-stage chains.** Only one program can own the audio device.
- **No `demiurge-sink`/`-source`.** Anything that writes to those is dead.

Bypass is a low-latency mode, not a production mode. Switch back to the
normal chain (Ctrl-C the bypass script) for sessions where replug
recovery and multi-stage chains matter.

## Why PipeWire reports zero xruns despite real corruption

`pw-top`'s xrun counter fires only when the **process callback misses
its deadline**. The chain has multiple buffering layers; one layer
returning slightly late (within its 2-quantum headroom) does not cause
an xrun report but does corrupt the audio downstream. The 2026-05-13
session noted the same pattern as "no-xrun glitch" without locating
the exact mechanism — bypass's success implicates the
`demiurge-sink-output` loopback / `alsa_output` ring as the locus,
since removing them removes the symptom.

This is the third independent symptom (after April's bit-crush
artifacts and yesterday's bit-crush-on-FX-engage) where the latency
floor at Q=128 in normal mode breaks under load while reporting
healthy from every monitoring surface PipeWire exposes. Bypass is the
known-good path for low-latency work on this hardware until the sync
layer's silent-corruption mode is properly diagnosed.

## What's next

### Tier 1 — push bypass period lower
- `period=32 ≈ 1.7 ms RTT` requires `globals.orc::ksmps = 32` (currently
  64; csound can't deliver blocks larger than the driver period).
- `period=16 ≈ 1.0 ms RTT` is aggressive; depends on USB scheduling
  jitter at sub-millisecond intervals.

### Tier 2 — promote bypass into the launcher
- Add `bypass` and `audio_interface` to `LiveConfig` in
  `src/demiurge-launcher-rs/src/live.rs`.
- Launcher reads them; when `bypass = on`, skips PW/JACK setup entirely
  and forks csound with raw-ALSA args directly. Saves the
  stop-service-on-launch dance.

### Tier 3 — diagnose the real sync-layer fault
- The fact that bypass at period=64 (effectively 2 alsa-ring quanta)
  is clean while sync at Q=128 (also has ALSA ring contributions) is
  not, suggests the loopback hop is the broken stage, not the ring
  itself. Worth confirming by setting up a chain where csound writes
  to alsa_output directly via pw-link (skipping demiurge-sink) and
  testing FX-engage.

## Files in this work

| path                                          | role                                       |
|-----------------------------------------------|--------------------------------------------|
| `testing/bypass/run-bypass.sh`                | orchestration: stop chain, run csound raw  |
| `testing/bypass/README.md`                    | usage + tradeoffs                          |
| `neptr/live.conf`                             | `bypass` + `audio_interface` user fields   |
| `testing/probe/{inject.csd, probe.sh, analyze.py}` | probe harness improved during this session (multi-tone + 6 detectors + --fx + --play) — did NOT catch the glitch, but the discovery process led to bypass |

---

## Session continuation — chain refinements, config-driven gate, PSU finding

After the initial bypass discovery this same session continued with
audio-chain work and a fix to make `bypass` config-driven instead of a
manual systemctl-disable. Net summary below.

### Preamp — always-on soft saturation (zero latency)

Bypass at period=64 (~4 ms RTT) is "raw" linear digital — strips away
the natural compression an analog rig provides (tube saturation, pickup
loading, etc.). Dynamics felt unmusically wide: peak transients
unclamped, quiet parts thin.

Added `neptr/csound/effects/preamp.orc` as a UDO:
```
opcode Preamp, aa, aa
    aL, aR xin
    iDrive = 1.4
    aOutL  = tanh(aL * iDrive) / iDrive
    aOutR  = tanh(aR * iDrive) / iDrive
    xout aOutL, aOutR
endop
```

- Point-wise sample math (`tanh`), zero buffering, zero added latency.
- Small-signal region passes through linear (output ≈ input).
- Above ~70% amplitude, soft-knee saturation rounds transients.
- Hard limit at ±1/iDrive ≈ ±0.71.

Wired into `neptr/csound/signal_chain.orc` as position 0, BEFORE all
$FX_BYPASS effects, called inline (NOT via `$FX_BYPASS` — no MIDI
bypass, inherently active).

Also added `#include "effects/preamp.orc"` immediately after globals
in `neptr/csound/neptrPhase4.csd`.

Tuning knob: edit `iDrive` in `preamp.orc`. 1.0 = effectively bypass;
1.4 = default analog-pedal feel; 2.0+ = clearly saturated. Tunable by
the user without launcher rebuild.

### Bypass gate — driven by user-facing config, not hardcoded

The first cut had `sudo systemctl disable demiurge` — wrong solution
because it removes the user's ability to flip back to normal mode by
editing config. Replaced with a config-driven `ExecCondition` gate.

Files installed on Pi:
- `/usr/local/bin/demiurge-bypass-gate.sh` — reads
  `/home/gorpi7/NEPTR_phase4/live.conf`. Exits 0 if `bypass = off`
  (or unset), exit 1 if `bypass = on`.
- `/etc/systemd/system/demiurge.service.d/bypass-gate.conf` — adds
  `ExecCondition=/usr/local/bin/demiurge-bypass-gate.sh` to the
  demiurge.service unit.

`demiurge.service` stays `enabled` for autostart. The gate decides
at runtime. Verified end-to-end:

```
demiurge-bypass-gate: live.conf has 'bypass = on' — skipping demiurge.service
systemd[1]: demiurge.service: Skipped due to 'exec-condition'.
systemd[1]: Condition check resulted in demiurge.service ... being skipped.
```

Local repo copies:
- `config/pi5-performance/demiurge-bypass-gate.sh`
- `config/systemd/demiurge.service.d/bypass-gate.conf`

User workflow: edit `bypass = on` in live.conf → reboot → demiurge
stays down. Edit `bypass = off` → reboot → demiurge runs normally. No
systemctl commands required.

### Preset tweaks (TS + CLEAN, midi_handler.orc)

User requested while listening in bypass mode:

| preset | param         | before | after | reason                          |
|--------|---------------|--------|-------|----------------------------------|
| TS     | `gkDelMix`    | 0.12   | 0.09  | spring delay wet to 75% of prior |
| TS     | `gkTSAmtAcc`  | 0.60   | 0.891 | TS drive bumped over the session: 0.60 → 0.75 → 0.825 → 0.891 |
| CLEAN  | `gkDelMix`    | 0.12   | 0.09  | spring delay wet to 75% of prior |

Comment header in `midi_handler.orc` still lists old "SLTDIRTY/DIRTY"
preset names that pre-date the current 5-button reorganization (per
2026-05-13 Phase 2 — CLEAN · TS · PHASER · TREMOLO · AMBIENT). Comment
is cosmetic-only; runtime behavior matches the new layout.

### Under-voltage event mid-session

Pi briefly threw `vcgencmd get_throttled = 0x50005` (under-voltage
NOW + currently throttled + both occurred-since-boot) and ran at
1 GHz instead of 2.4 GHz for a window of the session. Cleared on its
own — final state `0x50000` (occurred-since-boot only, not currently).

Symptom-relevant: csound DSP at 1 GHz has < half the budget per cycle
that it should — explains why FX-engage glitched even in bypass mode
during that window, but was clean before and after. The
`[[project_pi5_usb_current]]` memory documents the underlying issue:
Pi 5 with USB audio + DSP needs the official 5V/5A USB-C PSU AND
`usb_max_current_enable=1` in `/boot/firmware/config.txt`. If
under-voltage events keep recurring, the PSU is the next thing to
verify.

CPU governor confirmed correct: `performance` on all four cores,
`scaling_cur_freq = 2400000` under synthetic load. The issue is
firmware-level voltage protection clocking down regardless of OS
governor when current can't be delivered. Not a software fix.

### Probe additions (not strictly load-bearing for this fix)

Improved the probe harness during the discovery process, even though
the probe itself didn't find this glitch:

- `inject.csd` switched from single 1 kHz sine → pink noise → multi-tone
  (5 sines at 87/240/661/1820/5010 Hz). User noted that noise masks
  glitches; multi-tone leaves spectrum-gaps where glitches stand out.
- `analyze.py` got six independent detectors (HF clicks, crackle
  windows, repeated-sample stalls, sample discontinuities, RMS
  dropouts, RMS spikes) + tone-purity dB metric.
- `probe.sh` added `--fx` (engage heavy FX via `claude_midi.sh tap`)
  and `--play` (play recorded WAV back through Scarlett, bypassing
  chain, so ears can verify what the analyzer captured).

These work as designed. They didn't catch the FX-engage glitch because
the glitch only appears under specific conditions the probe didn't
exercise (signal-dependent + chain-state-dependent). The probe stays
useful for measuring latency and for monitoring across config changes.

### Operational state at end of session

```
bypass = on              in ~/NEPTR_phase4/live.conf
demiurge.service         enabled (autostart) + skipped at runtime via ExecCondition
pipewire / wireplumber   inactive (bypass script stops them)
csound (PID …)           -+rtaudio=alsa -odac:plughw:CARD=USB,DEV=0
                         -iadc:plughw:CARD=USB,DEV=0 -r 48000
                         -B 512 -b 64 --realtime
preamp                   active (always-on, drive=1.4)
TS preset                gkTSAmtAcc=0.75, gkDelMix=0.09
CLEAN preset             gkDelMix=0.09
```

Software-latency RTT: **~4 ms** at period=64.

### Files changed/created this continuation

| path                                                  | what                                             |
|-------------------------------------------------------|--------------------------------------------------|
| `neptr/csound/effects/preamp.orc`                     | new — always-on soft-saturation UDO              |
| `neptr/csound/neptrPhase4.csd`                        | +#include `effects/preamp.orc`                   |
| `neptr/csound/signal_chain.orc`                       | + Preamp UDO call at chain position 0            |
| `neptr/csound/midi_handler.orc`                       | TS + CLEAN preset value updates                  |
| `config/pi5-performance/demiurge-bypass-gate.sh`      | new — gate script (mirrors Pi `/usr/local/bin/`) |
| `config/systemd/demiurge.service.d/bypass-gate.conf`  | new — ExecCondition drop-in                      |
| `testing/probe/inject.csd`                            | multi-tone signal (was sine, briefly pink noise) |
| `testing/probe/analyze.py`                            | six detectors + tone-purity dB                   |
| `testing/probe/probe.sh`                              | `--fx` + `--play` flags                          |
| `testing/probe/README.md`                             | doc updates for new flags + detectors            |
| `testing/bypass/README.md`                            | empirical results added (clean at 64)            |

Memory entries written:
- `project_bypass_mode.md` — bypass status + tradeoffs
- `feedback_read_docs_first.md` — re-derive setup from current docs

### Known follow-ups (not done this session)

1. **PSU verification** — confirm 5V/5A USB-C PSU + `usb_max_current_enable=1`
   to stop the 0x50005 events from recurring.
   **RESOLVED 2026-07-13 — see `incidents/2026-07-13-psu-verification-resolved.md`.**
2. **Bypass gate's pair: an auto-launch unit for bypass.** Right now if
   bypass=on at boot, the gate prevents demiurge from running, but
   nothing launches `run-bypass.sh`. User must `ssh ... run-bypass.sh`
   after boot. A `demiurge-bypass.service` with the *opposite*
   ExecCondition would auto-launch bypass when `bypass=on`.
3. **Promote bypass into the Rust launcher** — add `bypass` and
   `audio_interface` to `LiveConfig`, have the launcher fork raw-ALSA
   csound directly when bypass=on. Eliminates the
   stop-services-then-launch dance and the script's job.
4. **Tier-3 diagnosis of the original sync-layer corruption** — set
   up a chain where csound writes directly to alsa_output via pw-link
   (skipping demiurge-sink loopback) under sync mode, and test FX-engage.
   If the glitch persists, it's the alsa_output ring; if it goes away,
   it's the loopback module. Either result narrows where to invest.
5. **Period=32** (~3 ms RTT) requires `globals.orc::ksmps = 32`
   (currently 64). Untried.

---

## Session continuation — mode-select replaces gate; chain polish (preamp/TS/pitchfork/presets)

After the gate+sibling-service architecture above, we collapsed both
units into a **single demiurge.service that decides at runtime which
audio engine to exec**, then iterated on the csound chain itself. The
gate-as-ExecCondition design is GONE; what's described above as
`bypass-gate.conf` + `demiurge-bypass.service` no longer exists. The
incident's "what's next" item #2 (auto-launcher for bypass) is now
implemented as the mode-select wrapper.

### Architectural shift: one service, runtime mode-select

The two-unit gate design (demiurge.service skipped + demiurge-bypass.service
runs, or vice versa, both `enabled`) was replaced by a single wrapper
that systemd execs in place of the launcher binary:

- `/usr/local/bin/demiurge-mode-select.sh` (root, repo source at
  `config/pi5-performance/demiurge-mode-select.sh`) reads
  `~/NEPTR_phase4/live.conf`:
  - `bypass = off` (or unset) → `exec /opt/demiurge/bin/demiurge-launcher`
    (normal Rust launcher with the PipeWire/JACK chain, unchanged).
  - `bypass = on` → drops privileges to `gorpi7`, stops
    `pipewire`/`wireplumber` user services (PW's ALSA monitor still
    holds the device open), then `exec chrt --fifo 74 csound …
    -+rtaudio=alsa -odac:plughw:CARD=USB,DEV=0
    -iadc:plughw:CARD=USB,DEV=0 -r 48000 -B <8×PERIOD> -b <PERIOD>
    --realtime <csd>`.
- `config/systemd/demiurge.service.d/mode-select.conf` clears the
  unit's ExecStart and points it at the wrapper. Also bumps
  `LimitRTPRIO=99` and `LimitMEMLOCK=infinity` so the in-process
  csound can chrt-74 freely.
- `config/systemd/demiurge.service.d/wait-pipewire.conf` was rewritten
  to be **bypass-aware**: each of its three ExecStartPre lines
  short-circuits with `exit 0` if live.conf says `bypass = on`, so the
  unit can launch raw-ALSA csound without waiting for
  `demiurge-sink:playback_FL` (which won't exist when PW is stopped).
- `config/systemd/demiurge.service.d/60-scrub-stray-mics.conf` and
  `config/systemd/demiurge.service.d/cpu-affinity.conf` (CPUAffinity=3)
  remain as before.

**Files deleted from repo this session:**
- `config/pi5-performance/demiurge-bypass-gate.sh` (replaced by mode-select)
- `config/systemd/demiurge-bypass.service` (no longer needed; single unit)
- `config/systemd/demiurge.service.d/bypass-gate.conf` (replaced by mode-select.conf)

User workflow is identical to before: edit `bypass = on/off` in
live.conf, reboot or `sudo systemctl restart demiurge`. The single
service decides at start time.

### Period reverted to 256

Mode-select's `PERIOD` env var (`DEMIURGE_BYPASS_PERIOD`) defaults to
**256** (was 64 → 128 → 256 across the session). Tightened periods
made FX-engage glitches return on some inputs; 256 is the current
known-good for daily use. csound is launched with `-b 256 -B 2048`.

### Preamp — five stages, drive lifted to 2.0

`neptr/csound/effects/preamp.orc` grew from a one-line tanh UDO into
a five-stage front-end. Still always-on, still zero MIDI bypass.
Order, all on each channel:

1. HP @ 80 Hz (`butterhp`) — kill subsonics below low-E (82 Hz).
2. Low-shelf @ 300 Hz, -2 dB (`pareq` mode 1) — de-mud lower mids.
3. High-shelf @ 3 kHz, +2.5 dB (`pareq` mode 2) — pick / presence lift.
4. LP @ 8 kHz (`tone`, one-pole) — fizz rolloff.
5. `tanh(aL * iDrive) / iDrive` with `iDrive = 2.0` (was 1.4) — soft
   saturation, sample-point math, no buffering.

All filter coefficients are i-rate, minimum-phase IIR or stateless,
so the added latency stays sub-millisecond in the guitar band.

### Tube screamer — RMS comp removed, static makeup curve

`neptr/csound/effects/tubescreamer.orc` had matched-loudness compression
(`rms` followed by `portk kComp, 0.05`). It pumped sustain: rms lagged
~5 ms, portk smoothed another 50 ms, so during fast decays kComp
ducked and the tail vanished. Replaced with **static drive-dependent
makeup**, no envelope follower, no feedback:

```
kMakeup = 0.026163 + gkTSAmtAcc * 0.03270375
```

(Started open-loop at `0.60 + drive*0.80` ≈ 1.20 at preset; way too
hot. Cut 8× to `0.30 + drive*0.40`; still hot. Cut another 15% to
`0.04 + drive*0.05`; close. Cut another 5% combined with +10% drive.
Then -15%, then -10%, then another -10% to land at the current
values.) Open-loop, stable across all input levels; sustain decays
linearly with the input rather than being chased by the comp.

### Pitchfork — pvscale crash fix + hopsize bump

`neptr/csound/effects/pitchfork.orc` was crashing csound at FX-engage.
Bug: `pvscale fLocked, kRatio, 1, 7` — the trailing `7` was setting
`kgain=7` (argument order is `kscal, kkeepform, kgain, kcoefs`; the
author probably meant `kcoefs=7` but mispositioned). With the 8×
overlap-add from hop=64, peak amplitude accumulated to ~56× and
produced Inf/NaN samples that killed csound. Fix: drop the `7` and let
`kgain` default to 1. Also bumped **hop 64 → 128** (4× overlap, latency
512+128 = 13.3 ms) so the ksmps=256 grid lines up more comfortably.

### Presets — single source of truth in `midi_handler.orc`

Three live presets, all sharing the same vibrato + spring-delay base:

| preset    | vibrato (freq, depth)  | spring delay (ms/fb/mix)  | TS              | reverb              |
| --------- | ---------------------- | ------------------------- | --------------- | ------------------- |
| CLEAN     | 0.5796 Hz, 0.00115 s   | 60.6 / 0.62 / 0.09        | off (+10% gain) | off                 |
| TS        | 0.5796 Hz, 0.00115 s   | 60.6 / 0.62 / 0.09        | drive=0.891     | off                 |
| AMBIENT   | 0.5796 Hz, 0.00115 s   | (off)                     | off             | FB=0.92, mix=0.70   |

**CLEAN +10% post-preamp gain:** `signal_chain.orc` has an inline
`if (gkCurrentPreset == 0)` block right after the Preamp UDO call that
multiplies aPrevL/R by 1.10. Pure level boost — no coloration —
applies ONLY in CLEAN. Lets the "super clean" preset sit hotter into
the downstream FX (vibrato + delay) without changing tone.

Vibrato (`freq=0.5796 Hz`, `depth=0.00115 s` ≈ 1.15 ms) is shared
across all three so the rotary "swirl" character is preset-independent.
Spring delay at 91% dry / 9% wet (`gkDelMix=0.09`), 62% feedback, 53 ms
(32nd-notes-but-15%-faster).

**TS toggle-off behavior changed:** pressing the TS preset button when
TS is already active now falls back to **CLEAN preset** (not full dry).
CLEAN and AMBIENT preset buttons still go to full dry on re-press.
Same TS button while a different preset is active selects TS.

CC75 (slot 1 button at menu 0) remains the cross-preset TREMOLO
toggle. CC76 (slot 2 button at menu 0) remains the cross-preset PHASER
toggle. CC74 (slot 0) is AMBIENT, CC72 (slot 4) is CLEAN, CC71
(slot 3) is TS. Comments at the top of `midi_handler.orc` still list
the older SLTDIRTY/DIRTY names — those rows are cosmetic; runtime
behavior is the new layout described above.

### Probe state (unchanged)

The probe harness (`testing/probe/`) is still the multi-tone signal at
87/240/661/1820/5010 Hz with six glitch detectors + tone-purity dB, and
the `--fx`/`--play` flags. No probe changes this continuation.

### Operational state at end of session

```
demiurge.service         single unit, enabled, ExecStart=mode-select.sh
mode-select.sh           reads live.conf bypass=on/off and execs accordingly
bypass = on              in ~/NEPTR_phase4/live.conf
                         (mode-select stops pipewire+wireplumber, execs csound)
csound                   chrt --fifo 74 csound -+rtaudio=alsa
                         -odac:plughw:CARD=USB,DEV=0
                         -iadc:plughw:CARD=USB,DEV=0
                         -r 48000 -B 2048 -b 256 --realtime
preamp                   active (always-on, iDrive=2.0, 5 stages)
TS preset                drive=0.891, kMakeup≈0.055
                         (formula: 0.026163 + gkTSAmtAcc * 0.03270375)
TS button toggle-off     falls back to CLEAN preset (not dry)
all presets              share vibrato freq=0.5796 Hz, depth=0.00115 s
CLEAN / TS               spring delay 53 ms / fb=0.62 / mix=0.09
AMBIENT                  reverb FB=0.92, mix=0.70 (no delay)
pitchfork                pvscale with default kgain (no crash), hop=128
```

Software-latency RTT @ period=256: ~5 quanta×256/48k = **~26 ms** worst
case via the ring; bypass collapses that to **2×256/48k ≈ 10.7 ms** of
software latency plus ~1.7 ms IO group delay.

### Files touched in this continuation

| path                                                   | what                                                                |
| ------------------------------------------------------ | ------------------------------------------------------------------- |
| `config/pi5-performance/demiurge-mode-select.sh`       | new — root wrapper, chooses launcher vs raw-ALSA csound at exec     |
| `config/systemd/demiurge.service.d/mode-select.conf`   | new — override unit's ExecStart, bump RT limits                     |
| `config/systemd/demiurge.service.d/wait-pipewire.conf` | rewritten — each ExecStartPre is bypass-aware (early exit 0)        |
| `config/pi5-performance/demiurge-bypass-gate.sh`       | deleted — replaced by mode-select.sh                                |
| `config/systemd/demiurge-bypass.service`               | deleted — single unit now                                           |
| `config/systemd/demiurge.service.d/bypass-gate.conf`   | deleted — replaced by mode-select.conf                              |
| `neptr/csound/effects/preamp.orc`                      | grew to 5 stages (HP/lo-shelf/hi-shelf/LP/tanh), iDrive 1.4 → 2.0   |
| `neptr/csound/effects/tubescreamer.orc`                | RMS comp removed; static drive-based makeup curve                   |
| `neptr/csound/effects/pitchfork.orc`                   | dropped bogus kgain=7 (was crashing csound); hop 64 → 128            |
| `neptr/csound/midi_handler.orc`                        | vibrato bumped; TS toggle-off → CLEAN; gkDelMix=0.09; gkTSAmtAcc=0.891 |
| `neptr/live.conf`                                      | `bypass`, `audio_interface` fields (committed earlier this session) |

### Memory entries updated this continuation

- `project_bypass_mode.md`     — current period/state + mode-select reference
- `project_bypass_gate.md`     — rewritten to reflect mode-select architecture
- `project_neptr_preamp.md`    — 5-stage front-end, iDrive=2.0
- `MEMORY.md` index            — keeps pointers current

