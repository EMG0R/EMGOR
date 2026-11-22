// test_daemon_tap.cpp -- BEHAVIOURAL test of the REAL demiurge-clock.cpp transport code
// (handle_tap / adopt_wire_tempo / broadcast_tempo / master_claim_by_tap), with ALSA stubbed
// and every MIDI send recorded. Host-only: no ALSA, no sockets bound, no audio.
//
// The daemon .cpp is #included, so what runs here is the shipping source, not a copy.
#include <alsa/asoundlib.h>
#include <vector>
#include <stdio.h>
#include <unistd.h>

struct Sent { int ch, cc, val; };
static std::vector<Sent> g_sent;
static int rec_output(snd_seq_t *, snd_seq_event_t *ev) {
    if (ev->type == SND_SEQ_EVENT_CONTROLLER || ev->data.control.param) {
        g_sent.push_back({ev->data.control.channel, (int)ev->data.control.param, ev->data.control.value});
    }
    return 0;
}
#define snd_seq_event_output_direct rec_output
#define main daemon_main
#include "../../src/demiurge-clock.cpp"
#undef main

static int fails = 0, passes = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int sent_cc(int cc) { int n = 0; for (auto &s : g_sent) if (s.cc == cc && s.ch == 15) n++; return n; }
static bool last_wire(double *bpm) {
    int msb = -1, lsb = -1;
    for (auto &s : g_sent) { if (s.ch != 15) continue; if (s.cc == 120) msb = s.val; if (s.cc == 121) lsb = s.val; }
    if (msb < 0 || lsb < 0) return false;
    *bpm = dt_wire14_to_bpm(dt_wire14_join((uint8_t)msb, (uint8_t)lsb));
    return true;
}
static void reset_taps() { dt_tap_reset(&g_tap); g_tap_count = 0; g_sent.clear(); }

int main() {
    clock_gettime(CLOCK_MONOTONIC, &g_time_epoch);
    g_osc_out_fd = -1;                                   // OSC publish is a no-op without a socket

    // ---- A. master role, PMOR slaved: two taps 500 ms apart -> 120 BPM, wire-canonical, broadcast at once
    g_role = ROLE_MASTER; g_bpm = 124.0; reset_taps();
    double ta = now_seconds();
    handle_tap(TAP_SRC_PMOR, 0.0);
    CHECK(g_tap_count == 1 && sent_cc(120) == 0, "first tap must only arm (count=%d, cc120=%d)", g_tap_count, sent_cc(120));
    usleep(500000);
    double tb = now_seconds();
    handle_tap(TAP_SRC_PMOR, 0.0);
    double w = 0;
    double want = 60.0 / (tb - ta);                       // the REAL spacing: usleep overshoots on a loaded host
    CHECK(last_wire(&w), "no CC120/121 after tempo tap");
    CHECK(fabs(w - want) < 0.1, "wire tempo %.3f vs real spacing %.3f BPM", w, want);
    CHECK(fabs(g_bpm - dt_bpm_canon(g_bpm)) < 1e-12, "daemon holds a non-canonical tempo %.6f (wire would decode to %.6f)", g_bpm, dt_bpm_canon(g_bpm));
    CHECK(fabs(g_bpm - w) < 1e-9, "daemon g_bpm %.6f != number it broadcast %.6f", g_bpm, w);
    CHECK(g_tempo_source == CLOCK_SRC_HUMAN_GESTURE && tempo_source_is_real(), "tap must register as rung-0 human gesture");
    CHECK(g_role == ROLE_MASTER && sent_cc(116) == 0, "a tap while already master must not touch the role");
    CHECK(sent_cc(119) >= 1, "legacy CC119 not sent alongside the number");
    printf("A  PMOR taps -> %.3f BPM broadcast as CC119 + CC120/121 immediately (g_bpm=%.3f)\n", w, g_bpm);

    // ---- E. a duplicated tap (the 3% USB duplicate) is ignored, not a 1 ms interval
    int before = g_tap_count;
    handle_tap(TAP_SRC_PMOR, 0.0);
    CHECK(g_tap_count == before, "duplicate tap counted");
    printf("E  duplicate tap ignored\n");

    // ---- F. the age field is honoured: arrival spacing 500 ms but the 2nd press happened 20 ms ago -> 480 ms -> 125 BPM
    reset_taps(); g_role = ROLE_MASTER;
    ta = now_seconds();
    handle_tap(TAP_SRC_NEPTR, 0.0);
    usleep(500000);
    tb = now_seconds();
    handle_tap(TAP_SRC_NEPTR, 20.0);
    want = 60.0 / ((tb - ta) - 0.020);
    CHECK(last_wire(&w) && fabs(w - want) < 0.1, "age not applied: %.3f want %.3f", w, want);
    printf("F  age compensation: 2nd press was 20 ms old -> %.3f BPM (expected %.3f; without the age it would be %.3f)\n", w, want, 60.0 / (tb - ta));

    // ---- B. FOLLOWER (PMOR is master): ONE NEPTR tap never takes the clock; a pair does
    reset_taps(); g_role = ROLE_FOLLOWER; g_bpm = 130.0;
    handle_tap(TAP_SRC_NEPTR, 0.0);
    CHECK(g_role == ROLE_FOLLOWER && sent_cc(116) == 0, "single stray tap stole the clock");
    usleep(450000);
    handle_tap(TAP_SRC_NEPTR, 0.0);
    CHECK(g_role == ROLE_MASTER, "tap pair on NEPTR did not claim master");
    bool claimed = false; for (auto &s : g_sent) if (s.ch == 15 && s.cc == 116 && s.val == 2) claimed = true;
    CHECK(claimed, "CC116=2 not announced on role claim");
    CHECK(last_wire(&w) && w > 130.0 && w < 136.5, "tempo after claim %.3f (450 ms spacing ~133)", w);
    printf("B  follower: 1 tap = nothing; tap pair -> CC116=2 + %.3f BPM\n", w);

    // ---- C. PMOR tap while we FOLLOW PMOR: ignored (PMOR analysed it; its number comes as CC120/121)
    reset_taps(); g_role = ROLE_FOLLOWER;
    handle_tap(TAP_SRC_PMOR, 0.0); usleep(300000); handle_tap(TAP_SRC_PMOR, 0.0);
    CHECK(g_tap_count == 0 && g_role == ROLE_FOLLOWER && sent_cc(120) == 0, "PMOR tap analysed twice (count=%d)", g_tap_count);
    printf("C  PMOR tap while following PMOR: ignored (no double analysis)\n");

    // ---- D. follower adopts the master's NUMBER, ignores it as master, PLL tempo is gated while the number is fresh
    g_role = ROLE_FOLLOWER; g_bpm = 100.0; g_wire_tempo_last_s = -1000.0;
    uint16_t wv = dt_bpm_to_wire14(137.25);
    adopt_wire_tempo(dt_wire14_msb(wv), dt_wire14_lsb(wv));
    CHECK(fabs(g_bpm - dt_bpm_canon(137.25)) < 1e-9 && wire_tempo_fresh() && g_tempo_source == CLOCK_SRC_EXTERNAL_MIDI,
          "follower did not adopt wire tempo (g_bpm=%.4f)", g_bpm);
    g_role = ROLE_MASTER; g_bpm = 99.0;
    adopt_wire_tempo(dt_wire14_msb(wv), dt_wire14_lsb(wv));
    CHECK(g_bpm == 99.0, "master adopted a bus tempo");
    printf("D  follower adopts 137.25 -> %.4f (canonical); master ignores bus tempo\n", dt_bpm_canon(137.25));

    printf("\n%d checks passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
