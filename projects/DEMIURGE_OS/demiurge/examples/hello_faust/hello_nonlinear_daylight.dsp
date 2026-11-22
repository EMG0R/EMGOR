// hello nonlinear daylight (Faust): one slow, sparse, Lydian voice (C Lydian).
// Three 0..1 params, identical names in every language: nld/density, nld/tone, nld/length.
// With `declare name "nld"` and the faust OSC layer (-osc), these are exposed as
// /nld/density, /nld/tone, /nld/length. Native ranges are applied below (pool wire is 0..1).
// Compiles with faust and answers a real /p -> /pout round trip through the reference arch
// (testing/stock-check/run.sh); not auditioned. UI: web/examples/nonlinear_daylight.html?stage=faust
declare name "nld";

import("stdfaust.lib");

density = hslider("density", 0.3, 0, 1, 0.001) : si.smoo;
tone    = hslider("tone",    0.5, 0, 1, 0.001) : si.smoo;
length  = hslider("length",  0.5, 0, 1, 0.001);

// event rate: ~12 s down to 0.6 s between notes (exponential)
period = 12 * pow(0.6 / 12, density);
trig   = os.lf_imptrain(1.0 / period);

// random Lydian degree (C Lydian over two octaves), latched on each trigger
scale  = waveform{0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19};
rnd    = (no.noise * 0.5 + 0.5) : ba.sAndH(trig);
idx    = int(rnd * 12) : min(11) : max(0);
midi   = 60 + rdtable(scale, idx);
freq   = ba.midikey2hz(midi);

// note length 0.8..10 s
rel    = 0.8 * pow(10 / 0.8, length);
gate   = trig : ba.pulsen(int(ma.SR * 0.01));
env    = en.ar(0.15, rel, gate);

// brightness: low-pass cutoff 300..6000 Hz (exponential)
fc     = 300 * pow(6000 / 300, tone);

voice  = (os.triangle(freq) + 0.3 * os.osc(freq * 2)) * env * 0.25 : fi.lowpass(2, fc);

process = voice <: _, _;
