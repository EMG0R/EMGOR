// TEST 12 control model (user 2026-11-17): a Trellis pad toggles an effect AND selects it; fader 1 is the dry/wet of the SELECTED effect; faders 2 and 3 do nothing.
//   pad on an off effect          -> effect on + selected (soft takeover armed)
//   pad on the selected, on effect -> effect off (it tails out through its CPU gate, zero cost at idle); it stays selected
//   pad on an on, unselected effect -> selected only
//   soft takeover: the mix does not jump to the fader; it follows once the fader has crossed (or is within 2 % of) the effect's current mix
// Adding effect #2 = one row in kPads[] (+ one case in peDrive()). Main loop only (~200 Hz).
#pragma once
// kind: 0 toggle (tap = on + select / off), 1 freeze GATE (hold = let sound in), 2 freeze RESET (fade the frozen sound out), 3 looper START (tap record/close/overdub, hold clear all), 4 looper PLAY/STOP
enum { PK_TOGGLE = 0, PK_FGATE, PK_FRESET, PK_LSTART, PK_LPLAY, PK_BLOOP };
struct PadFx { int8_t gi; const char* name; uint8_t rgb[3]; uint8_t kind; };
#define NOPAD { -1, "", {0,0,0}, 0 }
static const PadFx kPads[16] = {
    { GI_RING, "RINGMOD", {255, 154, 77}, PK_TOGGLE }, { GI_BLUR, "TRACING", {63, 207, 156}, PK_TOGGLE }, { GI_WAH, "AUTO-WAH", {255, 111, 176}, PK_TOGGLE }, { GI_FLANGE, "MOD BLOCK", {255, 111, 176}, PK_TOGGLE },   // pad 4: chorus / phaser / flanger (encoder 1 = mode)                                   // top row: 1 chorus (2 tracing, 3 flanger, 4 auto-wah follow when they pass)
    { GI_ROAR, "GLITCH", {200, 255, 60}, PK_TOGGLE }, { GI_MOODII, "MOOD", {220, 60, 200}, PK_TOGGLE }, { GI_AUTOPAN, "BLOOPER", {255, 170, 40}, PK_BLOOP }, { GI_CHORUS, "DRONEMAKER", {63, 207, 156}, PK_TOGGLE },   // pad 8: dronemaker (radio group)
                                         // pad 5 (second row, left): glitch, FSR = INTENS while it is selected
    { GI_FREEZE, "FREEZE GATE", {63, 207, 156}, PK_FGATE }, { GI_FREEZE, "FREEZE RESET", {63, 207, 156}, PK_FRESET },   // pads 9, 10
    NOPAD, { GI_LOOPER, "LOOPER", {232, 195, 74}, PK_LSTART },      // pad 12 only (pad 11 is free)
    { GI_DRONE, "LOOSE FOLLOW", {63, 207, 156}, PK_TOGGLE }, { GI_REVERB, "REVERB", {155, 133, 255}, PK_TOGGLE },             // bottom row: 13 drone, 14 reverb, 15 granular = radio group; 16 tape delay is independent
    { GI_GRAIN, "GRANULAR", {30, 230, 170}, PK_TOGGLE }, { GI_DELAY, "DELAY", {79, 192, 234}, PK_TOGGLE } };
static volatile bool g_nosave = false;                          // `nosave on|off`: soak / sim runs must never persist params (see fxParamsPoll)
static volatile uint16_t g_padReq = 0;                           // UI / sim pad taps (bit n-1), consumed by the tick
static bool g_peOn[GLUE_N] = {false}; static float g_peMix[GLUE_N] = {0};
static volatile int g_sel = -1;                                  // selected pad index (0..15) or -1
// FADER PICKUP (user 2026-11-17): selecting an effect snapshots the 3 fader positions (calibrated 0..1). A fader controls its mapped param only after it has moved >= 15 % of travel away from
// that snapshot; until then the param keeps its stored / default value (a parked or jittering fader never overwrites anything). Once picked up it follows directly. Re-armed on every selection.
// PER-EFFECT FADER MAP: kFMap[registry index][fader 0..2] = param name, "" = none. Default: fader 1 = MIX, faders 2 / 3 none. (auto-wah: SENSITIVITY / RANGE / Q when fx_wah.h lands)
static const float PICKUP = 0.15f;
static float g_snap[3] = {0, 0, 0}; static uint8_t g_live = 0;        // g_live: bit n = fader n+1 picked up
static const char* kWahMap[3] = { "SENS", "RANGE", "Q" }; static const char* kBlpMap[3] = { "PROB", "SIZE", "DIST" }; static const char* kMoodMap[3] = { "MIX", "LENGTH", "GEN" };
static const char* faderParam(int gi, int n) { if(gi == GI_WAH) return kWahMap[n]; if(gi == GI_AUTOPAN) return kBlpMap[n]; if(gi == GI_MOODII) return kMoodMap[n]; return n == 0 ? "MIX" : ""; }   // THE per-effect fader table: add a case (or a row) per effect; default fader 1 = MIX, faders 2 / 3 none
static uint16_t g_padPrev = 0;
static void peDrive(int gi, float mix, bool on) {
    if(gi == GI_DRONE) drGlueControls(on ? mix : 0.0f);
    else { FxG& g = g_fxg[gi]; if(!g.ok) return; if(on) g.mix = mix; g.latch = on; }     // the template gate: latch = wanted, g.mix = the user's wet level (the MIX param)
}   // the one place that maps an effect's mix to its glue
static void padSelect(int k) { g_sel = k; for(int n = 0; n < 3; n++) g_snap[n] = g_fs[n]; g_live = 0; }
static uint32_t g_refuseUntil[16]; static const char* g_why = ""; static uint32_t g_whyUntil = 0;   // CPU / memory refusal: the pad blinks red and the UI is told why
// ---- RUNTIME GUARD (ported from the full build; budget = the device-measured worst-block table of 2026-11-17).
// Admission: refuse a turn-on when (worst block of the last ~2 s) + (this effect's worst block: max(table, measured at run time)) would pass 70 %.
// Shed: the audio thread raises g_shedReq when 8 of the last 32 blocks were over 75 % or one block over 90 %; the main loop then switches the MOST RECENTLY ENABLED effect off (it fades out through its
// CPU gate), blinks its pad red, refuses it for 5 s, and says why in telemetry (`why`, `shed:[count, last registry index]`). At most one shed per 150 ms, repeated until the requests stop.
static const float kAdmitPk[GLUE_N] = { 0.150f, 0.155f, 0.132f /* [2] = GI_MOODII, device worst 13.2 % */, 0.020f, 0.154f, 0.112f, 0.085f, 0.158f /* [7] = GI_CHORUS slot = dronemaker (host worst, max settings) */, 0.0f, 0.150f, 0.047f, 0.073f, 0.023f, 0.038f, 0.0f, 0.220f };   // registry order: reverb drone - looper freeze trace mod(flange slot) mod - grain delay glitch ring wah - harmony
static volatile float ADMIT_LIMIT = 0.70f;                       // `sim admit <percent>` changes it for tests
static uint32_t g_enabledAt[GLUE_N], g_shedUntil[GLUE_N]; static volatile uint32_t g_shedCount = 0; static volatile int g_shedLast = -1; static char g_whyBuf[48];
static const char* admit(int gi, uint32_t now) {
    if(g_shedUntil[gi] > now) return "refused: shed a moment ago (cpu)";
    float est = kAdmitPk[gi]; if(gi != GI_DRONE && g_fxg[gi].ok && g_fxg[gi].pk > est) est = g_fxg[gi].pk;
    const float proj = g_cpu_pk2 + est;
    if(proj > ADMIT_LIMIT) { snprintf(g_whyBuf, sizeof(g_whyBuf), "refused: cpu %d%% projected", (int)(proj * 100.0f + 0.5f)); return g_whyBuf; }
    return nullptr;
}
static volatile uint32_t g_refuseCount = 0;
static bool padRefuse(int k, const char* why, uint32_t now) { if(!why) return false; g_refuseCount = g_refuseCount + 1; g_refuseUntil[k] = now + 900; g_why = why; g_whyUntil = now + 4000; return true; }
static volatile float g_joyStep = 0.0f; static volatile int g_joySemi = 0;                    // the stepped joystick zone (-2..+2) and its semitones
// LOOP_HOLD_MS: a press this long is a HOLD (not a click). A click (finger down to up) lasts 60-180 ms, so 350 ms is about 2x the longest normal click: the hold is recognised quickly (the pad starts fading at once)
// yet an ordinary tap can never reach it. The delete still needs the finger kept down through the 1 s fade: letting go during the fade CANCELS it, so an accidental long press loses nothing.
static const uint32_t LOOP_HOLD_MS = 350; static bool g_lpDelSim = false;
static uint32_t g_lpClickAt = 0, g_lpOvrPend = 0, g_lpDel = 0; static float g_lpMix0 = 1.0f;   // looper: pending single click (REC1), overdub after a double click, delete fade start + saved mix
static uint32_t g_frzClearMs = 0, g_loopEvt = 0; static bool g_gateSim = false;      // freeze reset fade start (ms), last looper event, freeze gate latched open by a UI / sim tap
static const int kRadio[3] = { 7, 13, 14 };                                      // radio group pads: dronemaker (8), reverb (14), granular (15); the freeze (pads 9 / 10) joins it through its own logic
static inline bool padRadio(int k) { return k == 7 || k == 13 || k == 14; }                // pads 13, 14, 15: drone / reverb / granular, one at a time (pad 16, the tape delay, stacks with everything)
// ---- HARMONY page (user 2026-11-17; chords padfx-4.12, 2026-11-19): either encoder button toggles it. Each pad is a CHORD under the played note (the trumpet stays the top voice), except three
// LATCHING single intervals (tap on / tap off, they survive leaving the page): pad 1 octave down, pad 5 fifth down, pad 12 fourth UP (the only upward pitch). Chord pads are momentary: hold = on.
// The DSP has 3 voices: latched intervals take theirs first, the most recent held chord fills what is left (its voices are listed most-defining first, so a 2-latch + chord keeps the colour tone).
// Every voicing is a moving-structure shape (sus / quartal / open fifths / triad-over-root): no minor-second rubs against the melody, so it planes in parallel with any line. Melody shown as the chord tone:
//   row 1:  [-12 L]       open 5ths -7 -12 -19 (5)   sus4 -2 -7 -12 (5)       sus2 -2 -7 -14 (9)
//   row 2:  [-7 L]        quartal -5 -10 -15 (4ths)  9sus4 -4 -9 -14 (9)      sus4add9 -3 -5 -10 (4)
//   row 3:  6/9 -5 -10 -14 (9)  major -5 -8 -12 (1)  minor -5 -9 -12 (1)      [+5 L]
//   row 4:  maj7 -4 -7 -11 (7)  lydian D/C -4 -9 -18 (#11)  min9 -4 -11 -14 (9)  6 -2 -5 -9 (6)
static const int8_t kHarmCh[16][3] = {
    {-12, 0, 0}, {-7, -12, -19}, {-2, -7, -12}, {-2, -7, -14},
    {-7, 0, 0},  {-5, -10, -15}, {-4, -9, -14}, {-3, -5, -10},
    {-5, -10, -14}, {-5, -8, -12}, {-5, -9, -12}, {5, 0, 0},
    {-4, -7, -11}, {-4, -9, -18}, {-4, -11, -14}, {-2, -5, -9} };
static const uint16_t kHarmLatch = (uint16_t)((1u << 0) | (1u << 4) | (1u << 11));
static bool g_harmMode = false; static uint16_t g_hvHeld = 0, g_hvLatch = 0; static int g_hvLast = -1; static int8_t g_hvOn[3] = {0, 0, 0}; static bool g_hvDirty = false, g_hvResync = true;
static uint32_t g_hvRel = 0; static volatile uint16_t g_padRelReq = 0; static volatile uint32_t g_simEncPress = 0;
static void harmWant(int8_t w[3]) {
    int n = 0; w[0] = w[1] = w[2] = 0;
    auto add = [&](int8_t s) { if(!s || n >= 3) return; for(int i = 0; i < n; i++) if(w[i] == s) return; w[n++] = s; };
    for(int k = 0; k < 16; k++) if(g_hvLatch & (1u << k)) add(kHarmCh[k][0]);
    int c = (g_hvLast >= 0 && (g_hvHeld & (1u << g_hvLast))) ? g_hvLast : -1;
    if(c < 0) for(int k = 0; k < 16; k++) if(g_hvHeld & (1u << k)) { c = k; break; }
    if(c >= 0) for(int i = 0; i < 3; i++) add(kHarmCh[c][i]);
}
static void harmTick(uint32_t now) {                                                     // every loop, on or off the page: drive the DSP voices to the wanted set (off first, so nothing gets stolen)
    FxG& h = g_fxg[GI_HARMONY]; if(!h.ok) return; hx::FxHarmony* hm = static_cast<hx::FxHarmony*>(h.fx);
    if(h.gate.state != FXS_ACTIVE) { g_hvResync = true; }                                 // a wake resets the effect: re-send everything once it is ACTIVE
    else if(g_hvDirty || g_hvResync) {
        int8_t w[3]; harmWant(w);
        if(g_hvResync) { hm->allOff(); g_hvOn[0] = g_hvOn[1] = g_hvOn[2] = 0; g_hvResync = false; }
        for(int i = 0; i < 3; i++) if(g_hvOn[i]) { bool keep = false; for(int j = 0; j < 3; j++) if(w[j] == g_hvOn[i]) keep = true; if(!keep) hm->voiceOff(g_hvOn[i]); }
        for(int j = 0; j < 3; j++) if(w[j]) { bool had = false; for(int i = 0; i < 3; i++) if(g_hvOn[i] == w[j]) had = true; if(!had) hm->voiceOn(w[j], 1.0f); }
        for(int i = 0; i < 3; i++) g_hvOn[i] = w[i]; g_hvDirty = false;
    }
    if(!g_hvHeld && !g_hvLatch && h.latch && now - g_hvRel > 150) h.latch = false;       // off 150 ms after the last voice goes
}
static void harmToggle(uint32_t now) {
#ifdef HX_NO_HARMPAGE
    (void)now; return;                                                        // bisect build: harmony init only, page disabled
#endif
    g_harmMode = !g_harmMode;
    if(!g_harmMode) { g_hvHeld = 0; g_hvLast = -1; g_hvDirty = true; g_hvRel = now ? now : 1; }   // leaving: held chords stop, latched intervals keep sounding
}
static void harmonyPage(uint16_t& rise, uint16_t& fall, uint16_t& req, uint16_t& hreq, uint16_t& rel, uint32_t now) {
    FxG& h = g_fxg[GI_HARMONY]; if(!h.ok) return;
    const uint16_t press = (uint16_t)(rise | hreq), release = (uint16_t)(fall | rel);
    for(int k = 0; k < 16; k++) {
        const uint16_t bit = (uint16_t)(1u << k);
        if(press & bit) {
            if(!h.latch && padRefuse(k, admit(GI_HARMONY, now), now)) continue;
            if(kHarmLatch & bit) { g_hvLatch ^= bit; if(!(g_hvLatch & bit)) g_hvRel = now ? now : 1; }
            else { g_hvHeld |= bit; g_hvLast = k; }
            if(!h.latch && (g_hvHeld | g_hvLatch)) { h.latch = true; g_enabledAt[GI_HARMONY] = now; }
            g_hvDirty = true;
        }
        if((release & bit) && !(kHarmLatch & bit)) { g_hvHeld &= (uint16_t)~bit; g_hvDirty = true; g_hvRel = now ? now : 1; }
    }
    rise = fall = req = hreq = rel = 0;
}
static void padTap(int k, uint32_t now) {                                             // PK_TOGGLE pads
    if(k < 0 || k > 15 || kPads[k].gi < 0 || kPads[k].kind != PK_TOGGLE) return;
    const int gi = kPads[k].gi;
    if(!g_peOn[gi]) {
        if(padRefuse(k, axAcquire(gi), now)) return;                              // the shared AXI slot is held by the other spectral effect
        if(padRefuse(k, admit(gi, now), now)) return;                              // projected CPU peak over 70 %
        g_enabledAt[gi] = now;
        if(padRadio(k)) {
            for(int q = 0; q < 3; q++) { const int j = kRadio[q]; if(j != k) g_peOn[kPads[j].gi] = false; }
            FxG& fz = g_fxg[GI_FREEZE]; if(fz.ok && fz.latch) { g_frzClearMs = now ? now : 1; g_gateSim = false; fxgAct(GI_FREEZE, hx::A_MAIN, 0.0f); }   // the freeze joins the radio group: it fades out and resets
        }
        g_peOn[gi] = true; padSelect(k); }
    else if(g_sel == k) g_peOn[gi] = false;
    else padSelect(k);
}
static volatile bool g_paramDirty = false;                       // a param was changed by the encoder: fxParamsPoll persists it
static int g_encSeen = 0; static bool g_encInit = false;
static void encStep(int d, uint32_t) {                            // encoder 1 acts on the SELECTED effect: delay base TIME, ringmod base rate (walls, no wrap)
    if(g_sel < 0) return; const int gi = kPads[g_sel].gi; FxG& g = g_fxg[gi]; if(!g.ok) return;
    if(gi == GI_DELAY) { const int i = g.fx->findParam("TIME"); if(i < 0) return; float t = g.fx->v[i] * powf(1.0889f, (float)d); t = t < 40.0f ? 40.0f : t > 1200.0f ? 1200.0f : t; g.fx->setParam(i, t); g_paramDirty = true; }   // 40 ms .. 1.2 s in 40 detents
    else if(gi == GI_FLANGE) { static_cast<hx::FxModBlock*>(g.fx)->stepMode(d); g_paramDirty = true; }   // MODE chorus -> phaser -> flanger, walls, no wrap
    else if(gi == GI_RING) { const int i = g.fx->findParam("FREQ"); if(i < 0) return; const float v = g.fx->v[i]; int idx = v <= 1.001f ? 0 : (int)(logf(v) / logf(2000.0f) * 40.0f + 0.5f); idx += d; idx = idx < 0 ? 0 : idx > 40 ? 40 : idx; g.fx->setParam(i, idx == 0 ? 1.0f : powf(2000.0f, (float)idx / 40.0f)); g_paramDirty = true; }   // 0 (off), then 1.2 Hz .. 2 kHz in 40 steps
}
static int g_chFsrM = -2; static volatile float g_simFsrM = -1.0f; static volatile uint32_t g_tapHw = 0, g_tapReq = 0;   // g_tapHw / g_tapReq: pad taps from the Trellis / from the UI or sim (ghost-press check)
static volatile uint16_t g_padHoldReq = 0;                                          // `sim padhold n` / UI: a long press (looper START: clear all)
static uint32_t g_blTapAt = 0;                                                    // blooper pad: first tap time, waiting out the double-tap window
static uint32_t g_ppressMs[16]; static bool g_plongDone[16];   // press time, hold action already fired, freeze gate latched open by a UI / sim tap
static const uint32_t PAD_HOLD_MS = 500, FRZ_FADE_MS = 300;
static void padFreezeGate(bool open, int k, uint32_t now) {                          // open: let sound into the freeze. First open wakes it (zero CPU before).
    FxG& g = g_fxg[GI_FREEZE]; if(!g.ok) return;
    if(open) { if(!g.latch && (padRefuse(k, axAcquire(GI_FREEZE), now) || padRefuse(k, admit(GI_FREEZE, now), now))) return; if(!g.latch) { g.latch = true; g_enabledAt[GI_FREEZE] = now; } for(int q = 0; q < 3; q++) g_peOn[kPads[kRadio[q]].gi] = false;   // opening the gate turns off dronemaker / reverb / granular (the radio group)
    fxgAct(GI_FREEZE, hx::A_MAIN, 1.0f); if(g_sel != k) padSelect(k); g_frzClearMs = 0; }
    else fxgAct(GI_FREEZE, hx::A_MAIN, 0.0f);                                        // gate closed: what was let in stays frozen
    (void)now;
}
static void padLooper(int k, bool hold, uint32_t now) {
    FxG& g = g_fxg[GI_LOOPER]; if(!g.ok) return;
    if(!g.latch) { if(padRefuse(k, admit(GI_LOOPER, now), now)) return; g.latch = true; g_enabledAt[GI_LOOPER] = now; } if(g_sel != k) padSelect(k);
    if(kPads[k].kind == PK_LSTART) fxgAct(GI_LOOPER, hold ? hx::A_CLEAR : hx::A_MAIN, 1.0f); else fxgAct(GI_LOOPER, hold ? hx::A_HOLD : hx::A_ALT, 1.0f);
    g_loopEvt = now;
}
static void fxControlsTick(const uint16_t* lin, uint32_t now) {
    if(g_chFsrM == -2) { g_chFsrM = -1; for(int i = 0; i < NCH; i++) if(!strcmp(kCh[i].name, "FSR")) g_chFsrM = i; }
    {   // FSR -> tape delay pressure. lin = the linearized channel 0..65535 (same source as the full build's delay TIME). Dead zone below 4 %, then rescaled to 0..1; the tape reaches 2 ms at 0.6.
        float f = g_simFsrM >= 0.0f ? g_simFsrM * 0.001f : (g_chFsrM >= 0 ? (float)lin[g_chFsrM] / 65535.0f : 0.0f);
        f = f < 0.04f ? 0.0f : (f - 0.04f) / 0.96f; g_fsr_p = f > 1.0f ? 1.0f : f; }
    {   // accel Y -> reverb WARP (v1 scale 0..127 -> -15..142 clipped, slope 1.236) and MODDEPTH (0.4 flat .. 1.0 at full warp)
        float t = (g_dly_ay + 1.0f) * 0.5f; t = t < 0 ? 0 : t > 1 ? 1 : t; float w = 1.236f * t - 0.118f; w = w < 0 ? 0 : w > 1 ? 1 : w; g_rv_warp_t = w; g_rv_modd_t = 0.4f + 0.6f * w; }
    // JOYSTICK ROUTING: if the selected effect is the mod block, MOOD or the blooper, the stick drives only that one; otherwise it drives all of them that are on.
    const int selGi = g_sel >= 0 ? kPads[g_sel].gi : -1; const bool selJoy = selGi == GI_FLANGE || selGi == GI_MOODII || selGi == GI_AUTOPAN;
    const float joyY = (g_simJoyY > -1.5f ? g_simJoyY : (float)g_jn_out[1] * 0.001f) * (float)JOY_UP_SIGN;
    const bool rtMod = !selJoy || selGi == GI_FLANGE, rtMood = !selJoy || selGi == GI_MOODII, rtBlp = !selJoy || selGi == GI_AUTOPAN;
    {   // STEPPED joystick for the blooper and MOOD: five zones = +12 / +7 / 0 / -7 / -12 semitones (octave up, fifth up, normal, fifth down, octave down), edges at 0.2 and 0.6 with 0.05 hysteresis.
        // The zone is held for as long as the stick is held (the DSP's multiplier is a steady rate, so new slices / GEN events / repeats all keep it); centre = 1x. DSP input x: mult = 2^(sign*(|x|-0.08)/0.92), so x = sign*(|st|/12*0.92+0.08).
        static int jz = 0; const float a = fabsf(joyY); int m = jz < 0 ? -jz : jz; int sg = jz > 0 ? 1 : -1;
        if(m > 0 && ((joyY > 0.0f) != (sg > 0)) && a > 0.25f) { m = 1; sg = joyY > 0.0f ? 1 : -1; }
        else if(m < 2 && a > (m == 0 ? 0.2f : 0.6f) + 0.05f) { m++; sg = joyY > 0.0f ? 1 : -1; }
        else if(m > 0 && a < (m == 1 ? 0.2f : 0.6f) - 0.05f) m--;
        jz = m ? sg * m : 0; const int semi = m == 0 ? 0 : (m == 1 ? 7 : 12); const float xs = m == 0 ? 0.0f : (float)sg * ((float)semi / 12.0f * 0.92f + 0.08f); g_joyStep = jz * 1.0f; g_joySemi = m ? sg * semi : 0;
        g_blSpeed = rtBlp ? xs : 0.0f; g_moodJoy = rtMood ? xs : 0.0f; }
    if(g_glue[GI_MOOD].ok) {   // joystick Y -> push amount (up only), deadzone, curve, smoothing
        static uint32_t lastJ = 0; const float dtj = lastJ ? (float)(now - lastJ) * 0.001f : 0.005f; lastJ = now; hx::Fx* mj = g_glue[GI_MOOD].fx;
        const float y = (g_simJoyY > -1.5f ? g_simJoyY : (float)g_jn_out[1] * 0.001f) * (float)JOY_UP_SIGN, up = y > 0.0f ? y : 0.0f, dz = mj->v[FxModJoy::P_DEAD];
        float u = up < dz ? 0.0f : (up - dz) / (1.0f - dz); u = u > 1.0f ? 1.0f : u; const float tgt = rtMod ? powf(u, mj->v[FxModJoy::P_CURVE]) : 0.0f;
        const float a = 1.0f - expf(-dtj / (mj->v[FxModJoy::P_SMOOTH] * 0.001f)); g_joyUp = g_joyUp + a * (tgt - g_joyUp); }
    g_glitchSel = g_sel >= 0 && kPads[g_sel].gi == GI_ROAR;
    for(int gi = 0; gi < GLUE_N; gi++) if(gi != GI_DRONE && g_fxg[gi].ok) g_peMix[gi] = g_fxg[gi].mix;   // the UI / restored MIX is the starting point of the takeover
    { const int det = g_enc[0].cnt / 4 + g_simEncDet; if(!g_encInit) { g_encInit = true; g_encSeen = det; } if(det != g_encSeen) { const int d = det - g_encSeen; g_encSeen = det; encStep(d, now); } }
    const uint16_t cur = g_keys; uint16_t rise = cur & (uint16_t)~g_padPrev, fall = (uint16_t)(~cur & g_padPrev); g_padPrev = cur;
    uint16_t req = g_padReq, hreq = g_padHoldReq, relq = g_padRelReq; g_padReq = (uint16_t)(g_padReq & ~req); g_padHoldReq = (uint16_t)(g_padHoldReq & ~hreq); g_padRelReq = (uint16_t)(g_padRelReq & ~relq);
    {   static uint32_t s1 = 0, s2 = 0; static bool ini = false; const uint32_t p1 = g_enc[0].presses + g_simEncPress, p2 = g_enc[1].presses; if(!ini) { ini = true; s1 = p1; s2 = p2; } if(p1 != s1 || p2 != s2) { s1 = p1; s2 = p2; harmToggle(now); } }
    if(g_harmMode) harmonyPage(rise, fall, req, hreq, relq, now);
    harmTick(now);
    for(int k = 0; k < 16; k++) {
        const uint16_t bit = (uint16_t)(1u << k); if(kPads[k].gi < 0) continue;
        if(rise & bit) { g_tapHw = g_tapHw + 1; g_ppressMs[k] = now; g_plongDone[k] = false; }
        if(req & bit) g_tapReq = g_tapReq + 1;
        switch(kPads[k].kind) {
        case PK_TOGGLE: if((rise | req) & bit) padTap(k, now); break;
        case PK_FGATE:  if(rise & bit) padFreezeGate(true, k, now); if(fall & bit) padFreezeGate(false, k, now);
                        if(req & bit) { g_gateSim = !g_gateSim; padFreezeGate(g_gateSim, k, now); } break;                // UI / sim tap toggles the gate (a click cannot hold)
        case PK_FRESET: if(((rise | req) & bit) && g_fxg[GI_FREEZE].ok && g_fxg[GI_FREEZE].latch) { g_frzClearMs = now ? now : 1; g_gateSim = false; fxgAct(GI_FREEZE, hx::A_MAIN, 0.0f); if(g_sel != k) padSelect(k); } break;
        case PK_BLOOP: {   // tap = toggle (acted on after the double-tap window), double-tap = cue a slice, hold 1 s = forget the buffer
            FxG& b = g_fxg[GI_AUTOPAN]; if(!b.ok) break; hx::FxBlooper* bl = static_cast<hx::FxBlooper*>(b.fx);
            if((cur & bit) && !g_plongDone[k] && (now - g_ppressMs[k]) >= 1000) { g_plongDone[k] = true; g_blTapAt = 0; bl->action(hx::A_CLEAR, 1.0f); if(g_sel != k) padSelect(k); }
            const bool tap = ((fall & bit) && !g_plongDone[k]) || (req & bit);
            if(tap) { if(g_sel != k) padSelect(k); if(g_blTapAt && now - g_blTapAt < 350) { g_blTapAt = 0; bl->action(hx::A_ALT, 1.0f); } else g_blTapAt = now ? now : 1; }
            if(hreq & bit) { g_blTapAt = 0; bl->action(hx::A_CLEAR, 1.0f); }
            if(g_blTapAt && now - g_blTapAt >= 350) { g_blTapAt = 0; bl->action(hx::A_MAIN, 1.0f); }
            } break;
        case PK_LSTART: case PK_LPLAY: {
            // ONE-PAD LOOPER. EMPTY: click = record at once. First pass (REC1): single click = close + play, double click (300 ms) = close and keep overdubbing. OVERDUB: click = play only. PLAY: click = overdub.
            // HOLD (350 ms, LOOP_HOLD_MS) in any state = 1 s fade-out, then delete (gone) unless the pad is released during the fade. A click in REC1 waits out the double-click window; every other click acts at once.
            FxG& L = g_fxg[GI_LOOPER]; if(!L.ok) break; hx::FxLooper* lp = static_cast<hx::FxLooper*>(L.fx);
            if((cur & bit) && !g_plongDone[k] && (now - g_ppressMs[k]) >= LOOP_HOLD_MS) { g_plongDone[k] = true; g_lpClickAt = 0; if(!g_lpDel && lp->state() != hx::FxLooper::S_EMPTY) { g_lpDel = now ? now : 1; g_lpMix0 = L.mix; g_lpDelSim = false; } }
            if((fall & bit) && g_plongDone[k] && g_lpDel && !g_lpDelSim) { g_lpDel = 0; L.mix = g_lpMix0; }                  // released during the fade-out: cancel the delete
            if(hreq & bit) { g_lpClickAt = 0; if(!g_lpDel && lp->state() != hx::FxLooper::S_EMPTY) { g_lpDel = now ? now : 1; g_lpMix0 = L.mix; g_lpDelSim = true; } }   // sim / UI padhold: delete without a finger to release
            const bool click = ((fall & bit) && !g_plongDone[k]) || (req & bit);
            if(click && !g_lpDel) {
                const int s0 = lp->state();
                if(s0 == hx::FxLooper::S_EMPTY) { padLooper(k, false, now); }                                          // record starts now
                else if(s0 == hx::FxLooper::S_REC1) { if(g_lpClickAt && now - g_lpClickAt < 300) { g_lpClickAt = 0; padLooper(k, false, now); g_lpOvrPend = now ? now : 1; } else g_lpClickAt = now ? now : 1; }
                else if(s0 == hx::FxLooper::S_STOP) { fxgAct(GI_LOOPER, hx::A_ALT, 1.0f); g_loopEvt = now; }
                else if(s0 != hx::FxLooper::S_TRIM) padLooper(k, false, now);                                          // PLAY -> overdub, OVR -> play only
            }
            } break;
        }
    }
    {   // freeze reset: FREEZE factor 100 -> 0 over 300 ms, then the freeze switches off (zero CPU) and the factor is back at 100
        FxG& z = g_fxg[GI_FREEZE]; static int iZ = -2; if(iZ == -2 && z.ok) iZ = z.fx->findParam("FREEZE");
        if(z.ok && iZ >= 0) {
            if(g_frzClearMs) { const float el = (float)(now - g_frzClearMs) / (float)FRZ_FADE_MS; if(el >= 1.0f) { g_frzClearMs = 0; z.fx->v[iZ] = 100.0f; z.latch = false; } else z.fx->v[iZ] = 100.0f * (1.0f - el); }
            else z.fx->v[iZ] = 100.0f;
        }
    }
    {   // looper click machinery
        FxG& L = g_fxg[GI_LOOPER]; hx::FxLooper* lp = L.ok ? static_cast<hx::FxLooper*>(L.fx) : nullptr;
        if(lp) {
            if(g_lpClickAt && now - g_lpClickAt >= 300) { g_lpClickAt = 0; if(lp->state() == hx::FxLooper::S_REC1) { fxgAct(GI_LOOPER, hx::A_MAIN, 1.0f); g_loopEvt = now; } }   // single click in the first pass: close + play
            if(g_lpOvrPend) { if(lp->state() == hx::FxLooper::S_PLAY) { fxgAct(GI_LOOPER, hx::A_MAIN, 1.0f); g_lpOvrPend = 0; g_loopEvt = now; } else if(now - g_lpOvrPend > 2500) g_lpOvrPend = 0; }   // double click: once the loop is closed, start overdubbing
            if(g_lpDel) { const float el = (float)(now - g_lpDel) / 1000.0f; if(el >= 1.0f) { fxgAct(GI_LOOPER, hx::A_CLEAR, 1.0f); L.mix = g_lpMix0; g_lpDel = 0; g_loopEvt = now; g_lpOvrPend = 0; g_lpClickAt = 0; } else L.mix = g_lpMix0 * (1.0f - el); }
        }
    }
    {   // the looper switches itself off once it is empty (CPU gate), 2 s after the last pad event
        FxG& L = g_fxg[GI_LOOPER]; if(L.ok && L.latch && now - g_loopEvt > 2000) { hx::FxLooper* lp = static_cast<hx::FxLooper*>(L.fx); if(lp->layers() == 0 && lp->state() == hx::FxLooper::S_EMPTY) L.latch = false; }
    }
    g_peOn[GI_AUTOPAN] = g_blOn != 0;
    g_peOn[GI_FREEZE] = g_fxg[GI_FREEZE].latch; g_peOn[GI_LOOPER] = g_fxg[GI_LOOPER].latch;      // latch-driven effects: the display follows the glue
    if(g_sel >= 0) {                                                                                  // fader pickup + map for the selected effect
        const int gi = kPads[g_sel].gi;
        for(int n = 0; n < 3; n++) {
            const char* pn = faderParam(gi, n); if(!*pn) continue;
            if(!(g_live & (1u << n)) && fabsf(g_fs[n] - g_snap[n]) >= PICKUP) g_live |= (uint8_t)(1u << n);
            if(!(g_live & (1u << n))) continue; const float f = g_fs[n];
            if(!strcmp(pn, "MIX")) { if(g_peOn[gi]) g_peMix[gi] = f; }
            else if(g_glue[gi].ok) { hx::Fx* fx = g_glue[gi].fx; const int pi = fx->findParam(pn); if(pi >= 0) { const Param& q = fx->pt[pi]; const bool lg = (q.flags & PF_LOG) && q.min > 0; fx->setParam(pi, lg ? q.min * powf(q.max / q.min, f) : q.min + f * (q.max - q.min)); } }
        }
    }
    for(int k = 0; k < 16; k++) {
        if(kPads[k].gi < 0) continue; const int gi = kPads[k].gi;
        if(kPads[k].kind == PK_TOGGLE) { bool on = g_peOn[gi]; if(gi == GI_RING && g_fxg[gi].ok) on = on && (g_fxg[gi].fx->v[g_ringFi >= 0 ? g_ringFi : 1] > 1.001f || g_fsr_p > 0.01f);   // ringmod at base 0 with no pressure = off: dry, no processing, zero CPU
            peDrive(gi, g_peMix[gi], on); }
        else if(kPads[k].kind == PK_BLOOP) {}                                                          // blooper: always running, MIX is a UI param, faders map to PROB / SIZE / DIST
        else if(g_peOn[gi] && !(gi == GI_LOOPER && g_lpDel)) g_fxg[gi].mix = g_peMix[gi];                                               // freeze / looper: latch is owned by the gate / looper logic, only the level follows fader 1
    }
    const float tsec = (float)now * 0.001f; hx::FxLooper* lp = static_cast<hx::FxLooper*>(g_fxg[GI_LOOPER].fx);
    for(int k = 0; k < 16; k++) {                                // pad LEDs: only table pads light; dim when off, bright when on
        float lv = 0.0f; const int gi = kPads[k].gi;
        if(gi >= 0) {
            switch(kPads[k].kind) {
            case PK_TOGGLE: lv = g_peOn[gi] ? 1.0f : 0.18f; break;
            case PK_BLOOP:  lv = g_blOn ? (g_blHas ? 1.0f : 0.5f) : (g_blHas ? 0.18f : 0.04f); break;   // no loop = dim
            case PK_FGATE:  lv = ((g_keys >> k) & 1 || g_gateSim) ? 1.0f : (g_fxg[GI_FREEZE].latch ? 0.45f : 0.18f); break;
            case PK_FRESET: lv = g_frzClearMs ? (fmodf(tsec, 0.2f) < 0.1f ? 1.0f : 0.3f) : 0.18f; break;
            case PK_LSTART: { const int st = lp ? lp->state() : 0; if(g_lpDel) { lv = 0.8f * (1.0f - (float)(now - g_lpDel) / 1000.0f); if(lv < 0.05f) lv = 0.05f; } else lv = (st == hx::FxLooper::S_REC1 || st == hx::FxLooper::S_OVR) ? (fmodf(tsec, 0.5f) < 0.25f ? 1.0f : 0.3f) : (st == hx::FxLooper::S_PLAY ? 0.8f : 0.18f); } break;
            case PK_LPLAY:  lv = 0.18f; break;
            }
        }
        uint8_t cr = kPads[k].rgb[0], cg = kPads[k].rgb[1], cb = kPads[k].rgb[2];
        if(gi == GI_FLANGE && g_fxg[gi].ok) { const int md = (int)(g_fxg[gi].fx->v[hx::FxModBlock::K_MODE] + 0.5f); if(md == 1) { cr = 190; cg = 120; cb = 255; } else if(md == 2) { cr = 255; cg = 170; cb = 120; } }   // mode tint: chorus pink, phaser violet, flanger amber
        g_tr[k].r = lv * cr / 255.0f; g_tr[k].g = lv * cg / 255.0f; g_tr[k].b = lv * cb / 255.0f;
        if(g_refuseUntil[k] > now) { const bool bl = ((g_refuseUntil[k] - now) / 110) & 1; g_tr[k].r = bl ? 1.0f : 0.1f; g_tr[k].g = 0.0f; g_tr[k].b = 0.0f; }   // refused: red blink
    }
    if(g_harmMode) for(int k = 0; k < 16; k++) { const bool L = (kHarmLatch >> k) & 1; const float lv = (((g_hvHeld | g_hvLatch) >> k) & 1) ? 0.95f : 0.13f; if(L) { g_tr[k].r = lv; g_tr[k].g = lv * 60 / 255.0f; g_tr[k].b = lv * 170 / 255.0f; } else { g_tr[k].r = lv * 155 / 255.0f; g_tr[k].g = lv * 133 / 255.0f; g_tr[k].b = lv; } }   // harmony page: chords violet, the 3 latches pink, sounding = bright
}
static void fxShedPoll(uint32_t now) {
    static uint32_t lastShed = 0;
    if(!g_shedReq) return; if(now - lastShed < 150) return;
    int victim = -1; uint32_t best = 0;
    for(int gi = 0; gi < GLUE_N; gi++) {
        if(kAdmitPk[gi] <= 0.0f) continue; const bool on = g_peOn[gi] || (gi != GI_DRONE && g_fxg[gi].ok && g_fxg[gi].latch);
        if(on && g_enabledAt[gi] >= best) { best = g_enabledAt[gi]; victim = gi; }
    }
    g_shedReq = false; lastShed = now; if(victim < 0) return;
    g_peOn[victim] = false; if(victim != GI_DRONE && g_fxg[victim].ok) g_fxg[victim].latch = false;
    if(victim == GI_HARMONY) { static_cast<hx::FxHarmony*>(g_fxg[victim].fx)->allOff(); g_hvHeld = g_hvLatch = 0; g_hvOn[0] = g_hvOn[1] = g_hvOn[2] = 0; g_hvDirty = false; }
    if(victim == GI_FREEZE) { g_frzClearMs = 0; g_gateSim = false; }
    g_shedUntil[victim] = now + 5000; g_shedCount = g_shedCount + 1; g_shedLast = victim;
    for(int k = 0; k < 16; k++) if(kPads[k].gi == victim) g_refuseUntil[k] = now + 1200;
    snprintf(g_whyBuf, sizeof(g_whyBuf), "shed: %s (cpu over 75%%)", g_glue[victim].id ? g_glue[victim].id : "effect"); g_why = g_whyBuf; g_whyUntil = now + 4000;
}
static void padTelemetry() {
    uint32_t on = 0; hx::FxLooper* lp = static_cast<hx::FxLooper*>(g_fxg[GI_LOOPER].fx);
    for(int k = 0; k < 16; k++) { if(kPads[k].gi < 0) continue; bool l = g_peOn[kPads[k].gi];
        if(kPads[k].kind == PK_FGATE) l = ((g_keys >> k) & 1) || g_gateSim; else if(kPads[k].kind == PK_FRESET) l = g_frzClearMs != 0;
        else if(kPads[k].kind == PK_BLOOP) l = g_blOn != 0; else if(kPads[k].kind == PK_LSTART) l = lp && (lp->state() == hx::FxLooper::S_REC1 || lp->state() == hx::FxLooper::S_OVR); else if(kPads[k].kind == PK_LPLAY) l = lp && (lp->state() == hx::FxLooper::S_PLAY || lp->state() == hx::FxLooper::S_OVR);
        if(l) on |= 1u << k; }
    const int s = g_sel; puts_(",\"pads\":{\"sel\":"); putd(s); puts_(",\"on\":"); putu(on); puts_(",\"fl\":"); putu(g_live); puts_(",\"mix\":"); putu(s >= 0 ? (uint32_t)(g_peMix[kPads[s].gi] * 1000.0f) : 0); { const int sg = g_sel >= 0 ? kPads[g_sel].gi : -1; float b = 0, l = 0; if(sg == GI_DELAY && g_fxg[sg].ok) { b = g_fxg[sg].fx->v[g_fxg[sg].fx->findParam("TIME")]; l = g_tapeLive; } else if(sg == GI_ROAR && g_fxg[sg].ok && g_glFi >= 0) { b = g_fxg[sg].fx->v[g_glFi] * 100.0f; l = g_glitchLive * 100.0f; } else if(sg == GI_RING && g_fxg[sg].ok && g_ringFi >= 0) { b = g_fxg[sg].fx->v[g_ringFi]; b = b <= 1.001f ? 0.0f : b; l = g_ringLive; }
      puts_(",\"bl\":["); putu((uint32_t)(b * 10.0f)); put(','); putu((uint32_t)(l * 10.0f)); put(']'); }
    puts_(",\"ax\":["); putu((uint32_t)g_ax_used[0]); put(','); putu((uint32_t)g_ax_used[1]); put(','); putd(g_ax_owner); puts_("]"); if(g_whyUntil && (int32_t)(g_whyUntil - (uint32_t)System::GetNow()) > 0) { puts_(",\"why\":\""); puts_(g_why); put('"'); }
    puts_(",\"jf\":["); putu((uint32_t)(g_joyUp * 1000.0f)); put(','); putd((int32_t)(g_joyFb * 1000.0f)); put(']');
    if(g_fxg[GI_FLANGE].ok) { puts_(",\"md\":"); putu((uint32_t)(g_fxg[GI_FLANGE].fx->v[hx::FxModBlock::K_MODE] + 0.5f)); }
    puts_(",\"hm\":"); put(g_harmMode ? '1' : '0'); puts_(",\"hv\":"); putu((uint16_t)(g_hvHeld | g_hvLatch)); puts_(",\"rf\":"); putu(g_refuseCount); puts_(",\"shed\":["); putu(g_shedCount); put(','); putd(g_shedLast); put(']');
    puts_(",\"js\":"); putd(g_joySemi); { hx::FxLooper* lq = g_fxg[GI_LOOPER].ok ? static_cast<hx::FxLooper*>(g_fxg[GI_LOOPER].fx) : nullptr; puts_(",\"lp\":["); putu(lq ? (uint32_t)lq->state() : 0); put(','); putu(lq ? (uint32_t)lq->layers() : 0); put(','); putu(g_lpDel ? 1u : 0u); put(']'); }
    puts_(",\"og\":"); putd(g_og_db10); puts_(",\"ns\":"); put(g_nosave ? '1' : '0'); puts_(",\"tp\":["); putu(g_tapHw); put(','); putu(g_tapReq); puts_("],\"keys\":"); putu(g_keys); puts_(",\"rst\":"); putu(g_rsr); puts_(",\"txd\":"); putu(g_txDrop); puts_("}");
}
static void padHello() {
    static const char* H = "0123456789abcdef"; puts_(",\"pm\":[");
    for(int k = 0; k < 16; k++) { if(k) put(','); put('['); put('"'); puts_(kPads[k].name); put('"'); put(','); put('"'); if(kPads[k].gi >= 0) for(int c = 0; c < 3; c++) { put(H[kPads[k].rgb[c] >> 4]); put(H[kPads[k].rgb[c] & 15]); } put('"'); put(','); put('"'); if(kPads[k].gi >= 0) puts_(g_glue[kPads[k].gi].id); put('"'); puts_(",["); for(int n = 0; n < 3; n++) { if(n) put(','); put('"'); if(kPads[k].gi >= 0) puts_(faderParam(kPads[k].gi, n)); put('"'); } put(']'); put(']'); }   // [name, rgb, effect id]
    puts_("],\"hi\":["); for(int k = 0; k < 16; k++) { if(k) put(','); putd(kHarmCh[k][0]); } put(']');
}
