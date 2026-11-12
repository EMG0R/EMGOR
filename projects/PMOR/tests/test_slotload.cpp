// test_slotload.cpp -- max/pmor_slotload.h: a cached .aud loaded into a sampler slot makes a ch3 note audible through the SAME path pmor.core~ uses
// (Mailbox::post on the message thread, Mailbox::apply at the top of the render block, samplerPlay, pmorRenderBlock). Plus format / failure edges.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <string>
#include <vector>
#include "config.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "params.h"
#include "render.h"
#include "../max/pmor_slotload.h"
static int passes = 0, fails = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static pmorslot::Mailbox mb;
static void boot() { memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit(); pmorSchemaInit(); }
static float play(int slot, double seconds) {      // ch3 note path: apply parked loads, then trigger + render
    float in0[BLOCK] = {0}, L[BLOCK], R[BLOCK], pk = 0; int blocks = (int)(seconds * 48000 / BLOCK);
    mb.apply(); samplerPlay((uint8_t)slot, 100 / 127.f);
    for (int b = 0; b < blocks; b++) { pmorRenderBlock(in0, L, R, BLOCK); for (int i = 0; i < (int)BLOCK; i++) { float a = fmaxf(fabsf(L[i]), fabsf(R[i])); if (!(a == a)) return -1.f; if (a > pk) pk = a; } }
    return pk;
}
static std::string writeAud(const std::string& dir, const char* name, const std::vector<int16_t>& v) {
    std::string f = dir + "/" + name; FILE* o = fopen(f.c_str(), "wb"); fwrite(v.data(), 2, v.size(), o); fclose(o); return f;
}
int main() {
    char tmpl[] = "/tmp/pmorslotXXXXXX"; std::string dir = mkdtemp(tmpl);
    // --- real cached sample, read-only
    {
        std::string home = getenv("HOME") ? getenv("HOME") : "", cd = home + "/Music/PMOR_Cache", real;
        if (DIR* d = opendir(cd.c_str())) { while (dirent* e = readdir(d)) { std::string n = e->d_name; if (n.size() > 4 && n.substr(n.size() - 4) == ".aud") { std::string f = cd + "/" + n; FILE* t = fopen(f.c_str(), "rb"); long sz = 0; if (t) { fseek(t, 0, SEEK_END); sz = ftell(t); fclose(t); } if (sz > 4000 && !(sz & 1)) { real = f; break; } } } closedir(d); }
        if (real.empty()) printf("note: no real .aud in %s, real-file case skipped\n", cd.c_str());
        else {
            boot(); float before = play(3, 0.3);
            CHECK(before < 1e-6f, "empty slot is silent (%.6f)", before);
            auto r = mb.post(3, real); CHECK(r == pmorslot::OK, "real file loads (%s)", real.c_str());
            float pk = play(3, 0.5); CHECK(pk > 0.001f, "ch3 note from real cached .aud is audible: peak %.6f (%s)", pk, real.c_str());
            CHECK(smp.len[3] > 0 && smp.len[3] <= SMP_LEN, "len %u", (unsigned)smp.len[3]);
        }
    }
    // --- synthetic: format + CRC
    std::vector<int16_t> sine(4800); for (size_t i = 0; i < sine.size(); i++) sine[i] = (int16_t)(20000 * sinf(2 * 3.14159265f * 440 * i / 48000));
    std::string f = writeAud(dir, "S1_AAAA.aud", sine);
    std::vector<uint8_t> raw(sine.size() * 2); memcpy(raw.data(), sine.data(), raw.size());
    uint32_t crc = pmorslot::crc32(raw.data(), raw.size());
    CHECK(pmorslot::crc32((const uint8_t*)"123456789", 9) == 0xCBF43926u, "crc32 check value");
    boot(); CHECK(mb.post(0, f, true, crc) == pmorslot::OK, "good crc loads");
    CHECK(play(0, 0.2) > 0.01f, "sine slot audible"); CHECK(smp.len[0] == 4800, "len 4800, got %u", (unsigned)smp.len[0]);
    CHECK(fabsf(smp_buf[0][1] - (float)sine[1] / 32767.f) < 1e-7f, "decode matches firmware int16/32767");
    CHECK(mb.post(0, f, true, crc ^ 1) == pmorslot::BAD_CRC, "wrong CRC rejected"); mb.apply();
    CHECK(smp.len[0] == 0 && play(0, 0.2) < 1e-6f, "wrong CRC -> silent slot, no crash");
    CHECK(mb.post(1, dir + "/nope.aud") == pmorslot::MISSING, "missing file"); mb.apply(); CHECK(smp.len[1] == 0 && play(1, 0.1) < 1e-6f, "missing -> silent");
    std::vector<int16_t> tiny = {1000, -1000, 1000, -1000}; std::string ft = writeAud(dir, "tiny.aud", tiny);
    CHECK(mb.post(2, ft) == pmorslot::OK, "short file loads"); mb.apply(); CHECK(smp.len[2] == 4, "short len %u", (unsigned)smp.len[2]); play(2, 0.1);
    FILE* o = fopen((dir + "/odd.aud").c_str(), "wb"); fwrite("abc", 1, 3, o); fclose(o);
    CHECK(mb.post(4, dir + "/odd.aud") == pmorslot::BAD_SIZE, "odd size rejected"); mb.apply(); CHECK(smp.len[4] == 0, "odd -> empty");
    o = fopen((dir + "/zero.aud").c_str(), "wb"); fclose(o);
    CHECK(mb.post(5, dir + "/zero.aud") == pmorslot::EMPTY, "zero-length rejected");
    std::vector<int16_t> big(SMP_LEN + 5000, 5000); std::string fb = writeAud(dir, "big.aud", big);
    CHECK(mb.post(6, fb) == pmorslot::OK, "oversize loads"); mb.apply(); CHECK(smp.len[6] == SMP_LEN, "clamped to SMP_LEN, got %u", (unsigned)smp.len[6]);
    CHECK(mb.post(0, "") == pmorslot::OK, "empty path clears"); mb.apply(); CHECK(smp.len[0] == 0, "cleared");
    CHECK(mb.post(99, f) != pmorslot::OK && mb.post(-1, f) != pmorslot::OK, "bad slot index ignored");
    // resample: 24 kHz file -> 2x length; stereo -> mono
    std::vector<float> out; std::vector<int16_t> lo(2400, 8000); std::string fl = writeAud(dir, "lo.aud", lo);
    CHECK(pmorslot::loadAud(fl, false, 0, out, 24000, 1) == pmorslot::OK && out.size() == 4800, "24k -> 48k doubles length (%zu)", out.size());
    std::vector<int16_t> st(2000); for (size_t i = 0; i < st.size(); i++) st[i] = (i & 1) ? 0 : 10000; std::string fs = writeAud(dir, "st.aud", st);
    CHECK(pmorslot::loadAud(fs, false, 0, out, 48000, 2) == pmorslot::OK && out.size() == 1000 && fabsf(out[10] - 5000.f / 32767.f) < 1e-6f, "stereo averaged to mono");
    // two posts before one apply: last wins
    boot(); mb.post(7, ft); mb.post(7, f); mb.apply(); CHECK(smp.len[7] == 4800, "latest post wins");
    printf("slotload: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
