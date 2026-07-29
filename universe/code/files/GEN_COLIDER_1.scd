// Synthesis Section
(
SynthDef(\fmSynth, { |out=0, freq=440, modRatio=2, modIndex=1, amp=0.1, ampAtk=0.01, ampDec=0.01, modAtk=0.01, modDec=0.01|
    var modEnv, ampEnv, modFreq, modSig, carSig;
    modEnv = EnvGen.kr(Env.perc(modAtk, modDec), doneAction: 2);
    ampEnv = EnvGen.kr(Env.perc(ampAtk, ampDec));
    modFreq = freq * modRatio;
    modSig = SinOsc.ar(modFreq, 0, modIndex * modEnv * freq);
    carSig = SinOsc.ar(freq + modSig) * ampEnv * amp;
    Out.ar(out, carSig ! 2);
}).add;

~bpm = 170;
~secondsPerBeat = 60 / ~bpm;
~masterProbability = 0.4;
~masterOffset = -1;

~notes = [60, 62, 64, 65, 67, 69, 71, 66];
~weights = [1, 1, 1, 0.5, 1, 1, 1, 0].normalizeSum;

~createNotePattern = {
    Pbind(
        \instrument, \fmSynth,
        \midinote, Pfunc {
            var baseNote = Pwrand(~notes, ~weights, 1).asStream.next + ~masterOffset;
            var octaveOffset = Pwrand([0, 12, -12, 24, -24, 36, -36], [0.1, 0.2, 0.3, 0.02, 0.2, 0.001, 0.2].normalizeSum, 1).asStream.next;
            baseNote + octaveOffset;
        },
        \modRatio, Pfunc {
            var baseRatio = Pwhite(0, 7).asStream.next;
            if (0.1.coin) {
                baseRatio = baseRatio + 0.25;
            };
            baseRatio;
        },
        \modIndex, Pwhite(1, 4),
        \ampAtk, Pwrand([0, 0], [0.5, 0.5], inf),
        \ampDec, Pwrand([1.2, 2.5], [0.5, 0.5], inf),
        \modAtk, Pwrand([0, 0.6], [0.5, 0.5], inf),
        \modDec, Pwrand([0.3, 1.1], [0.5, 0.5], inf),
        \dur, Pfunc { ~secondsPerBeat },
        \amp, Pfunc { ~masterProbability.coin.if(0.1, Rest(0)) }
    )
};

~notePatterns = Pn(~createNotePattern.value, inf);
~notePatterns.play(TempoClock(~bpm / 60));
)

// Drum Section: Kick and Snare
(
s.waitForBoot {
    ~kickBuf = Buffer.read(s, "/Users/emgor/Desktop/EMGOR_SYNTH/_samps/KICK.wav");
    ~snareBuf = Buffer.read(s, "/Users/emgor/Desktop/EMGOR_SYNTH/_samps/SNARE.wav");

    SynthDef(\drumPlayer, { |out=0, bufnum, amp=1|
        var sig;
        sig = PlayBuf.ar(2, bufnum, BufRateScale.kr(bufnum), doneAction: 2);
        Out.ar(out, sig * amp);
    }).add;

    ~bpm = 170;
    ~secondsPerBeat = 60 / ~bpm;

    ~kickPattern = Pbind(
        \instrument, \drumPlayer,
        \bufnum, ~kickBuf.bufnum,
        \dur, 4 * ~secondsPerBeat,
        \amp, 0.25
    );

    ~snarePattern = Pbind(
        \instrument, \drumPlayer,
        \bufnum, ~snareBuf.bufnum,
        \dur, 4 * ~secondsPerBeat,
        \amp, Pseq([0, 1], inf) * 0.2
    );

    Ppar([~kickPattern]).play(TempoClock(~bpm / 60));
};
)

