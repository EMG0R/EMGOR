// DEMIURGE hello_patching — SuperCollider STUTTER DELAY.
//
// Stereo stutter delay that re-picks tap time and feedback once per
// beat off the global clock (ch16 CC119 BPM broadcast). Sits between
// cpp_crush and the Csound ringmod/reverb stage.
//
// MIDI map:
//   ch16 CC119 — global BPM broadcast
//   ch 1 CC 60 — dry/wet mix
//   ch 1 CC 61 — feedback nudge

(
s.options.numInputBusChannels_(2);
s.options.numOutputBusChannels_(2);
s.options.sampleRate_(48000);
s.options.blockSize_(64);
s.waitForBoot({

    SynthDef(\stutter, { |wet=0.18, fbNudge=0.0, dtime=0.18, fb=0.12|
        var in, delL, delR, sig, smTime, smFB;
        in = SoundIn.ar([0, 1]);
        smTime = Lag.kr(dtime, 0.08);
        smFB   = Lag.kr((fb + fbNudge).clip(0.0, 0.40), 0.08);
        delL = CombL.ar(in[0], 2.0, smTime, smTime / (1 - smFB).max(0.05) * 4, 1);
        delR = CombL.ar(in[1], 2.0, smTime * 1.5, smTime / (1 - smFB).max(0.05) * 4, 1);
        // Gentle stereo widening
        sig = [ in[0] * (1 - wet) + delL * wet,
                in[1] * (1 - wet) + delR * wet ];
        sig = LeakDC.ar(sig);
        Out.ar(0, sig.tanh * 0.9);
    }).add;

    s.sync;

    ~stutter = Synth(\stutter, addAction: \addToTail);

    // === Shared state ===
    ~bpm = 110;
    ~tick = 0;

    // Fresh random 1/(2^n) subdivision of the beat, each beat.
    ~rollTime = {
        var beatS, divs, d, t;
        beatS = 60 / ~bpm;
        divs = [3, 4, 6, 8, 12, 16];
        d = divs.choose;
        t = beatS / d;
        ~stutter.set(\dtime, t.clip(0.025, 0.35));
        ~stutter.set(\fb,    0.08 + 0.12.rand);
    };

    // === MIDI listener ===
    MIDIClient.init;
    MIDIIn.connectAll;

    // ch16 CC119 (global BPM). SC MIDIdef channels are 0-indexed → 15.
    MIDIdef.cc(\bpm, { |val, num, chan|
        if (chan == 15 and: { num == 119 }) {
            ~bpm = 40 + (val / 127) * 200;
        };
    }, 119);

    // ch1 CC60 — dry/wet
    MIDIdef.cc(\wetMix, { |val, num, chan|
        if (chan == 0 and: { num == 60 }) {
            ~stutter.set(\wet, val / 127);
        };
    }, 60);

    // ch1 CC61 — feedback nudge (± 0.25)
    MIDIdef.cc(\fbNudge, { |val, num, chan|
        if (chan == 0 and: { num == 61 }) {
            ~stutter.set(\fbNudge, (val / 127 - 0.5) * 0.5);
        };
    }, 61);

    // MIDI realtime clock: 24 PPQN. Every 24 ticks = one beat.
    // SC emits MIDI clock as a func event — use MIDIdef.sysrt for 0xF8.
    MIDIdef.sysrt(\clockTick, { |val|
        // val is the sysrt status byte; 8 = clock tick (0xF8 & 0x0F = 8)
        if (val == 8) {
            ~tick = ~tick + 1;
            if ((~tick % 12) == 0) { AppClock.sched(0, { ~rollTime.value; nil }) };
        };
    });

    "sc_stutter.scd ready".postln;
});
)
