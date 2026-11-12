// test_engines.cpp -- drum engines (KICK SNARE HAT PERC): presets, depth knobs, generative FM, idle cost.
//   test_engines            run the checks
//   test_engines measure    print peak / rms150 per preset (used to level-match new presets)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "params.h"
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

static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

static void boot() {
    memset(&g, 0, sizeof(g));
    clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
}
struct Hit { std::vector<float> L, R; float peak = 0, rms150 = 0; bool finite = true; };
static Hit hit(int grp, int sound, float seconds = 2.0f, float vel = 1.f) {
    Hit h; uint8_t pad = drumGroupFirstPad((uint8_t)grp);
    drums.pad[pad].sound = (uint8_t)sound; drums.pad[pad].length = 0.5f; drums.pad[pad].pitch = 0; drums.pad[pad].tone = 0.5f;
    drumsTrigger(pad, vel);
    static float L[BLOCK], R[BLOCK];
    int nb = (int)(seconds * SR / BLOCK); double sq = 0; int sn = 0;
    for (int b = 0; b < nb; b++) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        drumsProcess(L, R, BLOCK);
        for (int i = 0; i < BLOCK; i++) {
            if (!isfinite(L[i]) || !isfinite(R[i])) h.finite = false;
            float m = fabsf(L[i]) > fabsf(R[i]) ? fabsf(L[i]) : fabsf(R[i]); if (m > h.peak) h.peak = m;
            if (b * BLOCK + i < (int)(0.15f * SR)) { sq += (double)L[i] * L[i] + (double)R[i] * R[i]; sn += 2; }
            h.L.push_back(L[i]); h.R.push_back(R[i]);
        }
    }
    h.rms150 = (float)sqrt(sq / (sn ? sn : 1));
    return h;
}
static bool same(const Hit& a, const Hit& b) { return a.L == b.L && a.R == b.R; }


struct SynM { float peak, att_ms, sus_ratio, bright, rel_ms; bool fin; };
// note on at 0, off at 1.5 s, run to 5 s. att = time to 60 % of the pre-release peak; sus = rms(1.0-1.5 s)/rms(first 150 ms after the peak);
// bright = RMS of the first difference / RMS (high = bright); rel = time after note-off to fall under -40 dB of the peak.
static SynM synAnalyze(int algo, int pre, int pad = 8) {
    SynM m{}; m.fin = true;
    memset(&g, 0, sizeof(g));
    clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    syn.algo = (uint8_t)algo; synLoadPreset((uint8_t)algo, (uint8_t)pre);
    seq.lane[ENG_SYNTH].polyphony = 1;
    std::vector<float> env; static float L[BLOCK], R[BLOCK];
    double dsq = 0, sq = 0; int nb = (int)(5.0f * SR / BLOCK), offb = (int)(1.5f * SR / BLOCK);
    synthNoteOn((uint8_t)pad, 1.f);
    float prev = 0;
    for (int b = 0; b < nb; b++) {
        if (b == offb) synthNoteOff((uint8_t)pad);
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); synthProcess(L, R, BLOCK);
        float e = 0;
        for (int i = 0; i < BLOCK; i++) { float x = 0.5f * (L[i] + R[i]); if (!isfinite(x)) m.fin = false; e += x * x; if (b * BLOCK < (int)SR) { dsq += (double)(x - prev) * (x - prev); sq += (double)x * x; } prev = x; }
        env.push_back(sqrtf(e / BLOCK));
    }
    float pk = 0; int pkb = 0; for (int b = 0; b < offb; b++) if (env[b] > pk) { pk = env[b]; pkb = b; }
    m.peak = pk;
    int ab = 0; while (ab < offb && env[ab] < 0.6f * pk) ab++;
    m.att_ms = ab * BLOCK * 1000.f / SR;
    double s1 = 0, s2 = 0; int n1 = 0, n2 = 0;
    for (int b = pkb; b < pkb + (int)(0.15f * SR / BLOCK) && b < offb; b++) { s1 += env[b] * env[b]; n1++; }
    for (int b = (int)(1.0f * SR / BLOCK); b < offb; b++) { s2 += env[b] * env[b]; n2++; }
    m.sus_ratio = (n1 && n2 && s1 > 0) ? sqrtf((float)(s2 / n2) / (float)(s1 / n1)) : 0;
    m.bright = sq > 0 ? (float)sqrt(dsq / sq) : 0;
    float ref = env[offb - 1] > 1e-6f ? env[offb - 1] : pk; int rb = offb; float thr = ref * 0.01f;
    while (rb < nb && env[rb] > thr) rb++;
    m.rel_ms = (rb - offb) * BLOCK * 1000.f / SR;
    return m;
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "synth")) {
        for (int a = 0; a < ALG_COUNT; a++) for (int p = 0; p < SYN_PRESETS; p++) { SynM m = synAnalyze(a, p);
            SynM lo = synAnalyze(a, p, 1), hi = synAnalyze(a, p, 15);
            printf("%-6s %-5s peak %.3f att %6.0fms sus %.2f bright %.3f (lo %.3f hi %.3f) rel %6.0fms\n", SYN_ALGO_NAMES[a], SYN_PRESET_NAMES[p], m.peak, m.att_ms, m.sus_ratio, m.bright, lo.bright, hi.bright, m.rel_ms); }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "measure")) {
        boot();
        for (int gi = 0; gi < DG_COUNT; gi++) {
            double sum = 0; int n = 0;
            for (int s = 0; s < DRUM_CLASSIC_SOUNDS; s++) { boot(); Hit h = hit(gi, s); sum += h.rms150; n++; }
            printf("== %s classic mean rms150 %.4f\n", DRUM_GROUP_NAMES[gi], sum / n);
            for (int s = DRUM_CLASSIC_SOUNDS; s < DRUM_SOUNDS; s++) { boot(); Hit h = hit(gi, s); printf("%d/%2d %-6s peak %.3f rms150 %.4f  want_gain_x %.3f\n", gi, s, DRUM_SOUND_NAMES[gi][s], h.peak, h.rms150, (float)(sum / n) / (h.rms150 + 1e-9f)); }
        }
        return 0;
    }

    // ---- LENGTH scales every kind's decay (was a no-op on 53 of 96); REVERSE on every preset ends (LEGACY used to sustain forever)
    for (int gi = 0; gi < DG_COUNT; gi++) for (int s = 0; s < DRUM_SOUNDS; s++) {
        uint8_t pad = drumGroupFirstPad((uint8_t)gi);
        boot(); Hit d = hit(gi, s, 1.5f);
        boot(); drums.pad[pad].length = 0.5f; Hit d2 = hit(gi, s, 1.5f); CHECK(same(d, d2), "length default not reproducible %d/%d", gi, s);
        boot(); { Hit sh; uint8_t pp = pad; drums.pad[pp].sound = (uint8_t)s; drums.pad[pp].length = 0.1f; drums.pad[pp].pitch = 0; drums.pad[pp].tone = 0.5f; drumsTrigger(pp, 1.f);
          static float L[BLOCK], R[BLOCK]; for (int b = 0; b < (int)(1.5f * SR / BLOCK); b++) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); drumsProcess(L, R, BLOCK); for (int i = 0; i < BLOCK; i++) { sh.L.push_back(L[i]); sh.R.push_back(R[i]); } }
          CHECK(!same(d, sh), "LENGTH has no effect on preset %d/%d (%s)", gi, s, DRUM_SOUND_NAMES[gi][s]); }
        boot(); drums.pad[pad].reverse = true; hit(gi, s, 8.0f); CHECK(drumsActiveVoices() == 0, "REVERSE never ends on preset %d/%d (%s)", gi, s, DRUM_SOUND_NAMES[gi][s]);
    }
    // ---- every preset of every engine: named, finite, audible, not clipping hard, and the voice goes idle
    for (int gi = 0; gi < DG_COUNT; gi++) {
        for (int s = 0; s < DRUM_SOUNDS; s++) {
            boot();
            CHECK(DRUM_SOUND_NAMES[gi][s] && DRUM_SOUND_NAMES[gi][s][0] && strlen(DRUM_SOUND_NAMES[gi][s]) <= 5, "preset %d/%d name", gi, s);
            Hit h = hit(gi, s, 6.0f);
            CHECK(h.finite, "preset %d/%d (%s): non-finite", gi, s, DRUM_SOUND_NAMES[gi][s]);
            CHECK(h.peak > 0.05f, "preset %d/%d (%s): silent (peak %.4f)", gi, s, DRUM_SOUND_NAMES[gi][s], h.peak);
            CHECK(h.peak < 2.0f, "preset %d/%d (%s): peak %.3f too hot", gi, s, DRUM_SOUND_NAMES[gi][s], h.peak);
            CHECK(drumsActiveVoices() == 0, "preset %d/%d (%s): voice still running after 6 s (idle cost)", gi, s, DRUM_SOUND_NAMES[gi][s]);
        }
    }
    // ---- idle engines cost nothing: no triggers => no active voices, silent output
    boot();
    { static float L[BLOCK], R[BLOCK]; memset(L, 0, sizeof L); memset(R, 0, sizeof R); drumsProcess(L, R, BLOCK);
      CHECK(drumsActiveVoices() == 0, "idle engine has active voices"); for (int i = 0; i < BLOCK; i++) CHECK(L[i] == 0.f && R[i] == 0.f, "idle engine output nonzero"); }

    // ---- depth knobs: neutral (0) is bit-identical to a boot with no knob; non-zero changes the sound
    for (int gi = 0; gi < DG_COUNT; gi++) {
        boot(); Hit base = hit(gi, 0, 1.0f);
        for (int k = 0; k < DENG_PARAMS; k++) {
            boot(); drums.eng[gi].x[k] = 0.f; Hit zero = hit(gi, 0, 1.0f);
            CHECK(same(base, zero), "engine %d knob %d at 0 changed the classic sound", gi, k);
            boot(); drums.eng[gi].x[k] = 0.6f; Hit on = hit(gi, 0, 1.0f);
            CHECK(on.finite && !same(base, on), "engine %d knob %d (%s) has no effect", gi, k, DRUM_ENGINE_PARAM_NAMES[gi][k]);
            CHECK(on.peak < 3.0f, "engine %d knob %d peak %.2f", gi, k, on.peak);
        }
    }
    // ---- knobs are in the schema with the right ranges (ids PD_DRUMS 272 + g*8 + k)
    boot();
    for (int gi = 0; gi < DG_COUNT; gi++) for (int k = 0; k < DENG_PARAMS; k++) {
        const PmorParamDesc* d = pmorSchemaFind(pmorParamId(PD_DRUMS, (uint16_t)(272 + gi * 8 + k)));
        CHECK(d != nullptr, "schema: engine %d knob %d missing", gi, k);
        if (d) CHECK(!strcmp(d->name, DRUM_ENGINE_PARAM_NAMES[gi][k]) && d->min == 0.f && d->max == 1.f && d->def == 0.f, "schema: engine %d knob %d wrong", gi, k);
    }
    // ---- generative FM: deterministic from the seed, different on every hit, chaos 0 hits still re-roll the ratio
    {
        boot(); Hit a1 = hit(DG_PERC, 16, 1.0f); Hit a2 = hit(DG_PERC, 16, 1.0f);
        boot(); Hit b1 = hit(DG_PERC, 16, 1.0f); Hit b2 = hit(DG_PERC, 16, 1.0f);
        CHECK(same(a1, b1) && same(a2, b2), "generative FM not reproducible from the seed");
        CHECK(!same(a1, a2), "generative FM: two hits identical");
        boot(); Hit c = hit(DG_PERC, 18, 1.0f); CHECK(c.finite && c.peak > 0.05f, "GEN3 broken");
    }
    // ---- synth: every algorithm x every factory preset renders a finite, audible, bounded chord; INIT == boot sound
    for (int a = 0; a < ALG_COUNT; a++) for (int pr = 0; pr < SYN_PRESETS; pr++) {
        boot(); syn.algo = (uint8_t)a; synLoadPreset((uint8_t)a, (uint8_t)pr);
        if (pr == 0) { SynthState ref; (void)ref; }
        synthNoteOn(12, 1.f); synthNoteOn(14, 1.f);
        static float L[BLOCK], R[BLOCK]; float pk = 0; bool fin = true;
        for (int b = 0; b < 1500; b++) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); synthProcess(L, R, BLOCK);
            for (int i = 0; i < BLOCK; i++) { if (!isfinite(L[i]) || !isfinite(R[i])) fin = false; float m = fabsf(L[i]) > fabsf(R[i]) ? fabsf(L[i]) : fabsf(R[i]); if (m > pk) pk = m; } }
        synthAllOff();
        CHECK(fin, "synth %s/%s non-finite", SYN_ALGO_NAMES[a], SYN_PRESET_NAMES[pr]);
        CHECK(pk > 0.01f && pk < 3.f, "synth %s/%s peak %.3f", SYN_ALGO_NAMES[a], SYN_PRESET_NAMES[pr], pk);
    }
    { boot(); SynPreset before = syn.preset[ALG_FM]; synLoadPreset(ALG_FM, 2); synLoadPreset(ALG_FM, 0);
      CHECK(!memcmp(&before, &syn.preset[ALG_FM], sizeof before), "synth INIT preset does not restore the boot sound"); }
    // ---- per-algorithm presets sound like their names (measured: attack / sustain / release / brightness / stability)
    for (int a = 0; a < ALG_COUNT; a++) {
        if (a == ALG_THIEF) continue;                               // random per-note patch: preset envelopes/filter/knobs are ignored by design
        bool ks = (a == ALG_PLUCK);                                 // a string rings by itself: slow-attack / sustained claims do not apply
        SynM pl = synAnalyze(a, 1), pd = synAnalyze(a, 2), ld = synAnalyze(a, 3), bs = synAnalyze(a, 4);
        const char* nm = SYN_ALGO_NAMES[a];
        for (int pr = 1; pr < SYN_PRESETS; pr++) for (int pad = 1; pad <= 15; pad += 7) {
            SynM m = synAnalyze(a, pr, pad);
            CHECK(m.fin && m.bright < 1.2f, "%s/%s pad %d unstable/aliased (bright %.2f)", nm, SYN_PRESET_NAMES[pr], pad, m.bright);
        }
        CHECK(pl.att_ms < 30.f && pl.sus_ratio < 0.35f, "%s PLUCK not plucky (att %.0f sus %.2f)", nm, pl.att_ms, pl.sus_ratio);
        if (!ks) CHECK(pd.att_ms >= 100.f, "%s PAD attack only %.0f ms", nm, pd.att_ms);
        CHECK(pd.rel_ms >= 800.f, "%s PAD release only %.0f ms", nm, pd.rel_ms);
        CHECK(ld.att_ms < 80.f, "%s LEAD attack %.0f ms", nm, ld.att_ms);
        if (!ks) CHECK(ld.sus_ratio >= 0.5f, "%s LEAD does not sustain (%.2f)", nm, ld.sus_ratio);
        CHECK(bs.att_ms < 15.f && bs.rel_ms < 500.f, "%s BASS att %.0f rel %.0f", nm, bs.att_ms, bs.rel_ms);
        CHECK(bs.bright < ld.bright, "%s BASS (%.3f) not darker than LEAD (%.3f)", nm, bs.bright, ld.bright);
        CHECK(bs.sus_ratio > 0.4f || ks, "%s BASS does not hold (%.2f)", nm, bs.sus_ratio);
    }
    // ---- sampler: swap exchanges audio/len/cfg; blank-all clears every length and touches nothing else
    {
        boot(); smp.len[1] = 1000; smp.len[5] = 400; for (int i = 0; i < 1000; i++) smp_buf[1][i] = 0.001f * i; for (int i = 0; i < 400; i++) smp_buf[5][i] = -0.002f * i;
        smp.slot[1].pitch = 0.5f;
        samplerSwapSlots(1, 5);
        CHECK(smp.len[1] == 400 && smp.len[5] == 1000, "swap lens");
        CHECK(smp_buf[5][999] == 0.001f * 999 && smp_buf[1][399] == -0.002f * 399, "swap audio");
        CHECK(smp.slot[5].pitch == 0.5f && smp.slot[1].pitch == 0.f, "swap cfg");
        samplerBlankAll(); int nz = 0; for (int i = 0; i < SMP_SLOTS; i++) nz += smp.len[i] != 0;
        CHECK(nz == 0, "blank all left %d slots", nz);
        CHECK(smp_buf[5][999] == 0.001f * 999, "blank all must not touch audio memory");
    }
    // ---- pool audition: silent when idle, plays the scratch buffer once, then goes idle by itself
    {
        boot(); static float L[BLOCK], R[BLOCK], in[BLOCK];
        memset(in, 0, sizeof in);
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); samplerProcess(in, L, R, BLOCK);
        bool silent = true; for (int i = 0; i < BLOCK; i++) silent &= (L[i] == 0.f && R[i] == 0.f);
        CHECK(silent && !samplerAuditionPlaying(), "audition not silent while idle");
        int16_t* b = samplerAuditionBuf(); for (int i = 0; i < 1000; i++) b[i] = (int16_t)(i % 2 ? 16000 : -16000);
        samplerAuditionPlay(1000);
        double e = 0; int blocks = 0; while (samplerAuditionPlaying() && blocks < 100) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); samplerProcess(in, L, R, BLOCK); for (int i = 0; i < BLOCK; i++) e += L[i] * L[i]; blocks++; }
        CHECK(e > 0.1 && blocks == 16, "audition played %d blocks, energy %.3f", blocks, e);
        samplerAuditionPlay(5000); samplerAuditionStop(); CHECK(!samplerAuditionPlaying(), "stop");
    }
    // ---- group boundaries: classic presets 0..15 untouched in count and names
    CHECK(DRUM_CLASSIC_SOUNDS == 16 && DRUM_SOUNDS >= 16, "classic kit size changed");
    printf(fails ? "test_engines: %d FAILURES\n" : "test_engines: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}
