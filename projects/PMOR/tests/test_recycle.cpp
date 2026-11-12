// test_recycle.cpp -- host side of the ASSUMED recycle-bin protocol (STUB block in max/pmor_host.h) against fakepmor.h (the real firmware has no recycle commands yet).
// delete moves to the bin (EDIT for the call, previous mode back), the bin lists with boots_left, restore returns the sample, empty only on an explicit call,
// firmware without the commands = "no recycle bin" and a strict no-op, usedBy() for the UI warning.
#include <stdio.h>
#include "fakepmor.h"

static int pass = 0, fail = 0;
#define CHECK(c, ...) do { if (c) pass++; else { fail++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int countCmd(const FakePmor& d, const std::string& pfx) { int n = 0; for (auto& c : d.cmds) if (c.rfind(pfx, 0) == 0) n++; return n; }

int main() {
    FakePmor dev; pmorhost::Client C; FakeLink L(dev, C);
    dev.add(1100, 2000, 1); dev.add(1101, 2200, 2); dev.add(1102, 2400, 3); dev.slotIds[3] = 1100; dev.slotIds[9] = 1100; dev.slotIds[5] = 1101;
    std::vector<std::pair<int, std::string>> deleted, restored; std::vector<pmorhost::Client::BinEntry> bl; int nobin = 0, pools = 0; std::string emptied;
    C.onDeleted = [&](int id, bool ok, const std::string& c) { deleted.push_back({id, ok ? "ok" : c}); };
    C.onBinRestored = [&](int id, bool ok, const std::string& c) { restored.push_back({id, ok ? "ok" : c}); };
    C.onBinList = [&](const std::vector<pmorhost::Client::BinEntry>& v) { bl = v; };
    C.onBinEmptied = [&](bool ok, const std::string& n) { emptied = (ok ? "ok " : "err ") + n; };
    C.onNoBin = [&]() { nobin++; };
    C.onPool = [&]() { pools++; };
    C.connect("STUDIO"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 8000); L.run(100);
    CHECK(C.state() == pmorhost::Client::READY && C.pool().size() == 3, "connected, 3 pool samples");

    // usedBy: which current-patch slots use an id (the UI warns before a delete)
    { auto u = C.usedBy(1100); CHECK(u.size() == 2 && u[0] == 3 && u[1] == 9, "usedBy(1100) = slots 3 and 9"); CHECK(C.usedBy(1102).empty(), "unused id -> no slots"); }

    // delete moves to the bin; EDIT for the call, STUDIO afterwards; manifest + slot map refreshed
    pools = 0; C.deleteSamples({1102, 1101});
    L.until([&] { return deleted.size() == 2 && C.hello["mode"] == "STUDIO" && pools >= 1; }, 3000); L.run(200);
    CHECK(deleted.size() == 2 && deleted[0].first == 1102 && deleted[0].second == "ok" && deleted[1].first == 1101 && deleted[1].second == "ok", "both ids deleted, in order");
    CHECK(dev.bin.count(1102) && dev.bin.count(1101) && !dev.pool.count(1102) && !dev.pool.count(1101), "moved to the bin, not hard-deleted (bin %zu, pool %zu)", dev.bin.size(), dev.pool.size());
    CHECK(dev.mode == "STUDIO" && C.hello["mode"] == "STUDIO", "back in STUDIO (device %s)", dev.mode.c_str());
    CHECK(C.pool().size() == 1 && C.usedBy(1101).empty(), "host manifest and slot map refreshed (pool %zu)", C.pool().size());
    CHECK(countCmd(dev, "BIN_EMPTY") == 0, "delete never emptied the bin");

    // bin listing: age + boots_left
    C.binList(); L.until([&] { return bl.size() == 2; }, 2000);
    CHECK(bl.size() == 2 && bl[0].id == 1101 && bl[0].len == 2200 && bl[0].name == "S1101" && bl[0].age == "0" && bl[0].boots == 5, "bin lists id/len/name/age/boots_left (%zu entries)", bl.size());
    dev.bin[1102].boots = 2; dev.bin[1102].age = "3600"; C.binList(); L.until([&] { return bl.size() == 2 && bl[1].boots == 2; }, 2000);
    CHECK(bl.size() == 2 && bl[1].id == 1102 && bl[1].boots == 2 && bl[1].age == "3600", "boots_left / age follow the device");

    // restore returns it
    pools = 0; C.binRestore(1101); L.until([&] { return !restored.empty() && C.pool().size() == 2 && C.hello["mode"] == "STUDIO"; }, 3000); L.run(100);
    CHECK(restored.size() == 1 && restored[0].first == 1101 && restored[0].second == "ok" && dev.pool.count(1101) && !dev.bin.count(1101), "restored to the pool");
    CHECK(C.pool().size() == 2 && dev.mode == "STUDIO", "manifest refreshed, STUDIO again");
    restored.clear(); C.binRestore(4242); L.until([&] { return !restored.empty(); }, 2000); L.run(100);
    CHECK(restored.size() == 1 && restored[0].second == "range" && dev.bin.size() == 1, "restore of an unknown id reports range, bin untouched");

    // empty only with an explicit call
    C.binList(); L.run(300); C.getPatch(); L.run(300); C.refreshPool(); L.run(300);
    CHECK(dev.bin.size() == 1 && countCmd(dev, "BIN_EMPTY") == 0, "nothing but an explicit binEmpty() empties the bin");
    C.binEmpty(); L.until([&] { return !emptied.empty(); }, 2000); L.run(100);
    CHECK(emptied == "ok 1" && dev.bin.empty() && countCmd(dev, "BIN_EMPTY YES") == 1 && dev.mode == "STUDIO", "binEmpty sends BIN_EMPTY YES once, bin empty (%s)", emptied.c_str());

    // an unknown pool id is a normal per-id error, the rest of the batch still goes
    deleted.clear(); C.deleteSamples({9999, 1100}); L.until([&] { return deleted.size() == 2; }, 3000); L.run(200);
    CHECK(deleted.size() == 2 && deleted[0].second == "range" && deleted[1].second == "ok" && dev.bin.count(1100), "unknown id -> range, next id still deleted");
    CHECK(C.usedBy(1100).empty() && dev.slotIds[3] == 0, "deleted sample no longer mapped to slots 3/9 after the refresh");

    // delete while not connected / empty list: no traffic
    { size_t n = dev.cmds.size(); C.deleteSamples({}); CHECK(dev.cmds.size() == n, "empty delete list sends nothing"); }

    // firmware WITHOUT the commands: ERR <CMD> unknown -> info, onNoBin, a strict no-op
    {
        FakePmor d2; pmorhost::Client C2; FakeLink L2(d2, C2); d2.hasBin = false; d2.add(1100, 2000, 1); d2.add(1101, 2200, 2); d2.slotIds[3] = 1100;
        int nb = 0; std::vector<int> del2; C2.onNoBin = [&]() { nb++; }; C2.onDeleted = [&](int id, bool, const std::string&) { del2.push_back(id); };
        C2.connect("STUDIO"); L2.until([&] { return C2.state() == pmorhost::Client::READY; }, 8000); L2.run(100);
        size_t pool0 = d2.pool.size(); C2.deleteSamples({1100, 1101}); L2.until([&] { return nb >= 1 && C2.hello["mode"] == "STUDIO"; }, 3000); L2.run(300);
        CHECK(nb == 1 && L2.info("firmware has no recycle bin yet"), "onNoBin + 'firmware has no recycle bin yet'");
        CHECK(d2.pool.size() == pool0 && d2.bin.empty() && del2.empty(), "nothing deleted");
        CHECK(countCmd(d2, "DELETE_POOL") == 1, "only the first DELETE_POOL was tried, the rest of the batch was not (%d)", countCmd(d2, "DELETE_POOL"));
        CHECK(d2.mode == "STUDIO" && C2.binUnsupported(), "mode restored, client remembers (this connection)");
        size_t n = d2.cmds.size(); C2.deleteSamples({1100}); C2.binList(); C2.binRestore(1100); C2.binEmpty(); L2.run(200);
        CHECK(d2.cmds.size() == n && nb == 5, "later delete/bin calls send nothing and answer nobin again (%d)", nb);
        CHECK(countCmd(d2, "BIN_EMPTY") == 0, "never sent BIN_EMPTY");
    }
    { FakePmor d3; pmorhost::Client C3; FakeLink L3(d3, C3); d3.hasBin = false; int nb = 0; C3.onNoBin = [&]() { nb++; };
      C3.connect("EDIT"); L3.until([&] { return C3.state() == pmorhost::Client::READY; }, 8000); C3.binList(); L3.run(300);
      CHECK(nb == 1 && L3.info("firmware has no recycle bin yet"), "BIN_LIST on old firmware -> nobin"); }
    { pmorhost::Client C4; std::string out; C4.send = [&](const std::string& s) { out += s; }; C4.deleteSamples({1}); C4.binEmpty(); CHECK(out.empty(), "disconnected: no traffic"); }
    // duplicate-CRC pool entries (migration without dedup): identical manifest crc + len, distinct ids; usedBy tells which copy the current patch references
    {
        FakePmor d; pmorhost::Client Cd; FakeLink Ld(d, Cd);
        d.add(1300, 3000, 7); d.addDup(1301, 1300); d.addDup(1302, 1300); d.add(1303, 3000, 8); d.slotIds[2] = 1301;
        Cd.connect("STUDIO"); Ld.until([&] { return Cd.state() == pmorhost::Client::READY; }, 8000); Ld.run(100);
        auto v = Cd.pool(); std::map<int, pmorhost::Client::Sample> m; for (auto& sm : v) m[sm.id] = sm;
        CHECK(v.size() == 4 && m[1300].crc != 0 && m[1300].crc == m[1301].crc && m[1301].crc == m[1302].crc && m[1300].len == m[1302].len, "1300/1301/1302 share crc + len");
        CHECK(m[1303].crc != m[1300].crc, "different audio, different crc");
        CHECK(Cd.usedBy(1301).size() == 1 && Cd.usedBy(1301)[0] == 2 && Cd.usedBy(1300).empty() && Cd.usedBy(1302).empty(), "only 1301 is referenced by the current patch");
        std::vector<std::pair<int, std::string>> del; Cd.onDeleted = [&](int id, bool ok, const std::string& c) { del.push_back({id, ok ? "ok" : c}); };
        Cd.deleteSamples({1300, 1302}); Ld.until([&] { return del.size() == 2; }, 3000); Ld.run(200);
        CHECK(d.pool.count(1301) && d.slotIds[2] == 1301 && !d.pool.count(1300) && !d.pool.count(1302) && d.bin.count(1300) && d.bin.count(1302), "deleting the unreferenced duplicates keeps the referenced copy (in the pool, still in slot 2)");
        CHECK(Cd.pool().size() == 2 && Cd.usedBy(1301).size() == 1, "host view after: 1301 + 1303, slot map intact");
    }
    printf("test_recycle: %d passed, %d failed\n", pass, fail); return fail ? 1 : 0;
}
