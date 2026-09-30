// UI and timing
float UI_NOTEprob;
0.6 => UI_NOTEprob;  
float bpm;
320.0 => bpm; 
dur beat;
(30.0 / bpm)::second => beat;
dur shortDur;
2.4 * beat => shortDur;  

// Synth mode and classic parameters
string synthMODE;
"PHYSMOD" => synthMODE;
string classicWAVETYPE;
"PLS" => classicWAVETYPE;
float pulseWidth;
0.5 => pulseWidth;
float pwmDepth;
1.0 => pwmDepth;

// note env
dur noteATTACK;
0::ms => noteATTACK;
dur noteDECAY;
2000::ms => noteDECAY;  
float noteSUSTAIN;
0 => noteSUSTAIN;  
dur noteRELEASE;
2000::ms => noteRELEASE;

// pitch env
dur pitchATTACK;
0::ms => pitchATTACK;  
dur pitchDECAY;
1000::ms => pitchDECAY;
float pitchSUSTAIN;
10 => pitchSUSTAIN;
dur pitchRELEASE;
1000::ms => pitchRELEASE;
float pitch_offset_start;
-12 => pitch_offset_start;
float pitch_offset_end;
-12 => pitch_offset_end;

// Filter param
float filterBaseFreq;
20000.0 => filterBaseFreq;
float filterAmount;
0.0 => filterAmount;
float filterResonance;
0.0 => filterResonance;  
dur filterATTACK;
200::ms => filterATTACK;  // Adjusted for slow movement
dur filterDECAY;
100::ms => filterDECAY;
float filterSUSTAIN;
0 => filterSUSTAIN;
dur filterRELEASE;
0::ms => filterRELEASE;

// LFO
float lfoRate;
0.1 => lfoRate;  // Slightly faster but still slow
float lfoDepth;
0.5 => lfoDepth;  // Reduced for smoother movement

// noise mod
float pnoisePROBABILITY;
0.0 => pnoisePROBABILITY;
float pnoiseSMOOTHING;
0.99 => pnoiseSMOOTHING;
float pnoiseDEPTH;
5.0 => pnoiseDEPTH;

// gain/oct
float gainScale;
0.3 => gainScale;  // Slightly increased
int octaveINIT;
-12 => octaveINIT;  // Raised to audible range

// timbre
float generativeTIMBRE_scaler;
0 => generativeTIMBRE_scaler;

// Wavetable parameters
int num_x;
4 => num_x;
int num_y;
4 => num_y;
int table_size;
2048 => table_size;
float wavetables[num_x][num_y][table_size];
int num_harmonics;
20 => num_harmonics;

// Physical model parameters
// BandedWG (BAND)
int bandedPreset;
0 => bandedPreset;
float bandedBowPressure;
0.5 => bandedBowPressure;
float bandedBowMotion;
0.5 => bandedBowMotion;
float bandedBowRate;
0.5 => bandedBowRate;
float bandedVibratoFreq;
6.0 => bandedVibratoFreq;
float bandedIntegration;
0.5 => bandedIntegration;
float bandedModesGain;
0.5 => bandedModesGain;
float bandedStrikePosition;
0.5 => bandedStrikePosition;

// BlowBotl (BOTL)
float blowBotlNoiseGain;
0.0 => blowBotlNoiseGain;
float blowBotlVibratoFreq;
6.0 => blowBotlVibratoFreq;
float blowBotlVibratoGain;
0.0 => blowBotlVibratoGain;
float blowBotlVolume;
1.0 => blowBotlVolume;
float blowBotlRate;
0.5 => blowBotlRate;

// BlowHole (BHOL)
float blowHoleReed;
0.5 => blowHoleReed;
float blowHoleNoiseGain;
0.0 => blowHoleNoiseGain;
float blowHoleTonehole;
0.5 => blowHoleTonehole;
float blowHoleVent;
0.5 => blowHoleVent;
float blowHoleRegister;
0.5 => blowHoleRegister;
float blowHolePressure;
1.0 => blowHolePressure;

// Bowed (BOWD)
float bowedBowPressure;
0.25 => bowedBowPressure;
float bowedBowPosition;
0.75 => bowedBowPosition;
float bowedVibratoFreq;
6.0 => bowedVibratoFreq;
float bowedVibratoGain;
0.02 => bowedVibratoGain;
float bowedVolume;
1.0 => bowedVolume;

// Brass (BRAS)
float brassLip;
0.5 => brassLip;
float brassSlide;
0.5 => brassSlide;
float brassVibratoFreq;
6.0 => brassVibratoFreq;
float brassVibratoGain;
0.0 => brassVibratoGain;
float brassVolume;
1.0 => brassVolume;

// Clarinet (CLAR)
float clarinetReed;
0.5 => clarinetReed;
float clarinetNoiseGain;
0.0 => clarinetNoiseGain;
float clarinetVibratoFreq;
6.0 => clarinetVibratoFreq;
float clarinetVibratoGain;
0.0 => clarinetVibratoGain;
float clarinetPressure;
1.0 => clarinetPressure;

// Flute (FLUT)
float fluteJetDelay;
0.5 => fluteJetDelay;
float fluteJetReflection;
0.5 => fluteJetReflection;
float fluteEndReflection;
0.5 => fluteEndReflection;
float fluteNoiseGain;
0.0 => fluteNoiseGain;
float fluteVibratoFreq;
6.0 => fluteVibratoFreq;
float fluteVibratoGain;
0.0 => fluteVibratoGain;
float flutePressure;
1.0 => flutePressure;

// Mandolin (MAND)
float mandolinBodySize;
1.0 => mandolinBodySize;
float mandolinPluckPos;
0.4 => mandolinPluckPos;
float mandolinStringDamping;
0.5 => mandolinStringDamping;
float mandolinStringDetune;
0.05 => mandolinStringDetune;

// ModalBar (MODA)
int modalBarPreset;
0 => modalBarPreset;
float modalBarStickHardness;
0.5 => modalBarStickHardness;
float modalBarStrikePosition;
0.5 => modalBarStrikePosition;
float modalBarVibratoFreq;
6.0 => modalBarVibratoFreq;
float modalBarVibratoGain;
0.0 => modalBarVibratoGain;
float modalBarDirectGain;
0.5 => modalBarDirectGain;
float modalBarMasterGain;
1.0 => modalBarMasterGain;
float modalBarVolume;
1.0 => modalBarVolume;

// Saxofony (SAXO)
float saxofonyStiffness;
0.5 => saxofonyStiffness;
float saxofonyAperture;
0.5 => saxofonyAperture;
float saxofonyNoiseGain;
0.0 => saxofonyNoiseGain;
float saxofonyVibratoFreq;
6.0 => saxofonyVibratoFreq;
float saxofonyVibratoGain;
0.0 => saxofonyVibratoGain;
float saxofonyBlowPosition;
0.5 => saxofonyBlowPosition;
float saxofonyPressure;
1.0 => saxofonyPressure;

// Shakers (SHKR)
int shakersPreset;
0 => shakersPreset;
float shakersEnergy;
1.0 => shakersEnergy;
float shakersDecay;
0.95 => shakersDecay;

// Sitar (SITR)
// No additional parameters

// StifKarp (STIF)
float stifKarpPickupPosition;
0.5 => stifKarpPickupPosition;
float stifKarpSustain;
0.5 => stifKarpSustain;
float stifKarpStretch;
0.5 => stifKarpStretch;
float stifKarpBaseLoopGain;
0.5 => stifKarpBaseLoopGain;

// VoicForm (VOIC)
string voicFormPhoneme;
"eee" => voicFormPhoneme;
float voicFormVoiced;
0.5 => voicFormVoiced;
float voicFormUnVoiced;
0.5 => voicFormUnVoiced;
float voicFormPitchSweepRate;
0.5 => voicFormPitchSweepRate;

// Note probabilities
float NOTE1prob;  // C
1.0 => NOTE1prob;
float NOTE2prob;  // C#
0.0 => NOTE2prob;
float NOTE3prob;  // D
1.0 => NOTE3prob;
float NOTE4prob;  // D#
0.0 => NOTE4prob;
float NOTE5prob;  // E
1.0 => NOTE5prob;
float NOTE6prob;  // F
1.0 => NOTE6prob;
float NOTE7prob;  // F#
0.0 => NOTE7prob;
float NOTE8prob;  // G
1.0 => NOTE8prob;
float NOTE9prob;  // G#
0.0 => NOTE9prob;
float NOTE10prob; // A
1.0 => NOTE10prob;
float NOTE11prob; // A#
0.0 => NOTE11prob;
float NOTE12prob; // B
1.0 => NOTE12prob;

// Base frequency
float baseFreq;
Std.mtof(60) => baseFreq;

float note_probs[12];
NOTE1prob => note_probs[0];
NOTE2prob => note_probs[1];
NOTE3prob => note_probs[2];
NOTE4prob => note_probs[3];
NOTE5prob => note_probs[4];
NOTE6prob => note_probs[5];
NOTE7prob => note_probs[6];
NOTE8prob => note_probs[7];
NOTE9prob => note_probs[8];
NOTE10prob => note_probs[9];
NOTE11prob => note_probs[10];
NOTE12prob => note_probs[11];

float note_pitches[12];
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

int octaveOFFSET_total;

// Track playing notes by MIDI note number
int playing[200];

// Custom WavetableOsc class
class WavetableOsc extends Chugen {
    float pos_x;
    float pos_y;
    float my_freq;
    float my_phase;
    float phase_inc;
    float sr;
    1.0::second / 1.0::samp => sr;

    fun void freq(float f) {
        f => my_freq;
        my_freq / sr => phase_inc;
    }

    fun void setPos(float px, float py) {
        px => pos_x;
        py => pos_y;
    }

    fun float tick(float in) {
        my_phase + phase_inc => my_phase;
        while (my_phase >= 1.0) {
            my_phase - 1.0 => my_phase;
        }
        int s;
        (my_phase * table_size) $ int % table_size => s;

        int x1;
        Math.floor(pos_x) $ int => x1;
        float frac_x;
        pos_x - x1 => frac_x;
        int x2;
        x1 + 1 => x2;
        if (x2 >= num_x) {
            x1 => x2;
            0.0 => frac_x;
        }

        int y1;
        Math.floor(pos_y) $ int => y1;
        float frac_y;
        pos_y - y1 => frac_y;
        int y2;
        y1 + 1 => y2;
        if (y2 >= num_y) {
            y1 => y2;
            0.0 => frac_y;
        }

        float v11;
        wavetables[x1][y1][s] => v11;
        float v12;
        wavetables[x1][y2][s] => v12;
        float v21;
        wavetables[x2][y1][s] => v21;
        float v22;
        wavetables[x2][y2][s] => v22;

        float v1;
        v11 * (1 - frac_y) + v12 * frac_y => v1;
        float v2;
        v21 * (1 - frac_y) + v22 * frac_y => v2;

        return v1 * (1 - frac_x) + v2 * frac_x;
    }
}

// Generate harmonic table
fun float[] genHarmonicTable(float x_frac, float y_frac) {
    float table[table_size];
    float max_val;
    0.0 => max_val;
    float power;
    0.5 + x_frac * 1.5 => power;  // Vary decay: low = complex (more harmonics), high = simple

    for (0 => int s; s < table_size; s++) {
        0.0 => table[s];
        for (1 => int h; h <= num_harmonics; h++) {
            float amp;
            1.0 / Math.pow(h, power) => amp;
            float sign;
            1.0 => sign;
            if (Math.randomf() < y_frac) {  // Higher y = more random signs for complexity/inharmonicity
                -1.0 => sign;
            } else if (y_frac < 0.3 && ((h - 1)/2 % 2) == 1) {
                -1.0 => sign;  // Alternate for triangle-like geometry
            }
            if (y_frac < 0.5 && h % 2 == 0) {
                0.0 => amp;  // Odd only for lower y, geometric purity
            }
            table[s] + Math.sin(2 * Math.PI * h * s / table_size) * amp * sign => table[s];
        }
        if (Math.fabs(table[s]) > max_val) {
            Math.fabs(table[s]) => max_val;
        }
    }
    if (max_val > 0.0) {
        for (0 => int s; s < table_size; s++) {
            table[s] / max_val => table[s];
        }
    }
    return table;
}

// Generate all wavetables
fun void genWavetables() {
    for (0 => int x; x < num_x; x++) {
        float x_frac;
        if (num_x > 1) x * 1.0 / (num_x - 1) => x_frac; else 0.0 => x_frac;
        for (0 => int y; y < num_y; y++) {
            float y_frac;
            if (num_y > 1) y * 1.0 / (num_y - 1) => y_frac; else 0.0 => y_frac;
            genHarmonicTable(x_frac, y_frac) @=> wavetables[x][y];
        }
    }
}

genWavetables();

string physModels[num_x][num_y];
"BAND" => physModels[0][0];
"BOTL" => physModels[0][1];
"BHOL" => physModels[0][2];
"BOWD" => physModels[0][3];
"BRAS" => physModels[1][0];
"CLAR" => physModels[1][1];
"FLUT" => physModels[1][2];
"MAND" => physModels[1][3];
"MODA" => physModels[2][0];
"SAXO" => physModels[2][1];
"SHKR" => physModels[2][2];
"SITR" => physModels[2][3];
"STIF" => physModels[3][0];
"VOIC" => physModels[3][1];
"BHOL" => physModels[3][2];
"BOWD" => physModels[3][3];

float phys_pos_x;
float phys_pos_y;
1.0 => phys_pos_x;
0.0 => phys_pos_y;

fun void safeAdvance(dur t) {
    if (t > 0::second) {
        t => now;
    }
}

fun void modulate(UGen osc, LPF lpf, float baseNote, dur sustainDur, dur releaseDur, int useNoise, string physModel) {
    SinOsc lfo => blackhole;
    lfoRate => lfo.freq;
    1.0 => lfo.gain;
    Noise n;
    OnePole op;
    if (useNoise) {
        n => op => blackhole;
        1.0 => n.gain;
        op.pole(pnoiseSMOOTHING);
    }
    time startTime;
    now => startTime;
    dur pitchAttackTime;
    pitchATTACK => pitchAttackTime;
    dur pitchDecayTime;
    pitchDECAY => pitchDecayTime;
    float pitchSustainLevel;
    pitchSUSTAIN => pitchSustainLevel;
    dur pitchReleaseTime;
    pitchRELEASE => pitchReleaseTime;
    dur pitchFullSustainStart;
    pitchAttackTime + pitchDecayTime => pitchFullSustainStart;

    // Calculate pitch release start level
    float pitchReleaseStart;
    0.0 => pitchReleaseStart;
    dur elapsedAtKeyOff;
    sustainDur => elapsedAtKeyOff;
    if (pitchAttackTime > 0::second && elapsedAtKeyOff < pitchAttackTime) {
        elapsedAtKeyOff / pitchAttackTime => pitchReleaseStart;
    } else if (pitchDecayTime > 0::second && elapsedAtKeyOff < pitchFullSustainStart) {
        dur decayElapsed;
        elapsedAtKeyOff - pitchAttackTime => decayElapsed;
        1.0 + (decayElapsed / pitchDecayTime) * (pitchSustainLevel - 1.0) => pitchReleaseStart;
    } else {
        pitchSustainLevel => pitchReleaseStart;
    }
    
    dur filterAttackTime;
    filterATTACK => filterAttackTime;
    dur filterDecayTime;
    filterDECAY => filterDecayTime;
    float filterSustainLevel;
    filterSUSTAIN => filterSustainLevel;
    dur filterReleaseTime;
    filterRELEASE => filterReleaseTime;
    dur filterFullSustainStart;
    filterAttackTime + filterDecayTime => filterFullSustainStart;

    // Calculate filter release start level
    float filterReleaseStart;
    0.0 => filterReleaseStart;
    sustainDur => elapsedAtKeyOff;
    if (filterAttackTime > 0::second && elapsedAtKeyOff < filterAttackTime) {
        elapsedAtKeyOff / filterAttackTime => filterReleaseStart;
    } else if (filterDecayTime > 0::second && elapsedAtKeyOff < filterFullSustainStart) {
        dur filterDecayElapsed;
        elapsedAtKeyOff - filterAttackTime => filterDecayElapsed;
        1.0 + (filterDecayElapsed / filterDecayTime) * (filterSustainLevel - 1.0) => filterReleaseStart;
    } else {
        filterSustainLevel => filterReleaseStart;
    }
    
    time keyOffTime;
    startTime + sustainDur => keyOffTime;
    time endTime;
    dur maxRelease;
    releaseDur => maxRelease;
    if (filterReleaseTime > maxRelease) {
        filterReleaseTime => maxRelease;
    }
    if (pitchReleaseTime > maxRelease) {
        pitchReleaseTime => maxRelease;
    }
    keyOffTime + maxRelease => endTime;
    
    while (now < endTime) {
        // Pitch envelope calculation
        float pitchEnv;
        0.0 => pitchEnv;
        time currentTime;
        now => currentTime;
        dur elapsed;
        currentTime - startTime => elapsed;
        if (currentTime < keyOffTime) {
            if (pitchAttackTime > 0::second && elapsed < pitchAttackTime) {
                elapsed / pitchAttackTime => pitchEnv;
            } else if (pitchDecayTime > 0::second && elapsed < pitchFullSustainStart) {
                dur decayElapsed;
                elapsed - pitchAttackTime => decayElapsed;
                1.0 + (decayElapsed / pitchDecayTime) * (pitchSustainLevel - 1.0) => pitchEnv;
            } else {
                pitchSustainLevel => pitchEnv;
            }
        } else {
            dur releaseElapsed;
            currentTime - keyOffTime => releaseElapsed;
            if (pitchReleaseTime > 0::second) {
                pitchReleaseStart * (1.0 - (releaseElapsed / pitchReleaseTime)) => pitchEnv;
            } else {
                pitchReleaseStart => pitchEnv;
            }
        }
        if (pitchEnv > 1.0) 1.0 => pitchEnv; else if (pitchEnv < 0.0) 0.0 => pitchEnv;
        float current_offset;
        pitch_offset_start => current_offset;
        if (currentTime >= keyOffTime) {
            pitch_offset_end => current_offset;
        }
        float pitch_mod;
        current_offset * (1.0 - pitchEnv) => pitch_mod;
        
        // Filter envelope calculation
        float filterEnv;
        0.0 => filterEnv;
        if (currentTime < keyOffTime) {
            if (filterAttackTime > 0::second && elapsed < filterAttackTime) {
                elapsed / filterAttackTime => filterEnv;
            } else if (filterDecayTime > 0::second && elapsed < filterFullSustainStart) {
                dur filterDecayElapsed;
                elapsed - filterAttackTime => filterDecayElapsed;
                1.0 + (filterDecayElapsed / filterDecayTime) * (filterSustainLevel - 1.0) => filterEnv;
            } else {
                filterSustainLevel => filterEnv;
            }
        } else {
            dur filterReleaseElapsed;
            currentTime - keyOffTime => filterReleaseElapsed;
            if (filterReleaseTime > 0::second) {
                filterReleaseStart * (1.0 - (filterReleaseElapsed / filterReleaseTime)) => filterEnv;
            } else {
                filterReleaseStart => filterEnv;
            }
        }
        if (filterEnv > 1.0) 1.0 => filterEnv; else if (filterEnv < 0.0) 0.0 => filterEnv;
        
        // Apply filter modulation (but amount=0 removes audibility)
        float currentFilterFreq;
        filterBaseFreq + filterAmount * filterEnv => currentFilterFreq;
        if (currentFilterFreq < 20.0) 20.0 => currentFilterFreq;
        currentFilterFreq => lpf.freq;
        
        // Pitch modulation (repurposed to wavetable pos; mod only LFO/noise)
        float mod;
        lfo.last() * lfoDepth => mod;
        if (useNoise) {
            mod + op.last() * pnoiseDEPTH => mod;
        }
        Std.mtof(baseNote + pitch_mod) => float modFund;
        
        if (synthMODE == "WAVETABLE") {
            (osc $ WavetableOsc).freq(modFund);
            
            // Wavetable position modulation (using envs + repurposed mod)
            float pos_x;
            filterEnv * (num_x - 1) + mod => pos_x;  // filterEnv + LFO/noise mod
            if (pos_x < 0) 0 => pos_x;
            if (pos_x > num_x - 1) num_x - 1 => pos_x;
            float pos_y;
            pitchEnv * (num_y - 1) + mod => pos_y;  // pitchEnv drives y, added mod for movement on both axes
            if (pos_y < 0) 0 => pos_y;
            if (pos_y > num_y - 1) num_y - 1 => pos_y;
            (osc $ WavetableOsc).setPos(pos_x, pos_y);
        } else if (synthMODE == "CLASSIC") {
            if (classicWAVETYPE != "NSE") {
                if (classicWAVETYPE == "SIN") {
                    (osc $ SinOsc).freq(modFund);
                } else if (classicWAVETYPE == "TRI") {
                    (osc $ TriOsc).freq(modFund);
                } else if (classicWAVETYPE == "SAW") {
                    (osc $ SawOsc).freq(modFund);
                } else if (classicWAVETYPE == "PLS") {
                    (osc $ PulseOsc).freq(modFund);
                }
            }
            if (classicWAVETYPE == "PLS") {
                float currentWidth;
                pulseWidth + mod * pwmDepth => currentWidth;
                if (currentWidth < 0.0) 0.0 => currentWidth;
                if (currentWidth > 1.0) 1.0 => currentWidth;
                (osc $ PulseOsc).width(currentWidth);
            }
        } else if (synthMODE == "PHYSMOD") {
            if (physModel != "SHKR") {
                (osc $ StkInstrument).freq(modFund);
            }
        }

        50::samp => now;
    }
}

fun void playNote(float freq, int midi) {
    ADSR noteEnv => dac;
    noteEnv.set(noteATTACK, noteDECAY, noteSUSTAIN, noteRELEASE);
    LPF lpf => noteEnv;
    filterResonance => lpf.Q;
    UGen osc;
    string currentPhysModel;
    if (synthMODE == "PHYSMOD") {
        int x;
        Math.floor(phys_pos_x) $ int => x;
        if (x < 0) 0 => x;
        if (x >= num_x) num_x - 1 => x;
        int y;
        Math.floor(phys_pos_y) $ int => y;
        if (y < 0) 0 => y;
        if (y >= num_y) num_y - 1 => y;
        physModels[x][y] => currentPhysModel;
    }
    if (synthMODE == "WAVETABLE") {
        WavetableOsc wto => lpf;
        wto @=> osc;
        wto.freq(freq);
        wto.gain(gainScale);
    } else if (synthMODE == "CLASSIC") {
        if (classicWAVETYPE == "SIN") {
            SinOsc so => lpf;
            so @=> osc;
            so.freq(freq);
            so.gain(gainScale);
        } else if (classicWAVETYPE == "TRI") {
            TriOsc to => lpf;
            to @=> osc;
            to.freq(freq);
            to.gain(gainScale);
        } else if (classicWAVETYPE == "SAW") {
            SawOsc so => lpf;
            so @=> osc;
            so.freq(freq);
            so.gain(gainScale);
        } else if (classicWAVETYPE == "PLS") {
            PulseOsc po => lpf;
            po @=> osc;
            po.freq(freq);
            po.gain(gainScale);
            pulseWidth => po.width;
        } else if (classicWAVETYPE == "NSE") {
            Noise no => lpf;
            no @=> osc;
            no.gain(gainScale);
        }
    } else if (synthMODE == "PHYSMOD") {
        if (currentPhysModel == "BAND") {
            float attackSeconds;
            noteATTACK / 1::second => attackSeconds;
            float localBandedBowRate;
            1.0 / (attackSeconds + 0.001) => localBandedBowRate;
            if (localBandedBowRate > 1.0) {
                1.0 => localBandedBowRate;
            }
            int localBandedPreset;
            Math.random2(0, 3) => localBandedPreset;
            float localBandedBowPressure;
            Math.random2f(0.0, 1.0) => localBandedBowPressure;
            float localBandedBowMotion;
            Math.random2f(0.0, 1.0) => localBandedBowMotion;
            float localBandedVibratoFreq;
            Math.random2f(0.0, 12.0) => localBandedVibratoFreq;
            float localBandedIntegration;
            Math.random2f(0.0, 1.0) => localBandedIntegration;
            float localBandedModesGain;
            Math.random2f(0.0, 1.0) => localBandedModesGain;
            float localBandedStrikePosition;
            Math.random2f(0.0, 1.0) => localBandedStrikePosition;
            BandedWG bwg => lpf;
            bwg.preset(localBandedPreset);
            bwg.bowPressure(localBandedBowPressure);
            bwg.bowMotion(localBandedBowMotion);
            bwg.bowRate(localBandedBowRate);
            bwg.controlChange(11, localBandedVibratoFreq);
            bwg.integrationConstant(localBandedIntegration);
            bwg.modesGain(localBandedModesGain);
            bwg.strikePosition(localBandedStrikePosition);
            bwg.freq(freq);
            bwg.gain(gainScale);
            bwg @=> osc;
        } else if (currentPhysModel == "BOTL") {
            float attackSeconds;
            noteATTACK / 1::second => attackSeconds;
            float localBlowBotlRate;
            1.0 / (attackSeconds + 0.001) => localBlowBotlRate;
            if (localBlowBotlRate > 1.0) {
                1.0 => localBlowBotlRate;
            }
            float localBlowBotlNoiseGain;
            Math.random2f(0.0, 0.2) => localBlowBotlNoiseGain;
            float localBlowBotlVibratoFreq;
            Math.random2f(0.0, 12.0) => localBlowBotlVibratoFreq;
            float localBlowBotlVibratoGain;
            Math.random2f(0.0, 0.1) => localBlowBotlVibratoGain;
            float localBlowBotlVolume;
            Math.random2f(0.5, 1.0) => localBlowBotlVolume;
            BlowBotl bb => lpf;
            bb.noiseGain(localBlowBotlNoiseGain);
            bb.vibratoFreq(localBlowBotlVibratoFreq);
            bb.vibratoGain(localBlowBotlVibratoGain);
            bb.volume(localBlowBotlVolume);
            bb.rate(localBlowBotlRate);
            bb.freq(freq);
            bb.gain(gainScale);
            bb @=> osc;
        } else if (currentPhysModel == "BHOL") {
            float localBlowHoleReed;
            Math.random2f(0.0, 1.0) => localBlowHoleReed;
            float localBlowHoleNoiseGain;
            Math.random2f(0.0, 0.2) => localBlowHoleNoiseGain;
            float localBlowHoleTonehole;
            Math.random2f(0.0, 1.0) => localBlowHoleTonehole;
            float localBlowHoleVent;
            Math.random2f(0.0, 1.0) => localBlowHoleVent;
            float localBlowHoleRegister;
            Math.random2f(0.0, 1.0) => localBlowHoleRegister;
            float localBlowHolePressure;
            Math.random2f(0.5, 1.0) => localBlowHolePressure;
            BlowHole bh => lpf;
            bh.reed(localBlowHoleReed);
            bh.noiseGain(localBlowHoleNoiseGain);
            bh.tonehole(localBlowHoleTonehole);
            bh.vent(localBlowHoleVent);
            bh.controlChange(1, localBlowHoleRegister * 128.0);
            bh.pressure(localBlowHolePressure);
            bh.freq(freq);
            bh.gain(gainScale);
            bh @=> osc;
        } else if (currentPhysModel == "BOWD") {
            float localBowedBowPressure;
            Math.random2f(0.0, 1.0) => localBowedBowPressure;
            float localBowedBowPosition;
            Math.random2f(0.0, 1.0) => localBowedBowPosition;
            float localBowedVibratoFreq;
            Math.random2f(0.0, 12.0) => localBowedVibratoFreq;
            float localBowedVibratoGain;
            Math.random2f(0.0, 0.1) => localBowedVibratoGain;
            float localBowedVolume;
            Math.random2f(0.5, 1.0) => localBowedVolume;
            Bowed bw => lpf;
            bw.bowPressure(localBowedBowPressure);
            bw.bowPosition(localBowedBowPosition);
            bw.vibratoFreq(localBowedVibratoFreq);
            bw.vibratoGain(localBowedVibratoGain);
            bw.volume(localBowedVolume);
            bw.freq(freq);
            bw.gain(gainScale);
            bw @=> osc;
        } else if (currentPhysModel == "BRAS") {
            float localBrassLip;
            Math.random2f(0.0, 1.0) => localBrassLip;
            float localBrassSlide;
            Math.random2f(0.0, 1.0) => localBrassSlide;
            float localBrassVibratoFreq;
            Math.random2f(0.0, 12.0) => localBrassVibratoFreq;
            float localBrassVibratoGain;
            Math.random2f(0.0, 0.1) => localBrassVibratoGain;
            float localBrassVolume;
            Math.random2f(0.5, 1.0) => localBrassVolume;
            Brass br => lpf;
            br.lip(localBrassLip);
            br.slide(localBrassSlide);
            br.vibratoFreq(localBrassVibratoFreq);
            br.vibratoGain(localBrassVibratoGain);
            br.volume(localBrassVolume);
            br.freq(freq);
            br.gain(gainScale);
            br @=> osc;
        } else if (currentPhysModel == "CLAR") {
            float localClarinetReed;
            Math.random2f(0.0, 1.0) => localClarinetReed;
            float localClarinetNoiseGain;
            Math.random2f(0.0, 0.2) => localClarinetNoiseGain;
            float localClarinetVibratoFreq;
            Math.random2f(0.0, 12.0) => localClarinetVibratoFreq;
            float localClarinetVibratoGain;
            Math.random2f(0.0, 0.1) => localClarinetVibratoGain;
            float localClarinetPressure;
            Math.random2f(0.5, 1.0) => localClarinetPressure;
            Clarinet cl => lpf;
            cl.reed(localClarinetReed);
            cl.noiseGain(localClarinetNoiseGain);
            cl.vibratoFreq(localClarinetVibratoFreq);
            cl.vibratoGain(localClarinetVibratoGain);
            cl.pressure(localClarinetPressure);
            cl.freq(freq);
            cl.gain(gainScale);
            cl @=> osc;
        } else if (currentPhysModel == "FLUT") {
            float localFluteJetDelay;
            Math.random2f(0.0, 1.0) => localFluteJetDelay;
            float localFluteJetReflection;
            Math.random2f(0.0, 1.0) => localFluteJetReflection;
            float localFluteEndReflection;
            Math.random2f(0.0, 1.0) => localFluteEndReflection;
            float localFluteNoiseGain;
            Math.random2f(0.0, 0.2) => localFluteNoiseGain;
            float localFluteVibratoFreq;
            Math.random2f(0.0, 12.0) => localFluteVibratoFreq;
            float localFluteVibratoGain;
            Math.random2f(0.0, 0.1) => localFluteVibratoGain;
            float localFlutePressure;
            Math.random2f(0.5, 1.0) => localFlutePressure;
            Flute fl => lpf;
            fl.jetDelay(localFluteJetDelay);
            fl.jetReflection(localFluteJetReflection);
            fl.endReflection(localFluteEndReflection);
            fl.noiseGain(localFluteNoiseGain);
            fl.vibratoFreq(localFluteVibratoFreq);
            fl.vibratoGain(localFluteVibratoGain);
            fl.pressure(localFlutePressure);
            fl.freq(freq);
            fl.gain(gainScale);
            fl @=> osc;
        } else if (currentPhysModel == "MAND") {
            float localMandolinBodySize;
            Math.random2f(0.5, 1.0) => localMandolinBodySize;
            float localMandolinPluckPos;
            Math.random2f(0.0, 1.0) => localMandolinPluckPos;
            float localMandolinStringDamping;
            Math.random2f(0.0, 1.0) => localMandolinStringDamping;
            float localMandolinStringDetune;
            Math.random2f(0.0, 0.1) => localMandolinStringDetune;
            Mandolin md => lpf;
            md.bodySize(localMandolinBodySize);
            md.pluckPos(localMandolinPluckPos);
            md.stringDamping(localMandolinStringDamping);
            md.stringDetune(localMandolinStringDetune);
            md.freq(freq);
            md.gain(gainScale);
            md @=> osc;
        } else if (currentPhysModel == "MODA") {
            int localModalBarPreset;
            Math.random2(0, 8) => localModalBarPreset;
            float localModalBarStickHardness;
            Math.random2f(0.0, 1.0) => localModalBarStickHardness;
            float localModalBarStrikePosition;
            Math.random2f(0.0, 1.0) => localModalBarStrikePosition;
            float localModalBarVibratoFreq;
            Math.random2f(0.0, 12.0) => localModalBarVibratoFreq;
            float localModalBarVibratoGain;
            Math.random2f(0.0, 0.1) => localModalBarVibratoGain;
            float localModalBarDirectGain;
            Math.random2f(0.0, 1.0) => localModalBarDirectGain;
            float localModalBarMasterGain;
            Math.random2f(0.5, 1.0) => localModalBarMasterGain;
            float localModalBarVolume;
            Math.random2f(0.5, 1.0) => localModalBarVolume;
            ModalBar mb => lpf;
            mb.preset(localModalBarPreset);
            mb.stickHardness(localModalBarStickHardness);
            mb.strikePosition(localModalBarStrikePosition);
            mb.vibratoFreq(localModalBarVibratoFreq);
            mb.vibratoGain(localModalBarVibratoGain);
            mb.directGain(localModalBarDirectGain);
            mb.masterGain(localModalBarMasterGain);
            mb.volume(localModalBarVolume);
            mb.freq(freq);
            mb.gain(gainScale);
            mb @=> osc;
        } else if (currentPhysModel == "SAXO") {
            float localSaxofonyStiffness;
            Math.random2f(0.0, 1.0) => localSaxofonyStiffness;
            float localSaxofonyAperture;
            Math.random2f(0.0, 1.0) => localSaxofonyAperture;
            float localSaxofonyNoiseGain;
            Math.random2f(0.0, 0.2) => localSaxofonyNoiseGain;
            float localSaxofonyVibratoFreq;
            Math.random2f(0.0, 12.0) => localSaxofonyVibratoFreq;
            float localSaxofonyVibratoGain;
            Math.random2f(0.0, 0.1) => localSaxofonyVibratoGain;
            float localSaxofonyBlowPosition;
            Math.random2f(0.0, 1.0) => localSaxofonyBlowPosition;
            float localSaxofonyPressure;
            Math.random2f(0.5, 1.0) => localSaxofonyPressure;
            Saxofony sx => lpf;
            sx.stiffness(localSaxofonyStiffness);
            sx.aperture(localSaxofonyAperture);
            sx.noiseGain(localSaxofonyNoiseGain);
            sx.vibratoFreq(localSaxofonyVibratoFreq);
            sx.vibratoGain(localSaxofonyVibratoGain);
            sx.blowPosition(localSaxofonyBlowPosition);
            sx.pressure(localSaxofonyPressure);
            sx.freq(freq);
            sx.gain(gainScale);
            sx @=> osc;
        } else if (currentPhysModel == "SHKR") {
            int localShakersPreset;
            Math.random2(0, 22) => localShakersPreset;
            float localShakersEnergy;
            Math.random2f(0.5, 1.0) => localShakersEnergy;
            float localShakersDecay;
            Math.random2f(0.9, 0.99) => localShakersDecay;
            Shakers sh => lpf;
            sh.preset(localShakersPreset);
            sh.energy(localShakersEnergy);
            sh.decay(localShakersDecay);
            sh.gain(gainScale);
            sh @=> osc;
        } else if (currentPhysModel == "SITR") {
            Sitar st => lpf;
            st.freq(freq);
            st.gain(gainScale);
            st @=> osc;
        } else if (currentPhysModel == "STIF") {
            float localStifKarpPickupPosition;
            Math.random2f(0.0, 1.0) => localStifKarpPickupPosition;
            float localStifKarpSustain;
            Math.random2f(0.0, 1.0) => localStifKarpSustain;
            float localStifKarpStretch;
            Math.random2f(0.0, 1.0) => localStifKarpStretch;
            float localStifKarpBaseLoopGain;
            Math.random2f(0.0, 1.0) => localStifKarpBaseLoopGain;
            StifKarp sk => lpf;
            sk.pickupPosition(localStifKarpPickupPosition);
            sk.sustain(localStifKarpSustain);
            sk.stretch(localStifKarpStretch);
            sk.baseLoopGain(localStifKarpBaseLoopGain);
            sk.freq(freq);
            sk.gain(gainScale);
            sk @=> osc;
        } else if (currentPhysModel == "VOIC") {
            string phonemes[11];
            "eee" => phonemes[0];
            "ihh" => phonemes[1];
            "ehh" => phonemes[2];
            "aaa" => phonemes[3];
            "ahh" => phonemes[4];
            "aww" => phonemes[5];
            "ohh" => phonemes[6];
            "uhh" => phonemes[7];
            "uuu" => phonemes[8];
            "erh" => phonemes[9];
            "sss" => phonemes[10];
            "fff" => phonemes[11];
            string localVoicFormPhoneme;
            phonemes[Math.random2(0, phonemes.cap()-1)] => localVoicFormPhoneme;
            float localVoicFormVoiced;
            Math.random2f(0.0, 1.0) => localVoicFormVoiced;
            float localVoicFormUnVoiced;
            Math.random2f(0.0, 1.0) => localVoicFormUnVoiced;
            float localVoicFormPitchSweepRate;
            Math.random2f(0.0, 1.0) => localVoicFormPitchSweepRate;
            VoicForm vf => lpf;
            vf.phoneme(localVoicFormPhoneme);
            vf.voiced(localVoicFormVoiced);
            vf.unVoiced(localVoicFormUnVoiced);
            vf.pitchSweepRate(localVoicFormPitchSweepRate);
            vf.freq(freq);
            vf.gain(gainScale);
            vf @=> osc;
        }
    }
    // Removed generative amps blend/random as wavetable grid handles complexity; structure kept minimal
    int useNoise;
    if (Math.randomf() < pnoisePROBABILITY) {
        1 => useNoise;
    } else {
        0 => useNoise;
    }
    spork ~ modulate(osc, lpf, Std.ftom(freq), shortDur, noteRELEASE, useNoise, currentPhysModel);
    noteEnv.keyOn();
    if (synthMODE == "PHYSMOD") {
        (osc $ StkInstrument).noteOn(1.0);
    }
    shortDur => now;
    noteEnv.keyOff();
    if (synthMODE == "PHYSMOD") {
        (osc $ StkInstrument).noteOff(1.0);
    }
    noteRELEASE => now;
    noteEnv =< dac;
    0 => playing[midi];
}

// MIDI listener for CC77
MidiIn min;
min.open(0);  // Assume MIDI port 0, adjust as needed

fun void midiListener() {
    MidiMsg msg;
    while (true) {
        min => now;
        while (min.recv(msg)) {
            if (msg.data1 == 176) {
                if (msg.data2 == 27) {
                    msg.data3 / 127.0 * (num_x - 1.0) => phys_pos_x;
                } else if (msg.data2 == 28) {
                    msg.data3 / 127.0 * (num_y - 1.0) => phys_pos_y;
                } else if (msg.data2 == 77) {  // Control Change on channel 1, CC77
                    // Always trigger a generative note, ignoring probability and bpm
                    float sum_prob;
                    0.0 => sum_prob;
                    for (0 => int i; i < 12; i++) {
                        sum_prob + note_probs[i] => sum_prob;
                    }
                    if (sum_prob > 0.0) {
                        Math.randomf() * sum_prob => float r;
                        float cum;
                        0.0 => cum;
                        float selectedFreq;
                        for (0 => int i; i < 12; i++) {
                            cum + note_probs[i] => cum;
                            if (r < cum) {
                                octaveINIT => octaveOFFSET_total;
                                if (Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;  // Lowered prob for low octaves
                                if (Math.randomf() < 0.05) octaveOFFSET_total - 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.75) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.5) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.33) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.2) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.15) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                if (Math.randomf() < 0.1) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                note_pitches[i] * Math.pow(2, octaveOFFSET_total / 12.0) => selectedFreq;
                                int midiNote;
                                60 + i + octaveOFFSET_total => midiNote;
                                if (playing[midiNote] == 0) {
                                    1 => playing[midiNote];
                                    spork ~ playNote(selectedFreq, midiNote);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

spork ~ midiListener();

while (true) {
    if (Math.randomf() < UI_NOTEprob) {
        float sum_prob;
        0.0 => sum_prob;
        for (0 => int i; i < 12; i++) {
            sum_prob + note_probs[i] => sum_prob;
        }
        if (sum_prob > 0.0) {
            Math.randomf() * sum_prob => float r;
            float cum;
            0.0 => cum;
            float selectedFreq;
            for (0 => int i; i < 12; i++) {
                cum + note_probs[i] => cum;
                if (r < cum) {
                    octaveINIT => octaveOFFSET_total;
                    if (Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;  // Lowered prob for low octaves
                    if (Math.randomf() < 0.05) octaveOFFSET_total - 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.75) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.5) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.33) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.2) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.15) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    if (Math.randomf() < 0.1) octaveOFFSET_total + 12 => octaveOFFSET_total;
                    note_pitches[i] * Math.pow(2, octaveOFFSET_total / 12.0) => selectedFreq;
                    int midiNote;
                    60 + i + octaveOFFSET_total => midiNote;
                    if (playing[midiNote] == 0) {
                        1 => playing[midiNote];
                        spork ~ playNote(selectedFreq, midiNote);
                    }
                    break;
                }
            }
        }
    }
    safeAdvance(beat);
}