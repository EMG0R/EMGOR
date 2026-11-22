// fakepmor.h -- a tiny in-process fake PMOR for host-side tests that need device states the real firmware simulator (test_hostclient's sim) cannot produce:
// controllable pool / slot ids / manifest crc, legacy (ver=0) manifests, ASSIGN + SAMPLE_READY, patch changes, and the ASSUMED recycle-bin commands
// (DELETE_POOL / BIN_LIST / BIN_RESTORE / BIN_EMPTY, see the STUB block in max/pmor_host.h; the real firmware does not have them yet).
// Speaks docs/PROTOCOL.md framing only as far as pmor_host.h needs it. Header-only; link pmor_core (CRC / base64).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "proto_util.h"
#include "../max/pmor_host.h"

struct FakePmor {
    struct Smp { std::vector<int16_t> pcm; uint32_t crc = 0; std::string name; };
    std::string toHost, mode = "NONE";
    int patch = 3;
    bool legacy = false;                 // legacy manifest: ids patch*16+slot, ver 00000000, no ids= in PATCHINFO
    bool hasBin = true;                  // false = firmware without the recycle commands: `ERR <CMD> unknown`
    std::map<int, Smp> pool;             // id -> sample (pool ids >= 1024, or legacy ids)
    std::vector<int> slotIds = std::vector<int>(16, 0);   // current patch: slot -> id (0 empty)
    struct Bin { Smp s; std::string age; int boots; };
    std::map<int, Bin> bin;
    std::vector<std::string> cmds;       // every command body received
    std::map<int, int> pulls;            // id -> number of PULL_SAMPLE offset-0 requests
    int bootLeft = 0;                    // HELLOs that still answer `booting=1` (every other command: `ERR <cmd> booting`)
    bool hasTempo = true; long tempo = 0; std::vector<long> tempos;   // TEMPO support (older firmware: ERR TEMPO unknown)
    // SAVE_PRESET + PULL_FILE R (preset files): hasSave=false -> `ERR SAVE_PRESET unknown` (older firmware); nosave -> `ERR SAVE_PRESET nosave`
    bool hasSave = true, nosave = false; std::vector<std::string> saves;     // every SAVE_PRESET command body as received
    std::map<int, std::map<int, std::string>> userPre;                      // engine -> n -> name (user presets; PRESETS/<E>.TXT is rendered from this)
    std::map<int, uint32_t> crcLie;      // id -> manifest crc to report instead of the real CRC (firmware that keeps a version token there)

    static std::vector<int16_t> makePcm(int len, int seed) { std::vector<int16_t> v(len); for (int i = 0; i < len; i++) v[i] = (int16_t)(((i * 7 + seed * 131) % 2001) * 13 - 13000); return v; }
    static uint32_t crcOf(const std::vector<int16_t>& p) { std::vector<uint8_t> b(p.size() * 2); for (size_t i = 0; i < p.size(); i++) { b[2 * i] = (uint8_t)p[i]; b[2 * i + 1] = (uint8_t)((uint16_t)p[i] >> 8); } return pmorCrc32Of(b.data(), b.size()); }
    static std::vector<uint8_t> bytesOf(const std::vector<int16_t>& p) { std::vector<uint8_t> b(p.size() * 2); for (size_t i = 0; i < p.size(); i++) { b[2 * i] = (uint8_t)p[i]; b[2 * i + 1] = (uint8_t)((uint16_t)p[i] >> 8); } return b; }
    void add(int id, int len, int seed) {
        Smp s; s.pcm = makePcm(len, seed); s.crc = crcOf(s.pcm);
        char n[24]; if (id >= 1024) snprintf(n, sizeof n, "S%d", id); else snprintf(n, sizeof n, "P%02dS%02d", id / 16, id % 16); s.name = n; pool[id] = s;
    }
    // duplicate-CRC entry: pool id `id` with the SAME audio as `srcId` (what the firmware migration produces: one pool id per patch slot, no dedup) -> identical manifest crc + length
    void addDup(int id, int srcId) { Smp s = pool.at(srcId); char n[24]; snprintf(n, sizeof n, "S%d", id); s.name = n; pool[id] = s; }
    std::vector<uint8_t> slotBytes(int slot) const { auto it = pool.find(slotIds[slot]); return it == pool.end() ? std::vector<uint8_t>() : bytesOf(it->second.pcm); }

    void line(const std::string& tag, const std::string& body) {
        std::string b = tag + " " + body; char c[8]; snprintf(c, sizeof c, "*%04X\n", (unsigned)pmorCrc16(b.data(), b.size())); toHost += "!" + b + c;
    }
    void push(const std::string& body) { line("-", body); }
    void patchChanged() { push("PATCH_CHANGED " + std::to_string(patch)); }
    // one frame from the host
    void rx(const std::string& f) {
        if (f.size() < 3 || f[0] != '!') return;
        size_t star = f.rfind('*'); std::string body = f.substr(1, star - 1); size_t sp = body.find(' ');
        std::string tag = body.substr(0, sp), cmd = body.substr(sp + 1); cmds.push_back(cmd);
        std::vector<std::string> t; { size_t i = 0; while (i < cmd.size()) { size_t j = cmd.find(' ', i); if (j == std::string::npos) j = cmd.size(); t.push_back(cmd.substr(i, j - i)); i = j + 1; } }
        const std::string& c = t[0];
        auto ok = [&](const std::string& s) { line(tag, "OK " + c + (s.empty() ? "" : " " + s)); };
        auto err = [&](const std::string& code) { line(tag, "ERR " + c + " " + code); };
        if (c == "HELLO" && bootLeft > 0) { bootLeft--; mode = "NONE"; ok("fw=fake mode=" + (t.size() > 1 ? t[1] : std::string("STUDIO")) + " booting=1"); return; }
        if (bootLeft > 0) { err("booting"); return; }
        if (c == "TEMPO") { if (!hasTempo) { err("unknown"); return; } if (mode != "STUDIO" && mode != "EDIT") { err("mode"); return; } long m = atol(t[1].c_str()); if (m < 40000 || m > 400000) { err("arg"); return; } tempo = m; tempos.push_back(m); ok("bpm=" + std::to_string(m)); return; }
        if (c == "HELLO") { mode = t.size() > 1 ? t[1] : "STUDIO"; ok("fw=fake mode=" + mode + " schema=00000000 patch=" + std::to_string(patch) + " clk=follow"); }
        else if (c == "BYE") { mode = "NONE"; ok(""); }
        else if (c == "PING") ok("");
        else if (c == "STATUS") ok("cm=0/0 clk=follow running=0 bpm=120");
        else if (c == "GET_SCHEMA" || c == "GET_PARAMS" || c == "GET_SEQ") ok("n=0");
        else if (c == "GET_MANIFEST") {
            int n = 0; for (auto& kv : pool) { char b[160]; unsigned crc = crcLie.count(kv.first) ? crcLie[kv.first] : (kv.first >= 1024 ? kv.second.crc : 0u);
                snprintf(b, sizeof b, "M %d %u 48000 1 %08X %s", kv.first, (unsigned)kv.second.pcm.size(), crc, kv.second.name.c_str()); line(tag, b); n++; }
            ok("n=" + std::to_string(n));
        }
        else if (c == "GET_PATCH") {
            std::string lens, ids; for (int s = 0; s < 16; s++) { auto it = pool.find(slotIds[s]); lens += (s ? "," : "") + std::to_string(it == pool.end() ? 0 : it->second.pcm.size()); ids += (s ? "," : "") + std::to_string(slotIds[s]); }
            line(tag, "PATCHINFO slot=" + std::to_string(patch) + " samples=" + lens + (legacy ? "" : " ids=" + ids));
            ok("slot=" + std::to_string(patch));
        }
        else if (c == "PULL_SAMPLE") {
            int id = atoi(t[1].c_str()); uint32_t off = (uint32_t)atoi(t[2].c_str()), mx = (uint32_t)atoi(t[3].c_str()); int cnt = t.size() > 4 ? atoi(t[4].c_str()) : 1;
            auto it = pool.find(id); if (it == pool.end()) { err("range"); return; }
            if (off == 0) pulls[id]++;
            std::vector<uint8_t> d = bytesOf(it->second.pcm); uint32_t total = (uint32_t)d.size(); if (mx > 192) mx = 192;
            uint32_t lastOff = off, lastN = 0;
            for (int k = 0; k < cnt && off < total; k++) {
                uint32_t n = total - off < mx ? total - off : mx; char b64[400]; size_t bl = pmorB64Encode(d.data() + off, n, b64); b64[bl] = 0;
                char b[600]; snprintf(b, sizeof b, "C %d %u %u %08X %s", id, (unsigned)off, (unsigned)n, (unsigned)pmorCrc32Of(d.data() + off, n), b64); line(tag, b);
                lastOff = off; lastN = n; off += n;
            }
            char b[96]; snprintf(b, sizeof b, "id=%d offset=%u n=%u total=%u", id, (unsigned)lastOff, (unsigned)lastN, (unsigned)total); ok(b);
        }
        else if (c == "ASSIGN_SAMPLE") {
            if (mode != "EDIT") { err("mode"); return; }
            int sl = atoi(t[1].c_str()), id = atoi(t[2].c_str()); if (sl < 0 || sl > 15 || !pool.count(id)) { err("range"); return; }
            ok("slot=" + std::to_string(sl) + " id=" + std::to_string(id)); slotIds[sl] = id; push("SAMPLE_READY " + std::to_string(sl));
        }
        else if (c == "SAVE_PRESET") {
            if (!hasSave) { err("unknown"); return; }
            if (mode != "EDIT") { err("mode"); return; }
            if (nosave) { err("nosave"); return; }
            if (t.size() < 4) { err("arg"); return; }
            int e = atoi(t[1].c_str()), n = atoi(t[2].c_str());
            if (e < 0 || e > 11 || n < (e >= 4 ? 5 : 0) || n > 31) { err("range"); return; }
            std::string nm = cmd.substr(cmd.find(' ', cmd.find(' ') + 1) + 1); nm = nm.substr(nm.find(' ') + 1); if (nm.size() > 9) { err("name"); return; }
            userPre[e][n] = nm; saves.push_back(cmd); ok("engine=" + std::to_string(e) + " n=" + std::to_string(n));
        }
        else if (c == "PULL_FILE") {
            if (t.size() < 6 || t[1] != "R") { err("range"); return; }
            int e = atoi(t[2].c_str()); uint32_t off = (uint32_t)atoi(t[3].c_str()), mx = (uint32_t)atoi(t[4].c_str()); int cnt = atoi(t[5].c_str());
            if (e < 0 || e > 11) { err("range"); return; }
            std::string txt = "PMORPRESET 2\nengine " + std::to_string(e) + "\n";
            for (auto& kv : userPre[e]) { std::string nm = kv.second; for (char& ch : nm) if (ch == ' ') ch = '_'; txt += "u " + std::to_string(kv.first) + " " + nm + " 00112233\n"; }
            txt += "end 0\n"; uint32_t total = (uint32_t)txt.size(); if (mx > 192) mx = 192;
            uint32_t lastN = 0;
            for (int k = 0; k < cnt && off < total; k++) {
                uint32_t n = total - off < mx ? total - off : mx; char b64[400]; size_t bl = pmorB64Encode((const uint8_t*)txt.data() + off, n, b64); b64[bl] = 0;
                char b[600]; snprintf(b, sizeof b, "F R %d %u %u %08X %s", e, (unsigned)off, (unsigned)n, (unsigned)pmorCrc32Of((const uint8_t*)txt.data() + off, n), b64); line(tag, b);
                lastN = n; off += n;
            }
            char b[96]; snprintf(b, sizeof b, "total=%u n=%u", (unsigned)total, (unsigned)lastN); ok(b);
        }
        else if (c == "DELETE_POOL" || c == "BIN_LIST" || c == "BIN_RESTORE" || c == "BIN_EMPTY") {
            if (!hasBin) { err("unknown"); return; }
            if (c == "BIN_LIST") { for (auto& kv : bin) { char b[160]; snprintf(b, sizeof b, "B %d %u %s %s %d", kv.first, (unsigned)kv.second.s.pcm.size(), kv.second.s.name.c_str(), kv.second.age.c_str(), kv.second.boots); line(tag, b); } ok("n=" + std::to_string(bin.size())); return; }
            if (mode != "EDIT") { err("mode"); return; }
            if (c == "BIN_EMPTY") { if (t.size() < 2 || t[1] != "YES") { err("arg"); return; } size_t n = bin.size(); bin.clear(); ok("n=" + std::to_string(n)); return; }
            int id = atoi(t[1].c_str());
            if (c == "DELETE_POOL") { auto it = pool.find(id); if (it == pool.end()) { err("range"); return; }
                Bin b; b.s = it->second; b.age = "0"; b.boots = 5; bin[id] = b; pool.erase(it); for (int& s : slotIds) if (s == id) s = 0; ok("id=" + std::to_string(id)); }
            else { auto it = bin.find(id); if (it == bin.end()) { err("range"); return; } pool[id] = it->second.s; bin.erase(it); ok("id=" + std::to_string(id)); }
        }
        else err("unknown");
    }
};

// A client wired to a FakePmor: run(ms) advances 5 ms steps (client <-> device).
struct FakeLink {
    FakePmor& dev; pmorhost::Client& C; std::vector<std::string> infos;
    FakeLink(FakePmor& d, pmorhost::Client& c) : dev(d), C(c) {
        C.send = [this](const std::string& s) { dev.rx(s); };
        C.onInfo = [this](const std::string& s) { infos.push_back(s); };
    }
    void run(int ms) { for (int t = 0; t < ms; t += 5) { std::string o; o.swap(dev.toHost); if (!o.empty()) C.feed(o.data(), o.size()); C.tick(5); } }
    bool until(std::function<bool()> p, int maxMs) { for (int t = 0; t < maxMs; t += 5) { if (p()) return true; run(5); } return p(); }
    bool info(const char* needle) const { for (auto& s : infos) if (s.find(needle) != std::string::npos) return true; return false; }
};
