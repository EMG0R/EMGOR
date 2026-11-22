// test_compat.cpp -- the host client is reverse-compatible by nature (EMGOR): params are matched BY ID, unknown ids from PMOR are mirrored and
// forwarded, ids PMOR lacks are skipped on write and reported, unknown HELLO/STATUS fields are ignored, missing fields default, and a schema /
// firmware / proto difference NEVER refuses the connection. Scripted fake devices: older firmware, newer firmware, very old (no GET_SCHEMA/PARAMS).
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <functional>
#include <set>
#include "../max/pmor_host.h"
#include "proto_util.h"
static int passes = 0, fails = 0;
#define EXPECT(c, ...) do { if (c) passes++; else { fails++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Mock {
    std::string toHost; std::vector<std::string> cmds;
    std::function<std::vector<std::string>(const std::string&, const std::string&)> reply;       // (cmd, args) -> body lines (the tag is added)
    static std::string frame(const std::string& tag, const std::string& body) { std::string b = tag + " " + body; char c[8]; snprintf(c, sizeof c, "*%04X\n", (unsigned)pmorCrc16(b.data(), b.size())); return "!" + b + c; }
    void rx(const std::string& f) {
        size_t sp = f.find(' '), st = f.find('*'); std::string tag = f.substr(1, sp - 1), body = f.substr(sp + 1, st - sp - 1);
        size_t a = body.find(' '); std::string cmd = body.substr(0, a), args = a == std::string::npos ? "" : body.substr(a + 1); cmds.push_back(cmd);
        for (auto& l : reply(cmd, args)) toHost += frame(tag, l);
    }
    void push(const std::string& line) { toHost += frame("-", line); }
};
static void run(pmorhost::Client& C, Mock& M, int ms) { for (int t = 0; t < ms; t += 5) { std::string o; o.swap(M.toHost); if (!o.empty()) C.feed(o.data(), o.size()); C.tick(5); } }
static bool until(pmorhost::Client& C, Mock& M, std::function<bool()> p, int ms) { for (int t = 0; t < ms; t += 5) { if (p()) return true; run(C, M, 5); } return p(); }
static int count(const Mock& M, const char* c) { int n = 0; for (auto& x : M.cmds) if (x == c) n++; return n; }

int main() {
    // ---------- older firmware: no proto/sr fields, 4-column schema (no def/flags/group), subset of ids, no SEQ/MANIFEST/FILE/JOIN support
    {
        printf("[older firmware]\n");
        Mock M; pmorhost::Client C; std::vector<std::string> infos; std::map<int, float> seen; int schemaCb = 0;
        C.onInfo = [&](const std::string& s) { infos.push_back(s); }; C.onParam = [&](uint16_t id, float v) { seen[id] = v; }; C.onSchema = [&]() { schemaCb++; };
        C.send = [&](const std::string& s) { M.rx(s); };
        M.reply = [](const std::string& c, const std::string&) -> std::vector<std::string> {
            if (c == "HELLO") return {"OK HELLO fw=old1 mode=STUDIO schema=DEADBEEF patch=1"};
            if (c == "GET_SCHEMA") return {"SCHEMA 1024 CHARACTR 0 1", "SCHEMA 1025 RATIO 0.5 4", "SCHEMA 2053 SOUND 0 15", "OK GET_SCHEMA n=3 hash=DEADBEEF"};
            if (c == "GET_PARAMS") return {"PARAMS 1024=0.5 1025=2 2053=3", "OK GET_PARAMS n=3"};
            if (c == "STATUS") return {"OK STATUS running=0 bpm=120"};
            if (c == "SET_PARAM") return {"OK SET_PARAM id=1024 val=0.7"};
            if (c == "PING") return {"OK PING up=1"};
            return {"ERR " + c + " unknown"};                      // GET_SEQ GET_MANIFEST PULL_FILE JOIN ...
        };
        C.connect("STUDIO");
        EXPECT(until(C, M, [&] { return C.state() == pmorhost::Client::READY; }, 5000), "READY against older firmware (state %s)", C.stateName());
        EXPECT(C.schema.size() == 3 && C.schema[1025].min == 0.5f && C.schema[1025].flags == 0 && C.schema[1025].group.empty(), "4-column schema: missing columns defaulted");
        EXPECT(C.params.size() == 3 && seen[2053] == 3.f && schemaCb == 1, "params by id (%zu), schema callback once", C.params.size());
        EXPECT(C.hello["proto"].empty() && C.hello["patch"] == "1", "missing HELLO fields are just absent");
        bool hashNote = false; for (auto& s : infos) if (s.find("schema hash differs") != std::string::npos) hashNote = true;
        EXPECT(hashNote, "hash mismatch is a note, not a refusal");
        int before = count(M, "SET_PARAM"); C.setParam(1024, 0.7f); C.setParam(3999, 1.f); run(C, M, 100);
        EXPECT(count(M, "SET_PARAM") == before + 1, "SET_PARAM of an id PMOR lacks is skipped locally (no traffic)");
        EXPECT(C.hasParam(1024) && !C.hasParam(3999), "hasParam by id");
        int pres = 0; C.onPresets = [&]() { pres++; }; C.pullPresets(); until(C, M, [&] { return pres == 1; }, 3000);
        EXPECT(pres == 1 && C.presets.empty(), "no preset files on old firmware: empty list, no failure");
        std::vector<std::string> js; C.onJoin = [&](bool ok, int, const std::string& c) { js.push_back(ok ? "ok" : c); }; C.join(4); run(C, M, 100);
        EXPECT(js.size() == 1 && js[0] == "unknown", "JOIN on firmware without it -> unknown, still connected (%s, state %s)", js.empty() ? "-" : js[0].c_str(), C.stateName());
        EXPECT(C.state() == pmorhost::Client::READY, "still READY");
    }
    // ---------- newer firmware: proto=2, unknown HELLO/STATUS fields, extra schema columns and ids, unknown pushes and events
    {
        printf("[newer firmware]\n");
        Mock M; pmorhost::Client C; std::vector<std::string> infos; std::map<int, float> seen; std::map<std::string, std::string> st;
        C.onInfo = [&](const std::string& s) { infos.push_back(s); }; C.onParam = [&](uint16_t id, float v) { seen[id] = v; }; C.onStatus = [&](const std::map<std::string, std::string>& m) { st = m; };
        C.send = [&](const std::string& s) { M.rx(s); };
        M.reply = [](const std::string& c, const std::string&) -> std::vector<std::string> {
            if (c == "HELLO") return {"OK HELLO fw=future9 proto=2 mode=STUDIO schema=12345678 patch=2 sr=96000 block=32 shiny=1 cm=0/0 weird=a=b=c"};
            if (c == "GET_SCHEMA") return {"SCHEMA 1024 CHARACTR 0 1 0.5 4 SYN.A0 X1 X2", "SCHEMA 1025 RATIO 0.5 4 2 0 SYN.A0", "SCHEMA 7777 NEWTHING -1 1 0 16 FUTURE.GRP extra", "SCHEMA 9001 OTHER 0 10 5 0 FUTURE", "OK GET_SCHEMA n=4 hash=12345678"};
            if (c == "GET_PARAMS") return {"PARAMS 1024=0.5 1025=2 7777=0.25 9001=5 mystery=junk", "OK GET_PARAMS n=4"};
            if (c == "GET_SEQ") return {"SEQ 0 1 16 0 0 0000", "OK GET_SEQ"};
            if (c == "GET_MANIFEST") return {"M 5000 100 48000 1 CAFE0001 S5000 extra tokens here", "OK GET_MANIFEST n=1"};
            if (c == "STATUS") return {"OK STATUS cm=2/2 rx=5/0/0/0 clk=follow running=1 bpm=121 newstat=77 store=2/0/0/0/0/0/0/9 pool=1"};
            if (c == "PING") return {"OK PING up=1"};
            return {"OK " + c};
        };
        C.connect("STUDIO");
        EXPECT(until(C, M, [&] { return C.state() == pmorhost::Client::READY; }, 5000), "READY against newer firmware (state %s)", C.stateName());
        EXPECT(C.schema.size() == 4 && C.schema.count(7777) && C.schema[1024].group == "SYN.A0", "extra columns ignored, unknown ids kept in the schema");
        EXPECT(seen.count(7777) && seen[7777] == 0.25f && seen.count(9001), "unknown ids from PMOR are mirrored and forwarded (devices ignore them)");
        EXPECT(C.hello["fw"] == "future9" && C.hello["proto"] == "2", "newer proto number accepted");
        M.push("P 7777 0.75"); M.push("P 1024 0.9"); M.push("NEWEVENT foo bar"); M.push("PATCH_CHANGED 2"); run(C, M, 400);
        EXPECT(seen[7777] == 0.75f && seen[1024] == 0.9f, "pushes for unknown and known ids both arrive");
        C.pollStatus(); run(C, M, 100);
        EXPECT(st["policy"] == "FOLLOW" && st["bus"] == "0" && st["bpm"] == "121", "STATUS with unknown fields: known ones parsed (policy %s)", st["policy"].c_str());
        EXPECT(C.state() == pmorhost::Client::READY && C.badFrames == 0, "unknown events did not disturb the link");
        bool unk = false; for (auto& s : infos) if (s.find("NEWEVENT") != std::string::npos) unk = true; EXPECT(unk, "unknown device event reported as info only");
    }
    // ---------- very old firmware: no GET_SCHEMA, no GET_PARAMS: connection still comes up (empty schema = nothing is blocked)
    {
        printf("[very old firmware]\n");
        Mock M; pmorhost::Client C; std::vector<std::string> infos; C.onInfo = [&](const std::string& s) { infos.push_back(s); }; C.send = [&](const std::string& s) { M.rx(s); };
        M.reply = [](const std::string& c, const std::string&) -> std::vector<std::string> {
            if (c == "HELLO") return {"OK HELLO fw=ancient mode=STUDIO"};
            if (c == "SET_PARAM") return {"OK SET_PARAM id=1 val=1"};
            if (c == "PING") return {"OK PING up=1"};
            return {"ERR " + c + " unknown"};
        };
        C.connect("STUDIO");
        EXPECT(until(C, M, [&] { return C.state() == pmorhost::Client::READY; }, 5000), "READY even without GET_SCHEMA / GET_PARAMS (state %s)", C.stateName());
        EXPECT(C.schema.empty() && C.hasParam(1024), "empty schema blocks nothing");
        int b = count(M, "SET_PARAM"); C.setParam(1024, 1.f); run(C, M, 100); EXPECT(count(M, "SET_PARAM") == b + 1, "SET_PARAM goes out");
        run(C, M, 4000); EXPECT(C.state() == pmorhost::Client::READY, "stays connected (PING liveness)");
    }
    printf("\ntest_compat: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
