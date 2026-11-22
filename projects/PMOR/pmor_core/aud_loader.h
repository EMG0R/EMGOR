// aud_loader.h -- sampler audio restore as a background job (hardware-free, header-only; store/patch.cpp wires FatFS / the sample pool in).
// Boot used to read up to SMP_SLOTS * SMP_LEN * 2 = 3 MB of sampler audio synchronously inside patchInit(); through FatFS with a USB service
// yield per sector that is ~12 s (macbert: HELLO ready ~12 s after a replug). Now patchInit() only arms this job and patchTick() runs one
// bounded step (<= AUDLD_CHUNK bytes of reads) per main-loop pass. Sources: the legacy .AUD sidecar (+ .BAK fallback) or pool sample ids.
#pragma once
#include <stdint.h>
#include <string.h>

#define AUDLD_CHUNK 2048u          // samples per step (4 KB)
#define AUDLD_SLOTS 16
#define AUDLD_MAX_LEN 96000u

struct AudSrc {                    // everything hardware: provided by the caller
    void* ctx;
    // bytes of file `name` at byte offset `off` -> buf; returns bytes read (0 = EOF/err, -1 = cannot open)
    int32_t (*file_read)(void* ctx, const char* name, uint32_t off, void* buf, uint32_t n);
    // samples of pool id at sample offset -> buf; returns samples read or -1
    int32_t (*id_read)(void* ctx, uint16_t id, uint32_t off, int16_t* buf, uint32_t n);
    uint32_t (*id_len)(void* ctx, uint16_t id);                       // 0 = unknown
    void (*put)(void* ctx, uint8_t slot, uint32_t off, const int16_t* s, uint32_t n);   // samples -> engine buffer
    void (*set_len)(void* ctx, uint8_t slot, uint32_t len);
};

struct AudLoader {
    enum Mode : uint8_t { IDLE = 0, FILE_HDR, FILE_DATA, FILE_TAIL, IDS } mode = IDLE;
    enum Result : uint8_t { PENDING = 0, OK, FAILED } result = OK;
    AudSrc src{}; char name[24] = {}; char bak[28] = {}; bool on_bak = false;
    uint32_t magic = 0; uint32_t lens[AUDLD_SLOTS] = {}; uint16_t ids[AUDLD_SLOTS] = {};
    uint8_t slot = 0; uint32_t off = 0, pos = 0; uint32_t steps = 0;
    bool busy() const { return mode != IDLE; }

    void beginFile(const AudSrc& s, const char* n, uint32_t aud_magic) {
        src = s; strncpy(name, n, sizeof(name) - 1); name[sizeof(name) - 1] = 0;
        snprintf_bak(); on_bak = false; magic = aud_magic; clearAll(); mode = FILE_HDR; result = PENDING; slot = 0; off = 0; pos = 0; steps = 0;
    }
    void beginIds(const AudSrc& s, const uint16_t* pool_ids) {
        src = s; memcpy(ids, pool_ids, sizeof(ids)); clearAll(); mode = IDS; result = PENDING; slot = 0; off = 0; steps = 0;
    }
    void cancel() { mode = IDLE; result = OK; }
    // one bounded unit of work; true while more remains
    bool step() {
        if (mode == IDLE) return false;
        steps++;
        switch (mode) {
        case FILE_HDR: {   // magic, version, lens[16]
            uint32_t h[2 + AUDLD_SLOTS];
            int32_t n = src.file_read(src.ctx, cur(), 0, h, sizeof h);
            if (n != (int32_t)sizeof h || h[0] != magic) return fail();
            for (int i = 0; i < AUDLD_SLOTS; i++) lens[i] = h[2 + i];
            pos = sizeof h; slot = 0; off = 0; mode = FILE_DATA; return true;
        }
        case FILE_DATA: {
            while (slot < AUDLD_SLOTS && lens[slot] == 0) { src.set_len(src.ctx, slot, 0); slot++; off = 0; }
            if (slot >= AUDLD_SLOTS) { mode = FILE_TAIL; return true; }
            uint32_t total = lens[slot] > AUDLD_MAX_LEN ? AUDLD_MAX_LEN : lens[slot];
            uint32_t c = total - off; if (c > AUDLD_CHUNK) c = AUDLD_CHUNK;
            int16_t buf[AUDLD_CHUNK];
            int32_t n = src.file_read(src.ctx, cur(), pos, buf, c * 2);
            if (n != (int32_t)(c * 2)) return fail();                 // short read: reject the whole file (caller's fallback = .BAK)
            src.put(src.ctx, slot, off, buf, c); off += c; pos += c * 2;
            if (off >= total) { src.set_len(src.ctx, slot, total); slot++; off = 0; }
            return true;
        }
        case FILE_TAIL: {
            uint32_t t = 0; int32_t n = src.file_read(src.ctx, cur(), pos, &t, 4);
            if (n != 4 || t != magic) return fail();                  // completion sentinel missing: partial file
            mode = IDLE; result = OK; return false;
        }
        case IDS: {
            while (slot < AUDLD_SLOTS && !ids[slot]) { src.set_len(src.ctx, slot, 0); slot++; off = 0; }
            if (slot >= AUDLD_SLOTS) { mode = IDLE; result = OK; return false; }
            uint32_t len = src.id_len(src.ctx, ids[slot]); if (!len) return fail();
            uint32_t total = len > AUDLD_MAX_LEN ? AUDLD_MAX_LEN : len;
            uint32_t c = total - off; if (c > AUDLD_CHUNK) c = AUDLD_CHUNK;
            int16_t buf[AUDLD_CHUNK];
            if (src.id_read(src.ctx, ids[slot], off, buf, c) != (int32_t)c) return fail();
            src.put(src.ctx, slot, off, buf, c); off += c;
            if (off >= total) { src.set_len(src.ctx, slot, total); slot++; off = 0; }
            return true;
        }
        default: return false;
        }
    }
private:
    const char* cur() const { return on_bak ? bak : name; }
    void snprintf_bak() { size_t i = 0; while (name[i] && i < sizeof(bak) - 5) { bak[i] = name[i]; i++; } const char* e = ".BAK"; for (int k = 0; k < 4; k++) bak[i++] = e[k]; bak[i] = 0; }
    void clearAll() { for (int i = 0; i < AUDLD_SLOTS; i++) { lens[i] = 0; src.set_len(src.ctx, (uint8_t)i, 0); } }
    bool fail() {
        if (mode != IDS && !on_bak) { on_bak = true; clearAll(); mode = FILE_HDR; pos = 0; slot = 0; off = 0; return true; }   // atomic-backup fallback, like audLoad()
        mode = IDLE; result = FAILED; clearAll(); return false;
    }
};
