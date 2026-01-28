// midi.ck
@import "globals.ck"

global Gain preReverbL;
global Gain preReverbR;
global Gain preChorusL;
global Gain preChorusR;
global Gain volGainL;
global Gain volGainR;
global Gain synthInputL;
global Gain synthInputR;

dur beat;
(30.0 / G.bpm)::second => beat;
dur shortDur;
2.4 * beat => shortDur;

// Vocal Synth Class
class VocalSynth extends Chugen {
    TwoPole r[3];
    Noise n => Envelope ne => r[0] => TwoZero z => Gain vocalGain;
    n => r[1] => z; 
    n => r[2] => z;
    Impulse i => Envelope ie => OnePole o => r[0];
    o => r[1];
    o => r[2];
    
    0.99 => o.pole;
    10.0 => o.gain;
    1.0 => z.b0;
    0.0 => z.b1;
    -1.0 => z.b2;
    4.0 => vocalGain.gain;
    
    int notDone;
    int currentSyllable;
    float baseFreq;
    
    fun void setSyllable(int syl) {
        syl => currentSyllable;
    }
    
    fun void freq(float f) {
        f => baseFreq;
    }
    
    fun float tick(float in) {
        return vocalGain.last();
    }
    
    fun void trigger() {
        if (currentSyllable == G.VOCAL_CHUHH) {
            spork ~ doChUhh();
        } else if (currentSyllable == G.VOCAL_KAYEH) {
            spork ~ doKayEh();
        } else {
            spork ~ doChUhh(); // Default
        }
    }
    
    fun void doChUhh() {
        0.03 => ne.time;
        1900.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        2700.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        3200.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        0.1 => ie.time;
        0.0 => n.gain;
        600.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1500.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        3900.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doKayEh() {
        0.005 => ne.time;
        0.007 => n.gain;
        380.0 => r[0].freq; 0.99 => r[0].radius; 0.7 => r[0].gain;
        1700.0 => r[1].freq; 0.99 => r[1].radius; 1.0 => r[1].gain;
        4500.0 => r[2].freq; 0.99 => r[2].radius; 0.7 => r[2].gain;
        0.0 => i.gain;
        1 => ne.keyOn;
        0.005::second => now;
        1 => ne.keyOff;
        0.01 => ne.time;
        0.01::second => now;
        
        0.1 => ie.time;
        0.0 => n.gain;
        530.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1840.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2480.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doimpulse() {
        baseFreq => float freq;
        while (notDone) {
            1.0 => i.next;
            (1.0 / freq)::second => now;
            freq * 0.995 => freq;
        }
    }
}

fun float[] genHarmonicTable(float x_frac, float y_frac) {
    float table[G.table_size];
    float max_val;
    0.0 => max_val;
    float power;
    0.5 + x_frac * 1.5 => power;
    for (0 => int s; s < G.table_size; s++) {
        0.0 => table[s];
        for (1 => int h; h <= G.num_harmonics; h++) {
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
            table[s] + Math.sin(2 * Math.PI * h * s / G.table_size) * amp * sign => table[s];
            if (Math.fabs(table[s]) > max_val) {
                Math.fabs(table[s]) => max_val;
            }
        }
    }
    if (max_val > 0.0) {
        for (0 => int s; s < G.table_size; s++) {
            table[s] / max_val => table[s];
        }
    }
    return table;
}

fun void genWavetables() {
    for (0 => int x; x < G.num_x; x++) {
        float x_frac;
        if (G.num_x > 1) x * 1.0 / (G.num_x - 1) => x_frac; else 0.0 => x_frac;
        for (0 => int y; y < G.num_y; y++) {
            float y_frac;
            if (G.num_y > 1) y * 1.0 / (G.num_y - 1) => y_frac; else 0.0 => y_frac;
            genHarmonicTable(x_frac, y_frac) @=> G.wavetables[x][y];
        }
    }
}
genWavetables();

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
        (my_phase * G.table_size) $ int % G.table_size => s;
        int x1;
        Math.floor(pos_x) $ int => x1;
        float frac_x;
        pos_x - x1 => frac_x;
        int x2;
        x1 + 1 => x2;
        if (x2 >= G.num_x) {
            x1 => x2;
            0.0 => frac_x;
        }
        int y1;
        Math.floor(pos_y) $ int => y1;
        float frac_y;
        pos_y - y1 => frac_y;
        int y2;
        y1 + 1 => y2;
        if (y2 >= G.num_y) {
            y1 => y2;
            0.0 => frac_y;
        }
        float v11;
        G.wavetables[x1][y1][s] => v11;
        float v12;
        G.wavetables[x1][y2][s] => v12;
        float v21;
        G.wavetables[x2][y1][s] => v21;
        float v22;
        G.wavetables[x2][y2][s] => v22;
        float v1;
        v11 * (1 - frac_y) + v12 * frac_y => v1;
        float v2;
        v21 * (1 - frac_y) + v22 * frac_y => v2;
        return v1 * (1 - frac_x) + v2 * frac_x;
    }
}

fun void modulate(UGen osc, LPF lpf, float baseNote, dur sustainDur, dur releaseDur, int useNoise, int physModel, int vocalSyllable) {
    dur pitchATTACK;
    G.pitchATTACK_seconds::second => pitchATTACK;
    dur pitchDECAY;
    G.pitchDECAY_seconds::second => pitchDECAY;
    dur pitchRELEASE;
    G.pitchRELEASE_seconds::second => pitchRELEASE;
    
    dur filterATTACK;
    G.filterATTACK_seconds::second => filterATTACK;
    dur filterDECAY;
    G.filterDECAY_seconds::second => filterDECAY;
    dur filterRELEASE;
    G.filterRELEASE_seconds::second => filterRELEASE;
    
    SinOsc lfo => blackhole;
    G.lfoRate => lfo.freq;
    1.0 => lfo.gain;
    
    Noise n;
    OnePole op;
    if (useNoise) {
        n => op => blackhole;
        1.0 => n.gain;
        op.pole(G.pnoiseSMOOTHING);
    }
    
    time startTime;
    now => startTime;
    dur pitchFullSustainStart;
    pitchATTACK + pitchDECAY => pitchFullSustainStart;
    
    float pitchReleaseStart;
    0.0 => pitchReleaseStart;
    dur elapsedAtKeyOff;
    sustainDur => elapsedAtKeyOff;
    if (pitchATTACK > 0::second && elapsedAtKeyOff < pitchATTACK) {
        elapsedAtKeyOff / pitchATTACK => pitchReleaseStart;
    } else if (pitchDECAY > 0::second && elapsedAtKeyOff < pitchFullSustainStart) {
        dur decayElapsed;
        elapsedAtKeyOff - pitchATTACK => decayElapsed;
        1.0 + (decayElapsed / pitchDECAY) * (G.pitchSUSTAIN - 1.0) => pitchReleaseStart;
    } else {
        G.pitchSUSTAIN => pitchReleaseStart;
    }
    
    dur filterFullSustainStart;
    filterATTACK + filterDECAY => filterFullSustainStart;
    
    float filterReleaseStart;
    0.0 => filterReleaseStart;
    sustainDur => elapsedAtKeyOff;
    if (filterATTACK > 0::second && elapsedAtKeyOff < filterATTACK) {
        elapsedAtKeyOff / filterATTACK => filterReleaseStart;
    } else if (filterDECAY > 0::second && elapsedAtKeyOff < filterFullSustainStart) {
        dur filterDecayElapsed;
        elapsedAtKeyOff - filterATTACK => filterDecayElapsed;
        1.0 + (filterDecayElapsed / filterDECAY) * (G.filterSUSTAIN - 1.0) => filterReleaseStart;
    } else {
        G.filterSUSTAIN => filterReleaseStart;
    }
    
    time keyOffTime;
    startTime + sustainDur => keyOffTime;
    time endTime;
    dur maxRelease;
    releaseDur => maxRelease;
    if (filterRELEASE > maxRelease) {
        filterRELEASE => maxRelease;
    }
    if (pitchRELEASE > maxRelease) {
        pitchRELEASE => maxRelease;
    }
    keyOffTime + maxRelease => endTime;
    
    while (now < endTime) {
        float pitchEnv;
        0.0 => pitchEnv;
        time currentTime;
        now => currentTime;
        dur elapsed;
        currentTime - startTime => elapsed;
        
        if (currentTime < keyOffTime) {
            if (pitchATTACK > 0::second && elapsed < pitchATTACK) {
                elapsed / pitchATTACK => pitchEnv;
            } else if (pitchDECAY > 0::second && elapsed < pitchFullSustainStart) {
                dur decayElapsed;
                elapsed - pitchATTACK => decayElapsed;
                1.0 + (decayElapsed / pitchDECAY) * (G.pitchSUSTAIN - 1.0) => pitchEnv;
            } else {
                G.pitchSUSTAIN => pitchEnv;
            }
        } else {
            dur releaseElapsed;
            currentTime - keyOffTime => releaseElapsed;
            if (pitchRELEASE > 0::second) {
                pitchReleaseStart * (1.0 - (releaseElapsed / pitchRELEASE)) => pitchEnv;
            } else {
                pitchReleaseStart => pitchEnv;
            }
        }
        if (pitchEnv > 1.0) 1.0 => pitchEnv; else if (pitchEnv < 0.0) 0.0 => pitchEnv;
        
        float current_offset;
        G.pitch_offset_start => current_offset;
        if (currentTime >= keyOffTime) {
            G.pitch_offset_end => current_offset;
        }
        float pitch_mod;
        current_offset * (1.0 - pitchEnv) => pitch_mod;
        
        float filterEnv;
        0.0 => filterEnv;
        if (currentTime < keyOffTime) {
            if (filterATTACK > 0::second && elapsed < filterATTACK) {
                elapsed / filterATTACK => filterEnv;
            } else if (filterDECAY > 0::second && elapsed < filterFullSustainStart) {
                dur filterDecayElapsed;
                elapsed - filterATTACK => filterDecayElapsed;
                1.0 + (filterDecayElapsed / filterDECAY) * (G.filterSUSTAIN - 1.0) => filterEnv;
            } else {
                G.filterSUSTAIN => filterEnv;
            }
        } else {
            dur filterReleaseElapsed;
            currentTime - keyOffTime => filterReleaseElapsed;
            if (filterRELEASE > 0::second) {
                filterReleaseStart * (1.0 - (filterReleaseElapsed / filterRELEASE)) => filterEnv;
            } else {
                filterReleaseStart => filterEnv;
            }
        }
        if (filterEnv > 1.0) 1.0 => filterEnv; else if (filterEnv < 0.0) 0.0 => filterEnv;
        
        float currentFilterFreq;
        G.filterBaseFreq + G.filterAmount * filterEnv => currentFilterFreq;
        if (currentFilterFreq < 20.0) 20.0 => currentFilterFreq;
        currentFilterFreq => lpf.freq;
        
        float mod;
        lfo.last() * G.lfoDepth => mod;
        if (useNoise) {
            mod + op.last() * G.pnoiseDEPTH => mod;
        }
        
        Std.mtof(baseNote + pitch_mod) => float modFund;
        
        if (G.synthMode == G.SYNTH_VOCAL) {
            (osc $ VocalSynth).freq(modFund);
            float pos_x;
            filterEnv * (G.num_x - 1) + mod => pos_x;
            if (pos_x < 0) 0 => pos_x;
            if (pos_x > G.num_x - 1) G.num_x - 1 => pos_x;
            float pos_y;
            pitchEnv * (G.num_y - 1) + mod => pos_y;
            if (pos_y < 0) 0 => pos_y;
            if (pos_y > G.num_y - 1) G.num_y - 1 => pos_y;
            Math.floor(pos_x) $ int => int x;
            Math.floor(pos_y) $ int => int y;
            if (x < 0) 0 => x;
            if (x >= G.num_x) G.num_x - 1 => x;
            if (y < 0) 0 => y;
            if (y >= G.num_y) G.num_y - 1 => y;
            G.vocalSyllables[x][y] => int newSyllable;
            if (newSyllable != vocalSyllable) {
                newSyllable => vocalSyllable;
                (osc $ VocalSynth).setSyllable(vocalSyllable);
                (osc $ VocalSynth).trigger();
            }
        } else if (G.synthMode == G.SYNTH_WAVETABLE) {
            (osc $ WavetableOsc).freq(modFund);
            float pos_x;
            filterEnv * (G.num_x - 1) + mod => pos_x;
            if (pos_x < 0) 0 => pos_x;
            if (pos_x > G.num_x - 1) G.num_x - 1 => pos_x;
            float pos_y;
            pitchEnv * (G.num_y - 1) + mod => pos_y;
            if (pos_y < 0) 0 => pos_y;
            if (pos_y > G.num_y - 1) G.num_y - 1 => pos_y;
            (osc $ WavetableOsc).setPos(pos_x, pos_y);
        } else if (G.synthMode == G.SYNTH_CLASSIC) {
            if (G.classicWaveType != G.CLASSIC_NSE) {
                if (G.classicWaveType == G.CLASSIC_SIN) {
                    (osc $ SinOsc).freq(modFund);
                } else if (G.classicWaveType == G.CLASSIC_TRI) {
                    (osc $ TriOsc).freq(modFund);
                } else if (G.classicWaveType == G.CLASSIC_SAW) {
                    (osc $ SawOsc).freq(modFund);
                } else if (G.classicWaveType == G.CLASSIC_PLS) {
                    (osc $ PulseOsc).freq(modFund);
                }
            }
            if (G.classicWaveType == G.CLASSIC_PLS) {
                float currentWidth;
                G.pulseWidth + mod * G.pwmDepth => currentWidth;
                if (currentWidth < 0.0) 0.0 => currentWidth;
                if (currentWidth > 1.0) 1.0 => currentWidth;
                (osc $ PulseOsc).width(currentWidth);
            }
        } else if (G.synthMode == G.SYNTH_PHYSMOD) {
            if (physModel != G.PHYS_SHKR) {
                (osc $ StkInstrument).freq(modFund);
            }
        }
        50::samp => now;
    }
}

fun void playNote(float freq, int midi) {
    dur noteATTACK;
    G.noteATTACK_seconds::second => noteATTACK;
    dur noteDECAY;
    G.noteDECAY_seconds::second => noteDECAY;
    dur noteRELEASE;
    G.noteRELEASE_seconds::second => noteRELEASE;
    
    ADSR noteEnv;
    noteEnv.set(noteATTACK, noteDECAY, G.noteSUSTAIN, noteRELEASE);
    LPF lpf => noteEnv => volGainL;
    noteEnv => volGainR;
    G.filterResonance => lpf.Q;
    
    UGen osc;
    int currentPhysModel;
    int currentVocalSyllable;
    
    if (G.synthMode == G.SYNTH_PHYSMOD) {
        int x;
        Math.floor(G.phys_pos_x) $ int => x;
        if (x < 0) 0 => x;
        if (x >= G.num_x) G.num_x - 1 => x;
        int y;
        Math.floor(G.phys_pos_y) $ int => y;
        if (y < 0) 0 => y;
        if (y >= G.num_y) G.num_y - 1 => y;
        G.physModels[x][y] => currentPhysModel;
    } else if (G.synthMode == G.SYNTH_VOCAL) {
        int x;
        Math.floor(G.vocal_pos_x) $ int => x;
        if (x < 0) 0 => x;
        if (x >= G.num_x) G.num_x - 1 => x;
        int y;
        Math.floor(G.vocal_pos_y) $ int => y;
        if (y < 0) 0 => y;
        if (y >= G.num_y) G.num_y - 1 => y;
        G.vocalSyllables[x][y] => currentVocalSyllable;
    }
    
    if (G.synthMode == G.SYNTH_VOCAL) {
        VocalSynth vs => lpf;
        vs @=> osc;
        vs.freq(freq);
        vs.setSyllable(currentVocalSyllable);
        vs.trigger();
        vs.gain(G.gainScale * 2.0);
    } else if (G.synthMode == G.SYNTH_WAVETABLE) {
        WavetableOsc wto => lpf;
        wto @=> osc;
        wto.freq(freq);
        wto.gain(G.gainScale);
    } else if (G.synthMode == G.SYNTH_CLASSIC) {
        if (G.classicWaveType == G.CLASSIC_SIN) {
            SinOsc so => lpf;
            so @=> osc;
            so.freq(freq);
            so.gain(G.gainScale);
        } else if (G.classicWaveType == G.CLASSIC_TRI) {
            TriOsc to => lpf;
            to @=> osc;
            to.freq(freq);
            to.gain(G.gainScale);
        } else if (G.classicWaveType == G.CLASSIC_SAW) {
            SawOsc so => lpf;
            so @=> osc;
            so.freq(freq);
            so.gain(G.gainScale);
        } else if (G.classicWaveType == G.CLASSIC_PLS) {
            PulseOsc po => lpf;
            po @=> osc;
            po.freq(freq);
            po.gain(G.gainScale);
            0.5 => po.width;
        } else if (G.classicWaveType == G.CLASSIC_NSE) {
            Noise no => lpf;
            no @=> osc;
            no.gain(G.gainScale);
        }
    } else if (G.synthMode == G.SYNTH_PHYSMOD) {
        // All physical models code (keeping all 14 models)
        if (currentPhysModel == G.PHYS_BAND) {
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
        } else if (currentPhysModel == G.PHYS_BOTL) {
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
        } else if (currentPhysModel == G.PHYS_BHOL) {
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
        } else if (currentPhysModel == G.PHYS_BOWD) {
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
        } else if (currentPhysModel == G.PHYS_BRAS) {
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
        } else if (currentPhysModel == G.PHYS_CLAR) {
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
        } else if (currentPhysModel == G.PHYS_FLUT) {
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
        } else if (currentPhysModel == G.PHYS_MAND) {
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
        } else if (currentPhysModel == G.PHYS_MODA) {
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
        } else if (currentPhysModel == G.PHYS_SAXO) {
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
        } else if (currentPhysModel == G.PHYS_SHKR) {
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
        } else if (currentPhysModel == G.PHYS_SITR) {
            Sitar st => lpf;
            st.freq(freq);
            st @=> osc;
        } else if (currentPhysModel == G.PHYS_STIF) {
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
        } else if (currentPhysModel == G.PHYS_VOIC) {
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
        osc.gain(G.gainScale * volFactor);
    }
    
    int useNoise;
    if (Math.randomf() < G.pnoisePROBABILITY) {
        1 => useNoise;
    } else {
        0 => useNoise;
    }
    
    spork ~ modulate(osc, lpf, Std.ftom(freq), shortDur, noteRELEASE, useNoise, currentPhysModel, currentVocalSyllable);
    noteEnv.keyOn();
    if (G.synthMode == G.SYNTH_PHYSMOD) {
        (osc $ StkInstrument).noteOn(1.0);
    }
    shortDur => now;
    noteEnv.keyOff();
    if (G.synthMode == G.SYNTH_PHYSMOD) {
        (osc $ StkInstrument).noteOff(1.0);
    }
    noteRELEASE => now;
    lpf =< noteEnv;
    noteEnv =< volGainL;
    noteEnv =< volGainR;
    0 => G.playing[midi];
}

volGainL => synthInputL;
volGainR => synthInputR;

MidiIn mins[0];
MidiMsg msg;
0 => int devCount;
MidiIn test;
while(test.open(devCount)) {
    devCount++;
}
for(0 => int i; i < devCount; i++) {
    MidiIn min;
    if(min.open(i)) {
        mins << min;
    }
}
if(mins.cap() == 0) {
    me.exit();
}

// Set starting synth mode to VOCAL
G.SYNTH_VOCAL => G.synthMode;
<<< "Starting in VOCAL synthesis mode" >>>;

fun string getSynthModeName(int mode) {
    if (mode == G.SYNTH_VOCAL) return "VOCAL";
    else if (mode == G.SYNTH_WAVETABLE) return "WAVETABLE";
    else if (mode == G.SYNTH_CLASSIC) return "CLASSIC";
    else if (mode == G.SYNTH_PHYSMOD) return "PHYSMOD";
    else return "UNKNOWN";
}

fun string getMenuName(int menu) {
    if (menu == 0) return "MAIN";
    else if (menu == 1) return "FX1";
    else if (menu == 2) return "FX2";
    else if (menu == 3) return "FX3";
    else if (menu == 4) return "FX4";
    else return "UNKNOWN";
}

fun void midi_listener() {
    while(true) {
        int had_msg;
        0 => had_msg;
        for(0 => int i; i < mins.cap(); i++) {
            while(mins[i].recv(msg)) {
                1 => had_msg;
                msg.data1 & 0xF0 => int status;
                (msg.data1 & 0x0F) + 1 => int channel;
                
                if(status == 0xB0) {
                    int cc;
                    msg.data2 => cc;
                    int val;
                    msg.data3 => val;
                    
                    if(cc == 70 && val == 127) {
                        (G.currentMenu - 1 + 5) % 5 => G.currentMenu;
                        <<< "MENU CHANGED:", getMenuName(G.currentMenu), "(CC70)" >>>;
                    }
                    else if(cc == 73 && val == 127) {
                        (G.currentMenu + 1) % 5 => G.currentMenu;
                        <<< "MENU CHANGED:", getMenuName(G.currentMenu), "(CC73)" >>>;
                    }
                    
                    else if(cc == 74 && val == 127 && G.currentMenu == 1) {
                        1.0 - G.gkVOLpedalBypass => G.gkVOLpedalBypass;
                        if(G.gkVOLpedalBypass < 0.5) {
                            8 => G.gkLastToggle;
                        }
                        <<< "VOL PEDAL BYPASS:", G.gkVOLpedalBypass, "(CC74)" >>>;
                        if(G.gkVOLpedalBypass != G.prevVOLpedalBypass) {
                            G.xout.start("/volpedal_state");
                            G.xout.add(G.gkVOLpedalBypass);
                            G.xout.send();
                            G.gkVOLpedalBypass => G.prevVOLpedalBypass;
                        }
                    }
                    else if(cc == 74 && val == 127 && G.currentMenu == 2) {
                        1.0 - G.gkVibBypass => G.gkVibBypass;
                        if(G.gkVibBypass < 0.5) {
                            9 => G.gkLastToggle;
                        }
                        <<< "VIBRATO BYPASS:", G.gkVibBypass, "(CC74)" >>>;
                        if(G.gkVibBypass != G.prevVibBypass) {
                            G.xout.start("/vib_state");
                            G.xout.add(G.gkVibBypass);
                            G.xout.send();
                            G.gkVibBypass => G.prevVibBypass;
                        }
                    }
                    else if(cc == 75 && val == 127 && G.currentMenu == 2) {
                        1.0 - G.gkChorusBypass => G.gkChorusBypass;
                        if(G.gkChorusBypass < 0.5) {
                            12 => G.gkLastToggle;
                        }
                        <<< "CHORUS BYPASS:", G.gkChorusBypass, "(CC75)" >>>;
                        if(G.gkChorusBypass != G.prevChorusBypass) {
                            G.xout.start("/chorus_state");
                            G.xout.add(G.gkChorusBypass);
                            G.xout.send();
                            G.gkChorusBypass => G.prevChorusBypass;
                        }
                    }
                    else if(cc == 74 && val == 127 && G.currentMenu == 3) {
                        1.0 - G.gkPitchBypass => G.gkPitchBypass;
                        if(G.gkPitchBypass < 0.5) {
                            11 => G.gkLastToggle;
                        }
                        <<< "PITCH SHIFT BYPASS:", G.gkPitchBypass, "(CC74)" >>>;
                        if(G.gkPitchBypass != G.prevPitchBypass) {
                            G.xout.start("/ps_state");
                            G.xout.add(G.gkPitchBypass);
                            G.xout.send();
                            G.gkPitchBypass => G.prevPitchBypass;
                        }
                    }
                    else if(cc == 75 && val == 127 && G.currentMenu == 3) {
                        1.0 - G.gkSpectralBypass => G.gkSpectralBypass;
                        if(G.gkSpectralBypass < 0.5) {
                            15 => G.gkLastToggle;
                        }
                        <<< "SPECTRAL BYPASS:", G.gkSpectralBypass, "(CC75)" >>>;
                        if(G.gkSpectralBypass != G.prevSpectralBypass) {
                            G.xout.start("/spectral_state");
                            G.xout.add(G.gkSpectralBypass);
                            G.xout.send();
                            G.gkSpectralBypass => G.prevSpectralBypass;
                        }
                    }
                    else if(cc == 75 && val == 127 && G.currentMenu == 1) {
                        1.0 - G.gkRingBypass => G.gkRingBypass;
                        if(G.gkRingBypass < 0.5) {
                            10 => G.gkLastToggle;
                        }
                        <<< "RING MOD BYPASS:", G.gkRingBypass, "(CC75)" >>>;
                        if(G.gkRingBypass != G.prevRingBypass) {
                            G.xout.start("/rm_state");
                            G.xout.add(G.gkRingBypass);
                            G.xout.send();
                            G.gkRingBypass => G.prevRingBypass;
                        }
                    }
                    else if(cc == 76 && val == 127 && G.currentMenu == 2) {
                        1.0 - G.gkPhaserBypass => G.gkPhaserBypass;
                        if(G.gkPhaserBypass < 0.5) {
                            16 => G.gkLastToggle; 
                        }
                        <<< "PHASER BYPASS:", G.gkPhaserBypass, "(CC76)" >>>;
                        if(G.gkPhaserBypass != G.prevPhaserBypass) {
                            G.xout.start("/phaser_state");
                            G.xout.add(G.gkPhaserBypass);
                            G.xout.send();
                            G.gkPhaserBypass => G.prevPhaserBypass;
                        }
                    }
                    else if(cc == 74 && val == 127 && G.currentMenu == 4) {
                        1.0 - G.gkDelayBypass => G.gkDelayBypass;
                        if(G.gkDelayBypass < 0.5) {
                            13 => G.gkLastToggle;
                        }
                        <<< "DELAY BYPASS:", G.gkDelayBypass, "(CC74)" >>>;
                        if(G.gkDelayBypass != G.prevDelayBypass) {
                            G.xout.start("/delay_state");
                            G.xout.add(G.gkDelayBypass);
                            G.xout.send();
                            G.gkDelayBypass => G.prevDelayBypass;
                        }
                    }
                    else if(cc == 72 && val == 127 && G.currentMenu == 4) {
                        1.0 - G.gkReverbBypass => G.gkReverbBypass;
                        if(G.gkReverbBypass < 0.5) {
                            14 => G.gkLastToggle;
                        }
                        <<< "REVERB BYPASS:", G.gkReverbBypass, "(CC72)" >>>;
                        if(G.gkReverbBypass != G.prevReverbBypass) {
                            G.xout.start("/reverb_state");
                            G.xout.add(G.gkReverbBypass);
                            G.xout.send();
                            G.gkReverbBypass => G.prevReverbBypass;
                        }
                    }
                    
                    else if(cc == 20) {
                        float step;
                        if(val == 127) 0.02 => step; else if(val == 0) -0.02 => step; else 0.0 => step;
                        
                        if(G.currentMenu == 0 && step != 0.0) {
                            G.bpm + step * 100.0 => G.bpm;
                            if(G.bpm > 600.0) 600.0 => G.bpm;
                            if(G.bpm < 30.0) 30.0 => G.bpm;
                            <<< "BPM:", G.bpm, "(CC20)" >>>;
                            if(G.bpm != G.prevBpm) {
                                G.xout.start("/bpm");
                                G.xout.add(G.bpm);
                                G.xout.send();
                                G.bpm => G.prevBpm;
                            }
                        }
                        else if(G.currentMenu == 2 && step != 0.0) {
                            if(G.gkLastToggle == 9) {
                                G.gkVibRate + step => G.gkVibRate;
                                if(G.gkVibRate > 1.0) 1.0 => G.gkVibRate;
                                if(G.gkVibRate < 0.0) 0.0 => G.gkVibRate;
                                <<< "VIBRATO RATE:", G.gkVibRate, "(CC20)" >>>;
                                if(G.gkVibRate != G.prevVibRate) {
                                    0.001 + 9.999 * G.gkVibRate => float freq_val;
                                    G.xout.start("/vib_freq");
                                    G.xout.add(freq_val);
                                    G.xout.send();
                                    G.gkVibRate => G.prevVibRate;
                                }
                            }
                            else if(G.gkLastToggle == 12) {
                                G.gkChorusRate + step => G.gkChorusRate;
                                if(G.gkChorusRate > 1.0) 1.0 => G.gkChorusRate;
                                if(G.gkChorusRate < 0.0) 0.0 => G.gkChorusRate;
                                <<< "CHORUS RATE:", G.gkChorusRate, "(CC20)" >>>;
                                if(G.gkChorusRate != G.prevChorusRate) {
                                    0.001 + 9.999 * G.gkChorusRate => float freq_val;
                                    G.xout.start("/chorus_rate");
                                    G.xout.add(freq_val);
                                    G.xout.send();
                                    G.gkChorusRate => G.prevChorusRate;
                                }
                            }
                            else if(G.gkLastToggle == 16) {
                                G.gkPhaserRate + step => G.gkPhaserRate;
                                if(G.gkPhaserRate > 1.0) 1.0 => G.gkPhaserRate;
                                if(G.gkPhaserRate < 0.0) 0.0 => G.gkPhaserRate;
                                <<< "PHASER RATE:", G.gkPhaserRate, "(CC20)" >>>;
                                if(G.gkPhaserRate != G.prevPhaserRate) {
                                    0.01 + 4.99 * G.gkPhaserRate => float freq_val;
                                    G.xout.start("/phaser_rate");
                                    G.xout.add(freq_val);
                                    G.xout.send();
                                    G.gkPhaserRate => G.prevPhaserRate;
                                }
                            }
                        }
                        else if(step != 0.0 && G.gkLastToggle == 13) {
                            G.gkDelayFeedback + step => G.gkDelayFeedback;
                            if(G.gkDelayFeedback > 1.0) 1.0 => G.gkDelayFeedback;
                            if(G.gkDelayFeedback < 0.0) 0.0 => G.gkDelayFeedback;
                            <<< "DELAY FEEDBACK:", G.gkDelayFeedback, "(CC20)" >>>;
                            if(G.gkDelayFeedback != G.prevDelayFeedback) {
                                G.xout.start("/delay_feedback");
                                G.xout.add(G.gkDelayFeedback);
                                G.xout.send();
                                G.gkDelayFeedback => G.prevDelayFeedback;
                            }
                        }
                    }
                    else if(cc == 22) {
                        float step;
                        if(val == 127) 0.01 => step; else if(val == 0) -0.01 => step; else 0.0 => step;
                        
                        if(step != 0.0 && G.gkLastToggle == 13) {
                            G.gkDelayModRate + step => G.gkDelayModRate;
                            if(G.gkDelayModRate > 1.0) 1.0 => G.gkDelayModRate;
                            if(G.gkDelayModRate < 0.0) 0.0 => G.gkDelayModRate;
                            <<< "DELAY MOD RATE:", G.gkDelayModRate, "(CC22)" >>>;
                            if(G.gkDelayModRate != G.prevDelayModRate) {
                                0.001 + 9.999 * G.gkDelayModRate => float freq_val;
                                G.xout.start("/delay_mod_rate");
                                G.xout.add(freq_val);
                                G.xout.send();
                                G.gkDelayModRate => G.prevDelayModRate;
                            }
                        }
                        else if(step != 0.0 && G.gkLastToggle == 14) {
                            G.gkReverbMix + step => G.gkReverbMix;
                            if(G.gkReverbMix > 1.0) 1.0 => G.gkReverbMix;
                            if(G.gkReverbMix < 0.0) 0.0 => G.gkReverbMix;
                            <<< "REVERB MIX:", G.gkReverbMix, "(CC22)" >>>;
                            if(G.gkReverbMix != G.prevReverbMix) {
                                G.xout.start("/reverb_mix");
                                G.xout.add(G.gkReverbMix);
                                G.xout.send();
                                G.gkReverbMix => G.prevReverbMix;
                            }
                        }
                    }
                    else if(cc == 23) {
                        float step;
                        if(val == 127) 0.05 => step; else if(val == 0) -0.05 => step; else 0.0 => step;
                        
                        if(G.currentMenu == 0 && step != 0.0) {
                            G.UI_NOTEprob + step => G.UI_NOTEprob;
                            if(G.UI_NOTEprob > 1.0) 1.0 => G.UI_NOTEprob;
                            if(G.UI_NOTEprob < 0.0) 0.0 => G.UI_NOTEprob;
                            <<< "NOTE PROBABILITY:", G.UI_NOTEprob, "(CC23)" >>>;
                            if(G.UI_NOTEprob != G.prevUI_NOTEprob) {
                                G.xout.start("/note_prob");
                                G.xout.add(G.UI_NOTEprob);
                                G.xout.send();
                                G.UI_NOTEprob => G.prevUI_NOTEprob;
                            }
                        }
                        else if(G.currentMenu == 2 && step != 0.0) {
                            if(G.gkLastToggle == 9) {
                                G.gkVibFeedback + step => G.gkVibFeedback;
                                if(G.gkVibFeedback > 0.99) 0.99 => G.gkVibFeedback;
                                if(G.gkVibFeedback < 0.0) 0.0 => G.gkVibFeedback;
                                <<< "VIBRATO FEEDBACK:", G.gkVibFeedback, "(CC23)" >>>;
                                if(G.gkVibFeedback != G.prevVibFeedback) {
                                    G.xout.start("/vib_feedback");
                                    G.xout.add(G.gkVibFeedback);
                                    G.xout.send();
                                    G.gkVibFeedback => G.prevVibFeedback;
                                }
                            }
                            else if(G.gkLastToggle == 12) {
                                G.gkChorusFeedback + step => G.gkChorusFeedback;
                                if(G.gkChorusFeedback > 0.99) 0.99 => G.gkChorusFeedback;
                                if(G.gkChorusFeedback < 0.0) 0.0 => G.gkChorusFeedback;
                                <<< "CHORUS FEEDBACK:", G.gkChorusFeedback, "(CC23)" >>>;
                                if(G.gkChorusFeedback != G.prevChorusFeedback) {
                                    G.xout.start("/chorus_feedback");
                                    G.xout.add(G.gkChorusFeedback);
                                    G.xout.send();
                                    G.gkChorusFeedback => G.prevChorusFeedback;
                                }
                            }
                            else if(G.gkLastToggle == 16) {
                                G.gkPhaserFeedback + step => G.gkPhaserFeedback;
                                if(G.gkPhaserFeedback > 0.99) 0.99 => G.gkPhaserFeedback;
                                if(G.gkPhaserFeedback < 0.0) 0.0 => G.gkPhaserFeedback;
                                <<< "PHASER FEEDBACK:", G.gkPhaserFeedback, "(CC23)" >>>;
                                if(G.gkPhaserFeedback != G.prevPhaserFeedback) {
                                    G.xout.start("/phaser_feedback");
                                    G.xout.add(G.gkPhaserFeedback);
                                    G.xout.send();
                                    G.gkPhaserFeedback => G.prevPhaserFeedback;
                                }
                            }
                        }
                        else if(step != 0.0 && G.gkLastToggle == 13) {
                            G.gkDelayPitch + step => G.gkDelayPitch;
                            if(G.gkDelayPitch > 12.0) 12.0 => G.gkDelayPitch;
                            if(G.gkDelayPitch < -12.0) -12.0 => G.gkDelayPitch;
                            <<< "DELAY PITCH:", G.gkDelayPitch, "(CC23)" >>>;
                            if(G.gkDelayPitch != G.prevDelayPitch) {
                                G.xout.start("/delay_pitch");
                                G.xout.add(G.gkDelayPitch);
                                G.xout.send();
                                G.gkDelayPitch => G.prevDelayPitch;
                            }
                        }
                    }

                    else if(cc == 24) {
                        float step;
                        if(val == 127) 0.01 => step; else if(val == 0) -0.01 => step; else 0.0 => step;
                        
                        if(G.currentMenu == 2 && step != 0.0) {
                            G.gkVibAmount + step => G.gkVibAmount;
                            if(G.gkVibAmount > 1.0) 1.0 => G.gkVibAmount;
                            if(G.gkVibAmount < 0.0) 0.0 => G.gkVibAmount;
                            <<< "VIBRATO AMOUNT:", G.gkVibAmount, "(CC24)" >>>;
                            if(G.gkVibAmount != G.prevVibAmount) {
                                G.gkVibAmount * 0.03 => float depth_val;
                                G.xout.start("/vib_depth");
                                G.xout.add(depth_val);
                                G.xout.send();
                                G.gkVibAmount => G.prevVibAmount;
                            }
                        }
                        else if(G.currentMenu == 3 && step != 0.0) {
                            G.gkPitchMix + step => G.gkPitchMix;
                            if(G.gkPitchMix > 1.0) 1.0 => G.gkPitchMix;
                            if(G.gkPitchMix < 0.0) 0.0 => G.gkPitchMix;
                            <<< "PITCH MIX:", G.gkPitchMix, "(CC24)" >>>;
                            if(G.gkPitchMix != G.prevPitchMix) {
                                G.xout.start("/dry_pct_ps");
                                G.xout.add((1.0 - G.gkPitchMix) * 100.0);
                                G.xout.send();
                                G.gkPitchMix => G.prevPitchMix;
                            }
                        }
                        else if(step != 0.0 && G.gkLastToggle == 13) {
                            G.gkDelayMix + step => G.gkDelayMix;
                            if(G.gkDelayMix > 1.0) 1.0 => G.gkDelayMix;
                            if(G.gkDelayMix < 0.0) 0.0 => G.gkDelayMix;
                            <<< "DELAY MIX:", G.gkDelayMix, "(CC24)" >>>;
                            if(G.gkDelayMix != G.prevDelayMix) {
                                G.xout.start("/delay_mix");
                                G.xout.add(G.gkDelayMix);
                                G.xout.send();
                                G.gkDelayMix => G.prevDelayMix;
                            }
                        }
                    }
                    else if(cc == 25) {
                        float step;
                        if(val == 127) 0.01 => step; else if(val == 0) -0.01 => step; else 0.0 => step;
                        
                        
                        if(G.currentMenu == 2 && step != 0.0) {
                            G.gkChorusAmount + step => G.gkChorusAmount;
                            if(G.gkChorusAmount > 1.0) 1.0 => G.gkChorusAmount;
                            if(G.gkChorusAmount < 0.0) 0.0 => G.gkChorusAmount;
                            <<< "CHORUS AMOUNT:", G.gkChorusAmount, "(CC25)" >>>;
                            if(G.gkChorusAmount != G.prevChorusAmount) {
                                G.gkChorusAmount * 0.01 => float depth_val;
                                G.xout.start("/chorus_depth");
                                G.xout.add(depth_val);
                                G.xout.send();
                                G.gkChorusAmount => G.prevChorusAmount;
                            }
                        }
                        else if(step != 0.0 && G.gkLastToggle == 13) {
                            G.gkDelayModDepth + step => G.gkDelayModDepth;
                            if(G.gkDelayModDepth > 1.0) 1.0 => G.gkDelayModDepth;
                            if(G.gkDelayModDepth < 0.0) 0.0 => G.gkDelayModDepth;
                            <<< "DELAY MOD DEPTH:", G.gkDelayModDepth, "(CC25)" >>>;
                            if(G.gkDelayModDepth != G.prevDelayModDepth) {
                                G.gkDelayModDepth * 0.02 => float depth_val;
                                G.xout.start("/delay_mod_depth");
                                G.xout.add(depth_val);
                                G.xout.send();
                                G.gkDelayModDepth => G.prevDelayModDepth;
                            }
                        }
                    }

                    else if(cc == 26) {
                        float step;
                        if(val == 127) 0.01 => step; else if(val == 0) -0.01 => step; else 0.0 => step;
                        
                        if(G.currentMenu == 2 && step != 0.0 && G.gkLastToggle == 16) {
                            G.gkPhaserDepth + step => G.gkPhaserDepth;
                            if(G.gkPhaserDepth > 1.0) 1.0 => G.gkPhaserDepth;
                            if(G.gkPhaserDepth < 0.0) 0.0 => G.gkPhaserDepth;
                            <<< "PHASER DEPTH:", G.gkPhaserDepth, "(CC26)" >>>;
                            if(G.gkPhaserDepth != G.prevPhaserDepth) {
                                G.xout.start("/phaser_depth");
                                G.xout.add(G.gkPhaserDepth);
                                G.xout.send();
                                G.gkPhaserDepth => G.prevPhaserDepth;
                            }
                        }
                    }

                    else if(cc == 29) {
                        float norm;
                        val / 127.0 => norm;
                        
                        if(G.gkLastToggle == 8) {
                            norm => G.gkVOLpedal;
                            <<< "VOLUME PEDAL:", G.gkVOLpedal, "(CC29)" >>>;
                            if(G.gkVOLpedal != G.prevVOLpedal) {
                                G.xout.start("/vol_pedal");
                                G.xout.add(G.gkVOLpedal);
                                G.xout.send();
                                G.gkVOLpedal => G.prevVOLpedal;
                            }
                        }
                        else if(G.gkLastToggle == 10) {
                            norm => G.gkRingRate;
                            <<< "RING MOD RATE:", G.gkRingRate, "(CC29)" >>>;
                            if(G.gkRingRate != G.prevRingRate) {
                                1.25 * Math.pow(200.0, G.gkRingRate) => float mod_freq_val;
                                G.xout.start("/ar_mod_freq");
                                G.xout.add(mod_freq_val);
                                G.xout.send();
                                G.gkRingRate => G.prevRingRate;
                            }
                        }
                        else if(G.gkLastToggle == 11) {
                            norm * 24.0 - 12.0 => G.gkPitchShift;
                            <<< "PITCH SHIFT:", G.gkPitchShift, "(CC29)" >>>;
                            if(G.gkPitchShift != G.prevPitchShift) {
                                G.xout.start("/semi");
                                G.xout.add(G.gkPitchShift);
                                G.xout.send();
                                G.gkPitchShift => G.prevPitchShift;
                            }
                        }
                        else if(G.gkLastToggle == 13) {
                            norm => G.gkDelayTime;
                            <<< "DELAY TIME:", G.gkDelayTime, "(CC29)" >>>;
                            if(G.gkDelayTime != G.prevDelayTime) {
                                G.xout.start("/delay_time");
                                G.xout.add(G.gkDelayTime);
                                G.xout.send();
                                G.gkDelayTime => G.prevDelayTime;
                            }
                        }
                        else if(G.gkLastToggle == 15) {
                            norm => G.gkSpectralWah;
                            <<< "SPECTRAL WAH:", G.gkSpectralWah, "(CC29)" >>>;
                            if(G.gkSpectralWah != G.prevSpectralWah) {
                                G.xout.start("/spectral_wah");
                                G.xout.add(G.gkSpectralWah);
                                G.xout.send();
                                G.gkSpectralWah => G.prevSpectralWah;
                            }
                        }
                        else if(G.gkLastToggle == 14) {
                            norm => G.gkReverbTime;
                            <<< "REVERB TIME:", G.gkReverbTime, "(CC29)" >>>;
                            if(G.gkReverbTime != G.prevReverbTime) {
                                G.xout.start("/reverb_time");
                                G.xout.add(G.gkReverbTime);
                                G.xout.send();
                                G.gkReverbTime => G.prevReverbTime;
                            }
                        }
                    }
                    else if(cc == 27) {
                        if(G.gkGridLock == 0) {
                            val / 127.0 * (G.num_x - 1.0) => float pos;
                            pos => G.phys_pos_x;
                            if(G.synthMode == G.SYNTH_VOCAL) {
                                pos => G.vocal_pos_x;
                            }
                            <<< "X POSITION:", pos, "(CC27)" >>>;
                        }
                    }
                    else if(cc == 28) {
                        if(G.gkGridLock == 0) {
                            val / 127.0 * (G.num_y - 1.0) => float pos;
                            pos => G.phys_pos_y;
                            if(G.synthMode == G.SYNTH_VOCAL) {
                                pos => G.vocal_pos_y;
                            }
                            <<< "Y POSITION:", pos, "(CC28)" >>>;
                        }
                    }
                    else if(cc == 77 && val == 127) {
                        1 - G.gkGridLock => G.gkGridLock;
                        <<< "GRID LOCK:", G.gkGridLock, "(CC77)" >>>;
                        G.xout.start("/grid_lock");
                        G.xout.add(G.gkGridLock);
                        G.xout.send();
                    }
                    else if(cc == 68 && val == 127) {
                        <<< "MANUAL TRIGGER (CC68)" >>>;
                        float sum_prob;
                        0.0 => sum_prob;
                        for(0 => int i; i < 12; i++) {
                            sum_prob + G.note_probs[i] => sum_prob;
                        }
                        if(sum_prob > 0.0) {
                            Math.randomf() * sum_prob => float r;
                            float cum;
                            0.0 => cum;
                            float selectedFreq;
                            for(0 => int i; i < 12; i++) {
                                cum + G.note_probs[i] => cum;
                                if(r < cum) {
                                    int octaveOFFSET_total;
                                    G.octaveINIT => octaveOFFSET_total;
                                    if(Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.05) octaveOFFSET_total - 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.75) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.5) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.33) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.2) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.15) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    if(Math.randomf() < 0.1) octaveOFFSET_total + 12 => octaveOFFSET_total;
                                    G.note_pitches[i] * Math.pow(2, octaveOFFSET_total / 12.0) => selectedFreq;
                                    int midiNote;
                                    60 + i + octaveOFFSET_total => midiNote;
                                    if(G.playing[midiNote] == 0) {
                                        1 => G.playing[midiNote];
                                        spork ~ playNote(selectedFreq, midiNote);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    else if(cc == 63 && val == 127) {
                        int currentSynthModeIndex;
                        G.synthMode => currentSynthModeIndex;
                        (currentSynthModeIndex + 1) % 4 => currentSynthModeIndex;
                        currentSynthModeIndex => G.synthMode;
                        <<< "SYNTH MODE CHANGED:", getSynthModeName(G.synthMode), "(CC63)" >>>;
                        G.xout.start("/synth_mode");
                        G.xout.add(G.synthMode);
                        G.xout.send();
                    }
                    else if(cc == 67 && val == 127) {
                        int currentClassicShapeIndex;
                        G.classicWaveType => currentClassicShapeIndex;
                        (currentClassicShapeIndex + 1) % 5 => currentClassicShapeIndex;
                        currentClassicShapeIndex => G.classicWaveType;
                        string waveNames[5];
                        "SINE" => waveNames[0];
                        "TRIANGLE" => waveNames[1];
                        "SAW" => waveNames[2];
                        "PULSE" => waveNames[3];
                        "NOISE" => waveNames[4];
                        <<< "CLASSIC WAVE TYPE:", waveNames[G.classicWaveType], "(CC67)" >>>;
                        G.xout.start("/classic_wavetype");
                        G.xout.add(G.classicWaveType);
                        G.xout.send();
                    }
                }
            }
        }
        if(!had_msg) {
            1::samp => now;
        }
    }
}

spork ~ midi_listener();
while(true) { 1::second => now; }