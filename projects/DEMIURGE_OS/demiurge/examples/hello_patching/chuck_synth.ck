// DEMIURGE hello_patching — ChucK pure-sine pad synth.
//
// Polyphonic sine pad. Generative attack (mostly 300ms–1.2s, occasionally
// fast), hold 1–3 beats, release 1.5–4s. 32 voices for overlap without
// stealing. Gain stack tuned so 15+ simultaneous voices stay below 0dBFS.
//
// MIDI map:
//   ch16 CC119 — global BPM broadcast
//   ch 1 CC  7 — master volume
//   ch 1 CC 91 — reverb mix
//
// PARAMETER (docs/parameters.md): /chuck/chorus (0..1 -> chorus mix 0..0.8).
//   Driven from the Demiurge UI through the pool and echoed back via /pout.
//   Replaces the old ch1 CC1 chorus control (one mechanism per job).
//   Needs the hook class loaded first:
//     chuck $DEMIURGE_RESOURCES/params/chuck/DemiurgeParams.ck chuck_synth.ck
//   Manifest: params/chuck.json (stage 1 -> port 9001; env
//   DEMIURGE_PARAM_PORT overrides).

// === FX chain ===
Gain synth_bus => Chorus chorus => JCRev reverb => Gain master => dac;
0.38 => synth_bus.gain;
0.22 => chorus.mix;
0.16 => chorus.modFreq;
0.28 => chorus.modDepth;
// JCRev at 0.48 mix: present reverb tail without pumping gain
0.48 => reverb.mix;
0.72 => master.gain;

// 32 voices. Per-voice peak = 0.08 * 0.38 * 0.72 = 0.022 each.
// 20 simultaneous voices = ~0.44 at master — clean headroom.
32 => int NVOICES;
SinOsc oscs[NVOICES];
ADSR   envs[NVOICES];
Gain   gains[NVOICES];
for (0 => int i; i < NVOICES; i++) {
    oscs[i] => envs[i] => gains[i] => synth_bus;
    0.0 => gains[i].gain;
    envs[i].set(400::ms, 200::ms, 0.72, 2000::ms);
}
0 => int voiceIdx;

fun void playVoice(int idx, float hz, float vel) {
    hz => oscs[idx].freq;
    (vel / 127.0) * 0.08 => gains[idx].gain;

    // Generative attack: 75% medium-slow (300ms–1.2s), 25% quick (50–280ms)
    float att_ms;
    if (Math.random2f(0.0, 1.0) < 0.25)
        Math.random2f(50.0, 280.0) => att_ms;
    else
        Math.random2f(300.0, 1200.0) => att_ms;

    // Generative release: 1.5–4s
    Math.random2f(1500.0, 4000.0) => float rel_ms;

    envs[idx].set(att_ms::ms, 200::ms, 0.72, rel_ms::ms);
    envs[idx].keyOn();

    // Hold: 1–3 beats (includes attack so note reaches full level)
    float beat_ms;
    60000.0 / bpm => beat_ms;
    Math.random2f(1.0, 3.0) => float hold_beats;
    (att_ms + hold_beats * beat_ms)::ms => dur holdDur;
    holdDur => now;

    envs[idx].keyOff();
    rel_ms::ms => now;
}

0.22 => float chorusMix;
0.48 => float reverbMix;
0.72 => float vol;
110.0 => float bpm;

fun float mtof(int m) { return 440.0 * Math.pow(2.0, (m - 69) / 12.0); }

MidiIn min;
MidiMsg msg;
if (!min.open(0)) {
    <<< "chuck_synth.ck: MIDI open failed" >>>;
} else {
    <<< "chuck_synth.ck: MIDI open on", min.name() >>>;
}

fun void midiLoop() {
    while (true) {
        min => now;
        while (min.recv(msg)) {
            msg.data1 & 0xF0 => int status;
            msg.data1 & 0x0F => int chan;
            if (msg.data1 == 0xF8) { continue; }
            if (status == 0x90 && msg.data3 > 0) {
                (voiceIdx + 1) % NVOICES => voiceIdx;
                spork ~ playVoice(voiceIdx, mtof(msg.data2), msg.data3 $ float);
            }
            if (status == 0xB0) {
                if (chan == 15 && msg.data2 == 119) {
                    40.0 + (msg.data3 / 127.0) * 200.0 => bpm;
                }
                else if (chan == 0 && msg.data2 == 7) {
                    (msg.data3 / 127.0) * 0.72 => vol;
                    vol => master.gain;
                }
                else if (chan == 0 && msg.data2 == 91) {
                    (msg.data3 / 127.0) => reverbMix;
                    reverbMix => reverb.mix;
                }
            }
        }
    }
}

// --- pool parameter: /chuck/chorus ---
DemiurgeParams pool;
9001 => int paramPort;
Std.getenv("DEMIURGE_PARAM_PORT") => string envPort;
if (envPort != "") Std.atoi(envPort) => paramPort;
pool.init("chuck", paramPort);
pool.add("/chuck/chorus", 0.35);

fun void paramLoop() {
    -1.0 => float last;
    while (true) {
        pool.get("/chuck/chorus") => float v;
        if (Math.fabs(v - last) > 0.001) {
            v => last;
            v * 0.8 => chorusMix;
            chorusMix => chorus.mix;
            pool.out("/chuck/chorus", v);
        }
        20::ms => now;
    }
}

spork ~ paramLoop();
spork ~ midiLoop();
while (true) { 1::second => now; }
