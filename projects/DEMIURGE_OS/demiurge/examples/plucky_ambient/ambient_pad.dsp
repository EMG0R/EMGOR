// ~/demiurge/examples/plucky_ambient/ambient_pad.dsp
//
// DEMIURGE plucky_ambient test — Faust drone source (NO reverb).
//
// Feeds the ambient chain in ~/demiurge/examples/plucky_ambient/live.conf:
//
//     chain =
//       ~/demiurge/examples/plucky_ambient/ambient_pad.dsp
//       ~/demiurge/examples/hello_patching/csound_reverb.csd
//
// Emits a slow-drifting three-voice chord (root + fifth + perfect
// fourth over root) with gentle vibrato. Deliberately dry — the
// Csound reverbsc stage immediately downstream turns this into the
// "ambient" half of the parallel test. Keep this file reverb-free;
// that isolation is the whole point of the test.

import("stdfaust.lib");

// Base pitch drifts glacially so the drone never sits static.
base_hz = 82.4 + os.osc(0.04) * 2.5;              // E2, ±2.5 Hz LFO
vib     = 1.0 + os.osc(3.7) * 0.004;              // 3.7 Hz, ±0.4% vibrato

// Three detuned voices: root, 5th, 4th up an octave.
v1 = os.osc(base_hz            * vib) * 0.35;
v2 = os.osc(base_hz * 3.0/2.0  * vib) * 0.26;
v3 = os.osc(base_hz * 8.0/3.0  * vib) * 0.18;

// Slow amplitude breath so even the dry source has motion.
breath = 0.72 + os.osc(0.09) * 0.22;

mono = (v1 + v2 + v3) * breath;

// Widen into stereo with a tiny phase offset between channels.
left  = mono * (1.0 + os.osc(0.13) * 0.05);
right = mono * (1.0 + os.osc(0.17) * 0.05);

process = left, right;
