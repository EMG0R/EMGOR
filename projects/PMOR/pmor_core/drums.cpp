// drums.cpp — drum voice management (16 SuperVoices, one per pad, each driven by that pad's own SuperDef), the public drums* API from drums.h and
// the editor param pages. The synth engine is drum_super.cpp; the 96 preset recipes (SoundDef) live in drum_sounds.cpp and are mapped onto a pad's
// SuperDef by superFromDef() whenever the pad's sound changes.
#include <string.h>
#include <new>
#include "drums_priv.h"
#include "drum_super.h"
#include "platform.h"
#include "fx.h"
#include "clock.h"
#include "shared_state.h"
#include "sequencer.h"
#include "events.h"

DrumsState drums;

extern const char* const DRUM_GROUP_NAMES[DG_COUNT] = { "KICK", "SNARE", "HAT", "PERC" };

// ==================================================================== module state
// ---- drum synth engine: one SuperDef (every synthesis parameter, editable) + one SuperVoice per pad, raw SDRAM storage (no static constructors over SDRAM)
alignas(8) static uint8_t SDRAM_BSS s_svRaw[N_PADS][sizeof(SuperVoice)];
alignas(8) static uint8_t SDRAM_BSS s_defRaw[N_PADS][sizeof(SuperDef)];    // the pad's live params (what the editor changes)
alignas(8) static uint8_t SDRAM_BSS s_refRaw[N_PADS][sizeof(SuperDef)];    // the preset's params as loaded (custom = differs from this)
static uint8_t s_loaded[N_PADS];                                           // preset index the pad's params were loaded from (0xFF = reload on next use)
#ifndef DRUM_MAX_VOICES
#define DRUM_MAX_VOICES 4          // simultaneous sounding drum voices (see stealIfFull): cpu_max 86 % at 4 on an otherwise idle rig, 114 % + overruns at 6
#endif
#define SV(p)  (*reinterpret_cast<SuperVoice*>(s_svRaw[p]))
#define SDEF(p) (*reinterpret_cast<SuperDef*>(s_defRaw[p]))
#define SREF(p) (*reinterpret_cast<SuperDef*>(s_refRaw[p]))
// drum cycle probe (DWT on hardware): cycles spent in the per-sample voice loop, over blocks that had a voice running
static volatile uint32_t s_cycMax = 0, s_cycSum = 0, s_cycVs = 0, s_cycBlocks = 0;
float g_drumBenchF = 0.f;                                                   // bench: mode*1e6 + sel*100 + n (see drumsBenchBlock)
static uint32_t   s_ctrlRnd = 0xC0FFEEu;         // pan / tom-degree draws (control-ish, not audio-rate)
static MetalBank  s_metal;
static uint32_t   s_noiseRnd = 0x1234ABCDu;      // ONE shared noise LCG for every voice this sample (§6.3)

// choke -> pending-retrigger bookkeeping (owned here; SuperVoice only fades)
static bool  s_pendRetrig[N_PADS];
static float s_pendVel[N_PADS];

// preview: lock-free pending mask, CONTROL sets bits, AUDIO clears them
static volatile uint32_t s_previewMask = 0;

// pad param pages: length/pitch/tone/reverse, one static Param row per pad
static float      s_revF[N_PADS];                // float shadow of cfg.reverse (Param needs a float*)
static Param      s_padParams[N_PADS][5];
static ParamPage  s_padPages[N_PADS];
static const char* const s_revNames[2] = { "OFF", "ON" };

static Param      s_engParams[DG_COUNT][DENG_PARAMS];
static ParamPage  s_engPages[DG_COUNT];
extern const char* const DRUM_ENGINE_PARAM_NAMES[DG_COUNT][DENG_PARAMS] = {
    { "CLICK", "SUB",   "DRIVE", "CRUSH" },    // KICK
    { "SNAP",  "SHELL", "DRIVE", "CRUSH" },    // SNARE
    { "TICK",  "RING",  "DRIVE", "CRUSH" },    // HAT
    { "KNOCK", "BODY",  "DRIVE", "CRUSH" },    // PERC
};
// ---- deep recipe knobs: multipliers (1 = neutral) applied to a per-pad COPY of the selected sound's recipe at trigger time (copy only when something is not 1)
float SDRAM_BSS drum_deep[DG_COUNT][DDEEP_PARAMS];
const char* const DRUM_DEEP_NAMES[DDEEP_PARAMS] = { "PITCH", "DECAY", "LAYER2", "NOISEF", "NOISEQ", "NOISEDEC", "SWEEP", "FM" };
static Param SDRAM_BSS s_deepParams[DG_COUNT][DDEEP_PARAMS];
static ParamPage SDRAM_BSS s_deepPages[DG_COUNT];
void drumsDeepReset() { for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DDEEP_PARAMS; k++) drum_deep[g2][k] = 1.f; }
const ParamPage* drumsDeepPage(uint8_t group) { return group < DG_COUNT ? &s_deepPages[group] : nullptr; }
static Param      s_mixParams[DG_COUNT];
static ParamPage  s_mixPage;

// default patch order within each 4-pad group quadrant (spec §4.1)
static const uint8_t kDefaultSound[DG_COUNT][4] = {
    { 0, 1, 2, 3 },   // KICK:  KSUB (v2 beefiest sub), KCLUB (v2 phone-speaker), 808, 909
    { 0, 1, 2, 3 },   // SNARE: SNP1 (tight), 808 (medium), GATE (big), CLAP9
    { 0, 1, 2, 3 },   // HAT:   HCL (v2 short), CH808 (short), CRASH (long), HOP (v2 medium open)
    { 0, 1, 2, 3 },   // PERC:  808 tom, chirp, cowbell, clave
};


// ==================================================================== synth engine: preset load, per-pad params, macros
static void loadPreset(uint8_t pad) {
    DrumGroup grp = drumPadGroup(pad); uint8_t sound = drums.pad[pad].sound < DRUM_SOUNDS ? drums.pad[pad].sound : 0;
    superFromDef(SDEF(pad), drumDef((uint8_t)grp, sound), (int)grp, drumPadIndexInGroup(pad));
    memcpy(&SREF(pad), &SDEF(pad), sizeof(SuperDef));
    s_loaded[pad] = sound;
}
static inline void ensureLoaded(uint8_t pad) { if (s_loaded[pad] != drums.pad[pad].sound) loadPreset(pad); }
float* drumsSynthParamPtr(uint8_t pad, uint16_t k) { return (pad < N_PADS && k < SDEF_NPARAMS) ? (float*)&SDEF(pad) + SDEF_PARAMS[k].off : nullptr; }
bool drumsPadCustom(uint8_t pad) { return pad < N_PADS && memcmp(&SDEF(pad), &SREF(pad), sizeof(SuperDef)) != 0; }
void drumsPadReload(uint8_t pad) { if (pad < N_PADS) loadPreset(pad); }
const float* drumsPadSynthGet(uint8_t pad) { return pad < N_PADS ? (const float*)&SDEF(pad) : nullptr; }
void drumsPadSynthSet(uint8_t pad, const float* v) {      // patch load: custom params for the pad's current sound (clamped to the schema ranges; NaN -> preset value)
    if (pad >= N_PADS || !v) return;
    loadPreset(pad);
    float* d = (float*)&SDEF(pad);
    for (size_t k = 0; k < SDEF_NPARAMS; k++) { float x = v[SDEF_PARAMS[k].off]; if (x >= SDEF_PARAMS[k].mn && x <= SDEF_PARAMS[k].mx) d[SDEF_PARAMS[k].off] = x; }
}
// The 8 DEEP recipe multipliers + the 4 depth knobs are group MACROS over the per-pad params: applied to the voice's hot copy at trigger, never stored.
static void applyDeep(SuperDef& d, uint8_t grp) {
    const float* x = drum_deep[grp];
    bool neutral = true; for (int k = 0; k < DDEEP_PARAMS; k++) if (x[k] != 1.f) { neutral = false; break; }
    if (neutral) return;
    d.f0 *= x[0]; d.tune *= x[0];
    d.rT[0] *= x[1]; d.rT[1] *= x[1]; d.rT[2] *= x[1]; d.oT0 *= x[1]; d.oT1 *= x[1]; d.gT *= x[1]; d.cTailT *= x[1]; d.pingT *= x[1]; d.mTA *= x[1]; d.mTB *= x[1]; d.bTA *= x[1]; d.bTB *= x[1]; d.fT60 *= x[1];
    d.rG[1] *= x[2]; d.rG[2] *= x[2]; d.oG1 *= x[2]; d.mgB *= x[2];
    d.nF *= x[3]; d.cF *= x[3]; d.mBp1 *= x[3]; d.mBp2 *= x[3];
    d.nQ *= x[4]; d.cQ *= x[4]; d.mBp1Q *= x[4]; d.mBp2Q *= x[4];
    d.nT60 *= x[5]; d.nHold *= x[5];
    d.oMs *= x[6]; d.oBMs *= x[6]; d.rClickMs *= x[6]; d.fClickMs *= x[6]; d.nSweepMs *= x[6]; d.d2Ms *= x[6];
    d.fIdx0 *= x[7]; d.fIdx1 *= x[7]; d.rSelf *= x[7]; if (d.rSelf > 1.f) d.rSelf = 1.f;
}
static void trigSuper(uint8_t pad, float vel) {
    DrumGroup grp = drumPadGroup(pad); ensureLoaded(pad);
    SuperVoice& v = SV(pad); v.dd = SDEF(pad); applyDeep(v.dd, (uint8_t)grp);
    v.trigger(v.dd, drums.pad[pad], vel, (uint8_t)grp, drumPadIndexInGroup(pad), s_ctrlRnd);
}

// Polyphony cap: one hit costs ~1.2-2.4k cycles per sample on the M7 (tools/drumbench.sh), 16 simultaneous voices are 2-3x the whole block budget.
// A new hit on an idle pad while DRUM_MAX_VOICES are sounding fades out (4 ms) the OLDEST sounding voice. Retriggering a pad never counts (it chokes itself).
static void stealIfFull(uint8_t pad) {
    int act = 0; uint8_t oldest = 0xFF; uint32_t oa = 0;
    for (uint8_t p = 0; p < N_PADS; p++) { if (p == pad) continue; const SuperVoice& o = SV(p); if (o.state == SuperVoice::VS_ACTIVE) { act++; if (o.age >= oa) { oa = o.age; oldest = p; } } }
    if (act >= DRUM_MAX_VOICES && oldest != 0xFF) SV(oldest).startChoke(4);
}

// ==================================================================== bench (hardware measurement; see tools/drumbench.sh)
// g_drumBenchF = mode*1e6 + sel*100 + n. 0 = off (normal operation).
// sel 0..95 = group*24+sound (n pads of that group sound), 99 = every pad cycles through its group's sounds. Pads are re-hit whenever idle or every ~1 s.
static void drumsBenchBlock() {
    uint32_t v = (uint32_t)g_drumBenchF; uint8_t mode = (uint8_t)(v / 1000000u), sel = (uint8_t)((v / 100u) % 100u), n = (uint8_t)(v % 100u);
    const bool everyBlk = (mode >= 5);   // mode 2: re-hit when idle / every ~1 s; mode 5: re-hit every block (measures only the first samples of a hit)
    static const uint8_t order[16] = { 0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15 };
    static uint32_t blk = 0; blk++;
    int used = 0;
    for (int i = 0; i < 16 && used < n; i++) {
        uint8_t pad = order[i]; uint8_t grp = (uint8_t)drumPadGroup(pad);
        if (sel < 96 && grp != sel / 24) continue;
        used++;
        bool idle = SV(pad).state == SuperVoice::VS_IDLE;
        if (idle || everyBlk || (blk & 63u) == 0u) {
            drums.pad[pad].sound = sel < 96 ? (uint8_t)(sel % 24) : (uint8_t)((drums.pad[pad].sound + 1) % DRUM_SOUNDS);
            drumsTrigger(pad, 1.f);
        }
    }
}
void drumsCycStats(uint32_t* mx, uint32_t* perVs, uint32_t* blocks, uint32_t* vs) {
    *mx = s_cycMax; *perVs = s_cycVs ? s_cycSum / s_cycVs : 0; *blocks = s_cycBlocks; *vs = s_cycVs;
    s_cycMax = 0; s_cycSum = 0; s_cycVs = 0; s_cycBlocks = 0;
}

// ==================================================================== pad <-> group
// TL quadrant (rows 0-1, cols 0-1) = PERC, TR = HAT, BL = KICK, BR = SNARE.
DrumGroup drumPadGroup(uint8_t pad) {
    uint8_t r = pad / 4, c = pad % 4;
    if (r < 2) return c < 2 ? DG_PERC : DG_HAT;
    return c < 2 ? DG_KICK : DG_SNARE;
}
uint8_t drumPadIndexInGroup(uint8_t pad) {
    return (uint8_t)(((pad / 4) & 1u) * 2u + ((pad % 4) & 1u));
}

// ==================================================================== init
void drumsInit() {
    memset(&drums, 0, sizeof(drums));
    drumsDeepReset();
    drums.volume = 1.f;
    for (int i = 0; i < DG_COUNT; i++) drums.group_vol[i] = 1.f;

    for (uint8_t pad = 0; pad < N_PADS; pad++) {
        DrumGroup grp = drumPadGroup(pad);
        uint8_t   idx = drumPadIndexInGroup(pad);
        DrumPadCfg& c = drums.pad[pad];
        c.sound   = kDefaultSound[grp][idx];
        c.length  = 0.5f;
        c.pitch   = 0.f;
        c.tone    = 0.5f;
        c.reverse = false;
        c.vol     = 1.f;
        s_revF[pad] = 0.f;

        s_pendRetrig[pad] = false;
        s_pendVel[pad] = 0.f;
    }

    for (uint8_t pad = 0; pad < N_PADS; pad++) { new (&s_svRaw[pad]) SuperVoice(); new (&s_defRaw[pad]) SuperDef(); new (&s_refRaw[pad]) SuperDef(); loadPreset(pad); }
    g_drumBenchF = 0.f;
    sdefInitEngines();
    s_ctrlRnd = 0xC0FFEEu; s_noiseRnd = 0x1234ABCDu;   // reproducible per init (a fresh process starts here, so renders are unchanged)
    s_metal.init();
    s_previewMask = 0;

    // ---- pad pages (length, pitch, tone, reverse) — built once, pointers
    // into drums.pad[pad] / s_revF[pad] stay valid for the life of the app.
    for (uint8_t pad = 0; pad < N_PADS; pad++) {
        DrumPadCfg& c = drums.pad[pad];
        Param* p = s_padParams[pad];
        p[0] = { "LENGTH", &c.length, 0.f, 1.f, 0.01f, PF_NONE, nullptr };
        p[1] = { "PITCH",  &c.pitch, -1.f, 1.f, 0.01f, PF_BIPOLAR, nullptr };
        p[2] = { "TONE",   &c.tone,  0.f, 1.f, 0.01f, PF_NONE, nullptr };
        p[3] = { "REV",    &s_revF[pad], 0.f, 1.f, 1.f, (uint8_t)(PF_ENUM | PF_QUANT), s_revNames };
        p[4] = { "VOL",    &c.vol,   0.f, 2.f, 0.1f, PF_PERCENT, nullptr };
        s_padPages[pad] = { "PAD", p, 5 };
    }

    // ---- mix page (group volumes, 0..125 %)
    s_mixParams[0] = { "KICK",  &drums.group_vol[DG_KICK],  0.f, 1.25f, 0.01f, PF_PERCENT, nullptr };
    s_mixParams[1] = { "SNARE", &drums.group_vol[DG_SNARE], 0.f, 1.25f, 0.01f, PF_PERCENT, nullptr };
    s_mixParams[2] = { "HAT",   &drums.group_vol[DG_HAT],   0.f, 1.25f, 0.01f, PF_PERCENT, nullptr };
    s_mixParams[3] = { "PERC",  &drums.group_vol[DG_PERC],  0.f, 1.25f, 0.01f, PF_PERCENT, nullptr };
    s_mixPage = { "MIX", s_mixParams, DG_COUNT };

    // ---- engine depth pages (live knobs, 0 = neutral)
    for (uint8_t g = 0; g < DG_COUNT; g++) {
        for (uint8_t k = 0; k < DENG_PARAMS; k++)
            s_engParams[g][k] = { DRUM_ENGINE_PARAM_NAMES[g][k], &drums.eng[g].x[k], 0.f, 1.f, 0.01f, PF_PERCENT, nullptr };
        s_engPages[g] = { DRUM_GROUP_NAMES[g], s_engParams[g], DENG_PARAMS };
        for (uint8_t k = 0; k < DDEEP_PARAMS; k++)
            s_deepParams[g][k] = { DRUM_DEEP_NAMES[k], &drum_deep[g][k], 0.25f, 4.f, 0.05f, (uint8_t)PF_LOG, nullptr };
        s_deepPages[g] = { DRUM_GROUP_NAMES[g], s_deepParams[g], DDEEP_PARAMS };
    }
}

const ParamPage* drumsEnginePage(uint8_t group) { return group < DG_COUNT ? &s_engPages[group] : nullptr; }
int drumsActiveVoices() {
    int n = 0;
    for (uint8_t i = 0; i < N_PADS; i++) if (SV(i).state != SuperVoice::VS_IDLE) n++;
    return n;
}
uint8_t drumGroupFirstPad(uint8_t group) {
    for (uint8_t p = 0; p < N_PADS; p++) if (drumPadGroup(p) == group && drumPadIndexInGroup(p) == 0) return p;
    return 0;
}

// ==================================================================== trigger / choke
// AUDIO plane, ISR-safe: no allocation, no blocking. Retrigger chokes the
// pad's own voice (3-5 ms raised-cosine fade) then restarts on the next
// drumsProcess sample once the fade has finished (see drumsProcess below).
void drumsTrigger(uint8_t pad, float vel) {
    if (pad >= N_PADS) return;
    { int v127 = (int)(vel * 127.f + 0.5f); pmorEventPush(EV_DRUM_ON, pad, (uint8_t)(v127 < 1 ? 1 : v127 > 127 ? 127 : v127), 0); }
    SuperVoice& v = SV(pad);
    if (v.state == SuperVoice::VS_IDLE) {
        stealIfFull(pad);
        trigSuper(pad, vel); s_pendRetrig[pad] = false;
    } else {
        v.startChoke(4);              // 3-5 ms fade of whatever's currently sounding
        s_pendRetrig[pad] = true;
        s_pendVel[pad] = vel;
    }
}

void drumsPreview(uint8_t pad) {
    if (pad >= N_PADS) return;
    fxNoteLive(ENG_DRUMS, pad);                                  // v2 live latch
    {   // tap guard: a sequenced hit of this pad just sounded -> one hit, not two
        uint32_t now = clockSampleNow(), guard = (uint32_t)(clk.step_samples * TAP_GUARD_STEPS);
        uint32_t last = g.seq_hit_at[ENG_DRUMS][pad];
        bool swallow = (last != 0u) && (now - last) < guard;
        g.tap_at[ENG_DRUMS][pad] = now ? now : 1u;
        if (swallow) return;
    }
    __atomic_fetch_or(&s_previewMask, (1u << pad), __ATOMIC_RELAXED);
}

// ==================================================================== process
void drumsProcess(float* outL, float* outR, size_t n) {
    // consume pending manual-preview hits (lock-free: audio owns the clear)
    uint32_t mask = __atomic_exchange_n(&s_previewMask, 0u, __ATOMIC_RELAXED);
    while (mask) {
        uint8_t pad = (uint8_t)__builtin_ctz(mask);
        mask &= mask - 1u;
        drumsTrigger(pad, 0.85f);
    }

    // sync the REVERSE shadow float -> the bool cfg actually uses
    for (uint8_t pad = 0; pad < N_PADS; pad++)
        drums.pad[pad].reverse = (s_revF[pad] >= 0.5f);

    float engVol = clampf(drums.volume, 0.f, ENGINE_VOL_MAX);
    for (uint8_t pad = 0; pad < N_PADS; pad++) ensureLoaded(pad);       // a sound change from any path (UI, host, patch): the new preset's params
    if (g_drumBenchF > 0.f) drumsBenchBlock();
    uint32_t cyc0 = pmorCycles(), vsN = 0;

    {
        // voice-major: one voice runs its whole block (code + state stay hot), the shared noise / metal signals are made once per block.
        // Sums are accumulated in pad order per sample, exactly the order of the old sample-major loop.
        float noise[BLOCK], metal[BLOCK], accL[BLOCK], accR[BLOCK];
        bool needMetal = false;
        for (uint8_t pad = 0; pad < N_PADS; pad++)
            if ((SV(pad).state != SuperVoice::VS_IDLE && SV(pad).dd.mG > 0.f) || (s_pendRetrig[pad] && SDEF(pad).mG > 0.f)) { needMetal = true; break; }
        for (size_t i = 0; i < n; i++) { noise[i] = lcg01(s_noiseRnd) * 2.f - 1.f; accL[i] = 0.f; accR[i] = 0.f; }
        if (needMetal) for (size_t i = 0; i < n; i++) metal[i] = s_metal.tick(); else s_metal.advance((uint32_t)n);
        for (uint8_t pad = 0; pad < N_PADS; pad++) {
            SuperVoice& v = SV(pad);
            if (v.state == SuperVoice::VS_IDLE) continue;
            for (size_t i = 0; i < n && v.state != SuperVoice::VS_IDLE; i++) {
                vsN++;
                bool wasChoke = (v.state == SuperVoice::VS_CHOKE);
                float s = v.process(needMetal ? metal[i] : 0.f, noise[i]);
                if (wasChoke && v.state == SuperVoice::VS_IDLE && s_pendRetrig[pad]) { s_pendRetrig[pad] = false; trigSuper(pad, s_pendVel[pad]); }
                float gv = clampf(drums.group_vol[v.group], 0.f, ENGINE_VOL_MAX);
                float gg = s * gv * engVol * clampf(drums.pad[pad].vol, 0.f, 2.f);
                accL[i] += gg * v.panL; accR[i] += gg * v.panR;
            }
        }
        for (size_t i = 0; i < n; i++) { outL[i] += flushBad(accL[i]); outR[i] += flushBad(accR[i]); }
    }
    uint32_t dc = pmorCycles() - cyc0;
    if (vsN) { if (dc > s_cycMax) s_cycMax = dc; s_cycSum += dc; s_cycVs += vsN; s_cycBlocks++; }
}

// ==================================================================== editor
const ParamPage* drumsPadPage(uint8_t pad) {
    if (pad >= N_PADS) return nullptr;
    return &s_padPages[pad];
}
const ParamPage* drumsMixPage() { return &s_mixPage; }
