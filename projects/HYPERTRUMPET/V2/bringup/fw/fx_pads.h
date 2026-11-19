// NeoTrellis pads -> effects: the v1 map exactly as audited in docs/V1_MAP_AUDIT.md (pad n = key n-1). Table: docs/TRELLIS_MAP.md.
// Runs in the main loop (lightsPoll, before the NeoTrellis LED frame is built): edge detection on g_keys, actions queued to the audio thread, LEDs written to g_tr[].
//   Pads 1, 2, 6, 15, 16 do nothing in v1: they are reserved, lit dim. Latch pads toggle on RELEASE (< 600 ms); a press of 600 ms or more on any effect pad FOCUSES that
//   effect (the web UI selects its panel) without toggling. Pad 4 (freeze let-in) acts on the press. Looper pads: tap = action, hold 500 ms = the second action.
//   Every turn-on goes through fxAllowOn(): if the projected total CPU peak would exceed 80 % the pad blinks red and the effect stays off.
#pragma once
static const uint32_t PAD_LONG_MS = 600, PAD_HOLD_MS = 500;
static const int8_t kPadFx[16] = { GI_FLANGE, GI_ROAR, GI_FREEZE, GI_FREEZE, GI_RING, GI_MOOD, GI_BLUR, GI_CHORUS, GI_DELAY, GI_DELAY, GI_DELAY, GI_DELAY, GI_LOOPER, GI_LOOPER, GI_AUTOPAN, GI_WAH };   // v1 pads 3,4,5,7..14 as audited; 1 2 6 15 16 (empty in v1) = on/off latches for the sensor-only effects
static uint16_t g_pprev = 0; static uint32_t g_ppress[16]; static bool g_plong[16]; static float g_pb[16];
static uint32_t g_loopEvt = 0, g_pledT = 0;           // g_dmode / g_dsmooth / g_frzClearMs live in fx_glue_chain.h (shared with fx_controls.h)
static inline int padFx(int k, bool) { return kPadFx[k]; }
static bool padTurnOn(int gi, uint32_t now) {                                 // true = allowed (and the caller switches it on)
    if(fxIsOn(gi)) return true;
    if(fxAllowOn(gi)) return true;
    g_fxg[gi].refuseMs = now + 700; return false;                            // blink red
}
static void padLatch(int gi, uint32_t now) {
    FxG& g = g_fxg[gi]; if(!g.ok) { g.refuseMs = now + 700; return; }
    if(g.latch) { g.latch = false; return; }
    if(!padTurnOn(gi, now)) return;
    g.latch = true;
}
static void padDelayMode(int mode, uint32_t now) {                          // 1 = pad 9 plain on, 2 = pad 11 preset A (feedback 0.787, mix 1.0), 3 = pad 12 preset B (mix 0.31, feedback 0.24, time 412 ms then the FSR). Exclusive.
    FxG& g = g_fxg[GI_DELAY]; if(!g.ok) { g.refuseMs = now + 700; return; }
    if(g_dmode == mode) { g_dmode = 0; g.latch = false; return; }
    if(!g.latch && !padTurnOn(GI_DELAY, now)) return;
    hx::FxDelay* d = static_cast<hx::FxDelay*>(g.fx);
    if(mode == 2) { g.mix = 1.0f; d->setParam(hx::FxDelay::P_FB, 0.787f); }
    else if(mode == 3) { g.mix = 0.31f; d->setParam(hx::FxDelay::P_FB, 0.24f); d->setParam(hx::FxDelay::P_TIME, 412.0f); }
    g_dmode = mode; g.latch = true;
}
// ---- HARMONY page: 16 pads = intervals (kHarmSemi), latching, at most 3 voices, the 4th press steals the oldest
static void harmApply() {
    FxG& h = g_fxg[GI_HARMONY]; if(!h.ok) return; hx::FxHarmony* hm = static_cast<hx::FxHarmony*>(h.fx); bool any = false;
    for(int s = 0; s < 3; s++) { const int p = g_hvPad[s]; hm->setSlot(s, p >= 0 ? kHarmSemi[p] : 0); if(p >= 0) any = true; }
    h.latch = any;
}
static void padHarmony(int k, uint32_t now) {
    FxG& h = g_fxg[GI_HARMONY]; if(!h.ok) { h.refuseMs = now + 700; return; }
    for(int s = 0; s < 3; s++) if(g_hvPad[s] == k) { g_hvPad[s] = -1; harmApply(); return; }                      // selected: toggle off
    int slot = -1; for(int s = 0; s < 3; s++) if(g_hvPad[s] < 0) { slot = s; break; }
    if(slot < 0) { uint32_t oldest = 0xFFFFFFFFu; for(int s = 0; s < 3; s++) if(g_hvAge[s] < oldest) { oldest = g_hvAge[s]; slot = s; } }   // 4th press steals the oldest
    bool anyBefore = false; for(int s = 0; s < 3; s++) if(g_hvPad[s] >= 0) anyBefore = true;
    if(!anyBefore && !fxIsOn(GI_HARMONY) && !fxAllowOn(GI_HARMONY)) { h.refuseMs = now + 700; return; }            // admission: only when it would switch the harmony on
    g_hvPad[slot] = k; g_hvAge[slot] = ++g_hvClock; harmApply();
}
static void padPress(int k, uint32_t now, bool) {
    if(g_harmMode) { g_ppress[k] = now; g_plong[k] = true; padHarmony(k, now); return; }                            // harmony page: latch on the press, no focus / long press
    g_ppress[k] = now; g_plong[k] = false;
    if(k == 3) {                                                                                                   // pad 4: freeze let-in, held
        FxG& g = g_fxg[GI_FREEZE]; if(g.ok && padTurnOn(GI_FREEZE, now)) { g.latch = true; fxgAct(GI_FREEZE, hx::A_MAIN, 1.0f); } else g.refuseMs = now + 700;
    }
}
static void padRelease(int k, uint32_t now, bool) {
    if(g_harmMode) return;
    const uint32_t dur = now - g_ppress[k]; const int gi = kPadFx[k]; const bool shortP = dur < PAD_LONG_MS;
    switch(k) {
    case 3: fxgAct(GI_FREEZE, hx::A_MAIN, 0.0f); break;                                                           // gate closed again: what was let in stays frozen
    case 2: if(shortP) { if(fxIsOn(GI_FREEZE) && !g_frzClearMs) g_frzClearMs = now ? now : 1; } else g_focus = GI_FREEZE; break;   // pad 3: freeze clear, 2.3 s ramp (fx_controls.h)
    case 9: if(shortP) { g_dsmooth = !g_dsmooth; fxgAct(GI_DELAY, hx::A_ALT, 1.0f); } else g_focus = GI_DELAY; break;   // pad 10: smooth-mode latch
    case 8: if(shortP) padDelayMode(1, now); else g_focus = GI_DELAY; break;                                       // pad 9: delay on
    case 10: if(shortP) padDelayMode(2, now); else g_focus = GI_DELAY; break;                                      // pad 11: preset A
    case 11: if(shortP) padDelayMode(3, now); else g_focus = GI_DELAY; break;                                      // pad 12: preset B
    case 12: case 13: if(!g_plong[k]) {                                                                           // looper tap: pad 13 record/overdub, pad 14 play/stop
        FxG& g = g_fxg[GI_LOOPER]; if(!g.ok) { g.refuseMs = now + 700; break; }
        if(!g.latch) { if(!padTurnOn(GI_LOOPER, now)) break; g.latch = true; }
        fxgAct(GI_LOOPER, k == 12 ? hx::A_MAIN : hx::A_ALT, 1.0f); g_loopEvt = now; } break;
    default:
        if(gi >= 0) { if(!shortP) g_focus = gi; else padLatch(gi, now); }                                          // ring (5), blur (7), chorus (8): mix latches
        break;                                                                                                     // reserved pads: nothing
    }
}
static void padLeds(uint32_t now, float dt, bool layerB) {
    const float t = (float)now * 0.001f; hx::FxLooper* lp = static_cast<hx::FxLooper*>(g_fxg[GI_LOOPER].fx);
    for(int k = 0; k < 16; k++) {
        if(g_harmMode) {                                                       // HARMONY page palette: warm rows (amber, coral, magenta, cyan), dim = free, bright = voice
            static const float pal[4][3] = { {0.61f, 0.52f, 1.0f}, {0.61f, 0.52f, 1.0f}, {0.61f, 0.52f, 1.0f}, {0.61f, 0.52f, 1.0f} };   // harmony page: one violet, matching the phone map card
            bool sel = false; for(int s2 = 0; s2 < 3; s2++) if(g_hvPad[s2] == k) sel = true;
            const float fl2 = g_flash[k] > 1.0f ? 1.0f : g_flash[k]; const float tg = (sel ? 0.95f : 0.13f) + 0.4f * fl2; g_pb[k] += (tg - g_pb[k]) * (1.0f - expf(-dt / 0.05f)); float bb = g_pb[k] > 1.0f ? 1.0f : g_pb[k];
            const float* c2 = pal[k >> 2]; g_tr[k].r = c2[0] * bb + 0.02f; g_tr[k].g = c2[1] * bb + 0.01f; g_tr[k].b = c2[2] * bb + 0.03f; if(g_tr[k].r > 1) g_tr[k].r = 1; if(g_tr[k].g > 1) g_tr[k].g = 1; if(g_tr[k].b > 1) g_tr[k].b = 1;
            continue;
        }
        const int gi = kPadFx[k]; float c[3] = { 0.60f, 0.45f, 1.0f }, tgt = 0.10f;                                         // reserved pads: dim lilac
        if(gi >= 0) { const FxDef* d = g_fxg[gi].d; if(d) { c[0] = d->rgb[0] / 255.0f; c[1] = d->rgb[1] / 255.0f; c[2] = d->rgb[2] / 255.0f; } }
        FxG* g = gi >= 0 ? &g_fxg[gi] : nullptr; const bool held = ((g_simPadOn ? g_simKeys : g_keys) >> k) & 1; const bool on = g && fxIsOn(gi);
        if(gi < 0) tgt = 0.10f;
        else if(k == 3) tgt = held ? 0.95f : (on ? 0.40f : 0.12f);                                                     // freeze let-in
        else if(k == 2) tgt = g_frzClearMs ? 0.30f + 0.5f * g_cv[5] : (on ? 0.55f : 0.15f);                            // freeze clear: shows the ramp
        else if(k == 8) tgt = (g_dmode == 1) ? 0.90f : 0.14f;                                                          // delay on
        else if(k == 9) tgt = g_dsmooth ? 0.60f : 0.12f;                                                               // smoothing mode
        else if(k == 10) tgt = (g_dmode == 2) ? 0.90f : 0.14f;
        else if(k == 11) tgt = (g_dmode == 3) ? 0.90f : 0.14f;
        else if(k == 12) { int s = lp ? lp->state() : 0; tgt = (s == hx::FxLooper::S_REC1 || s == hx::FxLooper::S_OVR) ? (fmodf(t, 0.5f) < 0.25f ? 1.0f : 0.35f) : (s == hx::FxLooper::S_EMPTY ? 0.14f : 0.45f); }
        else if(k == 13) { int s = lp ? lp->state() : 0; tgt = (s == hx::FxLooper::S_PLAY || s == hx::FxLooper::S_OVR) ? 0.85f : 0.14f; }
        else if(g) tgt = fxIsTail(gi) ? 0.25f + 0.6f * fxWet(gi) : (on ? 0.88f + 0.08f * sinf(t * 1.7f + k) : 0.15f);
        if(gi >= 0 && gi == g_focus) tgt += 0.10f * (0.5f + 0.5f * sinf(t * 2.6f));                         // focused effect breathes
        if(g && g->refuseMs > now) { bool blink = ((g->refuseMs - now) / 110) & 1; c[0] = 1.0f; c[1] = 0.0f; c[2] = 0.0f; tgt = blink ? 0.95f : 0.10f; g_pb[k] = tgt; }   // CPU refused / effect not available: red blink
        const float fl = g_flash[k] > 1.0f ? 1.0f : g_flash[k];                                                      // instant press feedback
        g_pb[k] += (tgt + 0.55f * fl - g_pb[k]) * (1.0f - expf(-dt / 0.06f));
        float b = g_pb[k] > 1.0f ? 1.0f : g_pb[k];
        g_tr[k].r = c[0] * b + 0.03f * (1.0f - b) + 0.5f * fl * b; g_tr[k].g = c[1] * b + 0.008f * (1.0f - b) + 0.5f * fl * b; g_tr[k].b = c[2] * b + 0.06f * (1.0f - b) + 0.5f * fl * b;   // dark violet floor
        if(g_tr[k].r > 1) g_tr[k].r = 1; if(g_tr[k].g > 1) g_tr[k].g = 1; if(g_tr[k].b > 1) g_tr[k].b = 1;
    }
}
static void padPoll(uint32_t now) {
    static uint32_t lastLed = 0; const bool layerB = false;
    const uint16_t cur = g_simPadOn ? g_simKeys : g_keys; uint16_t rise = cur & ~g_pprev, fall = ~cur & g_pprev; g_pprev = cur;
    for(int k = 0; k < 16; k++) { if(rise & (1u << k)) padPress(k, now, layerB); if(fall & (1u << k)) padRelease(k, now, layerB); }
    for(int k = 12; k <= 13 && !g_harmMode; k++) if(((cur >> k) & 1) && !g_plong[k] && now - g_ppress[k] >= PAD_HOLD_MS) {                  // looper hold: pad 13 clear all, pad 14 stop
        g_plong[k] = true; FxG& g = g_fxg[GI_LOOPER]; if(!g.ok) continue;
        if(!g.latch) { if(!padTurnOn(GI_LOOPER, now)) continue; g.latch = true; }
        fxgAct(GI_LOOPER, k == 12 ? hx::A_CLEAR : hx::A_HOLD, 1.0f); g_loopEvt = now;
    }
    for(int k = 0; k < 16 && !g_harmMode; k++) if(((cur >> k) & 1) && k != 12 && k != 13 && kPadFx[k] >= 0 && !g_plong[k] && now - g_ppress[k] >= PAD_LONG_MS) { g_plong[k] = true; const int gi = padFx(k, layerB); if(gi >= 0) g_focus = gi; }   // focus as soon as the press is long enough
    FxG& L = g_fxg[GI_LOOPER];                                                                                           // looper switches itself off once it is empty (CPU gate), 2 s after the last pad event
    if(L.ok && L.latch && now - g_loopEvt > 2000) { hx::FxLooper* lp = static_cast<hx::FxLooper*>(L.fx); if(lp->layers() == 0 && lp->state() == hx::FxLooper::S_EMPTY) L.latch = false; }
    if(now - lastLed >= 16) { float dt = (now - lastLed) * 0.001f; if(dt > 0.1f) dt = 0.1f; lastLed = now; padLeds(now, dt, layerB); }
}
