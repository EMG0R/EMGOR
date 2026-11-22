// src/demiurge-csound-host.cpp
//
// DEMIURGE embedded Csound host — Csound computed INSIDE the JACK callback.
//
// WHY THIS EXISTS
// ---------------
// `csound -+rtaudio=jack` does not compute in the JACK process callback. It
// runs the orchestra on its own engine thread and hands blocks to the JACK
// callback across a ring buffer, `-B`. That ring is pure latency sitting
// directly in the signal path, and it is the reason `demiurge-run-csound` has
// to `chrt` the engine thread separately from PipeWire's own callback thread:
// two RT threads, a queue between them, and the queue's depth added to every
// note the player hears.
//
// Cable-measured on this rig (docs/latency-measurements.md), quantum 64:
//
//     -B 1024   29.02 ms round trip   (+23.6 ms over the 5.45 ms rig floor)
//     -B  512   16.44 ms              (+11.0)
//     -B  256   10.95 ms              (+ 5.5)
//     -B  128    8.28 ms              (+ 2.83)
//
// The relationship is ~`-B` frames of latency: the ring is in the path, it is
// not slack. `-B 128` is the floor for the standalone binary because Csound
// derives `-b` from `-B` and `-b` must be an integer multiple of ksmps.
//
// This host removes the ring rather than shrinking it. It is an ordinary JACK
// client that owns audio I/O itself (`csoundSetHostImplementedAudioIO`) and
// calls `csoundPerformKsmps()` from `process()`, copying JACK's port buffers
// straight through Csound's spin/spout. There is no second thread and no
// queue: at quantum 64 with ksmps 64 that is exactly one control period per
// callback, and Csound's added latency is zero frames by construction.
//
// WHAT IT DELIBERATELY DOES NOT CHANGE
// ------------------------------------
// The instrument runs unmodified. Everything the standalone wrapper gave the
// orchestra is still given to it here, by passing the same options through
// `csoundSetOption`:
//
//   * MIDI stays Csound's own ALSA-seq (`-+rtmidi=alsaseq -M 14:0 -Q 14:0`).
//     rtmidi is independent of rtaudio, so host-implemented audio I/O does not
//     touch it — the shared "Midi Through" pool, the Teensy controls, the
//     touch daemon's ch16 CCs, the clock and the UI's CC102 sync request all
//     keep working with no bridge and no translation layer.
//   * Plugin opcodes load normally (NAM lives in Csound's plugin dir and knows
//     nothing about who is driving the engine).
//   * OSC opcodes run in the performance pass as before, on their own liblo
//     threads.
//   * `--realtime`, `-r`, `-d`, `-+ignore_csopts=1` and every `--omacro:` are
//     forwarded verbatim.
//
// The JACK client registers as `csound6` with ports input1/2 and output1/2 —
// the same identity the standalone binary presents — so the launcher's
// `resolve_client`, `aggregate::direct_input_device` / `direct_output_device`
// and every UI that reads the patch graph keep working with no change at all.
// Override with DEMIURGE_JACK_CLIENT if you ever need two side by side.
//
// KSMPS MUST DIVIDE THE QUANTUM
// -----------------------------
// One callback of `nframes` runs `nframes / ksmps` control periods. If ksmps
// does not divide nframes there is no honest way to proceed — a partial period
// would need buffering, which is the thing we removed. The host REFUSES at
// startup (and on any live buffer-size change) with the arithmetic printed,
// rather than glitching quietly. ksmps < quantum is fine and costs nothing;
// ksmps > quantum is refused.
//
// BUILD
//   g++ -std=c++17 -O2 -mcpu=native -o demiurge-csound-host
//       demiurge-csound-host.cpp -lcsound64 -ljack -lpthread
//
// USAGE
//   demiurge-csound-host [csound options...] <file.csd> [more options...]
//
// Driven by `demiurge-run-csound` when live.conf says `csound_host = embedded`.

#include <csound/csound.h>
#include <jack/jack.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// NOTHING ON THE AUDIO THREAD MAY CALL write(). NOT EVEN INDIRECTLY.
//
// This is the single most expensive lesson this project has learned, and it has
// now been learned three separate times, three different ways:
//
//   1. `printks` in the orchestra -> journald -> 829 xruns in 120 s.
//   2. Per-CC MIDI debug prints in the orchestra -> same cause, same result.
//   3. THIS: libcsound's own default message handler.
//
// (1) and (2) were fixed in the orchestra. (3) is a property of the HOST, and
// it is what this block exists to kill permanently.
//
// The standalone backend tolerates all three because Csound computes on its own
// engine thread behind the `-B` ring: a print stalls that thread, the ring
// covers it, and nobody hears anything. THIS host has no ring by design — the
// whole point — so a stalled compute IS a dropped buffer IS an audible click.
//
// What made it hard to see: `process()` below is scrupulous about never
// printing, and says so in its comments. But `csoundPerformKsmps()` is called
// FROM that callback, and any message the orchestra or the engine emits during
// that call goes to Csound's default handler, which does `vfprintf(stderr,...)`
// — and the launcher points every stage's stderr at a log file ON THE SD CARD.
// So the host's author covered their own printfs and left libcsound's.
//
// Observed in the field: NEPTR's own master clock broadcasts BPM on ch16 CC119
// continuously, and Csound answers every single one with "ctrl 119 has no
// exclus list". 2.5 stderr writes per second, from inside the RT callback,
// against a 1.33 ms budget at quantum 64 — measured 2.9 xruns/s. The clicking
// was load-INDEPENDENT (it persisted with the amp models bypassed), which is
// the tell: it tracked the clock, not the DSP.
//
// The fix routes messages by thread. Off the audio thread — compile, startup,
// cleanup — they print normally and immediately, because that is where the
// useful diagnostics are and there is no deadline to miss. ON the audio thread
// they are formatted into a fixed lock-free ring and drained by the supervise
// loop at 20 Hz. No allocation, no lock, no syscall in the callback.
//
// Deliberately a ring rather than a drop: an orchestra print is usually someone
// debugging, and silently eating their output would just trade one mystery for
// another. When the ring does overflow that is itself the diagnosis — a chatty
// orchestra — so the overflow is counted and reported as a NUMBER rather than
// left to show up as unexplained clicks.
// ---------------------------------------------------------------------------

// True only on the JACK process thread. Set by process() itself, so it needs no
// coordination and cannot be wrong about which thread it is on.
thread_local bool t_audioThread = false;

struct MsgRing {
    static constexpr size_t SLOTS  = 512;
    static constexpr size_t SLOTSZ = 240;

    char buf[SLOTS][SLOTSZ];
    // Single producer (the audio thread), single consumer (the supervise loop).
    std::atomic<uint64_t> head{0};
    std::atomic<uint64_t> tail{0};
    std::atomic<uint64_t> queued{0};   // messages emitted from the audio thread
    std::atomic<uint64_t> dropped{0};  // ...that did not fit
};

MsgRing g_msgs;

uint64_t now_ns()
{
    // CLOCK_MONOTONIC is a vDSO read on aarch64 — no syscall, safe in the
    // callback. This is the one thing the audio thread is allowed to ask the
    // kernel, and it does not actually ask it.
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

// ---------------------------------------------------------------------------
// State shared between the RT callback and the main thread.
//
// The callback never allocates, never locks, never prints. Anything it needs to
// tell the main thread it says through one of these atomics; the main thread
// polls them at 20 Hz and does the printing and the exiting.
// ---------------------------------------------------------------------------
struct Host {
    CSOUND*        cs        = nullptr;
    jack_client_t* client    = nullptr;
    jack_port_t*   in[2]     = {nullptr, nullptr};
    jack_port_t*   out[2]    = {nullptr, nullptr};

    MYFLT*         spin      = nullptr;   // ksmps * nchnls_i, interleaved
    MYFLT*         spout     = nullptr;   // ksmps * nchnls,   interleaved
    uint32_t       ksmps     = 0;
    uint32_t       nchnls    = 0;
    uint32_t       nchnls_i  = 0;

    std::atomic<bool>     run{false};     // callback computes only when true
    std::atomic<bool>     finished{false};// orchestra hit end of score
    std::atomic<uint32_t> badQuantum{0};  // nframes that failed the ksmps check
    std::atomic<uint64_t> xruns{0};
    bool                  bypass  = false;  // DEMIURGE_HOST_BYPASS=1 diagnostic

    // Drift instrumentation. The callback only ever ADDS to these; the stats
    // thread only reads them. See the DRIFT note by the stats block.
    std::atomic<uint64_t> cbCount{0};    // process() invocations
    std::atomic<uint64_t> hostFrames{0}; // frames this host has moved
    std::atomic<uint64_t> perfCalls{0};  // csoundPerformKsmps() invocations
    std::atomic<bool>     jackGone{false}; // server dropped us — see on_jack_shutdown

    // DEADLINE INSTRUMENT.
    //
    // "Am I at max CPU?" is not answerable from a mean, and it was not
    // answerable at all before this: the xrun counter below only ever printed
    // under DEMIURGE_HOST_STATS=1, so a rig that WAS xrunning several times a
    // second logged absolutely nothing and looked healthy. Dropouts are
    // tail-driven — the run that clicks is the run whose 99th percentile
    // crossed the budget while its mean sat at 30% — so record the tail.
    //
    // budgetNs is one JACK period (nframes/sr). Anything at or over it is a
    // callback that finished late, which is a dropped buffer, which is a click.
    std::atomic<uint64_t> budgetNs{0};
    std::atomic<uint64_t> dspMaxNs{0};
    std::atomic<uint64_t> dspSumNs{0};
    std::atomic<uint64_t> dspOver{0};
    // Occupancy histogram in 1/16ths of the budget; bucket 16+ is over budget.
    // Enough to read a p95 off directly, cheap enough to be a single increment.
    std::atomic<uint64_t> dspHist[20];

    // LIVE DSP LOAD METER (TASK 3b, UI dsp bar). Unlike dspSumNs/dspMaxNs
    // above (cumulative since boot / since last report — right for the 5 s
    // diagnostic log, wrong for "what is it doing RIGHT NOW"), this is just
    // the MOST RECENT callback's duration: a single relaxed store from the
    // audio thread, read by the supervise loop every tick and turned into
    // dt/budget for /dsp_load. No CAS, no accumulation — cheapest possible
    // thing to publish off the RT thread.
    std::atomic<uint64_t> lastDtNs{0};
};

Host g;

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

// ---------------------------------------------------------------------------
// Csound's message handler. Replaces the default, which writes to stderr from
// whatever thread emitted the message — including, in this host, the RT one.
// ---------------------------------------------------------------------------
void on_csound_message(CSOUND*, int /*attr*/, const char* fmt, va_list args)
{
    if (!t_audioThread) {
        // Main thread: compile diagnostics, plugin banners, cleanup. Print
        // straight through — this is the output people actually read, and
        // there is no deadline here to miss.
        std::vfprintf(stderr, fmt, args);
        return;
    }

    // AUDIO THREAD. Format into the ring and return; the supervise loop prints.
    g_msgs.queued.fetch_add(1, std::memory_order_relaxed);

    const uint64_t h = g_msgs.head.load(std::memory_order_relaxed);
    const uint64_t t = g_msgs.tail.load(std::memory_order_acquire);
    if (h - t >= MsgRing::SLOTS) {
        // Full. Dropping is the correct failure mode: the alternative is
        // blocking the audio thread on the reader, which is the bug.
        g_msgs.dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    std::vsnprintf(g_msgs.buf[h % MsgRing::SLOTS], MsgRing::SLOTSZ, fmt, args);
    g_msgs.head.store(h + 1, std::memory_order_release);
}

// Reads the deadline histogram and prints occupancy as a distribution, because
// a mean cannot tell you whether you are about to click and a p99 can.
void report_deadline(const char* why)
{
    const uint64_t budget = g.budgetNs.load(std::memory_order_relaxed);
    uint64_t total = 0;
    uint64_t hist[20];
    for (size_t i = 0; i < 20; ++i) {
        hist[i] = g.dspHist[i].load(std::memory_order_relaxed);
        total += hist[i];
    }
    if (!total || !budget) return;

    auto pct = [&](double frac) {
        const uint64_t want = static_cast<uint64_t>(static_cast<double>(total) * frac);
        uint64_t run = 0;
        for (size_t i = 0; i < 20; ++i) {
            run += hist[i];
            if (run >= want)
                return (static_cast<double>(i) + 1.0) / 16.0 * 100.0;
        }
        return 125.0;
    };

    const uint64_t over = g.dspOver.load(std::memory_order_relaxed);
    std::fprintf(stderr,
        "demiurge-csound-host: [%s] budget=%.2fms  mean=%.0f%%  p50<=%.0f%%  p95<=%.0f%%  "
        "p99<=%.0f%%  max=%.0f%%  over-budget=%llu/%llu (%.3f%%)  rt-msgs=%llu drop=%llu\n",
        why,
        static_cast<double>(budget) / 1e6,
        static_cast<double>(g.dspSumNs.load()) / static_cast<double>(total) /
            static_cast<double>(budget) * 100.0,
        pct(0.50), pct(0.95), pct(0.99),
        static_cast<double>(g.dspMaxNs.load()) / static_cast<double>(budget) * 100.0,
        static_cast<unsigned long long>(over),
        static_cast<unsigned long long>(total),
        100.0 * static_cast<double>(over) / static_cast<double>(total),
        static_cast<unsigned long long>(g_msgs.queued.load()),
        static_cast<unsigned long long>(g_msgs.dropped.load()));
}

// Called only from the supervise loop. The producer never overwrites a slot it
// has not seen us release, so reading up to `head` is safe without a lock.
void drain_messages()
{
    const uint64_t h = g_msgs.head.load(std::memory_order_acquire);
    uint64_t t = g_msgs.tail.load(std::memory_order_relaxed);
    for (; t < h; ++t)
        std::fputs(g_msgs.buf[t % MsgRing::SLOTS], stderr);
    if (t != g_msgs.tail.load(std::memory_order_relaxed)) {
        g_msgs.tail.store(t, std::memory_order_release);
        std::fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// Minimal OSC-over-UDP sender — TASK 3b (UI DSP load bar). Called ONLY from
// the supervise loop (main thread), never from process(): this is a
// blocking-capable syscall (socket + sendto), exactly the class of thing the
// long comment above MsgRing exists to keep off the audio thread. There is
// no liblo dependency here on purpose — Csound's own OSCsend opcodes already
// talk to 127.0.0.1:8000 from the orchestra side (globals.orc OSC3x_f/i);
// this is the same wire format, built by hand, for the one or two messages
// per tick this host needs to add alongside them.
//
// OSC packet shape: address string (null-padded to a 4-byte boundary) +
// type-tag string ",X" (same padding) + the argument, big-endian. No bundle
// wrapper needed for a single message.
// ---------------------------------------------------------------------------
void osc_pad_append(std::vector<char>& buf, const char* s)
{
    const size_t len = std::strlen(s);
    const size_t total = ((len + 1 + 3) / 4) * 4;   // +1 for the null terminator
    const size_t start = buf.size();
    buf.resize(start + total, '\0');
    std::memcpy(&buf[start], s, len);
}

void osc_send_float(int sock, const sockaddr_in& dst, const char* path, float v)
{
    if (sock < 0) return;
    std::vector<char> buf;
    osc_pad_append(buf, path);
    osc_pad_append(buf, ",f");
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = htonl(bits);
    const size_t start = buf.size();
    buf.resize(start + 4);
    std::memcpy(&buf[start], &bits, 4);
    (void)sendto(sock, buf.data(), buf.size(), 0,
                 reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
}

void osc_send_int(int sock, const sockaddr_in& dst, const char* path, int32_t v)
{
    if (sock < 0) return;
    std::vector<char> buf;
    osc_pad_append(buf, path);
    osc_pad_append(buf, ",i");
    const uint32_t bits = htonl(static_cast<uint32_t>(v));
    const size_t start = buf.size();
    buf.resize(start + 4);
    std::memcpy(&buf[start], &bits, 4);
    (void)sendto(sock, buf.data(), buf.size(), 0,
                 reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
}

// ---------------------------------------------------------------------------
// The whole point of the program.
//
// nframes / ksmps control periods, back to back, in the callback. No ring, no
// engine thread, no handoff: the samples JACK just captured are the samples
// Csound computes on, and the result goes out on the same callback.
// ---------------------------------------------------------------------------
int process(jack_nframes_t nframes, void*)
{
    // Identifies this thread to on_csound_message. A plain thread_local store;
    // it is the cheapest correct way to know "am I the audio thread" without
    // comparing pthread ids or coordinating with anyone.
    t_audioThread = true;

    const uint64_t tStart = now_ns();

    float* outL = static_cast<float*>(jack_port_get_buffer(g.out[0], nframes));
    float* outR = static_cast<float*>(jack_port_get_buffer(g.out[1], nframes));

    // Any reason not to compute is silence, not stale audio or a partial block.
    if (!g.run.load(std::memory_order_acquire) || g.finished.load(std::memory_order_acquire)) {
        std::memset(outL, 0, nframes * sizeof(float));
        std::memset(outR, 0, nframes * sizeof(float));
        return 0;
    }

    g.cbCount.fetch_add(1, std::memory_order_relaxed);
    g.hostFrames.fetch_add(nframes, std::memory_order_relaxed);

    const uint32_t ksmps = g.ksmps;
    if (nframes % ksmps != 0) {
        // Report once and go quiet; the main thread turns this into a clear
        // message and an exit. Printing here would be a syscall on the RT thread.
        g.badQuantum.store(nframes, std::memory_order_release);
        std::memset(outL, 0, nframes * sizeof(float));
        std::memset(outR, 0, nframes * sizeof(float));
        return 0;
    }

    const float* inL = static_cast<const float*>(jack_port_get_buffer(g.in[0], nframes));
    const float* inR = static_cast<const float*>(jack_port_get_buffer(g.in[1], nframes));

    const uint32_t nci = g.nchnls_i;
    const uint32_t nco = g.nchnls;
    // Re-fetch every callback rather than caching from startup: cheap (a struct
    // field read inside libcsound), and it removes any assumption that Csound
    // never moves these buffers after compile.
    MYFLT* const spin  = csoundGetSpin(g.cs);
    MYFLT* const spout = csoundGetSpout(g.cs);

    // DEMIURGE_HOST_BYPASS=1 — diagnostic only. Same JACK client, same ports,
    // same callback, but the samples go straight from input to output without
    // Csound in the middle. It exists to answer one question when a latency
    // measurement through this host looks wrong: is the anomaly in the JACK
    // plumbing, or in the engine? Compare against testing/latency/nullclient.
    if (g.bypass) {
        std::memcpy(outL, inL, nframes * sizeof(float));
        std::memcpy(outR, inR, nframes * sizeof(float));
        return 0;
    }

    for (jack_nframes_t base = 0; base < nframes; base += ksmps) {
        // 0dbfs is 1 in this orchestra, and JACK is +/-1.0, so this is a
        // straight interleave — no scaling, no format conversion beyond
        // float -> MYFLT (double on this build of Csound).
        for (uint32_t i = 0; i < ksmps; ++i) {
            spin[i * nci]     = static_cast<MYFLT>(inL[base + i]);
            spin[i * nci + 1] = static_cast<MYFLT>(inR[base + i]);
        }

        g.perfCalls.fetch_add(1, std::memory_order_relaxed);
        if (csoundPerformKsmps(g.cs) != 0) {
            g.finished.store(true, std::memory_order_release);
            std::memset(outL + base, 0, (nframes - base) * sizeof(float));
            std::memset(outR + base, 0, (nframes - base) * sizeof(float));
            return 0;
        }

        for (uint32_t i = 0; i < ksmps; ++i) {
            outL[base + i] = static_cast<float>(spout[i * nco]);
            outR[base + i] = static_cast<float>(spout[i * nco + 1]);
        }
    }

    // Deadline accounting. Only the full compute path is measured — the
    // silence, bad-quantum and bypass returns above are not what anyone is
    // asking about when they ask whether the engine is keeping up.
    const uint64_t dt = now_ns() - tStart;
    g.lastDtNs.store(dt, std::memory_order_relaxed);
    g.dspSumNs.fetch_add(dt, std::memory_order_relaxed);
    uint64_t prevMax = g.dspMaxNs.load(std::memory_order_relaxed);
    while (dt > prevMax &&
           !g.dspMaxNs.compare_exchange_weak(prevMax, dt, std::memory_order_relaxed))
        ; // CAS loop, but uncontended: one audio thread is the only writer.
    if (const uint64_t budget = g.budgetNs.load(std::memory_order_relaxed)) {
        if (dt >= budget) g.dspOver.fetch_add(1, std::memory_order_relaxed);
        size_t bucket = static_cast<size_t>((dt * 16) / budget);
        if (bucket > 19) bucket = 19;
        g.dspHist[bucket].fetch_add(1, std::memory_order_relaxed);
    }
    return 0;
}

int on_buffer_size(jack_nframes_t nframes, void*)
{
    // Live quantum change. Re-vet rather than trust: PipeWire can renegotiate
    // the graph quantum under us (another client asking for a bigger buffer,
    // a device change), and a quantum that ksmps does not divide has to stop
    // the host, not be papered over.
    if (g.ksmps && nframes % g.ksmps != 0)
        g.badQuantum.store(nframes, std::memory_order_release);
    // The deadline moved with the quantum; the histogram is relative to it.
    if (const jack_nframes_t sr = jack_get_sample_rate(g.client))
        g.budgetNs.store((static_cast<uint64_t>(nframes) * 1000000000ull) / sr,
                         std::memory_order_relaxed);
    return 0;
}

int on_xrun(void*)
{
    g.xruns.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

// PipeWire drops clients — a client that misses enough deadlines, a graph
// renegotiation, a server restart. Without this callback the host has NO WAY of
// knowing: it keeps spinning its supervise loop forever with no ports, no
// audio and no error, and because the process never exits systemd never
// restarts it. That is not theoretical. Deployed as the live backend, this host
// came up correctly, played, then deregistered every port mid-session and sat
// there alive at 0% CPU with the rig silent, until it was reverted by hand.
//
// EXITING IS NOT YET RECOVERY, AND THIS COMMENT USED TO CLAIM IT WAS.
// demiurge-launcher's Event::ChildExited handler calls mark_dead() and
// re-applies the graph; nothing anywhere respawns the stage. demiurge.service's
// Restart=on-failure restarts the LAUNCHER, and the launcher does not exit when
// one stage of several dies (the clock keeps its child-watcher's pid set
// non-empty). So a host that exits leaves the rig just as silent as a host that
// hangs — only now it is visibly dead instead of invisibly idle, which is
// strictly better for diagnosis and strictly no better for the player.
//
// That gap is not specific to this host: it is why the STANDALONE backend also
// sat with no audio path twice on 2026-08-10. Real recovery needs either stage
// respawn in the launcher or re-registration here; until one of those lands,
// treat this as fail-loudly, not fail-safe.
//
// Called from a JACK/PipeWire thread, so it does the minimum: set a flag.
void on_jack_shutdown(void*)
{
    g.run.store(false, std::memory_order_release);
    g.jackGone.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------

void usage()
{
    std::fprintf(stderr,
        "usage: demiurge-csound-host [csound options...] <file.csd> [options...]\n"
        "\n"
        "  Runs the .csd inside the JACK process callback (no -B ring).\n"
        "  Options are passed to Csound verbatim via csoundSetOption; do NOT\n"
        "  pass -B/-b or an rtaudio module — this host owns audio I/O.\n"
        "\n"
        "  env DEMIURGE_JACK_CLIENT   JACK client name (default: csound6)\n"
        "  env DEMIURGE_HOST_STATS=1  print DSP load / xrun stats every 10 s\n");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { usage(); return 1; }

    std::vector<std::string> options;
    std::string csd;

    // Argument shape follows the standalone command line the wrapper already
    // builds, which mixes joined and separated forms: `-d -+ignore_csopts=1
    // -r 48000 -M 14:0 --realtime --omacro:KSMPS=64 file.csd`.
    //
    // csoundSetOption takes ONE string per option, so a separated short flag
    // has to be rejoined ("-r" + "48000" -> "-r48000", which Csound's argdecode
    // accepts identically). Without this, "48000" arrives as a bare token and
    // Csound prints its usage banner and exits — with no indication of which
    // argument was at fault.
    // Deliberately only the value-taking short flags, and only the ones this
    // project actually passes. A flag wrongly listed here would swallow the
    // filename, so the list stays conservative rather than exhaustive.
    auto takes_value = [](const std::string& s) {
        static const char* v = "riokbBjmMQtTFuLR";
        return s.size() == 2 && s[0] == '-' && std::strchr(v, s[1]) != nullptr;
    };
    auto is_csd = [](const std::string& s) {
        return s.size() > 4 && s.compare(s.size() - 4, 4, ".csd") == 0;
    };

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (takes_value(a) && i + 1 < argc && argv[i + 1][0] != '-' && !is_csd(argv[i + 1]))
            a += argv[++i];
        if (!a.empty() && a[0] == '-') {
            // -B and -b are meaningless here and actively misleading if someone
            // copies an old command line across. Say so rather than ignore them.
            // DEMIURGE_HOST_ALLOW_B=1 is a DIAGNOSTIC escape hatch. Csound
            // reports inbuf/outbuf of 512 samples under host-implemented audio
            // I/O even though ksmps*nchnls is 128 — its default -b 256 software
            // buffer is still allocated. spin/spout + csoundPerformKsmps should
            // bypass it entirely, but "should" is not evidence, and the failure
            // mode worth ruling out here is buffer-shaped. This lets a test
            // force -b/-B down to ksmps and see whether inbuf/outbuf move.
            static const bool allowB = [] {
                const char* v = std::getenv("DEMIURGE_HOST_ALLOW_B");
                return v && *v && std::strcmp(v, "0") != 0;
            }();
            if (!allowB && (a.rfind("-B", 0) == 0 || a.rfind("-b", 0) == 0)) {
                std::fprintf(stderr,
                    "demiurge-csound-host: refusing '%s'.\n"
                    "  -B is the engine-thread ring this host exists to remove, and -b is\n"
                    "  derived from it. The JACK quantum is the only period here.\n", a.c_str());
                return 2;
            }
            if (a.rfind("-+rtaudio", 0) == 0 || a == "-odac" || a.rfind("-odac:", 0) == 0
                || a == "-iadc" || a.rfind("-iadc:", 0) == 0) {
                // Harmless but wrong-headed: the host is the audio device.
                continue;
            }
            options.push_back(a);
        } else if (csd.empty()) {
            csd = a;
        } else {
            options.push_back(a);
        }
    }

    if (csd.empty()) { usage(); return 1; }

    // Csound resolves `#include` against the current working directory, so a
    // multi-file orchestra only builds from its own directory. Same rule the
    // standalone wrapper follows.
    std::string dir = csd;
    const size_t slash = dir.find_last_of('/');
    std::string leaf = csd;
    if (slash != std::string::npos) {
        dir = dir.substr(0, slash);
        leaf = csd.substr(slash + 1);
        if (chdir(dir.c_str()) != 0)
            std::fprintf(stderr, "demiurge-csound-host: chdir('%s') failed, "
                                 "#include may not resolve\n", dir.c_str());
    }

    // ---- Csound ------------------------------------------------------------
    g.cs = csoundCreate(nullptr);
    if (!g.cs) { std::fprintf(stderr, "demiurge-csound-host: csoundCreate failed\n"); return 3; }

    // BEFORE csoundCompile, so compile diagnostics route through it too (they
    // arrive on this thread and print immediately, exactly as before). See the
    // long note by MsgRing: this is what stops an orchestra print from turning
    // into an SD-card write inside the RT callback, and it is not optional.
    csoundSetMessageCallback(g.cs, on_csound_message);

    // MUST precede compilation. This is what makes spin/spout the interface and
    // stops Csound from opening a device of its own. The second argument (MIDI)
    // stays 0: Csound keeps its own ALSA-seq MIDI, which is what the whole
    // control surface depends on.
    csoundSetHostImplementedAudioIO(g.cs, 1, 0);

    // Compile through csoundCompile(argc, argv) — the SAME entry point the
    // standalone binary uses — rather than csoundSetOption + csoundCompileCsd.
    //
    // This is not a style preference, it is a bug fix. `-+ignore_csopts=1` is
    // only honoured on the argv path. Set it with csoundSetOption and the CSD's
    // own <CsOptions> block is still applied, and applied LAST, so it wins.
    // NEPTR's CsOptions carry `-+rtmidi=alsa` and `-Ma` (raw ALSA MIDI, for a
    // different machine), which silently replaced the `-+rtmidi=alsaseq` we
    // asked for. MIDI input then came from the wrong backend, and the first
    // thing the orchestra sent OUT — chase_bliss_clock.orc steering the
    // demiurge clock on ch16 CC118 — went to rawmidi with a device that was
    // never opened:
    //
    //     rawmidi.c:1098: snd_rawmidi_write: Assertion `rawmidi' failed
    //
    // i.e. the whole engine aborted a few seconds in. Going through argv makes
    // option semantics identical to the standalone backend by construction,
    // which is the property we actually want from a drop-in replacement.
    std::vector<const char*> cargv;
    cargv.push_back("csound");
    for (const auto& o : options) cargv.push_back(o.c_str());
    cargv.push_back(leaf.c_str());

    if (csoundCompile(g.cs, static_cast<int>(cargv.size()), cargv.data()) != 0) {
        std::fprintf(stderr, "demiurge-csound-host: compile failed: %s\n", csd.c_str());
        csoundDestroy(g.cs);
        return 5;
    }

    g.ksmps    = csoundGetKsmps(g.cs);
    g.nchnls   = csoundGetNchnls(g.cs);
    g.nchnls_i = csoundGetNchnlsInput(g.cs);
    g.spin     = csoundGetSpin(g.cs);
    g.spout    = csoundGetSpout(g.cs);
    const double csSr = csoundGetSr(g.cs);

    if (g.nchnls < 2 || g.nchnls_i < 2) {
        std::fprintf(stderr,
            "demiurge-csound-host: orchestra is nchnls=%u nchnls_i=%u; this host is stereo-in/stereo-out.\n",
            g.nchnls, g.nchnls_i);
        csoundDestroy(g.cs);
        return 6;
    }

    // ---- JACK --------------------------------------------------------------
    { const char* b = std::getenv("DEMIURGE_HOST_BYPASS"); g.bypass = b && *b && std::strcmp(b, "0") != 0; }
    if (g.bypass)
        std::fprintf(stderr, "demiurge-csound-host: DEMIURGE_HOST_BYPASS=1 — Csound is NOT in the audio path\n");

    const char* clientName = std::getenv("DEMIURGE_JACK_CLIENT");
    if (!clientName || !*clientName) clientName = "csound6";

    // JackUseExactName is a SAFETY feature here, not a cosmetic one.
    //
    // Without it, a second instance registers as `csound6-1` and runs happily
    // in parallel with the first. That is bad for two independent reasons.
    // Cosmetically, the launcher's resolve_client looks for `csound6` and the
    // patcher UIs would show a disconnected graph. Far worse: unlike the
    // standalone backend — which computes on its own thread behind a ring —
    // every instance of THIS program runs a complete Csound orchestra inside a
    // SCHED_FIFO JACK callback. Two or three orphans (a killed terminal, a
    // hung script, a supervisor that restarted before the old process died)
    // are a genuine realtime-livelock risk on a 4-core Pi, and on this rig
    // systemd's RuntimeWatchdogUSec turns that into a hard board reset. It has
    // already happened once, during this program's own development.
    //
    // So: exactly one `csound6`. A second instance refuses to start and says
    // why, rather than quietly becoming a second RT load on the box.
    jack_status_t st;
    g.client = jack_client_open(clientName, static_cast<jack_options_t>(JackNoStartServer | JackUseExactName), &st);
    if (!g.client) {
        if (st & JackNameNotUnique) {
            std::fprintf(stderr,
                "demiurge-csound-host: a JACK client named '%s' already exists — refusing to start.\n"
                "  This host runs the whole orchestra inside the RT callback, so a second instance\n"
                "  is a second realtime load, not a harmless duplicate. Stop the old one first\n"
                "  (or set DEMIURGE_JACK_CLIENT to run a deliberate second engine).\n", clientName);
        } else {
            std::fprintf(stderr, "demiurge-csound-host: jack_client_open('%s') failed (status 0x%x)\n",
                         clientName, static_cast<unsigned>(st));
        }
        csoundDestroy(g.cs);
        return 7;
    }
    const char* realName = jack_get_client_name(g.client);

    const jack_nframes_t jackSr = jack_get_sample_rate(g.client);
    const jack_nframes_t quantum = jack_get_buffer_size(g.client);

    // The refusal the brief asks for: explicit, arithmetic shown, no glitching.
    if (quantum % g.ksmps != 0) {
        std::fprintf(stderr,
            "demiurge-csound-host: REFUSING TO RUN — ksmps does not divide the JACK quantum.\n"
            "  quantum = %u frames, ksmps = %u  (%u %% %u = %u)\n"
            "  A callback must be a whole number of control periods; anything else needs a\n"
            "  buffer, which is exactly what this host removes.\n"
            "  Fix either side:  live.conf `quantum = %u`   or   csound_extra = --omacro:KSMPS=%u\n",
            quantum, g.ksmps, quantum, g.ksmps, quantum % g.ksmps,
            g.ksmps, quantum);
        jack_client_close(g.client);
        csoundDestroy(g.cs);
        return 8;
    }
    // Csound's own software buffering, if any, is visible here. With
    // csoundSetHostImplementedAudioIO these SHOULD be ksmps*nchnls — i.e. the
    // spin/spout vectors and nothing more. Anything larger means Csound is
    // still staging audio through a -b buffer behind our back, which would be
    // exactly the ring this host exists to remove.
    std::fprintf(stderr,
        "demiurge-csound-host: csound sr=%g ksmps=%u nchnls=%u/%u  inbuf=%ld outbuf=%ld (expect %u)\n",
        csSr, g.ksmps, g.nchnls_i, g.nchnls,
        csoundGetInputBufferSize(g.cs), csoundGetOutputBufferSize(g.cs),
        g.ksmps * g.nchnls);

    if (static_cast<double>(jackSr) != csSr) {
        std::fprintf(stderr,
            "demiurge-csound-host: sample-rate mismatch — JACK %u Hz, orchestra sr = %g.\n"
            "  Pass -r %u (the launcher's DEMIURGE_RATE) or fix `sr` in the orchestra.\n",
            jackSr, csSr, jackSr);
        jack_client_close(g.client);
        csoundDestroy(g.cs);
        return 9;
    }

    // Port names match the standalone binary's exactly (input1/2, output1/2) so
    // the launcher's direct-link paths and the patcher UIs need no change.
    g.in[0]  = jack_port_register(g.client, "input1",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    g.in[1]  = jack_port_register(g.client, "input2",  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
    g.out[0] = jack_port_register(g.client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    g.out[1] = jack_port_register(g.client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!g.in[0] || !g.in[1] || !g.out[0] || !g.out[1]) {
        std::fprintf(stderr, "demiurge-csound-host: port registration failed\n");
        jack_client_close(g.client);
        csoundDestroy(g.cs);
        return 10;
    }

    // MUST be registered before activate: a client can be dropped at any time,
    // including during startup.
    jack_on_shutdown(g.client, on_jack_shutdown, nullptr);
    jack_set_process_callback(g.client, process, nullptr);
    jack_set_buffer_size_callback(g.client, on_buffer_size, nullptr);
    jack_set_xrun_callback(g.client, on_xrun, nullptr);

    // One period, in nanoseconds — the deadline every callback is racing.
    g.budgetNs.store((static_cast<uint64_t>(quantum) * 1000000000ull) / jackSr,
                     std::memory_order_relaxed);

    std::fprintf(stderr,
        "demiurge-csound-host: %s  sr=%u quantum=%u ksmps=%u  -> %u csoundPerformKsmps/callback\n",
        realName, jackSr, quantum, g.ksmps, quantum / g.ksmps);
    std::fflush(stderr);

    if (jack_activate(g.client) != 0) {
        std::fprintf(stderr, "demiurge-csound-host: jack_activate failed\n");
        jack_client_close(g.client);
        csoundDestroy(g.cs);
        return 11;
    }
    g.run.store(true, std::memory_order_release);

    // ---- OSC (TASK 3b: /dsp_load, /dsp_xrun to the UI) ----------------------
    // Same 127.0.0.1:8000 target the orchestra's own OSC3x_f/i macros send
    // to (globals.orc). A UDP socket to a loopback port either succeeds or
    // doesn't matter — there is no handshake, so a UI that isn't running yet
    // just means nobody is listening on that port. Non-fatal either way.
    int oscSock = -1;
    sockaddr_in oscAddr{};
    {
        oscSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (oscSock < 0) {
            std::fprintf(stderr,
                "demiurge-csound-host: socket() failed for the /dsp_load OSC "
                "sender — DSP load bar on the UI will not update.\n");
        } else {
            oscAddr.sin_family = AF_INET;
            oscAddr.sin_port = htons(8000);
            oscAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
    }

    // ---- supervise ---------------------------------------------------------
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    const bool stats = [] { const char* s = std::getenv("DEMIURGE_HOST_STATS"); return s && *s && std::strcmp(s, "0") != 0; }();
    int rc = 0;
    uint64_t tick = 0;
    uint64_t reportedDrops = 0;
    int32_t lastSentXruns = -1;
    while (!g_stop.load()) {
        usleep(50 * 1000);
        ++tick;

        // Anything the orchestra printed from the callback gets written HERE,
        // on this thread, where a blocking write costs nobody any audio.
        drain_messages();

        // /dsp_load + /dsp_xrun — every supervise tick (this loop's own 20 Hz
        // cadence; close enough to the 25 Hz the engine's own meters run at,
        // and this is the only place that knows the REAL callback-wall-time
        // vs. quantum deadline, per the DEADLINE INSTRUMENT note above).
        // /dsp_load is sent unconditionally each tick (it's continuous
        // telemetry, same discipline as the orchestra's own /meter); /dsp_xrun
        // only on an actual rising edge, since a genuine xrun is a rare,
        // discrete event, not something to spam every 50 ms.
        if (oscSock >= 0) {
            const uint64_t budget = g.budgetNs.load(std::memory_order_relaxed);
            if (budget) {
                const uint64_t dt = g.lastDtNs.load(std::memory_order_relaxed);
                const float load = static_cast<float>(
                    static_cast<double>(dt) / static_cast<double>(budget));
                osc_send_float(oscSock, oscAddr, "/dsp_load", load);
            }
            const int32_t xr = static_cast<int32_t>(g.xruns.load(std::memory_order_relaxed));
            if (xr != lastSentXruns) {
                lastSentXruns = xr;
                osc_send_int(oscSock, oscAddr, "/dsp_xrun", xr);
            }
        }

        // A chatty orchestra should be a number, not a mystery. Unconditional —
        // not gated behind DEMIURGE_HOST_STATS — because the whole reason this
        // bug survived so long is that the evidence was behind an unset env var.
        if (const uint64_t d = g_msgs.dropped.load(std::memory_order_relaxed); d != reportedDrops) {
            reportedDrops = d;
            std::fprintf(stderr,
                "demiurge-csound-host: message ring overflowed — %llu message(s) dropped.\n"
                "  The orchestra is printing from the audio thread faster than 20 Hz can\n"
                "  drain it. Nothing was written from the RT thread (that is the point), but\n"
                "  find the print and remove it: formatting is still work on the deadline.\n",
                static_cast<unsigned long long>(d));
            std::fflush(stderr);
        }

        if (const uint32_t bad = g.badQuantum.load(std::memory_order_acquire)) {
            std::fprintf(stderr,
                "demiurge-csound-host: STOPPING — the graph quantum changed to %u frames, which\n"
                "  ksmps=%u does not divide (%u %% %u = %u). Set live.conf `quantum` to a multiple\n"
                "  of ksmps, or set ksmps with csound_extra = --omacro:KSMPS=<n>.\n",
                bad, g.ksmps, bad, g.ksmps, bad % g.ksmps);
            rc = 12;
            break;
        }
        if (g.jackGone.load(std::memory_order_acquire)) {
            std::fprintf(stderr,
                "demiurge-csound-host: JACK/PipeWire dropped this client — exiting.\n"
                "  NOTE: nothing respawns a dead stage today, so audio does NOT come back on\n"
                "  its own; restart demiurge. Exiting at least makes the failure visible\n"
                "  instead of leaving a healthy-looking process with no ports.\n");
            rc = 13;
            break;
        }
        if (g.finished.load(std::memory_order_acquire)) {
            std::fprintf(stderr, "demiurge-csound-host: performance ended\n");
            break;
        }
        // Belt and braces: jack_on_shutdown is not guaranteed to fire on every
        // path a client can vanish by. If our ports are gone, so are we.
        if (tick % 40 == 0 && !jack_port_is_mine(g.client, g.out[0])) {
            std::fprintf(stderr,
                "demiurge-csound-host: our JACK ports have disappeared — exiting for restart.\n");
            rc = 13;
            break;
        }
        if (stats && tick % 100 == 0) {   // every 5 s
            // DRIFT INSTRUMENT.
            //
            // This host writes exactly `nframes` out for every `nframes` in, so
            // no producer/consumer mismatch can accumulate in the copy loop
            // itself. If the measured round trip nonetheless slides, then
            // Csound's notion of elapsed time must be diverging from the
            // callback's — and that is a subtraction, not a guess:
            //
            //   hostFrames  = sum of nframes over every process() call
            //   csSamples   = csoundGetCurrentTimeSamples(), Csound's own count
            //   drift       = csSamples - hostFrames
            //
            // We call csoundPerformKsmps() exactly hostFrames/ksmps times, so a
            // correct engine keeps drift pinned at 0. A drift that grows
            // LINEARLY identifies the cause outright: Csound is advancing its
            // clock at a different rate than we are feeding it.
            const uint64_t hf   = g.hostFrames.load();
            const uint64_t pc   = g.perfCalls.load();
            const int64_t  cst  = csoundGetCurrentTimeSamples(g.cs);
            const int64_t  drift = cst - static_cast<int64_t>(hf);
            std::fprintf(stderr,
                "demiurge-csound-host: cb=%llu hostFrames=%llu perfCalls=%llu(x%u=%llu) "
                "csSamples=%lld drift=%+lld scoreTime=%.3f dsp=%.1f%% xruns=%llu\n",
                static_cast<unsigned long long>(g.cbCount.load()),
                static_cast<unsigned long long>(hf),
                static_cast<unsigned long long>(pc), g.ksmps,
                static_cast<unsigned long long>(pc * g.ksmps),
                static_cast<long long>(cst), static_cast<long long>(drift),
                csoundGetScoreTime(g.cs),
                jack_cpu_load(g.client),
                static_cast<unsigned long long>(g.xruns.load()));
            report_deadline("stats");
            std::fflush(stderr);
        }
    }

    // Stop computing BEFORE the client goes away, so the callback can never
    // touch a destroyed Csound instance.
    g.run.store(false, std::memory_order_release);
    jack_deactivate(g.client);
    jack_client_close(g.client);

    // csoundCleanup runs the orchestra's end-of-performance path. NEPTR's
    // state_persist.orc ftsavek's on a 2 s timer rather than at exit, so the
    // settings table is already on disk either way — but a clean cleanup is
    // what makes that true of any patch that DOES flush at the end.
    // Always, not just under DEMIURGE_HOST_STATS: a session that clicked should
    // say so on its way out without anyone having known to ask in advance.
    drain_messages();
    report_deadline("final");
    std::fprintf(stderr, "demiurge-csound-host: %llu xrun(s) this session\n",
                 static_cast<unsigned long long>(g.xruns.load()));
    csoundCleanup(g.cs);
    csoundDestroy(g.cs);
    if (oscSock >= 0) close(oscSock);
    return rc;
}
