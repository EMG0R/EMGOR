// reverb.ck
@import "globals.ck"

global Gain preReverbL;
global Gain preReverbR;
global Gain postReverbL;
global Gain postReverbR;
global Gain reverbGainL;
global Gain reverbGainR;
global Gain directReverbL;
global Gain directReverbR;

global NRev revL;
global NRev revR;

<<< "Reverb activated and processing audio" >>>;
float reverb_on_smooth;
0.0 => reverb_on_smooth;
float smooth_mix;
G.gkReverbMix => smooth_mix;
dur control_dur;
1::ms => control_dur;
float control_rate;
1.0 / (control_dur / 1::second) => control_rate;

while (true) {
    Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
    1.0 - G.gkReverbBypass => float target_on;
    reverb_on_smooth * coeff + target_on * (1.0 - coeff) => reverb_on_smooth;
    reverb_on_smooth => G.gkReverbOnSmooth;

    G.gkReverbMix => float target_mix;
    smooth_mix * coeff + target_mix * (1.0 - coeff) => smooth_mix;

    (1.0 - smooth_mix) => directReverbL.gain;
    (1.0 - smooth_mix) => directReverbR.gain;
    smooth_mix * reverb_on_smooth => reverbGainL.gain;
    smooth_mix * reverb_on_smooth => reverbGainR.gain;

    control_dur => now;
}