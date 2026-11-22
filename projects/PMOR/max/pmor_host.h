// pmor_host.h -- host side of docs/PROTOCOL.md as a non-blocking client. No Max, no OS, no port I/O:
// the owner pipes bytes in (feed) and out (send), and calls tick(ms) regularly. Used by the pmor.proto
// Max object and by tests/test_hostclient.cpp (against the simulated PMOR).
//   connect(STUDIO|EDIT) -> HELLO -> GET_SCHEMA (hash checked) -> GET_PARAMS -> GET_SEQ 0..2 -> GET_MANIFEST -> synced
//   device P pushes -> onParam (never turned into SET_PARAM: loop rule), PATCH_CHANGED -> GET_PATCH, SEQ_CHANGED -> GET_SEQ
//   setParam(): SET_PARAM to the device; the stored value comes back via onParam
//   pool browser: pool() lists the manifest (pool ids >= 1024 = global samples, < 1024 = legacy patch*16+slot), pullId(), assign() (EDIT)
//   preset browser: pullPresets() reads PRESETS/<engine#0..11>.TXT through PULL_FILE R, applyPreset() = SET_PARAMs (PMOR stays the truth)
//   slot map: PMOR is the only truth (PATCHINFO ids= + manifest). slotsOk() is false from any non-READY state / PATCH_CHANGED / SAMPLE_READY until GET_PATCH + GET_MANIFEST
//   were both re-read; slotSample() answers nothing meanwhile (the owner blanks the slots). There is no host-side slot override.
//   recycle bin (STUB protocol, firmware pending): deleteSamples / binList / binRestore / binEmpty, see the marked block below
//   storage: STATUS poll (store=state/.../errors), storeRetry() = STORE_RETRY, then manifest + presets refresh when the job is done
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <math.h>
#include <ctype.h>
#include <functional>
#include "proto_util.h"

namespace pmorhost {

class Client {
public:
    enum State { DISCONNECTED, HELLO, SYNCING, READY };
    std::function<void(const std::string&)>   send;                // raw bytes to the CDC port
    std::function<void(uint16_t, float)>      onParam;             // device value for a schema id (get / push / set ack)
    std::function<void(const std::string&)>   onInfo;              // human-readable events and errors
    std::function<void()>                     onSynced;            // full resync (or a GET_PATCH) finished
    // --- sample cache (PROTOCOL section 8). A manifest entry is "M <id> <len_samples> <rate> <ch> <crc32hex> <name>".
    struct Sample { int id = 0; uint32_t len = 0, rate = 0, ch = 0, crc = 0 /* manifest token: cache key */; std::string name; };
    std::function<bool(const Sample&)>        haveSample;          // true = already cached (session/cache), skip the pull
    std::function<void(const Sample&, uint32_t done, uint32_t total, int queueLeft)> onSampleProgress;
    std::function<void(const Sample&, const std::vector<uint8_t>&, bool ok)>        onSampleDone;   // int16 LE mono bytes
    std::function<void()>                     onPool;              // manifest (re)loaded: pool() is current
    std::function<void(int slot, bool ok, const std::string& code)> onAssigned;   // ASSIGN_SAMPLE finished (SAMPLE_READY / SAMPLE_ERR / refusal)
    std::function<void()>                     onPresets;           // every preset file pulled + parsed: presets is current
    std::function<void(int state, int pending, int errors, int pool)> onStore;     // STATUS store=state/pending/exported/written/verified/skipped/errors pool=
    std::function<void(const std::map<std::string, std::string>&)> onStatus;       // every STATUS poll (mode clk running bpm cpu audio_overruns midi_drops ...)
    std::function<void(int slot)>             onPatch;             // current patch slot (sync, PATCH_CHANGED)
    std::function<void(int slot)>             onPatchSwitched;     // PMOR changed patch: whole patch pulled (GET_PATCH) and the manifest reloaded; the owner starts the sample pulls
    std::function<void(const std::string& mode)> onMode;          // session mode changed (switchMode)
    std::function<void(bool ok, int spp16, const std::string& tickAtOrCode)> onJoin;   // JOIN reply: ok -> tick_at, else the ERR code (noclock, mode, ...)
    bool                                      autoReconnect = true;   // after a lost link (or a failed connect) keep sending HELLO every 2 s until bye()/portClosed()
    // preset file entries (docs/STORAGE.md): engine 0..3 = drum groups KICK SNARE HAT PERC, 4..11 = synth algorithm 0..7; kind 'f' factory, 'u' user
    struct Preset { int engine = 0, n = 0; char kind = 'f'; std::string name; std::vector<uint8_t> blob; };
    std::vector<Preset>                       presets;
    // PMOR's own schema (GET_SCHEMA), the authority for "what exists": the device matches params BY ID. Unknown ids from PMOR are mirrored and passed on
    // (devices ignore what they have no control for), ids the device knows but PMOR lacks are reported through onSchema so the UI can disable them.
    // Compatibility rule (EMGOR): never refuse to connect over schema / firmware / proto differences; unknown fields are ignored, missing fields defaulted.
    struct SchemaEntry { int id = 0; std::string name, group; float min = 0, max = 1, def = 0; int flags = 0; };
    std::map<uint16_t, SchemaEntry>           schema;
    std::function<void()>                     onSchema;            // schema (re)loaded: `schema` is current (may be empty on firmware without GET_SCHEMA)
    bool hasParam(uint16_t id) const { return schema.empty() || schema.count(id) != 0; }     // empty schema = unknown firmware: do not block anything
    std::map<uint16_t, float>                 params;              // mirror of the device (PMOR is the source of truth)
    std::map<int, std::string>                seq;                 // engine -> "SEQ ..." line
    std::vector<std::string>                  manifest;            // "M ..." lines
    std::map<std::string, std::string>        hello;               // fw proto mode schema patch clk sr block
    std::string                               schemaHash;          // recomputed from GET_SCHEMA
    size_t                                    schemaCount = 0;
    int                                       nakCount = 0, badFrames = 0;

    State state() const { return st_; }
    const char* stateName() const { static const char* n[] = {"disconnected", "hello", "syncing", "ready"}; return n[st_]; }

    void connect(const std::string& mode /* STUDIO | EDIT | NEPTR */) {
        reset_(); st_ = HELLO; mode_ = mode; wanted_ = true; lastTry_ = now_; helloReq_();
    }
    // PMOR answers HELLO at once with `booting=1` while its slow storage boot runs (every other command: `ERR <cmd> booting`): wait and HELLO again every ~1 s, never fail
    void helloReq_() {
        request_("HELLO " + mode_ + " 1", [this](const std::vector<std::string>& r) {
            if (st_ != HELLO) return;
            bool boot = !r.empty() && ((ok_(r) && kv_(r.back())["booting"] == "1") || (r.back().rfind("ERR ", 0) == 0 && r.back().find(" booting") != std::string::npos));
            if (boot) { info_("PMOR is booting, waiting"); later_(1000, [this]() { if (st_ == HELLO && wanted_) helloReq_(); }); return; }
            if (!ok_(r)) { fail_("HELLO refused: " + last_(r)); return; }
            hello = kv_(r.back());
            info_("hello: fw=" + hello["fw"] + " mode=" + hello["mode"] + " schema=" + hello["schema"] + " patch=" + hello["patch"] + " clk=" + hello["clk"]);
            st_ = SYNCING; fullSync_();
        });
    }
    // TEMPO <bpm_milli> (Live's tempo -> PMOR's clock): valid 40000..400000, else ignored silently. Sent only while READY and resent after every (re)sync; an old firmware's
    // `ERR TEMPO unknown|arg|mode` is ignored (no state change, no info).
    void setTempo(double bpm) {
        long m = (long)(bpm * 1000.0 + 0.5); if (!(bpm == bpm) || m < 40000 || m > 400000) return;
        if (m == tempoMilli_ && tempoSent_) return;
        tempoMilli_ = m; sendTempo_();
    }
    long tempoMilli() const { return tempoMilli_; }
    void bye() { wanted_ = false; if (st_ == DISCONNECTED) return; request_("BYE", [this](const std::vector<std::string>&) { st_ = DISCONNECTED; }); }
    void portClosed() { wanted_ = false; reset_(); st_ = DISCONNECTED; }      // DTR low / unplug: the device is back in STANDALONE
    void setParam(uint16_t id, float v) {
        noteEdit_(id, v);                                    // the set's own copy of the current patch (see "host snapshot"): kept while unplugged, synced on reconnect
        if (st_ != READY && st_ != SYNCING) { if (!(eligible(id) && snap.valid)) info_("setparam ignored: not connected"); else if (!offlineNote_) { offlineNote_ = true; info_("not connected: edits are kept in the set and synced to PMOR on reconnect"); } return; }
        if (!hasParam(id)) { info_("setparam " + std::to_string(id) + " skipped: not on this firmware"); return; }
        char b[48]; snprintf(b, sizeof b, "SET_PARAM %u %.6f", (unsigned)id, v);
        request_(b, [this, id](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("SET_PARAM " + std::to_string(id) + " refused: " + last_(r)); return; }
            auto m = kv_(r.back()); auto it = m.find("val");
            if (it != m.end()) { float f = (float)atof(it->second.c_str()); params[id] = f; ackSnap_(id, f); if (onParam) onParam(id, f); }
        });
    }
    // Queue background pulls: current = only the patch the device is on (STUDIO), all = every manifest entry (EDIT).
    void pullSamples(bool all) {
        pullQ_.clear();
        if (!all && curIds_.empty() && poolIds_()) {
            pulling_ = false; pullGen_++;           // pool ids: which ids belong to the current patch comes from PATCHINFO ids= (GET_PATCH)
            request_("GET_PATCH", [this](const std::vector<std::string>& r) { if (ok_(r)) { absorb_(r); queuePulls_(false); } else info_("GET_PATCH failed: " + last_(r)); });
            return;
        }
        queuePulls_(all);
    }
    bool poolIds_() const { for (auto& l : manifest) { Sample sm; if (parseM_(l, sm) && sm.id >= 1024) return true; } return false; }
    void queuePulls_(bool all) {
        int slot = atoi(hello["patch"].c_str()); bool keep = false;
        for (auto& l : manifest) {
            Sample sm; if (!parseM_(l, sm)) continue; if (!all && !inCurrent_(sm.id, slot)) continue;
            if (pulling_ && !keep && sm.id == cur_.id && sm.len == cur_.len && sm.crc == cur_.crc) { keep = true; continue; }      // the pull in flight is still wanted: let it finish, do not restart it
            pullQ_.push_back(sm);
        }
        if (pulling_ && !keep) { pulling_ = false; pullGen_++; }
        pullNext_();
    }
    void cancelPulls() { pullQ_.clear(); pulling_ = false; pullGen_++; }
    size_t pullsQueued() const { return pullQ_.size() + (pulling_ ? 1 : 0); }

    // ---- pool browser
    // the pool/manifest sample behind sampler slot `slot` of the CURRENT patch (PATCHINFO ids=, or the legacy patch*16+slot id); false = empty slot / not in the manifest
    bool slotsOk() const { return slotsOk_; }
    const std::vector<int>& curIds() const { return curIds_; }
    // current-patch sampler slots that use sample `id` (PATCHINFO ids=, or the legacy patch*16+slot rule); empty when unknown / not used. The UI warns before a delete.
    std::vector<int> usedBy(int id) const {
        std::vector<int> v; if (!slotsOk_ || id <= 0) return v;
        if (!curIds_.empty()) { for (size_t i = 0; i < curIds_.size() && i < 16; i++) if (curIds_[i] == id) v.push_back((int)i); }
        else if (id < 1024 && id / 16 == atoi(hello.count("patch") ? hello.at("patch").c_str() : "-1")) v.push_back(id % 16);
        return v;
    }
    bool slotSample(int slot, Sample& out) const {
        if (slot < 0 || slot > 15 || !slotsOk_) return false;
        int id = -1;
        if (!curIds_.empty()) { if ((size_t)slot < curIds_.size()) id = curIds_[slot]; if (id <= 0) return false; }
        else id = atoi(hello.count("patch") ? hello.at("patch").c_str() : "0") * 16 + slot;
        for (auto& l : manifest) { Sample sm; if (parseM_(l, sm) && sm.id == id) { out = sm; return sm.len > 0; } }
        return false;
    }
    std::vector<Sample> pool() const { std::vector<Sample> v; for (auto& l : manifest) { Sample sm; if (parseM_(l, sm)) v.push_back(sm); } return v; }
    void refreshPool() { request_("GET_MANIFEST", [this](const std::vector<std::string>& r) {
        if (!ok_(r)) { info_("GET_MANIFEST failed: " + last_(r)); return; }
        manifest.clear(); absorb_(r); if (onPool) onPool(); }); }
    void pullId(int id) {          // queue one sample (pool id or legacy id) at the front
        if (pulling_ && cur_.id == id) return; for (auto& q : pullQ_) if (q.id == id) return;
        for (auto& l : manifest) { Sample sm; if (parseM_(l, sm) && sm.id == id) { pullQ_.push_front(sm); pullNext_(); return; } }
        info_("pull: unknown sample id " + std::to_string(id));
    }
    // ASSIGN_SAMPLE needs HELLO EDIT (STUDIO gets ERR mode): from STUDIO the client switches to EDIT for the assign (HELLO again = mode change,
    // nothing re-pushed), and returns to the previous mode when SAMPLE_READY / SAMPLE_ERR / a refusal arrives. Copies pool sample <id> into the
    // current patch's sampler slot; completion is the SAMPLE_READY / SAMPLE_ERR push.
    void assign(int slot, int id) {
        if (st_ != READY) { info_("assign ignored: not connected"); return; }
        if (slot < 0 || slot > 15) { fireAssigned_(slot, false, "arg"); return; }
        if (hello["mode"] == "EDIT") { doAssign_(slot, id); return; }
        std::string prev = hello["mode"];
        abortInflight_(); request_("HELLO EDIT 1", [this, slot, id, prev](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("assign: could not switch to EDIT: " + last_(r)); fireAssigned_(slot, false, "mode"); return; }
            hello["mode"] = "EDIT"; mode_ = "EDIT"; assignRestore_ = prev; info_("mode EDIT for sample assign");
            doAssign_(slot, id);
        });
    }

    // ======================================================================================================================================
    // RECYCLE-BIN STUB PROTOCOL -- ASSUMED, NOT IN docs/PROTOCOL.md (twink adds the firmware side; align this block with it when it lands).
    //   DELETE_POOL <id>      (EDIT)  -> `OK DELETE_POOL id=<id>` | `ERR DELETE_POOL <code>`   moves pool sample <id> to the bin (never a hard delete)
    //   BIN_LIST              (STUDIO/EDIT, multi-line) -> `B <id> <len_samples> <name> <deleted_age_s_or_ts> <boots_left>` per entry, then `OK BIN_LIST n=<count>`
    //   BIN_RESTORE <id>      (EDIT)  -> `OK BIN_RESTORE id=<id>` | `ERR BIN_RESTORE <code>`
    //   BIN_EMPTY YES         (EDIT)  -> `OK BIN_EMPTY n=<count>` | `ERR BIN_EMPTY <code>`   permanent; only ever sent from an explicit binEmpty() call
    // Firmware without them answers `ERR <CMD> unknown`: the client reports "firmware has no recycle bin yet" (info + onNoBin), does NOTHING destructive, attempts
    // no further ids of the batch, and stops trying until the next connect. Assumption: an unknown pool id is answered `range`/`arg`, never `unknown`.
    struct BinEntry { int id = 0; uint32_t len = 0; std::string name, age; int boots = 0; };      // age: raw token (seconds since deletion, or a timestamp: the UI decides)
    std::function<void(int id, bool ok, const std::string& code)> onDeleted;
    std::function<void(const std::vector<BinEntry>&)>             onBinList;
    std::function<void(int id, bool ok, const std::string& code)> onBinRestored;
    std::function<void(bool ok, const std::string& nOrCode)>      onBinEmptied;
    std::function<void()>                                         onNoBin;
    bool binUnsupported() const { return noBin_; }
    // EDIT for the delete (like assign), one DELETE_POOL per id in order, then back to the previous mode and a GET_PATCH + GET_MANIFEST refresh
    void deleteSamples(std::vector<int> ids) {
        if (st_ != READY) { info_("delete ignored: not connected"); return; }
        if (ids.empty()) return;
        if (noBin_) { info_("firmware has no recycle bin yet"); if (onNoBin) onNoBin(); return; }
        withEdit_([this, ids]() { delIds_ = ids; delIdx_ = 0; delDone_ = 0; delNext_(); }, [this, ids]() { for (int id : ids) if (onDeleted) onDeleted(id, false, "mode"); });
    }
    void binList() {
        if (st_ != READY) { info_("bin ignored: not connected"); return; }
        if (noBin_) { info_("firmware has no recycle bin yet"); if (onNoBin) onNoBin(); return; }
        request_("BIN_LIST", [this](const std::vector<std::string>& r) {
            if (!ok_(r)) { if (errCode_(r) == "unknown") noBinSeen_(); else info_("BIN_LIST failed: " + last_(r)); return; }
            std::vector<BinEntry> v;
            for (auto& l : r) if (l.rfind("B ", 0) == 0) { unsigned id, len; char name[64] = {0}, age[40] = {0}; int boots = 0;
                if (sscanf(l.c_str(), "B %u %u %63s %39s %d", &id, &len, name, age, &boots) >= 4) { BinEntry b; b.id = (int)id; b.len = len; b.name = name; b.age = age; b.boots = boots; v.push_back(b); } }
            if (onBinList) onBinList(v); });
    }
    void binRestore(int id) {
        if (st_ != READY) { info_("bin restore ignored: not connected"); return; }
        if (noBin_) { info_("firmware has no recycle bin yet"); if (onNoBin) onNoBin(); return; }
        withEdit_([this, id]() {
            request_("BIN_RESTORE " + std::to_string(id), [this, id](const std::vector<std::string>& r) {
                restoreMode_();
                if (ok_(r)) { if (onBinRestored) onBinRestored(id, true, ""); refreshSlots_([this](bool ok) { if (ok && onPool) onPool(); if (onSynced) onSynced(); }); return; }
                if (errCode_(r) == "unknown") { noBinSeen_(); return; }
                info_("BIN_RESTORE " + std::to_string(id) + " refused: " + last_(r)); if (onBinRestored) onBinRestored(id, false, errCode_(r)); }); },
            [this, id]() { if (onBinRestored) onBinRestored(id, false, "mode"); });
    }
    void binEmpty() {            // permanent: the UI must have confirmed (hold) before calling this
        if (st_ != READY) { info_("bin empty ignored: not connected"); return; }
        if (noBin_) { info_("firmware has no recycle bin yet"); if (onNoBin) onNoBin(); return; }
        withEdit_([this]() {
            request_("BIN_EMPTY YES", [this](const std::vector<std::string>& r) {
                restoreMode_();
                if (ok_(r)) { auto m = kv_(r.back()); if (onBinEmptied) onBinEmptied(true, m.count("n") ? m["n"] : "0"); return; }
                if (errCode_(r) == "unknown") { noBinSeen_(); return; }
                info_("BIN_EMPTY refused: " + last_(r)); if (onBinEmptied) onBinEmptied(false, errCode_(r)); }); },
            [this]() { if (onBinEmptied) onBinEmptied(false, "mode"); });
    }
    // ======================================================================================================================================

    // ---- generic PULL_FILE (kinds P K A L M T R W); data arrives verified chunk by chunk, resume = first unverified offset
    void pullFile(char kind, int slot, std::function<void(const std::vector<uint8_t>&, bool ok)> done) {
        FileJob j; j.kind = kind; j.slot = slot; j.done = std::move(done); fileQ_.push_back(std::move(j)); fileNext_();
    }
    // ---- preset browser
    void loadPresetFile(int engine, const std::vector<uint8_t>& d) { parsePresetFile_(engine, d); }     // parse one PRESETS/<E>.TXT (also used by tests with real firmware-written files)
    void pullPresets() {
        if (st_ != READY) { info_("presets ignored: not connected"); return; }
        presets.clear(); presetsLeft_ = 12;
        for (int e = 0; e < 12; e++)
            pullFile('R', e, [this, e](const std::vector<uint8_t>& d, bool ok) {
                if (ok) parsePresetFile_(e, d);               // a missing file (no SD / old card) is just "no presets for this engine"
                if (--presetsLeft_ == 0 && onPresets) onPresets();
            });
    }
    // apply = SET_PARAM per field; PMOR clamps and stays the source of truth. engine 0..3 drums need a pad (0..15), 4..11 = synth algo.
    bool applyPreset(int engine, int n, int pad = -1) {
        if (st_ != READY) { info_("preset ignored: not connected"); return false; }
        const Preset* pr = nullptr; for (auto& p : presets) if (p.engine == engine && p.n == n) { pr = &p; break; }
        if (!pr) { info_("preset " + std::to_string(engine) + "/" + std::to_string(n) + " not loaded"); return false; }
        const uint16_t SYN = 1u << 10, DRM = 2u << 10;
        if (engine >= 4 && engine <= 11) {
            if (pr->blob.size() < 4) { info_("preset " + pr->name + ": empty blob"); return false; }
            int algo = engine - 4; int nf = (int)(pr->blob.size() / sizeof(float)); if (nf > 20) nf = 20;      // older firmware: fewer fields (apply what exists); newer: extra fields ignored
            if (nf < 20) info_("preset " + pr->name + ": " + std::to_string(nf) + " of 20 fields (older SynPreset)");
            float f[20] = {0}; memcpy(f, pr->blob.data(), (size_t)nf * sizeof(float));
            setParam((uint16_t)(SYN | 256), (float)algo);
            for (int k = 0; k < nf; k++) setParam((uint16_t)(SYN | (algo * 32 + k)), f[k]);       // k order == schema order == SynPreset field order
            return true;
        }
        if (engine >= 0 && engine <= 3) {
            if (pad < 0 || pad > 15) { info_("drum preset needs a pad 0..15"); return false; }
            if (pr->kind == 'f') { setParam((uint16_t)(DRM | (pad * 8 + 5)), (float)n); return true; }          // factory: the sound index of the group
            if (pr->blob.size() < 20) { info_("drum user preset " + pr->name + ": blob " + std::to_string(pr->blob.size()) + " B, need 20 (DrumUser)"); return false; }
            float k[4]; memcpy(k, pr->blob.data() + 4, sizeof k);
            setParam((uint16_t)(DRM | (pad * 8 + 5)), (float)pr->blob[0]);
            for (int i = 0; i < 4; i++) setParam((uint16_t)(DRM | (272 + engine * 8 + i)), k[i]);
            return true;
        }
        return false;
    }

    // ---- save as user preset (firmware command, EDIT only)
    //   SAVE_PRESET <engine#0..11> <n> <name<=9 chars>   engine 0..3 = KICK SNARE HAT PERC (drums: n 0..31, saves the CURRENT pad's sound), 4..11 = SYN0..SYN7 (synth: n 5..31;
    //   0..4 are factory and are never sent). Replies: `OK ...` (after verify + PRESETS file version bump) | `ERR nosave` / `ERR SAVE_PRESET nosave` (NOSAVE: saving disabled) |
    //   `ERR SAVE_PRESET <code>` | `ERR unknown` (older firmware: remembered, later calls refuse locally). After OK that engine's PRESETS file is re-pulled (presets updated, onPresets fires).
    // Result: onPresetSaved(engine, n, ok, code) with code "" | nosave | unsupported | factory | range | name | mode | <firmware code>.
    std::function<void(int engine, int n, bool ok, const std::string& code)> onPresetSaved;
    bool saveUnsupported() const { return noSave_; }
    // A..Z 0..9 space - only (lower case is upper-cased, everything else dropped), trimmed, <= 9 chars
    static std::string sanitizePresetName(const std::string& in) {
        std::string o;
        for (char c : in) { if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A'); if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-') o += c; }
        size_t a = o.find_first_not_of(' '); if (a == std::string::npos) return ""; o = o.substr(a);
        if (o.size() > 9) o.resize(9);
        size_t b = o.find_last_not_of(' '); return o.substr(0, b + 1);
    }
    void savePreset(int engine, int n, const std::string& rawName) {
        auto fail = [this, engine, n](const std::string& code) { info_("SAVE_PRESET not sent: " + code); if (onPresetSaved) onPresetSaved(engine, n, false, code); };
        if (st_ != READY) { info_("save ignored: not connected"); return; }
        if (engine < 0 || engine > 11) { fail("range"); return; }
        if (engine >= 4 && n >= 0 && n < 5) { fail("factory"); return; }      // synth 0..4 are factory: never sent, whatever the UI did
        if (n < (engine >= 4 ? 5 : 0) || n > 31) { fail("range"); return; }
        std::string name = sanitizePresetName(rawName); if (name.empty()) { fail("name"); return; }
        if (noSave_) { fail("unsupported"); return; }
        withEdit_([this, engine, n, name]() {
            request_("SAVE_PRESET " + std::to_string(engine) + " " + std::to_string(n) + " " + name, [this, engine, n](const std::vector<std::string>& r) {
                restoreMode_();
                if (ok_(r)) { info_("preset saved " + std::to_string(engine) + "/" + std::to_string(n)); repullPresetEngine_(engine, n); return; }
                std::string c = saveErr_(r);
                if (c == "unknown") { noSave_ = true; info_("save not supported by this PMOR firmware"); c = "unsupported"; }
                else info_("SAVE_PRESET refused: " + last_(r));
                if (onPresetSaved) onPresetSaved(engine, n, false, c); }); },
            [this, engine, n]() { if (onPresetSaved) onPresetSaved(engine, n, false, "mode"); });
    }

    // ---- storage
    // STORE_RETRY: clear the sticky skip flag and rerun the storage jobs; then poll STATUS until the job settles and reload pool + presets
    void storeRetry() {
        if (st_ != READY) { info_("storeretry ignored: not connected"); return; }
        request_("STORE_RETRY", [this](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("STORE_RETRY refused: " + last_(r)); return; }
            info_("storage retry queued"); storeWatch_ = true; storeWatchLeft_ = 60; pollStatus();
        });
    }
    void pollStatus() {
        if (st_ != READY || statusPending_) return;
        statusPending_ = true;
        request_("STATUS", [this](const std::vector<std::string>& r) {
            statusPending_ = false; if (!ok_(r)) return;
            auto m = kv_(r.back());
            // clock policy: HELLO/STATUS `clk=` reads `follow` whenever mode=STUDIO regardless of the real role; `cm=<setting>/<policy>` is the policy
            // (0 AUTO = master, follows only while a bus clock arrives, back to master ~2 s after it stops; 1 INTERNAL; 2 FOLLOW; 3 MASTER).
            { static const char* const pn[4] = {"AUTO", "INTERNAL", "FOLLOW", "MASTER"}; unsigned a = 0, b = 0;
              if (m.count("cm") && sscanf(m["cm"].c_str(), "%u/%u", &a, &b) == 2) m["policy"] = pn[b & 3];
              unsigned irq = 0; if (m.count("rx") && sscanf(m["rx"].c_str(), "%u", &irq) == 1) { m["bus"] = (haveRx_ && irq != lastRxIrq_) ? "1" : "0"; lastRxIrq_ = irq; haveRx_ = true; }
              // real clock ROLE: prefer the firmware's role=<master|follow> (additive STATUS field); else infer it: FOLLOW policy follows, AUTO follows only while a bus clock arrives
              if (!m.count("role") || (m["role"] != "master" && m["role"] != "follow")) {
                  std::string pol = m.count("policy") ? m["policy"] : "";
                  m["role"] = (pol == "FOLLOW" || (pol != "MASTER" && pol != "INTERNAL" && m.count("bus") && m["bus"] == "1")) ? "follow" : "master"; } }
            if (onStatus) onStatus(m);
            auto it = m.find("store");
            if (it != m.end()) {
                int a[7] = {0, 0, 0, 0, 0, 0, 0}; sscanf(it->second.c_str(), "%d/%d/%d/%d/%d/%d/%d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);
                int pool = m.count("pool") ? atoi(m["pool"].c_str()) : 0;
                if (a[0] != lastStore_) { if (a[0] == 3) info_("storage finished with errors (" + std::to_string(a[6]) + "): send storeretry"); lastStore_ = a[0]; }
                if (onStore) onStore(a[0], a[1], a[6], pool);
                bool settled = a[0] == 2 || a[0] == 3;
                if (storeWatch_ && settled) { storeWatch_ = false; refreshPool(); pullPresets(); }
                if (!settled && a[0] == 1) storeWatchSeen_ = true;
                if (settled && storeWatchSeen_ && !storeWatch_) { storeWatchSeen_ = false; refreshPool(); }   // a migration finished by itself: the pool is now >= 1024
            }
        });
    }
    // HELLO again = mode change (nothing is re-pushed): perform = STUDIO, editor open = EDIT
    void switchMode(const std::string& m) {
        if (st_ != READY || m == hello["mode"]) return;
        abortInflight_(); request_("HELLO " + m + " 1", [this, m](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("mode " + m + " refused: " + last_(r)); return; }
            hello["mode"] = m; mode_ = m; assignRestore_.clear(); if (onMode) onMode(m); });
    }
    // JOIN <spp16> (STUDIO/EDIT): "the F8 nearest this request's arrival is tick 0 of 16th spp16". Send it at (not before) a 16th boundary of the host transport.
    // Reply `OK JOIN spp16= tick_at=` or `ERR JOIN noclock`. No retry: a late JOIN is wrong, the owner decides when to send the next one.
    // joinPrep(): call one 16th BEFORE join(). JOIN is a device "job" and answers `ERR JOIN busy` while a bulk reply (sample / file chunks) runs, and a late JOIN
    // is wrong (it snaps to the nearest tick), so new bulk chunk requests are held back until the JOIN reply (or 1.5 s) and the JOIN goes out immediately.
    void joinPrep() { joinHoldUntil_ = now_ + 1500; }
    void join(int spp16) {
        if (st_ != READY) { info_("join ignored: not connected"); if (onJoin) onJoin(false, spp16, "disconnected"); return; }
        if (spp16 < 0 || spp16 > 0x3FFF) { if (onJoin) onJoin(false, spp16, "arg"); return; }
        request_("JOIN " + std::to_string(spp16), [this, spp16](const std::vector<std::string>& r) {
            joinHoldUntil_ = 0;
            if (ok_(r)) { { auto mm = kv_(r.back()); info_("JOIN spp16=" + mm["spp16"] + " latency " + mm["tick_at"] + " us (must stay under half a tick, ~10 ms at 124 BPM)"); } auto m = kv_(r.back()); if (onJoin) onJoin(true, m.count("spp16") ? atoi(m["spp16"].c_str()) : spp16, m.count("tick_at") ? m["tick_at"] : ""); return; }
            std::string e = last_(r); size_t a = e.find(' ', 4); std::string code = a == std::string::npos ? e : e.substr(a + 1); code = code.substr(0, code.find(' '));
            info_("JOIN refused: " + e); if (onJoin) onJoin(false, spp16, code); });
    }
    // re-emit every mirrored param (the editor window was just opened: it ignored them while closed)
    void redumpParams() { for (auto& kv : params) if (onParam) onParam(kv.first, kv.second); }
    // ======================================================================================================================================
    // Host snapshot = the SET is the source of truth for the ONE patch it is in (EMGOR 2026-11-12).
    //   snap = the current patch's params (schema domains 1..4: synth, drums, sampler, sequencers) + the sampler slot -> pool id map, tagged with the PMOR patch slot.
    //   The owner (pmor.proto) persists it in the Live set (snapshotText / snapshotLoad) and only ever shows it; nothing is written to PMOR from a load.
    //   Kept current by: user edits (setParam, also while UNPLUGGED), PMOR pushes `P` while tracking, SET_PARAM acks, sample assigns, a patch switch on PMOR (adopt).
    //   Decision after EVERY full sync (resyncDecide_), values are pulled FROM PMOR first (GET_PARAMS), then:
    //     no snapshot yet (first connect)          -> ADOPT PMOR's patch ("adopted")
    //     PMOR patch slot == snapshot slot         -> PUSH the snapshot's params that differ (SET_PARAM, <= resyncPerSec, <= 4 in flight, `ERR busy` retried), confirm with a
    //                                                 GET_PARAMS re-pull, then assign the sampler slots that differ (ASSIGN_SAMPLE, one at a time, EDIT) -> "synced"
    //     PMOR patch slot != snapshot slot         -> NOTHING is written ("patchdiff", pmor slot, snapshot slot); snapAdopt() takes PMOR's patch, snapWrite() writes the snapshot
    //                                                 onto the patch PMOR is on (PROTOCOL has no patch-switch command: the user may also load the snapshot's patch on PMOR, the next
    //                                                 PATCH_CHANGED to that slot resyncs). Edits go to PMOR as before but are not recorded until one is chosen.
    //   Edits made on PMOR while unplugged are overwritten for that patch on reconnect (owner's choice). Pushes from PMOR are never echoed (they only update the snapshot).
    struct Snapshot { bool valid = false; int patch = -1; std::map<uint16_t, float> params; std::vector<int> slotIds; };
    Snapshot snap;
    std::function<void()> onSnapshot;                    // snapshot changed (the owner debounces and persists it)
    // kind: adopted(a=params b=patch) | pushing(a=left b=total) | synced(a=pushed b=mismatch c=slots assigned d=patch e=slots skipped) | patchdiff(a=PMOR slot b=snapshot slot) | cleared
    std::function<void(const std::string& kind, int a, int b, int c, int d, int e)> onResync;
    int resyncPerSec = 80;                               // SET_PARAM rate limit (never flood the CDC link)
    static bool eligible(uint16_t id) { int d = id >> 10; return d >= 1 && d <= 4; }
    bool snapTracking() const { return tracking_; }
    bool snapPending() const { return pendingChoice_; }
    bool snapResyncing() const { return resyncing_; }
    void snapClear() { snap = Snapshot(); tracking_ = false; pendingChoice_ = false; resyncQ_.clear(); assignQ_.clear(); resyncing_ = false; snapChanged_(); if (onResync) onResync("cleared", 0, 0, 0, 0, 0); }
    // "adopt X": the snapshot becomes PMOR's current patch (fresh GET_PARAMS first)
    void snapAdopt() {
        if (st_ != READY || !pendingChoice_) return;
        request_("GET_PARAMS", [this](const std::vector<std::string>& r) { if (ok_(r)) absorb_(r); adoptPmor_(); });
    }
    // "write Y": push the snapshot's params + sampler slots onto the patch PMOR is on (it becomes the snapshot's patch)
    void snapWrite() {
        if (st_ != READY || !pendingChoice_ || !snap.valid) return;
        request_("GET_PARAMS", [this](const std::vector<std::string>& r) { if (ok_(r)) absorb_(r); snap.patch = curPatch_(); pendingChoice_ = false; startPush_(); });
    }
    // text form (one symbol, no spaces / commas / semicolons: it travels through Max): PS1~<patch>~<slot ids '/'>~<id:val '/'>
    std::string snapshotText() const {
        if (!snap.valid) return std::string();
        std::string o = "PS1~" + std::to_string(snap.patch) + "~";
        for (size_t i = 0; i < snap.slotIds.size(); i++) { if (i) o += "/"; o += std::to_string(snap.slotIds[i]); }
        o += "~"; bool first = true; char b[40];
        for (auto& kv : snap.params) { snprintf(b, sizeof b, "%s%u:%.7g", first ? "" : "/", (unsigned)kv.first, (double)kv.second); first = false; o += b; }
        return o;
    }
    // returns true when the held snapshot changed. Never pushes anything (a Live set load is not a connection). Ignored while tracking a live link.
    bool snapshotLoad(const std::string& t) {
        if (t.rfind("PS1~", 0) != 0) return false;
        if (st_ == READY && tracking_) return false;
        size_t a = 4, b = t.find('~', a), c = b == std::string::npos ? b : t.find('~', b + 1); if (c == std::string::npos) return false;
        Snapshot n; n.valid = true; n.patch = atoi(t.substr(a, b - a).c_str());
        auto split = [](const std::string& x, char sep) { std::vector<std::string> v; size_t i = 0; while (i < x.size()) { size_t j = x.find(sep, i); if (j == std::string::npos) j = x.size(); if (j > i) v.push_back(x.substr(i, j - i)); i = j + 1; } return v; };
        for (auto& x : split(t.substr(b + 1, c - b - 1), '/')) n.slotIds.push_back(atoi(x.c_str()));
        for (auto& x : split(t.substr(c + 1), '/')) { size_t col = x.find(':'); if (col == std::string::npos) continue; n.params[(uint16_t)atoi(x.c_str())] = (float)atof(x.c_str() + col + 1); }
        if (n.params.empty()) return false;
        if (snap.valid && n.patch == snap.patch && n.params == snap.params && n.slotIds == snap.slotIds) return false;
        snap = n; return true;
    }
    int curPatch_() const { auto it = hello.find("patch"); if (it == hello.end() || it->second.empty() || !isdigit((unsigned char)it->second[0])) return -1; return atoi(it->second.c_str()); }
    void snapChanged_() { if (onSnapshot) onSnapshot(); }
    void noteEdit_(uint16_t id, float v) {
        if (!eligible(id) || !snap.valid) return;
        if (st_ == READY && !tracking_) return;              // choice pending: the snapshot waits for the user's decision
        snap.params[id] = v; snapChanged_();
    }
    void ackSnap_(uint16_t id, float f) { if (tracking_ && snap.valid && eligible(id)) { auto it = snap.params.find(id); if (it != snap.params.end() && fabsf(it->second - f) > 1e-6f) { it->second = f; snapChanged_(); } } }
    void pushSnap_(uint16_t id, float v) {                   // `P`: PMOR changed a value on its own
        if (!tracking_ || !snap.valid || !eligible(id)) return;
        if (resyncing_ && resyncPending_(id)) return;        // about to be overwritten by the set (owner's rule)
        snap.params[id] = v; snapChanged_();
    }
    bool resyncPending_(uint16_t id) const { for (auto x : resyncQ_) if (x == id) return true; return false; }
    void adoptPmor_() {
        snap.valid = true; snap.patch = curPatch_(); snap.params.clear(); snap.slotIds = curIds_;
        for (auto& kv : params) if (eligible(kv.first)) snap.params[kv.first] = kv.second;
        tracking_ = true; pendingChoice_ = false; resyncing_ = false; resyncQ_.clear(); assignQ_.clear();
        info_("snapshot: adopted PMOR patch " + std::to_string(snap.patch) + " (" + std::to_string(snap.params.size()) + " params)");
        snapChanged_(); if (onResync) onResync("adopted", (int)snap.params.size(), snap.patch, 0, 0, 0);
    }
    void resyncDecide_() {
        tracking_ = false; pendingChoice_ = false; resyncing_ = false; resyncQ_.clear(); assignQ_.clear(); offlineNote_ = false;
        int pm = curPatch_();
        if (!snap.valid || snap.params.empty() || pm < 0) { adoptPmor_(); return; }
        if (snap.patch != pm) {
            pendingChoice_ = true; info_("snapshot: PMOR is on patch " + std::to_string(pm) + ", the set holds patch " + std::to_string(snap.patch) + ": nothing written");
            if (onResync) onResync("patchdiff", pm, snap.patch, 0, 0, 0); return;
        }
        startPush_();
    }
    // PATCH_CHANGED handled (GET_PATCH + manifest re-read): pull the new patch's values, then follow PMOR (tracking) / resync (pending and PMOR landed on the snapshot's slot)
    void patchSwitchedSnap_() {
        if (st_ != READY) return;
        request_("GET_PARAMS", [this](const std::vector<std::string>& r) {
            if (ok_(r)) absorb_(r);
            int pm = curPatch_();
            if (pendingChoice_) { if (pm == snap.patch) { pendingChoice_ = false; startPush_(); } else if (onResync) onResync("patchdiff", pm, snap.patch, 0, 0, 0); return; }
            if (resyncing_) { resyncing_ = false; resyncQ_.clear(); assignQ_.clear(); }
            adoptPmor_();
        });
    }
    static bool close_(float a, float b) { return fabsf(a - b) <= 1e-5f + 1e-4f * fabsf(b); }
    void startPush_() {
        resyncQ_.clear(); assignQ_.clear(); resyncPushed_ = 0; resyncMismatch_ = 0; resyncSlots_ = 0; resyncSkipped_ = 0; inflight_ = 0; budget_ = 0; assignBusy_ = 0; confirmed_ = false;
        for (auto& kv : snap.params) {
            if (!hasParam(kv.first)) continue;
            auto it = params.find(kv.first);
            if (it == params.end() || !close_(it->second, kv.second)) resyncQ_.push_back(kv.first);
        }
        if (!snap.slotIds.empty() && !curIds_.empty()) {
            for (size_t sl = 0; sl < snap.slotIds.size() && sl < 16; sl++) {
                int want = snap.slotIds[sl], have = sl < curIds_.size() ? curIds_[sl] : 0;
                if (want <= 0 || want == have) continue;
                bool inPool = false; for (auto& l : manifest) { int id = 0; if (sscanf(l.c_str(), "M %d", &id) == 1 && id == want) { inPool = true; break; } }
                if (!inPool) { resyncSkipped_++; info_("snapshot: slot " + std::to_string(sl) + " wants sample S" + std::to_string(want) + ", which is no longer in PMOR's pool: left as it is"); continue; }
                assignQ_.push_back({(int)sl, want});
            }
        }
        tracking_ = true; resyncing_ = true; resyncTotal_ = (int)resyncQ_.size();
        info_("snapshot: pushing " + std::to_string(resyncTotal_) + " params and " + std::to_string(assignQ_.size()) + " sample slots from the set to patch " + std::to_string(snap.patch));
        if (onResync && resyncTotal_) onResync("pushing", resyncTotal_, resyncTotal_, 0, 0, 0);
        resyncAdvance_();
    }
    void resyncPump_(uint32_t dt) {                          // from tick(): rate-limited SET_PARAMs
        if (!resyncing_ || st_ != READY) return;
        budget_ += (double)dt * resyncPerSec / 1000.0; if (budget_ > 8) budget_ = 8;
        while (budget_ >= 1.0 && !resyncQ_.empty() && inflight_ < 4) {
            budget_ -= 1.0; uint16_t id = resyncQ_.front(); resyncQ_.pop_front();
            auto sit = snap.params.find(id); if (sit == snap.params.end()) continue;
            float v = sit->second; inflight_++;
            char b[48]; snprintf(b, sizeof b, "SET_PARAM %u %.6f", (unsigned)id, v);
            request_(b, [this, id](const std::vector<std::string>& r) {
                inflight_--;
                if (!resyncing_) return;
                if (!ok_(r)) {
                    if (!r.empty() && r.back().find(" busy") != std::string::npos) { resyncQ_.push_front(id); budget_ = 0; }      // a bulk reply is running: retry shortly
                    else { resyncMismatch_++; info_("snapshot: SET_PARAM " + std::to_string(id) + " refused: " + last_(r)); }
                } else {
                    resyncPushed_++; auto m = kv_(r.back()); auto it = m.find("val");
                    if (it != m.end()) { float f = (float)atof(it->second.c_str()); params[id] = f; if (onParam) onParam(id, f); }
                }
                if (onResync && resyncTotal_ && (resyncPushed_ % 16 == 0)) onResync("pushing", (int)resyncQ_.size() + inflight_, resyncTotal_, 0, 0, 0);
                resyncAdvance_();
            });
        }
    }
    void resyncAdvance_() {
        if (!resyncing_ || !resyncQ_.empty() || inflight_ > 0 || confirming_ || assigning_) return;
        if (resyncPushed_ > 0 && !confirmed_) {               // confirm: re-pull FROM PMOR and compare (clamped / refused values adopt PMOR's)
            confirming_ = true;
            request_("GET_PARAMS", [this](const std::vector<std::string>& r) {
                confirming_ = false; if (!resyncing_) return;
                if (ok_(r)) absorb_(r);
                int mm = 0;
                for (auto& kv : snap.params) { auto it = params.find(kv.first); if (it == params.end()) continue; if (!close_(it->second, kv.second)) { mm++; kv.second = it->second; } }
                resyncMismatch_ += mm; confirmed_ = true; snapChanged_(); resyncAdvance_();
            });
            return;
        }
        if (!assignQ_.empty()) { assignNext_(); return; }
        finishResync_();
    }
    void assignNext_() {
        if (assigning_ || assignQ_.empty()) return;
        if (pulling_ || fileBusy_) { later_(100, [this]() { if (resyncing_) assignNext_(); }); return; }      // a sample pull holds the link: wait
        assigning_ = true; auto a = assignQ_.front(); assignSlot_ = a.first; assign(a.first, a.second);
    }
    void resyncAssignDone_(int slot, bool ok, const std::string& code) {
        if (!resyncing_ || !assigning_ || slot != assignSlot_) return;
        assigning_ = false;
        if (!ok && code == "busy" && assignBusy_ < 5) { assignBusy_++; later_(200, [this]() { if (resyncing_) assignNext_(); }); return; }
        assignBusy_ = 0; if (!assignQ_.empty()) assignQ_.pop_front();
        if (ok) resyncSlots_++; else { resyncSkipped_++; info_("snapshot: slot " + std::to_string(slot) + " assign refused (" + code + ")"); }
        resyncAdvance_();
    }
    void fireAssigned_(int slot, bool ok, const std::string& code) { if (onAssigned) onAssigned(slot, ok, code); resyncAssignDone_(slot, ok, code); }
    void finishResync_() {
        resyncing_ = false; confirmed_ = false;
        info_("snapshot: synced " + std::to_string(resyncPushed_) + " params from the set (" + std::to_string(resyncMismatch_) + " differ after the re-pull, " + std::to_string(resyncSlots_) + " sample slots)");
        if (resyncSlots_ > 0) { snap.slotIds = curIds_; snapChanged_(); }
        if (onResync) onResync("synced", resyncPushed_, resyncMismatch_, resyncSlots_, snap.patch, resyncSkipped_);
    }
    bool tracking_ = false, pendingChoice_ = false, resyncing_ = false, confirming_ = false, confirmed_ = false, assigning_ = false, offlineNote_ = false;
    std::deque<uint16_t> resyncQ_; std::deque<std::pair<int, int>> assignQ_;
    int resyncPushed_ = 0, resyncMismatch_ = 0, resyncSlots_ = 0, resyncSkipped_ = 0, resyncTotal_ = 0, inflight_ = 0, assignSlot_ = -1, assignBusy_ = 0; double budget_ = 0;

    bool wantsConnection() const { return wanted_; }
    void getPatch() {       // whole current patch + the manifest (slot map needs both), then onPatch / onPool / onSynced
        refreshSlots_([this](bool ok) { if (ok) { if (onPatch) onPatch(atoi(hello["patch"].c_str())); if (onPool) onPool(); } if (onSynced) onSynced(); });
    }
    void feed(const char* d, size_t n) {
        buf_.append(d, n); lastRx_ = now_; size_t p;
        while ((p = buf_.find('\n')) != std::string::npos) {
            std::string l = buf_.substr(0, p); buf_.erase(0, p + 1);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l.empty() || l[0] != '!') continue;
            size_t s = l.rfind('*'); if (s == std::string::npos || l.size() - s != 5) { badFrames++; continue; }
            std::string body = l.substr(1, s - 1);
            if (pmorCrc16(body.data(), body.size()) != (uint16_t)strtoul(l.substr(s + 1).c_str(), nullptr, 16)) { badFrames++; continue; }
            size_t sp = body.find(' '); handle_(body.substr(0, sp), sp == std::string::npos ? "" : body.substr(sp + 1));
        }
    }
    // call at >= 10 Hz. In STUDIO/EDIT the device wants a frame at least every 2 s (it drops to STANDALONE after 5 s).
    void tick(uint32_t dtMs) {
        now_ += dtMs;
        if (st_ == DISCONNECTED) {
            if (wanted_ && autoReconnect && now_ - lastTry_ >= 1000) { lastTry_ = now_; std::string m = mode_; info_("reconnecting (" + m + ")"); connect(m); }
            return;
        }
        for (auto it = pend_.begin(); it != pend_.end();) {
            if (now_ - it->second.sent > (st_ == HELLO ? 1200u : 5000u)) {   // HELLO is single-line and answered at once: a lost one (PMOR drops bytes until it saw DTR) retries fast

                std::string b = it->second.body; info_("timeout: " + b); it = pend_.erase(it);
                fail_(st_ == READY ? "link lost (no answer to " + b + ")" : "timeout during connect/sync");   // unplugged / dead port: the device is STANDALONE again
                return;
            }
            else ++it;
        }
        if (st_ == READY && now_ - lastRx_ > 2000) { fail_("link lost (2 s silence)"); return; }   // PING every 1.5 s is always answered: 2 s of nothing = unplugged / dead port
        resyncPump_(dtMs);
        for (size_t i = 0; i < deferred_.size();) { if (now_ >= deferred_[i].first) { auto f = deferred_[i].second; deferred_.erase(deferred_.begin() + i); f(); } else i++; }
        if (st_ == READY && now_ - lastStatus_ >= (storeWatch_ || lastStore_ == 1 ? 1000u : 2000u) && !(pulling_ || fileBusy_)) { lastStatus_ = now_; if (storeWatch_ && --storeWatchLeft_ <= 0) { storeWatch_ = false; info_("storage retry: gave up waiting"); } pollStatus(); }
        if (st_ == READY && mode_ != "NEPTR" && now_ - lastTx_ > 1500) request_("PING", [](const std::vector<std::string>&) {});
    }

private:
    struct Req { std::string body; std::function<void(const std::vector<std::string>&)> done; std::vector<std::string> lines; uint32_t sent = 0; };
    State st_ = DISCONNECTED; std::string mode_, buf_; uint32_t now_ = 0, lastTx_ = 0, lastRx_ = 0; unsigned seq_ = 0;
    std::map<std::string, Req> pend_;
    std::deque<std::string> steps_;

    struct FileJob { char kind = 'R'; int slot = 0; std::function<void(const std::vector<uint8_t>&, bool)> done; };
    std::deque<FileJob> fileQ_; bool fileBusy_ = false; FileJob fcur_; std::vector<uint8_t> fdata_; uint32_t ftotal_ = 0; bool ftotalKnown_ = false; int fbad_ = 0;
    std::vector<std::pair<uint32_t, std::function<void()>>> deferred_;
    int presetsLeft_ = 0, lastStore_ = -1; bool storeWatch_ = false, storeWatchSeen_ = false, statusPending_ = false; int storeWatchLeft_ = 0; uint32_t lastStatus_ = 0;
    long tempoMilli_ = 0; bool tempoSent_ = false;
    void sendTempo_() { if (st_ != READY || tempoMilli_ <= 0) return; tempoSent_ = true; request_("TEMPO " + std::to_string(tempoMilli_), [](const std::vector<std::string>&) {}); }
    unsigned pullGen_ = 0; bool slotsOk_ = false, noBin_ = false;
    std::deque<Sample> pullQ_; bool pulling_ = false; Sample cur_; std::vector<uint8_t> curData_; uint32_t curTotal_ = 0; int curBad_ = 0;
    std::vector<int> curIds_;      // PATCHINFO ids= : pool ids of the current patch's slots (new layout); empty = legacy ids where id/16 is the patch
    bool inCurrent_(int id, int slot) const { if (!curIds_.empty()) { for (int x : curIds_) if (x == id) return true; return false; } return id / 16 == slot; }
    static bool parseM_(const std::string& l, Sample& sm) {
        unsigned id, len, rate, ch, crc; char name[64] = {0};
        if (sscanf(l.c_str(), "M %u %u %u %u %x %63s", &id, &len, &rate, &ch, &crc, name) < 5) return false;
        sm.id = (int)id; sm.len = len; sm.rate = rate; sm.ch = ch; sm.crc = crc; sm.name = name; return true;
    }
    void pullNext_() {
        if (pulling_ || fileBusy_ || st_ != READY) return;
        while (!pullQ_.empty()) {
            cur_ = pullQ_.front(); pullQ_.pop_front();
            if (cur_.len == 0) continue;                                    // empty slot
            if (haveSample && haveSample(cur_)) { if (onSampleProgress) onSampleProgress(cur_, 1, 1, (int)pullQ_.size()); continue; }
            pulling_ = true; curData_.clear(); curTotal_ = cur_.len * 2; curBad_ = 0; pullChunk_(); return;
        }
    }
    void pullChunk_() {
        unsigned gen = pullGen_;      // a replies that belongs to a cancelled / replaced pull is ignored
        if (joinHold_()) { later_(10, [this, gen]() { if (pulling_ && gen == pullGen_) pullChunk_(); }); return; }
        uint32_t off = (uint32_t)curData_.size();
        request_("PULL_SAMPLE " + std::to_string(cur_.id) + " " + std::to_string(off) + " 1024 8", [this, off, gen](const std::vector<std::string>& r) {
            if (!pulling_ || gen != pullGen_) return;
            if (busy_(r)) { later_(40, [this, gen]() { if (pulling_ && gen == pullGen_) pullChunk_(); }); return; }     // another multi-line reply is running: retry
            if (!ok_(r)) { info_("PULL_SAMPLE " + std::to_string(cur_.id) + " failed: " + last_(r)); finishPull_(false); return; }
            bool got = false; uint32_t n = 0;
            for (auto& l : r) if (l.rfind("C ", 0) == 0) {
                unsigned i2, o2, nb, crc; char b64[1700];
                if (sscanf(l.c_str(), "C %u %u %u %x %1699s", &i2, &o2, &nb, &crc, b64) == 5) {
                    uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf);
                    if (dl == (long)nb && o2 == (uint32_t)curData_.size() && pmorCrc32Of(buf, nb) == crc) { curData_.insert(curData_.end(), buf, buf + nb); got = true; n = nb; }   // chunks arrive in offset order (count=8 pipelining)
                }
            }
            if (!got && !r.empty() && r.back().find(" n=0") != std::string::npos) { finishPull_(true); return; }   // end of data
            if (!got) { if (++curBad_ > 5) { info_("sample " + cur_.name + ": too many bad chunks"); finishPull_(false); return; } }   // resume from the first unverified offset
            if (onSampleProgress) onSampleProgress(cur_, (uint32_t)curData_.size(), curTotal_, (int)pullQ_.size());
            if (curData_.size() >= curTotal_) { finishPull_(true); return; }
            (void)n; pullChunk_();
        });
    }
    std::string assignRestore_; unsigned lastRxIrq_ = 0; bool haveRx_ = false; bool wanted_ = false; uint32_t lastTry_ = 0; uint32_t joinHoldUntil_ = 0;
    bool joinHold_() const { return joinHoldUntil_ != 0 && (int32_t)(joinHoldUntil_ - now_) > 0; }
    void doAssign_(int slot, int id) {
        request_("ASSIGN_SAMPLE " + std::to_string(slot) + " " + std::to_string(id), [this, slot](const std::vector<std::string>& r) {
            if (!ok_(r)) { std::string e = last_(r); size_t a = e.find(' ', 4); std::string code = a == std::string::npos ? e : e.substr(a + 1); info_("ASSIGN_SAMPLE refused: " + e);
                restoreMode_(); fireAssigned_(slot, false, code.substr(0, code.find(' '))); } });     // success is reported by the later SAMPLE_READY push
    }
    void restoreMode_() {
        if (assignRestore_.empty()) return; std::string m = assignRestore_; assignRestore_.clear();
        abortInflight_(); request_("HELLO " + m + " 1", [this, m](const std::vector<std::string>& r) { if (ok_(r)) { hello["mode"] = m; mode_ = m; info_("mode back to " + m); } });
    }
    // GET_PATCH then GET_MANIFEST, in that order, as ONE unit: slotsOk_ is false until both are absorbed (a half-updated slot map is never used)
    void refreshSlots_(std::function<void(bool ok)> done) {
        slotsOk_ = false;
        request_("GET_PATCH", [this, done](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("GET_PATCH failed: " + last_(r)); curIds_.clear(); if (done) done(false); return; }
            curIds_.clear(); absorb_(r); auto m = kv_(r.back()); if (m.count("slot")) hello["patch"] = m["slot"];
            request_("GET_MANIFEST", [this, done](const std::vector<std::string>& r2) {
                if (!ok_(r2)) { info_("GET_MANIFEST failed: " + last_(r2)); if (done) done(false); return; }
                manifest.clear(); absorb_(r2); slotsOk_ = true; if (done) done(true); });
        });
    }
    static std::string errCode_(const std::vector<std::string>& r) {      // "ERR <CMD> <code> [text]" -> code
        std::string e = last_(r); size_t a = e.find(' ', 4); std::string c = a == std::string::npos ? "" : e.substr(a + 1); return c.substr(0, c.find(' '));
    }
    // EDIT for a mutating command: from STUDIO the client switches to EDIT (HELLO again = mode change), the previous mode comes back through restoreMode_()
    void withEdit_(std::function<void()> go, std::function<void()> fail) {
        if (hello["mode"] == "EDIT") { go(); return; }
        std::string prev = hello["mode"];
        abortInflight_(); request_("HELLO EDIT 1", [this, prev, go, fail](const std::vector<std::string>& r) {
            if (!ok_(r)) { info_("could not switch to EDIT: " + last_(r)); if (fail) fail(); return; }
            hello["mode"] = "EDIT"; mode_ = "EDIT"; assignRestore_ = prev; go(); });
    }
    bool noSave_ = false;
    // "ERR SAVE_PRESET <code> ..." or the bare "ERR <code>" -> code
    static std::string saveErr_(const std::vector<std::string>& r) {
        std::string e = last_(r); std::vector<std::string> t; size_t i = 0;
        while (i < e.size()) { size_t j = e.find(' ', i); if (j == std::string::npos) j = e.size(); if (j > i) t.push_back(e.substr(i, j - i)); i = j + 1; }
        if (t.size() >= 3 && t[1] == "SAVE_PRESET") return t[2];
        if (t.size() >= 2) return t[1] == "SAVE_PRESET" ? "" : t[1];
        return t.empty() ? "" : t[0];
    }
    // re-pull ONE engine's PRESETS/<E>.TXT, replace that engine's entries, then onPresets (UI menu) and onPresetSaved
    void repullPresetEngine_(int engine, int n) {
        pullFile('R', engine, [this, engine, n](const std::vector<uint8_t>& d, bool ok) {
            if (ok) { std::vector<Preset> keep; for (auto& p : presets) if (p.engine != engine) keep.push_back(p); presets.swap(keep); parsePresetFile_(engine, d); }
            else info_("preset file " + std::to_string(engine) + " re-pull failed");
            if (onPresets) onPresets();
            if (onPresetSaved) onPresetSaved(engine, n, true, ""); });
    }
    std::vector<int> delIds_; size_t delIdx_ = 0; int delDone_ = 0;
    void noBinSeen_() { noBin_ = true; info_("firmware has no recycle bin yet"); if (onNoBin) onNoBin(); }
    void delNext_() {
        if (delIdx_ >= delIds_.size()) { restoreMode_(); delIds_.clear(); if (delDone_) refreshSlots_([this](bool ok) { if (ok) { if (onPool) onPool(); } if (onSynced) onSynced(); }); return; }
        int id = delIds_[delIdx_++];
        request_("DELETE_POOL " + std::to_string(id), [this, id](const std::vector<std::string>& r) {
            if (ok_(r)) { delDone_++; if (onDeleted) onDeleted(id, true, ""); delNext_(); return; }
            std::string c = errCode_(r);
            if (c == "unknown") { noBinSeen_(); delIds_.clear(); delIdx_ = 0; restoreMode_(); return; }       // nothing was deleted, the rest is NOT attempted
            info_("DELETE_POOL " + std::to_string(id) + " refused: " + last_(r)); if (onDeleted) onDeleted(id, false, c); delNext_(); });
    }
    static bool busy_(const std::vector<std::string>& r) { return !r.empty() && r.back().rfind("ERR ", 0) == 0 && r.back().find(" busy") != std::string::npos; }
    void later_(uint32_t ms, std::function<void()> f) { deferred_.push_back({now_ + ms, std::move(f)}); }
    void fileNext_() {
        if (fileBusy_ || pulling_ || st_ != READY || fileQ_.empty()) return;
        fcur_ = std::move(fileQ_.front()); fileQ_.pop_front(); fileBusy_ = true; fdata_.clear(); ftotal_ = 0; ftotalKnown_ = false; fbad_ = 0; fileChunk_();
    }
    void fileChunk_() {
        if (joinHold_()) { later_(10, [this]() { if (fileBusy_) fileChunk_(); }); return; }
        uint32_t off = (uint32_t)fdata_.size();
        request_(std::string("PULL_FILE ") + fcur_.kind + " " + std::to_string(fcur_.slot) + " " + std::to_string(off) + " 1024 8", [this, off](const std::vector<std::string>& r) {
            if (!fileBusy_) return;
            if (busy_(r)) { later_(40, [this]() { if (fileBusy_) fileChunk_(); }); return; }
            if (!ok_(r)) { info_(std::string("PULL_FILE ") + fcur_.kind + " " + std::to_string(fcur_.slot) + " failed: " + last_(r)); finishFile_(false); return; }
            bool got = false;
            for (auto& l : r) if (l.rfind("F ", 0) == 0) {
                char k; int sl; unsigned o2, nb, crc; char b64[1700];
                if (sscanf(l.c_str(), "F %c %d %u %u %x %1699s", &k, &sl, &o2, &nb, &crc, b64) == 6) {
                    uint8_t buf[1100]; long dl = pmorB64Decode(b64, strlen(b64), buf);
                    if (dl == (long)nb && o2 == (uint32_t)fdata_.size() && pmorCrc32Of(buf, nb) == crc) { fdata_.insert(fdata_.end(), buf, buf + nb); got = true; }
                }
            }
            auto m = kv_(r.back()); if (m.count("total")) { ftotal_ = (uint32_t)strtoul(m["total"].c_str(), nullptr, 10); ftotalKnown_ = true; }
            if (ftotalKnown_ && fdata_.size() >= ftotal_) { finishFile_(true); return; }
            if (!got && m.count("n") && atoi(m["n"].c_str()) == 0) { finishFile_(ftotalKnown_ && fdata_.size() == ftotal_); return; }
            if (!got && ++fbad_ > 5) { info_("PULL_FILE: too many bad chunks"); finishFile_(false); return; }
            fileChunk_();
        });
    }
    void finishFile_(bool ok) {
        fileBusy_ = false; auto done = std::move(fcur_.done); auto data = std::move(fdata_); fdata_.clear();
        if (done) done(data, ok);
        fileNext_(); pullNext_();
    }
    static int hexv_(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
    // PRESETS/<ENGINE>.TXT: "PMORPRESET 2", "engine", "blob <size>", "f <n> <name|-> <hex>", "u <n> <name|-> <hex>", "end <crc>"
    void parsePresetFile_(int engine, const std::vector<uint8_t>& d) {
        std::string t(d.begin(), d.end()); size_t i = 0; int n0 = (int)presets.size();
        while (i < t.size()) {
            size_t e = t.find('\n', i); if (e == std::string::npos) e = t.size();
            std::string l = t.substr(i, e - i); i = e + 1; if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l.size() < 5 || (l[0] != 'f' && l[0] != 'u') || l[1] != ' ') continue;
            char name[40] = {0}; int n = 0; char hex[4096] = {0};
            if (sscanf(l.c_str() + 2, "%d %39s %4095s", &n, name, hex) < 2) continue;
            Preset p; p.engine = engine; p.n = n; p.kind = l[0]; p.name = name;      // "-" = unnamed in the file: the UI falls back to the schema's sound / preset names (drum_sounds[group][n], synth_presets[n])
            for (size_t h = 0; hex[h] && hex[h + 1]; h += 2) { int a = hexv_(hex[h]), b = hexv_(hex[h + 1]); if (a < 0 || b < 0) break; p.blob.push_back((uint8_t)(a * 16 + b)); }
            presets.push_back(std::move(p));
        }
        if ((int)presets.size() == n0) info_("presets: engine " + std::to_string(engine) + " has no entries");
    }
    void finishPull_(bool completed) {
        // Every chunk was CRC-checked on arrival. The manifest's crc field is, in the firmware today, a content VERSION token
        // (pool_ver), not CRC-32 of the stream (PROTOCOL.md says CRC-32), so it is the cache key, not a whole-file check.
        bool ok = completed && curData_.size() == curTotal_;
        if (completed && !ok) info_("sample " + cur_.name + ": got " + std::to_string(curData_.size()) + " of " + std::to_string(curTotal_) + " bytes");
        pulling_ = false;
        if (onSampleDone) onSampleDone(cur_, curData_, ok);
        fileNext_(); pullNext_();
    }
    void reset_() { tracking_ = false; pendingChoice_ = false; resyncing_ = false; confirming_ = false; confirmed_ = false; assigning_ = false; resyncQ_.clear(); assignQ_.clear(); inflight_ = 0; pullQ_.clear(); pulling_ = false; fileQ_.clear(); fileBusy_ = false; deferred_.clear(); presets.clear(); statusPending_ = false; storeWatch_ = false; storeWatchSeen_ = false; lastStore_ = -1; lastStatus_ = 0; assignRestore_.clear(); haveRx_ = false; curIds_.clear(); slotsOk_ = false; noBin_ = false; tempoSent_ = false; delIds_.clear(); delIdx_ = 0; buf_.clear(); pend_.clear(); steps_.clear(); params.clear(); seq.clear(); manifest.clear(); hello.clear(); schemaHash.clear(); schemaCount = 0; }
    void info_(const std::string& s) { if (onInfo) onInfo(s); }
    void fail_(const std::string& s) { resyncing_ = false; tracking_ = false; info_(s); pend_.clear(); steps_.clear(); st_ = DISCONNECTED; curIds_.clear(); slotsOk_ = false; pullQ_.clear(); pulling_ = false; pullGen_++; }
    static bool ok_(const std::vector<std::string>& r) { return !r.empty() && r.back().rfind("OK ", 0) == 0; }
    static std::string last_(const std::vector<std::string>& r) { return r.empty() ? "timeout" : r.back(); }
    static std::map<std::string, std::string> kv_(const std::string& line) {
        std::map<std::string, std::string> m; size_t i = 0;
        while (i < line.size()) { size_t sp = line.find(' ', i); if (sp == std::string::npos) sp = line.size();
            std::string t = line.substr(i, sp - i); size_t eq = t.find('='); if (eq != std::string::npos) m[t.substr(0, eq)] = t.substr(eq + 1); i = sp + 1; }
        return m;
    }
    // A HELLO starts a new session on PMOR (firmware 844270a): it drops the TX backlog, so replies to requests still in flight are never sent.
    // Answer those locally with an error instead of waiting for the 5 s link-lost timeout.
    void abortInflight_() {
        std::vector<std::function<void(const std::vector<std::string>&)>> dn;
        for (auto& kv : pend_) if (kv.second.body.compare(0, 5, "HELLO") != 0 && kv.second.body != "PING") dn.push_back(kv.second.done);
        for (auto it = pend_.begin(); it != pend_.end();) { if (it->second.body.compare(0, 5, "HELLO") != 0 && it->second.body != "PING") it = pend_.erase(it); else ++it; }
        for (auto& d : dn) if (d) d(std::vector<std::string>{"ERR session reset"});
    }
    void request_(const std::string& body, std::function<void(const std::vector<std::string>&)> done) {
        char t[12]; snprintf(t, sizeof t, "h%x", (unsigned)(++seq_ & 0xFFF)); std::string tag = t;
        std::string b = tag + " " + body; char c[8]; snprintf(c, sizeof c, "*%04X\n", (unsigned)pmorCrc16(b.data(), b.size()));
        Req r; r.body = body; r.done = std::move(done); r.sent = now_; pend_[tag] = std::move(r);
        lastTx_ = now_; if (send) send("!" + b + c);
    }
    void absorb_(const std::vector<std::string>& r) {      // GET_PARAMS / GET_PATCH lines
        for (auto& l : r) {
            if (l.rfind("PARAMS ", 0) == 0) {
                size_t i = 7; while (i < l.size()) { size_t sp = l.find(' ', i); if (sp == std::string::npos) sp = l.size();
                    std::string t = l.substr(i, sp - i); size_t eq = t.find('=');
                    if (eq != std::string::npos) { uint16_t id = (uint16_t)atoi(t.c_str()); float v = (float)atof(t.c_str() + eq + 1); params[id] = v; if (onParam) onParam(id, v); }
                    i = sp + 1; }
            } else if (l.rfind("SEQ ", 0) == 0) { int e = atoi(l.c_str() + 4); seq[e] = l; }
            else if (l.rfind("M ", 0) == 0) manifest.push_back(l);
            else if (l.rfind("PATCHINFO ", 0) == 0) { size_t q = l.find(" ids="); curIds_.clear(); if (q != std::string::npos) { size_t i = q + 5; while (i <= l.size()) { size_t c = l.find(',', i); if (c == std::string::npos) c = l.size(); curIds_.push_back(atoi(l.substr(i, c - i).c_str())); i = c + 1; } bool any = false; for (int x : curIds_) if (x > 0) any = true; if (!any) curIds_.clear(); } }      // all zeros (nothing pooled for this patch / un-migrated sim) = no id map: legacy patch*16+slot rule
        }
    }
    void fullSync_() {
        request_("GET_SCHEMA", [this](const std::vector<std::string>& r) {
            schema.clear();
            if (!ok_(r)) info_("GET_SCHEMA failed (" + last_(r) + "): continuing without a schema (older firmware)");      // never refuse over schema differences
            else {
                uint32_t h = 2166136261u; size_t n = 0;
                for (auto& l : r) if (l.rfind("SCHEMA ", 0) == 0) {
                    std::string rest = l.substr(7); h = pmorFnv1a(h, rest.data(), rest.size()); h = pmorFnv1a(h, "\n", 1); n++;
                    std::vector<std::string> t; size_t i = 0; while (i < rest.size()) { size_t sp = rest.find(' ', i); if (sp == std::string::npos) sp = rest.size(); t.push_back(rest.substr(i, sp - i)); i = sp + 1; }
                    if (t.empty()) continue;
                    SchemaEntry e; e.id = atoi(t[0].c_str()); if (t.size() > 1) e.name = t[1]; if (t.size() > 2) e.min = (float)atof(t[2].c_str()); if (t.size() > 3) e.max = (float)atof(t[3].c_str());
                    if (t.size() > 4) e.def = (float)atof(t[4].c_str()); if (t.size() > 5) e.flags = atoi(t[5].c_str()); if (t.size() > 6) e.group = t[6];   // missing columns keep their defaults, extra columns are ignored
                    schema[(uint16_t)e.id] = e;
                }
                char hs[16]; snprintf(hs, sizeof hs, "%08X", (unsigned)h); schemaHash = hs; schemaCount = n;
                if (hello.count("schema") && schemaHash != hello["schema"]) info_("schema hash differs from HELLO (" + schemaHash + " vs " + hello["schema"] + "); using the dumped schema");
            }
            if (onSchema) onSchema();
            request_("GET_PARAMS", [this](const std::vector<std::string>& r2) {
                if (!ok_(r2)) info_("GET_PARAMS failed (" + last_(r2) + "): continuing with no param values");
                else absorb_(r2);
                seqNext_(0);
            });
        });
    }
    void seqNext_(int e) {
        if (e > 2) {
            request_("GET_MANIFEST", [this](const std::vector<std::string>& r) {
                if (ok_(r)) absorb_(r); else info_("GET_MANIFEST failed: " + last_(r));
                // GET_PATCH after the manifest: PATCHINFO ids= is what maps the sampler slots to pool ids (the manifest alone cannot)
                request_("GET_PATCH", [this](const std::vector<std::string>& r2) {
                    bool pok = ok_(r2); if (pok) { curIds_.clear(); absorb_(r2); auto m = kv_(r2.back()); if (m.count("slot")) hello["patch"] = m["slot"]; } else info_("GET_PATCH failed: " + last_(r2));
                    slotsOk_ = pok;
                    st_ = READY; sendTempo_(); info_("synced: " + std::to_string(params.size()) + " params, " + std::to_string(manifest.size()) + " samples");
                    if (onPatch) onPatch(atoi(hello["patch"].c_str()));
                    if (onPool) onPool();
                    if (onSynced) onSynced();
                    pollStatus();
                    resyncDecide_();            // host snapshot vs PMOR (adopt / push the set's patch / ask)
                });
            });
            return;
        }
        request_("GET_SEQ " + std::to_string(e), [this, e](const std::vector<std::string>& r) { if (ok_(r)) absorb_(r); seqNext_(e + 1); });
    }
    void handle_(const std::string& tag, const std::string& body) {
        if (tag == "-") {
            if (body.rfind("P ", 0) == 0) { uint16_t id = (uint16_t)atoi(body.c_str() + 2); size_t sp = body.find(' ', 2); if (sp != std::string::npos) { float v = (float)atof(body.c_str() + sp + 1); params[id] = v; pushSnap_(id, v); if (onParam) onParam(id, v); } }
            else if (body.rfind("PATCH_CHANGED", 0) == 0) {
                info_("patch changed on PMOR: " + body);
                if (st_ == READY) {            // the device is the source of truth: pull the whole patch (never write one) AND reload the manifest (a new recording adds samples), then let the owner pull the samples
                    pullQ_.clear(); pullGen_++; pulling_ = false;
                    refreshSlots_([this](bool ok) {
                        int sl = atoi(hello["patch"].c_str());
                        if (ok) { if (onPatch) onPatch(sl); if (onPool) onPool(); }
                        if (onSynced) onSynced();
                        if (ok && onPatchSwitched) onPatchSwitched(sl);
                        if (ok) patchSwitchedSnap_(); });
                } }
            else if (body.rfind("SAMPLE_READY", 0) == 0) {      // the slot now holds another sample: re-read GET_PATCH (ids=) + GET_MANIFEST (len/crc) before anything maps it
                int sl = atoi(body.c_str() + 12); info_("sample ready in slot " + std::to_string(sl)); restoreMode_();
                refreshSlots_([this, sl](bool ok) { if (ok && onPool) onPool(); if (ok && tracking_ && !resyncing_) { snap.slotIds = curIds_; snapChanged_(); } fireAssigned_(sl, true, ""); }); }
            else if (body.rfind("SAMPLE_ERR", 0) == 0) { int sl = atoi(body.c_str() + 10); size_t sp = body.find(' ', 11); restoreMode_(); fireAssigned_(sl, false, sp == std::string::npos ? "io" : body.substr(sp + 1)); }
            else if (body.rfind("SEQ_CHANGED", 0) == 0) { int e = atoi(body.c_str() + 12); request_("GET_SEQ " + std::to_string(e), [this](const std::vector<std::string>& r) { if (ok_(r)) absorb_(r); }); }
            else if (body.rfind("NAK", 0) == 0) { nakCount++; info_("device NAK: " + body); }
            else info_("device: " + body);
            return;
        }
        auto it = pend_.find(tag); if (it == pend_.end()) return;
        it->second.lines.push_back(body);
        if (body.rfind("OK ", 0) == 0 || body.rfind("ERR ", 0) == 0) {
            auto done = it->second.done; auto lines = std::move(it->second.lines); std::string rbody = it->second.body; pend_.erase(it);
            // a multi-line request answered while another multi-line reply runs is `ERR <CMD> busy`: retry the GET_* reads (the chunk pulls retry themselves)
            bool bootingErr = body.rfind("ERR ", 0) == 0 && body.find(" booting") != std::string::npos;
            if ((bootingErr || (body.rfind("ERR ", 0) == 0 && body.find(" busy") != std::string::npos)) && (rbody.rfind("GET_", 0) == 0 || rbody == "BIN_LIST") && st_ != DISCONNECTED) {
                later_(bootingErr ? 1000 : 60, [this, rbody, done]() { if (st_ != DISCONNECTED) request_(rbody, done); }); return; }
            if (done) done(lines);
        }
    }
};

}  // namespace pmorhost
