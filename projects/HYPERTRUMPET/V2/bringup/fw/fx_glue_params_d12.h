// TEST 12 parameter mechanism for the drone ONLY (no registry): `p drone <PARAM> <v>`, `pd drone`, `preset reset`, `sim fader|noise|tone|off`, `fcal ...`, `mute`, `state`, `nosave`, `cap`.
// Persistence: libDaisy PersistentStorage at QSPI 0x700000 (prec.h, keyed by "drone:" + PARAM NAME, versioned + CRC). Saved once, 2 s after the last change, main loop only.
#pragma once
#include "prec.h"
static PersistentStorage<ParamRec> g_pstore(hw.qspi);
static const uint32_t OG_QSPI_OFFSET = 0x700000u;
static uint32_t g_og_dirty_ms = 0, g_save_ms = 0, g_saves = 0; static bool g_og_dirty = false, g_og_loaded = false; static volatile bool g_nosave = false;
static char g_pq[6][100]; static volatile uint8_t g_pqh = 0, g_pqt = 0; static char g_pline[100]; static volatile uint32_t g_pd_req = 0;
static void ogApply(int db10) { g_og_db10 = db10; g_og_lin = powf(10.0f, db10 / 200.0f); }
static void fxParamsRx(const char* line) { uint8_t h = g_pqh, nx = (uint8_t)((h + 1) % 6); if(nx == g_pqt) return; strncpy(g_pq[h], line, 99); g_pq[h][99] = 0; g_pqh = nx; }
static void recBuild(ParamRec& r) {
    prec_clear(r); r.db10 = g_og_db10; for(int i = 0; i < 3; i++) { r.flo[i] = g_flo[i]; r.fhi[i] = g_fhi[i]; } r.fvalid = g_fvalid;
    if(g_dr_ok) for(int i = 1; i < g_dr->np; i++) prec_add(r, prec_fnv("drone", g_dr->pt[i].name), g_dr->v[i]);       // param 0 (MIX) is the fader's
    prec_seal(r);
}
static void recApply(const ParamRec& r) {
    if(g_dr_ok) for(int i = 1; i < g_dr->np; i++) { float v; if(prec_find(r, prec_fnv("drone", g_dr->pt[i].name), &v)) g_dr->setParam(i, v); }
    g_fvalid = r.fvalid & 7; for(int i = 0; i < 3; i++) if(g_fvalid & (1u << i)) { if((int)r.fhi[i] - (int)r.flo[i] < (int)FADER_MIN_SPAN) g_fvalid &= ~(1u << i); else { g_flo[i] = r.flo[i]; g_fhi[i] = r.fhi[i]; } } g_flearn = (g_fvalid != 7);
}
static void fxParamsInit() {
    ParamRec def; recBuild(def); g_pstore.Init(def, OG_QSPI_OFFSET); ParamRec& r = g_pstore.GetSettings();
    if(prec_valid(r)) { ogApply(r.db10); recApply(r); g_og_loaded = true; } else ogApply(OG_DEF10);
    g_og_req = g_og_db10;
}
static void putf(float v) { if(v < 0) { put('-'); v = -v; } uint32_t i = (uint32_t)v; uint32_t fr = (uint32_t)((v - (float)i) * 10000.0f + 0.5f); if(fr >= 10000) { i++; fr -= 10000; } putu(i); put('.'); put('0' + fr / 1000); put('0' + (fr / 100) % 10); put('0' + (fr / 10) % 10); put('0' + fr % 10); }
static void sendPd() {
    if(!g_dr_ok) return; const int total = g_dr->np;
    for(int o = 0; o < total; o += 8) {
        beginFrame(); puts_("{\"t\":\"pd\",\"fx\":\"drone\",\"o\":"); putu(o); puts_(",\"n\":"); putu(total); puts_(",\"p\":[");
        for(int i = o; i < total && i < o + 8; i++) {
            const Param& p = g_dr->pt[i]; if(i > o) put(',');
            puts_("{\"i\":"); putu(i); puts_(",\"n\":\""); puts_(p.name); puts_("\",\"min\":"); putf(p.min); puts_(",\"max\":"); putf(p.max); puts_(",\"def\":"); putf(g_dr_def[i]); puts_(",\"v\":"); putf(g_dr->v[i]);
            puts_(",\"fl\":"); putu(p.flags); puts_(",\"st\":"); putf(p.step); if(i == 0) puts_(",\"lock\":\"fader 2\"");
            if((p.flags & PF_ENUM) && p.names) { puts_(",\"names\":["); int nn = (int)(p.max - p.min + 1.5f); for(int q = 0; q < nn; q++) { if(q) put(','); put('"'); puts_(p.names[q]); put('"'); } put(']'); }
            put('}');
        }
        puts_("]}\n"); sendWhole(); beginFrame();
    }
}
static void perr(const char* m) { beginFrame(); puts_("{\"t\":\"perr\",\"msg\":\""); puts_(m); puts_("\"}\n"); sendWhole(); beginFrame(); }
static volatile bool g_state_req = false;
static void simCmd(char** t, int nt) {
    if(nt < 2) return; const char* c = t[1];
    if(!strcmp(c, "off") || !strcmp(c, "reset")) { for(int i = 0; i < 3; i++) g_simFader[i] = -1; g_noiseAmp = 0.0f; g_toneAmp = 0.0f; }
    else if(!strcmp(c, "noise")) { if(nt >= 3) g_noiseAmp = powf(10.0f, (float)atof(t[2]) / 20.0f); else g_noiseAmp = 0.0f; }
    else if(!strcmp(c, "tone")) { if(nt >= 4) { g_toneHz = (float)atof(t[2]); g_toneAmp = powf(10.0f, (float)atof(t[3]) / 20.0f); } else g_toneAmp = 0.0f; }
    else if(!strcmp(c, "fader") && nt >= 4) { int k = atoi(t[2]) - 1; if(k >= 0 && k < 3) { float v = (float)atof(t[3]); g_simFader[k] = v < 0 ? 0 : v > 1000 ? 1000 : v; } }
}
static void sendState() {
    beginFrame(); puts_("{\"t\":\"state\",\"ms\":"); putu(System::GetNow()); puts_(",\"dr\":{\"st\":"); putu((uint32_t)g_drg.state); puts_(",\"tgt\":"); putf(g_dr_mix_req); puts_(",\"mix\":"); putf(g_dr_wet);
    puts_("},\"fs\":["); for(int i = 0; i < 3; i++) { if(i) put(','); putf(g_fs[i]); } puts_("],\"fader\":"); putu((uint32_t)DRONE_FADER); puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_("}\n"); sendWhole(); beginFrame();
}
static void paramCmd(uint32_t now) {
    char* t[6] = {0, 0, 0, 0, 0, 0}; int nt = 0; char* s = g_pline;
    while(*s && nt < 6) { while(*s == ' ') *s++ = 0; if(!*s) break; t[nt++] = s; while(*s && *s != ' ') s++; }
    if(nt >= 1 && !strcmp(t[0], "preset")) { if(g_dr_ok) for(int i = 1; i < g_dr->np; i++) g_dr->setParam(i, g_dr_def[i]); g_pd_req |= 1u; g_og_req = OG_DEF10; g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true; }
    else if(nt >= 2 && !strcmp(t[0], "pd")) { if(!strcmp(t[1], "drone")) g_pd_req |= 1u; else perr("unknown effect"); }
    else if(nt >= 4 && !strcmp(t[0], "p")) {                                // p drone <PARAM> <value>
        if(strcmp(t[1], "drone") || !g_dr_ok) { perr("unknown effect"); return; }
        for(char* c = t[2]; *c; c++) if(*c >= 'a' && *c <= 'z') *c -= 32;
        const int i = g_dr->findParam(t[2]); if(i < 0) { perr("unknown param"); return; } if(i == 0) { perr("param is locked to a sensor"); return; }
        g_dr->setParam(i, (float)atof(t[3])); g_og_dirty = true; g_og_dirty_ms = now;
    }
    else if(nt >= 2 && !strcmp(t[0], "nosave")) { g_nosave = !strcmp(t[1], "on"); if(g_nosave) g_og_dirty = false; }
    else if(nt >= 1 && !strcmp(t[0], "cap")) { g_capN = 0; g_capDone = false; g_capArm = true; }
    else if(nt >= 2 && !strcmp(t[0], "fcal")) faderCmd(t, nt);
    else if(nt >= 1 && !strcmp(t[0], "sim")) simCmd(t, nt);
    else if(nt >= 2 && !strcmp(t[0], "mute")) g_mute = !strcmp(t[1], "on");
    else if(nt >= 1 && !strcmp(t[0], "state")) g_state_req = true;
}
static inline bool simActive() { return g_simFader[0] >= 0 || g_simFader[1] >= 0 || g_simFader[2] >= 0 || g_noiseAmp > 0.0f || g_toneAmp > 0.0f; }
static void fxParamsPoll(uint32_t now) {
    while(g_pqt != g_pqh) { strcpy(g_pline, g_pq[g_pqt]); g_pqt = (uint8_t)((g_pqt + 1) % 6); paramCmd(now); }
    if(g_fdirty) { g_fdirty = false; g_og_dirty = true; g_og_dirty_ms = now; }
    if(g_fcal_hello) { g_fcal_hello = false; g_cmd_hello = true; }
    if(g_og_req != g_og_db10) { ogApply(g_og_req); g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true; }
    if(g_state_req) { g_state_req = false; sendState(); }
    if(g_pd_req) { g_pd_req = 0; sendPd(); }
    if(g_nosave || simActive()) g_og_dirty = false;                       // never persist during a run / sim input
    if(g_og_dirty && now - g_og_dirty_ms >= 2000) { ParamRec r; recBuild(r); g_pstore.GetSettings() = r; uint32_t t0 = System::GetNow(); g_pstore.Save(); g_save_ms = System::GetNow() - t0; g_saves++; g_og_dirty = false; g_cmd_hello = true; }
}
static void fxParamsHello() {
    puts_(",\"params\":{\"loaded\":"); put(g_og_loaded ? '1' : '0'); puts_(",\"saved\":"); put(g_og_dirty ? '0' : '1'); puts_(",\"svms\":"); putu(g_save_ms); puts_(",\"saves\":"); putu(g_saves);
    puts_(",\"fx\":[[\"drone\","); putu(g_dr_ok ? (uint32_t)g_dr->np : 0u); puts_("]]}"); faderHello();
}
static void capPoll() {
    if(!g_capDone) return;
    beginFrame(); puts_("{\"t\":\"cap\",\"o\":"); putu((uint32_t)g_capSent); puts_(",\"d\":["); for(int i = 0; i < 64; i++) { if(i) put(','); putd((int32_t)g_cap[g_capSent + i]); }
    puts_("]}\n"); if(sendWhole()) { g_capSent += 64; if(g_capSent >= 8192) g_capDone = false; } beginFrame();
}
