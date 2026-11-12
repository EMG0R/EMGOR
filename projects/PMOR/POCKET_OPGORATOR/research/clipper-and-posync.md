# Research: "The FL Studio clipper" on Daisy Seed, and Pocket Operator sync

Date: 2026-09-14. Two independent findings sections (A: clipper, B: PO sync). URLs cited inline.

---

## TASK A — What "the FL Studio clipper" is, and how to replicate it on a Daisy Seed

### A.1 Which FL Studio thing do people mean?

**Answer: Fruity Soft Clipper (the stock "Fruity Soft Clipper" effect plugin, credited to Didier "Gol" Dambrin).** Every producer-facing source that talks about "the FL clipper sound" means this plugin, not Fruity Limiter and not a master-bus setting:

- Schwabe Digital's *Orange Clip* is marketed as a "mathematically exact" recreation of "the iconic FL clipper", and Sound On Sound identifies that plugin as Fruity Soft Clipper: "recreates none other than Fruity Soft Clipper, a dynamics plug-in included with Image Line's FL Studio". https://www.soundonsound.com/news/orange-clip-schwabe-digital ; Gearspace thread: https://gearspace.com/threads/new-orange-clip-brings-the-iconic-fl-studio-sound-anywhere.1425546/
- Gumroad "Better Clipper" for FL calls itself "an improved version of the legendary soft clipper in FL" (controls: pre-gain, knee, output). https://prodbymaestro.gumroad.com/l/betterclipper
- The Image-Line forum has a thread about Orange Clip as a "soft clipper emulation" ( https://forum.image-line.com/viewtopic.php?t=324012 ) and a proposed "Fruity Soft Clipper 2" ( https://forum.image-line.com/viewtopic.php?p=2073311 ).
- Tutorials on the "Southside / 808 Mafia" blown-out sound and on "soft clipper vs limiter" all center on Fruity Soft Clipper on drum buses / 808s / master: https://www.soundshaper.org/blog/soft-clipper-vs-limiter-fl-studio , https://practicalaudiolab.substack.com/p/how-to-get-that-southside-808-mafia

The other candidates, for completeness:

| Candidate | What it is | Is it "the FL clipper"? |
|---|---|---|
| **Fruity Soft Clipper** | Stock effect. Controls: THRES (threshold, dB, drag on the graph), POST (post gain), output meter, input->output transfer graph. Manual calls it "a CPU-friendly soft limiter" whose "output volume per unit of input decreases after the threshold level" and points to Fruity Limiter for "more transparent clip protection". https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Fruity%20Soft%20Clipper.htm | **Yes.** |
| **Fruity Limiter** | Compressor + limiter + gate with SAT (saturation threshold), CEIL, ATT/REL, CURVE. Its limiter has a look-ahead equal to the compressor attack time ("sets a 'look-ahead' delay for the Limiter section ... see transients before they arrive"; adds latency). Default on the Master in new projects. https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Fruity%20Limiter.htm | No — it is the "clean" alternative. "Fruity Limiter knock" tutorials exist but the harmonic "FL sound" is the Soft Clipper. |
| **Audio settings "clip" option** | Audio Settings: "Hard-clip output at 0 dB (otherwise limiting will be applied)". Affects live playback only (hard clip vs. the OS limiter), not renders. https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_audio.htm . The Mixer manual explicitly says inserts cannot clip (32-bit float) and only the Master/ASIO outputs can. https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer.htm | No — it's a monitoring safety, plain hard clip. |
| **Edison "Clip"** | Edison has no separately documented "soft clip" tool in current manuals; Edison's normalize/limit functions are offline utilities. | No. |

So: **implement Fruity Soft Clipper.**

### A.2 What is publicly known about Fruity Soft Clipper's behaviour

Image-Line has never published the formula. What's observable / reported:

1. **Two regions, one fixed shape.** A Gearspace user comparing against Orange Clip: "two shapes, hard and soft. Threshold controls when the shape starts. The output curve does change, but the function itself does not, just where it starts." (Gearspace p.9: https://gearspace.com/board/new-product-alert-2-older-threads/1425546-new-orange-clip-brings-iconic-fl-studio-sound-anywhere-9.html ). I.e. a linear region up to the threshold, then a fixed knee/curve that runs from the threshold to the ceiling (0 dBFS, since the display maps input->output on a unit square), then flat (hard limit).
2. **The knee is soft and gets softer as THRES goes down.** Measured: "a -2.1 dB knee (when measured) at the default setting and gets suuuuuper soft as you turn the Thres control." "Stays 32 bit at every setting" (no internal truncation). (Gearspace p.11: https://gearspace.com/board/new-product-alert-2-older-threads/1425546-new-orange-clip-brings-iconic-fl-studio-sound-anywhere-11.html )
3. **Threshold display vs. actual onset.** Users mapping displayed threshold to measured onset of gain change: `-48 -> -26, -24 -> -13, -12 -> -6.5, -6 -> -3.3, -3 -> -1.6, -1.5 -> -0.8, 0 -> 0` dB — i.e. the measured onset sits at roughly *half* the displayed threshold in dB, and default (-4.4 dB displayed on both FL and Orange) measures about -2.1 dB. (Gearspace p.10: https://gearspace.com/board/new-product-alert-2-older-threads/1425546-new-orange-clip-brings-iconic-fl-studio-sound-anywhere-10.html ). This "half the dB" relationship is exactly what you get from a curve whose deviation from unity is very gradual near the knee start (a smooth quadratic/tangent knee) — measurement tools register onset only once the deviation exceeds a fraction of a dB.
4. **Defaults / range (from Orange Clip, which mirrors FL):** the Orange knob "controls the knee shape or size"; range soft = -51.8 dB ... default "medium" = **-4.4 dB** ... hard = 0.0 dB (a hard clipper). https://support.schwabedigital.com/hc/en-us/articles/30992825036051-Orange-Clip-What-does-the-Orange-knob-actually-do . Schwabe's guide: it "determines where wave shaping begins, measured downward from the clip ceiling." https://www.schwabedigital.com/orangeclipguide . A user got a null between Orange Clip and FL's default "somewhere in the fine adjustments of -3.9 on the Orange control" (p.9).
5. **Sonic description** (Schwabe, and thread consensus): "inflates low frequencies, flattens/crunches highs, smooths the mid-range"; "rounds off transients while making them hit harder and more thumpy"; "softening in the highs". That is the classic signature of a static, memoryless soft-knee waveshaper with a *large* knee region: low-frequency peaks get mildly compressed and even-ish harmonic energy is added, HF transients get rounded.
6. **No lookahead, no envelope.** The manual lists only THRES and POST and a static input->output graph; there is no attack/release/lookahead, and the Fruity *Limiter* manual is where lookahead is documented. Everything reported (null tests with a static waveshaper, "the function doesn't change, only where it starts") is consistent with a **memoryless per-sample transfer function**. Confirmed-by-absence rather than by an explicit Image-Line statement.
7. **Oversampling:** Fruity Soft Clipper does none (it is billed as "CPU-friendly" and is from the FL 3/4 era; Orange Clip adds linear/min-phase oversampling as its differentiator). The aliasing is arguably part of "the sound" on drums.
8. **Post gain:** plain output multiplier applied *after* the clipper. Because the curve tops out at the ceiling, POST > 0 dB will exceed 0 dBFS (FL floats), so on a fixed-point/DAC output you either hard-clip after POST or keep POST <= 0 dB. SoundShaper's recipe for the "FL" sound: THRES around -8 to -3 dB, POST around -1 dB, drive with pre-gain. https://www.soundshaper.org/blog/soft-clipper-vs-limiter-fl-studio

### A.3 Recommended replica transfer function

Design goals matching the evidence: linear below threshold `t`; a fixed-shape smooth knee from `t` to the ceiling `c` (=1.0); slope 1 at the knee start (so measured onset lands well above the displayed threshold, matching the "half the dB" observation); slope 0 at the ceiling; flat above. A quadratic (parabolic) knee is the simplest curve with exactly those constraints, is C1-continuous, costs a handful of MACs, and produces the "inflated lows / rounded highs" character. Optionally blend toward tanh for a rounder sound.

```
Let t = threshold (linear, 0 < t < 1), c = 1.0 (ceiling), w = c - t (knee width).
For u = |x|:
    u <= t        : y = u
    t < u < t+2w  : y = t + (u - t) - (u - t)^2 / (4w)        // parabola, slope 1 at u=t, slope 0 at u=t+2w, y(t+2w) = t + w = c
    u >= t + 2w   : y = c
y = sign(x) * y * postGain
```

Notes:
- With this parametrisation the ceiling is reached at input `t + 2w = 2 - t`, i.e. above 0 dBFS for any t < 1, which is exactly how FL's graph looks: the curve bends across the whole top of the square and "hard" clipping only occurs on very hot input. If you'd rather have the curve reach the ceiling exactly at input = 1.0 (so the graph is drawn inside the unit square), use `w = (c - t)` and the knee `y = t + w * (2v - v^2)` with `v = (u - t)/w` clamped to [0,1]; slope at knee start is then 2 (a visibly harder onset). The first form better matches the measured "onset ≈ half of displayed threshold" data.
- Default: displayed -4.4 dB → t = 10^(-4.4/20) ≈ 0.603. Map a user "THRES" control to t = 10^(dB/20) with range about -52 dB..0 dB (0 dB = hard clip, matching Orange's range).
- To dial in more "tanh-like" roundness (some listeners describe FL as softer than a parabola at extreme drives), replace the parabola with `y = t + w * tanh((u - t)/w)`. Slope 1 at the knee, asymptotic to c; the difference is only audible on >+6 dB overs. Use a cheap rational tanh: `tanh(z) ≈ z(27+z²)/(27+9z²)` clamped to ±1.
- POST gain: multiply after the curve; if POST > 0 dB, follow with a hard clamp to ±1 before the codec.
- Even-harmonic bias: the FL curve is symmetric (odd-order harmonics dominate). Don't add DC/asymmetry.

### A.4 Lookahead / oversampling on the STM32H7

- **Lookahead: none.** Memoryless. Zero latency — good for an always-on end stage.
- **Oversampling:** FL does not oversample, so a 1x implementation is the most faithful. If you want to tame aliasing on cymbals, 2x with a cheap half-band (e.g. 2-stage polyphase IIR or a 15-tap FIR) is affordable: the H7 at 480 MHz with FPU does a per-sample parabola in ~10 cycles; a 2x half-band pair adds roughly 40-80 cycles/sample, i.e. under 1% of the CPU at 48 kHz stereo. Recommendation: ship 1x (authentic), keep a compile-time 2x option. Antiderivative anti-aliasing (ADAA, first order) is another cheap option for a polynomial curve since the antiderivative of a parabola is a cubic — but it adds a half-sample delay and changes the character slightly; not needed for "the FL sound".
- Denormals/branches: use the branch-light form below; everything is straight-line float math except one `fminf`.

### A.5 Reference C implementation (per-sample, branch-light, float)

```c
// Fruity-Soft-Clipper-style static waveshaper. 48 kHz, no lookahead, no oversampling.
typedef struct {
    float t;        // threshold, linear (default 0.603 = -4.4 dB)
    float inv4w;    // 1 / (4*(1 - t)), precomputed
    float kneeEnd;  // t + 2*(1 - t) = 2 - t
    float post;     // post gain, linear (default 1.0)
} flclip_t;

static inline void flclip_set(flclip_t *s, float thresh_dB, float post_dB) {
    float t = powf(10.f, thresh_dB * 0.05f);
    if (t > 0.999f) t = 0.999f;             // 0 dB -> hard clip
    if (t < 0.0025f) t = 0.0025f;           // ~ -52 dB
    s->t = t;
    s->inv4w = 1.f / (4.f * (1.f - t));
    s->kneeEnd = 2.f - t;
    s->post = powf(10.f, post_dB * 0.05f);
}

static inline float flclip_process(const flclip_t *s, float x) {
    float u = fabsf(x);
    float sgn = copysignf(1.f, x);
    // clamp input into [0, kneeEnd]; above it the curve is flat at 1.0
    u = fminf(u, s->kneeEnd);
    // d = amount above threshold (0 when below) -> parabola is identity when d == 0
    float d = fmaxf(u - s->t, 0.f);
    float y = u - d * d * s->inv4w;          // = t + d - d^2/(4w) when d>0, = u when d==0
    y *= s->post;
    // final safety clamp for the DAC (only bites if post > 0 dB)
    y = fminf(y, 1.f);
    return sgn * y;
}
```

`fabsf/copysignf/fminf/fmaxf` compile to single VABS/VMIN/VMAX-type instructions on Cortex-M7 with `-O2 -ffast-math`, so the inner loop has no data-dependent branches. Process L and R independently (FL's plugin is per-channel, not linked).

### A.6 Three cheap fallback soft-clip curves

1. **Cubic (JOS / "x - x³/3")** — `f(x) = -2/3 for x <= -1; x - x³/3 for |x| <= 1; 2/3 for x >= 1`. Normalise by 1.5 to reach ±1. Only a 3rd harmonic below clip; very cheap. https://ccrma.stanford.edu/~jos/smith-nam/Soft_Clipping.html and https://ccrma.stanford.edu/~jos/pasp/Soft_Clipping.html
2. **Zölzer / DAFX "overdrive" piecewise-quadratic** (as in DAFX and the Baltic-Lab diode write-up): `f(x) = 2x for 0 <= x <= 1/3; (3 - (2 - 3x)²)/3 (= -3x² + 4x - 1/3) for 1/3 < x <= 2/3; 1 for x > 2/3`, odd-symmetric. Linear gain of 2 below 1/3, smooth quadratic knee, hard ceiling. https://baltic-lab.com/2023/08/dsp-diode-clipping-algorithm-for-overdrive-and-distortion-effects/ (also DAFX 2nd ed., Zölzer, Wiley: https://www.wiley.com/en-us/DAFX:+Digital+Audio+Effects,+2nd+Edition-p-9780470665992 )
3. **tanh** — `y = tanh(g*x)/tanh(g)` (normalised so full scale maps to 1) with the rational approximation `tanh(z) ≈ z(27+z²)/(27+9z²)`; the chowdsp SoftClipper is the polynomial cousin (`x - x^N/N` family). https://ccrma.stanford.edu/~jatin/chowdsp/chowdsp_utils/classchowdsp_1_1SoftClipper.html
4. (Bonus) **Airwindows ClipOnly2** — a hard clip at -0.2 dB (`refclip = 0.9549925859`) whose *entry and exit samples* are softened by blending with the previous sample: on crossing the threshold `out = 0.7058208 + last*0.2609148` (constants `0.73908513` and `0.26091487`, `hardness*refclip` and `softness*refclip`). One sample of state, no lookahead, almost free, sounds like a hard clip with the aliasing "click" removed. https://github.com/airwindows/airwindows/blob/master/plugins/WinVST/ClipOnly2/ClipOnly2Proc.cpp

---

## TASK B — Teenage Engineering Pocket Operator sync

### B.1 Official sync mode definitions

From TE's PO FAQ and the PO-33 guide (same text on all PO-xx models): "when sync is used the signal will be split between audio (right) and sync (left)". The mode is selected by holding the key below knob B and pressing BPM; modes SY2-SY5 are only offered when a cable is inserted in line-in. TE FAQ: https://teenage.engineering/guides/po-faq/en ; PO-33 guide: https://teenage.engineering/guides/po-33/en

| Mode | Input L (tip) | Input R (ring) | Output L (tip) | Output R (ring) | Role |
|---|---|---|---|---|---|
| **SY0** | audio L | audio R | audio L | audio R | Solo PO, internal clock (default) |
| **SY1** | audio L | audio R | **sync** | audio (mono mix of in + own) | Master PO (start of chain) |
| **SY2** | **sync** | (sync/ignored) | audio L | audio R | Slave to external device, end of chain |
| **SY3** | **sync** | (sync/ignored) | **sync** | audio (mono) | Slave to external device, middle of chain |
| **SY4** | **sync** | audio (mono) | audio L | audio R | Slave to a PO, end of chain |
| **SY5** | **sync** | audio (mono) | **sync** | audio (mono) | Slave to a PO, middle of chain |

Naming used by TE: SY0 "stereo / stereo", SY1 "stereo / mono+sync", SY2 "sync / stereo", SY3 "sync / mono+sync", SY4 "mono+sync / stereo", SY5 "mono+sync / mono+sync" (input / output). The community cheat-sheet summarises: "3.5 mm stereo · audio: right ch · sync: left ch · master = SY1/SY3 · slave = SY2/SY4". https://dbroesch.github.io/cheat-sheets/po-master-cheatsheet.pdf . The SoundTech table (A=audio, S=sync) lists SY2/SY3 input as "S/S" — i.e. in "sync" input modes the PO treats the input as a sync source only and any audio there is ignored; a Volca mono sync cable puts the pulse on the tip = left. https://www.soundtech.co.uk/musicians-blog/pocket-operator-sync-modes.html and https://www.spongefile.com/pocket-operator-sync-modes-explained/

Chain propagation: a PO in SY3/SY5 re-emits sync on its own output left channel (it does not passively wire input to output); the right channel carries the mono sum of incoming audio plus its own audio, so audio and clock daisy-chain down one TRS cable per hop. POs transmit **no start/stop/reset** — only a running clock; you press play on each unit (SoundTech; also noted on the Daisy forum: "PO's sync signal doesn't have start/stop like MIDI clock" https://community.daisy.audio/t/pocket-operator-sync-example-for-pod/4645 ).

### B.2 Electrical sync signal spec

Official (TE):
- Rate: **2 PPQN — one pulse per 8th note** (every second 16th step). TE's PO-modular guide states outright: "PO sync uses a straight 1/8 note pattern (2ppqn) at 2.6 V" and that its clock input accepts 12.5 ms ... 1.5 s between pulses. https://teenage.engineering/guides/po-modular/16
- Input limit: "use a signal that is maximum 5 Vpp; any stronger signals need to be reduced". https://teenage.engineering/guides/po-faq/en
- Output amplitude: **~2.6 V** peak per TE (PO-modular guide). Independent scope measurement of a PO-12 in SY1: single positive-going pulse, **~1 V above ground, ~2.5 ms wide**, two per beat (125 ms apart at 240 BPM). https://p0k3t0.wordpress.com/2017/10/18/pocket-operator-signals/ (amplitude discrepancy is plausibly load/headphone-out dependent; treat "1-2.6 V positive pulse" as the envelope).
- Input requirements collected by users (Synthstrom Deluge thread, quoting a TE spec sheet): **Vpp < 5 V, > 0.6 V; max pulse width 125 ms; min pulse width ~8 ms (measured)**; format "pulses per eighth note". https://forums.synthstrom.com/discussion/511/syncing-pocket-operators . SoundTech: pulse "between 3-67 ms". The Sonic-Pi experiment confirms the PO triggers reliably on a short, steep, non-negative-going click on the left channel at 8th-note rate, and drops ticks if the click is too soft or too slow-edged. https://medium.com/@heimpogo/lets-play-together-syncing-sonic-pi-with-a-pocket-operator-c52fc55085f2
- Polarity: rising edge, positive pulse, returns to 0 V.

Third-party interop:
- **Korg Volca**: SYNC OUT "sends a 5 V pulse of 15 ms" (3.5 mm mono jack, max out 5 V); SYNC IN max input 20 V, and when a plug is inserted the internal clock is ignored. https://www.manua.ls/korg/volca-beats/manual , https://www.korg.com/us/products/dj/volca_sample/specifications.php . Scope measurement: 5 V, 15 ms, **2 PPQN (8 pulses per 16-step bar)** — one pulse per 8th note. https://www.arduinoslovakia.eu/blog/2021/1/korg-volca-signal-sync-out?lang=en (Manual wording "at the beginning of each step" refers to the sync-clock steps, which are every 2 sequencer steps; newer volcas have a global "sync in/out step" and polarity settings — verify on the specific unit.) So Volca and PO clocks are rate-compatible (Hackaday/little-scale: "KORG volcas & monotrons should also work ... they run at the same rate" https://hackaday.com/2017/02/05/sync-your-pocket-synth-with-ableton/ ) and Volca → PO works directly with a mono or TRS cable into the PO in SY2/SY3 (pocketoperations FAQ: https://pocketoperations.com/pocket-operator-faqs.html ; Elektronauts: https://www.elektronauts.com/t/how-to-sync-te-pocket-operators-with-other-gear/44870 ).
- **Bastl / modular**: "operatorer" converts 24 PPQN to the 2 PPQN "the Pocket Operator expects"; sync pulses are "directly compatible with Arduino GPIO" levels. https://github.com/kleinpa/operatorer . Bastl's ATtiny clock-to-clock converter example exists for the same reason. https://github.com/bastl-instruments/bastlTrinity/blob/master/examples/attinyClockToClockConverter/attinyClockToClockConverter.ino . Deluge users drive POs from a 5 V gate output at 2 PPQN with a mono cable, no divider needed (Synthstrom thread). Eurorack triggers (5-10 V) must be attenuated to <= 5 Vpp for a PO input.
- PO → Volca: PO's ~1-2.6 V pulse is marginal for a Volca SYNC IN that expects a 5 V-ish pulse; users report it usually works but is the fragile direction. Your device should output a full 5 V pulse to be safe with Volcas.

### B.3 Implementation recommendation for a Daisy Seed device (one 3.5 mm input jack, one output jack)

Jack conventions to match POs: input tip = sync (L), ring = audio (R); output tip = sync (L), ring = audio (R). A 5-pin (switched) TRS jack gives you insertion detection on the tip/ring switches — use it the way the PO does (offer sync modes only when a cable is present).

**Clock IN — detecting a PO/Volca pulse on the codec ADC (left input):**
- The Seed's line inputs are AC-coupled and bipolar (approx. ±1 V full scale through the on-board resistor network). A 1-2.6 V PO pulse will be near/over full scale and a 5 V Volca pulse will exceed it: add a series resistor + clamp diodes (or a 2:1 divider) on the tip before the codec so 5 V lands at about ±0.5-0.8 of full scale. AC coupling turns a 2.5-15 ms rectangle into a positive spike followed by a droop/negative undershoot; that's fine because you only need the rising edge.
- Per-sample detector (runs in the audio callback, 48 kHz):
  ```
  hp = one-pole HPF (~20 Hz) to kill DC; s = hp(x_left)
  if (!armed && s > TH_HI)  { armed = 1; onPulse(); refractory = 0.002 s (96 samples); }
  if (armed  && s < TH_LO && refractory expired) armed = 0;   // hysteresis: TH_HI ~ 0.25 FS, TH_LO ~ 0.10 FS
  ```
  Hysteresis + a 2-5 ms refractory window debounces ringing and the AC-coupling undershoot; the PO minimum spacing (16th at 240 BPM = 62.5 ms at 2 PPQN) leaves huge margin. Time-stamp `onPulse()` with the sample counter, then BPM = 60 / (2 * interval_s). Treat each pulse as one 8th note; subdivide to 16ths (or 24 PPQN) internally with a simple PLL or by dividing the last interval. Because POs send no start/stop, latch "running" on the first pulse and time out after ~2 s of silence (matches TE's 1.5 s max interval on PO-modular).
- Alternative: bring the tip to a GPIO through a divider + Schmitt input and use an EXTI interrupt; simpler and precise, but then the jack's tip can't also carry audio, and you lose the ability to accept a soft "click track" from a DAW. The codec route is what the PO itself effectively does and is recommended when the jack must also serve as audio input.

**Clock OUT — generating the pulse:**
- **GPIO (recommended for compatibility):** drive a Seed GPIO (3.3 V) through a small MOSFET or NPN switch from 5 V (VIN/USB 5 V) with a ~470 Ω-1 kΩ series resistor onto the output tip. Pulse width 10-15 ms high (Volca standard is 15 ms, PO accepts 3-125 ms), one pulse per 8th note (2 PPQN), rising edge on the beat. This makes you a valid master for both POs (< 5 Vpp) and Volcas (5 V). If you also want a 24 PPQN/eurorack mode, only the pulse rate changes. Timing: schedule pulses from the audio callback's sample counter (48 samples = 1 ms at 48 kHz; buffer-level jitter at a 48-sample block is ~1 ms, fine) or from a hardware timer slaved to the same clock.
- **Audio DAC (fallback, no extra parts):** write a DC-free "click" to the left DAC channel: a full-scale positive half-cycle followed by return to zero (e.g. 2-3 ms rectangle at +1.0, then 0). The Seed line-out is AC-coupled and swings roughly ±1-1.5 V into a high-Z load, which is enough for a PO (> 0.6 Vpp spec; the PO's own output is ~1-2.6 V), but is *not* reliable for Volca SYNC IN (wants 5 V-ish) and the AC coupling will differentiate long pulses — keep them short (2-5 ms) and steep. The Sonic-Pi/Ableton/PocketSync approaches all use exactly this "audio click on the left channel at 8th-note rate" method with a PO as slave.
- Whichever way you drive the tip, put the mono audio on the ring (right) so a PO downstream in SY4/SY5 can take both from one TRS cable, and keep audio off the tip when a sync mode is active (mirror the SY1/SY3 behaviour).

Sources for this section: TE PO FAQ ( https://teenage.engineering/guides/po-faq/en ), PO-33 guide ( https://teenage.engineering/guides/po-33/en ), PO-modular guide ( https://teenage.engineering/guides/po-modular/16 ), SoundTech ( https://www.soundtech.co.uk/musicians-blog/pocket-operator-sync-modes.html ), Spongefile ( https://www.spongefile.com/pocket-operator-sync-modes-explained/ ), PO cheat-sheet ( https://dbroesch.github.io/cheat-sheets/po-master-cheatsheet.pdf ), p0k3t0 scope measurements ( https://p0k3t0.wordpress.com/2017/10/18/pocket-operator-signals/ ), Synthstrom thread ( https://forums.synthstrom.com/discussion/511/syncing-pocket-operators ), Volca Beats manual ( https://www.manua.ls/korg/volca-beats/manual ), Volca Sample specs ( https://www.korg.com/us/products/dj/volca_sample/specifications.php ), Volca sync scope ( https://www.arduinoslovakia.eu/blog/2021/1/korg-volca-signal-sync-out?lang=en ), Hackaday ( https://hackaday.com/2017/02/05/sync-your-pocket-synth-with-ableton/ ), operatorer ( https://github.com/kleinpa/operatorer ), Daisy PO sync example ( https://community.daisy.audio/t/pocket-operator-sync-example-for-pod/4645 ).
