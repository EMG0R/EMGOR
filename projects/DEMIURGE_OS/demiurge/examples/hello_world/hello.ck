// ~/demiurge/examples/hello_world/hello.ck
//
// DEMIURGE ChucK hello_world — fast generative arpeggio instrument.
//
// Reads notes and CC from the shared MIDI bus (Midi Through, port 0).
// Without a controller it generatively picks pentatonic notes on its own.
// With a controller: notes play live, CC1 morphs filter, CC7 = volume.
//
// Drop this path into a `chain =` block in ~/demiurge/live.conf to run it.

// === Voices ===
// A small bank of bandpass-filtered saw voices for a bright, chiming tone.
class Voice {
    SawOsc s => BPF f => ADSR e => Gain g => dac;
    0.18 => s.gain;
    1400 => f.freq;
    3.2  => f.Q;
    e.set(4::ms, 180::ms, 0.0, 200::ms);
    0.0 => g.gain;

    fun void play(float hz, float vel) {
        hz => s.freq;
        // Filter follows pitch, adds shimmer
        (hz * 3.2 + 900) => f.freq;
        (vel / 127.0) * 0.9 => g.gain;
        e.keyOn();
        200::ms => now;
        e.keyOff();
        220::ms => now;
    }
}

Voice v;

// === Shared state (nudged by CC) ===
0.5 => float morph;     // CC1: 0..1 morphs filter Q and color
0.8 => float vol;       // CC7

// Pentatonic scale in Hz (A minor pent starting at MIDI 57)
[57, 60, 62, 64, 67, 69, 72, 74, 76, 79] @=> int scale[];

fun float mtof(int m) { return 440.0 * Math.pow(2.0, (m - 69) / 12.0); }

// === MIDI in (shared bus, port 0 = Midi Through) ===
MidiIn min;
MidiMsg msg;

if (!min.open(0)) {
    <<< "hello.ck: could not open MIDI port 0 — running self-generative" >>>;
} else {
    <<< "hello.ck: MIDI open on", min.name() >>>;
}

// Background MIDI handler
fun void midiLoop() {
    while (true) {
        min => now;
        while (min.recv(msg)) {
            // Note on
            if ((msg.data1 & 0xF0) == 0x90 && msg.data3 > 0) {
                spork ~ v.play(mtof(msg.data2), msg.data3 $ float);
            }
            // CC
            if ((msg.data1 & 0xF0) == 0xB0) {
                if (msg.data2 == 1)  (msg.data3 / 127.0) => morph;
                if (msg.data2 == 7)  (msg.data3 / 127.0) => vol;
            }
        }
    }
}
spork ~ midiLoop();

// === Self-generative arpeggio when nothing is playing ===
// Runs continuously; overlaps harmlessly with MIDI-triggered notes.
0 => int idx;
while (true) {
    // Filter Q + cutoff morphs slowly
    (2.0 + morph * 8.0) => v.f.Q;
    // Wrap index through scale with occasional skips
    scale[idx % scale.cap()] => int n;
    // Random octave shift for variation
    if (Math.random2(0, 5) == 0) n + 12 => n;
    spork ~ v.play(mtof(n), 80.0 + Math.random2f(0.0, 30.0));
    1 +=> idx;
    // Tempo wiggles generatively around ~160 bpm arpeggiated 16ths
    (90 + Math.random2(0, 30))::ms => now;
}
