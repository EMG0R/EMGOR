// midiprobe.cpp -- ALSA-seq MIDI timing probe for PMOR on the Pi (real-time queue timestamps).
//   midiprobe capture SECONDS [CLIENT]            log every event from PMOR: "<t_ms> <kind> <a> <b> <c>"
//   midiprobe clock BPM SECONDS [startpos16]      send Start (or SPP+Continue) then 24 PPQN clock to PMOR, queue-timed
//   midiprobe stats FILE                          F8 interval stats (mean, stddev, max dev, drift) from a capture log
#include <alsa/asoundlib.h>
#include <poll.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
static snd_seq_t* seq; static int port; static int pmor = 28;
static int findClient(const char* name) {
    snd_seq_client_info_t* ci; snd_seq_client_info_alloca(&ci); snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) if (strstr(snd_seq_client_info_get_name(ci), name)) return snd_seq_client_info_get_client(ci);
    return -1;
}
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    if (!strcmp(argv[1], "stats")) {
        FILE* f = fopen(argv[2], "r"); char kind[16]; double t; int a, b, c; std::vector<double> ts;
        while (fscanf(f, "%lf %15s %d %d %d", &t, kind, &a, &b, &c) == 5) if (!strcmp(kind, "F8") && (argc < 5 || (t >= atof(argv[3]) && t < atof(argv[4])))) ts.push_back(t);
        if (ts.size() < 50) { printf("only %zu clock ticks\n", ts.size()); return 1; }
        double span = ts.back() - ts.front(), mean = span / (double)(ts.size() - 1), ss = 0, mx = 0;
        for (size_t i = 1; i < ts.size(); i++) { double d = ts[i] - ts[i - 1]; ss += (d - mean) * (d - mean); if (fabs(d - mean) > mx) mx = fabs(d - mean); }
        double bpm = 60000.0 / (mean * 24.0);
        // drift: linear fit residual of tick time vs index (ms) -> max deviation from the straight line
        double maxres = 0; for (size_t i = 0; i < ts.size(); i++) { double r = fabs(ts[i] - (ts.front() + mean * (double)i)); if (r > maxres) maxres = r; }
        printf("ticks=%zu span=%.1fms mean=%.4fms (%.3f BPM) std=%.4fms max_dev=%.3fms max_phase_dev_from_line=%.3fms\n", ts.size(), span, mean, bpm, sqrt(ss / (double)(ts.size() - 1)), mx, maxres);
        return 0;
    }
    snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0); snd_seq_set_client_name(seq, "midiprobe");
    pmor = getenv("PROBE_CLIENT") ? atoi(getenv("PROBE_CLIENT")) : findClient("Pocket OpGorator"); if (pmor < 0) { fprintf(stderr, "PMOR not found\n"); return 2; }
    int q = snd_seq_alloc_queue(seq);
    port = snd_seq_create_simple_port(seq, "probe", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (!strcmp(argv[1], "capture")) {
        snd_seq_port_info_t* pi; snd_seq_port_info_alloca(&pi); snd_seq_get_port_info(seq, port, pi);
        snd_seq_port_info_set_timestamping(pi, 1); snd_seq_port_info_set_timestamp_real(pi, 1); snd_seq_port_info_set_timestamp_queue(pi, q); snd_seq_set_port_info(seq, port, pi);
        snd_seq_connect_from(seq, port, pmor, 0); snd_seq_start_queue(seq, q, NULL); snd_seq_drain_output(seq);
        double secs = atof(argv[2]); snd_seq_event_t* ev;
        snd_seq_queue_status_t* qs; snd_seq_queue_status_alloca(&qs);
        struct pollfd pfd; int n = snd_seq_poll_descriptors(seq, &pfd, 1, POLLIN); (void)n;
        double t0 = -1;
        for (;;) {
            if (snd_seq_event_input_pending(seq, 1) <= 0) { poll(&pfd, 1, 100); }
            while (snd_seq_event_input(seq, &ev) >= 0) {
                double t = ev->time.time.tv_sec * 1000.0 + ev->time.time.tv_nsec / 1e6; if (t0 < 0) t0 = t; t -= t0;
                switch (ev->type) {
                    case SND_SEQ_EVENT_CLOCK: printf("%.4f F8 0 0 0\n", t); break;
                    case SND_SEQ_EVENT_START: printf("%.4f FA 0 0 0\n", t); break;
                    case SND_SEQ_EVENT_CONTINUE: printf("%.4f FB 0 0 0\n", t); break;
                    case SND_SEQ_EVENT_STOP: printf("%.4f FC 0 0 0\n", t); break;
                    case SND_SEQ_EVENT_SONGPOS: printf("%.4f F2 %d 0 0\n", t, ev->data.control.value); break;
                    case SND_SEQ_EVENT_NOTEON: printf("%.4f NON %d %d %d\n", t, ev->data.note.channel + 1, ev->data.note.note, ev->data.note.velocity); break;
                    case SND_SEQ_EVENT_NOTEOFF: printf("%.4f NOFF %d %d 0\n", t, ev->data.note.channel + 1, ev->data.note.note); break;
                    case SND_SEQ_EVENT_CONTROLLER: printf("%.4f CC %d %d %d\n", t, ev->data.control.channel + 1, ev->data.control.param, ev->data.control.value); break;
                    default: break;
                }
                if (t / 1000.0 > secs) return 0;
            }
            fflush(stdout);
        }
    }
    if (!strcmp(argv[1], "clock")) {
        double bpm = atof(argv[2]), secs = atof(argv[3]); int pos = argc > 4 ? atoi(argv[4]) : -1;
        if (getenv("PROBE_SELF")) { snd_seq_port_info_t* pi; snd_seq_port_info_alloca(&pi); snd_seq_get_port_info(seq, port, pi); snd_seq_port_info_set_timestamping(pi, 1); snd_seq_port_info_set_timestamp_real(pi, 1); snd_seq_port_info_set_timestamp_queue(pi, q); snd_seq_set_port_info(seq, port, pi); snd_seq_connect_to(seq, port, snd_seq_client_id(seq), port); }
        else snd_seq_connect_to(seq, port, pmor, 0);
        snd_seq_start_queue(seq, q, NULL);   // subscription only; events go to SUBSCRIBERS (explicit dest + subscription would double-deliver)
        double tick_ms = 60000.0 / (bpm * 24.0); long total = (long)(secs * 1000.0 / tick_ms);
        auto sched = [&](snd_seq_event_type_t type, double at_ms, int value) {
            snd_seq_event_t ev; snd_seq_ev_clear(&ev); ev.type = type; snd_seq_ev_set_source(&ev, port); snd_seq_ev_set_subs(&ev);
            snd_seq_real_time_t rt; rt.tv_sec = (unsigned)(at_ms / 1000.0); rt.tv_nsec = (unsigned)((at_ms - rt.tv_sec * 1000.0) * 1e6);
            snd_seq_ev_schedule_real(&ev, q, 0, &rt); if (type == SND_SEQ_EVENT_SONGPOS) ev.data.control.value = value;
            snd_seq_event_output(seq, &ev);
        };
        double t0 = 200.0;
        { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); printf("CLOCK_START_MONO_MS %.3f\n", ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6 + t0); fflush(stdout); }
        if (pos >= 0) { sched(SND_SEQ_EVENT_SONGPOS, t0 - 3, pos); sched(SND_SEQ_EVENT_CONTINUE, t0, 0); } else sched(SND_SEQ_EVENT_START, t0, 0);
        for (long i = 0; i < total; i++) { sched(SND_SEQ_EVENT_CLOCK, t0 + (double)i * tick_ms, 0); if (i % 512 == 511) snd_seq_drain_output(seq); }
        sched(SND_SEQ_EVENT_STOP, t0 + (double)total * tick_ms, 0);
        snd_seq_drain_output(seq);
        if (getenv("PROBE_SELF")) {      // loopback: read our own events back with real-time stamps (sender jitter check)
            double sum = 0, ss = 0, mx = 0; double last = -1; long n = 0; snd_seq_event_t* ev; struct pollfd pfd; snd_seq_poll_descriptors(seq, &pfd, 1, POLLIN);
            std::vector<double> ts;
            while (n < total) { if (snd_seq_event_input_pending(seq, 1) <= 0) poll(&pfd, 1, 200); while (snd_seq_event_input(seq, &ev) >= 0) { if (ev->type == SND_SEQ_EVENT_CLOCK) { ts.push_back(ev->time.time.tv_sec * 1000.0 + ev->time.time.tv_nsec / 1e6); n++; } } }
            for (size_t i = 1; i < ts.size(); i++) { double d = ts[i] - ts[i - 1] - tick_ms; ss += d * d; if (fabs(d) > mx) mx = fabs(d); sum += d; }
            printf("sender loopback: %zu ticks, interval error mean %.4f ms std %.4f ms max %.3f ms (ideal %.4f)\n", ts.size(), sum / (double)(ts.size() - 1), sqrt(ss / (double)(ts.size() - 1)), mx, tick_ms); return 0;
        }
        snd_seq_sync_output_queue(seq);
        return 0;
    }
    return 1;
}
