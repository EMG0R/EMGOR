// globals.ck
public class G {
    // Synth modes
    static int SYNTH_WAVETABLE;
    static int SYNTH_CLASSIC;
    static int SYNTH_PHYSMOD;
    static int SYNTH_VOCAL;
    
    // Classic wave types
    static int CLASSIC_SIN;
    static int CLASSIC_TRI;
    static int CLASSIC_SAW;
    static int CLASSIC_PLS;
    static int CLASSIC_NSE;
    
    // Physical models
    static int PHYS_BAND;
    static int PHYS_BOTL;
    static int PHYS_BHOL;
    static int PHYS_BOWD;
    static int PHYS_BRAS;
    static int PHYS_CLAR;
    static int PHYS_FLUT;
    static int PHYS_MAND;
    static int PHYS_MODA;
    static int PHYS_SAXO;
    static int PHYS_SHKR;
    static int PHYS_SITR;
    static int PHYS_STIF;
    static int PHYS_VOIC;
    
    
    // Vocal syllable types
    static int VOCAL_CHUHH;
    static int VOCAL_KAYEH;
    static int VOCAL_HOH;
    static int VOCAL_DER;
    static int VOCAL_PHII;
    static int VOCAL_BETAY;
    static int VOCAL_THOE;
    static int VOCAL_DHAA;
    static int VOCAL_KHOO;
    static int VOCAL_GHUU;
    static int VOCAL_CHIU;
    static int VOCAL_RHAE;
    static int VOCAL_HHAO;
    static int VOCAL_PHARA;
    
    // Core parameters
    static float bpm;
    static float UI_NOTEprob;
    static int synthMode;
    static int classicWaveType;
    static float phys_pos_x;
    static float phys_pos_y;
    static float vocal_pos_x;
    static float vocal_pos_y;
    static int num_x;
    static int num_y;
    static int table_size;
    static float wavetables[4][4][2048];
    static int num_harmonics;
    static int physModels[4][4];
    static int vocalSyllables[4][4];
    static float note_probs[12];
    static float note_pitches[12];
    static int octaveINIT;
    static int playing[200];
    
    // Envelope parameters
    static float noteATTACK_seconds;
    static float noteDECAY_seconds;
    static float noteSUSTAIN;
    static float noteRELEASE_seconds;
    static float pitchATTACK_seconds;
    static float pitchDECAY_seconds;
    static float pitchSUSTAIN;
    static float pitchRELEASE_seconds;
    static float pitch_offset_start;
    static float pitch_offset_end;
    
    // Filter parameters
    static float filterBaseFreq;
    static float filterAmount;
    static float filterResonance;
    static float filterATTACK_seconds;
    static float filterDECAY_seconds;
    static float filterSUSTAIN;
    static float filterRELEASE_seconds;
    
    // Modulation parameters
    static float lfoRate;
    static float lfoDepth;
    static float pnoisePROBABILITY;
    static float pnoiseSMOOTHING;
    static float pnoiseDEPTH;
    static float pulseWidth;
    static float pwmDepth;
    static float gainScale;
    
    // FX parameters - Volume/Bypass
    static float gkVOLpedal;
    static float gkVOLpedalBypass;
    static int gkLastToggle;
    static float gkPortTime;
    static float prevVOLpedal;
    static float prevVOLpedalBypass;
    
    // FX parameters - Vibrato
    static float gkVibBypass;
    static float prevVibBypass;
    static float gkVibRate;
    static float gkVibAmount;
    static float prevVibRate;
    static float prevVibAmount;
    static float gkVibFeedback;
    static float prevVibFeedback;
    
    // FX parameters - Chorus
    static float gkChorusBypass;
    static float prevChorusBypass;
    static float gkChorusRate;
    static float gkChorusAmount;
    static float prevChorusRate;
    static float prevChorusAmount;
    static float gkChorusFeedback;
    static float prevChorusFeedback;
    
    // FX parameters - Phaser
    static float gkPhaserBypass;
    static float prevPhaserBypass;
    static float gkPhaserRate;
    static float prevPhaserRate;
    static float gkPhaserDepth;
    static float prevPhaserDepth;
    static float gkPhaserFeedback;
    static float prevPhaserFeedback;
    
    // Phaser pole filters
    static TwoPole phaserPole1L;
    static TwoPole phaserPole2L;
    static TwoPole phaserPole3L;
    static TwoPole phaserPole4L;
    static TwoPole phaserPole1R;
    static TwoPole phaserPole2R;
    static TwoPole phaserPole3R;
    static TwoPole phaserPole4R;
    static Gain directPhaserL;
    static Gain directPhaserR;
    
    // FX parameters - Pitch
    static float gkPitchBypass;
    static float prevPitchBypass;
    static float gkPitchMix;
    static float prevPitchMix;
    static float gkPitchShift;
    static float prevPitchShift;
    
    // FX parameters - Ring
    static float gkRingBypass;
    static float prevRingBypass;
    static float gkRingRate;
    static float prevRingRate;
    
    // FX parameters - Spectral
    static float gkSpectralBypass;
    static float prevSpectralBypass;
    static float gkSpectralWah;
    static float prevSpectralWah;
    
    // FX parameters - Delay
    static float gkDelayBypass;
    static float prevDelayBypass;
    static float gkDelayMix;
    static float prevDelayMix;
    static float gkDelayTime;
    static float prevDelayTime;
    static float gkDelayFeedback;
    static float prevDelayFeedback;
    static float gkDelayModRate;
    static float prevDelayModRate;
    static float gkDelayModDepth;
    static float prevDelayModDepth;
    static float gkDelayPitch;
    static float prevDelayPitch;
    
    // FX parameters - Reverb
    static float gkReverbBypass;
    static float prevReverbBypass;
    static float gkReverbMix;
    static float prevReverbMix;
    static float gkReverbTime;
    static float prevReverbTime;
    
    // Smoothing and OSC
    static OscOut xout;
    static float gkVolOnSmooth;
    static float gkVibOnSmooth;
    static float gkChorusOnSmooth;
    static float gkPhaserOnSmooth;
    static float gkPitchOnSmooth;
    static float gkRingOnSmooth;
    static float gkSpectralOnSmooth;
    static float gkDelayOnSmooth;
    static float gkReverbOnSmooth;
    
    // UI state
    static float prevUI_NOTEprob;
    static float prevBpm;
    static int gkGridLock;
    static int currentMenu;
    static int currentSynthModeIndex;
    static int currentClassicShapeIndex;
    
    fun static void init() {
        // Initialize dimensions
        4 => num_x;
        4 => num_y;
        2048 => table_size;
        20 => num_harmonics;
        
        // Initialize synth mode values
        0 => SYNTH_WAVETABLE;
        1 => SYNTH_CLASSIC;
        2 => SYNTH_PHYSMOD;
        3 => SYNTH_VOCAL;
        
        // Initialize classic wave types
        0 => CLASSIC_SIN;
        1 => CLASSIC_TRI;
        2 => CLASSIC_SAW;
        3 => CLASSIC_PLS;
        4 => CLASSIC_NSE;
        
        // Initialize physical model types
        0 => PHYS_BAND;
        1 => PHYS_BOTL;
        2 => PHYS_BHOL;
        3 => PHYS_BOWD;
        4 => PHYS_BRAS;
        5 => PHYS_CLAR;
        6 => PHYS_FLUT;
        7 => PHYS_MAND;
        8 => PHYS_MODA;
        9 => PHYS_SAXO;
        10 => PHYS_SHKR;
        11 => PHYS_SITR;
        12 => PHYS_STIF;
        13 => PHYS_VOIC;
        
        // Initialize vocal syllable types
        0 => VOCAL_CHUHH;
        1 => VOCAL_KAYEH;
        2 => VOCAL_HOH;
        3 => VOCAL_DER;
        4 => VOCAL_PHII;
        5 => VOCAL_BETAY;
        6 => VOCAL_THOE;
        7 => VOCAL_DHAA;
        8 => VOCAL_KHOO;
        9 => VOCAL_GHUU;
        10 => VOCAL_CHIU;
        11 => VOCAL_RHAE;
        12 => VOCAL_HHAO;
        13 => VOCAL_PHARA;
        
        // Initialize physical model matrix
        PHYS_BAND => physModels[0][0];
        PHYS_BOTL => physModels[0][1];
        PHYS_BHOL => physModels[0][2];
        PHYS_BOWD => physModels[0][3];
        PHYS_BRAS => physModels[1][0];
        PHYS_CLAR => physModels[1][1];
        PHYS_FLUT => physModels[1][2];
        PHYS_MAND => physModels[1][3];
        PHYS_MODA => physModels[2][0];
        PHYS_SAXO => physModels[2][1];
        PHYS_SHKR => physModels[2][2];
        PHYS_SITR => physModels[2][3];
        PHYS_STIF => physModels[3][0];
        PHYS_VOIC => physModels[3][1];
        PHYS_BHOL => physModels[3][2];
        PHYS_BOWD => physModels[3][3];
        
        // Initialize vocal syllable matrix
        VOCAL_CHUHH => vocalSyllables[0][0];
        VOCAL_KAYEH => vocalSyllables[0][1];
        VOCAL_HOH => vocalSyllables[0][2];
        VOCAL_DER => vocalSyllables[0][3];
        VOCAL_PHII => vocalSyllables[1][0];
        VOCAL_BETAY => vocalSyllables[1][1];
        VOCAL_THOE => vocalSyllables[1][2];
        VOCAL_DHAA => vocalSyllables[1][3];
        VOCAL_KHOO => vocalSyllables[2][0];
        VOCAL_GHUU => vocalSyllables[2][1];
        VOCAL_CHIU => vocalSyllables[2][2];
        VOCAL_RHAE => vocalSyllables[2][3];
        VOCAL_HHAO => vocalSyllables[3][0];
        VOCAL_PHARA => vocalSyllables[3][1];
        VOCAL_CHUHH => vocalSyllables[3][2];
        VOCAL_KAYEH => vocalSyllables[3][3];
        
        // Initialize positions
        1.0 => phys_pos_x;
        0.0 => phys_pos_y;
        1.0 => vocal_pos_x;
        0.0 => vocal_pos_y;
        
        // Initialize synth state
        2 => synthMode;
        3 => classicWaveType;
        320.0 => bpm;
        0.6 => UI_NOTEprob;
        -12 => octaveINIT;
        
        // Initialize note probabilities
        1.0 => note_probs[0];
        0.0 => note_probs[1];
        1.0 => note_probs[2];
        0.0 => note_probs[3];
        1.0 => note_probs[4];
        1.0 => note_probs[5];
        0.0 => note_probs[6];
        1.0 => note_probs[7];
        0.0 => note_probs[8];
        1.0 => note_probs[9];
        0.0 => note_probs[10];
        1.0 => note_probs[11];
        
        // Initialize note pitches
        float baseFreq;
        Std.mtof(60) => baseFreq;
        baseFreq * Math.pow(2, 0.0/1200) => note_pitches[0];
        baseFreq * Math.pow(2, 100.0/1200) => note_pitches[1];
        baseFreq * Math.pow(2, 200.0/1200) => note_pitches[2];
        baseFreq * Math.pow(2, 300.0/1200) => note_pitches[3];
        baseFreq * Math.pow(2, 400.0/1200) => note_pitches[4];
        baseFreq * Math.pow(2, 500.0/1200) => note_pitches[5];
        baseFreq * Math.pow(2, 600.0/1200) => note_pitches[6];
        baseFreq * Math.pow(2, 700.0/1200) => note_pitches[7];
        baseFreq * Math.pow(2, 800.0/1200) => note_pitches[8];
        baseFreq * Math.pow(2, 900.0/1200) => note_pitches[9];
        baseFreq * Math.pow(2, 1000.0/1200) => note_pitches[10];
        baseFreq * Math.pow(2, 1100.0/1200) => note_pitches[11];
        
        // Initialize envelope parameters
        0.0 => noteATTACK_seconds;
        2.0 => noteDECAY_seconds;
        0.0 => noteSUSTAIN;
        2.0 => noteRELEASE_seconds;
        0.0 => pitchATTACK_seconds;
        1.0 => pitchDECAY_seconds;
        10.0 => pitchSUSTAIN;
        1.0 => pitchRELEASE_seconds;
        -12.0 => pitch_offset_start;
        -12.0 => pitch_offset_end;
        
        // Initialize filter parameters
        20000.0 => filterBaseFreq;
        0.0 => filterAmount;
        0.0 => filterResonance;
        0.2 => filterATTACK_seconds;
        0.1 => filterDECAY_seconds;
        0.0 => filterSUSTAIN;
        0.0 => filterRELEASE_seconds;
        
        // Initialize modulation parameters
        0.1 => lfoRate;
        0.5 => lfoDepth;
        0.0 => pnoisePROBABILITY;
        0.99 => pnoiseSMOOTHING;
        5.0 => pnoiseDEPTH;
        0.5 => pulseWidth;
        1.0 => pwmDepth;
        0.1 => gainScale;
        
        // Initialize FX parameters
        1.0 => gkVOLpedal;
        1.0 => gkVOLpedalBypass;
        1 => gkLastToggle;
        0.03 => gkPortTime;
        -1.0 => prevVOLpedal;
        -1.0 => prevVOLpedalBypass;
        
        1.0 => gkVibBypass;
        -1.0 => prevVibBypass;
        0.6 => gkVibRate;
        0.18 => gkVibAmount;
        -1.0 => prevVibRate;
        -1.0 => prevVibAmount;
        0.0 => gkVibFeedback;
        -1.0 => prevVibFeedback;
        
        1.0 => gkChorusBypass;
        -1.0 => prevChorusBypass;
        0.06 => gkChorusRate;
        0.5 => gkChorusAmount;
        -1.0 => prevChorusRate;
        -1.0 => prevChorusAmount;
        0.0 => gkChorusFeedback;
        -1.0 => prevChorusFeedback;
        
        // Initialize phaser parameters
        1.0 => gkPhaserBypass;
        -1.0 => prevPhaserBypass;
        0.3 => gkPhaserRate;
        -1.0 => prevPhaserRate;
        0.5 => gkPhaserDepth;
        -1.0 => prevPhaserDepth;
        0.3 => gkPhaserFeedback;
        -1.0 => prevPhaserFeedback;
        
        1.0 => gkPitchBypass;
        -1.0 => prevPitchBypass;
        0.5 => gkPitchMix;
        -1.0 => prevPitchMix;
        0.0 => gkPitchShift;
        -1.0 => prevPitchShift;
        
        1.0 => gkRingBypass;
        -1.0 => prevRingBypass;
        0.5 => gkRingRate;
        -1.0 => prevRingRate;
        
        1.0 => gkSpectralBypass;
        -1.0 => prevSpectralBypass;
        0.5 => gkSpectralWah;
        -1.0 => prevSpectralWah;
        
        1.0 => gkDelayBypass;
        -1.0 => prevDelayBypass;
        0.5 => gkDelayMix;
        -1.0 => prevDelayMix;
        0.5 => gkDelayTime;
        -1.0 => prevDelayTime;
        0.5 => gkDelayFeedback;
        -1.0 => prevDelayFeedback;
        0.06 => gkDelayModRate;
        -1.0 => prevDelayModRate;
        0.5 => gkDelayModDepth;
        -1.0 => prevDelayModDepth;
        0.0 => gkDelayPitch;
        -1.0 => prevDelayPitch;
        
        1.0 => gkReverbBypass;
        -1.0 => prevReverbBypass;
        0.5 => gkReverbMix;
        -1.0 => prevReverbMix;
        0.5 => gkReverbTime;
        -1.0 => prevReverbTime;
        
        // Initialize OSC
        xout.dest("127.0.0.1", 8000);
        
        // Initialize smoothing
        0.0 => gkVolOnSmooth;
        0.0 => gkVibOnSmooth;
        0.0 => gkChorusOnSmooth;
        0.0 => gkPhaserOnSmooth;
        0.0 => gkPitchOnSmooth;
        0.0 => gkRingOnSmooth;
        0.0 => gkSpectralOnSmooth;
        0.0 => gkDelayOnSmooth;
        0.0 => gkReverbOnSmooth;
        
        // Initialize UI state
        -1.0 => prevUI_NOTEprob;
        -1.0 => prevBpm;
        0 => gkGridLock;
        0 => currentMenu;
        2 => currentSynthModeIndex;
        3 => currentClassicShapeIndex;
        
        // Initialize playing array
        for (0 => int i; i < 200; i++) {
            0 => playing[i];
        }
    }
}