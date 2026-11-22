// sdlayout.cpp -- see sdlayout.h. Hardware-free; all I/O through StoreFs.
#include "sdlayout.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define AUD_MAGIC_L 0x44554150u   // 'PAUD' (same as store/patch.cpp's legacy sidecar)
#define AUD_HDR_BYTES (8u + 4u * SMP_SLOTS)
#define WAV_HDR 44u
#define CHUNK 2048u               // SD bytes per job tick (matches the legacy 2 KB-per-tick AUD save)

// ------------------------------------------------------------------ crc / paths / guard
static uint32_t crcUpd(uint32_t crc, const void* data, size_t n) {   // running state; start CRC_INIT
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++) { crc ^= p[i]; for (int b = 0; b < 8; b++) crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1); }
    return crc;
}
#define CRC_INIT 0xFFFFFFFFu
#define CRC_FIN(c) (~(c))

static void pth(const StoreCtx& c, char* o, size_t n, const char* fmt, unsigned a = 0) { char t[48]; snprintf(t, sizeof t, fmt, a); snprintf(o, n, "%s/%s", c.root, t); }
static bool pathOkToWrite(const StoreCtx& c, const char* path) {   // THE guard: only the new tree is writable/removable
    size_t rl = strlen(c.root);
    if (strncmp(path, c.root, rl) != 0 || path[rl] != '/') return false;
    const char* r = path + rl + 1;
    if (strcmp(r, "MANIFEST.TXT") == 0) return true;
    if (strncmp(r, "SAMPLES/S", 9) == 0 || strncmp(r, "PATCHES/P", 9) == 0 || strncmp(r, "PRESETS/", 8) == 0 || strncmp(r, "BIN/S", 5) == 0 || strcmp(r, "BIN/INDEX.TXT") == 0)
        return strstr(r, "..") == nullptr && strstr(r, ".PMR") == nullptr && strstr(r, ".AUD") == nullptr && strstr(r, ".BAK") == nullptr;
    return false;
}
static void sampleName(const StoreCtx& c, char* o, size_t n, uint16_t id) { char t[32]; snprintf(t, sizeof t, "SAMPLES/S%04u.WAV", (unsigned)id); snprintf(o, n, "%s/%s", c.root, t); }
static void patchTxtName(const StoreCtx& c, char* o, size_t n, unsigned slot) { char t[32]; snprintf(t, sizeof t, "PATCHES/P%02u.TXT", slot); snprintf(o, n, "%s/%s", c.root, t); }
static void legacyName(const StoreCtx& c, char* o, size_t n, unsigned slot, const char* ext) { snprintf(o, n, "%s/P%02u.%s", c.root, slot, ext); }

static bool fileCrc(StoreCtx& c, const char* path, uint32_t off, uint32_t n, uint32_t* out) {
    uint32_t crc = CRC_INIT;
    while (n) { uint32_t k = n > sizeof c.scratch ? (uint32_t)sizeof c.scratch : n;
        if (c.fs.read(c.fs.u, path, off, c.scratch, k) != (int)k) return false;
        crc = crcUpd(crc, c.scratch, k); off += k; n -= k; }
    *out = CRC_FIN(crc); return true;
}

// ------------------------------------------------------------------ writer (new tree only)
static bool wOpen(StoreCtx& c, const char* path) {
    StoreWr& w = c.w; w.ok = false; w.crc = CRC_INIT; w.total = 0; w.path[0] = 0;
    if (!pathOkToWrite(c, path) || strlen(path) >= sizeof w.path) return false;   // refuses legacy files, always
    strcpy(w.path, path); w.ok = c.fs.wbegin(c.fs.u, path); return w.ok;
}
static void wPut(StoreCtx& c, const void* d, uint32_t n) { StoreWr& w = c.w; if (!w.ok) return; if (!c.fs.wappend(c.fs.u, d, n)) { w.ok = false; return; } w.crc = crcUpd(w.crc, d, n); w.total += n; }
static void wLine(StoreCtx& c, const char* fmt, unsigned a = 0, unsigned b = 0, unsigned d = 0, unsigned e = 0) { char t[200]; int k = snprintf(t, sizeof t, fmt, a, b, d, e); if (k > 0) wPut(c, t, (uint32_t)k); }
static bool wCommit(StoreCtx& c) {            // promote .TMP; verification is the caller's (blocking wVerify or incremental)
    StoreWr& w = c.w; bool ok = w.ok && c.fs.wend(c.fs.u, true);
    if (!w.ok) c.fs.wend(c.fs.u, false);
    if (!ok) { c.errors++; return false; }
    c.written++; return true;
}
static bool wVerify(StoreCtx& c) { uint32_t got; StoreWr& w = c.w;
    bool ok = c.fs.size(c.fs.u, w.path) == (int32_t)w.total && fileCrc(c, w.path, 0, w.total, &got) && got == CRC_FIN(w.crc);
    if (ok) c.verified++; else c.errors++; return ok; }
static bool wClose(StoreCtx& c) { return wCommit(c) && wVerify(c); }

// ------------------------------------------------------------------ text reading
struct LineRd { uint32_t crc_before; };
template <class F> static bool eachLine(StoreCtx& c, const char* path, LineRd& lr, F cb, uint32_t maxBytes = 0xFFFFFFFFu) {
    int32_t sz = c.fs.size(c.fs.u, path); if (sz < 0) return false;
    uint32_t lim = (uint32_t)sz < maxBytes ? (uint32_t)sz : maxBytes;
    char* ln = c.ln; const uint32_t LNMAX = sizeof c.ln; uint32_t ll = 0; uint32_t crc = CRC_INIT;
    for (uint32_t off = 0; off < lim; ) {
        uint32_t k = lim - off; if (k > sizeof c.scratch) k = sizeof c.scratch;
        if (c.fs.read(c.fs.u, path, off, c.scratch, k) != (int)k) return false;
        off += k;
        for (uint32_t i = 0; i < k; i++) {
            char ch = (char)c.scratch[i];
            if (ch == '\n') { ln[ll] = 0; lr.crc_before = CRC_FIN(crc); if (!cb(ln)) return true;
                crc = crcUpd(crc, ln, ll); crc = crcUpd(crc, "\n", 1); ll = 0; }
            else if (ll < LNMAX - 1) ln[ll++] = ch; else return false;
        }
    }
    return true;
}

// ------------------------------------------------------------------ manifest
void storeInit(StoreCtx& c, const StoreFs& fs, const char* root) { memset(&c, 0, sizeof c); c.fs = fs; snprintf(c.root, sizeof c.root, "%s", root); }
static bool ensureDirs(StoreCtx& c) {
    const char* sub[] = { "", "SAMPLES", "PATCHES", "PRESETS" };
    for (auto s : sub) { char p[48]; if (*s) snprintf(p, sizeof p, "%s/%s", c.root, s); else snprintf(p, sizeof p, "%s", c.root); if (!c.fs.mkdir(c.fs.u, p)) { c.errors++; return false; } }
    return true;
}
const StoreSample* storeFindSample(const StoreCtx& c, uint16_t id) { for (int i = 0; i < c.n_smp; i++) if (c.smp[i].id == id) return &c.smp[i]; return nullptr; }

bool storeLoadManifest(StoreCtx& c) {
    c.n_smp = 0; char p[48]; pth(c, p, sizeof p, "MANIFEST.TXT");
    bool end_ok = false, bad = false; LineRd lr;
    bool rd = eachLine(c, p, lr, [&](char* l) {
        if (l[0] == 'M' && l[1] == ' ') { unsigned id, len, rate, ch, crc;
            if (sscanf(l + 2, "%u %u %u %u %x", &id, &len, &rate, &ch, &crc) != 5 || c.n_smp >= STORE_MAX_SAMPLES) { bad = true; return false; }
            c.smp[c.n_smp++] = StoreSample{ (uint16_t)id, len, rate, (uint8_t)ch, crc }; return true; }
        if (strncmp(l, "END ", 4) == 0) { unsigned n, crc; if (sscanf(l + 4, "%u %x", &n, &crc) == 2 && n == c.n_smp && crc == lr.crc_before) end_ok = true; return false; }
        return true; });
    if (!rd || bad || !end_ok) { c.n_smp = 0; return false; }
    return true;
}
static bool writeManifest(StoreCtx& c) {
    char p[48]; pth(c, p, sizeof p, "MANIFEST.TXT");
    if (!wOpen(c, p)) { c.errors++; return false; }
    wLine(c, "PMORSTORE 1\n"); wLine(c, "# M id len_samples rate ch crc32(pcm) file\n");
    for (int i = 0; i < c.n_smp; i++) { const StoreSample& s = c.smp[i];
        wLine(c, "M %u %u %u %u ", s.id, s.len, s.rate, s.ch); wLine(c, "%08x ", s.crc); wLine(c, "SAMPLES/S%04u.WAV\n", s.id); }
    uint32_t crc = CRC_FIN(c.w.crc); char t[48]; int k = snprintf(t, sizeof t, "END %u %08x\n", (unsigned)c.n_smp, (unsigned)crc); wPut(c, t, (uint32_t)k);
    bool ok = wClose(c); if (ok) c.man_dirty = false; return ok;
}

// ------------------------------------------------------------------ samples
static uint32_t le32(uint8_t* d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); return v; }
static void wavHeader(uint8_t* h, uint32_t len, uint32_t rate) {
    memcpy(h, "RIFF", 4); le32(h + 4, 36 + len * 2); memcpy(h + 8, "WAVEfmt ", 8); le32(h + 16, 16);
    h[20] = 1; h[21] = 0; h[22] = 1; h[23] = 0; le32(h + 24, rate); le32(h + 28, rate * 2); h[32] = 2; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4); le32(h + 40, len * 2);
}
bool storeSampleVerify(StoreCtx& c, uint16_t id) {
    const StoreSample* s = storeFindSample(c, id); if (!s) return false;
    char p[48]; sampleName(c, p, sizeof p, id); uint32_t crc;
    return c.fs.size(c.fs.u, p) == (int32_t)(WAV_HDR + s->len * 2) && fileCrc(c, p, WAV_HDR, s->len * 2, &crc) && crc == s->crc;
}
int32_t storeSampleRead(StoreCtx& c, uint16_t id, uint32_t off, int16_t* buf, uint32_t n) {
    const StoreSample* s = storeFindSample(c, id); if (!s || off > s->len) return -1;
    if (off + n > s->len) n = s->len - off;
    char p[48]; sampleName(c, p, sizeof p, id);
    int g = c.fs.read(c.fs.u, p, WAV_HDR + off * 2, buf, n * 2); return g < 0 ? -1 : g / 2;
}

// ------------------------------------------------------------------ patch text
typedef StorePatchTxt PatchTxt;
static void txtHeader(StoreCtx& c, unsigned slot, const PatchTxt& t) {
    wLine(c, "PMORPATCH 1\n"); wLine(c, "slot %u\n", slot); wLine(c, "fmt v8 %u\n", (unsigned)sizeof(Patch));
    wLine(c, "src_pmr %u ", t.pmr_size); wLine(c, "%u ", t.pmr_stamp); wLine(c, "%08x\n", t.pmr_crc);
    wLine(c, "src_aud %u ", t.aud_size); wLine(c, "%u ", t.aud_stamp); { char b[40]; int k = snprintf(b, sizeof b, "%s\n", t.aud_src[0] ? t.aud_src : "-"); wPut(c, b, (uint32_t)k); }
    for (int s = 0; s < SMP_SLOTS; s++) wLine(c, "s %u %u\n", s, t.ids[s]);
}
static void txtBlobLines(StoreCtx& c, const Patch& pb, size_t from, size_t lines) {
    const uint8_t* bp = (const uint8_t*)&pb; static const char* H = "0123456789abcdef";
    for (size_t o = from; o < sizeof pb && lines; o += 64, lines--) {
        char b[140]; size_t k = sizeof pb - o < 64 ? sizeof pb - o : 64, n = 0; b[n++] = 'b'; b[n++] = ' ';
        for (size_t i = 0; i < k; i++) { b[n++] = H[bp[o + i] >> 4]; b[n++] = H[bp[o + i] & 15]; }
        b[n++] = '\n'; wPut(c, b, (uint32_t)n);
    }
}
static void txtEnd(StoreCtx& c) { uint32_t crc = CRC_FIN(c.w.crc); char e[24]; int k = snprintf(e, sizeof e, "end %08x\n", (unsigned)crc); wPut(c, e, (uint32_t)k); }
static int hexv(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1; }
// header-only (stops at the first blob line) or full parse with blob + end-CRC check
static bool parsePatchTxt(StoreCtx& c, unsigned slot, Patch* pb, PatchTxt& t, bool full) {
    char p[48]; patchTxtName(c, p, sizeof p, slot);
    memset(&t, 0, sizeof t); size_t got = 0; bool end_ok = false, hdr = false, stop = false; LineRd lr;
    uint8_t* bp = (uint8_t*)pb;
    bool rd = eachLine(c, p, lr, [&](char* l) {
        if (strcmp(l, "PMORPATCH 1") == 0) { hdr = true; return true; }
        unsigned a, b, d; char s[16];
        if (sscanf(l, "src_pmr %u %u %x", &a, &b, &d) == 3) { t.pmr_size = a; t.pmr_stamp = b; t.pmr_crc = d; return true; }
        if (sscanf(l, "src_aud %u %u %15s", &a, &b, s) == 3) { t.aud_size = a; t.aud_stamp = b; snprintf(t.aud_src, sizeof t.aud_src, "%s", strcmp(s, "-") ? s : ""); return true; }
        if (sscanf(l, "s %u %u", &a, &b) == 2 && a < SMP_SLOTS) { t.ids[a] = (uint16_t)b; return true; }
        if (l[0] == 'b' && l[1] == ' ') {
            if (!full) { stop = true; return false; }
            for (const char* h = l + 2; h[0] && h[1]; h += 2) { int x = hexv(h[0]), y = hexv(h[1]); if (x < 0 || y < 0 || got >= sizeof(Patch)) return false; bp[got++] = (uint8_t)(x * 16 + y); }
            return true; }
        if (sscanf(l, "end %x", &d) == 1) { end_ok = (d == lr.crc_before); return false; }
        return true; }, full ? 0xFFFFFFFFu : 1024u);
    if (!full) return rd && hdr && stop;
    t.blob_ok = rd && hdr && end_ok && got == sizeof(Patch) && patchCrcOk(*pb);
    return t.blob_ok;
}

// ------------------------------------------------------------------ legacy sources (READ-ONLY)
static bool legacyPmr(StoreCtx& c, unsigned slot, Patch& pb, PatchTxt& t) {
    char p[48]; legacyName(c, p, sizeof p, slot, "PMR");
    int32_t sz = c.fs.size(c.fs.u, p);
    if (sz != (int32_t)sizeof(Patch) && sz != (int32_t)sizeof(Patch7)) return false;
    if (c.fs.read(c.fs.u, p, 0, c.scratch, (uint32_t)sz) != sz) return false;
    t.pmr_size = (uint32_t)sz; t.pmr_stamp = c.fs.stamp(c.fs.u, p); t.pmr_crc = CRC_FIN(crcUpd(CRC_INIT, c.scratch, (size_t)sz));
    return patchFromBlob(c.scratch, (size_t)sz, pb);
}
static bool audCheck(StoreCtx& c, const char* path, uint32_t lens[SMP_SLOTS]) {
    int32_t sz = c.fs.size(c.fs.u, path); if (sz < (int32_t)(AUD_HDR_BYTES + 4)) return false;
    uint32_t h[2 + SMP_SLOTS]; if (c.fs.read(c.fs.u, path, 0, h, sizeof h) != (int)sizeof h || h[0] != AUD_MAGIC_L) return false;
    uint64_t tot = 0; for (int i = 0; i < SMP_SLOTS; i++) { lens[i] = h[2 + i] > SMP_LEN ? SMP_LEN : h[2 + i]; tot += h[2 + i]; }
    if (tot * 2 + AUD_HDR_BYTES + 4 != (uint64_t)sz) return false;
    uint32_t tail; return c.fs.read(c.fs.u, path, (uint32_t)sz - 4, &tail, 4) == 4 && tail == AUD_MAGIC_L;
}
static bool legacyAud(StoreCtx& c, unsigned slot, PatchTxt& cur, uint32_t lens[SMP_SLOTS], char* aud, size_t apn) {
    const char* ext[2] = { "AUD", "AUD.BAK" };
    for (int e = 0; e < 2; e++) { legacyName(c, aud, apn, slot, ext[e]);
        if (audCheck(c, aud, lens)) { cur.aud_size = (uint32_t)c.fs.size(c.fs.u, aud); cur.aud_stamp = c.fs.stamp(c.fs.u, aud); snprintf(cur.aud_src, sizeof cur.aud_src, "%s", e ? "BAK" : "AUD"); return true; } }
    cur.aud_size = 0; cur.aud_stamp = 0; cur.aud_src[0] = 0; aud[0] = 0; memset(lens, 0, sizeof(uint32_t) * SMP_SLOTS); return false;
}
// cheap stamp-only form (no completeness check) for patchLoad / mid-job source-stability test
static void audStat(StoreCtx& c, unsigned slot, const char* src, uint32_t* size, uint32_t* stamp) {
    *size = 0; *stamp = 0; if (!src[0]) return;
    char p[48]; legacyName(c, p, sizeof p, slot, strcmp(src, "BAK") == 0 ? "AUD.BAK" : "AUD");
    int32_t s = c.fs.size(c.fs.u, p); if (s >= 0) { *size = (uint32_t)s; *stamp = c.fs.stamp(c.fs.u, p); }
}
static bool sameSrc(const PatchTxt& a, const PatchTxt& b) {
    return a.pmr_size == b.pmr_size && a.pmr_stamp == b.pmr_stamp && a.pmr_crc == b.pmr_crc &&
           a.aud_size == b.aud_size && a.aud_stamp == b.aud_stamp && strcmp(a.aud_src, b.aud_src) == 0;
}

// ------------------------------------------------------------------ the export job
void storeQueue(StoreCtx& c, uint8_t slot) { if (slot < STORE_PATCHES) { c.pending |= 1ull << slot; if (c.state != STORE_RUNNING) c.state = STORE_RUNNING; } }
void storeQueueAll(StoreCtx& c) { c.pending = ~0ull; c.preset_next = 0; c.state = STORE_RUNNING; }
void storeJobAbort(StoreCtx& c) {
    StoreJob& j = c.job;
    if (j.phase != J_IDLE) { if (c.w.ok || c.w.path[0]) c.fs.wend(c.fs.u, false); c.w.ok = false; c.w.path[0] = 0; c.pending |= 1ull << j.slot; j.phase = J_IDLE; }
}
static void presetStep(StoreCtx& c);
static void jobIdle(StoreCtx& c) { c.job.phase = J_IDLE; }
static void jobFail(StoreCtx& c) { c.errors++; jobIdle(c); }

bool storeJobTick(StoreCtx& c) {
    StoreJob& j = c.job;
    if (!c.started) { c.started = true; ensureDirs(c); storeLoadManifest(c); }
    if (j.phase == J_IDLE) {
        if (!c.pending) {
            if (c.preset_next < STORE_PRESET_ENGINES) { presetStep(c); return true; }       // after the patches: one preset file per call
            c.state = c.errors ? STORE_ERRORS : STORE_DONE; return false; }
        unsigned s = 0; while (!(c.pending & (1ull << s))) s++;
        c.pending &= ~(1ull << s); memset(&j, 0, sizeof j); j.slot = (uint8_t)s; j.phase = J_BEGIN; c.state = STORE_RUNNING;
    }
    switch (j.phase) {
    case J_BEGIN: {
        if (!legacyPmr(c, j.slot, c.jpb, j.cur)) { c.skipped++; jobIdle(c); break; }   // no valid legacy patch in this slot
        legacyAud(c, j.slot, j.cur, j.lens, j.aud, sizeof j.aud);
        PatchTxt old; bool fresh = parsePatchTxt(c, j.slot, nullptr, old, false) && sameSrc(old, j.cur);
        for (int k = 0; fresh && k < SMP_SLOTS; k++) if (old.ids[k] && !storeFindSample(c, old.ids[k])) fresh = false;   // manifest lost/behind
        if (fresh) { c.skipped++; jobIdle(c); break; }
        j.si = 0; j.off = AUD_HDR_BYTES; j.pos = 0; j.crc = CRC_INIT; j.phase = J_CRC; break; }
    case J_CRC: {
        while (j.si < SMP_SLOTS && (!j.lens[j.si] || !j.aud[0])) { j.off += j.lens[j.si] * 2; j.cur.ids[j.si] = 0; j.si++; }
        if (j.si >= SMP_SLOTS) { j.phase = c.man_dirty ? J_MANIFEST : J_TXT_H; break; }
        uint32_t total = j.lens[j.si] * 2, k = total - j.pos; if (k > CHUNK) k = CHUNK;
        if (c.fs.read(c.fs.u, j.aud, j.off + j.pos, c.scratch, k) != (int)k) { jobFail(c); break; }
        j.crc = crcUpd(j.crc, c.scratch, k); j.pos += k;
        if (j.pos < total) break;
        uint32_t crc = CRC_FIN(j.crc), len = j.lens[j.si]; j.pcrc = crc; j.len = len;
        int found = -1; for (int i = 0; i < c.n_smp; i++) if (c.smp[i].crc == crc && c.smp[i].len == len) { found = i; break; }
        j.pos = 0; j.crc = CRC_INIT;
        if (found >= 0) {
            char sp[48]; sampleName(c, sp, sizeof sp, c.smp[found].id);
            if (c.fs.size(c.fs.u, sp) == (int32_t)(WAV_HDR + len * 2)) { j.cur.ids[j.si] = c.smp[found].id; j.off += total; j.si++; break; }   // pool hit
            j.id = c.smp[found].id; j.is_new = false;                                                                     // listed but file lost: rewrite in place
        } else {
            if (c.n_smp >= STORE_MAX_SAMPLES) { jobFail(c); break; }
            uint16_t id = STORE_ID_BASE; for (int i = 0; i < c.n_smp; i++) if (c.smp[i].id >= id) id = (uint16_t)(c.smp[i].id + 1);
            j.id = id; j.is_new = true;
        }
        char sp[48]; sampleName(c, sp, sizeof sp, j.id);
        if (!wOpen(c, sp)) { jobFail(c); break; }
        uint8_t h[WAV_HDR]; wavHeader(h, len, (uint32_t)SR); wPut(c, h, WAV_HDR);
        j.phase = J_WAV_W; break; }
    case J_WAV_W: {
        uint32_t total = j.len * 2, k = total - j.pos; if (k > CHUNK) k = CHUNK;
        if (c.fs.read(c.fs.u, j.aud, j.off + j.pos, c.scratch, k) != (int)k) { c.fs.wend(c.fs.u, false); c.w.ok = false; jobFail(c); break; }
        wPut(c, c.scratch, k); j.pos += k;
        if (!c.w.ok) { c.fs.wend(c.fs.u, false); jobFail(c); break; }
        if (j.pos < total) break;
        if (!wCommit(c)) { jobIdle(c); break; }
        j.pos = 0; j.crc = CRC_INIT; j.phase = J_WAV_V; break; }
    case J_WAV_V: {
        char sp[48]; sampleName(c, sp, sizeof sp, j.id);
        uint32_t total = j.len * 2, k = total - j.pos; if (k > CHUNK) k = CHUNK;
        if (j.pos == 0 && c.fs.size(c.fs.u, sp) != (int32_t)(WAV_HDR + total)) { jobFail(c); break; }
        if (c.fs.read(c.fs.u, sp, WAV_HDR + j.pos, c.scratch, k) != (int)k) { jobFail(c); break; }
        j.crc = crcUpd(j.crc, c.scratch, k); j.pos += k;
        if (j.pos < total) break;
        if (CRC_FIN(j.crc) != j.pcrc) { jobFail(c); break; }
        c.verified++;
        if (j.is_new) { c.smp[c.n_smp++] = StoreSample{ j.id, j.len, (uint32_t)SR, 1, j.pcrc }; c.man_dirty = true; }
        j.cur.ids[j.si] = j.id; j.off += total; j.si++; j.pos = 0; j.crc = CRC_INIT; j.phase = J_CRC; break; }
    case J_MANIFEST:
        if (!writeManifest(c)) { jobIdle(c); break; }
        j.phase = J_TXT_H; break;
    case J_TXT_H: {
        // the legacy sources must not have changed under us (a save would also have called storeJobAbort)
        PatchTxt now; memset(&now, 0, sizeof now); uint32_t asz, ast; audStat(c, j.slot, j.cur.aud_src, &asz, &ast);
        char pp[48]; legacyName(c, pp, sizeof pp, j.slot, "PMR");
        if (c.fs.size(c.fs.u, pp) != (int32_t)j.cur.pmr_size || c.fs.stamp(c.fs.u, pp) != j.cur.pmr_stamp || asz != j.cur.aud_size || ast != j.cur.aud_stamp) { c.pending |= 1ull << j.slot; jobIdle(c); break; }
        char p[48]; patchTxtName(c, p, sizeof p, j.slot);
        if (!wOpen(c, p)) { jobFail(c); break; }
        txtHeader(c, j.slot, j.cur); j.pos = 0; j.phase = J_TXT_B; break; }
    case J_TXT_B:
        txtBlobLines(c, c.jpb, j.pos, 16); j.pos += 16 * 64;
        if (!c.w.ok) { c.fs.wend(c.fs.u, false); jobFail(c); break; }
        if (j.pos >= sizeof(Patch)) j.phase = J_TXT_E;
        break;
    case J_TXT_E:
        txtEnd(c); if (!wCommit(c)) { jobIdle(c); break; }
        j.pos = 0; j.crc = CRC_INIT; j.phase = J_TXT_V; break;
    case J_TXT_V: {
        uint32_t k = c.w.total - j.pos; if (k > CHUNK) k = CHUNK;
        if (c.fs.read(c.fs.u, c.w.path, j.pos, c.scratch, k) != (int)k) { jobFail(c); break; }
        j.crc = crcUpd(j.crc, c.scratch, k); j.pos += k;
        if (j.pos < c.w.total) break;
        if (c.fs.size(c.fs.u, c.w.path) != (int32_t)c.w.total || CRC_FIN(j.crc) != CRC_FIN(c.w.crc)) { jobFail(c); break; }
        c.verified++; c.exported++; jobIdle(c); break; }
    default: jobIdle(c); break;
    }
    if (c.job.phase == J_IDLE && !c.pending && c.preset_next >= STORE_PRESET_ENGINES) { c.state = c.errors ? STORE_ERRORS : STORE_DONE; return false; }
    return true;
}
bool storeMigrateAll(StoreCtx& c) { c.errors = 0; storeQueueAll(c); while (storeJobTick(c)) {} return c.errors == 0; }

bool storeDropPatch(StoreCtx& c, uint8_t slot) {
    char p[48]; patchTxtName(c, p, sizeof p, slot);
    if (!pathOkToWrite(c, p)) return false;
    if (c.job.phase != J_IDLE && c.job.slot == slot) storeJobAbort(c), c.pending &= ~(1ull << slot);
    c.pending &= ~(1ull << slot);
    return c.fs.size(c.fs.u, p) < 0 || c.fs.remove(c.fs.u, p);
}

// ------------------------------------------------------------------ reading the new layout
bool storeLoadPatch(StoreCtx& c, uint8_t slot, Patch& out, uint16_t ids[16]) {
    if (slot >= STORE_PATCHES) return false;
    PatchTxt t, cur; memset(&cur, 0, sizeof cur);
    if (!parsePatchTxt(c, slot, &c.pb2, t, true)) return false;
    if (!legacyPmr(c, slot, c.pb, cur)) return false;
    uint32_t lens[SMP_SLOTS]; char aud[48]; legacyAud(c, slot, cur, lens, aud, sizeof aud);
    if (!sameSrc(t, cur)) return false;
    out = c.pb2; if (ids) memcpy(ids, t.ids, sizeof t.ids); return true;
}
bool storeFreshIds(StoreCtx& c, uint8_t slot, uint32_t pmr_size, uint32_t pmr_crc, uint16_t ids[16]) {
    if (slot >= STORE_PATCHES) return false;
    PatchTxt t; if (!parsePatchTxt(c, slot, nullptr, t, false)) return false;
    if (t.pmr_size != pmr_size || t.pmr_crc != pmr_crc) return false;
    uint32_t asz, ast; audStat(c, slot, t.aud_src, &asz, &ast);
    if (asz != t.aud_size || ast != t.aud_stamp) return false;
    char pp[48]; legacyName(c, pp, sizeof pp, slot, "PMR"); if (c.fs.stamp(c.fs.u, pp) != t.pmr_stamp) return false;
    for (int k = 0; k < SMP_SLOTS; k++) if (t.ids[k] && !storeFindSample(c, t.ids[k])) return false;
    memcpy(ids, t.ids, sizeof t.ids); return true;
}
bool storeRecoverPatch(StoreCtx& c, uint8_t slot, Patch& out, uint16_t ids[16]) {
    if (slot >= STORE_PATCHES) return false;
    PatchTxt t; if (!parsePatchTxt(c, slot, &c.pb2, t, true)) return false;
    out = c.pb2; if (ids) memcpy(ids, t.ids, sizeof t.ids); return true;
}
uint16_t storeAliasToId(StoreCtx& c, uint16_t old_id) {
    unsigned p = old_id / 16, s = old_id % 16; if (p >= STORE_PATCHES) return 0;
    PatchTxt t; return parsePatchTxt(c, (unsigned)p, nullptr, t, false) ? t.ids[s] : 0;
}

// ------------------------------------------------------------------ presets
static const char* const ENGINES[STORE_PRESET_ENGINES] = { "KICK", "SNARE", "HAT", "PERC", "SYN0", "SYN1", "SYN2", "SYN3", "SYN4", "SYN5", "SYN6", "SYN7" };
const char* storePresetEngineName(int i) { return (i >= 0 && i < STORE_PRESET_ENGINES) ? ENGINES[i] : nullptr; }

static int engIndex(const char* e) { for (int i = 0; i < STORE_PRESET_ENGINES; i++) if (strcmp(ENGINES[i], e) == 0) return i; return -1; }
static size_t factoryBlobSize(int ei) { return ei < 4 ? drumsDefSize() : sizeof(SynPreset); }
static void presetPath(const StoreCtx& c, int ei, char* o, size_t n) { snprintf(o, n, "%s/PRESETS/%s.TXT", c.root, ENGINES[ei]); }
static void cleanName(const char* in, char out[10]) { snprintf(out, 10, "%s", in ? in : ""); for (char* q = out; *q; q++) if (*q <= ' ') *q = '_'; }

static void pfEmit(StoreCtx& c, char tag, int n, const PfEntry& e) {
    static const char* H = "0123456789abcdef";
    char head[40]; int k = snprintf(head, sizeof head, "%c %d %s ", tag, n, e.name[0] ? e.name : "-"); wPut(c, head, (uint32_t)k);
    char buf[128]; size_t m = 0;
    for (uint16_t i = 0; i < e.len; i++) { buf[m++] = H[e.blob[i] >> 4]; buf[m++] = H[e.blob[i] & 15]; if (m >= sizeof buf - 2) { wPut(c, buf, (uint32_t)m); m = 0; } }
    buf[m++] = '\n'; wPut(c, buf, (uint32_t)m);
}
static bool pfWrite(StoreCtx& c, int ei) {
    char p[48]; presetPath(c, ei, p, sizeof p);
    if (!wOpen(c, p)) { c.errors++; return false; }
    wLine(c, "PMORPRESET 2\n"); { char b[48]; int k = snprintf(b, sizeof b, "engine %s\n", ENGINES[ei]); wPut(c, b, (uint32_t)k); }
    wLine(c, "blob %u\n", (unsigned)factoryBlobSize(ei));
    for (int i = 0; i < PF_N; i++) if (c.pf.f[i].used) pfEmit(c, 'f', i, c.pf.f[i]);
    for (int i = 0; i < PF_N; i++) if (c.pf.u[i].used) pfEmit(c, 'u', i, c.pf.u[i]);
    txtEnd(c); return wClose(c);
}
static void pfFactory(StoreCtx& c, int ei) {      // firmware tables -> c.pf.f
    memset(&c.pf, 0, sizeof c.pf);
    if (ei < 4) for (int i = 0; i < DRUM_SOUNDS; i++) { PfEntry& e = c.pf.f[i]; e.used = true; cleanName(DRUM_SOUND_NAMES[ei][i], e.name); e.len = (uint16_t)drumsDefSize(); memcpy(e.blob, drumsDefBlob((uint8_t)ei, (uint8_t)i), e.len); }
    else for (int i = 0; i < SYN_PRESETS; i++) { PfEntry& e = c.pf.f[i]; SynPreset q; synFactoryPreset((uint8_t)(ei - 4), (uint8_t)i, q); e.used = true; cleanName(SYN_PRESET_NAMES[i], e.name); e.len = sizeof q; memcpy(e.blob, &q, sizeof q); }
}
// parse into c.pf; false if absent / bad crc / blob size mismatch (then c.pf is cleared)
static bool pfRead(StoreCtx& c, int ei) {
    memset(&c.pf, 0, sizeof c.pf); char p[48]; presetPath(c, ei, p, sizeof p);
    bool hdr = false, end_ok = false, bad = false; unsigned blob = 0; LineRd lr;
    bool rd = eachLine(c, p, lr, [&](char* l) {
        unsigned d, n; char nm[16]; char tag;
        if (strcmp(l, "PMORPRESET 2") == 0) { hdr = true; return true; }
        if (sscanf(l, "blob %u", &blob) == 1) return true;
        if (sscanf(l, "end %x", &d) == 1) { end_ok = (d == lr.crc_before); return false; }
        if ((l[0] == 'f' || l[0] == 'u') && l[1] == ' ') {
            tag = l[0]; char* q = l + 2; n = (unsigned)strtoul(q, &q, 10); if (n >= PF_N || *q != ' ') { bad = true; return false; }
            q++; char* sp = strchr(q, ' '); if (!sp || sp - q > 9) { bad = true; return false; }
            size_t nl = (size_t)(sp - q); memcpy(nm, q, nl); nm[nl] = 0;
            PfEntry& e = tag == 'f' ? c.pf.f[n] : c.pf.u[n]; snprintf(e.name, sizeof e.name, "%s", strcmp(nm, "-") ? nm : "");
            q = sp + 1; size_t hl = strlen(q); if (hl % 2 || hl / 2 > PF_BLOB) { bad = true; return false; }
            for (size_t i = 0; i < hl; i += 2) { int x = hexv(q[i]), y = hexv(q[i + 1]); if (x < 0 || y < 0) { bad = true; return false; } e.blob[i / 2] = (uint8_t)(x * 16 + y); }
            e.len = (uint16_t)(hl / 2); e.used = true; return true; }
        return true; });
    if (!(rd && hdr && end_ok && !bad) || blob != factoryBlobSize(ei)) { memset(&c.pf, 0, sizeof c.pf); return false; }
    for (int i = 0; i < PF_N; i++) if (c.pf.f[i].used && c.pf.f[i].len != factoryBlobSize(ei)) { memset(&c.pf, 0, sizeof c.pf); return false; }
    return true;
}
static void pfInstall(StoreCtx& c, int ei) {      // c.pf -> running firmware tables
    if (ei < 4) {
        for (int i = 0; i < DRUM_SOUNDS; i++) if (c.pf.f[i].used) drumsDefInstall((uint8_t)ei, (uint8_t)i, c.pf.f[i].blob, c.pf.f[i].len);
        for (int n = 0; n < DRUM_USER_SLOTS; n++) if (c.pf.u[n].used && c.pf.u[n].len == sizeof(DrumUser)) { DrumUser du; memcpy(&du, c.pf.u[n].blob, sizeof du); drumsUserSet((uint8_t)ei, (uint8_t)n, du.base, du.k, c.pf.u[n].name); }
    }
    else for (int i = 0; i < PF_N; i++) { const PfEntry& e = c.pf.f[i].used && i < SYN_FILE_PRESETS ? c.pf.f[i] : c.pf.u[i];
        const PfEntry& u = c.pf.u[i]; const PfEntry& use = u.used ? u : e;
        if (use.used && use.len == sizeof(SynPreset) && i < SYN_FILE_PRESETS) { SynPreset q; memcpy(&q, use.blob, sizeof q); synFileSet((uint8_t)(ei - 4), (uint8_t)i, q, use.name); } }
}

void storeQueuePresets(StoreCtx& c) { c.preset_next = 0; c.state = STORE_RUNNING; }
// one engine per call: export when missing, install when present
static void presetStep(StoreCtx& c) {
    int ei = c.preset_next++; char p[48]; presetPath(c, ei, p, sizeof p);
    if (c.fs.size(c.fs.u, p) < 0) {                        // additive: only a MISSING file is created
        pfFactory(c, ei); if (pfWrite(c, ei)) c.presets_written++;
    } else if (pfRead(c, ei)) { pfInstall(c, ei); c.presets_loaded++; }
    else c.presets_ignored++;                              // present but unusable: firmware tables stay, file left alone
}
bool storeSynUserSave(StoreCtx& c, uint8_t algo, uint8_t idx, const char* name, const SynPreset& p) {
    if (algo >= ALG_COUNT || idx < SYN_PRESETS || idx >= PF_N) return false;
    int ei = 4 + algo;
    if (!pfRead(c, ei)) pfFactory(c, ei);                      // missing/unusable file: start from the factory set
    PfEntry& e = c.pf.u[idx]; e.used = true; cleanName(name, e.name); e.len = sizeof p; memcpy(e.blob, &p, sizeof p);
    if (!pfWrite(c, ei)) return false;
    synFileSet(algo, idx, p, e.name); return true;
}
bool storeDrumUserSave(StoreCtx& c, uint8_t group, uint8_t n, const char* name, const DrumUser& u) {
    if (group >= 4 || n >= PF_N) return false;
    if (!pfRead(c, group)) pfFactory(c, group);
    PfEntry& e = c.pf.u[n]; e.used = true; cleanName(name, e.name); e.len = sizeof u; memcpy(e.blob, &u, sizeof u);
    if (!pfWrite(c, group)) return false;
    if (n < DRUM_USER_SLOTS) drumsUserSet(group, n, u.base, u.k, e.name);
    return true;
}
bool storeDrumUserLoad(StoreCtx& c, uint8_t group, uint8_t n, char name[10], DrumUser* u) {
    if (group >= 4 || n >= PF_N || !pfRead(c, group) || !c.pf.u[n].used || c.pf.u[n].len != sizeof(DrumUser)) return false;
    memcpy(u, c.pf.u[n].blob, sizeof *u); snprintf(name, 10, "%s", c.pf.u[n].name); return true;
}


// ------------------------------------------------------------------ recycle bin
static void binName(const StoreCtx& c, char* o, size_t n, uint16_t id) { snprintf(o, n, "%s/BIN/S%04u.WAV", c.root, (unsigned)id); }
static void binIndexName(const StoreCtx& c, char* o, size_t n) { snprintf(o, n, "%s/BIN/INDEX.TXT", c.root); }
static bool binLoad(StoreCtx& c) {
    if (c.bin_loaded) return true;
    c.n_bin = 0; char p[48]; binIndexName(c, p, sizeof p);
    if (c.fs.size(c.fs.u, p) < 0) { c.bin_loaded = true; return true; }            // no bin yet = empty
    bool end_ok = false, bad = false; LineRd lr;
    bool rd = eachLine(c, p, lr, [&](char* l) {
        if (l[0] == 'B' && l[1] == ' ') { unsigned id, len, rate, ch, crc, at;
            if (sscanf(l + 2, "%u %u %u %u %x %u", &id, &len, &rate, &ch, &crc, &at) != 6 || c.n_bin >= STORE_BIN_MAX) { bad = true; return false; }
            c.bin[c.n_bin++] = StoreBinEntry{ (uint16_t)id, len, rate, (uint8_t)ch, crc, at }; return true; }
        if (strncmp(l, "END ", 4) == 0) { unsigned n, crc; if (sscanf(l + 4, "%u %x", &n, &crc) == 2 && n == c.n_bin && crc == lr.crc_before) end_ok = true; return false; }
        return true; });
    if (!rd || bad || !end_ok) { c.n_bin = 0; return false; }
    c.bin_loaded = true; return true;
}
static bool binWrite(StoreCtx& c) {
    char dir[48]; snprintf(dir, sizeof dir, "%s/BIN", c.root); if (!c.fs.mkdir(c.fs.u, dir)) return false;
    char p[48]; binIndexName(c, p, sizeof p);
    if (!wOpen(c, p)) { c.errors++; return false; }
    wLine(c, "PMORBIN 1\n"); wLine(c, "# B id len_samples rate ch crc32(pcm) deleted_at_uptime_s\n");
    for (int i = 0; i < c.n_bin; i++) { const StoreBinEntry& b = c.bin[i]; wLine(c, "B %u %u %u %u ", b.id, b.len, b.rate, b.ch); wLine(c, "%08x ", b.crc); wLine(c, "%u\n", b.at); }
    uint32_t crc = CRC_FIN(c.w.crc); char t[48]; int k = snprintf(t, sizeof t, "END %u %08x\n", (unsigned)c.n_bin, (unsigned)crc); wPut(c, t, (uint32_t)k);
    return wClose(c);
}
int storeRefsScan(StoreCtx& c, uint16_t id, unsigned first, unsigned n) {
    int refs = 0;
    for (unsigned s = first; s < first + n && s < STORE_PATCHES; s++) {
        PatchTxt t; if (!parsePatchTxt(c, s, nullptr, t, false)) continue;
        for (int k = 0; k < SMP_SLOTS; k++) if (t.ids[k] == id) { refs++; break; }
    }
    return refs;
}
int storeBinDelete(StoreCtx& c, uint16_t id, uint32_t at_s) {
    if (!c.fs.rename) return -4;
    if (!binLoad(c)) return -3;
    const StoreSample* sp = storeFindSample(c, id); if (!sp) return -1;
    if (c.n_bin >= STORE_BIN_MAX) return -2;
    StoreSample s = *sp;
    char dir[48]; snprintf(dir, sizeof dir, "%s/BIN", c.root); if (!c.fs.mkdir(c.fs.u, dir)) return -3;
    char from[48], to[48]; sampleName(c, from, sizeof from, id); binName(c, to, sizeof to, id);
    if (!pathOkToWrite(c, from) || !pathOkToWrite(c, to)) return -3;
    if (c.fs.size(c.fs.u, to) >= 0) return -3;                                       // never overwrite a binned file
    if (!c.fs.rename(c.fs.u, from, to)) return -3;
    // 1) index first (the file is safe in BIN either way), 2) then drop it from the manifest
    c.bin[c.n_bin++] = StoreBinEntry{ s.id, s.len, s.rate, s.ch, s.crc, at_s };
    if (!binWrite(c)) { c.n_bin--; c.fs.rename(c.fs.u, to, from); return -3; }
    for (int i = 0; i < c.n_smp; i++) if (c.smp[i].id == id) { for (int j = i; j + 1 < c.n_smp; j++) c.smp[j] = c.smp[j + 1]; c.n_smp--; break; }
    c.man_dirty = true; if (!writeManifest(c)) return -3;
    return 0;
}
int storeBinCount(StoreCtx& c) { return binLoad(c) ? (int)c.n_bin : -1; }
bool storeBinAt(StoreCtx& c, int i, StoreBinEntry* out) { if (!binLoad(c) || i < 0 || i >= c.n_bin) return false; *out = c.bin[i]; return true; }
int storeBinRestore(StoreCtx& c, uint16_t id) {
    if (!c.fs.rename) return -3;
    if (!binLoad(c)) return -3;
    int bi = -1; for (int i = 0; i < c.n_bin; i++) if (c.bin[i].id == id) bi = i;
    if (bi < 0) return -1;
    if (storeFindSample(c, id) || c.n_smp >= STORE_MAX_SAMPLES) return -2;
    char from[48], to[48]; binName(c, from, sizeof from, id); sampleName(c, to, sizeof to, id);
    if (c.fs.size(c.fs.u, to) >= 0) return -2;
    StoreBinEntry b = c.bin[bi];
    if (!c.fs.rename(c.fs.u, from, to)) return -3;
    c.smp[c.n_smp++] = StoreSample{ b.id, b.len, b.rate, b.ch, b.crc }; c.man_dirty = true;
    if (!writeManifest(c)) { c.n_smp--; c.fs.rename(c.fs.u, to, from); return -3; }
    for (int j = bi; j + 1 < c.n_bin; j++) c.bin[j] = c.bin[j + 1]; c.n_bin--;
    return binWrite(c) ? 0 : -3;                                                    // (a failed index write leaves a stale line; the file is back in the pool)
}
uint32_t storeBinToken(StoreCtx& c) {
    if (!binLoad(c)) return 0;
    uint32_t h = 0x9E3779B9u ^ c.n_bin; for (int i = 0; i < c.n_bin; i++) { h = crcUpd(h, &c.bin[i].id, 2); h = crcUpd(h, &c.bin[i].crc, 4); h = crcUpd(h, &c.bin[i].at, 4); }
    return h ? h : 1u;
}
int storeBinEmpty(StoreCtx& c, uint32_t token) {
    if (!binLoad(c)) return -3;
    if (!token || token != storeBinToken(c)) return -1;
    int erased = 0;
    for (int i = 0; i < c.n_bin; i++) { char p[48]; binName(c, p, sizeof p, c.bin[i].id); if (pathOkToWrite(c, p) && c.fs.size(c.fs.u, p) >= 0 && c.fs.remove(c.fs.u, p)) erased++; }
    c.n_bin = 0; return binWrite(c) ? erased : -3;
}

// ------------------------------------------------------------------ one-time patch ARCHIVE (PMOR3/ARCHIVE_<k>/): rename-only, never an erase.
// Patch files = the legacy P%02u.{PMR,PMR.BAK,AUD,AUD.BAK,EXT,EXT.BAK} and PATCHES/P%02u.{TXT,TXT.BAK}. SAMPLES/, MANIFEST.TXT, BIN/, PRESETS/ and
// the globals are never named here. pathOkToWrite() is deliberately NOT relaxed: the archive has its own whitelist (archSrc), built from the slot.
static const char* const ARCH_EXT[] = { "PMR", "PMR.BAK", "AUD", "AUD.BAK", "EXT", "EXT.BAK", "TXT", "TXT.BAK" };
#define ARCH_NEXT 8
static void archSrc(const StoreCtx& c, char* o, size_t n, unsigned slot, int e) {
    if (e < 6) snprintf(o, n, "%s/P%02u.%s", c.root, slot, ARCH_EXT[e]); else snprintf(o, n, "%s/PATCHES/P%02u.%s", c.root, slot, ARCH_EXT[e]);
}
int storeArchiveScan(StoreCtx& c, unsigned first, unsigned n, uint32_t* hash, uint16_t* patches) {
    for (unsigned s = first; s < first + n && s < STORE_PATCHES; s++) {
        bool any = false;
        for (int e = 0; e < ARCH_NEXT; e++) {
            char p[48]; archSrc(c, p, sizeof p, s, e); int32_t sz = c.fs.size(c.fs.u, p);
            if (sz >= 0) { any = true; uint32_t v[2] = { (uint32_t)(s * 16 + e), (uint32_t)sz }; *hash = crcUpd(*hash, v, 8); }
        }
        if (any) ++*patches;
    }
    return 0;
}
static bool archExists(StoreCtx& c, const char* p) { return c.fs.size(c.fs.u, p) >= 0; }
int storeArchiveStart(StoreCtx& c) {           // k = the ARCHIVE_<k> used (resumed or new), -3 io, -4 unsupported
    if (!c.fs.rename) return -4;
    storeJobAbort(c); c.pending = 0;           // the export job may hold the one write handle; nothing left to re-export
    int last = 0; for (int k = 1; k < 100; k++) { char d[48]; snprintf(d, sizeof d, "%s/ARCHIVE_%d", c.root, k); if (archExists(c, d)) last = k; }
    int k = last;
    if (last) { char m[64]; snprintf(m, sizeof m, "%s/ARCHIVE_%d/DONE.TXT", c.root, last); if (archExists(c, m)) k = 0; }   // latest one finished: start a new one
    if (!k) k = last + 1;
    if (k > 99) return -3;
    char d[48], d2[64]; snprintf(d, sizeof d, "%s/ARCHIVE_%d", c.root, k); snprintf(d2, sizeof d2, "%s/PATCHES", d);
    if (!c.fs.mkdir(c.fs.u, d) || !c.fs.mkdir(c.fs.u, d2)) return -3;
    memset(&c.arch, 0, sizeof c.arch); c.arch.active = 1; c.arch.k = (uint8_t)k; return k;
}
bool storeArchiveActive(const StoreCtx& c) { return c.arch.active != 0; }
static void archFail(StoreCtx& c, const char* from) {
    if (c.arch.failed < 6) { const char* r = from + strlen(c.root) + 1; strncpy(c.arch.fail[c.arch.failed], r, 23); c.arch.fail[c.arch.failed][23] = 0; }
    c.arch.failed++;
}
int storeArchiveNext(StoreCtx& c) {
    StoreArch& a = c.arch; if (!a.active) return 0;
    for (int look = 0; look < 8; look++) {               // bounded: at most 8 stats this call
        if (a.slot >= STORE_PATCHES) {
            if (a.failed == 0) {                          // marker: ARCHIVE_<k> is complete (a failed run stays resumable)
                char m[64]; snprintf(m, sizeof m, "%s/ARCHIVE_%u/DONE.TXT", c.root, (unsigned)a.k);
                if (c.fs.wbegin(c.fs.u, m)) { char t[40]; int n = snprintf(t, sizeof t, "moved=%u\n", (unsigned)a.moved); c.fs.wappend(c.fs.u, t, (uint32_t)n); c.fs.wend(c.fs.u, true); }
            }
            a.active = 0; return 0;
        }
        char from[48]; archSrc(c, from, sizeof from, a.slot, a.ext);
        if (++a.ext >= ARCH_NEXT) { a.ext = 0; a.slot++; }
        if (!archExists(c, from)) continue;
        char to[72]; snprintf(to, sizeof to, "%s/ARCHIVE_%u/%s", c.root, (unsigned)a.k, from + strlen(c.root) + 1);
        bool ok = false;
        if (!archExists(c, to)) ok = c.fs.rename(c.fs.u, from, to);
        else for (int r = 1; r < 10 && !ok; r++) {        // already in the archive (earlier attempt, or a rewritten file): never overwrite, keep both
            char t2[80]; snprintf(t2, sizeof t2, "%s.%d", to, r); if (!archExists(c, t2)) ok = c.fs.rename(c.fs.u, from, t2); else continue; break;
        }
        if (ok) a.moved++; else archFail(c, from);
        return 1;
    }
    return 2;
}
int storeArchiveList(StoreCtx& c, unsigned slot, char* out, size_t n) {
    int cnt = 0; size_t o = 0; if (n) out[0] = 0;
    for (int e = 0; e < ARCH_NEXT; e++) {
        char p[48]; archSrc(c, p, sizeof p, slot, e); if (!archExists(c, p)) continue;
        int w = snprintf(out + o, n - o, "%s%s%s", cnt ? " " : "", e >= 6 ? "PATCHES/" : "", ARCH_EXT[e]); if (w < 0 || (size_t)w >= n - o) break; o += (size_t)w; cnt++;
    }
    return cnt;
}
