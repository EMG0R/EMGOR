// THE control map (docs/V1_MAP_AUDIT.md + the user's decisions of 2026-11-17), continuous controls -> effect params. Main loop only (~200 Hz), never the audio callback.
// Sensors NEVER switch an effect on any more: on/off is a pad (or the web UI); a sensor only drives a param of an effect that is already on.
//   accel Y   -> reverb WARP (v1 scale 0..127 -> -15..142 clipped, slope 1.236); feedback fixed
//   pot 1     -> roar DRIVE (pad 2 switches the roar)
//   FSR       -> delay TIME ALWAYS: 600 ms x sqrt(FSR) (absDelay `scale 0. 1. 0. 60. 0.5` = 60 x x^0.5, x 0.01 s; min 48 ms), tape-style repitch glide inside the delay (SMOOTH / 3 ms)
//   lidar     -> flanger MIX, inverted as v1 (far = more, 56..333 mm) (pad 1 switches it); autopan DEPTH and PERIOD 200..600 ms (pad 15)
//   joystick  -> MOOD transpose index (X) and mood probability (Y) (pad 6 switches it)
//   encoder   -> either button toggles the HARMONY page; in harmony mode encoder 1 sets the harmony mix; in the FX page rotation does nothing
//   faders    -> kFaderFx (fx_glue_chain.h). Reverb, grain and drone are EXCLUSIVE: the last fader to rise above 0 wins, the active one fades out, and a displaced effect cannot come
//                back until its fader has been at 0 once.
//   freeze    -> pad 3: FREEZE factor ramps 100 -> 0 over 2.3 s (v1 `clearer`), then the freeze switches off and the factor is back at 100
#pragma once
static int g_chPot1 = -2, g_chFsr = -2;
static int g_joyMode = 0, g_encSeen = 0; static uint32_t g_encPressSeen = 0, g_encPress2Seen = 0; static bool g_encInit = false;
static int pidx(int gi, const char* n) { return g_fxg[gi].ok ? g_fxg[gi].fx->findParam(n) : -1; }
static void faderApply(int gi, float v) { if(gi == GI_DRONE) drGlueControls(v); else g_extmix[gi] = v; }
static void fxControlsTick(const uint16_t* lin, uint32_t now) {
    if(g_chPot1 == -2) { g_chPot1 = g_chFsr = -1; for(int i = 0; i < NCH; i++) { if(!strcmp(kCh[i].name, "POT1")) g_chPot1 = i; if(!strcmp(kCh[i].name, "FSR")) g_chFsr = i; } }
    // faders: the one table, with the reverb / grain / drone exclusivity
    {
        static float prev[3] = {0, 0, 0};
        for(int k = 0; k < 3; k++) {
            const int gi = kFaderFx[k]; if(gi < 0) { prev[k] = g_fs[k]; continue; }
            const float v = g_fs[k]; const bool excl = (gi == GI_REVERB || gi == GI_GRAIN || gi == GI_DRONE); float tgt = v;
            if(excl) {
                const uint8_t bit = (uint8_t)(1u << k);
                if(v == 0.0f) { g_exBlocked &= (uint8_t)~bit; if(g_exOwner == gi) g_exOwner = -1; }                       // at 0: re-armed, and it no longer owns
                else if(!(g_exBlocked & bit)) {
                    if(prev[k] == 0.0f || g_exOwner == -1) {                                                                // rising edge (or nothing active): this one takes over
                        if(g_exOwner != -1 && g_exOwner != gi) for(int j = 0; j < 3; j++) if(kFaderFx[j] == g_exOwner) g_exBlocked |= (uint8_t)(1u << j);   // the displaced one needs its fader at 0 once
                        g_exOwner = gi;
                    }
                }
                tgt = (g_exOwner == gi && !(g_exBlocked & bit)) ? v : 0.0f;
            }
            prev[k] = v; faderApply(gi, tgt);
        }
    }
    // accel -> reverb warp
    { float t = ((g_simAccel ? g_simAy : g_dly_ay) + 1.0f) * 0.5f; t = t < 0 ? 0 : t > 1 ? 1 : t; float w = 1.236f * t - 0.118f; g_rv_warp_t = w < 0 ? 0 : w > 1 ? 1 : w; }
    // pot 1 -> roar drive (the roar itself is switched by pad 2)
    if((g_chPot1 >= 0 || g_simPot[0] >= 0) && g_fxg[GI_ROAR].ok) {
        static int iD = -2; if(iD == -2) iD = pidx(GI_ROAR, "DRIVE");
        const float p = g_simPot[0] >= 0 ? g_simPot[0] * 0.001f : (float)lin[g_chPot1] / 65535.0f; g_cv[0] = p;
        if(iD >= 0) g_fxg[GI_ROAR].fx->v[iD] = p;
    }
    // FSR -> delay time, always
    if(g_chFsr >= 0 || g_simFsr >= 0) {
        static int iT = -2; if(iT == -2) iT = pidx(GI_DELAY, "TIME");
        const float f = g_simFsr >= 0 ? g_simFsr * 0.001f : (float)lin[g_chFsr] / 65535.0f; g_cv[1] = f;
        if(iT >= 0) { float ms = 600.0f * sqrtf(f < 0 ? 0 : f); g_fxg[GI_DELAY].fx->v[iT] = ms < 48.0f ? 48.0f : ms; }
    }
    // lidar (inverted as v1: far = more) -> flanger mix and autopan depth / period; no valid lidar -> 0.5. The effects are switched by pads 1 and 15.
    { const bool ok = g_simLid >= 0 || (g_lid_st == 9 && (now - g_lid_ms) < 600); const float mm = g_simLid >= 0 ? g_simLid : (float)g_lid_mm; float m = ok ? (mm - 56.0f) / (333.0f - 56.0f) : 0.5f; m = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m; if(m < 0.02f) m = 0.0f;
      g_fxg[GI_FLANGE].mix = m; g_cv[2] = m;
      static int iAD = -2, iAP = -2; if(iAD == -2) { iAD = pidx(GI_AUTOPAN, "DEPTH"); iAP = pidx(GI_AUTOPAN, "PERIOD"); }
      if(g_fxg[GI_AUTOPAN].ok && iAD >= 0 && iAP >= 0) { g_fxg[GI_AUTOPAN].fx->v[iAD] = m; g_fxg[GI_AUTOPAN].fx->v[iAP] = 200.0f + 400.0f * (1.0f - m); } }
    // joystick -> MOOD (switched by pad 6)
    {
        static int iP = -2, iM = -2; if(iP == -2) { iP = pidx(GI_MOOD, "PITCH"); iM = pidx(GI_MOOD, "MOOD"); }
        const float x = (g_simJoy ? (float)g_simJx : (float)g_jn_out[0]) * 0.001f, y = (g_simJoy ? (float)g_simJy : (float)g_jn_out[1]) * 0.001f; g_cv[3] = (x + 1.0f) * 0.5f; g_cv[4] = (y + 1.0f) * 0.5f;
        FxG& m = g_fxg[GI_MOOD]; if(m.ok) { if(iP >= 0) m.fx->v[iP] = floorf((x + 1.0f) * 4.0f + 0.5f); if(iM >= 0) { const float pr = (y + 1.0f) * 0.5f; m.fx->v[iM] = pr > 0.0f ? (pr < 1.0f ? pr : 1.0f) : 0.0f; } }   // probability clamped 0..1, never -0.0
    }
    // encoders: either button toggles the HARMONY page; encoder 1 rotation = harmony mix while on that page
    {
        const int det = g_enc[0].cnt / 4 + g_simEncDet; const uint32_t p1 = g_enc[0].presses + g_simEncPress, p2 = g_enc[1].presses + g_simEnc2Press;
        if(!g_encInit) { g_encInit = true; g_encSeen = det; g_encPressSeen = p1; g_encPress2Seen = p2; }
        if(p1 != g_encPressSeen) { g_encPressSeen = p1; g_harmMode = !g_harmMode; }
        if(p2 != g_encPress2Seen) { g_encPress2Seen = p2; g_harmMode = !g_harmMode; }
        if(det != g_encSeen) { const int d = det - g_encSeen; g_encSeen = det; if(g_harmMode && g_fxg[GI_HARMONY].ok) { float mx = g_fxg[GI_HARMONY].mix + 0.05f * (float)d; g_fxg[GI_HARMONY].mix = mx < 0 ? 0 : mx > 1 ? 1 : mx; } }
        g_cv[6] = g_fxg[GI_HARMONY].mix; g_cv[7] = g_harmMode ? 1.0f : 0.0f;
    }
    // pad 3: freeze factor ramp
    {
        static int iZ = -2; if(iZ == -2) iZ = pidx(GI_FREEZE, "FREEZE");
        FxG& z = g_fxg[GI_FREEZE];
        if(z.ok && iZ >= 0) {
            if(g_frzClearMs) { float el = (float)(now - g_frzClearMs) / 2300.0f; if(el >= 1.0f) { g_frzClearMs = 0; z.fx->v[iZ] = 100.0f; z.latch = false; g_cv[5] = 0; } else { z.fx->v[iZ] = 100.0f * (1.0f - el); g_cv[5] = 1.0f - el; } }
            else { z.fx->v[iZ] = 100.0f; g_cv[5] = 0; }
        }
    }
}
