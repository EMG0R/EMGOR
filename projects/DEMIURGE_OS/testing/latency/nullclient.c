// nullclient.c — the zero-cost reference client for `rtdelay-through.sh`.
//
// WHY THIS EXISTS
//
// `rtdelay.sh` and `rtdelay-through.sh` measure two DIFFERENT graphs:
//
//   rtdelay.sh          rtdelay:out -> DAC -> cable -> ADC -> rtdelay:in
//   rtdelay-through.sh  rtdelay:out -> DAC -> cable -> ADC -> <client> -> rtdelay:in
//
// It is tempting to call the first one "the floor" and subtract it from the
// second to get "what the client costs". That subtraction is wrong, and it
// produced a genuinely impossible result while measuring the embedded Csound
// host: a round trip THROUGH an engine that came out ~40 frames BELOW the
// floor, reproducibly, across three separate runs.
//
// The two numbers are not comparable because inserting a client changes the
// graph's topological order, and with it PipeWire's scheduling of rtdelay
// relative to the capture node. The floor is a property of the floor's graph,
// not a hardware constant you can subtract from anything.
//
// So: measure the through-graph's own baseline, with a client in the path that
// provably adds nothing. This is that client — copy input to output, in the
// process callback, nothing else. Whatever it reads is the through topology's
// zero point, and THAT is what a real engine should be differenced against.
//
//   ./nullclient &                       # registers as `nullclient`
//   RTIN=nullclient:output1 ./rtdelay-through.sh 40 250
//
// Build:  gcc -O2 -o nullclient nullclient.c -ljack
// Run:    pw-jack ./nullclient      (needs the pipewire-jack shim, like rtdelay)

#include <jack/jack.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static jack_client_t *client;
static jack_port_t *in[2], *out[2];
static volatile sig_atomic_t stop = 0;

static void on_signal(int s) { (void)s; stop = 1; }

static int process(jack_nframes_t n, void *arg)
{
    (void)arg;
    for (int c = 0; c < 2; c++) {
        const float *i = jack_port_get_buffer(in[c], n);
        float *o = jack_port_get_buffer(out[c], n);
        memcpy(o, i, n * sizeof(float));
    }
    return 0;
}

int main(void)
{
    jack_status_t st;
    client = jack_client_open("nullclient", JackNoStartServer | JackUseExactName, &st);
    if (!client) { fprintf(stderr, "nullclient: jack_client_open failed (0x%x)\n", st); return 1; }

    // Same port names as the engines under test, so the runner scripts and the
    // RTIN= convention work identically against it.
    in[0]  = jack_port_register(client, "input1",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    in[1]  = jack_port_register(client, "input2",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    out[0] = jack_port_register(client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out[1] = jack_port_register(client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!in[0] || !in[1] || !out[0] || !out[1]) { fprintf(stderr, "nullclient: port registration failed\n"); return 2; }

    jack_set_process_callback(client, process, NULL);
    if (jack_activate(client)) { fprintf(stderr, "nullclient: activate failed\n"); return 3; }

    fprintf(stderr, "nullclient: up, quantum=%u rate=%u\n",
            jack_get_buffer_size(client), jack_get_sample_rate(client));
    fflush(stderr);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    while (!stop) usleep(100000);

    jack_deactivate(client);
    jack_client_close(client);
    return 0;
}
