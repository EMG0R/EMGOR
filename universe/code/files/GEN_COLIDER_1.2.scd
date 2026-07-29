s.options.memSize = 16384 * 8; // adjust as needed
s.reboot;

(
SynthDef(\fmSynth, {
    |out=0, freq=440, modRatio=2, modIndex=1, amp=0.01,
     ampAtk=0.005, ampDec=0.005,
     modAtk=0.005, modDec=0.005,
     pitchStart = -12, pitchEnd = -12,
     pitchAtkFrac=0.09, pitchDecFrac=0.04, pitchHoldFrac=0.9,
     vibratoRate=0.7, vibratoDepth=0.2,
     chorusDepth=0.005, chorusRate=0.3,
     // New delay params
     delayTime=0.04, delayModRate=0.2, delayModDepth=0.015,
     delayFeedback=0.3, delayMix=0.3,
     // Reverb params
     reverbMix=0.4, reverbTime=2.5, reverbDamp=0.3|

    var totalAmpTime, pitchAtkTime, pitchHoldTime, pitchDecTime,
    pitchEnv, freqMult, modEnv, ampEnv, modFreq, modSig, carSig,
    vibrato, chorusLFO, chorusSig, delayMod, localIn, combined, delayed, delaySig, reverbSig;

    // Some small expansions for pluck-like decays
    var ampDecPluck = ampDec * 4.2;
    var modDecPluck = modDec * 4.2;

    // Total amp envelope duration
    totalAmpTime = ampAtk + ampDecPluck;

    // Break down pitch envelope times
    pitchAtkTime  = (totalAmpTime * pitchAtkFrac).clip(0.01, totalAmpTime);
    pitchHoldTime = (totalAmpTime * pitchHoldFrac).clip(0.01, totalAmpTime);
    pitchDecTime  = (totalAmpTime * pitchDecFrac).clip(0.01, totalAmpTime);

    // Pitch envelope
    pitchEnv = EnvGen.kr(
        Env([pitchStart, 0, 0, pitchEnd],
            [pitchAtkTime, pitchHoldTime, pitchDecTime],
            \lin),
        doneAction:0
    );

    freqMult = 2.pow(pitchEnv / 12);

    // FM mod and amp envelopes
    modEnv = EnvGen.kr(Env.perc(modAtk, modDecPluck), doneAction: 0);
    ampEnv = EnvGen.kr(Env.perc(ampAtk, ampDecPluck), doneAction: 2);

    // Slow vibrato
    vibrato = SinOsc.kr(vibratoRate, 0, vibratoDepth).midiratio;

    modFreq = (freq * freqMult * vibrato) * modRatio;
    modSig  = SinOsc.ar(modFreq, 0, modIndex * modEnv * (freq * freqMult * vibrato));

    // "Dry" base FM sound
    carSig  = SinOsc.ar((freq * freqMult * vibrato) + modSig) * ampEnv * amp;

    // Simple chorus
    chorusLFO = SinOsc.kr(chorusRate, 0, chorusDepth);
    chorusSig = Mix.ar([
        DelayC.ar(carSig, 0.02, chorusLFO + 0.005),
        DelayC.ar(carSig, 0.02, chorusLFO.neg + 0.005),
        carSig
    ]) * 0.6;

    // ===== Interpolated delay w/ feedback & doppler LFO =====
    // Generate the LFO-based delay time
    delayMod = SinOsc.kr(delayModRate, 0, delayModDepth, delayTime);

    // Read any local feedback from the previous sample block
    localIn = LocalIn.ar(2);
    // Combine the chorus signal with the feedback (scaled by delayFeedback)
    combined = (chorusSig!2) + (localIn * delayFeedback);
    // Actual delay line w/ linear interpolation (maxDelay=1.0 for safe margin)
    delayed = DelayL.ar(combined, 1.0, delayMod);
    // Write the delayed signal out to feed next block
    LocalOut.ar(delayed);

    // Blend original vs. delayed according to delayMix
    delaySig = XFade2.ar(chorusSig, delayed, (delayMix * 2) - 1);

    // Reverb after the delay
    reverbSig = FreeVerb.ar(delaySig, reverbMix, reverbTime, reverbDamp);

    // Final output
    Out.ar(out, reverbSig ! 2);
}).add;

// For demo pattern
~bpm = 170;
~secondsPerBeat = 240 / ~bpm;
~masterProbability = 0.5;
~masterOffset = -2;
~notes = [60, 62, 64, 65, 67, 69, 71, 66];
~weights = [1, 1, 1, 0.6, 1, 1, 1, 0.1].normalizeSum;

~createNotePattern = {
    Pbind(
        \instrument, \fmSynth,
        \midinote, Pfunc {
            var baseNote = Pwrand(~notes, ~weights, 1).asStream.next + ~masterOffset;
            var octaveOffset = Pwrand(
                [0, 12, -12, 24, -24, 36, -36],
                [0.55, 0.01, 0.4, 0, 0.35, 0, 0.2].normalizeSum,
                1
            ).asStream.next;
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
        \ampAtk, Pwrand([2.001, 4.005], [2.7, 4.3], inf),
        \ampDec, Pwrand([2.8, 4.5], [2.7, 4.3], inf),
        \modAtk, Pwrand([2.001, 4.005], [2.7, 4.3], inf),
        \modDec, Pwrand([2.3, 4.9], [2.6, 4.4], inf),
        \dur, Pfunc { ~secondsPerBeat },
        \amp, Pfunc { ~masterProbability.coin.if(0.12, Rest(0)) },

        // subtle pitch shaping
        \pitchAtkFrac, Pwhite(0.01, 0.04),
        \pitchDecFrac, Pwhite(0.01, 0.05),

        // chorus
        \chorusRate, Pwhite(0.2, 0.3),
        \chorusDepth, Pwhite(0.001, 0.005),

        // delay
        \delayTime, Pwhite(0.03, 0.05),
        \delayModRate, Pwhite(0.1, 0.3),
        \delayModDepth, Pwhite(0.01, 0.02),
        \delayFeedback, Pwhite(0.2, 0.5),  // feedback range example
        \delayMix, Pwhite(0.2, 0.5),       // wet/dry blend

        // reverb
        \reverbMix, Pwhite(0.4, 0.5),
        \reverbTime, Pwhite(2, 4),
        \reverbDamp, Pwhite(0.2, 0.4)
    )
};

~notePatterns = Pn(~createNotePattern.value, inf);
~notePatterns.play(TempoClock(~bpm / 60));
)

(
s.waitForBoot {

	// Synth Kick: lower fundamental frequency, slower decay, some added drive
	SynthDef(\synthKick, { |out=0, amp=1, pan=0|
		var env = EnvGen.kr(Env.perc(0.01, 0.25, amp, curve: -2), doneAction: 2);
		var pitchEnv = EnvGen.kr(Env([50, 35], [0.1], \lin)); // lower starting pitch
		var freq = pitchEnv.midicps;
		var sig = SinOsc.ar(freq) * env;
		sig = sig * 1.2; // slight boost before distortion
		sig = (sig.tanh * 0.5) + (sig * 0.5); // blend distortion and original
		sig = Pan2.ar(sig, pan);
		Out.ar(out, sig);
	}).add;

	SynthDef(\synthSnare, { |out=0, amp=1, pan=0|
		var attack = EnvGen.kr(Env.perc(0, 0.2, 1), doneAction: 0);
		var decay = EnvGen.kr(Env.perc(0, 0.3, 1), doneAction: 2); // note: amp no longer directly here
		var noise = WhiteNoise.ar() * attack;
		var tonalBody = SinOsc.ar(200, 0, 0.4) * decay;
		var snare = HPF.ar(noise, 1500) + tonalBody;
		snare = snare * amp; // multiply the final signal by amp
		snare = Pan2.ar(snare, pan);
		Out.ar(out, snare);
	}).add;


	// Closed Hat (was the snare before): short, bright, high-passed noise
	SynthDef(\synthHat2, { |out=0, amp=1, pan=0|
		var env = EnvGen.kr(Env.perc(0.001, 0.05, amp), doneAction: 2);
		var noise = WhiteNoise.ar() * env;
		noise = HPF.ar(noise, 4000); // shift to higher frequencies
		noise = Pan2.ar(noise, pan);
		Out.ar(out, noise);
	}).add;

    // Synth Hat: quick burst of high-passed noise for a crisp hi-hat sound
    SynthDef(\synthHat, { |out=0, amp=1, pan=0|
        var env = EnvGen.kr(Env.perc(0.001, 0.05, amp), doneAction: 2);
        var noise = WhiteNoise.ar() * env;
        noise = HPF.ar(noise, 8000);
        noise = Pan2.ar(noise, pan);
        Out.ar(out, noise);
    }).add;

    // Tempo settings
    ~bpm = 170;
    ~secondsPerBeat = 240 / ~bpm;

    // Kick Pattern
    ~kickPattern = Pbind(
        \instrument, \synthKick,
        \dur, 4 * ~secondsPerBeat,
        \pan, 0,
        \amp, Pwrand([0.5, 0.75], [0.7, 1.0], inf)
    );

    // Snare Pattern
    ~snarePattern = Pbind(
        \instrument, \synthSnare,
        \dur, 7 * ~secondsPerBeat,
        \delay, 2 * ~secondsPerBeat,
        \pan, 0,
        \amp, Pwrand([0.3, 0.3], [0.5, 0.9], inf)
    );

    // Hat Pattern 1
    ~hat1Pattern = Pbind(
        \instrument, \synthHat,
        \dur, ~secondsPerBeat / 4,
        \pan, 0.3,
        \amp, Pwrand([0, 0.06], [0.1, 0.5], inf)
    );

    // Hat Pattern 2
    ~hat2Pattern = Pbind(
        \instrument, \synthHat,
        \dur, ~secondsPerBeat / 2,
        \delay, ~secondsPerBeat / 2,
        \pan, -0.3,
        \amp, Pwrand([0, 0.14], [0.1, 0.5], inf)
    );

    // Play all patterns in parallel
    Ppar([~kickPattern, ~snarePattern, ~hat1Pattern, ~hat2Pattern])
        .play(TempoClock(~bpm / 60));
};
)