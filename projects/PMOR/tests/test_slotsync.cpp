// test_slotsync.cpp -- EMGOR bug "the Ableton sequence used a different sample than the unplugged PMOR": the core must play exactly the sample PMOR has in a slot.
// pmor_slotsync.h (the logic pmor.proto uses) + pmor_host.h + pmor_cache.h against fakepmor.h. Every check reads the FILE that would be handed to pmor.core~ and compares
// its bytes with what the fake PMOR holds in that slot.   Build: see tests/README-style line in max/README.md (c++ -std=gnu++14 -I pmor_core -I . ... libpmor_core.a)
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fakepmor.h"
#include "../max/pmor_slotsync.h"

static int pass = 0, fail = 0;
#define CHECK(c, ...) do { if (c) pass++; else { fail++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool exists(const std::string& f) { struct stat st; return stat(f.c_str(), &st) == 0; }
static void writeFile(const std::string& f, const std::vector<uint8_t>& d) { FILE* o = fopen(f.c_str(), "wb"); if (o) { fwrite(d.data(), 1, d.size(), o); fclose(o); } }
static int countFiles(const std::string& dir, const std::string& pfx) { std::string cmd = "ls '" + dir + "' 2>/dev/null | grep -c '^" + pfx + "'"; FILE* p = popen(cmd.c_str(), "r"); int n = 0; if (p) { if (fscanf(p, "%d", &n) != 1) n = 0; pclose(p); } return n; }

struct Rig {
    FakePmor dev; pmorhost::Client C; FakeLink L; pmorcache::Cache cache; pmorslots::Sync sync; std::string root, sess, cdir;
    struct Slot { std::string path; bool hasCrc = false; uint32_t crc = 0; } slot[16]; int emits = 0;
    Rig(const char* tag) : L(dev, C), sync(C, cache) {
        char t[] = "/tmp/pmorslotsyncXXXXXX"; root = mkdtemp(t); (void)tag; sess = root + "/set/PMOR"; cdir = root + "/cache"; cache.session = sess; cache.cache = cdir;
        sync.emit = [this](int s, const std::string& p, bool hc, uint32_t c, uint32_t, uint32_t) { slot[s].path = p; slot[s].hasCrc = hc; slot[s].crc = c; emits++; };
        sync.info = [this](const std::string& s) { L.infos.push_back(s); };
        C.haveSample = [this](const pmorhost::Client::Sample& sm) { return sync.have(sm); };
        C.onSampleDone = [this](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>& d, bool ok) { if (sync.done(sm, d, ok)) sync.push(); };
        C.onPool = [this]() { sync.manifestReloaded(); sync.push(); };
        C.onSynced = [this]() { C.pullSamples(false); sync.push(); };
        C.onPatchSwitched = [this](int) { C.pullSamples(false); sync.push(); };
        C.onAssigned = [this](int, bool ok, const std::string&) { if (ok) { C.pullSamples(false); sync.push(); } };
    }
    ~Rig() { std::string cmd = "rm -rf '" + root + "'"; if (system(cmd.c_str())) {} }
    void connect(const char* mode = "STUDIO") { dev.toHost.clear(); C.connect(mode); L.until([&] { return C.state() == pmorhost::Client::READY; }, 8000); settle(); }
    void settle() { L.until([&] { return C.pullsQueued() == 0; }, 30000); L.run(100); }
    std::vector<uint8_t> played(int s) const { std::vector<uint8_t> b; if (slot[s].path.empty()) return b; pmorcache::Cache::readAll(slot[s].path, b); return b; }
    bool plays(int s) const { return !slot[s].path.empty() && played(s) == dev.slotBytes(s); }     // the core plays exactly what PMOR has in slot s
    bool silent(int s) const { return slot[s].path.empty(); }
};

int main() {
    // ---- 1. baseline pool layout: slots 3 and 5 hold pool ids 1100 / 1101
    {
        Rig r("base"); r.dev.add(1100, 2000, 1); r.dev.add(1101, 2000, 2); r.dev.add(1102, 2000, 3); r.dev.slotIds[3] = 1100; r.dev.slotIds[5] = 1101;
        r.connect();
        CHECK(r.plays(3) && r.plays(5), "slots 3/5 play PMOR's samples");
        CHECK(r.silent(0) && r.silent(4), "empty slots are silent");
        CHECK(r.slot[3].hasCrc && r.slot[3].crc == r.dev.pool[1100].crc, "pool id: manifest crc travels with the slot");
        CHECK(exists(r.sess + "/S1100_" + [&] { char b[16]; snprintf(b, sizeof b, "%08X.aud", (unsigned)r.dev.pool[1100].crc); return std::string(b); }()), "named S<id>_<crc8>.aud in the session dir");
    }
    // ---- 2. stale same-size file (wrong content under the right name) is rejected, quarantined, re-pulled
    {
        Rig r("stale"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100;
        char nm[40]; snprintf(nm, sizeof nm, "/S1100_%08X.aud", (unsigned)r.dev.pool[1100].crc);
        std::vector<uint8_t> wrong(4000, 0x11);                       // same length as the real thing, different content
        pmorcache::Cache::makeDirs(r.sess); pmorcache::Cache::makeDirs(r.cdir); writeFile(r.sess + nm, wrong); writeFile(r.cdir + nm, wrong);
        r.connect();
        CHECK(r.plays(3), "stale equal-size file in session + cache was not used; slot plays PMOR's sample");
        CHECK(exists(r.sess + nm + ".bad") && exists(r.cdir + nm + ".bad") || exists(r.sess + nm + ".bad"), "wrong-crc file quarantined (.bad)");
        CHECK(r.dev.pulls[1100] >= 1, "re-pulled from PMOR (%d pull(s))", r.dev.pulls[1100]);
        CHECK(r.L.info("quarantined"), "quarantine reported");
    }
    // ---- 3. file corrupted AFTER it was verified once: the next push refuses it (empty slot) and a re-pull fixes it; precedence session/cache does not matter
    {
        Rig r("corrupt"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100; r.connect();
        CHECK(r.plays(3), "initial");
        std::string f = r.slot[3].path; std::vector<uint8_t> b; pmorcache::Cache::readAll(f, b); b[100] ^= 0xFF; writeFile(f, b);
        r.sync.manifestReloaded(); r.sync.push();                     // (a same-size corrupt file; push must verify, refuse, re-pull)
        CHECK(r.silent(3) || r.plays(3), "never plays the corrupt file");
        CHECK(r.slot[3].path.empty() || r.slot[3].path != f || r.plays(3), "corrupt file not handed to the core");
        r.settle(); CHECK(r.plays(3), "after the re-pull the slot plays the right sample");
        // a good copy only in the cache dir, a wrong-crc copy in the session dir: cache copy used, session copy quarantined
        std::string good = r.slot[3].path; char nm[40]; snprintf(nm, sizeof nm, "/S1100_%08X.aud", (unsigned)r.dev.pool[1100].crc);
        std::vector<uint8_t> g; pmorcache::Cache::readAll(good, g); std::string other = good.rfind(r.sess, 0) == 0 ? r.cdir : r.sess; pmorcache::Cache::makeDirs(other);
        writeFile(other + nm, g); std::string bad = good; writeFile(bad, std::vector<uint8_t>(4000, 0x22));       // wrong in the original dir
        int pulls = r.dev.pulls[1100]; r.sync.manifestReloaded(); r.sync.push();
        { std::string sp = r.slot[3].path; std::vector<uint8_t> now; pmorcache::Cache::readAll(sp, now);
          CHECK(r.plays(3) && sp.size() > strlen(nm) && sp.compare(sp.size() - strlen(nm), strlen(nm), nm) == 0 && now == g, "the good copy wins when this dir's file has a wrong crc; a cache-only good copy is also copied into the session dir (%s)", sp.c_str()); }
        CHECK(r.dev.pulls[1100] == pulls, "no needless re-pull when the other dir has the good copy");
    }
    // ---- 4. two crc variants of one id coexist; only the exact crc is used
    {
        Rig r("variants"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100; r.connect();
        uint32_t crcA = r.dev.pool[1100].crc; std::string fa = r.slot[3].path;
        r.dev.add(1100, 2000, 9);                                      // PMOR re-recorded: same id, same length, new content + crc
        uint32_t crcB = r.dev.pool[1100].crc; CHECK(crcA != crcB, "variants differ");
        r.dev.patchChanged(); r.L.run(300); r.settle();
        CHECK(r.plays(3), "slot plays the NEW sample");
        CHECK(exists(fa) && r.slot[3].path != fa && r.slot[3].hasCrc && r.slot[3].crc == crcB, "old variant stays on disk untouched, new one has its own file");
        CHECK(countFiles(r.sess, "S1100_") == 2, "two files for id 1100 coexist (%d)", countFiles(r.sess, "S1100_"));
        r.dev.add(1100, 2000, 1);                                      // PMOR is back to variant A: the old file is valid again, no pull needed
        int pulls = r.dev.pulls[1100]; r.dev.patchChanged(); r.L.run(300); r.settle();
        CHECK(r.plays(3) && r.slot[3].path == fa && r.dev.pulls[1100] == pulls, "variant A found again by its exact crc, no re-pull");
    }
    // ---- 5. legacy ids: a stale same-size file on disk is never reused; every epoch re-pulls; files named by the host's CRC of the data
    {
        Rig r("legacy"); r.dev.legacy = true; r.dev.patch = 3; r.dev.add(3 * 16 + 0, 2000, 1); r.dev.add(3 * 16 + 5, 2000, 2); r.dev.slotIds[0] = 48; r.dev.slotIds[5] = 53;
        pmorcache::Cache::makeDirs(r.sess); writeFile(r.sess + "/P03S00_00000000.aud", std::vector<uint8_t>(4000, 0x33)); writeFile(r.cdir + "/P03S00_00000000.aud", std::vector<uint8_t>(4000, 0x33));
        pmorcache::Cache::makeDirs(r.cdir); writeFile(r.cdir + "/P03S00_00000000.aud", std::vector<uint8_t>(4000, 0x33));
        r.connect();
        CHECK(r.plays(0) && r.plays(5), "legacy slots play PMOR's samples, not the stale file");
        CHECK(r.slot[0].path.find("P03S00_00000000") == std::string::npos, "not the ver=0 file (%s)", r.slot[0].path.c_str());
        char b[24]; snprintf(b, sizeof b, "_%08X.aud", (unsigned)r.dev.pool[48].crc); CHECK(r.slot[0].path.find(b) != std::string::npos, "named by the host-computed CRC (%s)", r.slot[0].path.c_str());
        int p48 = r.dev.pulls[48];
        r.dev.add(48, 2000, 7);                                        // content changes on PMOR (same id, same length, ver still 0)
        r.dev.patchChanged(); r.L.run(300); r.settle();
        CHECK(r.dev.pulls[48] == p48 + 1 && r.plays(0), "legacy id re-pulled on PATCH_CHANGED, plays the new content (pulls %d -> %d, plays %d)", p48, r.dev.pulls[48], (int)r.plays(0));
    }
    // ---- 6. the override is gone: assign, then replug / resync => PMOR's ids, not the assigned one
    {
        Rig r("assign"); r.dev.add(1100, 2000, 1); r.dev.add(1101, 2400, 2); r.dev.add(1102, 2600, 3); r.dev.slotIds[3] = 1100; r.connect();
        r.C.assign(4, 1101); r.L.run(600); r.settle();
        CHECK(r.dev.slotIds[4] == 1101 && r.plays(4) && r.plays(3), "after SAMPLE_READY slot 4 plays pool 1101 (PMOR's slot map, re-read)");
        CHECK(r.C.hello["mode"] == "STUDIO", "back in STUDIO after the assign");
        // replug: PMOR (e.g. unsaved / reloaded) holds a different sample in slot 4 now
        r.C.portClosed(); r.sync.manifestReloaded(); r.sync.blankAll(); CHECK(r.silent(3) && r.silent(4), "not READY: every slot silent");
        r.dev.slotIds[4] = 1102; r.connect();
        CHECK(r.plays(4) && r.played(4).size() == 5200, "replug: slot 4 maps to PMOR's id 1102, not the earlier assignment");
        r.dev.slotIds[4] = 0; r.C.portClosed(); r.connect();
        CHECK(r.silent(4) && r.plays(3), "replug with an emptied slot 4: silent (no host-side memory)");
        // an assign that PMOR reports for a slot, then a full resync after the device forgot it
        r.C.assign(4, 1101); r.L.run(600); r.settle(); CHECK(r.plays(4), "assign again");
        r.dev.slotIds[4] = 0; r.dev.patchChanged(); r.L.run(400); r.settle(); CHECK(r.silent(4), "PATCH_CHANGED re-read: assignment forgotten when PMOR no longer has it");
    }
    // ---- 7. patch change remaps; slots of the new patch only
    {
        Rig r("patch"); r.dev.add(1100, 2000, 1); r.dev.add(1101, 2200, 2); r.dev.add(1102, 2400, 3); r.dev.slotIds[3] = 1100; r.dev.slotIds[5] = 1101; r.connect();
        r.dev.patch = 8; std::fill(r.dev.slotIds.begin(), r.dev.slotIds.end(), 0); r.dev.slotIds[3] = 1102; r.dev.slotIds[7] = 1100;
        r.dev.patchChanged(); r.L.run(400); r.settle();
        CHECK(r.plays(3) && r.plays(7) && r.silent(5) && r.C.hello["patch"] == "8", "patch 8: slot 3 -> 1102, slot 7 -> 1100, slot 5 silent");
    }
    // ---- 8. SAMPLE_READY refresh uses the NEW manifest (a pool sample whose length changed) before mapping
    {
        Rig r("ready"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100; r.connect();
        r.dev.add(1103, 3000, 5);                                      // appeared after the last manifest read
        r.C.assign(2, 1103); r.L.run(600); r.settle();
        CHECK(r.plays(2) && r.played(2).size() == 6000, "assign of a sample the host had not seen yet: manifest re-read, correct sample");
    }
    // ---- 9. manifest crc that is not the data CRC (firmware keeps a version token there): refused, reported, one re-pull per epoch, no loop
    {
        Rig r("lie"); r.dev.add(1100, 2000, 1); r.dev.crcLie[1100] = 0xC0DE0001; r.dev.slotIds[3] = 1100; r.connect();
        CHECK(r.silent(3), "data CRC != manifest crc: the slot is refused (silent)");
        CHECK(r.L.info("refused"), "reported");
        CHECK(r.dev.pulls[1100] <= 2, "no pull loop (%d pulls)", r.dev.pulls[1100]);
        CHECK(r.sync.state(r.C.pool()[0]) == 2, "pool listing says bad (2)");
    }
    // ---- 10. not READY blanks everything (Sync level)
    {
        Rig r("down"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100; r.connect(); CHECK(r.plays(3), "up");
        r.C.portClosed(); r.sync.push(); bool all = true; for (int s = 0; s < 16; s++) if (!r.silent(s)) all = false; CHECK(all, "all 16 slots silent when the link is down");
        CHECK(!r.C.slotsOk() && r.C.usedBy(1100).empty(), "client forgot the slot map");
    }
    // ---- 11. usedBy follows the current patch only
    {
        Rig r("used"); r.dev.add(1100, 2000, 1); r.dev.slotIds[3] = 1100; r.dev.slotIds[9] = 1100; r.connect();
        auto u = r.C.usedBy(1100); CHECK(u.size() == 2 && u[0] == 3 && u[1] == 9, "usedBy(1100) = slots 3,9");
        CHECK(r.C.usedBy(1101).empty(), "unused id -> none");
    }
    printf("test_slotsync: %d passed, %d failed\n", pass, fail); return fail ? 1 : 0;
}
