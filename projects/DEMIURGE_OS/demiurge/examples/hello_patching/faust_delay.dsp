// ~/demiurge/examples/hello_patching/faust_delay.dsp
//
// DEMIURGE hello_patching — stage 4 of 6: Faust GENERATIVE STUTTER DELAY.
//
// Stereo feedback delay. Delay time is tempo-synced to the global
// clock via ch16 CC119, then RE-SAMPLED ONCE PER BEAT from a random
// 0..1 value into a time range of 1/4..2 beats — every beat the delay
// re-picks its stutter time, creating a sequenced stutter that locks
// to the bar. Feedback is re-rolled per beat across a musical range
// the same way.
//
// MIDI pool: Faust2jackconsole speaks JACK MIDI natively. The launcher
// connects PipeWire's `Midi-Bridge:Midi Through Port-0 (capture)`
// directly to the faust binary's JACK MIDI input port, so every CC
// the shared MIDI pool carries reaches the DSP as if it were on an
// ordinary MIDI input.
//
// CC map:
//   ch16 CC119 — global clock BPM broadcast  (40..240 BPM)
//   ch 1 CC 40 — delay time nudge            (0..1 scales 0.5..1.5x)
//
// PARAMETER (docs/parameters.md): /faust/feedback  (0..1)
//   Faust's native OSC: build with -osc and the slider is addressable as
//   /faust_delay/feedback (float 0..1). The pool's `osc` sink maps pool path
//   /faust/feedback to that address (params/faust.json "address"). It replaces
//   the old CC41 nudge. LIMITATION: Faust's OSC output uses its own address
//   scheme, not `/pout <path> <val>`, so this stage does not report back by
//   itself — the UI echo for it comes from the pool's own /param fan-out.
//   TODO(pool): translate Faust -oscout (`/faust_delay/feedback v`) to /pout.
//
// Build on the Pi:
//   faust2jackconsole -midi -osc ~/demiurge/examples/hello_patching/faust_delay.dsp
// Launch the resulting binary `faust_delay` (no extension).

declare name "faust_delay";
import("stdfaust.lib");

bpm_cc  = hslider("bpm[midi:ctrl 119 16]",    0.35, 0, 1, 0.001) : si.smoo;
time_cc = hslider("time_nudge[midi:ctrl 40]", 0.5,  0, 1, 0.001) : si.smoo;
fb_cc   = hslider("feedback",                  0.5,  0, 1, 0.001) : si.smoo;  // pool: /faust/feedback

bpm    = 40.0 + bpm_cc * 200.0;
beat_s = 60.0 / bpm;

// --- Slow bar trigger — once every 4 beats ---
// One impulse per bar. Parameter re-rolls are at bar boundaries so
// the delay field stays coherent across a full musical phrase.
bar_trig = ba.beat(bpm);
beat_trig = bar_trig;

// --- Per-beat random values in 0..1 ---
// no.noise is a white-noise generator in [-1, 1]. Scaling and holding
// with ba.sAndH gives a fresh random value at every beat trigger that
// stays constant until the next beat.
rnd_time = ba.sAndH(beat_trig, no.noise * 0.5 + 0.5);
rnd_fb   = ba.sAndH(beat_trig, no.noise * 0.5 + 0.5);

// Map per-bar randoms to musical ranges.
// Time: 0.5 .. 3.0 beats (quadratic curve favours the ~1-beat region).
time_held_raw = 0.125 + rnd_time * rnd_time * 0.5;
// Feedback: 0.06..0.18 — short tails so individual octaves stay audible.
// Was 0.30..0.55 which caused 5–8s accumulation blurring all pitches.
fb_held_raw   = 0.06 + rnd_fb * 0.12;

// --- Final delay time + feedback ---
// Smooth the held values so the transitions between beats don't click.
nudge    = 0.5 + time_cc;
del_s    = (beat_s * time_held_raw * nudge) : si.smoo;
del_samp = del_s * ma.SR : max(64.0);

fb       = ((fb_held_raw * (0.7 + fb_cc * 0.4)) : min(0.40)) : si.smoo;

MAXDEL = 96000;  // ~2 sec @ 48 kHz
INTRP  = 1024;

// Feedback delay: (+ ~ *(fb)) reads prior output * fb and adds to input,
// then writes result into a smooth delay line. Result is the wet signal.
fbDelay(x) = x : (+ : de.sdelay(MAXDEL, INTRP, del_samp)) ~ *(fb);

// Per-channel: dry + wet — reduced wet to cut accumulation further
stage(x) = x * 1.0 + fbDelay(x) * 0.18;

process = par(i, 2, stage) : par(i, 2, ma.tanh);
