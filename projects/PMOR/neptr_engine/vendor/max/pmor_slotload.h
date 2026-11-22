// pmor_slotload.h -- load a cached PMOR sample (.aud: raw int16 LE mono 48 kHz, no header; PROTOCOL section 8) into a sampler slot.
// Same decode as firmware (store/patch.cpp audLoad*): v = int16 / 32767, clamped to SMP_LEN, smp.len[slot] = n.
//   message thread: pmorslot::Mailbox::post(slot, path[, crc]) reads + validates the file (no audio-thread IO), parks the decoded floats.
//   audio thread:   Mailbox::apply() copies parked slots into smp_buf / smp.len (memcpy only; nothing allocated or freed there).
// Failure (missing, empty, odd size, bad CRC) clears the slot (silent), never throws.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <string>
#include <vector>
#include "sampler.h"

namespace pmorslot {

inline uint32_t crc32(const uint8_t* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return ~c;
}

enum Result { OK = 0, MISSING, EMPTY, BAD_SIZE, BAD_CRC };

// path "" -> OK with out empty (clear). expectCrc: CRC-32/IEEE of the whole file when hasCrc.
inline Result loadAud(const std::string& path, bool hasCrc, uint32_t expectCrc, std::vector<float>& out, uint32_t rate = 48000, uint32_t ch = 1) {
    out.clear();
    if (path.empty()) return OK;
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return MISSING;
    std::vector<uint8_t> b; uint8_t tmp[8192]; size_t k;
    while ((k = fread(tmp, 1, sizeof tmp, f)) > 0) b.insert(b.end(), tmp, tmp + k);
    fclose(f);
    if (b.empty()) return EMPTY;
    if (b.size() & 1) return BAD_SIZE;
    if (hasCrc && crc32(b.data(), b.size()) != expectCrc) return BAD_CRC;
    if (ch < 1) ch = 1;
    size_t frames = b.size() / 2 / ch; if (frames == 0) return EMPTY;
    std::vector<float> src(frames);                                   // mono (channels averaged; firmware samples are always mono)
    for (size_t i = 0; i < frames; i++) { float a = 0; for (uint32_t c = 0; c < ch; c++) { size_t j = (i * ch + c) * 2; a += (float)(int16_t)(uint16_t)(b[j] | (b[j + 1] << 8)) / 32767.f; } src[i] = a / (float)ch; }
    if (rate == 0 || rate == 48000) { out = std::move(src); }
    else {                                                            // linear resample to the core's 48 kHz
        size_t m = (size_t)((double)frames * 48000.0 / rate); out.resize(m);
        for (size_t i = 0; i < m; i++) { double x = (double)i * rate / 48000.0; size_t i0 = (size_t)x, i1 = i0 + 1 < frames ? i0 + 1 : i0; float fr = (float)(x - i0); out[i] = src[i0] * (1.f - fr) + src[i1] * fr; }
    }
    if (out.size() > SMP_LEN) out.resize(SMP_LEN);
    return OK;
}

struct Mailbox {
    struct Load { std::vector<float> pcm; };
    std::atomic<Load*> pend[SMP_SLOTS] {};
    std::atomic<Load*> retire[SMP_SLOTS] {};   // applied loads, freed by the next message-thread post (never on the audio thread)
    std::atomic<uint32_t> any {0};             // bitmask of slots with something parked: one relaxed load per block when idle

    ~Mailbox() { for (int s = 0; s < SMP_SLOTS; s++) { delete pend[s].exchange(nullptr); delete retire[s].exchange(nullptr); } }

    // message thread
    Result post(int slot, const std::string& path, bool hasCrc = false, uint32_t crc = 0, uint32_t rate = 48000, uint32_t ch = 1) {
        if (slot < 0 || slot >= SMP_SLOTS) return MISSING;
        delete retire[slot].exchange(nullptr);
        Load* l = new Load; Result r = loadAud(path, hasCrc, crc, l->pcm, rate, ch);
        if (r != OK) l->pcm.clear();            // any failure = silent slot
        delete pend[slot].exchange(l);          // an unconsumed older post is replaced
        any.fetch_or(1u << slot, std::memory_order_release);
        return r;
    }
    // audio thread (call before rendering a block)
    void apply() {
        uint32_t m = any.load(std::memory_order_acquire); if (!m) return;
        any.fetch_and(~m, std::memory_order_acq_rel);
        for (int s = 0; s < SMP_SLOTS; s++) {
            if (!(m & (1u << s))) continue;
            Load* l = pend[s].exchange(nullptr); if (!l) continue;
            smp.len[s] = 0;                                     // silent while the buffer is rewritten
            size_t n = l->pcm.size();
            if (n) memcpy(smp_buf[s], l->pcm.data(), n * sizeof(float));
            smp.len[s] = (uint32_t)n;
            if (retire[s].exchange(l)) { /* a second apply before the message thread ran: previous one leaks (192 KB, practically never) */ }
        }
    }
};

}  // namespace pmorslot
