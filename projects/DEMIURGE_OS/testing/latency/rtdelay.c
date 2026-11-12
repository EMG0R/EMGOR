// demiurge-rtdelay — per-shot TRUE round-trip latency, integer samples.
//
// Why this exists, when jack_iodelay is already on the Pi:
//
//   jack_iodelay reports a CONTINUOUS phase-tracked estimate. On this rig it
//   drifts monotonically (~1.3 frames/s) because PipeWire treats the card's
//   capture and playback as two nodes and adaptively resamples the follower.
//   A drifting estimate cannot give you a mean and a spread that mean anything.
//
//   This tool fires N INDEPENDENT impulses. Each shot is measured on its own,
//   in integer samples, from the exact frame the click left the graph to the
//   exact frame its return crossed threshold. Drift becomes a visible spread
//   across shots instead of a silently sliding number.
//
// What it measures: out port -> (graph) -> DAC -> cable -> ADC -> (graph) -> in
// port. That is the honest round trip INCLUDING the converters. A chain stage
// that sits one client deep in the graph (which is what csound is, in direct
// in/direct out mode) sees the same path, so this number is the instrument's
// playing latency, not a synthetic floor.
//
// Out-of-band by construction: it makes no links itself. The runner script
// wires it, measures, and restores. It never touches live.conf.
//
// Build:  gcc -O2 -o rtdelay rtdelay.c -ljack -lm
// Run:    pw-jack ./rtdelay [shots] [gap_ms]
// Output: one line per shot "shot <n> <frames> <ms> peak <p>", then a summary.

#include <jack/jack.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WINDOW_FR   9600   // 200 ms @ 48k: capture window per shot
#define BURST_FR       8   // broadband click: 8 alternating full-scale samples
#define MAX_SHOTS   1000

static jack_client_t *client;
static jack_port_t *port_in, *port_out;

// --- shot state machine, owned by the RT callback -------------------------
// PHASE_WAIT  : counting down the inter-shot gap (lets the room/ring settle)
// PHASE_EMIT  : writing the burst, recording the absolute emit frame
// PHASE_LISTEN: filling capture[] for WINDOW_FR frames after the emit
// PHASE_DONE  : hand the window to the main thread for analysis
enum { PHASE_WAIT, PHASE_EMIT, PHASE_LISTEN, PHASE_DONE };
static volatile int phase = PHASE_WAIT;

static float capture[WINDOW_FR];
static volatile int cap_n;        // frames written into capture[] so far
static int gap_left;              // frames remaining in the WAIT phase
static int gap_frames;
static int burst_left;

static int process(jack_nframes_t nframes, void *arg) {
    (void)arg;
    float *in  = (float *)jack_port_get_buffer(port_in,  nframes);
    float *out = (float *)jack_port_get_buffer(port_out, nframes);
    memset(out, 0, sizeof(float) * nframes);

    for (jack_nframes_t i = 0; i < nframes; i++) {
        switch (phase) {
        case PHASE_WAIT:
            if (--gap_left <= 0) { phase = PHASE_EMIT; burst_left = BURST_FR; cap_n = 0; }
            break;
        case PHASE_EMIT:
            // The click starts at THIS frame. Everything downstream is measured
            // relative to it, and we start filling the capture window on the
            // very same frame, so index 0 of capture[] == emit instant. The
            // reported offset is therefore a direct sample count, with no
            // constant to subtract.
            out[i] = (burst_left % 2) ? 0.9f : -0.9f;
            if (cap_n < WINDOW_FR) capture[cap_n++] = in[i];
            if (--burst_left <= 0) phase = PHASE_LISTEN;
            break;
        case PHASE_LISTEN:
            if (cap_n < WINDOW_FR) capture[cap_n++] = in[i];
            if (cap_n >= WINDOW_FR) phase = PHASE_DONE;
            break;
        case PHASE_DONE:
        default:
            break; // main thread is analysing; stay silent
        }
    }
    return 0;
}

// Onset of the returned click inside capture[].
//
// Peak-relative, not absolute: the DAC reconstruction filter and the ADC
// anti-alias filter smear an 8-sample click into a short ringing burst whose
// absolute amplitude depends on the card's analogue gain, which we do not
// control. Finding the peak first and then walking BACK to the first sample
// above a fixed FRACTION of it makes the threshold self-calibrating, so the
// number is comparable across gain settings and across cards.
static int find_onset(const float *buf, int n, float *peak_out) {
    float peak = 0.0f;
    int peak_i = -1;
    for (int i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) { peak = a; peak_i = i; }
    }
    *peak_out = peak;
    if (peak < 0.005f) return -1;   // nothing came back: cable out, or muted
    float thresh = peak * 0.2f;
    for (int i = 0; i <= peak_i; i++) {
        if (fabsf(buf[i]) >= thresh) return i;
    }
    return peak_i;
}

static int cmp_int(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

int main(int argc, char **argv) {
    int shots   = (argc > 1) ? atoi(argv[1]) : 30;
    int gap_ms  = (argc > 2) ? atoi(argv[2]) : 400;
    if (shots < 1 || shots > MAX_SHOTS) shots = 30;

    client = jack_client_open("rtdelay", JackNoStartServer, NULL);
    if (!client) { fprintf(stderr, "rtdelay: cannot connect to JACK/PipeWire\n"); return 1; }

    jack_set_process_callback(client, process, NULL);
    port_in  = jack_port_register(client, "in",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    port_out = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!port_in || !port_out) { fprintf(stderr, "rtdelay: port registration failed\n"); return 1; }

    jack_nframes_t sr = jack_get_sample_rate(client);
    jack_nframes_t bs = jack_get_buffer_size(client);
    gap_frames = (int)((long)gap_ms * sr / 1000);
    gap_left   = gap_frames;

    if (jack_activate(client)) { fprintf(stderr, "rtdelay: activate failed\n"); return 1; }

    printf("rtdelay: rate %u, quantum %u, %d shots, %d ms gap\n", sr, bs, shots, gap_ms);
    fflush(stdout);

    int results[MAX_SHOTS];
    int got = 0, missed = 0;

    for (int s = 0; s < shots; s++) {
        while (phase != PHASE_DONE) usleep(2000);

        float peak = 0.0f;
        int onset = find_onset(capture, WINDOW_FR, &peak);
        if (onset < 0) {
            printf("shot %d MISS (peak %.4f — loopback cable connected? output muted?)\n", s + 1, peak);
            missed++;
        } else {
            results[got++] = onset;
            printf("shot %d %d frames %.3f ms peak %.3f\n",
                   s + 1, onset, 1000.0 * onset / (double)sr, peak);
        }
        fflush(stdout);

        // Re-arm. Ordering matters: reset the counters BEFORE leaving
        // PHASE_DONE, or the RT thread can enter EMIT against a stale gap.
        cap_n = 0;
        gap_left = gap_frames;
        phase = PHASE_WAIT;
    }

    jack_deactivate(client);
    jack_client_close(client);

    if (got == 0) { printf("\nrtdelay: NO SHOTS RETURNED — check the loopback cable.\n"); return 2; }

    qsort(results, got, sizeof(int), cmp_int);
    double sum = 0.0;
    for (int i = 0; i < got; i++) sum += results[i];
    double mean = sum / got;
    double var = 0.0;
    for (int i = 0; i < got; i++) var += (results[i] - mean) * (results[i] - mean);
    double sd = sqrt(var / got);

    printf("\n=== rtdelay summary (rate %u, quantum %u) ===\n", sr, bs);
    printf("shots      %d ok, %d missed\n", got, missed);
    printf("min        %d fr  %.3f ms\n", results[0], 1000.0 * results[0] / sr);
    printf("median     %d fr  %.3f ms\n", results[got / 2], 1000.0 * results[got / 2] / sr);
    printf("mean       %.1f fr  %.3f ms\n", mean, 1000.0 * mean / sr);
    printf("max        %d fr  %.3f ms\n", results[got - 1], 1000.0 * results[got - 1] / sr);
    printf("sd         %.1f fr  %.3f ms\n", sd, 1000.0 * sd / sr);
    printf("spread     %d fr  %.3f ms\n", results[got - 1] - results[0],
           1000.0 * (results[got - 1] - results[0]) / sr);
    return 0;
}
