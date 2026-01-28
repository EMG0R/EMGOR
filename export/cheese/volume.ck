@import "globals.ck"

global Gain preVolL;
global Gain preVolR;
global Gain postVolL;
global Gain postVolR;
global Gain volGainL;
global Gain volGainR;

preVolL => volGainL => postVolL;
preVolR => volGainR => postVolR;

fun void volume_shred() {
    0.0 => float vol_on_smooth;
    0.2 => float vol_smooth;
    
    5::ms => dur control_dur;
    1.0 / (control_dur / 1::second) => float control_rate;
    
    while (true) {
        Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
        
        1.0 - G.gkVOLpedalBypass => float target_on;
        vol_on_smooth * coeff + target_on * (1.0 - coeff) => vol_on_smooth;
        
        if (vol_on_smooth > 0.01) {
            G.gkVOLpedal => float target_vol;
            vol_smooth * coeff + target_vol * (1.0 - coeff) => vol_smooth;
            Math.min(vol_smooth, 0.2) => vol_smooth;
        }
        
        vol_smooth => volGainL.gain;
        vol_smooth => volGainR.gain;
        
        vol_on_smooth => G.gkVolOnSmooth;
        
        control_dur => now;
    }
}

spork ~ volume_shred();
while (true) { 1::second => now; }