@import "globals.ck"

global Gain preRingL;
global Gain preRingR;
global Gain postRingL;
global Gain postRingR;
global Gain directRingL;
global Gain directRingR;
global SinOsc ringMod;
global Gain ringGainL;
global Gain ringGainR;

preRingL => ringGainL => postRingL;
preRingR => ringGainR => postRingR;

fun void ring_mod_shred() {
    ringMod => blackhole;
    0.0 => float ring_on_smooth;
    1.25 * Math.pow(200.0, G.gkRingRate) => float smooth_rate;
    
    1::samp => dur control_dur;
    1.0 / (control_dur / 1::second) => float control_rate;
    Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
    
    while (true) {
        1.0 - G.gkRingBypass => float target_on;
        ring_on_smooth * coeff + target_on * (1.0 - coeff) => ring_on_smooth;
        
        1.25 * Math.pow(200.0, G.gkRingRate) => float target_freq;
        smooth_rate * coeff + target_freq * (1.0 - coeff) => smooth_rate;
        if (smooth_rate > 20000.0) 20000.0 => smooth_rate;
        if (smooth_rate < 0.1) 0.1 => smooth_rate;
        smooth_rate => ringMod.freq;
        
        ringMod.last() => float mod_raw;
        
        (mod_raw + 1.0) * 0.5 => float mod_norm;
        
        mod_norm * ring_on_smooth + 1.0 * (1.0 - ring_on_smooth) => float final_mod;
        
        final_mod * 0.5 => float clamped_gain;
        
        clamped_gain => ringGainL.gain;
        clamped_gain => ringGainR.gain;
        
        1.0 - ring_on_smooth => float dry_gain;
        dry_gain => directRingL.gain;
        dry_gain => directRingR.gain;
        
        ring_on_smooth => G.gkRingOnSmooth;
        
        control_dur => now;
    }
}

spork ~ ring_mod_shred();
while (true) { 1::second => now; }