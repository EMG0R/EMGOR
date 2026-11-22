// hello_nonlinear_daylight.scd  --  SuperCollider
// Minimal generative example: slow, sparse, Lydian, one voice.
// Three 0..1 params, same names in every language:
//   nld/density  event rate        nld/tone  brightness / cutoff
//   nld/length   note length
//
// Params come through the pool hook ($DEMIURGE_RESOURCES/params/supercollider/DemiurgeParams.scd):
//   /nld-sc/density  /nld-sc/tone  /nld-sc/length   (0..1, manifest params/nld-sc.json, port 9000)
// The pool writes `/p <path> <val>` to DEMIURGE_PARAM_PORT; this file answers with /pout on
// every change. The hook listens with thisProcess.openUDPPort, so no server is needed for the
// pool link: DEMIURGE_NO_AUDIO=1 skips scsynth entirely (offline check).
// UI: web/examples/nonlinear_daylight.html?stage=sc

(
var res = ("DEMIURGE_RESOURCES".getenv ? "/opt/demiurge/resources");
var noAudio = "DEMIURGE_NO_AUDIO".getenv.notNil;

(res +/+ "params/supercollider/DemiurgeParams.scd").load;
~dparamInit.("nld-sc", 0);                       // 0 -> env DEMIURGE_PARAM_PORT
~dparam.("/nld-sc/density", 0.3);
~dparam.("/nld-sc/tone", 0.5);
~dparam.("/nld-sc/length", 0.5);
~nld = (density: 0.3, tone: 0.5, length: 0.5);   // live param state, all 0..1

// Poll the hook at 20 Hz, echo changes to the pool.
Routine({
    var last = (density: -1, tone: -1, length: -1);
    inf.do {
        [\density, \tone, \length].do { |k|
            var v = ~dparamVal.("/nld-sc/%".format(k));
            ~nld[k] = v;
            if((v - last[k]).abs > 0.001) {
                last[k] = v;
                ~dpout.("/nld-sc/%".format(k), v);
            };
        };
        0.05.wait;
    };
}).play(AppClock);

if(noAudio.not) { s.waitForBoot {
    SynthDef(\nldVoice, { |out = 0, freq = 440, amp = 0.15, dur = 4, cutoff = 1500, pan = 0|
        var env = EnvGen.kr(Env.perc(dur * 0.35, dur * 0.65, 1, -3), doneAction: 2);
        var sig = Mix([Saw.ar(freq), Saw.ar(freq * 1.004), SinOsc.ar(freq / 2, 0, 0.6)]);
        sig = LPF.ar(sig, cutoff.clip(80, 12000), 0.8);
        sig = sig * env * amp;
        sig = sig + (CombL.ar(sig, 0.5, 0.37, 3) * 0.25);   // small tail
        Out.ar(out, Pan2.ar(sig, pan));
    }).add;
    s.sync;

    // Generative loop: re-reads params every event, so changes are live.
    ~nldTask.stop;
    ~nldTask = Task({
        var scale = Scale.lydian.degrees;           // C Lydian: 0 2 4 6 7 9 11
        var deg = 2, oct = 5;
        loop {
            var d = ~nld[\density], t = ~nld[\tone], l = ~nld[\length];
            var len = l.linexp(0, 1, 0.8, 10);              // seconds
            var wait = d.linexp(0, 1, 12, 0.6) * rrand(0.7, 1.3);
            deg = (deg + [-2, -1, 0, 1, 2, 3].choose).clip(0, 13);
            if(0.8.coin) {                                   // sparse: rests happen
                Synth(\nldVoice, [
                    \freq, (scale.wrapAt(deg) + (oct + (deg div: 7) - 1 * 12)).midicps,
                    \dur, len,
                    \cutoff, t.linexp(0, 1, 300, 6000),
                    \amp, 0.12,
                    \pan, rrand(-0.5, 0.5)
                ]);
            };
            wait.wait;
        };
    }).play(SystemClock);

    "hello_nonlinear_daylight: params via the pool hook".postln;
} };
)
