// scenario.cpp -- see scenario.h. Script grammar (one command per line, '#' comments):
//   seconds S                    total render length
//   bpm B                        clockSetBpm
//   set KEY VALUE                KEY in: syn.algo syn.volume drums.volume drums.pad.N.{sound,length,pitch,tone,reverse,vol}
//                                smp.volume smp.width smp.width_prob smp.polyphony smp.slot.N.{pitch,formant,cutoff,reverse}
//                                seq.E.{mode,probability,density,polyphony,oct_amount,oct_prob,arp_on,rec_on,bars,spb}
//                                fx.dist_algo clk.swing g.fsr g.lidar g.accel_x
//   syn_load ALGO PRESET         synLoadPreset (factory sound bank)
//   syn_preset ALGO IDX VALUE    syn.preset[ALGO].p[IDX]
//   seq_step E STEP MASK         seq.lane[E].step[STEP] = MASK (hex ok)
//   smp_fill SLOT LEN            deterministic test tone/noise into sampler slot
//   at T drum PAD VEL | synth_on PAD VEL | synth_off PAD | smp SLOT VEL | fx_press ID | fx_release ID
//   at T pads_held MASK | fsr V
#include "scenario.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
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

namespace {
struct Ev { uint32_t block; std::string cmd; double a, b; };

void bootEngines() {      // same order as hw/boot.cpp
    memset(&g, 0, sizeof(g));
    clockInit(120.f);
    seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
}

bool setKey(const std::string& k, double v) {
    float f = (float)v;
    int n, e;
    if (k == "syn.algo")        { syn.algo = (uint8_t)v; return true; }
    if (k == "syn.volume")      { syn.volume = f; return true; }
    if (k == "drums.volume")    { drums.volume = f; return true; }
    if (k == "smp.volume")      { smp.volume = f; return true; }
    if (k == "smp.width")       { smp.width = f; return true; }
    if (k == "smp.width_prob")  { smp.width_prob = f; return true; }
    if (k == "smp.polyphony")   { smp.polyphony = f; return true; }
    if (k == "fx.dist_algo")    { fx.dist_algo = (uint8_t)v; return true; }
    if (k == "clk.swing")       { clk.swing = f; return true; }
    if (k == "g.fsr")           { g.fsr = f; return true; }
    if (k == "g.lidar")         { g.lidar = f; return true; }
    if (k == "g.accel_x")       { g.accel_x = f; return true; }
    { int ge, gk; if (sscanf(k.c_str(), "drums.eng.%d.%d", &ge, &gk) == 2 && ge >= 0 && ge < DG_COUNT && gk >= 0 && gk < DENG_PARAMS) { drums.eng[ge].x[gk] = f; return true; } }
    char fld[32];
    if (sscanf(k.c_str(), "drums.pad.%d.%31s", &n, fld) == 2 && n >= 0 && n < N_PADS) {
        DrumPadCfg& c = drums.pad[n];
        if (!strcmp(fld, "sound"))   c.sound = (uint8_t)v;
        else if (!strcmp(fld, "length")) c.length = f;
        else if (!strcmp(fld, "pitch"))  c.pitch = f;
        else if (!strcmp(fld, "tone"))   c.tone = f;
        else if (!strcmp(fld, "reverse")) c.reverse = v != 0;
        else if (!strcmp(fld, "vol"))    c.vol = f;
        else return false;
        return true;
    }
    if (sscanf(k.c_str(), "smp.slot.%d.%31s", &n, fld) == 2 && n >= 0 && n < SMP_SLOTS) {
        SmpSlotCfg& c = smp.slot[n];
        if (!strcmp(fld, "pitch")) c.pitch = f;
        else if (!strcmp(fld, "formant")) c.formant = f;
        else if (!strcmp(fld, "cutoff")) c.cutoff = f;
        else if (!strcmp(fld, "reverse")) c.reverse = v != 0;
        else return false;
        return true;
    }
    if (sscanf(k.c_str(), "seq.%d.%31s", &e, fld) == 2 && e >= 0 && e < ENG_COUNT) {
        LaneSet& l = seq.lane[e];
        if (!strcmp(fld, "mode"))        { l.mode = (uint8_t)v; l.mode_f = f; }
        else if (!strcmp(fld, "probability")) l.probability = f;
        else if (!strcmp(fld, "density"))     l.density = f;
        else if (!strcmp(fld, "polyphony"))   l.polyphony = f;
        else if (!strcmp(fld, "oct_amount"))  l.oct_amount = f;
        else if (!strcmp(fld, "oct_prob"))    l.oct_prob = f;
        else if (!strcmp(fld, "arp_on"))      { l.arp_on = f; l.arp = v != 0; }
        else if (!strcmp(fld, "rec_on"))      l.rec_on = f;
        else if (!strcmp(fld, "bars"))        seqSetBars((Engine)e, (uint8_t)v);
        else if (!strcmp(fld, "spb"))         seqSetStepsPerBar((Engine)e, (uint8_t)v);
        else return false;
        return true;
    }
    return false;
}
}  // namespace

int scenarioRun(const char* script_path, const char* out_path, ScenarioBlockFn fn) {
    FILE* sf = fopen(script_path, "r");
    if (!sf) { fprintf(stderr, "scenario: cannot open %s\n", script_path); return 2; }
    bootEngines();
    double seconds = 1.0;
    std::vector<Ev> evs;
    char line[256];
    int ln = 0;
    while (fgets(line, sizeof line, sf)) {
        ln++;
        char* h = strchr(line, '#'); if (h) *h = 0;
        char w[5][64] = {{0}};
        int nw = sscanf(line, "%63s %63s %63s %63s %63s", w[0], w[1], w[2], w[3], w[4]);
        if (nw <= 0) continue;
        std::string c = w[0];
        if (c == "seconds")      seconds = atof(w[1]);
        else if (c == "bpm")     clockSetBpm((float)atof(w[1]));
        else if (c == "set")     { if (!setKey(w[1], strtod(w[2], 0))) { fprintf(stderr, "%s:%d bad key %s\n", script_path, ln, w[1]); return 3; } }
        else if (c == "syn_load") synLoadPreset((uint8_t)(atoi(w[1]) % ALG_COUNT), (uint8_t)atoi(w[2]));
        else if (c == "syn_preset") syn.preset[atoi(w[1]) % ALG_COUNT].p[atoi(w[2]) & 3] = (float)atof(w[3]);
        else if (c == "seq_step") { seq.lane[atoi(w[1]) % ENG_COUNT].step[atoi(w[2]) % MAXSTEPS] = (uint16_t)strtoul(w[3], 0, 0); }
        else if (c == "smp_fill") {
            int slot = atoi(w[1]) % SMP_SLOTS; uint32_t len = (uint32_t)atoi(w[2]); if (len > SMP_LEN) len = SMP_LEN;
            uint32_t s = 0xBADC0DEu + (uint32_t)slot;
            for (uint32_t i = 0; i < len; i++) {
                float env = expf(-(float)i / (0.25f * SR));
                s = s * 1664525u + 1013904223u;
                float nz = ((float)(s >> 8) * (1.f / 16777216.f) * 2.f - 1.f);
                smp_buf[slot][i] = env * (0.6f * sinf(6.2831853f * 220.f * (float)i / SR * (1.f + 0.5f * (float)slot / 16.f)) + 0.2f * nz);
            }
            smp.len[slot] = len;
        }
        else if (c == "at") {
            Ev e; e.block = (uint32_t)(atof(w[1]) * SR / BLOCK); e.cmd = w[2]; e.a = atof(w[3]); e.b = atof(w[4]);
            if (e.cmd == "pads_held") e.a = (double)strtoul(w[3], 0, 0);
            evs.push_back(e);
        }
        else { fprintf(stderr, "%s:%d unknown command %s\n", script_path, ln, w[0]); return 3; }
    }
    fclose(sf);

    FILE* of = fopen(out_path, "wb");
    if (!of) { fprintf(stderr, "scenario: cannot write %s\n", out_path); return 2; }
    uint32_t nblocks = (uint32_t)(seconds * SR / BLOCK);
    static float inbuf[BLOCK], L[BLOCK], R[BLOCK], il[2 * BLOCK];
    memset(inbuf, 0, sizeof inbuf);
    for (uint32_t b = 0; b < nblocks; b++) {
        for (const Ev& e : evs) {
            if (e.block != b) continue;
            if      (e.cmd == "drum")       drumsTrigger((uint8_t)e.a, (float)e.b);
            else if (e.cmd == "synth_on")   synthNoteOn((uint8_t)e.a, (float)e.b);
            else if (e.cmd == "synth_off")  synthNoteOff((uint8_t)e.a);
            else if (e.cmd == "smp")        samplerPlay((uint8_t)e.a, (float)e.b);
            else if (e.cmd == "fx_press")   fxPress((uint8_t)e.a);
            else if (e.cmd == "fx_release") fxRelease((uint8_t)e.a);
            else if (e.cmd == "pads_held")  g.pads_held = (uint16_t)e.a;
            else if (e.cmd == "fsr")        g.fsr = (float)e.a;
            else { fprintf(stderr, "scenario: unknown event %s\n", e.cmd.c_str()); fclose(of); return 3; }
        }
        fn(inbuf, L, R, BLOCK);
        for (int i = 0; i < BLOCK; i++) { il[2 * i] = L[i]; il[2 * i + 1] = R[i]; }
        fwrite(il, sizeof(float), 2 * BLOCK, of);
    }
    fclose(of);
    return 0;
}
