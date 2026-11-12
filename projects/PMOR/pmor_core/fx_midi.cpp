// fx_midi.cpp -- see fx_midi.h.
#include <string.h>
#include "fx_midi.h"
#include "fx.h"
#include "params.h"
#include "synth.h"
#include "core_config.h"

namespace {
const uint8_t PAR_AMT0 = 16, PAR_LID0 = 24, PAR_MIX0 = 32;   // ch6 CC bases
int16_t  SDRAM_BSS s_eng[FX_TOTAL];       // last engage value sent/received per FX id (-1 = not armed)
uint32_t SDRAM_BSS s_eng_t[FX_TOTAL];     // when (ms)
int16_t  s_perf[16]; int s_oct14 = 8192;   // ch6 perform-state CCs 2,4,8..12 (-1 = baseline unset) + 14 bit octave (CC3 MSB / CC35 LSB)
int16_t  s_amt = -1; uint32_t s_amt_t = 0;
int16_t  SDRAM_BSS s_par[48]; uint32_t SDRAM_BSS s_par_t[48];  // ch6 CC 16.. (value 0..127)
float    s_tok = 16.f; uint32_t s_tok_t = 0; bool s_tok_init = false;
bool     s_armed = false;
// schema index behind each ch6 param CC: pmorParamGet() is a linear scan of the ~490-entry schema, and fxMidiPoll runs ~23 of them per call
// (measured 2026-11-11: ~1.1 ms per poll, 22% of the main loop at the 5 ms gate). Resolved once per session, read by index afterwards.
int16_t  s_par_idx[48]; size_t s_idx_count = 0;

uint8_t q127(float v) { int x = (int)(v * 127.f + 0.5f); return (uint8_t)(x < 0 ? 0 : x > 127 ? 127 : x); }
// schema id behind a ch6 param CC, or 0
uint16_t parId(uint8_t cc) {
    if (cc >= PAR_AMT0 && cc < PAR_AMT0 + 8)  return pmorParamId(PD_FX, (uint16_t)(130 + cc - PAR_AMT0));
    if (cc >= PAR_LID0 && cc < PAR_LID0 + 8)  return pmorParamId(PD_FX, (uint16_t)(140 + cc - PAR_LID0));
    if (cc > PAR_MIX0 && cc < PAR_MIX0 + 8 && cc != 35) return pmorParamId(PD_FX, (uint16_t)(150 + cc - PAR_MIX0));
    return 0;
}
bool take(uint32_t now) {   // token bucket: FXMIDI_RATE_HZ sustained, 16 burst
    if (!s_tok_init) { s_tok_init = true; s_tok_t = now; }
    s_tok += (float)(uint32_t)(now - s_tok_t) * (FXMIDI_RATE_HZ / 1000.f); s_tok_t = now;
    if (s_tok > 16.f) s_tok = 16.f;
    if (s_tok < 1.f) return false;
    s_tok -= 1.f; return true;
}
}  // namespace

static void resolveParIdx() {
    for (int i = 0; i < 48; i++) s_par_idx[i] = -1;
    for (uint8_t cc = PAR_AMT0; cc < PAR_MIX0 + 8; cc++) { uint16_t id = parId(cc); if (id) s_par_idx[cc] = (int16_t)pmorSchemaIndexOf(id); }
    s_idx_count = pmorSchemaCount();
}
static uint8_t perfNow(uint8_t cc) {
    switch (cc) { case 2: return q127(fx.tilt_t); case 4: return q127(syn.perf_env_macro);
                  case 8: return q127(fx.perf_up); case 9: return q127(fx.perf_right); case 10: return q127(fx.perf_down); case 11: return q127(fx.perf_left);
                  default: return (uint8_t)(fx.perf_engine < 3 ? fx.perf_engine : 127); }
}
static int octave14() { float o = syn.perf_octave < -2.f ? -2.f : syn.perf_octave > 2.f ? 2.f : syn.perf_octave; return (int)(8192.f + o * 4095.5f + 0.5f); }
void fxMidiReset(void) {
    s_idx_count = 0;
    for (int i = 0; i < FX_TOTAL; i++) { s_eng[i] = -1; s_eng_t[i] = 0; }
    s_amt = -1; for (int i = 0; i < 48; i++) { s_par[i] = -1; s_par_t[i] = 0; }
    for (int i = 0; i < 16; i++) s_perf[i] = -1; s_oct14 = 8192;
    s_armed = false;
}

void fxMidiPoll(uint32_t now, FxMidiCc out) {
    if (s_idx_count != pmorSchemaCount()) resolveParIdx();
    if (!s_armed) {   // first call of a session: record the baseline, send nothing
        uint16_t eff[8]; float ef; fxEffectiveAll(eff, &ef);
        for (int b = 0; b < FX_BANKS; b++) { uint16_t m = eff[b]; for (int k = 0; k < 16; k++) s_eng[b * 16 + k] = (m >> k) & 1u ? 127 : 0; }
        s_amt = q127(ef);
        for (uint8_t cc = PAR_AMT0; cc < PAR_MIX0 + 8; cc++) { float v; if (s_par_idx[cc] >= 0 && pmorParamGetAt((size_t)s_par_idx[cc], &v)) s_par[cc] = q127(v); }
        { static const uint8_t pc[] = {2,4,8,9,10,11,12}; for (uint8_t c : pc) s_perf[c] = perfNow(c); s_oct14 = octave14(); }
        s_armed = true; return;
    }
    uint16_t eff[8]; float ef; fxEffectiveAll(eff, &ef);
    for (int b = 0; b < FX_BANKS; b++) {
        uint16_t m = eff[b];
        for (int k = 0; k < 16; k++) {
            int id = b * 16 + k; int16_t want = (m >> k) & 1u ? 127 : 0;
            if (s_eng[id] == want) continue;
            if (!take(now)) return;
            s_eng[id] = want; s_eng_t[id] = now; out(FXMIDI_CH_ENGAGE, (uint8_t)id, (uint8_t)want);
        }
    }
    int16_t a = q127(ef);
    int d = (int)a - (int)s_amt;
    if (d >= 2 || d <= -2 || (d != 0 && (a == 0 || a == 127))) { if (!take(now)) return; s_amt = a; s_amt_t = now; out(FXMIDI_CH_PARAM, 1, (uint8_t)a); }
    for (uint8_t cc = PAR_AMT0; cc < PAR_MIX0 + 8; cc++) {
        float v; if (s_par_idx[cc] < 0 || !pmorParamGetAt((size_t)s_par_idx[cc], &v)) continue;
        int16_t w = q127(v); if (w == s_par[cc]) continue;
        if (!take(now)) return;
        s_par[cc] = w; s_par_t[cc] = now; out(FXMIDI_CH_PARAM, cc, (uint8_t)w);
    }
    {   // perform state for the host (ch6): changes only, deadband 2 / exact at the ends, same bucket
        static const uint8_t pc[] = {2, 4, 8, 9, 10, 11, 12};
        for (uint8_t c : pc) {
            uint8_t v = perfNow(c); int dd = (int)v - (int)s_perf[c];
            if (c == 12 ? dd == 0 : !(dd >= 2 || dd <= -2 || (dd != 0 && (v == 0 || v == 127)))) continue;
            if (!take(now)) return;
            s_perf[c] = v; out(FXMIDI_CH_PARAM, c, v);
        }
        int o14 = octave14();
        if (o14 != s_oct14) { if (!take(now)) return; if (!take(now)) return; s_oct14 = o14; out(FXMIDI_CH_PARAM, 3, (uint8_t)(o14 >> 7)); out(FXMIDI_CH_PARAM, 35, (uint8_t)(o14 & 127)); }
    }
}

void fxMidiIn(uint32_t now, uint8_t ch, uint8_t cc, uint8_t val) {
    if (!s_armed) return;
    if (ch == FXMIDI_CH_ENGAGE) {
        if (cc >= FX_TOTAL) return;
        if ((uint32_t)(now - s_eng_t[cc]) < FXMIDI_ECHO_MS) return;                 // our own send coming back
        int16_t v = val >= 64 ? 127 : 0;
        if (s_eng[cc] == v) return;                                               // already in that state (echo of the loop playing, or a repeat)
        s_eng[cc] = v; s_eng_t[cc] = now;                                         // received: never re-sent
        fxExtSet((uint8_t)cc, v != 0);                                            // overrides + pauses PMOR's loop for this FX; not recorded (recording reads fx.held only)
    } else if (ch == FXMIDI_CH_PARAM) {
        if (cc == 1) {
            if ((uint32_t)(now - s_amt_t) < FXMIDI_ECHO_MS) return;
            if (val == s_amt) return;
            s_amt = val; s_amt_t = now; fx.ext_fsr = (float)val / 127.f; return;
        }
        uint16_t id = parId(cc); if (!id) return;
        if ((uint32_t)(now - s_par_t[cc]) < FXMIDI_ECHO_MS) return;
        if (val == s_par[cc]) return;
        s_par[cc] = val; s_par_t[cc] = now;
        pmorParamSet(id, (float)val / 127.f);
    }
}
