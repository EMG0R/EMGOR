// ~/demiurge/examples/hello_patching/cpp_crush.cpp
//
// DEMIURGE hello_patching — stage 5 of 6: C++ GENERATIVE BITCRUSHER.
//
// Reads stereo in from stage 4 (Faust delay), runs a bitcrusher
// (sample-rate + bit-depth reduction) whose on/off state is re-rolled
// once per beat from the shared MIDI clock — NOT per-note. This means
// the bitcrush is "only on some of the time" in the chain: each beat
// gets an independent 50/50 coin flip, and when crushed the bit depth
// and SR divider are fresh random values. The result: the crushed and
// clean passes are both obvious in the mix.
//
// MIDI pool access:
//   - Subscribes its ALSA seq input port from "Midi Through" (the
//     shared DEMIURGE bus) directly on startup. Does not rely on the
//     launcher's wire_midi_bus.
//   - Reads MIDI realtime 0xF8 clock ticks (24 PPQN) for beat timing.
//   - Reads ch16 CC119 for BPM broadcast.
//
// PARAMETER (docs/parameters.md): /cpp/crush (0..1) = probability that a beat
// is crushed (default 0.125 = the old 1-in-8). The pool sends
// `/p <path:s> <val:f>` over UDP to this stage's manifest port (stage 5 ->
// 9005, env DEMIURGE_PARAM_PORT overrides) and this stage reports its value
// with `/pout <path:s> <val:f>` to DEMIURGE_POOL_HOST:DEMIURGE_POOL_PORT
// (default 127.0.0.1:9102). C++ has no hook library: the OSC here is the
// minimal ",sf" encode/decode over a plain UDP socket (no liblo dependency).
//
// Build (on Pi):
//   g++ -O2 -o cpp_crush cpp_crush.cpp -ljack -lasound -lm -lpthread

#include <jack/jack.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <atomic>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

static jack_client_t *client = nullptr;
static jack_port_t *in_l = nullptr, *in_r = nullptr;
static jack_port_t *out_l = nullptr, *out_r = nullptr;

static double sr = 48000.0;

// Crusher state — updated from MIDI thread, read from audio thread.
// Plain atomics suffice for our coarse-grained values.
static std::atomic<int>    g_crush_on{0};       // 0 = passthrough, 1 = crush
static std::atomic<int>    g_bits{5};           // effective bit depth 2..14
static std::atomic<int>    g_sr_div{6};         // SR divider 1..24
static std::atomic<double> g_bpm{110.0};
static std::atomic<float>  g_crush_prob{0.125f}; // pool: /cpp/crush

// Sample-and-hold state (audio-thread-local)
static int    sh_counter = 0;
static float  sh_l = 0.0f, sh_r = 0.0f;

static inline float crush_sample(float x, int bits) {
    float levels = powf(2.0f, (float)(bits - 1)) - 1.0f;
    float s = (x >= 0.0f) ? 1.0f : -1.0f;
    return s * floorf(fabsf(x) * levels + 0.5f) / levels;
}

static int process(jack_nframes_t nframes, void *) {
    auto *il  = (jack_default_audio_sample_t *)jack_port_get_buffer(in_l,  nframes);
    auto *ir  = (jack_default_audio_sample_t *)jack_port_get_buffer(in_r,  nframes);
    auto *ol  = (jack_default_audio_sample_t *)jack_port_get_buffer(out_l, nframes);
    auto *o_r = (jack_default_audio_sample_t *)jack_port_get_buffer(out_r, nframes);

    int on     = g_crush_on.load();
    int bits   = g_bits.load();
    int sr_div = g_sr_div.load();
    if (sr_div < 1)  sr_div = 1;
    if (sr_div > 64) sr_div = 64;

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        float xL = il[i], xR = ir[i];

        if (!on) {
            // Clean passthrough, no DSP cost.
            ol[i]  = xL;
            o_r[i] = xR;
            continue;
        }

        // SR reduce via sample-and-hold
        if (sh_counter == 0) {
            sh_l = xL;
            sh_r = xR;
        }
        sh_counter = (sh_counter + 1) % sr_div;

        // Bit crush the held sample
        float yL = crush_sample(sh_l, bits);
        float yR = crush_sample(sh_r, bits);

        // Mostly-dry blend so when a crush beat hits it colors the
        // signal without destroying the sine pad underneath.
        ol[i]  = yL * 0.35f + xL * 0.65f;
        o_r[i] = yR * 0.35f + xR * 0.65f;
    }
    return 0;
}

// -------- MIDI reader (ALSA seq) --------
static snd_seq_t *seq = nullptr;
static int       seq_in_port = -1;
static int       g_tick_count = 0;

static void roll_crush_params() {
    // Rare flicker: roughly 1-in-8 beats crush, and keep it light.
    // When crushed, stay mellow (higher bit depth, smaller SR divider)
    // so the effect is a momentary color rather than a destructive hit.
    int on = ((rand() % 1000) < (int)(g_crush_prob.load() * 1000.0f)) ? 1 : 0;
    g_crush_on.store(on);
    if (on) {
        // Bit depth 8..13 (gentle)
        g_bits.store(8 + (rand() % 6));
        // SR divider 2..5
        g_sr_div.store(2 + (rand() % 4));
    }
    fprintf(stderr, "cpp_crush: beat roll on=%d bits=%d sr_div=%d\n",
            on, g_bits.load(), g_sr_div.load());
}

static void *midi_thread(void *) {
    snd_seq_event_t *ev = nullptr;
    while (snd_seq_event_input(seq, &ev) >= 0 && ev) {
        // 0xF8 = MIDI realtime Clock tick. 24 PPQN. Re-roll the crush
        // parameters once per beat (every 24 ticks).
        if (ev->type == SND_SEQ_EVENT_CLOCK) {
            g_tick_count++;
            if (g_tick_count % 24 == 0) {
                roll_crush_params();
            }
        } else if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
            int ch  = ev->data.control.channel;
            int cc  = ev->data.control.param;
            int val = ev->data.control.value;
            // ch16 (seq idx 15) CC119 = global BPM broadcast
            if (ch == 15 && cc == 119) {
                double bpm = 40.0 + (val / 127.0) * 200.0;
                g_bpm.store(bpm);
            }
        }
    }
    return nullptr;
}

// Explicitly subscribe our input port from "Midi Through" so we don't
// depend on the launcher's wire_midi_bus heuristic.
static void subscribe_from_midi_through() {
    if (!seq || seq_in_port < 0) return;

    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t   *pinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    int found_client = -1, found_port = -1;
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int cid = snd_seq_client_info_get_client(cinfo);
        const char *cname = snd_seq_client_info_get_name(cinfo);
        if (!cname) continue;
        if (strstr(cname, "Midi Through") || strstr(cname, "midi through")) {
            snd_seq_port_info_set_client(pinfo, cid);
            snd_seq_port_info_set_port(pinfo, -1);
            while (snd_seq_query_next_port(seq, pinfo) >= 0) {
                int caps = snd_seq_port_info_get_capability(pinfo);
                if (caps & SND_SEQ_PORT_CAP_READ) {
                    found_client = cid;
                    found_port   = snd_seq_port_info_get_port(pinfo);
                    break;
                }
            }
            if (found_client >= 0) break;
        }
    }

    if (found_client < 0) {
        fprintf(stderr, "cpp_crush: Midi Through not found — "
                        "clock/CC will only arrive via launcher wire-up\n");
        return;
    }

    snd_seq_addr_t sender  = {(uint8_t)found_client, (uint8_t)found_port};
    snd_seq_addr_t dest    = {(uint8_t)snd_seq_client_id(seq),
                              (uint8_t)seq_in_port};
    snd_seq_port_subscribe_t *sub;
    snd_seq_port_subscribe_alloca(&sub);
    snd_seq_port_subscribe_set_sender(sub, &sender);
    snd_seq_port_subscribe_set_dest(sub, &dest);
    if (snd_seq_subscribe_port(seq, sub) < 0) {
        fprintf(stderr, "cpp_crush: subscribe Midi Through -> in failed\n");
    } else {
        fprintf(stderr, "cpp_crush: subscribed from Midi Through (%d:%d)\n",
                found_client, found_port);
    }
}

static void midi_setup() {
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "cpp_crush: snd_seq_open failed (no MIDI)\n");
        seq = nullptr; return;
    }
    snd_seq_set_client_name(seq, "cpp_crush");
    seq_in_port = snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    subscribe_from_midi_through();
    pthread_t th;
    pthread_create(&th, nullptr, midi_thread, nullptr);
}

// -------- Parameter pool (OSC over UDP) --------
static const char *CRUSH_PATH = "/cpp/crush";
static int  g_param_sock = -1;
static sockaddr_in g_pool_addr;

// OSC string: bytes + NUL, padded to a multiple of 4.
static size_t osc_put_str(char *dst, const char *s) {
    size_t n = strlen(s) + 1;
    memcpy(dst, s, n);
    size_t pad = (4 - (n % 4)) % 4;
    memset(dst + n, 0, pad);
    return n + pad;
}

static void pool_out(const char *path, float v) {
    if (g_param_sock < 0) return;
    char buf[128]; size_t o = 0;
    o += osc_put_str(buf + o, "/pout");
    o += osc_put_str(buf + o, ",sf");
    o += osc_put_str(buf + o, path);
    uint32_t bits; memcpy(&bits, &v, 4); bits = htonl(bits);
    memcpy(buf + o, &bits, 4); o += 4;
    sendto(g_param_sock, buf, o, 0, (sockaddr *)&g_pool_addr, sizeof g_pool_addr);
}

static void *param_thread(void *) {
    char buf[512];
    while (true) {
        ssize_t n = recv(g_param_sock, buf, sizeof buf - 1, 0);
        if (n < 12) continue;
        buf[n] = 0;
        // "/p\0\0" ",sf\0" "<path>\0.." <float32 BE>
        if (strcmp(buf, "/p") != 0) continue;
        size_t o = 4;
        if (strncmp(buf + o, ",sf", 3) != 0) continue;
        o += 4;
        const char *path = buf + o;
        size_t pl = strlen(path) + 1; pl += (4 - (pl % 4)) % 4;
        o += pl;
        if (o + 4 > (size_t)n) continue;
        uint32_t bits; memcpy(&bits, buf + o, 4); bits = ntohl(bits);
        float v; memcpy(&v, &bits, 4);
        if (strcmp(path, CRUSH_PATH) == 0) {
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            g_crush_prob.store(v);
            pool_out(CRUSH_PATH, v);
        }
    }
    return nullptr;
}

static void param_setup() {
    int port = 9005;
    if (const char *e = getenv("DEMIURGE_PARAM_PORT")) port = atoi(e);
    const char *host = getenv("DEMIURGE_POOL_HOST");
    int pport = 9102;
    if (const char *e = getenv("DEMIURGE_POOL_PORT")) pport = atoi(e);

    g_param_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_param_sock < 0) { fprintf(stderr, "cpp_crush: param socket failed\n"); return; }
    sockaddr_in a{}; a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons((uint16_t)port);
    if (bind(g_param_sock, (sockaddr *)&a, sizeof a) < 0) {
        fprintf(stderr, "cpp_crush: param bind %d failed\n", port);
        close(g_param_sock); g_param_sock = -1; return;
    }
    g_pool_addr = sockaddr_in{}; g_pool_addr.sin_family = AF_INET;
    g_pool_addr.sin_port = htons((uint16_t)pport);
    inet_pton(AF_INET, host ? host : "127.0.0.1", &g_pool_addr.sin_addr);
    pthread_t th; pthread_create(&th, nullptr, param_thread, nullptr);
    pool_out(CRUSH_PATH, g_crush_prob.load());   // announce the default
    fprintf(stderr, "cpp_crush: params on udp %d, reporting to pool :%d\n", port, pport);
}

static volatile sig_atomic_t running = 1;
static void on_sig(int) { running = 0; }

int main() {
    srand((unsigned)time(nullptr));

    // DEMIURGE_NO_AUDIO=1: pool link only (no JACK, no ALSA seq). Used by the
    // offline checker (check.sh) so it never touches the live audio graph.
    const bool no_audio = getenv("DEMIURGE_NO_AUDIO") != nullptr;
    if (!no_audio) {
        jack_status_t status;
        client = jack_client_open("cpp_crush", JackNullOption, &status);
        if (!client) { fprintf(stderr, "cpp_crush: jack_client_open failed\n"); return 1; }

        sr = jack_get_sample_rate(client);
        jack_set_process_callback(client, process, nullptr);

        in_l  = jack_port_register(client, "in_1",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
        in_r  = jack_port_register(client, "in_2",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
        out_l = jack_port_register(client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        out_r = jack_port_register(client, "out_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        if (jack_activate(client)) {
            fprintf(stderr, "cpp_crush: jack_activate failed\n"); return 1;
        }
        fprintf(stderr, "cpp_crush: running at %.0f Hz\n", sr);

    }
    if (!no_audio) midi_setup();
    param_setup();

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);
    while (running) sleep(1);

    if (client) jack_client_close(client);
    if (seq) snd_seq_close(seq);
    return 0;
}
