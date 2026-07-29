// UI and timing
float UI_NOTEprob;
0.6 => UI_NOTEprob;
float bpm;
320.0 => bpm;
dur beat;
(30.0 / bpm)::second => beat;
dur shortDur;
2.4 * beat => shortDur;
// synth mode and params
string synthModes[3];
"WAVETABLE" => synthModes[0];
"CLASSIC" => synthModes[1];
"PHYSMOD" => synthModes[2];
int currentSynthModeIndex;
2 => currentSynthModeIndex;
string synthMODE;
synthModes[currentSynthModeIndex] => synthMODE;
string classicShapes[5];
"SIN" => classicShapes[0];
"TRI" => classicShapes[1];
"SAW" => classicShapes[2];
"PLS" => classicShapes[3];
"NSE" => classicShapes[4];
int currentClassicShapeIndex;
3 => currentClassicShapeIndex;
string classicWAVETYPE;
classicShapes[currentClassicShapeIndex] => classicWAVETYPE;
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
200::ms => filterATTACK;
dur filterDECAY;
100::ms => filterDECAY;
float filterSUSTAIN;
0 => filterSUSTAIN;
dur filterRELEASE;
0::ms => filterRELEASE;
// LFO
float lfoRate;
0.1 => lfoRate;
float lfoDepth;
0.5 => lfoDepth;
// noise mod
float pnoisePROBABILITY;
0.0 => pnoisePROBABILITY;
float pnoiseSMOOTHING;
0.99 => pnoiseSMOOTHING;
float pnoiseDEPTH;
5.0 => pnoiseDEPTH;
// gain/oct
float gainScale;
0.1 => gainScale;
int octaveINIT;
-12 => octaveINIT;
// timbre
float generativeTIMBRE_scaler;
0 => generativeTIMBRE_scaler;
// wavetable params
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
float NOTE1prob; // C
1.0 => NOTE1prob;
float NOTE2prob; // C#
0.0 => NOTE2prob;
float NOTE3prob; // D
1.0 => NOTE3prob;
float NOTE4prob; // D#
0.0 => NOTE4prob;
float NOTE5prob; // E
1.0 => NOTE5prob;
float NOTE6prob; // F
1.0 => NOTE6prob;
float NOTE7prob; // F#
0.0 => NOTE7prob;
float NOTE8prob; // G
1.0 => NOTE8prob;
float NOTE9prob; // G#
0.0 => NOTE9prob;
float NOTE10prob; // A
1.0 => NOTE10prob;
float NOTE11prob; // A#
0.0 => NOTE11prob;
float NOTE12prob; // B
1.0 => NOTE12prob;
// base freak
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
// playing notes by MIDI note number
int playing[200];
// wavetableOsc class
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
// generate harmonic table
fun float[] genHarmonicTable(float x_frac, float y_frac) {
    float table[table_size];
    float max_val;
    0.0 => max_val;
    float power;
    0.5 + x_frac * 1.5 => power;
    for (0 => int s; s < table_size; s++) {
        0.0 => table[s];
        for (1 => int h; h <= num_harmonics; h++) {
            float amp;
            1.0 / Math.pow(h, power) => amp;
            float sign;
            1.0 => sign;
            if (Math.randomf() < y_frac) {
                -1.0 => sign;
            } else if (y_frac < 0.3 && ((h - 1)/2 % 2) == 1) {
                -1.0 => sign;
            }
            if (y_frac < 0.5 && h % 2 == 0) {
                0.0 => amp;
            }
            table[s] + Math.sin(2 * Math.PI * h * s / table_size) * amp * sign => table[s];
            if (Math.fabs(table[s]) > max_val) {
                Math.fabs(table[s]) => max_val;
            }
        }
    }
    if (max_val > 0.0) {
        for (0 => int s; s < table_size; s++) {
            table[s] / max_val => table[s];
        }
    }
    return table;
}
// all wavetables gen
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
    // calculate pitch release start level
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
    // calculate filter release start level
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
        // pitch envelope calculation
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
     
        // filter envelope calculation
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
     
        // apply filter modulation (but amount=0 removes audibility)
        float currentFilterFreq;
        filterBaseFreq + filterAmount * filterEnv => currentFilterFreq;
        if (currentFilterFreq < 20.0) 20.0 => currentFilterFreq;
        currentFilterFreq => lpf.freq;
     
        // pitch modulation (repurposed to wavetable pos; mod only LFO/noise)
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
            filterEnv * (num_x - 1) + mod => pos_x; // filterEnv + LFO/noise mod
            if (pos_x < 0) 0 => pos_x;
            if (pos_x > num_x - 1) num_x - 1 => pos_x;
            float pos_y;
            pitchEnv * (num_y - 1) + mod => pos_y; // pitchEnv drives y, added mod for movement on both axes
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
    ADSR noteEnv;
    noteEnv.set(noteATTACK, noteDECAY, noteSUSTAIN, noteRELEASE);
    LPF lpf => noteEnv => volGainL;
    noteEnv => volGainR;
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
            1.0 => localBlowBotlVolume;
            BlowBotl bb => lpf;
            bb.noiseGain(localBlowBotlNoiseGain);
            bb.vibratoFreq(localBlowBotlVibratoFreq);
            bb.vibratoGain(localBlowBotlVibratoGain);
            bb.volume(localBlowBotlVolume);
            bb.rate(localBlowBotlRate);
            bb.freq(freq);
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
            1.0 => localBlowHolePressure;
            BlowHole bh => lpf;
            bh.reed(localBlowHoleReed);
            bh.noiseGain(localBlowHoleNoiseGain);
            bh.tonehole(localBlowHoleTonehole);
            bh.vent(localBlowHoleVent);
            bh.controlChange(1, localBlowHoleRegister * 128.0);
            bh.pressure(localBlowHolePressure);
            bh.freq(freq);
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
            1.0 => localBowedVolume;
            Bowed bw => lpf;
            bw.bowPressure(localBowedBowPressure);
            bw.bowPosition(localBowedBowPosition);
            bw.vibratoFreq(localBowedVibratoFreq);
            bw.vibratoGain(localBowedVibratoGain);
            bw.volume(localBowedVolume);
            bw.freq(freq);
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
            1.0 => localBrassVolume;
            Brass br => lpf;
            br.lip(localBrassLip);
            br.slide(localBrassSlide);
            br.vibratoFreq(localBrassVibratoFreq);
            br.vibratoGain(localBrassVibratoGain);
            br.volume(localBrassVolume);
            br.freq(freq);
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
            1.0 => localClarinetPressure;
            Clarinet cl => lpf;
            cl.reed(localClarinetReed);
            cl.noiseGain(localClarinetNoiseGain);
            cl.vibratoFreq(localClarinetVibratoFreq);
            cl.vibratoGain(localClarinetVibratoGain);
            cl.pressure(localClarinetPressure);
            cl.freq(freq);
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
            1.0 => localFlutePressure;
            Flute fl => lpf;
            fl.jetDelay(localFluteJetDelay);
            fl.jetReflection(localFluteJetReflection);
            fl.endReflection(localFluteEndReflection);
            fl.noiseGain(localFluteNoiseGain);
            fl.vibratoFreq(localFluteVibratoFreq);
            fl.vibratoGain(localFluteVibratoGain);
            fl.pressure(localFlutePressure);
            fl.freq(freq);
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
            1.0 => localModalBarMasterGain;
            float localModalBarVolume;
            1.0 => localModalBarVolume;
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
            1.0 => localSaxofonyPressure;
            Saxofony sx => lpf;
            sx.stiffness(localSaxofonyStiffness);
            sx.aperture(localSaxofonyAperture);
            sx.noiseGain(localSaxofonyNoiseGain);
            sx.vibratoFreq(localSaxofonyVibratoFreq);
            sx.vibratoGain(localSaxofonyVibratoGain);
            sx.blowPosition(localSaxofonyBlowPosition);
            sx.pressure(localSaxofonyPressure);
            sx.freq(freq);
            sx @=> osc;
        } else if (currentPhysModel == "SHKR") {
            int localShakersPreset;
            Math.random2(0, 22) => localShakersPreset;
            float localShakersEnergy;
            1.0 => localShakersEnergy;
            float localShakersDecay;
            0.95 => localShakersDecay;
            Shakers sh => lpf;
            sh.preset(localShakersPreset);
            sh.energy(localShakersEnergy);
            sh.decay(localShakersDecay);
            sh @=> osc;
        } else if (currentPhysModel == "SITR") {
            Sitar st => lpf;
            st.freq(freq);
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
            sk @=> osc;
        } else if (currentPhysModel == "VOIC") {
            string phonemes[12];
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
            vf @=> osc;
        }
        float volFactor;
        4 => volFactor;
        osc.gain(gainScale * volFactor);
    }
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
    lpf =< noteEnv;
    noteEnv =< volGainL;
    noteEnv =< volGainR;
    0 => playing[midi];
}
fun void sequencer() {
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
                        if (Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;
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
        (30.0 / bpm)::second => beat;
        2.4 * beat => shortDur;
        safeAdvance(beat);
    }
}
// input and signal chain init
adc.chan(0) => Gain volGainL;
adc.chan(1) => Gain volGainR;
Gain directVibL;
volGainL => directVibL;
Gain directVibR;
volGainR => directVibR;
Gain vibMixerL;
directVibL => vibMixerL;
Gain vibMixerR;
directVibR => vibMixerR;
Gain directChorusL;
vibMixerL => directChorusL;
Gain directChorusR;
vibMixerR => directChorusR;
Gain chorusMixerL;
directChorusL => chorusMixerL;
Gain chorusMixerR;
directChorusR => chorusMixerR;
Gain directPitchL;
chorusMixerL => directPitchL;
Gain directPitchR;
chorusMixerR => directPitchR;
Gain pitchMixerL;
directPitchL => pitchMixerL;
Gain pitchMixerR;
directPitchR => pitchMixerR;
Gain ringGainL;
pitchMixerL => ringGainL;
Gain ringGainR;
pitchMixerR => ringGainR;
DelayL vibDelayL;
30::ms => vibDelayL.max;
12::ms => vibDelayL.delay;
Gain vibFbL;
vibDelayL => vibFbL => vibDelayL;
DelayL vibDelayR;
30::ms => vibDelayR.max;
12::ms => vibDelayR.delay;
Gain vibFbR;
vibDelayR => vibFbR => vibDelayR;
Gain vibGainL;
Gain vibGainR;
DelayL chorusDelayL;
30::ms => chorusDelayL.max;
5::ms => chorusDelayL.delay;
Gain chorusFbL;
chorusDelayL => chorusFbL => chorusDelayL;
DelayL chorusDelayR;
30::ms => chorusDelayR.max;
5::ms => chorusDelayR.delay;
Gain chorusFbR;
chorusDelayR => chorusFbR => chorusDelayR;
Gain chorusGainL;
Gain chorusGainR;
PitShift pitchShiftL;
PitShift pitchShiftR;
Gain pitchWetL;
Gain pitchWetR;
SinOsc ringMod => blackhole;
Gain directDelayL;
Gain directDelayR;
Gain delayMixerL;
Gain delayMixerR;
ringGainL => directDelayL => delayMixerL;
ringGainR => directDelayR => delayMixerR;
DelayA delayDelayL;
1.5::second => delayDelayL.max;
0.5::second => delayDelayL.delay;
PitShift delPitchShiftL;
delPitchShiftL.mix(1.0);
Gain delayFbL;
delayDelayL => delPitchShiftL => delayFbL => delayDelayL;
Gain delayGainL;
delPitchShiftL => delayGainL => delayMixerL;
DelayA delayDelayR;
1.5::second => delayDelayR.max;
0.5::second => delayDelayR.delay;
PitShift delPitchShiftR;
delPitchShiftR.mix(1.0);
Gain delayFbR;
delayDelayR => delPitchShiftR => delayFbR => delayDelayR;
Gain delayGainR;
delPitchShiftR => delayGainR => delayMixerR;
Gain directReverbL;
Gain directReverbR;
Gain reverbMixerL;
Gain reverbMixerR;
delayMixerL => directReverbL => reverbMixerL => dac.chan(0);
delayMixerR => directReverbR => reverbMixerR => dac.chan(1);
NRev revL;
revL.mix(1.0);
NRev revR;
revR.mix(1.0);
Gain reverbGainL;
Gain reverbGainR;
// globals2
global float gkVOLpedal;
global float gkVOLpedalBypass;
global int gkLastToggle;
global float gkPortTime;
1.0 => gkVOLpedal;
1.0 => gkVOLpedalBypass;
1 => gkLastToggle;
0.03 => gkPortTime;
global float prevVOLpedal;
global float prevVOLpedalBypass;
-1.0 => prevVOLpedal;
-1.0 => prevVOLpedalBypass;
global float gkVibBypass;
1.0 => gkVibBypass;
global float prevVibBypass;
-1.0 => prevVibBypass;
global float gkVibRate;
global float gkVibAmount;
0.06 => gkVibRate;
0.18 => gkVibAmount;
global float prevVibRate;
global float prevVibAmount;
-1.0 => prevVibRate;
-1.0 => prevVibAmount;
global float gkVibFeedback;
0.0 => gkVibFeedback;
global float prevVibFeedback;
-1.0 => prevVibFeedback;
global float gkChorusBypass;
1.0 => gkChorusBypass;
global float prevChorusBypass;
-1.0 => prevChorusBypass;
global float gkChorusRate;
global float gkChorusAmount;
0.06 => gkChorusRate;
0.5 => gkChorusAmount;
global float prevChorusRate;
global float prevChorusAmount;
-1.0 => prevChorusRate;
-1.0 => prevChorusAmount;
global float gkChorusFeedback;
0.0 => gkChorusFeedback;
global float prevChorusFeedback;
-1.0 => prevChorusFeedback;
global float gkPitchBypass;
1.0 => gkPitchBypass;
global float prevPitchBypass;
-1.0 => prevPitchBypass;
global float gkPitchMix;
0.5 => gkPitchMix;
global float prevPitchMix;
-1.0 => prevPitchMix;
global float gkPitchShift;
0.0 => gkPitchShift;
global float prevPitchShift;
-1.0 => prevPitchShift;
global float gkRingBypass;
1.0 => gkRingBypass;
global float prevRingBypass;
-1.0 => prevRingBypass;
global float gkRingRate;
0.5 => gkRingRate;
global float prevRingRate;
-1.0 => prevRingRate;
global OscOut xout;
xout.dest("127.0.0.1", 8000);
global float gkVolOnSmooth;
0.0 => gkVolOnSmooth;
global float gkVibOnSmooth;
0.0 => gkVibOnSmooth;
global float gkChorusOnSmooth;
0.0 => gkChorusOnSmooth;
global float gkPitchOnSmooth;
0.0 => gkPitchOnSmooth;
global float gkRingOnSmooth;
0.0 => gkRingOnSmooth;
float prevUI_NOTEprob;
-1.0 => prevUI_NOTEprob;
float prevBpm;
-1.0 => prevBpm;
global int gkGridLock;
0 => gkGridLock;
int currentMenu;
0 => currentMenu;
global float gkDelayBypass;
1.0 => gkDelayBypass;
global float prevDelayBypass;
-1.0 => prevDelayBypass;
global float gkDelayMix;
0.5 => gkDelayMix;
global float prevDelayMix;
-1.0 => prevDelayMix;
global float gkDelayTime;
0.5 => gkDelayTime;
global float prevDelayTime;
-1.0 => prevDelayTime;
global float gkDelayFeedback;
0.5 => gkDelayFeedback;
global float prevDelayFeedback;
-1.0 => prevDelayFeedback;
global float gkDelayModRate;
0.06 => gkDelayModRate;
global float prevDelayModRate;
-1.0 => prevDelayModRate;
global float gkDelayModDepth;
0.5 => gkDelayModDepth;
global float prevDelayModDepth;
-1.0 => prevDelayModDepth;
global float gkDelayPitch;
0.0 => gkDelayPitch;
global float prevDelayPitch;
-1.0 => prevDelayPitch;
global float gkDelayOnSmooth;
0.0 => gkDelayOnSmooth;
global float gkReverbBypass;
1.0 => gkReverbBypass;
global float prevReverbBypass;
-1.0 => prevReverbBypass;
global float gkReverbMix;
0.5 => gkReverbMix;
global float prevReverbMix;
-1.0 => prevReverbMix;
global float gkReverbTime;
0.5 => gkReverbTime;
global float prevReverbTime;
-1.0 => prevReverbTime;
global float gkReverbOnSmooth;
0.0 => gkReverbOnSmooth;
// MIDI setup
MidiIn mins[0];
MidiMsg msg;
0 => int devCount;
MidiIn test;
while( test.open(devCount) )
{
    devCount++;
}
for(0 => int i; i < devCount; i++)
{
    MidiIn min;
    if( min.open(i) )
    {
        mins << min;
    }
}
if( mins.cap() == 0 )
{
    me.exit();
}
fun void midi_listener()
{
    while( true )
    {
        int had_msg;
        0 => had_msg;
        for( 0 => int i; i < mins.cap(); i++ )
        {
            while( mins[i].recv( msg ) )
            {
                1 => had_msg;
                msg.data1 & 0xF0 => int status;
                (msg.data1 & 0x0F) + 1 => int channel;
                if( status == 0x90 )
                {
                    if( msg.data3 > 0 )
                    {
                    }
                    else
                    {
                    }
                }
                else if( status == 0x80 )
                {
                }
                else if( status == 0xB0 )
                {
                    int cc;
                    msg.data2 => cc;
                    int val;
                    msg.data3 => val;
                    if( cc == 70 && val == 127 )
                    {
                        (currentMenu - 1 + 5) % 5 => currentMenu;
                        <<< "Menu -", currentMenu, "-", currentMenu, "-", 70 >>>;
                    }
                    else if( cc == 73 && val == 127 )
                    {
                        (currentMenu + 1) % 5 => currentMenu;
                        <<< "Menu -", currentMenu, "-", currentMenu, "-", 73 >>>;
                    }
                    else if( cc == 74 && val == 127 && currentMenu == 1 )
                    {
                        1.0 - gkVOLpedalBypass => gkVOLpedalBypass;
                        if( gkVOLpedalBypass < 0.5 )
                        {
                            8 => gkLastToggle;
                        }
                        if( gkVOLpedalBypass != prevVOLpedalBypass )
                        {
                            xout.start("/volpedal_state");
                            xout.add(gkVOLpedalBypass);
                            xout.send();
                            xout.start("/volpedal_state");
                            xout.add(gkVOLpedalBypass);
                            xout.send();
                            xout.start("/volpedal_state");
                            xout.add(gkVOLpedalBypass);
                            xout.send();
                            gkVOLpedalBypass => prevVOLpedalBypass;
                            <<< "volpedal_bypass -", gkVOLpedalBypass, "-", currentMenu, "-", 74 >>>;
                        }
                    }
                    else if( cc == 74 && val == 127 && currentMenu == 2 )
                    {
                        1.0 - gkVibBypass => gkVibBypass;
                        if( gkVibBypass < 0.5 )
                        {
                            9 => gkLastToggle;
                        }
                        if( gkVibBypass != prevVibBypass )
                        {
                            xout.start("/vib_state");
                            xout.add(gkVibBypass);
                            xout.send();
                            xout.start("/vib_state");
                            xout.add(gkVibBypass);
                            xout.send();
                            xout.start("/vib_state");
                            xout.add(gkVibBypass);
                            xout.send();
                            gkVibBypass => prevVibBypass;
                            <<< "vib_bypass -", gkVibBypass, "-", currentMenu, "-", 74 >>>;
                        }
                    }
                    else if( cc == 75 && val == 127 && currentMenu == 2 )
                    {
                        1.0 - gkChorusBypass => gkChorusBypass;
                        if( gkChorusBypass < 0.5 )
                        {
                            12 => gkLastToggle;
                        }
                        if( gkChorusBypass != prevChorusBypass )
                        {
                            xout.start("/chorus_state");
                            xout.add(gkChorusBypass);
                            xout.send();
                            xout.start("/chorus_state");
                            xout.add(gkChorusBypass);
                            xout.send();
                            xout.start("/chorus_state");
                            xout.add(gkChorusBypass);
                            xout.send();
                            gkChorusBypass => prevChorusBypass;
                            <<< "chorus_bypass -", gkChorusBypass, "-", currentMenu, "-", 75 >>>;
                        }
                    }
                    else if( cc == 74 && val == 127 && currentMenu == 3 )
                    {
                        1.0 - gkPitchBypass => gkPitchBypass;
                        if( gkPitchBypass < 0.5 )
                        {
                            11 => gkLastToggle;
                        }
                        if( gkPitchBypass != prevPitchBypass )
                        {
                            xout.start("/ps_state");
                            xout.add(gkPitchBypass);
                            xout.send();
                            xout.start("/ps_state");
                            xout.add(gkPitchBypass);
                            xout.send();
                            xout.start("/ps_state");
                            xout.add(gkPitchBypass);
                            xout.send();
                            gkPitchBypass => prevPitchBypass;
                            <<< "pitch_bypass -", gkPitchBypass, "-", currentMenu, "-", 74 >>>;
                        }
                    }
                    else if( cc == 75 && val == 127 && currentMenu == 1 )
                    {
                        1.0 - gkRingBypass => gkRingBypass;
                        if( gkRingBypass < 0.5 )
                        {
                            10 => gkLastToggle;
                        }
                        if( gkRingBypass != prevRingBypass )
                        {
                            xout.start("/rm_state");
                            xout.add(gkRingBypass);
                            xout.send();
                            xout.start("/rm_state");
                            xout.add(gkRingBypass);
                            xout.send();
                            xout.start("/rm_state");
                            xout.add(gkRingBypass);
                            xout.send();
                            gkRingBypass => prevRingBypass;
                            <<< "ring_bypass -", gkRingBypass, "-", currentMenu, "-", 75 >>>;
                        }
                    }
                    else if( cc == 74 && val == 127 && currentMenu == 4 )
                    {
                        1.0 - gkDelayBypass => gkDelayBypass;
                        if( gkDelayBypass < 0.5 )
                        {
                            13 => gkLastToggle;
                        }
                        if( gkDelayBypass != prevDelayBypass )
                        {
                            xout.start("/delay_state");
                            xout.add(gkDelayBypass);
                            xout.send();
                            xout.start("/delay_state");
                            xout.add(gkDelayBypass);
                            xout.send();
                            xout.start("/delay_state");
                            xout.add(gkDelayBypass);
                            xout.send();
                            gkDelayBypass => prevDelayBypass;
                            <<< "delay_bypass -", gkDelayBypass, "-", currentMenu, "-", 74 >>>;
                        }
                    }
                    else if( cc == 72 && val == 127 && currentMenu == 4 )
                    {
                        1.0 - gkReverbBypass => gkReverbBypass;
                        if( gkReverbBypass < 0.5 )
                        {
                            14 => gkLastToggle;
                        }
                        if( gkReverbBypass != prevReverbBypass )
                        {
                            xout.start("/reverb_state");
                            xout.add(gkReverbBypass);
                            xout.send();
                            xout.start("/reverb_state");
                            xout.add(gkReverbBypass);
                            xout.send();
                            xout.start("/reverb_state");
                            xout.add(gkReverbBypass);
                            xout.send();
                            gkReverbBypass => prevReverbBypass;
                            <<< "reverb_bypass -", gkReverbBypass, "-", currentMenu, "-", 72 >>>;
                        }
                    }
                    else if( cc == 20 )
                    {
                        float step;
                        if( val == 127 ) 0.05 => step; else if( val == 0 ) -0.05 => step; else 0.0 => step;
                        if( currentMenu == 0 && step != 0.0 )
                        {
                            bpm + step * 100.0 => bpm;
                            if( bpm > 600.0 ) 600.0 => bpm;
                            if( bpm < 30.0 ) 30.0 => bpm;
                            if( bpm != prevBpm )
                            {
                                xout.start("/bpm");
                                xout.add(bpm);
                                xout.send();
                                xout.start("/bpm");
                                xout.add(bpm);
                                xout.send();
                                xout.start("/bpm");
                                xout.add(bpm);
                                xout.send();
                                <<< "bpm -", bpm, "-", currentMenu, "-", 20 >>>;
                                bpm => prevBpm;
                            }
                        }
                        else if( currentMenu == 2 && step != 0.0 )
                        {
                            if( gkLastToggle == 9 )
                            {
                                gkVibRate + step => gkVibRate;
                                if( gkVibRate > 1.0 ) 1.0 => gkVibRate;
                                if( gkVibRate < 0.0 ) 0.0 => gkVibRate;
                                if( gkVibRate != prevVibRate )
                                {
                                    0.001 + 9.999 * gkVibRate => float freq_val;
                                    xout.start("/vib_freq");
                                    xout.add(freq_val);
                                    xout.send();
                                    xout.start("/vib_freq");
                                    xout.add(freq_val);
                                    xout.send();
                                    xout.start("/vib_freq");
                                    xout.add(freq_val);
                                    xout.send();
                                    <<< "vib_rate -", gkVibRate, "-", currentMenu, "-", 20 >>>;
                                    gkVibRate => prevVibRate;
                                }
                            }
                            else if( gkLastToggle == 12 )
                            {
                                gkChorusRate + step => gkChorusRate;
                                if( gkChorusRate > 1.0 ) 1.0 => gkChorusRate;
                                if( gkChorusRate < 0.0 ) 0.0 => gkChorusRate;
                                if( gkChorusRate != prevChorusRate )
                                {
                                    0.001 + 9.999 * gkChorusRate => float freq_val;
                                    xout.start("/chorus_rate");
                                    xout.add(freq_val);
                                    xout.send();
                                    xout.start("/chorus_rate");
                                    xout.add(freq_val);
                                    xout.send();
                                    xout.start("/chorus_rate");
                                    xout.add(freq_val);
                                    xout.send();
                                    <<< "chorus_rate -", gkChorusRate, "-", currentMenu, "-", 20 >>>;
                                    gkChorusRate => prevChorusRate;
                                }
                            }
                        }
                        else if( step != 0.0 && gkLastToggle == 13 )
                        {
                            gkDelayFeedback + step => gkDelayFeedback;
                            if( gkDelayFeedback > 1.0 ) 1.0 => gkDelayFeedback;
                            if( gkDelayFeedback < 0.0 ) 0.0 => gkDelayFeedback;
                            if( gkDelayFeedback != prevDelayFeedback )
                            {
                                xout.start("/delay_feedback");
                                xout.add(gkDelayFeedback);
                                xout.send();
                                xout.start("/delay_feedback");
                                xout.add(gkDelayFeedback);
                                xout.send();
                                xout.start("/delay_feedback");
                                xout.add(gkDelayFeedback);
                                xout.send();
                                <<< "delay_feedback -", gkDelayFeedback, "-", currentMenu, "-", 20 >>>;
                                gkDelayFeedback => prevDelayFeedback;
                            }
                        }
                    }
                    else if( cc == 22 )
                    {
                        float step;
                        if( val == 127 ) 0.01 => step; else if( val == 0 ) -0.01 => step; else 0.0 => step;
                        if( step != 0.0 && gkLastToggle == 13 )
                        {
                            gkDelayModRate + step => gkDelayModRate;
                            if( gkDelayModRate > 1.0 ) 1.0 => gkDelayModRate;
                            if( gkDelayModRate < 0.0 ) 0.0 => gkDelayModRate;
                            if( gkDelayModRate != prevDelayModRate )
                            {
                                0.001 + 9.999 * gkDelayModRate => float freq_val;
                                xout.start("/delay_mod_rate");
                                xout.add(freq_val);
                                xout.send();
                                xout.start("/delay_mod_rate");
                                xout.add(freq_val);
                                xout.send();
                                xout.start("/delay_mod_rate");
                                xout.add(freq_val);
                                xout.send();
                                <<< "delay_mod_rate -", gkDelayModRate, "-", currentMenu, "-", 22 >>>;
                                gkDelayModRate => prevDelayModRate;
                            }
                        }
                        else if( step != 0.0 && gkLastToggle == 14 )
                        {
                            gkReverbMix + step => gkReverbMix;
                            if( gkReverbMix > 1.0 ) 1.0 => gkReverbMix;
                            if( gkReverbMix < 0.0 ) 0.0 => gkReverbMix;
                            if( gkReverbMix != prevReverbMix )
                            {
                                xout.start("/reverb_mix");
                                xout.add(gkReverbMix);
                                xout.send();
                                xout.start("/reverb_mix");
                                xout.add(gkReverbMix);
                                xout.send();
                                xout.start("/reverb_mix");
                                xout.add(gkReverbMix);
                                xout.send();
                                <<< "reverb_mix -", gkReverbMix, "-", currentMenu, "-", 22 >>>;
                                gkReverbMix => prevReverbMix;
                            }
                        }
                    }
                    else if( cc == 23 )
                    {
                        float step;
                        if( val == 127 ) 0.2 => step; else if( val == 0 ) -0.2 => step; else 0.0 => step;
                        if( currentMenu == 0 && step != 0.0 )
                        {
                            UI_NOTEprob + step => UI_NOTEprob;
                            if( UI_NOTEprob > 1.0 ) 1.0 => UI_NOTEprob;
                            if( UI_NOTEprob < 0.0 ) 0.0 => UI_NOTEprob;
                            if( UI_NOTEprob != prevUI_NOTEprob )
                            {
                                xout.start("/note_prob");
                                xout.add(UI_NOTEprob);
                                xout.send();
                                xout.start("/note_prob");
                                xout.add(UI_NOTEprob);
                                xout.send();
                                xout.start("/note_prob");
                                xout.add(UI_NOTEprob);
                                xout.send();
                                <<< "note_prob -", UI_NOTEprob, "-", currentMenu, "-", 23 >>>;
                                UI_NOTEprob => prevUI_NOTEprob;
                            }
                        }
                        else if( currentMenu == 2 && step != 0.0 )
                        {
                            if( gkLastToggle == 9 )
                            {
                                gkVibFeedback + step => gkVibFeedback;
                                if( gkVibFeedback > 0.99 ) 0.99 => gkVibFeedback;
                                if( gkVibFeedback < 0.0 ) 0.0 => gkVibFeedback;
                                if( gkVibFeedback != prevVibFeedback )
                                {
                                    xout.start("/vib_feedback");
                                    xout.add(gkVibFeedback);
                                    xout.send();
                                    xout.start("/vib_feedback");
                                    xout.add(gkVibFeedback);
                                    xout.send();
                                    xout.start("/vib_feedback");
                                    xout.add(gkVibFeedback);
                                    xout.send();
                                    <<< "vib_feedback -", gkVibFeedback, "-", currentMenu, "-", 23 >>>;
                                    gkVibFeedback => prevVibFeedback;
                                }
                            }
                            else if( gkLastToggle == 12 )
                            {
                                gkChorusFeedback + step => gkChorusFeedback;
                                if( gkChorusFeedback > 0.99 ) 0.99 => gkChorusFeedback;
                                if( gkChorusFeedback < 0.0 ) 0.0 => gkChorusFeedback;
                                if( gkChorusFeedback != prevChorusFeedback )
                                {
                                    xout.start("/chorus_feedback");
                                    xout.add(gkChorusFeedback);
                                    xout.send();
                                    xout.start("/chorus_feedback");
                                    xout.add(gkChorusFeedback);
                                    xout.send();
                                    xout.start("/chorus_feedback");
                                    xout.add(gkChorusFeedback);
                                    xout.send();
                                    <<< "chorus_feedback -", gkChorusFeedback, "-", currentMenu, "-", 23 >>>;
                                    gkChorusFeedback => prevChorusFeedback;
                                }
                            }
                        }
                        else if( step != 0.0 && gkLastToggle == 13 )
                        {
                            gkDelayPitch + step => gkDelayPitch;
                            if( gkDelayPitch > 12.0 ) 12.0 => gkDelayPitch;
                            if( gkDelayPitch < -12.0 ) -12.0 => gkDelayPitch;
                            if( gkDelayPitch != prevDelayPitch )
                            {
                                xout.start("/delay_pitch");
                                xout.add(gkDelayPitch);
                                xout.send();
                                xout.start("/delay_pitch");
                                xout.add(gkDelayPitch);
                                xout.send();
                                xout.start("/delay_pitch");
                                xout.add(gkDelayPitch);
                                xout.send();
                                <<< "delay_pitch -", gkDelayPitch, "-", currentMenu, "-", 23 >>>;
                                gkDelayPitch => prevDelayPitch;
                            }
                        }
                    }
                    else if( cc == 24 )
                    {
                        float step;
                        if( val == 127 ) 0.01 => step; else if( val == 0 ) -0.01 => step; else 0.0 => step;
                        if( currentMenu == 2 && step != 0.0 )
                        {
                            gkVibAmount + step => gkVibAmount;
                            if( gkVibAmount > 1.0 ) 1.0 => gkVibAmount;
                            if( gkVibAmount < 0.0 ) 0.0 => gkVibAmount;
                            if( gkVibAmount != prevVibAmount )
                            {
                                gkVibAmount * 0.03 => float depth_val;
                                xout.start("/vib_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/vib_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/vib_depth");
                                xout.add(depth_val);
                                xout.send();
                                <<< "vib_amount -", gkVibAmount, "-", currentMenu, "-", 24 >>>;
                                gkVibAmount => prevVibAmount;
                            }
                        }
                        else if( currentMenu == 3 && step != 0.0 )
                        {
                            gkPitchMix + step => gkPitchMix;
                            if( gkPitchMix > 1.0 ) 1.0 => gkPitchMix;
                            if( gkPitchMix < 0.0 ) 0.0 => gkPitchMix;
                            if( gkPitchMix != prevPitchMix )
                            {
                                xout.start("/dry_pct_ps");
                                xout.add((1.0 - gkPitchMix) * 100.0);
                                xout.send();
                                xout.start("/dry_pct_ps");
                                xout.add((1.0 - gkPitchMix) * 100.0);
                                xout.send();
                                xout.start("/dry_pct_ps");
                                xout.add((1.0 - gkPitchMix) * 100.0);
                                xout.send();
                                <<< "pitch_mix -", gkPitchMix, "-", currentMenu, "-", 24 >>>;
                                gkPitchMix => prevPitchMix;
                            }
                        }
                        else if( step != 0.0 && gkLastToggle == 13 )
                        {
                            gkDelayMix + step => gkDelayMix;
                            if( gkDelayMix > 1.0 ) 1.0 => gkDelayMix;
                            if( gkDelayMix < 0.0 ) 0.0 => gkDelayMix;
                            if( gkDelayMix != prevDelayMix )
                            {
                                xout.start("/delay_mix");
                                xout.add(gkDelayMix);
                                xout.send();
                                xout.start("/delay_mix");
                                xout.add(gkDelayMix);
                                xout.send();
                                xout.start("/delay_mix");
                                xout.add(gkDelayMix);
                                xout.send();
                                <<< "delay_mix -", gkDelayMix, "-", currentMenu, "-", 24 >>>;
                                gkDelayMix => prevDelayMix;
                            }
                        }
                    }
                    else if( cc == 25 )
                    {
                        float step;
                        if( val == 127 ) 0.01 => step; else if( val == 0 ) -0.01 => step; else 0.0 => step;
                        if( currentMenu == 2 && step != 0.0 )
                        {
                            gkChorusAmount + step => gkChorusAmount;
                            if( gkChorusAmount > 1.0 ) 1.0 => gkChorusAmount;
                            if( gkChorusAmount < 0.0 ) 0.0 => gkChorusAmount;
                            if( gkChorusAmount != prevChorusAmount )
                            {
                                gkChorusAmount * 0.01 => float depth_val;
                                xout.start("/chorus_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/chorus_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/chorus_depth");
                                xout.add(depth_val);
                                xout.send();
                                <<< "chorus_amount -", gkChorusAmount, "-", currentMenu, "-", 25 >>>;
                                gkChorusAmount => prevChorusAmount;
                            }
                        }
                        else if( step != 0.0 && gkLastToggle == 13 )
                        {
                            gkDelayModDepth + step => gkDelayModDepth;
                            if( gkDelayModDepth > 1.0 ) 1.0 => gkDelayModDepth;
                            if( gkDelayModDepth < 0.0 ) 0.0 => gkDelayModDepth;
                            if( gkDelayModDepth != prevDelayModDepth )
                            {
                                gkDelayModDepth * 0.02 => float depth_val;
                                xout.start("/delay_mod_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/delay_mod_depth");
                                xout.add(depth_val);
                                xout.send();
                                xout.start("/delay_mod_depth");
                                xout.add(depth_val);
                                xout.send();
                                <<< "delay_mod_depth -", gkDelayModDepth, "-", currentMenu, "-", 25 >>>;
                                gkDelayModDepth => prevDelayModDepth;
                            }
                        }
                    }
                    else if( cc == 29 )
                    {
                        float norm;
                        val / 127.0 => norm;
                        if( gkLastToggle == 8 )
                        {
                            norm => gkVOLpedal;
                            if( gkVOLpedal != prevVOLpedal )
                            {
                                xout.start("/vol_pedal");
                                xout.add(gkVOLpedal);
                                xout.send();
                                gkVOLpedal => prevVOLpedal;
                                <<< "vol_pedal -", gkVOLpedal, "-", currentMenu, "-", 29 >>>;
                            }
                        }
                        else if( gkLastToggle == 10 )
                        {
                            norm => gkRingRate;
                            if( gkRingRate != prevRingRate )
                            {
                                1.25 * Math.pow(200.0, gkRingRate) => float mod_freq_val;
                                xout.start("/ar_mod_freq");
                                xout.add(mod_freq_val);
                                xout.send();
                                gkRingRate => prevRingRate;
                                <<< "ring_rate -", gkRingRate, "-", currentMenu, "-", 29 >>>;
                            }
                        }
                        else if( gkLastToggle == 11 )
                        {
                            norm * 24.0 - 12.0 => gkPitchShift;
                            if( gkPitchShift != prevPitchShift )
                            {
                                xout.start("/semi");
                                xout.add(gkPitchShift);
                                xout.send();
                                gkPitchShift => prevPitchShift;
                                <<< "pitch_shift -", gkPitchShift, "-", currentMenu, "-", 29 >>>;
                            }
                        }
                        else if( gkLastToggle == 13 )
                        {
                            norm => gkDelayTime;
                            if( gkDelayTime != prevDelayTime )
                            {
                                xout.start("/delay_time");
                                xout.add(gkDelayTime);
                                xout.send();
                                xout.start("/delay_time");
                                xout.add(gkDelayTime);
                                xout.send();
                                xout.start("/delay_time");
                                xout.add(gkDelayTime);
                                xout.send();
                                gkDelayTime => prevDelayTime;
                                <<< "delay_time -", gkDelayTime, "-", currentMenu, "-", 29 >>>;
                            }
                        }
                    }
                    else if (cc == 27) {
                        if (gkGridLock == 0) {
                            val / 127.0 * (num_x - 1.0) => phys_pos_x;
                            <<< "phys_pos_x -", phys_pos_x, "-", currentMenu, "-", 27 >>>;
                        }
                    } else if (cc == 28) {
                        if (gkGridLock == 0) {
                            val / 127.0 * (num_y - 1.0) => phys_pos_y;
                            <<< "phys_pos_y -", phys_pos_y, "-", currentMenu, "-", 28 >>>;
                        }
                    } else if (cc == 77 && val == 127) {
                        1 - gkGridLock => gkGridLock;
                        xout.start("/grid_lock");
                        xout.add(gkGridLock);
                        xout.send();
                        xout.start("/grid_lock");
                        xout.add(gkGridLock);
                        xout.send();
                        xout.start("/grid_lock");
                        xout.add(gkGridLock);
                        xout.send();
                        <<< "grid_lock -", gkGridLock, "-", currentMenu, "-", 77 >>>;
                    } else if (cc == 68 && val == 127) {
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
                                    if (Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;
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
                        <<< "trigger_note -", 1, "-", currentMenu, "-", 68 >>>;
                    } else if( cc == 63 && val == 127 ) {
                        (currentSynthModeIndex + 1) % 3 => currentSynthModeIndex;
                        synthModes[currentSynthModeIndex] => synthMODE;
                        <<< "synth_mode -", synthMODE, "-", currentMenu, "-", 63 >>>;
                        xout.start("/synth_mode");
                        xout.add(synthMODE);
                        xout.send();
                        xout.start("/synth_mode");
                        xout.add(synthMODE);
                        xout.send();
                        xout.start("/synth_mode");
                        xout.add(synthMODE);
                        xout.send();
                    } else if( cc == 67 && val == 127 ) {
                        (currentClassicShapeIndex + 1) % 5 => currentClassicShapeIndex;
                        classicShapes[currentClassicShapeIndex] => classicWAVETYPE;
                        <<< "classic_wavetype -", classicWAVETYPE, "-", currentMenu, "-", 67 >>>;
                        xout.start("/classic_wavetype");
                        xout.add(classicWAVETYPE);
                        xout.send();
                        xout.start("/classic_wavetype");
                        xout.add(classicWAVETYPE);
                        xout.send();
                        xout.start("/classic_wavetype");
                        xout.add(classicWAVETYPE);
                        xout.send();
                    }
                }
            }
        }
        if( !had_msg ) {
            1::samp => now;
        }
    }
}
// spork centralized MIDI listener
spork ~ midi_listener();
dur control_dur;
5::ms => control_dur;
float control_rate;
1.0 / (control_dur / 1::second) => control_rate;
fun void volume_shred()
{
    float vol_on_smooth;
    0.0 => vol_on_smooth;
    float vol_smooth;
    1.0 => vol_smooth;
    while( true )
    {
        Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
        float target_on;
        1.0 - gkVOLpedalBypass => target_on;
        vol_on_smooth * coeff + target_on * (1.0 - coeff) => vol_on_smooth;
        float target_vol;
        gkVOLpedal * vol_on_smooth + 1.0 * (1.0 - vol_on_smooth) => target_vol;
        vol_smooth * coeff + target_vol * (1.0 - coeff) => vol_smooth;
        Math.min(vol_smooth, 1.0) => vol_smooth;
        volGainL.gain(vol_smooth);
        volGainR.gain(vol_smooth);
        vol_on_smooth => gkVolOnSmooth;
        control_dur => now;
    }
}
fun void vibrato_shred()
{
    float vib_on_smooth;
    0.0 => vib_on_smooth;
    SinOsc lfo => blackhole;
    float smooth_rate;
    0.001 + 9.999 * gkVibRate => smooth_rate;
    float smooth_amount;
    gkVibAmount => smooth_amount;
    float smooth_feedback;
    gkVibFeedback => smooth_feedback;
    while( true )
    {
        Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
        float target_on;
        1.0 - gkVibBypass => target_on;
        vib_on_smooth * coeff + target_on * (1.0 - coeff) => vib_on_smooth;
        directVibL.gain(1.0 - vib_on_smooth);
        directVibR.gain(1.0 - vib_on_smooth);
        vibGainL.gain(vib_on_smooth);
        vibGainR.gain(vib_on_smooth);
        float target_rate;
        0.001 + 9.999 * gkVibRate => target_rate;
        smooth_rate * coeff + target_rate * (1.0 - coeff) => smooth_rate;
        lfo.freq(smooth_rate);
        float target_amount;
        gkVibAmount => target_amount;
        smooth_amount * coeff + target_amount * (1.0 - coeff) => smooth_amount;
        12::ms => dur center;
        30::ms * smooth_amount => dur depth;
        vibDelayL.delay(center + depth * lfo.last());
        vibDelayR.delay(center + depth * lfo.last());
        float target_feedback;
        gkVibFeedback => target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        vibFbL.gain(smooth_feedback);
        vibFbR.gain(smooth_feedback);
        vib_on_smooth => gkVibOnSmooth;
        control_dur => now;
    }
}
fun void chorus_shred()
{
    float chorus_on_smooth;
    0.0 => chorus_on_smooth;
    SinOsc lfo => blackhole;
    SinOsc lfoR => blackhole;
    lfo.phase(0.0);
    lfoR.phase(0.5);
    float smooth_rate;
    0.001 + 9.999 * gkChorusRate => smooth_rate;
    float smooth_amount;
    gkChorusAmount => smooth_amount;
    float smooth_feedback;
    gkChorusFeedback => smooth_feedback;
    while( true )
    {
        Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
        float target_on;
        1.0 - gkChorusBypass => target_on;
        chorus_on_smooth * coeff + target_on * (1.0 - coeff) => chorus_on_smooth;
        directChorusL.gain(1.0 - chorus_on_smooth * 0.5);
        directChorusR.gain(1.0 - chorus_on_smooth * 0.5);
        chorusGainL.gain(chorus_on_smooth * 0.5);
        chorusGainR.gain(chorus_on_smooth * 0.5);
        float target_rate;
        0.001 + 9.999 * gkChorusRate => target_rate;
        smooth_rate * coeff + target_rate * (1.0 - coeff) => smooth_rate;
        lfo.freq(smooth_rate);
        lfoR.freq(smooth_rate);
        float target_amount;
        gkChorusAmount => target_amount;
        smooth_amount * coeff + target_amount * (1.0 - coeff) => smooth_amount;
        5::ms => dur center;
        5::ms * smooth_amount => dur depth;
        chorusDelayL.delay(center + depth * lfo.last());
        chorusDelayR.delay(center + depth * lfoR.last());
        float target_feedback;
        gkChorusFeedback => target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        chorusFbL.gain(smooth_feedback);
        chorusFbR.gain(smooth_feedback);
        chorus_on_smooth => gkChorusOnSmooth;
        control_dur => now;
    }
}
fun void pitch_shifter_shred()
{
    float pitch_on_smooth;
    0.0 => pitch_on_smooth;
    Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
    float smooth_mix;
    gkPitchMix => smooth_mix;
    float smooth_shift;
    gkPitchShift => smooth_shift;
    while( true )
    {
        float target_on;
        1.0 - gkPitchBypass => target_on;
        pitch_on_smooth * coeff + target_on * (1.0 - coeff) => pitch_on_smooth;
        smooth_mix * coeff + gkPitchMix * (1.0 - coeff) => smooth_mix;
        smooth_shift * coeff + gkPitchShift * (1.0 - coeff) => smooth_shift;
        Math.pow(2.0, smooth_shift / 12.0) => float ratio;
        ratio => pitchShiftL.shift;
        ratio => pitchShiftR.shift;
        float dry_g;
        1.0 - pitch_on_smooth * smooth_mix => dry_g;
        dry_g => directPitchL.gain;
        dry_g => directPitchR.gain;
        float wet_g;
        pitch_on_smooth * smooth_mix => wet_g;
        wet_g => pitchWetL.gain;
        wet_g => pitchWetR.gain;
        pitch_on_smooth => gkPitchOnSmooth;
        control_dur => now;
    }
}
fun void delay_shred()
{
    float delay_on_smooth;
    0.0 => delay_on_smooth;
    SinOsc lfoL => blackhole;
    SinOsc lfoR => blackhole;
    lfoL.phase(0.0);
    lfoR.phase(0.5);
    float smooth_mix;
    gkDelayMix => smooth_mix;
    float smooth_time;
    gkDelayTime => smooth_time;
    float smooth_feedback;
    gkDelayFeedback => smooth_feedback;
    float smooth_mod_rate;
    0.001 + 9.999 * gkDelayModRate => smooth_mod_rate;
    float smooth_mod_depth;
    gkDelayModDepth => smooth_mod_depth;
    float smooth_pitch;
    gkDelayPitch => smooth_pitch;
    while( true )
    {
        Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
        float target_on;
        1.0 - gkDelayBypass => target_on;
        delay_on_smooth * coeff + target_on * (1.0 - coeff) => delay_on_smooth;
        float target_mix;
        gkDelayMix => target_mix;
        smooth_mix * coeff + target_mix * (1.0 - coeff) => smooth_mix;
        float target_time;
        0.001 + 1.199 * gkDelayTime => target_time;
        smooth_time * coeff + target_time * (1.0 - coeff) => smooth_time;
        float target_feedback;
        gkDelayFeedback * 0.3 => target_feedback;
        smooth_feedback * coeff + target_feedback * (1.0 - coeff) => smooth_feedback;
        smooth_feedback => delayFbL.gain;
        smooth_feedback => delayFbR.gain;
        float target_mod_rate;
        0.001 + 9.999 * gkDelayModRate => target_mod_rate;
        smooth_mod_rate * coeff + target_mod_rate * (1.0 - coeff) => smooth_mod_rate;
        lfoL.freq(smooth_mod_rate);
        lfoR.freq(smooth_mod_rate);
        float target_mod_depth;
        gkDelayModDepth => target_mod_depth;
        smooth_mod_depth * coeff + target_mod_depth * (1.0 - coeff) => smooth_mod_depth;
        dur center;
        smooth_time :: second => center;
        dur mod_depth;
        0.02 :: second * smooth_mod_depth => mod_depth;
        delayDelayL.delay(center + mod_depth * lfoL.last());
        delayDelayR.delay(center + mod_depth * lfoR.last());
        float target_pitch;
        gkDelayPitch => target_pitch;
        smooth_pitch * coeff + target_pitch * (1.0 - coeff) => smooth_pitch;
        Math.pow(2.0, smooth_pitch / 12.0) => float ratio;
        ratio => delPitchShiftL.shift;
        ratio => delPitchShiftR.shift;
        float dry_g;
        1.0 - delay_on_smooth * smooth_mix => dry_g;
        dry_g => directDelayL.gain;
        dry_g => directDelayR.gain;
        float wet_g;
        delay_on_smooth * smooth_mix => wet_g;
        wet_g => delayGainL.gain;
        wet_g => delayGainR.gain;
        delay_on_smooth => gkDelayOnSmooth;
        control_dur => now;
    }
}
fun void ring_mod_shred()
{
    float ring_on_smooth;
    0.0 => ring_on_smooth;
    Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
    float smooth_rate;
    1.25 * Math.pow(200.0, gkRingRate) => smooth_rate;
    while( true )
    {
        float target_on;
        1.0 - gkRingBypass => target_on;
        ring_on_smooth * coeff + target_on * (1.0 - coeff) => ring_on_smooth;
        float target_freq;
        1.25 * Math.pow(200.0, gkRingRate) => target_freq;
        smooth_rate * coeff + target_freq * (1.0 - coeff) => smooth_rate;
        ringMod.freq(smooth_rate);
        float mod;
        ringMod.last() * ring_on_smooth + 1.0 * (1.0 - ring_on_smooth) => mod;
        mod => ringGainL.gain;
        mod => ringGainR.gain;
        ring_on_smooth => gkRingOnSmooth;
        control_dur => now;
    }
}
fun void reverb_shred()
{
    float reverb_on_smooth;
    0.0 => reverb_on_smooth;
    float smooth_mix;
    gkReverbMix => smooth_mix;
    while( true )
    {
        Math.exp(-1.0 / (control_rate * gkPortTime)) => float coeff;
        float target_on;
        1.0 - gkReverbBypass => target_on;
        reverb_on_smooth * coeff + target_on * (1.0 - coeff) => reverb_on_smooth;
        float target_mix;
        gkReverbMix => target_mix;
        smooth_mix * coeff + target_mix * (1.0 - coeff) => smooth_mix;
        float dry_g;
        1.0 - reverb_on_smooth * smooth_mix => dry_g;
        dry_g => directReverbL.gain;
        dry_g => directReverbR.gain;
        float wet_g;
        reverb_on_smooth * smooth_mix => wet_g;
        wet_g => reverbGainL.gain;
        wet_g => reverbGainR.gain;
        reverb_on_smooth => gkReverbOnSmooth;
        control_dur => now;
    }
}
spork ~ volume_shred();
spork ~ vibrato_shred();
spork ~ chorus_shred();
spork ~ pitch_shifter_shred();
spork ~ ring_mod_shred();
spork ~ delay_shred();
spork ~ reverb_shred();
spork ~ sequencer();
int wetConnectedVib;
0 => wetConnectedVib;
int wetConnectedChorus;
0 => wetConnectedChorus;
int wetConnectedPitch;
0 => wetConnectedPitch;
int wetConnectedDelay;
0 => wetConnectedDelay;
int wetConnectedReverb;
0 => wetConnectedReverb;
while (true) {
    10::ms => now;
    if (gkVibBypass >= 0.5 && gkVibOnSmooth < 0.01 && wetConnectedVib) {
        volGainL =< vibDelayL;
        vibDelayL =< vibGainL;
        vibGainL =< vibMixerL;
        volGainR =< vibDelayR;
        vibDelayR =< vibGainR;
        vibGainR =< vibMixerR;
        0 => wetConnectedVib;
    } else if (gkVibBypass < 0.5 && !wetConnectedVib) {
        volGainL => vibDelayL => vibGainL => vibMixerL;
        volGainR => vibDelayR => vibGainR => vibMixerR;
        1 => wetConnectedVib;
    }
    if (gkChorusBypass >= 0.5 && gkChorusOnSmooth < 0.01 && wetConnectedChorus) {
        vibMixerL =< chorusDelayL;
        vibMixerR =< chorusDelayR;
        chorusDelayL =< chorusGainL;
        chorusDelayR =< chorusGainR;
        chorusGainL =< chorusMixerL;
        chorusGainR =< chorusMixerR;
        0 => wetConnectedChorus;
    } else if (gkChorusBypass < 0.5 && !wetConnectedChorus) {
        vibMixerL => chorusDelayL => chorusGainL => chorusMixerL;
        vibMixerR => chorusDelayR => chorusGainR => chorusMixerR;
        1 => wetConnectedChorus;
    }
    if (gkPitchBypass >= 0.5 && gkPitchOnSmooth < 0.01 && wetConnectedPitch) {
        chorusMixerL =< pitchShiftL;
        chorusMixerR =< pitchShiftR;
        pitchShiftL =< pitchWetL;
        pitchShiftR =< pitchWetR;
        pitchWetL =< pitchMixerL;
        pitchWetR =< pitchMixerR;
        0 => wetConnectedPitch;
    } else if (gkPitchBypass < 0.5 && !wetConnectedPitch) {
        chorusMixerL => pitchShiftL => pitchWetL => pitchMixerL;
        chorusMixerR => pitchShiftR => pitchWetR => pitchMixerR;
        1 => wetConnectedPitch;
    }
    if (gkDelayBypass >= 0.5 && gkDelayOnSmooth < 0.01 && wetConnectedDelay) {
        ringGainL =< delayDelayL;
        delayDelayL =< delPitchShiftL;
        delPitchShiftL =< delayGainL;
        delPitchShiftL =< delayFbL;
        delayGainL =< delayMixerL;
        ringGainR =< delayDelayR;
        delayDelayR =< delPitchShiftR;
        delPitchShiftR =< delayGainR;
        delPitchShiftR =< delayFbR;
        delayGainR =< delayMixerR;
        0 => wetConnectedDelay;
    } else if (gkDelayBypass < 0.5 && !wetConnectedDelay) {
        ringGainL => delayDelayL => delPitchShiftL => delayGainL => delayMixerL;
        delPitchShiftL => delayFbL => delayDelayL;
        ringGainR => delayDelayR => delPitchShiftR => delayGainR => delayMixerR;
        delPitchShiftR => delayFbR => delayDelayR;
        1 => wetConnectedDelay;
    }
    if (gkReverbBypass >= 0.5 && gkReverbOnSmooth < 0.01 && wetConnectedReverb) {
        delayMixerL =< revL;
        revL =< reverbGainL;
        reverbGainL =< reverbMixerL;
        delayMixerR =< revR;
        revR =< reverbGainR;
        reverbGainR =< reverbMixerR;
        0 => wetConnectedReverb;
    } else if (gkReverbBypass < 0.5 && !wetConnectedReverb) {
        delayMixerL => revL => reverbGainL => reverbMixerL;
        delayMixerR => revR => reverbGainR => reverbMixerR;
        1 => wetConnectedReverb;
    }
}