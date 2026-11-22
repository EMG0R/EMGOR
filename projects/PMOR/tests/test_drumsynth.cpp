// test_drumsynth.cpp -- the drum synth engine (SuperVoice) against the original per-kind voice (tests/ref/drum_voice_ref.*).
// Every one of the 96 presets, 8 scenarios, 3 s at 48 kHz. SNR = 10 log10(sum ref^2 / sum err^2).
//   non-LEGACY presets: >= 105 dB (bit-identical or float rounding)      LEGACY presets: >= 48 dB (owner accepts ~55 dB)
//   REVERSE: non-LEGACY same as above; LEGACY is not compared (old endlessly sustained until 63ea3f9; both now end at totalLen) -> only checked to be finite + silent at the end.
//   test_drumsynth table   prints the full SNR table
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "shared_state.h"
#include "scale.h"
#include "drum_super.h"
#include "ref/drum_voice_ref.h"
SharedState g;
struct Scn { const char* name; float len, pitch, tone, vel; bool rev; };
static const Scn SC[] = { {"base",0.5f,0.f,0.5f,1.f,false}, {"vel.6",0.5f,0.f,0.5f,0.6f,false}, {"len.2",0.2f,0.f,0.5f,1.f,false},
  {"len.85",0.85f,0.f,0.5f,1.f,false}, {"pitch+.5",0.5f,0.5f,0.5f,1.f,false}, {"tone0",0.5f,0.f,0.f,1.f,false}, {"tone1",0.5f,0.f,1.f,1.f,false}, {"rev",0.5f,0.f,0.5f,1.f,true} };
static const int NS = sizeof(SC) / sizeof(SC[0]);
int main(int argc, char** argv) {
    bool table = argc > 1; int fails = 0;
    scaleInit(); drumsInit();
    const int N = 48000 * 3;
    int bitid[NS] = {0}, ge105[NS] = {0}, ge60[NS] = {0}, tot[NS] = {0}; double worstNL[NS], worstL[NS]; for (int k = 0; k < NS; k++) worstNL[k] = worstL[k] = 999;
    if (table) { printf("grp/id name     |"); for (int k = 0; k < NS; k++) printf(" %-9s", SC[k].name); printf("\n"); }
    for (int gi = 0; gi < DG_COUNT; gi++) for (int si = 0; si < DRUM_SOUNDS; si++) {
        const SoundDef& d = DRUM_DEFS[gi][si]; const bool legacy = d.kind == VK_LEGACY;
        if (table) printf("%d/%2d %-6s  |", gi, si, DRUM_SOUND_NAMES[gi][si]);
        for (int k = 0; k < NS; k++) {
            const Scn& sc = SC[k];
            DrumPadCfg cfg{ (uint8_t)si, sc.len, sc.pitch, sc.tone, sc.rev, 1.f };
            static RefVoice a; static SuperVoice b; a = RefVoice(); b = SuperVoice(); MetalBank m1, m2; m1.init(); m2.init();
            uint32_t sA = 0xC0FFEEu, sB = 0xC0FFEEu, nA = 0x1234ABCDu, nB = 0x1234ABCDu;
            SuperDef sd; superFromDef(sd, d, gi, 0);
            a.trigger(d, cfg, sc.vel, gi, 0, si, sA); b.trigger(sd, cfg, sc.vel, gi, 0, sB);
            bool ident = true, fin = true; double se = 0, sr = 0, tail = 0;
            for (int n = 0; n < N; n++) {
                float mA = m1.tick(), mB = m2.tick(); float xA = lcg01(nA) * 2.f - 1.f, xB = lcg01(nB) * 2.f - 1.f;
                float oa = a.process(mA, xA) * a.panL, ob = b.process(mB, xB) * b.panL;
                if (!isfinite(ob)) fin = false;
                if (oa != ob) ident = false; double e = (double)oa - ob; se += e * e; sr += (double)oa * oa;
                if (n > N - 4800) tail += (double)ob * ob;
            }
            double snr = sr > 0 ? (se > 0 ? 10 * log10(sr / se) : 200.0) : 200.0;
            bool cmp = !(legacy && sc.rev);
            if (!fin) { fails++; printf("FAIL: non-finite %d/%d %s\n", gi, si, sc.name); }
            if (sA != sB) { fails++; printf("FAIL: RNG draw count differs %d/%d %s\n", gi, si, sc.name); }
            if (legacy && sc.rev && sd.lenNom < 2.8f && tail > 1e-12) { fails++; printf("FAIL: legacy reverse not silent at the end %d/%d\n", gi, si); }
            if (cmp) {
                tot[k]++; if (ident) bitid[k]++; if (snr >= 105) ge105[k]++; if (snr >= 60) ge60[k]++;
                double& w = legacy ? worstL[k] : worstNL[k]; if (snr < w) w = snr;
                double need = legacy ? 48.0 : 105.0;
                if (snr < need) { fails++; printf("FAIL: %d/%d %s %s snr %.1f dB < %.0f\n", gi, si, DRUM_SOUND_NAMES[gi][si], sc.name, snr, need); }
            }
            if (table) { if (!cmp) printf(" (n/a)    "); else if (ident) printf(" ident    "); else printf(" %6.1fdB ", snr); }
        }
        if (table) printf("\n");
    }
    // every preset's mapped params must lie inside the schema range (else a SET / patch load would clamp the preset's own value)
    for (int gi = 0; gi < DG_COUNT; gi++) for (int si = 0; si < DRUM_SOUNDS; si++) for (int idx = 0; idx < 4; idx++) {
        SuperDef sd; superFromDef(sd, DRUM_DEFS[gi][si], gi, idx); const float* f = (const float*)&sd;
        for (size_t k = 0; k < SDEF_NPARAMS; k++) { float v = f[SDEF_PARAMS[k].off];
            if (!(v >= SDEF_PARAMS[k].mn && v <= SDEF_PARAMS[k].mx)) { fails++; printf("FAIL: preset %d/%d %s: %s = %g outside [%g, %g]\n", gi, si, DRUM_SOUND_NAMES[gi][si], SDEF_PARAMS[k].name, v, SDEF_PARAMS[k].mn, SDEF_PARAMS[k].mx); } }
    }
    for (size_t k = 0; k < SDEF_NPARAMS; k++) if (strlen(SDEF_PARAMS[k].name) > 8) { fails++; printf("FAIL: param name too long: %s\n", SDEF_PARAMS[k].name); }
    printf("drumsynth vs original voice (96 presets; SNR dB):\n");
    for (int k = 0; k < NS; k++) printf("  %-9s compared %2d  bit-identical %2d  >=105dB %2d  >=60dB %2d  worst non-LEGACY %.1f  worst LEGACY %.1f\n", SC[k].name, tot[k], bitid[k], ge105[k], ge60[k], worstNL[k], worstL[k]);
    printf(fails ? "FAILED (%d)\n" : "all ok\n", fails);
    return fails ? 1 : 0;
}
