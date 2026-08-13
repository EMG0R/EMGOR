// CIESEN — generative ambient synth (audio-only build)
//
// This is the audio engine from _CIESEN.ck with every ChuGL/WebGPU visual
// call stripped out, so it runs on the plain AudioWorklet-based `webchuck`
// package (no SharedArrayBuffer / COOP+COEP required, works on iPhone).
// Visuals are drawn separately in JS on a plain <canvas> in ciesen.html —
// this file exposes global variables as the audio<->visual bridge:
//
//   settable from JS (chuck.setFloat), 0..1 normalized:
//     gCtrlPitch, gCtrlKick, gCtrlWaves, gCtrlSin, gCtrlStorm, gCtrlBird
//
//   polled from JS (chuck.getFloat / getInt / getFloatArray):
//     gKickCount (int, increments per kick), gKickIntensity (float 0..1)
//     gBirdCount (int, increments per chirp), gBirdPanBuf[8], gBirdFreqBuf[8]
//     gRainCount (int, increments per drop), gRainPanBuf[32]
//     gThunderLevel (float 0..1, continuous), gWaveLevel (float 0..1, continuous)
//     gPadBright (float 0..1, continuous — sine-pad brightness)
//
// The full ChuGL/WebGPU version (with its own copy of this same audio
// engine, inline) lives at ../ciesen-gl.html / _CIESEN.ck.
//
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
1.8 => masterL.gain;
1.8 => masterR.gain;

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

// ── JS-facing control inputs (normalized 0..1, settable via chuck.setFloat) ──
global float gCtrlPitch; 0.458 => gCtrlPitch;
global float gCtrlKick;  0.33  => gCtrlKick;
global float gCtrlWaves; 0.80  => gCtrlWaves;
global float gCtrlSin;   0.75  => gCtrlSin;
global float gCtrlStorm; 0.25  => gCtrlStorm;
global float gCtrlBird;  0.50  => gCtrlBird;

// ── JS-facing event/level outputs (polled via chuck.getFloat/getInt/getFloatArray) ──
global int   gKickCount;      0 => gKickCount;
global float gKickIntensity;  0.0 => gKickIntensity;
global float gBirdPanBuf[8];
global float gBirdFreqBuf[8];
global int   gBirdCount;      0 => gBirdCount;
global float gRainPanBuf[32];
global int   gRainCount;      0 => gRainCount;
global float gThunderLevel;   0.0 => gThunderLevel;
global float gWaveLevel;      0.0 => gWaveLevel;
global float gPadBright;      0.0 => gPadBright;

// maps JS-settable control globals onto the internal macro variables the
// original ChuGL-integrated version updated once per render frame
fun void controlLoop() {
    while( true ) {
        -12.0 + gCtrlPitch * 24.0 => gPitch;
        gCtrlKick => gKickMacro;
        gCtrlWaves => gWavesMacro;
        gCtrlSin => gSineMacro;
        gCtrlStorm => gThunderMacro;
        gCtrlStorm => gRainMacro;
        gCtrlBird => gBirdMacro;
        16::ms => now;
    }
}

// tracks overall pad brightness (sum of active sine-pad amplitudes) so
// JS can drive the background-shape glow the way the ChuGL version did
fun void brightnessLoop() {
    while( true ) {
        0.0 => float rawSineAmp;
        for( 0 => int si; si < 24; si++ )
            if( svActive[si] ) rawSineAmp + svAmp[si] => rawSineAmp;
        Math.min(rawSineAmp * 2.0, 1.0) * 0.5 => gPadBright;
        30::ms => now;
    }
}

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
            gKickCount + 1 => gKickCount;
            vol => gKickIntensity;

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
                    newFreq * (1.0 + vibLFO * 0.0075) => sineOsc[i].freq;
                    newFreq * 3.0 * (1.0 + vibLFO * 0.0075) => sineHarm[i].freq;
                    // locked-in timbre: harmonic fades in at upper SIN
                    Math.max(0.0, (gSineMacro - 0.3) * 1.43) => float morphAmt;
                    if( morphAmt > 1.0 ) 1.0 => morphAmt;
                    0.15 * (1.0 - morphAmt * 0.4) => sineOsc[i].gain;
                    0.08 * morphAmt => sineHarm[i].gain;
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

        10::ms => now;
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
                        bpan => gBirdPanBuf[gBirdCount % 8];
                        freq => gBirdFreqBuf[gBirdCount % 8];
                        gBirdCount + 1 => gBirdCount;
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
        0.0 => float wLevel;
        for( 0 => int ch; ch < 4; ch++ ) {
            if( m > 0.0 ) {
                0.85 * vol * gScMult => wavesGain[ch].gain;
                wavesGain[ch].gain() => float _waG;
                if( _waG > wLevel ) _waG => wLevel;
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
        wLevel => gWaveLevel;
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
        0.0 => float thLevel;
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
                        thunderGain[ch].gain() => float _thG;
                        if( _thG > thLevel ) _thG => thLevel;
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
        thLevel => gThunderLevel;
        50::ms => now;
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
                        dropPan => gRainPanBuf[gRainCount % 32];
                        gRainCount + 1 => gRainCount;
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
        20::ms => now;
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
        15::ms => now;
    }
}

spork ~ kickLoop();
spork ~ sineLoop();
spork ~ chordLoop();
spork ~ birdLoop();
spork ~ wavesLoop();
spork ~ thunderLoop();
spork ~ rainLoop();
spork ~ controlLoop();
spork ~ brightnessLoop();

// keep the main shred alive — WebChucK's getFloat/getInt/getFloatArray need
// the shred (and its globals) to stay resolvable for the life of the page.
// pluckLoop() is intentionally never sporked: the FM pluck arp is a muted
// voice, kept wired up but silent, exactly as in the original piece.
while( true ) { 1::second => now; }
