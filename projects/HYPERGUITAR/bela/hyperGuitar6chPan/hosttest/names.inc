const char *SYNTH_NAMES[SYNTH_NMODES] = {
	"JUNO PAD",     // 0  PWM pulse + sub, ladder LP, built-in chorus - SYNTHESIS default
	"OB PAD",       // 1  two saws +-8c, 12 dB SVF lowpass, warm
	"PROPHET PAD",  // 2  saw + slow-PWM pulse, resonant ladder
	"STRING MACH",  // 3  Solina-style 8'+4' saws, 3-phase ensemble chorus
	"CS80 PAD",     // 4  two saws, slow vibrato, highpass + ladder band shape
	"ANALOG BRASS", // 5  saws, high-resonance ladder, strong level->cutoff, fast bloom
	"DRIFT POLY",   // 6  three saws with independent heavy drift
	"WARM SUB PAD", // 7  triangle + square sub, dark ladder
	"DRIFT PAD",    // 8  (old 0) three sines, one drifting, octave air
	"GLASS",        // 9  (old 1) sine + a whisper of 2nd/3rd harmonic
	"RINGMOD BELL", // 10 (old 2) carrier x irrational-ratio modulator, inharmonic
	"SUB GROWL",    // 11 (old 3) fundamental + sub-octave through a dark lowpass
	"SOFT SQUARE",  // 12 (old 4) band-limited square, double lowpass
	"WARM TRI",     // 13 (old 5) two soft triangles 4c apart (was the SYNTHESIS default)
	"WAVEFOLDER",   // 14 (old 6) sine through a tanh wavefolder, breathing fold depth
	"MOOG LEAD",    // 15 (old 7) saw through a resonant lowpass, static opening
	"ACID SQUELCH", // 16 (old 8) 303-style pulse, near-self-osc filter
	"FM GROWL",     // 17 (old 9) 2:1 FM, velocity-set constant index
	"FORMANT VOWEL",// 18 (old 10) real 2-band resonant filterbank, ooh<->ah morph
	"SUPERSAW",     // 19 (old 11) six saws +-19c, softened top
	"CHIP LEAD",    // 20 (old 12) stepped-duty pulse, raw 8-bit character
	"SAW LEAD",     // 21 (old 13) 2 saws ~7c apart
	"SYNC LEAD",    // 22 (old 14) hard-synced slave osc, drifting ratio
};
