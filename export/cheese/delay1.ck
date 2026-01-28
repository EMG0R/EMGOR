// delay.ck
@import "globals.ck"

global Gain preDelayL;
global Gain preDelayR;
global Gain postDelayL;
global Gain postDelayR;
global Gain directDelayL;
global Gain directDelayR;

global DelayA delayDelayL;
global DelayA delayDelayR;
global PitShift delPitchShiftL;
global PitShift delPitchShiftR;
global Gain delayFbL;
global Gain delayFbR;
global Gain delayGainL;
global Gain delayGainR;

class TanhSat extends Chugen {
    float drive;
    2.5 => drive;

    fun float tick(float in) {
        return Math.tanh(in * drive) / Math.tanh(drive);
    }

    fun void setDrive(float d) {
        d => drive;
    }
}

TanhSat satL => delayGainL;
TanhSat satR => delayGainR;

global float gkDelaySaturation;
2.5 => gkDelaySaturation;
gkDelaySaturation => satL.setDrive;
gkDelaySaturation => satR.setDrive;

fun void delay_shred() {
    <<< "Delay activated and processing audio" >>>;
    
    float delay_on_smooth;
    1.0 - G.gkDelayBypass => delay_on_smooth;
    
    SinOsc lfoL => blackhole;
    SinOsc lfoR => blackhole;
    lfoL.phase(0.0);
    lfoR.phase(0.5);
    
    float smooth_mix;
    G.gkDelayMix => smooth_mix;
    float smooth_time;
    G.gkDelayTime => smooth_time;
    float smooth_feedback;
    G.gkDelayFeedback => smooth_feedback;
    float smooth_mod_rate;
    0.001 + 9.999 * G.gkDelayModRate => smooth_mod_rate;
    float smooth_mod_depth;
    G.gkDelayModDepth => smooth_mod_depth;
    float smooth_pitch;
    G.gkDelayPitch => smooth_pitch;
    float smooth_modulatedL_seconds;
    0.5 => smooth_modulatedL_seconds;
    float smooth_modulatedR_seconds;
    0.5 => smooth_modulatedR_seconds;
    float smooth_ps_mix;
    1.0 => smooth_ps_mix;
    float smooth_drive;
    gkDelaySaturation => smooth_drive;
    
    dur control_dur;
    1::samp => control_dur;
    float control_rate;
    1.0 / (control_dur / 1::second) => control_rate;
    
    // Initialize gains to correct starting values
    1.0 - delay_on_smooth * smooth_mix => float dry_g;
    delay_on_smooth * smooth_mix => float wet_g;
    dry_g => directDelayL.gain;
    dry_g => directDelayR.gain;
    wet_g => delayGainL.gain;
    wet_g => delayGainR.gain;
    
    while (true) {
        Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
        
        1.0 - G.gkDelayBypass => float target_on;
        delay_on_smooth * coeff + target_on * (1.0 - coeff) => delay_on_smooth;
        
        G.gkDelayMix => float target_mix;
        smooth_mix * coeff + target_mix * (1.0 - coeff) => smooth_mix;
        
        0.001 + 1.199 * G.gkDelayTime => float target_time;
        smooth_time * coeff + target_time * (1.0 - coeff) => smooth_time;
        
        G.gkDelayFeedback * 0.99 => float target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        smooth_feedback => delayFbL.gain;
        smooth_feedback => delayFbR.gain;
        
        0.001 + 9.999 * G.gkDelayModRate => float target_mod_rate;
        smooth_mod_rate * coeff + target_mod_rate * (1.0 - coeff) => smooth_mod_rate;
        lfoL.freq(smooth_mod_rate);
        lfoR.freq(smooth_mod_rate);
        
        G.gkDelayModDepth => float target_mod_depth;
        smooth_mod_depth * coeff + target_mod_depth * (1.0 - coeff) => smooth_mod_depth;
        
        smooth_time :: second => dur center;
        0.02 * smooth_mod_depth => float max_mod_depth_seconds;
        (center - 0.5::samp) / 1::second => float safe_mod_depth_seconds;
        Math.min(max_mod_depth_seconds, safe_mod_depth_seconds) => float used_mod_depth_seconds;
        if (used_mod_depth_seconds < 0.0) 0.0 => used_mod_depth_seconds;
        used_mod_depth_seconds :: second => dur mod_depth;
        
        float target_modulatedL_seconds;
        (center + mod_depth * lfoL.last()) / 1::second => target_modulatedL_seconds;
        float target_modulatedR_seconds;
        (center + mod_depth * lfoR.last()) / 1::second => target_modulatedR_seconds;
        
        smooth_modulatedL_seconds * coeff + target_modulatedL_seconds * (1.0 - coeff) => smooth_modulatedL_seconds;
        smooth_modulatedR_seconds * coeff + target_modulatedR_seconds * (1.0 - coeff) => smooth_modulatedR_seconds;
        
        Math.max(0.00001, smooth_modulatedL_seconds) :: second => delayDelayL.delay;
        Math.max(0.00001, smooth_modulatedR_seconds) :: second => delayDelayR.delay;
        
        G.gkDelayPitch => float target_pitch;
        smooth_pitch * coeff + target_pitch * (1.0 - coeff) => smooth_pitch;
        Math.pow(2.0, smooth_pitch / 12.0) => float ratio;
        ratio => delPitchShiftL.shift;
        ratio => delPitchShiftR.shift;
        
        float target_ps_mix;
        if (G.gkDelayPitch == 0.0) 0.0 => target_ps_mix; else 1.0 => target_ps_mix;
        smooth_ps_mix * coeff + target_ps_mix * (1.0 - coeff) => smooth_ps_mix;
        smooth_ps_mix => delPitchShiftL.mix;
        smooth_ps_mix => delPitchShiftR.mix;
        
        gkDelaySaturation => float target_drive;
        smooth_drive * coeff + target_drive * (1.0 - coeff) => smooth_drive;
        smooth_drive => satL.setDrive;
        smooth_drive => satR.setDrive;
        
        1.0 - delay_on_smooth * smooth_mix => dry_g;
        delay_on_smooth * smooth_mix => wet_g;
        
        dry_g => directDelayL.gain;
        dry_g => directDelayR.gain;
        wet_g => delayGainL.gain;
        wet_g => delayGainR.gain;
        
        delay_on_smooth => G.gkDelayOnSmooth;
        
        control_dur => now;
    }
}

spork ~ delay_shred();

while (true) { 1::second => now; }