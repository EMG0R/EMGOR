// lockin.ck -- USS instrument: lockin  (NEPTR Phase 4's built-in synth mode, "Mid-Air Thief")
// Copied from NEPTR_phase4/chuck/synthi.ck. THE NOTE ALGORITHM AND THE VOICE
// (playLockinNote, pickAndPlayNote, every constant of them) ARE UNCHANGED.
//
// What changed for USS (everything else below is the original text):
//   * ch1 CC68 (arcade) MIDI handler -> the `trig` param (hard-mapped to the arcade button):
//     rising edge = fire one note now; held >= 5 s = toggle the internal sequencer.
//     The sequencer now boots ON (USS is generative by default; the original booted OFF).
//   * the original's own fixed constants are now driven live by the common USS params
//     (neutral at the defaults): density -> NOTE_PROB, length -> LOCKIN_LENGTH,
//     tone -> FILTER_OFFSET_SEMITONES, pitch -> LOCKIN_PITCH_SEMITONES,
//     intensity -> master volume, space -> a reverb send (0 = dry, as the original)
//   * BPM is NOT a param: it still comes from the transport (ch16 CC119 from demiurge-clock
//     via Midi Through). Set env USS_NO_MIDI=1 to skip opening MIDI (offline checks).
//   * a limiter after the master gain (transparent below 0.7)
//   * ARCADE NOTE: the original listened to CC68, which is Teensy button b9; the arcade
//     button on the NEPTR body is b1 (CC60). The hard map uses b1 like every USS instrument.
//
// Original header follows.
//
// Replicates the LOCKIN note player from
// ______PHASE3/cheese/synth.ck (playLockinNote) with all G.*
// dependencies replaced by hardcoded "good init" constants pulled
// from ______PHASE3/cheese/globals.ck. No CC controls, no MIDI in
// other than:
//
//   ch16 CC119  — master BPM broadcast from demiurge-clock
//                 (0..127 → 40..200, mapped here to BPM)
//   ch1  CC68   — arcade button (Teensy MIDI_CHANNEL=1, button 9):
//                   press (note-on)  → fire one note immediately
//                   hold ≥ 5s        → toggle internal sequencer
//
// (USS: the sequencer boots ON, see above.)
//
// Run on the Pi as part of NEPTR_phase4 via launch_phase4.sh.
//
// NOTE CHOICE ALGORITHM (2026-05 overhaul):
//   - 5 modal scales (Ionian/Dorian/Phrygian/Lydian/Mixolydian)
//     that rotate slowly over time (~8-24 notes per mode)
//   - Markov interval tendencies: next note biased by shortest
//     chromatic distance from last note (steps > thirds > leaps)
//   - Harmonic series boost: notes from C overtone series get
//     extra weight (C, G, E, Bb, D, F#, A)
//   - Lévy-flight register: symmetric octave drift with heavy-tailed
//     leap probability — equal coverage across C2–C7 (MIDI 36–96)

// ============================================
// LOCKED-IN INIT CONSTANTS (no controls)
// ============================================
124.0          => float BPM;        // matches demiurge-clock default
0.6            => float NOTE_PROB;

0.09           => float MIN_DECAY;
2.0            => float MAX_DECAY;
0.09           => float MIN_RELEASE;
2.0            => float MAX_RELEASE;
0.5            => float LOCKIN_LENGTH;

// pitch envelope (effectively flat — sustain >> 1 means no audible
// pitch sag in the non-decay branch)
0.0            => float PITCH_ATTACK_S;
1.0            => float PITCH_DECAY_S;
10.0           => float PITCH_SUSTAIN;
1.0            => float PITCH_RELEASE_S;
-12.0          => float PITCH_OFFSET_START;
-12.0          => float PITCH_OFFSET_END;
0.0            => float FILTER_OFFSET_SEMITONES;
0.0            => float LOCKIN_PITCH_SEMITONES;

// chromatic frequencies from middle C (C4 = MIDI 60)
float NOTE_PITCHES[12];
Std.mtof(60) => float baseFreq;
for (0 => int i; i < 12; i++) {
    baseFreq * Math.pow(2.0, i / 12.0) => NOTE_PITCHES[i];
}

// 200-entry voice-busy guard (matches G.playing)
int playing[200];
for (0 => int i; i < 200; i++) 0 => playing[i];

// ============================================
// NOTE CHOICE STATE
// ============================================

// Last played chromatic index (0–11); -1 = no previous note
int lastNoteIndex;
-1 => lastNoteIndex;

// Lévy-flight register in semitones from NOTE_PITCHES base (C4).
// Clamped so MIDI note stays in [36, 96] = C2–C7.
int currentRegister;
0 => currentRegister;

// 5 modal scales × 12 chromatic pitches (flat array: scale*12 + note)
// 0=Ionian  1=Dorian  2=Phrygian  3=Lydian  4=Mixolydian
float SCALES[60];
for (0 => int j; j < 60; j++) 0.0 => SCALES[j];
// Ionian   — C D E F G A B
1.0 => SCALES[0];  1.0 => SCALES[2];  1.0 => SCALES[4];
1.0 => SCALES[5];  1.0 => SCALES[7];  1.0 => SCALES[9];  1.0 => SCALES[11];
// Dorian   — C D Eb F G A Bb
1.0 => SCALES[12]; 1.0 => SCALES[14]; 1.0 => SCALES[15];
1.0 => SCALES[17]; 1.0 => SCALES[19]; 1.0 => SCALES[21]; 1.0 => SCALES[22];
// Phrygian — C Db Eb F G Ab Bb
1.0 => SCALES[24]; 1.0 => SCALES[25]; 1.0 => SCALES[27];
1.0 => SCALES[29]; 1.0 => SCALES[31]; 1.0 => SCALES[32]; 1.0 => SCALES[34];
// Lydian   — C D E F# G A B
1.0 => SCALES[36]; 1.0 => SCALES[38]; 1.0 => SCALES[40];
1.0 => SCALES[42]; 1.0 => SCALES[43]; 1.0 => SCALES[45]; 1.0 => SCALES[47];
// Mixolydian — C D E F G A Bb
1.0 => SCALES[48]; 1.0 => SCALES[50]; 1.0 => SCALES[52];
1.0 => SCALES[53]; 1.0 => SCALES[55]; 1.0 => SCALES[57]; 1.0 => SCALES[58];

// Scale rotation state
int currentScale;    0 => currentScale;
int notesInScale;    0 => notesInScale;
int nextScaleChange; 16 => nextScaleChange;

// Markov interval tendency weights: index = shortest chromatic distance (0–6)
// 0=unison  1=m2  2=M2  3=m3  4=M3  5=P4/P5  6=tritone
// Steps and thirds dominate; tritone has a spicy bump for tension
float INTERVAL_WEIGHTS[7];
[0.02, 0.22, 0.20, 0.16, 0.14, 0.12, 0.14] @=> INTERVAL_WEIGHTS;

// Harmonic series boost per chromatic degree (root = C = index 0)
// First 7 overtones of C mapped to 12-TET: C(0) G(7) E(4) Bb(10) D(2) F#(6) A(9)
float HARMONIC_BOOST[12];
[1.6, 1.0, 1.3, 1.0, 1.4, 1.0, 1.2, 1.4, 1.0, 1.2, 1.2, 1.0] @=> HARMONIC_BOOST;

// ============================================
// ARCADE BUTTON STATE
// ============================================
int clockOn;     1 => clockOn;      // USS: generative by default (original: 0)
int arcadeHeld;  0 => arcadeHeld;
time arcadePressTime;
now => arcadePressTime;
5::second => dur HOLD_THRESHOLD;

// ============================================
// MASTER OUT (synth voices)
// ============================================
Gain volGainL; Gain volGainR;
Dyno limL; Dyno limR;
limL.limit(); limR.limit();
0.7 => limL.thresh => limR.thresh;
0.05 => limL.slopeAbove => limR.slopeAbove;
0.5::ms => limL.attackTime => limR.attackTime;
80::ms => limL.releaseTime => limR.releaseTime;
volGainL => limL => dac.left;
volGainR => limR => dac.right;
0.6 => volGainL.gain;
0.6 => volGainR.gain;
Gain sendL; Gain sendR; JCRev revL; JCRev revR;
volGainL => sendL => revL => limL;
volGainR => sendR => revR => limR;
1.0 => revL.mix => revR.mix;
0.0 => sendL.gain => sendR.gain;

// ============================================
// USS PARAMS (pool hook)
// ============================================
DemiurgeParams pool;
pool.init( "uss-lockin", 0 );               // 0 -> env DEMIURGE_PARAM_PORT
"/uss-lockin/" => string NS;
["density", "tone", "length", "space", "intensity", "pitch", "trig"] @=> string PN[];
[0.5, 0.5, 0.5, 0.0, 0.5, 0.5, 0.0] @=> float PD[];
for (0 => int i; i < PN.size(); i++) pool.add( NS + PN[i], PD[i] );
Std.getenv("USS_NO_MIDI") != "" => int noMidi;

// ============================================
// MASTER CLOCK SYNC
// ============================================
fun void clockSync() {
    MidiIn min;
    MidiMsg msg;
    if (noMidi) return;
    if (!min.open("Midi Through")) return;
    while (true) {
        min => now;
        while (min.recv(msg)) {
            if ((msg.data1 & 0xF0) == 0xB0 && (msg.data1 & 0x0F) == 15
                && msg.data2 == 119) {
                40.0 + (msg.data3 $ float) * (200.0 - 40.0) / 127.0 => BPM;
            }
        }
    }
}
spork ~ clockSync();

// ============================================
// LOCKIN NOTE PLAYER (timbre unchanged)
// ============================================
fun void playLockinNote(float freq, int midi) {
    float minDecay;
    MIN_DECAY => minDecay;
    float maxDecay;
    MAX_DECAY => maxDecay;
    if (maxDecay < minDecay) minDecay => maxDecay;
    float minRelease;
    MIN_RELEASE => minRelease;
    float maxRelease;
    MAX_RELEASE => maxRelease;
    if (maxRelease < minRelease) minRelease => maxRelease;

    float lengthFactor;
    Math.randomf() => lengthFactor;
    lengthFactor * LOCKIN_LENGTH => lengthFactor;
    if (lengthFactor > 1.0) 1.0 => lengthFactor;

    dur noteATTACK;
    0.001::second => noteATTACK;
    dur noteDECAY;
    (minDecay + (maxDecay - minDecay) * lengthFactor)::second => noteDECAY;
    float localNoteSUSTAIN;
    0.0 => localNoteSUSTAIN;
    dur noteRELEASE;
    (minRelease + (maxRelease - minRelease) * lengthFactor)::second => noteRELEASE;

    // KEYBOARD TRACKING
    float keyTrackAmount;
    0.25 => keyTrackAmount;
    float midiNote;
    Std.ftom(freq) => midiNote;
    float semitonesFromMiddleC;
    midiNote - 60.0 => semitonesFromMiddleC;
    float keyTrackMultiplier;
    Math.pow(2.0, (semitonesFromMiddleC * keyTrackAmount) / 12.0) => keyTrackMultiplier;

    // FILTER SETTINGS with keyboard tracking
    float filterFloorBase;
    500.0 => filterFloorBase;
    float filterCeilingBase;
    Math.randomf() * 1000.0 => filterCeilingBase;
    float filterFloor;
    filterFloorBase * keyTrackMultiplier => filterFloor;
    float filterCeiling;
    filterCeilingBase * keyTrackMultiplier => filterCeiling;
    if (filterFloor < 20.0) 20.0 => filterFloor;
    if (filterFloor > 18000.0) 18000.0 => filterFloor;
    if (filterCeiling < filterFloor) filterFloor => filterCeiling;
    if (filterCeiling > 18000.0) 18000.0 => filterCeiling;
    float filterRes;
    Math.randomf() * 3.5 => filterRes;
    dur filterATTACK;
    (Math.randomf() * 0.15)::second => filterATTACK;
    dur filterDECAY;
    0.05::second => filterDECAY;
    float filterSUSTAIN;
    0.0 => filterSUSTAIN;
    dur filterRELEASE;
    0.05::second => filterRELEASE;

    // ANALOG DETUNE
    float detuneCents;
    6.0 + Math.randomf() * 6.0 => detuneCents;

    // Random vibrato per oscillator
    float vib1Rate; 3.5 + Math.randomf() * 2.5 => vib1Rate;
    float vib2Rate; 4.0 + Math.randomf() * 2.0 => vib2Rate;
    float vib3Rate; 3.0 + Math.randomf() * 3.0 => vib3Rate;
    float vib4Rate; 4.5 + Math.randomf() * 2.5 => vib4Rate;
    float vibDepthCents;
    4.0 + Math.randomf() * 6.0 => vibDepthCents;
    float phase1; Math.randomf() * 2.0 * Math.PI => phase1;
    float phase2; Math.randomf() * 2.0 * Math.PI => phase2;
    float phase3; Math.randomf() * 2.0 * Math.PI => phase3;
    float phase4; Math.randomf() * 2.0 * Math.PI => phase4;

    // PITCH ENVELOPE (15% chance of pitch decay)
    int doPitchDecay;
    if (Math.randomf() < 0.15) 1 => doPitchDecay;
    else 0 => doPitchDecay;
    dur pitchATTACK;
    PITCH_ATTACK_S::second => pitchATTACK;
    dur pitchDECAY;
    PITCH_DECAY_S::second => pitchDECAY;
    float pitchSustainLevel;
    PITCH_SUSTAIN => pitchSustainLevel;
    dur pitchRELEASE;
    PITCH_RELEASE_S::second => pitchRELEASE;
    dur pitchFullSustainStart;
    pitchATTACK + pitchDECAY => pitchFullSustainStart;
    if (doPitchDecay) {
        0::second => pitchATTACK;
        0.5::second => pitchDECAY;
        0.0 => pitchSustainLevel;
        pitchATTACK + pitchDECAY => pitchFullSustainStart;
    }
    dur elapsedAtKeyOff;
    noteATTACK + noteDECAY => elapsedAtKeyOff;
    float pitchReleaseStart;
    0.0 => pitchReleaseStart;
    if (pitchATTACK > 0::second && elapsedAtKeyOff < pitchATTACK) {
        elapsedAtKeyOff / pitchATTACK => pitchReleaseStart;
    } else if (pitchDECAY > 0::second && elapsedAtKeyOff < pitchFullSustainStart) {
        dur decayElapsed;
        elapsedAtKeyOff - pitchATTACK => decayElapsed;
        1.0 + (decayElapsed / pitchDECAY) * (pitchSustainLevel - 1.0) => pitchReleaseStart;
    } else {
        pitchSustainLevel => pitchReleaseStart;
    }

    // LEFT CHANNEL - Two detuned saws
    SawOsc sawL1 => Gain mixL;
    SawOsc sawL2 => mixL;
    0.45 => sawL1.gain;
    0.40 => sawL2.gain;

    LPF lpfL;
    ADSR envL;
    mixL => lpfL => envL;
    filterFloor => lpfL.freq;
    filterRes => lpfL.Q;
    envL.set(noteATTACK, noteDECAY, localNoteSUSTAIN, noteRELEASE);

    0.7 => envL.gain;
    envL => volGainL;

    // RIGHT CHANNEL - Two detuned saws
    SawOsc sawR1 => Gain mixR;
    SawOsc sawR2 => mixR;
    0.45 => sawR1.gain;
    0.40 => sawR2.gain;

    LPF lpfR;
    ADSR envR;
    mixR => lpfR => envR;
    (filterFloor * (0.92 + Math.randomf() * 0.16)) => lpfR.freq;
    (filterRes * (0.9 + Math.randomf() * 0.2)) => lpfR.Q;
    envR.set(noteATTACK, noteDECAY, localNoteSUSTAIN, noteRELEASE);

    0.7 => envR.gain;
    envR => volGainR;

    envL.keyOn();
    envR.keyOn();

    // MODULATION LOOP
    time startTime;
    now => startTime;
    dur totalDur;
    noteATTACK + noteDECAY + noteRELEASE => totalDur;

    while (now < startTime + totalDur) {
        dur elapsed;
        now - startTime => elapsed;
        float t;
        elapsed / 1::second => t;

        // FILTER ENVELOPE (ADSR)
        float filterEnvValue;
        if (elapsed < filterATTACK) {
            (elapsed / filterATTACK) => filterEnvValue;
        } else if (elapsed < filterATTACK + filterDECAY) {
            dur fDecayElapsed;
            elapsed - filterATTACK => fDecayElapsed;
            1.0 - ((fDecayElapsed / filterDECAY) * (1.0 - filterSUSTAIN)) => filterEnvValue;
        } else if (elapsed < noteATTACK + noteDECAY) {
            filterSUSTAIN => filterEnvValue;
        } else {
            dur releaseElapsed;
            elapsed - (noteATTACK + noteDECAY) => releaseElapsed;
            if (releaseElapsed < filterRELEASE) {
                filterSUSTAIN * (1.0 - (releaseElapsed / filterRELEASE)) => filterEnvValue;
            } else {
                0.0 => filterEnvValue;
            }
        }
        if (filterEnvValue < 0.0) 0.0 => filterEnvValue;
        if (filterEnvValue > 1.0) 1.0 => filterEnvValue;

        float cutoffL;
        filterFloor + (filterCeiling - filterFloor) * filterEnvValue => cutoffL;
        float cutoffR;
        (filterFloor * 0.95) + ((filterCeiling * 1.05) - (filterFloor * 0.95)) * filterEnvValue => cutoffR;

        float filterMult;
        Math.pow(2.0, FILTER_OFFSET_SEMITONES / 12.0) => filterMult;
        cutoffL * filterMult => cutoffL;
        cutoffR * filterMult => cutoffR;

        if (cutoffL < 20.0) 20.0 => cutoffL;
        if (cutoffL > 18000.0) 18000.0 => cutoffL;
        if (cutoffR < 20.0) 20.0 => cutoffR;
        if (cutoffR > 18000.0) 18000.0 => cutoffR;

        cutoffL => lpfL.freq;
        cutoffR => lpfR.freq;

        // VIBRATO
        float vib1;
        Math.sin(2.0 * Math.PI * vib1Rate * t + phase1) * vibDepthCents => vib1;
        float vib2;
        Math.sin(2.0 * Math.PI * vib2Rate * t + phase2) * vibDepthCents => vib2;
        float vib3;
        Math.sin(2.0 * Math.PI * vib3Rate * t + phase3) * vibDepthCents => vib3;
        float vib4;
        Math.sin(2.0 * Math.PI * vib4Rate * t + phase4) * vibDepthCents => vib4;

        // PITCH ENVELOPE
        float pitchEnv;
        0.0 => pitchEnv;
        if (elapsed < pitchATTACK) {
            (elapsed / pitchATTACK) => pitchEnv;
        } else if (elapsed < pitchFullSustainStart) {
            dur pDecayElapsed;
            elapsed - pitchATTACK => pDecayElapsed;
            1.0 + (pDecayElapsed / pitchDECAY) * (pitchSustainLevel - 1.0) => pitchEnv;
        } else if (elapsed < noteATTACK + noteDECAY) {
            pitchSustainLevel => pitchEnv;
        } else {
            dur releaseElapsed;
            elapsed - (noteATTACK + noteDECAY) => releaseElapsed;
            if (releaseElapsed < pitchRELEASE) {
                pitchReleaseStart * (1.0 - (releaseElapsed / pitchRELEASE)) => pitchEnv;
            } else {
                pitchReleaseStart => pitchEnv;
            }
        }
        if (pitchEnv > 1.0) 1.0 => pitchEnv;
        if (pitchEnv < 0.0) 0.0 => pitchEnv;

        float current_offset;
        PITCH_OFFSET_START => current_offset;
        if (elapsed >= noteATTACK + noteDECAY) {
            PITCH_OFFSET_END => current_offset;
        }
        float pitch_mod;
        if (doPitchDecay) {
            -12.0 * (1.0 - pitchEnv) => pitch_mod;
        } else {
            current_offset * (1.0 - pitchEnv) => pitch_mod;
        }

        float extraCents;
        pitch_mod * 100.0 + LOCKIN_PITCH_SEMITONES * 100.0 => extraCents;

        freq * Math.pow(2.0, (detuneCents + vib1 + extraCents) / 1200.0) => sawL1.freq;
        freq * Math.pow(2.0, (-detuneCents + vib2 + extraCents) / 1200.0) => sawL2.freq;
        freq * Math.pow(2.0, (-detuneCents * 0.7 + vib3 + extraCents) / 1200.0) => sawR1.freq;
        freq * Math.pow(2.0, (detuneCents * 0.7 + vib4 + extraCents) / 1200.0) => sawR2.freq;

        5::ms => now;
    }

    envL.keyOff();
    envR.keyOff();
    10::ms => now;

    // CLEANUP
    sawL1 =< mixL;
    sawL2 =< mixL;
    mixL =< lpfL;
    lpfL =< envL;
    envL =< volGainL;

    sawR1 =< mixR;
    sawR2 =< mixR;
    mixR =< lpfR;
    lpfR =< envR;
    envR =< volGainR;

    0 => playing[midi];
}

// ============================================
// PICK AND PLAY — deep note choice algorithm
//
// Algorithm layers (all interact multiplicatively):
//   1. Mode rotation: 5 scales cycle every 8–24 notes (35% chance
//      of switch at threshold; prefers adjacent modes for coherence)
//   2. Chromatic weights: scale tones = 1.0, non-scale = 0.08
//      (chromatic notes are rare but not forbidden for color)
//   3. Harmonic boost: overtone-series notes get up to ×1.6 weight
//   4. Markov interval: weight multiplied by INTERVAL_WEIGHTS[dist]
//      where dist = shortest chromatic distance from last note (0–6)
//      — strongly prefers stepwise motion, occasional leaps
//   5. Lévy-flight register: symmetric octave drifts keep coverage
//      equal across C2–C7; heavy-tailed jumps (±2, ±3 oct) create
//      dramatic register sweeps while rubber-band clamping at edges
// ============================================
fun void pickAndPlayNote() {
    // --- mode rotation ---
    notesInScale + 1 => notesInScale;
    if (notesInScale >= nextScaleChange) {
        0 => notesInScale;
        if (Math.randomf() < 0.35) {
            // 60% of the time drift to adjacent mode (more musically coherent)
            if (Math.randomf() < 0.6) {
                int dir;
                if (Math.randomf() < 0.5) 1 => dir; else -1 => dir;
                (currentScale + dir + 5) % 5 => currentScale;
            } else {
                Math.random2(0, 4) => currentScale;
            }
        }
        8 + Math.random2(0, 16) => nextScaleChange;
    }

    // --- build per-note weights ---
    float weights[12];
    for (0 => int i; i < 12; i++) {
        // scale membership: in-scale = 1.0, chromatic passing tones = 0.08
        float sm;
        SCALES[currentScale * 12 + i] => sm;
        if (sm < 0.5) 0.08 => sm;

        // harmonic series multiplicative boost
        sm * HARMONIC_BOOST[i] => weights[i];

        // Markov interval tendency from last note
        if (lastNoteIndex >= 0) {
            int rawDist;
            if (i >= lastNoteIndex) i - lastNoteIndex => rawDist;
            else lastNoteIndex - i => rawDist;
            int dist;
            if (rawDist > 6) 12 - rawDist => dist; else rawDist => dist;
            weights[i] * INTERVAL_WEIGHTS[dist] => weights[i];
        }
    }

    // --- weighted selection ---
    float totalW;
    0.0 => totalW;
    for (0 => int i; i < 12; i++) totalW + weights[i] => totalW;
    if (totalW <= 0.0) return;

    Math.randomf() * totalW => float r;
    float cum;
    0.0 => cum;
    int selectedNote;
    -1 => selectedNote;
    for (0 => int i; i < 12; i++) {
        cum + weights[i] => cum;
        if (r <= cum && selectedNote < 0) i => selectedNote;
    }
    if (selectedNote < 0) 0 => selectedNote;
    selectedNote => lastNoteIndex;

    // --- Lévy-flight register (symmetric, equal octave coverage) ---
    // 50% stay | 17% ±oct | 5% ±2oct | 3% ±3oct
    Math.randomf() => float rr;
    if (rr < 0.50) {
        // stay
    } else if (rr < 0.67) {
        currentRegister + 12 => currentRegister;
    } else if (rr < 0.84) {
        currentRegister - 12 => currentRegister;
    } else if (rr < 0.89) {
        currentRegister + 24 => currentRegister;
    } else if (rr < 0.94) {
        currentRegister - 24 => currentRegister;
    } else if (rr < 0.97) {
        currentRegister + 36 => currentRegister;
    } else {
        currentRegister - 36 => currentRegister;
    }

    // rubber-band clamp: MIDI 36 (C2) to 96 (C7)
    int midiTest;
    60 + selectedNote + currentRegister => midiTest;
    while (midiTest < 36) {
        currentRegister + 12 => currentRegister;
        60 + selectedNote + currentRegister => midiTest;
    }
    while (midiTest > 96) {
        currentRegister - 12 => currentRegister;
        60 + selectedNote + currentRegister => midiTest;
    }

    // --- fire note ---
    NOTE_PITCHES[selectedNote] * Math.pow(2.0, currentRegister / 12.0) => float selectedFreq;
    60 + selectedNote + currentRegister => int midiNote;

    if (midiNote >= 0 && midiNote < 200 && playing[midiNote] == 0) {
        1 => playing[midiNote];
        spork ~ playLockinNote(selectedFreq, midiNote);
    }
}

// ============================================
// ARCADE BUTTON HOLD DETECTOR
// ============================================
fun void holdDetect() {
    HOLD_THRESHOLD => now;
    if (arcadeHeld) {
        1 - clockOn => clockOn;
        if (clockOn) <<< "ARCADE: CLOCK ON" >>>;
        else         <<< "ARCADE: CLOCK OFF" >>>;
    }
}

// ============================================
// USS PARAM LOOP + ARCADE (the `trig` param replaces the CC68 handler)
//   trig rising edge -> fire one note, start the 5 s hold detector
//   trig falling edge -> released
// ============================================
fun float pow2(float x) { return Math.pow(2.0, x); }
fun void paramLoop() {
    float last[PN.size()];
    for (0 => int i; i < last.size(); i++) -1.0 => last[i];
    0 => int trigWas;
    while (true) {
        pool.get( NS + "density" ) => float d;
        Math.min( 1.0, d * 1.2 ) => NOTE_PROB;                           // 0.5 -> 0.6 (the original)
        pool.get( NS + "length" ) => LOCKIN_LENGTH;                      // 0.5 (the original)
        (pool.get( NS + "tone" ) - 0.5) * 48.0 => FILTER_OFFSET_SEMITONES;   // 0.5 -> 0
        Math.round( (pool.get( NS + "pitch" ) - 0.5) * 24.0 ) => LOCKIN_PITCH_SEMITONES;   // 0.5 -> 0
        0.6 * pow2( (pool.get( NS + "intensity" ) - 0.5) * 2.0 ) => volGainL.gain => volGainR.gain;
        pool.get( NS + "space" ) * 0.6 => sendL.gain => sendR.gain;
        pool.get( NS + "trig" ) => float tg;
        if (tg >= 0.5 && !trigWas) {
            1 => trigWas;
            1 => arcadeHeld;
            now => arcadePressTime;
            pickAndPlayNote();
            spork ~ holdDetect();
        } else if (tg < 0.5 && trigWas) {
            0 => trigWas;
            0 => arcadeHeld;
        }
        for (0 => int i; i < PN.size(); i++) {
            pool.get( NS + PN[i] ) => float v;
            if (Math.fabs( v - last[i] ) > 0.001) { v => last[i]; pool.out( NS + PN[i], v ); }
        }
        20::ms => now;
    }
}
spork ~ paramLoop();

// ============================================
// INTERNAL SEQUENCER
// ============================================
fun void sequencer() {
    while (true) {
        if (clockOn && Math.randomf() < NOTE_PROB) {
            pickAndPlayNote();
        }
        dur beat;
        (15.0 / Math.max(1.0, BPM))::second => beat;
        beat => now;
    }
}

spork ~ sequencer();
while (true) { 1::second => now; }
