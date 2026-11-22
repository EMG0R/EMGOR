// proto.cpp -- the CDC line protocol, see docs/PROTOCOL.md (the contract) and proto.h.
// All state is static; everything runs in the CONTROL plane (main loop). Zero cost while no host is connected.
#include "proto.h"
#include "proto_util.h"
#include "params.h"
#include "param.h"
#include "core_config.h"
#include "shared_state.h"
#include "sequencer.h"
#include "sampler.h"
#include "synth.h"
#include "drums.h"
#include "clock.h"
#include "patch_format.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

namespace {
const ProtoOps* O = nullptr;

#define LINE_MAX_RX   512
#define TXQ_N         16384
#define CHUNK_MAX     1024     // raw bytes per C/F chunk frame (base64 1368 chars + header: one line <= ~1450 B)
#define SBODY_N       1600
#define TOK_MAX       12
#define LIVENESS_MS   5000u
#define PUSH_GAP_MS   5u
#define BULK_GAP_MS   0u
#define TX_STALL_MS   2000u    // no TX progress this long while connected -> drop the queue + session (self-heal)
#define BURST_MS      16u      // a bulk job may keep the loop up to this long per pass, servicing USB/MIDI between chunks

// ---------------------------------------------------------------- state
ProtoMode s_mode = PM_STANDALONE;
bool      s_conn = false;
uint32_t  s_last_rx = 0; bool s_have_rx = false;
uint32_t  s_schema_hash = 0;
char      s_rx[LINE_MAX_RX]; uint16_t s_rx_n = 0; bool s_rx_over = false;
uint8_t   SDRAM_BSS s_txq[TXQ_N]; uint32_t s_tx_r = 0, s_tx_w = 0;
float     SDRAM_BSS s_shadow[768]; bool s_shadow_ok = false; size_t s_scan = 0; uint32_t s_last_push_scan = 0;
uint16_t  s_preset_dirty = 0;
uint32_t  s_epoch = 0; uint8_t s_slot_seen = 0xFF;
uint32_t  s_seq_hash[3] = {0, 0, 0}; uint32_t s_seq_stamp = 0; bool s_seq_dirty[3] = {false, false, false};
bool      s_patch_changed = false;
uint16_t  s_sample_ready = 0, s_sample_err = 0; uint8_t s_sample_errcode[16];
uint32_t  s_nak_win = 0; uint8_t s_nak_n = 0;
uint32_t  s_last_chunk_ms = 0; uint32_t s_chunks = 0;

enum JobKind : uint8_t { J_NONE = 0, J_SCHEMA, J_PARAMS, J_SEQ, J_MANIFEST, J_PULL, J_PATCH, J_PULLF, J_MAN2, J_JOIN, J_BINDEL, J_BINLIST, J_ARCH, J_ARCHLIST };
static uint32_t SDRAM_BSS s_arch_hash; static uint16_t SDRAM_BSS s_arch_n;
struct Job { JobKind kind; char tag[5]; int a, b, c; uint32_t u, v; uint8_t sub; } job;

// ---------------------------------------------------------------- tx ring
bool s_lead_nl = false;                          // next frame goes out after a '\n' (session reset / HELLO flush)
uint32_t s_tx_prog = 0, s_tx_stalls = 0;           // last TX progress (ms); watchdog fires
inline uint32_t txFree() { return TXQ_N - (s_tx_w - s_tx_r); }
void txPut(const char* p, size_t n) { for (size_t i = 0; i < n; i++) s_txq[(s_tx_w++) % TXQ_N] = (uint8_t)p[i]; }
void txFlush() {
    while (s_tx_r != s_tx_w) {
        uint32_t used = s_tx_w - s_tx_r, off = s_tx_r % TXQ_N, run = TXQ_N - off;
        if (run > used) run = used;
        size_t sp = O->tx_space();
        if (sp == 0) return;
        if (run > sp) run = sp;
        size_t got = O->tx(s_txq + off, run);
        s_tx_r += (uint32_t)got;
        if (got) s_tx_prog = O->now_ms();
        if (got < run) return;
    }
}
// !TAG BODY*CRC\n ; returns false (nothing queued) if the ring cannot hold it
bool sendf(const char* tag, const char* fmt, ...) {
    static char SDRAM_BSS body[SBODY_N];      // not on the stack (DTCM headroom): control plane only, never re-entered
    int n = snprintf(body, sizeof body, "%s ", tag);
    va_list ap; va_start(ap, fmt);
    int m = vsnprintf(body + n, sizeof body - (size_t)n, fmt, ap);
    va_end(ap);
    if (m < 0) return false;
    size_t len = (size_t)n + (size_t)m; if (len > sizeof body - 1) len = sizeof body - 1;
    char tail[8];
    snprintf(tail, sizeof tail, "*%04X\n", (unsigned)pmorCrc16(body, len));
    if (txFree() < len + 9) return false;
    if (s_lead_nl) { char nl = '\n'; txPut(&nl, 1); s_lead_nl = false; }   // stale bytes of an old session may still sit in the transport FIFO ending mid-line: terminate that line so the first frame of the new session is not glued to it (hosts skip empty lines)
    char bang = '!'; txPut(&bang, 1); txPut(body, len); txPut(tail, 6);
    return true;
}
inline bool room(uint32_t n) { return txFree() >= n; }

// ---------------------------------------------------------------- schema hash
uint32_t computeSchemaHash() {
    uint32_t h = 2166136261u; char line[160];
    for (size_t i = 0; i < pmorSchemaCount(); i++) {
        const PmorParamDesc* d = pmorSchemaAt(i);
        char a[24], b[24], c[24];
        pmorFmtFloat(a, d->min); pmorFmtFloat(b, d->max); pmorFmtFloat(c, d->def);
        int n = snprintf(line, sizeof line, "%u %s %s %s %s %u %s\n", (unsigned)d->id, d->name, a, b, c, (unsigned)d->flags, d->group);
        h = pmorFnv1a(h, line, (size_t)n);
    }
    return h;
}

// ---------------------------------------------------------------- mode
void setMode(ProtoMode m) {
    if (m == s_mode) return;
    s_mode = m;
    job.kind = J_NONE;
    if (m >= PM_STUDIO) { s_shadow_ok = false; }
    if (O->mode_changed) O->mode_changed(m);
}
enum { RC_DTR_FALL = 0, RC_DTR_RISE, RC_USB, RC_LIVE, RC_BYE, RC_STALL, RC_HELLO, RC_N };
uint32_t s_resets[RC_N];
void goStandalone();
void resetSession(int cause) {      // one clean re-entry: queue, job, partial RX line, mode, transport FIFO all dropped; counted by cause
    if (cause >= 0 && cause < RC_N) s_resets[cause]++;
    goStandalone();
    if (O && O->tx_reset) O->tx_reset();
    s_tx_prog = O ? O->now_ms() : 0;
}
void goStandalone() {
    s_have_rx = false; s_lead_nl = true;
    s_tx_r = s_tx_w = 0; job.kind = J_NONE; s_rx_n = 0; s_rx_over = false;
    s_patch_changed = false; s_sample_ready = s_sample_err = 0;
    setMode(PM_STANDALONE);
}

// ---------------------------------------------------------------- command table
struct Cmd { const char* name; uint8_t modes; };       // allowed-mode bitmask
#define M_NEP (1u << PM_NEPTR)
#define M_STU (1u << PM_STUDIO)
#define M_EDI (1u << PM_EDIT)
#define M_ALL (M_NEP | M_STU | M_EDI)
const Cmd CMDS[] = {
    {"GET_SCHEMA", M_ALL}, {"GET_PARAMS", M_ALL}, {"GET_SEQ", M_ALL}, {"GET_MANIFEST", M_ALL}, {"GET_PATCH", M_ALL}, {"STATUS", M_ALL},
    {"SET_PARAM", M_STU | M_EDI}, {"AUDITION", M_STU | M_EDI}, {"PULL_SAMPLE", M_STU | M_EDI},
    {"PULL_FILE", M_STU | M_EDI}, {"SAVE_SCRATCH", M_EDI}, {"JOIN", M_STU | M_EDI}, {"STORE_RETRY", M_STU | M_EDI}, {"TEMPO", M_STU | M_EDI}, {"SAVE_PRESET", M_EDI}, {"DELETE_POOL", M_EDI}, {"BIN_LIST", M_EDI}, {"BIN_RESTORE", M_EDI}, {"BIN_EMPTY", M_EDI}, {"ARCHIVE_PATCHES", M_EDI}, {"ARCHIVE_LIST", M_STU | M_EDI}, {"NOSAVE", M_STU | M_EDI}, {"ASSIGN_SAMPLE", M_EDI}, {"DELETE_SAMPLE", M_EDI}, {"CLEAR_PATCH", M_EDI}, {"CLOCKMODE", M_EDI},
};
const Cmd* findCmd(const char* n) { for (auto& c : CMDS) if (!strcmp(c.name, n)) return &c; return nullptr; }

long toL(const char* s, bool* ok) { char* e; long v = strtol(s, &e, 10); if (e == s || *e) { *ok = false; return 0; } return v; }
bool validTag(const char* t) { size_t n = strlen(t); if (n < 1 || n > 4) return false; for (; *t; t++) if (!((*t >= '0' && *t <= '9') || (*t >= 'A' && *t <= 'Z') || (*t >= 'a' && *t <= 'z') || *t == '-')) return false; return true; }

void startJob(JobKind k, const char* tag, int a = 0, int b = 0, int c = 0, uint32_t u = 0) {
    job.kind = k; strncpy(job.tag, tag, 4); job.tag[4] = 0; job.a = a; job.b = b; job.c = c; job.u = u; job.v = 0; job.sub = 0;
}

void handle(char** t, int nt) {
    const char* tag = t[0]; const char* cmd = nt > 1 ? t[1] : "";
    if (nt < 2) { sendf(tag, "ERR ? arg"); return; }
    uint32_t now = O->now_ms();
    s_last_rx = now; s_have_rx = true;
    bool ok = true;

    if (!strcmp(cmd, "HELLO")) {
        ProtoMode m = PM_STANDALONE;
        if (nt >= 3 && !strcmp(t[2], "NEPTR")) m = PM_NEPTR;
        else if (nt >= 3 && !strcmp(t[2], "STUDIO")) m = PM_STUDIO;
        else if (nt >= 3 && !strcmp(t[2], "EDIT")) m = PM_EDIT;
        else { sendf(tag, "ERR HELLO arg"); return; }
        // A fresh HELLO proves the host is alive and wants a NEW session: abort any running multi-line job, drop the stale TX backlog and
        // the push shadow, so the reply goes out within one poll (never queued behind 16 KB of an old GET_PARAMS) in every state.
        s_lead_nl = true;
        if (job.kind != J_NONE || s_tx_r != s_tx_w) { s_resets[RC_HELLO]++; job.kind = J_NONE; s_tx_r = s_tx_w = 0; if (O->tx_reset) O->tx_reset(); }
        s_shadow_ok = false; s_patch_changed = false; s_preset_dirty = 0;
        setMode(m);
        s_schema_hash = computeSchemaHash();
        s_slot_seen = O->patch_slot(); s_epoch = O->patch_epoch();
        static char SDRAM_BSS st[1100]; st[0] = 0; if (O->status) O->status(st, sizeof st);
        sendf(tag, "OK HELLO fw=%s proto=1 mode=%s schema=%08X patch=%u sr=48000 block=64 %s", O->fw, protoModeName(m), (unsigned)s_schema_hash, (unsigned)O->patch_slot(), st);
        return;
    }
    if (!strcmp(cmd, "BYE")) { sendf(tag, "OK BYE"); txFlush(); s_resets[RC_BYE]++; goStandalone(); return; }
    if (!strcmp(cmd, "PING")) { sendf(tag, "OK PING up=%lu", (unsigned long)now); return; }

    const Cmd* c = findCmd(cmd);
    if (!c) { sendf(tag, "ERR %s unknown", cmd); return; }
    if (s_mode == PM_STANDALONE) { sendf(tag, "ERR %s nohello", cmd); return; }
    if (!((c->modes >> s_mode) & 1u)) { sendf(tag, "ERR %s mode", cmd); return; }
    bool multi = !strcmp(cmd, "GET_SCHEMA") || !strcmp(cmd, "GET_PARAMS") || !strcmp(cmd, "GET_SEQ") || !strcmp(cmd, "GET_MANIFEST") ||
                 !strcmp(cmd, "GET_PATCH") || !strcmp(cmd, "PULL_SAMPLE") || !strcmp(cmd, "PULL_FILE") ||
                 !strcmp(cmd, "DELETE_POOL") || !strcmp(cmd, "BIN_LIST") || !strcmp(cmd, "ARCHIVE_PATCHES") || !strcmp(cmd, "ARCHIVE_LIST");
    if (multi && job.kind != J_NONE) { sendf(tag, "ERR %s busy", cmd); return; }

    if (!strcmp(cmd, "GET_SCHEMA"))      startJob(J_SCHEMA, tag);
    else if (!strcmp(cmd, "GET_PARAMS")) startJob(J_PARAMS, tag);
    else if (!strcmp(cmd, "GET_MANIFEST")) startJob((O->man_count && O->man_count() > 0) ? J_MAN2 : J_MANIFEST, tag);
    else if (!strcmp(cmd, "GET_PATCH"))  startJob(J_PATCH, tag);
    else if (!strcmp(cmd, "GET_SEQ")) {
        long e = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || e < 0 || e >= ENG_COUNT) { sendf(tag, "ERR GET_SEQ arg"); return; }
        startJob(J_SEQ, tag, (int)e);
    }
    else if (!strcmp(cmd, "DELETE_POOL")) {
        long id = nt >= 3 ? toL(t[2], &ok) : -1; bool force = nt >= 4 && !strcmp(t[3], "force");
        if (!ok || id < 0 || id > 65535 || (nt >= 4 && !force) || !O->bin_delete || !O->refs_scan) { sendf(tag, "ERR DELETE_POOL arg"); return; }
        startJob(J_BINDEL, tag, (int)id, force ? 1 : 0, 0, 0);
    }
    else if (!strcmp(cmd, "BIN_LIST")) { if (!O->bin_count) { sendf(tag, "ERR BIN_LIST arg"); return; } startJob(J_BINLIST, tag); }
    else if (!strcmp(cmd, "BIN_RESTORE")) {
        long id = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || id < 0 || id > 65535 || !O->bin_restore) { sendf(tag, "ERR BIN_RESTORE arg"); return; }
        int r = O->bin_restore((uint16_t)id);
        if (r == 0) sendf(tag, "OK BIN_RESTORE id=%ld", id); else if (r == -1) sendf(tag, "ERR BIN_RESTORE notfound"); else if (r == -2) sendf(tag, "ERR BIN_RESTORE exists"); else sendf(tag, "ERR BIN_RESTORE io");
    }
    else if (!strcmp(cmd, "BIN_EMPTY")) {
        unsigned long tok = 0; char* e = nullptr; if (nt >= 3) tok = strtoul(t[2], &e, 16);
        if (nt < 3 || !e || *e || !O->bin_empty) { sendf(tag, "ERR BIN_EMPTY arg"); return; }
        int r = O->bin_empty((uint32_t)tok);
        if (r >= 0) sendf(tag, "OK BIN_EMPTY n=%d", r); else if (r == -1) sendf(tag, "ERR BIN_EMPTY token"); else sendf(tag, "ERR BIN_EMPTY io");
    }
    else if (!strcmp(cmd, "ARCHIVE_PATCHES")) {    // no arg: scan + token; <token>: verify the token, then START the device-owned background job (a = token, 0 = none; u = slot cursor of the scan)
        unsigned long tok = 0; char* e = nullptr; if (nt >= 3) tok = strtoul(t[2], &e, 16);
        if (nt > 3 || (nt == 3 && (!e || *e || !tok)) || !O->arch_scan || !O->arch_begin || !O->arch_list || !O->arch_state) { sendf(tag, "ERR ARCHIVE_PATCHES arg"); return; }
        char st[160]; if (O->arch_state(st, sizeof st) == 1) { sendf(tag, "OK ARCHIVE_PATCHES running %s", st); return; }   // already moving: report, never restart
        startJob(J_ARCH, tag, (int)(uint32_t)tok, 0, 0, 0); s_arch_hash = 0x7A3C91E5u; s_arch_n = 0;
    }
    else if (!strcmp(cmd, "ARCHIVE_LIST")) {       // read-only: the patch files still on the card, per slot, + the job state
        if (nt != 2 || !O->arch_list || !O->arch_state) { sendf(tag, "ERR ARCHIVE_LIST arg"); return; }
        startJob(J_ARCHLIST, tag);
    }
    else if (!strcmp(cmd, "SAVE_PRESET")) {
        long e = nt >= 3 ? toL(t[2], &ok) : -1; long n = (ok && nt >= 4) ? toL(t[3], &ok) : -1;
        long pad = -1; if (ok && nt >= 6) { pad = toL(t[5], &ok); if (pad < 0 || pad > 15) ok = false; }
        if (!ok || nt < 5 || nt > 6 || !O->save_preset) { sendf(tag, "ERR SAVE_PRESET arg"); return; }
        char nm[10]; size_t k = 0; for (const char* q = t[4]; *q && k < 9; q++) nm[k++] = *q; nm[k] = 0;
        if (strlen(t[4]) > 9) { sendf(tag, "ERR SAVE_PRESET arg"); return; }
        int r = O->save_preset((int)e, (int)n, nm, (int)pad);
        if (r == -1) sendf(tag, "ERR SAVE_PRESET arg"); else if (r == -3) sendf(tag, "ERR SAVE_PRESET nosave"); else if (r != 0) sendf(tag, "ERR SAVE_PRESET io");
        else { protoPresetsChanged((int)e); sendf(tag, "OK SAVE_PRESET engine=%ld n=%ld name=%s%s%.0ld", e, n, nm, pad >= 0 ? " pad=" : "", pad >= 0 ? pad : 0L); }
    }
    else if (!strcmp(cmd, "TEMPO")) {
        long milli = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || milli < 40000 || milli > 400000) { sendf(tag, "ERR TEMPO arg"); return; }
        if (O->tempo_seed) O->tempo_seed((uint32_t)milli);
        sendf(tag, "OK TEMPO bpm=%ld", milli);
    }
    else if (!strcmp(cmd, "STATUS")) {
        static char SDRAM_BSS st[1100]; st[0] = 0; if (O->status) O->status(st, sizeof st);
        sendf(tag, "OK STATUS mode=%s %s", protoModeName(s_mode), st);
    }
    else if (!strcmp(cmd, "SET_PARAM")) {
        float v; long id = nt >= 4 ? toL(t[2], &ok) : -1;
        if (!ok || id < 0 || id > 0xFFFF || nt < 4 || !pmorParseFloat(t[3], &v)) { sendf(tag, "ERR SET_PARAM arg"); return; }
        int idx = pmorSchemaIndexOf((uint16_t)id);
        if (idx < 0) { sendf(tag, "ERR SET_PARAM unknown"); return; }
        if ((uint16_t)id == pmorParamId(PD_GLOBAL, 0) && O->follow_locked && O->follow_locked()) { sendf(tag, "ERR SET_PARAM follow"); return; }
        const PmorParamDesc* d = pmorSchemaAt((size_t)idx);
        if (v < d->min - 1e-6f || v > d->max + 1e-6f) { sendf(tag, "ERR SET_PARAM range"); return; }
        pmorParamSet((uint16_t)id, v);
        float st = 0.f; pmorParamGetAt((size_t)idx, &st);
        if (s_shadow_ok) s_shadow[idx] = st;                 // no echo back to the originator
        if (O->param_changed) O->param_changed();
        char fv[24]; pmorFmtFloat(fv, st);
        sendf(tag, "OK SET_PARAM id=%ld val=%s", id, fv);
    }
    else if (!strcmp(cmd, "AUDITION")) {
        long a = nt >= 5 ? toL(t[3], &ok) : -1; long vel = nt >= 5 ? toL(t[4], &ok) : -1;
        char k = nt >= 3 ? t[2][0] : 0;
        if (!ok || (k != 'D' && k != 'S' && k != 'M') || a < 0 || a > 15 || vel < 1 || vel > 127) { sendf(tag, "ERR AUDITION arg"); return; }
        O->audition(k, (uint8_t)a, (uint8_t)vel);
        sendf(tag, "OK AUDITION");
    }
    else if (!strcmp(cmd, "PULL_SAMPLE")) {
        long id = nt >= 5 ? toL(t[2], &ok) : -1; long off = nt >= 5 ? toL(t[3], &ok) : -1; long mx = nt >= 5 ? toL(t[4], &ok) : -1;
        long cnt = nt >= 6 ? toL(t[5], &ok) : 1;
        if (!ok || id < 0 || id > 65535 || off < 0 || (off & 1) || mx < 2 || mx > CHUNK_MAX || (mx & 1) || cnt < 1 || cnt > 16) { sendf(tag, "ERR PULL_SAMPLE arg"); return; }
        startJob(J_PULL, tag, (int)id, (int)off, (int)mx, (uint32_t)cnt);
    }
    else if (!strcmp(cmd, "PULL_FILE")) {      // PULL_FILE kind slot offset max [count]
        char k = nt >= 3 ? t[2][0] : 0;
        long sl = nt >= 6 ? toL(t[3], &ok) : -1; long off = nt >= 6 ? toL(t[4], &ok) : -1; long mx = nt >= 6 ? toL(t[5], &ok) : -1;
        long cnt = nt >= 7 ? toL(t[6], &ok) : 1;
        if (!ok || !strchr("PKALMTRW", k) || sl < 0 || sl > 65535 || off < 0 || mx < 1 || mx > CHUNK_MAX || cnt < 1 || cnt > 16) { sendf(tag, "ERR PULL_FILE arg"); return; }
        if (!O->file_read) { sendf(tag, "ERR PULL_FILE unknown"); return; }
        startJob(J_PULLF, tag, (int)sl, (int)off, (int)mx, (uint32_t)cnt); job.v = (uint32_t)k;
    }
    else if (!strcmp(cmd, "SAVE_SCRATCH")) {      // SAVE_SCRATCH slot : EDIT only; refuses a slot that already holds a patch
        long sl = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || sl < 0 || sl > 63 || !O->save_scratch) { sendf(tag, "ERR SAVE_SCRATCH arg"); return; }
        int r = O->save_scratch((uint8_t)sl);
        if (r == 0) sendf(tag, "OK SAVE_SCRATCH slot=%ld", sl); else sendf(tag, "ERR SAVE_SCRATCH %s", r == -1 ? "mode" : "io");
    }
    else if (!strcmp(cmd, "JOIN")) {      // JOIN <spp16>: the 0xF8 nearest this request's arrival is tick 0 of 16th <spp16>, running
        long sp = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || sp < 0 || sp > 16383) { sendf(tag, "ERR JOIN arg"); return; }
        if (!O->join_begin || O->join_begin((uint16_t)sp) != 0) { sendf(tag, "ERR JOIN noclock"); return; }
        if (job.kind != J_NONE) { sendf(tag, "ERR JOIN busy"); return; }
        startJob(J_JOIN, tag, (int)sp);
    }
    else if (!strcmp(cmd, "STORE_RETRY")) {
        if (!O->store_retry || O->store_retry() != 0) sendf(tag, "ERR STORE_RETRY io"); else sendf(tag, "OK STORE_RETRY");
    }
    else if (!strcmp(cmd, "NOSAVE")) {
        if (nt < 3) { sendf(tag, "ERR NOSAVE arg"); return; }
        if (O->nosave) O->nosave(t[2][0] == '1'); sendf(tag, "OK NOSAVE %c", t[2][0] == '1' ? '1' : '0');
    }
    else if (!strcmp(cmd, "ASSIGN_SAMPLE")) {
        long sl = nt >= 4 ? toL(t[2], &ok) : -1; long id = nt >= 4 ? toL(t[3], &ok) : -1;
        if (!ok || sl < 0 || sl > 15 || id < 0 || id > 65535) { sendf(tag, "ERR ASSIGN_SAMPLE arg"); return; }
        int r = O->assign((uint8_t)sl, (uint16_t)id);
        if (r == 0) sendf(tag, "OK ASSIGN_SAMPLE slot=%ld id=%ld", sl, id); else sendf(tag, "ERR ASSIGN_SAMPLE %s", r == -2 ? "busy" : "unknown");
    }
    else if (!strcmp(cmd, "DELETE_SAMPLE")) {
        long sl = nt >= 3 ? toL(t[2], &ok) : -1;
        if (!ok || sl < 0 || sl > 15) { sendf(tag, "ERR DELETE_SAMPLE arg"); return; }
        O->del((uint8_t)sl); sendf(tag, "OK DELETE_SAMPLE slot=%ld", sl);
    }
    else if (!strcmp(cmd, "CLEAR_PATCH")) {
        if (nt < 3 || strcmp(t[2], "YES")) { sendf(tag, "ERR CLEAR_PATCH arg"); return; }
        O->clear(); sendf(tag, "OK CLEAR_PATCH");
    }
    else if (!strcmp(cmd, "CLOCKMODE")) {
        int m = -1; if (nt >= 3) { if (!strcmp(t[2], "AUTO")) m = 0; else if (!strcmp(t[2], "INTERNAL")) m = 1; else if (!strcmp(t[2], "FOLLOW")) m = 2; else if (!strcmp(t[2], "MASTER")) m = 3; }
        if (m < 0) { sendf(tag, "ERR CLOCKMODE arg"); return; }
        O->set_clockmode((uint8_t)m); sendf(tag, "OK CLOCKMODE %s", t[2]);
    }
}

bool frameGood(const char* f, size_t n, const char** lastStar) {
    const char* last = nullptr;
    for (size_t i = 1; i < n; i++) if (f[i] == '*') last = f + i;
    if (!last || (size_t)(f + n - last) != 5) return false;
    char hx[5]; memcpy(hx, last + 1, 4); hx[4] = 0; char* e; unsigned want = (unsigned)strtoul(hx, &e, 16);
    if (*e) return false;
    if (pmorCrc16(f + 1, (size_t)(last - f - 1)) != (uint16_t)want) return false;
    *lastStar = last; return true;
}
void line() {
    s_rx[s_rx_n] = 0;
    if (s_rx_n > 0 && s_rx[s_rx_n - 1] == '\r') s_rx[--s_rx_n] = 0;
    if (s_rx_n == 0) return;
    // a frame starts at any '!' whose !BODY*CRC checks out: a partial line left over from before an unplug, glued in front of the
    // next request, must not eat that request (try every '!' from the left; the first good frame wins)
    const char* lastStar = nullptr; size_t at = 0; bool found = false;
    for (size_t i = 0; i < s_rx_n; i++) if (s_rx[i] == '!' && frameGood(s_rx + i, s_rx_n - i, &lastStar)) { at = i; found = true; break; }
    if (!found) {
        if (s_rx[0] != '!') { if (O->legacy_line) O->legacy_line(s_rx); return; }
        uint32_t now = O->now_ms();
        if (now - s_nak_win >= 1000u) { s_nak_win = now; s_nak_n = 0; }
        if (s_nak_n < 10) { s_nak_n++; sendf("-", "NAK bad_crc"); }
        return;
    }
    char* fr = s_rx + at; *(char*)lastStar = 0;
    char* t[TOK_MAX]; int nt = 0; char* p = fr + 1;
    while (*p && nt < TOK_MAX) { while (*p == ' ') p++; if (!*p) break; t[nt++] = p; while (*p && *p != ' ') p++; if (*p) *p++ = 0; }
    if (nt == 0 || !validTag(t[0])) { sendf("-", "NAK bad_frame"); return; }
    handle(t, nt);
}

// ---------------------------------------------------------------- jobs
bool seqHexLine(const char* tag, int e, uint32_t& off) {
    const LaneSet& l = seq.lane[e];
    uint32_t total = (uint32_t)l.bars * l.steps_per_bar; if (total > MAXSTEPS) total = MAXSTEPS;
    uint32_t n = total - off; if (n > 90) n = 90;
    char hex[90 * 4 + 1]; for (uint32_t i = 0; i < n; i++) snprintf(hex + i * 4, 5, "%04X", (unsigned)l.step[off + i]);
    hex[n * 4] = 0;
    if (!sendf(tag, "SEQ %d %u %u %u %u %s", e, (unsigned)l.bars, (unsigned)l.steps_per_bar, (unsigned)(l.mode_f + 0.5f), (unsigned)off, hex)) return false;
    off += n; return true;
}

bool paramsStep(const char* tag, uint32_t& idx) {      // one PARAMS line; true when this call finished the list
    char body[400]; int n = 0; size_t cnt = pmorSchemaCount();
    while (idx < cnt && n < 340) {
        const PmorParamDesc* d = pmorSchemaAt(idx); float v = 0; pmorParamGetAt(idx, &v);
        char fv[24]; pmorFmtFloat(fv, v);
        n += snprintf(body + n, sizeof body - (size_t)n, "%s%u=%s", n ? " " : "", (unsigned)d->id, fv);
        idx++;
    }
    body[n] = 0;
    sendf(tag, "PARAMS %s", body);
    return idx >= cnt;
}

void runJob() {
    if (job.kind == J_NONE || !room(1700)) return;
    switch (job.kind) {
    case J_SCHEMA: {
        size_t cnt = pmorSchemaCount();
        while (job.u < cnt && room(520)) {
            const PmorParamDesc* d = pmorSchemaAt(job.u); char a[24], b[24], c[24];
            pmorFmtFloat(a, d->min); pmorFmtFloat(b, d->max); pmorFmtFloat(c, d->def);
            sendf(job.tag, "SCHEMA %u %s %s %s %s %u %s", (unsigned)d->id, d->name, a, b, c, (unsigned)d->flags, d->group);
            job.u++;
        }
        if (job.u >= cnt) { sendf(job.tag, "OK GET_SCHEMA n=%u hash=%08X", (unsigned)cnt, (unsigned)s_schema_hash); job.kind = J_NONE; }
        break; }
    case J_PARAMS: {
        uint32_t idx = job.u; bool done = paramsStep(job.tag, idx); job.u = idx;
        if (done) { sendf(job.tag, "OK GET_PARAMS n=%u", (unsigned)pmorSchemaCount()); job.kind = J_NONE; }
        break; }
    case J_SEQ: {
        if (seqHexLine(job.tag, job.a, job.u)) {
            const LaneSet& l = seq.lane[job.a]; uint32_t total = (uint32_t)l.bars * l.steps_per_bar; if (total > MAXSTEPS) total = MAXSTEPS;
            if (job.u >= total) { sendf(job.tag, "OK GET_SEQ"); job.kind = J_NONE; }
        }
        break; }
    case J_PATCH: {      // sub 0: params, 1..3: seq lanes, 4: info
        if (job.sub == 0) { uint32_t idx = job.u; bool done = paramsStep(job.tag, idx); job.u = idx; if (done) { job.sub = 1; job.u = 0; job.v = 0; } }
        else if (job.sub <= 3) {
            int e = job.sub - 1; const LaneSet& l = seq.lane[e]; uint32_t total = (uint32_t)l.bars * l.steps_per_bar; if (total > MAXSTEPS) total = MAXSTEPS;
            if (seqHexLine(job.tag, e, job.u) && job.u >= total) { job.sub++; job.u = 0; }
        } else {
            char lens[16 * 8]; int n = 0; for (int s = 0; s < SMP_SLOTS; s++) n += snprintf(lens + n, sizeof lens - (size_t)n, "%s%lu", s ? "," : "", (unsigned long)smp.len[s]);
            char ids[16 * 6 + 2]; int m = 0; ids[0] = 0;
            if (O->alias) { for (int sl = 0; sl < SMP_SLOTS; sl++) m += snprintf(ids + m, sizeof ids - (size_t)m, "%s%u", sl ? "," : "", (unsigned)(smp.len[sl] ? O->alias((uint16_t)(O->patch_slot() * 16 + sl)) : 0)); }
            sendf(job.tag, "PATCHINFO slot=%u samples=%s%s%s", (unsigned)O->patch_slot(), lens, m ? " ids=" : "", ids);
            sendf(job.tag, "OK GET_PATCH slot=%u", (unsigned)O->patch_slot()); job.kind = J_NONE;
        }
        break; }
    case J_MANIFEST: {      // u = patch cursor, v = slot cursor, c = count
        while (job.u < PATCH_SLOTS && room(520)) {
            int r = O->pool_scan((uint8_t)job.u);
            if (r == 0) return;                                  // SD busy this pass: resume next poll
            if (r > 0) {
                while (job.v < SMP_SLOTS && room(520)) {
                    uint32_t len = O->pool_len((uint8_t)job.u, (uint8_t)job.v);
                    if (len) { sendf(job.tag, "M %u %lu 48000 1 %08lX P%02uS%02u", (unsigned)(job.u * 16 + job.v), (unsigned long)len,
                                     (unsigned long)O->pool_ver((uint8_t)job.u, (uint8_t)job.v), (unsigned)job.u, (unsigned)job.v); job.c++; }
                    job.v++;
                }
                if (job.v < SMP_SLOTS) return;
            }
            job.u++; job.v = 0;
        }
        if (job.u >= PATCH_SLOTS) { sendf(job.tag, "OK GET_MANIFEST n=%d", job.c); job.kind = J_NONE; }
        break; }
    case J_MAN2: {      // new-layout manifest: global pool ids
        int n = O->man_count();
        while (job.u < (uint32_t)n && room(520)) {
            uint16_t id; uint32_t len, rate, ver; uint8_t ch; char nm[16] = "";
            if (O->man_at((int)job.u, &id, &len, &rate, &ch, &ver, nm) && len)
                { sendf(job.tag, "M %u %lu %lu %u %08lX %s", (unsigned)id, (unsigned long)len, (unsigned long)rate, (unsigned)ch, (unsigned long)ver, nm); job.c++; }
            job.u++;
        }
        if (job.u >= (uint32_t)n) { sendf(job.tag, "OK GET_MANIFEST n=%d", job.c); job.kind = J_NONE; }
        break; }
    case J_BINDEL: {   // a = id, b = force, u = patch-slot cursor of the reference scan, c = refs found
        if (!job.b && job.u < PATCH_SLOTS) { job.c += O->refs_scan((uint16_t)job.a, (unsigned)job.u, 16); job.u += 16; return; }   // 16 patch headers per pass
        if (job.c > 0 && !job.b) { sendf(job.tag, "ERR DELETE_POOL referenced refs=%d", job.c); job.kind = J_NONE; return; }
        int r = O->bin_delete((uint16_t)job.a);
        if (r == 0) sendf(job.tag, "OK DELETE_POOL id=%d bin=1 refs=%d", job.a, job.c);
        else if (r == -1) sendf(job.tag, "ERR DELETE_POOL notfound"); else if (r == -2) sendf(job.tag, "ERR DELETE_POOL binfull");
        else if (r == -5) sendf(job.tag, "ERR DELETE_POOL nosave"); else sendf(job.tag, "ERR DELETE_POOL io");
        job.kind = J_NONE; return; }
    case J_ARCH: {
        if (job.u < PATCH_SLOTS) { if (O->arch_scan(job.u, 16, &s_arch_hash, &s_arch_n) < 0) { sendf(job.tag, "ERR ARCHIVE_PATCHES io"); job.kind = J_NONE; return; } job.u += 16; return; }   // scan 16 slots per pass
        uint32_t tk = s_arch_hash ? s_arch_hash : 1u;
        if (job.a == 0) { sendf(job.tag, "OK ARCHIVE_PATCHES token=%08lX patches=%u", (unsigned long)tk, (unsigned)s_arch_n); job.kind = J_NONE; return; }
        if ((uint32_t)job.a != tk) { sendf(job.tag, "ERR ARCHIVE_PATCHES token"); job.kind = J_NONE; return; }
        int k = O->arch_begin();                                           // starts the device-side job (one file per main-loop pass) and returns: it never depends on this session
        if (k < 0) sendf(job.tag, k == -5 ? "ERR ARCHIVE_PATCHES nosave" : "ERR ARCHIVE_PATCHES io");
        else sendf(job.tag, "OK ARCHIVE_PATCHES started dir=ARCHIVE_%d patches=%u (progress: ARCHIVE_LIST)", k, (unsigned)s_arch_n);
        job.kind = J_NONE; return; }
    case J_ARCHLIST: {   // u = slot cursor, c = files remaining, v = slots with files; 4 slots per pass
        if (!room(600)) return;
        if (job.u < PATCH_SLOTS) { for (int i = 0; i < 4 && job.u < PATCH_SLOTS; i++, job.u++) { char nm[96]; int n = O->arch_list((unsigned)job.u, nm, sizeof nm); if (n > 0) { sendf(job.tag, "A %u %s", (unsigned)job.u, nm); job.c += n; job.v++; } } return; }
        char st[200]; O->arch_state(st, sizeof st);
        sendf(job.tag, "OK ARCHIVE_LIST remaining=%d slots=%lu %s", job.c, (unsigned long)job.v, st); job.kind = J_NONE; return; }
    case J_BINLIST: {
        int n = O->bin_count();
        if (n < 0) { sendf(job.tag, "ERR BIN_LIST io"); job.kind = J_NONE; return; }
        int k = 0;
        while (job.u < (uint32_t)n && k < 4 && room(520)) {
            uint16_t id; uint32_t len, at; if (O->bin_at((int)job.u, &id, &len, &at)) sendf(job.tag, "B %u %lu S%04u %lu", (unsigned)id, (unsigned long)len, (unsigned)id, (unsigned long)at);
            job.u++; k++;
        }
        if (job.u >= (uint32_t)n) { sendf(job.tag, "OK BIN_LIST n=%d token=%08lX", n, (unsigned long)(O->bin_token ? O->bin_token() : 0)); job.kind = J_NONE; }
        return; }
    case J_JOIN: {
        int32_t late = 0; int r = O->join_poll ? O->join_poll(&late) : -2;
        if (r == 1) { char lb[16]; snprintf(lb, sizeof lb, "%ld", (long)late); sendf(job.tag, "OK JOIN spp16=%d tick_at=%s", job.a, lb); job.kind = J_NONE; }
        else if (r < 0) { sendf(job.tag, "ERR JOIN noclock"); job.kind = J_NONE; }
        return; }
    case J_PULL: {
        uint32_t now = O->now_ms();
        if (now - s_last_chunk_ms < BULK_GAP_MS || (O->bus_idle && !O->bus_idle())) return;      // clock + notes first
        static uint8_t SDRAM_BSS buf[CHUNK_MAX]; int32_t n = O->pool_read((uint16_t)job.a, (uint32_t)job.b, buf, (uint32_t)job.c);
        if (n == -2) return;                                     // SD not ready yet
        if (n < 0) { sendf(job.tag, "ERR PULL_SAMPLE io"); job.kind = J_NONE; return; }
        if (n > 0) {
            static char SDRAM_BSS b64[4 * ((CHUNK_MAX + 2) / 3) + 4]; size_t bl = pmorB64Encode(buf, (size_t)n, b64); b64[bl] = 0;
            sendf(job.tag, "C %d %d %d %08lX %s", job.a, job.b, (int)n, (unsigned long)pmorCrc32Of(buf, (size_t)n), b64);
            s_chunks++;
        }
        s_last_chunk_ms = now;
        if (n > 0 && (uint32_t)n == (uint32_t)job.c && job.u > 1) { job.b += n; job.u--; return; }   // more chunks owed: next pass
        sendf(job.tag, "OK PULL_SAMPLE id=%d offset=%d n=%d total=%lu", job.a, job.b, (int)n,
              (unsigned long)(((O->sample_len && O->sample_len((uint16_t)job.a)) ? O->sample_len((uint16_t)job.a) : O->pool_len((uint8_t)(job.a / 16), (uint8_t)(job.a % 16))) * 2u));
        job.kind = J_NONE;
        break; }
    case J_PULLF: {
        uint32_t now = O->now_ms();
        if (now - s_last_chunk_ms < BULK_GAP_MS || (O->bus_idle && !O->bus_idle())) return;
        static uint8_t SDRAM_BSS buf[CHUNK_MAX]; uint32_t total = 0;
        int32_t n = O->file_read((char)job.v, (uint16_t)job.a, (uint32_t)job.b, buf, (uint32_t)job.c, &total);
        if (n < 0) { sendf(job.tag, "ERR PULL_FILE io"); job.kind = J_NONE; return; }
        if (n > 0) {
            static char SDRAM_BSS b64[4 * ((CHUNK_MAX + 2) / 3) + 4]; size_t bl = pmorB64Encode(buf, (size_t)n, b64); b64[bl] = 0;
            sendf(job.tag, "F %c %d %d %d %08lX %s", (char)job.v, job.a, job.b, (int)n, (unsigned long)pmorCrc32Of(buf, (size_t)n), b64);
            s_chunks++;
        }
        s_last_chunk_ms = now;
        if (n > 0 && (uint32_t)n == (uint32_t)job.c && job.u > 1) { job.b += n; job.u--; return; }
        sendf(job.tag, "OK PULL_FILE kind=%c slot=%d offset=%d n=%d total=%lu", (char)job.v, job.a, job.b, (int)n, (unsigned long)total);
        job.kind = J_NONE;
        break; }
    default: job.kind = J_NONE;
    }
}

// ---------------------------------------------------------------- pushes (device -> host)
uint32_t seqHash(int e) {
    const LaneSet& l = seq.lane[e]; uint32_t total = (uint32_t)l.bars * l.steps_per_bar; if (total > MAXSTEPS) total = MAXSTEPS;
    uint32_t h = 2166136261u; h = pmorFnv1a(h, &l.bars, 1); h = pmorFnv1a(h, &l.steps_per_bar, 1);
    uint8_t m = (uint8_t)(l.mode_f + 0.5f); h = pmorFnv1a(h, &m, 1); h = pmorFnv1a(h, l.step, total * 2u);
    return h;
}
void resetShadow() {
    size_t n = pmorSchemaCount(); if (n > 768) n = 768;
    for (size_t i = 0; i < n; i++) pmorParamGetAt(i, &s_shadow[i]);
    for (int e = 0; e < 3; e++) s_seq_hash[e] = seqHash(e);
    s_shadow_ok = true; s_scan = 0;
}
void pushes() {
    uint32_t now = O->now_ms();
    if (!s_shadow_ok) { resetShadow(); s_epoch = O->patch_epoch(); s_slot_seen = O->patch_slot(); return; }
    // patch switched: single PATCH_CHANGED, values re-baselined silently
    uint32_t ep = O->patch_epoch();
    if (ep != s_epoch || O->patch_slot() != s_slot_seen) {
        s_epoch = ep; s_slot_seen = O->patch_slot(); resetShadow(); s_patch_changed = true;
    }
    for (int e = 0; e < 12 && room(80); e++) if (s_preset_dirty & (1u << e)) { sendf("-", "PRESETS_CHANGED %d", e); s_preset_dirty = (uint16_t)(s_preset_dirty & ~(1u << e)); }
    if (s_patch_changed && room(80)) { sendf("-", "PATCH_CHANGED %u", (unsigned)O->patch_slot()); s_patch_changed = false; }
    for (uint8_t s = 0; s < 16 && room(80); s++) {
        if (s_sample_ready & (1u << s)) { sendf("-", "SAMPLE_READY %u", (unsigned)s); s_sample_ready &= (uint16_t)~(1u << s); }
        if (s_sample_err & (1u << s))   { sendf("-", "SAMPLE_ERR %u io", (unsigned)s); s_sample_err &= (uint16_t)~(1u << s); }
    }
    if (now - s_seq_stamp >= 100u) {
        s_seq_stamp = now;
        for (int e = 0; e < 3; e++) { uint32_t h = seqHash(e); if (h != s_seq_hash[e]) { s_seq_hash[e] = h; s_seq_dirty[e] = true; } }
    }
    for (int e = 0; e < 3 && room(80); e++) if (s_seq_dirty[e]) { sendf("-", "SEQ_CHANGED %d", e); s_seq_dirty[e] = false; }
    // schema param edits
    if (now - s_last_push_scan < PUSH_GAP_MS) return;
    s_last_push_scan = now;
    size_t cnt = pmorSchemaCount(); if (cnt > 768) cnt = 768;
    for (size_t k = 0; k < cnt && room(100); k++) {
        size_t i = s_scan; s_scan = (s_scan + 1) % cnt;
        float v = 0; pmorParamGetAt(i, &v);
        if (v != s_shadow[i]) {
            char fv[24]; pmorFmtFloat(fv, v);
            if (!sendf("-", "P %u %s", (unsigned)pmorSchemaAt(i)->id, fv)) break;
            s_shadow[i] = v;
        }
    }
}
}  // namespace

const char* protoModeName(ProtoMode m) {
    switch (m) { case PM_NEPTR: return "NEPTR"; case PM_STUDIO: return "STUDIO"; case PM_EDIT: return "EDIT"; default: return "STANDALONE"; }
}
void protoInit(const ProtoOps* ops) {
    O = ops; s_mode = PM_STANDALONE; s_conn = false; job.kind = J_NONE; s_rx_n = 0; s_tx_r = s_tx_w = 0; s_shadow_ok = false; s_chunks = 0;
    s_schema_hash = computeSchemaHash();
}
ProtoMode protoMode(void) { return s_mode; }
bool protoActive(void) { return s_mode >= PM_STUDIO; }
uint32_t protoSchemaHash(void) { return s_schema_hash; }
uint32_t protoStatBulkChunks(void) { return s_chunks; }
uint32_t protoStatTxStalls(void) { return s_tx_stalls; }
void protoPresetsChanged(int engine) { if (engine >= 0 && engine < 12) s_preset_dirty = (uint16_t)(s_preset_dirty | (1u << engine)); }
uint32_t protoDebugJob(void) { return ((uint32_t)job.kind << 24) | ((s_tx_w - s_tx_r) & 0xFFFFFFu); }
void protoSampleDone(uint8_t slot, int err) { if (slot > 15) return; if (err) s_sample_err |= (uint16_t)(1u << slot); else s_sample_ready |= (uint16_t)(1u << slot); }
void protoConnected(bool dtr) {
    if (!O) { s_conn = dtr; return; }
    if (dtr == s_conn) return;
    s_conn = dtr;
    uint32_t now = O->now_ms();
    // A DTR rise right after a command line was handled (the host opened the port and sent HELLO in one breath) must not kill that
    // fresh session; otherwise a rise or fall starts a clean one.
    if (dtr && s_have_rx && (uint32_t)(now - s_last_rx) < 200u) { s_resets[RC_DTR_RISE]++; return; }
    resetSession(dtr ? RC_DTR_RISE : RC_DTR_FALL);
    s_last_rx = now;
}
void protoUsbReset(void) { if (O) { resetSession(RC_USB); s_last_rx = O->now_ms(); } }   // mount / unmount / suspend / resume edge seen by the transport
uint32_t protoStatResets(int cause) { return (cause >= 0 && cause < RC_N) ? s_resets[cause] : 0; }
void protoRx(const uint8_t* d, size_t n) {
    if (!O) return;
    for (size_t i = 0; i < n; i++) {
        char c = (char)d[i];
        if (c == '\n') { if (!s_rx_over) line(); s_rx_n = 0; s_rx_over = false; continue; }
        if (s_rx_over) continue;
        if (s_rx_n >= LINE_MAX_RX - 1) { s_rx_over = true; continue; }
        s_rx[s_rx_n++] = c;
    }
}
void protoPoll(void) {
    if (!O) return;
    uint32_t now = O->now_ms();
    // DTR is not required: a host that never asserts it (or asserts it late) still gets its replies while it is talking to us
    if (!s_conn && !(s_have_rx && (uint32_t)(now - s_last_rx) < 2000u)) return;
    if (s_mode >= PM_STUDIO && now - s_last_rx > LIVENESS_MS) { resetSession(RC_LIVE); return; }
    // TX watchdog: bytes waiting (ring, or transport FIFO full) and nothing accepted for TX_STALL_MS -> the host is not
    // draining. Drop the queue and the session (host re-HELLOs), clear the transport FIFO, count it.
    if (s_tx_r == s_tx_w && O->tx_space() > 0) s_tx_prog = now;
    else if ((uint32_t)(now - s_tx_prog) > TX_STALL_MS) {
        s_tx_stalls++; s_resets[RC_STALL]++; s_tx_prog = now;
        goStandalone();
        if (O->tx_reset) O->tx_reset();
        return;
    }
    uint32_t t0 = now;
    do {
        uint32_t r0 = s_tx_r, w0 = s_tx_w;
        runJob();
        if (s_mode >= PM_STUDIO && job.kind == J_NONE) pushes();
        txFlush();
        if (job.kind == J_NONE && s_tx_r == s_tx_w) break;
        if (s_tx_r == r0 && s_tx_w == w0) break;           // nothing queued, nothing drained: wait for the transport instead of spinning the whole burst
        if (O->service) O->service();                // MIDI clock + USB keep running between chunks
    } while ((uint32_t)(O->now_ms() - t0) < BURST_MS);
}
