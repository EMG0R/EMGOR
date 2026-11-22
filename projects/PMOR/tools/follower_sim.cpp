// follower_sim.cpp -- measures the REAL MIDI-clock follower (usb/clock_engine.c, compiled unmodified) driving the REAL grid
// (pmor_core/clock.cpp) and the REAL control glue (hw/transport_link.cpp) against simulated Ableton-style input. No hardware.
// Build/run: bash tools/follower_sim.sh [--assert-baseline]
//
// What is real:    clock_engine.c (IRQ-stamp path dcd_rx_complete_hook -> tud_midi_rx_cb -> clock_engine_poll -> midi_rx_byte ->
//                  handle_realtime -> follower_phase_correct, ferr/iv stats, tempo estimator), clock.cpp, transport_link.cpp.
// What is stubbed: tusb.h (4-byte packet RX FIFO here; TX dropped), pmor_tick()/pmor_tick_hz() (virtual 200 MHz timer),
//                  board_get_now_ms(), daisy::System, dwc2 diag counters. The audio ISR is replayed in firmware order:
//                  clock_engine_note_audio_block(64) -> clockTick(s) -> clock_engine_process_audio in 48+16 chunks.
//                  Main loop = transportTick() + clock_engine_poll() every 0.25 ms of virtual time (no OLED/SD stalls).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <array>
#include <deque>
#include "daisy_seed.h"
#include "pmor_core/clock.h"
#include "pmor_core/shared_state.h"
#include "hw/transport_link.h"
extern "C" {
#include "usb/clock_engine.h"
#include "usb/audio_format.h"
#include "usb/usb_descriptors.h"
}
#include "tusb.h"

uint32_t daisy::System::s_now_ms = 0;
SharedState g;

// ------------------------------------------------------------------ virtual time + stubs
static double g_vt = 0.0;                                   // virtual seconds
static const uint32_t TICK_HZ = 200000000u;                 // Daisy TIM tick rate
extern "C" {
uint32_t pmor_tick(void)    { return (uint32_t)(uint64_t)llround(g_vt * (double)TICK_HZ); }
uint32_t pmor_tick_hz(void) { return TICK_HZ; }
uint32_t board_get_now_ms(void) { return (uint32_t)(g_vt * 1000.0); }
void pmor_probe_set(const char*, int) {}
void pmor_oled_test(int) {} void pmor_audio_ctl(int) {} void pmor_oled_contrast(int) {} void pmor_pb5_test(int) {} void pmor_sine_set(int) {}
uint64_t clock_engine_grid_step_q32(void) { return clockStepSamplesQ32(); }   // identical one-liner to hw/audio_callback.cpp
volatile uint32_t g_dwc2_midi_in_kicks = 0, g_dwc2_midi_in_kick_bytes = 0, g_dwc2_midi_in_fifo_pushes = 0, g_dwc2_midi_in_fifo_bytes = 0;

}
static std::deque<std::array<uint8_t,4>> g_fifo;
extern "C" {
bool     tud_midi_mounted(void) { return true; }
uint32_t tud_midi_available(void) { return (uint32_t)g_fifo.size() * 4u; }
bool     tud_midi_packet_read(uint8_t p[4]) { if (g_fifo.empty()) return false; memcpy(p, g_fifo.front().data(), 4); g_fifo.pop_front(); return true; }
bool     tud_midi_packet_write(uint8_t const*) { return true; }
uint32_t tud_midi_stream_write(uint8_t, uint8_t const*, uint32_t n) { return n; }
uint32_t tud_midi_n_stream_write(uint8_t, uint8_t, uint8_t const*, uint32_t n) { return n; }
uint32_t tud_cdc_write(void const*, uint32_t n) { return n; }
uint32_t tud_cdc_write_flush(void) { return 0; }
void     tud_task(void) {}
}

// ------------------------------------------------------------------ PRNG (fixed seed)
static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static double urand() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return ((g_rng >> 11) + 0.5) / 9007199254740992.0; }
static double grand() { return sqrt(-2.0 * log(urand())) * cos(6.283185307179586 * urand()); }

// ------------------------------------------------------------------ sender model
struct Ev { double ideal; double arr; uint8_t st; uint8_t d1, d2; bool tick; };
struct Sender {
    std::vector<Ev> ev;                        // sorted by arrival, order preserved
    std::vector<double> ideal16;               // ideal time of every 16th boundary (tick idx % 6 == 0 since Start/Continue)
    std::vector<std::pair<double,double>> bpm_seg; // (t_from, bpm) truth; ramp handled by fn
    std::vector<std::pair<double,double>> run;  // [start, stop) intervals while the sender is playing
    int   ramp = 0;  double ramp_t0 = 0;
    double bpm_at(double t) const { if (ramp) { double u = (t - ramp_t0) / 20.0; if (u < 0) u = 0; if (u > 1) u = 1; return 90.0 + 40.0 * u; } return bpm_seg[0].second; }
};
enum Jit { J_NONE, J_GAUSS, J_BURST };
// emit ticks [t0, t1) starting with `first` status (0xFA/0xFB); returns nothing, appends to s
static void emit_run(Sender& s, double t0, double t1, uint8_t first, double bpm0) {
    s.run.push_back({t0, t1});
    s.ev.push_back({t0, t0, first, 0, 0, false});
    double t = t0; int k = 0;
    while (t < t1) {
        double bpm = s.ramp ? s.bpm_at(t) : bpm0;
        s.ev.push_back({t, t, 0xF8, 0, 0, true});
        if (k % 6 == 0) s.ideal16.push_back(t);
        k++; t += 60.0 / (24.0 * bpm);
    }
}
static void apply_jitter(Sender& s, Jit j, double sigma_ms, double nominal_iv) {
    double prev = -1e9, stall_end = -1.0; int stall_j = 0;
    for (auto& e : s.ev) {
        double a = e.ideal;
        if (e.tick) {
            if (j == J_GAUSS) { a += grand() * sigma_ms * 1e-3; if (urand() < 0.02) a += 3e-3; }
            else if (j == J_BURST) {
                a += urand() * 4e-3;                                    // sender wake-up lateness 0..4 ms
                if (a >= stall_end && urand() < 0.10) { stall_end = a + (8.0 + 17.0 * urand()) * 1e-3; stall_j = 0; }   // stall: hold ticks, release as a burst
                if (a < stall_end) { a = stall_end + 0.04e-3 * stall_j; stall_j++; }
            }
        }
        if (a < prev) a = prev;                                         // order preserved
        e.arr = a; prev = a;
    }
    (void)nominal_iv;
}

// ------------------------------------------------------------------ simulation
struct Sample { double t; uint32_t fm, fp, fx, fn; double tmean, tp99, tmax, tbias; };
struct Result {
    Sample at[4]; double terr_mean, terr_max, lock_s; uint32_t ivmin_us, ivmax_us; bool ok;
};
static const double SAMPLE_T[4] = { 5, 15, 30, 60 };

struct Fire { double t; double err; double bpm_err_rel; };

static void deliver(uint8_t st, uint8_t d1, uint8_t d2, bool nbytes3, std::vector<std::array<uint8_t,4>>& pk) {
    std::array<uint8_t,4> p;
    if (nbytes3) { p = { 0x03, st, d1, d2 }; } else p = { 0x0F, st, 0, 0 };
    pk.push_back(p);
}

static Result simulate(Sender& s, double t_end, double first_start_t) {
    Result R; memset(&R, 0, sizeof R);
    // ---- fresh firmware state, STUDIO follow exactly as hw/studio.cpp applyMode() does
    g_vt = 0; daisy::System::s_now_ms = 0; g_fifo.clear();
    clock_engine_init();
    clockInit(120.f);
    clock_engine_set_policy(0);
    clock_engine_set_studio(true);
    clock_engine_ferr_reset();
    uint32_t a, b; clock_engine_iv_stats(&a, &b);
    uint32_t S = 0;                                         // render.cpp s_sample_now (clock domain), block-start sample
    const double BLK = 64.0 / 48000.0, POLL = 0.25e-3;
    double tb = 0, tp = 0; size_t ia = 0;
    std::vector<Fire> fires; int si = 0;
    uint32_t ivmin = 0xFFFFFFFFu, ivmax = 0;
    double bsum = 0, bmax = 0; long bn = 0; double next_bpm_t = 0.0; double last_start = -1;
    std::vector<double> run_starts; for (auto& r : s.run) run_starts.push_back(r.first);
    // fresh-lock detection helper data is computed after the run from `fires`
    auto running_at = [&](double t) { for (auto& r : s.run) if (t >= r.first && t < r.second) return true; return false; };
    auto truth_bpm = [&](double t) { return s.bpm_at(t); };
    auto true_err = [&](double fire_t) -> double {
        auto it = std::lower_bound(s.ideal16.begin(), s.ideal16.end(), fire_t);
        double best = 1e9; if (it != s.ideal16.end()) best = *it - fire_t;
        if (it != s.ideal16.begin()) { double d = *(it - 1) - fire_t; if (fabs(d) < fabs(best)) best = d; }
        return -best;                                         // + = PMOR fires AFTER the sender's boundary
    };
    auto stats_of = [&](double t, double& mean, double& p99, double& mx, double& bias) {
        std::vector<double> v, sg; for (int i = (int)fires.size() - 1; i >= 0 && (int)v.size() < 256; i--) { if (fires[i].t > t) continue; v.push_back(fabs(fires[i].err)); sg.push_back(fires[i].err); }
        if (v.empty()) { mean = p99 = mx = bias = 0; return; }
        double sum = 0, sb = 0; for (double x : v) sum += x; for (double x : sg) sb += x; std::sort(v.begin(), v.end());
        mean = sum / v.size(); bias = sb / sg.size(); mx = v.back(); size_t i99 = v.size() * 99 / 100; if (i99 >= v.size()) i99 = v.size() - 1; p99 = v[i99];
    };
    while (g_vt < t_end + 1e-9) {
        double ta = ia < s.ev.size() ? s.ev[ia].arr : 1e18;
        double tn = std::min(std::min(tb, tp), ta);
        g_vt = tn; daisy::System::s_now_ms = board_get_now_ms();
        if (tn > t_end) break;
        if (tn == ta) {
            // one USB transfer = every event whose arrival is within 10 us of this one; IRQ stamp = now
            std::vector<std::array<uint8_t,4>> pk; double tt = ta;
            while (ia < s.ev.size() && s.ev[ia].arr - tt < 10e-6 + 1e-12 && s.ev[ia].arr <= t_end) { tt = s.ev[ia].arr; auto& e = s.ev[ia]; deliver(e.st, e.d1, e.d2, e.st == 0xF2, pk); ia++; }
            if (pk.empty()) { ia = s.ev.size(); continue; }
            g_vt = tt;
            for (auto& p : pk) g_fifo.push_back(p);
            dcd_rx_complete_hook(EPNUM_MIDI);
            tud_midi_rx_cb(0);
        } else if (tn == tb) {
            clock_engine_note_audio_block(64);
            uint32_t nsw = clk.next_step_whole; uint32_t nsf = clk.next_step_frac;
            int fired = clockTick(S);
            if (fired && running_at(g_vt)) { double bt = ((double)nsw + nsf / 4294967296.0) / 48000.0; fires.push_back({ bt, true_err(bt), fabs((double)clock_engine_get_bpm() - truth_bpm(g_vt)) / truth_bpm(g_vt) }); }
            S += 64;
            static float inb[64], l[64], r[64]; const float* ip[2] = { inb, inb }; float* op[2] = { l, r };
            clock_engine_process_audio(ip, op, 48);
            ip[0] = ip[1] = inb + 48; op[0] = l + 48; op[1] = r + 48;
            clock_engine_process_audio(ip, op, 16);
            tb += BLK;
        } else {
            transportTick(); clock_engine_poll(); tp += POLL;
        }
        // periodic measurements
        for (auto& st : run_starts) if (g_vt >= st && st > last_start) last_start = st;
        if (g_vt >= next_bpm_t) {
            next_bpm_t += 0.05;
            if (running_at(g_vt) && g_vt - last_start >= 3.0) {
                double e = fabs((double)clock_engine_get_bpm() - truth_bpm(g_vt)); bsum += e; bn++; if (e > bmax) bmax = e;
            }
        }
        if (si < 4 && g_vt - first_start_t >= SAMPLE_T[si]) {
            Sample& sm = R.at[si]; sm.t = SAMPLE_T[si];
            sm.fn = clock_engine_ferr_stats(&sm.fm, &sm.fp, &sm.fx);
            uint32_t mn, mx; clock_engine_iv_stats(&mn, &mx); if (mx) { if (mn < ivmin) ivmin = mn; if (mx > ivmax) ivmax = mx; }
            double m, p, x, bi; stats_of(g_vt, m, p, x, bi); sm.tmean = m; sm.tp99 = p; sm.tmax = x; sm.tbias = bi;
            si++;
        }
    }
    R.terr_mean = bn ? bsum / bn : 0; R.terr_max = bmax;
    R.ivmin_us = ivmin == 0xFFFFFFFFu ? 0 : ivmin; R.ivmax_us = ivmax;
    // time to first stable lock: first fire after the first Start such that it and the next 7 fires have a true-error SPREAD (max-min) < 2 ms
    // (a constant offset from the sender is bias, reported separately) and the tempo estimate is within 1 % of the sender's
    R.lock_s = -1;
    for (size_t i = 0; i + 8 <= fires.size(); i++) {
        if (fires[i].t < first_start_t) continue;
        double lo = 1e9, hi = -1e9; bool ok = true;
        for (size_t j = i; j < i + 8; j++) { lo = std::min(lo, fires[j].err); hi = std::max(hi, fires[j].err); if (fires[j].bpm_err_rel > 0.01) ok = false; }
        if (ok && hi - lo < 2e-3) { R.lock_s = fires[i].t - first_start_t; break; }
    }
    R.ok = true; return R;
}

// ------------------------------------------------------------------ scenarios
struct Scn { char name[40]; double bpm; int kind; double sigma; };
static Result run_scn_inproc(const Scn& c) {
    Sender s; s.bpm_seg.push_back({0, c.bpm});
    const double T0 = 0.5;                                  // Start arrives after 0.5 s of idle follow mode
    double dur = 60.6;
    Jit j = J_NONE;
    switch (c.kind) {
    case 1: j = J_NONE; emit_run(s, T0, T0 + dur, 0xFA, c.bpm); break;
    case 2: j = J_GAUSS; emit_run(s, T0, T0 + dur, 0xFA, c.bpm); break;
    case 3: j = J_BURST; emit_run(s, T0, T0 + dur, 0xFA, c.bpm); break;
    case 4: j = J_NONE; s.ramp = 1; s.ramp_t0 = T0; emit_run(s, T0, T0 + dur, 0xFA, 90); break;
    case 5: {   // Start, Stop @15.5, Start @18, Stop @35, SPP 64 @36, Continue @37 -> 60.6
        j = J_NONE;
        emit_run(s, T0, T0 + 15.5, 0xFA, c.bpm); s.ev.push_back({T0 + 15.6, T0 + 15.6, 0xFC, 0, 0, false});
        emit_run(s, T0 + 18.0, T0 + 35.0, 0xFA, c.bpm); s.ev.push_back({T0 + 35.1, T0 + 35.1, 0xFC, 0, 0, false});
        s.ev.push_back({T0 + 36.0, T0 + 36.0, 0xF2, 64 & 0x7F, 64 >> 7, false});
        emit_run(s, T0 + 37.0, T0 + dur, 0xFB, c.bpm);
        std::stable_sort(s.ev.begin(), s.ev.end(), [](const Ev& a, const Ev& b) { return a.ideal < b.ideal; });
        // the Stop events were pushed with their run: fix run intervals to end at the Stop
        break; }
    }
    apply_jitter(s, j, c.sigma, 60.0 / (24.0 * c.bpm));
    return simulate(s, T0 + dur, T0);
}

// clock_engine.c keeps all its state in file statics with no reset, so every scenario runs in a fresh forked process (cold boot each time).
static Result run_scn(const Scn& c) {
    int fd[2]; if (pipe(fd)) { perror("pipe"); exit(2); }
    pid_t pid = fork(); if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) { g_rng = 0x9E3779B97F4A7C15ull; Result r = run_scn_inproc(c); if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r) _exit(3); _exit(0); }
    Result r; memset(&r, 0, sizeof r); ssize_t n = read(fd[0], &r, sizeof r); int st = 0; waitpid(pid, &st, 0); close(fd[0]); close(fd[1]);
    if (n != (ssize_t)sizeof r) { fprintf(stderr, "harness failure: scenario child died\n"); exit(2); }
    return r;
}

int main(int argc, char** argv) {
    bool assert_base = argc > 1 && !strcmp(argv[1], "--assert-baseline");
    std::vector<Scn> sc;
    const double bp[4] = { 80, 100, 124, 140 };   // 124 = the hardware test tempo (STUDIO_STATUS baseline 0.31 / 1.02 ms mean / p99)
    char nm[40];
    if (assert_base) { Scn c = { "2 jitter s=0.5", 100, 2, 0.5 }; sc.push_back(c); }
    else {
        for (double b : bp) { snprintf(nm, sizeof nm, "1 clean"); Scn c; strcpy(c.name, nm); c.bpm = b; c.kind = 1; c.sigma = 0; sc.push_back(c); }
        const double sg[3] = { 0.2, 0.5, 1.0 };
        for (double x : sg) for (double b : bp) { Scn c; snprintf(c.name, sizeof c.name, "2 jitter s=%.1f", x); c.bpm = b; c.kind = 2; c.sigma = x; sc.push_back(c); }
        for (double b : bp) { Scn c; strcpy(c.name, "3 alsa-burst"); c.bpm = b; c.kind = 3; c.sigma = 0; sc.push_back(c); }
        { Scn c; strcpy(c.name, "4 ramp 90>130/20s"); c.bpm = 90; c.kind = 4; c.sigma = 0; sc.push_back(c); }
        for (double b : bp) { Scn c; strcpy(c.name, "5 stop/start/SPP"); c.bpm = b; c.kind = 5; c.sigma = 0; sc.push_back(c); }
    }
    printf("follower_sim: real usb/clock_engine.c + clock.cpp + transport_link.cpp, STUDIO follow, 48 kHz block 64, seed fixed\n");
    printf("ferr = firmware STATUS ferr_us (mean/p99/max over last<=256 corrections, ms); T = TRUE |PMOR 16th - sender ideal 16th| over last 256 fires (ms)\n\n");
    printf("%-18s %4s | %-19s | %-19s | %-19s | %-19s | %-9s %-6s | %-13s\n", "scenario", "bpm", "ferr@5s m/p99/max", "ferr@15s", "ferr@30s", "ferr@60s", "tempoErr", "lock_s", "iv_ms min/max");
    Result last{}; std::vector<Result> all;
    for (auto& c : sc) {
        Result r = run_scn(c); all.push_back(r); last = r;
        printf("%-18s %4.0f |", c.name, c.bpm);
        for (int i = 0; i < 4; i++) printf(" %5.2f/%5.2f/%5.2f |", r.at[i].fm / 1000.0, r.at[i].fp / 1000.0, r.at[i].fx / 1000.0);
        printf(" %.2f/%.2f", r.terr_mean, r.terr_max);
        if (r.lock_s >= 0) printf(" %6.2f |", r.lock_s); else printf("  never |");
        printf(" %5.2f/%5.2f\n", r.ivmin_us / 1000.0, r.ivmax_us / 1000.0);
    }
    if (!assert_base) {
        printf("\nTRUE phase error vs the sender's ideal boundaries (ms): mean/p99/max, signed bias\n");
        printf("%-18s %4s | %-26s | %-26s | %-26s | %-26s\n", "scenario", "bpm", "T@5s", "T@15s", "T@30s", "T@60s");
        for (size_t k = 0; k < sc.size(); k++) {
            printf("%-18s %4.0f |", sc[k].name, sc[k].bpm);
            for (int i = 0; i < 4; i++) printf(" %5.2f/%5.2f/%5.2f %+6.2f |", all[k].at[i].tmean * 1e3, all[k].at[i].tp99 * 1e3, all[k].at[i].tmax * 1e3, all[k].at[i].tbias * 1e3);
            printf("\n");
        }
    }
    if (assert_base) {
        double p99_ms = last.at[3].fp / 1000.0;
        printf("assert-baseline: scenario 2 sigma=0.5 @100 BPM ferr p99 @60s = %.3f ms (threshold 5.000 ms)\n", p99_ms);
        if (last.at[3].fp > 5000u) { printf("FAIL\n"); return 1; }
        printf("PASS\n");
    }
    return 0;
}
