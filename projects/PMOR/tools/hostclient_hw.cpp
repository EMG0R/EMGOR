// hostclient_hw.cpp -- skynet's max/pmor_host.h (the Max object's protocol engine) against the REAL PMOR on a serial port.
// Mirrors tests/test_hostclient.cpp, minus the parts that need a simulated device (device-originated edits, forced PATCH_CHANGED).
// Every hardware session sends NOSAVE 1 first, and the parameter snapshot is asserted identical at the end.
//   build_host/hostclient_hw /dev/ttyACM0
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <string>
#include <vector>
#include "../max/pmor_host.h"
#include "proto_util.h"
static int g_pass = 0, g_fail = 0;
#define EXPECT(c, ...) do { if (c) g_pass++; else { g_fail++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int fd = -1; static std::string dev;
static void openPort() { fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK); if (fd < 0) { perror("open"); exit(2); } termios t; tcgetattr(fd, &t); cfmakeraw(&t); t.c_cflag |= CLOCAL | CREAD; tcsetattr(fd, TCSANOW, &t); int b = TIOCM_DTR | TIOCM_RTS; ioctl(fd, TIOCMBIS, &b); }
static pmorhost::Client C;
static void pumpOnce(int ms) { pollfd p{fd, POLLIN, 0}; if (poll(&p, 1, ms) > 0) { char b[512]; ssize_t n = ::read(fd, b, sizeof b); if (n > 0) C.feed(b, (size_t)n); } C.tick(ms); }
static void run(int ms) { for (int t = 0; t < ms; t += 5) pumpOnce(5); }
template <class F> static bool until(F pred, int maxMs) { for (int t = 0; t < maxMs; t += 5) { if (pred()) return true; pumpOnce(5); } return pred(); }
static std::string frame(const std::string& tag, const std::string& body) { std::string b = tag + " " + body; char t[8]; snprintf(t, sizeof t, "*%04X\n", (unsigned)pmorCrc16(b.data(), b.size())); return "!" + b + t; }
int main(int argc, char** argv) {
    dev = argc > 1 ? argv[1] : "/dev/ttyACM0"; openPort();
    int synced = 0; std::vector<std::string> infos; float lastVol = -1; const uint16_t VOL = (2 << 10) * 0 + (1 << 10 | 257);   // PD_SYNTH=1, index 257
    C.send = [](const std::string& s) { size_t o = 0; while (o < s.size()) { ssize_t n = ::write(fd, s.data() + o, s.size() - o); if (n < 0) { usleep(1000); continue; } o += (size_t)n; } };
    C.onParam = [&](uint16_t id, float v) { if (id == VOL) lastVol = v; };
    C.onInfo = [&](const std::string& s) { infos.push_back(s); };
    C.onSynced = [&]() { synced++; };

    printf("[connect + full resync]\n");
    C.connect("STUDIO");
    EXPECT(until([] { return C.state() == pmorhost::Client::READY; }, 15000), "READY (state %s)", C.stateName());
    C.send(frame("hc1", "NOSAVE 1")); run(200);       // never let a test write a user patch
    EXPECT(C.hello["mode"] == "STUDIO" && C.hello["proto"] == "1", "hello fields mode=%s", C.hello["mode"].c_str());
    EXPECT(C.schemaCount > 400 && C.schemaHash == C.hello["schema"], "schema %zu entries hash %s vs hello %s", C.schemaCount, C.schemaHash.c_str(), C.hello["schema"].c_str());
    EXPECT(C.params.size() == C.schemaCount, "params mirrored %zu", C.params.size());
    EXPECT(C.seq.size() == 3, "sequences %zu", C.seq.size());
    EXPECT(!C.manifest.empty(), "manifest %zu", C.manifest.size());
    EXPECT(synced == 1, "onSynced once (%d)", synced);
    auto snap0 = C.params; auto seq0 = C.seq;
    printf("  fw=%s patch=%s clk=%s, %zu params, %zu manifest entries\n", C.hello["fw"].c_str(), C.hello["patch"].c_str(), C.hello["clk"].c_str(), C.params.size(), C.manifest.size());

    printf("[host edit -> device (restored afterwards)]\n");
    float orig = C.params[VOL]; lastVol = -1; C.setParam(VOL, 0.5f);
    EXPECT(until([&] { return fabsf(lastVol - 0.5f) < 1e-3f; }, 2000), "ack value %.3f", lastVol);
    size_t ni = infos.size(); C.setParam(VOL, 99.f); run(400);
    EXPECT(infos.size() > ni && infos.back().find("refused") != std::string::npos, "range error reported: %s", infos.empty() ? "none" : infos.back().c_str());
    lastVol = -1; C.setParam(VOL, orig); until([&] { return fabsf(lastVol - orig) < 1e-3f; }, 2000);

    printf("[sample cache]\n");
    {
        int done = 0, bad = 0, prog = 0; uint32_t lastTotal = 0; std::vector<int> ids;
        C.haveSample = [](const pmorhost::Client::Sample&) { return false; };
        C.onSampleProgress = [&](const pmorhost::Client::Sample&, uint32_t, uint32_t t, int) { prog++; lastTotal = t; };
        C.onSampleDone = [&](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>&, bool ok) { done++; if (!ok) bad++; ids.push_back(sm.id); };
        C.pullSamples(false);
        EXPECT(until([&] { return done >= 1 && C.pullsQueued() == 0; }, 90000), "current-patch pulls finish (pool ids need GET_PATCH first: wait for the first sample)");
        int slot = atoi(C.hello["patch"].c_str());
        bool onlyCurrent = true; for (int id : ids) if (id < 1024 && id / 16 != slot) onlyCurrent = false;   // pool ids (>= 1024) come from PATCHINFO ids=
        EXPECT(done >= 1 && bad == 0 && onlyCurrent, "STUDIO pulled %d sample(s) of patch %d, %d bad", done, slot, bad);
        EXPECT(prog > 5 && lastTotal > 0, "progress callbacks (%d, total %u bytes)", prog, lastTotal);
        int d2 = 0; C.haveSample = [](const pmorhost::Client::Sample&) { return true; }; C.onSampleDone = [&](const pmorhost::Client::Sample&, const std::vector<uint8_t>&, bool) { d2++; };
        C.pullSamples(false); run(300);
        EXPECT(d2 == 0 && C.pullsQueued() == 0, "cached samples are not pulled again");
        C.onSampleDone = nullptr; C.onSampleProgress = nullptr;
    }

    printf("[getPatch resync]\n");
    int s0 = synced; C.getPatch(); until([&] { return synced > s0; }, 8000);
    EXPECT(synced == s0 + 1, "GET_PATCH resync (synced %d -> %d)", s0, synced);

    printf("[liveness: no traffic for 4 s]\n");
    run(4000);
    EXPECT(C.state() == pmorhost::Client::READY, "still READY after 4 s idle (auto PING)");
    C.send(frame("hc2", "GET_PARAMS")); run(1500);

    printf("[restore check]\n");
    { int diff = 0; for (auto& kv : snap0) { auto it = C.params.find(kv.first); if (kv.first == (6 << 10) || it == C.params.end()) continue; if (fabsf(it->second - kv.second) > 1e-4f) { diff++; printf("  CHANGED %u: %g -> %g\n", kv.first, kv.second, it->second); } }
      EXPECT(diff == 0, "parameter mirror identical before/after (%d differ)", diff); }

    printf("[unplug / replug (port close + reopen)]\n");
    C.portClosed(); ::close(fd); usleep(700000); openPort(); usleep(300000);
    synced = 0; C.connect("EDIT");
    EXPECT(until([] { return C.state() == pmorhost::Client::READY; }, 15000), "resync after replug");
    EXPECT(C.hello["mode"] == "EDIT" && synced == 1, "EDIT mode, one resync");

    printf("[bye]\n");
    C.bye(); run(400);
    EXPECT(C.state() == pmorhost::Client::DISCONNECTED, "BYE");
    printf("\nhostclient_hw: %d passed, %d failed (bad frames %d, naks %d)\n", g_pass, g_fail, C.badFrames, C.nakCount);
    return g_fail ? 1 : 0;
}
