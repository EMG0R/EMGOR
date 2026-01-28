// formantTimbre.ck
@import "globals.ck"

global Gain preSpectralL;
global Gain preSpectralR;
global Gain postSpectralL;
global Gain postSpectralR;
global Gain spectralGainL;
global Gain spectralGainR;
global Gain directSpectralL;
global Gain directSpectralR;

global FFT fftL;
global FFT fftR;
global IFFT ifftL;
global IFFT ifftR;

int FFT_SIZE;
fftL.size() => FFT_SIZE;

FFT_SIZE / 4 => int HOP_SIZE;

Windowing.hann(FFT_SIZE/2) => fftL.window => fftR.window => ifftL.window => ifftR.window;

complex ZL[FFT_SIZE/2];
complex ZR[FFT_SIZE/2];

1::second / 1::samp => float SR;

<<< "Formant WAH activated and processing audio" >>>;

float spectral_on_smooth;
0.0 => spectral_on_smooth;
float smooth_wah;
G.gkSpectralWah => smooth_wah;

fun float mirrorIndex(float idx, float max_idx) {
    while (idx < 0) {
        -idx => idx;
    }
    while (idx > max_idx) {
        2 * max_idx - idx => idx;
    }
    return idx;
}

while (true) {
    float hop_seconds;
    HOP_SIZE * 1.0 / SR => hop_seconds;
    Math.exp(-1.0 / (hop_seconds / G.gkPortTime)) => float coeff;

    1.0 - G.gkSpectralBypass => float target_on;
    spectral_on_smooth * coeff + target_on * (1.0 - coeff) => spectral_on_smooth;
    spectral_on_smooth => G.gkSpectralOnSmooth;

    G.gkSpectralWah => float target_wah;
    smooth_wah * coeff + target_wah * (1.0 - coeff) => smooth_wah;

    (1.0 - spectral_on_smooth) => directSpectralL.gain => directSpectralR.gain;
    spectral_on_smooth => spectralGainL.gain => spectralGainR.gain;

    Math.pow(smooth_wah, 1.3) => float effective_wah;

    Math.log(0.504) => float log_min;
    Math.log(10.0) => float log_max;
    Math.exp(log_min + (log_max - log_min) * effective_wah) => float ratio;

    fftL.upchuck();
    fftR.upchuck();

    int S;
    FFT_SIZE / 2 => S;
    float max_idx;
    S - 1.0 => max_idx;

    8.0 => float group_size;

    float new_magsL[S];
    float orig_energyL;
    0.0 => orig_energyL;
    float new_energyL;
    0.0 => new_energyL;

    for( int i; i < S; i++ )
    {
        polar origL;
        fftL.cval(i) $ polar => origL;
        origL.mag * origL.mag +=> orig_energyL;

        float src;
        i / ratio => src;
        mirrorIndex(src, max_idx) => src;

        Math.floor(src / group_size) * group_size => float grouped_src;
        grouped_src $ int => int s1;

        float new_magL;
        0.0 => new_magL;
        if( s1 >= 0 && s1 < S )
        {
            (fftL.cval(s1) $ polar).mag => new_magL;
        }

        new_magL => new_magsL[i];
        new_magL * new_magL +=> new_energyL;
    }

    float scaleL;
    if( new_energyL > 0.0 )
    {
        Math.sqrt( orig_energyL / new_energyL ) => scaleL;
    }
    else
    {
        1.0 => scaleL;
    }

    for( int i; i < S; i++ )
    {
        polar origL;
        fftL.cval(i) $ polar => origL;
        float adj_magL;
        new_magsL[i] * scaleL => adj_magL;

        if( origL.mag > 1e-10 )
        {
            (adj_magL / origL.mag) * fftL.cval(i) => ZL[i];
        }
        else
        {
            #(0,0) => ZL[i];
        }
    }

    float new_magsR[S];
    float orig_energyR;
    0.0 => orig_energyR;
    float new_energyR;
    0.0 => new_energyR;

    for( int i; i < S; i++ )
    {
        polar origR;
        fftR.cval(i) $ polar => origR;
        origR.mag * origR.mag +=> orig_energyR;

        float src;
        i / ratio => src;
        mirrorIndex(src, max_idx) => src;

        Math.floor(src / group_size) * group_size => float grouped_src;
        grouped_src $ int => int s1;

        float new_magR;
        0.0 => new_magR;
        if( s1 >= 0 && s1 < S )
        {
            (fftR.cval(s1) $ polar).mag => new_magR;
        }

        new_magR => new_magsR[i];
        new_magR * new_magR +=> new_energyR;
    }

    float scaleR;
    if( new_energyR > 0.0 )
    {
        Math.sqrt( orig_energyR / new_energyR ) => scaleR;
    }
    else
    {
        1.0 => scaleR;
    }

    for( int i; i < S; i++ )
    {
        polar origR;
        fftR.cval(i) $ polar => origR;
        float adj_magR;
        new_magsR[i] * scaleR => adj_magR;

        if( origR.mag > 1e-10 )
        {
            (adj_magR / origR.mag) * fftR.cval(i) => ZR[i];
        }
        else
        {
            #(0,0) => ZR[i];
        }
    }

    ifftL.transform( ZL );
    ifftR.transform( ZR );

    HOP_SIZE::samp => now;
}