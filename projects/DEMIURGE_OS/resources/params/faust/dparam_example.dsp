declare name "dparam_example";
declare options "[osc:on]";
import("stdfaust.lib");

// Demiurge parameter pool conventions for Faust (see README.md). Every control is a pool
// parameter /<stage>/<label>, a 0..1 float on the pool wire.
//
//  1. [osc:/addr 0 1]  Faust's own OSC alias: the engine accepts 0..1 at /addr and maps it
//     LINEARLY onto the slider's range itself. Nothing for the pool to denormalize.
//  2. no alias         Faust's default address /<dsp name>/<label> takes NATIVE units; the
//     pool's osc sink denormalizes 0..1 -> native from the manifest's "native" {min,max,curve}.
//     Use this when you need an exponential range: [scale:log] -> curve "exp".
cutoff = hslider("cutoff[osc:/cutoff 0 1][unit:Hz]", 1000, 40, 12000, 0.01);  // (1) linear alias
freq   = hslider("freq[scale:log][unit:Hz]", 220, 20, 2000, 0.01);            // (2) exp, pool-denormalized
res    = hslider("res", 0.3, 0, 1, 0.001);                                    // (2) native 0..1

process = os.sawtooth(freq) : fi.resonlp(cutoff, 1 + res * 10, 0.5) <: (_, _);
