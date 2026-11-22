/// pmor.proto : host side of docs/PROTOCOL.md for Max. Pure message/byte object, it never opens a port:
/// wire a port object's byte output into this inlet and this object's byte outlet back into the port.
///
///   inlet:   int / list          bytes received from the PMOR CDC port
///            hello STUDIO|EDIT|NEPTR       HELLO, then full resync from PMOR
///            bye                           BYE
///            closed                        the port closed / unplugged (device is STANDALONE again)
///            setparam <id> <value>         SET_PARAM to PMOR (PMOR stays the source of truth)
///            snapload <text> | snapadopt | snapwrite | snapclear   the Live set holds the CURRENT patch (pmor_host.h "Host snapshot"): out: `snapshot <text>` (store it) and `snapstate <kind> a b c d e`
///            getpatch                      pull the whole current patch
///            sessiondir <path>             cache dir inside the Live set's folder (read first, written first if usable)
///            cachedir <path>               fallback cache dir (default ~/Music/PMOR_Cache)
///            tempo <bpm>                   Live's tempo -> `TEMPO <round(bpm*1000)>` to PMOR (valid 40..400 BPM, anything else ignored silently; only while READY; the last value is
///                                          resent after every (re)sync; `ERR TEMPO unknown|arg|mode` from older firmware is ignored, no state change)
///            pull current|all|off          sample pulls: STUDIO pulls the current patch, EDIT all (automatic after sync; attribute autopull)
///            pullid <id>                   pull one pool sample now (pool ids >= 1024, legacy patch*16+slot below)
///            slots                         re-send every sampler slot (`slot <n> <path> ...`) to pmor.core~ via outlet 1 (done automatically after sync, pulls, patch change, assign).
///                                          The slot map is PMOR's alone (PATCHINFO ids= + manifest, re-read after SAMPLE_READY / PATCH_CHANGED); a file whose CRC-32 does not match
///                                          PMOR's manifest crc is quarantined (.bad) and the slot stays empty until a good copy is pulled; not READY = all slots empty (pmor_slotsync.h)
///            assign <slot> <id>            ASSIGN_SAMPLE (switches to EDIT for the assign, then back); result arrives as `assigned`
///            pool                          re-emit the pool listing (poolbegin / pool ... / poolend)
///            deletepool <id> [<id> ...]    DELETE_POOL each id (-> PMOR's recycle bin; STUB protocol, see pmor_host.h; EDIT for the call); results `deleted <id> ok|err <code>`; firmware without it: `nobin`
///            bin                           BIN_LIST -> binbegin / bin <id> <len> <name> <age> <boots_left> / binend <n>
///            binrestore <id>               BIN_RESTORE -> `binrestored <id> ok|err <code>`
///            binempty                      BIN_EMPTY YES (permanent; the UI confirms first) -> `binemptied ok <n>|err <code>`
///            savepreset <engine> <n> <name> SAVE_PRESET (EDIT; `_` in the name = space); result `presetsaved <engine> <n> ok|err <code>` then the preset list is re-sent
///            presets                       pull + parse PRESETS/<engine>.TXT via PULL_FILE R (engine 0-3 drum groups, 4-11 synth algos)
///            preset <engine> <n> [pad]     apply a preset as SET_PARAMs (drum engines need a pad 0..15)
///            storeretry                    STORE_RETRY, then pool + presets reload when PMOR's storage job settles
///            status                        poll STATUS now (also polled every 2 s while READY)
///            mode STUDIO|EDIT              HELLO again = mode change, no resync (editor opened -> EDIT, closed -> STUDIO); `hello <other mode>` while ready does the same
///            join <spp16>                  CDC JOIN: "the F8 nearest this request's arrival is tick 0 of 16th <spp16>" (send AT a 16th boundary); reply `joined <spp16> <tick_at>` / `joinerr <spp16> <code>`
///            joinprep                      send one 16th BEFORE `join`: holds back bulk chunk requests (JOIN answers ERR busy during a bulk reply)
///            findport                      locate PMOR by USB identity (VID cafe PID 4f70, IOKit registry only, never opens a port): outlet 2 `pmorport <letter> <path>` or `pmorport none`.
///                                          letter = Max [serial] letter = index in the sorted /dev/cu.* list (a Bela usbmodem4488.. is never returned)
///            dumpparams                    re-emit every mirrored param on outlet 1 (the editor was just opened)
///   outlet 0: list of bytes for the port
///   outlet 1: param <id> <value>  -> wire to pmor.core~ (every value PMOR reports: resync, pushes, set acks)
///   outlet 2: status messages: state <name>, info <text...>, synced <params> <samples>,
///             sample <name> cached|pulled|bad, progress <name> <bytes_done> <bytes_total> <samples_left>,
///             poolbegin / pool <id> <len_samples> <name> <cached 0|1|2> <slots> <patch> <rate> <crc x<hex>|-> <known 0|1> / poolend <count>, assigned <slot> ok|err <code>,
///             (pool args: id = pool id (>= 1024) or legacy patch*16+slot; len_samples (seconds = len/rate); name = PMOR's manifest name (S<id> or P<nn>S<ss>);
///              cached 0 = not on this Mac, 1 = cached and CRC-verified, 2 = refused / quarantined (bad CRC or failed pull, re-pulled once per manifest reload);
///              slots = comma list of CURRENT-patch sampler slots using it (e.g. `3,5`) or `-`; patch = legacy patch number for P<nn>S<ss> names, else `-`; rate = Hz.
///              The first four args are the old layout: older consumers keep working. Usage by OTHER patches is not known (needs PULL_FILE T PATCHES/Pnn.TXT).),
///             binbegin / bin <id> <len_samples> <name> <age> <boots_left> / binend <n> (recycle bin; age = seconds since deletion or a timestamp, raw), deleted <id> ok|err <code>,
///             binrestored <id> ok|err <code>, binemptied ok <n>|err <code>, nobin (firmware has no recycle bin yet: nothing was changed),
///             presetbegin / preset <engine> <n> <f|u> <name> / presetend <count>, store <state 0..3> <pending> <errors> <pool>,
///             fw <fw> <proto> (HELLO fields, informational),
///             schemabegin <n> / schema <id> <name> <min> <max> <def> <flags> <group> / schemaend <n> (PMOR's own schema, after each sync; match controls BY ID),
///             joined / joinerr (above), mode <STUDIO|EDIT> (session mode changed), patch <slot>, status <mode> <policy AUTO|INTERNAL|FOLLOW|MASTER> <running> <bpm> <cpu> <overruns> <midi_drops> <bus clock arriving 0|1> <role master|follow>
/// A pushed P line is never turned back into SET_PARAM (loop rule). Liveness PING runs on an internal 100 ms timer.

#include "c74_min.h"
#include "pmor_host.h"
#include "pmor_cache.h"
#include "pmor_slotsync.h"
#include "pmor_findport.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

using namespace c74::min;

class pmor_proto : public object<pmor_proto> {
public:
	MIN_DESCRIPTION {"PMOR STUDIO host protocol client (bytes in/out, no port I/O)"};
	MIN_TAGS {"midi, protocol"};
	MIN_AUTHOR {"EMGOR"};
	MIN_RELATED {"pmor.core~"};

	inlet<>  in {this, "(bytes, connect/bye/closed/setparam/getpatch)"};
	outlet<> out_bytes {this, "(list) bytes to the port"};
	outlet<> out_core {this, "(param id value) for pmor.core~"};
	outlet<> out_info {this, "(status)"};

	pmor_proto(const atoms& = {}) {
		m_c.send = [this](const std::string& s) {
			atoms a; a.reserve(s.size());
			for (unsigned char ch : s) a.push_back((long)ch);
			out_bytes.send(a);
		};
		m_c.onParam = [this](uint16_t id, float v) { out_core.send("param", (long)id, (double)v); };
		m_c.onInfo = [this](const std::string& s) { atoms a {symbol("info")}; a.push_back(symbol(s)); out_info.send(a); };
		m_sync.emit = [this](int sl, const std::string& path, bool hasCrc, uint32_t crc, uint32_t rate, uint32_t ch) {
			if (path.empty()) { out_core.send("slot", (long)sl); return; }
			char c[16]; if (hasCrc) snprintf(c, sizeof c, "%08X", (unsigned)crc); else snprintf(c, sizeof c, "-");
			atoms a {symbol("slot"), (long)sl, symbol(path), symbol(c), (long)rate, (long)ch}; out_core.send(a);
		};
		m_sync.info = [this](const std::string& s) { atoms a {symbol("info")}; a.push_back(symbol(s)); out_info.send(a); };
		m_c.onSynced = [this]() {
			out_info.send("synced", (long)m_c.params.size(), (long)m_c.manifest.size()); report_state();
			if (m_autopull && !m_synced_once_pull) { m_synced_once_pull = true; m_c.pullPresets(); start_pull(m_c.hello["mode"] == "EDIT"); }
			push_slots();      // after the pull queue exists: a missing sample is not queued twice
		};
		m_c.onSchema = [this]() {
			out_info.send("fw", symbol(m_c.hello.count("fw") ? m_c.hello["fw"] : "?"), symbol(m_c.hello.count("proto") ? m_c.hello["proto"] : "?"));   // informational only: never a reason to refuse      // PMOR's schema by id: devices disable controls PMOR lacks and may show unknown ids generically
			out_info.send("schemabegin", (long)m_c.schema.size());
			for (auto& kv : m_c.schema) { auto& e = kv.second; out_info.send("schema", (long)e.id, symbol(e.name), (double)e.min, (double)e.max, (double)e.def, (long)e.flags, symbol(e.group.empty() ? "-" : e.group)); }
			out_info.send("schemaend", (long)m_c.schema.size());
		};
		m_c.onPool = [this]() { m_sync.manifestReloaded(); emit_pool(); push_slots(); };      // a reloaded manifest = new epoch: legacy ids are re-pulled, pool files re-verified
		m_c.onMode = [this](const std::string& m) { out_info.send("mode", symbol(m)); if (m_autopull && m_c.state() == pmorhost::Client::READY) start_pull(m == "EDIT"); };   // EDIT: pull ALL samples, STUDIO: the current patch
		m_c.onPatchSwitched = [this](int) { if (m_autopull) start_pull(m_c.hello["mode"] == "EDIT"); push_slots(); };   // patch switch / new recording: pull its samples in the background (progress messages)
		m_c.onJoin = [this](bool ok, int spp, const std::string& x) { if (ok) out_info.send("joined", (long)spp, symbol(x)); else out_info.send("joinerr", (long)spp, symbol(x)); };
		m_c.onAssigned = [this](int slot, bool ok, const std::string& code) {
			if (ok) out_info.send("assigned", (long)slot, symbol("ok")); else out_info.send("assigned", (long)slot, symbol("err"), symbol(code));
			// ok: the host already re-read GET_PATCH + GET_MANIFEST (SAMPLE_READY), the slot map is PMOR's; pull what is missing, then map
			if (ok) { if (m_autopull) start_pull(m_c.hello["mode"] == "EDIT"); push_slots(); }
		};
		m_c.onSnapshot = [this]() { m_snapDirty = true; };      // debounced in the 100 ms tick: `snapshot <text>` -> the device stores it in the Live set (pattr)
		m_c.onResync = [this](const std::string& k, int a, int b, int c, int d, int e) { out_info.send("snapstate", symbol(k), (long)a, (long)b, (long)c, (long)d, (long)e); };
		m_c.onNoBin = [this]() { out_info.send("nobin"); };
		m_c.onDeleted = [this](int id, bool ok, const std::string& code) { if (ok) out_info.send("deleted", (long)id, symbol("ok")); else out_info.send("deleted", (long)id, symbol("err"), symbol(code)); };
		m_c.onBinRestored = [this](int id, bool ok, const std::string& code) { if (ok) out_info.send("binrestored", (long)id, symbol("ok")); else out_info.send("binrestored", (long)id, symbol("err"), symbol(code)); };
		m_c.onBinEmptied = [this](bool ok, const std::string& x) { if (ok) out_info.send("binemptied", symbol("ok"), (long)atoi(x.c_str())); else out_info.send("binemptied", symbol("err"), symbol(x)); };
		m_c.onBinList = [this](const std::vector<pmorhost::Client::BinEntry>& v) {
			out_info.send("binbegin");
			for (auto& b : v) out_info.send("bin", (long)b.id, (long)b.len, symbol(b.name), symbol(b.age), (long)b.boots);
			out_info.send("binend", (long)v.size());
		};
		m_c.onPresets = [this]() {
			out_info.send("presetbegin");
			for (auto& p : m_c.presets) out_info.send("preset", (long)p.engine, (long)p.n, symbol(std::string(1, p.kind)), symbol(p.name));
			out_info.send("presetend", (long)m_c.presets.size());
		};
		m_c.onPresetSaved = [this](int e, int n, bool ok, const std::string& code) { if (ok) out_info.send("presetsaved", (long)e, (long)n, symbol("ok")); else out_info.send("presetsaved", (long)e, (long)n, symbol("err"), symbol(code)); };
		m_c.onStore = [this](int st, int pend, int err, int pool) { out_info.send("store", (long)st, (long)pend, (long)err, (long)pool); };
		m_c.onPatch = [this](int slot) { out_info.send("patch", (long)slot); };
		m_c.onStatus = [this](const std::map<std::string, std::string>& m) {
			auto g = [&](const char* k) { auto it = m.find(k); return it == m.end() ? std::string("-") : it->second; };
			atoms a {symbol("status"), symbol(g("mode")), symbol(g("policy") != "-" ? g("policy") : g("clk")), (long)atoi(g("running").c_str()), (double)atof(g("bpm").c_str()),
			         (long)atoi(g("cpu").c_str()), (long)atoi(g("audio_overruns").c_str()), (long)atoi(g("midi_drops").c_str()), (long)atoi(g("bus").c_str()), symbol(g("role"))};
			out_info.send(a);
		};
		m_c.haveSample = [this](const pmorhost::Client::Sample& sm) {
			bool have = m_sync.have(sm);
			if (have) out_info.send("sample", symbol(sm.name), symbol("cached"));
			return have;
		};
		m_c.onSampleProgress = [this](const pmorhost::Client::Sample& sm, uint32_t d, uint32_t t, int left) {
			if (d != 1 || t != 1) out_info.send("progress", symbol(sm.name), (long)d, (long)t, (long)left);
		};
		m_c.onSampleDone = [this](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>& data, bool ok) {
			if (m_sync.done(sm, data, ok)) { out_info.send("sample", symbol(sm.name), symbol("pulled")); push_slots(); }
			else out_info.send("sample", symbol(sm.name), symbol("bad"));
		};
	}

	message<> m_int {this, "int", "one byte from the port",
		MIN_FUNCTION { char ch = (char)(int)args[0]; m_c.feed(&ch, 1); report_state(); return {}; }};

	message<> m_list {this, "list", "bytes from the port",
		MIN_FUNCTION { std::string s; for (auto& a : args) s.push_back((char)(int)a); m_c.feed(s.data(), s.size()); report_state(); return {}; }};

	message<> m_connect {this, "hello", "hello <STUDIO|EDIT|NEPTR>",
		MIN_FUNCTION {
			std::string mode = args.empty() ? "STUDIO" : std::string(symbol(args[0]));
			if (mode != "STUDIO" && mode != "EDIT" && mode != "NEPTR") { cerr << "pmor.proto: mode must be STUDIO, EDIT or NEPTR" << endl; return {}; }
			start_tick();
			if (m_c.state() == pmorhost::Client::READY && m_c.hello["mode"] != mode) { m_c.switchMode(mode); return {}; }   // already connected: mode change only, no resync
			m_synced_once_pull = false; m_c.connect(mode); report_state(); return {};
		}};

	message<> m_findport {this, "findport", MIN_FUNCTION {
		char l = 0; std::string path;
		if (pmorfind::find(l, path)) { atoms a {symbol("pmorport"), symbol(std::string(1, l)), symbol(path)}; out_info.send(a); }
		else out_info.send("pmorport", symbol("none"));
		return {};
	}};
	message<> m_bye {this, "bye", MIN_FUNCTION { m_c.bye(); if (m_snapDirty) flush_snapshot(); return {}; }};
	message<> m_closed {this, "closed", MIN_FUNCTION { m_c.portClosed(); report_state(); if (m_snapDirty) flush_snapshot(); return {}; }};
	// The set holds the current patch (see pmor_host.h "Host snapshot"). `snapload <text>` = the stored value coming back from the Live set (never writes to PMOR); `snapshot <text>`
	// goes out whenever the held copy changed (<= 2 per second). snapadopt / snapwrite = the two buttons shown when PMOR is on another patch than the set; snapclear forgets the copy.
	message<> m_snapload {this, "snapload", "snapload <text>", MIN_FUNCTION { if (!args.empty() && args[0].type() == c74::min::message_type::symbol_argument) m_c.snapshotLoad(std::string(symbol(args[0]))); return {}; }};
	message<> m_snapadopt {this, "snapadopt", MIN_FUNCTION { m_c.snapAdopt(); return {}; }};
	message<> m_snapwrite {this, "snapwrite", MIN_FUNCTION { m_c.snapWrite(); return {}; }};
	message<> m_snapclear {this, "snapclear", MIN_FUNCTION { m_c.snapClear(); flush_snapshot(); return {}; }};
	message<> m_getpatch {this, "getpatch", MIN_FUNCTION { m_c.getPatch(); return {}; }};
	message<> m_setparam {this, "setparam", "setparam <id> <value>",
		MIN_FUNCTION {
			if (args.size() < 2) { cerr << "pmor.proto: setparam <id> <value>" << endl; return {}; }
			m_c.setParam((uint16_t)(int)args[0], (float)(double)args[1]); return {};
		}};

	message<> m_sessiondir {this, "sessiondir", "sessiondir <path>",
		MIN_FUNCTION { m_cachev.session = args.empty() ? std::string() : pmorcache::Cache::expand(std::string(symbol(args[0]))); push_slots(); return {}; }};      // session-vs-cache precedence never matters: same id + crc = same content, wrong-crc files are never used
	message<> m_cachedir {this, "cachedir", "cachedir <path>",
		MIN_FUNCTION { m_cachev.cache = args.empty() ? std::string() : pmorcache::Cache::expand(std::string(symbol(args[0]))); push_slots(); return {}; }};
	message<> m_pull {this, "pull", "pull current|all|off",
		MIN_FUNCTION {
			std::string w = args.empty() ? "current" : std::string(symbol(args[0]));
			if (w == "off") m_c.cancelPulls(); else if (w == "current" || w == "all") start_pull(w == "all");
			else cerr << "pmor.proto: pull current|all|off" << endl;
			return {};
		}};
	message<> m_pullid {this, "pullid", "pullid <id>",
		MIN_FUNCTION { if (args.empty()) { cerr << "pmor.proto: pullid <id>" << endl; return {}; } m_c.pullId((int)args[0]); return {}; }};
	message<> m_assign {this, "assign", "assign <slot> <id>",
		MIN_FUNCTION { if (args.size() < 2) { cerr << "pmor.proto: assign <slot> <id>" << endl; return {}; } m_c.assign((int)args[0], (int)args[1]); return {}; }};
	message<> m_deletepool {this, "deletepool", "deletepool <id> [<id> ...]",
		MIN_FUNCTION { std::vector<int> ids; for (auto& a : args) ids.push_back((int)a); if (ids.empty()) { cerr << "pmor.proto: deletepool <id> [<id> ...]" << endl; return {}; } m_c.deleteSamples(ids); return {}; }};
	message<> m_bin {this, "bin", MIN_FUNCTION { m_c.binList(); return {}; }};
	message<> m_binrestore {this, "binrestore", "binrestore <id>", MIN_FUNCTION { if (args.empty()) { cerr << "pmor.proto: binrestore <id>" << endl; return {}; } m_c.binRestore((int)args[0]); return {}; }};
	message<> m_binempty {this, "binempty", MIN_FUNCTION { m_c.binEmpty(); return {}; }};
	message<> m_tempo {this, "tempo", "tempo <bpm>", MIN_FUNCTION { if (!args.empty()) m_c.setTempo((double)args[0]); return {}; }};
	message<> m_slots {this, "slots", MIN_FUNCTION { push_slots(); return {}; }};
	message<> m_pool {this, "pool", MIN_FUNCTION { emit_pool(); return {}; }};
	message<> m_presets {this, "presets", MIN_FUNCTION { m_c.pullPresets(); return {}; }};
	message<> m_preset {this, "preset", "preset <engine> <n> [pad]",
		MIN_FUNCTION { if (args.size() < 2) { cerr << "pmor.proto: preset <engine> <n> [pad]" << endl; return {}; } m_c.applyPreset((int)args[0], (int)args[1], args.size() > 2 ? (int)args[2] : -1); return {}; }};
	message<> m_savepreset {this, "savepreset", "savepreset <engine> <n> <name>",      // name: `_` stands for a space (one symbol from pmor_browser.js); any further atoms are joined with a space
		MIN_FUNCTION { if (args.size() < 3) { cerr << "pmor.proto: savepreset <engine> <n> <name>" << endl; return {}; }
			std::string nm; for (size_t i = 2; i < args.size(); i++) { std::string t; if (args[i].type() == c74::min::message_type::int_argument) t = std::to_string((long)args[i]); else t = std::string(symbol(args[i]));
				for (char& ch : t) if (ch == '_') ch = ' '; nm += (i > 2 ? " " : "") + t; }
			m_c.savePreset((int)args[0], (int)args[1], nm); return {}; }};
	message<> m_mode {this, "mode", "mode STUDIO|EDIT", MIN_FUNCTION { if (!args.empty()) m_c.switchMode(std::string(symbol(args[0]))); return {}; }};
	message<> m_joinprep {this, "joinprep", MIN_FUNCTION { m_c.joinPrep(); return {}; }};
	message<> m_join {this, "join", "join <spp16>", MIN_FUNCTION { if (args.empty()) { cerr << "pmor.proto: join <spp16>" << endl; return {}; } m_c.join((int)args[0]); return {}; }};
	message<> m_dumpparams {this, "dumpparams", MIN_FUNCTION { m_c.redumpParams(); return {}; }};
	message<> m_storeretry {this, "storeretry", MIN_FUNCTION { m_c.storeRetry(); return {}; }};
	message<> m_status {this, "status", MIN_FUNCTION { m_c.pollStatus(); return {}; }};
	attribute<bool> autopull {this, "autopull", true, description {"pull samples automatically after a sync (STUDIO: current patch, EDIT: all)"},
		setter { MIN_FUNCTION { m_autopull = (bool)args[0]; return args; } }};

	// The liveness timer (100 ms: client tick = PING every 1.5 s, request timeouts, STATUS poll). It must be started per INSTANCE: the Min message
	// "maxclass_ready" is class-level and never fired for it, so earlier builds never ticked (no PING: PMOR drops to STANDALONE after 5 s).
	// "setup" is Min's per-instance post-construction message; `hello` also starts it (guarded), so a missed setup cannot leave it off.
	timer<> m_tick {this, MIN_FUNCTION { m_c.tick(100); report_state(); if (m_snapDirty && ++m_snapAge >= 5) flush_snapshot(); m_tick.delay(100); return {}; }};
	message<> m_setup {this, "setup", MIN_FUNCTION { start_tick(); return {}; }};

private:
	pmorhost::Client m_c;
	bool m_autopull {true}, m_synced_once_pull {false};
	bool m_ticking {false}, m_snapDirty {false}; int m_snapAge {0};
	void flush_snapshot() { m_snapDirty = false; m_snapAge = 0; std::string t = m_c.snapshotText(); if (!t.empty()) out_info.send("snapshot", symbol(t)); }
	void start_tick() { if (!m_ticking) { m_ticking = true; m_tick.delay(100); } }

	pmorcache::Cache m_cachev;
	pmorslots::Sync m_sync {m_c, m_cachev};
	// Sampler slot -> cached sample file -> pmor.core~ (outlet 1, same wire as `param`): `slot <n> <path> <crc32hex|-> <rate> <ch>`, or `slot <n>` = silence. All of the mapping /
	// verification / re-pull logic lives in pmor_slotsync.h (tested without Max); not READY -> every slot is emptied.
	void push_slots() { m_sync.push(); }
	void emit_pool() {
		out_info.send("poolbegin");
		auto v = m_c.pool();
		for (auto& sm : v) {
			std::string sl; for (int x : m_c.usedBy(sm.id)) { if (!sl.empty()) sl += ","; sl += std::to_string(x); }
			int pn = -1, ps = -1; std::string pat = "-";
			if (sm.id < 1024) { if (sscanf(sm.name.c_str(), "P%dS%d", &pn, &ps) == 2 && pn >= 0) pat = std::to_string(pn); else pat = std::to_string(sm.id / 16); }
			// crc: pool ids only (>= 1024 carry the real CRC-32 of the PCM); legacy ids have no trusted crc -> "-" (never grouped). Prefixed `x` so Max never reads it as a number.
			char cb[16]; if (sm.id >= 1024 && sm.crc) snprintf(cb, sizeof cb, "x%08X", (unsigned)sm.crc); else snprintf(cb, sizeof cb, "-");
			out_info.send("pool", (long)sm.id, (long)sm.len, symbol(sm.name), (long)m_sync.state(sm), symbol(sl.empty() ? "-" : sl), symbol(pat), (long)(sm.rate ? sm.rate : 48000), symbol(cb), (long)(m_c.slotsOk() ? 1 : 0));
		}
		out_info.send("poolend", (long)v.size());
	}
	void start_pull(bool all) { if (m_c.state() == pmorhost::Client::READY) m_c.pullSamples(all); }
	int m_last_state {-1};
	void report_state() {
		if ((int)m_c.state() != m_last_state) {
			m_last_state = (int)m_c.state(); out_info.send("state", symbol(m_c.stateName()));
			if (m_c.state() != pmorhost::Client::READY) { m_sync.manifestReloaded(); m_sync.blankAll(); }      // link not READY: PMOR's slot contents are unknown, the core plays nothing
		}
	}
};

MIN_EXTERNAL(pmor_proto);
