# 2026-10-02 — audible "bitcrush" on the rebuilt rig (UNRESOLVED)

**Status: RESOLVED 2026-10-03** (see the RESOLVED section at the bottom; the text between is the original handoff, kept as written). Handoff for a fresh investigator. The previous session fixed a
lot of real bugs, none of which fixed this symptom.

---

## The symptom

User's words, verbatim and repeatedly:

> "sound is completely fucked bitcrushed"
> "bruh it is not volume it is fucking bitcrushed"
> "NO DIFFERENCE WHAT THE FUCK IS DIFFERENT WHAT GOT LOST SETTINGS, BIT RATE,
> SAMPLE RATE, MISMATCH, DEMIURGE DEFAULTS"
> "something w the hifi berry or synclayer was bitcrushing"

He is at the bench and can hear it. You cannot. **Take his ears as the
measurement of record.** The previous session twice talked itself out of his
description in favour of its own analysis and wasted his time both times.

It is audible with `sync_layer = on`. He believes bypass was cleaner, but that
was never A/B'd rigorously — see "Not actually established" below.

---

## Context: this is a fresh rebuild

The Pi 5's SD card was physically destroyed on 2026-10-01 and the rig was
rebuilt from scratch on 2026-10-02. **So every difference from "the old rig that
worked" is a candidate.** Things known lost with the card:

- `~/demiurge/state/neptr_settings.ftab` — the persisted engine state. The file
  that exists now was created fresh today; it is NOT his tuned state.
- `~/demiurge/presets/` — Pi-side presets (`PRESET_DIR` is independent of the
  repo; only `neptr/presets/chiller_ambient.*` is tracked).
- `/var/lib/alsa/asound.state` — the codec mixer state.
- `~/demiurge/touch-cal.conf` — recalibrated today, fine now.
- `~/demiurge/live.conf` — the installer "never overwrites live.conf", but
  there was none on a fresh card, so it installed the **repo default**. His
  real rig had months of tuning in that file. **This is a strong lead and was
  never fully chased.**
- `~/demiurge/sets/` — EMPTY. The web API reports the active patch as
  `neptr_gorcore`, and memory references `sets/neptr_gorcore.conf` carrying its
  own quantum. That set file does not exist on the rebuilt rig.

---

## Ruled out, with evidence

Each of these was measured, not assumed.

| Suspect | Evidence it is NOT the cause |
|---|---|
| Sample-rate mismatch | live.conf 48000 · PipeWire `force-rate 48000` · card `rate: 48000` · csound `-r 48000`. All four agree. |
| Period/quantum mismatch | quantum 128 · card `period_size: 128` · csound `-b 128`. Agree. |
| Bit depth | Was genuinely broken (see Fixed). Now csound runs `-f` (float) and card is `S32_LE`. Did not fix the symptom. |
| ksmps/quantum lockstep | Was genuinely mismatched (64 vs 128). Now both 128 via `csound_extra = --omacro:KSMPS=128`. Did not fix it. |
| Codec mixer gain | PGA Gain L/R `0.0dB`, ADC `0.00dB` (note: 23% of range IS 0 dB on this control — do not "fix" it to 85%, that applies +32 dB), Digital 207/207 0 dB, Analogue 100%, all three `Max Overclock` at 0. |
| De-emphasis | Was ON (driver default) with DSP Program `FIR interpolation with de-emphasis`. Set to OFF / `Ringing-less low latency FIR` and `alsactl store`d. No change to the symptom. |
| csound ring depth (`-B`) | Tried 256 (= 2×quantum, current wrapper default) and 1024 (the older verified value). No audible difference. Reverted to default. |
| Hardware ADC→DAC passthrough | No monitor/loopback/sidetone control exists in this codec's mixer. |
| Multiple clients on the card | `pw-link -l` shows only csound ↔ the HiFiBerry. |
| xruns | 0 across every check, including `xrun_60s` from the web API. The artifact is **not** underrun-driven. |

---

## Changed during the hunt (current state ≠ old rig)

Anything here could in principle have introduced its own problem; all were
intended as fixes.

1. **Bela audio card hidden.** `config/wireplumber/99-demiurge-disable-bela-audio.conf`
   existed in the repo since 2026-09-25 and had **never been installed**. The
   Bela enumerates a dead 44.1 kHz USB audio card; the launcher counted it as a
   second wired output, direct mode disengaged, and the chain became
   `csound(128) → demiurge-sink loopback(node.latency 256) → DAC(128)`. That
   file's own header calls this *"the IR crackle that survived three different
   IR implementations"* and says it clicks **with no xrun and no CPU load** —
   which matches this symptom closely. Installing it restored
   `graph: OUT direct → ...pro-output-0 (demiurge-sink loopback bypassed)`.
   **The user reports the artifact persisted after this.** Either it is a second
   cause, or the rule did not take full effect — worth re-verifying from
   scratch.
2. **Interface pinned** via `demiurge-interface "HiFiBerry DAC+ADC Pro"` →
   `~/demiurge/interface.conf`. This is the user-config mechanism, not a
   hardcode. He asked for an explicit auto/aggregate/manual **mode** as a
   first-class config concept; that work was started and then stopped, so it is
   NOT implemented.
3. **`csound_extra = --omacro:KSMPS=128`** appended to live.conf.
4. **`-f` (float)** added to the JACK branch of `demiurge-run-csound`.
5. **`loginctl enable-linger`** — this was the cause of a *different*,
   genuinely severe problem: without it PipeWire only ran while someone was
   SSH'd in, so the HiFiBerry node vanished on logout and csound crash-looped.
   Fixed and verified.
6. `dtparam=spi=on` (touchscreen), touch recalibrated, venv rebuilt against
   apt's `python3-pygame` (the pip wheel bundles an SDL2 without kmsdrm).

---

## Not actually established (do not inherit these as facts)

- **That bypass is clean and sync-layer is not.** `sync_layer` was flipped to
  `off` once, the user was not asked to confirm the artifact specifically, and
  two other things changed in the same window. Never A/B'd properly. **Do this
  first — it is cheap and it halves the problem.**
- **That the artifact is in the signal path at all.** It may be the NEPTR patch
  running at fresh defaults with no saved state or presets. Memory notes NEPTR
  has 13 always-on instruments and effects blended with `ntrpol` dry/wet rather
  than gated, so defaults are not necessarily a neutral sound.
- **The previous session's capture analysis.** Two captures of `csound6:output1`
  via `pw-jack jack_rec` showed long runs of identical samples (256, then 128,
  tracking `-B`/`-b`) and few distinct values. That *looked* like a smoking gun
  and was presented as one. But `jack_rec` writes **16-bit** regardless of `-b`,
  and the signal was only **−47.8 dBFS RMS / 0.027 peak**, where a 16-bit file
  legitimately has ~1500 distinct values and long runs in quiet passages. The
  long runs were checked and sit at values of ±2…±18 out of 32768 — i.e. the
  bottom few bits of the capture, not necessarily of csound's output. **The data
  is inconclusive, not exculpatory.** A real 32-bit float capture would settle
  it; `jack_rec` cannot provide one.
- **Why the output was 30 dB down** (peak 0.027). Never explained. Could be
  normal if he was barely playing, or could be a real gain-staging problem. The
  user rejected the volume theory, so it was dropped — but it was never
  *disproven*, and "quiet signal amplified at the monitors" is audibly
  indistinguishable from bitcrush.

---

## The right tool, now installed

**`demiurge-ringtest`** (`src/wrappers/demiurge-ringtest`) is purpose-built for
exactly this question and was **not installed** on the rebuilt rig:

> "capture the guitar input and the engine output at the SAME instant, then say
> which one the ring is in. A ring present on the input is analog or acoustic
> and no amount of DSP work will touch it; a clean input under a ringing output
> is generated inside the chain, and its frequency says where."

It discovers the input node correctly via `pw-dump` (a manual attempt to record
`csound6:input1` failed because that is a sink, not a source). Run it **while
the artifact is audible**: `demiurge-ringtest [seconds]`, default 20.

Commit `21185e2` changed the installer to install `src/wrappers/*` and
`config/wireplumber/*.conf` by **glob**, so re-running the installer now lands
this and the Bela rule.

---

## Suggested order of attack

1. Re-run `setup-script/setup-demiurge.sh` (or at least Phases 3–7) so every
   artifact is present. Verify `demiurge-ringtest`, `demiurge-quantum-limits`,
   `demiurge-interface` and the Bela WirePlumber rule all exist.
2. **`demiurge-ringtest` while the artifact is audible.** Input vs chain. This
   is the branch point and everything else is guesswork until it is answered.
3. A/B `sync_layer = on` vs `off` with the user listening and **nothing else
   changing**. Ask him explicitly about the artifact, not "how does it sound".
4. If the artifact is in the chain: suspect patch state/presets, not transport.
   Compare against `neptr/presets/chiller_ambient.*` or a minimal `.csd` through
   the same path.
5. If the artifact is on the input: it is analog/acoustic or the ADC. Nothing in
   DSP or PipeWire will touch it.
6. Reconstruct `live.conf` against what his tuned rig had, rather than the repo
   default. `sets/neptr_gorcore.conf` is missing entirely and the launcher
   reports that as the active patch.

---

## Constraints (hard rules, from the user)

- **Never restart `demiurge.service`, csound, or `neptr-ui` without asking each
  time**, and never leave the engine down. Recovering an already-dead engine is
  fine.
- **No audio tests without direct permission each time.** Playing a tone through
  his monitors unannounced is exactly what this guards against.
- Never touch the EEPROM / `rpi-eeprom-update`. `usb_max_current_enable=1`
  stays. He owns `power=` / config.txt throttling.
- Phase 8 (overclock + reboot) is unapproved; he may also swap to an 8 GB board.
- Pi user is **`gorpi7v2`**, host `demiurge`. `/home/gorpi7` literals have
  caused three separate production failures — grep for them.

## Access

`ssh gorpi7v2@demiurge.local` — key auth, NOPASSWD sudo. The Pi was offline at
the time of writing; reconnect before assuming anything above is still live.

---

# Investigation 2 (2026-10-02, repo-only, Pi offline)

Nothing below was measured. The Pi was offline, so this is a read of the repo against the
verified-good rig in `docs/2026-08-12-state.md`. Every item is a lead to MEASURE when the Pi is back.

His verified-good config: `sync_layer = on`, `quantum = 64`, `csound_extra = --omacro:PRESET_BANK=2`,
device period 64x8 both directions (a generated `60-demiurge-period.conf`, lost), pro-audio profile,
2 channels. The rebuilt rig got the repo default (quantum 128, no PRESET_BANK) instead.

## The 8 findings

1. **`~/demiurge/sets/neptr_gorcore.conf` is gone.** His active set. The repo only tracked surf, bass, car, testing.
2. **The generated period file is gone, and rate never followed `live.conf`.** `/etc/wireplumber/wireplumber.conf.d/60-demiurge-period.conf` is written by `demiurge-quantum-limits apply-period`, so a fresh install never has it. The device fell back to 128x4. A hand-edit of `quantum =` never moved the device period (the launcher only vets). The device-side rate pins (50-demiurge.conf USB rule, 99-demiurge-lowlatency.conf allowed-rates, virtual.conf) were hardcoded 48000, so a non-48k `rate =` would silently resample.
3. **Codec mixer state lost.** `/var/lib/alsa/asound.state` was gone, so ADC PGA gain and the ADC input mux sit at driver defaults. The previous session measured engine output at -47.8 dBFS RMS / 0.027 peak and never explained it. A guitar into a line-level ADC at 0 dB PGA gives exactly that. NEPTR's onset detectors and NAM pregains are calibrated for a hotter signal. No tool stored or restored the mixer, and the repo recorded none of his values.
4. **STATE_DIR on the SD card.** `demiurge-state-ram` (tmpfs state, mirrored to SD by a timer) was hand-placed on the old card. Its drop-in was never installed (the installer glob skipped `*.conf`) and the script hardcoded `/home/gorpi7`. `demiurge-run-csound` passes `--omacro:STATE_DIR=$HOME/demiurge/state` (SD). Instr 97 then writes the settings table on the AUDIO THREAD every 2 s while anything changes, measured at 2.3 to 8 ms per write against a 1.33 ms period. An audible periodic glitch with no xrun counter attached.
5. **`demiurge-capture-guard` was installed and enabled by Phase 7d.** It runs as root, hardcodes `gorpi7`, and its theory was disproved (memory: adc-capture-dma-stall). On a stall false positive it stops demiurge, stops PipeWire as the wrong user and rebinds the codec.
6. **Patched `librtjack.so` not installed.** `config/csound/librtjack.so` fixes a stack smash on JACK port names longer than 63 characters. Pro-audio names like `alsa_output.platform-soc_107c000000_sound.pro-output-0:playback_AUX0` qualify. The installer never installed it, and `deploy-rtjack.sh` pointed at a path that does not exist on the rebuilt rig.
7. **`60-scrub-stray-mics.conf` hardcodes a Scarlett Solo serial.** Dead and brand-named; the launcher already scrubs stray links generically.
8. **No backup of the Pi's user-owned state existed anywhere.**

## Ranked likely causes of the audible artifact (measure in this order)

a. Mixer state: input gain and ADC mux. Explains the -30 dB signal; detectors and NAM would chatter.
b. STATE_DIR on SD: audio-thread SD writes every 2 s.
c. Wrong set: quantum 128 plus the other session's `--omacro:KSMPS=128` and PRESET_BANK 0, instead of his 64/64/bank 2. The patch has been tuned at ksmps 64 since 2026-07-26.
d. Stock librtjack, or direct mode not actually engaged. Re-verify with `demiurge-audit-audio`.
e. Anything `demiurge-ringtest` attributes to the input side (analog). Out of DSP's reach.

## What shipped (repo only, nothing deployed)

Architecture: ONE knob. `live.conf` `rate =` / `quantum =` drive everything device-side through `demiurge-quantum-limits lockstep`.

- `src/wrappers/demiurge-quantum-limits` — `lockstep`, user-dir fragments (period, rate, pipewire clock, and a rate-substituted shadow of `demiurge-virtual.conf` so the loopbacks follow `rate =` too), `--dry-run`, metadata pin. Never fails the service. Known limit: the aloop and PMOR rules (90-/95-) still pin 48000 and sort after the generated rate file; PMOR is fixed at 48 kHz in firmware, so that is correct.
- `src/wrappers/demiurge-audio`, `src/wrappers/demiurge-run-csound`, `src/wrappers/demiurge-state-ram`, `src/wrappers/demiurge-capture-guard` — apply path, RAM-aware STATE_DIR, `$HOME`-derived SD dir, no user literal.
- `src/wrappers/demiurge-mixer` (new) — `show|store|restore|gain|input`; store writes `alsactl` and `~/demiurge/asound.state`.
- `src/wrappers/demiurge-audit-audio` (new) — read-only one-shot report across every layer.
- `setup-script/demiurge-qrestore` — user-dir drop-in, home derived from the user.
- `setup-script/setup-demiurge.sh`, `setup-script/deploy-rtjack.sh` — drop-in glob, sets seeding, rig-backup seeding, librtjack swap, capture-guard disabled, retired `/etc` period file removed, lockstep run at end of Phase 7.
- `config/systemd/demiurge.service.d/10-clock-lockstep.conf` (new), `20-state-ram.conf` (moved from `src/wrappers/`), `config/_attic/60-scrub-stray-mics.conf` (moved).
- `config/pi5-performance/demiurge-pipewire-lowlatency.conf`, `config/wireplumber/50-demiurge.conf` — comments: static files are boot seeds.
- `neptr/scripts/sets/neptr_gorcore.conf` (new), `demiurge/live.conf` (quantum 64, PRESET_BANK=2, set tag).
- `setup-script/backup-rig.sh`, `setup-script/restore-rig.sh`, `rig-backup/README.md`, `.gitignore` — the backup kit (finding 8).
- Docs: `demiurge/docs/gotchas.md` (Q7c lockstep), `demiurge/docs/config-reference.md`, `INSTRUCTIONS.md` (sections 10, 11, 12), `setup-script/README.md`.

## Pi-side runbook (when it is back online)

Do these in order. Do not restart audio or play test audio without his go-ahead each time.

1. `demiurge-audit-audio` — paste the output. Checks every layer at once.
2. `demiurge-mixer show` — read PGA gain and ADC input mux. If PGA is 0 dB: `demiurge-mixer gain 20` (then by ear 10-30), `demiurge-mixer store`. This is the -30 dB lead.
3. Deploy repo (sync) -> `sudo setup-script/setup-demiurge.sh 6` then `7` (installs the lockstep drop-in, sets, state-ram drop-in, rtjack, disables capture-guard). Then, WITH HIS OK, `demiurge-set neptr_gorcore`.
4. `demiurge-ringtest 20` while the artifact is audible: input vs output.
5. A/B `sync_layer` on/off with NOTHING else changing, asking specifically about the artifact.
6. When it sounds right: `demiurge-mixer store`; on the Mac `setup-script/backup-rig.sh`; commit.


---

## RESOLVED 2026-10-03

**Cause.** The NAM csound plugin on the rebuilt Pi was built from a stale
single-instance copy of the opcode (`DEMIURGE_OS/nam/src/`, via
`setup-script/deploy-nam.sh` -> `nam/setup/build-csound-nam.sh`). It ran L then R
through ONE autoregressive model per 64-sample block, so each channel convolved
against the other channel's state. The correct per-channel opcode
(`get_active_pair`, `modelL`/`modelR`, calibrated pregain) lives in
`neptr/csound/nam/src/` with a vendored NeuralAmpModelerCore in `libs/`. The
build script also cloned unpinned upstream master (the 2026-09-18 core) instead of
the vendored v0.5.3.

**Proof.**
- `testing/nam/test-nam.sh` TIER 2b (channel isolation): CROSSTALK -4.5 dB FAIL
  with the stale build, -inf PASS with the correct one.
- The decoded effect-state table showed NAM was the ONLY active effect.
- Spectra showed +25 dB of HF content with no sample holds, no quantization steps
  and no clipping: not a bit-depth/rate problem, which is why every
  rate/bit-depth hunt found nothing.

**Fix on the Pi.** Rebuilt from `neptr/csound/nam` (vendored core), both plugin
paths unified into the single
`/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so`, sha 3beae64d78d50f18.
In the repo: `nam/src` retired to `nam/_attic_src_single_instance/` (with README),
`deploy-nam.sh` and setup-demiurge.sh Phase 5b both call
`neptr/csound/nam/build-on-pi.sh` (vendored core, pinned fallback clone, -j2),
and Phase 5b fails loudly if `get_active_pair` is missing from the .so.

**Collateral fixes (real, but NOT the cause).** ADC PGA 0 dB -> +20 dB stored;
RAM state dir; gorcore set restored; lockstep; Ethernet static IP lost (re-set).

**Brownout.** The first rebuild attempt used `-j4`; the Pi 5 browned out and
REBOOTED mid-build (undervolt flag 0x50000). Build parallelism is now capped at
`-j2` in `build-on-pi.sh`.

## Investigation 3 — the 10 Hz chop (2026-10-03)

**Symptom.** The player heard NEPTR "cutting in and out". The patch has
tempo-synced effects; the tempo was wrong.

**Measured on the rig (facts).**
- `demiurge-clock` sends 0xF8 + CCs to Midi Through `14:0` (port 131:0 -> 14:0)
  AND subscribes its input (14:0 -> 131:1). Midi Through re-broadcasts every
  event to all subscribers INCLUDING the sender and strips the source
  (aseqdump shows everything from 14:0), so the `source.client == self` guard
  never fired.
- In follower mode the daemon recovered tempo from its own echoed ticks:
  positive feedback. BPM slid 120 -> 40 (floor) in one session and 150 -> 300
  (ceiling) in another. `clock_status.conf` read `role = follower, source =
  external-midi, bpm = 303.5` while the only thing emitting clock was 131:0
  itself; PMOR (20:0) emitted zero 0xF8 in a 2 s dump. At 300 BPM an 8th is
  0.1 s: the chop.
- After `demiurge-sync demiurge` the role became master but the daemon KEPT
  303.5 BPM (it held the recovered tempo). Killing the daemon reset it.
- clockrole auto-election switched to `role=opgorator` purely because PMOR
  was plugged in, though PMOR was not transmitting clock.
- `demiurge-midi-connect` wired the Bela's USB MIDI gadget (ALSA client
  "Bela-7D5B", a dead port) into the hub both ways; the kernel logged
  `ALSA: seq_midi: MIDI output buffer overrun` ~1100 times per 5 s. Its
  EXCLUDE was documented as space separated but matched `*"$EXCLUDE"*`, a
  single substring.

**Shipped (repo; Pi needs rebuild/install, see docs/clock.md).**
- `src/demiurge-clock.cpp`: every sent 0xF8/START/CONTINUE/STOP/CC116 is
  stamped; the first matching receive within 1.5 ms (`SELF_ECHO_WINDOW_S`) is
  dropped as our echo (PMOR's firmware does the same). Master never adopts
  tempo from received 0xF8. Follower->master (CC116=2 or reclaim) re-seeds
  tempo from the last master-side value (--bpm/live.conf, Link, CC118) and
  logs it. An echo-only stream no longer refreshes the 5 s reclaim backstop.
  New `ext_clock = yes|no` field in `clock_status.conf`.
- `src/demiurge-launcher-rs/src/clockrole.rs`: election requires
  `pmor_transport = playing` AND `ext_clock = yes` (tests added, 46 pass on
  the Mac). Documented in `demiurge/docs/clock-role-auto.md`.
- `src/wrappers/demiurge-midi-connect`: EXCLUDE is a real word list, default
  patterns "Teensy MIDI" and "Bela", applied to BOTH directions (device->hub
  and hub->device clock-sink pass); same device/reason as
  `99-demiurge-disable-bela-audio.conf`.
- `demiurge/docs/clock.md`: the one documented daemon + launcher rebuild
  recipe.

**Not verified.** The C++ was not built against real ALSA/Link here (stub
header syntax check only); the echo window and the ext_clock streak have not
been measured on the rig.

## RESOLVED FOR REAL — 2026-10-03 13:10, by ear + measurement

The remaining "dropouts / cutting in and out" was **CHOP (instr 92, GLITCH menu
slot 3, toggle 24) running** — a tick-synced trance gate that mutes/chops
16th-note cells with probability 0.5. It was allocated at engine start alongside
the always-on instruments. A steady 440 Hz tone through the engine showed 281
drops to exact digital zero in 70 s, spacing 0.06/0.12 s at 124 BPM and doubling
at 62 BPM (tempo-locked, probabilistic). Cutting MIDI clock to csound did not
stop it (GLT TICK free-runs at gkGLITCH_bpm). The player switched CHOP off in
the UI and the rig went clean. Working state saved: RAM state flushed to SD,
mixer stored, `~/demiurge/backups/working-20261003/`, `rig-backup/latest/`.

Lesson for the next hunt: `grep "new alloc for instr" ~/.demiurge/logs/*.log`
first — an instrument that is allocated is RUNNING whatever its bypass global
claims, and the glitch block is excluded from preset recall on purpose.
