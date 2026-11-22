# SoundMagic rack — build contract (Phase 1)

Design: `docs/specs/2026-10-04-soundmagic-spectral-rack-design.md`. Catalog: `docs/research/soundmagic_spectral_catalog.md`.
Registry (generated, never hand-edit): `sm_params.json` / `sm_params.orc` from `tools/gen_sm_params.py` (`--check` in tests).

## Numbers
- `giSMP` ftable, 768 slots, `idx = fx*32 + p`; `giSMPmin/max/def` same layout; stamp `SM_STAMP`=1 at slot 767.
- `giSMon` ftable, 24 slots, 1 = effect on. `giSMfft` NOT a table: an effect's FFT index is its own "FFT Size" param (`giSMfftp[fx]` gives the idx, or -1 → use rack default `gkSMDefaultFFT` (index, default 2 = 2048)).
- FFT index 0..7 → 512,1024,2048,4096,8192,16384,32768,65536. hop = N/4, win = N, Hann (`pvsanal ..., 1, 1`: format 1 = amp+phase, wintype 1 = Hann). Verified on Csound 6.18: `pvsanal`(fmt 1) → `pvs2tab` → `tab2pvs`(fmt 1) → `pvsynth` round-trips.
- Toggle ids: `75 + fx` (75..98). Instrs: 241..248 = rack groups for FFT index 0..7; 249 = rack control (always on, started from score); 250 = SM persistence.
- Rack order = fx id ascending (0 AVERAGE … 18 PARTGLD). Groups chain ascending FFT size.

## UDO signatures (sm_core.orc owns the first block, kernels each in sm_<name>.orc)
```
; new-frame flag: kernels do their per-frame work ONLY when kNew == 1 (pvs2tab frame counter changed);
; otherwise they return their arrays unchanged. Arrays are length N/2+1.
kMag[], kPha[]  SM_Common    kMag[], kPha[], ifx, kNew, iN
    ; applies, in this order, this effect's: Lo bin cutoff, Hi bin cutoff (% of bins → zero),
    ; Brightness (tilt: mag *= 10^((b/100)*24dB*(k/nbins)/20), b in -100..100), Randomize phases
    ; (uniform -pi..pi per bin when param==1), Gain (dB). Params missing for an effect are skipped
    ; (read via SM_HasP). Called by the rack AFTER each kernel.
kVal            SM_P         ifx, ip                 ; = tab:k(ifx*32+ip, giSMP)  (k-rate read)
iHas            SM_HasP      ifx, iname_hash?        ; NOT used — instead sm_params.orc #defines SMP_<CODE>_<PARAM>
kMod            SM_Variance  kBase, kVarType, kVarAmt, kLfoRate, kInputAmp
    ; his "Variance type": 0 None, 1 Random (per frame uniform ±amt), 2 LFO (sine, rate Hz, ±amt),
    ; 3 Link amplitude (amt * input frame RMS norm 0..1), 4 Link inverted amp. Returns kBase + modulation.
kPk[], kN       SM_Peaks     kMag[], kFuzz, kMaxBins ; Dolson/Laroche peak picking → region start indices (phase 2)
kMag[], kPha[]  SM_<Kernel>  kMag[], kPha[], ifx, kNew, iN
    ; kernel = his core process. Wet/Dry (if the effect has one) and Feedback (if it has one) are
    ; applied INSIDE the kernel against the arrays it was given. State = local arrays `init`ed in the
    ; UDO (per call site, persists). Params read with SM_P($SM_<CODE>, $SMP_<CODE>_<PARAM>) or tab:k.
```
Kernel UDO names (from sm_params.json `udo`): SM_Averaging, SM_Blurring, SM_DroneMaker, SM_Freezing, SM_GateAndHold,
SM_Weave, SM_Shimmer, SM_Shuffle, SM_Pulsing, SM_Harmonizer, SM_BinShift, SM_PitchShift, SM_Stretch, SM_Tracing,
SM_Filterbank, SM_GlidingFilters, SM_Granulation, SM_Emergence, SM_PartialGlide.
Phase 1 kernels: SM_Freezing, SM_Blurring, SM_Averaging, SM_BinShift, SM_PitchShift, SM_Shuffle. The rack must
compile with the other 13 ABSENT: `sm_rack.orc` only references kernels whose file exists via a `#ifdef SM_HAVE_<CODE>`
set in each kernel file (`#define SM_HAVE_FREEZE ##`).

## Rack group instr (241+s), owned by sm_rack.orc
```
aL = gaSMChainL ; aR = gaSMChainR            ; chain bus (instr 1 fills from aPrev before, reads after)
fL pvsanal aL, iN, iN/4, iN, 1, 1  (and R);  kMagL[],kPhaL[] pvs2tab ...; kNew = changed:k(kframe)
for each fx in 0..18 with SM_HAVE_<CODE> and tab:k(fx, giSMon)==1 and SM_FFTidx(fx)==s:
    kMagL,kPhaL SM_<Kernel> kMagL,kPhaL,fx,kNew,iN ; kMagL,kPhaL SM_Common kMagL,kPhaL,fx,kNew,iN  (same R)
fO tab2pvs kMag,kPha, iN/4, iN, 1; aO pvsynth fO; dcblock; limit ±0.99 → gaSMChainL/R (in place)
```
Control instr 249: every k-cycle, for s in 0..7: kWant = any on effect resolves to s → `event`/`turnoff2` 241+s
(same FX_BYPASS semantics, hard off). Also `gkSMActive` = any on; signal_chain uses it to decide whether to read the
chain bus (so bypass = bit-exact dry, zero cost). Mono input, stereo processing (both channels), as his.

## OSC / control (owned by the engine agent)
- IN on 7770 (existing `OSCinit`): `/st` `ii` (fx, 0|1) → `tablew` giSMon + on toggle-ON `gkLastToggle = 75+fx`,
  `gkLastToggle29 = 75+fx`;  `/sp` `iif` (fx, p, val) → clamp to min/max, `tablew` giSMP.
- Rotations: in each `rot_*.orc` ONE branch `elseif (gkLastToggle >= 75 && gkLastToggle <= 98)` →
  `SM_RotStep(fx, heroSlot, kDir)`; hero slot per rotation CC: CC24→P1, CC25→P2, CC26→P3, CC21→P4, CC22→P5, CC20→P6,
  CC23→P7 (giSMhero[fx*8+k], -1 = no-op). Step = (max-min)/100 per detent, enums/bools step 1.
- Expression CC29: in `expr_cc29.orc` ONE range branch before the registry → write giSMpedal[fx] param absolutely
  (min + norm*(max-min)).
- OUT to UI on 8000: `/sm_state` `ii` (fx, on), `/sm_p` `iif` (fx, p, val) — ONE sender site each, driven by a
  change scan over giSMon/giSMP (and full re-send when gkOscRefresh==1). Never one socket per param.
- Persistence (instr 250): dual-file `ftsavek` of giSMP + giSMon to `$STATE_DIR/sm_params.ftab`/`.b.ftab` and
  `sm_on.ftab`/`.b.ftab` on change (2 s scan, same pattern as state_persist.orc), load at boot, stamp check → defaults.
  Preset bank: `giSMPBank` 17×768 + `giSMonBank` 17×24, copied in the SAME instrs that capture/recall presets.

## UI (owned by the UI agent) — neptrPhase4_UI.py, menu 2 custom page
- Loads `csound/spectral/sm_params.json` (path relative to the UI file). Tiles for fx 0..18 (phase ≥2 tiles drawn
  dimmed/"soon" but still tappable to open the deep sheet — params exist engine-side). Legacy 5-slot row stays.
- Sends OSC to 127.0.0.1:7770 (`/st`, `/sp`), listens `/sm_state` `/sm_p` on the existing 8000 route.
- Deep sheet: every param, generated from the json (float fader; bool toggle; enum stepper with labels).

## Rules for all agents
- Mac only. Never ssh to the Pi, never restart anything, never run audio out; `csound -n` renders only.
- Syntax check the full engine: from `csound/`: `csound --syntax-check-only --omacro:VST_NO_NAM=1 neptrPhase4.csd`
  (NOTE: it false-passes reserved-word errors like naming a var `kr`; benches must actually render).
- No git commits. Edit only your owned files. Docs you own are listed in your brief.

## Amendments after Phase 1 build (2026-10-04, measured on Csound 6.18)
- **`pvsanal` ignores its format argument.** The second `pvs2tab` array is per-bin FREQUENCY (Hz), not phase, for
  fmt 0/1/2 alike (440 Hz sine reads 439.9997). All kernels treat "kPha" as kFrq. A kernel that moves a bin by d adds
  d·sr/N to its frequency, which keeps moved partials coherent (Laroche-style locking for free). SM_Common's
  "Randomize phases" = random frequency offset ±2 bins per frame (uniform per-hop phase increment), not a phase write.
  This is the documented deviation from his literal method; by-ear on the Pi decides if it needs revisiting.
- Rack default FFT index lives at `giSMP[736]` (fx 23, p 0), set by `/sp 23 0 <idx>`; the UI RACK sheet uses it.
- Encoder/pedal taper: params with max/min > 100 (Hz-like) step on a log taper; integer ranges ≤ 8 step by 1; else
  (max-min)/100 per detent. Linear-only in the original contract text is superseded.
- `/sm_p` change scan skips slots 736..767 (rack default, generation counter, stamp) — not echoed to the UI.
- Phase-2 kernels: add `#include "spectral/sm_<name>.orc"` to `neptrPhase4.csd` by hand (no conditional include).
- Averaging is an exponential mean (a = 2/(n+1)); a 1000-frame ring at N=65536 is not storable. Shuffle at 100 %
  loses ~5.5 dB in the time domain because a permuted STFT is inconsistent under overlap-add — property, not bug.

## Amendment: hero / pedal remap over OSC (2026-10-04)
- IN 7770: `/sh` `iii` (fx, k 0..6, idx) → `giSMhero[fx*8+k] = idx`; `/shp` `ii` (fx, idx) → `giSMpedal[fx] = idx`.
  fx clamped 0..23, k clamped 0..6; idx must be -1 (none) or inside `fx*32 .. fx*32+31`, else the message is dropped.
  Drained in `midi_handler.orc` next to `/sp`. `SM_RotStep` / `SM_PedalSet` already read the tables per k-cycle, so a remap is live.
- OUT 8000: `/sm_hero` `iii` (fx, k, idx) and `/sm_pedal` `ii` (fx, idx), change-scanned in `handler_osc.orc` (shadows
  `giSMheroSh` 192 / `giSMpedalSh` 24, init -99 so the first cycle and every `gkOscRefresh` resend the whole mapping;
  hero scan 48 slots/cycle from a rotating pointer, one send per k-cycle per address, one OSCsend site each).
- UI (`sm_page.py`): per-effect `hero_slots` (7, seeded from sm_params.json, -1 empty) updated by `/sm_hero`; the focused
  strip shows the first 3 filled slots; `/sm_pedal` sets `pedal[fx]`, drawn as a small PEDAL tag over that fader when it is a hero.

> **Amendment 2026-10-05:** the rack UI moved off menu 2 onto its own page, **menu 20 NORRIS** (touch-only, no engine slot branches); menu 2 SPECTRAL is the plain five-slot menu again and HYPERGUITAR moved 20 -> 21 (`$HG_MENU`). OSC protocol unchanged.

## Web mirror (web agent, 2026-10-04) — engine side DONE 2026-10-05: all four /sm_* sends mirrored to 8005
The web UI page `#/neptr/spectral` (linked from the NORRIS menu 20 page) (web/demiurge_web.py, class SmRack) shows live rack values. 8000 is owned by the
pygame UI, so the web backend listens on its own UDP port **127.0.0.1:8005** (`DEMIURGE_SM_MIRROR_PORT`). NOTE: the
brief said 8004, but 8004 is the video engine's port (`VIDEO_OSC_PORT`, `/nv/*`, `/sensor` fan-out); 8005 is free
(hyperguitar spec reserves "8004/8005 extra telemetry").
- The engine must ALSO send, to 127.0.0.1:8005, exactly the messages it sends the UI on 8000 from the same change
  scan: `/sm_state` `ii` (fx, on) and `/sm_p` `iif` (fx, p, val), including the full re-send on `gkOscRefresh`.
  One extra `OSCsend` per scan site (same message, second port); never a socket per param.
- Optional, same port: `/sm_hero` `iii` and `/sm_pedal` `ii` are accepted and ignored for now.
- The web side SENDS (to 7770): `/st ii`, `/sp iif`, plus hero curation `/sh iii` (fx, k 0..6, absolute param idx or
  -1 = empty slot) and `/shp ii` (fx, absolute param idx). The engine must accept `/sh` `/shp` and write
  `giSMhero[fx*8+k]` / `giSMpedal[fx]` live (and echo `/sm_hero` `/sm_pedal` as above).
- Until the mirror is heard the page shows the last value it sent, italic, marked "unconfirmed".
- Hero/pedal picks are curated in `sm_heroes.json` (names per effect code; slot = list position, null = empty).
  `tools/gen_sm_params.py` reads it when present (OVERLAY is only the first-run seed); registry `heroes` may now
  contain -1 for an empty slot between filled ones.
