// supersaw.ck -- USS instrument: supersaw  (Emory Smith's first-week ChucK piece)
// Derived from inbox/uss_sources/3_supersaw_week1_CHomework1.ck (9/18/2025).
//
// The sound is the original's: 7 SawOsc voices (+ two chord-tone saws each) with the
// same detunes, random per-saw offsets and per-saw pan, the 165 bpm square arp over
// five chords with the -12..0 bend at both ends of every chord, then the 15-measure
// drum section (kick / snare / noise hats, sine / triangle / square slide gestures
// that fire with probability 0.4, the saw bass). The original is one long unrolled
// score that ends; here the same score is written with arrays and plays on a loop.
//
// What was ADDED (all neutral at the defaults tone 0.5, space 0, length 0.5, intensity 0.5):
//   * the 7 USS performance params through the pool hook (see uss/README.md)
//   * trig: a supersaw chord stab (the piece's own saw stack) whose root follows pitch
//   * a master gain (intensity), a lowpass / detune-width (tone), a reverb send (space),
//     and a limiter at the very end so nothing clips
// KNOWN QUIRKS KEPT ON PURPOSE (each saw Pan2 is wired to dac four times = +12 dB; reproduced as a x4 gain):
// in the original every chord section switches its chord
// stack to the up5 intervals (+4, +14) on its first step (swTme = 0), so the written
// up0..up4 intervals never sound; the hats go to the centre because panHat is not in
// their chain. Both are the sound of the piece.
//
// Run:  chuck $DEMIURGE_RESOURCES/params/chuck/DemiurgeParams.ck supersaw.ck
//       (the launcher puts the hook ahead of the file for you)

DemiurgeParams pool;
pool.init( "uss-supersaw", 0 );             // 0 -> env DEMIURGE_PARAM_PORT
"/uss-supersaw/" => string NS;
["density", "tone", "length", "space", "intensity", "pitch", "trig"] @=> string PN[];
[0.5, 0.5, 0.5, 0.0, 0.5, 0.5, 0.0] @=> float PD[];
for( 0 => int i; i < PN.size(); i++ ) pool.add( NS + PN[i], PD[i] );

0.5 => float dens;  0.5 => float tone;  0.5 => float lenP;  0.0 => float space;
0.5 => float inten; 0.5 => float pitchP; 0.0 => float trigP;
1.0 => float wid;                           // detune width (tone above 0.5)

// ---------------------------------------------------------------- graph
// voices -> tone filter (+ reverb send) -> master gain -> limiter -> dac.
// drums go straight to master (the original had them straight to dac).
Gain mixL; Gain mixR;
LPF lpfL => mixL;  LPF lpfR => mixR;
20000 => lpfL.freq => lpfR.freq;  0.707 => lpfL.Q => lpfR.Q;
Gain sendL => JCRev revL => mixL;  Gain sendR => JCRev revR => mixR;
lpfL => sendL;  lpfR => sendR;
1.0 => revL.mix => revR.mix;  0.0 => sendL.gain => sendR.gain;
Dyno limL; Dyno limR;
limL.limit(); limR.limit();
0.6 => limL.thresh => limR.thresh;
0.02 => limL.slopeAbove => limR.slopeAbove;
0.1::ms => limL.attackTime => limR.attackTime;
80::ms => limL.releaseTime => limR.releaseTime;
mixL => limL => dac.chan(0);  mixR => limR => dac.chan(1);

fun void toBus( UGen u ) { u => lpfL; u => lpfR; }

7 => int NS7;                               // numSaws
0.075 / NS7 => float sawG;
0.7 => float chdGFac;
SawOsc saw[NS7]; SawOsc chA[NS7]; SawOsc chB[NS7]; Pan2 pan[NS7];
[0.0, -0.08, -0.1, 0.0, 0.1, 0.07, 0.0] @=> float dt[];
float po[NS7];
for( 0 => int i; i < NS7; i++ )
{
    Math.random2f( -0.11, 0.11 ) => po[i];
    -1.0 + (2.0 * i / (NS7 - 1)) => pan[i].pan;
    4.0 => pan[i].gain;     // the original wires every Pan2 to dac four times (+12 dB); kept
    saw[i] => pan[i];  chA[i] => pan[i];  chB[i] => pan[i];
    pan[i].left => lpfL;  pan[i].right => lpfR;
}
SqrOsc sqArp => Pan2 panArp;  panArp.left => lpfL;  panArp.right => lpfR;
0.15 => sqArp.gain;

SqrOsc sqr; SqrOsc sqDown; TriOsc triUp; TriOsc triDown; SinOsc sin1; SinOsc sin2; SinOsc sin3; TriOsc oscGl;
toBus( sqr ); toBus( sqDown ); toBus( triUp ); toBus( triDown ); toBus( sin1 ); toBus( sin2 ); toBus( sin3 ); toBus( oscGl );
0 => sqr.gain => sqDown.gain => triUp.gain => triDown.gain => sin1.gain => sin2.gain => sin3.gain => oscGl.gain;

SinOsc kOsc => ADSR kEnv;      kEnv => mixL;  kEnv => mixR;   0 => kOsc.gain;
SinOsc snT1 => ADSR snEnv1;    SinOsc snT2 => ADSR snEnv2;    Noise snN => ADSR snEnv3;
snEnv1 => mixL; snEnv1 => mixR; snEnv2 => mixL; snEnv2 => mixR; snEnv3 => mixL; snEnv3 => mixR;
0 => snT1.gain => snT2.gain => snN.gain;
Noise hatN => ADSR hatEnv;     hatEnv => mixL;  hatEnv => mixR;   0 => hatN.gain;

// the chord stab (trig): the same detuned saw stack, own envelope + pans
SawOsc stR[NS7]; SawOsc stA[NS7]; SawOsc stB[NS7]; ADSR stEnv[NS7]; Pan2 stPan[NS7];
for( 0 => int i; i < NS7; i++ )
{
    -1.0 + (2.0 * i / (NS7 - 1)) => stPan[i].pan;
    stR[i] => stEnv[i]; stA[i] => stEnv[i]; stB[i] => stEnv[i];
    stEnv[i] => stPan[i];
    stPan[i].left => lpfL;  stPan[i].right => lpfR;
    stEnv[i].set( 4::ms, 300::ms, 0.0, 250::ms );
}

// ---------------------------------------------------------------- timing (original constants)
165.0 => float bpm;
60.0 / bpm => float beatSec;
beatSec::second => dur beat;
beat / 8 => dur noteD;
beat / 2 => dur eigthD;
1::ms => dur stepD;
beat / 1.5 => dur bendD;
(bendD / stepD) $ int => int steps;
(noteD / stepD) $ int => int noteSt;
200::ms => dur slideD;
(slideD / stepD) $ int => int slideSt;
(eigthD / stepD) $ int => int delSt;
(eigthD / stepD) $ int => int posSt;
Math.pow( 0.96, stepD / (150::ms / 50) ) => float kMult;

// ---------------------------------------------------------------- the piece's data
[[65,69,72,76],[62,69,74,78],[70,77,86,93],[67,70,74,81],[63,67,70,79]] @=> int chdN[][];
[65-24, 62-24, 58-24, 67-24, 63-24] @=> int roots[];
[64, 64, 64, 64, 128] @=> int chordNotes[];          // noteD multiples per chord
[2.5, 3.0, 3.0, 3.0, 3.0] @=> float chdG[];          // saw gain factor per chord
[3.0, 3.5, 3.5, 3.5, 3.5] @=> float chdG3[];         // ... for saw 3
4 => int UP5A;  14 => int UP5B;                      // up5a/up5b: what every chord actually sounds

fun float gOf( float g, float g3, int i ) { if( i == 3 ) return g3; return g; }

fun void silenceSaws()
{
    for( 0 => int i; i < NS7; i++ )
    {
        0 => saw[i].gain; 0 => saw[i].freq; 0 => chA[i].gain; 0 => chA[i].freq; 0 => chB[i].gain; 0 => chB[i].freq;
    }
}
fun void sawGains( float g, float g3, int chords, float chdMul )
{
    for( 0 => int i; i < NS7; i++ )
    {
        gOf( g, g3, i ) * sawG => saw[i].gain;
        if( chords ) { gOf( g, g3, i ) * sawG * chdGFac => chA[i].gain => chB[i].gain; }
        else { 0 => chA[i].gain => chB[i].gain; }
    }
}
fun void setSaws( float base, float off, int cA, int cB, int chords )
{
    for( 0 => int i; i < NS7; i++ )
    {
        (dt[i] + po[i]) * wid => float d;
        Std.mtof( base + off + d ) => saw[i].freq;
        if( chords )
        {
            Std.mtof( base + 12 + cA + off + d ) => chA[i].freq;
            Std.mtof( base + 12 + cB + off + d ) => chB[i].freq;
        }
    }
}
// the -12 -> 0 riser at the start and 0 -> -12 fall at the end of a span
fun float bendOff( int s, int tot )
{
    0.0 => float off;
    if( s < steps ) { (s $ float) / (steps - 1) => float t; -12.0 + t * 12.0 => off; }
    else if( s >= tot - steps ) { ((s - (tot - steps)) $ float) / (steps - 1) => float t; 0.0 + t * -12.0 => off; }
    return off;
}

// ---------------------------------------------------------------- params
fun float pow2( float x ) { return Math.pow( 2.0, x ); }
fun void applyParams()
{
    pool.get( NS + "density" ) => dens;
    pool.get( NS + "tone" ) => tone;
    pool.get( NS + "length" ) => lenP;
    pool.get( NS + "space" ) => space;
    pool.get( NS + "intensity" ) => inten;
    pool.get( NS + "pitch" ) => pitchP;
    // tone: below 0.5 darkens (lowpass 20k -> 400 Hz); above 0.5 widens the detune up to 2.5x
    if( tone >= 0.5 ) { 20000.0 => lpfL.freq => lpfR.freq; 1.0 + (tone - 0.5) * 3.0 => wid; }
    else { 20000.0 * Math.pow( 0.02, (0.5 - tone) * 2.0 ) => lpfL.freq => lpfR.freq; 1.0 => wid; }
    space * 0.6 => sendL.gain => sendR.gain;
    pow2( (inten - 0.5) * 2.0 ) => mixL.gain => mixR.gain;
}

// one chord stab: the piece's chord stack, root picked by pitch (5 chord roots x 3 octaves)
[34, 38, 39, 41, 43] @=> int stabRoots[];
fun void stabOff( float ls )
{
    (450 * ls)::ms => now;
    for( 0 => int i; i < NS7; i++ ) stEnv[i].keyOff();
}
fun void fireStab()
{
    (pitchP * 14.999) $ int => int idx;
    stabRoots[idx % 5] + 12 * (idx / 5) => int root;
    pow2( (lenP - 0.5) * 2.0 ) => float ls;
    for( 0 => int i; i < NS7; i++ )
    {
        (dt[i] + po[i]) * wid => float d;
        Std.mtof( root + d ) => stR[i].freq;
        Std.mtof( root + 12 + UP5A + d ) => stA[i].freq;
        Std.mtof( root + 12 + UP5B + d ) => stB[i].freq;
        3.0 * sawG * 2.2 => stR[i].gain;
        3.0 * sawG * chdGFac * 2.2 => stA[i].gain => stB[i].gain;
        stEnv[i].set( 4::ms, (300 * ls)::ms, 0.0, (250 * ls)::ms );
        stEnv[i].keyOn();
    }
    spork ~ stabOff( ls );
}

fun void paramLoop()
{
    float last[PN.size()];
    for( 0 => int i; i < last.size(); i++ ) -1.0 => last[i];
    0 => int trigWas;
    while( true )
    {
        applyParams();
        pool.get( NS + "trig" ) => trigP;
        if( trigP >= 0.5 && !trigWas ) { 1 => trigWas; fireStab(); }
        else if( trigP < 0.5 ) 0 => trigWas;
        for( 0 => int i; i < PN.size(); i++ )
        {
            pool.get( NS + PN[i] ) => float v;
            if( Math.fabs( v - last[i] ) > 0.001 ) { v => last[i]; pool.out( NS + PN[i], v ); }
        }
        20::ms => now;
    }
}
spork ~ paramLoop();

// ---------------------------------------------------------------- gestures (the drum section's blips)
// slot -> oscillator: 0 sqUp(sqr) 1 sqDn 2 triUp 3 triDn 4 sin1Up 5 sin2Dn 6 sin3 7 oscGl 8 sinB7(sin1) 9 sinRt(sin2) 10 sqFix(sqr)
11 => int NG;
Osc @ gOsc[NG];
sqr @=> gOsc[0]; sqDown @=> gOsc[1]; triUp @=> gOsc[2]; triDown @=> gOsc[3]; sin1 @=> gOsc[4];
sin2 @=> gOsc[5]; sin3 @=> gOsc[6]; oscGl @=> gOsc[7]; sin1 @=> gOsc[8]; sin2 @=> gOsc[9]; sqr @=> gOsc[10];
// kind: 0 slide, 1 two blips on a random note, 2 fixed note for 200 steps
[0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 2] @=> int gKind[];
[48.0, 72.0, 48.0, 72.0, 48.0, 72.0, 48.0, 48.0, 0.0, 0.0, 0.0] @=> float gLo[];   // random start range
[72.0, 96.0, 72.0, 96.0, 72.0, 96.0, 96.0, 96.0, 0.0, 0.0, 0.0] @=> float gHi[];
[12.0, -12.0, 12.0, -12.0, 12.0, -12.0, 0.0, 0.0, 0.0, 0.0, 0.0] @=> float gSpan[];
[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 82.0, 72.0, 77.0] @=> float gFix[];
int gAct[NG]; int gDel[NG]; int gSt[NG]; float gM0[NG]; float gM1[NG];

fun void gStart( int g )
{
    1 => gAct[g]; delSt => gDel[g]; 0 => gSt[g];
    if( gKind[g] == 0 ) { Math.random2f( gLo[g], gHi[g] ) => gM0[g]; gM0[g] + gSpan[g] => gM1[g]; }
    else if( gKind[g] == 1 ) Math.random2f( gLo[g], gHi[g] ) => gM0[g];
    0 => gOsc[g].gain;
}
fun void gTick( int g )
{
    if( !gAct[g] ) return;
    if( gDel[g] > 0 ) { gDel[g]--; return; }
    gSt[g] => int st;
    if( gKind[g] == 0 )
    {
        if( st == 0 ) 0.2 => gOsc[g].gain;
        (st $ float) / (slideSt - 1) => float t;
        Std.mtof( gM0[g] + t * (gM1[g] - gM0[g]) ) => gOsc[g].freq;
        gSt[g]++;
        if( gSt[g] >= slideSt ) { 0 => gOsc[g].gain; 0 => gAct[g]; }
    }
    else if( gKind[g] == 1 )
    {
        if( st == 0 ) { 0.2 => gOsc[g].gain; Std.mtof( gM0[g] ) => gOsc[g].freq; }
        gSt[g]++;
        if( gSt[g] == 80 ) 0 => gOsc[g].gain;
        if( gSt[g] == 100 ) { 0.2 => gOsc[g].gain; Std.mtof( gM0[g] ) => gOsc[g].freq; }
        if( gSt[g] == 200 ) { 0 => gOsc[g].gain; 0 => gAct[g]; }
    }
    else
    {
        if( st == 0 ) { 0.2 => gOsc[g].gain; Std.mtof( gFix[g] ) => gOsc[g].freq; }
        gSt[g]++;
        if( gSt[g] == 200 ) { 0 => gOsc[g].gain; 0 => gAct[g]; }
    }
}
// per drum-section position, which gesture (-1 none): even measures (>= 2) and odd measures (>= 3)
[-2, 5, 4, 5, 6, 1, 2, 1, 4, 1, 7, 6, 2, 7, 3, 5] @=> int evenG[];   // -2 = pos 0 special (sqUp 0.4 + bass)
[1, 8, 9, 10, 4, 3, 0, 1, 4, 6, 3, 0, 2, 5, 7, 6] @=> int oddG[];

// bass (saw section on even measures, pos 0)
0 => int basAct; 0 => int basSt; 0 => int basTotSt; 26 => int basBaseM;

// ---------------------------------------------------------------- the two halves of the piece
0 => int arpIdx;
fun void chordSection( int k )
{
    roots[k] => int baseM;
    ((chordNotes[k] * noteD * pow2( (lenP - 0.5) * 2.0 )) / stepD) $ int => int totSt;
    silenceSaws();
    sawGains( chdG[k], chdG3[k], 1, chdGFac );
    0 => arpIdx;
    for( 0 => int s; s < totSt; s++ )
    {
        if( s % noteSt == 0 )
        {
            Std.mtof( chdN[k][arpIdx % 4] ) => sqArp.freq;
            Math.random2f( -1.0, 1.0 ) => panArp.pan;
            arpIdx++;
        }
        bendOff( s, totSt ) => float off;
        if( s == 0 ) 0.0 => off;
        setSaws( baseM, off, UP5A, UP5B, 1 );
        stepD => now;
    }
    silenceSaws();
}

fun void drumSection()
{
    // arp slide 81 -> 26 over 800 ms
    (800::ms / stepD) $ int => int arpStps;
    81.0 => float arpStartM;  26.0 => float arpEndM;
    1 => int arpAct;  0 => int arpSt;
    0 => int meas;  0 => int pos;
    0 => int kAct; 0 => int kT; 0 => int snAct; 0 => int snTme; 0 => int hatAct; 0 => int hatT;
    7.5 => float hatDec; 2.5 => float hatRel;
    0 => basAct;
    while( meas < 15 )
    {
        0.4 * pow2( (dens - 0.5) * 3.0 ) => float pg;   if( pg > 1.0 ) 1.0 => pg;
        0.5 * pow2( (dens - 0.5) * 3.0 ) => float ph;   if( ph > 1.0 ) 1.0 => ph;
        -1 => int g;
        if( meas >= 2 && meas % 2 == 0 ) evenG[pos] => g;
        else if( meas >= 3 && meas % 2 == 1 ) oddG[pos] => g;
        if( g == -2 )
        {
            if( Math.random2f( 0.0, 1.0 ) < pg ) gStart( 0 );
            26 => basBaseM;
            ((4 * eigthD) / stepD) $ int => basTotSt;
            sawGains( 2.6, 3.3, 0, chdGFac );
            1 => basAct; 0 => basSt;
        }
        else if( g >= 0 && Math.random2f( 0.0, 1.0 ) < pg ) gStart( g );
        if( pos == 0 || pos == 15 )
        {
            0.8 => kOsc.gain;
            kEnv.set( 0::ms, 150::ms, 0.0, 50::ms );
            200.0 => kOsc.freq;
            kEnv.keyOn();  0 => kT;  1 => kAct;
        }
        if( pos == 4 || pos == 14 )
        {
            0.3 => snT1.gain; 0.3 => snT2.gain; 0.5 => snN.gain;
            snEnv1.set( 0::ms, 50::ms, 0.0, 10::ms );
            snEnv2.set( 0::ms, 50::ms, 0.0, 10::ms );
            snEnv3.set( 0::ms, 200::ms, 0.0, 10::ms );
            174.0 => snT1.freq; 349.0 => snT2.freq;
            snEnv1.keyOn(); snEnv2.keyOn(); snEnv3.keyOn();
            0 => snTme; 1 => snAct;
        }
        if( (pos % 2 == 1) && Math.random2f( 0.0, 1.0 ) < ph )
        {
            0.3 => hatN.gain;
            hatEnv.set( 0::ms, 7.5::ms, 0.0, 2.5::ms );
            hatEnv.keyOn(); 0 => hatT; 7.5 => hatDec; 2.5 => hatRel; 1 => hatAct;
        }
        if( (pos % 2 == 0) && pos > 0 && Math.random2f( 0.0, 1.0 ) < ph )
        {
            0.4 => hatN.gain;
            hatEnv.set( 0::ms, 25::ms, 0.0, 5::ms );
            hatEnv.keyOn(); 0 => hatT; 25 => hatDec; 5 => hatRel; 1 => hatAct;
        }
        if( meas == 0 && pos == 4 ) 0 => sqArp.gain;
        for( 0 => int p; p < posSt; p++ )
        {
            if( arpAct )
            {
                (arpSt $ float) / (arpStps - 1) => float t;
                Std.mtof( arpStartM + t * (arpEndM - arpStartM) ) => sqArp.freq;
                arpSt++;
                if( arpSt >= arpStps ) 0 => arpAct;
            }
            for( 0 => int q; q < NG; q++ ) gTick( q );
            if( basAct )
            {
                bendOff( basSt, basTotSt ) => float off;
                setSaws( basBaseM, off, 0, 0, 0 );
                basSt++;
                if( basSt >= basTotSt ) { silenceSaws(); 0 => basAct; }
            }
            if( kAct )
            {
                kOsc.freq() * kMult => kOsc.freq;
                kT++;
                if( kT == 150 ) kEnv.keyOff();
                if( kT == 200 ) { 0 => kAct; 0 => kOsc.gain; }
            }
            if( snAct )
            {
                snTme++;
                if( snTme == 200 ) { snEnv1.keyOff(); snEnv2.keyOff(); snEnv3.keyOff(); }
                if( snTme == 210 ) { 0 => snAct; 0 => snT1.gain; 0 => snT2.gain; 0 => snN.gain; }
            }
            if( hatAct )
            {
                hatT++;
                if( hatT == hatDec ) hatEnv.keyOff();
                if( hatT == hatDec + hatRel ) { 0 => hatAct; 0 => hatN.gain; }
            }
            stepD => now;
        }
        pos++;
        if( pos == 16 ) { 0 => pos; meas++; }
    }
    silenceSaws();
}

// ---------------------------------------------------------------- play, forever
while( true )
{
    0.15 => sqArp.gain;
    for( 0 => int k; k < 5; k++ ) chordSection( k );
    drumSection();
}
