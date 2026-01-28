// pitchshift.ck
@import "globals.ck"

global Gain postRingL;
global Gain postRingR;
global Gain preDelayL;
global Gain preDelayR;
global Gain directPitchL;
global Gain directPitchR;

global PitShift pitchShiftL;
global PitShift pitchShiftR;
global Gain pitchWetL;
global Gain pitchWetR;

1.0 => pitchShiftL.mix;
1.0 => pitchShiftR.mix;

<<< "Pitch shifter activated and processing audio" >>>;

float pitch_on_smooth;
0.0 => pitch_on_smooth;
float smooth_mix;
G.gkPitchMix => smooth_mix;
float smooth_shift;
G.gkPitchShift => smooth_shift;

dur control_dur;
1::ms => control_dur;
float control_rate;
1.0 / (control_dur / 1::second) => control_rate;

while (true) {
    Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
    
    1.0 - G.gkPitchBypass => float target_on;
    pitch_on_smooth * coeff + target_on * (1.0 - coeff) => pitch_on_smooth;
    pitch_on_smooth => G.gkPitchOnSmooth;

    G.gkPitchMix => float target_mix;
    smooth_mix * coeff + target_mix * (1.0 - coeff) => smooth_mix;

    G.gkPitchShift => float target_shift;
    smooth_shift * coeff + target_shift * (1.0 - coeff) => smooth_shift;

    Math.pow(2.0, smooth_shift / 12.0) => float ratio;
    ratio => pitchShiftL.shift;
    ratio => pitchShiftR.shift;

    (1.0 - smooth_mix) => directPitchL.gain;
    (1.0 - smooth_mix) => directPitchR.gain;
    
    smooth_mix * pitch_on_smooth => pitchWetL.gain;
    smooth_mix * pitch_on_smooth => pitchWetR.gain;

    control_dur => now;
}