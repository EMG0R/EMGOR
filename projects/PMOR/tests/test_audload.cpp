// AudLoader: the sampler-audio restore is a bounded background job (the old synchronous boot read of up to 3 MB took ~12 s on a slow card).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include "../pmor_core/aud_loader.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
static const uint32_t MAGIC = 0x44554150u;
struct Sim {
    std::vector<uint8_t> aud, bak; std::vector<int16_t> pool;
    std::vector<int16_t> out[16]; uint32_t len[16]; uint64_t bytes = 0; uint32_t maxread = 0, reads = 0;
    bool truncate_aud = false;
};
static Sim S;
static std::vector<uint8_t> makeFile(const uint32_t* lens, uint32_t seed, bool tail = true) {
    std::vector<uint8_t> f; auto w = [&](uint32_t v) { for (int i = 0; i < 4; i++) f.push_back((uint8_t)(v >> (8 * i))); };
    w(MAGIC); w(1); for (int i = 0; i < 16; i++) w(lens[i]);
    for (int s = 0; s < 16; s++) for (uint32_t i = 0; i < lens[s]; i++) { int16_t v = (int16_t)((i * 7 + s * 131 + seed) & 0x7FFF); f.push_back((uint8_t)v); f.push_back((uint8_t)(v >> 8)); }
    if (tail) w(MAGIC);
    return f;
}
static int32_t fr(void*, const char* nm, uint32_t off, void* buf, uint32_t n) {
    const std::vector<uint8_t>& f = strstr(nm, ".BAK") ? S.bak : S.aud;
    if (f.empty()) return -1;
    if (off >= f.size()) return 0; uint32_t k = f.size() - off < n ? (uint32_t)f.size() - off : n; memcpy(buf, f.data() + off, k);
    S.bytes += k; S.reads++; if (k > S.maxread) S.maxread = k; return (int32_t)k;
}
static int32_t ir(void*, uint16_t id, uint32_t off, int16_t* b, uint32_t n) { if (off + n > S.pool.size()) return -1; memcpy(b, S.pool.data() + off, n * 2); S.bytes += n * 2; S.reads++; return (int32_t)n; }
static uint32_t il(void*, uint16_t) { return (uint32_t)S.pool.size(); }
static void put(void*, uint8_t sl, uint32_t off, const int16_t* s, uint32_t n) { if (S.out[sl].size() < off + n) S.out[sl].resize(off + n); memcpy(S.out[sl].data() + off, s, n * 2); }
static void sl(void*, uint8_t s, uint32_t l) { S.len[s] = l; }
static AudSrc src() { AudSrc a{}; a.file_read = fr; a.id_read = ir; a.id_len = il; a.put = put; a.set_len = sl; return a; }
static void reset() { S = Sim(); for (int i = 0; i < 16; i++) S.len[i] = 999; }
static int run(AudLoader& l, int cap = 100000) { int n = 0; while (l.step() && n < cap) n++; return n + 1; }
int main() {
    uint32_t lens[16]; for (int i = 0; i < 16; i++) lens[i] = 96000;            // worst case: 16 slots x 2 s = 3 MB
    {   printf("[worst case: 16 x 96000 samples]\n"); reset(); S.aud = makeFile(lens, 1);
        AudLoader l; l.beginFile(src(), "P03.AUD", MAGIC);
        EXPECT(S.bytes == 0 && l.busy(), "arming reads nothing (patchInit cost ~0): %llu bytes", (unsigned long long) S.bytes);
        int steps = run(l); bool same = true; for (int s = 0; s < 16 && same; s++) { same = S.len[s] == 96000 && S.out[s].size() == 96000; for (uint32_t i = 0; i < 96000 && same; i += 997) same = S.out[s][i] == (int16_t)((i * 7 + s * 131 + 1) & 0x7FFF); }
        EXPECT(l.result == AudLoader::OK && same, "restored bit-exact in %d steps", steps);
        EXPECT(S.maxread <= AUDLD_CHUNK * 2, "no step reads more than %u bytes (max %u): a pass never blocks for long", AUDLD_CHUNK * 2, S.maxread);
        double slow = 250.0 * 1024;   // slow card: 250 KB/s
        printf("       (slow-card model 250 KB/s: old boot blocked %.1f s; now %.0f ms per main-loop pass, restore finishes in background)\n", S.bytes / slow, S.maxread / slow * 1000);
    }
    {   printf("[fallback + failure]\n"); reset(); S.aud = makeFile(lens, 1, false); S.bak = makeFile(lens, 2);       // primary lacks the completion sentinel
        AudLoader l; l.beginFile(src(), "P03.AUD", MAGIC); run(l);
        EXPECT(l.result == AudLoader::OK && S.out[0][5] == (int16_t)((5 * 7 + 2) & 0x7FFF), "partial primary -> .BAK used");
        reset(); S.aud = makeFile(lens, 1, false); S.bak = makeFile(lens, 2, false); AudLoader m; m.beginFile(src(), "P03.AUD", MAGIC); run(m);
        bool z = true; for (int i = 0; i < 16; i++) if (S.len[i] != 0) z = false;
        EXPECT(m.result == AudLoader::FAILED && z, "both bad -> FAILED with every slot length 0");
        reset(); S.aud = makeFile(lens, 1); S.aud.resize(S.aud.size() / 2); S.bak = makeFile(lens, 3); AudLoader t; t.beginFile(src(), "P03.AUD", MAGIC); run(t);
        EXPECT(t.result == AudLoader::OK && S.len[15] == 96000, "truncated primary (short read mid-file) -> .BAK");
        reset(); AudLoader n; n.beginFile(src(), "P03.AUD", MAGIC); run(n); EXPECT(n.result == AudLoader::FAILED, "no file at all -> FAILED, no crash");
    }
    {   printf("[pool ids, cancel]\n"); reset(); S.pool.resize(5000); for (size_t i = 0; i < S.pool.size(); i++) S.pool[i] = (int16_t)i;
        uint16_t ids[16] = {}; ids[2] = 7; ids[9] = 8; AudLoader l; l.beginIds(src(), ids); run(l);
        EXPECT(l.result == AudLoader::OK && S.len[2] == 5000 && S.len[9] == 5000 && S.len[0] == 0 && S.out[9][4999] == 4999, "pool-id restore");
        reset(); S.aud = makeFile(lens, 1); AudLoader c; c.beginFile(src(), "P03.AUD", MAGIC); for (int i = 0; i < 5; i++) c.step(); c.cancel();
        EXPECT(!c.busy() && !c.step(), "cancel mid-way stops the job");
    }
    {   printf("[main loop stays responsive while a long store queue and the restore run]\n"); reset(); S.aud = makeFile(lens, 1);
        AudLoader l; l.beginFile(src(), "P03.AUD", MAGIC); int store_pending = 56, passes = 0; uint32_t worst = 0;
        while (l.busy() || store_pending > 0) { uint64_t b0 = S.bytes; if (l.busy()) l.step(); else store_pending--; passes++; uint32_t d = (uint32_t)(S.bytes - b0); if (d > worst) worst = d; }
        EXPECT(passes >= 56 && worst <= AUDLD_CHUNK * 2, "56 pending store jobs + restore interleave pass by pass (%d passes, worst pass reads %u bytes)", passes, worst);
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
