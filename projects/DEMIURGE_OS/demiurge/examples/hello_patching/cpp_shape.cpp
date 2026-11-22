// ~/demiurge/examples/hello_patching/cpp_shape.cpp
//
// DEMIURGE hello_patching — stage 6 of 7: C++ WAVESHAPER / DRIVE.
//
// Takes stereo in from stage 5 (Csound reverb), runs it through a
// self-modulating tanh waveshaper with LFO-driven drive and bias, then
// out to stage 7 (Strudel is MIDI-only; stays out of audio chain).
//
// Generative motion: drive and bias follow two slow incommensurate LFOs
// so the saturation breathes — the effect audibly comes and goes.
//
// CC60 = drive manual override (0..1)
// CC61 = bias  manual override (0..1)
//
// Build (on Pi):
//   g++ -O2 -o cpp_shape cpp_shape.cpp -ljack -lasound -lm -lpthread

#include <jack/jack.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

static jack_client_t *client = nullptr;
static jack_port_t *in_l = nullptr, *in_r = nullptr;
static jack_port_t *out_l = nullptr, *out_r = nullptr;

static double sr = 48000.0;

// LFO state
static double lfo1_phase = 0.0;   // drive LFO
static double lfo2_phase = 0.0;   // bias  LFO

// Shared MIDI state (CC overrides)
static volatile float cc_drive = 0.5f;
static volatile float cc_bias  = 0.5f;

static int process(jack_nframes_t nframes, void *) {
    auto *il = (jack_default_audio_sample_t *)jack_port_get_buffer(in_l,  nframes);
    auto *ir = (jack_default_audio_sample_t *)jack_port_get_buffer(in_r,  nframes);
    auto *ol = (jack_default_audio_sample_t *)jack_port_get_buffer(out_l, nframes);
    auto *o_r = (jack_default_audio_sample_t *)jack_port_get_buffer(out_r, nframes);

    const double TWO_PI = 6.28318530717958647692;
    const double inv_sr = 1.0 / sr;

    // LFO rates — slow, irrational relationship so they never lock
    const double r1 = 0.09;   // Hz
    const double r2 = 0.13;   // Hz

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        lfo1_phase += TWO_PI * r1 * inv_sr;
        lfo2_phase += TWO_PI * r2 * inv_sr;
        if (lfo1_phase > TWO_PI) lfo1_phase -= TWO_PI;
        if (lfo2_phase > TWO_PI) lfo2_phase -= TWO_PI;

        double lfo_d = 0.5 + 0.5 * sin(lfo1_phase);   // 0..1
        double lfo_b = sin(lfo2_phase);               // -1..1

        // Drive: 1.0 (clean) .. 12.0 (crunchy), biased by CC60
        double drive = 1.0 + lfo_d * 7.0 + (cc_drive - 0.5f) * 6.0f;
        if (drive < 1.0) drive = 1.0;
        if (drive > 14.0) drive = 14.0;

        // Bias: small DC-ish asymmetry, ±0.15
        double bias = lfo_b * 0.10 + (cc_bias - 0.5f) * 0.2f;

        // Makeup gain falls as drive rises, keeps level roughly constant
        double makeup = 1.0 / (0.7 + 0.25 * drive);

        double xL = (double)il[i] + bias;
        double xR = (double)ir[i] + bias;

        double yL = tanh(xL * drive) * makeup;
        double yR = tanh(xR * drive) * makeup;

        // Dry passthrough blend so the chain never fully colors out
        double dry = 0.35;
        double wet = 0.85;

        double mL = il[i] * dry + yL * wet;
        double mR = ir[i] * dry + yR * wet;

        if (mL >  0.98) mL =  0.98; if (mL < -0.98) mL = -0.98;
        if (mR >  0.98) mR =  0.98; if (mR < -0.98) mR = -0.98;

        ol[i]  = (float)mL;
        o_r[i] = (float)mR;
    }
    return 0;
}

// -------- MIDI reader (ALSA seq) --------
static snd_seq_t *seq = nullptr;

static void *midi_thread(void *) {
    snd_seq_event_t *ev = nullptr;
    while (snd_seq_event_input(seq, &ev) >= 0 && ev) {
        if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            int cc  = ev->data.control.param;
            int val = ev->data.control.value;
            float v = val / 127.0f;
            if (cc == 60) cc_drive = v;
            if (cc == 61) cc_bias  = v;
        }
    }
    return nullptr;
}

static void midi_setup() {
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        fprintf(stderr, "cpp_shape: snd_seq_open failed (no MIDI)\n");
        seq = nullptr; return;
    }
    snd_seq_set_client_name(seq, "cpp_shape");
    snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    pthread_t th;
    pthread_create(&th, nullptr, midi_thread, nullptr);
}

static volatile sig_atomic_t running = 1;
static void on_sig(int) { running = 0; }

int main() {
    jack_status_t status;
    client = jack_client_open("cpp_shape", JackNullOption, &status);
    if (!client) { fprintf(stderr, "cpp_shape: jack_client_open failed\n"); return 1; }

    sr = jack_get_sample_rate(client);
    jack_set_process_callback(client, process, nullptr);

    in_l  = jack_port_register(client, "in_1",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    in_r  = jack_port_register(client, "in_2",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    out_l = jack_port_register(client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_r = jack_port_register(client, "out_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate(client)) {
        fprintf(stderr, "cpp_shape: jack_activate failed\n"); return 1;
    }
    fprintf(stderr, "cpp_shape: running at %.0f Hz\n", sr);

    midi_setup();

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);
    while (running) sleep(1);

    jack_client_close(client);
    if (seq) snd_seq_close(seq);
    return 0;
}
