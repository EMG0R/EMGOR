// Generic parameter mechanism for every effect, driven only by the HYPER_CORE descriptors (Fx::pt: name, min, max, flags, step) plus the FxGlue registry (defaults, lock labels).
//   p <fx> <PARAM> <value>   set a param (rejected for sensor-locked params)
//   pd <fx>                  dump the descriptors and values as JSON lines (8 params per line), the UI builds its sliders from them
//   preset reset             all params (and the output gain) back to the defaults
// Persistence: libDaisy PersistentStorage at QSPI offset 0x700000 (see main.cpp for the layout rationale), record in prec.h keyed by effect id + param NAME with a version and a CRC.
// A bad CRC / wrong version = defaults. Saved once, 2 s after the last change, from the main loop only. The time the save blocks the main loop is measured and reported (svms).
#pragma once
#include "prec.h"
static PersistentStorage<ParamRec> g_pstore(hw.qspi);
static const uint32_t OG_QSPI_OFFSET = 0x700000u;
static uint32_t g_og_dirty_ms = 0, g_save_ms = 0, g_saves = 0; static bool g_og_dirty = false, g_og_loaded = false;
static char g_pq[6][100]; static volatile uint8_t g_pqh = 0, g_pqt = 0; static char g_pline[100]; static volatile uint32_t g_pd_req = 0;   // 6-deep command queue: ISR produces, main loop consumes
static void ogApply(int db10) { g_og_db10 = db10; g_og_lin = powf(10.0f, db10 / 200.0f); }
static void fxParamsRx(const char* line) {                  // CDC ISR: just keep the line, the main loop parses it
    uint8_t h = g_pqh, nx = (uint8_t)((h + 1) % 6); if(nx == g_pqt) return;
    strncpy(g_pq[h], line, 99); g_pq[h][99] = 0; g_pqh = nx;
}
static void recBuild(ParamRec& r) {
    prec_clear(r); r.db10 = g_og_db10; for(int i = 0; i < 3; i++) { r.flo[i] = g_flo[i]; r.fhi[i] = g_fhi[i]; } r.fvalid = g_fvalid;
    for(int k = 0; k < GLUE_N; k++) { FxGlue& g = g_glue[k]; if(!g.ok) continue; for(int i = 0; i < g.fx->np; i++) { if(g.lock[i]) continue; prec_add(r, prec_fnv(g.id, g.fx->pt[i].name), glueGet(g, i)); } }
    prec_seal(r);
}
static void recApply(const ParamRec& r) {
    for(int k = 0; k < GLUE_N; k++) { FxGlue& g = g_glue[k]; if(!g.ok) continue; for(int i = 0; i < g.fx->np; i++) { float v; if(!g.lock[i] && prec_find(r, prec_fnv(g.id, g.fx->pt[i].name), &v)) { if(i == 0 && g.mixUser) *g.mixUser = v < 0 ? 0 : v > 1 ? 1 : v; else g.fx->setParam(i, v); } } g.fx->Fx::snap(); }
    g_fvalid = r.fvalid & 7; for(int i = 0; i < 3; i++) if(g_fvalid & (1u << i)) { if((int)r.fhi[i] - (int)r.flo[i] < (int)FADER_MIN_SPAN) g_fvalid &= ~(1u << i); else { g_flo[i] = r.flo[i]; g_fhi[i] = r.fhi[i]; } } g_flearn = (g_fvalid != 7);   // stored fader endpoints: learning only continues for faders without them
}
static void fxParamsInit() {                                // after fxGlueInit(), before the audio starts
    ParamRec def; recBuild(def);
    g_pstore.Init(def, OG_QSPI_OFFSET);                     // a blank sector gets the defaults written here
    ParamRec& r = g_pstore.GetSettings();
    if(prec_valid(r)) { ogApply(r.db10); recApply(r); g_og_loaded = true; } else ogApply(OG_DEF10);
    g_og_req = g_og_db10;
}
static void putf(float v) { if(v < 0) { put('-'); v = -v; } uint32_t i = (uint32_t)v; uint32_t fr = (uint32_t)((v - (float)i) * 10000.0f + 0.5f); if(fr >= 10000) { i++; fr -= 10000; } putu(i); put('.'); put('0' + fr / 1000); put('0' + (fr / 100) % 10); put('0' + (fr / 10) % 10); put('0' + fr % 10); }
static void sendPd(int k) {
    FxGlue& g = g_glue[k]; if(!g.ok) return; const int total = g.fx->np;
    for(int o = 0; o < total; o += 8) {
        beginFrame(); puts_("{\"t\":\"pd\",\"fx\":\""); puts_(g.id); puts_("\",\"o\":"); putu(o); puts_(",\"n\":"); putu(total); puts_(",\"p\":[");
        for(int i = o; i < total && i < o + 8; i++) {
            const Param& p = g.fx->pt[i];
            if(i > o) put(',');
            puts_("{\"i\":"); putu(i); puts_(",\"n\":\""); puts_(p.name); puts_("\",\"min\":"); putf(p.min); puts_(",\"max\":"); putf(p.max); puts_(",\"def\":"); putf(g.def[i]); puts_(",\"v\":"); putf(glueGet(g, i));
            puts_(",\"fl\":"); putu(p.flags); puts_(",\"st\":"); putf(p.step);
            if(g.lock[i]) { puts_(",\"lock\":\""); puts_(g.lock[i]); puts_("\""); }
            if((p.flags & PF_ENUM) && p.names) { puts_(",\"names\":["); int nn = (int)(p.max - p.min + 1.5f); for(int q = 0; q < nn; q++) { if(q) put(','); put('"'); puts_(p.names[q]); put('"'); } put(']'); }
            put('}');
        }
        puts_("]}\n"); sendWhole(); beginFrame();
    }
}
static void perr(const char* m) { beginFrame(); puts_("{\"t\":\"perr\",\"msg\":\""); puts_(m); puts_("\"}\n"); sendWhole(); beginFrame(); }
static int glueFind(const char* id) { for(int k = 0; k < GLUE_N; k++) if(g_glue[k].id && !strcmp(g_glue[k].id, id)) return k; return -1; }
static volatile bool g_state_req = false;
// `sim fader <1..3> <0..1000> | sim pad <n> | sim noise <dbfs>|off | sim tone <hz> <dbfs>|off | sim off`: the minimal virtual inputs (sim fader enters faderStep, so the dead zones are the real ones)
static void simCmd(char** t, int nt) {
    if(nt < 2) return; const char* c = t[1];
    if(!strcmp(c, "off") || !strcmp(c, "reset")) { g_simFsrM = -1.0f; g_simJoyY = -2.0f; for(int i = 0; i < 3; i++) g_simFader[i] = -1; g_noiseAmp = 0.0f; g_toneAmp = 0.0f; }
    else if(!strcmp(c, "noise")) { if(nt >= 3) g_noiseAmp = powf(10.0f, (float)atof(t[2]) / 20.0f); else g_noiseAmp = 0.0f; }
    else if(!strcmp(c, "tone")) { if(nt >= 4) { g_toneHz = (float)atof(t[2]); g_toneAmp = powf(10.0f, (float)atof(t[3]) / 20.0f); } else g_toneAmp = 0.0f; }
    else if(!strcmp(c, "admit") && nt >= 3) ADMIT_LIMIT = (float)atof(t[2]) * 0.01f;            // sim admit <percent>: admission limit (test)
    else if(!strcmp(c, "shed")) g_shedReq = true;                                              // sim shed: raise the shed request (test)
    else if(!strcmp(c, "padrel") && nt >= 3) { int n = atoi(t[2]); if(n >= 1 && n <= 16) g_padRelReq = (uint16_t)(g_padRelReq | (1u << (n - 1))); }   // sim padrel <n>: release (harmony page)
    else if(!strcmp(c, "encpress")) g_simEncPress = g_simEncPress + 1;                          // sim encpress: encoder button = toggle the harmony page
    else if(!strcmp(c, "padhold") && nt >= 3) { int n = atoi(t[2]); if(n >= 1 && n <= 16) g_padHoldReq = (uint16_t)(g_padHoldReq | (1u << (n - 1))); }   // sim padhold <n>: a long press (looper START = clear all)
    else if(!strcmp(c, "pad") && nt >= 3) { int n = atoi(t[2]); if(n >= 1 && n <= 16) g_padReq = (uint16_t)(g_padReq | (1u << (n - 1))); }   // sim pad <n>: one tap, same path as the Trellis
    else if(!strcmp(c, "joy") && nt >= 3) { const float v = (float)atof(t[2]); g_simJoyY = v < -1500 ? -2.0f : v * 0.001f; }   // sim joy <y permille> (test only; < -1500 = hardware)
    else if(!strcmp(c, "enc") && nt >= 3) g_simEncDet = g_simEncDet + atoi(t[2]);   // sim enc <detents>: encoder 1 detents
    else if(!strcmp(c, "fsr") && nt >= 3) { float v = (float)atof(t[2]); g_simFsrM = v < 0 ? -1.0f : v > 1000 ? 1000.0f : v; }   // sim fsr <0..1000>, negative = hardware
    else if(!strcmp(c, "fader") && nt >= 4) { int k = atoi(t[2]) - 1; if(k >= 0 && k < 3) { float v = (float)atof(t[3]); g_simFader[k] = v < 0 ? 0 : v > 1000 ? 1000 : v; } }
}
static void sendState() {
    beginFrame(); puts_("{\"t\":\"state\",\"ms\":"); putu(System::GetNow()); puts_(",\"dr\":{\"st\":"); putu((uint32_t)g_drg.state); puts_(",\"tgt\":"); putf(g_dr_mix_req); puts_(",\"mix\":"); putf(g_dr_wet);
    puts_("},\"fs\":["); for(int i = 0; i < 3; i++) { if(i) put(','); putf(g_fs[i]); } puts_("],\"fader\":"); putd(g_sel); puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_("}\n"); sendWhole(); beginFrame();
}
static void paramCmd(uint32_t now) {
    char* t[6] = {0, 0, 0, 0, 0, 0}; int nt = 0; char* s = g_pline;
    while(*s && nt < 6) { while(*s == ' ') *s++ = 0; if(!*s) break; t[nt++] = s; while(*s && *s != ' ') s++; }
    if(nt >= 1 && !strcmp(t[0], "preset")) {                                // preset reset
        for(int k = 0; k < GLUE_N; k++) { FxGlue& g = g_glue[k]; if(!g.ok) continue; for(int i = 0; i < g.fx->np; i++) if(!g.lock[i]) { if(i == 0 && g.mixUser) *g.mixUser = g.def[0]; else g.fx->setParam(i, g.def[i]); } g_pd_req |= 1u << k; }
        g_og_req = OG_DEF10; g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true;
    } else if(nt >= 2 && !strcmp(t[0], "pad")) { int n = atoi(t[1]); if(n >= 1 && n <= 16) { const uint16_t b = (uint16_t)(1u << (n - 1)); if(nt >= 3 && t[2][0] == 'h') g_padHoldReq = (uint16_t)(g_padHoldReq | b); else if(nt >= 3 && t[2][0] == 'r') g_padRelReq = (uint16_t)(g_padRelReq | b); else g_padReq = (uint16_t)(g_padReq | b); }   // UI pad click = a Trellis tap
    } else if(nt >= 2 && !strcmp(t[0], "pd")) {                             // pd drone
        int k = glueFind(t[1]); if(k >= 0) g_pd_req |= 1u << k; else perr("unknown effect");
    } else if(nt >= 4 && !strcmp(t[0], "p")) {                              // p drone <PARAM> <value>
        int k = glueFind(t[1]); if(k < 0 || !g_glue[k].ok) { perr("unknown effect"); return; }
        for(char* c = t[2]; *c; c++) if(*c >= 'a' && *c <= 'z') *c -= 32;
        int i = g_glue[k].fx->findParam(t[2]); if(i < 0) { perr("unknown param"); return; }
        if(g_glue[k].lock[i]) { perr("param is locked to a sensor"); return; }
        { const float x = (float)atof(t[3]); if(i == 0 && g_glue[k].mixUser) *g_glue[k].mixUser = x < 0 ? 0 : x > 1 ? 1 : x; else g_glue[k].fx->setParam(i, x); } g_og_dirty = true; g_og_dirty_ms = now;
    } else if(nt >= 1 && !strcmp(t[0], "cap")) { g_capN = 0; g_capDone = false; g_capArm = true;   // capture the output
    } else if(nt >= 1 && !strcmp(t[0], "harm")) { g_simEncPress = g_simEncPress + 1;   // UI: toggle the harmony page
    } else if(nt >= 2 && !strcmp(t[0], "nosave")) { g_nosave = !strcmp(t[1], "on"); if(g_nosave) g_og_dirty = false;   // runners set it for their whole run: nothing is persisted, nothing is marked dirty
    } else if(nt >= 2 && !strcmp(t[0], "fcal")) faderCmd(t, nt);
    else if(nt >= 1 && !strcmp(t[0], "sim")) simCmd(t, nt);
    else if(nt >= 2 && !strcmp(t[0], "mute")) { g_mute = !strcmp(t[1], "on"); }
    else if(nt >= 1 && !strcmp(t[0], "state")) g_state_req = true;
}
static inline bool simActive() { return g_simFader[0] >= 0 || g_simFader[1] >= 0 || g_simFader[2] >= 0 || g_simFsrM >= 0.0f || g_noiseAmp > 0.0f || g_toneAmp > 0.0f; }
static void fxParamsPoll(uint32_t now) {
    while(g_pqt != g_pqh) { strcpy(g_pline, g_pq[g_pqt]); g_pqt = (uint8_t)((g_pqt + 1) % 6); paramCmd(now); }
    if(g_paramDirty) { g_paramDirty = false; g_og_dirty = true; g_og_dirty_ms = now; }
    if(g_fdirty) { g_fdirty = false; g_og_dirty = true; g_og_dirty_ms = now; }
    if(g_fcal_hello) { g_fcal_hello = false; g_cmd_hello = true; }
    if(g_og_req != g_og_db10) { ogApply(g_og_req); g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true; }
    if(g_state_req) { g_state_req = false; sendState(); }
    if(g_pd_req) { for(int k = 0; k < GLUE_N; k++) if(g_pd_req & (1u << k)) sendPd(k); g_pd_req = 0; }
    if(g_nosave || simActive()) g_og_dirty = false;                             // never persist while a run / sim input is active: changes made then are dropped, not saved later
    if(g_og_dirty && now - g_og_dirty_ms >= 2000) {                         // debounce: one save, 2 s after the last change
        ParamRec r; recBuild(r); g_pstore.GetSettings() = r;
        uint32_t t0 = System::GetNow(); g_pstore.Save(); g_save_ms = System::GetNow() - t0; g_saves++; g_og_dirty = false; g_cmd_hello = true;   // svms = how long the main loop was blocked
    }
}
static void fxParamsHello() {
    puts_(",\"params\":{\"loaded\":"); put(g_og_loaded ? '1' : '0'); puts_(",\"saved\":"); put(g_og_dirty ? '0' : '1'); puts_(",\"svms\":"); putu(g_save_ms); puts_(",\"saves\":"); putu(g_saves); puts_(",\"fx\":[");
    for(int k = 0; k < GLUE_N; k++) { if(k) put(','); put('['); put('"'); puts_(g_glue[k].id ? g_glue[k].id : ""); put('"'); put(','); putu(g_glue[k].ok ? g_glue[k].fx->np : 0); put(']'); }
    puts_("]}"); faderHello();
}

static void capPoll() {                                  // main loop: one 64-sample line per call while a finished capture is being dumped
    if(!g_capDone) return;
    beginFrame(); puts_("{\"t\":\"cap\",\"o\":"); putu((uint32_t)g_capSent); puts_(",\"d\":[");
    for(int i = 0; i < 64; i++) { if(i) put(','); putd((int32_t)g_cap[g_capSent + i]); }
    puts_("]}\n"); if(sendWhole()) { g_capSent += 64; if(g_capSent >= 8192) g_capDone = false; } beginFrame();
}
