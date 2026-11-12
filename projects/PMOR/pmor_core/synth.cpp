// synth.cpp — polysynth engine: voice allocation/stealing, generative pan,
// performance macros, amp/filter/pitch envelopes, per-voice Chamberlin SVF,
// mixdown, and the editor's Param pages. Algorithm oscillator code lives in
// synth_algos.cpp (Voice model + character rolls in synth_private.h).
#include "synth_private.h"
#include "sequencer.h"
#include "fx.h"
#include "shared_state.h"
#include "scale.h"
#include "clock.h"
#include "events.h"
#include <string.h>

SynthState syn;
Voice      synVoices[SYN_VOICES];

extern const char* const SYN_ALGO_NAMES[ALG_COUNT] = {
    "THIEF", "FM", "WAVE", "PLUCK", "SUPER", "CZ", "FOLD", "ADD"
};

static uint32_t synRng          = 0xC0FFEE17u;
static uint32_t synNoteCounter  = 0;
static uint32_t synSlotCounter  = 0;
void synthReseed(uint32_t s) { synRng = 0xC0FFEE17u; if (s) synRng ^= s * 2654435761u; else { synNoteCounter = 0; synSlotCounter = 0; } }   // seed 0 = the full deterministic reset (parity renders in one process); a nonzero live reseed leaves voice bookkeeping alone
static OnePole  synPerfOctSmoo;   // block-rate smoothing of perf_octave, ~120ms

float synAlgoShadow = 0.f;        // ALGO param page's float shadow of syn.algo (PF_ENUM needs a float*)
static void synBuildPages();      // builds the 8x4 static ParamPage sets; forward-declared, defined below

// ---------------------------------------------------------------- helpers

// perf_env_macro (0..1): 0 = plucky (times x0.06, sustain forced to 0 so the note dies while
// held), 0.5 = the preset as authored, 1 = ambient. On the ambient side the RELEASE stretches
// (up to x20 -> ~3.5 s tails) while the attack only opens a little (x3) — a long note must hit
// as loud as a short one, so the attack never gets slow enough to miss a sequenced gate
// (EMGOR 2026-09-15: "long notes were way quieter than short ones").
// AMBIENT side (EMGOR 2026-09-15, final): at macro = 1 every note becomes a swell — attack
// fades IN over SYN_AMB_ATK_S, the sequencer HOLDS the note until the swell has fully arrived,
// then it fades OUT over SYN_AMB_REL_S. In between the macro interpolates log-wise from the
// note's own times to these targets. Plucky side (m < 0.5) unchanged.
#define SYN_AMB_ATK_S  1.5f
#define SYN_AMB_REL_S  3.5f
#define SYN_AMB_ATK_COEF 0.00004f   // Thief one-pole attack coef at m = 1 (tau ~0.5 s -> ~1.5 s swell)
#define SYN_AMB_REL_COEF 0.00006f   // Thief one-pole release coef at m = 1 (tau ~0.35 s -> ~3.5 s tail)
static float s_seqAtkCap = 0.f;                 // seconds; 0 = none. Set around synTriggerNote (sequenced notes).
static inline float synAmb(float m) { return (m > 0.5f) ? (m - 0.5f) * 2.f : 0.f; }
static inline float synPluck(float m) { return (m <= 0.5f) ? lerpf(0.06f, 1.f, m * 2.f) : 1.f; }
// log interpolation own -> target by amb (0..1); works for coefficients and for seconds
static inline float synTowards(float own, float target, float amb) {
    if (own <= 0.f) own = 1e-6f;
    return own * fastPow2(amb * (logf(target / own) * 1.4426950f));
}
static Adsr synScaledAmpEnv(const Adsr& base) {
    float m = clampf(syn.perf_env_macro, 0.f, 1.f), amb = synAmb(m), pk = synPluck(m);
    Adsr out = base;
    out.s = (m <= 0.5f) ? lerpf(0.f, base.s, m * 2.f) : base.s;
    float a = base.a * pk, d = base.d * pk, r = base.r * pk;
    if (amb > 0.f) {
        if (a < SYN_AMB_ATK_S) a = synTowards(a, SYN_AMB_ATK_S, amb);
        if (r < SYN_AMB_REL_S) r = synTowards(r, SYN_AMB_REL_S, amb);
        out.s = lerpf(base.s, 1.f, amb);                  // a swell sits at full level while held
    }
    if (s_seqAtkCap > 0.f && a > s_seqAtkCap) a = s_seqAtkCap;   // sequenced: attack budget
    out.a = a; out.d = d; out.r = r;
    return out;
}
// ---- sequenced / arp notes and the swell: THE ENVELOPE ADAPTS TO THE SEQUENCING, never the
// reverse (EMGOR 2026-09-15: the tilt must not change what the arp/generator plays). A sequenced
// note may live at most about `poly x grid interval` before the pool needs its voice, so its
// swell attack is budgeted to ~45 % of that and it is HELD for 1.3 x that attack — it always
// arrives at full level and nothing piles up. Hand-played notes are unbudgeted (hold as long as
// you like). If the pool is full anyway, the oldest note is stolen with a 120 ms fade (smooth).
static inline float synSeqAttackSeconds() {     // the swell attack a sequenced note gets right now
    float amb = synAmb(clampf(syn.perf_env_macro, 0.f, 1.f));
    if (amb <= 0.f) return 0.f;
    float atk = synTowards(0.02f, SYN_AMB_ATK_S, amb);
    const LaneSet& l = seq.lane[ENG_SYNTH];
    int poly = (int)clampf(l.polyphony, 1.f, (float)SYN_MAX_POLY);
    float rate = (l.arp_rate >= 2.f) ? l.arp_rate : 2.f;
    float budget = clk.step_samples * rate * (float)poly * 0.45f / SR;
    if (atk > budget) atk = budget;
    if (atk < 0.02f) atk = 0.02f;
    return atk;
}

// poly==2 -> 0.25/0.75, poly==3 -> 0.15/0.50/0.85 by slot; poly==1 or >=4 ->
// generative width around a centre (>=4 also spreads the centres evenly).
static void synComputePan(Voice& v, int poly, int slotIdx, float width, float widthProb) {
    width = clampf(width, 0.f, 1.f);
    widthProb = clampf(widthProb, 0.f, 1.f);
    if (poly == 2) { v.pan = (slotIdx == 0) ? 0.25f : 0.75f; return; }
    if (poly == 3) { v.pan = (slotIdx == 0) ? 0.15f : ((slotIdx == 1) ? 0.50f : 0.85f); return; }
    float base = 0.5f;
    if (poly >= 4) {
        int denom = poly - 1; if (denom < 1) denom = 1;
        base = 0.5f - width * 0.5f + width * ((float)slotIdx / (float)denom);
    }
    if (lcg01(synRng) < widthProb) {
        float r = (lcg01(synRng) * 2.f - 1.f) * (width * 0.5f);
        v.pan = clampf(base + r, 0.f, 1.f);
    } else {
        v.pan = base;
    }
}

// finds a physical voice slot for a new note, stealing the oldest live note
// (fast ~20ms release, never a hard cut) if the polyphony cap is already met.
static inline void synEmitOff(const Voice& v);
static int synAllocVoice(int poly) {
    int activeCount = 0;
    for (int i = 0; i < SYN_VOICES; i++)
        if (synVoices[i].ampEnv.stage != ADSR::IDLE && !synVoices[i].stolen) activeCount++;

    if (activeCount >= poly) {
        int oldest = -1; uint32_t oldestId = 0xFFFFFFFFu;
        for (int i = 0; i < SYN_VOICES; i++) {
            Voice& vv = synVoices[i];
            if (vv.ampEnv.stage != ADSR::IDLE && !vv.stolen && vv.note_id < oldestId) {
                oldestId = vv.note_id; oldest = i;
            }
        }
        if (oldest >= 0) {
            synVoices[oldest].stolen = true;
            synEmitOff(synVoices[oldest]);                       // MIDI: the stolen note's off (it used to stay on until the same pitch was played again)
            float stealR = 1.f - fastExp(-3.f / (0.12f * SR));   // ~120 ms release: a fade, not a click
            synVoices[oldest].ampEnv.r_coef = stealR;
            synVoices[oldest].ampEnv.stage = ADSR::REL;          // already non-idle (see loop above)
            if (synVoices[oldest].algo == ALG_THIEF) { synVoices[oldest].apart[4] = 0.f; synVoices[oldest].apart[5] = stealR; }
        }
    }

    for (int i = 0; i < SYN_VOICES; i++)
        if (synVoices[i].ampEnv.stage == ADSR::IDLE) return i;

    // pool exhausted (all spare slots are releasing): take the QUIETEST releasing voice — the one
    // closest to silence — so the cut is as inaudible as it can be; never a loud voice.
    int pick = -1; float lvl = 1e9f;
    for (int i = 0; i < SYN_VOICES; i++) {
        const Voice& vv = synVoices[i];
        float l = (vv.algo == ALG_THIEF) ? vv.apart[3] : vv.ampEnv.level;
        if (vv.stolen && l < lvl) { lvl = l; pick = i; }
    }
    if (pick >= 0) return pick;
    int oldest = 0; uint32_t oldestId = 0xFFFFFFFFu;
    for (int i = 0; i < SYN_VOICES; i++)
        if (synVoices[i].note_id < oldestId) { oldestId = synVoices[i].note_id; oldest = i; }
    return oldest;
}

static inline void synEmitOff(const Voice& v) { uint8_t mn; uint16_t bd; pmorHzToMidi(v.note_hz, &mn, &bd); pmorEventPush(EV_SYN_OFF, mn, 0, 0); }
static void synTriggerNote(int degree, int octave, float vel, uint8_t pad, int32_t gate_samples) {
    if (octave < 0) octave = 0;
    if (octave > SYN_OCTAVES - 1) octave = SYN_OCTAVES - 1;
    float hz = scaleHz(degree, octave);
    uint8_t algo = syn.algo;
    const SynPreset& pr = syn.preset[algo];

    int poly = (int)clampf(seq.lane[ENG_SYNTH].polyphony, 1.f, (float)SYN_MAX_POLY);
    int slot = synAllocVoice(poly);   // a stolen voice emits its note-off here, BEFORE this note's note-on below
    { uint8_t mn; uint16_t bd; pmorHzToMidi(hz, &mn, &bd); int v127 = (int)(vel * 127.f + 0.5f); pmorEventPush(EV_SYN_ON, mn, (uint8_t)(v127 < 1 ? 1 : v127 > 127 ? 127 : v127), bd); }
    Voice& v = synVoices[slot];

    // reset per-voice scratch but keep the SVF state clamp-safe (fresh note = fresh filter).
    v = Voice();
    v.algo = algo;
    v.pad = pad;
    v.note_hz = hz;
    v.note_id = ++synNoteCounter;
    v.gate_samples = gate_samples;
    v.atk_min_coef = (s_seqAtkCap > 0.f) ? 3.f / (s_seqAtkCap * SR) : 0.f;   // Thief: 95 % within the budget
    (void)vel;   // velocity is reserved for a future amp-scale hook; amp uses the preset ADSR peak

    int slotIdx = (int)(synSlotCounter++ % (uint32_t)((poly < 1) ? 1 : poly));
    synComputePan(v, poly, slotIdx, pr.width, pr.width_prob);

    Adsr amp = synScaledAmpEnv(pr.amp);
    v.ampEnv.setTimes(amp.a, amp.d, amp.s, amp.r);
    v.ampEnv.gate(true);
    v.filtEnv.setTimes(pr.filt.a, pr.filt.d, pr.filt.s, pr.filt.r);
    v.filtEnv.gate(true);

    synAlgoNoteOn(v, pr, synRng);   // may set v.pitchGlideEnable (THIEF re-roll)

    // PITCH ENV: start `pitch_amt` semitones away and glide INTO the note over glide_in.
    // pitch_amt == 0 -> no glide at all (the default).
    float amtMul = fastPow2(clampf(pr.pitch_amt, -24.f, 24.f) * (1.f / 12.f));
    if (pr.glide_in > 1e-4f && v.pitchGlideEnable && fabsf(pr.pitch_amt) > 0.01f) {
        v.pitch_mul = amtMul; v.pitch_target = 1.f;
        v.pitch_rate = fabsf(1.f - amtMul) / (pr.glide_in * SR);
    } else {
        v.pitch_mul = 1.f; v.pitch_target = 1.f; v.pitch_rate = 0.f;
    }
}

// ---------------------------------------------------------------- lifecycle
static SynPreset s_initPreset[ALG_COUNT];   // boot-default sound per algorithm (preset 0)
SynLfo synLfo = { 3.f, 0.f, 0.f, 0.f };
static float s_lfoPhase = 0.f, s_lfoSh = 0.f; static uint32_t s_lfoRng = 0x2545F491u;
void synLfoReset() { synLfo = { 3.f, 0.f, 0.f, 0.f }; s_lfoPhase = 0.f; s_lfoSh = 0.f; }
void synthInit() {
    memset(&syn, 0, sizeof(syn));
    synLfoReset();
    syn.volume = 1.f;
    syn.perf_env_macro = 0.5f;
    syn.perf_cutoff = 1.f;
    syn.master_prob = 0.5f;
    syn.algo = ALG_THIEF;

    for (int i = 0; i < SYN_VOICES; i++) synVoices[i] = Voice();
    memset(synKpBuf, 0, sizeof(synKpBuf));   // SDRAM is not zeroed at reset
    synBuildWavetables();
    synPerfOctSmoo.setMs(80.f, BLOCK_HZ);
    synPerfOctSmoo.set(0.f);

    // default presets — tuned so every algorithm sounds good immediately.
    SynPreset& thief = syn.preset[ALG_THIEF];
    thief.p[0] = 0.5f; thief.p[1] = 0.15f; thief.p[2] = 0.15f; thief.p[3] = 0.25f;
    thief.amp  = {0.010f, 0.60f, 0.70f, 0.90f};
    thief.cutoff = 2200.f; thief.resonance = 0.25f;
    thief.filt = {0.010f, 0.40f, 0.50f, 0.60f}; thief.filt_amt = 0.35f;
    thief.glide_in = 0.f; thief.glide_out = 0.f; thief.pitch_amt = 0.f;
    thief.width = 0.5f; thief.width_prob = 0.30f;

    SynPreset& fm = syn.preset[ALG_FM];
    fm.p[0] = 2.f; fm.p[1] = 0.5f; fm.p[2] = 0.25f; fm.p[3] = 0.15f;
    fm.amp = {0.005f, 0.30f, 0.40f, 0.50f};
    fm.cutoff = 4500.f; fm.resonance = 0.20f;
    fm.filt = {0.005f, 0.25f, 0.30f, 0.40f}; fm.filt_amt = 0.20f;
    fm.glide_in = 0.f; fm.glide_out = 0.f; fm.pitch_amt = 0.f;
    fm.width = 0.4f; fm.width_prob = 0.20f;

    SynPreset& wave = syn.preset[ALG_WAVE];
    wave.p[0] = 3.f; wave.p[1] = 0.3f; wave.p[2] = 0.3f; wave.p[3] = 0.4f;
    wave.amp = {0.020f, 0.50f, 0.60f, 0.80f};
    wave.cutoff = 3200.f; wave.resonance = 0.30f;
    wave.filt = {0.020f, 0.40f, 0.50f, 0.60f}; wave.filt_amt = 0.30f;
    wave.glide_in = 0.f; wave.glide_out = 0.f; wave.pitch_amt = 0.f;
    wave.width = 0.5f; wave.width_prob = 0.30f;

    SynPreset& pluck = syn.preset[ALG_PLUCK];
    pluck.p[0] = 0.4f; pluck.p[1] = 0.7f; pluck.p[2] = 0.3f; pluck.p[3] = 0.5f;
    pluck.amp = {0.001f, 0.30f, 0.0f, 0.20f};
    pluck.cutoff = 6500.f; pluck.resonance = 0.10f;
    pluck.filt = {0.001f, 0.20f, 0.0f, 0.15f}; pluck.filt_amt = 0.10f;
    pluck.glide_in = 0.f; pluck.glide_out = 0.f; pluck.pitch_amt = 0.f;
    pluck.width = 0.5f; pluck.width_prob = 0.40f;

    SynPreset& super = syn.preset[ALG_SUPER];
    super.p[0] = 0.5f; super.p[1] = 0.5f; super.p[2] = 0.6f; super.p[3] = 0.2f;
    super.amp = {0.010f, 0.40f, 0.80f, 0.60f};
    super.cutoff = 5000.f; super.resonance = 0.20f;
    super.filt = {0.010f, 0.30f, 0.60f, 0.40f}; super.filt_amt = 0.15f;
    super.glide_in = 0.f; super.glide_out = 0.f; super.pitch_amt = 0.f;
    super.width = 0.7f; super.width_prob = 0.50f;

    SynPreset& cz = syn.preset[ALG_CZ];
    cz.p[0] = 0.4f; cz.p[1] = 0.3f; cz.p[2] = 0.3f; cz.p[3] = 0.3f;
    cz.amp = {0.005f, 0.35f, 0.50f, 0.40f};
    cz.cutoff = 8000.f; cz.resonance = 0.15f;
    cz.filt = {0.005f, 0.30f, 0.40f, 0.30f}; cz.filt_amt = 0.20f;
    cz.glide_in = 0.f; cz.glide_out = 0.f; cz.pitch_amt = 0.f;
    cz.width = 0.4f; cz.width_prob = 0.25f;

    SynPreset& fold = syn.preset[ALG_FOLD];
    fold.p[0] = 0.4f; fold.p[1] = 0.5f; fold.p[2] = 0.3f; fold.p[3] = 0.5f;
    fold.amp = {0.020f, 0.50f, 0.70f, 0.70f};
    fold.cutoff = 7000.f; fold.resonance = 0.10f;
    fold.filt = {0.020f, 0.40f, 0.60f, 0.50f}; fold.filt_amt = 0.15f;
    fold.glide_in = 0.f; fold.glide_out = 0.f; fold.pitch_amt = 0.f;
    fold.width = 0.5f; fold.width_prob = 0.30f;

    SynPreset& add = syn.preset[ALG_ADD];
    add.p[0] = 0.f; add.p[1] = 0.5f; add.p[2] = 0.4f; add.p[3] = 0.1f;
    add.amp = {0.030f, 0.60f, 0.70f, 1.0f};
    add.cutoff = 9500.f; add.resonance = 0.05f;
    add.filt = {0.030f, 0.50f, 0.60f, 0.80f}; add.filt_amt = 0.10f;
    add.glide_in = 0.f; add.glide_out = 0.f; add.pitch_amt = 0.f;
    add.width = 0.6f; add.width_prob = 0.35f;

    for (int a = 0; a < ALG_COUNT; a++) s_initPreset[a] = syn.preset[a];   // remembered for preset 0 (INIT)
    synBuildPages();
}

extern const char* const SYN_PRESET_NAMES[SYN_PRESETS] = { "INIT", "PLUCK", "PAD", "LEAD", "BASS" };

// Per-algorithm factory sounds, tuned from the meaning of each algorithm's own four knobs (see THIEF_NAMES.. below) and measured
// by tests/test_engines.cpp (attack time, sustain, release, brightness). Row = {p[4], amp ADSR, cutoff, reso, filt ADSR, filt_amt, width, width_prob}.
// THIEF is the exception: it is a random-per-note patch whose preset ADSR/filter/knobs are ignored by design (synth_algos.cpp),
// so its four styles can only change stereo width.
struct SynStyle { float p[4]; Adsr amp; float cutoff, reso; Adsr filt; float filt_amt, width, width_prob; };
static const SynStyle kStyle[ALG_COUNT][SYN_PRESETS - 1] = {
 /* THIEF */ { { {0.5f,0.15f,0.15f,0.25f}, {0.01f,0.6f,0.7f,0.9f},  2200.f,0.25f, {0.01f,0.4f,0.5f,0.6f},  0.35f, 0.15f,0.10f },
              { {0.5f,0.15f,0.15f,0.25f}, {0.01f,0.6f,0.7f,0.9f},  2200.f,0.25f, {0.01f,0.4f,0.5f,0.6f},  0.35f, 1.00f,0.80f },
              { {0.5f,0.15f,0.15f,0.25f}, {0.01f,0.6f,0.7f,0.9f},  2200.f,0.25f, {0.01f,0.4f,0.5f,0.6f},  0.35f, 0.40f,0.30f },
              { {0.5f,0.15f,0.15f,0.25f}, {0.01f,0.6f,0.7f,0.9f},  2200.f,0.25f, {0.01f,0.4f,0.5f,0.6f},  0.35f, 0.00f,0.00f } },
 /* FM    */ { { {2.f,0.6f,0.12f,0.0f},   {0.002f,0.35f,0.f,0.25f}, 1500.f,0.10f, {0.002f,0.2f,0.f,0.15f}, 0.08f, 0.4f,0.2f },
              { {1.f,0.25f,2.5f,0.05f},   {0.7f,1.2f,0.8f,1.8f},    4500.f,0.15f, {0.8f,1.0f,0.6f,1.5f},   0.20f, 0.9f,0.6f },
              { {2.f,0.55f,1.5f,0.35f},   {0.01f,0.3f,0.85f,0.25f}, 2500.f,0.25f, {0.01f,0.3f,0.7f,0.25f}, 0.04f, 0.3f,0.1f },
              { {1.f,0.4f,0.3f,0.1f},     {0.003f,0.25f,0.7f,0.12f},1200.f,0.20f, {0.002f,0.2f,0.3f,0.12f},0.50f, 0.1f,0.0f } },
 /* WAVE  */ { { {2.f,0.2f,0.1f,0.7f},    {0.002f,0.35f,0.f,0.25f}, 1500.f,0.10f, {0.002f,0.2f,0.f,0.15f}, 0.08f, 0.4f,0.2f },
              { {3.f,0.4f,0.6f,0.2f},     {0.7f,1.2f,0.8f,1.8f},    4000.f,0.15f, {0.8f,1.0f,0.6f,1.5f},   0.20f, 0.9f,0.6f },
              { {7.f,0.3f,0.25f,0.f},    {0.01f,0.3f,0.85f,0.25f}, 4500.f,0.30f, {0.01f,0.3f,0.7f,0.25f}, 0.25f, 0.3f,0.1f },
              { {6.f,0.2f,0.05f,0.f},    {0.003f,0.25f,0.85f,0.15f},1400.f,0.25f, {0.002f,0.2f,0.7f,0.12f},0.f, 0.1f,0.0f } },
 /* PLUCK */ { { {0.3f,0.8f,0.25f,0.45f},  {0.001f,1.5f,0.f,0.30f},  1500.f,0.10f, {0.001f,0.3f,0.f,0.2f},  0.06f, 0.4f,0.2f },
              { {0.6f,0.3f,0.5f,0.97f},   {0.004f,2.5f,0.6f,2.0f},  3500.f,0.15f, {0.004f,1.5f,0.5f,1.5f}, 0.10f, 0.9f,0.6f },
              { {0.15f,0.9f,0.1f,0.92f},  {0.002f,1.0f,0.7f,0.4f},  4500.f,0.25f, {0.002f,0.5f,0.6f,0.3f}, 0.10f, 0.3f,0.1f },
              { {0.55f,0.25f,0.4f,0.6f},  {0.002f,0.6f,0.3f,0.2f},  900.f,0.20f,  {0.002f,0.3f,0.2f,0.2f}, 0.30f, 0.1f,0.0f } },
 /* SUPER */ { { {0.3f,0.4f,0.8f,0.1f},    {0.002f,0.3f,0.f,0.2f},   7000.f,0.20f, {0.001f,0.2f,0.f,0.15f}, 0.60f, 0.5f,0.3f },
              { {0.7f,0.8f,1.0f,0.2f},    {0.7f,1.2f,0.8f,1.8f},    3500.f,0.15f, {0.8f,1.0f,0.6f,1.5f},   0.20f, 0.95f,0.7f },
              { {0.5f,0.5f,0.9f,0.1f},    {0.01f,0.3f,0.85f,0.25f}, 5000.f,0.30f, {0.01f,0.3f,0.7f,0.25f}, 0.25f, 0.5f,0.3f },
              { {0.1f,0.2f,0.2f,1.0f},    {0.003f,0.3f,0.7f,0.12f}, 700.f,0.25f,  {0.002f,0.2f,0.3f,0.12f},0.40f, 0.1f,0.0f } },
 /* CZ    */ { { {0.5f,0.5f,0.8f,0.6f},    {0.002f,0.3f,0.f,0.2f},   9000.f,0.15f, {0.001f,0.2f,0.f,0.15f}, 0.30f, 0.4f,0.2f },
              { {0.2f,0.2f,0.2f,0.2f},    {0.7f,1.2f,0.8f,1.8f},    5000.f,0.15f, {0.8f,1.0f,0.6f,1.5f},   0.15f, 0.9f,0.6f },
              { {0.6f,0.6f,0.0f,0.3f},    {0.01f,0.3f,0.85f,0.25f}, 3500.f,0.25f,{0.01f,0.3f,0.7f,0.25f}, 0.10f, 0.3f,0.1f },
              { {0.3f,0.2f,0.5f,0.8f},    {0.003f,0.25f,0.7f,0.12f},900.f,0.25f,  {0.002f,0.2f,0.3f,0.12f},0.45f, 0.1f,0.0f } },
 /* FOLD  */ { { {0.3f,0.5f,0.9f,0.4f},    {0.002f,0.3f,0.f,0.2f},   4000.f,0.15f, {0.001f,0.2f,0.f,0.15f}, 0.15f, 0.4f,0.2f },
              { {0.15f,0.5f,0.1f,0.2f},   {0.7f,1.2f,0.8f,1.8f},    5000.f,0.15f, {0.8f,1.0f,0.6f,1.5f},   0.15f, 0.9f,0.6f },
              { {0.5f,0.5f,0.3f,0.6f},    {0.01f,0.3f,0.85f,0.25f}, 5000.f,0.30f, {0.01f,0.3f,0.7f,0.25f}, 0.20f, 0.3f,0.1f },
              { {0.2f,0.5f,0.4f,0.3f},    {0.003f,0.25f,0.7f,0.12f},1000.f,0.25f, {0.002f,0.2f,0.3f,0.12f},0.40f, 0.1f,0.0f } },
 /* ADD   */ { { {0.3f,0.5f,0.9f,0.0f},    {0.002f,0.4f,0.f,0.25f},  1500.f,0.10f,{0.001f,0.2f,0.f,0.15f}, 0.08f, 0.4f,0.2f },
              { {-0.3f,0.5f,0.1f,0.5f},   {0.8f,1.2f,0.8f,2.0f},    2500.f,0.10f, {0.8f,1.0f,0.6f,1.5f},   0.05f, 0.9f,0.6f },
              { {0.5f,0.3f,0.2f,0.1f},    {0.01f,0.3f,0.85f,0.25f}, 5000.f,0.15f,{0.01f,0.3f,0.7f,0.25f}, 0.15f, 0.3f,0.1f },
              { {-0.8f,0.8f,0.3f,0.0f},   {0.003f,0.25f,0.7f,0.12f},1500.f,0.15f, {0.002f,0.2f,0.3f,0.12f},0.30f, 0.1f,0.0f } },
};

static SynPreset SDRAM_BSS s_filePreset[ALG_COUNT][SYN_FILE_PRESETS];   // SDRAM: only meaningful where s_fileOn is set
static bool      s_fileOn[ALG_COUNT][SYN_FILE_PRESETS];
static char SDRAM_BSS s_fileName[ALG_COUNT][SYN_FILE_PRESETS][10];   // SDRAM, valid where s_fileOn
void synFileSet(uint8_t a, uint8_t i, const SynPreset& p, const char* name) {
    if (a >= ALG_COUNT || i >= SYN_FILE_PRESETS) return;
    s_filePreset[a][i] = p; strncpy(s_fileName[a][i], name ? name : "", 9); s_fileName[a][i][9] = 0; __sync_synchronize(); s_fileOn[a][i] = true;
}
const char* synFileName(uint8_t a, uint8_t i) { return (a < ALG_COUNT && i < SYN_FILE_PRESETS && s_fileOn[a][i]) ? s_fileName[a][i] : nullptr; }
int synFileNextFree(uint8_t a) { if (a >= ALG_COUNT) return -1; for (int i = SYN_PRESETS; i < SYN_FILE_PRESETS; i++) if (!s_fileOn[a][i]) return i; return -1; }
bool synFileGet(uint8_t a, uint8_t i, SynPreset& o) { if (a >= ALG_COUNT || i >= SYN_FILE_PRESETS || !s_fileOn[a][i]) return false; o = s_filePreset[a][i]; return true; }

bool synFactoryPreset(uint8_t algo, uint8_t idx, SynPreset& out) {
    if (algo >= ALG_COUNT || idx >= SYN_PRESETS) return false;
    if (idx == 0) { out = s_initPreset[algo]; return true; }
    const SynStyle& t = kStyle[algo][idx - 1];
    SynPreset q = s_initPreset[algo];
    for (int i = 0; i < 4; i++) q.p[i] = t.p[i];
    q.amp = t.amp; q.cutoff = t.cutoff; q.resonance = t.reso; q.filt = t.filt; q.filt_amt = t.filt_amt;
    q.glide_in = q.glide_out = 0.f; q.pitch_amt = 0.f; q.width = t.width; q.width_prob = t.width_prob;
    out = q; return true;
}
void synLoadPreset(uint8_t algo, uint8_t idx) {
    if (algo >= ALG_COUNT || idx >= SYN_FILE_PRESETS) return;
    if (s_fileOn[algo][idx]) { syn.preset[algo] = s_filePreset[algo][idx]; return; }   // SD file wins; firmware tables are the fallback
    SynPreset q; if (synFactoryPreset(algo, idx, q)) syn.preset[algo] = q;
}

// ---------------------------------------------------------------- last-note memory (stutter)
// A copy of each pad's most recent voice right after note-on: pitch, pan, and the whole
// per-note character (Thief's random type/filter/envelope, FM ratios, ...). A stutter re-fires
// THIS, never a new roll of the dice — so it is the same note repeating (EMGOR 2026-09-15).
static Voice   s_lastNote[N_PADS];
static bool    s_lastValid[N_PADS];
static float   s_lastEnvTarget[N_PADS];     // Thief apart[4] as set at note-on

static void synRemember(uint8_t pad, const Voice& v) {
    if (pad >= N_PADS) return;
    s_lastNote[pad] = v;
    s_lastValid[pad] = true;
    s_lastEnvTarget[pad] = v.apart[4];
}

static inline bool synTapGuard(uint8_t pad) {   // true = swallow this manual hit (a sequenced one just sounded)
    uint32_t now = clockSampleNow();
    uint32_t guard = (uint32_t)(clk.step_samples * TAP_GUARD_STEPS);
    uint32_t last = g.seq_hit_at[ENG_SYNTH][pad & 15];
    bool swallow = (last != 0u) && (now - last) < guard;
    g.tap_at[ENG_SYNTH][pad & 15] = now ? now : 1u;
    return swallow;
}

// ---------------------------------------------------------------- CONTROL plane
void synthNoteOn(uint8_t pad, float vel) {
    fxNoteLive(ENG_SYNTH, pad);                  // v2 live latch: stutters have material
    if (synTapGuard(pad)) return;                // the sequencer just played this note: one note, not two
    int degree, octave;
    scalePadToNote(pad, degree, octave);
    synTriggerNote(degree, octave, vel, pad, -1);
    for (int i = 0; i < SYN_VOICES; i++)
        if (synVoices[i].pad == pad && synVoices[i].note_id == synNoteCounter) { synRemember(pad, synVoices[i]); break; }
}

void synthNoteOff(uint8_t pad) {
    for (int i = 0; i < SYN_VOICES; i++) {
        Voice& v = synVoices[i];
        if (v.pad == pad && v.gate_samples < 0 && v.ampEnv.stage != ADSR::IDLE && !v.stolen) {
            v.ampEnv.release(); synEmitOff(v);
            if (v.algo == ALG_THIEF) v.apart[4] = 0.f;                      // V2: envTarget = 0
            const SynPreset& pr = syn.preset[v.algo];
            if (pr.glide_out > 1e-4f && fabsf(pr.pitch_amt) > 0.01f) {   // glide OUT of the note to +-amt
                float amtMul = fastPow2(clampf(pr.pitch_amt, -24.f, 24.f) * (1.f / 12.f));
                v.pitch_target = amtMul;
                v.pitch_rate = fabsf(1.f - amtMul) / (pr.glide_out * SR);
            }
        }
    }
}

void synthAllOff() {
    float stealR = 1.f - fastExp(-3.f / (0.02f * SR));
    for (int i = 0; i < SYN_VOICES; i++) {
        Voice& v = synVoices[i];
        if (v.ampEnv.stage != ADSR::IDLE) {
            synEmitOff(v);                                       // MIDI: every sounding note ends (transport stop)
            v.ampEnv.r_coef = stealR;
            v.ampEnv.stage = ADSR::REL;
            if (v.algo == ALG_THIEF) { v.apart[4] = 0.f; v.apart[5] = stealR; }
        }
        v.gate_samples = -1;
    }
}

// ---------------------------------------------------------------- AUDIO plane
void synthFireStep(uint16_t mask, uint32_t gstep) {
    (void)gstep;
    if (!mask) return;
    int offset = seqPickOctaveOffset(seq.lane[ENG_SYNTH]);
    float   atkS = synSeqAttackSeconds();
    int32_t gate = (int32_t)(clk.step_samples * 0.9f);
    int32_t hold = (int32_t)(atkS * 1.3f * SR); if (hold > gate) gate = hold;   // held until the swell arrives
    if (gate < 1) gate = 1;
    s_seqAtkCap = atkS;
    for (int pad = 0; pad < N_PADS; pad++) {
        if (!((mask >> pad) & 1u)) continue;
        int degree, octave;
        scalePadToNote((uint8_t)pad, degree, octave);
        octave += offset;
        if (octave < 0) octave = 0;
        if (octave > SYN_OCTAVES - 1) octave = SYN_OCTAVES - 1;
        synTriggerNote(degree, octave, 0.8f, 0xFF, gate);
        for (int i = 0; i < SYN_VOICES; i++)
            if (synVoices[i].note_id == synNoteCounter) { synRemember((uint8_t)pad, synVoices[i]); break; }
    }
    s_seqAtkCap = 0.f;
}

void synthRetrigLast(uint16_t mask) {
    if (!mask) return;
    int poly = (int)clampf(seq.lane[ENG_SYNTH].polyphony, 1.f, (float)SYN_MAX_POLY);
    float   atkS = synSeqAttackSeconds();
    int32_t gate = (int32_t)(clk.step_samples * 0.9f);
    int32_t hold = (int32_t)(atkS * 1.3f * SR); if (hold > gate) gate = hold;
    if (gate < 1) gate = 1;
    for (int pad = 0; pad < N_PADS; pad++) {
        if (!((mask >> pad) & 1u) || !s_lastValid[pad]) continue;
        int slot = synAllocVoice(poly);
        Voice& v = synVoices[slot];
        v = s_lastNote[pad];                      // same pitch, pan, algorithm and per-note character
        v.stolen = false;
        v.note_id = ++synNoteCounter;
        v.pad = 0xFF;
        v.gate_samples = gate;
        v.atk_min_coef = (atkS > 0.f) ? 3.f / (atkS * SR) : 0.f;
        for (int k = 0; k < 5; k++) v.phase[k] = 0.f;
        v.svf_lp = v.svf_bp = 0.f;
        v.pitch_mul = 1.f; v.pitch_target = 1.f; v.pitch_rate = 0.f;
        if (v.algo == ALG_THIEF) { v.apart[3] = 0.f; v.apart[4] = s_lastEnvTarget[pad]; }
        else {
            for (int k = 0; k < 8; k++) v.apart[k] = s_lastNote[pad].apart[k];
            if (v.algo == ALG_PLUCK) v.iaux[2] = 0;   // re-seed the string
            v.ampEnv.gate(true); v.filtEnv.gate(true);
        }
    }
}

void synthProcess(float* outL, float* outR, size_t n) {
    // ALGO param page's float shadow -> syn.algo (see synAlgoSelectPage()).
    // Two writers exist: the ALGO Param (synAlgoShadow, via the editor) and patchLoad
    // (syn.algo directly). Whichever CHANGED since last block wins; nothing is reverted.
    {
        static uint8_t lastAlgo = 0xFF; static float lastShadow = -1.f;
        int sh = (int)(synAlgoShadow + 0.5f);
        if (sh < 0) sh = 0; if (sh >= ALG_COUNT) sh = ALG_COUNT - 1;
        if ((float)sh != lastShadow && lastShadow >= 0.f) syn.algo = (uint8_t)sh;   // editor moved it
        else if (syn.algo != lastAlgo) synAlgoShadow = (float)syn.algo;             // patch/other set it
        if (syn.algo >= ALG_COUNT) syn.algo = 0;
        synAlgoShadow = (float)syn.algo;
        lastAlgo = syn.algo; lastShadow = synAlgoShadow;
    }

    float octGlobalMul = fastPow2(synPerfOctSmoo.process(syn.perf_octave));
    float volume = clampf(syn.volume, 0.f, ENGINE_VOL_MAX);
    // LFO (block rate). depth 0: untouched (no phase advance, every multiplier is exactly 1)
    float lfoCutMul = 1.f;
    if (synLfo.depth > 0.f) {
        float dep = clampf(synLfo.depth, 0.f, 1.f);
        s_lfoPhase += clampf(synLfo.rate_hz, 0.05f, 20.f) * (float)n / SR;
        if (s_lfoPhase >= 1.f) { s_lfoPhase -= (float)(int)s_lfoPhase; s_lfoRng = s_lfoRng * 1664525u + 1013904223u; s_lfoSh = (float)(s_lfoRng >> 8) * (2.f / 16777216.f) - 1.f; }
        int shp = (int)(synLfo.shape + 0.5f), dst = (int)(synLfo.dest + 0.5f);
        float x = s_lfoPhase, l;
        if (shp == LFO_TRI)         l = (x < 0.5f) ? (4.f * x - 1.f) : (3.f - 4.f * x);
        else if (shp == LFO_SQUARE) l = (x < 0.5f) ? 1.f : -1.f;
        else if (shp == LFO_SH)     l = s_lfoSh;
        else                        l = sinf(6.2831853f * x);
        if (dst == LFOD_PITCH)      octGlobalMul *= fastPow2(l * dep * (2.f / 12.f));
        else if (dst == LFOD_AMP)   volume *= 1.f - dep * 0.25f * (1.f - l);   // tremolo: 100 % .. (1 - dep/2)
        else                        lfoCutMul = fastPow2(l * dep * 2.f);
    }
    bool perfActive = syn.perf_active;
    float perfCut   = clampf(syn.perf_cutoff, 0.f, 1.f);
    float perfCutHz = 200.f * fastPow2(perfCut * 5.9f);                       // 200 Hz .. 12 kHz, log (musical) sweep
#if PERF_ACCEL_FILTER
    float thiefCutMul = perfActive ? fastPow2(lerpf(-4.5f, 0.5f, perfCut)) : 1.f;   // Thief's own VCF: /22 closed .. x1.4 open
#else
    const float thiefCutMul = 1.f; perfActive = false; (void)perfCutHz;             // PERF card leaves the filter alone
#endif

    static float thiefL[BLOCK], thiefR[BLOCK];
    bool thiefAny = false;
    memset(thiefL, 0, n * sizeof(float)); memset(thiefR, 0, n * sizeof(float));

    for (int vi = 0; vi < SYN_VOICES; vi++) {
        Voice& v = synVoices[vi];
        if (v.ampEnv.stage == ADSR::IDLE) continue;

        const SynPreset& pr = syn.preset[v.algo];

        // block-rate filter coefficient: keytracked cutoff + filter envelope, capped for SVF stability.
        float filtLevel = v.filtEnv.processBlock((uint32_t)n);
        float baseCut = perfActive ? perfCutHz : pr.cutoff;
        float cutHz = baseCut + pr.filt_amt * filtLevel * (12000.f - 40.f);
        cutHz *= lfoCutMul;
        cutHz = clampf(cutHz, 40.f, 12000.f);
        float keytrack = sqrtf(v.note_hz * (1.f / 220.f));
        float fcHz = cutHz * keytrack;
        float f = 2.f * sinf(3.14159265f * clampf(fcHz, 20.f, 0.45f * SR) / SR);
        // Chamberlin SVF (lp += f*bp; bp += f*(x-lp-q*bp)): poles stable iff f*q < 2 and f^2 + 2*f*q < 4.
        // The old flat 0.9 cap broke that at low resonance (q ~ 2): f=0.9 -> 0.81 + 3.6 > 4, unstable, Nyquist
        // tone on high notes. Cap f at 0.9 as before AND at 95 % of the stability limit for this q, so every
        // (cutoff, reso, note) is stable and cases that were already stable are unchanged.
        float q = 2.0f * (1.02f - clampf(pr.resonance, 0.f, 1.f));
        float fMax = svfMaxF(q);
        if (f > fMax) f = fMax;
        if (f < 0.001f) f = 0.001f;
        v.svf_f = f;
        v.svf_q = q;

        if (v.algo == ALG_THIEF) {
            // ===== V2 voice loop, verbatim (L2286-2428): one-pole AR env, drift, per-note VCF =====
            float m = clampf(syn.perf_env_macro, 0.f, 1.f), amb = synAmb(m);   // plucky <-> ambient macro
            float atk = v.apart[2] / synPluck(m);
            float rel = (m <= 0.5f) ? v.apart[5] * lerpf(1.5f, 1.f, m * 2.f) : v.apart[5];
            if (amb > 0.f) {                                                    // swell: fade in, hold, fade out
                if (atk > SYN_AMB_ATK_COEF) atk = synTowards(atk, SYN_AMB_ATK_COEF, amb);
                if (rel > SYN_AMB_REL_COEF) rel = synTowards(rel, SYN_AMB_REL_COEF, amb);
            }
            if (v.atk_min_coef > 0.f && atk < v.atk_min_coef) atk = v.atk_min_coef;   // sequenced: attack budget
            // ambient side: keep the per-note VCF from closing all the way, or long notes go dim
            float fcFloor = lerpf(0.f, 0.22f, amb);
            if (syn.wash_coef > 0.f && !v.stolen) rel = syn.wash_coef;          // FX WASH: v2 relCoef override (stolen voices still die fast)
            if (atk > 0.5f) atk = 0.5f;
            // plucky half: once the attack has peaked, the held target drops toward 0 so the note
            // decays under the finger (m = 0 -> dies completely, m = 0.5 -> untouched sustain)
            if (m < 0.5f && v.apart[4] > 0.f && v.apart[3] >= v.apart[4] * 0.97f && v.gate_samples != 0)
                v.apart[4] *= lerpf(0.f, 1.f, m * 2.f);
            for (size_t i = 0; i < n; i++) {
                if (v.gate_samples > 0) {
                    v.gate_samples--;
                    if (v.gate_samples == 0) { v.apart[4] = 0.f; v.gate_samples = -1; synEmitOff(v); }
                }
                float& env = v.apart[3];
                float eCoef = (v.apart[4] > env) ? atk : rel;
                env += (v.apart[4] - env) * eCoef;
                if (env < 0.0004f && v.apart[4] == 0.f) { v.ampEnv.stage = ADSR::IDLE; break; }
                float hz = v.note_hz * v.pitch_mul * octGlobalMul;
                float raw = synAlgoRender(vi, v, pr, hz / SR, env, synRng);
                raw = flushBad(raw);
                // per-note VCF pluck — states HARD-CLAMPED so ringing cannot grow
                v.aux[2] += (v.aux[3] - v.aux[2]) * v.aux[4];
                float fc = v.aux[2]; if (fc < fcFloor) fc = fcFloor;
                fc *= thiefCutMul * lfoCutMul; if (fc > 0.9f) fc = 0.9f;
                v.svf_lp += fc * v.svf_bp;
                float vhp = raw - v.svf_lp - v.aux[5] * v.svf_bp;
                v.svf_bp += fc * vhp;
                v.svf_lp = clampf(flushBad(v.svf_lp), -6.f, 6.f);
                v.svf_bp = clampf(flushBad(v.svf_bp), -6.f, 6.f);
                float sig = v.svf_lp * (0.05f * env);
                thiefL[i] += sig * (1.f - v.pan);
                thiefR[i] += sig * v.pan;
            }
            thiefAny = true;
            continue;
        }

        if (syn.wash_coef > 0.f && !v.stolen) v.ampEnv.r_coef = syn.wash_coef; // FX WASH: long blurry tail (stolen voices still die fast)
        for (size_t i = 0; i < n; i++) {
            if (v.ampEnv.stage == ADSR::IDLE) break;

            if (v.gate_samples > 0) {
                v.gate_samples--;
                if (v.gate_samples == 0) { v.ampEnv.release(); v.gate_samples = -1; synEmitOff(v); }
            }

            float envA = v.ampEnv.process();

            if (v.pitch_mul != v.pitch_target) {
                if (v.pitch_mul < v.pitch_target) {
                    v.pitch_mul += v.pitch_rate;
                    if (v.pitch_mul > v.pitch_target) v.pitch_mul = v.pitch_target;
                } else {
                    v.pitch_mul -= v.pitch_rate;
                    if (v.pitch_mul < v.pitch_target) v.pitch_mul = v.pitch_target;
                }
            }
            float hz = v.note_hz * v.pitch_mul * octGlobalMul;
            float phaseInc = hz / SR;

            float raw = synAlgoRender(vi, v, pr, phaseInc, envA, synRng);
            raw = flushBad(raw);

            v.svf_lp += v.svf_f * v.svf_bp;
            float hp = raw - v.svf_lp - v.svf_q * v.svf_bp;
            v.svf_bp += v.svf_f * hp;
            v.svf_lp = clampf(flushBad(v.svf_lp), -6.f, 6.f);
            v.svf_bp = clampf(flushBad(v.svf_bp), -6.f, 6.f);

            float sig = flushBad(v.svf_lp * envA * volume * 0.05f);
            outL[i] += sig * (1.f - v.pan);
            outR[i] += sig * v.pan;
        }
    }
    if (thiefAny) {
        // V2 synth bus: master SVF (open) -> saturation drive tanh(x*2.0) -> mix at 0.8
        for (size_t i = 0; i < n; i++) {
            float dl = thiefL[i] * 2.0f, dr = thiefR[i] * 2.0f;
            dl = (dl > 3.f) ? 1.f : (dl < -3.f) ? -1.f : dl * (27.f + dl * dl) / (27.f + 9.f * dl * dl);
            dr = (dr > 3.f) ? 1.f : (dr < -3.f) ? -1.f : dr * (27.f + dr * dr) / (27.f + 9.f * dr * dr);
            outL[i] += dl * 0.8f * volume;
            outR[i] += dr * 0.8f * volume;
        }
    }
}

// ---------------------------------------------------------------- editor pages
static const char* THIEF_NAMES[4] = { "CHARACTR", "GLIDEPCT", "SWELLPCT", "DRIFT" };
static const char* FM_NAMES[4]    = { "RATIO", "INDEX", "IDXDECAY", "FEEDBK" };
static const char* WAVE_NAMES[4]  = { "TABLE", "MORPH", "DETUNE", "ENVMORPH" };
static const char* PLUCK_NAMES[4] = { "DAMP", "BRIGHT", "PICKPOS", "DECAY" };
static const char* SUPER_NAMES[4] = { "DETUNE", "SPREAD", "MIX", "SUB" };
static const char* CZ_NAMES[4]    = { "DISTORT", "RESO", "ENVDIST", "SHAPE" };
static const char* FOLD_NAMES[4]  = { "FOLD", "SYM", "ENVFOLD", "DRIVE" };
static const char* ADD_NAMES[4]   = { "TILT", "ODDEVEN", "DECAYHI", "DRIFT" };

static Param     s_algoParams[ALG_COUNT][4];
static ParamPage s_algoPages[ALG_COUNT];

static Param     s_envParams[ALG_COUNT][4];
static ParamPage s_envPages[ALG_COUNT];
static Param     s_filtParams[ALG_COUNT][7];
static ParamPage s_filtPages[ALG_COUNT];
static Param     s_pitchParams[ALG_COUNT][3];
static ParamPage s_pitchPages[ALG_COUNT];
static Param     s_widthParams[ALG_COUNT][2];
static ParamPage s_widthPages[ALG_COUNT];

static Param     s_algoSelParam;
static ParamPage s_algoSelPage;
static Param     s_mixParam;
static ParamPage s_mixPage;

static Param     SDRAM_BSS s_lfoParams[4];   // SDRAM: DTCM headroom is near its gate
static ParamPage SDRAM_BSS s_lfoPage;
static const char* const LFO_SHAPE_NAMES[LFO_SHAPES] = { "SINE", "TRI", "SQUARE", "S&H" };
static const char* const LFO_DEST_NAMES[LFOD_DESTS]  = { "CUTOFF", "PITCH", "AMP" };
static bool s_pagesBuilt = false;

static void synBuildPages() {
    if (s_pagesBuilt) return;
    s_pagesBuilt = true;

    const char* const* names4[ALG_COUNT] = {
        THIEF_NAMES, FM_NAMES, WAVE_NAMES, PLUCK_NAMES, SUPER_NAMES, CZ_NAMES, FOLD_NAMES, ADD_NAMES
    };

    for (int a = 0; a < ALG_COUNT; a++) {
        SynPreset& pr = syn.preset[a];
        for (int k = 0; k < 4; k++) {
            float mn = 0.f, mx = 1.f; uint8_t flags = PF_PERCENT;
            if (a == ALG_FM && k == 0) { mn = 0.1f; mx = 16.f; flags = PF_NONE; }
            if (a == ALG_FM && k == 2) { mn = 0.f; mx = 4.f; flags = PF_NONE; }
            if (a == ALG_WAVE && k == 0) { mn = 0.f; mx = (float)(WT_N - 1); flags = PF_NONE; }
            if (a == ALG_ADD && k == 0) { mn = -1.f; mx = 1.f; flags = PF_BIPOLAR; }
            s_algoParams[a][k] = { names4[a][k], &pr.p[k], mn, mx, (mx - mn) * 0.01f, flags, nullptr };
        }
        s_algoPages[a] = { "ALGO", s_algoParams[a], 4 };

        s_envParams[a][0] = { "ATTACK",  &pr.amp.a, 0.001f, 4.f, 0.01f, PF_PAIR_X, nullptr };
        s_envParams[a][1] = { "DECAY",   &pr.amp.d, 0.005f, 6.f, 0.01f, PF_NONE,   nullptr };
        s_envParams[a][2] = { "SUSTAIN", &pr.amp.s, 0.f,    1.f, 0.01f, PF_PAIR_X | PF_PERCENT, nullptr };
        s_envParams[a][3] = { "RELEASE", &pr.amp.r, 0.005f, 6.f, 0.01f, PF_NONE,   nullptr };
        s_envPages[a] = { "ENV", s_envParams[a], 4 };

        s_filtParams[a][0] = { "CUTOFF",   &pr.cutoff,    40.f, 12000.f, 10.f, PF_PAIR_X, nullptr };
        s_filtParams[a][1] = { "RESO",     &pr.resonance, 0.f,  1.f,     0.01f, PF_PERCENT, nullptr };
        s_filtParams[a][2] = { "FATTACK",  &pr.filt.a,    0.001f, 4.f,   0.01f, PF_PAIR_X, nullptr };
        s_filtParams[a][3] = { "FDECAY",   &pr.filt.d,    0.005f, 6.f,   0.01f, PF_NONE,   nullptr };
        s_filtParams[a][4] = { "FSUSTAIN", &pr.filt.s,    0.f,    1.f,   0.01f, PF_PAIR_X | PF_PERCENT, nullptr };
        s_filtParams[a][5] = { "FRELEASE", &pr.filt.r,    0.005f, 6.f,   0.01f, PF_NONE,   nullptr };
        s_filtParams[a][6] = { "FAMOUNT",  &pr.filt_amt,  -1.f,   1.f,   0.01f, PF_BIPOLAR, nullptr };
        s_filtPages[a] = { "FILTER", s_filtParams[a], 7 };

        s_pitchParams[a][0] = { "SEMIS",    &pr.pitch_amt, -24.f, 24.f, 1.f, (uint8_t)(PF_PAIR_X | PF_QUANT | PF_BIPOLAR), nullptr };
        s_pitchParams[a][1] = { "GLIDEIN",  &pr.glide_in,  0.f, 2.f, 0.01f, PF_NONE,   nullptr };
        s_pitchParams[a][2] = { "GLIDEOUT", &pr.glide_out, 0.f, 2.f, 0.01f, PF_NONE,   nullptr };
        s_pitchPages[a] = { "PITCH", s_pitchParams[a], 3 };

        s_widthParams[a][0] = { "WIDTH",    &pr.width,      0.f, 1.f, 0.01f, PF_PAIR_X | PF_PERCENT, nullptr };
        s_widthParams[a][1] = { "WIDTHPRB", &pr.width_prob, 0.f, 1.f, 0.01f, PF_PERCENT, nullptr };
        s_widthPages[a] = { "WIDTH", s_widthParams[a], 2 };
    }

    synAlgoShadow = (float)syn.algo;
    s_algoSelParam = { "ALGO", &synAlgoShadow, 0.f, (float)(ALG_COUNT - 1), 1.f, PF_QUANT | PF_ENUM, SYN_ALGO_NAMES };
    s_algoSelPage  = { "ALGO", &s_algoSelParam, 1 };

    s_mixParam = { "VOLUME", &syn.volume, 0.f, ENGINE_VOL_MAX, 0.01f, PF_PERCENT, nullptr };
    s_mixPage  = { "MIX", &s_mixParam, 1 };

    s_lfoParams[0] = { "LFORATE",  &synLfo.rate_hz, 0.05f, 20.f, 0.05f, (uint8_t)PF_LOG, nullptr };
    s_lfoParams[1] = { "LFODEPTH", &synLfo.depth,   0.f,   1.f,  0.01f, (uint8_t)PF_PERCENT, nullptr };
    s_lfoParams[2] = { "LFOSHAPE", &synLfo.shape,   0.f, (float)(LFO_SHAPES - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), LFO_SHAPE_NAMES };
    s_lfoParams[3] = { "LFODEST",  &synLfo.dest,    0.f, (float)(LFOD_DESTS - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), LFO_DEST_NAMES };
    s_lfoPage = { "LFO", s_lfoParams, 4 };
}

const ParamPage* synAlgoParamPage(uint8_t algo) {
    synBuildPages();
    if (algo >= ALG_COUNT) algo = 0;
    return &s_algoPages[algo];
}
const ParamPage* synEnvPage()    { synBuildPages(); return &s_envPages[syn.algo]; }
const ParamPage* synFilterPage() { synBuildPages(); return &s_filtPages[syn.algo]; }
const ParamPage* synPitchPage()  { synBuildPages(); return &s_pitchPages[syn.algo]; }
const ParamPage* synWidthPage()  { synBuildPages(); return &s_widthPages[syn.algo]; }
const ParamPage* synAlgoSelectPage() { synBuildPages(); return &s_algoSelPage; }
const ParamPage* synMixPage()        { synBuildPages(); return &s_mixPage; }
const ParamPage* synLfoPage()        { synBuildPages(); return &s_lfoPage; }
