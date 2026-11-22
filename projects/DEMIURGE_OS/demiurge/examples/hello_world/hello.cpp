// ~/demiurge/examples/hello_world/hello.cpp
//
// DEMIURGE C++ hello_world — generative detuned-sine drone via JACK,
// with ALSA seq MIDI in so it can tempo-lock to the master clock
// (ch16 CC119) and respond to external note-ons.
//
// Registers as JACK client `cpp_hello` with 2 output ports. Opens an
// ALSA seq client and subscribes to "Midi Through" (the shared DEMIURGE
// bus, client 14:0). Note-on retunes the root; CC1 morphs partial gain
// spread; CC7 = master volume; ch16 CC119 updates the master BPM that
// drives the per-partial shimmer LFO rates.
//
// BUILD (on Pi):
//   g++ -O2 -std=c++17 hello.cpp -o hello \
//       $(pkg-config --cflags --libs jack alsa) -lpthread -lm
//
// RUN:
//   LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack ./hello
//
// Drop this .cpp path into a `chain =` block in ~/demiurge/live.conf and
// the launcher compiles it on first run, then links its outputs into the
// chain terminating at demiurge-sink.

#include <jack/jack.h>
#include <alsa/asoundlib.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <thread>
#include <unistd.h>

static jack_client_t *client = nullptr;
static jack_port_t *out_l = nullptr;
static jack_port_t *out_r = nullptr;
static double sr = 48000.0;

// Generative state — 4 partials, each with its own shimmer LFO.
struct Partial {
    double phase;
    double lfo_phase;
    double lfo_rate;
    double ratio;
    double base_gain;
};

static Partial P[4] = {
    { 0, 0, 0.11, 1.000, 0.32 },
    { 0, 0, 0.17, 2.003, 0.20 },
    { 0, 0, 0.23, 3.011, 0.13 },
    { 0, 0, 0.31, 4.027, 0.08 },
};

// Shared state, written by the MIDI thread, read by the JACK RT thread.
static std::atomic<double> root_hz{110.0};
static std::atomic<double> root_target{110.0};
static std::atomic<double> bpm{110.0};
static std::atomic<double> morph{0.5};
static std::atomic<double> volume{0.85};

static int drift_counter = 0;

static inline float softclip(float x) { return tanhf(x); }

static int process(jack_nframes_t nframes, void *) {
    auto *bl = (jack_default_audio_sample_t *)jack_port_get_buffer(out_l, nframes);
    auto *br = (jack_default_audio_sample_t *)jack_port_get_buffer(out_r, nframes);

    const double inv_sr = 1.0 / sr;
    const double TWO_PI = 6.28318530717958647692;

    double rh = root_hz.load();
    double rt = root_target.load();
    double bpmv = bpm.load();
    double mp = morph.load();
    double vl = volume.load();

    // BPM scales the shimmer LFOs (faster clock -> quicker shimmer).
    double bpm_scale = bpmv / 110.0;

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        rh += (rt - rh) * 0.00005;

        float sL = 0.0f, sR = 0.0f;
        for (int k = 0; k < 4; ++k) {
            Partial &p = P[k];
            p.lfo_phase += TWO_PI * p.lfo_rate * bpm_scale * inv_sr;
            if (p.lfo_phase > TWO_PI) p.lfo_phase -= TWO_PI;
            double shimmer = 0.5 + 0.5 * sin(p.lfo_phase);
            double g = p.base_gain * (0.35 + 0.65 * shimmer);
            // morph widens the gain spread between partials
            double spread = 1.0 + (k * 0.25 - 0.375) * (mp * 1.4);
            g *= spread > 0 ? spread : 0;

            double freq = rh * p.ratio;
            p.phase += TWO_PI * freq * inv_sr;
            if (p.phase > TWO_PI) p.phase -= TWO_PI;
            double s = sin(p.phase) * g;

            if (k & 1) { sL += 0.6 * s; sR += 1.0 * s; }
            else       { sL += 1.0 * s; sR += 0.6 * s; }
        }

        bl[i] = softclip(sL * 0.55f * (float)vl);
        br[i] = softclip(sR * 0.55f * (float)vl);

        drift_counter++;
        if (drift_counter > (int)(sr * 3)) {
            drift_counter = 0;
            int step = (rand() % 5) - 2;
            if ((rand() % 10) == 0) step += (rand() % 2) ? 12 : -12;
            double mult = pow(2.0, step / 12.0);
            double nt = rt * mult;
            if (nt < 55.0)  nt = 55.0;
            if (nt > 330.0) nt = 330.0;
            rt = nt;
        }
    }
    root_hz.store(rh);
    root_target.store(rt);
    return 0;
}

// === ALSA seq MIDI input ===
static volatile sig_atomic_t running = 1;
static void on_sig(int) { running = 0; }

static void midi_thread() {
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        fprintf(stderr, "cpp_hello: snd_seq_open failed\n");
        return;
    }
    snd_seq_set_client_name(seq, "cpp_hello");
    int port = snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION);
    // Subscribe to Midi Through (client 14:0)
    snd_seq_addr_t src = {14, 0};
    snd_seq_addr_t dst = {(unsigned char)snd_seq_client_id(seq), (unsigned char)port};
    snd_seq_port_subscribe_t *sub;
    snd_seq_port_subscribe_alloca(&sub);
    snd_seq_port_subscribe_set_sender(sub, &src);
    snd_seq_port_subscribe_set_dest(sub, &dst);
    snd_seq_subscribe_port(seq, sub);
    fprintf(stderr, "cpp_hello: subscribed to Midi Through 14:0\n");

    while (running) {
        snd_seq_event_t *ev;
        if (snd_seq_event_input(seq, &ev) < 0) continue;
        if (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity > 0) {
            double hz = 440.0 * pow(2.0, (ev->data.note.note - 69) / 12.0);
            root_target.store(hz);
        } else if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            int ch  = ev->data.control.channel;
            int num = ev->data.control.param;
            int val = ev->data.control.value;
            if (ch == 0 && num == 1) morph.store(val / 127.0);
            if (ch == 0 && num == 7) volume.store(val / 127.0);
            // Master BPM on ch16 CC119 (0-indexed channel 15)
            if (ch == 15 && num == 119) bpm.store(40.0 + (val / 127.0) * 200.0);
        }
    }
    snd_seq_close(seq);
}

int main() {
    jack_status_t status;
    client = jack_client_open("cpp_hello", JackNullOption, &status);
    if (!client) { fprintf(stderr, "cpp_hello: jack_client_open failed\n"); return 1; }

    sr = jack_get_sample_rate(client);
    jack_set_process_callback(client, process, nullptr);

    out_l = jack_port_register(client, "out_1",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_r = jack_port_register(client, "out_2",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate(client)) {
        fprintf(stderr, "cpp_hello: jack_activate failed\n"); return 1;
    }
    fprintf(stderr, "cpp_hello: running at %.0f Hz\n", sr);

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);

    std::thread mt(midi_thread);
    while (running) sleep(1);
    mt.join();

    jack_client_close(client);
    return 0;
}
