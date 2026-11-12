// src/demiurge-clock.cpp
//
// DEMIURGE global clock source.
//
// A core system service — not an example. Emits MIDI realtime clock
// (0xF8 @ 24 PPQN) and transport messages (0xFA start / 0xFC stop) on
// an ALSA seq port that gets aconnected to "Midi Through" — the shared
// DEMIURGE MIDI bus every language already listens on.
//
// Every program in the chain gets the same tempo grid for free. All
// clock CCs live on MIDI channel 16, tucked away at the top of the CC
// space so they don't collide with musical CCs:
//
//   ch16 CC119 — BPM broadcast (READ). Clock writes this once per beat.
//                Every program reads it to stay in tempo.
//   ch16 CC118 — BPM steer     (WRITE). Any program writing this on the
//                bus retunes the clock. The clock echoes the new value
//                on CC119 next beat so the whole chain follows.
//   ch16 CC117 — transport     (>=64 = start, <64 = stop).
//   ch16 CC116 — role          (1 = this daemon becomes FOLLOWER, i.e. an
//                external device such as the Pocket OpGorator is master;
//                2 = this daemon becomes MASTER). The daemon re-announces
//                its current role on CC116 every ~2s so a late-plugged
//                device converges.
//   ch16 CC114 — tempo-source CLASS (READ). 127 = the CURRENT bpm/CC119 value
//                comes from a REAL driver -- an external MIDI clock, an
//                actually-applied Ableton Link tempo, a CC118 steer, or a
//                human tap-tempo gesture (see the tempo-source ladder below).
//                0 = tempo is only the live.conf/startup default; nobody is
//                actually driving it. This is DELIBERATELY independent of
//                CC116 (role): once PMOR stopped claiming the clock while
//                silent, this daemon holding MASTER (CC116=2) became its
//                NORMAL resting state, so CC116 alone can no longer answer
//                "is there a real grid" -- see g_tempo_source / CLOCK_SRC_*
//                below, which is the SAME ladder state that already decides
//                g_bpm and the clock_status.conf `source =` line; CC114 is
//                just that verdict put on the bus so non-C++ consumers (e.g.
//                midi_handler.orc) can gate on it without re-deriving it.
//                Sent change-triggered and in the existing ~2s CC116
//                re-announce.
//
// 0..127 maps to 40..400 BPM for both CC119 and CC118.
//
// TRANSPORT / TAP (2026-10-05, bathtub realizations section 2) -- the same clock, not a second one:
//   ch16 CC120 + CC121 -- tempo as a 14-bit number (MSB/LSB), the CANONICAL wire value
//                (see demiurge-transport.h and docs/clock.md "Tempo is a number"). Sent with
//                every CC119 and immediately on a tap. A follower adopts it instead of the
//                PLL's jittery interval average; the PLL keeps doing phase.
//   ch16 CC112 -- a tap on PMOR (value = age in ms since the press, ISR-stamped on the Daisy).
//                PMOR only sends it while it is SLAVED; when it is master it analyses locally.
//   ch16 CC113 -- a tap on the NEPTR arcade button (value = age in ms).
//   OSC /transport/tap ,if (source, age_ms) on UDP 127.0.0.1:9100 -- the same event from any
//                other pool source. Every tap, from any path, is republished to the pool on
//                127.0.0.1:9101 together with /transport/tempo ,f  /transport/bar ,i
//                /transport/start /transport/stop.
//   Tap analysis happens HERE, once, with the shared analyzer in demiurge-transport.h (the
//   same header is compiled into the PMOR firmware for standalone use). A tap does NOT move
//   the daemon's phase: tempo authority and phase authority are different things.
//
// Follower mode: when role == follower, incoming SND_SEQ_EVENT_CLOCK
// (0xF8) ticks from another device on the bus are PLL-averaged over a
// sliding window (>=24 ticks) to recover BPM + phase; the tick thread
// locks its own emission to that recovered tempo/phase and keeps
// re-broadcasting 0xF8/CC119, so downstream consumers (looper grid,
// csound, etc.) don't need to change. Incoming START/STOP/CONTINUE map
// onto the same transport handling as CC117. If no external tick
// arrives for >2s, the daemon freewheels at the last recovered tempo;
// if NO follower activity (tick or fresh role-claim) has been seen for
// FOLLOWER_RECLAIM_TIMEOUT_S (5s), the daemon reclaims MASTER on its own
// rather than sitting in follower indefinitely — see that constant's
// comment for why 5s and why this matters even though freewheel alone
// keeps audio flowing.
// An echo-only tick stream never refreshes the reclaim backstop (the echo is
// dropped before follower_on_clock_tick), so a follower hearing nothing but
// itself reclaims MASTER after FOLLOWER_RECLAIM_TIMEOUT_S.
// Role persists in ~/demiurge/sync.conf (`role = opgorator|demiurge`),
// read once at startup; a `--role=master|follower` CLI flag overrides
// the file. Runtime role flips arrive live as CC116.
//
// SELF-ECHO IMMUNITY (2026-10-03): this daemon sends to Midi Through (14:0)
// AND listens on 14:0. Midi Through re-broadcasts every event to every
// subscriber INCLUDING the sender and STRIPS the source (everything reads as
// from 14:0), so the from_self (source.client) test never fires. In follower
// mode the daemon recovered tempo from its own echoed ticks: positive
// feedback, BPM walked 120 -> 40 (floor) and 150 -> 300 (ceiling). Fix: every
// 0xF8/START/CONTINUE/STOP/CC116 we SEND is stamped; the first matching event
// received within SELF_ECHO_WINDOW_S of that stamp is our own echo and is
// dropped (one drop per send, so a real tick can't be eaten twice). PMOR's
// clock_engine.c does the same. Also: in role=master received 0xF8 is NEVER
// used for tempo (tempo = live.conf bpm / Link / CC118 only), and a
// follower->master flip re-seeds g_bpm from the last master-side tempo
// instead of freezing whatever the PLL had recovered.
//
// The PLL only accepts tick-to-tick intervals that are physically
// possible at 24 PPQN between 20 and 400 BPM (6.25ms..125ms) -- this
// floor is what rejects PMOR's known ~3% duplicate-0xF8 firmware bug
// (duplicates land 0.3-1.5ms after the real tick, well under the
// floor) without letting a rejected duplicate re-anchor the next
// interval. A gap past the ceiling re-anchors instead of wedging.
// Recovered BPM comes from a trimmed mean (drops the window's min/max)
// rather than a plain mean, so one surviving bad sample can't skew
// tempo the way it used to. See demiurge-clock-pll.h for the algorithm
// (shared byte-for-byte with the macOS test harness in testing/clock/)
// and firmware-tinyusb/src/clock_engine.c for the firmware's matching
// gate.
//
// Tempo-source priority ladder: when more than one thing asks for a
// tempo change, a deliberate HUMAN_GESTURE (PMOR tap tempo, detected as
// a CC118 landing right after a CC116=2 role-claim) beats EXTERNAL_MIDI
// (this PLL), which beats CC118 steer, which beats Ableton Link, which
// beats the startup `--bpm`/live.conf value. A lower-priority source is
// ignored while a higher one has been heard within the last 2.0s
// (matching chase_bliss_clock.orc's own local arbiter window) — except
// HUMAN_GESTURE, which is never blocked by anything. This is a safety
// net on top of, not a replacement for, chase_bliss_clock.orc's existing
// CC118 self-gating. See demiurge-clock-pll.h and docs/clock.md for the
// full ladder.
//
// Transport-role status export: this daemon also writes
// ~/demiurge/clock_status.conf every ~2s with PMOR's own transport state
// (from inbound CC117) and a loop-activity proxy (from CC118-steer
// recency), so demiurge-launcher-rs's clockrole.rs can make a
// transport-aware role decision instead of handing off master on mere
// USB presence. See the "Transport-role status export" comment near
// g_pmor_transport_playing below, and clockrole.rs itself.
//
// Ableton Link integration:
//
// If the master config /boot/firmware/demiurge.conf contains a line
//
//   link = on
//
// the clock participates in an Ableton Link session. Link becomes the
// outer authority: network tempo and transport decisions propagate into
// g_bpm / g_playing, and the MIDI-realtime / CC119 / CC117 broadcast
// layer still fans out to every language on the bus as usual. CC118 steer
// writes from languages update Link's tempo, and Link echoes it back out
// to the rest of the session.
//
// If link is off (default), the clock behaves exactly as before — a
// standalone master emitting MIDI realtime + CC broadcasts.
//
// Build (on Pi):
//   With Link:    g++ -std=c++17 -O2 -DDEMIURGE_LINK -o demiurge-clock demiurge-clock.cpp -lasound -lpthread
//   Without:      g++ -O2 -o demiurge-clock demiurge-clock.cpp -lasound -lpthread
//
// See docs/CLOCK.md for the full protocol + per-language sync recipes.

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "demiurge-clock-pll.h"
#include "demiurge-transport.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef DEMIURGE_LINK
#include <ableton/Link.hpp>
#include <chrono>
#endif

static snd_seq_t *seq = nullptr;
static int out_port = -1;
static int in_port  = -1;
static int g_my_client_id = -1;

static volatile double g_bpm     = 124.0;
static volatile int    g_playing = 1;
static volatile sig_atomic_t running = 1;
static int g_link_enabled = 0;
static const char *BOOT_CONFIG = "/boot/firmware/demiurge.conf";

// --- Role / follower-mode state --------------------------------------
enum { ROLE_MASTER = 0, ROLE_FOLLOWER = 1 };
static volatile int g_role = ROLE_MASTER;

// How long a FOLLOWER can go without a legitimate external tick before
// the daemon reclaims MASTER on its own, rather than sitting in
// follower forever. This is a SAFETY BACKSTOP, not the primary
// role-decision path -- clockrole.rs (the launcher) is the authoritative,
// persisted decision (it writes sync.conf via demiurge-sync and polls
// transport state every ~2s heartbeat), but that path depends on the
// launcher process being alive and its poll cadence landing. The daemon
// must never depend on an external process to stop sitting silent/stuck
// in follower -- same philosophy as the existing tick-freewheel behavior
// (follower mode already keeps emitting at the last recovered tempo with
// zero outside help if ticks stop).
//
// Why reclaiming matters even though freewheel already keeps audio
// flowing: role also gates the CC116 re-announce value (every ~2s) that
// chase_bliss_clock.orc and any other CC118 sender read to decide whether
// DEMIURGE is authoritative. If role never reclaims, a vanished PMOR
// leaves the daemon broadcasting "follower" forever, so a NEPTR loop can
// never regain master even though the device that was supposed to be
// driving the bus is gone for good.
//
// 5.0s, not the PLL's own 2.0s freewheel/recency window: reclaiming is a
// bigger action than freewheeling (it flips CC116 for the whole bus), so
// it should tolerate a brief hiccup (a dropped USB frame, a 2-3s gap)
// without flapping the role back and forth. 5s is long enough to clearly
// distinguish "PMOR is gone" from "PMOR's clock momentarily stuttered",
// short enough that a performance is never left in an unrecoverable
// follower state for long.
#define FOLLOWER_RECLAIM_TIMEOUT_S 5.0

// Last time this device heard a LEGITIMATE (non-duplicate) external tick
// or an explicit role-claim while in follower mode, in now_seconds()
// units. Refreshed by follower_on_clock_tick() and by entering follower
// mode itself (so a fresh follower assignment gets the full timeout to
// prove itself before being reclaimed, rather than starting from 0 and
// looking instantly stale the next time now_seconds() is read).
// input_thread-only write (role-flip path and the tick path both run on
// that thread), tick_thread-only read (the reclaim check) -- same
// lock-free, best-effort convention as the rest of this file's shared
// state.
static volatile double g_follower_last_activity_s = 0.0;

// PLL tick-recovery state (written by input_thread, read by tick_thread;
// lock-free best-effort, same convention as g_bpm/g_playing above). The
// actual gate/window/averaging algorithm lives in demiurge-clock-pll.h
// so it can be shared byte-for-byte with the macOS test harness in
// testing/clock/ -- see that header for FIX 1 (duplicate-tick rejection
// + outlier-resistant averaging).
static clock_pll_state_t g_pll;
static struct timespec g_last_ext_tick;
static volatile int g_have_ext_tick = 0;
// Plain (non-volatile) struct, same convention as g_last_ext_tick above:
// input_thread writes it, tick_thread only reads it after observing
// g_phase_pending go to 1 (the volatile flag is the actual handshake).
static struct timespec g_phase_anchor;
static volatile int g_phase_pending = 0;

// --- Tempo-source priority ladder (FIX 2) -----------------------------
// See demiurge-clock-pll.h for the ladder rules. g_source_last_seen is
// written by whichever thread owns that source (input_thread for
// EXTERNAL_MIDI/CC118, tick_thread for LINK) and read by both when
// deciding whether a lower-priority source may apply -- same best-effort
// lock-free convention as the rest of this file's shared state.
static double g_source_last_seen[CLOCK_SRC_COUNT];
static volatile int g_tempo_source = CLOCK_SRC_LIVE_CONF_BPM;

// True iff g_tempo_source currently names a REAL driver rather than the
// live.conf/startup default -- this is the one place that question gets
// answered, reused by both the CC114 broadcast (tick_thread) and the
// clock_status.conf `tempo_source_real =` line (write_status_file) so
// there is exactly one notion of "is tempo really being driven", not two.
// CLOCK_SRC_LIVE_CONF_BPM is the ladder's lowest-priority, default-only
// entry (see demiurge-clock-pll.h); everything else (HUMAN_GESTURE,
// EXTERNAL_MIDI, CC118_STEER, LINK) only ever becomes g_tempo_source when
// that source actually applied a tempo -- for LINK specifically, see the
// g_tempo_source = CLOCK_SRC_LINK assignment in tick_thread: it is gated by
// clock_tempo_source_may_apply() AND only reached when the Link session's
// tempo differs from g_bpm, i.e. a Link peer is actually driving tempo, not
// merely "link = on" with the session sitting on the default. So this
// function needs no separate Link-peer-count check of its own.
static inline int tempo_source_is_real(void) {
    return g_tempo_source != CLOCK_SRC_LIVE_CONF_BPM;
}

// --- Transport-role status export (for clockrole.rs) -------------------
//
// clockrole.rs (the launcher) needs to know, cheaply and on every ~2s
// heartbeat, whether PMOR is actually SOUNDING right now and whether
// NEPTR has a loop actively driving tempo -- not just "is a USB device
// plugged in". Re-probing the MIDI bus from Rust (shelling to aseqdump
// like demiurge-sync's probe_clock does) is slow (it blocks for a
// multi-second capture window) and would duplicate state this daemon
// already tracks live. Instead the daemon writes a tiny status file
// every ~2s (piggybacking on the existing role/BPM status line in
// tick_thread) that clockrole.rs just reads -- same pattern as
// sync.conf/live.conf, one writer, cheap readers.
//
// Two independent signals, both input_thread-only writes (both CC117 and
// CC118 arrive there), tick_thread-only reads (for the status-file
// write) -- same lock-free convention as the rest of this file:
//
//   - g_pmor_transport_playing / g_pmor_transport_last_seen_s: PMOR's own
//     transport state, from inbound ch16 CC117 (>=64 start, <64 stop).
//     NEPTR never writes CC117 (confirmed: no CC117 send anywhere under
//     neptr/csound/ as of this change) -- only PMOR is expected to, once
//     its firmware grows real sounding/silent edge announcements, which
//     is what makes an inbound CC117 an unambiguous PMOR-transport signal
//     rather than something DEMIURGE has to guess the sender of.
//   - g_loop_active_last_seen_s: a proxy for "NEPTR has a loop actively
//     driving tempo", reusing g_source_last_seen[CLOCK_SRC_CC118_STEER]
//     (the existing ladder's own recency tracking for ROUTINE CC118
//     writes -- gesture CC118s are tracked separately under
//     CLOCK_SRC_HUMAN_GESTURE and deliberately excluded from this proxy).
//     This works because chase_bliss_clock.orc already self-gates: it
//     only writes CC118 while ITS OWN local arbiter believes the loop is
//     authoritative (see demiurge-clock-pll.h's ladder comment), so
//     "a routine CC118 arrived recently" already means "a loop is
//     running and driving tempo" without NEPTR needing a new dedicated
//     signal of its own.
// External-clock EVIDENCE for clockrole.rs (`ext_clock =` in the status file):
// non-echo 0xF8 arrivals, tracked in EVERY role (before the master/follower
// split) so a master can tell "PMOR is really emitting clock" from "PMOR is
// just plugged in / announcing CC117". A streak of >= EXT_CLOCK_MIN_STREAK
// ticks (one beat) with gaps < EXT_CLOCK_GAP_S, last seen within
// ROLE_SIGNAL_FRESH_S. input_thread-only writes, tick_thread read.
#define EXT_CLOCK_GAP_S 0.25
#define EXT_CLOCK_MIN_STREAK 24
static volatile double g_ext_clock_last_s = -1000.0;
static volatile int    g_ext_clock_streak = 0;
static volatile int    g_pmor_transport_playing      = 0;
static volatile double g_pmor_transport_last_seen_s  = -1000.0;

// ch16 CC115 — NEPTR's own "I hold a loop" announce (1 = at least one looper
// channel holds audio). Added 2026-10-01: loop_active used to be INFERRED from
// routine-CC118 recency, but chase_bliss_clock.orc self-gates those writes to
// only fire while it believes the loop owns tempo -- so the proxy went dark the
// instant PMOR took master, which is precisely when the election needs to know
// a loop is still running. CC115 is sent UNGATED by clock authority, so it is a
// fact rather than an inference. The CC118 proxy is kept as a fallback for a
// NEPTR too old to emit CC115.
static volatile int    g_loop_held                   = 0;
static volatile double g_loop_held_last_seen_s       = -1000.0;
// Window within which a transport/loop signal counts as "known" rather
// than "no information yet" -- matches FOLLOWER_RECLAIM_TIMEOUT_S so the
// daemon's own reclaim decision and the status file clockrole.rs reads
// agree on what "stale" means.
#define ROLE_SIGNAL_FRESH_S FOLLOWER_RECLAIM_TIMEOUT_S

static const char *CLOCK_STATUS_FILENAME = "clock_status.conf";

// Timestamp (now_seconds()) of the last inbound (non-self) CC116=2
// ("I'm claiming master") event, regardless of whether it actually
// changed g_role -- a repeated tap sends CC116=2 every time even if
// we're already master. input_thread-only: both the CC116 and CC118
// branches that touch this live in input_thread's single-threaded
// event loop, so no lock-free convention is needed here (unlike the
// g_* globals above, which are genuinely written from one thread and
// read from another).
static double g_last_cc116_claim_master_s = -1000.0;

// Monotonic seconds since daemon start. Using an epoch captured at
// startup (rather than raw CLOCK_MONOTONIC, which can already be a large
// number of seconds since boot) keeps double precision comfortably far
// below the millisecond scale the PLL cares about, indefinitely.
static struct timespec g_time_epoch;
static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)(ts.tv_sec - g_time_epoch.tv_sec) +
           (double)(ts.tv_nsec - g_time_epoch.tv_nsec) / 1e9;
}

#ifdef DEMIURGE_LINK
// Created in main() if link is enabled. Never touched after construction.
static ableton::Link *g_link = nullptr;
#endif

// Poor man's config parser — picks out `key = value` lines. Ignores
// comments and whitespace. Returns 0 if key not present.
static int parse_bool_key(const char *path, const char *key) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    int result = 0;
    size_t keylen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, key, keylen) != 0) continue;
        p += keylen;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, "on", 2) == 0 ||
            strncasecmp(p, "true", 4) == 0 ||
            strncasecmp(p, "yes", 3) == 0 ||
            strncasecmp(p, "1", 1) == 0) {
            result = 1;
        }
        break;
    }
    fclose(f);
    return result;
}

// Reads `role = opgorator|demiurge` from ~/demiurge/sync.conf. Returns
// ROLE_FOLLOWER for "opgorator" (the OpGorator is master, we follow),
// ROLE_MASTER for "demiurge", or -1 if the key/file is missing (caller
// decides the default in that case).
static int parse_role_key(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    int result = -1;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "role", 4) != 0) continue;
        p += 4;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, "opgorator", 9) == 0) result = ROLE_FOLLOWER;
        else if (strncasecmp(p, "demiurge", 8) == 0) result = ROLE_MASTER;
        break;
    }
    fclose(f);
    return result;
}

// --- Self-echo filter ----------------------------------------------------
// Midi Through delivers an event to all subscribers in-kernel, synchronously,
// so our own echo arrives within tens to a few hundred microseconds of the
// send. Real ticks are >= 6.25 ms apart (400 BPM ceiling). 1.5 ms is wide
// enough to cover scheduler jitter on the input thread and narrow enough
// that it is a quarter of the minimum tick spacing. Each send arms ONE
// pending slot and the first matching receive consumes it, so at worst one
// real event that lands inside the window is mistaken for the echo.
#define SELF_ECHO_WINDOW_S 0.0015
enum { ECHO_CLOCK = 0, ECHO_START, ECHO_CONTINUE, ECHO_STOP, ECHO_CC116, ECHO_CC120, ECHO_CC121, ECHO_N };
// tick_thread writes (send), input_thread consumes -- same lock-free
// best-effort convention as the rest of this file.
static volatile double g_echo_sent_s[ECHO_N];
static volatile int    g_echo_pending[ECHO_N];

static void echo_note_send(int kind) {
    g_echo_sent_s[kind]  = now_seconds();   // stamp BEFORE the output call
    g_echo_pending[kind] = 1;
}
// Returns 1 iff the event just received is the echo of our own send.
static int echo_is_ours(int kind) {
    if (!g_echo_pending[kind]) return 0;
    g_echo_pending[kind] = 0;
    return (now_seconds() - g_echo_sent_s[kind]) <= SELF_ECHO_WINDOW_S;
}

// Tempo to fall back to when we become MASTER: the last tempo that came from a
// master-side source (startup --bpm / live.conf, Link, CC118 steer). Never
// the PLL's recovered value.
static volatile double g_master_bpm    = 124.0;
static volatile int    g_master_source = CLOCK_SRC_LIVE_CONF_BPM;

// follower -> master: drop the recovered tempo and re-seed. Call from either
// thread after g_role has been set to ROLE_MASTER.
static void master_reseed_tempo(const char *why) {
    double recovered = g_bpm;
    g_bpm = g_master_bpm;
    g_tempo_source = g_master_source;
    g_source_last_seen[CLOCK_SRC_EXTERNAL_MIDI] = -1000.0;  // stop blocking lower rungs
    fprintf(stderr, "clock: master re-seed bpm %.1f -> %.1f (%s; discarded recovered tempo)\n",
            recovered, (double)g_master_bpm, why);
}

// Forward
// Sends now come from tick_thread, input_thread (CC116 claim) and osc_thread (tap -> immediate tempo
// broadcast), so serialise them: one event write at a time on the shared snd_seq handle.
static pthread_mutex_t g_send_mu = PTHREAD_MUTEX_INITIALIZER;

static void send_realtime(unsigned char byte) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    switch (byte) {
        case 0xF8: ev.type = SND_SEQ_EVENT_CLOCK;    break;
        case 0xFA: ev.type = SND_SEQ_EVENT_START;    break;
        case 0xFB: ev.type = SND_SEQ_EVENT_CONTINUE; break;
        case 0xFC: ev.type = SND_SEQ_EVENT_STOP;     break;
        default: return;
    }
    switch (byte) {
        case 0xF8: echo_note_send(ECHO_CLOCK);    break;
        case 0xFA: echo_note_send(ECHO_START);    break;
        case 0xFB: echo_note_send(ECHO_CONTINUE); break;
        case 0xFC: echo_note_send(ECHO_STOP);     break;
    }
    pthread_mutex_lock(&g_send_mu);
    snd_seq_event_output_direct(seq, &ev);
    pthread_mutex_unlock(&g_send_mu);
}

static void send_cc(int ch, int cc, int val) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_controller(&ev, ch, cc, val);
    if (ch == 15 && cc == 116) echo_note_send(ECHO_CC116);
    if (ch == 15 && cc == 120) echo_note_send(ECHO_CC120);
    if (ch == 15 && cc == 121) echo_note_send(ECHO_CC121);
    pthread_mutex_lock(&g_send_mu);
    snd_seq_event_output_direct(seq, &ev);
    pthread_mutex_unlock(&g_send_mu);
}

// Shared transport-state setter — used by both the CC117 write path and
// (in follower mode) incoming SND_SEQ_EVENT_START/STOP/CONTINUE. Commits
// to Link only when we're the master (in follower mode the external MIDI
// master wins; Link, if enabled, just goes along for the ride via the
// tick thread's own re-broadcast).
static void set_playing(int play);

// TRANSPORT = EITHER SOURCE IS MAKING SOUND (2026-10-01).
// Previously g_playing tracked ONLY PMOR's ch16 CC117, so recording a loop on
// NEPTR with PMOR silent left transport stopped: no 0xFA was ever sent, and a
// follower that never receives a Start can lock to the 16th grid but has no idea
// which step is beat 1. Sixteenths lined up; downbeats did not.
//
// NEPTR does not announce transport (it has no CC117), but it DOES announce
// ch16 CC115 "I hold a loop", ungated by clock authority. That is the missing
// half, so transport is now the OR of the two: whoever makes sound first sends
// Start and thereby defines the bar, and transport only stops once both are
// quiet. Deliberately an OR and not "PMOR wins" -- the user's rule is that
// whoever started first owns the grid, and that has to work from either side.
static void transport_reconcile(void) {
    int pmor_sounding = (g_pmor_transport_last_seen_s > 0.0 &&
                         (now_seconds() - g_pmor_transport_last_seen_s) <= ROLE_SIGNAL_FRESH_S)
                        ? g_pmor_transport_playing : 0;
    int loop_running  = (g_loop_held_last_seen_s > 0.0 &&
                         (now_seconds() - g_loop_held_last_seen_s) <= ROLE_SIGNAL_FRESH_S)
                        ? g_loop_held : 0;
    set_playing((pmor_sounding || loop_running) ? 1 : 0);
}

static void set_playing(int play) {
    if (play == g_playing) return;
    g_playing = play;
#ifdef DEMIURGE_LINK
    if (g_link_enabled && g_link && g_role == ROLE_MASTER) {
        auto state = g_link->captureAppSessionState();
        state.setIsPlaying(play != 0, std::chrono::microseconds(0));
        g_link->commitAppSessionState(state);
    }
#endif
}

// ===========================================================================
// TRANSPORT: tap analysis (once, here), canonical 14-bit tempo, OSC pool bridge
// (2026-10-05, bathtub realizations section 2).  See the banner comment at the
// top of this file and demiurge-transport.h.
//
// Threads: handle_tap() runs on input_thread (MIDI taps) AND osc_thread (OSC
// taps), so it takes g_tap_mu. Everything it writes (g_bpm, g_tempo_source...)
// is the same lock-free best-effort shared state the rest of this file uses.
// ===========================================================================
#define TRANSPORT_OSC_IN_PORT   DT_OSC_IN_PORT    // 127.0.0.1: we LISTEN here for /transport/tap
#define TRANSPORT_OSC_OUT_PORT  DT_OSC_OUT_PORT   // 127.0.0.1: we PUBLISH /transport/* here
enum { TAP_SRC_PMOR = 0, TAP_SRC_NEPTR = 1, TAP_SRC_OTHER = 2 };
#define TAP_MAX_AGE_MS 250.0           // a "tap age" older than this is a bug, not latency
#define WIRE_TEMPO_FRESH_S 3.0         // CC120/121 from the external master counts as the tempo for this long

static pthread_mutex_t g_tap_mu = PTHREAD_MUTEX_INITIALIZER;
static dt_tap_t        g_tap;
static volatile int    g_tap_count = 0;            // accepted taps since start (status/diagnostics)
static volatile int    g_last_tap_src = -1;
static volatile double g_last_tap_s = -1000.0;
static int             g_wire_msb = 0, g_wire_have_msb = 0;
static volatile double g_wire_tempo_last_s = -1000.0;   // last CC120/121 pair adopted while following

static int g_osc_out_fd = -1;
static struct sockaddr_in g_osc_out_addr;

static int wire_tempo_fresh(void) {
    return (now_seconds() - g_wire_tempo_last_s) <= WIRE_TEMPO_FRESH_S;
}

// --- OSC publish (encoding lives in demiurge-transport.h so it is unit-tested) ---
// types: "" | "f" | "i" | "if"
static void osc_publish(const char *addr, const char *types, int ival, double fval) {
    if (g_osc_out_fd < 0) return;
    unsigned char b[128];
    size_t n = dt_osc_encode(b, sizeof b, addr, types, (int32_t)ival, (float)fval);
    if (n) (void)sendto(g_osc_out_fd, b, n, 0, (struct sockaddr *)&g_osc_out_addr, sizeof g_osc_out_addr);
}

// CC119 (legacy 7-bit) + CC120/121 (canonical 14-bit) + OSC, all from the ONE g_bpm.
static void broadcast_tempo(double bpm) {
    int v = (int)lround((bpm - 40.0) / 360.0 * 127.0);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    send_cc(15, 119, v);
    uint16_t w = dt_bpm_to_wire14(bpm);
    send_cc(15, 120, dt_wire14_msb(w));
    send_cc(15, 121, dt_wire14_lsb(w));
    osc_publish("/transport/tempo", "f", 0, dt_wire14_to_bpm(w));
}

static void master_claim_by_tap(double now_s) {
    // A deliberate tap on a device that is NOT the clock master means "set the tempo": the
    // tempo can only be set by the master, so the tap takes master (same rule as PMOR's own
    // tap gesture, mirrored). Only reached once a tap PAIR has produced a tempo -- one stray
    // tap never moves the clock.
    if (g_role == ROLE_MASTER) return;
    g_role = ROLE_MASTER;
    clock_pll_state_init(&g_pll);
    g_have_ext_tick = 0;
    g_phase_pending = 0;
    g_follower_last_activity_s = now_s;
    fprintf(stderr, "clock: role -> MASTER (tap on a non-master device)\n");
    send_cc(15, 116, 2);
}

// src: TAP_SRC_*, age_ms: how long ago the press happened (device-reported, ISR-stamped).
static void handle_tap(int src, double age_ms) {
    if (!(age_ms >= 0.0)) age_ms = 0.0;
    if (age_ms > TAP_MAX_AGE_MS) age_ms = TAP_MAX_AGE_MS;
    double now_s = now_seconds();
    double t_s   = now_s - age_ms / 1000.0;

    pthread_mutex_lock(&g_tap_mu);
    // Every tap from every path is visible to the pool, exactly once.
    osc_publish("/transport/tap", "if", src, age_ms);

    // PMOR taps while PMOR is the master we follow: PMOR analysed it itself and its number
    // arrives as CC120/121. PMOR only sends CC112 while slaved, so this is a role
    // disagreement that CC116 will settle -- ignore rather than analyse twice.
    if (src == TAP_SRC_PMOR && g_role == ROLE_FOLLOWER) {
        pthread_mutex_unlock(&g_tap_mu);
        return;
    }

    double bpm = 0.0;
    int r = dt_tap_feed(&g_tap, (int64_t)(t_s * 1e6), &bpm);
    if (r == DT_TAP_IGNORED) {                       // bounce / duplicated USB-MIDI byte
        pthread_mutex_unlock(&g_tap_mu);
        return;
    }
    g_tap_count++;
    g_last_tap_src = src;
    g_last_tap_s = now_s;

    if (r == DT_TAP_TEMPO) {
        if (g_role == ROLE_FOLLOWER) master_claim_by_tap(now_s);
        // The wire value is canonical: quantise FIRST, adopt the quantised number.
        double canon = dt_bpm_canon(bpm);
        g_source_last_seen[CLOCK_SRC_HUMAN_GESTURE] = now_s;   // rung 0, never blocked
        g_bpm = canon;
        g_tempo_source = CLOCK_SRC_HUMAN_GESTURE;
        g_master_bpm = canon;  g_master_source = CLOCK_SRC_HUMAN_GESTURE;
        fprintf(stderr, "clock: TAP src=%d age=%.1fms -> BPM %.3f (raw %.3f, %d taps in window)\n",
                src, age_ms, canon, bpm, g_tap.n);
#ifdef DEMIURGE_LINK
        if (g_link_enabled && g_link && g_role == ROLE_MASTER) {
            auto state = g_link->captureAppSessionState();
            state.setTempo(canon, std::chrono::microseconds(0));
            g_link->commitAppSessionState(state);
        }
#endif
        broadcast_tempo(canon);   // immediately, not on the next beat: devices show THIS number
    }
    pthread_mutex_unlock(&g_tap_mu);
}

// A follower adopts the external master's CC120/121 pair as THE tempo. The PLL keeps
// recovering phase (and is the fallback tempo for foreign gear that sends no number).
static void adopt_wire_tempo(int msb, int lsb) {
    if (g_role != ROLE_FOLLOWER) return;
    double now_s = now_seconds();
    double bpm = dt_wire14_to_bpm(dt_wire14_join((uint8_t)msb, (uint8_t)lsb));
    g_wire_tempo_last_s = now_s;
    g_source_last_seen[CLOCK_SRC_EXTERNAL_MIDI] = now_s;
    g_tempo_source = CLOCK_SRC_EXTERNAL_MIDI;
    g_follower_last_activity_s = now_s;
    if (fabs(bpm - g_bpm) > 1e-6) {
        g_bpm = bpm;
        fprintf(stderr, "clock: BPM -> %.3f (external master, CC120/121)\n", bpm);
    }
}

// --- OSC in: /transport/tap  ,if (source, age_ms) | ,i | ,  (no args = source other, age 0) ---
static void *osc_thread(void *) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { fprintf(stderr, "clock: osc socket failed (taps over OSC disabled)\n"); return nullptr; }
    int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(TRANSPORT_OSC_IN_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);          // loopback ONLY: the pool is local
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) {
        fprintf(stderr, "clock: osc bind 127.0.0.1:%d failed (%s) -- taps over OSC disabled\n",
                TRANSPORT_OSC_IN_PORT, strerror(errno));
        close(fd);
        return nullptr;
    }
    unsigned char b[256];
    while (running) {
        ssize_t n = recv(fd, b, sizeof b - 1, 0);
        int src; double age;
        if (n < 8 || !dt_osc_parse_tap(b, (size_t)n, &src, &age)) continue;
        handle_tap(src, age);
    }
    close(fd);
    return nullptr;
}

// Called from input_thread on every incoming external SND_SEQ_EVENT_CLOCK
// while in follower mode. Runs the tick through the shared PLL (see
// demiurge-clock-pll.h): intervals are gated to what's physically
// possible at 24 PPQN between MIN_BPM/MAX_BPM (rejects PMOR's known
// duplicate-byte bug without re-anchoring on the duplicate), a stall
// past the ceiling re-anchors instead of wedging, and the recovered BPM
// comes from a trimmed mean (outlier-resistant) over the window rather
// than a plain mean. Updates g_bpm once the window has filled, and drops
// a phase anchor for tick_thread to snap to.
static void follower_on_clock_tick() {
    // Master never adopts tempo from received 0xF8 (role may have flipped on
    // the tick thread between the caller's check and here).
    if (g_role != ROLE_FOLLOWER) return;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double now_s = now_seconds();

    clock_pll_result_t res = clock_pll_on_tick(&g_pll, now_s);

    if (res.verdict != CLOCK_PLL_REJECT_DUPLICATE) {
        // Legitimate external-clock activity (first tick ever, an
        // accepted interval, or a stall re-anchor) -- refresh
        // EXTERNAL_MIDI's recency for the tempo-source ladder even on
        // ticks that don't move the BPM average this time. A rejected
        // duplicate must NOT refresh this: it isn't a second real tick.
        g_source_last_seen[CLOCK_SRC_EXTERNAL_MIDI] = now_s;
        g_tempo_source = CLOCK_SRC_EXTERNAL_MIDI;
        // Also counts as follower-mode "activity" for the reclaim timeout
        // below -- see FOLLOWER_RECLAIM_TIMEOUT_S.
        g_follower_last_activity_s = now_s;
    }

    if (res.bpm_updated) {
        // The PLL's tempo is a measurement of the sender's tick jitter. When the master also
        // sends the tempo as a NUMBER (CC120/121, canonical), that number is the tempo and the
        // PLL only supplies phase. The PLL tempo remains the fallback for foreign gear.
        if (!wire_tempo_fresh()) g_bpm = res.bpm;
        g_phase_anchor = now;
        g_phase_pending = 1;
    }

    g_last_ext_tick = now;
    g_have_ext_tick = 1;
}

// Writes ~/demiurge/clock_status.conf for clockrole.rs to poll (see the
// "Transport-role status export" comment above g_pmor_transport_playing).
// Atomic write (temp file + rename), same pattern as every other
// DEMIURGE config file this project writes (live.conf's device header,
// sync.conf). Called from tick_thread's existing ~2s status block, so
// this costs one small file write per ~2s, not per tick.
static void write_status_file(double bpm) {
    char path[1024];
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(path, sizeof(path), "%s/demiurge/%s", home, CLOCK_STATUS_FILENAME);
    } else {
        snprintf(path, sizeof(path), "demiurge/%s", CLOCK_STATUS_FILENAME);
    }
    char tmp_path[1040];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return; // best-effort; a missing ~/demiurge dir just means no status, not a crash

    double now_s = now_seconds();
    double pmor_age = now_s - g_pmor_transport_last_seen_s;
    double loop_age = now_s - g_source_last_seen[CLOCK_SRC_CC118_STEER];

    fprintf(f, "# demiurge-clock live status — written every ~2s, read-only.\n");
    fprintf(f, "# consumed by demiurge-launcher-rs's clockrole.rs for transport-aware\n");
    fprintf(f, "# role election (see clockrole.rs for the full election logic).\n");
    fprintf(f, "role = %s\n", g_role == ROLE_FOLLOWER ? "follower" : "master");
    fprintf(f, "bpm = %.1f\n", bpm);
    fprintf(f, "source = %s\n", clock_tempo_source_name((clock_tempo_source_t)g_tempo_source));
    // tempo_source_real: the same verdict broadcast on ch16 CC114 (see the
    // banner comment and tempo_source_is_real() above), exported here too so
    // it's debuggable at the bench without sniffing the MIDI bus.
    fprintf(f, "tempo_source_real = %s\n", tempo_source_is_real() ? "yes" : "no");
    // pmor_transport: PMOR's own sounding/silent state, from inbound CC117.
    // "unknown" if none has ever been seen, or the last one is older than
    // ROLE_SIGNAL_FRESH_S — a stale reading must read the same as no
    // reading at all to the election logic on the other end.
    if (g_pmor_transport_last_seen_s < 0.0 || pmor_age > ROLE_SIGNAL_FRESH_S) {
        fprintf(f, "pmor_transport = unknown\n");
    } else {
        fprintf(f, "pmor_transport = %s\n", g_pmor_transport_playing ? "playing" : "stopped");
    }
    fprintf(f, "pmor_transport_age_s = %.1f\n", pmor_age);
    // ext_clock: evidence that a real (non-echo) external 0xF8 stream is on
    // the bus. clockrole.rs requires this before electing follower.
    {
        double ext_age = now_s - g_ext_clock_last_s;
        fprintf(f, "ext_clock = %s\n",
                (g_ext_clock_streak >= EXT_CLOCK_MIN_STREAK && ext_age <= ROLE_SIGNAL_FRESH_S) ? "yes" : "no");
    }
    // loop_active: proxy for "NEPTR has a loop driving tempo" — see the
    // export comment above for why routine CC118 recency is used here.
    // Prefer the AUTHORITATIVE CC115 announce; fall back to the old
    // routine-CC118 recency proxy only when CC115 has never been heard (a
    // NEPTR predating that announce). Reported age tracks whichever was used.
    double held_age = now_s - g_loop_held_last_seen_s;
    if (g_loop_held_last_seen_s > 0.0 && held_age <= ROLE_SIGNAL_FRESH_S) {
        fprintf(f, "loop_active = %s\n", g_loop_held ? "yes" : "no");
        loop_age = held_age;
    } else if (g_source_last_seen[CLOCK_SRC_CC118_STEER] <= 0.0 || loop_age > ROLE_SIGNAL_FRESH_S) {
        fprintf(f, "loop_active = unknown\n");
    } else {
        fprintf(f, "loop_active = yes\n");
    }
    fprintf(f, "loop_active_age_s = %.1f\n", loop_age);
    // Transport (2026-10-05): the number every display should show, at full wire precision
    // (the old `bpm = %.1f` line above stays for the launcher; this is the 14-bit canonical value).
    fprintf(f, "bpm_wire = %.3f\n", dt_bpm_canon(bpm));
    fprintf(f, "tempo_from_number = %s\n", wire_tempo_fresh() ? "yes" : "no");
    fprintf(f, "tap_count = %d\n", (int)g_tap_count);
    fprintf(f, "last_tap_source = %s\n", g_last_tap_src == TAP_SRC_PMOR ? "pmor" :
                                          g_last_tap_src == TAP_SRC_NEPTR ? "neptr" :
                                          g_last_tap_src == TAP_SRC_OTHER ? "other" : "none");
    fprintf(f, "last_tap_age_s = %.1f\n", now_s - g_last_tap_s);

    fclose(f);
    rename(tmp_path, path); // atomic on the same filesystem
}

// Tick thread — emits 0xF8 at 24 PPQN, plus start/stop on state change,
// plus a ch16 CC119 BPM broadcast once per beat so non-clock-savvy
// listeners can still track tempo changes.
//
// When DEMIURGE_LINK is enabled and g_link_enabled is 1, we also poll the
// Link session for tempo and transport changes and mirror them into
// g_bpm / g_playing. Link is the outer authority in that case.
static void *tick_thread(void *) {
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    int tick_count = 0;
    int last_playing = -1;
    uint32_t bar_tick = 0;         // 24-PPQN ticks since the last Start (96 = one 4/4 bar)
    int last_tempo_real_sent = -1; // -1 = "never sent"; forces an initial CC114
    struct timespec last_role_announce;
    clock_gettime(CLOCK_MONOTONIC, &last_role_announce);

    while (running) {
#ifdef DEMIURGE_LINK
        // Link is only the outer tempo authority when we're master. In
        // follower mode the external MIDI clock (recovered via the PLL
        // in follower_on_clock_tick) wins, and we don't commit anything
        // back into a Link session while following.
        if (g_link_enabled && g_link && g_role == ROLE_MASTER) {
            auto state = g_link->captureAppSessionState();
            double link_bpm = state.tempo();
            if (fabs(link_bpm - g_bpm) > 0.25) {
                // Tempo-source ladder (FIX 2): Link is priority 3, below
                // HUMAN_GESTURE, EXTERNAL_MIDI and CC118_STEER. Mark that we heard a
                // change from Link either way, but only actually apply
                // it if nothing higher-priority is fresh right now.
                double now_s = now_seconds();
                g_source_last_seen[CLOCK_SRC_LINK] = now_s;
                if (clock_tempo_source_may_apply(CLOCK_SRC_LINK, g_source_last_seen, now_s)) {
                    g_bpm = link_bpm;
                    g_tempo_source = CLOCK_SRC_LINK;
                    g_master_bpm = link_bpm;  g_master_source = CLOCK_SRC_LINK;
                }
            }
            // startStopSync propagates transport across the Link session.
            int link_playing = state.isPlaying() ? 1 : 0;
            if (link_playing != g_playing) {
                g_playing = link_playing;
            }
        }
#endif

        // Follower mode: g_bpm is kept current by follower_on_clock_tick's
        // PLL (called from input_thread), so the normal cadence below
        // already tracks recovered tempo. On top of that, snap our own
        // schedule to the incoming pulse's phase whenever a fresh anchor
        // lands, and freewheel (just keep ticking at the last recovered
        // tempo) if the external clock has gone quiet for >2s.
        if (g_role == ROLE_FOLLOWER) {
            if (g_phase_pending) {
                next = g_phase_anchor;
                g_phase_pending = 0;
            }
            // Freewheel: nothing to do here besides *not* resetting
            // g_bpm — it already holds the last recovered tempo, and we
            // just keep emitting on the existing cadence below.

            // Reclaim backstop: see FOLLOWER_RECLAIM_TIMEOUT_S above for
            // the full reasoning. If this daemon has gone that long
            // without a legitimate external tick or a fresh role-claim,
            // assume the external master is gone for good (not just a
            // momentary stutter the 2s PLL freewheel window already
            // tolerates) and take master back unilaterally -- same reset
            // sequence as an incoming CC116=2, done locally instead of
            // waiting for one to arrive.
            double since_activity = now_seconds() - g_follower_last_activity_s;
            if (since_activity > FOLLOWER_RECLAIM_TIMEOUT_S) {
                g_role = ROLE_MASTER;
                clock_pll_state_init(&g_pll);
                g_have_ext_tick = 0;
                g_phase_pending = 0;
                master_reseed_tempo("reclaim");
                fprintf(stderr,
                        "clock: role -> MASTER (reclaimed: no follower activity for %.1fs > %.1fs)\n",
                        since_activity, FOLLOWER_RECLAIM_TIMEOUT_S);
            }
        }

        // ch16 CC114 — tempo-source CLASS, change-triggered (see the banner
        // comment and tempo_source_is_real() above). Checked every tick
        // (cheap int compare) so a source flip reaches the bus within one
        // 24-PPQN tick instead of waiting for the ~2s role announce below,
        // which also re-sends it unconditionally for late-joining readers.
        {
            int tempo_real_now = tempo_source_is_real() ? 127 : 0;
            if (tempo_real_now != last_tempo_real_sent) {
                send_cc(15, 114, tempo_real_now);
                last_tempo_real_sent = tempo_real_now;
            }
        }

        double bpm = g_bpm;
        double tick_s = 60.0 / bpm / 24.0;
        long ns = (long)(tick_s * 1e9);
        next.tv_nsec += ns;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec  += 1;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);

        if (g_playing != last_playing) {
            send_realtime(g_playing ? 0xFA : 0xFC);
            if (g_playing) bar_tick = 0;                       // bar 0 starts at Start
            osc_publish(g_playing ? "/transport/start" : "/transport/stop", "", 0, 0.0);
            last_playing = g_playing;
            fprintf(stderr, "clock: transport %s\n", g_playing ? "START" : "STOP");
        }

        // CLOCK RUNS CONTINUOUSLY, transport does not gate it
        // (bench-observed 2026-10-01). This used to be `if (g_playing)`, which
        // created a circular dependency with PMOR: PMOR announces "I am silent"
        // on ch16 CC117 -> set_playing(false) here -> we emit no 0xF8 at all ->
        // PMOR, which is the FOLLOWER, has no tick stream to lock to -> its grid
        // free-runs at its own tempo. The Pi's clock only ran while PMOR played,
        // but PMOR could only sync while the Pi's clock ran, so the two simply
        // never shared a grid.
        //
        // Emitting 0xF8 unconditionally is also the standard convention: a clock
        // source streams 24 PPQN continuously and uses 0xFA/0xFB/0xFC (sent just
        // above, still strictly on a transport EDGE) to start and stop the
        // receiving SEQUENCER. Tempo and phase stay available at rest, which is
        // what lets a follower be already locked the instant you hit play.
        send_realtime(0xF8);
        tick_count++;
        if (g_playing && (bar_tick % 96) == 0) osc_publish("/transport/bar", "i", (int)(bar_tick / 96), 0.0);
        if (g_playing) bar_tick++;
        if (tick_count % 24 == 0) {
            broadcast_tempo(bpm);   // CC119 (legacy 7-bit) + CC120/121 (canonical) + /transport/tempo
        }

        // Re-announce our current role on CC116 every ~2s so a
        // late-plugged device (or a UI that just opened) converges.
        {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double since = (now.tv_sec - last_role_announce.tv_sec) +
                           (now.tv_nsec - last_role_announce.tv_nsec) / 1e9;
            if (since >= 2.0) {
                send_cc(15, 116, g_role == ROLE_FOLLOWER ? 1 : 2);
                // Unconditional re-send of CC114 alongside CC116/CC119, same
                // reason as the role announce: a late-plugged device (or a
                // CC114 that arrived while a reader wasn't listening) needs
                // more than just the change-triggered send above to converge.
                last_tempo_real_sent = tempo_source_is_real() ? 127 : 0;
                send_cc(15, 114, last_tempo_real_sent);
                // TEMPO IS NOT TRANSPORT (bench-observed 2026-10-01). The only
                // other CC119 send lives inside the `if (g_playing)` tick block
                // (every 24 ticks), so while transport is STOPPED the bus never
                // carried a tempo at all. NEPTR reads it with
                // `ctrl7 16, 119, 40, 400`, and ctrl7 reports 0 for a controller
                // that has never arrived -- which maps to the range MINIMUM, 40.
                // Measured: NEPTR's beat ran at 40 BPM while this daemon held
                // 124, and a loop recorded at rest would have quantised to 40.
                // A stopped clock still has a tempo, and NEPTR needs it to
                // quantise the FIRST loop, before anything is playing. So the
                // tempo is re-announced here unconditionally, independent of
                // g_playing, exactly like the role and tempo-source class.
                broadcast_tempo(bpm);
                last_role_announce = now;
                fprintf(stderr, "clock: status bpm=%.1f role=%s source=%s tempo_real=%d\n",
                        bpm, g_role == ROLE_FOLLOWER ? "follower" : "master",
                        clock_tempo_source_name((clock_tempo_source_t)g_tempo_source),
                        last_tempo_real_sent);
                write_status_file(bpm);
            }
        }
    }
    return nullptr;
}

// Input listener — accepts steering commands from any program on the bus.
static void *input_thread(void *) {
    while (running) {
        snd_seq_event_t *ev = nullptr;
        if (snd_seq_event_input(seq, &ev) < 0 || !ev) {
            usleep(1000);
            continue;
        }
        // Feedback guard: never react to events we sourced ourselves
        // (our own 0xF8/CC116 re-broadcast loops back on the shared bus).
        int from_self = (ev->source.client == g_my_client_id);

        // Clock-in branch: raw MIDI-realtime clock + transport, only
        // meaningful (and only acted on) while we're following someone
        // else's tempo.
        if (ev->type == SND_SEQ_EVENT_CLOCK    ||
            ev->type == SND_SEQ_EVENT_START    ||
            ev->type == SND_SEQ_EVENT_STOP     ||
            ev->type == SND_SEQ_EVENT_CONTINUE) {
            if (from_self) continue;
            // Midi Through strips the source, so from_self never fires for
            // the hub echo: drop our own echo by send-time instead.
            {
                int k = ev->type == SND_SEQ_EVENT_CLOCK    ? ECHO_CLOCK :
                        ev->type == SND_SEQ_EVENT_START    ? ECHO_START :
                        ev->type == SND_SEQ_EVENT_CONTINUE ? ECHO_CONTINUE : ECHO_STOP;
                if (echo_is_ours(k)) continue;
            }
            if (ev->type == SND_SEQ_EVENT_CLOCK) {
                double t = now_seconds();
                g_ext_clock_streak = (t - g_ext_clock_last_s <= EXT_CLOCK_GAP_S) ? g_ext_clock_streak + 1 : 1;
                g_ext_clock_last_s = t;
            }
            // Master NEVER adopts tempo/transport from received realtime.
            if (g_role != ROLE_FOLLOWER) continue;
            switch (ev->type) {
                case SND_SEQ_EVENT_CLOCK:
                    follower_on_clock_tick();
                    break;
                case SND_SEQ_EVENT_START:
                    set_playing(1);
                    fprintf(stderr, "clock: transport START (external)\n");
                    break;
                case SND_SEQ_EVENT_CONTINUE:
                    set_playing(1);
                    fprintf(stderr, "clock: transport CONTINUE (external)\n");
                    break;
                case SND_SEQ_EVENT_STOP:
                    set_playing(0);
                    fprintf(stderr, "clock: transport STOP (external)\n");
                    break;
                default:
                    break;
            }
            continue;
        }

        if (ev->type != SND_SEQ_EVENT_CONTROLLER) continue;

        // Clock control lives on MIDI channel 16 (ALSA seq idx 15).
        // Ignore anything on other channels — those are musical CCs.
        if (ev->data.control.channel != 15) continue;

        int cc  = ev->data.control.param;
        int val = ev->data.control.value;

        if (cc == 116) {
            // Role negotiation. value 1 = OpGorator (or any other
            // external device) is master, we become follower; value 2 =
            // we're master. Ignore our own re-announcement.
            if (from_self) continue;
            if (echo_is_ours(ECHO_CC116)) continue;  // our own re-announce via Midi Through
            double now_s = now_seconds();
            // Gesture correlation (FIX: human-gesture ladder rung): stamp
            // every inbound "I'm claiming master" (val==2) regardless of
            // whether it actually changes g_role -- a tap-tempo gesture
            // sends this every time, and a CC118 arriving within
            // CLOCK_GESTURE_WINDOW_S of this stamp is what makes that
            // CC118 a deliberate human tap instead of a routine
            // loop-derived steer. See the CC118 branch below and
            // demiurge-clock-pll.h's CLOCK_SRC_HUMAN_GESTURE comment.
            if (val == 2) g_last_cc116_claim_master_s = now_s;
            int new_role = g_role;
            if (val == 1) new_role = ROLE_FOLLOWER;
            else if (val == 2) new_role = ROLE_MASTER;
            if (new_role != g_role) {
                g_role = new_role;
                // Reset PLL/freewheel state on every role flip so a
                // stale window from a previous follower stint can't
                // leak into the new one.
                clock_pll_state_init(&g_pll);
                g_have_ext_tick = 0;
                g_phase_pending = 0;
                // Fresh grace period for the follower-reclaim timeout
                // (FOLLOWER_RECLAIM_TIMEOUT_S): a device that just
                // claimed master gets the full timeout to start sending
                // ticks before this daemon reclaims out from under it.
                g_follower_last_activity_s = now_s;
                fprintf(stderr, "clock: role -> %s (via CC116)\n",
                        g_role == ROLE_FOLLOWER ? "FOLLOWER (external master)" : "MASTER");
                if (g_role == ROLE_MASTER) master_reseed_tempo("CC116=2");
            }
        } else if (cc == 118) {
            // BPM steer — any program on the bus can write CC118 to
            // retune the clock. Unlike CC119 (our broadcast), this is
            // the WRITE side, so there's no self-echo to worry about.
            //
            // Human-gesture detection: a CC118 arriving within
            // CLOCK_GESTURE_WINDOW_S of the last inbound CC116=2 claim
            // is treated as part of ONE deliberate tap-tempo gesture
            // (PMOR: claim master, then immediately steer) rather than a
            // routine loop-derived steer -- see demiurge-clock-pll.h's
            // CLOCK_SRC_HUMAN_GESTURE comment for why CC116-adjacency is
            // the signal we picked (it's the only thing a gesture does
            // that chase_bliss_clock.orc's routine CC118 never does: a
            // local tap always asserts "I'm taking over" first). This
            // reclassifies which ladder rung the write is recorded
            // under; it does not change anything else about how CC118 is
            // parsed or applied.
            int is_gesture = (now_seconds() - g_last_cc116_claim_master_s) <= CLOCK_GESTURE_WINDOW_S;
            clock_tempo_source_t src = is_gesture ? CLOCK_SRC_HUMAN_GESTURE : CLOCK_SRC_CC118_STEER;
            //
            // Tempo-source ladder (FIX 2, extended by the gesture rung
            // above): a routine CC118 is priority 2, below EXTERNAL_MIDI
            // only -- a safety net, not a fight, since chase_bliss_clock.
            // orc (the main CC118 sender) already self-gates and stays
            // silent while DEMIURGE is authoritative. A GESTURE CC118 is
            // priority 0 and is therefore never blocked by anything,
            // including a live external-MIDI PLL lock -- "if I started
            // tap tempo, it varispeeds the loop already happening to
            // match it" means a deliberate tap must win even over an
            // active follower lock. We still record that a steer arrived
            // under its rung (so the ladder's recency bookkeeping stays
            // correct for both routine and gesture writes) even when we
            // don't act on it.
            double new_bpm = 40.0 + (val / 127.0) * 360.0;
            double now_s = now_seconds();
            g_source_last_seen[src] = now_s;
            if (!clock_tempo_source_may_apply(src, g_source_last_seen, now_s)) {
                // Only reachable for a ROUTINE steer (a gesture is rung 0
                // and clock_tempo_source_may_apply() never blocks rung
                // 0): external MIDI clock is fresh (e.g. we're actively
                // following PMOR) -- ignore the steer. The PLL owns
                // tempo right now; a stray routine CC118 must not fight
                // it. A deliberate tap always gets through instead.
            } else if (fabs(new_bpm - g_bpm) > 0.25) {
                g_bpm = new_bpm;
                g_tempo_source = src;
                g_master_bpm = new_bpm;  g_master_source = src;
                fprintf(stderr, "clock: BPM -> %.1f (via CC118%s)\n",
                        new_bpm, is_gesture ? ", HUMAN GESTURE" : "");
#ifdef DEMIURGE_LINK
                if (g_link_enabled && g_link && g_role == ROLE_MASTER) {
                    auto state = g_link->captureAppSessionState();
                    state.setTempo(new_bpm, std::chrono::microseconds(0));
                    g_link->commitAppSessionState(state);
                }
#endif
            }
        } else if (cc == 112) {
            handle_tap(TAP_SRC_PMOR, (double)val);     // PMOR encoder tap (only sent while PMOR is slaved)
        } else if (cc == 113) {
            handle_tap(TAP_SRC_NEPTR, (double)val);    // NEPTR arcade button
        } else if (cc == 120 || cc == 121) {
            if (from_self || echo_is_ours(cc == 120 ? ECHO_CC120 : ECHO_CC121)) continue;  // our own broadcast
            if (cc == 120) { g_wire_msb = val; g_wire_have_msb = 1; }
            else if (g_wire_have_msb) adopt_wire_tempo(g_wire_msb, val);
        } else if (cc == 115) {
            // NEPTR loop-held announce. Recorded unconditionally; it never
            // touches tempo or role directly -- clockrole.rs reads it out of
            // clock_status.conf and makes the election decision.
            g_loop_held             = (val >= 64) ? 1 : 0;
            g_loop_held_last_seen_s = now_seconds();
            transport_reconcile();   // a NEPTR loop starting is a transport start
        } else if (cc == 117) {
            // Transport. Also PMOR's own sounding/silent announcement
            // (see the "Transport-role status export" comment above) --
            // record it unconditionally so clockrole.rs's status-file
            // read always reflects the most recent CC117, regardless of
            // whether it actually flips g_playing.
            g_pmor_transport_playing     = (val >= 64) ? 1 : 0;
            g_pmor_transport_last_seen_s = now_seconds();
            transport_reconcile();   // OR'd with NEPTR's CC115, not PMOR alone
        }
    }
    return nullptr;
}

static void on_sig(int) { running = 0; }

// CLI:
//   demiurge-clock                      (parse link from /boot/firmware/demiurge.conf — legacy)
//   demiurge-clock --link=on|off        (explicit, used by the launcher)
//   demiurge-clock --link on|off        (same thing, space-separated)
//
// When the launcher manages the clock it always passes an explicit flag so
// config lookup is skipped entirely — that keeps the cost of "link = off"
// truly zero (no mtime polls, no disk reads, no Link object construction).
int main(int argc, char **argv) {
    // Epoch for now_seconds() -- see its definition above. Captured first,
    // before anything else, so every timestamp for the rest of the
    // process's life is a small, precise offset from it.
    clock_gettime(CLOCK_MONOTONIC, &g_time_epoch);

    int explicit_link = -1;
    int explicit_role = -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "--link=", 7) == 0) {
            const char *v = a + 7;
            explicit_link = (strcasecmp(v, "on") == 0 || strcasecmp(v, "true") == 0 ||
                             strcasecmp(v, "yes") == 0 || strcmp(v, "1") == 0) ? 1 : 0;
        } else if (strcmp(a, "--link") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            explicit_link = (strcasecmp(v, "on") == 0 || strcasecmp(v, "true") == 0 ||
                             strcasecmp(v, "yes") == 0 || strcmp(v, "1") == 0) ? 1 : 0;
        } else if (strncmp(a, "--bpm=", 6) == 0) {
            double v = atof(a + 6);
            if (v >= 40.0 && v <= 400.0) g_bpm = v;
        } else if (strcmp(a, "--bpm") == 0 && i + 1 < argc) {
            double v = atof(argv[++i]);
            if (v >= 40.0 && v <= 400.0) g_bpm = v;
        } else if (strncmp(a, "--role=", 7) == 0) {
            const char *v = a + 7;
            if (strcasecmp(v, "follower") == 0) explicit_role = ROLE_FOLLOWER;
            else if (strcasecmp(v, "master") == 0) explicit_role = ROLE_MASTER;
        } else if (strcmp(a, "--role") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcasecmp(v, "follower") == 0) explicit_role = ROLE_FOLLOWER;
            else if (strcasecmp(v, "master") == 0) explicit_role = ROLE_MASTER;
        }
    }
    if (explicit_link >= 0) {
        g_link_enabled = explicit_link;
    } else {
        g_link_enabled = parse_bool_key(BOOT_CONFIG, "link");
    }

    // Role: --role flag wins outright; otherwise read ~/demiurge/sync.conf
    // (`role = opgorator|demiurge`). Missing file/key => master — the
    // "OpGorator attached => follower by default" policy lives in the
    // demiurge-sync CLI/UI, which writes sync.conf; this daemon just
    // reads whatever's there.
    if (explicit_role >= 0) {
        g_role = explicit_role;
    } else {
        char sync_path[1024];
        const char *home = getenv("HOME");
        if (home && *home) {
            snprintf(sync_path, sizeof(sync_path), "%s/demiurge/sync.conf", home);
        } else {
            snprintf(sync_path, sizeof(sync_path), "demiurge/sync.conf");
        }
        int file_role = parse_role_key(sync_path);
        g_role = (file_role >= 0) ? file_role : ROLE_MASTER;
    }
    g_master_bpm = g_bpm;   // --bpm / live.conf value (launcher passes it) is the master seed
    fprintf(stderr, "demiurge_clock: role=%s\n", g_role == ROLE_FOLLOWER ? "follower" : "master");

#ifdef DEMIURGE_LINK
    if (g_link_enabled) {
        g_link = new ableton::Link(g_bpm);
        g_link->enable(true);
        g_link->enableStartStopSync(true);
        // Push initial transport=playing so the tick_thread's first poll
        // doesn't see isPlaying()=false and kill the clock immediately.
        {
            auto state = g_link->captureAppSessionState();
            state.setIsPlaying(true, std::chrono::microseconds(0));
            g_link->commitAppSessionState(state);
        }
        fprintf(stderr, "demiurge_clock: Ableton Link enabled (initial BPM=%.1f)\n", g_bpm);
    } else {
        fprintf(stderr, "demiurge_clock: Ableton Link disabled (set `link = on` in %s)\n", BOOT_CONFIG);
    }
#else
    if (g_link_enabled) {
        fprintf(stderr, "demiurge_clock: `link = on` requested but binary was built without DEMIURGE_LINK\n");
    }
#endif

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "demiurge_clock: snd_seq_open failed\n");
        return 1;
    }
    snd_seq_set_client_name(seq, "demiurge_clock");
    g_my_client_id = snd_seq_client_id(seq);

    out_port = snd_seq_create_simple_port(seq, "out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    in_port = snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

    if (out_port < 0 || in_port < 0) {
        fprintf(stderr, "demiurge_clock: port create failed\n");
        return 1;
    }

    // Subscribe both ways to "Midi Through" (client 14:0 — kernel
    // module `snd-seq-dummy`, always present on Linux) so the clock
    // joins the shared bus:
    //   out_port -> 14:0    broadcasts (0xF8, CC119) reach every reader
    //   14:0     -> in_port steer commands (CC118) reach us
    // Without the IN subscription, any program writing CC118 to
    // Midi Through would never reach the daemon.
    if (snd_seq_connect_to(seq, out_port, 14, 0) < 0) {
        fprintf(stderr, "demiurge_clock: connect_to 14:0 failed (already linked by launcher?)\n");
    }
    if (snd_seq_connect_from(seq, in_port, 14, 0) < 0) {
        fprintf(stderr, "demiurge_clock: connect_from 14:0 failed\n");
    }

    fprintf(stderr, "demiurge_clock: started at BPM=%.1f (24 PPQN)\n", g_bpm);

    // OSC pool bridge (loopback only): publish /transport/* to 127.0.0.1:9101, listen for
    // /transport/tap on 127.0.0.1:9100. Failure here must never take the clock down.
    g_osc_out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&g_osc_out_addr, 0, sizeof g_osc_out_addr);
    g_osc_out_addr.sin_family = AF_INET;
    g_osc_out_addr.sin_port = htons(TRANSPORT_OSC_OUT_PORT);
    g_osc_out_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dt_tap_reset(&g_tap);

    pthread_t th1, th2, th3;
    pthread_create(&th1, nullptr, tick_thread, nullptr);
    pthread_create(&th2, nullptr, input_thread, nullptr);
    pthread_create(&th3, nullptr, osc_thread, nullptr);

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);
    while (running) sleep(1);

    snd_seq_close(seq);
    return 0;
}
