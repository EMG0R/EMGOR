// viebs - generative ambient music therapy synth + visuals

// c major scale, 6 octaves
[
    65.41,  73.42,  82.41,  87.31,  98.00, 110.00, 123.47, 130.81,
    130.81, 146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63,
    261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25,
    523.25, 587.33, 659.25, 698.46, 783.99, 880.00, 987.77, 1046.50,
    1046.50, 1174.66, 1318.51, 1396.91, 1567.98, 1760.00, 1975.53, 2093.00,
    2093.00, 2349.32, 2637.02, 2793.83, 3135.96, 3520.00, 3951.07, 4186.01
] @=> float cMajor[];

// stereo master volume
Gain masterL => dac.left;
Gain masterR => dac.right;
3 => masterL.gain;
3 => masterR.gain;

// stereo audio graph — everything routes through master
Gain kickOut => masterL;
kickOut => masterR;
// sine uses per-voice Pan2 (no shared bus) — routed to master below
Gain birdBus => Pan2 birdPan;
birdPan.left => masterL;
birdPan.right => masterR;
Gain rainBus => Pan2 rainPan;
rainPan.left => masterL;
rainPan.right => masterR;
Gain pluckBus => Pan2 pluckPan;
pluckPan.left => masterL;
pluckPan.right => masterR;

// kick - body + sub, no click
SinOsc kickBody => ADSR kickBodyEnv => kickOut;
SinOsc kickSub => ADSR kickSubEnv => kickOut;
0.55 => kickBody.gain;
0.25 => kickSub.gain;
90.0 => kickBody.freq;
45.0 => kickSub.freq;
kickBodyEnv.set( 2::ms, 260::ms, 0.0, 10::ms );
kickSubEnv.set( 3::ms, 200::ms, 0.0, 10::ms );
0.0 => kickOut.gain;

// sine voices - 24 polyphonic pads, per-voice stereo panning
SinOsc sineOsc[24];
ADSR sineEnv[24];
Gain sineAmp[24];
Pan2 sinePanV[24];
float svPan[24];

for( 0 => int i; i < 24; i++ ) {
    sineOsc[i] => sineEnv[i] => sineAmp[i] => sinePanV[i];
    sinePanV[i].left => masterL;
    sinePanV[i].right => masterR;
    0.15 => sineOsc[i].gain;
    0.0 => sineAmp[i].gain;
    sineEnv[i].set( 2000::ms, 6000::ms, 0.0, 30::ms );
    0.0 => svPan[i];
}

// birds - 8 voices, 2 per type
SinOsc birdOsc[8];
ADSR birdEnv[8];
Gain birdAmp[8];

for( 0 => int i; i < 8; i++ ) {
    birdOsc[i] => birdEnv[i] => birdAmp[i] => birdBus;
    0.2 => birdOsc[i].gain;
    0.0 => birdAmp[i].gain;
    birdEnv[i].set( 50::ms, 150::ms, 0.0, 30::ms );
}

// waves - 4ch stereo spread
Noise wavesNoise[4];
LPF wavesLPF[4];
Gain wavesGain[4];
Pan2 wavesPan[4];

for( 0 => int ch; ch < 4; ch++ ) {
    wavesNoise[ch] => wavesLPF[ch] => wavesGain[ch] => wavesPan[ch];
    wavesPan[ch].left => masterL;
    wavesPan[ch].right => masterR;
    0.3 => wavesNoise[ch].gain;
    120.0 => wavesLPF[ch].freq;
    0.707 => wavesLPF[ch].Q;
    0.0 => wavesGain[ch].gain;
}
-0.7 => wavesPan[0].pan;
0.7 => wavesPan[1].pan;
-0.3 => wavesPan[2].pan;
0.3 => wavesPan[3].pan;

// thunder - rumble w/ mids for phone speakers, stereo spread
Noise thunderNoise[4];
LPF thunderLPF[4];
Gain thunderGain[4];
Pan2 thunderPan[4];
// extra mid layer for iphone audibility
Noise thunderMidNoise[4];
BPF thunderBPF[4];
Gain thunderMidGain[4];

for( 0 => int ch; ch < 4; ch++ ) {
    thunderNoise[ch] => thunderLPF[ch] => thunderGain[ch] => thunderPan[ch];
    thunderPan[ch].left => masterL;
    thunderPan[ch].right => masterR;
    thunderMidNoise[ch] => thunderBPF[ch] => thunderMidGain[ch] => thunderPan[ch];
    0.4 => thunderNoise[ch].gain;
    80.0 => thunderLPF[ch].freq;
    1.2 => thunderLPF[ch].Q;
    0.0 => thunderGain[ch].gain;
    0.35 => thunderMidNoise[ch].gain;
    350.0 => thunderBPF[ch].freq;
    2.0 => thunderBPF[ch].Q;
    0.0 => thunderMidGain[ch].gain;
}
-0.6 => thunderPan[0].pan;
0.6 => thunderPan[1].pan;
-0.2 => thunderPan[2].pan;
0.2 => thunderPan[3].pan;

// rain - 12 voices
Noise rainNoiseSrc[4];
ADSR rainEnv[12];
LPF rainFilt[12];
Gain rainAmp[12];

for( 0 => int i; i < 12; i++ ) {
    i / 3 => int ch;
    rainNoiseSrc[ch] => rainEnv[i] => rainFilt[i] => rainAmp[i] => rainBus;
    0.3 => rainNoiseSrc[ch].gain;
    rainEnv[i].set( 1::ms, 5::ms, 0.0, 30::ms );
    3000.0 => rainFilt[i].freq;
    0.707 => rainFilt[i].Q;
    0.0 => rainAmp[i].gain;
}

// pluck - fm synthesis, 4 voices
// carrier + modulator with whole number ratios
SinOsc pluckCar[4];
SinOsc pluckMod[4];
ADSR pluckEnv[4];
Gain pluckAmp[4];
LPF pluckFilt[4];
// fm ratios: 1:1, 2:1, 3:2, 3:1
[1.0, 2.0, 1.5, 3.0] @=> float fmRatios[];

for( 0 => int i; i < 4; i++ ) {
    pluckMod[i] => pluckCar[i];
    pluckCar[i] => pluckFilt[i] => pluckEnv[i] => pluckAmp[i] => pluckBus;
    0.2 => pluckCar[i].gain;
    0.0 => pluckAmp[i].gain;
    pluckEnv[i].set( 3::ms, 140::ms, 0.0, 20::ms );
    2500.0 => pluckFilt[i].freq;
    1.5 => pluckFilt[i].Q;
}

// macros - each orb controls prob+vol merged
// initial values must match ctrlDefaults
// pitch: B3 above lowest octave. range is -12 to +12 semitones.
// lowest = -12 (C). B is 11 semitones above C. so pitch = -12 + 11 = -1. ctrlVal = (-1+12)/24 = 0.458
-1.0 => float gPitch;
0.33 => float gKickMacro;
135.0 => float gBPM;
0.75 => float gSineMacro;
0.50 => float gBirdMacro;
0.80 => float gWavesMacro;
0.50 => float gPluckMacro;
1.0 => float gThunderMacro;
0.15 => float gRainMacro;

// state
0 => int bmaj7Suppress; // when 1, sineLoop + chordLoop stop triggering new notes
0.0 => float scEnv;
1.0 => float gScMult;
0.0 => float scSmooth;
0 => int spawnKick;

// sine voice state
int svActive[24];
time svTrigTime[24];
dur svLife[24];
int svNote[24];
float svAmp[24];
float svFreq[24];

// bird state
int bvActive[8];
time bvTrigTime[8];
dur bvLife[8];
int bvNote[8];
float bvSweep[8];
float bvBaseFreq[8];
int bvType[8];

int bsBurstLeft[4];
time bsNextNote[4];
time bsNextCall[4];
int bsCurrentNote[4];
int bsRel[4];

[3, 2, 8, 4] @=> int bMinBurst[];
[6, 4, 16, 8] @=> int bMaxBurst[];
[50, 200, 20, 40] @=> int bMinInt[];
[120, 400, 40, 80] @=> int bMaxInt[];
[1, -1, 0, 0] @=> int bPitchMode[];
[15.0, 40.0, 5.0, 10.0] @=> float bAtk[];
[60.0, 160.0, 20.0, 50.0] @=> float bDec[];
[32, 28, 38, 30] @=> int bStartNote[];
[8, 8, 8, 10] @=> int bNoteRange[];
[0.3, -1.0, -0.15, 0.05] @=> float bMinSweep[];
[1.5, -0.3, 0.15, 0.5] @=> float bMaxSweep[];
[25.0, 6.0, 40.0, 15.0] @=> float bVibRate[];
[0.5, 0.12, 1.0, 0.25] @=> float bVibDepth[];

// rain voice state
int rvActive[12];
time rvTrigTime[12];
dur rvLife[12];

// wave state
float wvPhase[4];
float wvPhaseRate[4];
time wvNextSweep[4];
int wvSweeping[4];
int wvSweepUp[4];
time wvSweepStart[4];
float wvMaxCut[4];
float wvSweepDur[4];

// thunder state
int thSweeping[4];
time thSweepStart[4];
float thMaxCut[4];
float thDuration[4];
float thMaxGain[4];
time thNextTrig[4];

// pluck state
int pkVoice;
time pkNextNote;
int pkLastNotes[8];
0 => int pkNoteCount;

// spawn triggers
int spawnSine;
int spawnBird;
int spawnThunder;
int spawnPluck;

// rain visual: 1:1 audio-to-visual mapping
float rainDropX[128];
float rainDropY[128];
0 => int rainDropCount;

float sineSpawnFreq[24];
float sineSpawnAmp[24];
int sineSpawnNote[24];
0 => int sineSpawnCount;

float pluckSpawnFreq[8];
0 => int pluckSpawnCount;

// helpers
fun int findFreeSine() {
    for( 0 => int i; i < 24; i++ )
        if( !svActive[i] ) return i;
    return -1;
}

fun int findFreeBird( int btype ) {
    btype * 2 => int base;
    for( 0 => int i; i < 2; i++ )
        if( !bvActive[base + i] ) return base + i;
    return -1;
}

fun int findFreeRain( int ch ) {
    ch * 3 => int base;
    for( 0 => int i; i < 3; i++ )
        if( !rvActive[base + i] ) return base + i;
    return -1;
}

fun void triggerSineNote( int noteIdx, float vol, float pitch ) {
    findFreeSine() => int i;
    if( i < 0 ) return;
    if( noteIdx < 0 ) 0 => noteIdx;
    if( noteIdx > 47 ) 47 => noteIdx;

    cMajor[noteIdx] * Math.pow(2.0, pitch / 12.0) * 0.5 => float freq;
    freq => sineOsc[i].freq;
    sineEnv[i].set( 1200::ms, 3500::ms, 0.0, 30::ms );
    sineEnv[i].keyOn();
    0.22 * vol => float amp;
    amp => sineAmp[i].gain;
    // per-voice panning — no volume jumps
    Math.random2f(-0.7, 0.7) => float pan;
    pan => svPan[i];
    pan => sinePanV[i].pan;

    1 => svActive[i];
    now => svTrigTime[i];
    4700::ms => svLife[i];
    noteIdx => svNote[i];
    amp => svAmp[i];
    freq => svFreq[i];

    if( sineSpawnCount < 24 ) {
        freq => sineSpawnFreq[sineSpawnCount];
        amp => sineSpawnAmp[sineSpawnCount];
        noteIdx => sineSpawnNote[sineSpawnCount];
        sineSpawnCount + 1 => sineSpawnCount;
    }
    spawnSine + 1 => spawnSine;

    // feed pluck arp
    if( pkNoteCount < 8 ) {
        noteIdx => pkLastNotes[pkNoteCount];
        pkNoteCount + 1 => pkNoteCount;
    } else {
        for( 1 => int n; n < 8; n++ )
            pkLastNotes[n-1] => pkLastNotes[n];
        noteIdx => pkLastNotes[7];
    }
}

// trigger sine voice by raw frequency (for chords outside cMajor scale)
fun void triggerSineByFreq( float baseFreq, float vol ) {
    findFreeSine() => int i;
    if( i < 0 ) return;

    baseFreq * Math.pow(2.0, gPitch / 12.0) * 0.5 => float freq;
    freq => sineOsc[i].freq;
    sineEnv[i].set( 1200::ms, 3500::ms, 0.0, 30::ms );
    sineEnv[i].keyOn();
    0.22 * vol => float amp;
    amp => sineAmp[i].gain;
    Math.random2f(-0.7, 0.7) => float pan;
    pan => svPan[i];
    pan => sinePanV[i].pan;

    1 => svActive[i];
    now => svTrigTime[i];
    4700::ms => svLife[i];
    -1 => svNote[i]; // flag: use svFreq for pitch updates
    amp => svAmp[i];
    baseFreq * 0.5 => svFreq[i]; // store base freq (with 0.5 factor, no pitch)

    if( sineSpawnCount < 24 ) {
        freq => sineSpawnFreq[sineSpawnCount];
        amp => sineSpawnAmp[sineSpawnCount];
        0 => sineSpawnNote[sineSpawnCount];
        sineSpawnCount + 1 => sineSpawnCount;
    }
    spawnSine + 1 => spawnSine;
}

// audio shreds

fun void kickLoop() {
    while( true ) {
        gKickMacro => float m;
        // 0-50%: volume ramps to max. 50-100%: speed ramps up
        Math.min(m / 0.5, 1.0) => float vol;
        135.0 => float bpm;
        if( m > 0.5 ) 135.0 + (m - 0.5) / 0.5 * 50.0 => bpm;
        bpm => gBPM;

        if( m > 0.0 ) {
            vol * 0.36 => kickOut.gain;
            kickBodyEnv.keyOn();
            kickSubEnv.keyOn();
            vol => scEnv;
            spawnKick + 1 => spawnKick;

            // pitch sweep body 90->55, sub 45->27
            90.0 => kickBody.freq;
            45.0 => kickSub.freq;
            6::ms => now;
            70.0 => kickBody.freq;
            35.0 => kickSub.freq;
            6::ms => now;
            58.0 => kickBody.freq;
            29.0 => kickSub.freq;
            6::ms => now;
            55.0 => kickBody.freq;
            27.0 => kickSub.freq;

            (60.0 / bpm)::second - 18::ms => dur wait;
            if( wait > 0::samp ) wait => now;
        } else {
            0.0 => kickOut.gain;
            0.0 => scEnv;
            50::ms => now;
        }
    }
}

fun void sineLoop() {
    now => time sineNextNote;
    while( true ) {
        gSineMacro => float m;
        // macro = probability only, volume fixed at 0.9
        m => float prob;
        0.9 => float vol;
        gPitch => float pitch;
        gBPM => float bpm;
        (60.0 / bpm)::second => dur wholeNote;

        // update active note pitches in realtime
        for( 0 => int i; i < 24; i++ ) {
            if( svActive[i] ) {
                if( now - svTrigTime[i] > svLife[i] ) {
                    sineEnv[i].keyOff();
                    0.0 => sineAmp[i].gain;
                    0 => svActive[i];
                } else {
                    // svNote >= 0: cMajor lookup; svNote < 0: use stored base freq
                    0.0 => float newFreq;
                    if( svNote[i] >= 0 )
                        cMajor[svNote[i]] * Math.pow(2.0, pitch / 12.0) * 0.5 => newFreq;
                    else
                        svFreq[i] * Math.pow(2.0, pitch / 12.0) => newFreq;
                    newFreq => sineOsc[i].freq;
                }
            }
        }

        if( m > 0.0 && now >= sineNextNote && !bmaj7Suppress ) {
            // trigger more often
            if( Math.random2f(0.0, 1.0) < prob * 0.9 ) {
                Math.random2(0, 39) => int noteIdx;
                triggerSineNote( noteIdx, vol, pitch );
            }
            // mostly quarter notes for more density
            if( Math.random2f(0.0, 1.0) < 0.75 )
                now + wholeNote / 4.0 => sineNextNote;
            else
                now + wholeNote / 2.0 => sineNextNote;
        }

        // chord handled by separate chordLoop shred

        10::ms => now;
    }
}

// DEDICATED b7maj7 chord event — completely independent, ALWAYS fires every 15s
fun void chordLoop() {
    10::second => now; // initial wait
    while( true ) {
        // always fire if sine macro > 0 and not suppressed for bmaj7
        if( gSineMacro > 0.0 && !bmaj7Suppress ) {
            gPitch => float pitch;
            // Cmaj7 chord tones: C(0) E(2) G(4) B(6) in each octave block of 8
            // fire 10-14 notes spread across octaves 2-5, bias high
            Math.random2(10, 14) => int numNotes;
            for( 0 => int cn; cn < numNotes; cn++ ) {
                // octaves 2-5 (indices 16-47), weighted toward higher
                Math.random2(2, 5) => int oct;
                [0, 2, 4, 6] @=> int deg[];
                oct * 8 + deg[Math.random2(0, 3)] => int cNote;
                if( cNote > 47 ) 47 => cNote;
                triggerSineNote( cNote, 0.95, pitch );
            }
        }
        // exactly 15 seconds between chords
        15::second => now;
    }
}

// Bmaj7 chord burst — suppress other sine activity, let decays breathe, then hit
// Bmaj7 = B, D#, F#, A# from octave 2 up to octave 7 for sparkle
fun void bmaj7Loop() {
    [
        // low warmth
        123.47, 155.56, 185.00, 233.08,    // B2, D#3, F#3, A#3
        // mid body
        246.94, 311.13, 369.99, 466.16,    // B3, D#4, F#4, A#4
        // upper
        493.88, 622.25, 739.99, 932.33,    // B4, D#5, F#5, A#5
        // high shimmer
        987.77, 1244.51, 1479.98, 1864.66, // B5, D#6, F#6, A#6
        // sparkle top
        1975.53, 2489.02                    // B6, D#7
    ] @=> float bmaj7[];

    12::second => now; // initial offset from chordLoop
    while( true ) {
        if( gSineMacro > 0.0 ) {
            // 1. suppress normal sine + chord triggering
            1 => bmaj7Suppress;

            // 2. breathe — let existing notes decay
            Math.random2f(2.0, 3.5)::second => now;

            // 3. fire the Bmaj7 — big lush chord across full range
            //    10-16 notes: a few low, mostly mid, a few high sparkles
            Math.random2(10, 16) => int numNotes;
            for( 0 => int cn; cn < numNotes; cn++ ) {
                0 => int idx;
                Math.random2f(0.0, 1.0) => float roll;
                if( roll < 0.15 )
                    // low warmth (indices 0-3)
                    Math.random2(0, 3) => idx;
                else if( roll < 0.55 )
                    // mid body (indices 4-7)
                    Math.random2(4, 7) => idx;
                else if( roll < 0.80 )
                    // upper (indices 8-11)
                    Math.random2(8, 11) => idx;
                else if( roll < 0.93 )
                    // high shimmer (indices 12-15)
                    Math.random2(12, 15) => idx;
                else
                    // sparkle top (indices 16-17)
                    Math.random2(16, 17) => idx;
                triggerSineByFreq( bmaj7[idx], 0.95 );
            }

            // 4. let the chord ring for a moment before resuming
            1.5::second => now;
            0 => bmaj7Suppress;
        }
        // fire every 12-20 seconds
        Math.random2(12, 20)::second => now;
    }
}

fun void birdLoop() {
    for( 0 => int t; t < 4; t++ ) {
        now + Math.random2(500, 2000)::ms => bsNextCall[t];
        now => bsNextNote[t];
        0 => bsBurstLeft[t];
    }

    while( true ) {
        gBirdMacro => float m;
        // prob ramps 0-40%, vol ramps 40-100%
        Math.min(m / 0.4, 1.0) * 0.23 => float probRaw;
        0.3 => float vol;
        if( m > 0.4 ) 0.3 + (m - 0.4) / 0.6 * 0.7 => vol;
        gPitch => float pitch;
        Math.pow(probRaw, 3.0) => float prob;

        for( 0 => int i; i < 8; i++ ) {
            if( bvActive[i] && now - bvTrigTime[i] > bvLife[i] ) {
                birdEnv[i].keyOff();
                0.0 => birdAmp[i].gain;
                0 => bvActive[i];
            }
        }

        // bird pitch modulation
        for( 0 => int i; i < 8; i++ ) {
            if( bvActive[i] ) {
                (now - bvTrigTime[i]) / second => float elapsedSec;
                bvLife[i] / second => float lifeSec;
                elapsedSec / lifeSec => float progress;
                if( progress > 1.0 ) 1.0 => progress;
                bvType[i] => int bt;
                bvBaseFreq[i] * Math.pow(2.0, bvSweep[i] * progress) => float swept;
                Math.sin(elapsedSec * bVibRate[bt] * 6.2832) => float vib;
                swept * Math.pow(2.0, bVibDepth[bt] / 12.0 * vib) => swept;
                swept => birdOsc[i].freq;
            }
        }

        if( m > 0.0 ) {
            for( 0 => int t; t < 4; t++ ) {
                if( now >= bsNextCall[t] && bsBurstLeft[t] <= 0 ) {
                    if( Math.random2f(0.0, 1.0) < prob ) {
                        Math.random2(bMinBurst[t], bMaxBurst[t]) => bsBurstLeft[t];
                        bStartNote[t] + Math.random2(0, bNoteRange[t]) => bsCurrentNote[t];
                        0 => bsRel[t];
                        now => bsNextNote[t];
                    }
                    now + 20::ms => bsNextCall[t];
                }

                if( bsBurstLeft[t] > 0 && now >= bsNextNote[t] ) {
                    findFreeBird(t) => int v;
                    if( v >= 0 ) {
                        bsCurrentNote[t] + bsRel[t] => int noteIdx;
                        if( noteIdx < 0 ) 0 => noteIdx;
                        if( noteIdx > 47 ) 47 => noteIdx;
                        cMajor[noteIdx] * Math.pow(2.0, pitch / 12.0) => float freq;
                        freq => birdOsc[v].freq;
                        freq => bvBaseFreq[v];
                        t => bvType[v];
                        bAtk[t] + Math.random2f(-5.0, 5.0) => float a;
                        bDec[t] + Math.random2f(-10.0, 10.0) => float d;
                        if( a < 1.0 ) 1.0 => a;
                        if( d < 1.0 ) 1.0 => d;
                        birdEnv[v].set( a::ms, d::ms, 0.0, 15::ms );
                        birdEnv[v].keyOn();
                        0.3 * vol => birdAmp[v].gain;
                        Math.random2f(-0.8, 0.8) => birdPan.pan;
                        1 => bvActive[v];
                        now => bvTrigTime[v];
                        (a + d)::ms => bvLife[v];
                        noteIdx => bvNote[v];
                        Math.random2f(bMinSweep[t], bMaxSweep[t]) => bvSweep[v];
                        0 => int delta;
                        if( bPitchMode[t] == 1 ) Math.random2(1, 3) => delta;
                        else if( bPitchMode[t] == -1 ) -Math.random2(1, 3) => delta;
                        else Math.random2(-3, 3) => delta;
                        bsRel[t] + delta + Math.random2(-1, 1) => bsRel[t];
                        if( bsRel[t] > 10 ) 10 => bsRel[t];
                        if( bsRel[t] < -10 ) -10 => bsRel[t];
                        now + Math.random2(bMinInt[t], bMaxInt[t])::ms => bsNextNote[t];
                        bsBurstLeft[t] - 1 => bsBurstLeft[t];
                        if( bsBurstLeft[t] <= 0 )
                            now + (800 + Math.random2(0, 2000))::ms => bsNextCall[t];
                        spawnBird + 1 => spawnBird;
                    } else {
                        now + 10::ms => bsNextNote[t];
                    }
                }
            }
        }
        5::ms => now;
    }
}

fun void wavesLoop() {
    for( 0 => int ch; ch < 4; ch++ ) {
        Math.random2f(0.05, 0.15) => wvPhaseRate[ch];
        Math.random2f(0.0, 6.28) => wvPhase[ch];
        now + Math.random2(10, 30)::second => wvNextSweep[ch];
        0 => wvSweeping[ch];
    }
    while( true ) {
        gWavesMacro => float m;
        // prob ramps 0-40%, vol ramps 40-100%
        Math.min(m / 0.4, 1.0) => float prob;
        0.3 => float vol;
        if( m > 0.4 ) 0.3 + (m - 0.4) / 0.6 * 0.7 => vol;
        gPitch => float pitch;
        0.016 => float dt;
        // lower filter ceiling — keep it chill
        300.0 + (pitch + 12.0) / 24.0 * 1200.0 => float filterMax;
        for( 0 => int ch; ch < 4; ch++ ) {
            if( m > 0.0 ) {
                0.85 * vol * gScMult => wavesGain[ch].gain;
                wvPhase[ch] + 6.28 * wvPhaseRate[ch] * dt => wvPhase[ch];
                if( wvPhase[ch] > 6.28 ) wvPhase[ch] - 6.28 => wvPhase[ch];
                150.0 + Math.sin(wvPhase[ch]) * 80.0 => float cutoff;
                if( now >= wvNextSweep[ch] && !wvSweeping[ch] ) {
                    if( Math.random2f(0.0, 1.0) < prob * 0.5 ) {
                        1 => wvSweeping[ch];
                        1 => wvSweepUp[ch];
                        now => wvSweepStart[ch];
                        // mostly small sweeps, 15% chance of big one
                        if( Math.random2f(0.0, 1.0) < 0.15 )
                            filterMax * Math.random2f(0.7, 1.0) => wvMaxCut[ch];
                        else
                            filterMax * Math.random2f(0.15, 0.35) => wvMaxCut[ch];
                        Math.random2f(3.0, 7.0) => wvSweepDur[ch];
                    }
                    now + Math.random2(6, 22)::second => wvNextSweep[ch];
                }
                if( wvSweeping[ch] ) {
                    (now - wvSweepStart[ch]) / second => float elapsed;
                    elapsed / wvSweepDur[ch] => float p;
                    if( wvSweepUp[ch] ) {
                        if( p < 1.0 ) cutoff + (wvMaxCut[ch] - 120.0) * p => cutoff;
                        else { 0 => wvSweepUp[ch]; now => wvSweepStart[ch]; }
                    } else {
                        if( p < 1.0 ) cutoff + (wvMaxCut[ch] - 120.0) * (1.0 - p) => cutoff;
                        else 0 => wvSweeping[ch];
                    }
                }
                if( cutoff < 50.0 ) 50.0 => cutoff;
                if( cutoff > 12000.0 ) 12000.0 => cutoff;
                cutoff => wavesLPF[ch].freq;
            } else {
                0.0 => wavesGain[ch].gain;
            }
        }
        16::ms => now;
    }
}

fun void thunderLoop() {
    for( 0 => int ch; ch < 4; ch++ ) {
        now + Math.random2(2, 10)::second => thNextTrig[ch];
        0 => thSweeping[ch];
    }
    while( true ) {
        gThunderMacro => float m;
        Math.min(m / 0.25, 1.0) * 0.4 => float trigProb;
        Math.min(m * 2.0, 1.0) => float vol;
        for( 0 => int ch; ch < 4; ch++ ) {
            if( m > 0.0 ) {
                if( now >= thNextTrig[ch] && !thSweeping[ch] ) {
                    if( Math.random2f(0.0, 1.0) < trigProb ) {
                        1 => thSweeping[ch];
                        now => thSweepStart[ch];
                        // percussive rumble
                        80.0 + Math.random2f(0.0, 140.0) => thMaxCut[ch];
                        Math.random2f(3.0, 12.0) => thDuration[ch];
                        0.5 + Math.random2f(0.0, 0.35) => thMaxGain[ch];
                        now + Math.random2(5, 20)::second => thNextTrig[ch];
                        spawnThunder + 1 => spawnThunder;
                    } else {
                        now + 500::ms => thNextTrig[ch];
                    }
                }
                if( thSweeping[ch] ) {
                    (now - thSweepStart[ch]) / second => float elapsed;
                    elapsed / thDuration[ch] => float p;
                    if( p < 1.0 ) {
                        0.0 => float g;
                        if( p < 0.06 ) {
                            // fast percussive attack
                            (p / 0.06) * (p / 0.06) * thMaxGain[ch] => g;
                        } else {
                            // smooth decay with rumble variation
                            thMaxGain[ch] * Math.pow(1.0 - p, 0.6) => float baseG;
                            (0.6 + Math.random2f(0.0, 0.4)) * baseG => g;
                        }
                        // lpf sweeps up then down - warm rumble
                        0.0 => float cutoff;
                        if( p < 0.3 ) {
                            60.0 + thMaxCut[ch] * (p / 0.3) => cutoff;
                        } else {
                            thMaxCut[ch] * Math.pow(1.0 - (p - 0.3) / 0.7, 0.4) + 40.0 => cutoff;
                        }
                        if( cutoff < 40.0 ) 40.0 => cutoff;
                        if( cutoff > 220.0 ) 220.0 => cutoff;
                        cutoff => thunderLPF[ch].freq;
                        0.707 + Math.random2f(0.0, 0.3) => thunderLPF[ch].Q;
                        g * vol * 0.94 * gScMult => thunderGain[ch].gain;
                        // mid layer at 350Hz for phone speaker audibility
                        g * vol * 0.45 * gScMult => thunderMidGain[ch].gain;
                    } else {
                        0 => thSweeping[ch];
                        0.0 => thunderGain[ch].gain;
                        0.0 => thunderMidGain[ch].gain;
                    }
                } else {
                    0.0 => thunderGain[ch].gain;
                    0.0 => thunderMidGain[ch].gain;
                }
            } else {
                0.0 => thunderGain[ch].gain;
                0.0 => thunderMidGain[ch].gain;
                0 => thSweeping[ch];
            }
        }
        50::ms => now;
    }
}

fun void rainLoop() {
    while( true ) {
        gRainMacro => float m;
        // macro = probability only, volume fixed at 0.5
        m => float prob;
        0.5 => float vol;
        for( 0 => int i; i < 12; i++ ) {
            if( rvActive[i] && now - rvTrigTime[i] > rvLife[i] ) {
                rainEnv[i].keyOff();
                0.0 => rainAmp[i].gain;
                0 => rvActive[i];
            }
        }
        if( m > 0.0 ) {
            for( 0 => int ch; ch < 4; ch++ ) {
                if( Math.random2f(0.0, 1.0) < prob ) {
                    findFreeRain(ch) => int v;
                    if( v >= 0 ) {
                        // rain: ultra soft — long attack, no resonance
                        Math.random2f(2.0, 5.0) => float a;
                        Math.random2f(30.0, 80.0) => float d;
                        rainEnv[v].set( a::ms, d::ms, 0.0, 25::ms );
                        // heavily filtered — only very top shimmer
                        6000.0 + Math.random2f(0.0, 1500.0) => rainFilt[v].freq;
                        0.05 + Math.random2f(0.0, 0.02) => rainFilt[v].Q;
                        rainEnv[v].keyOn();
                        (0.03 + Math.random2f(0.0, 0.06)) * (vol + 0.25) => rainAmp[v].gain;
                        // per-drop panning — visual matches audio
                        Math.random2f(-0.9, 0.9) => float dropPan;
                        dropPan => rainPan.pan;
                        1 => rvActive[v];
                        now => rvTrigTime[v];
                        (a + d)::ms => rvLife[v];
                        // 1:1 visual rain drop — X position matches pan
                        if( rainDropCount < 128 ) {
                            dropPan => rainDropX[rainDropCount];
                            1.0 => rainDropY[rainDropCount];
                            rainDropCount + 1 => rainDropCount;
                        }
                    }
                }
            }
        }
        5::ms => now;
    }
}

fun void pluckLoop() {
    0 => pkVoice;
    now => pkNextNote;
    10 => pkLastNotes[0]; 14 => pkLastNotes[1];
    18 => pkLastNotes[2]; 22 => pkLastNotes[3];
    4 => pkNoteCount;
    while( true ) {
        gPluckMacro => float m;
        // macro = probability only, volume fixed at 0.9
        0.9 => float vol;
        m => float plkProb;
        gPitch => float pitch;
        gBPM => float bpm;
        (60.0 / bpm / 4.0)::second => dur sixteenth;
        if( m > 0.0 && now >= pkNextNote && pkNoteCount > 0 ) {
            // wider range: jump across octaves
            pkLastNotes[Math.random2(0, pkNoteCount - 1)] + Math.random2(-8, 8) => int noteIdx;
            if( noteIdx < 0 ) 0 => noteIdx;
            if( noteIdx > 47 ) 47 => noteIdx;
            cMajor[noteIdx] * Math.pow(2.0, pitch / 12.0) => float freq;
            // fm setup: modulator at whole ratio
            fmRatios[pkVoice] => float ratio;
            freq * ratio => pluckMod[pkVoice].freq;
            freq * ratio * Math.random2f(0.5, 2.0) => pluckMod[pkVoice].gain;
            freq => pluckCar[pkVoice].freq;
            freq * 3.0 + 500.0 => float fCut;
            if( fCut > 6000.0 ) 6000.0 => fCut;
            fCut => pluckFilt[pkVoice].freq;
            pluckEnv[pkVoice].keyOn();
            0.25 * vol => pluckAmp[pkVoice].gain;
            Math.random2f(-0.6, 0.6) => pluckPan.pan;
            if( pluckSpawnCount < 8 ) {
                freq => pluckSpawnFreq[pluckSpawnCount];
                pluckSpawnCount + 1 => pluckSpawnCount;
            }
            spawnPluck + 1 => spawnPluck;
            (pkVoice + 1) % 4 => pkVoice;
            if( Math.random2f(0.0, 1.0) < plkProb * 0.8 + 0.2 )
                now + sixteenth => pkNextNote;
            else
                now + sixteenth * 2 => pkNextNote;
        } else if( m <= 0.0 ) {
            now + 50::ms => pkNextNote;
        }
        5::ms => now;
    }
}

spork ~ kickLoop();
spork ~ sineLoop();
spork ~ chordLoop();
spork ~ bmaj7Loop();
spork ~ birdLoop();
spork ~ wavesLoop();
spork ~ thunderLoop();
spork ~ rainLoop();
spork ~ pluckLoop();

// visuals

GWindow.title( "V I E B S" );
GWindow.windowed( 1100, 700 );
GG.camera().posZ( 6.0 );

GG.bloom( 1 );
GG.bloomPass().intensity( 2.0 );
GG.bloomPass().radius( 0.65 );
GG.bloomPass().threshold( 0.0 );
GG.bloomPass().levels( 4 );

// background plane
GPlane bg --> GG.scene();
bg.sca( 40.0 );
bg.posZ( -2.5 );
bg.color( @(0.012, 0.012, 0.025) );

// sine visual pool — 1:1 with sine audio voices
// each circle follows the EXACT sine ADSR: 1200ms atk, 3500ms decay, 0 sus
24 => int SINE_VP;
GCircle sineShape[24];
float ssLife[24], ssMaxLife[24];
float ssX[24], ssY[24];
float ssR[24], ssG[24], ssB[24];

for( 0 => int i; i < 24; i++ ) {
    sineShape[i] --> GG.scene();
    sineShape[i].posZ( -0.3 );
    sineShape[i].sca( 0.0 );
    0.0 => ssLife[i];
}

// rain visual pool - 1:1 with audio
128 => int RAIN_VP;
GCircle rainDrop[128];
float rdLife[128], rdMaxLife[128];
float rdX[128], rdY[128], rdVX[128], rdVY[128], rdSz[128];
0 => int rdHead;

for( 0 => int i; i < 128; i++ ) {
    rainDrop[i] --> GG.scene();
    rainDrop[i].posZ( 0.5 );
    rainDrop[i].sca( 0.0 );
    0.0 => rdLife[i];
}

fun void spawnVisualRainDrop( float normX, float normY, float hW, float hH ) {
    rdHead => int i;
    (rdHead + 1) % 128 => rdHead;
    // dots falling 1/3 down the screen — fast velocity, long life
    hH * 2.0 * 0.55 => float targetDist;
    Math.random2f(0.7, 1.0) => float distMul;
    targetDist * distMul => float dist;
    2.5 => float speed;
    dist / speed => float life;
    life => rdLife[i];
    life => rdMaxLife[i];
    normX * hW => rdX[i];
    hH * 0.95 => rdY[i];
    0.0 => rdVX[i];
    -1.0 * speed => rdVY[i];
    Math.random2f(0.02, 0.04) => rdSz[i];
}

// spawn sine visual — matches voice index 1:1
// shape follows exact ADSR envelope: 1200ms attack, 3500ms decay
fun void spawnSineVisual( int idx, float freq, float amp, float hW, float hH ) {
    if( idx < 0 || idx >= 24 ) return;
    // life matches sine ADSR total: attack + decay = 4700ms
    4.7 => ssLife[idx];
    4.7 => ssMaxLife[idx];
    // position: random spread across screen
    Math.random2f(-0.85, 0.85) * hW => ssX[idx];
    Math.random2f(-0.6, 0.6) * hH => ssY[idx];
    // color from frequency — low=warm, high=cool
    Math.min(1.0, freq / 2000.0) => float t;
    0.3 + 0.5 * (1.0 - t) => ssR[idx];
    0.2 + 0.3 * t => ssG[idx];
    0.5 + 0.45 * t => ssB[idx];
}

// control orbs - 8 orbs (no master prob)
8 => int NUM_CTRL;
GCircle ctrlBody[8];
GCircle ctrlGlow[8];
GCircle ctrlInner[8];

// vibrant orb colors: pitch, kick, sine, bird, waves, pluck, thunder, rain
[0.1, 0.9, 0.2, 0.9, 0.15, 0.6, 0.2, 0.25] @=> float ctrlCR[];
[0.75, 0.25, 0.4, 0.75, 0.8, 0.2, 0.15, 0.85] @=> float ctrlCG[];
[0.85, 0.15, 0.95, 0.1, 0.7, 0.9, 0.75, 0.6] @=> float ctrlCB[];

float ctrlVal[8];
// orbs: pitch, kick, sine, bird, waves, pluck, thunder, rain
[0.458, 0.33, 0.75, 0.50, 0.80, 0.50, 1.0, 0.15] @=> float ctrlDefaults[];

float orbSwayPh[8];
float orbSwayRt[8];

for( 0 => int i; i < 8; i++ ) {
    ctrlDefaults[i] => ctrlVal[i];
    Math.random2f(0.0, 6.28) => orbSwayPh[i];
    Math.random2f(0.25, 0.6) => orbSwayRt[i];
    ctrlGlow[i] --> GG.scene();
    ctrlGlow[i].posZ( 0.01 );
    ctrlBody[i] --> GG.scene();
    ctrlBody[i].posZ( 0.02 );
    ctrlInner[i] --> GG.scene();
    ctrlInner[i].posZ( 0.03 );
}

// text labels under orbs
GText ctrlLabel[8];
["PITCH", "KICK", "SIN", "BIRD", "WAVES", "ARP", "THUNDER", "RAIN"] @=> string labelText[];
for( 0 => int i; i < 8; i++ ) {
    ctrlLabel[i] --> GG.scene();
    ctrlLabel[i].posZ( 0.04 );
    ctrlLabel[i].text( labelText[i] );
    ctrlLabel[i].sca( 0.1 );
    ctrlLabel[i].color( @(0.85, 0.85, 0.85) );
}

-1 => int grabIdx;
0.0 => float grabOffsetY;

// main render loop
0.0 => float globalTime;
0 => int frameCount;

while( true ) {
    GG.nextFrame() => now;
    GG.dt() => float dt;
    globalTime + dt => globalTime;
    frameCount + 1 => frameCount;

    // window + world bounds
    GG.windowWidth() $ float => float winW;
    GG.windowHeight() $ float => float winH;
    if( winW < 10.0 ) 10.0 => winW;
    if( winH < 10.0 ) 10.0 => winH;
    winW / winH => float aspect;
    6.0 => float camZ;
    camZ * Math.tan( 22.5 * 3.14159265 / 180.0 ) => float halfH;
    halfH * aspect => float halfW;

    // mouse input
    GWindow.mousePos() => vec2 mpos;
    GWindow.mouseLeft() => int mDown;

    // normalized mouse [0,1]
    mpos.x / winW => float mNormX;
    mpos.y / winH => float mNormY;

    // orb positions in screen space, 8% padding
    float orbNormX[8];
    float orbNormY[8];
    for( 0 => int i; i < 8; i++ ) {
        0.08 + 0.84 * (i $ float) / 7.0
            + Math.sin(globalTime * orbSwayRt[i] + orbSwayPh[i]) * 0.018
            => orbNormX[i];
        0.82 - ctrlVal[i] * 0.74
            + Math.sin(globalTime * orbSwayRt[i] * 0.6 + orbSwayPh[i] + 1.5) * 0.009
            => orbNormY[i];
    }

    // screen to world for rendering
    float orbDispX[8];
    float orbDispY[8];
    for( 0 => int i; i < 8; i++ ) {
        (orbNormX[i] - 0.5) * 2.0 * halfW => orbDispX[i];
        (0.5 - orbNormY[i]) * 2.0 * halfH => orbDispY[i];
    }

    Math.min(winW, winH) / 700.0 => float winScale;
    if( winScale > 1.5 ) 1.5 => winScale;
    if( winScale < 0.5 ) 0.5 => winScale;

    // mouse grab in normalized screen space
    if( mDown && grabIdx < 0 ) {
        -1 => int closest;
        999.0 => float closestDist;
        for( 0 => int i; i < 8; i++ ) {
            mNormX - orbNormX[i] => float dx;
            (mNormY - orbNormY[i]) * aspect => float dy;
            Math.sqrt(dx * dx + dy * dy) => float dist;
            if( dist < 0.06 && dist < closestDist ) {
                dist => closestDist;
                i => closest;
            }
        }
        if( closest >= 0 ) {
            closest => grabIdx;
            mNormY - orbNormY[closest] => grabOffsetY;
        }
    }

    if( !mDown ) -1 => grabIdx;

    if( grabIdx >= 0 ) {
        mNormY - grabOffsetY => float dragNormY;
        (0.82 - dragNormY) / 0.74 => float norm;
        if( norm < 0.0 ) 0.0 => norm;
        if( norm > 1.0 ) 1.0 => norm;
        norm => ctrlVal[grabIdx];
    }

    // map to params - 8 orbs: pitch, kick, sine, bird, waves, pluck, thunder, rain
    -12.0 + ctrlVal[0] * 24.0 => gPitch;
    ctrlVal[1] => gKickMacro;
    ctrlVal[2] => gSineMacro;
    ctrlVal[3] => gBirdMacro;
    ctrlVal[4] => gWavesMacro;
    ctrlVal[5] => gPluckMacro;
    ctrlVal[6] => gThunderMacro;
    ctrlVal[7] => gRainMacro;

    // sidechain - 2x more dramatic
    scSmooth + (scEnv - scSmooth) * Math.min(1.0, 50.0 * dt) => scSmooth;
    scEnv * Math.exp( -2.0 * dt ) => scEnv;
    if( scEnv < 0.003 ) 0.0 => scEnv;
    if( scSmooth < 0.003 ) 0.0 => scSmooth;

    // kick vol maxes at 50% of macro
    Math.min(gKickMacro / 0.5, 1.0) => float kickVol;
    kickVol * scSmooth => float rawDuck;
    if( rawDuck > 1.0 ) 1.0 => rawDuck;
    Math.pow(rawDuck, 0.5) * 0.30 => float duck;
    1.0 - duck => gScMult;

    // bus volumes (sidechain applied via gScMult)
    // vol derived from macro in each loop, but bus gain scales overall
    // per-voice sine sidechain — each voice keeps its own pan/gain
    Math.max(0.0, (gSineMacro - 0.25) / 0.75) * 1.5 * gScMult => float sineGainMul;
    for( 0 => int si; si < 24; si++ ) {
        if( svActive[si] ) svAmp[si] * sineGainMul => sineAmp[si].gain;
    }
    Math.max(0.0, (gBirdMacro - 0.25) / 0.75) * 1.25 * gScMult => birdBus.gain;
    0.88 * gScMult => rainBus.gain;
    Math.max(0.0, (gPluckMacro - 0.25) / 0.75) * 1.2 * gScMult => pluckBus.gain;

    // sidechain visual (20% intensity)
    1.0 + duck * 0.4 => float scPulse;
    1.0 + duck * 0.5 => float scBright;

    // sine visual: if voice is active but visual isn't, spawn it
    // guarantees 1:1 sine audio → visual circle relationship
    for( 0 => int vi; vi < 24; vi++ ) {
        if( svActive[vi] && ssLife[vi] <= 0.0 ) {
            spawnSineVisual( vi, svFreq[vi], svAmp[vi], halfW, halfH );
        }
    }

    // reset spawn counters (audio shreds still increment these)
    0 => spawnKick;
    0 => sineSpawnCount;
    0 => spawnSine;
    0 => spawnBird;
    0 => spawnThunder;

    // rain: 1:1 audio drop -> visual drop
    while( rainDropCount > 0 ) {
        rainDropCount - 1 => rainDropCount;
        spawnVisualRainDrop(
            rainDropX[rainDropCount], rainDropY[rainDropCount],
            halfW, halfH
        );
    }

    // reset remaining spawn counters
    0 => spawnPluck;
    0 => pluckSpawnCount;

    // update rain drops - fall far down screen
    for( 0 => int i; i < 128; i++ ) {
        if( rdLife[i] > 0.0 ) {
            rdLife[i] - dt => rdLife[i];
            if( rdLife[i] <= 0.0 ) {
                0.0 => rdLife[i];
                rainDrop[i].sca( 0.0 );
            } else {
                rdX[i] + rdVX[i] * dt => rdX[i];
                rdY[i] + rdVY[i] * dt => rdY[i];
                rdLife[i] / rdMaxLife[i] => float lifeLeft;
                rainDrop[i].posX( rdX[i] );
                rainDrop[i].posY( rdY[i] );
                // stay full size, fade only in last 15%
                1.0 => float alpha;
                if( lifeLeft < 0.15 ) lifeLeft / 0.15 => alpha;
                rainDrop[i].sca( rdSz[i] * alpha );
                0.9 * alpha => float rb;
                rainDrop[i].color( @(0.78 * rb, 0.82 * rb, 0.93 * rb) );
            }
        }
    }

    // update sine visual circles — scale follows EXACT sine ADSR envelope
    // ADSR: 1200ms attack, 3500ms decay, 0.0 sustain, 30ms release
    // total life = 4700ms = 4.7s
    for( 0 => int i; i < 24; i++ ) {
        if( ssLife[i] > 0.0 ) {
            ssLife[i] - dt => ssLife[i];
            if( ssLife[i] <= 0.0 ) {
                0.0 => ssLife[i];
                sineShape[i].sca( 0.0 );
            } else {
                ssMaxLife[i] - ssLife[i] => float elapsed;
                // ADSR envelope shape: attack 1.2s, decay 3.5s
                0.0 => float env;
                if( elapsed < 1.2 ) {
                    // attack: 0 → 1 over 1.2s
                    elapsed / 1.2 => env;
                } else {
                    // decay: 1 → 0 over 3.5s
                    1.0 - (elapsed - 1.2) / 3.5 => env;
                    if( env < 0.0 ) 0.0 => env;
                }

                // big circle: max size ~0.8 world units, scales with window
                env * 0.8 * winScale => float sz;
                sineShape[i].sca( sz );
                sineShape[i].posX( ssX[i] );
                sineShape[i].posY( ssY[i] );
                // color fades with envelope
                env * 0.12 => float bright;
                sineShape[i].color( @(
                    ssR[i] * bright,
                    ssG[i] * bright,
                    ssB[i] * bright
                ) );
            }
        }
    }

    // background color — simplified (no thColorBoost since bg shapes disabled)
    bg.color( @(
        0.012 + duck * 0.003,
        0.012 + duck * 0.002,
        0.025 + duck * 0.005
    ) );

    // control orbs - all fixed size, no growing/shrinking
    0.22 * winScale => float orbFixedSz;
    for( 0 => int i; i < 8; i++ ) {
        ctrlVal[i] => float norm;
        0.5 + norm * 0.5 => float bright;
        orbFixedSz => float thisSz;
        0.02 => float thisZ;

        ctrlGlow[i].posX( orbDispX[i] );
        ctrlGlow[i].posY( orbDispY[i] );
        ctrlGlow[i].posZ( thisZ - 0.01 );
        ctrlGlow[i].sca( thisSz * 2.5 );
        ctrlGlow[i].color( @(
            ctrlCR[i] * 0.12 * bright,
            ctrlCG[i] * 0.12 * bright,
            ctrlCB[i] * 0.12 * bright
        ) );

        ctrlBody[i].posX( orbDispX[i] );
        ctrlBody[i].posY( orbDispY[i] );
        ctrlBody[i].posZ( thisZ );
        ctrlBody[i].sca( thisSz );
        ctrlBody[i].color( @(
            ctrlCR[i] * 0.6 * bright,
            ctrlCG[i] * 0.6 * bright,
            ctrlCB[i] * 0.6 * bright
        ) );

        ctrlInner[i].posX( orbDispX[i] );
        ctrlInner[i].posY( orbDispY[i] );
        ctrlInner[i].posZ( thisZ + 0.01 );
        ctrlInner[i].sca( thisSz * 0.4 );
        ctrlInner[i].color( @(
            (0.5 + ctrlCR[i] * 0.5) * bright,
            (0.5 + ctrlCG[i] * 0.5) * bright,
            (0.5 + ctrlCB[i] * 0.5) * bright
        ) );

        // label fixed at bottom — scaled to window width
        (0.08 + 0.84 * (i $ float) / 7.0 - 0.5) * 2.0 * halfW => float labelX;
        ctrlLabel[i].posX( labelX );
        ctrlLabel[i].posY( -halfH * 0.92 );
        halfW * 0.06 => float lblSca;
        if( lblSca > 0.27 ) 0.27 => lblSca;
        ctrlLabel[i].sca( lblSca );
    }
}
