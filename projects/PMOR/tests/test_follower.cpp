// test_follower.cpp -- drives the REAL usb/clock_engine.c (follower) + the real pmor_core grid (clock.cpp) with a synthetic 124 BPM MIDI clock
// stream: Start, then 32 bars. Models the audio callback (64-sample blocks), IRQ-time stamping (dcd_rx_complete_hook + tud_midi_rx_cb) and USB
// delivery: clean, late-only jitter (half-normal sigma 0.5 ms) and bunching (2-4 ticks in one transfer).
// Asserts: follower phase error p99 < 2 ms (ferr), and the |mean| offset of PMOR's 16th boundaries from the true 16ths < 0.5 ms.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <algorithm>
extern "C" {
#include "clock_engine.h"
}
#include "clock.h"
#include "shared_state.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "render.h"
#include "params.h"
#include "demiurge-transport.h"

// ---------------------------------------------------------------- stubs for the USB / board layer clock_engine.c links against
extern "C" {
static uint64_t g_tick = 0;                                       // 1 tick = 1/1000 sample  (tick_hz = 48 MHz)
uint32_t pmor_tick(void) { return (uint32_t) g_tick; }
uint32_t pmor_tick_hz(void) { return 48000000u; }
uint32_t board_get_now_ms(void) { return (uint32_t) (g_tick / 48000u); }
volatile uint32_t g_dwc2_midi_in_fifo_bytes = 0, g_dwc2_midi_in_fifo_pushes = 0, g_dwc2_midi_in_kick_bytes = 0, g_dwc2_midi_in_kicks = 0;
static std::deque<uint8_t> s_fifo;                                // 4-byte packets, host -> device
bool     tud_midi_packet_write(uint8_t const[4]) { return true; }
bool     tud_midi_packet_read(uint8_t p[4]) { if (s_fifo.size() < 4) return false; for (int i = 0; i < 4; i++) { p[i] = s_fifo.front(); s_fifo.pop_front(); } return true; }
uint32_t tud_midi_available(void) { return (uint32_t) s_fifo.size(); }
bool     tud_midi_mounted(void) { return true; }
uint32_t tud_midi_stream_read(void*, uint32_t) { return 0; }
uint32_t tud_cdc_write(void const*, uint32_t n) { return n; }
uint32_t tud_cdc_write_flush(void) { return 0; }
void pmor_audio_ctl(int) {}  void pmor_oled_contrast(int) {}  void pmor_oled_test(int) {}  void pmor_pb5_test(int) {}  void pmor_probe_set(const char*, int) {}
void pmor_sine_set(int) {}
uint64_t clock_engine_grid_step_q32(void) { return clockStepSamplesQ32(); }
void dcd_rx_complete_hook(uint8_t epnum);
void tud_midi_rx_cb(uint8_t itf);
}

// ---------------------------------------------------------------- simulation
struct Rng { uint64_t s; double u() { s = s * 6364136223846793005ull + 1442695040888963407ull; return ((s >> 11) & ((1ull << 53) - 1)) / 9007199254740992.0; }
             double n() { double a = u() + 1e-12, b = u(); return sqrt(-2.0 * log(a)) * cos(6.283185307 * b); } };
struct Arrival { double t; std::vector<uint8_t> bytes; bool join = false; int spp = 0; };      // one USB transfer: arrival time (samples) + raw MIDI realtime bytes

static double g_stall_ms = 0; static uint32_t g_stall_until = 0; static int g_stall_pending = 0;   // main-loop stall model (samples): MIDI rx callback deferred
static void sendTransfer(const Arrival& a) {                   // IRQ-time stamp, packets into the FIFO, tud_task callback
    g_tick = (uint64_t) llround(a.t * 1000.0);
    dcd_rx_complete_hook(2);                                    // EPNUM_MIDI
    for (uint8_t b : a.bytes) { s_fifo.push_back(0x0F); s_fifo.push_back(b); s_fifo.push_back(0); s_fifo.push_back(0); }
    if (g_stall_until > 0) { g_stall_pending++; return; }   // main loop stalled: the IRQ already stamped, the tud_task callback runs when it comes back
    tud_midi_rx_cb(0);
}

struct Result { double mean_ms, std_ms, ferr_mean_us, ferr_p99_us, ferr_max_us; int n; int pos_const; bool pos_consistent; int join_rc; int join_late_us; bool join_done; };

static double g_bpm_nom = 124.0, g_ppm = 0.0; static int g_bars = 32;       // long-run drift test: nominal tempo, host clock period error vs PMOR's sample clock (ppm), length
static double g_max_abs_ms = 0, g_drift_ms = 0; static float g_lead_ms = 0.f;   // follower clock lead under test
static int g_preroll = 0;                           // ticks of clock streamed BEFORE Start (a DAW's pre-roll)
static double g_seed_pre = 0, g_seed_mid = 0;      // TEMPO seeds: before the first F8, and once mid-stream (bar 10)
static int g_early_skip = 0; static double g_early_2nd_ms = 0, g_early_max_ms = 0, g_early_first_ms = 0; static int g_early_n = 0;   // offsets of the first 16ths after Start
static double g_fh_press = -1; static std::vector<double> g_fh_fires; static long g_fh_j = -1; static double g_fh_T0 = 0;   // first-hit recording scenario: true press time (samples) -> lane 'drums' pad 3 fire instants
static int g_gap_at = -1, g_gap_n = 0;   // freewheel scenario: no ticks for g_gap_n ticks starting at tick g_gap_at (no Start/SPP after it: the tick count is lost)
static int g_auto_roles_seen = 0; static int g_role_1p5 = -1, g_role_2p5 = -1;   // role 1.5 s / 2.5 s after the last tick
static Result run(double sigma_ms, double bunch_prob, uint64_t seed, bool verbose, bool auto_role = false, int join_at16 = -1, double join_lat_ms = 0.0) {
    memset(&g, 0, sizeof g); clockInit(120.f); clk.follow_lead_ms = g_lead_ms; clockSetLoopLen(16);
    s_fifo.clear(); g_tick = 0; clock_engine_set_studio(false); clock_engine_init(); clock_engine_set_policy(0);
    if (auto_role) {   // AUTO policy: the bus claims the clock with CC116 = 2 on channel 16 (DEMIURGE master)
        uint8_t p[3] = {0xBF, 116, 2}; s_fifo.push_back(0x0B); s_fifo.push_back(p[0]); s_fifo.push_back(p[1]); s_fifo.push_back(p[2]);
    } else clock_engine_set_studio(true);              // STUDIO/EDIT: forced follower (the real use case for a DAW clock)
    if (g_seed_pre > 0) clock_engine_tempo_seed((float) g_seed_pre);
    if (g_fh_press >= 0) { seqInit(); seq.lane[ENG_DRUMS].mode_f = (float) SEQ_PLAY; seq.lane[ENG_DRUMS].mode = SEQ_PLAY; seq.lane[ENG_DRUMS].rec_on = 1.f; clockSetLoopLen(seqLongestLoop()); seqSetGridFollowed(true); g_fh_fires.clear(); g_fh_j = -1; }
    Rng rng{seed};
    const double BPM = g_bpm_nom, P = 60.0 * 48000.0 / (BPM * 24.0) * (1.0 + g_ppm * 1e-6);             // samples per tick
    const int BARS = g_bars, TICKS = BARS * 96, BLOCK_N = 64;
    // ---- build the arrival list
    std::vector<Arrival> arr; const double T0 = 64.0 * 400.0 + 31.0 + (double) g_preroll * P;          // Start / tick 0 true time (arbitrary phase inside a block)
    double lastT = T0 - 20.0 - (double) g_preroll * P - 100.0;
    if (join_at16 < 0) { Arrival s; s.t = T0 - 20.0; s.bytes = { 0xFA }; arr.push_back(s); }   // plain run: Start. JOIN run: the DAW is already playing, no 0xFA
    for (int k = -g_preroll; k < TICKS;) {
        int group = 1; if (rng.u() < bunch_prob) group = 2 + (int) (rng.u() * 3.0); if (group > 4) group = 4;
        if (k + group > TICKS) group = TICKS - k;
        if (k < 0 && k + group > 0) group = -k;
        double tk = T0 + (double) (k + group - 1) * P;                        // a bunched transfer leaves when its LAST tick is due
        double late = fabs(rng.n()) * sigma_ms * 48.0;                        // late-only, half-normal
        Arrival a; a.t = std::max(tk + late, lastT + 1.0); lastT = a.t; a.bytes.assign((size_t) group, 0xF8);
        arr.push_back(a); k += group;
    }
    if (g_gap_at >= 0) { double a = T0 + (double) g_gap_at * P, b = T0 + (double) (g_gap_at + g_gap_n) * P; std::vector<Arrival> keep; for (auto& x : arr) if (!(x.t >= a && x.t < b)) keep.push_back(x); arr = keep; }
    if (g_preroll) std::stable_sort(arr.begin(), arr.end(), [](const Arrival& a, const Arrival& b) { return a.t < b.t; });
    if (join_at16 >= 0) {   // the host sends JOIN at the DAW's 16th boundary number join_at16 (true time T0 + j*6P), with latency jitter
        Arrival j; j.join = true; j.spp = join_at16; j.t = T0 + (double) join_at16 * 6.0 * P + join_lat_ms * 48.0; arr.push_back(j);
        std::sort(arr.begin(), arr.end(), [](const Arrival& a, const Arrival& b) { return a.t < b.t; });
    }
    // ---- run the audio clock
    size_t ai = 0; std::vector<double> offs; std::vector<double> bounds; std::vector<uint32_t> gsteps; uint32_t sample_now = 0; int join_rc = 99, join_late = 0; bool join_done = false, join_wait = false;
    const double lastArr = arr.empty() ? 0.0 : arr.back().t;
    const uint32_t nblocks = (uint32_t) ((T0 + (double) TICKS * P) / BLOCK_N) + 100 + (auto_role ? 3 * 48000 / BLOCK_N : 0u);   // AUTO: keep running 3 s past the last tick
    for (uint32_t b = 0; b < nblocks; b++, sample_now += BLOCK_N) {
        g_tick = (uint64_t) sample_now * 1000ull;
        if (g_stall_until && sample_now >= g_stall_until) { g_stall_until = 0; if (g_stall_pending) { while (g_stall_pending) { g_stall_pending--; tud_midi_rx_cb(0); } clock_engine_poll(); } }
        clock_engine_note_audio_block(BLOCK_N);                               // advances g_sample_clock, takes the block-start tick
        if (g_seed_mid > 0 && (double) sample_now >= T0 + 10.0 * 96.0 * P) { clock_engine_tempo_seed((float) g_seed_mid); g_seed_mid = 0; }
        // arrivals that happen during the PREVIOUS block period were already delivered below; deliver those landing in [sample_now, +64) after the callback
        uint32_t nb_whole = clockNextStepSample(); double nb_t = (double) nb_whole + clk.next_step_frac / 4294967296.0;
        int steps = clockTick(sample_now);
        if (steps > 0) { bounds.push_back(nb_t); gsteps.push_back(clk.gstep); if (g_fh_press >= 0 && g_fh_j >= 0 && (seqFire(ENG_DRUMS, clk.gstep, 0, 0) & 8u)) g_fh_fires.push_back(nb_t); }
        if (g_fh_press >= 0 && g_fh_j < 0 && (double) sample_now + BLOCK_N > g_fh_press + 40.0 * 48.0) {   // the pad is processed ~40 ms after the press: stamp = the true press instant
            seqRecordHit(ENG_DRUMS, 3, (uint32_t) llround(g_fh_press)); g_fh_j = 1; g_fh_T0 = T0; }
        while (ai < arr.size() && arr[ai].t < (double) sample_now + BLOCK_N) {
            if (arr[ai].t >= (double) sample_now) {
                if (arr[ai].join) {   // CDC request: IRQ-stamped on arrival, handled by the main loop a little later (0..3 ms)
                    g_tick = (uint64_t) llround(arr[ai].t * 1000.0); dcd_rx_complete_hook(3);
                    g_tick += (uint64_t) (rng.u() * 3.0 * 48000.0); join_rc = clock_engine_join_begin((uint16_t) arr[ai].spp); join_wait = join_rc == 0;
                } else { if (g_stall_ms > 0 && !arr[ai].bytes.empty() && arr[ai].bytes[0] == 0xFA) g_stall_until = sample_now + (uint32_t) (g_stall_ms * 48.0); sendTransfer(arr[ai]); if (!g_stall_until) clock_engine_poll(); if (verbose && g_preroll && !arr[ai].bytes.empty() && arr[ai].bytes[0] == 0xFA) printf("    START at %.0f: engine bpm %.2f clk.bpm %.2f\n", arr[ai].t, clock_engine_get_bpm(), clk.bpm); }
            }
            ai++;
        }
        if (join_wait) { int32_t l; int r = clock_engine_join_poll(&l); if (r == 1) { join_done = true; join_late = l; join_wait = false; } else if (r < 0) join_wait = false; }
        g_tick = (uint64_t) (sample_now + BLOCK_N) * 1000ull;
        if (!g_stall_until) clock_engine_poll();
        if (auto_role) { double tt = (double) sample_now - lastArr; if (tt >= 1.5 * 48000.0 && g_role_1p5 < 0) g_role_1p5 = (int) clock_engine_get_role(); if (tt >= 2.5 * 48000.0 && g_role_2p5 < 0) g_role_2p5 = (int) clock_engine_get_role(); }
        if (auto_role && b > 3000 && b < nblocks - 3000 - 3 * 48000 / BLOCK_N && clock_engine_get_role() != CLOCK_ROLE_DEMIURGE_MASTER) g_auto_roles_seen++;   // must stay follower while ticks arrive
        // what hw/transport_link.cpp does every pass: adopt the received tempo while slaved
        if (clock_engine_get_role() == CLOCK_ROLE_DEMIURGE_MASTER && clock_engine_clock_mode() == PMOR_CLKMODE_SLAVE) {
            float bb = clock_engine_get_bpm(); if (bb > 1.f && bb != clk.bpm) clockSetBpm(bb);
        }
    }
    // ---- offsets of PMOR's 16th boundaries vs the true 16ths (every 6th tick from T0), after 8 bars of settling
    const double S16 = 6.0 * P; double sum = 0, sum2 = 0; int n = 0;
    for (double bt : bounds) { if (bt < T0 + 8.0 * 16.0 * S16 / 1.0) continue; if (bt > T0 + (double) (BARS - 1) * 16.0 * S16) continue;
        double d = fmod(bt - T0, S16); if (d > S16 / 2) d -= S16; if (d < -S16 / 2) d += S16; sum += d; sum2 += d * d; n++; if (verbose && n < 6) printf("    boundary %.1f offset %.3f ms\n", bt, d / 48.0); }
    {   g_max_abs_ms = 0; g_drift_ms = 0; std::vector<double> ds;
        for (double bt : bounds) { if (bt < T0 + 8.0 * 16.0 * S16) continue; if (bt > T0 + (double) (BARS - 1) * 16.0 * S16) continue;
            double d = fmod(bt - T0, S16); if (d > S16 / 2) d -= S16; if (d < -S16 / 2) d += S16; ds.push_back(d / 48.0); if (fabs(d / 48.0) > g_max_abs_ms) g_max_abs_ms = fabs(d / 48.0); }
        size_t q = ds.size() / 5; if (q > 8) { double a = 0, b = 0; for (size_t i = 0; i < q; i++) { a += ds[i]; b += ds[ds.size() - 1 - i]; } g_drift_ms = (b - a) / (double) q; } }
    g_early_max_ms = 0; g_early_n = 0;
    for (double bt : bounds) { if (bt < T0 - 30.0) continue; if (bt > T0 + 12.0 * S16) break;
        double d = fmod(bt - T0, S16); if (d > S16 / 2) d -= S16; if (d < -S16 / 2) d += S16; if (g_early_n == 0) g_early_first_ms = d / 48.0; if (g_early_n == 1) g_early_2nd_ms = d / 48.0; if (g_early_n >= g_early_skip && fabs(d / 48.0) > g_early_max_ms) g_early_max_ms = fabs(d / 48.0); g_early_n++; if (verbose) printf("    early boundary %d: %+.3f ms\n", g_early_n, d / 48.0); }
    // position consistency: the labelled 16th (gstep % 16) minus the TRUE 16th index must be one constant for every boundary
    int pc = -1; bool cons = true; int np = 0;
    for (size_t i = 0; i < bounds.size(); i++) { double jf = (bounds[i] - T0) / S16; long jr = lround(jf); if (jr < 0 || bounds[i] < T0 + 8.0 * 16.0 * S16 || bounds[i] > T0 + (double) (BARS - 1) * 16.0 * S16) continue;
        int c = (int) (((long) (gsteps[i] % 16u) - jr) % 16 + 16) % 16; if (pc < 0) pc = c; else if (c != pc) cons = false; np++; }
    Result r{}; r.pos_const = pc; r.pos_consistent = cons && np > 100; r.join_rc = join_rc; r.join_late_us = join_late; r.join_done = join_done; r.n = n; double m = n ? sum / n : 0; r.mean_ms = m / 48.0; r.std_ms = n ? sqrt(sum2 / n - m * m) / 48.0 : 0;
    uint32_t fm = 0, fp = 0, fx = 0; clock_engine_ferr_stats(&fm, &fp, &fx); r.ferr_mean_us = fm; r.ferr_p99_us = fp; r.ferr_max_us = fx;
    return r;
}

int main(int argc, char** argv) {
    bool verbose = argc > 1;
    struct Case { const char* name; double sigma, bunch; } cases[] = { {"clean", 0.0, 0.0}, {"jitter sigma 0.5 ms", 0.5, 0.0}, {"jitter 0.5 ms + bunching 2-4", 0.5, 0.2} };
    int fails = 0;
    for (auto& c : cases) {
        Result r = run(c.sigma, c.bunch, 12345, verbose);
        bool ok = r.n > 100 && r.ferr_p99_us < 2000.0 && fabs(r.mean_ms) < 0.5;
        printf("%-30s boundaries %4d  offset mean %+7.3f ms std %6.3f ms | ferr mean %7.0f us p99 %7.0f us max %7.0f us  %s\n", c.name, r.n, r.mean_ms, r.std_ms, r.ferr_mean_us, r.ferr_p99_us, r.ferr_max_us, ok ? "PASS" : "FAIL");
        if (!ok) fails++;
    }
    {   // clock lead: the grid sits `lead` ms EARLIER than the followed 16ths (offset mean = -lead), also with jitter, and returns to 0 at lead 0
        const float leads[] = { 30.f, 15.f, -10.f, 40.f, -20.f };
        for (float L : leads) {
            g_lead_ms = L; Result r = run(0.5, 0.2, 4242, false); g_lead_ms = 0.f;
            bool ok = r.n > 100 && fabs(r.mean_ms + (double) L) < 0.5 && r.std_ms < 1.5;
            printf("%-30s lead %+5.0f ms: offset mean %+7.3f ms (want %+.1f) std %6.3f ms  %s\n", "clock lead", (double) L, r.mean_ms, -(double) L, r.std_ms, ok ? "PASS" : "FAIL"); if (!ok) fails++;
        }
    }
    {   // JOIN: a host connects while the DAW already plays (no 0xFA). Positions must match a normal Start run (same labelling), phase < 0.5 ms
        Result st = run(0.0, 0.0, 1, false);
        // no clock: JOIN refused (never followed, and as master)
        clock_engine_set_studio(false); clock_engine_init(); bool refused_cold = clock_engine_join_begin(5) == -1;
        printf("%-30s JOIN with no clock refused: %s\n", "JOIN no clock", refused_cold ? "yes PASS" : "NO FAIL"); if (!refused_cold) fails++;
        struct JC { const char* name; double lat_ms; int j; bool bunch; } jc[] = { {"JOIN exactly on the boundary", 0.0, 40, false}, {"JOIN +4.5 ms late", 4.5, 41, false}, {"JOIN -4.5 ms early", -4.5, 42, false},
                                                                             {"JOIN +3 ms, jitter+bunching", 3.0, 43, true}, {"JOIN -5 ms, jitter+bunching", -5.0, 44, true} };
        for (auto& c : jc) {
            Result r = run(c.bunch ? 0.5 : 0.0, c.bunch ? 0.2 : 0.0, 99 + (uint64_t) c.j, false, false, c.j, c.lat_ms);
            bool ok = r.join_rc == 0 && r.join_done && r.n > 100 && fabs(r.mean_ms) < 0.5 && r.ferr_p99_us < 2000.0 && r.pos_consistent && r.pos_const == st.pos_const;
            printf("%-30s rc %d applied %d tick_at %+6d us | offset mean %+7.3f ms | ferr p99 %6.0f us | positions %s (const %d vs Start run %d)  %s\n", c.name, r.join_rc, (int) r.join_done, r.join_late_us, r.mean_ms, r.ferr_p99_us, r.pos_consistent ? "consistent" : "BROKEN", r.pos_const, st.pos_const, ok ? "PASS" : "FAIL");
            if (!ok) fails++;
        }
    }
    {   // AUTO policy: follows while ticks arrive (never drops to master mid-stream), and is master again ~2 s after they stop
        g_auto_roles_seen = 0; g_role_1p5 = g_role_2p5 = -1; Result r = run(0.5, 0.2, 777, false, true);
        bool follow_held = g_role_1p5 == (int) CLOCK_ROLE_DEMIURGE_MASTER, master_back = g_role_2p5 == (int) CLOCK_ROLE_OPG_MASTER;   // still follow 1.5 s after the last tick, master again at 2.5 s
        bool ok = g_auto_roles_seen == 0 && r.ferr_p99_us < 2000.0 && follow_held && master_back;
        printf("%-30s mid-stream role flips %d | ferr p99 %7.0f us | role 1.5 s after stop: %s, 2.5 s after: %s  %s\n", "AUTO role (CC116 claim)", g_auto_roles_seen, r.ferr_p99_us, g_role_1p5 == 2 ? "follow" : "master", g_role_2p5 == 2 ? "follow" : "master", ok ? "PASS" : "FAIL");
        if (!ok) fails++;
    }
    {   // seed trigger: every accepted Start/Continue counts, even a Stop->Start faster than one main-loop pass
        s_fifo.clear(); g_tick = 0; clock_engine_set_studio(false); clock_engine_init(); clock_engine_set_studio(true);
        uint32_t c0 = clock_engine_start_count();
        sendTransfer({1000.0, {0xFA}}); clock_engine_poll();
        sendTransfer({2000.0, {0xFC}}); sendTransfer({2100.0, {0xFA}}); clock_engine_poll();       // Stop+Start inside one poll: the running edge is invisible
        sendTransfer({3000.0, {0xFB}}); clock_engine_poll();
        uint32_t d = clock_engine_start_count() - c0;
        printf("%-30s start count +%u for Start, Stop+Start, Continue (want 3): %s\n", "seed trigger", (unsigned) d, d == 3 ? "PASS" : "FAIL"); if (d != 3) fails++;
    }
    {   // Start transient: 1 bar of F8 pre-roll, then Start; the first 12 16ths must sit on the grid like the steady state, also when the
        // main loop stalls for 16 ms around the Start (macbert: the 2nd note after Start was 16.5 / 15.7 ms late at 100 / 124 BPM)
        for (double stall : {0.0, 8.0, 16.0}) {
            g_stall_ms = stall; g_preroll = 96; Result r = run(0.0, 0.0, 6, getenv("EARLY") != nullptr); double em = g_early_max_ms; int en = g_early_n; g_preroll = 0; g_stall_ms = 0;
            bool ok = en >= 8 && em < 1.0;
            printf("%-30s pre-roll 96 ticks, loop stall %4.1f ms at Start: first %d 16ths max |offset| %.3f ms (steady mean %+.3f)  %s\n", "start transient", stall, en, em, r.mean_ms, ok ? "PASS" : "FAIL"); if (!ok) fails++;
        }
    }
    {   // METRONOME follows the transport: silent from Stop to Start/Continue, first click on the Start tick, accent on the bar downbeat at SPP
        memset(&g, 0, sizeof(g)); clockInit(124.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit(); pmorSchemaInit(); clk.follow_lead_ms = g_lead_ms;
        seq.lane[ENG_DRUMS].rec_on = 1.f;                                        // record mode: the metronome is armed
        s_fifo.clear(); g_tick = 0; clock_engine_set_studio(false); clock_engine_init(); clock_engine_set_policy(0);
        clock_engine_set_transport_cb([](bool run) { if (clock_engine_studio()) pmorSetTransportRun(run); pmorSetMetroGate(run); });   // = hw/studio.cpp opTransportCb
        clock_engine_set_studio(true);
        const double P = 60.0 * 48000.0 / (124.0 * 24.0), T0 = 64.0 * 100.0 + 31.0 + 96 * P;   // tick 0 of the first Start
        struct Ev { double t; std::vector<uint8_t> b; std::vector<uint8_t> sys; };
        std::vector<Ev> evs;
        for (int k = -96; k < 0; k++) evs.push_back({T0 + k * P, {0xF8}, {}});                              // 1 bar of pre-roll
        evs.push_back({T0 - 20.0, {0xFA}, {}});
        for (int k = 0; k < 2 * 96; k++) evs.push_back({T0 + k * P, {0xF8}, {}});                          // 2 bars playing
        const double Tstop = T0 + 2 * 96 * P - 5.0;                                                         // Stop, then 4 bars of clock
        evs.push_back({Tstop, {0xFC}, {}});
        for (int k = 2 * 96; k < 6 * 96; k++) evs.push_back({T0 + k * P, {0xF8}, {}});
        const double Tcont = T0 + 6 * 96 * P - 40.0;                                                        // SPP = bar 3 (32 16ths), Continue
        evs.push_back({Tcont - 30.0, {}, {0xF2, 32, 0}});
        evs.push_back({Tcont, {0xFB}, {}});
        for (int k = 6 * 96; k < 7 * 96; k++) evs.push_back({T0 + k * P, {0xF8}, {}});
        std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });
        size_t ei = 0; static float L[64], R[64], in0[64];
        long ind_in_stop = 0, ind_in_play = 0; long first_click = -1, click_in_stop = 0; double first_cont_hz = 0, first_start_hz = 0; long cont_click = -1;
        const uint32_t tail_ms = 100;
        uint32_t nb = (uint32_t) ((T0 + 7 * 96 * P) / 64.0) + 50, sample_now = 0;
        for (uint32_t b = 0; b < nb; b++, sample_now += 64) {
            g_tick = (uint64_t) sample_now * 1000ull; clock_engine_note_audio_block(64);
            while (ei < evs.size() && evs[ei].t < (double) sample_now + 64) {
                if (evs[ei].t >= (double) sample_now) {
                    g_tick = (uint64_t) llround(evs[ei].t * 1000.0); dcd_rx_complete_hook(2);
                    for (uint8_t x : evs[ei].b) { s_fifo.push_back(0x0F); s_fifo.push_back(x); s_fifo.push_back(0); s_fifo.push_back(0); }
                    if (!evs[ei].sys.empty()) { s_fifo.push_back(0x03); for (uint8_t x : evs[ei].sys) s_fifo.push_back(x); }
                    tud_midi_rx_cb(0); clock_engine_poll();
                }
                ei++;
            }
            memset(in0, 0, sizeof in0);
            pmorRenderBlock(in0, L, R, 64);
            clock_engine_poll();
            { bool fr = pmorFollowedRun(); if ((double) sample_now > Tstop + 100 && (double) sample_now < Tcont - 100) { if (fr) ind_in_stop++; } else if ((double) sample_now > T0 + 2 * P && (double) sample_now < Tstop - 100 && !fr) ind_in_play++; }
            bool nz = false; long at = -1; for (int i = 0; i < 64; i++) if (fabsf(L[i]) > 1e-5f) { nz = true; at = (long) sample_now + i; break; }
            if (first_click < 0 && nz && (double) sample_now > T0 - 2 * P) { first_click = at; first_start_hz = metro.inc * 48000.0; }
            if ((double) sample_now > Tstop + tail_ms * 48.0 && (double) sample_now < Tcont - 100 && nz) click_in_stop++;
            if ((double) sample_now > Tcont - 60 && cont_click < 0 && nz) { cont_click = at; first_cont_hz = metro.inc * 48000.0; }
        }
        double d0 = (double) first_click - T0;
        bool ok4 = ind_in_stop == 0 && ind_in_play == 0;
        bool ok1 = first_click >= 0 && fabs(d0) <= 64.0 + 20.0, ok2 = click_in_stop == 0, ok3 = cont_click >= 0 && fabs(first_cont_hz - 1800.0) < 1.0 && first_start_hz > 1790.0;
        printf("%-30s first click %+.0f samples from the Start tick (%s) | clicks during 4 bars stopped: %ld (%s) | Continue SPP=bar 3: first click %.0f Hz (1800 = accent) (%s)\n", "metronome follows transport",
               d0, ok1 ? "PASS" : "FAIL", click_in_stop, ok2 ? "PASS" : "FAIL", first_cont_hz, ok3 ? "PASS" : "FAIL");
        printf("%-30s OLED/pad indicators: lit-while-stopped blocks %ld, dark-while-playing blocks %ld (both must be 0): %s\n", "indicators follow transport", ind_in_stop, ind_in_play, ok4 ? "PASS" : "FAIL");
        if (!(ok1 && ok2 && ok3 && ok4)) fails++;
    }
    {   // AUTO-follow tempo messages from the bus (CC116 role claim, CC119 7-bit tempo, CC120/121 14-bit tempo number), channel 16, no clock ticks needed
        auto cc16 = [](uint8_t cc, uint8_t v) { s_fifo.push_back(0x0B); s_fifo.push_back(0xBF); s_fifo.push_back(cc); s_fifo.push_back(v); tud_midi_rx_cb(0); clock_engine_poll(); };
        auto number = [&](double bpm) { uint16_t w = dt_bpm_to_wire14(bpm); cc16(120, (uint8_t) (w >> 7)); cc16(121, (uint8_t) (w & 127)); };
        auto settle = []() { for (int i = 0; i < 4; i++) { clock_engine_note_audio_block(64); clock_engine_poll(); } };
        int f0 = fails; char why[160] = "";
        s_fifo.clear(); g_tick = 0; clock_engine_set_studio(false); clock_engine_init(); clock_engine_set_policy(0);
        cc16(116, 2); settle();
        if (clock_engine_get_role() != CLOCK_ROLE_DEMIURGE_MASTER) { fails++; snprintf(why, sizeof why, "CC116=2 did not make PMOR follow"); }
        cc16(119, 21); settle();                                                        // legacy 7-bit: 40 + 21/127*360 = 99.5
        if (fabs(clock_engine_get_bpm() - 99.528) > 0.1) { fails++; snprintf(why, sizeof why, "CC119 legacy tempo not applied (%.3f)", clock_engine_get_bpm()); }
        number(117.3); settle();                                                        // the 14-bit number outranks the 7-bit value
        double nb = dt_bpm_canon(117.3);
        if (fabs(clock_engine_get_bpm() - nb) > 0.001) { fails++; snprintf(why, sizeof why, "CC120/121 number not applied (%.4f want %.4f)", clock_engine_get_bpm(), nb); }
        cc16(119, 21); settle();                                                        // ... and while it is fresh a stale 7-bit CC119 must not overwrite it
        if (fabs(clock_engine_get_bpm() - nb) > 0.001) { fails++; snprintf(why, sizeof why, "CC119 overrode a fresh tempo number (%.4f)", clock_engine_get_bpm()); }
        cc16(116, 1); settle(); double mb = clock_engine_get_bpm();                    // back to master: the bus' tempo messages are our own echo and must be ignored
        number(90.0); cc16(119, 60); settle();
        if (clock_engine_get_role() != CLOCK_ROLE_OPG_MASTER || fabs(clock_engine_get_bpm() - mb) > 0.001) { fails++; snprintf(why, sizeof why, "as master CC119/CC121 changed the tempo (%.4f -> %.4f)", mb, clock_engine_get_bpm()); }
        clock_engine_set_policy(1); cc16(116, 2); settle();                             // user pinned INTERNAL: no bus claim may take the clock
        if (clock_engine_get_role() != CLOCK_ROLE_OPG_MASTER) { fails++; snprintf(why, sizeof why, "INTERNAL policy accepted a bus claim"); }
        clock_engine_set_policy(0);
        printf("%-30s CC116 claim, CC119 7-bit tempo, CC120/121 number outranks CC119, master ignores echoes, INTERNAL pin refuses claim: %s%s\n", "AUTO-follow tempo CCs", fails == f0 ? "PASS" : "FAIL ", why);
    }
    {   // FREEWHEEL gap >= 2 s without a Start/SPP: the clock resumes with the tick count unknown. The snap must land on the tick that belongs to PMOR's own
        // 16th (the grid free-ran through the gap at the same tempo), not on the tick the old count happens to call a boundary (up to 5 ticks = 130 ms away)
        for (int gap_n : {104, 105, 107, 110}) {    // clearly > 2 s at 124 BPM (a bunched transfer straddling the cut eats a few more ticks); lost ticks mod 6 = 2, 3, 5, 2
            g_gap_at = 12 * 96 + 7; g_gap_n = gap_n; g_bars = 32; Result r = run(0.3, 0.1, 2024 + (uint64_t) gap_n, false); double mx = g_max_abs_ms; g_gap_at = -1;
            bool ok = r.n > 100 && mx < 1.5;
            printf("%-30s %d ticks (%.2f s) lost mid-stream, no Start/SPP: max |offset| of PMOR's 16ths vs the sender's %.3f ms  %s\n", "freewheel gap re-lock", gap_n, gap_n * 60.0 / (124.0 * 24.0), mx, ok ? "PASS" : "FAIL"); if (!ok) fails++;
        }
    }
    {   // FIRST-HIT recording on a followed grid: REC armed on an empty lane, the player's first hit at an arbitrary instant (not on a 16th) becomes
        // lane step 0, snapped to the nearest 16th of the FOLLOWED grid; every later loop cycle must hit the true 16th grid (no drift) over 32 bars
        for (double frac : {0.2, 0.45, 0.55, 0.8}) {
            const double P = 60.0 * 48000.0 / (124.0 * 24.0), S16 = 6.0 * P, T0 = 64.0 * 400.0 + 31.0;
            g_fh_press = T0 + 20.0 * S16 + frac * S16; Result r = run(0.5, 0.2, 31337, false); g_fh_press = -1;
            long jn = lround(20.0 + frac);   // nearest true 16th index of the press
            double want_t = T0 + (double) jn * S16; double worst = 0, spacing_worst = 0; size_t n = g_fh_fires.size();
            for (size_t i = 0; i < n; i++) { double d = fmod(g_fh_fires[i] - want_t, 16.0 * S16); if (d > 8.0 * S16) d -= 16.0 * S16; if (d < -8.0 * S16) d += 16.0 * S16; if (fabs(d) > worst) worst = fabs(d); if (i) { double sp = fabs(g_fh_fires[i] - g_fh_fires[i - 1] - 16.0 * S16); if (sp > spacing_worst) spacing_worst = sp; } }
            bool ok = r.n > 100 && n >= 28 && worst / 48.0 < 1.0 && spacing_worst / 48.0 < 1.5;
            printf("%-30s press %.2f 16ths past a boundary: lane fired %zu times in 32 bars, max |offset| from the true 16th %.3f ms, worst cycle-to-cycle error %.3f ms  %s\n", "first-hit follow", frac, n, worst / 48.0, spacing_worst / 48.0, ok ? "PASS" : "FAIL"); if (!ok) fails++;
        }
    }
    {   // long run: a host clock whose period differs from PMOR's sample clock by +/-200 ppm (5 minutes at 100 BPM): phase must stay locked, no creeping drift
        for (double ppm : {0.0, 200.0, -200.0}) {
            g_bpm_nom = 100.0; g_bars = 125; g_ppm = ppm; Result r = run(0.3, 0.1, 4242, false); double mx = g_max_abs_ms, dr = g_drift_ms; g_bpm_nom = 124.0; g_bars = 32; g_ppm = 0;
            bool ok = r.n > 1000 && mx < 1.0 && fabs(dr) < 0.3;
            printf("%-30s 100 BPM x 5 min, host clock %+4.0f ppm, jitter: %d boundaries, max |offset| %.3f ms, drift (last vs first fifth) %+.3f ms  %s\n", "long-run lock", ppm, r.n, mx, dr, ok ? "PASS" : "FAIL"); if (!ok) fails++;
        }
    }
    {   // cold Start (no pre-roll: Live sends F8 only while playing), 100 BPM against PMOR's 120 default: the first 16ths while the tempo is learned
        g_early_skip = 2; g_bpm_nom = 100.0; g_bars = 8; Result r = run(0.3, 0.1, 77, getenv("EARLY") != nullptr); double em = g_early_max_ms; int en = g_early_n; double e2 = g_early_2nd_ms; g_bpm_nom = 124.0; g_bars = 32; g_early_skip = 0; (void) r;
        bool ok = en >= 8 && em < 1.0;
        printf("%-30s cold Start @100 BPM vs 120 default: 2nd boundary %+.1f ms (tempo learned mid-step), boundaries 3.. max |offset| %.3f ms  %s\n", "cold start tempo learn", e2, em, ok ? "PASS" : "FAIL"); if (!ok) fails++;
    }
    {   // TEMPO seed: PMOR sat at 120, the host sends TEMPO 100000 before a cold Start at 100 (jitter 0.3 ms): every boundary from step 0 within 1 ms
        g_seed_pre = 100.0; g_early_skip = 1; g_bpm_nom = 100.0; g_bars = 8; Result r = run(0.3, 0.1, 77, getenv("EARLY") != nullptr); double em = g_early_max_ms; int en = g_early_n; double e1 = g_early_first_ms, e2 = g_early_2nd_ms;
        g_seed_pre = 0; g_early_skip = 0; g_bpm_nom = 124.0; g_bars = 32; (void) r;
        bool ok = en >= 8 && em < 1.0 && fabs(e2) < 1.0;
        printf("%-30s seeded cold Start @100 BPM vs 120: boundaries 2.. max |offset| %.3f ms (2nd %+.3f ms)  %s\n", "TEMPO seed, cold start", em, e2, ok ? "PASS" : "FAIL"); if (!ok) fails++; (void) e1;
        // TEMPO 101000 while streaming 100 (no jitter): no jump, stays locked to F8
        g_seed_mid = 101.0; g_bpm_nom = 100.0; g_bars = 32; Result r2 = run(0.0, 0.0, 5, false); double mx = g_max_abs_ms; double dr = g_drift_ms; g_seed_mid = 0; g_bpm_nom = 124.0; g_bars = 32;
        bool ok2 = r2.n > 100 && mx < 0.5;
        printf("%-30s TEMPO 101000 while streaming 100 BPM: max |offset| %.3f ms over the run, drift %+.3f ms  %s\n", "TEMPO seed, streaming", mx, dr, ok2 ? "PASS" : "FAIL"); if (!ok2) fails++;
    }
    printf("test_follower: %s\n", fails ? "FAILED" : "PASS");
    return fails ? 1 : 0;
}
