// sampler.cpp — 16-slot mono sampler. Record on-press/off-release with fades,
// up to 8 voices (varispeed pitch, 2-grain formant-shift, one-pole cutoff,
// reverse, generative pan), oldest-steal / same-slot choke with 5 ms fade.
#include <string.h>
#include "sampler.h"
#include "sequencer.h"
#include "shared_state.h"
#include "fx.h"
#include "engine_util.h"
#include "clock.h"
#include "events.h"

// ---------------------------------------------------------------- storage
SamplerState smp;
float SDRAM_BSS smp_buf[SMP_SLOTS][SMP_LEN];

// ---------------------------------------------------------------- constants
static constexpr uint32_t GRAIN_LEN      = 960u;   // 20 ms @ 48 kHz
static constexpr uint32_t GRAIN_HALF     = GRAIN_LEN / 2u;
static constexpr uint32_t FADE_IN_SMP    = 96u;    // 2 ms voice-start fade
static constexpr uint32_t FADE_NAT_SMP   = 96u;    // 2 ms natural end fade
static constexpr uint32_t FADE_CHOKE_SMP = 240u;   // 5 ms retrigger/steal fade
static constexpr uint32_t REC_FADE_SMP   = 144u;   // 3 ms record fade
static constexpr uint32_t REC_LEN_MIN    = 480u;

// ---------------------------------------------------------------- pan/rand state
static uint32_t s_rng = 0xC001D00Du;
void samplerReseed(uint32_t s) { s_rng = 0xC001D00Du; if (s) s_rng ^= s * 2654435761u; }

// ---------------------------------------------------------------- grain window LUT
static float s_hann[GRAIN_LEN];

// ---------------------------------------------------------------- reverse shadow (float, editor-facing)
static float s_reverse_shadow[SMP_SLOTS];

// ---------------------------------------------------------------- manual play (control -> audio)
static volatile uint16_t s_pending_mask = 0;
static float             s_pending_vel[SMP_SLOTS];

// ---------------------------------------------------------------- voices
enum VState : uint8_t { V_FREE = 0, V_FADEIN, V_SUSTAIN, V_FADEOUT };

struct SmpVoice {
    VState   state;
    int8_t   slot;
    uint32_t start_id;
    // playback
    int8_t   dir;             // +1 forward, -1 reverse
    float    pos;              // plain-mode read position
    float    rate;             // grain-internal / plain resample speed (2^pitch * varispeed)
    bool     plain;             // pitch==0 && formant==0 -> bit-transparent path
    // formant grains
    float    formant_ratio;    // 2^formant, advances the grain-schedule position
    float    sched_pos;
    float    g_read[2];
    float    g_age[2];
    // one-pole cutoff (bypass when cutoff >= ~1)
    bool     filt_on;
    float    filt_a;
    float    filt_y;
    // amplitude envelope (fade-in / sustain / fade-out)
    float    amp;
    float    amp_step;
    // mix
    float    gain;
    float    pan;               // 0..1, 0=full L .. 1=full R
    // region / loop / release (SmpSlotExt, latched at voice start)
    float    rs, re; bool loop; uint32_t rel_smp;
    // stutter: stut = this voice is a chop; mute = gain of a shadow (the real voice, silenced while chops play)
    bool     stut; float mute, mute_to, mute_step;
    // natural-end tracking (output-sample countdown)
    uint32_t out_left;
    // steal/choke queue: next note to start once this voice's fade-out completes
    int8_t   pending_slot;
    float    pending_vel;
};
static SmpVoice voices[SYN_MAX_POLY];
static uint32_t s_voice_seq = 0;

// TASK 3 (2026-10): active-voice count for samplerActiveVoices() -- see sampler.h.
// Written only from the audio-ISR context (startVoice() / the V_FREE transition in
// samplerProcess()'s per-sample loop below), read from the control loop; volatile,
// no lock, same convention as the rest of this codebase's ISR/control-loop splits.
static volatile uint8_t s_active_voice_count = 0;

// ---------------------------------------------------------------- param pages
static const char* const REV_NAMES[2] = { "OFF", "ON" };
SmpSlotExt SDRAM_BSS smp_ext[SMP_SLOTS];
static Param     SDRAM_BSS s_extParams[SMP_SLOTS][5];
static ParamPage SDRAM_BSS s_extPages[SMP_SLOTS];
static Param     s_slotParams[SMP_SLOTS][4];
static ParamPage s_slotPages[SMP_SLOTS];
static Param     s_mixParams[4];
static ParamPage s_mixPageObj;

static void buildParamPages() {
    for (int i = 0; i < SMP_SLOTS; i++) {
        Param* p = s_slotParams[i];
        p[0] = { "PITCH",   &smp.slot[i].pitch,   -1.f, 1.f,  0.01f, (uint8_t)(PF_PAIR_X | PF_BIPOLAR), nullptr };
        p[1] = { "FORMANT", &smp.slot[i].formant, -1.f, 1.f,  0.01f, (uint8_t)PF_BIPOLAR,               nullptr };
        p[2] = { "CUTOFF",  &smp.slot[i].cutoff,   0.f, 1.f,  0.01f, (uint8_t)PF_NONE,                  nullptr };
        p[3] = { "REVERSE", &s_reverse_shadow[i],  0.f, 1.f,  1.f,   (uint8_t)(PF_QUANT | PF_ENUM),     REV_NAMES };
        s_slotPages[i] = { "SAMPLE", s_slotParams[i], 4 };
    }
    for (int i = 0; i < SMP_SLOTS; i++) {
        Param* p = s_extParams[i];
        p[0] = { "START",   &smp_ext[i].start,      0.f, 1.f,    0.005f, (uint8_t)PF_PERCENT,              nullptr };
        p[1] = { "END",     &smp_ext[i].end,        0.f, 1.f,    0.005f, (uint8_t)PF_PERCENT,              nullptr };
        p[2] = { "LOOP",    &smp_ext[i].loop_f,     0.f, 1.f,    1.f,    (uint8_t)(PF_QUANT | PF_ENUM),    REV_NAMES };
        p[3] = { "ATTACK",  &smp_ext[i].attack_ms,  1.f, 2000.f, 1.f,    (uint8_t)PF_LOG,                  nullptr };
        p[4] = { "RELEASE", &smp_ext[i].release_ms, 1.f, 4000.f, 1.f,    (uint8_t)PF_LOG,                  nullptr };
        s_extPages[i] = { "SMP EXT", s_extParams[i], 5 };
    }
    s_mixParams[0] = { "VOLUME",   &smp.volume,     0.f,  1.25f, 0.01f, (uint8_t)PF_PERCENT, nullptr };
    s_mixParams[1] = { "WIDTH",    &smp.width,      0.f,  1.f,   0.01f, (uint8_t)PF_PAIR_X,  nullptr };
    s_mixParams[2] = { "WIDTHPRB", &smp.width_prob, 0.f,  1.f,   0.01f, (uint8_t)PF_NONE,    nullptr };
    s_mixParams[3] = { "POLY",     &smp.polyphony,  1.f,  8.f,   1.f,   (uint8_t)(PF_QUANT | PF_INT), nullptr };
    s_mixPageObj   = { "SAMPLER", s_mixParams, 4 };
}

// ---------------------------------------------------------------- init
static bool s_stut_prev = false;
void samplerInit() {
    s_stut_prev = false;
    memset(smp_buf, 0, sizeof(smp_buf));   // hwBoot runs engine inits before patchInit: safe
    memset(&smp, 0, sizeof(smp));
    for (int i = 0; i < SMP_SLOTS; i++) {
        smp.slot[i].pitch   = 0.f;
        smp.slot[i].formant = 0.f;
        smp.slot[i].cutoff  = 1.f;
        smp.slot[i].reverse = false;
        smp.len[i]          = 0;
        s_reverse_shadow[i] = 0.f;
        smp_ext[i] = SmpSlotExt{ 0.f, 1.f, 0.f, SMP_ATK_DEF_MS, SMP_REL_DEF_MS };
        s_pending_vel[i]    = 0.f;
    }
    smp.volume     = 1.0f;
    smp.width      = 0.3f;
    smp.width_prob = 0.5f;
    smp.polyphony  = 4.f;
    smp.rec_slot   = -1;
    smp.rec_pos    = 0;
    smp.audio_dirty = false;

    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        v.state = V_FREE; v.slot = -1; v.start_id = 0;
        v.dir = 1; v.pos = 0.f; v.rate = 1.f; v.plain = true;
        v.formant_ratio = 1.f; v.sched_pos = 0.f;
        v.g_read[0] = v.g_read[1] = 0.f; v.g_age[0] = v.g_age[1] = 0.f;
        v.filt_on = false; v.filt_a = 0.f; v.filt_y = 0.f;
        v.amp = 0.f; v.amp_step = 0.f;
        v.gain = 0.f; v.pan = 0.5f;
        v.out_left = 0; v.pending_slot = -1; v.pending_vel = 0.f;
        v.rs = 0.f; v.re = 0.f; v.loop = false; v.rel_smp = FADE_NAT_SMP;
        v.stut = false; v.mute = v.mute_to = 1.f; v.mute_step = 0.f;
    }
    s_pending_mask = 0;
    s_voice_seq = 0;
    s_active_voice_count = 0;   // TASK 3: every voice reset to V_FREE above
    s_rng = 0xC001D00Du;

    for (uint32_t i = 0; i < GRAIN_LEN; i++)
        s_hann[i] = 0.5f - 0.5f * cosf(6.28318530718f * (float)i / (float)GRAIN_LEN);

    buildParamPages();
}

// ---------------------------------------------------------------- buffer read (linear interp, clamped)
static inline float bufRead(int8_t slot, float pos, uint32_t len) {
    if (len == 0) return 0.f;
    if (pos < 0.f) pos = 0.f;
    float maxp = (float)(len - 1);
    if (pos > maxp) pos = maxp;
    uint32_t i0 = (uint32_t)pos;
    uint32_t i1 = (i0 + 1u < len) ? (i0 + 1u) : i0;
    float frac = pos - (float)i0;
    float a = smp_buf[slot][i0];
    float b = smp_buf[slot][i1];
    return a + (b - a) * frac;
}

// ---------------------------------------------------------------- record finalize (fade-out + len)
static void finishRecording() {
    int8_t rs = smp.rec_slot;
    if (rs < 0) return;
    uint32_t len = smp.rec_pos;
    if (len < REC_LEN_MIN) len = REC_LEN_MIN;
    if (len > SMP_LEN)      len = SMP_LEN;
    uint32_t fadeN = (len < REC_FADE_SMP) ? len : REC_FADE_SMP;
    for (uint32_t i = 0; i < fadeN; i++) {
        uint32_t idx = len - 1u - i;
        float g = (float)i / (float)fadeN;      // 0 at buffer end .. 1 moving inward
        smp_buf[rs][idx] *= g;
    }
    smp.len[rs]  = len;
    smp.rec_slot = -1;
    smp.audio_dirty = true;
}

// ---------------------------------------------------------------- CONTROL plane: record
bool samplerRecordStart(uint8_t slot) {
    if (slot >= SMP_SLOTS) return false;
    if (smp.len[slot] > 0) return false;   // must delete first
    smp.rec_pos  = 0;
    smp.rec_slot = (int8_t)slot;           // audio plane starts copying next block: press-exact
    return true;
}
void samplerRecordStop() { finishRecording(); }

void samplerDelete(uint8_t slot) {
    if (slot >= SMP_SLOTS) return;
    smp.len[slot] = 0;
    smp.audio_dirty = true;
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state != V_FREE && v.slot == (int8_t)slot) {
            v.pending_slot = -1;
            v.state = V_FADEOUT;
            v.amp_step = v.amp / (float)FADE_CHOKE_SMP;
        }
    }
}

void samplerBlankAll() { for (uint8_t i = 0; i < SMP_SLOTS; i++) samplerDelete(i); }

void samplerSwapSlots(uint8_t a, uint8_t b) {
    if (a >= SMP_SLOTS || b >= SMP_SLOTS || a == b) return;
    uint32_t la = smp.len[a], lb = smp.len[b];
    smp.len[a] = 0; smp.len[b] = 0;                          // silent (and voices read len 0) while the buffers trade places
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state != V_FREE && (v.slot == (int8_t)a || v.slot == (int8_t)b)) {
            v.pending_slot = -1; v.state = V_FADEOUT; v.amp_step = v.amp / (float)FADE_CHOKE_SMP;
        }
    }
    uint32_t n = la > lb ? la : lb;
    for (uint32_t i = 0; i < n; i++) { float t = smp_buf[a][i]; smp_buf[a][i] = smp_buf[b][i]; smp_buf[b][i] = t; }
    SmpSlotCfg tc = smp.slot[a]; smp.slot[a] = smp.slot[b]; smp.slot[b] = tc;
    SmpSlotExt te = smp_ext[a]; smp_ext[a] = smp_ext[b]; smp_ext[b] = te;
    s_reverse_shadow[a] = smp.slot[a].reverse ? 1.f : 0.f; s_reverse_shadow[b] = smp.slot[b].reverse ? 1.f : 0.f;
    smp.len[a] = lb; smp.len[b] = la;
    smp.audio_dirty = true;
}

void samplerPlay(uint8_t slot, float vel) {
    if (slot >= SMP_SLOTS) return;
    fxNoteLive(ENG_SMP, slot);                                   // v2 live latch
    {   // tap guard: a sequenced hit of this slot just sounded -> one hit, not two
        uint32_t now = clockSampleNow(), guard = (uint32_t)(clk.step_samples * TAP_GUARD_STEPS);
        uint32_t last = g.seq_hit_at[ENG_SMP][slot];
        bool swallow = (last != 0u) && (now - last) < guard;
        g.tap_at[ENG_SMP][slot] = now ? now : 1u;
        if (swallow) return;
    }
    s_pending_vel[slot] = vel;
    s_pending_mask |= (uint16_t)(1u << slot);
}

// ---------------------------------------------------------------- pan
static float pickPan(int poly) {
    if (poly == 2) {
        static uint32_t rr = 0;
        static const float tbl[2] = { 0.25f, 0.75f };
        return tbl[rr++ & 1u];
    }
    if (poly == 3) {
        static uint32_t rr = 0;
        static const float tbl[3] = { 0.15f, 0.5f, 0.85f };
        float v = tbl[rr % 3u]; rr++; return v;
    }
    if (lcg01(s_rng) < smp.width_prob) {
        float d = (lcg01(s_rng) * 2.f - 1.f) * smp.width;
        return clampf(0.5f + d, 0.f, 1.f);
    }
    return 0.5f;
}

// ---------------------------------------------------------------- voice start
static void startVoice(SmpVoice& v, uint8_t slot, float vel) {
    { int v127 = (int)(vel * 127.f + 0.5f); pmorEventPush(EV_SMP_ON, slot, (uint8_t)(v127 < 1 ? 1 : v127 > 127 ? 127 : v127), 0); }
    SmpSlotCfg& cfg = smp.slot[slot];
    uint32_t len = smp.len[slot];
    v.slot = (int8_t)slot;
    v.dir  = cfg.reverse ? -1 : 1;
    const SmpSlotExt& ex = smp_ext[slot];
    {   // region: defaults (0..1) give rs = 0, re = len exactly (the old voice)
        float rs = ex.start <= 0.f ? 0.f : (float)len * ex.start, re = ex.end >= 1.f ? (float)len : (float)len * ex.end;
        if (re - rs < 2.f) { rs = 0.f; re = (float)len; }                 // empty / inverted window: play the whole recording
        v.rs = rs; v.re = re; v.loop = ex.loop_f > 0.5f;
    }
    v.rel_smp = (ex.release_ms == SMP_REL_DEF_MS) ? FADE_NAT_SMP : (uint32_t)(ex.release_ms * 48.f) + 1u;
    v.pos  = cfg.reverse ? (v.re > 0.f ? v.re - 1.f : 0.f) : v.rs;
    v.rate = fastPow2(cfg.pitch) * clk.varispeed;
    if (v.rate < 0.01f) v.rate = 0.01f;
    v.formant_ratio = fastPow2(cfg.formant);
    v.plain = (cfg.pitch == 0.f && cfg.formant == 0.f);
    v.sched_pos = v.pos;
    v.g_read[0] = v.pos; v.g_read[1] = v.pos;
    v.g_age[0]  = 0.f;   v.g_age[1]  = (float)GRAIN_HALF;

    v.filt_on = (cfg.cutoff < 0.999f);
    if (v.filt_on) {
        float freq = 200.f * fastPow2(cfg.cutoff * 6.643856f);   // 200 Hz..20 kHz exp
        v.filt_a = 1.f - fastExp(-6.28318530718f * freq / SR);
    }
    v.filt_y = 0.f;

    int poly = (int)(smp.polyphony + 0.5f);
    if (poly < 1) poly = 1;
    if (poly > SYN_MAX_POLY) poly = SYN_MAX_POLY;
    v.pan  = pickPan(poly);
    v.gain = vel * smp.volume * 0.5f;

    v.amp = 0.f;
    v.amp_step = (ex.attack_ms == SMP_ATK_DEF_MS) ? 1.f / (float)FADE_IN_SMP : 1.f / (ex.attack_ms * 48.f);
    v.state = V_FADEIN;
    v.pending_slot = -1;
    v.stut = false; v.mute = v.mute_to = 1.f; v.mute_step = 0.f;
    v.start_id = ++s_voice_seq;
    s_active_voice_count++;   // TASK 3: FREE -> active (every startVoice() call is a FREE voice being claimed)

    float advance = v.plain ? v.rate : v.formant_ratio;
    if (advance < 0.01f) advance = 0.01f;
    v.out_left = v.loop ? (uint32_t)(8.f * SR) : (uint32_t)((v.re - v.rs) / advance);   // loop: ends on the slot's next hit (choke) or after 8 s
}

static void startFadeOut(SmpVoice& v, uint32_t durSamples) {
    if (v.state == V_FREE) return;
    v.state = V_FADEOUT;
    v.amp_step = v.amp / (float)durSamples;
}

static SmpVoice* findFree() {
    for (int i = 0; i < SYN_MAX_POLY; i++) if (voices[i].state == V_FREE) return &voices[i];
    return nullptr;
}
static SmpVoice* oldestActive() {
    SmpVoice* best = nullptr; uint32_t bestId = 0xFFFFFFFFu;
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if ((v.state == V_SUSTAIN || v.state == V_FADEIN) && v.start_id < bestId) { bestId = v.start_id; best = &v; }
    }
    return best;
}

static void triggerVoice(uint8_t slot, float vel) {
    if (slot >= SMP_SLOTS) return;
    if (smp.len[slot] == 0) return;

    // retrigger of a still-sounding same slot: choke it (5 ms), new hit gets its own voice
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state != V_FREE && v.slot == (int8_t)slot && v.state != V_FADEOUT)
            startFadeOut(v, FADE_CHOKE_SMP);
    }

    int poly = (int)(smp.polyphony + 0.5f);
    if (poly < 1) poly = 1;
    if (poly > SYN_MAX_POLY) poly = SYN_MAX_POLY;

    int active = 0;
    for (int i = 0; i < SYN_MAX_POLY; i++)
        if (voices[i].state == V_SUSTAIN || voices[i].state == V_FADEIN) active++;

    if (active >= poly) {
        SmpVoice* victim = oldestActive();
        if (victim) {
            startFadeOut(*victim, FADE_CHOKE_SMP);
            victim->pending_slot = (int8_t)slot;
            victim->pending_vel  = vel;
            return;
        }
    }

    SmpVoice* fv = findFree();
    if (fv) { startVoice(*fv, slot, vel); return; }

    // pool physically exhausted (all fading choke voices): queue on the first unqueued one
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state == V_FADEOUT && v.pending_slot < 0) { v.pending_slot = (int8_t)slot; v.pending_vel = vel; return; }
    }
    // otherwise: drop the note (all 8 voices already queued) — extremely rare, never clicks
}

// ---------------------------------------------------------------- AUDIO plane
void samplerFireStep(uint16_t mask) {
    for (uint8_t k = 0; k < SMP_SLOTS; k++)
        if ((mask >> k) & 1u) triggerVoice(k, 0.8f);
}

static void retrigSlot(uint8_t slot, float vel) {
    if (slot >= SMP_SLOTS || smp.len[slot] == 0) return;
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state == V_FREE || v.slot != (int8_t)slot || v.state == V_FADEOUT) continue;
        if (v.stut) startFadeOut(v, FADE_CHOKE_SMP);                         // the previous chop ends (5 ms), as a retrigger always did
        else { v.mute_to = 0.f; v.mute_step = 1.f / (float)FADE_CHOKE_SMP; } // the real voice keeps advancing, silenced: the shadow playhead
    }
    SmpVoice* fv = findFree();
    if (!fv) return;                                                         // pool exhausted: skip this chop, never steal the shadow
    startVoice(*fv, slot, vel); fv->stut = true;
}
void samplerRetrig(uint16_t mask) { for (uint8_t k = 0; k < SMP_SLOTS; k++) if ((mask >> k) & 1u) retrigSlot(k, 0.8f); }
static void stutterReleased() {
    for (int i = 0; i < SYN_MAX_POLY; i++) {
        SmpVoice& v = voices[i];
        if (v.state == V_FREE) continue;
        if (v.stut) { if (v.state != V_FADEOUT) startFadeOut(v, FADE_CHOKE_SMP); }
        else if (v.mute_to < 1.f) { v.mute_to = 1.f; v.mute_step = 1.f / (float)FADE_CHOKE_SMP; }   // the shadow comes back where it is
    }
}

// TASK 3: see sampler.h. Control-plane read of the ISR-maintained volatile counter.
int samplerActiveVoices(void) { return (int)s_active_voice_count; }

static int16_t SDRAM_BSS s_aud[SMP_LEN];
static volatile uint32_t s_aud_len = 0, s_aud_pos = 0;
static volatile bool s_aud_on = false;
int16_t* samplerAuditionBuf() { return s_aud; }
void samplerAuditionStop() { s_aud_on = false; }
void samplerAuditionPlay(uint32_t len) {
    if (len > SMP_LEN) len = SMP_LEN;
    s_aud_on = false; s_aud_len = len; s_aud_pos = 0;
    s_aud_on = len > 0;
}
bool samplerAuditionPlaying() { return s_aud_on; }

void samplerProcess(const float* inL, float* outL, float* outR, size_t n) {
    // REVERSE shadow (edited as a float enum) -> the real bool cfg, synced here
    for (int i = 0; i < SMP_SLOTS; i++)
        smp.slot[i].reverse = (s_reverse_shadow[i] > 0.5f);

    { bool st = fxStutterActive(); if (s_stut_prev && !st) stutterReleased(); s_stut_prev = st; }

    // manual-play pending mask, consumed lock-free
    uint16_t m = s_pending_mask;
    if (m) {
        s_pending_mask &= (uint16_t)~m;
        for (uint8_t k = 0; k < SMP_SLOTS; k++)
            if ((m >> k) & 1u) triggerVoice(k, s_pending_vel[k]);
    }

    // ---- pool audition (idle = one flag test)
    if (s_aud_on) {
        uint32_t p = s_aud_pos, len = s_aud_len;
        for (size_t i = 0; i < n; i++) {
            if (p >= len) { s_aud_on = false; break; }
            float x = (float)s_aud[p++] * (0.7f / 32768.f);
            outL[i] += x; outR[i] += x;
        }
        s_aud_pos = p;
    }

    // ---- recording: press-exact, starts on the very next block ----
    if (smp.rec_slot >= 0) {
        int8_t rs = smp.rec_slot;
        for (size_t i = 0; i < n; i++) {
            uint32_t p = smp.rec_pos;
            if (p >= SMP_LEN) { finishRecording(); break; }
            float x = inL[i];
            if (p < REC_FADE_SMP) x *= (float)p / (float)REC_FADE_SMP;
            smp_buf[rs][p] = flushBad(x);
            smp.rec_pos = p + 1u;
        }
    }

    // ---- voices ----
    for (int vi = 0; vi < SYN_MAX_POLY; vi++) {
        SmpVoice& v = voices[vi];
        if (v.state == V_FREE) continue;

        for (size_t i = 0; i < n; i++) {
            if (v.state == V_FREE) break;
            int8_t   slot = v.slot;
            uint32_t len  = smp.len[slot];

            float s;
            if (v.loop) {   // wrap the playhead(s) inside the region
                float reg = v.re - v.rs;
                if (v.plain) { if (v.pos >= v.re) v.pos -= reg; else if (v.pos < v.rs) v.pos += reg; }
                else { if (v.sched_pos >= v.re) v.sched_pos -= reg; else if (v.sched_pos < v.rs) v.sched_pos += reg;
                       for (int gI = 0; gI < 2; gI++) { if (v.g_read[gI] >= v.re) v.g_read[gI] -= reg; else if (v.g_read[gI] < v.rs) v.g_read[gI] += reg; } }
            }
            if (v.plain) {
                s = bufRead(slot, v.pos, len);
                v.pos += (float)v.dir * v.rate;
            } else {
                v.sched_pos += (float)v.dir * v.formant_ratio;
                float acc = 0.f;
                for (int gI = 0; gI < 2; gI++) {
                    if (v.g_age[gI] >= (float)GRAIN_LEN) {
                        v.g_read[gI] = v.sched_pos;
                        v.g_age[gI] -= (float)GRAIN_LEN;
                    }
                    uint32_t idx = (uint32_t)v.g_age[gI];
                    if (idx >= GRAIN_LEN) idx = GRAIN_LEN - 1u;
                    acc += bufRead(slot, v.g_read[gI], len) * s_hann[idx];
                    v.g_read[gI] += (float)v.dir * v.rate;
                    v.g_age[gI]  += 1.f;
                }
                s = acc;
            }

            if (v.filt_on) { v.filt_y += (s - v.filt_y) * v.filt_a; s = flushDenorm(v.filt_y); }
            s = flushBad(s);

            switch (v.state) {
                case V_FADEIN:
                    v.amp += v.amp_step;
                    if (v.amp >= 1.f) { v.amp = 1.f; v.state = V_SUSTAIN; }
                    break;
                case V_FADEOUT:
                    v.amp -= v.amp_step;
                    if (v.amp <= 0.f) {
                        v.amp = 0.f; v.state = V_FREE;
                        if (s_active_voice_count) s_active_voice_count--;   // TASK 3: active -> FREE
                        if (v.pending_slot >= 0) {
                            uint8_t ps = (uint8_t)v.pending_slot; float pv = v.pending_vel;
                            v.pending_slot = -1;
                            startVoice(v, ps, pv);
                        }
                    }
                    break;
                default: break;   // V_SUSTAIN
            }

            if (v.mute != v.mute_to) { float d = v.mute_to - v.mute; v.mute += d > v.mute_step ? v.mute_step : d < -v.mute_step ? -v.mute_step : d; }
            float outv = s * v.amp * v.gain * v.mute;
            outL[i] += outv * (1.f - v.pan);
            outR[i] += outv * v.pan;

            if (v.state == V_SUSTAIN && v.out_left > 0u) {
                v.out_left--;
                if (v.out_left == 0u) startFadeOut(v, v.rel_smp);
            }
        }
    }
}

// ---------------------------------------------------------------- editor
const ParamPage* samplerSlotPage(uint8_t slot) {
    if (slot >= SMP_SLOTS) return nullptr;
    return &s_slotPages[slot];
}
void samplerExtReset() { for (int i = 0; i < SMP_SLOTS; i++) smp_ext[i] = SmpSlotExt{ 0.f, 1.f, 0.f, SMP_ATK_DEF_MS, SMP_REL_DEF_MS }; }   // a patch load / new patch: the live extras go back to the defaults
const ParamPage* samplerExtPage(uint8_t slot) { return slot < SMP_SLOTS ? &s_extPages[slot] : nullptr; }
const ParamPage* samplerMixPage() { return &s_mixPageObj; }
