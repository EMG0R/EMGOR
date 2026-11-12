// audio_callback.cpp — the AUDIO plane. Never blocks, never touches I2C.
// Order per block: polls → clock → sequencer fire → engines → fx → output → meter.
#include <string.h>
#include "../config.h"
#include "../hw/hw.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "daisy_seed.h"
extern "C" {
#include "usb/clock_engine.h"
#include "usb/audio_format.h"
}
using namespace daisy;

static uint32_t s_sample_now = 0;   // absolute sample time of the start of this block

// TASK 2 (2026-10): C-linkage adapter so usb/clock_engine.c (pure C, can't call a
// C++-mangled symbol) can read engine/clock.h's exact grid period and derive its
// MIDI tick scheduling from the SAME fixed-point source clock.cpp's own grid
// accumulator uses -- see usb/clock_engine.h's declaration for the full rationale.
extern "C" uint64_t clock_engine_grid_step_q32(void) { return clockStepSamplesQ32(); }

// stutter / BZZ retrigger: re-fire the last step's events at the sub-division the FX host chose
static void onRetrig(const uint16_t* masks) {
    for(uint8_t k = 0; k < N_PADS; k++)
        if((masks[ENG_DRUMS] >> k) & 1u) drumsTrigger(k, 0.9f);
    synthRetrigLast(masks[ENG_SYNTH]);   // the SAME notes/characters that were playing — a real stutter
    samplerFireStep(masks[ENG_SMP]);
}
static bool s_retrig_registered = false;

// live-play de-doubling: drop sequenced hits whose pad was tapped by hand within TAP_GUARD before
// this step (the tap already sounded), and stamp the hits that do fire so a tap right AFTER them
// is swallowed by the engines' own guard.
static void tapGuardMasks(uint16_t* masks, uint32_t now) {
    uint32_t guard = (uint32_t)(clk.step_samples * TAP_GUARD_STEPS);
    for(int e = 0; e < ENG_COUNT; e++) {
        uint16_t m = masks[e];
        for(int p = 0; p < 16 && m; p++) {
            if(!((m >> p) & 1u)) continue;
            uint32_t t = g.tap_at[e][p];
            if(t != 0u && (now - t) < guard) { masks[e] &= (uint16_t)~(1u << p); continue; }
            g.seq_hit_at[e][p] = now ? now : 1u;
        }
    }
}
static bool     s_dbl_pending = false;   // DBL: the second (mid-step) shadow event
static uint32_t s_dbl_step = 0, s_dbl_at = 0;
static float s_perfL[BLOCK], s_perfR[BLOCK];

// clock_engine_process_audio() requires n_frames <= AUDIO_BLOCK_SAMPLES (48):
// its beat scratch buffer is that size. Our block is 64, so feed it in ≤48
// chunks — the routine is per-sample stateful and advances its own sample
// clock by n_frames, so chunking is exactly equivalent. Run IN PLACE on the
// final stereo block: SY0 passes audio through, SY1/SY2/SY5 replace one
// channel with the PO-sync click, SY3/SY4 consume sync-in from `in`.
static void poSyncProcess(const float* codecL, const float* codecR, float* L, float* R, size_t n) {
    // The routine's `in` is what it ROUTES to the outputs (SY0 stereo, SY1/2
    // mono on the free channel) and what it WATCHES for sync pulses (SY3/SY4/
    // SY5). So: the sync-in channel gets the codec input, everything else gets
    // our own mix (mono where the mode collapses to one channel), and the
    // result lands in L/R in place.
    static float sL[BLOCK], sR[BLOCK];
    if(n > BLOCK) n = BLOCK;
    sync_mode_t mode = clock_engine_get_sync_mode();
    switch(mode) {
        case SY3_IN_L:   // watch codec L; audio (from `in` R) → both outs
            for(size_t i = 0; i < n; i++) { sL[i] = codecL[i]; sR[i] = 0.5f * (L[i] + R[i]); }
            break;
        case SY4_IN_R:   // watch codec R; audio (from `in` L) → both outs
            for(size_t i = 0; i < n; i++) { sL[i] = 0.5f * (L[i] + R[i]); sR[i] = codecR[i]; }
            break;
        case SY5_PASS:   // watch codec L, repeat click on L; audio (from `in` R) on R
            for(size_t i = 0; i < n; i++) { sL[i] = codecL[i]; sR[i] = 0.5f * (L[i] + R[i]); }
            break;
        default:         // SY0/SY1/SY2: our mix is the audio source
            for(size_t i = 0; i < n; i++) { sL[i] = L[i]; sR[i] = R[i]; }
            break;
    }
    size_t off = 0;
    while(off < n) {
        size_t c = n - off; if(c > AUDIO_BLOCK_SAMPLES) c = AUDIO_BLOCK_SAMPLES;
        const float* ip[2] = { sL + off, sR + off };
        float*       op[2] = { L + off, R + off };
        clock_engine_process_audio(ip, op, c);
        off += c;
    }
}

void AudioCallback(float** in, float** out, size_t n)
{
    uint32_t t_us0 = System::GetUs();

    joyPollFromIsr(s_sample_now);                       // enc1 + joystick ADC, 750 Hz; s_sample_now = this block's first sample (stamps encoder presses)
    if(!s_retrig_registered) { fxSetRetrigCallback(onRetrig); s_retrig_registered = true; }
    clk.varispeed_target = fx.varispeed_req;            // HALF / DBL / STOP shadow clock
    // Advance the clock engine's sample clock. Required or the boot-listen
    // window never elapses and the device never emits MIDI clock (2026-08-16).
    clock_engine_note_audio_block((uint32_t)n);

    // ── clock + sequencer ──────────────────────────────────────────────────
    int steps = clockTick(s_sample_now);
    {   // record-mode metronome: audible iff any lane's REC (seq.lane[e].rec_on, the one record flag) is on
        bool anyRec = false;
        for(int e = 0; e < ENG_COUNT; e++) if(seq.lane[e].rec_on > 0.5f) anyRec = true;
        metroBlock(anyRec);
    }
    // TASK 1 (2026-10): advance the generative-subdivision accumulator every block
    // (independent of whether the master 16th fired this block — a coarser GENRATE,
    // e.g. 1/4, legitimately fires less often than every 16th), and keep a running
    // total so sequencer.cpp's seqFire() can tell "did a new subdivision boundary
    // happen since I last rolled the dice" without engine/clock.h needing to expose
    // its own tick-count field (it only reports fired-this-block, 0..8, see clock.h).
    static uint32_t s_gen_tick_total = 0;
    s_gen_tick_total += (uint32_t)clockGenTick();
    if(steps) {
        uint32_t gstep = clk.gstep;
        uint16_t held  = g.pads_held;
        uint16_t masks[ENG_COUNT];
        uint16_t synHeld = g.syn_chord ? g.syn_chord : (g.pad_engine == ENG_SYNTH ? held : 0);   // latched chord persists across menus
        // HALF / DBL / STOP are MASTER-CLOCK-DERIVED (EMGOR 2026-09-14): the step that
        // plays is a pure function of the master step within the current bar, so the
        // "1" of every bar always lands together, and letting go at any moment drops
        // straight back onto the master grid with nothing to realign.
        //   DBL : bar plays twice per master bar  -> shadow = base + (2*i)   % 16 (and +1 at mid-step)
        //   HALF: first half of the bar, stretched -> shadow = base + (i / 2), fires on even steps only
        //   STOP: no events (tape stops), pitch handled by clk.varispeed
        uint32_t bar  = MAX_STEPS_BAR;
        uint32_t base = gstep - (gstep % bar);
        uint32_t i    = gstep % bar;
        float    vs   = fx.varispeed_req;
        bool     fire = true;
        uint32_t fstep = gstep;
        s_dbl_pending = false;
        if(vs < 0.2f) {                     // STOP
            fire = false;
        } else if(vs < 0.75f) {             // HALF
            if(i & 1u) fire = false;
            fstep = base + (i >> 1);
        } else if(vs > 1.5f) {              // DBL
            fstep = base + ((2u * i) % bar);
            s_dbl_pending = true;
            s_dbl_step    = base + ((2u * i + 1u) % bar);
            s_dbl_at      = s_sample_now + (uint32_t)(clk.step_samples * 0.5f);
        }
        for(int e = 0; e < ENG_COUNT; e++) masks[e] = fire ? seqFire((Engine)e, fstep, e == ENG_SYNTH ? synHeld : (g.pad_engine == e ? held : 0), s_gen_tick_total) : 0;
        metroStep(gstep, seq.lane[ENG_DRUMS].steps_per_bar);   // the play-mode count rides the SAME master 16th as everything above
        fxOnStep(gstep, masks);                          // stutters may rewrite the masks
        tapGuardMasks(masks, s_sample_now);
        for(uint8_t k = 0; k < N_PADS; k++)
            if((masks[ENG_DRUMS] >> k) & 1u) drumsTrigger(k, 1.f);
        synthFireStep(masks[ENG_SYNTH], gstep);
        samplerFireStep(masks[ENG_SMP]);
    }
    // DBL mid-step event
    if(s_dbl_pending && (int32_t)(s_sample_now + n - s_dbl_at) >= 0) {
        s_dbl_pending = false;
        uint16_t masks[ENG_COUNT];
        uint16_t held = g.pads_held;
        uint16_t synHeld = g.syn_chord ? g.syn_chord : (g.pad_engine == ENG_SYNTH ? held : 0);
        for(int e = 0; e < ENG_COUNT; e++) masks[e] = seqFire((Engine)e, s_dbl_step, e == ENG_SYNTH ? synHeld : (g.pad_engine == e ? held : 0), s_gen_tick_total);
        tapGuardMasks(masks, s_sample_now);
        for(uint8_t k = 0; k < N_PADS; k++)
            if((masks[ENG_DRUMS] >> k) & 1u) drumsTrigger(k, 1.f);
        synthFireStep(masks[ENG_SYNTH], s_dbl_step);
        samplerFireStep(masks[ENG_SMP]);
    }

    // ── render ─────────────────────────────────────────────────────────────
    float* L = out[0];
    float* R = out[1];
    memset(L, 0, n * sizeof(float));
    memset(R, 0, n * sizeof(float));
    // engines; the one in PERFORMANCE mode renders to its own bus so the joystick
    // throws (spring / delay / dist / crush) touch only that engine
    uint8_t perf = fx.perf_engine;
    bool    perf_on = (perf == ENG_DRUMS || perf == ENG_SMP) &&
                      (fx.perf_up > 0.001f || fx.perf_right > 0.001f || fx.perf_down > 0.001f || fx.perf_left > 0.001f);
    static bool    perf_tail = false;                    // keep running the perf bus until the tails die
    static uint8_t perf_tail_eng = 0xFF;                 // engine latched for the tail (nav clears fx.perf_engine
                                                           // to 0xFF the instant the PERF card is left, so the live
                                                           // value can't be used once perf_on goes false — this
                                                           // keeps fxProcessPerf running on the right bus until the
                                                           // tail actually finishes decaying, per spec).
    if(perf_on) { perf_tail = true; perf_tail_eng = perf; }
    uint8_t tail_eng = perf_tail ? perf_tail_eng : perf;
    synthProcess(L, R, n);
    if(perf_tail && tail_eng == ENG_DRUMS) {
        memset(s_perfL, 0, n * sizeof(float)); memset(s_perfR, 0, n * sizeof(float));
        drumsProcess(s_perfL, s_perfR, n);
        fxProcessPerf(s_perfL, s_perfR, n);
        for(size_t i = 0; i < n; i++) { L[i] += s_perfL[i]; R[i] += s_perfR[i]; }
        samplerProcess(in[0], L, R, n);
    } else if(perf_tail && tail_eng == ENG_SMP) {
        drumsProcess(L, R, n);
        memset(s_perfL, 0, n * sizeof(float)); memset(s_perfR, 0, n * sizeof(float));
        samplerProcess(in[0], s_perfL, s_perfR, n);
        fxProcessPerf(s_perfL, s_perfR, n);
        for(size_t i = 0; i < n; i++) { L[i] += s_perfL[i]; R[i] += s_perfR[i]; }
    } else {
        drumsProcess(L, R, n);
        samplerProcess(in[0], L, R, n);
    }
    if(perf_tail && !perf_on) {                          // tails: run ~3 s after release, then drop the bus
        static uint32_t quiet_blocks = 0;
        if(++quiet_blocks > (uint32_t)(3.f * BLOCK_HZ)) { perf_tail = false; quiet_blocks = 0; }
    }
    fxProcess(L, R, n, g.fsr, g.lidar, g.accel_x, g.on_fx_page);
    metroProcess(L, R, n);                               // after FX (stutter/crush never touch it), before the master stage (fader + clip guard still apply)
    outputProcess(L, R, n);                              // fader + soft clip + hard ±1 + NaN guard + g.out_amp
    poSyncProcess(in[0], in[1], L, R, n);                // PO-sync click / sync-in (never wired in v2)

    // ── shared state for the control plane ────────────────────────────────
    s_sample_now += (uint32_t)n;
    g.gstep = clk.gstep;
    {
        uint32_t spb = seq.lane[ENG_DRUMS].steps_per_bar;
        g.bar = spb ? clk.gstep / spb : clk.gstep / MAX_STEPS_BAR;
    }
    g.bpm = clk.bpm;

    uint32_t dt = System::GetUs() - t_us0;
    const float block_us = (float)n * (1000000.f / SR);
    g.cpu_load = (float)dt / block_us;
}
