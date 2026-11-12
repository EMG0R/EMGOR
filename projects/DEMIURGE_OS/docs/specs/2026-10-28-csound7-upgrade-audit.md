# NEPTR Phase 4 — full system audit + Csound 7 upgrade assessment (2026-10-28)

Audited against local `main` (the newest tree; gorcave's `gorpi7v5-main` is a
stale 10-09 publish snapshot, 161 commits behind). gorpi7v5 was offline, so
nothing was measured live — every "measure" item below is still open.
Sources: Csound 7 release notes / csound.com / Debian tracker / Cabbage docs
(web, 2026-10-28); the code audit cites file:line throughout.

## TL;DR

- **Csound 7.0.0 is stable as of 2026-10-21.** apt on RPi OS Trixie still
  gives **6.18.1** and will for the foreseeable future (Debian has 7 flagged
  unpackaged) → 7 means **build from source on the Pi 5**. No arm64 binary.
- **Upgrade is worth doing, not urgent, and must be staged** — never on the
  live rig first. Three compiled things break and need a port + rebuild:
  the NAM opcode (`csound-nam.so`), `demiurge-csound-host`, and the patched
  `librtjack.so` (which becomes unnecessary if upstream's bounded copy holds).
- **The biggest wins available right now need no upgrade at all** (section 3.1):
  cap the OSC drain loops, spread the 330-send UI refresh burst, stop
  computing mono input twice, add denormal protection on seven feedback
  paths, fix the `--check` that rewrites `state_persist.orc`, and drop
  `-B` on the ALSA path from 8×Q to 2–4×Q.
- **What 7 uniquely buys NEPTR:** per-UDO oversampling (oversample only
  drives/NAM), `mkir`+`GENdeconv` (capture your own cab/room IRs in-engine),
  GEN53 size limit lifted, native PipeWire backend, the `thread=5` plugin bug
  gone by design, bounded rtjack port names, a typed language with structs
  and `for…in` that would let the position-addressed state map and the
  unrolled poly-delay macros be rewritten sanely.

## 1. Baseline (as deployed)

| Thing | Value | Where |
|---|---|---|
| Csound | 6.18.1 (apt) | `setup-script/setup-demiurge.sh:298` |
| sr / ksmps | 48000 / **64** default (`#define KSMPS`) | `neptr/csound/globals.orc:12` |
| Live set | bank 2 (gorcore), quantum **128**, `sync_layer=on`, `power=high` | rig-backup `live.conf` |
| Other sets | surf/bass: bank 0/1, q128, `sync_layer=off` (ALSA path); car_mode: `-j1`, Link on | rig-backup `sets/` |
| Effects | 61 `.orc` (46 bus-type via `$FX_BYPASS`, 3 tail, ~9 inline UDOs) | `effects/`, `signal_chain.orc` |
| Always-on instrs | ~20 (score + file-scope `schedule`) | `neptrPhase4.csd:184-203` |
| Control | OSC 7770 → MIDI dispatch loop; 9 `OSClisten` polls, 307 change-detects/k | `midi_handler.orc`, `handler_osc.orc` |
| State | 330 `tablew` slots, position-addressed, A/B files + layout stamp | `state_persist.orc` ↔ `state_unpack.inc.orc` |
| Compiled deps on 6.x ABI | `csound-nam.so`, `demiurge-csound-host`, patched `librtjack.so` (all `plugins64-6.0`) | setup script Phases 5/5b/1a-ii |

Note: `ksmps` is 64 but the live quantum is 128 → 2× the k-rate work the
period needs. Safe (ksmps ≤ quantum), just wasteful; see 3.1.

## 2. Csound 7 — what changes for us

### 2.1 Fixes / improvements that touch documented NEPTR pain
| 6.18 pain | Status in 7 | Confidence |
|---|---|---|
| plugin `thread=5` never schedules aperf (NAM forced to k-rate) | OENTRY reworked (`init/perf/deinit`, no `thread`); ambiguity gone | high |
| rtjack 63-char port-name stack smash (patched `.so`) | master uses bounded `snprintf` into `MAX_NAME_LEN+12`; exact fixing commit not located | medium — **test on device** |
| `compress` ignores ratio/knee changes; `ftconv`/`pconvolve`/`liveconv` bugs | fixed per 7.0.0 notes | high |
| ALSA backend hardcodes 1024-frame buffer, ignores `-B` (21 ms bypass floor) | **not verified** | unknown |
| `getenv` removed, `-iadc` under `ignore_csopts`, macro `$NAME.` rules | **not verified** | unknown |
| `OSClisten`/`OSCinit` | removed in early 7 betas, **back as plugins since 2026-01** → must confirm the plugin ships/loads in 7.0.0 | medium |
| `pvsanal` format arg ignored | not verified | unknown |

### 2.2 New capability worth having
- **Local sample rate in UDOs** (`oversample`/`undersample`) — oversample
  the 5 drives + NAM only, instead of `OS4Core`'s interpreted 4× loop
  (18 call sites, per channel) or the whole orchestra.
- **`mkir` + `GENdeconv`** — sweep + deconvolution inside Csound: capture
  your own cab/room/plate IRs for `ir_reverb.orc` without external tools.
  GEN53 power-of-two limit lifted.
- **Native PipeWire backend** (`-+rtaudio=pipewire`, needs
  `libpipewire-0.3-dev` ≥ 0.3.50) — a third path alongside rtjack and the
  embedded host; worth measuring against both.
- **Language:** explicit types, structs, `for…in`, `switch`, array literals /
  slices, new UDO syntax. Directly relevant to: the position-addressed state
  map (replace with a struct/array single source), the 16 unrolled
  `PDLY_TAP` macros and 5 `HgSendLane` copies (loop-instantiated opcodes
  were impossible in 6), and the 251-`printks` preset elseif chains.
- Background note-init off the audio thread, bounded log queues,
  `--realtime` race fixes — relevant to the cold-start xrun storm
  (workaround #23) and the stderr-from-RT-thread handler (#10).
- UDP server speaks OSC natively; `unschedule`, `stm*` state machines,
  `memplay`, `remap`, `OSCRaw` arrays.

### 2.3 Migration cost — the three compiled dependents
**NAM opcode** (`neptr/csound/nam/src/csound_nam_opcode.cpp`): mechanical.
`OENTRY` → 10 fields (`opname, dsblksiz:size_t, flags, outypes, intypes,
init, perf, deinit, useropinfo, deprecated`): drop `thread`, `iopadr/kopadr`
→ `init/perf`, `nullptr` deinit (or move `nam_deinit` there). `csdl.h` still
exists; `LINKAGE` still provided and now also emits `csoundModuleInfo()`
(loader rejects plugins without it — rebuild is mandatory). `GetSr/GetKsmps`
→ `CS_ESR`/`CS_KSMPS`-style macros. `MYFLT` → `cs_float` (alias kept).
Install dir `plugins64-7.0`, env `OPCODE7DIR64`. Opportunity: run it truly
a-rate now that the `thread=5` bug is gone.

**demiurge-csound-host** (`src/demiurge-csound-host.cpp`, 962 lines, 16 API
calls): ~8 one-line edits —
`csoundCreate(hostData, opcodedir)` gains an arg; `csoundCompile(argc,argv)`
no longer starts the engine → add `csoundStart()` at both call sites;
`csoundSetHostImplementedAudioIO` → `csoundSetHostAudioIO(csound)`;
`csoundGetNchnls/NchnlsInput` → `csoundGetChannels(cs, isInput)`;
`csoundGetInput/OutputBufferSize` removed (spin/spout only);
`csoundGetCurrentTimeSample` → `csoundGetCurrentTimeSamples` (int64);
`csoundCleanup` removed → `csoundReset`/`csoundDestroy`. Unchanged:
`PerformKsmps`, `GetSpin/GetSpout` (**engine no longer clears spin — memset
if a partial block**), `GetSr/GetKsmps`, `GetScoreTime`,
`SetMessageCallback`, `Destroy`. Library stays `libcsound64`, soname 7.0.
Workaround #8 (`ignore_csopts` only via `csoundCompile(argv)`) must be
re-checked since compile/start split.

**librtjack patch:** likely obsolete; verify with a long-port-name device
(the Scarlett) before deleting `config/csound/librtjack.so` + Phase 5.

**Orchestra:** compatibility claimed full. Known break: **unary minus
precedence** (`-2^2` is now −4) — grep every `-x^` / `-k^` site. 74
deprecated opcodes still work; `--error-deprecated` will list ours
(`outs`, `spec*` family if any). Opt-in corrections (`resony`, `metro2`,
`vosim`) default to old behaviour.

**Cabbage:** Cabbage 3 (3.0.15-beta) targets Csound 7, VST3/CLAP/AUv2, still
labelled breaking-changes beta; Cabbage 2 is EOL. The VST twin needs the same
7 pass — and it is already drifting (section 3, D).

### 2.4 Re-test list (6.18 workarounds; the code audit's full table B)
Re-test on 7, in priority order: #1 `getenv`/`--omacro` paths · #2 `OSClisten`
locals→globals + plugin load · #3 `thread=5` (opportunity) · #7/#8 `-iadc` /
`ignore_csopts` precedence · #9 rtjack ring + patched `.so` · #10 stderr from
RT thread · #14/#15 `pvsanal` format arg + hop<ksmps crash · #16 `&&`
short-circuit (array clamps in `sm_partialglide.orc:66`) · #19 loop-instantiated
opcodes · #20 all of poly delay v5 (`mincer` geometry tuned by measurement) ·
#21 `ftsavek` on the audio thread · #23 `--realtime` deferred init · #25 host
I/O default `-b 256` allocation · #27 `-j` (measured flat on 6.18; 7 presence
unverified).
Full 29-row table with file:line lives in the audit transcript; copy into
`docs/csound7-retest.md` when the port starts.

## 3. Findings and recommendations (independent of the upgrade)

### 3.1 Do now on 6.18 — small, safe, high value
1. **`generate_vst.py --check` rewrites `state_persist.orc`** (`vst/generate_vst.py:638`, `emit_state_orc()` runs before the `--check` branch) and its generator is stale for slots 302+. Gate it. This bit the audit itself (restored from HEAD, lossless).
2. **ksmps ceiling is 128, not 256** — real minimum pvsanal hops are 128 (`attack.orc:87`, `onset_pick.orc:64`, rack at N=512); any `KSMPS` > 128 hits the documented crash. Add an init-time assert; fix the docs. Separately: with live quantum 128, consider `--omacro:KSMPS=128` on the gorcore set to halve k-rate work (or drop quantum to 64 to actually use the 64).
3. **Cap the OSC drain loops** (`midi_handler.orc:418, 666, 683` — unbounded on the RT thread) and **spread the `gkOscRefresh` burst** (~330 `OSCsend` = 330 `sendto()` in one 1.33 ms block; likeliest unexplained spike source).
4. **Mono input computed twice**: with `STEREO_IN=1` + silent R, or mono-sum (`signal_chain.orc:82-89`), every pre-stereo stage runs both channels — 2 NAM WaveNets (`nam_loader.cpp:151`), 2 oversamplers per drive, 2 pvs chains. Detect silent R / mono mode and run one lane. Also skip `H90Shift` at semi=0 in the delay (instr 62).
5. **Denormals**: `denorm` only in `reverb.orc:15`, `grainverb.orc:310`. Missing on delay, tapedelay, nebula, habit, freezeverb, diffusion, tape; the embedded host never sets FPCR flush-to-zero. Add `denorm` on every feedback loop; set FTZ in `demiurge-csound-host` `process()`.
6. **NAM loader thread** is a plain `std::thread` (`nam_loader.cpp:36-56`) inheriting FIFO 74 + the engine's core pin → its 2–3 s model load + 1.5 s loudness probe can starve the engine on profile switch (equal-priority FIFO doesn't preempt). **Measure first** (`ps -eLo`, `chrt -p` on the loader TID during a switch); if confirmed, `SCHED_OTHER` + affinity cores 0-2. Model swap is also a hard cut — crossfade it.
7. **ALSA path `-B Q*8`** (`demiurge-run-csound:247` → 1024 frames at q128; surf + bass sets use it) is "inferred, not measured" ≈ 11 ms of ring. Measure, then 2–4×Q.
8. **Dead code / collisions**: `globals.csd` (926 lines, no referrers), `looper_persist.orc` (compiled out), dead UDOs `DopplerSpin/LoFi/STUTfreqshifter/STUTiteration/pitchshifter4`, three `state_persist.orc.*` copies; **instr-number collisions** `tubescreamer.orc:12` (100 vs clock) and `bincrusher_v1_coarsen.orc` (122) sitting unincluded in `effects/` — move to `_retired/`.
9. **Tests assert no DSP and don't compile the orchestra** (393 tests, all UI/host-side). Add: `csound --syntax-check-only` on the real csd (note workaround #18: it false-passes reserved words), and a write/unpack **index-parity** test for `state_persist.orc` ↔ `state_unpack.inc.orc` (13 unpack slots 302-304, 309-318 had no literal write line found — confirm).
10. `CsOptions` in `neptrPhase4.csd` (`-b128 -B256 -Ma -odac:hw:2,0`) are dead under `-+ignore_csopts=1` — delete or comment to stop misleading readers.

### 3.2 Structural (bigger; mostly 6.18-possible, nicer on 7)
- **Bus latency tax**: every `$FX_BYPASS` bus effect costs one ksmps (instr 1 reads last cycle's return; drives were inlined for this reason, `signal_chain.orc:281-289`). 5 active effects ≈ +6.7 ms at ksmps 64 / +13 ms at 128. Order-aware chain (numbered instrs or inline UDOs) removes it. High cost, real latency win.
- **NAM is last in chain** (`signal_chain.orc:782`): delay/reverb/IR feed the amp model. Offer pre/post placement — cheap conceptually, touches state layout.
- **Spectral**: 29 `pvsanal` sites; SPECDLY and BINSCATTER analyse the same bus twice; serial stages (`forms`, `formant`, `bincrush*`, `lossy`, rack) resynthesise→re-analyse. Pass fsigs → saves pairs and one FFT window of latency per hop.
- **Interpreted per-sample loops → C++ plugin** (precedent: NAM): `OS4Core` 4× loop, `SrReducer`, `tape`, `delay` reverse, `blooper`, `stutter`×4, `pllfuzz`, `mood2`, `chainfb`. On 7, `oversample` replaces `OS4Core` outright.
- **Single-source state map** replacing position-addressed `tablew`/unpack — removes the lockstep fragility and the VST drift at once. On 7, a struct/array makes this clean.
- **Duplication** (full table in audit): 5 near-identical OSC-send UDO families (27 hard-coded `127.0.0.1:8000`), 26 hand-rolled `interp` dry/wet blocks (+8 `ntrpol`), 49 `limit ±0.9x` safety clamps, 81 inline DC blocks, 53 `tanh` soft-clips, 4 separate `ptrack` instances (tracksynth, impulse_synth, feedbacker, onward). Shared UDOs for crossfade/limit/DC/softclip/pitch-track are low-risk refactors.
- **VST twin drift**: `NEPTR_VST.csd` lacks 33 includes vs the Pi csd (whole rack, mixer, hyperguitar); `--check` says VST and APP are stale; VST state persistence is partly a separate implementation. Decide: regenerate from one source, or declare the twin a subset.

### 3.3 Musical / usage fit (what the architecture could carry that it doesn't)
- **No tuner / pitch display** — yet four effects each run their own `ptrack`. One shared tracker → tuner page + feeds harmonizer.
- **No global key/scale**: Lydian exists only inside the rack's SuperFilterbank. Pitchfork and poly-delay intervals are chromatic. A global key/scale with a **diatonic-aware harmonizer** and **gamelan (pelog/slendro) tuning tables** for resonator/harmonizer is the single most "you" feature missing.
- **Generative ambient**: no free-running modulators or multi-destination macros; the pedal owns one destination at a time (last toggled wins). The body's **LiDAR×2, FSR×2, accel, joystick, piezo are unmapped** — the `/p` absolute-write path (35 destinations) exists but the pool deploy is staged, not live (`NEPTR_POOL_BENCH.md`). A small modulation matrix over `/p` unlocks all of it.
- **Bass**: no crossover / split-band distortion path.
- **Over-built vs use**: 19-effect SoundMagic rack (4 heavy effects = one core, absent from the VST); three bincrush variants; legacy `formant` vs `forms`; `pitchshifter` (OLD PS); 16-tap poly delay at ~35-60% of a core.
- **Well matched, leave alone**: NAM rows, IR rows, looper + loop-derived clock, ATTACK swell, tremolo placed pre-delay/reverb for surf.

## 4. Proposed sequence

1. **Now (6.18):** 3.1 items 1, 2, 3, 5, 8, 10 — all small, none risky. Then measure 4, 6, 7 before changing them.
2. **Csound 7 trial — on a scratch SD / the spare board, never the live rig:** build 7.0.0 from source (`USE_PIPEWIRE`, `BUILD_PLUGINS`), port NAM opcode + host (2.3), run the re-test list (2.4), grep `-x^` precedence, confirm `OSClisten` plugin, measure: bypass latency floor (does the ALSA 1024 issue persist), rtjack with the Scarlett, `-j`, PipeWire backend vs embedded host.
3. **Only if (2) is clean:** flip the live rig, delete the rtjack patch + 6.18 workarounds that retest clean, then start the 7-only work: `oversample` on drives/NAM, `mkir` IR capture, state-map rewrite, poly-delay loop rewrite.
4. **In parallel, no Csound dependency:** tuner + global key/scale + harmonizer + gamelan tables; modulation matrix over `/p` for the body sensors.

## 5. Not determined
Per-effect live CPU on the Pi 5 for the current chain; real peak k-cycle
spikes; whether preset recall (512+768+24 table copies + dozens of `event
"i"` in one k-cycle, no `prealloc`) audibly glitches; NAM loader scheduling;
whether 6.18 on aarch64 sets FTZ; Csound 7 status of the ALSA `-B` bug,
`getenv`, `-iadc`, macro rules, `-j`; Pi 5 build time for 7; Cabbage 3 arm64.
