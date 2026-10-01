------------------------------------------------------------------------STARTUP-------------------------------------------------------
(
s.options.memSize = 16384 * 8;
s.options.numOutputBusChannels = 8;
s.reboot;
)

------------------------------------------------------------------------SYNTH--------------------------------------------------------
(
// ===== globals =====
~bpm               = 170;
~secondsPerBeat    = 240 / ~bpm;
~masterProbability = 0.8;
~masterOffset      = -1;
~notes             = [60, 62, 64, 65, 67, 69, 71, 66];
~weights           = [1, 1, 1, 0.6, 1, 1, 1, 0.1].normalizeSum;
~pitchEnvAmt       = 0;
~ampEnvAmt         = 0;

// ===== SynthDef =====
SynthDef(\fmSynth, { | out = 0, chan = 0, freq = 440, modRatio = 2, modIndex = 1, amp = 0,
    ampAtk = 0.005, ampDec = 0.005, modAtk = 0.005, modDec = 0.005,
    pitchStart = -12, pitchEnd = -12,
    pitchAtkFrac = 0.09, pitchDecFrac = 0.04, pitchHoldFrac = 0.9,
    vibratoRate = 0.7, vibratoDepth = 0.2,
    chorusDepth = 0.005, chorusRate = 0.3,
    delayTime = 0.04, delayModRate = 0.2, delayModDepth = 0.015,
    delayFeedback = 0.3, delayMix = 0.3,
    reverbMix = 0.4, reverbTime = 2.5, reverbDamp = 0.3,
    satAmt = 1.0, envScale = 1.0,
    pitchEnvAmt = 0, ampEnvAmt = 0 |
    var totalAmpTime, pitchAtkTime, pitchHoldTime, pitchDecTime;
    var pitchEnv, freqMult, modEnv, ampEnv, modFreq, modSig, carSig;
    var vibrato, chorusLFO, chorusSig, delayMod, localIn, combined;
    var delayed, delaySig, reverbSig, satSig;
    var ampDecPluck, modDecPluck;
    var newAmpAtk, newAmpDec, newModAtk, newModDec;
    var baseAtk, baseHold, baseDec, scaleFac;
    var ampScaleFac, ampAtkTime, ampDecTime;

    newAmpAtk  = max(0.001, ampAtk * envScale);
    newAmpDec  = max(0.001, ampDec * envScale);
    newModAtk  = max(0.001, modAtk * envScale);
    newModDec  = max(0.001, modDec * envScale);
    ampDecPluck = newAmpDec * 4.2;
    modDecPluck = newModDec * 4.2;

    ampScaleFac = ampEnvAmt * 1.0;
    ampAtkTime  = max(0.001, newAmpAtk  * ampScaleFac);
    ampDecTime  = max(0.001, ampDecPluck * ampScaleFac);
    ampEnv      = EnvGen.kr(Env.perc(ampAtkTime, ampDecTime), doneAction: 2);

    totalAmpTime = newAmpAtk + ampDecPluck;
    baseAtk  = (totalAmpTime * pitchAtkFrac).clip(0.001, totalAmpTime);
    baseHold = (totalAmpTime * pitchHoldFrac).clip(0.001, totalAmpTime);
    baseDec  = (totalAmpTime * pitchDecFrac).clip(0.001, totalAmpTime);
    scaleFac     = pitchEnvAmt * 10;
    pitchAtkTime = (baseAtk  * scaleFac).max(0.001);
    pitchHoldTime= (baseHold * scaleFac).max(0.001);
    pitchDecTime = (baseDec  * scaleFac).max(0.001);
    pitchEnv = EnvGen.kr(
        Env([pitchStart, 0, 0, pitchEnd],
            [pitchAtkTime, pitchHoldTime, pitchDecTime], \lin),
        doneAction: 0
    );
    freqMult = 2.pow(pitchEnv / 12);

    modEnv = EnvGen.kr(Env.perc(newModAtk, modDecPluck), doneAction: 0);
    vibrato = SinOsc.kr(vibratoRate, 0, vibratoDepth).midiratio;
    modFreq = (freq * freqMult * vibrato) * modRatio;
    modSig  = SinOsc.ar(modFreq, 0, modIndex * modEnv * (freq * freqMult * vibrato));
    carSig  = SinOsc.ar((freq * freqMult * vibrato) + modSig) * ampEnv;

    chorusLFO = SinOsc.kr(chorusRate, 0, chorusDepth);
    chorusSig = Mix.ar([
        DelayC.ar(carSig, 0.02, chorusLFO + 0.005),
        DelayC.ar(carSig, 0.02, chorusLFO.neg + 0.005),
        carSig
    ]) * 0.6;

    delayMod = SinOsc.kr(delayModRate, 0, delayModDepth, delayTime);
    localIn  = LocalIn.ar(2);
    combined = (chorusSig ! 2) + (localIn * delayFeedback);
    delayed  = DelayL.ar(combined, 1.0, delayMod);
    LocalOut.ar(delayed);
    delaySig = XFade2.ar(chorusSig, delayed, (delayMix * 2) - 1);

    reverbSig = FreeVerb.ar(delaySig, reverbMix, reverbTime, reverbDamp);
    satSig    = tanh(reverbSig * satAmt) / tanh(satAmt);
    Out.ar(out + chan, satSig * amp);
}).add;

// ===== pattern generator =====
~createNotePattern = {
    Pbind(
        \instrument, \fmSynth,
        \chan,        Pfunc { (0..7).choose },
        \midinote,    Pfunc {
            var base = Pwrand(~notes, ~weights, 1).asStream.next + ~masterOffset;
            var oct  = Pwrand(
                [0, 12, -12, 24, -24, 36, -36],
                [0.55, 0.01, 0.4, 0, 0.8, 0.001, 0.4].normalizeSum,
                1
            ).asStream.next;
            base + oct;
        },
        \modRatio,    Pfunc {
            var r = Pwhite(0, 7).asStream.next;
            if (0.1.coin) { r = r + 0.25 };
            r;
        },
        \modIndex,     Pwhite(1, 5),
        \ampAtk,       Pwrand([2.001, 4.005], [2.7, 4.3], inf),
        \ampDec,       Pwrand([2.8, 4.5],   [2.7, 4.3], inf),
        \modAtk,       Pwrand([2.001, 4.005], [2.7, 4.3], inf),
        \modDec,       Pwrand([2.3, 4.9],   [2.6, 4.4], inf),
        \dur,          Pfunc { ~secondsPerBeat },
        \amp,          Pfunc { ~masterProbability.coin.if(0.04, Rest(0)) },
        \pitchAtkFrac, Pwhite(0.01, 0.04),
        \pitchDecFrac, Pwhite(0.01, 0.05),
        \chorusRate,   Pwhite(0.2, 0.3),
        \chorusDepth,  Pwhite(0.001, 0.005),
        \delayTime,    Pwhite(0.03, 0.05),
        \delayModRate, Pwhite(0.1, 0.3),
        \delayModDepth,Pwhite(0.01, 0.02),
        \delayFeedback,Pwhite(0.2, 0.5),
        \delayMix,     Pwhite(0.2, 0.5),
        \reverbMix,    Pwhite(0.4, 0.5),
        \reverbTime,   Pwhite(2, 4),
        \reverbDamp,   Pwhite(0.2, 0.4),
        \satAmt,       Pwhite(0.0, 3.0),
        \envScale,     Pwhite(0.0, 1.0),
        \pitchEnvAmt,  Pfunc { ~pitchEnvAmt },
        \ampEnvAmt,    Pfunc { ~ampEnvAmt }
    )
};

// ===== clock & play =====
~clock        = TempoClock(~bpm / 60);
~notePatterns = Pn(~createNotePattern.value, inf).play(~clock);

// ===== GUI =====
(

w = Window("YEW EYE", Rect(400, 200, 220, 210)).front;

StaticText(w, Rect(10, 10, 200, 20)).string_("amp env");
a = Slider(w, Rect(10, 30, 200, 20)).value_(~ampEnvAmt);
a.action = { ~ampEnvAmt = a.value };

StaticText(w, Rect(10, 60, 200, 20)).string_("pitch env");
k = Slider(w, Rect(10, 80, 200, 20)).value_(~pitchEnvAmt);
k.action = { ~pitchEnvAmt = k.value };

StaticText(w, Rect(10, 110, 200, 20)).string_("chance");
c = Slider(w, Rect(10, 130, 200, 20)).value_(~masterProbability);
c.action = { ~masterProbability = c.value };

StaticText(w, Rect(10, 160, 200, 20)).string_("tempo");
~bpmSliderInit = ((~bpm / 20).log / (500 / 20).log);
t = Slider(w, Rect(10, 180, 200, 20)).value_(~bpmSliderInit);
t.action = { |sl|


    ~bpm = 20 * ((500 / 20) ** sl.value);
    ~secondsPerBeat = 240 / ~bpm;
    ~clock.tempo = ~bpm / 60;
};
)
)

-----------------------------------------------------------------------DRUMS---------------------------------------------------------
(
s.waitForBoot {
	SynthDef(\synthKick, { |out=0, amp=1|
		var env = EnvGen.kr(Env.perc(0.01, 0.25, amp, curve: -2), doneAction: 2);
		var pitchEnv = EnvGen.kr(Env([50, 35], [0.1], \lin));
		var freq = pitchEnv.midicps;
		var sig = SinOsc.ar(freq) * env;
		sig = sig * 1.2;
		sig = (sig.tanh * 0.5) + (sig * 0.5);
		sig = sig * 0.3;
		Out.ar(out, sig ! 8 * 0.6);
	}).add;

	SynthDef(\synthSnare, { |out=0, amp=1|
		var attack = EnvGen.kr(Env.perc(0, 0.2, 1), doneAction: 0);
		var decay = EnvGen.kr(Env.perc(0, 0.3, 1), doneAction: 2);
		var noise = WhiteNoise.ar() * attack;
		var tonalBody = SinOsc.ar(200, 0, 0.4) * decay;
		var snare = (HPF.ar(noise, 1500) + tonalBody) * amp;
		Out.ar(out, snare * 0.6);
	}).add;

	SynthDef(\synthHat2, { |out=0, amp=1|
		var env = EnvGen.kr(Env.perc(0.001, 0.05, amp), doneAction: 2);
		var noise = HPF.ar(WhiteNoise.ar(), 4000) * env;
		Out.ar(out, noise * 0.6);
	}).add;

	SynthDef(\synthHat, { |out=0, amp=1|
		var env = EnvGen.kr(Env.perc(0.001, 0.05, amp), doneAction: 2);
		var noise = HPF.ar(WhiteNoise.ar(), 8000) * env;
		Out.ar(out, noise * 0.6);
	}).add;

	~bpm = 170;
	~secondsPerBeat = 240 / ~bpm;

	~kickPattern = Pbind(
		\instrument, \synthKick,
		\dur, 0.25 * ~secondsPerBeat,
		\amp, Pwrand([0, 0.1], [0.8, 1.0], inf)
	);

	~snarePattern = Pbind(
		\instrument, \synthSnare,
		\dur, 7 * ~secondsPerBeat,
		\delay, 2 * ~secondsPerBeat,
		\out, Pfunc { (0..7).choose },
		\amp, Pwrand([0.1, 0.2], [0.5, 0.9], inf)
	);

	~hat1Pattern = Pbind(
		\instrument, \synthHat,
		\dur, ~secondsPerBeat / 2,
		\out, Pfunc { (0..7).choose },
		\amp, Pwrand([0, 0.06], [0.4, 1], inf)
	);

	~hat2Pattern = Pbind(
		\instrument, \synthHat,
		\dur, ~secondsPerBeat / 4,
		\out, Pfunc { (0..7).choose },
		\amp, Pwrand([0, 0.06], [0.9, 1], inf)
	);

	Ppar([~kickPattern, ~snarePattern, ~hat1Pattern, ~hat2Pattern])
		.play(TempoClock(~bpm / 60));
};
)

-------------------------------------------------------------------------END---------------------------------------------------------