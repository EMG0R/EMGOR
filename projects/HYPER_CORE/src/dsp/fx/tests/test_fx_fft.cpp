// test_fx_fft.cpp -- FFT round trip, known spectrum, STFT identity (not a stdSuite test).
#include "../fx_fft.h"
#include "fx_testutil.h"
using namespace hx;

struct PassS {   // identity spectral callback
    void frameBegin(Stft&) {}
    void bins(Stft& st, int k0, int k1) { for (int c = 0; c < st.nOut; c++) for (int k = k0; k < k1; k++) { st.oRe[c][k] = st.inRe[k]; st.oIm[c][k] = st.inIm[k]; } }
    void frameEnd(Stft&) {}
};

int main() {
    printf("[fft] FFT/STFT test\n");
    std::vector<uint8_t> mem(4 << 20, 0xCD); Arena a; a.bind(mem.data(), mem.size());
    Stft st; bool ok = st.init(a, 1, 2, true, 1.f); HX_CHECK(ok, "init");
    printf("  arena used %zu bytes\n", a.used);
    for (int idx = 0; idx < 3; idx++) {
        st.setSize(idx); int N = st.N;
        std::vector<float> x(N), re(N / 2 + 1), im(N / 2 + 1), y(N);
        uint32_t sd = 5; for (int i = 0; i < N; i++) x[i] = rnd01(sd) * 2.f - 1.f;
        st.rfft(x.data(), re.data(), im.data());
        // compare with direct DFT (double) at a few bins
        double maxe = 0; for (int k : {0, 1, 7, N / 4, N / 2 - 1, N / 2}) {
            double sr = 0, si = 0; for (int n = 0; n < N; n++) { double ang = 6.283185307179586 * (double)k * n / N; sr += x[n] * cos(ang); si -= x[n] * sin(ang); }
            maxe = fmax(maxe, fmax(fabs(sr - re[k]), fabs(si - im[k]))); }
        HX_CHECK(maxe < 2e-3, "N=%d DFT mismatch %.2e", N, maxe);
        st.irfft(re.data(), im.data(), y.data());
        double e = 0; for (int i = 0; i < N; i++) e = fmax(e, fabs(y[i] - x[i]));
        printf("  N=%d roundtrip max err %.2e, DFT err %.2e\n", N, e, maxe);
        HX_CHECK(e < 1e-5, "N=%d round trip err %.2e", N, e);
        // single-bin sine: exactly bin 37.0 -> peak at 37
        for (int i = 0; i < N; i++) x[i] = sinf(6.2831853f * 37.f * i / N);
        st.rfft(x.data(), re.data(), im.data());
        int pk = 0; float pm = 0; for (int k = 0; k <= N / 2; k++) { float m = hypotf(re[k], im[k]); if (m > pm) { pm = m; pk = k; } }
        HX_CHECK(pk == 37, "N=%d sine peak at %d", N, pk);
        HX_CHECK(fabsf(pm - N / 2) < 0.01f * N / 2, "N=%d sine peak mag %.2f (want %d)", N, pm, N / 2);
    }
    // STFT identity, sliced and unsliced, odd block sizes
    for (int sliced = 0; sliced < 2; sliced++) for (int idx = 0; idx < 3; idx++) for (int bs : {48, 17, 128}) {
        std::vector<uint8_t> m2(4 << 20, 0xCD); Arena a2; a2.bind(m2.data(), m2.size());
        Stft s; s.init(a2, idx, 2, sliced != 0, 1.f); PassS ps;
        int total = 3 * s.N + 4000; std::vector<float> in(total), o0(total), o1(total); uint32_t sd = 9;
        for (int i = 0; i < total; i++) in[i] = 0.7f * (rnd01(sd) * 2.f - 1.f);
        for (int p = 0; p < total; p += bs) { int n = (total - p) < bs ? (total - p) : bs; float* outs[2] = {o0.data() + p, o1.data() + p}; s.process(in.data() + p, outs, n, ps); }
        int Lt = s.latency(); double err = 0; for (int i = Lt; i < total; i++) err = fmax(err, fmax(fabs(o0[i] - in[i - Lt]), fabs(o1[i] - in[i - Lt])));
        printf("  STFT identity sliced=%d N=%d bs=%d latency=%d err %.2e\n", sliced, s.N, bs, Lt, err);
        HX_CHECK(err < 1e-4, "identity err %.2e sliced=%d N=%d bs=%d", err, sliced, s.N, bs);
    }
    printf("[fft] %s (%d failures)\n", hxt::failCount() ? "FAIL" : "PASS", hxt::failCount());
    return hxt::failCount() ? 1 : 0;
}
