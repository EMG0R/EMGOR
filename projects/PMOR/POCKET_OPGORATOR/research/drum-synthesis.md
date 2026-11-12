# Drum Synthesis Recipe Book — 64 voices for Daisy Seed (STM32H7, 48 kHz, float)

Target: 4 groups × 16 synthesized (not sampled) drum voices, each group ordered
index 0 = "classic/normal" → index 15 = "wonky/experimental". Everything here is
meant to be implementable directly from the numbers given; where a number is a
measured/derived value from a source it is cited inline, where it is a
starting-point tuning it is marked *(tune)*.

---------------------------------------------------------------------------

## 0. Conventions used in every recipe

* **Sample rate** 48 kHz. All decays are given as **T60** (time to fall 60 dB)
  unless stated otherwise. Exponential decay coefficient per sample:
  `k = exp(-6.9078 / (T60_s * fs))`, envelope `e[n] = e[n-1] * k`.
  (One-pole "decay" as used in Plaits/DaisySP: `e -= e * (1/(tau*fs))`, tau in seconds;
  T60 ≈ 6.9·tau.)
* **Pitch envelope** written as `f_start → f_end over T (curve)`. "exp" means the
  *frequency offset* decays exponentially: `f(t) = f_end + (f_start - f_end)·exp(-t/τ)`
  with τ = T/5 so that the sweep is ~99% finished at T.  "lin" = linear.
* **Bridged-T resonator** (the 808 workhorse) is modelled digitally as a 2-pole
  band-pass / damped-sine resonator pinged by a short pulse:
  `y[n] = 2 r cos(ω) y[n-1] − r² y[n-2] + x[n]` with `ω = 2π f0 / fs`,
  `r = exp(-π f0 / (Q fs))`, T60 ≈ `2.2·Q/f0` s → `Q = T60·f0/2.2`.
  This is exactly what DaisySP's `AnalogBassDrum`/`AnalogSnareDrum` do
  (`Svf`-style resonator with Q = 1500·2^(decay·80/12) for the kick,
  2000·2^(decay_xt·84/12) for the snare) and what Werner shows analytically
  (2nd-order band-pass with positive feedback,
  https://csclub.uwaterloo.ca/~pbarfuss/dafx14_kurt_james_werner_a_physically_informed_ci.pdf).
* **"Swing-type VCA"** = the 808's diode-gated transistor VCA. Werner's cymbal
  paper fits it with a soft clip of sharpness α = 3.5
  (https://www.icmc14-smc14.net/images/proceedings/OS24-B10-TheTR-808Cymbal.pdf);
  Plaits/DaisySP `HiHat` uses `(s/(1+|s|) + 1)·gain` (asymmetric, adds
  even harmonics). Use `swing(s,g) = g · s/(1+|s|)` as a cheap stand-in.
* **Noise**: "white" = uniform/LCG PRNG. "clocked noise" = sample-and-hold
  white noise at rate fc (DaisySP `ClockedNoise`); the 909 uses a binary
  (LFSR) noise generator, so clocked/1-bit noise is *more* authentic than
  float white noise for 909 sounds (https://www.tiptopaudio.com/manuals/Tiptop_Audio_SD909_ns.pdf).
* DaisySP (https://github.com/electro-smith/DaisySP, `Source/Drums/`) ships
  `AnalogBassDrum`, `SyntheticBassDrum`, `AnalogSnareDrum`, `SyntheticSnareDrum`,
  `HiHat<MetallicNoiseSource, VCA, resonance>` (with `SquareNoise` and
  `RingModNoise`). All five are straight ports of Mutable Instruments **Plaits**
  models 14/15/16 (OUT and AUX outputs), MIT licensed
  (https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/).
  Where a recipe says **"DaisySP: yes"** you can instantiate the class and set
  the numbers given; otherwise you build from `Oscillator`, `Svf`,
  `WhiteNoise`, `ClockedNoise`, `AdEnv`/`Decay`, `Fm2`.

---------------------------------------------------------------------------

## 1. Reference topologies of the classics (one paragraph each)

**TR-808 bass drum.** A bridged-T RC network around an op-amp, biased just
below self-oscillation, is "pinged" by a 1 ms trigger pulse (3.5 V base, up to
~13.5 V with accent) that has been pulse-shaped (rising edge = trigger level,
falling edge clipped to R162/(R162+R163)·V + 0.71 V). The network rings at
≈49.5 Hz measured (service notes say 56 Hz) and the ring decays according to the
amount of positive feedback (Decay knob = high-shelf attenuation in the feedback
path; 50–800 ms, ~300 ms centre). A "retriggering" transistor leaks current
into the network for ≈5–6 ms after the trigger, pushing the pitch up by more
than an octave (~130 Hz) during the attack — that is the 808's "click/punch" —
and a slow leak causes a gentle downward "sigh" over the decay, more with
accent. Then a one-pole low-pass (Tone), a 6.7 Hz DC-blocking high-pass, and a
VCA. Sources: Werner DAFx-14 paper (above);
https://www.baratatronix.com/blog/808-bd-synthesis ;
https://archive.org/stream/synthmanual-roland-tr-808-service-notes/rolandtr-808servicenotes_djvu.txt.

**TR-909 bass drum.** A true VCO (triangle core) whose pitch is swept by an
envelope (EG3: instant attack, slow decay; "Tune" = sweep depth). The triangle is
saturated by back-to-back diodes into a "hexagonal" wave and low-passed into a
rounded sine. A separate VCA envelope (EG1, AR; Accent raises attack level and
shortens/lengthens decay). A parallel "click" path: a short pulse (essentially
the trigger) plus low-pass-filtered noise through its own envelope (EG2), mixed
in before the output. Sources: Gordon Reid, SOS "Practical Bass Drum Synthesis"
https://www.soundonsound.com/techniques/practical-bass-drum-synthesis ;
Robin Whittle's 909 mod notes https://www.firstpr.com.au/rwi/tr-909/TR-909-Sound-Mods.pdf
(pitch pot range 0.43×…4.7× nominal, Tune depth up to 2× stock sweep, max decay 4× stock);
https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1099922-synthesizing-tr-909-kick-drum.html.

**TR-808 snare.** Two bridged-T resonators, one low (service notes 238 Hz
"typical"; component-value analysis gives ~173–250 Hz) and one high (476 Hz per
service notes; ~336–499 Hz by components), each pinged by the trigger, mixed by
the Tone pot. White noise → high-pass → swing-type VCA driven by its own
envelope; "Snappy" scales the trigger into that envelope (i.e. noise level and
effective length). Note the SOS article and most emulations use ~180 Hz and
~330 Hz (the 0,1 modes of a real 14" snare), which is what the 909 *also*
approximates. Sources:
https://www.tumblr.com/kurtjameswerner/51352144814/chuck-tr-808-emulator-snare-drum-sd-emulation ;
https://www.baratatronix.com/cascadia/cascadia-simplified-808-snare ;
https://www.soundonsound.com/techniques/practical-snare-drum-synthesis.

**TR-909 snare.** Two triangle VCOs (VCO2 tuned higher, ~fifth/tritone above),
rounded by diodes, each with its own envelope/VCA, both pitch-swept by a short
envelope (Tune). Binary (shift-register) noise → low-pass, then split into a
direct path and a high-pass path (Snappy = gain, Tone = length), each with its
own contour. Stock 909 has a ~24 ms flat "hold" at the start of the snare-noise
envelope (C68) — that hold is part of the 909 clap-like crack. Sources: SOS
snare article; Whittle 909 mods PDF; Tiptop SD909 manual.

**TR-808 hand clap.** White noise → band-pass ≈1 kHz → two parallel VCAs. VCA 1
is driven by a sawtooth "re-trigger" envelope: three ~10 ms discharge cycles
then one ~20 ms uninterrupted discharge (≈50 ms total — the "snap"); VCA 2 is
driven by a smooth ~100 ms decay (the "reverb" tail). Both summed. Sources:
https://www.baratatronix.com/blog/cascadia-808-clap-synthesis ; 808 service
notes. The 909 clap is the same idea with BPF ≈1140 Hz, Q ≈1.95 (schematic
derivation) and 4 bursts ~11 ms apart
(https://www.kvraudio.com/forum/viewtopic.php?p=6510980 ;
https://cim.mcgill.ca/~clark/nordmodularbook/nm_percussion.html ;
https://noiseengineering.us/blogs/loquelic-literitas-the-blog/percussion-synthesis-part-3-the-sound-of-one-synth-clapping/).

**TR-808 hi-hat / cymbal (the "metal bank").** Six CMOS Schmitt-trigger
(HD14584) square oscillators, ~48% duty, at **205.3, 304.4, 369.6, 522.7 Hz
(fixed) plus 540 Hz and 800 Hz (factory targets; trimmable 254.3–627.2 Hz and
359.4–1149.9 Hz)**, summed equally, then split into two bridged-T band-pass
filters at **≈3440 Hz and ≈7100 Hz** that deliberately emphasise the upper
harmonics and suppress the fundamentals. Cymbal: both bands, separate swing VCAs
(one envelope), the high band through a resonant high-pass (~10.5 kHz peak,
3rd order), Tone mixes the paths; decay 350–1200 ms. Hi-hats: only the high
(7.1 kHz) band, through a high-pass and a VCA shared by OH and CH envelopes:
CH ≈ 50 ms fixed, OH 90–600 ms. Sources: Werner ICMC-14 cymbal paper;
https://www.baratatronix.com/blog/cascadia-808-cymbal-hi-hat-synthesis ;
Plaits `SquareNoise` uses the same bank expressed as ratios
**1.0, 1.304, 1.466, 1.787, 1.932, 2.536 × 414 Hz** (= 414, 540, 607, 740, 800,
1050 Hz — pichenettes' re-voicing, brighter than the real 808).

**TR-606 hi-hat/cymbal.** Same architecture (six HD14584B squares, two BPFs at
3440/7100 Hz) but at **246.4, 308.0, 367.0, 418.2, 440.4, 627.2 Hz**; CH/OH share
one VCA with two envelopes; OH decay is tempo-dependent and is choked by CH
(https://www.baratatronix.com/blog/606-cymbal-and-hi-hat-synthesis).

**TR-808 cowbell.** Two of the same Schmitt-trigger squares (Werner: **540 Hz
and 800 Hz**, an out-of-tune 5th of 6.8 semitones; SOS/other sources quote
587 Hz and 845 Hz measured on other units — ratio ≈1.44–1.48 either way) →
4th-order band-pass whose two peaks sit on the oscillators; derived component
analysis gives centre **881 Hz, Q 4.82, BW 183 Hz**; envelope = sharp attack,
two-stage decay (fast then long tail, ~50 ms "mid" in the service chart, 600 ms
audible tail), swing VCA. Sources:
https://www.academia.edu/7462829/More_Cowbell_a_Physically_Informed_Circuit_Bendable_Digital_Model_of_the_TR_808_Cowbell ;
https://www.baratatronix.com/blog/808-cowbell-bandpass-filter-frequency ;
https://www.soundonsound.com/techniques/synthesizing-cowbells-claves.

**TR-808 rimshot / clave.** Two bridged-T generators, **IC20 ≈ 1667 Hz** and
**IC21 ≈ 455 Hz**, envelope 10 ms (service chart), mixed → VCA → high-pass;
clave mode combines the halves of IC20 into a single **≈2500 Hz** ring with
~25 ms decay, no filter. Sources: https://www.baratatronix.com/blog/808-rimshot ;
https://syntherjack.net/the-perfect-808-rimshot-clave/ ; 808 service notes.

**TR-808 toms / congas.** One bridged-T per pair, tunable: **LT 80–100 Hz
(90 centre), MT 120–160 Hz (135), HT 165–220 Hz (185)**; decays LT 200 ms,
MT 130 ms, HT 100 ms; diodes in the network give a pitch drop at the attack;
toms add a little low-passed pink noise with slightly longer decay than the
body, congas bypass half the network (no noise, shorter, harder).
Source: https://www.baratatronix.com/blog/808-tom-synthesis.

**TR-808 maracas.** Band-passed white noise through a fast envelope
(service chart: 25/35 ms).

**Sample-based classics (must be approximated):** TR-909 hats/crash/ride are
6-bit ROM samples through analog VCA/filters; TR-707, LinnDrum (8-bit µ-law,
~28 kHz) and Oberheim DMX (8-bit companded) are fully sampled machines. The
recipes below give synthetic stand-ins that capture the gesture (bit-crush,
low sample-rate replay, gated noise) rather than the exact waveform.

**Simmons SDS-V.** Per voice: one triangle VCO with a "bend" (pitch sweep) and
optional LFO, one noise source with its own tone filter and sweep, a click
derived from the pad impact, SSM2044 4-pole VCF with resonance, four simple AR
envelopes, three CA3080 VCAs (https://syntherjack.net/sdsv-voice-board/).
The "Simmons" sound = a long linear-ish pitch sweep on a triangle + resonant
VCF sweep + noise, all governed by one envelope.

---------------------------------------------------------------------------

## 2. KICKS (group 0)

| # | Name | DaisySP class |
|---|------|---------------|
| 0 | 808 Kick | `AnalogBassDrum` (or handmade resonator) |
| 1 | 909 Kick | `SyntheticBassDrum` |
| 2 | Sub Kick | sine + env |
| 3 | Phone Kick (mid-forward) | sine + click + waveshaper |
| 4 | 606 Kick | resonator |
| 5 | 707 Kick (synthetic stand-in) | sine + noise |
| 6 | Linn Kick (stand-in) | sine + clicky noise, decimated |
| 7 | Simmons Kick | tri + VCF sweep |
| 8 | FM/Metallic Kick | `Fm2` |
| 9 | Distorted Kick | sine → tanh/foldback |
|10 | Long Boom | resonator, long decay |
|11 | Pitched Zap Kick | wide exp sweep |
|12 | Rumble/Layered Kick | two resonators detuned |
|13 | Glitch Kick | retriggered pulses / gated |
|14 | Noise-punch Kick | inverted-env noise + sine |
|15 | Reverse/Wonky Kick | reversed env + upward pitch |

### K0 — TR-808 kick (faithful)
* **Topology**: bridged-T resonator pinged by shaped pulse; attack FM; slow sigh.
* **Recipe (handmade)**
  * Resonator f0 = **49.5 Hz** (range 30–80 Hz via pitch input; service manual nominal 56 Hz).
  * Q from Decay: T60 = 50 ms … 800 ms (default **300 ms**) → `Q = T60·f0/2.2` (≈ 6.7 … 108 at 49.5 Hz).
  * Exciter: 1 ms rectangular pulse, amplitude 1.0 (+ accent up to ×3.9, mirroring 3.5→13.5 V), low-passed with τ = 0.1 ms, then diode-shaped: for negative going part use `0.7·x/(1+|x|)` (DaisySP constants).
  * Attack pitch bump: for the first **6 ms**, f = f0·(1 + 1.7·a(t)) where a(t) is the 6 ms pulse low-passed with τ = 0.1 ms → peak ≈ **130 Hz** falling back to f0 (Werner; Baratatronix).
  * Self-FM "sigh": `f = f0·(1 + 0.08·self_fm·|y|)` with self_fm 0…1 (DaisySP) → pitch a few % higher while loud, flattening as it decays. With accent, drive higher for the classic accented-808 pitch drop.
  * Tone: one-pole LP, cutoff = 4·f0·2^(tone·9) capped at Nyquist (DaisySP: `4·f0·2^(tone·108/12)`); at tone = 0 the click is gone, at 1 the 6 ms bump is audible as a click.
  * Output HP 6.7 Hz (DC block) — use 10–20 Hz.
* **DaisySP**: `AnalogBassDrum` — SetFreq(49.5), SetDecay(0.5), SetTone(0.5), SetAttackFmAmount(0.5…1), SetSelfFmAmount(0.3), SetAccent(0.5).  Parameter ranges: all 0…1 (Plaits Model 14 OUT).
* **Ranges**: f0 30–80 Hz, decay 50–800 ms, tone 0–1, accent 0–1.

### K1 — TR-909 kick (faithful)
* **Topology**: triangle VCO → diode "hexagon" → LP → VCA; pitch EG; click = pulse + LP noise.
* **Recipe**
  * Osc: triangle, or DaisySP's "distorted sine" `2·tri/(1+|tri|)` (SyntheticBassDrum, dirtiness blends between tri-derived and true sine).
  * f_end = **50–60 Hz** (E1–A#1; users describe stock 909 tune as lower than 808 body, ~E3 partial region; tune pot 0.43×…4.7× stock in Whittle's mod).
  * Pitch env: f_start = f_end × (1 + 3.5·fm_amount) i.e. up to **4.5×** (DaisySP: `f0·(1 + fm_env_amount·3.5·fm_lp)`), decay τ = 8 ms·(1 + 4·fm_decay) → **8…40 ms** τ (T60 ≈ 55…275 ms). Stock 909: fm_amount ≈ 0.5, fm_decay ≈ 0.3 → 50 → ~140 Hz over ~100 ms *(tune)*. SOS/MusicRadar: "200–500 ms" pitch-env decay knob range.
  * Amp env: attack 0 (with a 1.3 ms gate that starts the pitch mod), body decay τ = 20 ms·2^(decay·60/12) → **20 ms…640 ms** τ; stock ≈ 0.6 → τ 160 ms (T60 ≈ 1.1 s? cap at 700 ms). Transient decay τ = 5 ms.
  * Click: trigger impulse → HP(coefficient 0.1) → LP (0.5), then 5 kHz LP anti-alias, plus attack noise = white → LP(0.05) → HP(0.005) (DaisySP `SyntheticBassDrumClick` / `SyntheticBassDrumAttackNoise`). In SOS terms: noise LP "≈5 kHz, resonance ~18 %", envelope 5–15 ms.
  * "Transistor VCA": `s = (x − 0.6)·gain; y = 3s/(2+|s|) + 0.3·gain` — asymmetric soft clip adding 2nd harmonic; drive range 0.9×…10× (Whittle).
  * Output LP tone: 0–1 maps ~1–10 kHz.
* **DaisySP**: `SyntheticBassDrum` SetFreq(55), SetFmEnvelopeAmount(0.5), SetFmEnvelopeDecay(0.3), SetDecay(0.55), SetDirtiness(0.3), SetTone(0.6), SetAccent(0.5) (Plaits Model 14 AUX).

### K2 — Sub kick
* Pure sine, f0 = **40 Hz** (input 25–60 Hz). Pitch env 1.5×f0 → f0 over 15 ms (exp) — enough to give a transient, not enough to hear a sweep.
* Amp: attack 1 ms (raised-cosine), T60 = 600 ms, exponential; add a linear tail so the last 100 ms fade to zero without a click.
* Very light `tanh(1.2·x)` for a bit of 2nd/3rd harmonic so it exists on small speakers.
* CPU: trivial.

### K3 — "Phone speaker" kick (mid-forward)
* Body: sine f_end = **65 Hz**; pitch env 220 → 65 Hz over 40 ms (exp).
* Harmonics: waveshape `y = tanh(3·x)` then `x + 0.5·x²` (rectifier adds octave); this puts energy at 130–400 Hz where phone speakers actually work.
* Click layer: 3 ms burst of white noise → BP 2.5 kHz Q 2, amplitude 0.4 with T60 12 ms.
* Mid transient: second sine 180 → 120 Hz over 25 ms, T60 60 ms, level 0.5.
* Amp: attack 0.5 ms, T60 250 ms. Post: HP 45 Hz so it doesn't waste headroom; optional gentle peak EQ +4 dB @ 200 Hz.
* Ranges: pitch 50–90 Hz, tone = click level & rectifier amount, decay 100–500 ms.

### K4 — TR-606 kick
* 606 uses an 808-style bridged-T but smaller: shorter decay, more click. Recipe = K0 with f0 = **60 Hz**, T60 = **180 ms**, attack bump 2× f0 for 4 ms, tone LP 1.5 kHz, add a 2 ms sharp pulse (click) at −8 dB.

### K5 — 707 kick (stand-in for the 12-bit sample)
* Sine 58 Hz, pitch env 120 → 58 Hz over 35 ms; T60 220 ms; layer noise burst → LP 1.2 kHz, T60 20 ms.
* Then **bit-crush to 12 bits and sample-hold at 25 kHz** (707 ROM rate is ~25 kHz) — the aliasing/quantisation is the "707" grit.

### K6 — LinnDrum-style kick (stand-in for the 8-bit µ-law sample)
* Sine 60 Hz with a fast 2-stage pitch env: 300 → 90 Hz over 8 ms then 90 → 60 over 60 ms; T60 300 ms; strong click: 1 ms pulse through BP 1.8 kHz Q 1.5. Companded 8-bit quantisation (µ-law: `sign(x)·ln(1+255|x|)/ln(256)` quantise to 256 steps then expand) and 28 kHz sample-hold. Gentle LP 8 kHz.

### K7 — Simmons kick
* Triangle 55 Hz; pitch env 4× → 1× over **250 ms, linear** (the Simmons bend is a long, almost linear droop).
* SSM2044-style 4-pole LP (DaisySP `MoogLadder` or `Svf` 24 dB), cutoff 3 kHz → 150 Hz over 250 ms, resonance 0.4.
* Noise: white → LP 4 kHz, T60 80 ms, level 0.3; click 1 ms pulse.
* Amp AR: attack 0, T60 400 ms. Ranges: bend depth 1–8×, bend time 50–600 ms.

### K8 — FM / metallic kick
* DaisySP `Fm2`: carrier 55 Hz, ratio **1.5** (index sweeps 6 → 0 over 30 ms, exp). Result: a metallic "ping" transient that collapses to a sine.
* Add pitch env 2× → 1× over 20 ms on the carrier.
* T60 250 ms. Tone knob = max index (0–12), ratio choices 1.5 / 2 / 2.76 / 3.5 for increasingly inharmonic.

### K9 — Distorted kick
* Start from K1 (909) with decay 0.7; drive stage: `y = tanh(g·x)` with g = 4…20 followed by a 1-pole LP 2.5 kHz to tame fizz, then `fold(x) = x − 4·floor((x+1)/2)·…` (triangle wavefolder) at 20 % blend for the rave/gabber character. DC-block after folding. Tone knob = LP cutoff; drive tied to accent.

### K10 — Long boom (808 "Jeep" kick)
* K0 with T60 = **1.2–2.0 s**, f0 = 45 Hz, self-FM 0.6 (gives the audible pitch sigh), tone 0.2, output `tanh(1.5x)`. Level-match aggressively (it is loud & long).

### K11 — Pitched zap kick
* Sine (or tri) f_start = **2 kHz → f_end 50 Hz** over 80 ms, *exponential in pitch* (i.e. linear in log-frequency: `f = f_end·(f_start/f_end)^(exp(-t/τ))` ) — this is the laser/zap curve. T60 300 ms. Tone = f_start (500 Hz–8 kHz). "Sweep time" ties to length ratio.

### K12 — Rumble / layered kick
* Two resonators (K0 engine): 48 Hz T60 400 ms + 72 Hz T60 150 ms (a 3:2 detune), second at −6 dB, plus sub-octave sine 24 Hz at −12 dB for 80 ms. Slight random detune (±2 %) per trigger for movement.

### K13 — Glitch kick
* K1 engine but the exciter retriggers **every 12 ms for the first 60 ms** (5 pulses), each retrigger at 70 % of the previous amplitude and pitch env restarted; amp env is a rectangular 8 ms gate on each retrigger plus a 150 ms tail. Add 1-bit noise (LFSR) burst 4 ms at −10 dB. Result: stuttering "buzz-roll" kick.

### K14 — Noise-punch kick
* Sine 50 Hz, T60 200 ms, *plus* white noise → LP whose cutoff sweeps 6 kHz → 200 Hz over 40 ms (exp), noise env T60 40 ms at 0 dB relative to sine. The falling LP is the "punch". Tone knob = start cutoff.

### K15 — Reverse / wonky kick
* K0 engine run with **time-reversed envelopes** (see §6.1): amplitude rises exponentially over the length then cuts (5 ms release), pitch sweeps *up* 50 → 130 Hz at the end. Add slow 6 Hz sine FM of ±10 % during the swell. Length 200–800 ms.

---------------------------------------------------------------------------

## 3. SNARES / CLAPS (group 1)

| # | Name | DaisySP |
|---|------|---------|
| 0 | 808 Snare | `AnalogSnareDrum` (tone<0.667 → 2 modes) |
| 1 | 909 Snare | `SyntheticSnareDrum` |
| 2 | 808 Clap | handmade |
| 3 | 909 Clap | handmade |
| 4 | Tight snare | resonator + noise |
| 5 | Big / gated snare | noise + hold + hard gate |
| 6 | 808 Rimshot | 2 resonators |
| 7 | DMX-style snare | noise + 8-bit |
| 8 | Linn-style snare | resonators + companded noise |
| 9 | FM snare | `Fm2` + noise |
|10 | Noise burst (white) | noise + env |
|11 | Noise burst (BP sweep) | noise + BP sweep |
|12 | Brush / swish | noise + slow env |
|13 | Modal/metal snare | 5-mode resonator (Plaits tone>0.667) |
|14 | Zap snare | sine sweep + noise |
|15 | Reverse snare | reversed env |

### S0 — TR-808 snare
* **Recipe (handmade)**
  * Two resonators pinged by 1 ms pulse: **f1 = 180 Hz** (T60 ≈ 160 ms) and **f2 = 330 Hz** (T60 ≈ 90 ms) — these are the widely used values; for a "service-manual 808" use 238 Hz / 476 Hz (ratio 2.0) instead. Tone knob = crossfade f1↔f2 (0.5 = equal).
  * Each resonator has its own tiny pitch drop: multiply f by (1 + 0.15·e_pulse) for the first 2 ms.
  * Noise: white → HP **~1.5–2 kHz** (Werner's model needed an extra LP ~8–10 kHz to stop it being too bright) → swing VCA `n/(1+|n|)` → envelope attack 0, T60 = **60 ms + snappy·240 ms** (SOS/MusicRadar "100–600 ms" range). Snappy 0…1 also scales noise gain 0…1.
  * Mix: tonal 0.6 + noise 0.4·snappy; soft clip at ±3 (DaisySP clips the resonator excitation at ±3).
* **DaisySP**: `AnalogSnareDrum` SetFreq(180) (modes at ratios 1, 2.00, 3.18, 4.16, 5.62; only modes 1–2 active for tone < 0.667), SetTone(0.4), SetDecay(0.4) (Q = 2000·2^(decay_xt·84/12), modes 1–4 get 0.25×Q), SetSnappy(0.5) (noise BP at 16·f0, gain ×1.5), SetAccent(0.5).
* Ranges: f0 120–260 Hz, decay 40–400 ms, snappy 0–1, tone 0–1.

### S1 — TR-909 snare
* **Recipe (handmade)**
  * Two triangle→"rounded" oscillators (`2·tri/(1+|tri|)`), **f1 = 185 Hz, f2 = 1.47·f1 ≈ 272 Hz** (DaisySP ratio; a tritone-ish "wrong" fifth), each with its own decay: T60 f1 = 120 ms, f2 = 80 ms.
  * Pitch env on both: f·(1 + 4·fm_amount·e), τ = **7 ms** (DaisySP), fm_amount ≈ 0.25 → 2× → 1× over ~35 ms.
  * Noise: 1-bit LFSR/clocked noise (rate 20–25 kHz) → LP 7 kHz → HP **10·f0 ≈ 1.9 kHz** (DaisySP uses HP 10·f0 and LP 35·f0, resonance 0.5 + 2·snappy) → envelope with **hold 40–70 ms** then decay τ = 10 ms·2^(-(decay·60 + snappy·7)/12) (DaisySP `SyntheticSnareDrum`; stock 909 has a 24 ms hold, Whittle).
  * Mix `sqrt(1−snappy)·tone + sqrt(snappy)·noise`; drum decay τ = 15 ms·2^((−decay·72 − fm·12 + snappy·7)/12) (negative exponent means longer decay for higher knob — copy the DaisySP formula verbatim).
* **DaisySP**: `SyntheticSnareDrum` SetFreq(185), SetFmAmount(0.25), SetDecay(0.5), SetSnappy(0.5), SetAccent(0.5).

### S2 — TR-808 clap
* White noise → BP **1000 Hz, Q ≈ 2** *(Q not documented; 1.5–3 sounds right)*.
* VCA A ("snap"): sawtooth re-trigger envelope: at t = 0, 10, 20 ms start a fresh exponential decay of τ = 4 ms (decay to ~10 % in 10 ms); at t = 30 ms start a final decay of τ = 8 ms (20 ms). Total ≈ 50 ms. (808 service notes: 3 cycles of the sawtooth generator, then IC23 pin 1 releases; Baratatronix: "three 10 ms cycles then one 20 ms".)
* VCA B ("reverb"): same noise, smooth env attack 5 ms, T60 **100 ms** (service chart "100"), mixed at −6 dB (turn down for snappier).
* Output: `tanh(2x)`; ranges: BP 700–1500 Hz, repeat spacing 8–14 ms, tail 60–300 ms, tail level 0–1.

### S3 — TR-909 clap
* Same structure with **BP 1140 Hz, Q 1.95** (KVR schematic derivation), **4 bursts 11 ms apart** (Nord Modular book: "four pulses ~11 ms apart"), each burst τ = 3.5 ms; then a tail envelope T60 ≈ 150 ms with attack 10 ms (909's "reverb" has a slower swell than the 808's). Use 1-bit noise for authenticity. Post: LP 9 kHz. Optional "density" (Whittle): number of bursts 2–8.

### S4 — Tight snare
* Resonators 200 Hz (T60 50 ms) + 360 Hz (T60 35 ms); pitch bump 1.4× for 3 ms.
* Noise: white → BP 3 kHz Q 0.7 → env T60 45 ms. Mix 50/50. Short overall (< 80 ms). Add hard `tanh(2.5x)`.

### S5 — Big / gated snare
* Body: resonators 160 + 300 Hz T60 250 ms; noise: white → HP 800 Hz → env with **hold 120 ms then 5 ms hard cut** (the 80s gate), noise level 0.8; plus a 30 ms noise "room" pre-delay copy at −8 dB (feed the noise through a 25 ms delay line, comb-ish) for size. Tone knob = HP cutoff 300 Hz–3 kHz; length = hold time.

### S6 — TR-808 rimshot
* Two resonators pinged: **1667 Hz** and **455 Hz** (Baratatronix), amplitudes 1.0 and 0.6, both T60 ≈ **10–15 ms** (service chart 10 ms); output → HP 400 Hz → `tanh(4x)` (the 808 RS is quite clipped) → VCA env attack 0, T60 25 ms. Pitch input scales both.

### S7 — DMX-style snare (stand-in for the 8-bit sample)
* Sine 190 Hz with pitch drop 1.6× → 1× over 10 ms, T60 70 ms; noise: white → BP 1.8 kHz Q 1 (T60 110 ms) + white → HP 5 kHz (T60 60 ms); mix 0.4/0.6/0.3.
* Then **8-bit companded quantisation** and **sample-hold at 24 kHz** with no anti-alias filter (the DMX aliasing sizzle is the sound). Slight 6 dB boost at 2 kHz.

### S8 — LinnDrum-style snare
* Resonators 180 (T60 150 ms) + 330 Hz (T60 90 ms) plus **a 440 Hz mode T60 60 ms at −10 dB**, noise → LP 6 kHz, env T60 130 ms with 8 ms attack (Linn snares have a softer "thwack"). µ-law 8-bit, 28 kHz hold, LP 10 kHz.

### S9 — FM snare
* `Fm2` carrier 200 Hz, ratio **2.31** (inharmonic), index 8 → 1 over 40 ms (exp); carrier pitch env 1.5× → 1× over 15 ms; T60 120 ms.
* Noise: white → BP 2.2 kHz Q 1.2, T60 90 ms at −4 dB. Tone = max index; pitch = carrier.

### S10 — Noise burst (white)
* White noise → env attack 0.5 ms, T60 **80 ms**, no filter. Tone knob = a single 1-pole tilt (LP 20 kHz→2 kHz). Cheapest snare.

### S11 — Noise burst (BP sweep)
* White noise → BP Q 3, centre sweeps **6 kHz → 400 Hz over 60 ms** (exp), env T60 120 ms. Tone = end frequency, pitch = start frequency. Sounds like a "pew"-snare.

### S12 — Brush / swish
* Pink-ish noise (white → 1-pole LP 3 kHz + HP 500 Hz) with **attack 40 ms, T60 250 ms**, plus a 2 ms click at onset −12 dB. Add slow BP wobble 200 Hz→1 kHz over the length. Reverse flag makes a great "swoosh".

### S13 — Modal / metal snare
* DaisySP `AnalogSnareDrum` with tone > 0.667 (all 5 modes: 1, 2.00, 3.18, 4.16, 5.62 × f0 — modes 3–5 are the inharmonic partials of a circular membrane), f0 = 220 Hz, decay 0.6, snappy 0.3. Tone knob sweeps mode balance.

### S14 — Zap snare
* Sine **1800 → 200 Hz over 30 ms** (log-linear zap curve as in K11), T60 90 ms, plus white noise → HP 2 kHz T60 60 ms at −3 dB. `tanh(3x)`.

### S15 — Reverse snare
* S1 (909) with time-reversed envelopes (noise swells over the length, tone enters at the end, pitch env runs upward), 5 ms hard release. Length 150–600 ms.

---------------------------------------------------------------------------

## 4. HI-HATS / CYMBALS (group 2)

Shared engine: a **MetalBank** = 6 square oscillators (naive squares are fine
*if* you band-pass afterwards; the 808 filters kill the fundamentals anyway; to
avoid aliasing buzz use polyBLEP squares or run the bank at 2× and decimate —
Werner used 4× oversampling) → BP1 3440 Hz (Q ≈ 2–3) → BP2 7100 Hz (Q ≈ 2–3) →
HP (Q 0.5–1) → swing VCA → decay env. Using DaisySP: `HiHat<SquareNoise,
SwingVCA, true>` gives the metallic bank (at Plaits' 414 Hz tuning), `HiHat<RingModNoise,
LinearVCA, false>` gives the AUX ring-mod variant. Plaits' HiHat applies BP at
`150 Hz·2^(tone·72/12)` (150 Hz–9.6 kHz), Q = 3 + 6·tone, followed by HP at the
same frequency Q 0.5, and blends in clocked noise (`noisiness²`) whose clock is
`f0·(16 + 16·(1−noisiness))`. Envelope: two rates, `1 − 0.003·2^(−7·decay)` above
the knee and `1 − 0.0025·2^(−3·decay)` below (fast then slower tail).

| # | Name |
|---|------|
| 0 | 808 Closed Hat |
| 1 | 808 Open Hat |
| 2 | 909 Closed Hat (stand-in) |
| 3 | 909 Open Hat (stand-in) |
| 4 | 606 Closed Hat |
| 5 | 606 Open Hat |
| 6 | 808 Cymbal / Crash (long) |
| 7 | Ride |
| 8 | Noise Closed Hat |
| 9 | Noise Open Hat |
|10 | Ring-mod Hat (Plaits AUX) |
|11 | Pedal / choke hat |
|12 | Shaker-hat (filtered noise, slow attack) |
|13 | FM Cymbal |
|14 | Bit-crushed hat |
|15 | Reverse cymbal |

### H0 — 808 closed hat
* Bank: squares at **205.3, 304.4, 369.6, 522.7, 540, 800 Hz**, equal mix (sum/6).
* BP 7100 Hz Q 2.5 (only the high band feeds the hats) → HP 6 kHz Q 0.7 (the 808's HP before the VCA).
* Swing VCA; env attack 0.2 ms, T60 = **50 ms** (fixed on the 808; expose 20–120 ms).
* Tone knob: HP cutoff 3–10 kHz. Pitch input: scale all six oscillators together (±1 octave) — nominal "808" is 1.0.
* DaisySP: `HiHat<SquareNoise, SwingVCA, true>`, SetFreq(205.3·? — DaisySP's SquareNoise uses ratios 1…2.536 off f0; set f0 = 300 Hz for a close 808 balance), SetTone(0.9), SetDecay(0.15), SetNoisiness(0.1), SetAccent(0.6).

### H1 — 808 open hat
* As H0 but T60 = **90–600 ms** (default 350 ms), and the envelope is two-stage: fast initial fall (first 30 ms drop ~6 dB) then the long tail (implement as sum of 0.4·env(T60 = 40 ms) + 0.6·env(T60 = length)).
* Choke: triggering H0 must set H1's envelope to a 5 ms release (shared VCA behaviour).

### H2 — 909 closed hat (stand-in for the 6-bit sample)
* White noise → **HP 7 kHz** 2nd order → BP 9.5 kHz Q 1.2 → mix with a MetalBank at 1.25× 808 frequencies at −6 dB (to add some inharmonic sizzle) → **6-bit quantise + 32 kHz sample-hold** → env attack 0.3 ms, T60 **60 ms**. Then a 1-pole LP 14 kHz (909 output stage roll-off).

### H3 — 909 open hat
* H2 with T60 **250–800 ms** (default 400 ms), env with linear-ish fall in the first 100 ms (the 909's VCA is driven by a linear-then-exponential contour). Choke rule as H1.

### H4 — 606 closed hat
* Bank **246.4, 308.0, 367.0, 418.2, 440.4, 627.2 Hz**, BP 7100 Hz Q 2.5, resonant HP 8 kHz Q 2 (606's HP is noticeably resonant → thinner, "tinnier"), T60 **40 ms**.

### H5 — 606 open hat
* H4 with T60 200–500 ms; choked by H4. (606 OH decay is tempo-dependent — expose as length.)

### H6 — 808 cymbal / long crash
* Bank as H0. Two bands: BP 3440 Hz Q 2.5 → VCA (env T60 = length) and BP 7100 Hz Q 2.5 → resonant HP 10.5 kHz Q 2 → VCA (env T60 = 0.6·length; the high band dies faster). Tone knob = mix of the two bands (808 "Tone").
* Length **350–1200 ms** (default 800 ms); attack 0.3 ms; swing VCA (`s/(1+|s|)`). This is the genuine 808 CY.

### H7 — Ride
* Same bank but with 3 of the 6 oscillators shifted to ratios that are nearer-harmonic (use 205.3, 304.4, 369.6, **739 (= 2×369.6), 1044, 1600 Hz**) → BP 5 kHz Q 1.5 → plus a **"bell" ping**: sine 2.9 kHz + sine 4.3 kHz (T60 400 ms, −10 dB) → env: attack 0.5 ms, T60 **1.5–2.5 s**, with the wash band (BP 5 kHz) at −6 dB relative to the ping. Mod Wiggler "ride mod" thread on the 808 cymbal describes lengthening the low band and adding a ping (https://www.modwiggler.com/forum/viewtopic.php?t=266697).

### H8 — Noise closed hat
* White noise → HP **8 kHz** 4th order (2× Svf) → env T60 **35 ms**. Tone = HP cutoff 2–12 kHz. Trivially cheap. (MusicRadar recipe: HP 2 kHz+, 12–24 dB/oct, 200 ms max.)

### H9 — Noise open hat
* H8 with T60 300–1000 ms, add slight resonance (Q 1.5) at the HP corner for shimmer and a very slow (0.3 Hz-ish) random modulation of cutoff ±10 %.

### H10 — Ring-mod hat (Plaits AUX / DaisySP `RingModNoise`)
* Three square×saw pairs ring-modulated: **200 Hz×7530 Hz, 510×8075, 730×10500** (scaled by `f0/(0.01+f0)`) summed, → BP (tone) → linear VCA. Brighter, more "digital" than the six-square bank. DaisySP: `HiHat<RingModNoise, LinearVCA, false>`.

### H11 — Pedal / choke hat
* H0 but with T60 15 ms body plus a second, delayed hit at +25 ms (−6 dB) — the "chick" of a foot-closed hat; HP 5 kHz; add LP 9 kHz for dullness.

### H12 — Shaker-hat
* White → BP **6 kHz Q 1** → env **attack 20 ms**, T60 90 ms; a second BP at 9 kHz Q 3 at −6 dB; random per-hit amplitude ±2 dB. Reverse flag = swept "sh-KA".

### H13 — FM cymbal
* `Fm2`: carrier 1.1 kHz, ratio **1.414** (√2, inharmonic), index 20 constant; add second Fm2 carrier 1.7 kHz ratio 2.76 index 12; sum → HP 3 kHz → env attack 1 ms, T60 900 ms. Tone = index. Classic DX7-style crash.

### H14 — Bit-crushed hat
* H0 bank, then **4-bit quantise and 11 kHz sample-hold**, then BP 4 kHz Q 1, T60 50–300 ms. Aliasing folds the harmonics into a gritty "chiptune" hat.

### H15 — Reverse cymbal
* H6 with reversed envelopes (swell over length 300 ms–2 s, then 5 ms cut) and the HP cutoff rising 2 → 10 kHz over the swell. Cheap and hugely useful.

---------------------------------------------------------------------------

## 5. PERCUSSION (group 3)

All pitched voices take a `pitch` input (Hz or note); the nominal frequency
listed is what index 0-style "808" playback uses.

| # | Name |
|---|------|
| 0 | 808 Low Tom |
| 1 | 808 Mid Tom |
| 2 | 808 High Tom |
| 3 | 909 Tom |
| 4 | 808 Conga |
| 5 | 808 Cowbell |
| 6 | 808 Clave |
| 7 | 808 Maracas / Shaker |
| 8 | Wood block |
| 9 | Chirp (sine sweep) |
|10 | Zap |
|11 | Laser |
|12 | Bongo/tabla-ish (modal) |
|13 | Metal ping (ring-mod) |
|14 | Noise tick / glitch |
|15 | Wonky bubble (reverse chirp + wobble) |

### P0/P1/P2 — 808 toms (Low 90 Hz, Mid 135 Hz, High 185 Hz)
* Resonator (K0 engine) at pitch input; nominal **LT 90 Hz (80–100), MT 135 Hz (120–160), HT 185 Hz (165–220)**; T60 **LT 200 ms, MT 130 ms, HT 100 ms** (Baratatronix / 808 service chart).
* Pitch drop from the diodes: f·(1 + 0.5·e) with e decaying τ = 6 ms (≈ 1.5× at onset; larger than the kick's proportionally).
* Noise: pink (white → 1-pole LP 1 kHz) → LP 800 Hz → env T60 = 1.2 × body T60, level −18 dB ("on the edge of hearing").
* Tone knob = LP on the resonator output 500 Hz–5 kHz; excitation clipped `tanh`. DaisySP: `AnalogBassDrum` with SetFreq(135), SetDecay(0.25), SetAttackFmAmount(0.3), SetTone(0.7) is a perfectly good 808 tom.

### P3 — 909 tom
* Two triangle→rounded oscillators at f and **1.5·f** (a fifth; MusicRadar), f nominal 120 Hz (range 60–300); pitch env 2× → 1× over **100–200 ms** (T60-ish, exp); amp T60 300 ms; second osc at −6 dB decaying faster (T60 150 ms). Noise: 1-bit noise → LP 2 kHz env T60 30 ms at −12 dB. LP 24 dB at 40–60 % (≈ 2–4 kHz) sweeping with the same envelope.

### P4 — 808 conga
* K0 engine with **no noise**, f nominal 200 Hz (LC ~ 200, MC ~ 100? service chart: LC 200 Hz-ish, HC ~ 300 Hz — treat as 100/200/300 selectable via pitch), T60 60–90 ms, pitch drop 1.3× over 4 ms, output `tanh(2.5x)` (congas are hard and clicky).

### P5 — 808 cowbell
* Two squares at **540 Hz and 800 Hz** (Werner; alternatively 587/845 as measured by SOS — ratio 1.44–1.48; keep the ratio, let pitch scale both). 48 % duty.
* Sum → 4th-order BP: two 2-pole BPs centred on the two oscillator frequencies each with Q ≈ 5 (Baratatronix derives a combined centre 881 Hz, Q 4.82, +10 dB peak); this passes the fundamentals and 3rd harmonics only.
* Envelope: attack 0; **two-stage decay**: fast stage T60 ≈ 15 ms down to −8 dB, then tail T60 ≈ 600 ms (service chart "50 ms" is the fast portion; MusicRadar's "≈600 ms" is the audible tail). Implement as env = 0.6·exp(−t/3 ms) + 0.4·exp(−t/90 ms).
* Swing VCA `s/(1+|s|)` — the clipping is part of the timbre. Output HP 300 Hz.
* Tone knob: BP Q 2–8; pitch input scales both oscillators.

### P6 — 808 clave (and rimshot as S6)
* Single resonator **2500 Hz**, T60 **25 ms** (service chart), pinged by 1 ms pulse, no filter, light `tanh(3x)`; pitch input scales. Clave/"woodblock" sweet spot 1.5–3.5 kHz (MusicRadar).

### P7 — 808 maracas / shaker
* White noise → BP **~4–6 kHz Q 1** (the 808 noise band-pass feeds MA & CP) → env attack **3 ms**, T60 **25–35 ms** (service chart). Shaker variant: attack 15 ms, T60 80 ms, BP 7 kHz. Random ±1.5 dB per hit.

### P8 — Wood block
* Resonator at **1.2 kHz** T60 40 ms + second at **2.1 kHz** T60 25 ms (−6 dB), pinged by a 0.5 ms pulse; pitch input scales both; add 3 ms noise click through BP 3 kHz at −10 dB. Tone = ratio of the two.

### P9 — Chirp (short pitched sine sweep)
* Sine **f_start = 4·f_end → f_end** over **30 ms**, exp; f_end from pitch (nominal 400 Hz); amp attack 0.5 ms, T60 60 ms. Reverse gives an upward "boop". Sub-ms cost.

### P10 — Zap
* Sine/tri f **3 kHz → 40 Hz over 120 ms, log-linear curve** (constant semitones/ms: `f = f_end·(f_start/f_end)^(1 − t/T)`), amp attack 0 T60 = T; `tanh(2x)`. Tone = start frequency 1–8 kHz.

### P11 — Laser
* Square/saw (polyBLEP) **6 kHz → 300 Hz over 250 ms, log-linear**, with a fast 40 Hz 20 % square-wave AM (the "pew-pew" stutter) and a 2-pole LP tracking 2× the pitch (Q 3); T60 = length. Tone = AM rate 10–80 Hz.

### P12 — Bongo / tabla-ish (modal)
* 3 resonators at f, **1.59·f, 2.14·f** (approx. circular-membrane modes 0,1 / 1,1 / 2,1 relative ratios 1 : 1.59 : 2.14), gains 1 / 0.5 / 0.3, T60 150 / 90 / 60 ms; pitch drop 1.25× over 8 ms; exciter = 0.5 ms pulse + 2 ms noise. f nominal 250 Hz.

### P13 — Metal ping (ring-mod)
* Square 1.05 kHz × sine 1.49 kHz (ratio √2) → BP 2.5 kHz Q 2 → env T60 300 ms; add sine 3.1 kHz ping T60 120 ms. Pitch scales all. Sounds like a struck pipe / DX bell-perc.

### P14 — Noise tick / glitch
* 1-bit LFSR noise clocked at **pitch × 16** (e.g. 3.2 kHz clock — the low clock rate is the pitch) → env attack 0, T60 12 ms, three retriggers 6 ms apart at decreasing level; HP 1 kHz. Very cheap "digital tick".

### P15 — Wonky bubble
* Chirp run in reverse (f rises 0.25·f → f over the length) with a 12 Hz sine vibrato ±8 % and an amplitude env that swells over 60 % of the length then decays; pitch nominal 600 Hz, length 80–400 ms; feed through a resonant LP (Q 4) whose cutoff follows the pitch ×3. Bubbly "blip-boop".

---------------------------------------------------------------------------

## 6. Engineering sections

### 6.1 Uniform parameter interface
Every voice implements:
```
struct DrumParams {
  float length;   // 0..1 → "length ratio": multiplies ALL time constants
                  // (amp T60, pitch-env τ, noise env, hold, retrigger spacing)
                  // by 2^((length-0.5)*4)  → 0.25× … 4× of the voice's nominal
  float pitch;    // Hz (pitched voices) — unpitched voices treat it as a
                  // ±1-octave scale factor on their internal frequencies
  float tone;     // 0..1 → the voice's most musically useful colour control
                  // (808 kick: LP/click; snare: osc↔noise; hats: HP cutoff;
                  //  cowbell: BP Q; FM: index; zap: start freq)
  float accent;   // 0..1 → level and, where the original did it, timbre
  bool  reverse;  // see below
};
void Trigger(const DrumParams&);  float Process();
```
* **Length ratio**: keep each voice's nominal times in a table and multiply at
  trigger time; do not scale pulse widths (1 ms exciter) or anti-click ramps.
* **Reverse**: envelopes are generated as functions of *normalised time*
  `u = t/L` (L = total length after the ratio). For `reverse`, evaluate every
  envelope at `1 − u`: amplitude becomes a swell, pitch env sweeps upward,
  hold stages move to the end. Because the exciter-based (resonator) voices
  can't literally run backwards, implement reverse for them as: run the
  normal synthesis into the same output but *multiply by* the reversed
  amplitude envelope divided by the forward one (i.e. a gain curve
  `g(u) = envA(1−u)/max(envA(u), ε)` clamped to ≤ +12 dB); for pitch, use the
  reversed pitch envelope directly as the resonator frequency. Finish every
  reversed voice with a fixed 5 ms raised-cosine release so the cut never clicks.
  Re-trigger structures (clap bursts) reverse their burst order.

### 6.2 Level matching
* At build time (or first boot, cached), render every voice with default
  params, accent = 1, length = 0.5, into a scratch buffer, measure
  **peak** and apply `gain = 0.891 / peak` (−1 dBFS) as the voice's static
  trim. Store the 64 trims in flash. Because kicks are peaky but hats are
  RMS-dense, additionally compute short-term RMS over the first 50 ms and
  cap any voice whose 50 ms RMS exceeds the group median by > 6 dB
  (avoid the noise voices being subjectively too loud after peak-normalising).
* Length / tone changes at runtime are *not* re-normalised — that's the
  player's dynamics. Accent maps linearly to 0.5…1.0 gain (plus timbre).
* Sum bus: 16 voices at −1 dBFS peak can clip; run the drum bus through a
  soft clip `x/(1+|x|)` or a 1-knob limiter with 1 ms look-ahead.

### 6.3 CPU cost tiers (48 kHz, Cortex-M7 @ 480 MHz, float, approximate)
| Tier | Content | µs/sample-block of 1 | Examples |
|------|---------|---------------------|----------|
| A (≈ 0.1–0.2 µs/sample, ~1 %) | 1 sine/tri + 1–2 one-pole envs, tanh | K2, K3, K11, P9, P10, P6, S10 |
| B (≈ 0.3–0.5 µs) | 1–2 biquad resonators + noise + Svf | K0, K4, S0, S4, S6, P0–P2, P4, P8, P12 |
| C (≈ 0.5–0.8 µs) | DaisySP Synthetic* models, Fm2 + noise, 909 snare w/ 2 Svf | K1, K7, K8, S1, S9, P3 |
| D (≈ 0.8–1.5 µs) | 6-osc metal bank + 2–3 Svf + swing VCA | H0–H7, H10, H14, P5 |
| E (≈ 1.5–3 µs) | banks with oversampling/polyBLEP, 5-mode snare, ring-mod cymbal with 2 Fm2, gated snare with delay | H6 (2 bands), H13, S5, S13, P13 |
Budgeting: 16 simultaneous voices worst case ≈ 16 × 1.2 µs ≈ 19 µs/sample ≈
92 % of the 20.8 µs sample period — **too much**. Mitigations: (1) share one
MetalBank instance across all hat/cymbal voices (only one bank runs; each hat
voice has its own filters/VCA) — cuts tier D/E to tier B; (2) run the 6
squares as integer phase accumulators with MSB extraction (DaisySP
`SquareNoise` does exactly this: `noise += phase[i] >> 31`) — ~6 adds/sample;
(3) voice-steal: a re-triggered voice restarts in place, never allocates;
(4) precompute `SemitonesToRatio`/`exp` at trigger time, never per sample;
(5) use `arm_sin_f32`/LUT sines. With (1)+(2) a realistic full 16-voice hit
is 6–9 µs/sample, leaving > 50 % for the polysynth + FX.

### 6.4 Anti-click note-on
* **Never hard-reset oscillator phase to 0 on a voice that is still
  sounding.** Options: (a) if the voice's current output |y| > 0.01, apply a
  1–2 ms linear fade-out of the *old* output while the new hit starts (a
  2-sample-history crossfade: keep `old` running for 64 samples); (b) for
  resonator voices, don't reset the state — just inject the new pulse (this
  is what the real 808 does, and DaisySP's `AnalogBassDrum` does not clear its
  resonator on Trigger); (c) for sine/FM voices, start the new note at the
  phase where `sin(φ) = current_output` with matching slope sign — free of
  discontinuity.
* Envelopes with "attack 0": use a 0.25–0.5 ms raised-cosine (12–24 samples)
  attack — inaudible on drums but removes the DC step click. Exceptions: the
  909 click and the 808 pulse are *supposed* to be steps — pass them through
  the click path only, and keep the tonal body's attack ≥ 0.25 ms.
* Noise bursts: start the noise envelope at 0 and ramp over 0.2 ms rather
  than gating at full level; DC-block (HP 20 Hz) after any half-wave/rectifier
  waveshaper.
* Choke groups (CH chokes OH, etc.): release the choked voice with a 3–5 ms
  raised-cosine, never a hard cut.
* Any envelope reaching < −90 dB should be zeroed and the voice marked idle
  (saves CPU and avoids denormals; keep `FZ` flag on the M7 FPU on).

---------------------------------------------------------------------------

## 7. Source list
* Werner et al., "A Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum Circuit", DAFx-14 — https://csclub.uwaterloo.ca/~pbarfuss/dafx14_kurt_james_werner_a_physically_informed_ci.pdf
* Werner et al., "The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital Model", ICMC-14 — https://www.icmc14-smc14.net/images/proceedings/OS24-B10-TheTR-808Cymbal.pdf
* Werner et al., "More Cowbell", ICMC-14 — https://www.academia.edu/7462829/More_Cowbell_a_Physically_Informed_Circuit_Bendable_Digital_Model_of_the_TR_808_Cowbell
* Werner, ChucK 808 snare emulation notes — https://www.tumblr.com/kurtjameswerner/51352144814/chuck-tr-808-emulator-snare-drum-sd-emulation
* Roland TR-808 service notes (voice frequency/decay chart p.14) — https://archive.org/stream/synthmanual-roland-tr-808-service-notes/rolandtr-808servicenotes_djvu.txt
* Baratatronix circuit write-ups: BD https://www.baratatronix.com/blog/808-bd-synthesis ; CY/HH https://www.baratatronix.com/blog/cascadia-808-cymbal-hi-hat-synthesis ; 606 https://www.baratatronix.com/blog/606-cymbal-and-hi-hat-synthesis ; toms https://www.baratatronix.com/blog/808-tom-synthesis ; cowbell BPF https://www.baratatronix.com/blog/808-cowbell-bandpass-filter-frequency ; clap https://www.baratatronix.com/blog/cascadia-808-clap-synthesis ; rimshot https://www.baratatronix.com/blog/808-rimshot ; snare https://www.baratatronix.com/cascadia/cascadia-simplified-808-snare
* Gordon Reid, Sound On Sound Synth Secrets: bass drum https://www.soundonsound.com/techniques/practical-bass-drum-synthesis ; snare https://www.soundonsound.com/techniques/practical-snare-drum-synthesis ; cowbell/claves https://www.soundonsound.com/techniques/synthesizing-cowbells-claves
* Robin Whittle, TR-909 sound mods (BD/SD/HH/HCP circuit details) — https://www.firstpr.com.au/rwi/tr-909/TR-909-Sound-Mods.pdf
* KVR: 909/808 clap emulation (BPF 1140 Hz Q 1.95) — https://www.kvraudio.com/forum/viewtopic.php?p=6510980 ; 808 bridged-T modelling — https://www.kvraudio.com/forum/viewtopic.php?t=418439
* Nord Modular book, percussion chapter — https://cim.mcgill.ca/~clark/nordmodularbook/nm_percussion.html
* Noise Engineering, clap synthesis — https://noiseengineering.us/blogs/loquelic-literitas-the-blog/percussion-synthesis-part-3-the-sound-of-one-synth-clapping/
* MusicRadar, classic analogue drum recipes — https://www.musicradar.com/how-to/how-to-recreate-classic-analogue-drum-sounds-in-your-daw-and-with-hardware
* Perfect Circuit, kick synthesis — https://www.perfectcircuit.com/signal/kick-drum-synthesis
* Mutable Instruments Plaits manual (models 14–16) — https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/ ; Peaks manual — https://pichenettes.github.io/mutable-instruments-documentation/modules/peaks/manual/
* DaisySP sources (Plaits ports) — https://github.com/electro-smith/DaisySP/tree/master/Source/Drums (`analogbassdrum.cpp`, `synthbassdrum.cpp`, `analogsnaredrum.cpp`, `synthsnaredrum.cpp`, `hihat.h/.cpp`)
* Sonic Charge Microtonic manual (osc pitch-mod modes, env ranges 10 ms–10 s) — https://www.manualsdir.com/manuals/349731/sonic-charge-microtonic.html?page=16
* SyntherJack 808 rimshot/clave — https://syntherjack.net/the-perfect-808-rimshot-clave/ ; SDS-V voice board — https://syntherjack.net/sdsv-voice-board/
* Tiptop SD909 manual (binary noise generator) — https://www.tiptopaudio.com/manuals/Tiptop_Audio_SD909_ns.pdf
* Mod Wiggler 808 cymbal ride mod — https://www.modwiggler.com/forum/viewtopic.php?t=266697
