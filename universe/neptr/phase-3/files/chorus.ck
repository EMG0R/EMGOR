@import "globals.ck"

global Gain preChorusL;
global Gain preChorusR;
global Gain postChorusL;
global Gain postChorusR;

global DelayL chorL;
global DelayL chorR;
global Gain chorusFbL;
global Gain chorusFbR;
global Gain chorusGainL;
global Gain chorusGainR;

preChorusL => chorL => chorusGainL => postChorusL;
preChorusR => chorR => chorusGainR => postChorusR;
chorL => chorusFbL => chorL;
chorR => chorusFbR => chorR;

fun void chorusShred() {
    <<< "Chorus activated and processing audio" >>>;
    SinOsc lfo => blackhole;
    SinOsc lfoR => blackhole;
    lfoR.phase(0.5);
    float current_delayL;
    5.0::ms / 1::samp => current_delayL;
    float current_delayR;
    5.0::ms / 1::samp => current_delayR;
    float chorus_on_smooth;
    0.0 => chorus_on_smooth;
    float smooth_rate;
    0.001 + 9.999 * G.gkChorusRate => smooth_rate;
    float smooth_amount;
    G.gkChorusAmount => smooth_amount;
    float smooth_feedback;
    G.gkChorusFeedback => smooth_feedback;
    dur control_dur;
    1::ms => control_dur;
    float control_rate;
    1.0 / (control_dur / 1::second) => control_rate;
    while (true) {
        Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
        1.0 - G.gkChorusBypass => float target_on;
        chorus_on_smooth * coeff + target_on * (1.0 - coeff) => chorus_on_smooth;
        chorusGainL.gain(chorus_on_smooth * 0.1);
        chorusGainR.gain(chorus_on_smooth * 0.1);
        chorus_on_smooth => G.gkChorusOnSmooth;
        0.001 + 9.999 * Math.min(G.gkChorusRate, 0.15) => float target_rate;
        smooth_rate * coeff + target_rate * (1.0 - coeff) => smooth_rate;
        lfo.freq(smooth_rate);
        lfoR.freq(smooth_rate);
        G.gkChorusAmount => float target_amount;
        smooth_amount * coeff + target_amount * (1.0 - coeff) => smooth_amount;
        5::ms => dur center;
        5::ms * smooth_amount => dur depth;
        (center + depth * lfo.last()) / 1::samp => float target_delayL;
        (center + depth * lfoR.last()) / 1::samp => float target_delayR;
        current_delayL * coeff + target_delayL * (1.0 - coeff) => current_delayL;
        current_delayR * coeff + target_delayR * (1.0 - coeff) => current_delayR;
        current_delayL :: samp => chorL.delay;
        current_delayR :: samp => chorR.delay;
        G.gkChorusFeedback => float target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        Math.min(smooth_feedback * 0.9, 1) => smooth_feedback;
        chorusFbL.gain(smooth_feedback);
        chorusFbR.gain(smooth_feedback);
        control_dur => now;
    }
}

spork ~ chorusShred();

while (true) { 1::second => now; }