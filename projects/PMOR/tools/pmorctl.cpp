// pmorctl.cpp -- reference host implementation + protocol test client for docs/PROTOCOL.md.
//   pmorctl sim                      run the full test suite against an in-process simulated PMOR (engines + proto core)
//   pmorctl port DEV test            run the same suite against a real PMOR on DEV (e.g. /dev/ttyACM0)
//   pmorctl port DEV raw HELLO STUDIO 1     send one request, print the reply lines
//   pmorctl port DEV drift SECONDS   STUDIO hello, pull a sample repeatedly while timestamping MIDI clock (needs --midi)
// Copy the framing code (crc16, frame(), Reader) into the Max external.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <functional>
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

// ================================================================= link abstraction
struct Link {
    virtual ~Link() {}
    virtual void write(const std::string& s) = 0;
    virtual std::string read(int timeout_ms) = 0;   // whatever is available (may be empty)
    virtual void reopen() {}                        // close + reopen (replug simulation)
    virtual void advance(int ms) { (void)ms; }      // sim: move time
    bool sim = false;
};

// ================================================================= framing (copy me)
static std::string frame(const std::string& tag, const std::string& body) {
    std::string b = tag + " " + body; char t[8];
    snprintf(t, sizeof t, "*%04X\n", (unsigned)pmorCrc16(b.data(), b.size()));
    return "!" + b + t;
}
struct Reader {      // splits a byte stream into verified frames; resyncs on '\n'
    std::vector<std::string> badLines;
    std::string buf; std::deque<std::pair<std::string, std::string>> q; int bad = 0;
    void feed(const std::string& s) {
        buf += s; size_t p;
        while ((p = buf.find('\n')) != std::string::npos) {
            std::string l = buf.substr(0, p); buf.erase(0, p + 1);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l.empty() || l[0] != '!') continue;
            size_t st = l.rfind('*'); if (st == std::string::npos || l.size() - st != 5) { bad++; if (badLines.size() < 4) badLines.push_back(l.substr(0, 150)); continue; }
            std::string body = l.substr(1, st - 1);
            if (pmorCrc16(body.data(), body.size()) != (uint16_t)strtoul(l.substr(st + 1).c_str(), nullptr, 16)) { bad++; if (badLines.size() < 4) badLines.push_back(l.substr(0, 150)); continue; }
            size_t sp = body.find(' '); q.push_back({body.substr(0, sp), sp == std::string::npos ? "" : body.substr(sp + 1)});
        }
    }
};

struct Client {
    Link& L; Reader R; int seq = 0; std::deque<std::string> async;     // unsolicited ("-") bodies
    explicit Client(Link& l) : L(l) {}
    std::string raw;   // all bytes seen since last clear (for legacy console replies)
    std::string nextTag() { char t[8]; snprintf(t, sizeof t, "q%x", (unsigned)(++seq & 0xFFF)); return t; }
    void pump(int ms) { std::string s = L.read(ms); if (!s.empty()) { raw += s; R.feed(s); } }
    void drainAsync() {
        for (auto it = R.q.begin(); it != R.q.end();) { if (it->first == "-") { async.push_back(it->second); it = R.q.erase(it); } else ++it; }
    }
    // returns all body lines for this request; final line is OK.../ERR...; empty vector on timeout
    std::vector<std::string> request(const std::string& body, int timeout_ms = 3000) {
        std::string tag = nextTag(); L.write(frame(tag, body));
        std::vector<std::string> out; int waited = 0;
        while (waited <= timeout_ms) {
            drainAsync();
            while (!R.q.empty()) {
                auto f = R.q.front(); R.q.pop_front();
                if (f.first == "-") { async.push_back(f.second); continue; }
                if (f.first != tag) continue;
                out.push_back(f.second);
                if (f.second.rfind("OK ", 0) == 0 || f.second.rfind("ERR ", 0) == 0) return out;
            }
            pump(5); waited += 5; L.advance(5);
        }
        return {};
    }
};

// ================================================================= tests
static int g_fail = 0, g_pass = 0;
#define EXPECT(c, ...) do { if (c) g_pass++; else { g_fail++; printf("  FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::map<std::string, std::string> kv(const std::string& line) {
    std::map<std::string, std::string> m; size_t i = 0;
    while (i < line.size()) { size_t sp = line.find(' ', i); if (sp == std::string::npos) sp = line.size();
        std::string t = line.substr(i, sp - i); size_t eq = t.find('='); if (eq != std::string::npos) m[t.substr(0, eq)] = t.substr(eq + 1); i = sp + 1; }
    return m;
}
static bool isOK(const std::vector<std::string>& r) { return !r.empty() && r.back().rfind("OK ", 0) == 0; }
static bool isERR(const std::vector<std::string>& r, const char* code) { return !r.empty() && r.back().rfind("ERR ", 0) == 0 && r.back().find(std::string(" ") + code) != std::string::npos; }

static std::map<int, std::string> snapshotParams(Client& c) {   // id -> value text, BPM excluded (it follows the clock)
    std::map<int, std::string> m; auto r = c.request("GET_PARAMS", 8000);
    for (auto& l : r) if (l.rfind("PARAMS ", 0) == 0) { size_t i = 7; while (i < l.size()) { size_t sp = l.find(' ', i); if (sp == std::string::npos) sp = l.size(); std::string t = l.substr(i, sp - i); size_t eq = t.find('='); if (eq != std::string::npos && atoi(t.c_str()) != (int)pmorParamId(PD_GLOBAL, 0)) m[atoi(t.c_str())] = t.substr(eq + 1); i = sp + 1; } }
    for (int e = 0; e < 3; e++) { auto q = c.request("GET_SEQ " + std::to_string(e)); std::string j; for (auto& l : q) j += l; m[100000 + e] = j; }
    return m;
}
static void testSuite(Client& c, bool sim) {
    printf("[hello / modes]\n");
    auto r = c.request("SET_PARAM 1031 0.5");  EXPECT(isERR(r, "nohello"), "before hello: %s", r.empty() ? "timeout" : r.back().c_str());
    r = c.request("HELLO STUDIO 1");           EXPECT(isOK(r), "hello studio");
    std::map<std::string, std::string> h; if (!r.empty()) h = kv(r.back());
    printf("  fw=%s mode=%s schema=%s patch=%s\n", h["fw"].c_str(), h["mode"].c_str(), h["schema"].c_str(), h["patch"].c_str());
    EXPECT(h["mode"] == "STUDIO" && h["proto"] == "1" && h["schema"].size() == 8, "hello fields");
    r = c.request("PING");                      EXPECT(isOK(r), "ping");
    r = c.request("NOSAVE 1");                  EXPECT(isOK(r), "NOSAVE: this suite never lets a user patch be written");
    auto snap0 = snapshotParams(c);
    r = c.request("CLEAR_PATCH YES");           EXPECT(isERR(r, "mode"), "EDIT-only cmd refused in STUDIO");
    r = c.request("HELLO NEPTR 1");             EXPECT(isOK(r), "hello neptr");
    r = c.request("SET_PARAM 1031 0.5");        EXPECT(isERR(r, "mode"), "SET_PARAM refused in NEPTR");
    r = c.request("GET_PARAMS");                EXPECT(isOK(r), "GET_PARAMS allowed in NEPTR");
    r = c.request("HELLO EDIT 1");              EXPECT(isOK(r), "hello edit");
    r = c.request("NOSAVE 1");                  EXPECT(isOK(r), "NOSAVE (edit)");

    printf("[schema]\n");
    r = c.request("GET_SCHEMA", 8000);          EXPECT(isOK(r), "GET_SCHEMA");
    uint32_t hh = 2166136261u; int n = 0; std::vector<std::pair<int, std::string>> schema;
    for (auto& l : r) if (l.rfind("SCHEMA ", 0) == 0) {
        std::string rest = l.substr(7); hh = pmorFnv1a(hh, rest.data(), rest.size()); hh = pmorFnv1a(hh, "\n", 1); n++;
        schema.push_back({atoi(rest.c_str()), rest});
    }
    char hs[16]; snprintf(hs, sizeof hs, "%08X", (unsigned)hh);
    auto fin = kv(r.empty() ? "" : r.back());
    printf("  %d entries, hash %s (device says %s, hello said %s)\n", n, hs, fin["hash"].c_str(), h["schema"].c_str());
    EXPECT(fin["hash"] == hs && h["schema"] == hs, "schema hash recomputed from dump matches device");
    EXPECT(n == atoi(fin["n"].c_str()) && n > 400, "entry count");
    EXPECT(!sim || (size_t)n == pmorSchemaCount(), "sim count");

    printf("[params get/set]\n");
    r = c.request("GET_PARAMS", 6000);          EXPECT(isOK(r), "GET_PARAMS");
    std::map<int, float> params; for (auto& l : r) if (l.rfind("PARAMS ", 0) == 0) { size_t i = 7; while (i < l.size()) { size_t sp = l.find(' ', i); if (sp == std::string::npos) sp = l.size(); std::string t = l.substr(i, sp - i); size_t eq = t.find('='); if (eq != std::string::npos) params[atoi(t.c_str())] = (float)atof(t.c_str() + eq + 1); i = sp + 1; } }
    EXPECT((int)params.size() == n, "params count %zu vs schema %d", params.size(), n);
    int VOL = (int)pmorParamId(PD_SYNTH, 257);
    float orig = params[VOL];
    r = c.request("SET_PARAM " + std::to_string(VOL) + " 0.5");     EXPECT(isOK(r) && kv(r.back())["val"] == "0.5", "set synth volume: %s", r.empty() ? "timeout" : r.back().c_str());
    r = c.request("SET_PARAM " + std::to_string(VOL) + " 99");      EXPECT(isERR(r, "range"), "out of range refused");
    r = c.request("SET_PARAM 65000 1");                              EXPECT(isERR(r, "unknown"), "unknown id");
    for (int i = 0; i < 20; i++) { c.pump(5); c.L.advance(5); }
    c.drainAsync(); bool echoed = false; for (auto& a : c.async) if (a.rfind("P " + std::to_string(VOL) + " ", 0) == 0) echoed = true;
    EXPECT(!echoed, "no echo of own SET_PARAM");
    r = c.request("SET_PARAM " + std::to_string(VOL) + " " + std::to_string(orig));  EXPECT(isOK(r), "restore volume");
    if (sim) {      // device-originated edit -> push
        c.async.clear(); syn.volume = 0.8f;
        for (int i = 0; i < 40; i++) { c.pump(5); c.L.advance(5); }
        c.drainAsync(); bool got = false; for (auto& a : c.async) if (a.rfind("P " + std::to_string(VOL) + " 0.8", 0) == 0) got = true;
        EXPECT(got, "device edit pushed as P"); syn.volume = orig;
    }

    printf("[sequencer / patch]\n");
    for (int e = 0; e < 3; e++) { r = c.request("GET_SEQ " + std::to_string(e)); EXPECT(isOK(r) && r.size() >= 2 && r[0].rfind("SEQ ", 0) == 0, "GET_SEQ %d", e); }
    r = c.request("GET_PATCH", 8000);           EXPECT(isOK(r), "GET_PATCH");
    bool info = false; for (auto& l : r) if (l.rfind("PATCHINFO ", 0) == 0) info = true; EXPECT(info, "PATCHINFO present");
    r = c.request("STATUS");                    EXPECT(isOK(r), "STATUS: %s", r.empty() ? "timeout" : r.back().c_str());
    if (!r.empty()) printf("  %s\n", r.back().c_str());

    printf("[samples]\n");
    r = c.request("GET_MANIFEST", 10000);       EXPECT(isOK(r), "GET_MANIFEST");
    std::vector<std::string> man; for (auto& l : r) if (l.rfind("M ", 0) == 0) man.push_back(l);
    printf("  manifest: %zu samples\n", man.size());
    if (!man.empty()) {
        int id = atoi(man[0].c_str() + 2); uint32_t len = (uint32_t)strtoul(man[0].c_str() + 2 + std::to_string(id).size() + 1, nullptr, 10);
        std::vector<uint8_t> data; uint32_t total = len * 2, off = 0; int bad = 0;
        // pull half, "disconnect", resume from the first unverified offset
        for (int pass = 0; pass < 2; pass++) {
            uint32_t limit = pass == 0 ? total / 2 : total;
            if (pass == 1) { c.request("PING"); }
            while (off < limit) {
                r = c.request("PULL_SAMPLE " + std::to_string(id) + " " + std::to_string(off) + " 192", 4000);
                if (!isOK(r)) { bad++; break; }
                bool got = false;
                for (auto& l : r) if (l.rfind("C ", 0) == 0) {
                    char b64[1700]; unsigned i2, o2, nb; unsigned crc; if (sscanf(l.c_str(), "C %u %u %u %x %1699s", &i2, &o2, &nb, &crc, b64) == 5) {
                        uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf);
                        if (dl == (long)nb && pmorCrc32Of(buf, nb) == crc && o2 == off) { data.insert(data.end(), buf, buf + nb); off += nb; got = true; } else bad++;
                    }
                }
                if (!got) { bad++; break; }
            }
        }
        EXPECT(bad == 0 && off == total, "sample pull verified: %u/%u bytes, %d bad chunks", off, total, bad);
        r = c.request("PULL_SAMPLE " + std::to_string(id) + " " + std::to_string(total) + " 192");
        EXPECT(isOK(r) && kv(r.back())["n"] == "0", "pull at end -> n=0");
        r = c.request("PULL_SAMPLE " + std::to_string(id) + " 1 192");   EXPECT(isERR(r, "arg"), "odd offset refused");
        r = c.request("PULL_SAMPLE " + std::to_string(id) + " 0 192 4", 4000);
        int cl = 0; for (auto& l : r) if (l.rfind("C ", 0) == 0) cl++;
        EXPECT(isOK(r) && cl == (total >= 4 * 192 ? 4 : (int)((total + 191) / 192)), "pipelined pull count=4 -> %d chunks", cl);
    }
    r = c.request("AUDITION D 0 20");           EXPECT(isOK(r), "AUDITION");
    if (sim) { r = c.request("JOIN 0"); EXPECT(isERR(r, "noclock"), "JOIN without a live clock refused"); }
    r = c.request("JOIN 99999");               EXPECT(isERR(r, "arg"), "JOIN range checked");
    {   // raw card file, read-only
        r = c.request("PULL_FILE P " + h["patch"] + " 0 192 3", 4000);
        int fl = 0; for (auto& l : r) if (l.rfind("F ", 0) == 0) fl++;
        EXPECT(isOK(r) && fl == 3, "PULL_FILE patch file: %s", r.empty() ? "timeout" : r.back().c_str());
        if (isOK(r)) { auto k = kv(r.back()); printf("  patch file: total=%s bytes\n", k["total"].c_str()); }
        r = c.request("PULL_FILE X 0 0 192");  EXPECT(isERR(r, "arg"), "bad file kind refused");
    }

    printf("[framing robustness]\n");
    std::string good = frame("zz1", "PING"); std::string bad = good; bad[3] = (bad[3] == 'P') ? 'Q' : 'P';
    c.R.q.clear(); c.L.write(bad); for (int i = 0; i < 30; i++) { c.pump(5); c.L.advance(5); }
    bool nak = false; for (auto& f : c.R.q) if (f.second.find("NAK") != std::string::npos) nak = true;
    EXPECT(nak, "bad CRC -> NAK");
    c.L.write("garbage without newline "); c.L.write(good.substr(0, 5)); c.L.write(good.substr(5));    // glued junk then split frame
    for (int i = 0; i < 30; i++) { c.pump(5); c.L.advance(5); }
    bool pong = false; for (auto& f : c.R.q) if (f.first == "zz1") pong = true;
    EXPECT(pong, "junk-prefixed frame is recovered (resync at the '!' whose CRC checks out)");
    r = c.request("PING");                      EXPECT(isOK(r), "alive after garbage");
    std::string split = frame("zz2", "PING"); c.R.q.clear();
    for (char ch : split) { c.L.write(std::string(1, ch)); c.pump(1); c.L.advance(1); }       // one byte per write
    for (int i = 0; i < 30; i++) { c.pump(5); c.L.advance(5); }
    pong = false; for (auto& f : c.R.q) if (f.first == "zz2") pong = true;
    EXPECT(pong, "byte-at-a-time frame accepted");

    printf("[restore check]\n");
    {   auto snap1 = snapshotParams(c); int diff = 0;
        for (auto& kvp : snap0) { auto it = snap1.find(kvp.first); if (it == snap1.end() || it->second != kvp.second) { diff++; printf("  CHANGED %d: %s -> %s\n", kvp.first, kvp.second.c_str(), it == snap1.end() ? "(gone)" : it->second.c_str()); } }
        EXPECT(diff == 0 && snap0.size() == snap1.size(), "all %zu params + 3 sequencer lanes identical before/after the suite (%d differ)", snap0.size(), diff); }
    printf("[bye / replug]\n");
    r = c.request("BYE");                       EXPECT(isOK(r), "BYE");
    r = c.request("GET_PARAMS");                EXPECT(isERR(r, "nohello"), "standalone after BYE");
    r = c.request("HELLO STUDIO 1");            EXPECT(isOK(r), "hello again");
    c.L.reopen(); c.R = Reader();
    r = c.request("GET_PARAMS");                EXPECT(isERR(r, "nohello"), "port reopen -> standalone: %s", r.empty() ? "timeout" : r.back().c_str());
    r = c.request("HELLO STUDIO 1");            EXPECT(isOK(r), "resync hello");
    r = c.request("GET_PARAMS", 6000);          EXPECT(isOK(r), "full resync GET_PARAMS");
    c.request("BYE");
}

// ================================================================= simulated device
namespace sim {
static uint32_t now = 1000, epoch = 1; static uint8_t slot = 3;
static std::string toHost;                       // device -> host bytes
static std::vector<int16_t> pool[64][16];
static const char* audit = "";
// CDC TX model of TinyUSB + DWC2: tud_cdc_write() fills a FIFO, an IN transfer reads straight OUT OF that FIFO and completes on the host's next
// poll (one <= 512 B transfer per frame). Clearing the FIFO while a transfer is in flight breaks the accounting: the transfer never completes,
// the endpoint stays busy, and every later write queues forever (RX is unaffected). badReset models firmware that does that (0c48eaa).
static bool stall = false; static int resets = 0;        // stall: the host stopped draining the CDC TX FIFO
static std::string fifo; static bool inflight = false, wedged = false, badReset = false, bannerDone = false; static size_t xferN = 0;
static const size_t FIFO_CAP = 768;   // usb/tusb_config.h CFG_TUD_CDC_TX_BUFSIZE; FS bulk EP = 64 B per 1 ms frame
static void kick() { if (!inflight && !fifo.empty()) { inflight = true; xferN = fifo.size(); } }
static size_t tx(const uint8_t* d, size_t n) { if (stall) return 0; size_t sp = FIFO_CAP - fifo.size(); if (n > sp) n = sp; fifo.append((const char*)d, n); kick(); return n; }
static size_t txSpace() { return stall ? 0 : FIFO_CAP - fifo.size(); }
static void txReset() { resets++; if (badReset) { fifo.clear(); if (inflight) wedged = true; } }
static void usbFrame(int frames) { for (int f = 0; f < frames; f++) { if (!inflight || stall || wedged) return; size_t d = xferN < 64 ? xferN : 64; toHost.append(fifo, 0, d); fifo.erase(0, d); xferN -= d; if (!xferN) { inflight = false; kick(); } } }
static int svEngine = -1, svN = -1, svPad = -2; static std::string svName; static bool svNosave = false;
static int savePreset(int e, int n, const char* nm, int pad) { if (e < 0 || e > 11 || n < 0 || n > 31 || (e >= 4 && n < 5) || (pad >= 0 && e >= 4)) return -1; svPad = pad; if (svNosave) return -3; svEngine = e; svN = n; svName = nm; return 0; }
struct BinE { uint16_t id; uint32_t len, at; }; static std::vector<BinE> binv; static std::vector<uint16_t> poolIds = {1024, 1025, 1026}; static int binScans = 0, binNosave = 0;
static int refsScan(uint16_t id, unsigned first, unsigned n) { binScans++; return (id == 1024 && 3 >= first && 3 < first + n) ? 1 : 0; }
static int binDelete(uint16_t id) { if (binNosave) return -5; for (size_t i = 0; i < poolIds.size(); i++) if (poolIds[i] == id) { poolIds.erase(poolIds.begin() + (long)i); binv.push_back({id, 2000u + id, 100u + (uint32_t)binv.size()}); return 0; } return -1; }
static int binCount() { return (int)binv.size(); }
static bool binAt(int i, uint16_t* id, uint32_t* len, uint32_t* at) { if (i < 0 || i >= (int)binv.size()) return false; *id = binv[(size_t)i].id; *len = binv[(size_t)i].len; *at = binv[(size_t)i].at; return true; }
static uint32_t binToken() { uint32_t h = 0xABCD0001u + (uint32_t)binv.size(); for (auto& b : binv) h = h * 31 + b.id; return h; }
static int binRestore(uint16_t id) { for (size_t i = 0; i < binv.size(); i++) if (binv[i].id == id) { poolIds.push_back(id); binv.erase(binv.begin() + (long)i); return 0; } return -1; }
static int binEmpty(uint32_t tok) { if (tok != binToken()) return -1; int n = (int)binv.size(); binv.clear(); return n; }
// ARCHIVE_PATCHES sim: the device owns the job; archTick() is the main loop's patchTick (ONE file per pass), driven from SimLink::read, never from a session.
static std::vector<int> archSlots = {0, 5, 9, 40}; static int archK = 0, archNextK = 1, archNosave = 0, archScans = 0, archTicks = 0, archRun = 0, archMoved = 0, archFailed = 0, archDone = 0, archFailSlot = -1, archCur = 0;
static std::vector<int> archFailedSlots;
static int archScan(unsigned first, unsigned n, uint32_t* h, uint16_t* cnt) { archScans++; for (int s : archSlots) if ((unsigned)s >= first && (unsigned)s < first + n) { (*cnt)++; *h = *h * 31 + (uint32_t)s; } return 0; }
static int archBegin() { if (archNosave) return -5; if (!archK || archDone) { archK = archNextK++; archDone = 0; } archRun = 1; archMoved = archFailed = archCur = 0; archFailedSlots.clear(); return archK; }
static void archTick() {
    if (!archRun) return; archTicks++;
    if (archCur >= (int)archSlots.size()) { archRun = 0; if (!archFailed) archDone = 1; return; }
    int sl = archSlots[(size_t)archCur];
    if (sl == archFailSlot) { archFailed++; archFailedSlots.push_back(sl); archCur++; } else { archMoved++; archSlots.erase(archSlots.begin() + archCur); }
}
static int archList(unsigned slot, char* out, size_t n) { for (int s : archSlots) if ((unsigned)s == slot) { snprintf(out, n, "PMR"); return 1; } if (n) out[0] = 0; return 0; }
static int archState(char* out, size_t n) {
    int o = snprintf(out, n, "state=%s k=%d moved=%d failed=%d", archRun ? "running" : "idle", archK, archMoved, archFailed);
    if (archFailed) { o += snprintf(out + o, n - (size_t)o, " fail="); for (size_t i = 0; i < archFailedSlots.size(); i++) o += snprintf(out + o, n - (size_t)o, "%sP%02d.PMR", i ? "," : "", archFailedSlots[i]); }
    return archRun;
}
static uint32_t seeded = 0; static void tempoSeed(uint32_t m) { seeded = m; }
static bool idle() { return true; }
static uint32_t nowMs() { return now; }
static uint8_t patchSlot() { return slot; }
static uint32_t patchEpoch() { return epoch; }
static void changed() {}
static bool follow() { return false; }
static void status(char* b, size_t n) { snprintf(b, n, "tx_stalls=%u resets=%u/%u/%u/%u/%u/%u/%u clk=internal running=0 bpm=%d", (unsigned)protoStatTxStalls(), protoStatResets(0), protoStatResets(1), protoStatResets(2), protoStatResets(3), protoStatResets(4), protoStatResets(5), protoStatResets(6), (int)(clk.bpm + 0.5f)); }
static int scan(uint8_t) { return 1; }
static uint32_t plen(uint8_t p, uint8_t s) { return p == slot ? smp.len[s] : (uint32_t)pool[p][s].size(); }
static uint32_t pver(uint8_t p, uint8_t s) { return 0x1000u + p * 16 + s; }
static int32_t pread(uint16_t id, uint32_t off, uint8_t* buf, uint32_t mx) {
    uint8_t p = id / 16, s = id % 16; uint32_t len = plen(p, s); uint32_t total = len * 2;
    if (off >= total) return 0; uint32_t n = total - off; if (n > mx) n = mx;
    for (uint32_t i = 0; i < n; i += 2) {
        int16_t v = p == slot ? (int16_t)(smp_buf[s][(off + i) / 2] * 32767.f) : pool[p][s][(off + i) / 2];
        buf[i] = (uint8_t)v; buf[i + 1] = (uint8_t)((uint16_t)v >> 8);
    }
    return (int32_t)n;
}
static int assign(uint8_t, uint16_t) { return 0; }
static int del(uint8_t s) { samplerDelete(s); return 0; }
static int clr() { return 0; }
static void aud(char, uint8_t, uint8_t) {}
static void clkmode(uint8_t) {}
static void legacy(const char*) {}
static void mode(ProtoMode) {}
static bool newman = false;   // "simnew": serve GET_MANIFEST through the new-layout hooks (same ids)
static int mancount() { if (!newman) return 0; int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) n++; return n; }
static bool manat(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* ver, char* name) {
    int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) { if (n++ == i) { *id = (uint16_t)(p * 16 + q); *len = plen((uint8_t)p, (uint8_t)q); *rate = 48000; *ch = 1; *ver = 0xABCD0000u + (uint32_t)*id; snprintf(name, 16, "S%04u", (unsigned)*id); return true; } } return false; }
static uint32_t slen(uint16_t id) { return plen((uint8_t)(id / 16), (uint8_t)(id % 16)); }
static bool nosaveFlag = false; static void nosv(bool on) { nosaveFlag = on; }
static int32_t fread_(char k, uint16_t slotn, uint32_t off, uint8_t* buf, uint32_t max, uint32_t* total) {   // fake 1000-byte patch file
    if (k != 'P' || slotn != 3) return -1; *total = 1000; if (off >= 1000) return 0; uint32_t n = 1000 - off; if (n > max) n = max;
    for (uint32_t i = 0; i < n; i++) buf[i] = (uint8_t)((off + i) * 7); return (int32_t)n; }
static const ProtoOps ops = { tx, txSpace, idle, nowMs, "sim", patchSlot, patchEpoch, changed, follow, status, scan, plen, pver, pread, assign, del, clr, aud, clkmode, legacy, mode, fread_, nullptr, mancount, manat, slen, nullptr, nullptr, nullptr, nullptr, nullptr, nosv, txReset, tempoSeed, savePreset, refsScan, binDelete, binCount, binAt, binToken, binRestore, binEmpty, archScan, archBegin, archList, archState };

struct SimLink : Link {
    SimLink() { sim = true; }
    void write(const std::string& s) override { protoRx((const uint8_t*)s.data(), s.size()); }
    std::string read(int ms) override { protoPoll(); archTick(); usbFrame(ms < 1 ? 1 : ms); std::string o; o.swap(toHost); return o; }
    // host (re)opens the port. Real order inside one device main-loop pass: printBootMarksOnce() writes the banner as soon as DTR is up
    // (once per boot), THEN studioPoll() sees the DTR edge and resets the session.
    void reopen() override {
        protoConnected(false);
        if (!bannerDone) { bannerDone = true; const char* b = "PMOR v3 boot ms: usb=1 oled=2 sens=3 sd=4 patch=5\r\n"; tx((const uint8_t*)b, strlen(b)); }
        protoConnected(true); toHost.clear();
    }
    void advance(int ms) override { now += (uint32_t)ms; }
};
static void boot() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
    for (int i = 0; i < 4000; i++) smp_buf[3][i] = 0.5f * sinf(i * 0.05f); smp.len[3] = 4000;       // a live sample in the current patch
    for (int p : {0, 7}) for (int s : {0, 5}) { pool[p][s].resize(2500 + p * 10 + s); for (size_t i = 0; i < pool[p][s].size(); i++) pool[p][s][i] = (int16_t)(1000 * (int)((i * 7 + p + s) % 31) - 15000); }
    protoInit(&ops); protoConnected(true);
    fifo.clear(); inflight = wedged = false; bannerDone = true; toHost.clear();   // (a suite boot is already past the banner)
}
static void coldBoot() {   // power-up: engines + protocol up, USB mounted, nobody has opened the port yet
    boot(); protoConnected(false); fifo.clear(); inflight = wedged = false; bannerDone = false; toHost.clear(); now += 2000;
}
}  // namespace sim

// ================================================================= real serial port
struct PortLink : Link {
    std::string dev; int fd = -1;
    explicit PortLink(const std::string& d) : dev(d) { open_(); usleep(400000); }   // let the device see DTR before the first bytes (they are dropped while it is not "connected")
    void open_() {
        fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) { perror("open"); exit(2); }
        termios t; tcgetattr(fd, &t); cfmakeraw(&t); t.c_cflag |= CLOCAL | CREAD; tcsetattr(fd, TCSANOW, &t);
        int dtr = TIOCM_DTR | TIOCM_RTS; ioctl_dtr(dtr, true);
    }
    void ioctl_dtr(int bits, bool on) { ::ioctl(fd, on ? TIOCMBIS : TIOCMBIC, &bits); }
    void write(const std::string& s) override { size_t o = 0; while (o < s.size()) { ssize_t n = ::write(fd, s.data() + o, s.size() - o); if (n < 0) { if (errno == EAGAIN) { usleep(1000); continue; } perror("write"); return; } o += (size_t)n; } }
    std::string read(int ms) override {
        pollfd p{fd, POLLIN, 0}; std::string o; if (::poll(&p, 1, ms) > 0) { char b[512]; ssize_t n = ::read(fd, b, sizeof b); if (n > 0) o.assign(b, (size_t)n); } return o;
    }
    void reopen() override { ioctl_dtr(TIOCM_DTR, false); ::close(fd); usleep(700000); open_(); usleep(300000); }
};

int main(int argc, char** argv) {
    if (argc >= 2 && (!strcmp(argv[1], "sim") || !strcmp(argv[1], "simnew") || !strcmp(argv[1], "simbad"))) {
        sim::newman = !strcmp(argv[1], "simnew"); sim::badReset = !strcmp(argv[1], "simbad");     // simbad = negative control: firmware that clears the TX FIFO on a session reset
        {   printf("[cold boot, fresh open, HELLO]\n");
            sim::coldBoot(); sim::SimLink l0; Client c0(l0);
            l0.reopen();                                                                              // host opens the port (DTR up), banner goes out
            auto r = c0.request("HELLO STUDIO 1", 100); EXPECT(isOK(r), "cold boot + fresh DTR open: HELLO produces bytes on the wire within 100 ms");
            r = c0.request("STATUS", 100); EXPECT(isOK(r), "and STATUS right after");
            for (int i = 0; i < 5; i++) { l0.reopen(); r = c0.request("HELLO STUDIO 1", 100); if (!isOK(r)) break; }
            EXPECT(isOK(r), "five more close/open cycles: HELLO still answered");
            if (sim::badReset) { printf("simbad: %d failed (expected > 0 for the negative control)\n", g_fail); return g_fail ? 0 : 3; }
        }
        sim::boot(); sim::SimLink l; Client c(l);
        testSuite(c, true);
        {   printf("[TEMPO seed]\n");
            auto r = c.request("HELLO STUDIO 1"); EXPECT(isOK(r), "hello");
            r = c.request("TEMPO 100000"); EXPECT(isOK(r) && r.back().find("bpm=100000") != std::string::npos && sim::seeded == 100000, "TEMPO 100000 -> OK TEMPO bpm=100000, seed delivered (%s)", r.empty() ? "timeout" : r.back().c_str());
            r = c.request("TEMPO 39999");  EXPECT(isERR(r, "TEMPO"), "TEMPO below range -> ERR TEMPO arg");
            r = c.request("TEMPO 400001"); EXPECT(isERR(r, "TEMPO"), "TEMPO above range -> ERR TEMPO arg");
            r = c.request("TEMPO abc");    EXPECT(isERR(r, "TEMPO"), "TEMPO non-number -> ERR TEMPO arg");
            r = c.request("TEMPO");        EXPECT(isERR(r, "TEMPO"), "TEMPO no argument -> ERR TEMPO arg");
            c.request("BYE"); r = c.request("TEMPO 100000"); EXPECT(isERR(r, "nohello"), "TEMPO needs a session");
            c.request("HELLO STUDIO 1"); }
        {   printf("[recycle bin]\n");
            auto r = c.request("HELLO EDIT 1"); EXPECT(isOK(r), "hello edit");
            sim::binScans = 0; r = c.request("DELETE_POOL 1024"); EXPECT(isERR(r, "referenced") && r.back().find("refs=1") != std::string::npos && sim::poolIds.size() == 3, "referenced sample refused (%s), nothing moved", r.empty() ? "timeout" : r.back().c_str());
            EXPECT(sim::binScans == 4, "the reference scan ran in %d bounded steps of 16 patches", sim::binScans);
            r = c.request("DELETE_POOL 1024 force"); EXPECT(isOK(r) && sim::binv.size() == 1 && sim::poolIds.size() == 2, "force moves it to the bin");
            r = c.request("DELETE_POOL 1025"); EXPECT(isOK(r) && sim::binv.size() == 2, "unreferenced moves without force");
            r = c.request("DELETE_POOL 4000"); EXPECT(isERR(r, "notfound"), "unknown id -> ERR notfound");
            r = c.request("DELETE_POOL abc");  EXPECT(isERR(r, "DELETE_POOL"), "bad id -> ERR arg");
            r = c.request("BIN_LIST"); std::string tok; int bl = 0; for (auto& l : r) if (l.rfind("B ", 0) == 0) bl++; if (!r.empty()) tok = kv(r.back())["token"];
            EXPECT(isOK(r) && bl == 2 && !tok.empty() && r.front().rfind("B 1024 3024 S1024 ", 0) == 0, "BIN_LIST: %d lines `B <id> <len> <name> <deleted_at>` + OK n= token=%s", bl, tok.c_str());
            r = c.request("BIN_EMPTY DEADBEEF"); EXPECT(isERR(r, "token") && sim::binv.size() == 2, "wrong token erases nothing");
            r = c.request("BIN_RESTORE 1024"); EXPECT(isOK(r) && sim::binv.size() == 1 && sim::poolIds.size() == 2, "BIN_RESTORE brings it back");
            r = c.request("BIN_RESTORE 1024"); EXPECT(isERR(r, "notfound"), "restore of something not in the bin -> ERR notfound");
            r = c.request("BIN_LIST"); tok = kv(r.back())["token"]; r = c.request("BIN_EMPTY " + tok); EXPECT(isOK(r) && sim::binv.empty() && r.back().find("n=1") != std::string::npos, "BIN_EMPTY with the BIN_LIST token is the only erase");
            sim::binNosave = 1; r = c.request("DELETE_POOL 1026"); EXPECT(isERR(r, "nosave"), "NOSAVE session -> ERR nosave"); sim::binNosave = 0;
            c.request("BYE"); c.request("HELLO STUDIO 1"); r = c.request("DELETE_POOL 1026"); EXPECT(isERR(r, "mode"), "STUDIO (not EDIT) -> ERR mode"); r = c.request("BIN_LIST"); EXPECT(isERR(r, "mode"), "BIN_LIST needs EDIT");
            c.request("HELLO STUDIO 1"); }
        {   printf("[ARCHIVE_PATCHES]\n");
            auto r = c.request("HELLO EDIT 1"); EXPECT(isOK(r), "hello edit");
            r = c.request("ARCHIVE_PATCHES"); std::string tok = kv(r.back())["token"];
            EXPECT(isOK(r) && !tok.empty() && kv(r.back())["patches"] == "4" && sim::archScans == 4 && sim::archTicks == 0, "no-arg gate: token=%s patches=4 after %d bounded scan steps, nothing moved", tok.c_str(), sim::archScans);
            r = c.request("ARCHIVE_PATCHES DEADBEEF"); EXPECT(isERR(r, "token") && sim::archSlots.size() == 4 && !sim::archRun, "wrong token moves nothing");
            sim::archNosave = 1; r = c.request("ARCHIVE_PATCHES " + tok); EXPECT(isERR(r, "nosave") && sim::archSlots.size() == 4, "NOSAVE session -> ERR nosave"); sim::archNosave = 0;
            r = c.request("ARCHIVE_PATCHES zz"); EXPECT(isERR(r, "arg"), "bad token -> ERR arg");
            r = c.request("ARCHIVE_LIST"); EXPECT(isOK(r) && kv(r.back())["remaining"] == "4" && kv(r.back())["slots"] == "4" && kv(r.back())["state"] == "idle" && r[0] == "A 0 PMR", "ARCHIVE_LIST: 4 files left, idle (%s)", r.empty() ? "timeout" : r.back().c_str());
            // (1)+(2): start returns at once; the device moves ONE file per main-loop pass on its own
            sim::archTicks = 0; r = c.request("ARCHIVE_PATCHES " + tok, 8000);
            EXPECT(isOK(r) && r.back().find("started dir=ARCHIVE_1") != std::string::npos, "start replies at once with the folder: %s", r.empty() ? "timeout" : r.back().c_str());
            r = c.request("ARCHIVE_PATCHES"); EXPECT(isOK(r) && r.back().find("running") != std::string::npos, "second start while running = progress report, no restart: %s", r.empty() ? "timeout" : r.back().c_str());
            for (int i = 0; i < 40 && sim::archRun; i++) c.pump(5);
            EXPECT(!sim::archRun && sim::archSlots.empty() && sim::archMoved == 4 && sim::archTicks == 5, "moved 4 files one per pass (%d ticks incl. the finishing one)", sim::archTicks);
            r = c.request("ARCHIVE_LIST"); EXPECT(isOK(r) && kv(r.back())["remaining"] == "0" && kv(r.back())["moved"] == "4" && kv(r.back())["state"] == "idle", "ARCHIVE_LIST after: nothing left, moved=4"); }
        {   printf("[ARCHIVE_PATCHES session drop]\n");   // the observed hardware failure: the session dies mid-job (liveness / BYE / HELLO); the job must finish anyway
            sim::archSlots = {0, 2, 5, 9, 11, 40, 41, 50}; sim::archDone = 1; sim::archFailSlot = -1; sim::archTicks = 0;
            auto r = c.request("HELLO EDIT 1"); r = c.request("ARCHIVE_PATCHES"); std::string tok = kv(r.back())["token"];
            r = c.request("ARCHIVE_PATCHES " + tok); EXPECT(isOK(r) && r.back().find("dir=ARCHIVE_2") != std::string::npos, "second archive = ARCHIVE_2 (previous was finished)");
            uint32_t live0 = protoStatResets(3);
            c.L.advance(6000); c.pump(5); c.pump(5);                    // host goes silent > 5 s: liveness drops the session
            EXPECT(protoStatResets(3) == live0 + 1 && protoMode() == PM_STANDALONE, "liveness dropped the session (resets=%u)", protoStatResets(3));
            EXPECT(sim::archRun || sim::archSlots.empty(), "job survives the session drop");
            for (int i = 0; i < 60 && sim::archRun; i++) c.pump(5);
            EXPECT(!sim::archRun && sim::archSlots.empty() && sim::archMoved == 8, "job completed with nobody connected: moved=%d", sim::archMoved);
            r = c.request("ARCHIVE_LIST"); EXPECT(isERR(r, "nohello"), "list needs a session");
            r = c.request("HELLO EDIT 1"); r = c.request("ARCHIVE_LIST"); EXPECT(isOK(r) && kv(r.back())["remaining"] == "0" && kv(r.back())["moved"] == "8", "re-HELLO reads the finished result");
            // BYE and a fresh HELLO mid-job do not stop it either
            sim::archSlots = {1, 3, 7}; sim::archDone = 1; r = c.request("ARCHIVE_PATCHES"); tok = kv(r.back())["token"]; r = c.request("ARCHIVE_PATCHES " + tok); EXPECT(isOK(r), "third start");
            c.request("BYE"); EXPECT(sim::archRun, "BYE mid-job: still running"); c.request("HELLO EDIT 1");
            for (int i = 0; i < 40 && sim::archRun; i++) c.pump(5);
            EXPECT(sim::archSlots.empty() && sim::archMoved == 3, "finished after BYE + HELLO"); }
        {   printf("[ARCHIVE_PATCHES stuck file]\n");    // a file whose rename keeps failing: skipped + named, the rest moves, a retry resumes the SAME folder
            sim::archSlots = {0, 5, 9, 40}; sim::archDone = 1; sim::archFailSlot = 5;
            auto r = c.request("HELLO EDIT 1"); r = c.request("ARCHIVE_PATCHES"); std::string tok = kv(r.back())["token"];
            r = c.request("ARCHIVE_PATCHES " + tok); int k1 = sim::archK; EXPECT(isOK(r), "start");
            for (int i = 0; i < 40 && sim::archRun; i++) c.pump(5);
            r = c.request("ARCHIVE_LIST"); auto m = kv(r.back());
            EXPECT(isOK(r) && m["moved"] == "3" && m["failed"] == "1" && m["fail"] == "P05.PMR" && m["remaining"] == "1" && !sim::archDone, "one failed file named (fail=%s), 3 moved, 1 left, folder not marked done", m["fail"].c_str());
            sim::archFailSlot = -1; r = c.request("ARCHIVE_PATCHES"); tok = kv(r.back())["token"];
            r = c.request("ARCHIVE_PATCHES " + tok); EXPECT(isOK(r) && sim::archK == k1 && r.back().find("dir=ARCHIVE_" + std::to_string(k1)) != std::string::npos, "retry resumes the same ARCHIVE_%d (no new folder)", k1);
            for (int i = 0; i < 40 && sim::archRun; i++) c.pump(5);
            EXPECT(sim::archSlots.empty() && sim::archDone && sim::archK == k1, "retry finished, folder marked done"); }
        {   auto r = c.request("HELLO EDIT 1"); (void)r; c.request("BYE"); c.request("HELLO STUDIO 1"); r = c.request("ARCHIVE_PATCHES"); EXPECT(isERR(r, "mode"), "STUDIO -> ERR mode");
            r = c.request("ARCHIVE_LIST"); EXPECT(isOK(r), "ARCHIVE_LIST is read-only, allowed in STUDIO"); c.request("HELLO STUDIO 1"); }
        {   printf("[SAVE_PRESET]\n");
            auto r = c.request("HELLO EDIT 1"); EXPECT(isOK(r), "hello edit");
            r = c.request("SAVE_PRESET 1 7 MYSNARE"); EXPECT(isOK(r) && sim::svEngine == 1 && sim::svN == 7 && sim::svName == "MYSNARE", "drums engine 1 n 7 saved (%s)", r.empty() ? "timeout" : r.back().c_str());
            { bool pc = false; for (int t = 0; t < 100 && !pc; t += 5) { c.pump(5); c.L.advance(5); c.drainAsync(); for (auto& a : c.async) if (a == "PRESETS_CHANGED 1") pc = true; } EXPECT(pc, "PRESETS_CHANGED 1 pushed so hosts re-pull the file"); }
            r = c.request("SAVE_PRESET 6 9 LEAD");    EXPECT(isOK(r) && sim::svEngine == 6, "synth engine 6 (SYN2) idx 9 saved");
            r = c.request("SAVE_PRESET 1 9 A_B_9 5"); EXPECT(isOK(r) && sim::svName == "A_B_9" && sim::svPad == 5 && r.back().find("name=A_B_9 pad=5") != std::string::npos, "name with '_' kept literal on the wire, optional pad 5 passed (%s)", r.empty() ? "timeout" : r.back().c_str());
            r = c.request("SAVE_PRESET 1 9 X 16"); EXPECT(isERR(r, "SAVE_PRESET"), "pad 16 -> ERR arg");
            r = c.request("SAVE_PRESET 6 9 X 3"); EXPECT(isERR(r, "SAVE_PRESET"), "pad on a synth engine -> ERR arg");
            r = c.request("SAVE_PRESET 6 3 LEAD");    EXPECT(isERR(r, "SAVE_PRESET"), "synth idx < 5 (factory) -> ERR arg");
            r = c.request("SAVE_PRESET 12 9 X");      EXPECT(isERR(r, "SAVE_PRESET"), "engine 12 -> ERR arg");
            r = c.request("SAVE_PRESET 1 32 X");      EXPECT(isERR(r, "SAVE_PRESET"), "n 32 -> ERR arg");
            r = c.request("SAVE_PRESET 1 7 TOOLONGNAME1"); EXPECT(isERR(r, "SAVE_PRESET"), "name > 9 chars -> ERR arg");
            sim::svNosave = true; r = c.request("SAVE_PRESET 1 8 NOPE"); EXPECT(isERR(r, "nosave"), "NOSAVE session -> ERR nosave"); sim::svNosave = false;
            c.request("BYE"); r = c.request("HELLO STUDIO 1"); r = c.request("SAVE_PRESET 1 7 X"); EXPECT(isERR(r, "mode"), "STUDIO (not EDIT) -> ERR mode");
            c.request("HELLO STUDIO 1"); }
        {   printf("[replug: session re-entry, every state, HELLO within 100 ms]\n");
            auto hello = [&]() { auto r = c.request("HELLO STUDIO 1", 100); return isOK(r); };
            auto resets = [&](int i) { auto r = c.request("STATUS"); if (r.empty()) return -1; std::string v = kv(r.back())["resets"]; int k = 0; size_t p = 0; while (k < i && (p = v.find('/', p)) != std::string::npos) { p++; k++; } return atoi(v.c_str() + (k == i ? p : 0)); };
            (void) resets;
            // unplug mid GET_PARAMS: read only the first bytes of the reply, then replug
            hello(); c.L.write(frame("gp", "GET_PARAMS")); c.pump(5); c.R = Reader(); c.L.reopen(); c.R = Reader(); sim::toHost.clear();
            EXPECT(hello(), "HELLO answered after an unplug in the middle of GET_PARAMS");
            { auto r = c.request("GET_PARAMS", 6000); EXPECT(isOK(r), "GET_PARAMS works after that replug"); }
            // unplug mid PULL_FILE
            c.L.write(frame("pf", "PULL_FILE P 3 0 256 4")); c.pump(5); c.L.reopen(); c.R = Reader();
            EXPECT(hello(), "HELLO answered after an unplug in the middle of PULL_FILE");
            // replug with a garbage partial line still in the RX buffer
            c.L.write("!q1 GET_PAR"); c.L.reopen(); c.R = Reader(); c.L.write("\x01\x02garbage!zz NOT*FRAME"); 
            EXPECT(hello(), "HELLO answered with a partial line + garbage left from before the unplug");
            c.L.write("!q9 PING"); c.L.write(frame("hh", "HELLO STUDIO 1"));                          // a partial frame glued in front of a good request
            { bool got = false; for (int t = 0; t < 100 && !got; t += 5) { c.pump(5); c.L.advance(5); for (auto& f : c.R.q) if (f.first == "hh" && f.second.rfind("OK HELLO", 0) == 0) got = true; } EXPECT(got, "a partial frame glued in front of a HELLO does not eat it"); }
            // DTR toggle x50, HELLO each time
            int okc = 0; for (int i = 0; i < 50; i++) { c.L.reopen(); c.R = Reader(); if (hello()) okc++; }
            EXPECT(okc == 50, "DTR toggle x50: HELLO answered every time (%d/50)", okc);
            // HELLO flood with no reads in between, then one more: still answered, session sane
            for (int i = 0; i < 300; i++) c.L.write(frame("f", "HELLO STUDIO 1"));
            c.R = Reader(); sim::toHost.clear(); EXPECT(hello(), "HELLO flood x300: next HELLO answered");
            // HELLO in the middle of a running job (no reopen): answered at once, the old job's lines stop
            c.L.write(frame("gj", "GET_PARAMS")); c.pump(0);
            c.R = Reader(); sim::toHost.clear();
            { auto r = c.request("HELLO STUDIO 1", 100); EXPECT(isOK(r), "HELLO mid-job answered within 100 ms");
              c.pump(50); bool stray = false; for (auto& f : c.R.q) if (f.first == "gj") stray = true; EXPECT(!stray, "the aborted job sends nothing after the HELLO reply"); }
            // no DTR at all: a host that never asserts it still gets answers
            protoConnected(false); sim::toHost.clear(); c.R = Reader();
            EXPECT(hello(), "HELLO answered with DTR low");
            // a stalled transport + replug: stale TX backlog cleared, first thing out is the HELLO reply
            sim::stall = true; for (int t = 0; t < 400; t += 5) { pmorParamSet((uint16_t)pmorSchemaAt(0)->id, (float)(t % 7) * 0.1f); c.L.advance(5); c.L.read(0); }
            sim::stall = false; c.L.reopen(); c.R = Reader(); EXPECT(hello(), "replug after a stalled TX: HELLO answered, no stale backlog first");
            { auto r = c.request("STATUS"); std::string v = r.empty() ? "" : kv(r.back())["resets"]; EXPECT(!v.empty() && v != "0/0/0/0/0/0/0", "STATUS resets=<by cause>: %s", v.c_str()); }
            c.request("BYE"); }
        {   printf("[tx stall: host stops draining while params flood]\n");
            auto stat = [&](const char* k) { auto r = c.request("STATUS"); return r.empty() ? std::string("?") : kv(r.back())[k]; };
            auto r = c.request("HELLO STUDIO 1"); EXPECT(isOK(r), "hello before stall");
            int before = atoi(stat("tx_stalls").c_str()), id0 = (int)pmorSchemaAt(0)->id;
            sim::stall = true;
            for (int t = 0; t < 3500; t += 5) {          // 3.5 s: param flood (1 edit / 5 ms) + a PING per second keeps the liveness rule off
                pmorParamSet((uint16_t)id0, (float)(t % 7) * 0.1f); c.L.advance(5); c.L.read(0);
                if (t % 1000 == 0) c.L.write(frame("pp", "PING"));
            }
            sim::stall = false;
            EXPECT(sim::resets >= 1, "transport reset hook called (%d)", sim::resets);
            r = c.request("HELLO STUDIO 1");             EXPECT(isOK(r), "HELLO answered right after the stall, no replug");
            int after = atoi(stat("tx_stalls").c_str());   EXPECT(after > before, "STATUS tx_stalls %d -> %d", before, after);
            r = c.request("GET_PARAMS", 6000);           EXPECT(isOK(r), "session fully usable after self-heal");
            // short stall: ring full of pushes, no watchdog yet; a fresh HELLO must still be answered
            sim::stall = true;
            for (int t = 0; t < 1500; t += 5) { pmorParamSet((uint16_t)id0, (float)(t % 5) * 0.1f); c.L.advance(5); c.L.read(0); }
            sim::stall = false;
            r = c.request("HELLO STUDIO 1");             EXPECT(isOK(r), "HELLO not swallowed by a stale backlog");
            c.request("BYE"); }
        printf("\npmorctl sim: %d passed, %d failed\n", g_pass, g_fail); return g_fail ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "port")) {
        PortLink l(argv[2]); Client c(l);
        if (!strcmp(argv[3], "test")) { testSuite(c, false); printf("\npmorctl port: %d passed, %d failed\n", g_pass, g_fail); return g_fail ? 1 : 0; }
        if (!strcmp(argv[3], "raw")) {
            std::string body; for (int i = 4; i < argc; i++) { if (i > 4) body += " "; body += argv[i]; }
            for (auto& s : c.request(body, 8000)) printf("%s\n", s.c_str()); return 0;
        }
    }
    if (argc >= 4 && !strcmp(argv[1], "port") && !strcmp(argv[3], "script")) {
        // pmorctl port DEV script "HELLO EDIT 1" "sleep 2" "STATUS" ... : one session, replies printed
        PortLink l(argv[2]); Client c(l);
        struct BadReport { Client& cc; ~BadReport() { if (cc.R.bad) { fprintf(stderr, "[pmorctl: %d bad/garbled frames received]\n", cc.R.bad); for (auto& b : cc.R.badLines) fprintf(stderr, "   BAD: %s\n", b.c_str()); } } } badReport{c};
        for (int i = 4; i < argc; i++) {
            std::string a = argv[i];
            if (a.rfind("sleep ", 0) == 0) { int ms = (int)(atof(a.c_str() + 6) * 1000); for (int t = 0; t < ms; t += 20) { c.pump(20); c.drainAsync(); if (t % 1500 == 1480) c.request("PING"); } continue; }   // keeps the session alive (liveness 2 s rule)
            if (a.rfind("legacy ", 0) == 0) { c.raw.clear(); c.L.write(a.substr(7) + "\n"); for (int t = 0; t < 600; t += 20) c.pump(20); printf("> %s\n%s\n", a.c_str(), c.raw.c_str()); continue; }
            if (a.rfind("pullfile ", 0) == 0) {    // pullfile KIND SLOT OUTFILE : one raw card file, CRC-checked per chunk
                char kc; int slot; char of[200]; int cmax = 1024; if (sscanf(a.c_str() + 9, "%c %d %199s %d", &kc, &slot, of, &cmax) < 3) continue;
                uint32_t off = 0, total = 1; std::vector<uint8_t> data; bool good = true; timespec pt0; clock_gettime(CLOCK_MONOTONIC, &pt0);
                while (off < total) { auto r = c.request(std::string("PULL_FILE ") + kc + " " + std::to_string(slot) + " " + std::to_string(off) + " " + std::to_string(cmax) + " " + std::to_string(getenv("PULL_COUNT") ? atoi(getenv("PULL_COUNT")) : 16), 6000);
                    if (r.empty() || r.back().rfind("OK", 0) != 0) { printf("pullfile %c %d: %s\n", kc, slot, r.empty() ? "timeout" : r.back().c_str()); good = false; break; }
                    total = (uint32_t)strtoul(kv(r.back())["total"].c_str(), 0, 10); bool got = false;
                    for (auto& x : r) { char kk; int sl2; unsigned o2, nb, crc; char b64[1700]; if (sscanf(x.c_str(), "F %c %d %u %u %x %1699s", &kk, &sl2, &o2, &nb, &crc, b64) == 6) { uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf); if (dl == (long)nb && o2 == off && pmorCrc32Of(buf, nb) == crc) { data.insert(data.end(), buf, buf + nb); off += nb; got = true; } else good = false; } }
                    if (!got) { good = false; break; } }
                if (good) { FILE* f = fopen(of, "wb"); fwrite(data.data(), 1, data.size(), f); fclose(f); { timespec pt1; clock_gettime(CLOCK_MONOTONIC, &pt1); double sec = (pt1.tv_sec - pt0.tv_sec) + (pt1.tv_nsec - pt0.tv_nsec) / 1e9; printf("pullfile %c %d -> %s (%zu bytes, crc32 %08x) %.2f s = %.0f KB/s\n", kc, slot, of, data.size(), pmorCrc32Of(data.data(), data.size()), sec, data.size() / 1024.0 / sec); } }
                continue; }
            if (a.rfind("watchbpm ", 0) == 0) {    // watchbpm SECONDS : print CLOCK_MONOTONIC ms of every pushed BPM change (id 6144)
                double secs = atof(a.c_str() + 9); timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0); double base = t0.tv_sec * 1000.0 + t0.tv_nsec / 1e6;
                for (;;) { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); double now = t.tv_sec * 1000.0 + t.tv_nsec / 1e6; if (now - base > secs * 1000.0) break;
                    c.pump(5); c.drainAsync(); while (!c.async.empty()) { std::string x = c.async.front(); c.async.pop_front(); if (x.rfind("P 6144 ", 0) == 0) printf("BPM %.3f %s\n", now, x.c_str() + 7); }
                    static double lastping = 0; if (now - lastping > 1500) { lastping = now; c.request("PING"); } }
                continue; }
            if (a.rfind("backup ", 0) == 0) {      // backup DIR : read-only dump of everything the protocol exposes
                std::string dir = a.substr(7); std::string mk = "mkdir -p '" + dir + "/samples'"; if (system(mk.c_str())) return 3;
                auto dump = [&](const char* cmd, const char* file) { auto r = c.request(cmd, 15000); FILE* f = fopen((dir + "/" + file).c_str(), "w"); for (auto& l : r) fprintf(f, "%s\n", l.c_str()); fclose(f); printf("%s -> %s (%zu lines)\n", cmd, file, r.size()); return r; };
                dump("STATUS", "status.txt"); dump("GET_SCHEMA", "schema.txt"); dump("GET_PATCH", "current_patch.txt");
                for (int e = 0; e < 3; e++) dump(("GET_SEQ " + std::to_string(e)).c_str(), (std::string("seq") + std::to_string(e) + ".txt").c_str());
                auto man = dump("GET_MANIFEST", "manifest.txt"); int ok = 0, fail = 0; if (getenv("PMORCTL_FILES_ONLY")) man.clear();
                for (auto& l : man) if (l.rfind("M ", 0) == 0) {
                    unsigned id, len; if (sscanf(l.c_str(), "M %u %u", &id, &len) != 2) continue;
                    std::vector<uint8_t> data; uint32_t off = 0, total = len * 2; bool good = true;
                    while (off < total) {
                        auto r = c.request("PULL_SAMPLE " + std::to_string(id) + " " + std::to_string(off) + " 1024 16", 4000);
                        if (r.empty() || r.back().rfind("OK", 0) != 0) { good = false; break; }
                        bool got = false;
                        for (auto& x : r) { unsigned i2, o2, nb, crc; char b64[1700]; if (sscanf(x.c_str(), "C %u %u %u %x %1699s", &i2, &o2, &nb, &crc, b64) == 5) { uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf); if (dl == (long)nb && o2 == off && pmorCrc32Of(buf, nb) == crc) { data.insert(data.end(), buf, buf + nb); off += nb; got = true; } else good = false; } }
                        if (!got || !good) { good = false; break; }
                    }
                    char fn[96]; snprintf(fn, sizeof fn, "%s/samples/P%02uS%02u.i16", dir.c_str(), id / 16, id % 16);
                    if (good && data.size() == total) { FILE* f = fopen(fn, "wb"); fwrite(data.data(), 1, data.size(), f); fclose(f); ok++; } else { fail++; printf("  FAILED id %u\n", id); }
                }
                printf("samples: %d ok, %d failed\n", ok, fail);
                {   // raw card files (read-only): every patch slot's .PMR/.PMR.BAK/.AUD/.AUD.BAK
                    system(("mkdir -p '" + dir + "/files'").c_str()); int nf = 0, nbad = 0; const char kinds[] = "PKAL"; const char* ext[] = {".PMR", ".PMR.BAK", ".AUD", ".AUD.BAK"};
                    for (int slot = 0; slot < 64; slot++) for (int ki = 0; ki < 4; ki++) {
                        uint32_t off = 0, total = 1; std::vector<uint8_t> data; bool exists = true, good = true;
                        while (off < total) {
                            auto r = c.request(std::string("PULL_FILE ") + kinds[ki] + " " + std::to_string(slot) + " " + std::to_string(off) + " 1024 16", 6000);
                            if (r.empty() || r.back().rfind("OK", 0) != 0) { exists = false; break; }
                            auto k = kv(r.back()); total = (uint32_t)strtoul(k["total"].c_str(), 0, 10); bool got = false;
                            for (auto& x : r) { char kc; int sl2; unsigned o2, nb, crc; char b64[1700]; if (sscanf(x.c_str(), "F %c %d %u %u %x %1699s", &kc, &sl2, &o2, &nb, &crc, b64) == 6) { uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf); if (dl == (long)nb && o2 == off && pmorCrc32Of(buf, nb) == crc) { data.insert(data.end(), buf, buf + nb); off += nb; got = true; } else good = false; } }
                            if (!got) break;
                        }
                        if (!exists) continue;
                        if (good && data.size() == total) { char fn[96]; snprintf(fn, sizeof fn, "%s/files/P%02d%s", dir.c_str(), slot, ext[ki]); FILE* f = fopen(fn, "wb"); fwrite(data.data(), 1, data.size(), f); fclose(f); nf++; } else { nbad++; printf("  FILE FAILED slot %d kind %c\n", slot, kinds[ki]); }
                    }
                    printf("card files: %d ok, %d failed\n", nf, nbad);
                    // new tree (read-only): MANIFEST.TXT, PATCHES/Pnn.TXT, PRESETS/<engine>.TXT
                    system(("mkdir -p '" + dir + "/newtree'").c_str()); int nn = 0;
                    for (int ki = 0; ki < 3; ki++) for (int slot = (ki == 0 ? 0 : 0); slot < (ki == 0 ? 1 : ki == 1 ? 64 : 12); slot++) {
                        char kc = "MTR"[ki]; uint32_t off = 0, total = 1; std::vector<uint8_t> data; bool exists = true, good = true;
                        while (off < total) {
                            auto r = c.request(std::string("PULL_FILE ") + kc + " " + std::to_string(slot) + " " + std::to_string(off) + " 1024 16", 6000);
                            if (r.empty() || r.back().rfind("OK", 0) != 0) { exists = false; break; }
                            auto k = kv(r.back()); total = (uint32_t)strtoul(k["total"].c_str(), 0, 10); bool got = false;
                            for (auto& x : r) { char kk; int sl2; unsigned o2, nb, crc; char b64[1700]; if (sscanf(x.c_str(), "F %c %d %u %u %x %1699s", &kk, &sl2, &o2, &nb, &crc, b64) == 6) { uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf); if (dl == (long)nb && o2 == off && pmorCrc32Of(buf, nb) == crc) { data.insert(data.end(), buf, buf + nb); off += nb; got = true; } else good = false; } }
                            if (!got) break;
                        }
                        if (exists && good && data.size() == total) { char fn[96]; snprintf(fn, sizeof fn, "%s/newtree/%c%02d.txt", dir.c_str(), kc, slot); FILE* f = fopen(fn, "wb"); fwrite(data.data(), 1, data.size(), f); fclose(f); nn++; }
                    }
                    printf("new-tree files: %d\n", nn);
                }
                continue;
            }
            if (a.rfind("stream ", 0) == 0) {      // stream ID SECONDS : pull the sample over and over, verify every chunk
                int id = atoi(a.c_str() + 7); double secs = atof(strrchr(a.c_str(), ' ') + 1); timeval t0, t1; gettimeofday(&t0, 0);
                long chunks = 0, bad = 0, bytes = 0, total = 0;
                for (;;) {
                    gettimeofday(&t1, 0); if ((t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6 > secs) break;
                    uint32_t off = 0;
                    for (;;) {
                        auto r = c.request("PULL_SAMPLE " + std::to_string(id) + " " + std::to_string(off) + " " + std::to_string(getenv("STREAM_CHUNK") ? atoi(getenv("STREAM_CHUNK")) : 1024) + " 16", 3000);
                        if (r.empty() || r.back().rfind("OK", 0) != 0) { bad++; break; }
                        unsigned i2, o2, nb, crc; char b64[1700]; bool got = false;
                        for (auto& x : r) if (sscanf(x.c_str(), "C %u %u %u %x %1699s", &i2, &o2, &nb, &crc, b64) == 5) {
                            uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf);
                            if (dl == (long)nb && o2 == off && pmorCrc32Of(buf, nb) == crc) { chunks++; bytes += nb; off += nb; got = true; } else bad++;
                        }
                        if (!got) break;
                    }
                    total++;
                }
                gettimeofday(&t1, 0); double el = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
                printf("stream: %ld full passes, %ld chunks, %ld bytes in %.1fs = %.0f chunks/s %.0f B/s, bad=%ld\n", total, chunks, bytes, el, chunks / el, bytes / el, bad);
                continue;
            }
            std::vector<std::string> r;
            for (int attempt = 0; attempt < (a.rfind("HELLO", 0) == 0 ? 4 : 1); attempt++) { r = c.request(a, a.rfind("HELLO", 0) == 0 ? 2000 : 8000); if (!r.empty()) break; }   // the first bytes after a port (re)open can be lost before the device sees DTR
            printf("> %s\n", a.c_str()); for (auto& x : r) printf("  %s\n", x.c_str()); if (r.empty()) printf("  (timeout)\n");
        }
        return 0;
    }
    fprintf(stderr, "usage: pmorctl sim | pmorctl port DEV test | pmorctl port DEV raw CMD...\n"); return 1;
}
