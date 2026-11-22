// fx_mapping.cpp -- HYPERTRUMPET V2 control -> FX mapping (HxMapper). Spec: HYPERTRUMPET_V2/docs/FX_MAP.md ("v2 mapping");
// the code and that section must stay 1:1. Portable C++17, no heap, no std::function, no per-sample work.
//
// ============================== CONTROL-RATE API CONTRACT ==============================
//  * Call HxMapper::init(&engine) once, then HxMapper::update(controls, dtSec) from the MAIN LOOP (control rate,
//    ~200 Hz to 1 kHz; dtSec = real time since the previous call, clamped internally to 0.2 ms .. 50 ms).
//    NEVER call update(), init(), selectSlot() or loadDefaultPatch() from the audio callback/ISR:
//    FxEngine::setEffect() builds the effect in the slot arena (allocation + memset) and is not audio-thread safe.
//  * setEffect() is only called on EXPLICIT user actions: enc1 hold+turn then release (commit), the bank pads and
//    effect prev/next pads of the BANK overlay page, the 1 s-hold reset pad, and loadDefaultPatch(). Never from a
//    continuous control. While an effect is swapped the slot passes audio through (engine behaviour).
//  * HxControls is level-based for pads/clicks (held = true); the mapper edge-detects internally (first update
//    primes the previous state, so a pad held at boot does not fire). encDelta is consumed per call (detents since
//    the last call). All continuous inputs are normalised by the HAL (see fx_mapping.h) and may be fed raw: the mapper
//    applies the per-control one-pole smoothing (rate aware: alpha = 1 - exp(-dt/tau)) and deadzones itself.
//  * Writes into the engine: Fx::setRoleNorm(role, n) for role targets, Fx::setParam(0, x) for slot level (param 0 is
//    always MIX), Fx::action(A_*, v), FxEngine::setBypass(), FxEngine::setValveHint(hz). Reads: engine->f0(), env(),
//    effectType(), fx(slot)->pt[]/v[]/role[]/np. It never touches effect internals.
//  * A continuous target is only written when its value moved by more than a small epsilon (or after the slot's
//    effect changed), so values edited with enc2 stay put until that physical control is moved again (knob semantics).
//  * leds() is refreshed on every update(); firmware applies its own brightness cap (~0.45) and sends at ~12.5 Hz.
//  * Only static storage is used (pool of HX_MAP_POOL mappers); init() takes a free one.
// =======================================================================================
#include "fx_mapping.h"
#include <stdio.h>

namespace hx {
namespace {

// ------------------------------------------------------------------ constants (all documented in FX_MAP.md)
#define HX_MAP_POOL 4
const int   NBANK = 4;
const char* const kBankName[NBANK] = { "CORE", "SPEC", "LOOP", "GRIT" };
const char* const kBank[NBANK][HX_NSLOTS] = {
    { "harm", "grain",  "reverb", "delay"  },   // A CORE  (default patch)
    { "mood", "freeze", "blur",   "flange" },   // B SPEC
    { "harm", "looper", "reverb", "ring"   },   // C LOOP
    { "harm", "grain",  "roar",   "wah"    },   // D GRIT
};
const uint8_t kSlotCol[HX_NSLOTS][3] = {        // violet palette, one hue per slot
    { 130,  40, 255 },   // S1 violet
    { 225,  40, 200 },   // S2 magenta-violet
    {  60,  70, 255 },   // S3 indigo
    { 175, 125, 255 },   // S4 lilac
};
const uint8_t kWhiteV[3] = { 215, 190, 255 };   // bank / overlay accent

// smoothing time constants (s)
const float TAU_VALVE = 0.004f, TAU_FADER = 0.040f, TAU_POT = 0.040f, TAU_JOY = 0.025f, TAU_TILT = 0.120f,
            TAU_LIDAR = 0.090f, TAU_FSR_UP = 0.015f, TAU_FSR_DN = 0.120f, TAU_ENV = 0.030f,
            TAU_HALF_UP = 0.070f, TAU_HALF_DN = 0.100f;
const float EPS_APPLY = 0.003f;                 // write threshold for continuous targets
const float VALVE_ON = 0.55f, VALVE_OFF = 0.40f;
const float FSR_DEAD = 0.03f;
const float TILT_GAIN = 1.236f;                 // v1 accel-X scale 0..127 -> -15..142
const float SHAKE_THR = 0.85f, SHAKE_TIME = 0.08f, SHAKE_REFRACT = 2.0f;
const float RESET_HOLD = 1.0f;                  // overlay pad 15 hold time to reload default patch
const float FLASH_DECAY = 6.0f;                 // 1/s

enum Tgt { T_F1, T_F2, T_F3, T_JX, T_JY, T_HALF, T_TILTY, T_TILTX, T_AMT4, T_TIME4, T_TONE4, T_COUNT };
const int kTgtSlot[T_COUNT] = { 0, 1, 2, 0, 0, 0, 1, 2, 3, 3, 3 };

static inline float fin(float x) { return (x * 0.f == 0.f) ? x : 0.f; }
static inline float cl01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
static inline float alphaOf(float dt, float tau) { return 1.f - expf(-dt / tau); }
static inline float sstep(float a, float b, float x) { float t = cl01((x - a) / (b - a)); return t * t * (3.f - 2.f * t); }

struct Sm {
    float y = 0.f; bool primed = false;
    float tick(float x, float a) { if (!primed) { y = x; primed = true; } else y += (x - y) * a; return y; }
};

enum PadMain { PM_TRIGGER, PM_HOLD_MAIN, PM_HOLD_HOLD };   // what pad row 1 does per effect id
static PadMain padMainKind(const char* id) {
    if (!id) return PM_TRIGGER;
    if (!strcmp(id, "delay") || !strcmp(id, "freeze")) return PM_HOLD_MAIN;   // A_MAIN is a held gate (v1 pads 9 / 4)
    if (!strcmp(id, "ring")) return PM_HOLD_HOLD;                              // A_HOLD is a held gate (v1 pad 5)
    return PM_TRIGGER;
}
// joystick-X centre (neutral ladder position as a 0..1 role value): harmonizer INTVL 5/13 = unison, MOOD ladder idx 4/8 = 0 st
static float neutralPitchNorm(const char* id) {
    if (id && !strcmp(id, "harm")) return 5.f / 13.f;
    if (id && !strcmp(id, "mood")) return 4.f / 8.f;
    return 0.5f;
}

} // namespace

// ------------------------------------------------------------------ state (static pool)
struct HxMapper::Impl {
    bool used = false, primed = false;
    // smoothers
    Sm valve[3], fader[3], pot[2], joyX, joyY, tiltX, tiltY, lidar, fsr, env, half;
    bool vDown[3] = {};
    float lastHint = 0.f;
    // dirty tracking: NaN = force write
    float last[T_COUNT];
    // slots
    int  cachedType[HX_NSLOTS];
    bool defCaptured[HX_NSLOTS];
    float defV[HX_NSLOTS][HX_MAXP];
    // pads / clicks
    uint16_t prevPads = 0;
    bool prevJoyClick = false, prevEncClick[2] = {false, false};
    float flash[16];
    bool padHeldMain[HX_NSLOTS] = {};           // a hold-type MAIN/HOLD action is currently down
    int  bank = 0;
    bool joyEngaged = false;
    bool dryAll = false; uint8_t dryMask = 0;   // dryAll: all slots bypassed, mask = bypass state to restore
    // encoders
    bool enc1Turned = false, enc2Turned = false; int stage = -1;   // stage: effect type being browsed (enc1 hold)
    // gestures
    float shakeT = 0.f, shakeRef = 0.f, holdReset = 0.f; bool resetDone = false;
    float hintHold = 0.f; int partial = 4;   // last tracked partial (default 4 = Bb4 open)
    bool statusDirty = false;
};

// ------------------------------------------------------------------ helpers (file-local, take impl + engine)
namespace {

static const char* typeId(int t) { const FxTypeInfo* ti = fxType(t); return ti ? ti->id : nullptr; }

template <class IM> static void invalidateSlot(IM& I, int s) {
    for (int t = 0; t < T_COUNT; t++) if (kTgtSlot[t] == s) I.last[t] = NAN;
}

template <class IM> static void applyRole(IM& I, FxEngine* e, int tgt, Role r, float n) {
    n = cl01(fin(n));
    float l = I.last[tgt];
    if (l == l && fabsf(n - l) < EPS_APPLY) return;
    Fx* f = e->fx(kTgtSlot[tgt]);
    if (!f) return;
    f->setRoleNorm(r, n);
    I.last[tgt] = n;
}

template <class IM> static void applyMix(IM& I, FxEngine* e, int tgt, float n) {
    n = cl01(fin(n));
    float l = I.last[tgt];
    if (l == l && fabsf(n - l) < EPS_APPLY) return;
    Fx* f = e->fx(kTgtSlot[tgt]);
    if (!f || f->np < 1) return;
    f->setParam(0, paramDenorm(f->pt[0], n));
    I.last[tgt] = n;
}

} // namespace

// ------------------------------------------------------------------ public API
void HxMapper::init(FxEngine* e) {
    static Impl g_pool[HX_MAP_POOL];
    eng_ = e; impl_ = nullptr;
    for (int i = 0; i < HX_MAP_POOL; i++) if (!g_pool[i].used) { impl_ = &g_pool[i]; break; }
    if (!impl_) impl_ = &g_pool[0];                // pool exhausted: reuse slot 0 (single-mapper firmware / sim never hits this)
    Impl& I = *impl_;
    I = Impl();
    I.used = true;
    for (int t = 0; t < T_COUNT; t++) I.last[t] = NAN;
    for (int s = 0; s < HX_NSLOTS; s++) { I.cachedType[s] = -2; I.defCaptured[s] = false; }
    for (int p = 0; p < 16; p++) I.flash[p] = 0.f;
    leds_ = HxLeds(); prev_ = HxControls(); t_ = 0.f; page_ = 0; selSlot_ = 0; selParam_ = 0; status_[0] = 0;
    loadDefaultPatch();
}

template <class IM> static void loadBankImpl(IM& I, FxEngine* e, int b) {
    if (b < 0 || b >= NBANK) return;
    for (int s = 0; s < HX_NSLOTS; s++) {
        int t = fxTypeById(kBank[b][s]);
        e->setEffect(s, t);                       // allocates: control thread only
        I.cachedType[s] = -2;                     // force re-sync (captures defaults, invalidates targets)
        I.defCaptured[s] = false;
        I.padHeldMain[s] = false;
    }
    I.bank = b;
}

void HxMapper::loadDefaultPatch() {
    if (!eng_ || !impl_) return;
    Impl& I = *impl_;
    for (int s = 0; s < HX_NSLOTS; s++) eng_->setBypass(s, false);
    I.dryAll = false; I.joyEngaged = false;
    loadBankImpl(I, eng_, 0);
    for (int t = 0; t < T_COUNT; t++) I.last[t] = NAN;
    selSlot_ = 0;
    Fx* f = eng_->fx(0);
    selParam_ = (f && f->role[R_AMT] != 0xFF) ? f->role[R_AMT] : 0;
    snprintf(status_, sizeof status_, "BANK A %s", kBankName[0]);
}

void HxMapper::selectSlot(int s) {
    selSlot_ = s & 3;
    if (!eng_) return;
    Fx* f = eng_->fx(selSlot_);
    selParam_ = (f && f->role[R_AMT] != 0xFF) ? f->role[R_AMT] : 0;
    if (f) snprintf(status_, sizeof status_, "S%d %s", selSlot_ + 1, f->name);
    else   snprintf(status_, sizeof status_, "S%d EMPTY", selSlot_ + 1);
}

void HxMapper::update(const HxControls& c, float dtSec) {
    if (!eng_ || !impl_) return;
    Impl& I = *impl_;
    float dt = dtSec;
    if (!(dt > 0.0002f)) dt = 0.0002f;
    if (dt > 0.05f) dt = 0.05f;
    t_ += dt;
    FxEngine* e = eng_;

    // ---- 0. sync slot bookkeeping (effect changed by us or by an outside caller)
    for (int s = 0; s < HX_NSLOTS; s++) {
        int t = e->effectType(s);
        Fx* f = e->fx(s);
        if (t != I.cachedType[s]) { I.cachedType[s] = t; I.defCaptured[s] = false; invalidateSlot(I, s); I.padHeldMain[s] = false;
            if (s == selSlot_) selSlot_ = s; }
        if (f && !I.defCaptured[s]) { for (int i = 0; i < HX_MAXP; i++) I.defV[s][i] = f->v[i]; I.defCaptured[s] = true;
            if (s == selSlot_) selParam_ = (f->role[R_AMT] != 0xFF) ? f->role[R_AMT] : 0; }
    }

    const bool first = !I.primed;
    if (first) { I.prevPads = c.pads; I.prevJoyClick = c.joyClick; I.prevEncClick[0] = c.encClick[0]; I.prevEncClick[1] = c.encClick[1]; I.primed = true; }

    // ---- 1. smoothing of all continuous inputs
    float v[3], fa[3], po[2];
    for (int k = 0; k < 3; k++) { v[k] = cl01(I.valve[k].tick(cl01(fin(c.valve[k])), alphaOf(dt, TAU_VALVE))); fa[k] = cl01(I.fader[k].tick(cl01(fin(c.fader[k])), alphaOf(dt, TAU_FADER))); }
    for (int k = 0; k < 2; k++) po[k] = cl01(I.pot[k].tick(cl01(fin(c.pot[k])), alphaOf(dt, TAU_POT)));
    float jx = I.joyX.tick(fin(c.joyX), alphaOf(dt, TAU_JOY)), jy = I.joyY.tick(fin(c.joyY), alphaOf(dt, TAU_JOY));
    float tx = I.tiltX.tick(fin(c.tiltX), alphaOf(dt, TAU_TILT)), ty = I.tiltY.tick(fin(c.tiltY), alphaOf(dt, TAU_TILT));
    float li = cl01(I.lidar.tick(cl01(fin(c.lidar)), alphaOf(dt, TAU_LIDAR)));
    float fsrIn = cl01(fin(c.fsr));
    if (!I.fsr.primed) I.fsr.tick(fsrIn, 1.f);
    float fsrS = I.fsr.tick(fsrIn, alphaOf(dt, fsrIn > I.fsr.y ? TAU_FSR_UP : TAU_FSR_DN));
    float envS = cl01(I.env.tick(cl01(fin(e->env())), alphaOf(dt, TAU_ENV)));
    jx = jx < -1.f ? -1.f : (jx > 1.f ? 1.f : jx); jy = jy < -1.f ? -1.f : (jy > 1.f ? 1.f : jy);

    // ---- 2. valves: combination -> pitch hint; half-valve dwell -> S1 MOD   (NEW in v2, v1 valves were unused)
    int semis = 0; static const int kDrop[3] = { 2, 1, 3 };
    for (int k = 0; k < 3; k++) {
        if (!I.vDown[k] && v[k] > VALVE_ON) I.vDown[k] = true; else if (I.vDown[k] && v[k] < VALVE_OFF) I.vDown[k] = false;
        if (I.vDown[k]) semis += kDrop[k];
    }
    {
        // Valve pitch hint = valve combination (fundamental) x partial estimate. The partial is learned from the tracker while it is
        // confident and REMEMBERED when it drops out, so a fingering change still moves the hint (EMGOR decision 2026-11-15). The
        // harmonizer/mood use tracked f0 when confident and this hint otherwise (ShifterCore::setPitch).
        float f0 = e->f0(), hint = I.lastHint;
        float fund = 116.54f * exp2f(-(float)semis / 12.f);         // Bb trumpet pedal tone lowered by the valve combo
        if (f0 > 50.f) {
            int kk = (int)(f0 / fund + 0.5f); if (kk < 2) kk = 2; if (kk > 12) kk = 12;
            I.partial = kk; hint = fund * (float)kk; I.hintHold = 0.15f;
        } else { I.hintHold -= dt; hint = (I.hintHold > 0.f || envS > 0.04f) ? fund * (float)I.partial : 0.f; }
        if (fabsf(hint - I.lastHint) > 0.5f) { e->setValveHint(hint); I.lastHint = hint; }
    }
    float hraw = 0.f;
    for (int k = 0; k < 3; k++) { float m = v[k] < 1.f - v[k] ? v[k] : 1.f - v[k]; float h = sstep(0.12f, 0.35f, m); if (h > hraw) hraw = h; }
    float halfS = I.half.tick(hraw, alphaOf(dt, hraw > I.half.y ? TAU_HALF_UP : TAU_HALF_DN));

    // ---- 3. pads (edge detect)
    uint16_t rise = (uint16_t)(c.pads & ~I.prevPads), fall = (uint16_t)(~c.pads & I.prevPads);
    I.prevPads = c.pads;
    int newPage = (c.pads & (1u << 12)) ? 1 : 0;
    if (newPage != page_) { page_ = newPage; snprintf(status_, sizeof status_, page_ ? "BANK  %s" : "PLAY", kBankName[I.bank]); I.holdReset = 0.f; I.resetDone = false; }
    for (int p = 0; p < 16; p++) I.flash[p] -= I.flash[p] * (1.f - expf(-FLASH_DECAY * dt)); // exp decay
    for (int p = 0; p < 16; p++) if (rise & (1u << p)) I.flash[p] = 1.f;

    // releases of hold-type actions are always delivered, whatever the page
    for (int s = 0; s < HX_NSLOTS; s++) {
        int p = 4 + s;
        if ((fall & (1u << p)) && I.padHeldMain[s]) {
            I.padHeldMain[s] = false;
            const char* id = typeId(e->effectType(s));
            PadMain k = padMainKind(id);
            if (k == PM_HOLD_MAIN) e->action(s, A_MAIN, 0.f); else if (k == PM_HOLD_HOLD) e->action(s, A_HOLD, 0.f);
        }
    }
    if (!page_) {
        for (int p = 0; p < 16; p++) {
            if (!(rise & (1u << p)) || p == 12) continue;
            int row = p >> 2, col = p & 3;
            if (row == 0) {                                   // bypass latch
                if (e->effectType(col) >= 0) { e->setBypass(col, !e->bypassed(col)); I.dryAll = false;
                    snprintf(status_, sizeof status_, "S%d %s %s", col + 1, e->fx(col) ? e->fx(col)->name : "-", e->bypassed(col) ? "BYP" : "ON"); }
            } else if (row == 1) {                            // MAIN (trigger or hold per effect)
                const char* id = typeId(e->effectType(col));
                PadMain k = padMainKind(id);
                if (k == PM_HOLD_MAIN) { e->action(col, A_MAIN, 1.f); I.padHeldMain[col] = true; }
                else if (k == PM_HOLD_HOLD) { e->action(col, A_HOLD, 1.f); I.padHeldMain[col] = true; }
                else e->action(col, A_MAIN, 1.f);
            } else if (row == 2) {                            // ALT (trigger)
                e->action(col, A_ALT, 1.f);
            } else if (row == 3 && col >= 1) {                // CLEAR S2..S4
                e->action(col, A_CLEAR, 1.f);
            }
        }
    } else {
        for (int p = 0; p < 16; p++) {
            if (!(rise & (1u << p)) || p == 12) continue;
            int row = p >> 2, col = p & 3;
            if (row == 0) { loadBankImpl(I, e, col); for (int s = 0; s < HX_NSLOTS; s++) e->setBypass(s, false); I.dryAll = false;
                snprintf(status_, sizeof status_, "BANK %c %s", 'A' + col, kBankName[col]); }
            else if (row == 1) {
                if (col <= 1) {                               // prev / next effect for the focus slot (commit immediately)
                    int n = fxTypeCount(); int cur = e->effectType(selSlot_); if (cur < 0) cur = 0;
                    int nt = (cur + (col ? 1 : n - 1)) % n;
                    e->setEffect(selSlot_, nt); I.cachedType[selSlot_] = -2;
                    const FxTypeInfo* ti = fxType(nt); snprintf(status_, sizeof status_, "S%d > %s", selSlot_ + 1, ti ? ti->name : "?");
                } else {                                      // prev / next parameter
                    Fx* f = e->fx(selSlot_);
                    if (f && f->np > 0) { selParam_ = (selParam_ + (col == 3 ? 1 : f->np - 1)) % f->np;
                        snprintf(status_, sizeof status_, "%s %s", f->name, f->pt[selParam_].name); }
                }
            } else if (row == 2) { selectSlot(col); }
            else if (row == 3) {
                if (col == 1) {                               // DRY: bypass everything / restore
                    if (!I.dryAll) { I.dryMask = 0; for (int s = 0; s < HX_NSLOTS; s++) { if (e->bypassed(s)) I.dryMask |= (uint8_t)(1u << s); e->setBypass(s, true); } I.dryAll = true; snprintf(status_, sizeof status_, "DRY"); }
                    else { for (int s = 0; s < HX_NSLOTS; s++) e->setBypass(s, (I.dryMask >> s) & 1); I.dryAll = false; snprintf(status_, sizeof status_, "FX RESTORED"); }
                } else if (col == 2) {                        // joystick engage (same as the stick click)
                    I.joyEngaged = !I.joyEngaged; I.last[T_JX] = I.last[T_JY] = NAN;
                    snprintf(status_, sizeof status_, I.joyEngaged ? "STICK ON" : "STICK OFF");
                }
            }
        }
        // pad 15: hold 1 s -> reload the default patch
        if (c.pads & (1u << 15)) { I.holdReset += dt; if (I.holdReset >= RESET_HOLD && !I.resetDone) { I.resetDone = true; loadDefaultPatch(); } }
        else { I.holdReset = 0.f; I.resetDone = false; }
    }

    // ---- 4. joystick click: latch engage (PLAY page only matters not: it always latches)
    if (c.joyClick && !I.prevJoyClick) {
        I.joyEngaged = !I.joyEngaged; I.last[T_JX] = I.last[T_JY] = NAN;
        snprintf(status_, sizeof status_, I.joyEngaged ? "STICK ON" : "STICK OFF");
    }
    I.prevJoyClick = c.joyClick;

    // ---- 5. encoders
    {
        // enc1: turn = focus slot; hold+turn = browse effect (staged), release = commit; release without turn = bypass focus slot
        bool down = c.encClick[0];
        int d = c.encDelta[0];
        if (down && !I.prevEncClick[0]) { I.enc1Turned = false; I.stage = e->effectType(selSlot_); }
        if (d) {
            if (down) {
                int n = fxTypeCount(); int cur = I.stage < 0 ? 0 : I.stage;
                cur = ((cur + d) % n + n) % n; I.stage = cur; I.enc1Turned = true;
                const FxTypeInfo* ti = fxType(cur); snprintf(status_, sizeof status_, "S%d ? %s", selSlot_ + 1, ti ? ti->name : "?");
            } else {
                selectSlot(((selSlot_ + d) % HX_NSLOTS + HX_NSLOTS) % HX_NSLOTS);
            }
        }
        if (!down && I.prevEncClick[0]) {
            if (I.enc1Turned) {
                if (I.stage != e->effectType(selSlot_)) { e->setEffect(selSlot_, I.stage); I.cachedType[selSlot_] = -2;
                    const FxTypeInfo* ti = fxType(I.stage); snprintf(status_, sizeof status_, "S%d = %s", selSlot_ + 1, ti ? ti->name : "?"); }
            } else if (e->effectType(selSlot_) >= 0) {
                e->setBypass(selSlot_, !e->bypassed(selSlot_)); I.dryAll = false;
                snprintf(status_, sizeof status_, "S%d %s", selSlot_ + 1, e->bypassed(selSlot_) ? "BYP" : "ON");
            }
            I.enc1Turned = false; I.stage = -1;
        }
        I.prevEncClick[0] = down;
    }
    {
        // enc2: turn = edit selected param; hold+turn = choose param; release without turn = reset param to its default
        bool down = c.encClick[1];
        int d = c.encDelta[1];
        Fx* f = e->fx(selSlot_);
        if (down && !I.prevEncClick[1]) I.enc2Turned = false;
        if (d && f && f->np > 0) {
            if (selParam_ >= f->np) selParam_ = 0;
            if (down) {
                selParam_ = ((selParam_ + d) % f->np + f->np) % f->np; I.enc2Turned = true;
                snprintf(status_, sizeof status_, "%s %s", f->name, f->pt[selParam_].name);
            } else {
                const Param& p = f->pt[selParam_]; float x = f->v[selParam_];
                if (p.flags & (PF_QUANT | PF_ENUM | PF_INT)) x += (float)d * (p.step > 0.f ? p.step : 1.f);
                else if ((p.flags & PF_LOG) && p.min > 0.f) x *= powf(p.max / p.min, (float)d / 64.f);
                else { float st = (p.max - p.min) / 64.f; if (p.step > st) st = p.step; x += (float)d * st; }
                f->setParam(selParam_, x);
                snprintf(status_, sizeof status_, "%s %.2f", p.name, f->v[selParam_]);
            }
        }
        if (!down && I.prevEncClick[1]) {
            if (!I.enc2Turned && f && f->np > 0 && I.defCaptured[selSlot_]) {
                if (selParam_ >= f->np) selParam_ = 0;
                f->setParam(selParam_, I.defV[selSlot_][selParam_]);
                snprintf(status_, sizeof status_, "%s RESET", f->pt[selParam_].name);
            }
            I.enc2Turned = false;
        }
        I.prevEncClick[1] = down;
    }

    // ---- 6. shake gesture -> S2: freeze-grab on a freeze effect (A_ALT latch), clear on anything else (A_CLEAR)
    {
        if (c.shake > SHAKE_THR) I.shakeT += dt; else I.shakeT = 0.f;
        if (I.shakeRef > 0.f) I.shakeRef -= dt;
        if (I.shakeT > SHAKE_TIME && I.shakeRef <= 0.f) {
            I.shakeRef = SHAKE_REFRACT; I.shakeT = 0.f;
            const char* id = typeId(e->effectType(1));
            if (id && !strcmp(id, "freeze")) e->action(1, A_ALT, 1.f); else e->action(1, A_CLEAR, 1.f);
        }
    }

    // ---- 7. continuous targets
    // slot levels: F1..F3 -> MIX of S1..S3
    applyMix(I, e, T_F1, fa[0]); applyMix(I, e, T_F2, fa[1]); applyMix(I, e, T_F3, fa[2]);
    // joystick (only while engaged): X -> S1 PITCH (effect-aware centre), Y -> S1 AMT
    if (I.joyEngaged) {
        float nc = neutralPitchNorm(typeId(e->effectType(0)));
        float nx = jx < 0.f ? nc * (1.f + jx) : nc + (1.f - nc) * jx;
        applyRole(I, e, T_JX, R_PITCH, nx);
        applyRole(I, e, T_JY, R_AMT, 0.5f + 0.5f * jy);
    }
    // half-valve smear -> S1 MOD
    applyRole(I, e, T_HALF, R_MOD, 0.8f * halfS);
    // tilt: Y -> S2 MOD, X -> S3 MOD (v1 reverb warp: scale 1.236 around the centre)
    applyRole(I, e, T_TILTY, R_MOD, 0.5f + 0.5f * TILT_GAIN * ty);
    applyRole(I, e, T_TILTX, R_MOD, 0.5f + 0.5f * TILT_GAIN * tx);
    // S4: FSR pressure and breath (pot2 = breath depth) -> AMT; pot1 -> TIME; lidar -> TONE (close = dark)
    float fsrC = cl01((fsrS - FSR_DEAD) / (1.f - FSR_DEAD));
    float breath = po[1] * sqrtf(envS);
    float amt4 = fsrC > breath ? fsrC : breath;
    applyRole(I, e, T_AMT4, R_AMT, amt4);
    applyRole(I, e, T_TIME4, R_TIME, po[0]);
    applyRole(I, e, T_TONE4, R_TONE, 1.f - li);

    // ---- 8. LED frame
    for (int p = 0; p < 16; p++) {
        int row = p >> 2, col = p & 3;
        float lvl = 0.f; const uint8_t* rgb = kSlotCol[col];
        float breathe = 0.93f + 0.07f * sinf(6.2831853f * t_ / 7.f + (float)p * 0.5f);   // slow lava-lamp breathing
        bool has = e->effectType(col) >= 0;
        if (!page_) {
            if (p == 12) { rgb = kWhiteV; lvl = 0.30f + (I.joyEngaged ? 0.25f : 0.f) + 0.7f * I.flash[p]; }
            else if (!has) lvl = 0.f;
            else if (row == 0) {
                float mix = col < 3 ? fa[col] : amt4;
                lvl = e->bypassed(col) ? 0.06f : (0.30f + 0.70f * mix) * breathe;
                if (col == selSlot_) lvl += 0.12f;
                lvl += 0.5f * I.flash[p];
            }
            else if (row == 1) lvl = ((c.pads >> p) & 1 ? 1.0f : 0.16f) + 0.4f * I.flash[p];
            else if (row == 2) lvl = 0.10f + 0.8f * I.flash[p];
            else lvl = 0.05f + 0.9f * I.flash[p];
        } else {
            if (row == 0) { rgb = kSlotCol[col]; lvl = (col == I.bank) ? 1.0f : 0.14f; lvl += 0.5f * I.flash[p]; }
            else if (row == 1) { rgb = kSlotCol[selSlot_]; lvl = (col <= 1 ? 0.45f : 0.20f) + 0.5f * I.flash[p]; if (col >= 2) rgb = kWhiteV; }
            else if (row == 2) { lvl = (col == selSlot_) ? 1.0f : 0.14f; }
            else {
                if (p == 12) { rgb = kWhiteV; lvl = 1.0f; }
                else if (col == 1) { rgb = kWhiteV; lvl = I.dryAll ? 1.0f : 0.15f; }
                else if (col == 2) { rgb = kWhiteV; lvl = I.joyEngaged ? 1.0f : 0.15f; }
                else { rgb = kWhiteV; lvl = 0.10f + 0.9f * cl01(I.holdReset / RESET_HOLD); }
            }
        }
        lvl = cl01(lvl);
        for (int k = 0; k < 3; k++) leds_.rgb[p][k] = (uint8_t)(rgb[k] * lvl + 0.5f);
    }
    prev_ = c;
    (void)first;
}

} // namespace hx
