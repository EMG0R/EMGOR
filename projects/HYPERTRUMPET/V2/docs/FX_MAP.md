# HYPERTRUMPET V2: FX map (v1 audit + v2 control mapping)

Status 2026-11-15. Sources audited read-only: `hyperTrumpet/PERFRMNC.maxpat` (parsed with node, all 35 subpatchers, cords and inlet indices checked), `midiinHyperTrump.maxpat`, `hyperTrumpCode/hyperTrumpCode.ino`, Max 9 refpages (counter, line, line~, random, transport, delay, scale). Prior scrape `V1_SCRAPE.md` was used as a map, not trusted; where the real patch disagrees, this file wins (section 4).
Implementation of section 6 (the v2 mapping): `HYPER_CORE/src/dsp/fx/mapping/fx_mapping.cpp`, test `tests/test_mapping.cpp`. Section 6 and the code are 1:1; if one changes, change both.
Marks: **UNCERTAIN** = could not be confirmed from the files (needs a run in Max or the closed package). **NEW** = v2 design, not in v1.

---

## 1. v1 effects (what, algorithm, params, chain order)

Chain is strictly serial at top level, all audio is hard-wired cords (no send~/receive~, no feedback loops in the Max graph). Verified cord by cord:

`adc~ ch1+ch2 -> *~ -0.3 (L bus) / *~ -0.3 (R bus) (identical, dual mono) -> [1] shiftr -> [2] looper -> [3] freeze -> [4] drone -> [5] blur -> [6] flanger -> [7] chorus -> [8] MOOD -> [9] superMassive -> [10] mcCurdyGrain -> [11] absDelay -> [12] roar -> [13] ringmod -> dac~ 1,2`.

| # | Block | What it is | Algorithm | Params that matter | Mix topology |
|---|---|---|---|---|---|
| 1 | p shiftr | Dual-voice harmonizer | 2 voices x 2 channels of `pfft~ gizmo_loadme 1024 4` (Max gizmo~ FFT pitch shifter, hop 4), ratio 2^(st/12) | ladder index 0..13 (init 5 = bypass), voice gains | dry + A + B in parallel, gains by state (instant, no ramp) |
| 2 | rnbo~ looper | 12-layer stereo looper | `record~`/`groove~` per layer, 80 s each, layer counter; params transp -12..12 st, reverse, volume -18..+18 dB | tap/hold gestures | dry*(1 - layers/12) + loop sum |
| 3 | p freeze | Spectral Freezing (Michael Norris AU) | closed AU; input gate with 250 ms line~ | Freeze factor attr, `freezing` mix (127), `amount` | dry (unity, additive) + 0.65*AU*mix |
| 4 | p drone | Spectral DroneMaker (Norris AU) | closed AU, algorithm unknown | mix only | dry*(1-m) + AU*m, linear, unsmoothed |
| 5 | p blur | Spectral Blurring (Norris AU) | closed AU | mix only | dry*(1-m) + AU*m |
| 6 | p flanger | McCurdy Flanger.csd (Cabbage AU) | Csound `flanger` opcode, LFO, thru-zero | rate 0.08 (->0.257 Hz), depth 0.78 (->7.8 ms), fback 0.95 (->0.9), mix | AU-internal mix, no external dry |
| 7 | p chorus | `abl.dsp.chorus~` (Ableton) | closed | gain +3 dB, mod 0.3, rate 0.9 Hz, mix 0/1 | internal mix attr |
| 8 | p MOOD | `supervp.trans~` (IRCAM SuperVP) random pitch ladder | phase vocoder, `@transients on @shapeinv on @envtrans on`, `stereopres on` | ladder -24,-17,-12,-9,0,+7,+12,+19,+24 st; probability; auto toggle | dry*(1-w) + shifted*w, w = line~ 100 ms |
| 9 | p superMassive | 3x ValhallaSupermassive | closed reverb; 3 instances, Mode 0.0417/0.2083/0.8333, DelayNote 0.327/0.301/0.258, Feedback 0.709, DelayWarp 0.709 | `supa` mix, `warp`, `feedback` | dry*(1-m) + 0.43*(V1 L+R, V2, V3 L+R) * m, linear |
| 10 | p mcCurdyGrain | McCurdy DelayGrain.csd (Cabbage AU) + Norris octave-up AU | granular delay 1048576-sample buffer, per-grain random size/delay/pan/amp/filter | GSize 0.0148..0.0848 s, Dens 500/s, Dly2 1.01 s, PanSpread 1, AmpSpread 0.8, reverse 0.7 | stage A: dry vs octave AU (autooct) feeds the grain AU; stage B: dry (original input) vs grain wet by `grains` mix |
| 11 | p absDelay | `abl.device.delay~` + `tanh~` | stereo delay, ping-pong on (set at load), tanh outside the feedback loop, wet*0.62 | time 0..0.6 s, feedback, mix, chance | dry*(1-m) + wet*m (no ramp, see corrections) |
| 12 | p roar | `abl.device.roar~` | saturator; only `shaper_amount_1` driven | amount 0..1, output gain 0.3..1 | no external dry |
| 13 | p ringmod | `abl.dsp.ringmod~` | ring modulator | frequency 0..800 Hz, mix 0/1 | internal mix |

Orphans in the file, not in the audio path: `p autopan`, `p lil rando`, `abl.device.redux~` inside MOOD, `p butt tog` for pad 2.

### Per-effect notes (verified against the patch)

**[1] Harmonizer.** One integer index 0..13 (loadmess 5). enc1 CW = +1 (clamped at 13), CCW = -1 (clamped at 0), enc1 click = back to 5; enc2 inlets exist but are unwired. Table (A, B semitones): 0:(-12) 1:(-12,-7) 2:(-7) 3:(-12,-5) 4:(-5) 5:bypass 6:(-12,+5) 7:(+5) 8:(+12,+5) 9:(-12,+7) 10:(+7) 11:(+12,+7) 12:(+12) 13: no handler (keeps 12). Gains: neutral dry 1, voices 0; single voice A 0.6 and dry untouched (so it inherits 1 or 0.5 from the previous state); two voices 0.5 each and dry 0.5. Voice A ratio comes from one `transratio`, voice B from another; changes are instant.

**[2] Looper.** Layer counter drives dry level `1 - n/12`. Pad 13 (in3) and pad 14 (in4) go through `metro 50` + `counter` + `> 10` comparators to `LOOPrecIN/LOOPdeleteALLin` and `LOOPplayIN/LOOPstopIN`. Tap = rec/overdub (13) and play (14); hold > ~0.55 s (counter > 10 at 50 ms) = delete all (13) / stop (14). Structure verified, exact tap/hold semantics are inferred: **UNCERTAIN** until run.

**[3] Freeze.** Wet is `0.65 * AU`, dry stays at unity (additive, the `(1-mix)` multipliers have no audio input). The AU input is gated by `1 - block` through a real 250 ms `line~` (a `message 250` feeds the right inlet before each target). `block` toggle = pad 4 is **held = block 0 = new audio flows in**; released = block 1 = input closed, the AU holds. Pad 3 (`p clearer`): Freeze factor -> 0.01 over 2.3 s, then 1.1 s later back to `amount` (1.0) over 2.3 s. Both the press and the release edge run this cycle. L and R are swapped inside (inlet 1 -> dry R / AU R); harmless because the chain is dual mono.

**[4]/[5] Drone/Blur.** Mix only. Drone mix = slider 1. Blur mix = pad 7 latch, 0 <-> 60/127 = 0.472. Both are linear crossfades with the float written straight into a `*~` signal inlet (no smoothing).

**[6] Flanger.** Csound flanger opcode per channel: `adlt = kdelay + LFO*depth`, `flanger ain, adlt, fback, 1.2`, DC block, outer mix ThruZero: `ntrpol(-dry(20 us), wet, mix)`. Rate/depth/fback set by `loadmess` at load (0.08/0.78/0.95), only `mix` live: lidar, inverted.

**[7] Chorus.** Mix attribute only, pad 8 latch toggles 0/1 (100 % wet).

**[8] MOOD.** The `metro 16n` (125 ms at the default 120 bpm, **UNCERTAIN**: tempo is not stored in the patch) rolls `random 0. 1.` each tick; wet w = 1 if roll <= mood probability else 0, ramped 100 ms by `line~` (a `message 100` fires into the right inlet first, verified by connection order). Mood probability = joystick Y (0..1, 0.5 at centre), default 0 (always dry). Pitch ladder index = `scale 8 120 0 8` of CC30 (= 127 - raw reading, so stick up = high CC), index 0 for CC <= 21 (cells 14 wide), index 8 for CC >= 120. The index is only updated when joystick X sends a new value; the metro does not re-sample it. With `auto` toggle on (UI only, no CC drives it) the index is `random 9` per tick, passed only on ticks where the roll passes the mood probability.

**[9] Reverb.** Three Supermassive instances at 100 % internal mix; outputs combined: wetL = 0.43*(V1.L+V1.R) + 0.43*V2.L, wetR = 0.43*V2.R + 0.43*(V3.L+V3.R); then `verbMIX` dry*(1-m) + wet*m. Only `Mix` (slider 3) and `DelayWarp` (tilt) move live; `Feedback` is set by loadmess (0.709) and has no controller.

**[10] Grain delay + octave.** Stage A mixes the Norris Pitch Shift AU (+1200 cents, saved state) into the feed of the grain AU with weight w from `autooct`: every `metro 8n` (250 ms at 120 bpm) roll `random 0. 1.`; w = 1 only if roll <= 0.06, else 0; 100 ms `line~` ramp. So ~6 % of eighth notes the grain engine sees an octave-up signal. Stage B crossfades the **original input** against the grain output by slider 2. The grain AU uses its own internal level 1 / mix 1.

**[11] Delay.** Manual time = `0.6 * sqrt(1 - t)` s where t is the `time` dial normalised; with the FSR path (`time` dial = 127 - FSR) that is `0.6*sqrt(FSR/127)` s. Stochastic mode (pad 9 **held** enables; starts enabled at load until the first pad 9 release): `metro 16n`, roll `random 1.`; if `chance` (dial, default 1.0, no controller) >= roll then delay `mix` attr = 1 and a new random time `0.6*sqrt(r/100)` s (`random 100.`, L = R), else `mix` attr = 0. Pad 11 latch: `delau` ramps 0 <-> 127 in 700 ms (`line`), feedback dial = 100/127 = 0.787. Pad 12 latch: `delau` ramps 0 <-> 40/127 = 0.315, feedback 30/127 = 0.236, `time` dial set to 67/127, so delay = 0.6*sqrt(1 - 0.528) = 0.412 s; the FSR drives time only while pad 12's ramp output is exactly 0. At load `loadmess 100` runs a 700 ms ramp of the feedback dial to 127/127.

**[12] Roar.** Pot 1 -> `shaper_amount_1` a (0..1) and output gain `scale~ 0 1 0.3 1 2` of `(1 - a)`: gain = 0.3 + 0.7*(1-a)^2.

**[13] Ringmod.** FSR 0..127 -> `scale 0 127 0 800` Hz -> `line` with a 200 ms ramp time (the time message fires first by connection order, verified); mix attr 0/1 by pad 5 **held**.

---

## 2. v1 control map (control -> CC -> Max receiver -> effect param -> formula -> smoothing)

Firmware (Teensy 4.1, MIDI ch 3, ignored by the receiver): analog block 50 Hz, each value = mean of 4 `analogRead` (10 bit) mapped to 0..127, then for sliders/pots `map(2..125 -> 0..127)` clamped; a CC is only sent when it differs from the last sent by >= 2. No other smoothing in firmware. Max side: the dial/slider just forwards, no `slide`/`line` on any continuous controller path, except those listed.

| Control | CC | Max receiver | Lands on | Mapping formula | Smoothing | Note |
|---|---|---|---|---|---|---|
| Slider 1 | 20 | `r trSlid1` -> dial | [4] drone mix | v/127 linear | none (float into `*~`) | |
| Slider 2 | 21 | `r trSlid2` -> dial | [10] stage-B mix (grain wet) | v/127 linear | none | |
| Slider 3 | 22 | `r trSlid3` -> dial `supa` | [9] reverb mix | v/127 linear | none | also INIT sweep |
| Lidar | 23 | `r trLidar` -> `127 - x` -> dial | [6] flanger `mix` | (127 - CC)/127; near hand = 0, far = 1 | firmware 8-sample moving average at 30 Hz; none in Max | clamp 56..333 mm, CC = map(56..333 -> 127..0) |
| FSR | 24 | `r trFSR` | [13] ring freq; [11] delay time | freq = (CC/127)*800 Hz, 200 ms glide; time = 0.6*sqrt(CC/127) s | glide on ring only | firmware range 2..50 so a light touch saturates |
| Pot 1 | 28 | `r trPot1` -> dial | [12] roar amount | a = CC/127; gain 0.3 + 0.7(1-a)^2 | none | |
| Pot 2 | 29 | `r trPot2` | none (receiver has no outlet use) | | | unmapped |
| Joystick X | 30 | `r trJoyX` -> `127 - x` -> dial `pitch` -> MOOD (`127 - x`) | [8] ladder index | idx = scale(8..120 -> 0..8)(CC30) | none; gated by joystick latch | firmware sends 127 - reading |
| Joystick Y | 31 | `r trJoyY` -> dial `mood` | [8] wet probability | CC/127 | none; gated by latch; wet ramp 100 ms | |
| Joy click | 32 | `r trJoyButt` | joystick latch | counter -> /2 == 1 | | phase **UNCERTAIN** (counter start) |
| Valves 1-3 | 33-35 | all -> `s trMag1` | **nothing** | | | digital Hall switches (INPUT_PULLUP), no receiver |
| Enc 1 | 36 | `trEnc1Neg/Pos` | [1] ladder -1 / +1 | 127 = CW, 0 = CCW, one msg per detent | none | |
| Enc 2 | 37 | `trEnc2Neg/Pos` -> shiftr inlets 6/7 | **nothing** (unwired) | | | |
| Enc 1 click | 38 | `r trEnc1Butt` | [1] reset to idx 5 | | | |
| Enc 2 click | 39 | `r trEnc2Butt` | nothing | | | |
| Arcade | 40 | none | nothing | | | not in midiin patch |
| Accel X | 41 | `r trAcel` | [9] `DelayWarp` | warp = (-15 + 157*CC/127) clipped to 0..127, /127; centre 64 -> 0.505 | none | slope 1.236 |
| Accel Y/Z, gyro XYZ | 42-46 | monitor only | nothing | | | |
| Breath (BME680) | 47 | -> `s trMag1` | nothing | | | |
| Contact mics | 48,49 | not mapped | nothing | | | |
| Pads 1-16 | 50-65 | `trButt1..16`, 1 on press and 0 on release | see section 3 | | | each press = 2 events |

Pad table (verified): 1 unused, 2 nothing (output unconnected), 3 freeze clear, 4 freeze hold-open, 5 ring mix hold, 6 unused, 7 blur latch, 8 chorus latch, 9 stochastic delay hold, 10 delay smoothmode latch, 11 delay-on (full, fb 0.79), 12 delay-on (0.31, fb 0.24, fixed time), 13 looper rec/overdub (hold = delete all), 14 looper play (hold = stop), 15 unused, 16 unused. Latches are `counter 3` toggles driven by both edges, so each full press flips them; whether the first press arms or the first release arms depends on the counter's first output: **UNCERTAIN**.

---

## 3. What the v1 sensors did musically

It is a set-and-sculpt rig: everything processes the live mic continuously; there is no note-level triggering.
- **Sliders 1-3**: wet levels of drone, grain delay, big reverb: the main "space" sculpting.
- **Pot 1**: roar saturation. **FSR**: pressure = ring-mod frequency and delay time (a "throw" gesture). **Lidar**: hand over the bell = flanger depth (far = more). **Tilt X**: reverb warp (size/motion). **Joystick** (after a click): pitch ladder + probability of a random shifted voice (the "mood" gesture). **Enc 1**: manual harmony interval ladder.
- **Pads**: freeze release/hold, ring and stochastic delay as momentary holds, blur/chorus/delay presets as latches, looper rec/play.
- **Transmitted but unused**: valves 1-3, breath (BME680, abs pressure delta, 0.02 hPa deadband, full scale 0.32 hPa), both contact mics (peak of 10 reads, 0..400 -> 0..127), IMU Y/Z and gyro XYZ, enc 2 turn and click, pot 2, arcade button, pads 1, 2, 6, 15, 16.
- Valves in v1 are digital switches. v2 has analog Hall depth, which is a new expressive axis (section 6.6).

---

## 4. Audit corrections to V1_SCRAPE.md

1. **`line~ 250` is not a 250 ms ramp.** In `line~`, the argument is the *initial value*. Every `line~ 250` in `absDelay/delayMIX` has no time input, so the delay mix gain jumps instantly (and sits at 250 before the first float arrives, covered by INIT/loadmess; audible at load: **UNCERTAIN**). The scrape said "250 ms mix ramp". Real ramps exist only where a `message N` feeds the right inlet: freeze input gate (250 ms), MOOD wet and autooct (100 ms), ringmod freq `line` (200 ms), pad `line` ramps (700 ms). Connection order checked by patching-rect x: the time message fires first in all of them.
2. **Delay `pingpong`** is set to 1 at load (loadmess 1 -> attr). The scrape said "not touched".
3. **btn10 / `smoothmode` polarity**: toggle on -> `smoothmode 0`, toggle off -> `smoothmode 1` (inverted vs the scrape). Also `loadbang -> delay 1000 -> toggle 0` (smoothmode 1 at 1 s) and an extra forced `smoothmode 1` at 10.02 s that overrides anything set in the first 10 s. What `smoothmode` does inside `abl.device.delay~`: **UNCERTAIN** (package not found on this Mac).
4. **MOOD ladder**: the scrape labelled the index source "(127 - pitch dial) = joyX raw". The signal path is raw reading -> firmware `127 - x` (CC30) -> Max `127 - x` -> dial -> MOOD `127 - x` -> CC30 again -> `scale 8 120 0 8`. Net: index from CC30, stick up = high. `scale` with all-int arguments is assumed to output ints (truncating): cells 14 CC-units wide, idx 0 for CC <= 21, idx 8 for CC >= 120 (**UNCERTAIN**: int vs float output of `scale`). The index updates only on joystick X events, not on the metro (scrape implied per-16th).
5. **Freeze clear** runs on both press and release edges (toggle passes 0 and `del` re-triggers on int 0/1); scrape said "on turn-on".
6. **Delay pad 12 time**: `time` dial 67/127 gives 0.412 s (not stated in the scrape). Pad 11 feedback 0.787.
7. Looper tap/hold: threshold is `counter > 10` at `metro 50` (~0.55 s); logic structure verified, semantic stays **UNCERTAIN**.
8. Confirmed as the scrape said (spot-checked): chain order and cords, all CC -> send mappings in the midiin patch (valves + breath all `trMag1`), harmonizer table incl. idx 6 (-12,+5), grain stage A/B topology, freeze additive dry and L/R swap, 0.43 reverb combination, autooct 6 %, shaper gain curve, `.ino` scaling (4x average then 2..125 map, FSR 2..50, joystick X flipped, lidar 56..333 mm 8-sample average, mic peaks, IMU +-8000 map, breath formula, enc 127/0 per detent).

---

## 5. Credit and licensing

- **Iain McCurdy**: Flanger.csd (2012) and DelayGrain.csd (2013), CC BY-NC-SA 4.0. The v2 flanger and grain delay are clean C++ ports of the algorithm (not copied code), with this credit in the file headers and in the instrument's credits; the share-alike/non-commercial terms apply to anything derived from the Csound source.
- **Michael Norris Spectral AUs** (Freezing, Blurring, DroneMaker, Pitch Shift), **Valhalla Supermassive**, **IRCAM SuperVP**, **Ableton `abl.*`** (chorus, delay, roar, ringmod): closed/proprietary. v2 re-derives the behaviour (STFT hold/smear, FDN reverb, PSOLA/delay-line shifting, standard chorus/delay/saturator/ring mod); no code, presets or binaries copied. Names appear only as "inspired by" in docs.
- Max-bundled `gizmo~` example (harmonizer) is re-derived as a delay-line shifter.

---

## 6. v2 mapping

Hardware (IO_LIST.md): 3 analog Hall valves, 3 faders, 2 pots, FSR, joystick X/Y + click, 2 encoders with push, MPU6050, VL53L0X, NeoTrellis 4x4 RGB, bell mic mono in, stereo out. Principle: keep v1's intent and best gestures; continuous *expression* moves to playing-driven sources (valve depth, breath envelope `ctx.env`, FSR, tilt, lidar); set-and-sculpt macros stay on faders/pots; the Trellis is a performance layout.

### 6.1 Engine model

4 serial slots S1..S4 in `FxEngine`. Any effect from the registry can sit in any slot (`setEffect`). Per slot: effect type, bypass (15 ms crossfade, a fully bypassed slot costs no CPU), `MIX` (param 0, always present). Controls address **roles**, not effects: `R_AMT` intensity/wet, `R_TIME` time/size/rate, `R_TONE` brightness/damping/filter, `R_MOD` motion/warp, `R_FB` feedback/decay, `R_PITCH` interval/frequency (an effect declares which param each role moves; a missing role is skipped). Actions: `A_MAIN`, `A_ALT`, `A_CLEAR`, `A_HOLD`, effect-specific (see FX_STATUS.md / `status/*.json`): harm MAIN reset unison, ALT next interval; grain MAIN next window, ALT next distribution; delay MAIN held stochastic gate, ALT smooth/glide; freeze MAIN held gate (let audio in), ALT latch freeze, CLEAR release; looper MAIN tap rec/overdub, ALT play/stop, CLEAR delete all, HOLD stop; mood MAIN wet on, ALT re-roll, CLEAR wet off; ring HOLD held gate; chorus/flange/delay/grain CLEAR clear buffers.

**Slots have a function so controls keep a stable meaning when effects change:** S1 PITCH (harm, mood), S2 TEXTURE (grain, freeze, looper), S3 SPACE (reverb, blur, roar), S4 EXPRESSION (delay, flange, ring, wah).

### 6.2 Default patch (bank A "CORE") and why

| Slot | Effect | v1 origin | Why |
|---|---|---|---|
| S1 | harm | [1] shiftr, enc1 | the most central gesture; delay-line shifter ~3 % of the M7 with two voices |
| S2 | grain | [10] grain delay, slider 2 | signature texture; worst case ~24 voices, the heaviest default effect (budget it first) |
| S3 | reverb | [9] Supermassive, slider 3 + tilt | space; 8-line FDN ~6.6 % (4-line lite ~4 %) |
| S4 | delay | [11] absDelay, FSR + 4 pads | echo throw under finger pressure; ~1.5 %, leaves headroom for flange/ring/wah in other banks |

Estimated total in the default patch is well under half of the Seed's M7 budget (see FX_STATUS.md after `make bench`). Freeze, flanger, ring, MOOD, looper are central in v1 but need either hold gestures or are less constantly used, so they live in banks B/C.

### 6.3 Banks (4 patches of 4, loaded from the BANK page, A..D)

| Bank | S1 | S2 | S3 | S4 | Intent |
|---|---|---|---|---|---|
| A CORE (default) | harm | grain | reverb | delay | v1's core sound |
| B SPEC | mood | freeze | blur | flange | spectral / v1 MOOD + freeze + lidar-flange |
| C LOOP | harm | looper | reverb | ring | looping with harmony, ring throw |
| D GRIT | harm | grain | roar | wah | dirty + hand-wah (lidar) |

Any other effect (chorus, drone once implemented, etc.) is reachable per slot by encoder browse or the BANK-page prev/next pads. Loading a bank or an effect calls `FxEngine::setEffect` (allocates in the slot arena): **the firmware must call `HxMapper::update()` from the main loop, never the audio ISR**; `setEffect` only runs on explicit actions (enc1 hold+turn+release, bank pads, prev/next effect pads, 1 s reset hold, `loadDefaultPatch()`); a swapping slot passes audio through.

### 6.4 Continuous controls: role per control, ranges, smoothing

All smoothing is a one-pole per control, `alpha = 1 - exp(-dt/tau)`, so behaviour is independent of the update rate (dt clamped to 0.2..50 ms). The Seed's own rates: Halls ~750 Hz, faders/pots 100 Hz, FSR 750 Hz, joystick 750 Hz, IMU 100 Hz, lidar 33 Hz; the mapper is called at 200 Hz to 1 kHz. A target is written to the engine only if it moved by >= 0.003 (normalised) or its slot's effect changed, so a value edited with enc2 stays until the physical control is touched again.

| Control | Source rate | Target | Formula (n = role value 0..1 -> param via `setRoleNorm`) | Smoothing tau | Note |
|---|---|---|---|---|---|
| Fader 1 | 100 Hz | S1 MIX | n = fader | 40 ms | harmony blend (v1: slider 1 was drone wet) |
| Fader 2 | 100 Hz | S2 MIX | n = fader | 40 ms | grain wet (v1 slider 2) |
| Fader 3 | 100 Hz | S3 MIX | n = fader | 40 ms | reverb wet (v1 slider 3) |
| Pot 1 | 100 Hz | S4 R_TIME | n = pot | 40 ms | delay time / flange rate / wah range (v1 time dial) |
| Pot 2 | 100 Hz | breath depth for S4 | breath = pot2 * sqrt(env) | 40 ms | NEW: 0 = off; 1 = breath alone can open S4 fully |
| FSR | 750 Hz | S4 R_AMT | amt = max(fsrC, breath), fsrC = (fsr - 0.03)/0.97 | up 15 ms / down 120 ms | pressure throw, v1 FSR; slow release lets echoes bloom out |
| Breath `ctx.env` | block | via pot 2 only | env smoothed | 30 ms | NEW: replaces v1 breath (BME680, unused) with the mic envelope |
| Lidar (33 Hz) | 33 Hz | S4 R_TONE | n = 1 - lidar (hand close = dark/low, far = open) | 90 ms | hand-mute / hand-wah / flange offset; v1 lidar -> flanger |
| Tilt X (roll) | 100 Hz | S3 R_MOD | n = 0.5 + 0.5*1.236*tiltX (clamped) | 120 ms | v1 accel X -> reverb warp, same slope |
| Tilt Y (bell up/down) | 100 Hz | S2 R_MOD | n = 0.5 + 0.5*1.236*tiltY | 120 ms | NEW: grain delay offset / freeze smear |
| Shake (gyro mag) | 100 Hz | S2 action | shake > 0.85 for 80 ms, 2 s refractory: freeze in S2 -> A_ALT (grab/latch), else A_CLEAR | | NEW |
| Joystick X (latched) | 750 Hz | S1 R_PITCH | x<0: n = c*(1+x); x>0: n = c + (1-c)*x; c = unison centre (harm 5/13, mood 4/8, else 0.5) | 25 ms | v1 X = ladder; centre = unison |
| Joystick Y (latched) | 750 Hz | S1 R_AMT | n = 0.5 + 0.5*y | 25 ms | v1 Y = mood probability (harm: voices) |
| Joystick click | edge | engage latch | toggle | | v1 latch; first click enables in v2; engaging forces a rewrite |
| Valve 1-3 depth | 750 Hz | pitch hint + half-valve | see 6.6 | 4 ms | NEW |
| Enc 1 / Enc 2 | 750 Hz poll | see 6.5 | | | |

Deadzones: joystick deadzone is the HAL's; FSR 0.03 here; tilt has none (centre = 0.5); fader/pot change threshold 0.003.

### 6.5 Encoders

- **Enc 1 turn**: select focus slot S1..S4 (wraps). **Hold + turn**: browse effects for the focus slot (staged, status line shows the candidate); **release commits** (`setEffect`). **Short click** (no turn): toggle focus-slot bypass.
- **Enc 2 turn**: edit the focus slot's selected parameter (quantised/enum params one step per detent; continuous: range/64, or `step` if larger; log params multiplicative). **Hold + turn**: choose which parameter. **Short click** (no turn): reset that parameter to its value when the effect was created.
- Selected parameter defaults to the slot's R_AMT param whenever the focus slot or its effect changes. Status line (`statusLine()`) shows the latest action.

### 6.6 Valves (NEW design, v1 valves did nothing)

1. **Pitch hint.** Pressed = depth > 0.55 (release below 0.40, hysteresis). Combination drop in semitones = 2*V1 + 1*V2 + 3*V3 (standard trumpet). Open-tube fundamental 116.54 Hz (Bb pedal) lowered by that drop; the partial nearest the tracked `f0` (partials 2..12) is sent as `FxEngine::setValveHint(hz)` (0 after 150 ms of no pitch). It only stabilises the tracker/harmonizer; it does not change the sound by itself.
2. **Half-valve smear.** h = max over valves of smoothstep(0.12, 0.35, min(d, 1-d)), then asymmetric smoothing (attack 70 ms, release 100 ms) so fast strokes do not count and a dwell mid-stroke does. Target: S1 R_MOD = 0.8*h (harm: detune up to ~40 cents; mood: auto probability). A half-valve smear thus gives a gentle detune of the harmony.
3. Not mapped yet (open): per-valve sends of slot k+1; valve combination -> automatic harmony interval.

### 6.7 Trellis: PLAY page (default) and BANK page

Pad index n = row*4 + col (row 0 top). **Columns are slots S1..S4.** Colours: S1 violet (130,40,255), S2 magenta-violet (225,40,200), S3 indigo (60,70,255), S4 lilac (175,125,255); accent white-violet (215,190,255) (all before the firmware's brightness cap).

PLAY page:
```
        S1            S2            S3            S4
row 0   [ 0 BYP  ]    [ 1 BYP  ]    [ 2 BYP  ]    [ 3 BYP  ]    latch: toggle slot bypass
row 1   [ 4 MAIN ]    [ 5 MAIN ]    [ 6 MAIN ]    [ 7 MAIN ]    trigger, or HOLD for delay/freeze (A_MAIN 1/0) and ring (A_HOLD 1/0)
row 2   [ 8 ALT  ]    [ 9 ALT  ]    [10 ALT  ]    [11 ALT  ]    trigger A_ALT
row 3   [12 BANK ]    [13 CLEAR]    [14 CLEAR]    [15 CLEAR]    CLEAR trigger S2..S4; pad 12 HELD = BANK page
```
Hold vs latch: bypass pads latch; MAIN pads are *holds* for effects whose action is a gate (press sends 1, release sends 0) and one-shot triggers otherwise (release sends nothing, so grain's "next window" fires once per press); ALT and CLEAR always trigger on the press edge. Hold releases are delivered even if the page changed. S1 CLEAR is not mapped (harm CLEAR = MAIN; mood CLEAR = bypass).

BANK page (while pad 12 is held; releasing returns to PLAY):
```
row 0   [ A ]  [ B ]  [ C ]  [ D ]        load bank (setEffect x4, all bypasses cleared)
row 1   [prev fx][next fx][prev param][next param]   focus slot effect (commits immediately) / selected param
row 2   [ S1 ] [ S2 ] [ S3 ] [ S4 ]       select focus slot
row 3   [held ][ DRY ][ STICK][ RESET ]   DRY = bypass all / restore; STICK = joystick engage; RESET = hold 1 s = default patch
```

LED rules (violet, bold minimal, slow breathing +-7 % over 7 s, flash decay 6 /s):
- PLAY row 0: slot colour; bypassed = 6 %; active = 30 % + 70 % x slot level (S1..S3 fader, S4 its AMT); +12 % on the focus slot; flash on press. Empty slot: off.
- Row 1: 16 % idle, 100 % while held (shows the hold), flash on press. Row 2: 10 % idle, 90 % flash. Row 3 CLEAR: 5 % idle, 90 % flash. Pad 12: accent colour at 30 % (+25 % while the stick is engaged).
- BANK page: row 0 slot colours, current bank 100 % others 14 %; row 1 focus-slot colour 45 % (prev/next effect) and accent 20 % (param pads); row 2 focus slot 100 % others 14 %; row 3: pad 12 100 %, DRY 100 % while dry, STICK 100 % while engaged, RESET fills 10..100 % while held.
- LED frame is refreshed every `update()`; the firmware sends it at ~12.5 Hz with its own cap (~0.45).

### 6.8 What is not mapped yet / open questions

| Item | Status | Recommendation |
|---|---|---|
| S3 SIZE / DECAY / S1 TIME, TONE, S2 TIME, TONE | no physical control, enc2 only | add a SHIFT layer on the pots (hold enc2 click) only if it is missed in rehearsal |
| Looper v1 hold gestures (hold rec = delete all, hold play = stop) | replaced by CLEAR pad and ALT toggle | keep unless a gloved play style needs them |
| Mic envelope as breath | uses `ctx.env`, not the BME680 | drop the BME680 for v2 as the IO list does |
| Contact mics, arcade | no v2 hardware | none |
| Lidar polarity per effect | TONE polarity differs by effect (reverb DAMP is a cutoff, higher = brighter) | verify by ear on the bench, flip per effect id if needed |
| Valve -> per-valve sends, valve -> harmony | open | wait for hands-on with the Hall sensors |
| Pitch hint consumers | the engine stores `valvePitchHz`; effects do not yet use it | consume it in the pitch tracker as a prior |
| Drone effect | registry stub (algorithm unknown) | derive by ear from DroneMaker, put it in bank B S2 when ready |
| Freeze trigger | A_MAIN held gate + shake latch, no auto-freeze on breath release | consider "freeze on note off" later |
| Joystick on spring return | centre = unison; X/Y axes per the HAL | confirm physical axis orientation |
| Bank change glitch | `setEffect` clears the slot (tails die) | acceptable; only on explicit action |

### 6.9 Uncertain items (audit)

Counter first-output phase (all latch pads, joystick latch); transport tempo (assumed 120 bpm for `metro 16n/8n`); `abl.*` attribute semantics and ranges (smoothmode, delay feedback max 0.95 vs a normalised 1.0 from the fb dial); Norris AU parameter meaning; looper tap/hold; `scale` int truncation in MOOD; physical sensor mounting (which hand has which sensor) is not in the files.

## 7. v1 object -> our file -> status (audit of PERFRMNC.maxpat, 2026-11-17)
Traced with a script over the subpatchers (`p autopan`, `p lil rando`, `p MOOD` + `p automood` + `p pshiftMIX`, `p mcCurdyGrain/p autooct`, `p shiftr`, `p superMassive`). Details of the findings under the table.

| v1 object | Our file | Status |
|---|---|---|
| `p shiftr` (pfft~ gizmo x2 voices, ladder idx 0..13, init 5) | `fx_harmonizer.h` (+ glue) | tables match the verified ladder (section 6 correction 8). As of 1.12 the encoder no longer steps it: it is web-toggled only; the harmony page (`fx_harmony.h`, 3 shifted voices, user-specified interval grid, not a v1 object) replaces the encoder use |
| `rnbo~ looper` | `fx_looper.h` | see `docs/LOOPER.md` |
| Freeze AU (`p freeze`, `p clearer` 2300 ms ramp) | `fx_freeze.h` | owned by the FFT work; pad 3 ramp and pad 4 let-in in `fx_controls.h` / `fx_pads.h` |
| DroneMaker AU (`p drone`) | `fx_drone.h` | other owner |
| Blur AU (`p blur`) | `fx_blur.h` | other owner |
| `p flanger` (McCurdy Csound) | `fx_flanger.h` | lidar mix in `fx_controls.h` |
| `p autopan` | `fx_autopan.h` + `fx_glue_autopan.h` | v1 audio inlets/outlets unconnected (never audible); law implemented exactly (below); pad 15 latch, depth/period locked to the lidar |
| `p lil rando` | `fx_lilrando.h` (control source, not an effect) | v1: no inlets, no outlets, no cords to it: drives nothing. Same here, nothing reads it |
| `abl.dsp.chorus~` | `fx_chorus.h` | matches |
| `p MOOD`: `supervp.trans~` + `p automood` + `p pshiftMIX` | `fx_mood.h` | matches (ladder -24,-17,-12,-9,0,7,12,19,24 st, `metro 16n` roll vs probability, 100 ms wet line~); joystick X = ladder index, Y = probability |
| `abl.device.redux~` inside MOOD | `fx_mood.h` BITS param | v1: the redux outputs go to two `*~ 0.6` objects whose outputs connect to nothing, so it is inaudible. Implemented as BITS (4..16, default 16 = off) so the v1 sound is unchanged |
| `p superMassive` (3x ValhallaSupermassive) | `fx_reverb.h` | approximated by one 4-line FDN (budget); topology below |
| `p mcCurdyGrain` + Spectral Pitch Shift + `p autooct` | `fx_grain.h` | autooct law implemented (below) |
| `abl.device.delay~` (`p absDelay`) | `fx_delay.h` | matches (section 1) |
| `abl.device.roar~` | `fx_roar.h` | pad 2 latch, pot 1 -> DRIVE; ADAA shaper and a 4x oversampling stage above DRIVE 0.6 (drone agent), `fx_halfband.h` |
| `abl.dsp.ringmod~` | `fx_ringmod.h` | pad 5 latch; the FSR now drives the DELAY time, ringmod keeps its own FREQ |

### Findings
- **autopan law** (cords): x = control 0..1. Output gain G = 1 + x through `line` 110 ms on both outputs. lo = 1 - x. Every `metro` = 200 + 400 (1 - x) ms a random r gives pan targets gL = lo + r (1 - lo), gR = lo + (1 - r)(1 - lo); both glide with `line 500` (500 ms); left audio x gL, right audio x gR. v1 uses `random 1.` (integer, always 0): taken literally the pan is the fixed pair (lo, 1); we implement the intended uniform random.
- **MOOD**: automood's index-to-semitone messages, in index order: 24, 19, 12, 7, 0, -9, -12, -17, -24 (gated by the toggles) - the same nine-step ladder as `fx_mood.h` LADDER[] read in the other direction (the joystick path inverts `127 - x`, section 6 correction 4). The transpose goes to `supervp.trans~` as `* 100` cents. Joystick Y reaches automood inlet 1 = the probability compare; bitdepth is NOT driven by Y (the audit's "Y -> bitdepth" is the dead redux branch).
- **autooct**: `metro 8n` (250 ms at 120 bpm) rolls `random 0. 1.`; the octave-up gate is 1 when roll <= 0.06 (the patch tests `> 0.06` and routes the false case to 1), smoothed by `line~` 100 ms, and it gates the Spectral Pitch Shift on the grain input. `fx_grain.h` now does this as a global gate (OCTUP = the threshold, default 0.06, 250 ms ticks, 100 ms smoothing) and each new grain reads +12 st with the gate's current value as the probability; before it was an independent 6 % coin per grain.
- **shiftr**: the message boxes in the patch hold the per-state interval values 5, -5, -12, -5, 7, 12, 7, 7, -12, 12, -7, ...; the index-to-voice mapping was verified earlier (FX_MAP section 6 item 8: "harmonizer table incl. idx 6 (-12,+5)") and `fx_harmonizer.h` A/B tables encode it. Not re-derived message by message here.
- **Supermassive topology**: three instances in PARALLEL on the same stereo input. Wet L = 0.43 x (instance 1 L + instance 1 R + instance 2 L), wet R = 0.43 x (instance 2 R + instance 3 L + instance 3 R); dry and the sum feed `p verbMIX`. So each instance is scaled x0.43 and instance 1 and 3 are mono-summed into one side. Ours is one FDN: density is lower (impulse-response echo density 0.56 vs 0.72 for the 8-line, three parallel instances denser still) and the wet level against the plugin is not matched (the plugin level is unknown). Status: approximate.

Note (1.12): the pad map, sensor map, exclusivity and harmony page are in `docs/TRELLIS_MAP.md`; the pad/sensor sentences above that mention `fx <id> on` describe the web path, which still works.
