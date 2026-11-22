// ~/demiurge/examples/hello_patching/sc_grain.scd
//
// DEMIURGE hello_patching — stage 3 of 5: SuperCollider GRANULAR cloud.
//
// Takes stereo in from stage 2 (Pd gate), granulates it, outputs to
// stage 4 (Faust crush). Grains evolve generatively — density and
// pitch shift drift on their own LFOs, overridable by CC30/CC31.
//
// PARAMETER (docs/parameters.md): /sc/density (0..1 -> 6..90 grains/s).
//   Driven from the Demiurge UI through the pool and echoed back via /pout.
//   Replaces the old CC30 density control (CC31 shift stays on MIDI).
//   The file loads the hook itself from
//     $DEMIURGE_RESOURCES/params/supercollider/DemiurgeParams.scd
//   (sclang runs one file, so the launcher cannot prepend it). It defines
//   ~dparamInit, ~dparam.(path, dflt), ~dparamVal.(path) and ~dpout.(path, v).
//   Manifest: params/sc.json (stage 3 -> port 9003).
//
// Self-toggles a "big grain" vs "tiny grain" mode every ~6 seconds
// so you can hear the effect coming and going.

(
// --- pool parameter hook (loaded from $DEMIURGE_RESOURCES, no server needed) ---
~dres = "DEMIURGE_RESOURCES".getenv ? "/opt/demiurge/resources";
(~dres +/+ "params/supercollider/DemiurgeParams.scd").load;
~dparamInit.("sc", 0);                 // 0 -> env DEMIURGE_PARAM_PORT (stage 3 -> 9003)
// No server yet, so ~dparam hands back a Function; ~dparamVal is always valid.
~dens = ~dparam.("/sc/density", 0.5);
// Polled at 20 Hz, echoed to the pool on change. Runs whether or not the
// server booted (~gran is nil until it has), so the pool link is testable
// offline: DEMIURGE_NO_AUDIO=1 skips scsynth entirely (used by check.sh).
Routine({
    var last = -1;
    inf.do {
        var v = ~dparamVal.("/sc/density");
        if ((v - last).abs > 0.001) {
            last = v;
            ~gran !? { |g| g.set(\density, v) };
            ~dpout.("/sc/density", v);
        };
        0.05.wait;
    };
}).play(AppClock);

~noAudio = "DEMIURGE_NO_AUDIO".getenv.notNil;
s.options.numInputBusChannels_(2);
s.options.numOutputBusChannels_(2);
s.options.sampleRate_(48000);
s.options.blockSize_(64);
if(~noAudio.not) { s.waitForBoot({

    // === running capture buffer (2 seconds stereo) — allocate FIRST ===
    ~buf = Buffer.alloc(s, s.sampleRate * 2, 2);
    s.sync;

    // Capture synth — live input into rolling buffer
    SynthDef(\capture, { |bufnum|
        var in = SoundIn.ar([0, 1]);
        RecordBuf.ar(in, bufnum, loop: 1);
    }).add;

    // Granular synth — reads from the capture buffer.
    // Pass bufnum via args so we never capture a nil reference.
    SynthDef(\granular, { |bufnum, density=0.5, shift=0.5, wet=1.0|
        var in, grains, sig, trig, dens, rate, pos;

        in = SoundIn.ar([0, 1]);

        dens = density.linlin(0, 1, 6, 90)
             + LFNoise1.kr(0.2).range(-8, 8);

        rate = shift.linlin(0, 1, 0.5, 2.0);

        trig = Dust.ar(dens);

        pos = LFNoise1.kr(0.3).range(0, BufDur.kr(bufnum));

        grains = Mix([
            GrainBuf.ar(2, trig, 0.08, bufnum, rate, pos, 2, 0, -1, 0.2),
            GrainBuf.ar(2, trig, 0.12, bufnum, rate * 1.005, pos + 0.001, 2, 0, -1, 0.2)
        ]);

        // Always keep dry bleed so the chain never fully mutes.
        // wet controls the amount of grain layer on top of a 0.6 dry floor.
        sig = (in * 0.55) + (grains * wet * 1.2);
        sig = LeakDC.ar(sig);
        sig = sig.tanh * 0.85;

        Out.ar(0, sig);
    }).add;

    s.sync;

    Synth(\capture,  [\bufnum, ~buf], addAction: \addToHead);
    ~gran = Synth(\granular, [\bufnum, ~buf, \density, ~dparamVal.("/sc/density"), \shift, 0.5, \wet, 1.0], addAction: \addToTail);

    // === MIDI bus (CC31 shift) ===
    MIDIClient.init;
    MIDIIn.connectAll;

    MIDIdef.cc(\grain_shift, { |val, num|
        ~gran.set(\shift, val / 127);
    }, ccNum: 31);

    // === generative mode flip (wet on / wet off every 4..8s) ===
    Routine({
        inf.do { |i|
            var dur, wet;
            dur = rrand(4.0, 8.0);
            // wet stays in a generous range so the grain layer always adds color
            wet = if ((i % 2) == 0) { 0.9 } { 0.5 };
            ~gran.set(\wet, wet);
            dur.wait;
        };
    }).play(AppClock);

    "sc_grain.scd ready — granular cloud running".postln;
}); };
)
