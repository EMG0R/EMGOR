// ~/demiurge/examples/hello_world/hello.scd
//
// DEMIURGE SuperCollider hello_world — granular pad + generative melodic
// blips. Self-plays generatively. Tempo-locks to the master clock via
// ch16 CC119 (demiurge-clock BPM broadcast).
//
// MIDI in  : shared ALSA seq "Midi Through" (MIDIIn.connectAll).
// CC map   : ch1 CC1 = grain density, ch1 CC7 = volume,
//            ch16 CC119 = master BPM. SC channels are 0-indexed, so
//            ch16 == chan 15 in MIDIdef callbacks.
// Audio    : JACK client `SuperCollider` — drop this .scd path into a
//            `chain =` block in ~/demiurge/live.conf, the launcher links
//            SuperCollider's output into the chain terminating at
//            demiurge-sink.
//
// Launch (on Pi, headless — sclang needs an offscreen Qt or it aborts
// with `qt.qpa.xcb: could not connect to display`):
//
//   QT_QPA_PLATFORM=offscreen \
//   LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack \
//     sclang ~/demiurge/examples/hello_world/hello.scd

(
s.options.numInputBusChannels_(2);
s.options.numOutputBusChannels_(2);
s.options.sampleRate_(48000);
s.options.blockSize_(64);
s.waitForBoot({

    // === shared state ===
    ~morph   = 0.5;   // CC1 grain density morph
    ~volume  = 0.85;  // CC7
    ~bpm     = 110;   // ch16 CC119 master BPM broadcast

    SynthDef(\pad, { |out=0, freq=220, amp=0.2, gate=1|
        var sig, env, detune, grain;
        env = EnvGen.kr(Env.adsr(0.8, 1.2, 0.7, 1.5), gate, doneAction: 2);
        detune = LFNoise2.kr(0.3!6).range(-0.03, 0.03);
        grain = Mix(Saw.ar(freq * (1 + detune)) * 0.12);
        grain = RLPF.ar(grain, LFNoise1.kr(0.2).range(800, 3200), 0.4);
        sig = grain * env * amp;
        Out.ar(out, sig!2);
    }).add;

    SynthDef(\blip, { |out=0, freq=440, amp=0.3, dur=0.18|
        var sig, env;
        env = EnvGen.kr(Env.perc(0.002, dur, 1, -4), doneAction: 2);
        sig = SinOsc.ar(freq) * 0.4
            + Pulse.ar(freq * 2.01, 0.3) * 0.15;
        sig = HPF.ar(sig, 200);
        sig = sig * env * amp;
        Out.ar(out, sig!2);
    }).add;

    s.sync;

    // === self-generative pad drone ===
    ~pad = Synth(\pad, [\freq, 110, \amp, 0.18]);

    // === self-generative melody, tempo-locked to master BPM ===
    ~scale = [0, 3, 5, 7, 10];
    ~root  = 60;
    ~pidx  = 0;

    ~melody = Routine({
        inf.do { |i|
            var note, freq, beatsec, dur;
            ~pidx = ~pidx + [-1, 0, 1, 2].choose;
            note = ~root + ~scale.wrapAt(~pidx) + (12 * (i.mod(7) < 2).asInteger);
            freq = note.midicps;
            beatsec = 60 / ~bpm;
            dur = beatsec / [2, 3, 4, 6].choose;  // 8ths to 24ths
            Synth(\blip, [\freq, freq, \amp, ~volume * 0.35 * (~morph + 0.3), \dur, dur]);
            dur.wait;
        };
    }).play;

    // === MIDI from shared bus ===
    MIDIClient.init;
    MIDIIn.connectAll;

    MIDIdef.noteOn(\hw_note, { |vel, nn|
        Synth(\blip, [
            \freq, nn.midicps,
            \amp,  ~volume * (vel / 127) * 0.7,
            \dur,  0.3
        ]);
    });

    // CC router — SC MIDIdef.cc gives (val, num, chan). Channels are
    // 0-indexed so ch16 == chan 15.
    MIDIdef.cc(\hw_cc, { |val, num, chan|
        if (chan == 0 and: { num == 1 }) { ~morph  = val / 127 };
        if (chan == 0 and: { num == 7 }) { ~volume = val / 127 };
        if (chan == 15 and: { num == 119 }) { ~bpm = 40 + (val / 127 * 200) };
    });

    "hello.scd ready — self-generative, MIDI live, BPM synced".postln;
});
)
