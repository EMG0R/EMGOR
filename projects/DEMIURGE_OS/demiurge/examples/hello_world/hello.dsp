// ~/demiurge/examples/hello_world/hello.dsp
//
// DEMIURGE Faust hello_world — generative stereo drone with evolving
// FM sidebands and slow filter sweeps. Lives as a drone; MIDI note
// messages retune the root, CC1 = FM depth, CC7 = master volume,
// CC119 = master BPM (drives LFO rates so the whole drone glides with
// the global clock).
//
// BUILD (on Pi — MUST use faust2jackconsole, NOT faust2jack or
// faust2jackserver). faust2jack needs GTK (not installed on the Pi),
// and faust2jackserver rejects the `-midi` flag.
//
//   faust2jackconsole -midi ~/demiurge/examples/hello_world/hello.dsp
//
// RUN (stdin must stay open or the console binary exits immediately):
//
//   LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack \
//     tail -f /dev/null | ./hello
//
// JACK ports are 0-indexed: `hello:out_0/out_1` (audio out),
// `hello:in_0/in_1` (audio in), `hello:midi_in_1/midi_out_1` (MIDI).
// Wire midi_in_1 to Midi Through via PipeWire's `Midi-Bridge` node.
//
// MIDI BINDING GOTCHA — do NOT specify a channel in [midi:...] tags.
// On faust2jackconsole the channel numbering is off by one in a way
// that makes `[midi:ctrl 119 15]` receive channel 15 (not ch16), and
// `[midi:ctrl 119 16]` silently breaks the binding entirely. Omit the
// channel and Faust will accept the CC from any channel, which is fine
// here because CC119 is reserved for the clock broadcast anyway.

import("stdfaust.lib");

// === MIDI parameters ===
// freq is set by note-on; falls back to A2 when nothing has played yet.
freq = hslider("freq[midi:keyon 60]", 110, 30, 2000, 0.01) : si.smoo;

// CC1 / CC7 / CC119 — no channel specifier (see gotcha above)
fmDepth = hslider("fm_depth[midi:ctrl 1]", 0.35, 0, 1, 0.001) : si.smoo;
vol     = hslider("volume[midi:ctrl 7]",   0.8,  0, 1, 0.001) : si.smoo;

// Master BPM: CC119 0..127 -> 40..240 BPM.
bpmRaw  = hslider("bpm[midi:ctrl 119]", (110-40)/200, 0, 1, 0.001) : si.smoo;
bpm     = 40.0 + bpmRaw * 200.0;
beatHz  = bpm / 60.0;
// Slow modulator rates derived from beatHz so the drone glides when
// the clock steers.
slow(cyclesPerBeats) = beatHz / cyclesPerBeats;

// === Oscillators ===
lfo(hz, amp) = os.osc(hz) * amp;

modFreq  = freq * (2 + lfo(slow(14), 0.4));    // 1 cycle / 14 beats
modDepth = 200.0 * fmDepth * (1 + lfo(slow(6), 0.25));

modulator = os.osc(modFreq) * modDepth;
carrier   = os.osc(freq + modulator);

detune = os.osc(freq * 1.007) * 0.5 + os.osc(freq * 0.993) * 0.5;

voice = (carrier * 0.55 + detune * 0.25);

// === Filter sweep, tempo-locked ===
cutoff = (lfo(slow(12), 1200) + 1500) : si.smoo;
resQ   = 4.0 + lfo(slow(5), 2.0);

filtered = fi.resonbp(cutoff, resQ, 1.0, voice);

// === Stereo widening ===
left  = filtered + lfo(slow(10), 0.08);
right = filtered + lfo(slow(8),  0.08);

// === Final gain + soft saturation ===
out(x) = (x * vol * 0.6) : ma.tanh;

process = left : out, right : out;
