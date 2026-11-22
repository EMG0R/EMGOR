// kweight.h — RBJ biquad (direct form 2 transposed) + ITU-R BS.1770
// K-weighting stages. shared by the load-time loudness probe (nam_loader.cpp)
// and the live dry-referenced volume compensator (csound_nam_opcode.cpp).
#pragma once

#include <cmath>

struct KwBiquad {
    double b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    double z1{0}, z2{0};

    double tick(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0; }

    // BS.1770 stage 1: +4 dB high shelf. f0/Q/G constants are the reference
    // pre-filter design values from ITU-R BS.1770 (48 kHz prototype),
    // recomputed here for the actual sample rate.
    static KwBiquad shelf(double fs) {
        KwBiquad q;
        const double G = 3.99984385397, f0 = 1681.9744509555319,
                     Q = 0.7071752369554196;
        double A  = std::pow(10.0, G / 40.0);
        double w0 = 2.0 * M_PI * f0 / fs;
        double c = std::cos(w0), s = std::sin(w0);
        double alpha = s / (2.0 * Q);
        double a0 =      (A + 1) - (A - 1) * c + 2 * std::sqrt(A) * alpha;
        q.b0 =  A * ((A + 1) + (A - 1) * c + 2 * std::sqrt(A) * alpha) / a0;
        q.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        q.b2 =  A * ((A + 1) + (A - 1) * c - 2 * std::sqrt(A) * alpha) / a0;
        q.a1 =  2 * ((A - 1) - (A + 1) * c) / a0;
        q.a2 =       ((A + 1) - (A - 1) * c - 2 * std::sqrt(A) * alpha) / a0;
        return q;
    }

    // BS.1770 stage 2: RLB high-pass, f0 38.135 Hz, Q 0.5003271
    static KwBiquad highpass(double fs) {
        KwBiquad q;
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        double w0 = 2.0 * M_PI * f0 / fs;
        double c = std::cos(w0), s = std::sin(w0);
        double alpha = s / (2.0 * Q);
        double a0 = 1 + alpha;
        q.b0 =  ((1 + c) / 2) / a0;
        q.b1 = (-(1 + c))     / a0;
        q.b2 =  ((1 + c) / 2) / a0;
        q.a1 = (-2 * c)       / a0;
        q.a2 =  (1 - alpha)   / a0;
        return q;
    }
};

// two-stage K-weighting chain for one metering channel.
struct KwChain {
    KwBiquad s1, s2;
    void init(double fs) { s1 = KwBiquad::shelf(fs); s2 = KwBiquad::highpass(fs); }
    double tick(double x) { return s2.tick(s1.tick(x)); }
    void reset() { s1.reset(); s2.reset(); }
};
