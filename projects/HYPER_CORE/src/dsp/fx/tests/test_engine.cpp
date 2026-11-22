#include "../fx_engine.h"
#include "fx_testutil.h"
#include <chrono>
using namespace hx;
static uint64_t nowNs() { return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::vector<uint8_t> mem((size_t)HX_NSLOTS * HX_SLOT_ARENA_BYTES, 0xCD);
int main() {
    g_nowNs = nowNs;
    FxEngine e; e.init(48000.f, mem.data(), mem.size());
    int n = fxTypeCount(); printf("registry: %d effects\n", n);
    // every effect in every slot position (rotating), 4 slots at once, process, bypass toggles, swaps mid-stream
    float in[48], L[48], R[48]; long t = 0; uint32_t seed = 99;
    for (int round = 0; round < n; round++) {
        for (int s = 0; s < HX_NSLOTS; s++) { bool ok = e.setEffect(s, (round + s * 3) % n); HX_CHECK(ok, "setEffect slot %d type %d", s, (round + s * 3) % n); }
        hxt::Stats st;
        for (int b = 0; b < 600; b++) {
            for (int i = 0; i < 48; i++, t++) in[i] = 0.4f * sinf(6.2831853f * 311.f * t / 48000.f) * (0.6f + 0.4f * sinf(t * 0.0001f));
            if (b % 97 == 0) e.setBypass(b % 4, !e.bypassed(b % 4));
            e.process(in, L, R, 48); hxt::scan(st, L, 48); hxt::scan(st, R, 48);
        }
        HX_CHECK(!st.bad, "NaN in engine round %d", round);
        HX_CHECK(st.peak < 20.f, "engine peak %.2f round %d", st.peak, round);
    }
    // empty chain = passthrough to both channels
    for (int s = 0; s < HX_NSLOTS; s++) e.setEffect(s, -1);
    for (int i = 0; i < 48; i++) in[i] = 0.25f * (i & 1 ? 1 : -1);
    e.process(in, L, R, 48);
    for (int i = 0; i < 48; i++) HX_CHECK(L[i] == in[i] && R[i] == in[i], "empty chain not passthrough");
    printf("engine total ns/block %.0f\n", e.totalNs());
    printf("[engine] %s\n", hxt::failCount() ? "FAIL" : "PASS");
    return hxt::failCount() ? 1 : 0;
}
