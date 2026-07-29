// phaser.ck
@import "globals.ck"

global Gain prePhaserL;
global Gain prePhaserR;
global Gain postPhaserL;
global Gain postPhaserR;
global Gain phaserFbL;
global Gain phaserFbR;
global Gain phaserGainL;
global Gain phaserGainR;
global Gain directPhaserL;
global Gain directPhaserR;

BiQuad phaserPole1L;
BiQuad phaserPole1R;
BiQuad phaserPole2L;
BiQuad phaserPole2R;
BiQuad phaserPole3L;
BiQuad phaserPole3R;
BiQuad phaserPole4L;
BiQuad phaserPole4R;

prePhaserL => directPhaserL => postPhaserL;
prePhaserR => directPhaserR => postPhaserR;
prePhaserL => phaserPole1L => phaserPole2L => phaserPole3L => phaserPole4L => phaserGainL => postPhaserL;
prePhaserR => phaserPole1R => phaserPole2R => phaserPole3R => phaserPole4R => phaserGainR => postPhaserR;

phaserPole4L => phaserFbL => phaserPole1L;
phaserPole4R => phaserFbR => phaserPole1R;

fun void phaserShred() {
    <<< "Phaser activated and processing audio" >>>;
    
    SinOsc lfo => blackhole;
    
    float phaser_on_smooth;
    0.0 => phaser_on_smooth;
    
    float smooth_rate;
    0.001 + 9.999 * G.gkPhaserRate => smooth_rate;
    float smooth_depth;
    G.gkPhaserDepth => smooth_depth;
    float smooth_feedback;
    G.gkPhaserFeedback => smooth_feedback;
    
    dur control_dur;
    1::ms => control_dur;
    float control_rate;
    1.0 / (control_dur / 1::second) => control_rate;
    
    // Higher radius = sharper notches = more dramatic phaser
    0.99 => float base_radius;
    base_radius => phaserPole1L.prad => phaserPole1R.prad;
    base_radius => phaserPole2L.prad => phaserPole2R.prad;
    base_radius => phaserPole3L.prad => phaserPole3R.prad;
    base_radius => phaserPole4L.prad => phaserPole4R.prad;
    
    0 => phaserPole1L.norm => phaserPole1R.norm;
    0 => phaserPole2L.norm => phaserPole2R.norm;
    0 => phaserPole3L.norm => phaserPole3R.norm;
    0 => phaserPole4L.norm => phaserPole4R.norm;
    
    1.0 => directPhaserL.gain => directPhaserR.gain;
    
    while (true) {
        Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
        
        1.0 - G.gkPhaserBypass => float target_on;
        phaser_on_smooth * coeff + target_on * (1.0 - coeff) => phaser_on_smooth;
        phaser_on_smooth => G.gkPhaserOnSmooth;
        
        0.01 + 4.99 * G.gkPhaserRate => float target_rate;
        smooth_rate * coeff + target_rate * (1.0 - coeff) => smooth_rate;
        lfo.freq(smooth_rate);
        
        G.gkPhaserDepth => float target_depth;
        smooth_depth * coeff + target_depth * (1.0 - coeff) => smooth_depth;
        
        // Wider frequency spread for more dramatic sweep
        200.0 => float base_freq1;
        600.0 => float base_freq2;
        1800.0 => float base_freq3;
        5400.0 => float base_freq4;
        
        float lfo_val;
        lfo.last() => lfo_val;
        
        // Bigger sweep range: 3 octaves each direction at full depth
        base_freq1 * Math.pow(2.0, lfo_val * smooth_depth * 3.0) => float freq1;
        base_freq2 * Math.pow(2.0, lfo_val * smooth_depth * 3.0) => float freq2;
        base_freq3 * Math.pow(2.0, lfo_val * smooth_depth * 3.0) => float freq3;
        base_freq4 * Math.pow(2.0, lfo_val * smooth_depth * 3.0) => float freq4;
        
        Math.max(20.0, Math.min(20000.0, freq1)) => freq1;
        Math.max(20.0, Math.min(20000.0, freq2)) => freq2;
        Math.max(20.0, Math.min(20000.0, freq3)) => freq3;
        Math.max(20.0, Math.min(20000.0, freq4)) => freq4;
        
        freq1 => phaserPole1L.pfreq => phaserPole1R.pfreq;
        freq2 => phaserPole2L.pfreq => phaserPole2R.pfreq;
        freq3 => phaserPole3L.pfreq => phaserPole3R.pfreq;
        freq4 => phaserPole4L.pfreq => phaserPole4R.pfreq;
        
        // Configure each BiQuad as all-pass
        phaserPole1L.b0( phaserPole1L.a2() );
        phaserPole1L.b1( phaserPole1L.a1() );
        phaserPole1L.b2( 1.0 );
        phaserPole1R.b0( phaserPole1R.a2() );
        phaserPole1R.b1( phaserPole1R.a1() );
        phaserPole1R.b2( 1.0 );
        
        phaserPole2L.b0( phaserPole2L.a2() );
        phaserPole2L.b1( phaserPole2L.a1() );
        phaserPole2L.b2( 1.0 );
        phaserPole2R.b0( phaserPole2R.a2() );
        phaserPole2R.b1( phaserPole2R.a1() );
        phaserPole2R.b2( 1.0 );
        
        phaserPole3L.b0( phaserPole3L.a2() );
        phaserPole3L.b1( phaserPole3L.a1() );
        phaserPole3L.b2( 1.0 );
        phaserPole3R.b0( phaserPole3R.a2() );
        phaserPole3R.b1( phaserPole3R.a1() );
        phaserPole3R.b2( 1.0 );
        
        phaserPole4L.b0( phaserPole4L.a2() );
        phaserPole4L.b1( phaserPole4L.a1() );
        phaserPole4L.b2( 1.0 );
        phaserPole4R.b0( phaserPole4R.a2() );
        phaserPole4R.b1( phaserPole4R.a1() );
        phaserPole4R.b2( 1.0 );
        
        // Aggressive feedback for resonant swoosh
        G.gkPhaserFeedback => float target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        smooth_feedback * 0.75 => float safe_feedback;
        safe_feedback => phaserFbL.gain => phaserFbR.gain;
        
        // Full wet/dry mix for maximum cancellation notches
        phaser_on_smooth * 1.0 => float wet_amount;
        
        // Compensate volume: as wet increases, scale both down to prevent boost
        // dry + inverted_wet at worst case sums to ~1.5x, so pad by 0.65
        1.0 - (phaser_on_smooth * 0.35) => float output_pad;
        
        wet_amount * -1.0 * output_pad => phaserGainL.gain => phaserGainR.gain;
        1.0 * output_pad => directPhaserL.gain => directPhaserR.gain;
        
        control_dur => now;
    }
}

spork ~ phaserShred();

while (true) { 1::second => now; }