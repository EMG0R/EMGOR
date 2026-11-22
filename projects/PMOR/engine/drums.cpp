// drums.cpp — voice management (16 voices, one per pad), the public drums*
// API from drums.h, and the editor param pages. Synthesis engines + the 64
// SoundDef recipes live in drum_sounds.cpp (see drums_priv.h for the split).
#include <string.h>
#include "drums_priv.h"
#include "fx.h"
#include "clock.h"
#include "shared_state.h"
#include "sequencer.h"

DrumsState drums;

extern const char* const DRUM_GROUP_NAMES[DG_COUNT] = { "KICK", "SNARE", "HAT", "PERC" };

// ==================================================================== module state
static DrumVoice  s_voice[DRUM_VOICES];         // DRUM_VOICES == N_PADS, 1:1 with pads
static uint32_t   s_ctrlRnd = 0xC0FFEEu;         // pan / tom-degree draws (control-ish, not audio-rate)
static MetalBank  s_metal;
static uint32_t   s_noiseRnd = 0x1234ABCDu;      // ONE shared noise LCG for every voice this sample (§6.3)

// choke -> pending-retrigger bookkeeping (owned here; DrumVoice only fades)
static bool  s_pendRetrig[N_PADS];
static float s_pendVel[N_PADS];

// preview: lock-free pending mask, CONTROL sets bits, AUDIO clears them
static volatile uint32_t s_previewMask = 0;

// pad param pages: length/pitch/tone/reverse, one static Param row per pad
static float      s_revF[N_PADS];                // float shadow of cfg.reverse (Param needs a float*)
static Param      s_padParams[N_PADS][5];
static ParamPage  s_padPages[N_PADS];
static const char* const s_revNames[2] = { "OFF", "ON" };

static Param      s_mixParams[DG_COUNT];
static ParamPage  s_mixPage;

// default patch order within each 4-pad group quadrant (spec §4.1)
static const uint8_t kDefaultSound[DG_COUNT][4] = {
    { 0, 1, 2, 3 },   // KICK:  KSUB (v2 beefiest sub), KCLUB (v2 phone-speaker), 808, 909
    { 0, 1, 2, 3 },   // SNARE: SNP1 (tight), 808 (medium), GATE (big), CLAP9
    { 0, 1, 2, 3 },   // HAT:   HCL (v2 short), CH808 (short), CRASH (long), HOP (v2 medium open)
    { 0, 1, 2, 3 },   // PERC:  808 tom, chirp, cowbell, clave
};

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

        s_voice[pad] = DrumVoice();
        s_pendRetrig[pad] = false;
        s_pendVel[pad] = 0.f;
    }

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
}

// ==================================================================== trigger / choke
// AUDIO plane, ISR-safe: no allocation, no blocking. Retrigger chokes the
// pad's own voice (3-5 ms raised-cosine fade) then restarts on the next
// drumsProcess sample once the fade has finished (see drumsProcess below).
void drumsTrigger(uint8_t pad, float vel) {
    if (pad >= N_PADS) return;
    DrumVoice& v = s_voice[pad];
    if (v.state == DrumVoice::VS_IDLE) {
        DrumGroup grp = drumPadGroup(pad);
        uint8_t   idx = drumPadIndexInGroup(pad);
        const DrumPadCfg& cfg = drums.pad[pad];
        uint8_t   sound = cfg.sound < DRUM_SOUNDS ? cfg.sound : 0;
        v.trigger(DRUM_DEFS[grp][sound], cfg, vel, (uint8_t)grp, idx, sound, s_ctrlRnd);
        s_pendRetrig[pad] = false;
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

    for (size_t i = 0; i < n; i++) {
        float metal = s_metal.tick();
        float noise = lcg01(s_noiseRnd) * 2.f - 1.f;   // one shared noise sample this tick

        float sumL = 0.f, sumR = 0.f;
        for (uint8_t pad = 0; pad < N_PADS; pad++) {
            DrumVoice& v = s_voice[pad];
            if (v.state == DrumVoice::VS_IDLE) continue;

            bool wasChoke = (v.state == DrumVoice::VS_CHOKE);
            float s = v.process(metal, noise);

            if (wasChoke && v.state == DrumVoice::VS_IDLE && s_pendRetrig[pad]) {
                s_pendRetrig[pad] = false;
                DrumGroup grp = drumPadGroup(pad);
                uint8_t   idx = drumPadIndexInGroup(pad);
                const DrumPadCfg& cfg = drums.pad[pad];
                uint8_t   sound = cfg.sound < DRUM_SOUNDS ? cfg.sound : 0;
                v.trigger(DRUM_DEFS[grp][sound], cfg, s_pendVel[pad], (uint8_t)grp, idx, sound, s_ctrlRnd);
            }

            float gv = clampf(drums.group_vol[v.group], 0.f, ENGINE_VOL_MAX);
            float g = s * gv * engVol * clampf(drums.pad[pad].vol, 0.f, 2.f);
            sumL += g * v.panL;
            sumR += g * v.panR;
        }

        outL[i] += flushBad(sumL);
        outR[i] += flushBad(sumR);
    }
}

// ==================================================================== editor
const ParamPage* drumsPadPage(uint8_t pad) {
    if (pad >= N_PADS) return nullptr;
    return &s_padPages[pad];
}
const ParamPage* drumsMixPage() { return &s_mixPage; }
