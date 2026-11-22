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
    { FxG& fl = g_fxg[GI_FLANGE]; if(fl.ok) { int ti = fl.fx->findParam("THRUZ"); if(fl.fx->pt[ti >= 0 ? ti : 0].name && ti >= 0) fl.fx->setParam(ti, 0.0f); } }   // migration: the saved THRUZ=1 of older builds must not win (thru-zero puts polarity-inverted dry in the output)
    g_fvalid = r.fvalid & 7; for(int i = 0; i < 3; i++) if(g_fvalid & (1u << i)) { if((int)r.fhi[i] - (int)r.flo[i] < (int)FADER_MIN_SPAN) g_fvalid &= ~(1u << i); else { g_flo[i] = r.flo[i]; g_fhi[i] = r.fhi[i]; } } g_flearn = (g_fvalid != 7);   // stored fader endpoints: learning only continues for faders without them
}
static void fxParamsInit() {                                // after fxGlueInit(), before the audio starts
    ParamRec def; recBuild(def);
    g_pstore.Init(def, OG_QSPI_OFFSET);                     // a blank sector gets the defaults written here
    ParamRec& r = g_pstore.GetSettings();
    if(prec_valid(r)) { ogApply(r.db10); recApply(r); g_og_loaded = true; } else ogApply(-120);
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
static void simAllOff() { g_simPadOn = false; g_simKeys = 0; for(int i = 0; i < 3; i++) g_simFader[i] = -1; g_simPot[0] = g_simPot[1] = -1; g_simFsr = -1; g_simLid = -1; g_simAccel = false; g_simJoy = false; g_simEnc = false; g_simJbtn = false; }
// `sim ...`: virtual inputs; `sim off` hands everything back to the hardware, `sim reset` also puts every effect and pad state back to the boot state (the soak runner uses it between rows)
static void simCmd(char** t, int nt, uint32_t now) {
    if(nt < 2) return; const char* c = t[1];
    if(!strcmp(c, "off")) { simAllOff(); }
    else if(!strcmp(c, "reset")) {
        simAllOff(); for(int gi = 0; gi < GLUE_N; gi++) { g_fxg[gi].latch = false; g_fxg[gi].hold = false; g_extmix[gi] = 0.0f; } drGlueControls(0.0f);
        g_dmode = 0; g_dsmooth = false; g_joyMode = 0; g_frzClearMs = 0; g_pprev = 0; g_focus = -1; g_harmMode = false; for(int s = 0; s < 3; s++) g_hvPad[s] = -1; g_exOwner = -1; g_exBlocked = 0; harmApply(); g_fxg[GI_HARMONY].mix = 0.7f;
        g_simFader[0] = g_simFader[1] = g_simFader[2] = 0.0f; g_simPot[0] = 0.0f; g_simFsr = 0.0f; g_simLid = 0.0f; g_simAccel = true; g_simAy = -1.0f;   // everything at rest: faders down, pot 0, lidar near (flanger off), accel low
        FxG& l = g_fxg[GI_LOOPER]; if(l.ok) fxgAct(GI_LOOPER, hx::A_CLEAR, 1.0f);
        g_encInit = false; g_simEncDet = -(g_enc[0].cnt / 4);
    }
    else if(!strcmp(c, "noise")) { if(nt >= 3) g_noiseAmp = powf(10.0f, (float)atof(t[2]) / 20.0f); else g_noiseAmp = 0.0f; }   // sim noise <dbfs> | sim noise off
    else if(!strcmp(c, "tone")) { if(nt >= 4) { float db = (float)atof(t[3]); g_toneHz = (float)atof(t[2]); g_toneAmp = powf(10.0f, db / 20.0f); } else g_toneAmp = 0.0f; }   // sim tone <hz> <dbfs> | sim tone off
    else if(!strcmp(c, "pad") && nt >= 4) { int n = atoi(t[2]); if(n < 1 || n > 16) return; const uint16_t b = (uint16_t)(1u << (n - 1)); g_simPadOn = true; if(!strcmp(t[3], "down")) { g_simKeys |= b; g_flash[n - 1] = 1.0f; } else g_simKeys &= (uint16_t)~b; }
    else if(!strcmp(c, "fader") && nt >= 4) { int k = atoi(t[2]) - 1; if(k >= 0 && k < 3) { float v = (float)atof(t[3]); g_simFader[k] = v < 0 ? 0 : v > 1000 ? 1000 : v; } }
    else if(!strcmp(c, "pot") && nt >= 4) { int k = atoi(t[2]) - 1; if(k >= 0 && k < 2) { float v = (float)atof(t[3]); g_simPot[k] = v < 0 ? 0 : v > 1000 ? 1000 : v; } }
    else if(!strcmp(c, "fsr") && nt >= 3) { float v = (float)atof(t[2]); g_simFsr = v < 0 ? 0 : v > 1000 ? 1000 : v; }
    else if(!strcmp(c, "lidar") && nt >= 3) { float v = (float)atof(t[2]); g_simLid = v < 0 ? 0 : v; }
    else if(!strcmp(c, "accel") && nt >= 5) { float y = (float)atof(t[3]); g_simAy = y < -2 ? -2 : y > 2 ? 2 : y; g_simAccel = true; }
    else if(!strcmp(c, "joy") && nt >= 5) { g_simJx = atoi(t[2]); g_simJy = atoi(t[3]); const bool b = atoi(t[4]) != 0; if(b && !g_simJbtn) g_simJpress = g_simJpress + 1; g_simJbtn = b; g_simJoy = true; }
    else if(!strcmp(c, "enc") && nt >= 4) { int k = atoi(t[2]); g_simEnc = true; if(k == 1) { if(!strcmp(t[3], "press")) g_simEncPress = g_simEncPress + 1; else g_simEncDet += atoi(t[3]); } else if(k == 2 && !strcmp(t[3], "press")) g_simEnc2Press = g_simEnc2Press + 1; }   // either button toggles the harmony page; encoder 2 rotation unused
    (void)now;
}
static void sendState() {                                // one JSON line: every effect's gate state, wants, target and current mix, mapped params; pad / sensor mode state
    beginFrame(); puts_("{\"t\":\"state\",\"ms\":"); putu(System::GetNow()); puts_(",\"fx\":[");
    for(int gi = 0; gi < GLUE_N; gi++) {
        FxG& g = g_fxg[gi]; if(gi) put(',');
        puts_("{\"id\":\""); puts_(g_glue[gi].id ? g_glue[gi].id : ""); puts_("\",\"ok\":"); put(g_glue[gi].ok ? '1' : '0'); puts_(",\"st\":"); putu(gi == GI_DRONE ? (uint32_t)g_drg.state : (uint32_t)g.gate.state);
        float tgt = 0, cur = 0;
        if(gi == GI_DRONE) { tgt = g_dr_mix_req; cur = g_dr_wet; } else if(g.ok) { const float ex = g.d->ext ? *g.d->ext : 0.0f, wn = g.want() ? g.mix : 0.0f; tgt = ex > wn ? ex : wn; cur = g.fx->sm[0]; }
        puts_(",\"lat\":"); put(g.latch ? '1' : '0'); puts_(",\"hold\":"); put(g.hold ? '1' : '0'); puts_(",\"tgt\":"); putf(tgt); puts_(",\"mix\":"); putf(cur);
        puts_(",\"ref\":"); put(g.refuseMs > (uint32_t)System::GetNow() ? '1' : '0'); puts_(",\"nan\":"); putu(g_nan[gi]); puts_(",\"p\":{");
        if(g_glue[gi].ok && gi != GI_DRONE) { bool first = true; FxGlue& gl = g_glue[gi];
            for(int i = 1; i < gl.fx->np; i++) { const char* nm = gl.fx->pt[i].name; const bool mapped = gl.lock[i] != nullptr || (gi == GI_DELAY && (!strcmp(nm, "FB") || !strcmp(nm, "TIME")));
                if(!mapped) continue; if(!first) put(','); first = false; put('"'); puts_(nm); puts_("\":"); putf(gl.fx->v[i]); } }
        put('}'); put('}');
    }
    puts_("],\"fs\":["); for(int i = 0; i < 3; i++) { if(i) put(','); putf(g_fs[i]); }
    puts_("],\"warp\":"); putf(g_rv_warp_t); puts_(",\"dmode\":"); putu((uint32_t)g_dmode); puts_(",\"smooth\":"); put(g_dsmooth ? '1' : '0'); puts_(",\"joy\":"); putu((uint32_t)g_joyMode);
    puts_(",\"frz\":"); putf(g_cv[5]); puts_(",\"frzclr\":"); put(g_frzClearMs ? '1' : '0'); puts_(",\"foc\":"); putd(g_focus); puts_(",\"shed\":["); putu(g_shed); put(','); putd(g_shedLast); puts_("],\"hmode\":"); put(g_harmMode ? '1' : '0'); puts_(",\"hmix\":"); putf(g_fxg[GI_HARMONY].mix); puts_(",\"hv\":["); for(int s = 0; s < 3; s++) { if(s) put(','); putd(g_hvPad[s] >= 0 ? kHarmSemi[g_hvPad[s]] : 0); } puts_("],\"ex\":["); putd(g_exOwner); put(','); putu(g_exBlocked); puts_("]"); puts_(",\"in\":"); putu((g_simPadOn ? 1u : 0u) | (g_simFader[0] >= 0 ? 2u : 0u) | (g_simFader[1] >= 0 ? 4u : 0u) | (g_simFader[2] >= 0 ? 8u : 0u) | (g_simPot[0] >= 0 ? 16u : 0u) | (g_simPot[1] >= 0 ? 32u : 0u) | (g_simFsr >= 0 ? 64u : 0u) | (g_simLid >= 0 ? 128u : 0u) | (g_simAccel ? 256u : 0u) | (g_simJoy ? 512u : 0u) | (g_simEnc ? 1024u : 0u));
    { hx::FxLooper* lp = static_cast<hx::FxLooper*>(g_fxg[GI_LOOPER].fx); puts_(",\"loop\":["); putu(lp ? (uint32_t)lp->state() : 0); put(','); putu(lp ? (uint32_t)lp->layers() : 0); put(','); putu(lp ? (uint32_t)lp->length() : 0); put(']'); }
    puts_(",\"mute\":"); put(g_mute ? '1' : '0'); puts_("}\n"); sendWhole(); beginFrame();
}
static void paramCmd(uint32_t now) {
    char* t[6] = {0, 0, 0, 0, 0, 0}; int nt = 0; char* s = g_pline;
    while(*s && nt < 6) { while(*s == ' ') *s++ = 0; if(!*s) break; t[nt++] = s; while(*s && *s != ' ') s++; }
    if(nt >= 1 && !strcmp(t[0], "preset")) {                                // preset reset
        for(int k = 0; k < GLUE_N; k++) { FxGlue& g = g_glue[k]; if(!g.ok) continue; for(int i = 0; i < g.fx->np; i++) if(!g.lock[i]) { if(i == 0 && g.mixUser) *g.mixUser = g.def[0]; else g.fx->setParam(i, g.def[i]); } g_pd_req |= 1u << k; }
        g_og_req = -120; g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true;
    } else if(nt >= 2 && !strcmp(t[0], "pd")) {                             // pd <fx>
        int k = glueFind(t[1]); if(k >= 0) g_pd_req |= 1u << k; else perr("unknown effect");
    } else if(nt >= 4 && !strcmp(t[0], "p")) {                              // p <fx> <PARAM> <value>
        int k = glueFind(t[1]); if(k < 0 || !g_glue[k].ok) { perr("unknown effect"); return; }
        for(char* c = t[2]; *c; c++) if(*c >= 'a' && *c <= 'z') *c -= 32;
        int i = g_glue[k].fx->findParam(t[2]); if(i < 0) { perr("unknown param"); return; }
        if(g_glue[k].lock[i]) { perr("param is locked to a sensor"); return; }
        { float x = (float)atof(t[3]); if(i == 0 && g_glue[k].mixUser) *g_glue[k].mixUser = x < 0 ? 0 : x > 1 ? 1 : x; else g_glue[k].fx->setParam(i, x); }   // MIX of a pad effect is the user's wet level
        g_og_dirty = true; g_og_dirty_ms = now;
    } else if(nt >= 3 && !strcmp(t[0], "fx")) {                             // fx <id> on|off|toggle: the UI's toggle, same rules as the pads (CPU refusal included)
        int k = glueFind(t[1]); if(k <= GI_DRONE) { perr(k < 0 ? "unknown effect" : "mix is on a fader"); return; }
        FxG& g = g_fxg[k]; const bool on = !strcmp(t[2], "on") || (!strcmp(t[2], "toggle") && !g.latch);
        if(on) { if(!g.latch) { if(g.ok && (fxIsOn(k) || fxAllowOn(k))) g.latch = true; else g.refuseMs = now + 700; } } else g.latch = false;
    } else if(nt >= 2 && !strcmp(t[0], "focus")) { int k = glueFind(t[1]); if(k >= 0) g_focus = k; }
    else if(nt >= 2 && !strcmp(t[0], "fcal")) faderCmd(t, nt);
    else if(nt >= 1 && !strcmp(t[0], "sim")) simCmd(t, nt, now);
    else if(nt >= 2 && !strcmp(t[0], "mute")) { g_mute = !strcmp(t[1], "on"); }
    else if(nt >= 1 && !strcmp(t[0], "state")) g_state_req = true;
}
static void fxParamsPoll(uint32_t now) {
    while(g_pqt != g_pqh) { strcpy(g_pline, g_pq[g_pqt]); g_pqt = (uint8_t)((g_pqt + 1) % 6); paramCmd(now); }
    if(g_fdirty) { g_fdirty = false; g_og_dirty = true; g_og_dirty_ms = now; }
    if(g_fcal_hello) { g_fcal_hello = false; g_cmd_hello = true; }
    if(g_og_req != g_og_db10) { ogApply(g_og_req); g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true; }
    if(g_state_req) { g_state_req = false; sendState(); }
    if(g_pd_req) { for(int k = 0; k < GLUE_N; k++) if(g_pd_req & (1u << k)) sendPd(k); g_pd_req = 0; }
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
