// generative ambient synth with interactive visuals

// c major scale across 6 octaves, 8 notes per octave block
[
    65.41,  73.42,  82.41,  87.31,  98.00, 110.00, 123.47, 130.81,
    130.81, 146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63,
    261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25,
    523.25, 587.33, 659.25, 698.46, 783.99, 880.00, 987.77, 1046.50,
    1046.50, 1174.66, 1318.51, 1396.91, 1567.98, 1760.00, 1975.53, 2093.00,
    2093.00, 2349.32, 2637.02, 2793.83, 3135.96, 3520.00, 3951.07, 4186.01
] @=> float cMajor[];

// stereo master bus, everything routes here before hitting dac
Gain masterL => dac.left;
Gain masterR => dac.right;
3 => masterL.gain;
3 => masterR.gain;

// each instrument has its own gain bus into the master
Gain kickOut => masterL;
kickOut => masterR;
// sine pads use per-voice pan2 so each note has its own stereo position
// birds also use per-voice pan2 so each chirp pans independently
Gain birdL => masterL;
Gain birdR => masterR;
Gain rainBus => Pan2 rainPan;
rainPan.left => masterL;
rainPan.right => masterR;
Gain pluckBus => Pan2 pluckPan;
pluckPan.left => masterL;
pluckPan.right => masterR;

// kick drum is two sine oscillators, body at 90hz and sub at 45hz
// both shaped by adsr envelopes with basically instant 2ms attack
// the body does a pitch sweep from 90 down to 55hz over 18ms for that thump
SinOsc kickBody => ADSR kickBodyEnv => kickOut;
SinOsc kickSub => ADSR kickSubEnv => kickOut;
0.55 => kickBody.gain;
0.30 => kickSub.gain;
90.0 => kickBody.freq;
45.0 => kickSub.freq;
SinOsc kickClick => ADSR kickClickEnv => kickOut;
0.18 => kickClick.gain;
180.0 => kickClick.freq;
kickClickEnv.set( 1::ms, 40::ms, 0.0, 5::ms );
kickBodyEnv.set( 2::ms, 260::ms, 0.0, 10::ms );
kickSubEnv.set( 3::ms, 200::ms, 0.0, 10::ms );
0.0 => kickOut.gain;

// 24 sine pad voices — fundamental + harmonic overlay for locked-in timbre
SinOsc sineOsc[24];
SinOsc sineHarm[24];
ADSR sineEnv[24];
Gain sineAmp[24];
Pan2 sinePanV[24];
float svPan[24];

for( 0 => int i; i < 24; i++ ) {
    sineOsc[i] => sineEnv[i] => sineAmp[i] => sinePanV[i];
    sineHarm[i] => sineEnv[i];
    sinePanV[i].left => masterL;
    sinePanV[i].right => masterR;
    0.15 => sineOsc[i].gain;
    0.0 => sineHarm[i].gain;
    0.0 => sineAmp[i].gain;
    sineEnv[i].set( 2000::ms, 6000::ms, 0.0, 30::ms );
    0.0 => svPan[i];
}

// 8 bird voices, 2 per bird type (chirp up, chirp down, trill, warble)
// each bird does a frequency sweep with vibrato during its short life
// per-voice stereo pan so each chirp can appear anywhere in the field
SinOsc birdOsc[8];
ADSR birdEnv[8];
Gain birdAmp[8];
Pan2 birdPanV[8];

for( 0 => int i; i < 8; i++ ) {
    birdOsc[i] => birdEnv[i] => birdAmp[i] => birdPanV[i];
    birdPanV[i].left => birdL;
    birdPanV[i].right => birdR;
    0.2 => birdOsc[i].gain;
    0.0 => birdAmp[i].gain;
    birdEnv[i].set( 50::ms, 150::ms, 0.0, 30::ms );
}

// ocean waves are 4 channels of filtered white noise spread across stereo
// a low pass filter slowly sweeps up and down to create the wash effect
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

// thunder is filtered noise with a low pass for the rumble and a
// bandpass at 350hz so it still comes through on phone speakers
// 4 channels with stereo spread for width
Noise thunderNoise[4];
LPF thunderLPF[4];
Gain thunderGain[4];
Pan2 thunderPan[4];
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

// rain: hiss + boil bursts + individual drops (reduced for iPhone)
Noise rainHiss => BPF rainHissFilt => Gain rainHissGain;
rainHissGain => masterL;
rainHissGain => masterR;
1200.0 => rainHissFilt.freq; 1.2 => rainHissFilt.Q;
0.0 => rainHissGain.gain;

Noise rainBoilNoise[4];
BPF rainBoilFilt[4];
ADSR rainBoilEnv[4];
Gain rainBoilAmp[4];
int rbActive[4];
time rbTrigTime[4];
dur rbLife[4];

for( 0 => int i; i < 4; i++ ) {
    rainBoilNoise[i] => rainBoilFilt[i] => rainBoilEnv[i] => rainBoilAmp[i];
    rainBoilAmp[i] => masterL;
    rainBoilAmp[i] => masterR;
    0.8 => rainBoilNoise[i].gain;
    rainBoilEnv[i].set( 0.2::ms, 5::ms, 0.0, 1::ms );
    8000.0 => rainBoilFilt[i].freq;
    3.5 => rainBoilFilt[i].Q;
    0.0 => rainBoilAmp[i].gain;
    0 => rbActive[i];
}

Noise rainImpactNoise[8];
BPF rainImpactFilt[8];
ADSR rainImpactEnv[8];
Gain rainImpactAmp[8];
SinOsc rainBubble[8];
ADSR rainBubbleEnv[8];
Gain rainBubbleAmp[8];

for( 0 => int i; i < 8; i++ ) {
    rainImpactNoise[i] => rainImpactFilt[i] => rainImpactEnv[i] => rainImpactAmp[i] => rainBus;
    0.9 => rainImpactNoise[i].gain;
    rainImpactEnv[i].set( 0.4::ms, 10::ms, 0.0, 3::ms );
    5000.0 => rainImpactFilt[i].freq;
    1.8 => rainImpactFilt[i].Q;
    0.0 => rainImpactAmp[i].gain;
    rainBubble[i] => rainBubbleEnv[i] => rainBubbleAmp[i] => rainBus;
    rainBubbleEnv[i].set( 0.3::ms, 15::ms, 0.0, 5::ms );
    5000.0 => rainBubble[i].freq;
    0.0 => rainBubbleAmp[i].gain;
}

// pluck uses fm synthesis with 4 voices, each carrier sine gets
// modulated by another sine at whole number frequency ratios
// (1:1, 2:1, 3:2, 3:1) then through a low pass and adsr
SinOsc pluckCar[4];
SinOsc pluckMod[4];
ADSR pluckEnv[4];
Gain pluckAmp[4];
LPF pluckFilt[4];
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

// each orb controls a macro value from 0 to 1 that maps to
// probability and volume for its instrument
// pitch orb shifts everything by -12 to +12 semitones
-1.0 => float gPitch;
0.33 => float gKickMacro;
135.0 => float gBPM;
0.75 => float gSineMacro;
0.50 => float gBirdMacro;
0.80 => float gWavesMacro;
0.50 => float gPluckMacro;
1.0 => float gThunderMacro;
0.15 => float gRainMacro;

// sidechain state
0.0 => float scEnv;
1.0 => float gScMult;
0.0 => float scSmooth;
0 => int spawnKick;

// per-voice tracking for sine pads
int svActive[24];
time svTrigTime[24];
dur svLife[24];
int svNote[24];
float svAmp[24];
float svFreq[24];

// per-voice tracking for birds, bvPan stores stereo position for visuals
int bvActive[8];
time bvTrigTime[8];
dur bvLife[8];
int bvNote[8];
float bvSweep[8];
float bvBaseFreq[8];
int bvType[8];
float bvPan[8];

// bird burst scheduling, 4 types
int bsBurstLeft[4];
time bsNextNote[4];
time bsNextCall[4];
int bsCurrentNote[4];
int bsRel[4];

// bird type parameters: burst sizes, timing, pitch behavior, envelope, sweep, vibrato
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

int rvActive[8];
time rvTrigTime[8];
dur rvLife[8];

float wvPhase[4];
float wvPhaseRate[4];
time wvNextSweep[4];
int wvSweeping[4];
int wvSweepUp[4];
time wvSweepStart[4];
float wvMaxCut[4];
float wvSweepDur[4];

int thSweeping[4];
time thSweepStart[4];
float thMaxCut[4];
float thDuration[4];
float thMaxGain[4];
time thNextTrig[4];

int pkVoice;
time pkNextNote;
int pkLastNotes[8];
0 => int pkNoteCount;

// visual spawn triggers, audio shreds set these and the render loop reads them
int spawnSine;
int spawnBird;
int spawnThunder;
int spawnPluck;

// rain drop positions for 1:1 audio-to-visual mapping
float rainDropX[32];
float rainDropY[32];
0 => int rainDropCount;

// sine spawn data so visuals know what frequency and amplitude triggered
float sineSpawnFreq[24];
float sineSpawnAmp[24];
int sineSpawnNote[24];
0 => int sineSpawnCount;

float pluckSpawnFreq[8];
0 => int pluckSpawnCount;

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

fun int findFreeBoil() {
    for( 0 => int i; i < 4; i++ )
        if( !rbActive[i] ) return i;
    return -1;
}

fun int findFreeRain( int ch ) {
    ch * 2 => int base;
    for( 0 => int i; i < 2; i++ )
        if( !rvActive[base + i] ) return base + i;
    return -1;
}

// trigger a sine pad voice from the c major scale
// looks up the frequency, applies pitch shift, sets a 1200ms attack
// 3500ms decay adsr, random stereo pan, and queues a visual spawn
fun void triggerSineNote( int noteIdx, float vol, float pitch ) {
    findFreeSine() => int i;
    if( i < 0 ) return;
    if( noteIdx < 0 ) 0 => noteIdx;
    if( noteIdx > 47 ) 47 => noteIdx;

    cMajor[noteIdx] * Math.pow(2.0, pitch / 12.0) * 0.25 => float freq;
    freq => sineOsc[i].freq;
    freq * 3.0 => sineHarm[i].freq;
    0.15 => sineOsc[i].gain;
    0.0 => sineHarm[i].gain;
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

    if( pkNoteCount < 8 ) {
        noteIdx => pkLastNotes[pkNoteCount];
        pkNoteCount + 1 => pkNoteCount;
    } else {
        for( 1 => int n; n < 8; n++ )
            pkLastNotes[n-1] => pkLastNotes[n];
        noteIdx => pkLastNotes[7];
    }
}

// kick loop runs on its own shred, fires the two sine oscs
// and sweeps their pitch down over 18ms for the thump
// the macro 0-50% controls volume, 50-100% speeds up the tempo
fun void kickLoop() {
    while( true ) {
        gKickMacro => float m;
        Math.min(m / 0.5, 1.0) => float vol;
        131.0 => float bpm;
        if( m > 0.5 ) 131.0 + (m - 0.5) / 0.5 * 50.0 => bpm;
        bpm => gBPM;

        if( m > 0.0 ) {
            vol * 0.432 => kickOut.gain;
            kickBodyEnv.keyOn();
            kickSubEnv.keyOn();
            kickClickEnv.keyOn();
            vol => scEnv;
            spawnKick + 1 => spawnKick;

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

// sine pad loop picks random notes from the scale and triggers them
// also updates active voice pitches in realtime when the pitch orb moves
fun void sineLoop() {
    now => time sineNextNote;
    while( true ) {
        gSineMacro => float m;
        Math.min(m * 2.0, 1.0) => float prob;
        0.9 => float vol;
        gPitch => float pitch;
        gBPM => float bpm;
        (60.0 / bpm)::second => dur wholeNote;

        for( 0 => int i; i < 24; i++ ) {
            if( svActive[i] ) {
                if( now - svTrigTime[i] > svLife[i] ) {
                    sineEnv[i].keyOff();
                    0.0 => sineAmp[i].gain;
                    0 => svActive[i];
                } else {
                    0.0 => float newFreq;
                    if( svNote[i] >= 0 )
                        cMajor[svNote[i]] * Math.pow(2.0, pitch / 12.0) * 0.25 => newFreq;
                    else
                        svFreq[i] * Math.pow(2.0, pitch / 12.0) => newFreq;
                    // slow vibrato (half intensity)
                    (now - svTrigTime[i]) / second => float elapsedV;
                    Math.sin(elapsedV * 0.5 * 6.2832) => float vibLFO;
                    newFreq * (1.0 + vibLFO * 0.004) => sineOsc[i].freq;
                    0.15 => sineOsc[i].gain;
                    0.0 => sineHarm[i].gain;
                }
            }
        }

        if( m > 0.0 && now >= sineNextNote ) {
            if( Math.random2f(0.0, 1.0) < prob * 0.9 ) {
                Math.random2(0, 39) => int noteIdx;
                triggerSineNote( noteIdx, vol, pitch );
            }
            if( Math.random2f(0.0, 1.0) < 0.75 )
                now + wholeNote / 4.0 => sineNextNote;
            else
                now + wholeNote / 2.0 => sineNextNote;
        }

        20::ms => now;
    }
}

// fires a big cmaj7 chord every 15 seconds, 10-14 notes spread
// across octaves 2-5 biased toward the higher registers
fun void chordLoop() {
    10::second => now;
    while( true ) {
        if( gSineMacro > 0.0 ) {
            gPitch => float pitch;
            Math.random2(10, 14) => int numNotes;
            for( 0 => int cn; cn < numNotes; cn++ ) {
                Math.random2(2, 5) => int oct;
                [0, 2, 4, 6] @=> int deg[];
                oct * 8 + deg[Math.random2(0, 3)] => int cNote;
                if( cNote > 47 ) 47 => cNote;
                triggerSineNote( cNote, 0.95, pitch );
            }
        }
        15::second => now;
    }
}

// 4 bird types each with 2 voices, they fire in bursts of chirps
// each chirp sweeps its frequency up or down with vibrato on top
// type 0 chirps up, type 1 chirps down, type 2 trills fast, type 3 warbles
// each voice gets its own stereo pan position for wide spatial spread
fun void birdLoop() {
    for( 0 => int t; t < 4; t++ ) {
        now + Math.random2(500, 2000)::ms => bsNextCall[t];
        now => bsNextNote[t];
        0 => bsBurstLeft[t];
    }

    while( true ) {
        gBirdMacro => float m;
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
                        // per-voice pan, full stereo field
                        Math.random2f(-1.0, 1.0) => float bpan;
                        bpan => birdPanV[v].pan;
                        bpan => bvPan[v];
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
        15::ms => now;
    }
}

// ocean waves use slow sine-modulated filter cutoff with occasional
// longer sweeps where the lpf opens up then closes back down
fun void wavesLoop() {
    for( 0 => int ch; ch < 4; ch++ ) {
        Math.random2f(0.05, 0.15) => wvPhaseRate[ch];
        Math.random2f(0.0, 6.28) => wvPhase[ch];
        now + Math.random2(10, 30)::second => wvNextSweep[ch];
        0 => wvSweeping[ch];
    }
    while( true ) {
        gWavesMacro => float m;
        Math.min(m / 0.4, 1.0) => float prob;
        0.3 => float vol;
        if( m > 0.4 ) 0.3 + (m - 0.4) / 0.6 * 0.7 => vol;
        gPitch => float pitch;
        0.016 => float dt;
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
        25::ms => now;
    }
}

// thunder fires rumble events that sweep the filter up then back down
// fast percussive attack followed by long random decay, the gain
// wobbles slightly during the tail for organic texture
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
                            (p / 0.06) * (p / 0.06) * thMaxGain[ch] => g;
                        } else {
                            thMaxGain[ch] * Math.pow(1.0 - p, 0.6) => float baseG;
                            (0.6 + Math.random2f(0.0, 0.4)) * baseG => g;
                        }
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
        75::ms => now;
    }
}

// rain: hiss + boil bursts + Minnaert-model individual drops
fun void rainLoop() {
    while( true ) {
        gRainMacro => float m;
        0.7 => float vol;

        m * m * 2.0 => float hissGain;
        1200.0 + m * 3000.0 => rainHissFilt.freq;
        hissGain * 0.014 => rainHissGain.gain;

        for( 0 => int i; i < 4; i++ ) {
            if( rbActive[i] && now - rbTrigTime[i] > rbLife[i] ) {
                rainBoilEnv[i].keyOff();
                0.0 => rainBoilAmp[i].gain;
                0 => rbActive[i];
            }
        }
        for( 0 => int i; i < 8; i++ ) {
            if( rvActive[i] && now - rvTrigTime[i] > rvLife[i] ) {
                rainImpactEnv[i].keyOff();
                rainBubbleEnv[i].keyOff();
                0.0 => rainImpactAmp[i].gain;
                0.0 => rainBubbleAmp[i].gain;
                0 => rvActive[i];
            }
        }

        if( m > 0.0 ) {
            Math.random2(0, 1 + (m * m * 2.0) $ int) => int boilCount;
            for( 0 => int b; b < boilCount; b++ ) {
                if( Math.random2f(0.0, 1.0) < m * m * 0.85 ) {
                    findFreeBoil() => int bv;
                    if( bv >= 0 ) {
                        4000.0 + Math.pow(Math.random2f(0.0,1.0),0.6)*10000.0 => rainBoilFilt[bv].freq;
                        2.5 + Math.random2f(0.0, 3.0) => rainBoilFilt[bv].Q;
                        rainBoilEnv[bv].keyOn();
                        (0.003 + Math.random2f(0.0,0.005)) * m * vol => rainBoilAmp[bv].gain;
                        1 => rbActive[bv];
                        now => rbTrigTime[bv];
                        7::ms => rbLife[bv];
                    }
                }
            }
            for( 0 => int ch; ch < 4; ch++ ) {
                if( Math.random2f(0.0, 1.0) < m * m * 0.9 ) {
                    findFreeRain(ch) => int v;
                    if( v >= 0 ) {
                        Math.pow(Math.random2f(0.0,1.0), 2.2) => float dropSize;
                        9000.0 - dropSize*7500.0 + Math.random2f(-600.0,600.0) => float impactFreq;
                        if( impactFreq < 1000.0 ) 1000.0 => impactFreq;
                        if( impactFreq > 11000.0 ) 11000.0 => impactFreq;
                        1.2 + (1.0-dropSize)*2.8 => float impactQ;
                        Math.random2f(0.3, 0.8) => float atk;
                        3.0 + dropSize*57.0 + Math.random2f(-2.0,6.0) => float dec;
                        impactFreq => rainImpactFilt[v].freq;
                        impactQ => rainImpactFilt[v].Q;
                        rainImpactEnv[v].set( atk::ms, dec::ms, 0.0, 3::ms );
                        rainImpactEnv[v].keyOn();
                        (0.009 + Math.random2f(0.0,0.016)) * (vol+0.2) => rainImpactAmp[v].gain;
                        if( Math.random2f(0.0,1.0) < 0.25 + dropSize*0.65 ) {
                            800.0 + (1.0-dropSize)*(1.0-dropSize)*14200.0
                                + Math.random2f(-200.0,200.0)*(1.0+dropSize*3.0) => float bubFreq;
                            if( bubFreq < 600.0 ) 600.0 => bubFreq;
                            if( bubFreq > 16000.0 ) 16000.0 => bubFreq;
                            (4.0 + dropSize*26.0)::ms => dur bubTau;
                            bubFreq => rainBubble[v].freq;
                            rainBubbleEnv[v].set( 0.3::ms, bubTau, 0.0, 3::ms );
                            rainBubbleEnv[v].keyOn();
                            (0.002 + dropSize*0.012) * (vol+0.2) => rainBubbleAmp[v].gain;
                        } else {
                            0.0 => rainBubbleAmp[v].gain;
                        }
                        Math.random2f(-0.9, 0.9) => float dropPan;
                        dropPan => rainPan.pan;
                        1 => rvActive[v];
                        now => rvTrigTime[v];
                        (atk + dec + 5.0)::ms => rvLife[v];
                        if( rainDropCount < 32 ) {
                            dropPan => rainDropX[rainDropCount];
                            1.0 => rainDropY[rainDropCount];
                            rainDropCount + 1 => rainDropCount;
                        }
                    }
                }
            }
        } else {
            0.0 => rainHissGain.gain;
        }
        10::ms => now;
    }
}

// pluck arp uses fm synthesis, the modulator frequency is a whole
// number ratio of the carrier so it sounds metallic and bell-like
// notes jump around the scale based on what the sine pads played
fun void pluckLoop() {
    0 => pkVoice;
    now => pkNextNote;
    10 => pkLastNotes[0]; 14 => pkLastNotes[1];
    18 => pkLastNotes[2]; 22 => pkLastNotes[3];
    4 => pkNoteCount;
    while( true ) {
        Math.max(0.0, (gSineMacro - 0.5) * 2.0) => float m;
        0.9 => float vol;
        m => float plkProb;
        gPitch => float pitch;
        gBPM => float bpm;
        (60.0 / bpm / 4.0)::second => dur sixteenth;
        if( m > 0.0 && now >= pkNextNote && pkNoteCount > 0 ) {
            pkLastNotes[Math.random2(0, pkNoteCount - 1)] + Math.random2(-8, 8) => int noteIdx;
            if( noteIdx < 0 ) 0 => noteIdx;
            if( noteIdx > 47 ) 47 => noteIdx;
            cMajor[noteIdx] * Math.pow(2.0, pitch / 12.0) => float freq;
            fmRatios[pkVoice] => float ratio;
            freq * ratio => pluckMod[pkVoice].freq;
            freq * ratio * Math.random2f(0.5, 2.0) => pluckMod[pkVoice].gain;
            freq => pluckCar[pkVoice].freq;
            freq * 3.0 + 500.0 => float fCut;
            if( fCut > 6000.0 ) 6000.0 => fCut;
            fCut => pluckFilt[pkVoice].freq;
            (200.0 - m * 197.0)::ms => dur atkDur;
            (600.0 - m * 520.0)::ms => dur decDur;
            pluckEnv[pkVoice].set( atkDur, decDur, 0.0, 20::ms );
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
spork ~ birdLoop();
spork ~ wavesLoop();
spork ~ thunderLoop();
spork ~ rainLoop();
// visuals

GWindow.title( "V I E B S" );
GWindow.windowed( 1100, 700 );
GG.camera().posZ( 6.0 );

GG.bloom( 1 );
GG.bloomPass().intensity( 2.2 );
GG.bloomPass().radius( 0.3 );
GG.bloomPass().threshold( 0.0 );
GG.bloomPass().levels( 1 );

// background plane
GPlane bg --> GG.scene();
bg.sca( 40.0 );
bg.posZ( -2.5 );
bg.color( @(0.005, 0.003, 0.010) );

// background shapes - mix of circles and rectangles, varied aspect ratios
// some thin ones rotated look like diamonds and triangles
// slowly drifting, fading in and out over 10-30 second cycles
5 => int NUM_BG_POLY;
GCircle bgCirc[2];
GPlane bgRect[3];
float bpX[5], bpY[5], bpVX[5], bpVY[5], bpPhase[5], bpRotSpd[5];
float bpBaseSca[5], bpAspX[5], bpAspY[5];
float bpR[5], bpG[5], bpB[5], bpLife[5], bpMaxLife[5];

for( 0 => int i; i < 5; i++ ) {
    if( i < 2 ) {
        bgCirc[i] --> GG.scene();
        bgCirc[i].posZ( -2.0 + Math.random2f(0.0, 0.4) );
    } else {
        bgRect[i-2] --> GG.scene();
        bgRect[i-2].posZ( -2.0 + Math.random2f(0.0, 0.4) );
    }
    Math.random2f(-4.5, 4.5) => bpX[i];
    Math.random2f(-3.5, 3.5) => bpY[i];
    Math.random2f(-0.008, 0.008) => bpVX[i];
    Math.random2f(-0.006, 0.006) => bpVY[i];
    Math.random2f(0.0, 6.28) => bpPhase[i];
    Math.random2f(0.06, 0.2) => bpRotSpd[i];
    if( Math.random2f(0.0, 1.0) < 0.5 ) -1.0 * bpRotSpd[i] => bpRotSpd[i];
    Math.random2f(1.2, 3.5) => bpBaseSca[i];
    Math.random2f(0.4, 1.4) => bpAspX[i];
    Math.random2f(0.4, 1.4) => bpAspY[i];
    Math.random2f(0.0, 6.28) => float bpHue;
    0.020 + 0.04 * Math.max(0.0, Math.sin(bpHue)) => bpR[i];
    0.015 + 0.035 * Math.max(0.0, Math.sin(bpHue + 2.09)) => bpG[i];
    0.020 + 0.045 * Math.max(0.0, Math.sin(bpHue + 4.19)) => bpB[i];
    Math.random2f(15.0, 40.0) => bpMaxLife[i];
    Math.random2f(0.0, bpMaxLife[i]) => bpLife[i];
    if( i < 2 ) {
        bgCirc[i].posX( bpX[i] );
        bgCirc[i].posY( bpY[i] );
        bgCirc[i].scaX( bpBaseSca[i] * bpAspX[i] );
        bgCirc[i].scaY( bpBaseSca[i] * bpAspY[i] );
        bgCirc[i].rotZ( Math.random2f(0.0, 6.28) );
    } else {
        bgRect[i-2].posX( bpX[i] );
        bgRect[i-2].posY( bpY[i] );
        bgRect[i-2].scaX( bpBaseSca[i] * bpAspX[i] );
        bgRect[i-2].scaY( bpBaseSca[i] * bpAspY[i] );
        bgRect[i-2].rotZ( Math.random2f(0.0, 6.28) );
    }
}

// kick circle, single dark blue
1 => int KICK_VP;
GCircle kickShape[1];
float ksLife[1], ksMaxLife[1];
float ksR[1], ksG[1], ksB[1];
0 => int ksHead;

kickShape[0] --> GG.scene();
kickShape[0].posZ( -2.3 );
kickShape[0].sca( 0.0 );
0.0 => ksLife[0];

// particles removed for iPhone perf

// bird triangles, thin elongated planes that follow the audio pan position
8 => int BIRD_VP;
GPlane birdTri[8];
float bdLife[8], bdMaxLife[8];
float bdX[8], bdY[8];
float bdRotBase[8];

for( 0 => int i; i < 8; i++ ) {
    birdTri[i] --> GG.scene();
    birdTri[i].posZ( 0.9 );
    birdTri[i].sca( 0.0 );
    0.0 => bdLife[i];
    Math.random2f(0.0, 6.28) => bdRotBase[i];
}

// rain drops visual pool
32 => int RAIN_VP;
GCircle rainDrop[32];
float rdLife[32], rdMaxLife[32];
float rdX[32], rdY[32], rdVX[32], rdVY[32], rdSz[32];

for( 0 => int i; i < 32; i++ ) {
    rainDrop[i] --> GG.scene();
    rainDrop[i].posZ( 0.5 );
    rainDrop[i].sca( 0.0 );
    0.0 => rdLife[i];
}

fun void spawnVisualRainDrop( float normX, float normY, float hW, float hH ) {
    -1 => int i;
    for( 0 => int s; s < 32; s++ ) {
        if( rdLife[s] <= 0.0 ) { s => i; break; }
    }
    if( i < 0 ) return;
    hH * 2.0 * 0.65 => float targetDist;
    Math.random2f(0.7, 1.0) * targetDist => float dist;
    2.5 => float speed;
    dist / speed => float life;
    life => rdLife[i];
    life => rdMaxLife[i];
    normX * hW => rdX[i];
    hH * 0.95 => rdY[i];
    Math.random2f(-0.15, 0.15) => rdVX[i];
    -1.0 * speed => rdVY[i];
    Math.random2f(0.02, 0.04) => rdSz[i];
}

fun void spawnKickVisual( float hW, float hH ) {
    ksHead => int i;
    (ksHead + 1) % KICK_VP => ksHead;
    0.32 => ksLife[i];
    0.32 => ksMaxLife[i];
    0.0 => ksR[i];
    0.015 => ksG[i];
    0.12 => ksB[i];
}

// bird triangle spawn, position reflects the stereo pan position
fun void spawnBirdDot( int voice, float freq, float pan, float hW, float hH ) {
    voice => int i;
    if( i < 0 || i >= BIRD_VP ) return;
    0.5 => bdLife[i];
    0.5 => bdMaxLife[i];
    pan * 0.85 * hW => bdX[i];
    Math.random2f(-0.2, 0.6) * hH => bdY[i];
    Math.random2f(0.0, 6.28) => bdRotBase[i];
}

// 6 control orbs: PITCH KICK SIN BIRD WAVES RAIN (ARP+THUNDER merged into SIN/RAIN)
6 => int NUM_CTRL;
GCircle ctrlBody[6];
GCircle ctrlInner[6];

// blue→yellow gradient left to right: deep blue, indigo, teal, green, warm, gold
[0.15, 0.30, 0.20, 0.40, 0.65, 0.85] @=> float ctrlCR[];
[0.22, 0.25, 0.45, 0.50, 0.55, 0.75] @=> float ctrlCG[];
[0.70, 0.60, 0.55, 0.35, 0.25, 0.10] @=> float ctrlCB[];

float ctrlVal[6];
[0.458, 0.33, 0.80, 0.75, 0.25, 0.50] @=> float ctrlDefaults[];

float orbSwayPh[6];
float orbSwayRt[6];

for( 0 => int i; i < 6; i++ ) {
    ctrlDefaults[i] => ctrlVal[i];
    Math.random2f(0.0, 6.28) => orbSwayPh[i];
    Math.random2f(0.3, 0.8) => orbSwayRt[i];
    ctrlBody[i] --> GG.scene();
    ctrlBody[i].posZ( 0.02 );
    ctrlInner[i] --> GG.scene();
    ctrlInner[i].posZ( 0.03 );
}

GText ctrlLabel[6];
["PITCH", "KICK", "WAVES", "SIN", "STORM", "BIRD"] @=> string labelText[];
for( 0 => int i; i < 6; i++ ) {
    ctrlLabel[i] --> GG.scene();
    ctrlLabel[i].posZ( 0.04 );
    ctrlLabel[i].text( labelText[i] );
    ctrlLabel[i].sca( 0.1 );
    ctrlLabel[i].color( @(0.85, 0.85, 0.85) );
}

// orb trails
12 => int TRAIL_COUNT;
GCircle orbTrail[12];
float trLife[12], trMaxLife[12];
float trX[12], trY[12], trSz[12];
float trR[12], trG[12], trB[12];
0 => int trHead;

for( 0 => int i; i < 12; i++ ) {
    orbTrail[i] --> GG.scene();
    orbTrail[i].posZ( 0.005 );
    orbTrail[i].sca( 0.0 );
    0.0 => trLife[i];
}

-1 => int grabIdx;
0.0 => float grabOffsetY;

0.0 => float globalTime;
0 => int frameCount;

while( true ) {
    GG.nextFrame() => now;
    GG.dt() => float dt;
    globalTime + dt => globalTime;
    frameCount + 1 => frameCount;

    GG.windowWidth() $ float => float winW;
    GG.windowHeight() $ float => float winH;
    if( winW < 10.0 ) 10.0 => winW;
    if( winH < 10.0 ) 10.0 => winH;
    winW / winH => float aspect;
    6.0 => float camZ;
    camZ * Math.tan( 22.5 * 3.14159265 / 180.0 ) => float halfH;
    halfH * aspect => float halfW;

    GWindow.mousePos() => vec2 mpos;
    GWindow.mouseLeft() => int mDown;

    mpos.x / winW => float mNormX;
    mpos.y / winH => float mNormY;

    float orbNormX[6];
    float orbNormY[6];
    for( 0 => int i; i < 6; i++ ) {
        0.08 + 0.84 * (i $ float) / 5.0
            + Math.sin(globalTime * orbSwayRt[i] + orbSwayPh[i]) * 0.028
            + Math.sin(globalTime * orbSwayRt[i] * 2.3 + orbSwayPh[i] * 0.7) * 0.012
            => orbNormX[i];
        0.82 - ctrlVal[i] * 0.74
            + Math.sin(globalTime * orbSwayRt[i] * 0.6 + orbSwayPh[i] + 1.5) * 0.015
            + Math.cos(globalTime * orbSwayRt[i] * 1.7 + orbSwayPh[i] * 1.3) * 0.008
            => orbNormY[i];
    }

    float orbDispX[6];
    float orbDispY[6];
    for( 0 => int i; i < 6; i++ ) {
        (orbNormX[i] - 0.5) * 2.0 * halfW => orbDispX[i];
        (0.5 - orbNormY[i]) * 2.0 * halfH => orbDispY[i];
    }

    Math.min(winW, winH) / 700.0 => float winScale;
    if( winScale > 1.5 ) 1.5 => winScale;
    if( winScale < 0.5 ) 0.5 => winScale;

    if( mDown && grabIdx < 0 ) {
        -1 => int closest;
        999.0 => float closestDist;
        for( 0 => int i; i < 6; i++ ) {
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

    -12.0 + ctrlVal[0] * 24.0 => gPitch;
    ctrlVal[1] => gKickMacro;
    ctrlVal[2] => gWavesMacro;
    ctrlVal[3] => gSineMacro;
    ctrlVal[4] => gThunderMacro;
    ctrlVal[4] => gRainMacro;
    ctrlVal[5] => gBirdMacro;

    // sidechain ducking from the kick, gentle so it doesn't pump too hard
    scSmooth + (scEnv - scSmooth) * Math.min(1.0, 50.0 * dt) => scSmooth;
    scEnv * Math.exp( -2.0 * dt ) => scEnv;
    if( scEnv < 0.003 ) 0.0 => scEnv;
    if( scSmooth < 0.003 ) 0.0 => scSmooth;

    Math.min(gKickMacro / 0.5, 1.0) => float kickVol;
    kickVol * scSmooth => float rawDuck;
    if( rawDuck > 1.0 ) 1.0 => rawDuck;
    Math.pow(rawDuck, 0.5) * 0.15 => float duck;
    1.0 - duck => gScMult;

    // apply sidechain to all the instrument buses
    Math.min(gSineMacro * 2.0, 1.0) * 1.5 * gScMult => float sineGainMul;
    for( 0 => int si; si < 24; si++ ) {
        if( svActive[si] ) svAmp[si] * sineGainMul => sineAmp[si].gain;
    }
    Math.max(0.0, (gBirdMacro - 0.25) / 0.75) * 1.25 * gScMult => float birdGainVal;
    birdGainVal => birdL.gain;
    birdGainVal => birdR.gain;
    0.88 * gScMult => rainBus.gain;
    0.0 => pluckBus.gain;

    // sidechain visual pulse, subtle
    1.0 + duck * 0.4 => float scPulse;
    1.0 + duck * 0.5 => float scBright;

    // kick: grow and shrink circle only
    while( spawnKick > 0 ) {
        spawnKickVisual( halfW, halfH );
        spawnKick - 1 => spawnKick;
    }

    // sine: no particles — sineBright handles visual response
    0 => sineSpawnCount;
    0 => spawnSine;

    // bird: spawn triangle at stereo pan position
    if( spawnBird > 0 ) {
        for( 0 => int i; i < 8; i++ ) {
            if( bvActive[i] && now - bvTrigTime[i] < 80::ms ) {
                spawnBirdDot( i, bvBaseFreq[i], bvPan[i], halfW, halfH );
            }
        }
    }
    0 => spawnBird;

    0 => spawnThunder;
    0 => spawnPluck;
    0 => pluckSpawnCount;

    while( rainDropCount > 0 ) {
        rainDropCount - 1 => rainDropCount;
        spawnVisualRainDrop( rainDropX[rainDropCount], rainDropY[rainDropCount], halfW, halfH );
    }
    if( gRainMacro > 0.0 ) {
        gRainMacro * gRainMacro * 3.0 => float extraF;
        extraF $ int => int extraDrops;
        if( Math.random2f(0.0, 1.0) < (extraF - extraDrops) ) extraDrops + 1 => extraDrops;
        for( 0 => int d; d < extraDrops; d++ ) {
            spawnVisualRainDrop( Math.random2f(-1.0, 1.0), 1.0, halfW, halfH );
        }
    }

    // rain drops falling
    for( 0 => int i; i < 32; i++ ) {
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
                1.0 => float alpha;
                if( lifeLeft < 0.15 ) lifeLeft / 0.15 => alpha;
                rainDrop[i].sca( rdSz[i] * alpha );
                0.9 * alpha => float rb;
                rainDrop[i].color( @(0.78 * rb, 0.82 * rb, 0.93 * rb) );
            }
        }
    }

    // kick circle update
    for( 0 => int i; i < KICK_VP; i++ ) {
        if( ksLife[i] > 0.0 ) {
            ksLife[i] - dt => ksLife[i];
            if( ksLife[i] <= 0.0 ) {
                0.0 => ksLife[i];
                kickShape[i].sca( 0.0 );
            } else {
                ksMaxLife[i] - ksLife[i] => float elapsed;
                0.0 => float env;
                if( elapsed < 0.015 ) {
                    elapsed / 0.015 => env;
                } else {
                    1.0 - (elapsed - 0.015) / 0.305 => env;
                    if( env < 0.0 ) 0.0 => env;
                }

                env * 9.0 * winScale => float sz;
                kickShape[i].sca( sz );
                kickShape[i].posX( 0.0 );
                kickShape[i].posY( 0.0 );
                0.06 + Math.pow(env, 0.8) * 0.06 => float kbright;
                kickShape[i].color( @(
                    ksR[i] * kbright,
                    ksG[i] * kbright,
                    ksB[i] * kbright
                ) );
            }
        }
    }

    // bird triangles, small yellow shapes that pop at the pan position
    for( 0 => int i; i < BIRD_VP; i++ ) {
        if( bdLife[i] > 0.0 ) {
            bdLife[i] - dt => bdLife[i];
            if( bdLife[i] <= 0.0 ) {
                0.0 => bdLife[i];
                birdTri[i].sca( 0.0 );
            } else {
                bdLife[i] / bdMaxLife[i] => float t;
                0.0 => float env;
                if( t > 0.85 ) (1.0 - t) / 0.15 => env;
                else t / 0.85 => env;
                0.4 * winScale * env => float sz;
                birdTri[i].scaX( sz * 0.4 );
                birdTri[i].scaY( sz );
                birdTri[i].posX( bdX[i] + Math.random2f(-0.03, 0.03) );
                birdTri[i].posY( bdY[i] + Math.random2f(-0.03, 0.03) );
                birdTri[i].rotZ( bdRotBase[i] + Math.random2f(-0.2, 0.2) );
                env * 0.8 => float bbright;
                birdTri[i].color( @(1.0 * bbright, 0.92 * bbright, 0.08 * bbright) );
            }
        }
    }

    // background polygons, slowly drifting with color cycling
    gThunderMacro * 0.02 => float thColorBoost;
    1.0 + duck * 0.08 => float kickGrow;
    0.0 => float rawSineAmp;
    for( 0 => int si; si < 24; si++ ) {
        if( svActive[si] ) rawSineAmp + svAmp[si] => rawSineAmp;
    }
    Math.min(rawSineAmp * 2.0, 1.0) * 0.5 => float sineBright;
    for( 0 => int i; i < 5; i++ ) {
        bpLife[i] - dt => bpLife[i];
        if( bpLife[i] <= 0.0 ) {
            Math.random2f(15.0, 40.0) => bpMaxLife[i];
            bpMaxLife[i] => bpLife[i];
            Math.random2f(-halfW, halfW) => bpX[i];
            Math.random2f(-halfH, halfH) => bpY[i];
            Math.random2f(-0.008, 0.008) => bpVX[i];
            Math.random2f(-0.006, 0.006) => bpVY[i];
            Math.random2f(0.06, 0.2) => bpRotSpd[i];
            if( Math.random2f(0.0, 1.0) < 0.5 ) -1.0 * bpRotSpd[i] => bpRotSpd[i];
            Math.random2f(1.2, 3.5) => bpBaseSca[i];
            Math.random2f(0.4, 1.4) => bpAspX[i];
            Math.random2f(0.4, 1.4) => bpAspY[i];
            Math.random2f(0.0, 6.28) => float h;
            0.020 + 0.04 * Math.max(0.0, Math.sin(h)) => bpR[i];
            0.015 + 0.035 * Math.max(0.0, Math.sin(h + 2.09)) => bpG[i];
            0.020 + 0.045 * Math.max(0.0, Math.sin(h + 4.19)) => bpB[i];
        }
        bpPhase[i] + dt * 0.2 => bpPhase[i];
        bpX[i] + bpVX[i] * dt => bpX[i];
        bpY[i] + bpVY[i] * dt => bpY[i];
        bpLife[i] / bpMaxLife[i] => float lifeRatio;
        1.0 - lifeRatio => float age;
        1.0 => float fade;
        if( age < 0.15 ) age / 0.15 => fade;
        if( lifeRatio < 0.15 ) lifeRatio / 0.15 => fade;

        bpR[i] * fade * 0.525 * (1.0 + sineBright) + thColorBoost * 0.14 * fade => float cr;
        bpG[i] * fade * 0.525 * (1.0 + sineBright) + Math.sin(bpPhase[i] + 2.0) * 0.004 * fade => float cg;
        bpB[i] * fade * 0.525 * (1.0 + sineBright) + thColorBoost * 0.10 * fade => float cb;

        if( i < 2 ) {
            bgCirc[i].posX( bpX[i] );
            bgCirc[i].posY( bpY[i] );
            bgCirc[i].rotZ( globalTime * bpRotSpd[i] + bpPhase[i] );
            bgCirc[i].scaX( bpBaseSca[i] * bpAspX[i] * kickGrow );
            bgCirc[i].scaY( bpBaseSca[i] * bpAspY[i] * kickGrow );
            bgCirc[i].color( @(cr, cg, cb) );
        } else {
            bgRect[i-2].posX( bpX[i] );
            bgRect[i-2].posY( bpY[i] );
            bgRect[i-2].rotZ( globalTime * bpRotSpd[i] + bpPhase[i] );
            bgRect[i-2].scaX( bpBaseSca[i] * bpAspX[i] * kickGrow );
            bgRect[i-2].scaY( bpBaseSca[i] * bpAspY[i] * kickGrow );
            bgRect[i-2].color( @(cr, cg, cb) );
        }
    }

    // background color, deep purple-blue with subtle kick pulse and thunder warmth
    bg.color( @(
        0.005 + duck * 0.002 + thColorBoost * 0.08,
        0.003 + duck * 0.001 + thColorBoost * 0.012,
        0.010 + duck * 0.004 + thColorBoost * 0.12
    ) );

    // spawn orb trail particles
    if( frameCount % 10 == 0 ) {
        for( 0 => int i; i < 6; i++ ) {
            trHead => int ti;
            (trHead + 1) % 12 => trHead;
            0.6 => trLife[ti];
            0.6 => trMaxLife[ti];
            orbDispX[i] => trX[ti];
            orbDispY[i] => trY[ti];
            0.33 * winScale * 0.5 => trSz[ti];
            ctrlCR[i] => trR[ti];
            ctrlCG[i] => trG[ti];
            ctrlCB[i] => trB[ti];
        }
    }

    // update trail particles
    for( 0 => int i; i < 12; i++ ) {
        if( trLife[i] > 0.0 ) {
            trLife[i] - dt => trLife[i];
            if( trLife[i] <= 0.0 ) {
                0.0 => trLife[i];
                orbTrail[i].sca( 0.0 );
            } else {
                trLife[i] / trMaxLife[i] => float alpha;
                alpha * alpha => float fade;
                orbTrail[i].posX( trX[i] );
                orbTrail[i].posY( trY[i] );
                orbTrail[i].sca( trSz[i] * fade );
                fade * 0.08 => float tb;
                orbTrail[i].color( @(trR[i] * tb, trG[i] * tb, trB[i] * tb) );
            }
        }
    }

    // control orbs
    1.1 * winScale => float orbFixedSz;
    for( 0 => int i; i < 6; i++ ) {
        ctrlVal[i] => float norm;
        0.5 + norm * 0.5 => float obright;
        orbFixedSz => float thisSz;
        0.02 => float thisZ;

        Math.random2f(-0.006, 0.006) => float ojx;
        Math.random2f(-0.006, 0.006) => float ojy;

        ctrlBody[i].posX( orbDispX[i] + ojx );
        ctrlBody[i].posY( orbDispY[i] + ojy );
        ctrlBody[i].posZ( thisZ );
        ctrlBody[i].sca( thisSz );
        ctrlBody[i].color( @(
            ctrlCR[i] * 0.45 * obright,
            ctrlCG[i] * 0.45 * obright,
            ctrlCB[i] * 0.45 * obright
        ) );

        ctrlInner[i].posX( orbDispX[i] + ojx );
        ctrlInner[i].posY( orbDispY[i] + ojy );
        ctrlInner[i].posZ( thisZ + 0.01 );
        ctrlInner[i].sca( thisSz * 0.4 );
        ctrlInner[i].color( @(
            (0.12 + ctrlCR[i] * 0.3) * obright,
            (0.12 + ctrlCG[i] * 0.3) * obright,
            (0.12 + ctrlCB[i] * 0.3) * obright
        ) );

        (0.08 + 0.84 * (i $ float) / 5.0 - 0.5) * 2.0 * halfW => float labelX;
        ctrlLabel[i].posX( labelX );
        ctrlLabel[i].posY( -halfH * 0.92 );
        halfW * 0.06 => float lblSca;
        if( lblSca > 0.27 ) 0.27 => lblSca;
        ctrlLabel[i].sca( lblSca );
    }
}
