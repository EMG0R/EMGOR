// ~/demiurge/examples/hello_patching/faust_crush.dsp
//
// DEMIURGE hello_patching — stage 4 of 5: Faust BITCRUSHER / SR REDUCTION.
//
// Takes stereo in from stage 3 (SC granular), crushes bits and sample
// rate in a generative on/off pattern, outputs to stage 5 (Csound reverb).
//
// CC40 = crush amount (bit depth, 16 clean → 3 gnarly)
// CC41 = crush rate   (sample rate divider, 1 clean → 32 gnarly)
//
// Self-toggles between "crushed" and "clean" every few seconds using
// an internal LFO so the effect audibly comes and goes.
//
// Build on the Pi:
//   faust2jackconsole ~/demiurge/examples/hello_patching/faust_crush.dsp
// Launch the resulting binary `faust_crush` (no extension).

import("stdfaust.lib");

// === CC inputs ===
crushCC = hslider("crush_bits[midi:ctrl 40]", 0.35, 0, 1, 0.001) : si.smoo;
rateCC  = hslider("crush_rate[midi:ctrl 41]", 0.35, 0, 1, 0.001) : si.smoo;

// === Self-generative wet/dry LFO — flips roughly every 3–5 seconds ===
// A slow square-ish wave so you hear the effect punch in and out
flip = os.lf_squarewave(0.25) * 0.5 + 0.5;  // 0 or 1, ~4 sec period

// Bit depth: CC-driven, multiplied by self-generative flip so the
// crush effectively disables itself periodically
bits = 16 - (crushCC * 13 * flip);

// Sample rate divider: CC-driven, modulated by flip
srMul = 1 + (rateCC * 31 * flip);

// === Bitcrush + SR reduce per channel ===
crush(x) = sign * floor(abs(x) * levels + 0.5) / levels
with {
    levels = pow(2, bits - 1) - 1;
    sign   = (x >= 0) * 2 - 1;
};

// Sample-rate reduction: sample-and-hold gated by a periodic trigger
// Counter that wraps at int(srMul), fires trig when it hits 0.
srReduce(x) = ba.sAndH(trig, x)
with {
    n       = int(srMul) : max(1);
    counter = (+(1) : %(n)) ~ _;
    trig    = counter == 0;
};

stage(x) = x : crush : srReduce;

// === Master mix + safety clip ===
volume = hslider("volume[midi:ctrl 7]", 0.9, 0, 1, 0.001) : si.smoo;

out(x) = x * volume : ma.tanh;

process = par(i, 2, stage : out);
