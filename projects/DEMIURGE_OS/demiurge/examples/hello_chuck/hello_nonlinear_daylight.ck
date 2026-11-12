// hello_nonlinear_daylight.ck -- Demiurge example (ChucK)
// Slow, sparse, Lydian generative voice. Three 0-1 params, the same in every
// language so one UI (web/examples/nonlinear_daylight.html?stage=chuck) drives them all:
//   /nld-chuck/density  event rate      /nld-chuck/tone  brightness / cutoff
//   /nld-chuck/length   note length
// Params come through the pool hook (resources/params/chuck/DemiurgeParams.ck): the
// pool writes `/p <path> <0..1>` to DEMIURGE_PARAM_PORT (manifest params/nld-chuck.json,
// port 9000) and this file answers with /pout whenever a value changes.
// The class must exist first:
//   chuck $DEMIURGE_RESOURCES/params/chuck/DemiurgeParams.ck hello_nonlinear_daylight.ck
// (the launcher puts the hook ahead of the patch for you).

DemiurgeParams pool;
pool.init( "nld-chuck", 0 );            // 0 -> env DEMIURGE_PARAM_PORT
pool.add( "/nld-chuck/density", 0.35 );
pool.add( "/nld-chuck/tone", 0.50 );
pool.add( "/nld-chuck/length", 0.50 );

// live values, 0-1 (re-read from the hook every note, and echoed on change)
0.35 => float density;
0.50 => float tone;
0.50 => float length;

fun void params()
{
    -1.0 => float ld; -1.0 => float lt; -1.0 => float ll;
    while( true )
    {
        pool.get( "/nld-chuck/density" ) => density;
        pool.get( "/nld-chuck/tone" ) => tone;
        pool.get( "/nld-chuck/length" ) => length;
        if( Math.fabs( density - ld ) > 0.001 ) { density => ld; pool.out( "/nld-chuck/density", density ); }
        if( Math.fabs( tone - lt ) > 0.001 ) { tone => lt; pool.out( "/nld-chuck/tone", tone ); }
        if( Math.fabs( length - ll ) > 0.001 ) { length => ll; pool.out( "/nld-chuck/length", length ); }
        20::ms => now;
    }
}
spork ~ params();

// ---- voice --------------------------------------------------------------
TriOsc osc => LPF lpf => ADSR env => JCRev rev => dac;
0.25 => osc.gain;
0.12 => rev.mix;
2 => lpf.Q;

// C Lydian, two octaves from C3
[0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19] @=> int scale[];
48 => int root;
4 => int idx;

fun void tick()
{
    // tone -> cutoff 300..5000 Hz (exponential)
    300.0 * Math.pow( 5000.0/300.0, tone ) => lpf.freq;
}

while( true )
{
    tick();
    // gentle random walk over the scale, favoring small steps
    Math.random2( -2, 2 ) +=> idx;
    Math.max( 0, Math.min( scale.size()-1, idx ) ) $ int => idx;
    Std.mtof( root + scale[idx] ) => osc.freq;

    // length 0.3..4 s note
    0.3 + length * 3.7 => float dur_s;
    env.set( (dur_s*0.3)::second, (dur_s*0.3)::second, 0.5, (dur_s*0.4)::second );
    env.keyOn();
    (dur_s*0.6)::second => now;
    env.keyOff();

    // density -> gap: 8 s (sparse) .. 0.4 s; random jitter, sometimes a rest
    8.0 * Math.pow( 0.4/8.0, density ) => float gap;
    (gap * Math.random2f( 0.6, 1.5 ))::second => now;
}
