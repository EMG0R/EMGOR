//vibrato.ck
@import "globals.ck"

global Gain preVibL;
global Gain preVibR;
global Gain postVibL;
global Gain postVibR;

global DelayL vibL;
global DelayL vibR;
global Gain vibFbL;
global Gain vibFbR;
global Gain vibGainL;
global Gain vibGainR;

preVibL => vibL => vibGainL => postVibL;
preVibR => vibR => vibGainR => postVibR;
vibL => vibFbL => vibL;
vibR => vibFbR => vibR;

fun void vibratoShred() {
    <<< "Vibrato activated and processing audio" >>>;
    SinOsc lfo => blackhole;
    float current_delayL;
    7.0::ms / 1::samp => current_delayL;
    float current_delayR;
    7.0::ms / 1::samp => current_delayR;
    float vib_on_smooth;
    0.0 => vib_on_smooth;
    float smooth_rate;
    0.001 + 9.999 * G.gkVibRate => smooth_rate;
    float smooth_amount;
    G.gkVibAmount => smooth_amount;
    float smooth_feedback;
    G.gkVibFeedback => smooth_feedback;
    dur control_dur;
    1::ms => control_dur;
    float control_rate;
    1.0 / (control_dur / 1::second) => control_rate;
    50::ms => vibL.max;
    50::ms => vibR.max;
    
    while (true) {
        Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
        1.0 - G.gkVibBypass => float target_on;
        vib_on_smooth * coeff + target_on * (1.0 - coeff) => vib_on_smooth;
        vibGainL.gain(vib_on_smooth * 0.1);
        vibGainR.gain(vib_on_smooth * 0.1);
        vib_on_smooth => G.gkVibOnSmooth;

        0.0001 + 9.9999 * G.gkVibRate => float target_rate;
        smooth_rate * coeff + target_rate * (1.0 - coeff) => smooth_rate;
        lfo.freq(smooth_rate);

        G.gkVibAmount => float target_amount;
        smooth_amount * coeff + target_amount * (1.0 - coeff) => smooth_amount;
        7::ms => dur center;
        5::ms * smooth_amount => dur depth;
        (center + depth * lfo.last()) / 1::samp => float target_delayL;
        (center + depth * lfo.last()) / 1::samp => float target_delayR;
        Math.max(0.001, target_delayL) => target_delayL;
        Math.max(0.001, target_delayR) => target_delayR;
        current_delayL * coeff + target_delayL * (1.0 - coeff) => current_delayL;
        current_delayR * coeff + target_delayR * (1.0 - coeff) => current_delayR;
        current_delayL :: samp => vibL.delay;
        current_delayR :: samp => vibR.delay;
        
        G.gkVibFeedback => float target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        Math.min(smooth_feedback * 0.9, 0.99) => smooth_feedback;
        vibFbL.gain(smooth_feedback);
        vibFbR.gain(smooth_feedback);
        
        control_dur => now;
    }
}

spork ~ vibratoShred();

while (true) { 1::second => now; }