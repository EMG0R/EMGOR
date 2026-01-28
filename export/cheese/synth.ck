@import "globals.ck"
G.init();

global Gain volGainL;
global Gain volGainR;

// Vocal Synth Class - manages the vocal synthesis architecture
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
    
    fun void trigger() {
        // Trigger the appropriate syllable based on currentSyllable
        if (currentSyllable == G.VOCAL_CHUHH) {
            spork ~ doChUhh();
        } else if (currentSyllable == G.VOCAL_KAYEH) {
            spork ~ doKayEh();
        } else if (currentSyllable == G.VOCAL_HOH) {
            spork ~ doHOh();
        } else if (currentSyllable == G.VOCAL_DER) {
            spork ~ doDEr();
        } else if (currentSyllable == G.VOCAL_PHII) {
            spork ~ doPhiI();
        } else if (currentSyllable == G.VOCAL_BETAY) {
            spork ~ doBetaY();
        } else if (currentSyllable == G.VOCAL_THOE) {
            spork ~ doThOe();
        } else if (currentSyllable == G.VOCAL_DHAA) {
            spork ~ doDhAa();
        } else if (currentSyllable == G.VOCAL_KHOO) {
            spork ~ doKhOo();
        } else if (currentSyllable == G.VOCAL_GHUU) {
            spork ~ doGhUu();
        } else if (currentSyllable == G.VOCAL_CHIU) {
            spork ~ doChiU();
        } else if (currentSyllable == G.VOCAL_RHAE) {
            spork ~ doRhAe();
        } else if (currentSyllable == G.VOCAL_HHAO) {
            spork ~ doHhAo();
        } else if (currentSyllable == G.VOCAL_PHARA) {
            spork ~ doPharA();
        }
    }
    
    // Syllable functions (consonant + vowel combinations)
    fun void doChUhh() {
        // Ch consonant
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
        
        // Uhh vowel
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
        // Kay consonant
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
        
        // Eh vowel
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
    
    fun void doHOh() {
        // H consonant
        0.03 => ne.time;
        530.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1840.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2480.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // Oh vowel
        0.1 => ie.time;
        0.0 => n.gain;
        400.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        850.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2410.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.2::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doDEr() {
        // D consonant
        0.005 => ne.time;
        0.007 => n.gain;
        200.0 => r[0].freq; 0.99 => r[0].radius; 0.7 => r[0].gain;
        2200.0 => r[1].freq; 0.99 => r[1].radius; 1.0 => r[1].gain;
        3600.0 => r[2].freq; 0.99 => r[2].radius; 0.7 => r[2].gain;
        0.0 => i.gain;
        1 => ne.keyOn;
        0.005::second => now;
        1 => ne.keyOff;
        0.01 => ne.time;
        0.01::second => now;
        
        // Er vowel
        0.1 => ie.time;
        0.0 => n.gain;
        490.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1350.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        1690.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.2::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doPhiI() {
        // Phi consonant
        0.03 => ne.time;
        250.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1200.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2600.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // i vowel
        0.1 => ie.time;
        0.0 => n.gain;
        240.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        2400.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        3000.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doBetaY() {
        // Beta consonant
        0.03 => ne.time;
        250.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1100.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // y vowel
        0.1 => ie.time;
        0.0 => n.gain;
        235.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        2100.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2800.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doThOe() {
        // Th consonant
        0.03 => ne.time;
        300.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1500.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        5000.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // oe vowel
        0.1 => ie.time;
        0.0 => n.gain;
        370.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1900.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2600.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doDhAa() {
        // Dh consonant
        0.03 => ne.time;
        300.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1400.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        4500.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // aa vowel
        0.1 => ie.time;
        0.0 => n.gain;
        750.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        940.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doKhOo() {
        // Kh consonant
        0.03 => ne.time;
        400.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1200.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2600.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // oo vowel
        0.1 => ie.time;
        0.0 => n.gain;
        500.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        700.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doGhUu() {
        // Gh consonant
        0.03 => ne.time;
        400.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1100.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // uu vowel
        0.1 => ie.time;
        0.0 => n.gain;
        300.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1390.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2800.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doChiU() {
        // Chi consonant
        0.03 => ne.time;
        500.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1000.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2200.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // u vowel
        0.1 => ie.time;
        0.0 => n.gain;
        250.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        595.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doRhAe() {
        // Rh consonant
        0.03 => ne.time;
        500.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        900.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2100.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // ae vowel
        0.1 => ie.time;
        0.0 => n.gain;
        820.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1530.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doHhAo() {
        // Hh consonant
        0.03 => ne.time;
        700.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1300.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        2000.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // ao vowel
        0.1 => ie.time;
        0.0 => n.gain;
        700.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        760.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
        1 => notDone;
        spork ~ doimpulse();
        0.4 => i.gain;
        1 => ie.keyOn;
        0.15::second => now;
        1 => ie.keyOff;
        0.1::second => now;
        0 => notDone;
    }
    
    fun void doPharA() {
        // Phar consonant
        0.03 => ne.time;
        700.0 => r[0].freq; 0.99 => r[0].radius; 1.0 => r[0].gain;
        1200.0 => r[1].freq; 0.99 => r[1].radius; 0.7 => r[1].gain;
        1900.0 => r[2].freq; 0.99 => r[2].radius; 0.8 => r[2].gain;
        0.0 => i.gain;
        0.02 => n.gain;
        1 => ne.keyOn;
        0.03::second => now;
        1 => ne.keyOff;
        0.03::second => now;
        
        // a vowel
        0.1 => ie.time;
        0.0 => n.gain;
        850.0 * (baseFreq/440.0) => r[0].freq; 0.995 => r[0].radius; 1.0 => r[0].gain;
        1610.0 * (baseFreq/440.0) => r[1].freq; 0.995 => r[1].radius; 0.5 => r[1].gain;
        2500.0 => r[2].freq; 0.99 => r[2].radius; 0.2 => r[2].gain;
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
            genHarmonicTable(x_frac, y_frac) @=> float temp[];
            for (0 => int s; s < G.table_size; s++) {
                temp[s] => G.wavetables[x][y][s];
            }
        }
    }
}

genWavetables();

dur beat;
(30.0 / G.bpm)::second => beat;
dur shortDur;
2.4 * beat => shortDur;

fun void modulate(UGen osc, LPF lpf, float baseNote, dur sustainDur, dur releaseDur, int useNoise, int physModel, int vocalSyllable) {
    dur pitchATTACK;
    G.pitchATTACK_seconds::second => pitchATTACK;
    dur pitchDECAY;
    G.pitchDECAY_seconds::second => pitchDECAY;
    float pitchSustainLevel;
    G.pitchSUSTAIN => pitchSustainLevel;
    dur pitchRELEASE;
    G.pitchRELEASE_seconds::second => pitchRELEASE;
    
    dur filterATTACK;
    G.filterATTACK_seconds::second => filterATTACK;
    dur filterDECAY;
    G.filterDECAY_seconds::second => filterDECAY;
    float filterSustainLevel;
    G.filterSUSTAIN => filterSustainLevel;
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
    dur pitchAttackTime;
    pitchATTACK => pitchAttackTime;
    dur pitchDecayTime;
    pitchDECAY => pitchDecayTime;
    dur pitchReleaseTime;
    pitchRELEASE => pitchReleaseTime;
    dur pitchFullSustainStart;
    pitchAttackTime + pitchDecayTime => pitchFullSustainStart;
    
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
    dur filterReleaseTime;
    filterRELEASE => filterReleaseTime;
    dur filterFullSustainStart;
    filterAttackTime + filterDecayTime => filterFullSustainStart;
    
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
        G.pitch_offset_start => current_offset;
        if (currentTime >= keyOffTime) {
            G.pitch_offset_end => current_offset;
        }
        float pitch_mod;
        current_offset * (1.0 - pitchEnv) => pitch_mod;
        
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
        
        if (G.synthMode == G.SYNTH_WAVETABLE) {
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
        } else if (G.synthMode == G.SYNTH_VOCAL) {
            (osc $ VocalSynth).freq(modFund);
            // Update syllable position based on modulation
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
        }
        50::samp => now;
    }
}

fun void playNote(float freq, int midi) {
    dur noteATTACK;
    G.noteATTACK_seconds::second => noteATTACK;
    dur noteDECAY;
    G.noteDECAY_seconds::second => noteDECAY;
    float localNoteSUSTAIN;
    G.noteSUSTAIN => localNoteSUSTAIN;
    dur noteRELEASE;
    G.noteRELEASE_seconds::second => noteRELEASE;
    
    ADSR noteEnv;
    noteEnv.set(noteATTACK, noteDECAY, localNoteSUSTAIN, noteRELEASE);
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
    
    if (G.synthMode == G.SYNTH_WAVETABLE) {
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
            G.pulseWidth => po.width;
        } else if (G.classicWaveType == G.CLASSIC_NSE) {
            Noise no => lpf;
            no @=> osc;
            no.gain(G.gainScale);
        }
    } else if (G.synthMode == G.SYNTH_PHYSMOD) {
        if (currentPhysModel == G.PHYS_BAND) {
            BandedWG bwg => lpf;
            bwg.preset(Math.random2(0, 3));
            bwg.bowPressure(Math.random2f(0.0, 1.0));
            bwg.bowMotion(Math.random2f(0.0, 1.0));
            bwg.bowRate(0.5);
            bwg.freq(freq);
            bwg @=> osc;
        } else if (currentPhysModel == G.PHYS_BOTL) {
            BlowBotl bb => lpf;
            bb.noiseGain(Math.random2f(0.0, 0.2));
            bb.freq(freq);
            bb @=> osc;
        } else if (currentPhysModel == G.PHYS_BHOL) {
            BlowHole bh => lpf;
            bh.reed(Math.random2f(0.0, 1.0));
            bh.freq(freq);
            bh @=> osc;
        } else if (currentPhysModel == G.PHYS_BOWD) {
            Bowed bw => lpf;
            bw.bowPressure(Math.random2f(0.0, 1.0));
            bw.freq(freq);
            bw @=> osc;
        } else if (currentPhysModel == G.PHYS_BRAS) {
            Brass br => lpf;
            br.lip(Math.random2f(0.0, 1.0));
            br.freq(freq);
            br @=> osc;
        } else if (currentPhysModel == G.PHYS_CLAR) {
            Clarinet cl => lpf;
            cl.reed(Math.random2f(0.0, 1.0));
            cl.freq(freq);
            cl @=> osc;
        } else if (currentPhysModel == G.PHYS_FLUT) {
            Flute fl => lpf;
            fl.jetDelay(Math.random2f(0.0, 1.0));
            fl.freq(freq);
            fl @=> osc;
        } else if (currentPhysModel == G.PHYS_MAND) {
            Mandolin md => lpf;
            md.bodySize(Math.random2f(0.5, 1.0));
            md.freq(freq);
            md @=> osc;
        } else if (currentPhysModel == G.PHYS_MODA) {
            ModalBar mb => lpf;
            mb.preset(Math.random2(0, 8));
            mb.freq(freq);
            mb @=> osc;
        } else if (currentPhysModel == G.PHYS_SAXO) {
            Saxofony sx => lpf;
            sx.stiffness(Math.random2f(0.0, 1.0));
            sx.freq(freq);
            sx @=> osc;
        } else if (currentPhysModel == G.PHYS_SHKR) {
            Shakers sh => lpf;
            sh.preset(Math.random2(0, 22));
            sh @=> osc;
        } else if (currentPhysModel == G.PHYS_SITR) {
            Sitar st => lpf;
            st.freq(freq);
            st @=> osc;
        } else if (currentPhysModel == G.PHYS_STIF) {
            StifKarp sk => lpf;
            sk.freq(freq);
            sk @=> osc;
        } else if (currentPhysModel == G.PHYS_VOIC) {
            VoicForm vf => lpf;
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
            vf.phoneme(phonemes[Math.random2(0, 11)]);
            vf.freq(freq);
            vf @=> osc;
        }
        osc.gain(G.gainScale * 4.0);
    } else if (G.synthMode == G.SYNTH_VOCAL) {
        VocalSynth vs;
        vs.vocalGain => lpf;
        vs @=> osc;
        vs.freq(freq);
        vs.setSyllable(currentVocalSyllable);
        vs.trigger();
        vs.vocalGain.gain(G.gainScale * 2.0);
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
    
    // Cleanup
    if (G.synthMode == G.SYNTH_VOCAL) {
        (osc $ VocalSynth).vocalGain =< lpf;
    }
    lpf =< noteEnv;
    noteEnv =< volGainL;
    noteEnv =< volGainR;
    0 => G.playing[midi];
}

fun void sequencer() {
    while (true) {
        if (Math.randomf() < G.UI_NOTEprob) {
            float sum_prob;
            0.0 => sum_prob;
            for (0 => int i; i < 12; i++) {
                sum_prob + G.note_probs[i] => sum_prob;
            }
            if (sum_prob > 0.0) {
                Math.randomf() * sum_prob => float r;
                float cum;
                0.0 => cum;
                for (0 => int i; i < 12; i++) {
                    cum + G.note_probs[i] => cum;
                    if (r < cum) {
                        int octaveOFFSET_total;
                        G.octaveINIT => octaveOFFSET_total;
                        if (Math.randomf() < 0.1) octaveOFFSET_total - 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.05) octaveOFFSET_total - 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.75) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.5) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.33) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.2) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.15) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        if (Math.randomf() < 0.1) octaveOFFSET_total + 12 => octaveOFFSET_total;
                        
                        float selectedFreq;
                        G.note_pitches[i] * Math.pow(2, octaveOFFSET_total / 12.0) => selectedFreq;
                        int midiNote;
                        60 + i + octaveOFFSET_total => midiNote;
                        if (G.playing[midiNote] == 0) {
                            1 => G.playing[midiNote];
                            spork ~ playNote(selectedFreq, midiNote);
                        }
                        break;
                    }
                }
            }
        }
        (30.0 / G.bpm)::second => beat;
        2.4 * beat => shortDur;
        beat => now;
    }
}

spork ~ sequencer();

<<< "Synth running with VOCAL synthesis - routed through FX chain via volGainL/R" >>>;
<<< "Press button to cycle through synth modes: WAVETABLE -> CLASSIC -> PHYSMOD -> VOCAL" >>>;

while(true) {
    1::second => now;
}