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
//
// 0..127 maps to 40..400 BPM for both CC119 and CC118.
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

#ifdef DEMIURGE_LINK
#include <ableton/Link.hpp>
#include <chrono>
#endif

static snd_seq_t *seq = nullptr;
static int out_port = -1;
static int in_port  = -1;

static volatile double g_bpm     = 124.0;
static volatile int    g_playing = 1;
static volatile sig_atomic_t running = 1;
static int g_link_enabled = 0;
static const char *BOOT_CONFIG = "/boot/firmware/demiurge.conf";

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

// Forward
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
    snd_seq_event_output_direct(seq, &ev);
}

static void send_cc(int ch, int cc, int val) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_controller(&ev, ch, cc, val);
    snd_seq_event_output_direct(seq, &ev);
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

    while (running) {
#ifdef DEMIURGE_LINK
        if (g_link_enabled && g_link) {
            auto state = g_link->captureAppSessionState();
            double link_bpm = state.tempo();
            if (fabs(link_bpm - g_bpm) > 0.25) {
                g_bpm = link_bpm;
            }
            // startStopSync propagates transport across the Link session.
            int link_playing = state.isPlaying() ? 1 : 0;
            if (link_playing != g_playing) {
                g_playing = link_playing;
            }
        }
#endif

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
            last_playing = g_playing;
            fprintf(stderr, "clock: transport %s\n", g_playing ? "START" : "STOP");
        }

        if (g_playing) {
            send_realtime(0xF8);
            tick_count++;
            if (tick_count % 24 == 0) {
                int v = (int)lround((bpm - 40.0) / 360.0 * 127.0);
                if (v < 0)   v = 0;
                if (v > 127) v = 127;
                send_cc(15, 119, v);
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
        if (ev->type != SND_SEQ_EVENT_CONTROLLER) continue;

        // Clock control lives on MIDI channel 16 (ALSA seq idx 15).
        // Ignore anything on other channels — those are musical CCs.
        if (ev->data.control.channel != 15) continue;

        int cc  = ev->data.control.param;
        int val = ev->data.control.value;

        if (cc == 118) {
            // BPM steer — any program on the bus can write CC118 to
            // retune the clock. Unlike CC119 (our broadcast), this is
            // the WRITE side, so there's no self-echo to worry about.
            double new_bpm = 40.0 + (val / 127.0) * 360.0;
            if (fabs(new_bpm - g_bpm) > 0.25) {
                g_bpm = new_bpm;
                fprintf(stderr, "clock: BPM -> %.1f (via CC118)\n", new_bpm);
#ifdef DEMIURGE_LINK
                if (g_link_enabled && g_link) {
                    auto state = g_link->captureAppSessionState();
                    state.setTempo(new_bpm, std::chrono::microseconds(0));
                    g_link->commitAppSessionState(state);
                }
#endif
            }
        } else if (cc == 117) {
            int play = val >= 64 ? 1 : 0;
            if (play != g_playing) {
                g_playing = play;
#ifdef DEMIURGE_LINK
                if (g_link_enabled && g_link) {
                    auto state = g_link->captureAppSessionState();
                    state.setIsPlaying(play != 0, std::chrono::microseconds(0));
                    g_link->commitAppSessionState(state);
                }
#endif
            }
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
    int explicit_link = -1;
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
        }
    }
    if (explicit_link >= 0) {
        g_link_enabled = explicit_link;
    } else {
        g_link_enabled = parse_bool_key(BOOT_CONFIG, "link");
    }

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

    pthread_t th1, th2;
    pthread_create(&th1, nullptr, tick_thread, nullptr);
    pthread_create(&th2, nullptr, input_thread, nullptr);

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);
    while (running) sleep(1);

    snd_seq_close(seq);
    return 0;
}
