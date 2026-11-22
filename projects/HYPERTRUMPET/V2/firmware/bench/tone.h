// tone.h -- synthetic trumpet-ish test tone for the benchmark (no mic needed). Wavetable of a brass-like spectrum
// (harmonics 1..12, amplitude ~ 1/h^0.7 with a formant bump near 1.2 kHz), 5.5 Hz vibrato, 4-note Bb arpeggio with
// 40 ms attacks and 60 ms gaps, so the pitch tracker sees voiced + unvoiced and notes change.
#pragma once
#include <math.h>
struct BenchTone {
    static const int TN = 2048;
    float tab[TN + 1]; float ph = 0.f, env = 0.f, vib = 0.f; int noteSamples = 0, idx = 0; bool silent = false; float sr = 48000.f;
    void init(float s) {
        sr = s;
        for (int i = 0; i <= TN; i++) tab[i] = 0.f;
        float norm = 0.f;
        for (int h = 1; h <= 12; h++) {
            float fh = 233.f * h; float a = powf((float)h, -0.7f) * (1.f + 1.5f * expf(-((fh - 1200.f) * (fh - 1200.f)) / (2.f * 400.f * 400.f)));
            for (int i = 0; i <= TN; i++) tab[i] += a * sinf(6.2831853f * (float)h * (float)i / (float)TN);
            norm += a;
        }
        for (int i = 0; i <= TN; i++) tab[i] /= norm * 0.7f;
    }
    void process(float* out, int n) {
        static const float notes[6] = { 233.08f, 349.23f, 466.16f, 587.33f, 466.16f, 349.23f };
        const int noteLen = (int)(0.4f * sr), gap = (int)(0.06f * sr);
        for (int k = 0; k < n; k++) {
            if (silent) { out[k] = 0.f; continue; }
            if (noteSamples >= noteLen) { noteSamples = 0; idx = (idx + 1) % 6; }
            bool on = noteSamples < noteLen - gap;
            env += ((on ? 1.f : 0.f) - env) * (on ? 0.0005f : 0.003f);
            vib += 5.5f / sr; if (vib >= 1.f) vib -= 1.f;
            float vr = 1.f + 0.005f * sinf(6.2831853f * vib);          // +-8 cents (sinf per sample is fine here: ~0.5% CPU)
            ph += notes[idx] * vr / sr; if (ph >= 1.f) ph -= 1.f;
            float x = ph * (float)TN; int i0 = (int)x; float fr = x - (float)i0;
            out[k] = (tab[i0] + (tab[i0 + 1] - tab[i0]) * fr) * env * 0.45f;
            noteSamples++;
        }
    }
};
