// index.ck

@import "globals.ck"
G.init();

global Gain preReverbL;
global Gain preReverbR;
global Gain postReverbL;
global Gain postReverbR;
global Gain reverbGainL;
global Gain reverbGainR;
global Gain directReverbL;
global Gain directReverbR;

global Gain adcInputL;
global Gain adcInputR;
global Gain synthInputL;
global Gain synthInputR;
global Gain preVolL;
global Gain preVolR;
global Gain postVolL;
global Gain postVolR;
global Gain volGainL;
global Gain volGainR;

global Gain preVibL;
global Gain preVibR;
global Gain postVibL;
global Gain postVibR;

global Gain preChorusL;
global Gain preChorusR;
global Gain postChorusL;
global Gain postChorusR;

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

global Gain preRingL;
global Gain preRingR;
global Gain postRingL;
global Gain postRingR;

global Gain preSpectralL;
global Gain preSpectralR;
global Gain postSpectralL;
global Gain postSpectralR;

global Gain directPitchL;
global Gain directPitchR;
global PitShift pitchShiftL;
global PitShift pitchShiftR;
global Gain pitchWetL;
global Gain pitchWetR;

global Gain preDelayL;
global Gain preDelayR;
global Gain postDelayL;
global Gain postDelayR;

global DelayL vibL;
global DelayL vibR;
50::ms => vibL.max;
50::ms => vibR.max;
7::ms => vibL.delay;
7::ms => vibR.delay;
global Gain vibFbL;
global Gain vibFbR;
global Gain vibGainL;
global Gain vibGainR;

global DelayL chorL;
global DelayL chorR;
30::ms => chorL.max;
30::ms => chorR.max;
5::ms => chorL.delay;
5::ms => chorR.delay;
global Gain chorusFbL;
global Gain chorusFbR;
global Gain chorusGainL;
global Gain chorusGainR;

global Gain ringGainL;
global Gain ringGainR;
global Gain directRingL;
global Gain directRingR;
global SinOsc ringMod;

global FFT fftL;
global FFT fftR;
global IFFT ifftL;
global IFFT ifftR;
512 => int FFT_SIZE;
FFT_SIZE => fftL.size;
FFT_SIZE => fftR.size;
global Gain spectralGainL;
global Gain spectralGainR;
global Gain directSpectralL;
global Gain directSpectralR;

global DelayA delayDelayL;
global DelayA delayDelayR;
1.5::second => delayDelayL.max;
1.5::second => delayDelayR.max;
0.5::second => delayDelayL.delay;
0.5::second => delayDelayR.delay;
global PitShift delPitchShiftL;
global PitShift delPitchShiftR;
1.0 => delPitchShiftL.mix;
1.0 => delPitchShiftR.mix;
global Gain delayFbL;
global Gain delayFbR;
global Gain delayGainL;
global Gain delayGainR;
global Gain directDelayL;
global Gain directDelayR;

global NRev revL;
global NRev revR;
1.0 => revL.mix;
1.0 => revR.mix;

1 => adcInputL.gain;
1 => adcInputR.gain;
0.2 => synthInputL.gain;
0.2 => synthInputR.gain;

adc.chan(0) => adcInputL => preVolL;
adc.chan(1) => adcInputR => preVolR;

synthInputL => preVolL;
synthInputR => preVolR;

preVolL => volGainL => postVolL => preVibL => postVibL => preChorusL => postChorusL => prePhaserL => postPhaserL => preRingL => directRingL => postRingL => preSpectralL => postSpectralL => directPitchL => preDelayL => directDelayL => postDelayL => preReverbL => postReverbL => dac.chan(0);
preVolR => volGainR => postVolR => preVibR => postVibR => preChorusR => postChorusR => prePhaserR => postPhaserR => preRingR => directRingR => postRingR => preSpectralR => postSpectralR => directPitchR => preDelayR => directDelayR => postDelayR => preReverbR => postReverbR => dac.chan(1);

int wetConnectedVol;
0 => wetConnectedVol;
int volShredID;
-1 => volShredID;

int wetConnectedVib;
0 => wetConnectedVib;
int vibShredID;
-1 => vibShredID;

int wetConnectedChorus;
0 => wetConnectedChorus;
int chorusShredID;
-1 => chorusShredID;

int wetConnectedPhaser;
0 => wetConnectedPhaser;
int phaserShredID;
-1 => phaserShredID;

int wetConnectedRing;
0 => wetConnectedRing;
int ringShredID;
-1 => ringShredID;

int wetConnectedSpectral;
0 => wetConnectedSpectral;
int spectralShredID;
-1 => spectralShredID;

int wetConnectedPitch;
0 => wetConnectedPitch;
int pitchShredID;
-1 => pitchShredID;

int wetConnectedDelay;
0 => wetConnectedDelay;
int delayShredID;
-1 => delayShredID;

int wetConnectedReverb;
0 => wetConnectedReverb;
int reverbShredID;
-1 => reverbShredID;

fun void dynamicConnectLoop() {
    while (true) {
        10::ms => now;
        
        // Volume pedal dynamic connection
        if (G.gkVOLpedalBypass >= 0.5 && wetConnectedVol) {
            if (volShredID != -1) {
                Machine.remove(volShredID);
                -1 => volShredID;
            }
            0 => wetConnectedVol;
        } else if (G.gkVOLpedalBypass < 0.5 && !wetConnectedVol) {
            if (volShredID == -1) {
                Machine.add("volume.ck") => volShredID;
            }
            1 => wetConnectedVol;
        }
        
        // Vibrato dynamic connection
        if (G.gkVibBypass >= 0.5 && G.gkVibOnSmooth < 0.01 && wetConnectedVib) {
            if (vibShredID != -1) {
                Machine.remove(vibShredID);
                -1 => vibShredID;
            }
            preVibL =< vibL;
            vibL =< vibGainL;
            vibGainL =< postVibL;
            vibL =< vibFbL;
            vibFbL =< vibL;
            preVibR =< vibR;
            vibR =< vibGainR;
            vibGainR =< postVibR;
            vibR =< vibFbR;
            vibFbR =< vibR;
            preVibL => postVibL;
            preVibR => postVibR;
            0 => wetConnectedVib;
        } else if (G.gkVibBypass < 0.5 && !wetConnectedVib) {
            preVibL =< postVibL;
            preVibR =< postVibR;
            if (vibShredID == -1) {
                Machine.add("vibrato.ck") => vibShredID;
            }
            preVibL => vibL => vibGainL => postVibL;
            preVibR => vibR => vibGainR => postVibR;
            vibL => vibFbL => vibL;
            vibR => vibFbR => vibR;
            1 => wetConnectedVib;
        }
        
        // Chorus dynamic connection
        if (G.gkChorusBypass >= 0.5 && G.gkChorusOnSmooth < 0.01 && wetConnectedChorus) {
            if (chorusShredID != -1) {
                Machine.remove(chorusShredID);
                -1 => chorusShredID;
            }
            preChorusL =< chorL;
            chorL =< chorusGainL;
            chorusGainL =< postChorusL;
            chorL =< chorusFbL;
            chorusFbL =< chorL;
            preChorusR =< chorR;
            chorR =< chorusGainR;
            chorusGainR =< postChorusR;
            chorR =< chorusFbR;
            chorusFbR =< chorR;
            preChorusL => postChorusL;
            preChorusR => postChorusR;
            0 => wetConnectedChorus;
        } else if (G.gkChorusBypass < 0.5 && !wetConnectedChorus) {
            preChorusL =< postChorusL;
            preChorusR =< postChorusR;
            if (chorusShredID == -1) {
                Machine.add("chorus.ck") => chorusShredID;
            }
            preChorusL => chorL => chorusGainL => postChorusL;
            preChorusR => chorR => chorusGainR => postChorusR;
            chorL => chorusFbL => chorL;
            chorR => chorusFbR => chorR;
            1 => wetConnectedChorus;
        }
        
        // Phaser dynamic connection
         if (G.gkPhaserBypass >= 0.5 && G.gkPhaserOnSmooth < 0.01 && wetConnectedPhaser) {
            if (phaserShredID != -1) {
                Machine.remove(phaserShredID);
                -1 => phaserShredID;
            }
            prePhaserL =< G.phaserPole1L;
            G.phaserPole1L =< G.phaserPole2L;
            G.phaserPole2L =< G.phaserPole3L;
            G.phaserPole3L =< G.phaserPole4L;
            G.phaserPole4L =< phaserGainL;
            phaserGainL =< postPhaserL;
            G.phaserPole4L =< phaserFbL;
            phaserFbL =< G.phaserPole1L;
            
            prePhaserR =< G.phaserPole1R;
            G.phaserPole1R =< G.phaserPole2R;
            G.phaserPole2R =< G.phaserPole3R;
            G.phaserPole3R =< G.phaserPole4R;
            G.phaserPole4R =< phaserGainR;
            phaserGainR =< postPhaserR;
            G.phaserPole4R =< phaserFbR;
            phaserFbR =< G.phaserPole1R;
            
            prePhaserL =< directPhaserL;
            directPhaserL =< postPhaserL;
            prePhaserR =< directPhaserR;
            directPhaserR =< postPhaserR;
            
            prePhaserL => postPhaserL;
            prePhaserR => postPhaserR;
            0 => wetConnectedPhaser;
        
        } else if (G.gkPhaserBypass < 0.5 && !wetConnectedPhaser) {
            prePhaserL =< postPhaserL;
            prePhaserR =< postPhaserR;
            if (phaserShredID == -1) {
                Machine.add("phaser.ck") => phaserShredID;
            }
            prePhaserL => directPhaserL => postPhaserL;
            prePhaserR => directPhaserR => postPhaserR;
            prePhaserL => G.phaserPole1L => G.phaserPole2L => G.phaserPole3L => G.phaserPole4L => phaserGainL => postPhaserL;
            prePhaserR => G.phaserPole1R => G.phaserPole2R => G.phaserPole3R => G.phaserPole4R => phaserGainR => postPhaserR;
            G.phaserPole4L => phaserFbL => G.phaserPole1L;
            G.phaserPole4R => phaserFbR => G.phaserPole1R;
            1 => wetConnectedPhaser;
        }
        
        // Ring mod dynamic connection
        if (G.gkRingBypass >= 0.5 && G.gkRingOnSmooth < 0.01 && wetConnectedRing) {
            if (ringShredID != -1) {
                Machine.remove(ringShredID);
                -1 => ringShredID;
            }
            preRingL =< ringGainL;
            ringGainL =< postRingL;
            preRingR =< ringGainR;
            ringGainR =< postRingR;
            0 => wetConnectedRing;
        } else if (G.gkRingBypass < 0.5 && !wetConnectedRing) {
            if (ringShredID == -1) {
                Machine.add("ringmod.ck") => ringShredID;
            }
            preRingL => ringGainL => postRingL;
            preRingR => ringGainR => postRingR;
            1 => wetConnectedRing;
        }
        
        // Spectral dynamic connection
        if (G.gkSpectralBypass >= 0.5 && G.gkSpectralOnSmooth < 0.01 && wetConnectedSpectral) {
            if (spectralShredID != -1) {
                Machine.remove(spectralShredID);
                -1 => spectralShredID;
            }
            preSpectralL =< fftL;
            fftL =< ifftL;
            ifftL =< spectralGainL;
            spectralGainL =< postSpectralL;
            preSpectralR =< fftR;
            fftR =< ifftR;
            ifftR =< spectralGainR;
            spectralGainR =< postSpectralR;
            preSpectralL =< directSpectralL;
            directSpectralL =< postSpectralL;
            preSpectralR =< directSpectralR;
            directSpectralR =< postSpectralR;
            preSpectralL => postSpectralL;
            preSpectralR => postSpectralR;
            0 => wetConnectedSpectral;
        } else if (G.gkSpectralBypass < 0.5 && !wetConnectedSpectral) {
            preSpectralL =< postSpectralL;
            preSpectralR =< postSpectralR;
            if (spectralShredID == -1) {
                Machine.add("spectralWah.ck") => spectralShredID;
            }
            preSpectralL => directSpectralL => postSpectralL;
            preSpectralR => directSpectralR => postSpectralR;
            preSpectralL => fftL => ifftL => spectralGainL => postSpectralL;
            preSpectralR => fftR => ifftR => spectralGainR => postSpectralR;
            1 => wetConnectedSpectral;
        }
        
        // Pitch shift dynamic connection
        if (G.gkPitchBypass >= 0.5 && G.gkPitchOnSmooth < 0.01 && wetConnectedPitch) {
            if (pitchShredID != -1) {
                Machine.remove(pitchShredID);
                -1 => pitchShredID;
            }
            1.0 => directPitchL.gain;
            1.0 => directPitchR.gain;
            postSpectralL =< pitchShiftL;
            pitchShiftL =< pitchWetL;
            pitchWetL =< preDelayL;
            postSpectralR =< pitchShiftR;
            pitchShiftR =< pitchWetR;
            pitchWetR =< preDelayR;
            postSpectralL =< directPitchL;
            directPitchL =< preDelayL;
            postSpectralR =< directPitchR;
            directPitchR =< preDelayR;
            postSpectralL => directPitchL => preDelayL;
            postSpectralR => directPitchR => preDelayR;
            0 => wetConnectedPitch;
        } else if (G.gkPitchBypass < 0.5 && !wetConnectedPitch) {
            postSpectralL =< directPitchL;
            directPitchL =< preDelayL;
            postSpectralR =< directPitchR;
            directPitchR =< preDelayR;
            if (pitchShredID == -1) {
                Machine.add("pitchShifter.ck") => pitchShredID;
            }
            postSpectralL => directPitchL => preDelayL;
            postSpectralR => directPitchR => preDelayR;
            postSpectralL => pitchShiftL => pitchWetL => preDelayL;
            postSpectralR => pitchShiftR => pitchWetR => preDelayR;
            1 => wetConnectedPitch;
        }
        
        // Delay dynamic connection
        if (G.gkDelayBypass >= 0.5 && G.gkDelayOnSmooth < 0.01 && wetConnectedDelay) {
            if (delayShredID != -1) {
                Machine.remove(delayShredID);
                -1 => delayShredID;
            }
            preDelayL =< delayDelayL;
            delayDelayL =< delPitchShiftL;
            delPitchShiftL =< delayGainL;
            delPitchShiftL =< delayFbL;
            delayFbL =< delayDelayL;
            delayGainL =< postDelayL;
            preDelayR =< delayDelayR;
            delayDelayR =< delPitchShiftR;
            delPitchShiftR =< delayGainR;
            delPitchShiftR =< delayFbR;
            delayFbR =< delayDelayR;
            delayGainR =< postDelayR;
            preDelayL =< directDelayL;
            directDelayL =< postDelayL;
            preDelayR =< directDelayR;
            directDelayR =< postDelayR;
            preDelayL => postDelayL;
            preDelayR => postDelayR;
            0 => wetConnectedDelay;
        } else if (G.gkDelayBypass < 0.5 && !wetConnectedDelay) {
            preDelayL =< postDelayL;
            preDelayR =< postDelayR;
            if (delayShredID == -1) {
                Machine.add("delay1.ck") => delayShredID;
            }
            preDelayL => delayDelayL => delPitchShiftL => delayGainL => postDelayL;
            delPitchShiftL => delayFbL => delayDelayL;
            preDelayR => delayDelayR => delPitchShiftR => delayGainR => postDelayR;
            delPitchShiftR => delayFbR => delayDelayR;
            preDelayL => directDelayL => postDelayL;
            preDelayR => directDelayR => postDelayR;
            1 => wetConnectedDelay;
        }
        
        // Reverb dynamic connection
        if (G.gkReverbBypass >= 0.5 && G.gkReverbOnSmooth < 0.01 && wetConnectedReverb) {
            if (reverbShredID != -1) {
                Machine.remove(reverbShredID);
                -1 => reverbShredID;
            }
            preReverbL =< revL;
            revL =< reverbGainL;
            reverbGainL =< postReverbL;
            preReverbR =< revR;
            revR =< reverbGainR;
            reverbGainR =< postReverbR;
            preReverbL =< directReverbL;
            directReverbL =< postReverbL;
            preReverbR =< directReverbR;
            directReverbR =< postReverbR;
            preReverbL => postReverbL;
            preReverbR => postReverbR;
            0.0 => revL.mix;
            0.0 => revR.mix;
            0 => wetConnectedReverb;
        } else if (G.gkReverbBypass < 0.5 && !wetConnectedReverb) {
            preReverbL =< postReverbL;
            preReverbR =< postReverbR;
            if (reverbShredID == -1) {
                Machine.add("reverb1.ck") => reverbShredID;
            }
            preReverbL => directReverbL => postReverbL;
            preReverbR => directReverbR => postReverbR;
            preReverbL => revL => reverbGainL => postReverbL;
            preReverbR => revR => reverbGainR => postReverbR;
            1.0 => revL.mix;
            1.0 => revR.mix;
            1 => wetConnectedReverb;
        }
    }
}

Machine.add("synth.ck");
50::ms => now;  
Machine.add("midi.ck");
Machine.add("volume.ck");
spork ~ dynamicConnectLoop();

while (true) {
    10000000::ms => now;
}