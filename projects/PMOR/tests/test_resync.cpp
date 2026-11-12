// test_resync.cpp -- the SET is the source of truth for the patch it is in (max/pmor_host.h "Host snapshot") against the in-process fake PMOR (tests/fakepmor.h).
// Unplug / edit in Live / replug -> PMOR matches Live; PMOR on another patch -> nothing written + adopt / write; first connect -> adopt; load never pushes; no echo of PMOR edits.
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "fakepmor.h"

static int g_fail = 0, g_pass = 0;
#define EXPECT(c, ...) do { if (c) g_pass++; else { g_fail++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Rig {
    FakePmor dev; pmorhost::Client C; FakeLink L; std::vector<std::string> kinds; int a = 0, b = 0, c = 0, d = 0, e = 0; int snapChanges = 0;
    Rig() : L(dev, C) {
        C.onResync = [this](const std::string& k, int a_, int b_, int c_, int d_, int e_) { kinds.push_back(k); a = a_; b = b_; c = c_; d = d_; e = e_; };
        C.onSnapshot = [this]() { snapChanges++; };
        dev.patch = 3;
        dev.prm = {{1024, 0.25f}, {1025, 0.5f}, {1281, 0.8f}, {2048, 10.f}, {2304, 0.6f}, {3072, 0.f}, {4096, 1.f}, {5120, 0.f}, {6144, 124.f}};   // 5120 = FX engage, 6144 = global: never part of the snapshot
        dev.add(1100, 400, 1); dev.add(1101, 400, 2); dev.add(1102, 400, 3);
        dev.slotIds[0] = 1100; dev.slotIds[1] = 1101;
    }
    void connect(const char* mode = "STUDIO") { kinds.clear(); C.connect(mode); }
    bool settle(int ms = 4000) { return L.until([&] { return C.state() == pmorhost::Client::READY && !C.snapResyncing() && (!kinds.empty()); }, ms); }
    bool last(const char* k) const { return !kinds.empty() && kinds.back() == k; }
    int count(const char* cmd) const { int n = 0; for (auto& s : dev.cmds) if (s.rfind(cmd, 0) == 0) n++; return n; }
};

int main() {
    // ---- 1. first connect with an empty snapshot = adopt PMOR (nothing written)
    {
        Rig r; r.connect(); EXPECT(r.settle(), "no sync");
        EXPECT(r.last("adopted"), "first connect should adopt, got %s", r.kinds.empty() ? "-" : r.kinds.back().c_str());
        EXPECT(r.C.snap.valid && r.C.snap.patch == 3, "snapshot tagged with PMOR patch 3, got %d", r.C.snap.patch);
        EXPECT(r.C.snap.params.size() == 7, "snapshot holds the 7 patch params (not FX / global), got %zu", r.C.snap.params.size());
        EXPECT(!r.C.snap.params.count(5120) && !r.C.snap.params.count(6144), "FX engage / global params are not part of the snapshot");
        EXPECT(r.count("SET_PARAM") == 0, "adopt writes nothing to PMOR (%d SET_PARAM)", r.count("SET_PARAM"));
        EXPECT(r.C.snap.slotIds.size() == 16 && r.C.snap.slotIds[0] == 1100 && r.C.snap.slotIds[1] == 1101, "sampler slot ids adopted");
        EXPECT(r.C.snapTracking(), "tracking after adopt");
    }
    // ---- 2. unplug -> edit in Live (and on PMOR) -> replug: PMOR matches Live for the patch
    {
        Rig r; r.connect(); r.settle();
        r.C.setParam(1024, 0.75f);                                              // Live edit while connected: sent AND kept
        r.L.run(100);
        EXPECT(r.dev.prm[1024] == 0.75f && r.C.snap.params[1024] == 0.75f, "connected edit reaches PMOR and the snapshot");
        r.C.portClosed(); r.dev.mode = "NONE"; r.L.run(50);                       // unplug
        size_t sentBefore = r.dev.cmds.size();
        r.C.setParam(1025, 0.9f); r.C.setParam(2304, 0.1f); r.C.setParam(4096, 0.f); r.C.setParam(6144, 90.f);   // producer edits in Live while unplugged (6144 is global: not recorded)
        r.L.run(50);
        EXPECT(r.dev.cmds.size() == sentBefore, "nothing is sent while unplugged");
        EXPECT(r.C.snap.params[1025] == 0.9f && r.C.snap.params[2304] == 0.1f && r.C.snap.params[4096] == 0.f, "unplugged edits land in the snapshot");
        r.dev.prm[1281] = 0.2f; r.dev.prm[1025] = 0.33f; r.dev.prm[5120] = 1.f;      // someone edits on PMOR meanwhile (+ an FX engage that must survive)
        r.dev.cmds.clear(); r.dev.setCmds.clear();
        r.connect(); EXPECT(r.settle(), "replug sync");
        EXPECT(r.last("synced"), "replug with the same patch syncs, got %s", r.kinds.empty() ? "-" : r.kinds.back().c_str());
        EXPECT(r.dev.prm[1025] == 0.9f && r.dev.prm[2304] == 0.1f && r.dev.prm[4096] == 0.f && r.dev.prm[1281] == 0.8f && r.dev.prm[1024] == 0.75f, "PMOR matches Live (edits made on PMOR while unplugged are overwritten)");
        EXPECT(r.dev.prm[5120] == 1.f && r.dev.prm[6144] == 124.f, "FX engage / global params untouched");
        std::vector<int> sc = r.dev.setCmds; std::sort(sc.begin(), sc.end());
        EXPECT((sc == std::vector<int>{1025, 1281, 2304, 4096}), "only the params that differ are pushed (%zu sent)", sc.size());
        EXPECT(r.a == 4 && r.b == 0, "synced 4 params, 0 mismatch (a=%d b=%d)", r.a, r.b);
        // the re-pull confirm: a GET_PARAMS after the last SET_PARAM
        int lastSet = -1, lastGet = -1; for (size_t i = 0; i < r.dev.cmds.size(); i++) { if (r.dev.cmds[i].rfind("SET_PARAM", 0) == 0) lastSet = (int)i; if (r.dev.cmds[i].rfind("GET_PARAMS", 0) == 0) lastGet = (int)i; }
        EXPECT(lastGet > lastSet && lastSet >= 0, "confirm re-pull after the pushes (set %d get %d)", lastSet, lastGet);
        EXPECT(r.C.snapTracking(), "tracking again after the sync");
    }
    // ---- 3. rate limit: 300 differing params never flood (<= 4 in flight, ~resyncPerSec)
    {
        Rig r; for (int i = 0; i < 300; i++) r.dev.prm[1024 + 256 + 2 + (i % 200) + (i / 200) * 0] = 0.f;   // 200 more params in domain 1
        r.connect(); r.settle();
        r.C.portClosed(); r.dev.mode = "NONE";
        for (auto& kv : std::vector<std::pair<int, float>>{}) (void)kv;
        for (auto& kv : r.C.snap.params) r.C.setParam(kv.first, 0.5f);
        size_t n = r.C.snap.params.size();
        r.dev.cmds.clear(); r.dev.setCmds.clear(); r.connect();
        int t = 0; size_t maxIn5 = 0, prev = 0; bool ok = r.L.until([&] { t += 5; if (t % 100 == 0) { size_t cur = r.dev.setCmds.size(); maxIn5 = std::max(maxIn5, cur - prev); prev = cur; } return r.last("synced"); }, 12000);
        EXPECT(ok, "big resync finished"); EXPECT(r.dev.setCmds.size() >= n - 2, "pushed ~%zu params, got %zu", n, r.dev.setCmds.size());
        EXPECT(maxIn5 <= 10, "never more than ~8 SET_PARAM per 100 ms (saw %zu)", maxIn5);
    }
    // ---- 4. a Live set load alone never writes: snapshot loaded, no connection -> zero frames; before READY zero SET_PARAM
    {
        Rig r; r.connect(); r.settle(); std::string txt = r.C.snapshotText(); EXPECT(txt.rfind("PS1~3~", 0) == 0 && txt.find(' ') == std::string::npos && txt.find(',') == std::string::npos && txt.find(';') == std::string::npos, "text form is one clean symbol: %.40s", txt.c_str());
        Rig q; q.dev.prm[1025] = 0.1f;                                         // fresh device instance (a set just loaded), PMOR holds other values
        bool ch = q.C.snapshotLoad(txt); EXPECT(ch, "snapshot loads"); q.L.run(300);
        EXPECT(q.dev.cmds.empty(), "load alone sends nothing (%zu cmds)", q.dev.cmds.size());
        q.connect(); size_t firstSet = 1u << 30, getPatch = 1u << 30;
        q.L.until([&] { for (size_t i = 0; i < q.dev.cmds.size(); i++) { if (q.dev.cmds[i].rfind("GET_PATCH", 0) == 0 && getPatch > i) getPatch = i; if (q.dev.cmds[i].rfind("SET_PARAM", 0) == 0 && firstSet > i) firstSet = i; } return q.last("synced"); }, 4000);
        EXPECT(q.dev.cmds.size() > 3 && getPatch < firstSet, "no SET_PARAM before the handshake finished (GET_PATCH at %zu, first SET_PARAM at %zu)", getPatch, firstSet);
        EXPECT(q.dev.prm[1025] == 0.5f, "after the handshake PMOR matches the set (1025 = %g)", q.dev.prm[1025]);
        EXPECT(!q.C.snapshotLoad(txt) , "re-loading the identical text is a no-op (no pattr echo loop)");
        EXPECT(!q.C.snapshotLoad("garbage") && !q.C.snapshotLoad("PS1~3~"), "malformed text ignored");
    }
    // ---- 5. PMOR on a different patch: nothing written, then adopt X
    {
        Rig r; r.connect(); r.settle(); std::string txt = r.C.snapshotText();
        Rig q; q.dev.patch = 5; q.dev.prm[1024] = 0.99f; q.C.snapshotLoad(txt); q.connect(); EXPECT(q.settle(), "sync");
        EXPECT(q.last("patchdiff") && q.a == 5 && q.b == 3, "patchdiff pmor 5 vs set 3 (a=%d b=%d)", q.a, q.b);
        EXPECT(q.count("SET_PARAM") == 0 && q.C.snapPending() && !q.C.snapTracking(), "no write while the choice is open");
        q.C.setParam(1025, 0.12f); q.L.run(60);
        EXPECT(q.C.snap.patch == 3 && q.C.snap.params[1025] == 0.5f, "edits do not touch the frozen snapshot");
        q.C.snapAdopt(); q.L.run(100);
        EXPECT(q.last("adopted") && q.C.snap.patch == 5 && q.C.snap.params[1024] == 0.99f && q.C.snapTracking() && !q.C.snapPending(), "adopt X: snapshot is PMOR's patch 5");
        EXPECT(q.dev.prm[1025] == 0.12f, "the pass-through edit stayed on PMOR");
    }
    // ---- 5b. ... or write Y onto PMOR's patch
    {
        Rig r; r.connect(); r.settle(); std::string txt = r.C.snapshotText();
        Rig q; q.dev.patch = 5; q.dev.prm[1024] = 0.99f; q.dev.prm[1281] = 0.01f; q.C.snapshotLoad(txt); q.connect(); q.settle();
        q.C.snapWrite(); EXPECT(q.L.until([&] { return q.last("synced"); }, 4000), "write finished");
        EXPECT(q.dev.prm[1024] == 0.25f && q.dev.prm[1281] == 0.8f && q.C.snap.patch == 5, "the set's values are on PMOR now; snapshot re-tagged to patch 5");
    }
    // ---- 5c. choice open, user loads the set's patch on PMOR itself -> PATCH_CHANGED resyncs
    {
        Rig r; r.connect(); r.settle(); std::string txt = r.C.snapshotText();
        Rig q; q.dev.patch = 5; q.dev.prm[1024] = 0.99f; q.C.snapshotLoad(txt); q.connect(); q.settle();
        q.dev.patch = 3; q.dev.prm[1024] = 0.11f; q.dev.patchChanged();
        EXPECT(q.L.until([&] { return q.last("synced"); }, 4000), "resync on the user's own patch load");
        EXPECT(q.dev.prm[1024] == 0.25f, "PMOR patch 3 matches the set after PMOR switched to it");
        q.kinds.clear(); q.dev.patch = 8; q.dev.prm[1024] = 0.7f; q.dev.patchChanged();                      // tracking: PMOR moves on -> the set follows PMOR (adopt)
        EXPECT(q.L.until([&] { return q.last("adopted"); }, 4000) && q.C.snap.patch == 8 && q.C.snap.params[1024] == 0.7f, "a patch switch on PMOR while connected re-targets the snapshot");
    }
    // ---- 6. no echo: PMOR-originated edits update the snapshot and are never sent back
    {
        Rig r; r.connect(); r.settle(); r.dev.cmds.clear(); r.dev.setCmds.clear();
        r.dev.pmorEdit(1025, 0.42f); r.dev.pmorEdit(5120, 1.f); r.L.run(200);
        EXPECT(r.C.snap.params[1025] == 0.42f && !r.C.snap.params.count(5120), "PMOR edit updates the snapshot (FX excluded)");
        EXPECT(r.dev.setCmds.empty(), "nothing is echoed back to PMOR");
    }
    // ---- 7. clamp: PMOR clamps a pushed value -> the re-pull adopts PMOR's value and reports it
    {
        Rig r; r.connect(); r.settle(); r.C.portClosed(); r.dev.mode = "NONE"; r.C.setParam(1024, 5.f);
        r.dev.clampMax[1024] = 1.f; r.connect(); EXPECT(r.settle(), "sync");
        EXPECT(r.last("synced") && r.b >= 0 && r.C.snap.params[1024] == 1.f && r.dev.prm[1024] == 1.f, "clamped value adopted (snap %g)", r.C.snap.params[1024]);
    }
    // ---- 8. busy: SET_PARAM `ERR busy` is retried, not lost
    {
        Rig r; r.connect(); r.settle(); r.C.portClosed(); r.dev.mode = "NONE"; r.C.setParam(1024, 0.6f); r.C.setParam(1025, 0.7f);
        r.dev.busySet = 3; r.connect(); EXPECT(r.settle(6000), "sync");
        EXPECT(r.dev.prm[1024] == 0.6f && r.dev.prm[1025] == 0.7f, "busy retried: values arrived (%g %g)", r.dev.prm[1024], r.dev.prm[1025]);
    }
    // ---- 9. sampler slots follow the same rule (EDIT assign, mode restored); a vanished sample is skipped
    {
        Rig r; r.connect(); r.settle();
        r.C.assign(2, 1102); r.L.run(300);                                      // Live assigns pool sample 1102 to slot 2 while connected -> snapshot follows
        EXPECT(r.C.snap.slotIds.size() > 2 && r.C.snap.slotIds[2] == 1102, "assign while connected updates the snapshot");
        r.C.portClosed(); r.dev.mode = "NONE"; r.dev.slotIds[0] = 1101; r.dev.slotIds[2] = 0;      // PMOR changed its slots while unplugged
        r.connect(); EXPECT(r.settle(6000), "sync");
        EXPECT(r.L.until([&] { return r.last("synced") && !r.C.snapResyncing(); }, 6000), "slots resync finished");
        EXPECT(r.dev.slotIds[0] == 1100 && r.dev.slotIds[2] == 1102, "slots match the set: %d %d", r.dev.slotIds[0], r.dev.slotIds[2]);
        EXPECT(r.c == 2 && r.dev.mode == "STUDIO", "2 slots assigned, STUDIO restored (c=%d mode=%s)", r.c, r.dev.mode.c_str());
        // vanished sample
        r.C.portClosed(); r.dev.mode = "NONE"; r.dev.pool.erase(1102); r.dev.slotIds[2] = 0; r.connect(); r.settle(6000);
        EXPECT(r.L.until([&] { return r.last("synced"); }, 6000) && r.e == 1 && r.dev.slotIds[2] == 0, "missing sample skipped, not invented (e=%d)", r.e);
    }
    printf("test_resync: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
