// sequencer.cpp — three lane-sets (drums/synth/sampler): MANUAL/PLAY/GEN,
// live record, grow/shrink tiling, generative firing (density*probability,
// drum group bias, synth generative arpeggiator) and the plain arp.
#include <math.h>
#include <string.h>
#include "sequencer.h"
#include "synth.h"
#include "clock.h"
#include "scale.h"
#include "drums.h"
#include "engine_util.h"

SeqState seq;

// ---------------------------------------------------------------- lifecycle
void seqInit() {
    memset(&seq, 0, sizeof(seq));
    for (int e = 0; e < ENG_COUNT; e++) {
        LaneSet& l = seq.lane[e];
        l.bars          = 1;
        l.steps_per_bar = 16;
        l.mode          = SEQ_MANUAL;
        l.probability   = 0.5f;
        l.density       = 0.5f;
        l.rec_on        = 0.f;
        l.hand_f        = 0.f;   // retired (see sequencer.h); kept so LaneSet/patch layout is unchanged
        l.polyphony     = 8.f;    // full polyphony (8) by default (EMGOR)
        l.oct_amount    = 0.f;
        l.oct_prob      = 0.f;
        l.arp           = false;
        l.arp_on        = 0.f;
        l.arp_rate      = 2.f;   // 1/8 by default (EMGOR: gen/arp at 16ths was too busy)
        l.recording     = false;
        l.bars_f        = (float)l.bars;
        l.steps_f       = (float)l.steps_per_bar;
        l.mode_f        = (float)l.mode;
    }
}

uint32_t seqLongestLoop() {
    uint32_t m = 1;
    for (int e = 0; e < ENG_COUNT; e++) {
        uint32_t len = seqLoopLen(seq.lane[e]);
        if (len > m) m = len;
    }
    return m;
}

// ---------------------------------------------------------------- editing (CONTROL plane)
void seqSetBars(Engine e, uint8_t bars) {
    if (e >= ENG_COUNT) return;
    if (bars < 1) bars = 1;
    if (bars > MAX_BARS) bars = MAX_BARS;
    LaneSet& l = seq.lane[e];
    uint8_t  oldBars = l.bars;
    uint32_t oldLen  = (uint32_t)oldBars * l.steps_per_bar;
    uint32_t newLen  = (uint32_t)bars * l.steps_per_bar;
    if (newLen > MAXSTEPS) newLen = MAXSTEPS;

    if (bars > oldBars && oldLen > 0) {
        // grow: tile the existing loop to fill (1->8: 8 copies; 3->9: 3 copies;
        // a non-multiple ratio tiles and truncates the last partial copy).
        for (uint32_t i = oldLen; i < newLen; i++) l.step[i] = l.step[i % oldLen];
    }
    // shrink: do nothing beyond newLen — the old data past the new loop
    // length stays exactly where it was ("shrink keeps hidden data").
    l.bars   = bars;
    l.bars_f = (float)bars;
    clockSetLoopLen(seqLongestLoop());
}

void seqSetStepsPerBar(Engine e, uint8_t spb) {
    if (e >= ENG_COUNT) return;
    if (spb < 1) spb = 1;
    if (spb > MAX_STEPS_BAR) spb = MAX_STEPS_BAR;
    LaneSet& l = seq.lane[e];
    uint8_t oldSpb = l.steps_per_bar;
    if (spb != oldSpb) {
        uint16_t tmp[MAXSTEPS];
        memcpy(tmp, l.step, sizeof(tmp));
        for (uint32_t b = 0; b < l.bars; b++) {
            uint32_t oldBase = b * oldSpb;
            uint32_t newBase = b * spb;
            for (uint32_t i = 0; i < spb; i++) {
                uint32_t dst = newBase + i;
                if (dst >= MAXSTEPS) break;
                uint16_t val = 0;
                if (oldSpb > 0) {
                    uint32_t src = oldBase + (i % oldSpb);   // tile/truncate within the bar
                    if (src < MAXSTEPS) val = tmp[src];
                }
                l.step[dst] = val;
            }
        }
        // (data at/after l.bars*max(oldSpb,spb) that this reflow never
        // touches is simply whatever was already there — kept hidden, same
        // rule as seqSetBars.)
    }
    l.steps_per_bar = spb;
    l.steps_f = (float)spb;
    clockSetLoopLen(seqLongestLoop());
}

void seqToggle(Engine e, uint32_t step, uint8_t lane) {
    if (e >= ENG_COUNT || step >= MAXSTEPS || lane >= N_LANES) return;
    seq.lane[e].step[step] ^= (uint16_t)(1u << lane);
}

void seqClearLane(Engine e, uint8_t lane) {
    if (e >= ENG_COUNT || lane >= N_LANES) return;
    uint16_t m = (uint16_t)~(1u << lane);
    for (uint32_t i = 0; i < MAXSTEPS; i++) seq.lane[e].step[i] &= m;
}

void seqClear(Engine e) {
    if (e >= ENG_COUNT) return;
    memset(seq.lane[e].step, 0, sizeof(seq.lane[e].step));
}

void seqRecordHit(Engine e, uint8_t lane, uint32_t now_sample) {
    if (e >= ENG_COUNT || lane >= N_LANES) return;
    LaneSet& l = seq.lane[e];
    uint32_t loopLen = seqLoopLen(l);
    if (loopLen == 0) loopLen = 1;

    // LOOP TAP-IN: quantise the press to the MASTER GRID, never to the tap itself.
    //
    // `now_sample` is the press instant, stamped when the keypad poll first SAW the press
    // (main.cpp onPad, g.pad_press_sample) -- not "now". By the time this runs, the audio ISR may
    // already have fired one or more 16th boundaries since that instant (the UI event is processed
    // later, and the loop can stall for tens of ms). The old code measured the press against the
    // CURRENT boundary and so put a hit that landed just before a boundary on the wrong side of
    // it. This version measures against the master grid in absolute samples and is correct for a
    // press of any age.
    //
    // Grid state: clk.gstep = the step that most recently FIRED; the NEXT boundary (index gstep+1)
    // is at next_step_whole/frac; steps are clk.step_q samples apart (Q32.32). The boundary
    // nearest the press is `next - k*step` with k = round(dist_to_next / step), index gstep+1-k.
    // The ISR advances gstep and next_step_whole in two separate stores, so read them as a
    // consistent pair (re-read until gstep is stable) instead of tearing across a boundary.
    uint32_t g0, gs, nxt_whole, nxt_frac;
    do {
        g0 = clk.gstep;
        nxt_whole = clk.next_step_whole;
        nxt_frac  = clk.next_step_frac;
        gs = clk.gstep;
    } while (g0 != gs);
    const double step = (double)clk.step_q / 4294967296.0;                // samples per 16th
    if (!(step > 0.0)) return;
    // Exact: the int32 difference is bounded by the press age (<< 2^31), only that small number
    // becomes a double -- same pattern clock.cpp uses.
    const double dist_to_next = (double)(int32_t)(nxt_whole - now_sample) + (double)nxt_frac / 4294967296.0;
    const int32_t k = (int32_t)floor(dist_to_next / step + 0.5);
    const int64_t idx = (int64_t)gs + 1 - (int64_t)k;                     // global 16th index of the nearest boundary
    int64_t local = idx % (int64_t)loopLen;
    if (local < 0) local += (int64_t)loopLen;
    if ((uint32_t)local < MAXSTEPS) l.step[(uint32_t)local] |= (uint16_t)(1u << lane);
}

// ---------------------------------------------------------------- generative arp (PLAY mode)
static uint8_t s_arp_idx[ENG_COUNT] = {0, 0, 0};
static uint16_t arpAdvance(Engine e, uint16_t unionMask) {
    if (unionMask == 0) return 0;
    uint8_t start = s_arp_idx[e];
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t pad = (uint8_t)((start + i) % 16);
        if ((unionMask >> pad) & 1u) {
            s_arp_idx[e] = (uint8_t)(pad + 1);
            return (uint16_t)(1u << pad);
        }
    }
    return 0;
}

// ---------------------------------------------------------------- generative firing (GEN mode)
static uint32_t s_gen_lcg[ENG_COUNT] = {0x9E3779B9u, 0x85EBCA6Bu, 0xC2B2AE35u};

// kick beats 1&3, snare 2&4, hats on 8ths, perc sparse — biases a per-pad
// generative "chance" multiplier on top of density*probability.
static float drumGroupWeight(DrumGroup grp, uint32_t local_step, uint8_t steps_per_bar) {
    uint32_t quarter = (steps_per_bar >= 4) ? (steps_per_bar / 4) : 1;
    uint32_t eighth   = (steps_per_bar >= 8) ? (steps_per_bar / 8) : 1;
    switch (grp) {
        case DG_KICK: {
            uint32_t beat = local_step / quarter;
            return (beat == 0 || beat == 2) ? 1.0f : 0.25f;
        }
        case DG_SNARE: {
            uint32_t beat = local_step / quarter;
            return (beat == 1 || beat == 3) ? 1.0f : 0.15f;
        }
        case DG_HAT:
            return ((local_step % eighth) == 0) ? 0.9f : 0.2f;
        case DG_PERC:
        default:
            return 0.12f;   // sparse
    }
}

// MONOPHONIC generative drums (EMGOR): at most ONE generative hit per 16th. Rolling all
// 16 pads independently could fire a dozen resonator voices on one step and overwhelm the
// board; this picks whether the step fires at all, then which single pad, weighted by
// the groove bias (kick on 1&3, snare 2&4, hats on 8ths, perc sparse).
static uint16_t genFireDrums(LaneSet& l, uint32_t local_step) {
    uint32_t& s = s_gen_lcg[ENG_DRUMS];
    float fire = clampf(l.density * l.probability * 2.4f, 0.f, 0.95f);   // 50%/50% -> 60% of steps
    if (lcg01(s) >= fire) return 0;
    float w[N_PADS]; float tot = 0.f;
    for (uint8_t pad = 0; pad < N_PADS; pad++) {
        w[pad] = drumGroupWeight(drumPadGroup(pad), local_step, l.steps_per_bar);
        tot += w[pad];
    }
    if (tot <= 0.f) return 0;
    float r = lcg01(s) * tot;
    for (uint8_t pad = 0; pad < N_PADS; pad++) {
        r -= w[pad];
        if (r <= 0.f) return (uint16_t)(1u << pad);
    }
    return (uint16_t)(1u << (N_PADS - 1));
}

// flat per-pad roll — used for the sampler (no positional bias specified).
static uint16_t genFireFlat(Engine e, LaneSet& l) {
    uint32_t& s = s_gen_lcg[e];
    uint16_t  mask = 0;
    float     chance = clampf(l.density * l.probability, 0.f, 1.f);
    for (uint8_t pad = 0; pad < N_PADS; pad++)
        if (lcg01(s) < chance) mask |= (uint16_t)(1u << pad);
    return mask;
}

// synth generative arpeggiator: held notes constrain the candidate set;
// polyphony caps how many of them can land on this step.
static uint16_t genFireSynth(LaneSet& l, uint16_t held) {
    uint32_t& s = s_gen_lcg[ENG_SYNTH];
    uint16_t  candidates = held ? held : 0xFFFFu;
    int       poly = (int)clampf(l.polyphony, 1.f, (float)SYN_MAX_POLY);
    // per-candidate chance is normalised by how many candidates there are, so constraining the
    // generator to a held/latched chord of 1-3 pads keeps the same note RATE instead of going
    // sparse (EMGOR 2026-09-15: "notes start triggering super sparsely until I re-enter PERF")
    int       cnt = __builtin_popcount((unsigned)candidates); if (cnt < 1) cnt = 1;
    // (the env macro / tilt NEVER touches sequencing — EMGOR 2026-09-15; the envelope adapts to the
    //  sequencing instead, see synth.cpp's attack budget)
    float     chance = clampf(l.density * l.probability * 16.f / (float)cnt, 0.f, 1.f);
    uint16_t  mask = 0;
    int       picked = 0;
    for (uint8_t pad = 0; pad < N_PADS && picked < poly; pad++) {
        if (!((candidates >> pad) & 1u)) continue;
        if (lcg01(s) < chance) { mask |= (uint16_t)(1u << pad); picked++; }
    }
    return mask;
}

// ---------------------------------------------------------------- generative subdivision gate
// (2026-10, TASK 1) per-engine "last gen_tick_count we rolled the dice at" -- lets SEQ_GEN
// re-roll only when engine/clock.h's independent generative accumulator has actually crossed
// a subdivision boundary since the last time this engine was asked, instead of on every 16th.
// At the default GEN_DIV_1_16 the gen accumulator is phase-locked 1:1 with the master 16th
// grid (clockComputeGenStepQ() for that index == clk.step_q exactly), so it advances by
// exactly 1 on every master 16th and this gate is true every single call -- i.e. unchanged
// behaviour until something selects a different GENRATE, per clock.h's contract.
static uint32_t s_gen_last_tick[ENG_COUNT] = {0, 0, 0};
static inline bool genTickGate(Engine e, uint32_t gen_tick_count) {
    if (gen_tick_count == s_gen_last_tick[e]) return false;
    s_gen_last_tick[e] = gen_tick_count;
    return true;
}

// ---------------------------------------------------------------- AUDIO plane
uint16_t seqFire(Engine e, uint32_t gstep, uint16_t held, uint32_t gen_tick_count) {
    if (e >= ENG_COUNT) return 0;
    LaneSet& l = seq.lane[e];
    l.arp = (l.arp_on >= 0.5f);      // sync the float editor shadow -> bool each step
    {                                 // sync the MODE editor shadow -> the enum each step
        int m = (int)(l.mode_f + 0.5f);
        if (m < 0) m = 0;
        if (m >= SEQ_MODE_COUNT) m = SEQ_MODE_COUNT - 1;
        l.mode = (uint8_t)m;
    }

    if (l.mode == SEQ_MANUAL) return 0;

    uint32_t loopLen = seqLoopLen(l);
    if (loopLen == 0) loopLen = 1;
    uint32_t local = gstep % loopLen;
    uint16_t playMask = (local < MAXSTEPS) ? l.step[local] : 0;

    if (l.mode == SEQ_PLAY) {
        uint16_t unionMask = playMask | held;
        uint32_t rate = (l.arp_rate >= 1.f) ? (uint32_t)(l.arp_rate + 0.5f) : 1u;
        if (l.arp && unionMask != 0 && (gstep % rate) == 0)
            return arpAdvance(e, unionMask);
        return playMask;
    }

    // SEQ_GEN
    if (l.probability <= 0.f) return playMask;   // no generative dice -> plain recorded pattern

    // TASK 1: dice only re-roll on a generative-subdivision boundary (GENRATE), not every
    // 16th -- see genTickGate()/clock.h. No new boundary since the last roll this engine
    // made -> nothing new to say this 16th.
    if (!genTickGate(e, gen_tick_count)) return 0;

    uint32_t local_in_bar = (l.steps_per_bar > 0) ? (local % l.steps_per_bar) : 0;
    switch (e) {
        case ENG_DRUMS: {
            // GEN + performance (drums & sampler): a HELD pad becomes a live generative roll —
            // it fires on this 16th with chance 0.35 + 0.65*probability (so at max probability
            // it is a straight 16th roll, at low probability a sparse stutter), on top of the
            // generative bed. Release = the roll stops. Pads are one-shot in every other mode.
            static uint32_t sh = 0x5EEDD1CEu;
            uint16_t m = genFireDrums(l, local_in_bar);
            float pr = 0.35f + 0.65f * l.probability;
            int added = 0;
            for (uint8_t k = 0; k < 16 && added < 2; k++)
                if (((held >> k) & 1u) && lcg01(sh) < pr) { m |= (uint16_t)(1u << k); added++; }
            return m;
        }
        case ENG_SYNTH: {
            // TASK 1 (2026-10): generative synth used to walk the ARPRATE grid here
            // (never finer than 8ths) as its only rate control. ARPRATE/arp_rate is a
            // DIFFERENT job -- it still paces the plain performer arpeggiator in the
            // SEQ_PLAY branch above (arpAdvance() over held/sequenced notes), which has
            // nothing to do with generative density. That was the one overlapping use:
            // removed here in favour of the dedicated GENRATE subdivision (genTickGate()
            // above already gated this whole switch on it), so GEN mode now has exactly
            // one rate control, same as PLAY mode.
            return genFireSynth(l, held);
        }
        case ENG_SMP: {
            static uint32_t sh2 = 0xA5A5F00Du;
            uint16_t m = genFireFlat(e, l);
            float pr = 0.35f + 0.65f * l.probability;
            for (uint8_t k = 0; k < 16; k++)
                if (((held >> k) & 1u) && lcg01(sh2) < pr) m |= (uint16_t)(1u << k);
            return m;
        }
        default:        return 0;
    }
}

int seqPickOctaveOffset(const LaneSet& l) {
    static uint32_t s = 0xB5297A4Du;
    if (lcg01(s) >= l.oct_prob) return 0;
    if (l.oct_amount >= 4.5f) {          // uniform over all 5 octaves
        int r = (int)(lcg01(s) * 5.f);
        if (r > 4) r = 4;
        return r - 2;                    // -2..+2
    }
    int maxOct = (int)(l.oct_amount + 0.5f);   // round
    if (maxOct <= 0) return 0;
    int span = 2 * maxOct + 1;
    int r = (int)(lcg01(s) * (float)span);
    if (r >= span) r = span - 1;
    return r - maxOct;
}

// ---------------------------------------------------------------- params for the editor
static const char* const ARP_NAMES[2]     = {"OFF", "ON"};
static const char* const ARPRATE_NAMES[5] = {"", "1/16", "1/8", "1/8", "1/4"};
// SEQ_MANUAL/SEQ_PLAY/SEQ_GEN, per sequencer.h's SeqMode
static const char* const MODE_NAMES[SEQ_MODE_COUNT] = {"MANUAL", "PLAY", "GEN"};
static const char* const CLEAR_NAMES[2]   = {"CLICK", "CLICK"};
static float s_clear_f[ENG_COUNT] = {0.f, 0.f, 0.f};   // CLEAR list item (click = wipe the engine's sequence; nav intercepts)

static const Param DRUM_SEQ_P[] = {
    {"REC",     &seq.lane[ENG_DRUMS].rec_on,      0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), ARP_NAMES},
    {"CLEAR",   &s_clear_f[ENG_DRUMS],            0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), CLEAR_NAMES},
    {"MODE",    &seq.lane[ENG_DRUMS].mode_f,      0.f, (float)(SEQ_MODE_COUNT - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), MODE_NAMES},
    {"PROB",    &seq.lane[ENG_DRUMS].probability, 0.f, 1.f, 0.01f, PF_PERCENT, nullptr},
    {"DENSITY", &seq.lane[ENG_DRUMS].density,      0.f, 1.f, 0.01f, PF_PERCENT, nullptr},
    {"BARS",    &seq.lane[ENG_DRUMS].bars_f,       1.f, 16.f, 1.f,  PF_QUANT,   nullptr},
    {"STEPS",   &seq.lane[ENG_DRUMS].steps_f,      1.f, 16.f, 1.f,  PF_QUANT,   nullptr},
};
static const ParamPage DRUM_SEQ_PAGE = {"SEQ", DRUM_SEQ_P, (uint8_t)(sizeof(DRUM_SEQ_P) / sizeof(DRUM_SEQ_P[0]))};

static const Param SMP_SEQ_P[] = {
    {"REC",     &seq.lane[ENG_SMP].rec_on,      0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), ARP_NAMES},
    {"CLEAR",   &s_clear_f[ENG_SMP],            0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), CLEAR_NAMES},
    {"MODE",    &seq.lane[ENG_SMP].mode_f,      0.f, (float)(SEQ_MODE_COUNT - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), MODE_NAMES},
    {"PROB",    &seq.lane[ENG_SMP].probability, 0.f, 1.f, 0.01f, PF_PERCENT, nullptr},
    {"DENSITY", &seq.lane[ENG_SMP].density,      0.f, 1.f, 0.01f, PF_PERCENT, nullptr},
    {"BARS",    &seq.lane[ENG_SMP].bars_f,       1.f, 16.f, 1.f,  PF_QUANT,   nullptr},
    {"STEPS",   &seq.lane[ENG_SMP].steps_f,      1.f, 16.f, 1.f,  PF_QUANT,   nullptr},
};
static const ParamPage SMP_SEQ_PAGE = {"SEQ", SMP_SEQ_P, (uint8_t)(sizeof(SMP_SEQ_P) / sizeof(SMP_SEQ_P[0]))};

static const Param SYN_SEQ_P[] = {
    {"REC",     &seq.lane[ENG_SYNTH].rec_on,      0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), ARP_NAMES},
    {"CLEAR",   &s_clear_f[ENG_SYNTH],            0.f, 1.f, 1.f, (uint8_t)(PF_QUANT | PF_ENUM), CLEAR_NAMES},
    {"MODE",    &seq.lane[ENG_SYNTH].mode_f,      0.f, (float)(SEQ_MODE_COUNT - 1), 1.f, (uint8_t)(PF_QUANT | PF_ENUM), MODE_NAMES},
    {"PROB",    &seq.lane[ENG_SYNTH].probability, 0.f, 1.f,  0.01f, PF_PERCENT,                    nullptr},
    {"DENSITY", &seq.lane[ENG_SYNTH].density,      0.f, 1.f,  0.01f, PF_PERCENT,                    nullptr},
    {"POLY",    &seq.lane[ENG_SYNTH].polyphony,    1.f, 8.f,  1.f,   (uint8_t)(PF_QUANT | PF_INT),  nullptr},
    {"OCTAVE",  &seq.lane[ENG_SYNTH].oct_amount,   0.f, 5.f,  0.1f,  PF_PAIR_X,                     nullptr},
    {"OCTPROB", &seq.lane[ENG_SYNTH].oct_prob,     0.f, 1.f,  0.01f, PF_PERCENT,                    nullptr},
    {"ARP",     &seq.lane[ENG_SYNTH].arp_on,       0.f, 1.f,  1.f,   (uint8_t)(PF_QUANT | PF_ENUM), ARP_NAMES},
    {"ARPRATE", &seq.lane[ENG_SYNTH].arp_rate,     1.f, 4.f,  1.f,   (uint8_t)(PF_QUANT | PF_ENUM), ARPRATE_NAMES},
    {"BARS",    &seq.lane[ENG_SYNTH].bars_f,       1.f, 16.f, 1.f,   PF_QUANT,                      nullptr},
    {"STEPS",   &seq.lane[ENG_SYNTH].steps_f,      1.f, 16.f, 1.f,   PF_QUANT,                      nullptr},
};
static const ParamPage SYN_SEQ_PAGE = {"SEQ", SYN_SEQ_P, (uint8_t)(sizeof(SYN_SEQ_P) / sizeof(SYN_SEQ_P[0]))};

const ParamPage* seqParamPage(Engine e) {
    switch (e) {
        case ENG_DRUMS: return &DRUM_SEQ_PAGE;
        case ENG_SYNTH: return &SYN_SEQ_PAGE;
        case ENG_SMP:   return &SMP_SEQ_PAGE;
        default:        return nullptr;
    }
}
