// pmor_slotsync.h -- sampler slot -> sample file mapping for pmor.core~ (used by pmor.proto, tested by tests/test_slotsync.cpp without Max).
// Rule: the core plays exactly the sample PMOR has in that slot. PMOR is the only truth: slot -> id comes from PATCHINFO ids= / legacy patch*16+slot
// (Client::slotSample), id -> length/crc from the manifest. There is no host-side override.
//   pool id (>= 1024): cache file S<id>_<crc8>.aud, crc = PMOR's manifest CRC-32 of the PCM. push() verifies the file's content CRC-32 against it BEFORE the
//     `slot` goes out; mismatch/missing -> file quarantined (.bad), the slot is sent EMPTY (refused), the id is re-pulled once (per manifest epoch), re-pushed when done.
//     A pulled stream whose CRC-32 differs from the manifest crc is refused as well (never stored).
//   legacy id (< 1024): manifest `ver` is 0 / a timestamp = untrusted. Every manifest epoch re-pulls it; the file is named by the CRC-32 the host computed of the
//     downloaded bytes and is only used within that epoch (a stale same-size file on disk is never reused).
//   epoch = any manifest reload (full sync, PATCH_CHANGED, SAMPLE_READY, storage retry). Not READY -> every slot is blanked.
#pragma once
#include <stdint.h>
#include <map>
#include <set>
#include <functional>
#include <string>
#include <vector>
#include "pmor_host.h"
#include "pmor_cache.h"

namespace pmorslots {

class Sync {
public:
    Sync(pmorhost::Client& c, pmorcache::Cache& cache) : c_(c), cache_(cache) {}
    // path empty = `slot <n>` (silence). hasCrc/crc = what pmor.core~ re-checks on load.
    std::function<void(int slot, const std::string& path, bool hasCrc, uint32_t crc, uint32_t rate, uint32_t ch)> emit;
    std::function<void(const std::string&)> info;

    void manifestReloaded() { legacy_.clear(); requested_.clear(); bad_.clear(); }      // new epoch: legacy ids are re-pulled, pool ids re-verified
    void blankAll() { for (int s = 0; s < 16; s++) if (emit) emit(s, std::string(), false, 0, 0, 0); }
    // pull gate (Client::haveSample): true = no pull needed
    bool have(const pmorhost::Client::Sample& sm) {
        if (sm.id >= 1024) return !findPool_(sm).empty();
        auto it = legacy_.find(sm.id); return it != legacy_.end() && it->second.len == sm.len;
    }
    // Client::onSampleDone; true = stored and usable
    bool done(const pmorhost::Client::Sample& sm, const std::vector<uint8_t>& data, bool ok) {
        if (!ok || data.size() != (size_t)sm.len * 2) { bad_.insert(sm.id); return false; }
        uint32_t crc = pmorcache::crc32(data.data(), data.size());
        if (sm.id >= 1024) {
            if (crc != sm.crc) { bad_.insert(sm.id); char b[96]; snprintf(b, sizeof b, "sample S%d: pulled data CRC %08X != manifest %08X, refused", sm.id, (unsigned)crc, (unsigned)sm.crc); if (info) info(b); return false; }
            if (cache_.store(pmorcache::Cache::poolName(sm.id), sm.crc, data).empty()) { bad_.insert(sm.id); return false; }
            bad_.erase(sm.id); return true;
        }
        std::string f = cache_.store(sm.name, crc, data); if (f.empty()) { bad_.insert(sm.id); return false; }
        legacy_[sm.id] = {f, crc, sm.len}; bad_.erase(sm.id); return true;
    }
    // listing state without reading files: 0 not cached, 1 cached, 2 bad (refused / quarantined this epoch)
    int state(const pmorhost::Client::Sample& sm) const {
        if (bad_.count(sm.id)) return 2;
        if (sm.id >= 1024) return cache_.find(pmorcache::Cache::poolName(sm.id), sm.crc, sm.len, false).empty() ? 0 : 1;
        auto it = legacy_.find(sm.id); return it != legacy_.end() && it->second.len == sm.len ? 1 : 0;
    }
    void push() {
        if (c_.state() != pmorhost::Client::READY || !c_.slotsOk()) { blankAll(); return; }
        for (int sl = 0; sl < 16; sl++) {
            pmorhost::Client::Sample sm; std::string f; bool hasCrc = false; uint32_t crc = 0;
            if (c_.slotSample(sl, sm)) {
                if (sm.id >= 1024) {
                    f = findPool_(sm); hasCrc = true; crc = sm.crc;
                } else {
                    auto it = legacy_.find(sm.id);
                    if (it != legacy_.end() && it->second.len == sm.len && pmorcache::Cache::sizeIs(it->second.path, (uintmax_t)sm.len * 2)) {
                        f = it->second.path; hasCrc = true; crc = it->second.crc;
                        std::string g = cache_.intoSession(f, crc); if (!g.empty()) { it->second.path = g; f = g; }      // pulled while the set was unsaved: copy into <set>/PMOR once the folder is known
                    }
                }
                if (f.empty() && requested_.insert(sm.id).second) c_.pullId(sm.id);      // missing / refused: one re-pull per epoch, push() runs again when it is stored
            }
            if (f.empty()) { if (emit) emit(sl, std::string(), false, 0, 0, 0); continue; }
            if (emit) emit(sl, f, hasCrc, crc, sm.rate ? sm.rate : 48000, sm.ch ? sm.ch : 1);
        }
    }
private:
    std::string findPool_(const pmorhost::Client::Sample& sm) {      // verified lookup; files that failed their CRC are reported, and the id is `bad` until a good copy is stored
        std::string f = cache_.find(pmorcache::Cache::poolName(sm.id), sm.crc, sm.len);
        bool q = !cache_.quarantined.empty();
        for (auto& x : cache_.quarantined) if (info) info("cache file failed its CRC, quarantined: " + x);
        cache_.quarantined.clear(); if (q && f.empty()) bad_.insert(sm.id);
        return f;
    }
    struct Leg { std::string path; uint32_t crc; uint32_t len; };
    pmorhost::Client& c_; pmorcache::Cache& cache_;
    std::map<int, Leg> legacy_; std::set<int> requested_, bad_;
};

}  // namespace pmorslots
