// simpmor_udp.cpp -- the simulated PMOR of tests/test_hostclient.cpp (same sim:: ops: pooled layout, preset files, store states, assign)
// behind a UDP server, so Max (which can only open /dev/cu.* ports) can drive the real devices against it headlessly.
//   simpmor_udp <port> [--pooled] [--drop-after-bytes N] [--log file] [--quiet]
//   one datagram received on 127.0.0.1:<port> = bytes from the host (protoRx); device output goes back as datagrams (<= 480 bytes)
//   to 127.0.0.1:<port+1> (the udpreceive of pmor_port.js `open udp<port>`).
//   first datagram -> protoConnected(true); datagram "PORTCLOSE" -> protoConnected(false) (and clears a yank).
//   --drop-after-bytes N: cable yank. After N bytes have been sent to the host the sim goes silent AND ignores input until a datagram
//   containing HELLO (reconnect) or PORTCLOSE arrives. One shot.
//   control datagrams (prefix "SIM "): SIM STORE <0..3> | SIM POOLED <0|1> | SIM VOL <0..1.25> (device-side edit -> push) | SIM BUS <n> (rx= bus ticks += n) | SIM POLICY <0..3> | SIM EPOCH | SIM DROP | SIM QUIT
//   log lines: <ms> RX <host frame> | TX <first 100 chars of device frame> | EV <event>  (ASSIGN slot id, SAMPLE_READY slot, STORE_RETRY, ...)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "proto.h"
#include "proto_util.h"
#include "params.h"
#include "param.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"

static FILE* g_log = nullptr; static bool g_quiet = false;
static uint32_t wallMs() { struct timeval tv; gettimeofday(&tv, nullptr); static long long t0 = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000; return (uint32_t)((long long)tv.tv_sec * 1000 + tv.tv_usec / 1000 - t0); }
static void logf_(const char* fmt, ...) {
    if (!g_log) return; char b[600]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fprintf(g_log, "%u %s\n", wallMs(), b); fflush(g_log);
}
static std::vector<std::pair<int, int>> g_pendingReady;      // assign results to report on the next poll
namespace sim {
static uint32_t now = 1000, epoch = 1; static uint8_t slot = 3;
static std::string toHost;
static std::vector<int16_t> pool[64][16];
static size_t tx(const uint8_t* d, size_t n) { toHost.append((const char*)d, n); return n; }
static size_t txSpace() { return 1u << 20; }
static bool idle() { return true; }
static uint32_t nowMs() { return now; }
static uint8_t patchSlot() { return slot; }
static uint32_t patchEpoch() { return epoch; }
static void changed() {}
static bool follow() { return false; }
static int policy = 0, busTicks = 0;
static bool pooled = false; static int storeState = 2, storePolls = 0, storeRetries = 0, poolErrors = 0;
static std::vector<std::pair<int, int>> assigned;
static void status(char* b, size_t n) {
    if (storeState == 1 && ++storePolls >= 2) storeState = 2;          // a retried storage job settles after two polls
    snprintf(b, n, "cm=0/%d rx=%d/0/0/0 clk=follow running=0 bpm=%d cpu=12 audio_overruns=0 midi_drops=0 store=%d/%d/0/0/0/0/%d pool=%d", policy, busTicks, (int)(clk.bpm + 0.5f), storeState, storeState == 1 ? 5 : 0, storeState == 3 ? 2 : 0, pooled ? 3 : 0);
}
static int scan(uint8_t) { return 1; }
static uint16_t poolIdOf(uint16_t id) { return id >= 1024 ? (uint16_t)(id - 1024) : id; }
static uint32_t plen(uint8_t p, uint8_t s) { return p == slot ? smp.len[s] : (uint32_t)pool[p][s].size(); }
static uint32_t pver(uint8_t p, uint8_t s) { return 0x1000u + p * 16 + s; }
static int32_t pread(uint16_t id0, uint32_t off, uint8_t* buf, uint32_t mx) {
    uint16_t id = poolIdOf(id0); uint8_t p = id / 16, s = id % 16; uint32_t total = plen(p, s) * 2; if (off >= total) return 0; uint32_t n = total - off; if (n > mx) n = mx;
    for (uint32_t i = 0; i < n; i += 2) { int16_t v = p == slot ? (int16_t)(smp_buf[s][(off + i) / 2] * 32767.f) : pool[p][s][(off + i) / 2]; buf[i] = (uint8_t)v; buf[i + 1] = (uint8_t)((uint16_t)v >> 8); }
    return (int32_t)n;
}
static int assign(uint8_t slot, uint16_t id) { assigned.push_back({slot, id}); logf_("EV ASSIGN_SAMPLE slot=%u id=%u", (unsigned)slot, (unsigned)id); g_pendingReady.push_back({slot, (int)id}); return 0; }
static int del(uint8_t s) { samplerDelete(s); return 0; }
static int clr() { return 0; }

// new-layout hooks: global pool ids 1024+ (alias of patch*16+slot), PRESETS/<engine>.TXT through file_read 'R', STORE_RETRY
static int mancount() { if (!pooled) return 0; int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) n++; return n; }
static bool manat(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* ver, char* name) {
    int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) { if (n++ == i) {
        *id = (uint16_t)(1024 + p * 16 + q); *len = plen((uint8_t)p, (uint8_t)q); *rate = 48000; *ch = 1; *ver = 0xC0DE0000u + (uint32_t)(p * 16 + q); snprintf(name, 16, "S%u", (unsigned)*id); return true; } }
    return false;
}
static uint32_t slen(uint16_t id) { return plen((uint8_t)(poolIdOf(id) / 16), (uint8_t)(poolIdOf(id) % 16)); }
static uint16_t alias(uint16_t old) { return pooled ? (uint16_t)(1024 + old) : 0; }
static int storeRetry() { logf_("EV STORE_RETRY"); storeRetries++; storeState = 1; storePolls = 0; return 0; }
static std::string hexOf(const void* p, size_t n) { static const char* H = "0123456789ABCDEF"; std::string o; for (size_t i = 0; i < n; i++) { uint8_t b = ((const uint8_t*)p)[i]; o += H[b >> 4]; o += H[b & 15]; } return o; }
static std::string presetText(int e) {
    std::string t = "PMORPRESET 2\nengine " + std::to_string(e) + "\n";
    if (e >= 4) { SynPreset f; synFactoryPreset((uint8_t)(e - 4), 1, f); SynPreset u = f; u.cutoff = 1234.f; u.resonance = 0.25f;
        t += "f 1 PLUCK " + hexOf(&f, sizeof f) + "\nu 5 MYSOUND " + hexOf(&u, sizeof u) + "\n"; }
    else { uint8_t du[20] = {0}; du[0] = 2; float k[4] = {0.1f, 0.2f, 0.3f, 0.4f}; memcpy(du + 4, k, sizeof k);
        t += "f 3 SNAPPY - 00\nu 0 MYDRUM " + hexOf(du, sizeof du) + "\n"; }
    return t + "end 0\n";
}
static int32_t fread_(char k, uint16_t slotn, uint32_t off, uint8_t* buf, uint32_t mx, uint32_t* total) {
    if (k != 'R' || slotn >= 12 || poolErrors) return -1;
    std::string t = presetText(slotn); *total = (uint32_t)t.size(); if (off >= t.size()) return 0;
    uint32_t n = (uint32_t)t.size() - off; if (n > mx) n = mx; memcpy(buf, t.data() + off, n); return (int32_t)n;
}
static void aud(char, uint8_t, uint8_t) {}
static void clkmode(uint8_t) {}
static void legacy(const char*) {}
static void mode(ProtoMode) {}
static const ProtoOps ops = { tx, txSpace, idle, nowMs, "sim", patchSlot, patchEpoch, changed, follow, status, scan, plen, pver, pread, assign, del, clr, aud, clkmode, legacy, mode, fread_, nullptr, mancount, manat, slen, alias, storeRetry, nullptr, nullptr };
static void boot() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
    for (int i = 0; i < 4000; i++) smp_buf[3][i] = 0.5f * sinf(i * 0.05f); smp.len[3] = 4000;
    for (int p : {0, 7}) for (int s : {0, 5}) { pool[p][s].resize(2500 + p * 10 + s); for (size_t i = 0; i < pool[p][s].size(); i++) pool[p][s][i] = (int16_t)(1000 * (int)((i * 7 + p + s) % 31) - 15000); }
    protoInit(&ops);
}
}

static volatile sig_atomic_t g_stop = 0;
static void onSig(int) { g_stop = 1; }

int main(int argc, char** argv) {
    int port = 0; long dropAfter = 0; const char* logPath = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--pooled") sim::pooled = true;
        else if (a == "--drop-after-bytes" && i + 1 < argc) dropAfter = atol(argv[++i]);
        else if (a == "--log" && i + 1 < argc) logPath = argv[++i];
        else if (a == "--quiet") g_quiet = true;
        else if (a[0] != '-') port = atoi(a.c_str());
    }
    if (port <= 0 || port > 65534) { fprintf(stderr, "usage: simpmor_udp <port> [--pooled] [--drop-after-bytes N] [--log file] [--quiet]\n"); return 2; }
    if (logPath) { g_log = fopen(logPath, "w"); if (!g_log) { perror("log"); return 2; } }
    signal(SIGINT, onSig); signal(SIGTERM, onSig);
    sim::boot();
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) { perror("socket"); return 1; }
    sockaddr_in me {}; me.sin_family = AF_INET; me.sin_port = htons((uint16_t)port); inet_pton(AF_INET, "127.0.0.1", &me.sin_addr);
    if (bind(fd, (sockaddr*)&me, sizeof me) != 0) { perror("bind"); return 1; }
    int rb = 1 << 20; setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rb, sizeof rb);
    sockaddr_in dst = me; dst.sin_port = htons((uint16_t)(port + 1));
    if (!g_quiet) printf("simpmor_udp: listening 127.0.0.1:%d, replies to %d%s\n", port, port + 1, sim::pooled ? " (pooled)" : "");
    fflush(stdout);

    bool connected = false, silent = false; long sent = 0; std::string rxLine;
    auto setConnected = [&](bool c) { if (c != connected) { connected = c; protoConnected(c); logf_("EV connected=%d", c ? 1 : 0); } };
    auto flushOut = [&]() {
        if (sim::toHost.empty()) return;
        std::string o; o.swap(sim::toHost);
        if (silent || !connected) return;                                  // yanked: nothing reaches the host
        for (size_t off = 0; off < o.size();) {
            size_t n = o.size() - off; if (n > 480) n = 480;
            if (dropAfter > 0 && sent + (long)n >= dropAfter) {            // cable yank: last bytes cut mid-frame, then silence
                n = (size_t)(dropAfter - sent);
                if (n) sendto(fd, o.data() + off, n, 0, (sockaddr*)&dst, sizeof dst);
                sent += (long)n; silent = true; dropAfter = 0; logf_("EV YANK after %ld bytes sent", sent); return;
            }
            sendto(fd, o.data() + off, n, 0, (sockaddr*)&dst, sizeof dst); sent += (long)n; off += n;
        }
        { size_t k = o.find('\n'); logf_("TX %.100s", o.substr(0, k == std::string::npos ? 100 : k).c_str()); }
    };
    while (!g_stop) {
        fd_set rs; FD_ZERO(&rs); FD_SET(fd, &rs); timeval tv {0, 2000};
        int r = select(fd + 1, &rs, nullptr, nullptr, &tv);
        if (r > 0 && FD_ISSET(fd, &rs)) {
            for (;;) {
                uint8_t buf[2048]; ssize_t n = recvfrom(fd, buf, sizeof buf, MSG_DONTWAIT, nullptr, nullptr); if (n <= 0) break;
                std::string d((char*)buf, (size_t)n);
                if (d.rfind("SIM ", 0) == 0) {
                    int v = 0; logf_("CTL %s", d.c_str());
                    if (sscanf(d.c_str(), "SIM STORE %d", &v) == 1) { sim::storeState = v; sim::storePolls = 0; }
                    else if (sscanf(d.c_str(), "SIM POOLED %d", &v) == 1) sim::pooled = v != 0;
                    else if (sscanf(d.c_str(), "SIM VOL %d", &v) == 1) { float f = 0; sscanf(d.c_str(), "SIM VOL %f", &f); syn.volume = f; }
                    else if (sscanf(d.c_str(), "SIM BUS %d", &v) == 1) sim::busTicks += v;
                    else if (sscanf(d.c_str(), "SIM POLICY %d", &v) == 1) sim::policy = v;
                    else if (d.rfind("SIM EPOCH", 0) == 0) sim::epoch++;
                    else if (d.rfind("SIM DROP", 0) == 0) { silent = true; logf_("EV YANK (SIM DROP)"); }
                    else if (d.rfind("SIM QUIT", 0) == 0) g_stop = 1;
                    continue;
                }
                if (d == "PORTCLOSE" || d.rfind("PORTCLOSE", 0) == 0) { setConnected(false); silent = false; rxLine.clear(); logf_("RX PORTCLOSE"); continue; }
                if (silent) {
                    if (d.find("HELLO") == std::string::npos) continue;     // dead cable ignores everything until the host reconnects
                    silent = false; logf_("EV reconnect after yank");
                    protoConnected(false); connected = false;
                }
                setConnected(true);
                rxLine += d; size_t p;
                while ((p = rxLine.find('\n')) != std::string::npos) { logf_("RX %s", rxLine.substr(0, p).c_str()); rxLine.erase(0, p + 1); }
                if (rxLine.size() > 4096) rxLine.clear();
                protoRx((const uint8_t*)d.data(), d.size());
            }
        }
        sim::now = 1000 + wallMs();
        if (connected) protoPoll();
        if (!g_pendingReady.empty() && !silent) { auto pr = g_pendingReady; g_pendingReady.clear(); for (auto& x : pr) { logf_("EV SAMPLE_READY slot=%d id=%d", x.first, x.second); protoSampleDone((uint8_t)x.first, 0); } }
        flushOut();
    }
    logf_("EV quit"); if (g_log) fclose(g_log); close(fd);
    return 0;
}
