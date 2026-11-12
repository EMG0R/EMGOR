// render.cpp -- the AUDIO plane's per-block orchestration, hardware-free.
// Order per block: clock -> sequencer fire -> engines -> fx -> metro -> output -> shared state.
// (extracted verbatim from the firmware's AudioCallback; the hardware parts -- joystick ADC poll,
//  clock_engine sample clock, PO-sync click, CPU meter -- stay in hw/audio_callback.cpp.)
#include <string.h>
#include "core_config.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "render.h"
#include "events.h"

static uint32_t s_sample_now = 0;   // absolute sample time of the start of this block
uint32_t pmorSampleNow(void) { return s_sample_now; }
static volatile bool s_run = true;
static volatile bool s_metro_gate = true;   // followed transport for the metronome ALONE (AUTO-follow outside a session: the sequencer keeps its own behaviour)
void pmorSetTransportRun(bool run) { s_run = run; }
bool pmorTransportRun(void) { return s_run; }
void pmorSetMetroGate(bool run) { s_metro_gate = run; }
bool pmorFollowedRun(void) { return s_run && s_metro_gate; }
void pmorReseed(uint32_t seed28) {
    seed28 &= 0x0FFFFFFFu;
    fxReseed(seed28); synthReseed(seed28); samplerReseed(seed28); seqReseed(seed28);
    pmorEventPushSeed(seed28);
}

// Host (pmor.core~ in Live): PMOR is the only place that sequences/generates. The host plays ONLY the notes it receives
// (PMOR's MIDI, recorded clips), so muting a Live track silences it. Firmware leaves this false.
bool g_render_notes_only = false;

static void onRetrig(const uint16_t* masks) {
    for(uint8_t k = 0; k < N_PADS; k++)
        if((masks[ENG_DRUMS] >> k) & 1u) drumsTrigger(k, 0.9f);
    synthRetrigLast(masks[ENG_SYNTH]);   // the SAME notes/characters that were playing — a real stutter
    samplerRetrig(masks[ENG_SMP]);   // chops; the shadow playhead keeps the sample's true position
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

void pmorRenderBlock(const float* in0, float* L, float* R, size_t n)
{
    if(!s_retrig_registered) { fxSetRetrigCallback(onRetrig); s_retrig_registered = true; }
    clk.varispeed_target = fx.varispeed_req;            // HALF / DBL / STOP shadow clock

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
    if(steps && s_run) {
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
        for(int e = 0; e < ENG_COUNT; e++) masks[e] = (fire && !g_render_notes_only) ? seqFire((Engine)e, fstep, e == ENG_SYNTH ? synHeld : (g.pad_engine == e ? held : 0), s_gen_tick_total) : 0;
        if(s_metro_gate) metroStep(gstep, seq.lane[ENG_DRUMS].steps_per_bar);   // the play-mode count rides the SAME master 16th as everything above
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
        for(int e = 0; e < ENG_COUNT; e++) masks[e] = g_render_notes_only ? 0 : seqFire((Engine)e, s_dbl_step, e == ENG_SYNTH ? synHeld : (g.pad_engine == e ? held : 0), s_gen_tick_total);
        tapGuardMasks(masks, s_sample_now);
        for(uint8_t k = 0; k < N_PADS; k++)
            if((masks[ENG_DRUMS] >> k) & 1u) drumsTrigger(k, 1.f);
        synthFireStep(masks[ENG_SYNTH], s_dbl_step);
        samplerFireStep(masks[ENG_SMP]);
    }

    // ── render ─────────────────────────────────────────────────────────────
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
        samplerProcess(in0, L, R, n);
    } else if(perf_tail && tail_eng == ENG_SMP) {
        drumsProcess(L, R, n);
        memset(s_perfL, 0, n * sizeof(float)); memset(s_perfR, 0, n * sizeof(float));
        samplerProcess(in0, s_perfL, s_perfR, n);
        fxProcessPerf(s_perfL, s_perfR, n);
        for(size_t i = 0; i < n; i++) { L[i] += s_perfL[i]; R[i] += s_perfR[i]; }
    } else {
        drumsProcess(L, R, n);
        samplerProcess(in0, L, R, n);
    }
    if(perf_tail && !perf_on) {                          // tails: run ~3 s after release, then drop the bus
        static uint32_t quiet_blocks = 0;
        if(++quiet_blocks > (uint32_t)(3.f * BLOCK_HZ)) { perf_tail = false; quiet_blocks = 0; }
    }
    fxProcess(L, R, n, g.fsr, g.lidar, g.accel_x, g.on_fx_page);
    metroProcess(L, R, n);                               // after FX (stutter/crush never touch it), before the master stage (fader + clip guard still apply)
    outputProcess(L, R, n);                              // fader + soft clip + hard ±1 + NaN guard + g.out_amp
    // ── shared state for the control plane ────────────────────────────────
    s_sample_now += (uint32_t)n;
    g.gstep = clk.gstep;
    {
        uint32_t spb = seq.lane[ENG_DRUMS].steps_per_bar;
        g.bar = spb ? clk.gstep / spb : clk.gstep / MAX_STEPS_BAR;
    }
    g.bpm = clk.bpm;

}
