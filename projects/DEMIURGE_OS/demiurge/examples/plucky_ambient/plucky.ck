// ~/demiurge/examples/plucky_ambient/plucky.ck
//
// DEMIURGE plucky_ambient test — SUPER plucky self-playing arpeggio.
//
// Used by ~/demiurge/examples/plucky_ambient/live.conf as the
// lead-voice chain. Bright, tight, dry. NO reverb here — the test is
// that this path stays dry while the parallel ambient chain gets
// drowned in Csound reverbsc, and they sum cleanly at demiurge-sink.
//
// Self-generative pentatonic arpeggio in A minor. Listens to the
// shared MIDI bus for ch16 CC119 (global BPM) to lock its pulse to
// demiurge-clock; without the clock it free-runs around 160 BPM.

// === Voice: saw -> bandpass -> tight plucky envelope ===
SawOsc s => BPF f => ADSR e => Gain g => dac;
0.22 => s.gain;
2200 => f.freq;
5.0  => f.Q;
// 3 ms attack, 90 ms decay, 0 sustain, 110 ms release — that's the
// "plucky" signature. Keep it this short; any longer and the test
// starts bleeding into the ambient character we want to isolate.
e.set(3::ms, 90::ms, 0.0, 110::ms);
0.0 => g.gain;

fun float mtof(int m) { return 440.0 * Math.pow(2.0, (m - 69) / 12.0); }

fun void pluck(int midi_note, float vel) {
    mtof(midi_note) => s.freq;
    (mtof(midi_note) * 2.6 + 800) => f.freq;
    (vel / 127.0) * 0.95 => g.gain;
    e.keyOn();
    80::ms => now;
    e.keyOff();
    140::ms => now;
}

// A minor pentatonic across three octaves
[57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84] @=> int scale[];

// === Clock reader — lock pulse to ch16 CC119 ===
140.0 => float bpm;
MidiIn min;
MidiMsg msg;
if (!min.open(0)) {
    <<< "plucky.ck: MIDI open failed — free-running" >>>;
} else {
    <<< "plucky.ck: MIDI open on", min.name() >>>;
}

fun void midiLoop() {
    while (true) {
        min => now;
        while (min.recv(msg)) {
            if ((msg.data1 & 0xF0) == 0xB0 && (msg.data1 & 0x0F) == 15
                && msg.data2 == 119) {
                40.0 + (msg.data3 / 127.0) * 200.0 => bpm;
            }
        }
    }
}
spork ~ midiLoop();

// === Self-generative arpeggio ===
0 => int idx;
while (true) {
    // 16th notes at the current tempo
    (60.0 / bpm / 4.0 * 1000.0)::ms => dur sixteenth;
    scale[idx % scale.cap()] => int n;
    if (Math.random2(0, 4) == 0) n + 12 => n;
    spork ~ pluck(n, 85.0 + Math.random2f(0.0, 35.0));
    1 +=> idx;
    sixteenth => now;
}
