// patch.cpp — 64 patches on SD (store/patch.h). QSPI holds only Globals
// (magic + version, like v2-audit.md §8 L1254's PersistentStorage<SessionBank>).
// Files: PATCHnn.PMR (sound-only snapshot, atomic via store/files.h) and
// PATCHnn.AUD (int16 PCM sidecar for non-empty sampler slots, same layout
// as v2's 'PAUD': [magic][ver][uint32 lens[16]][pcm...][magic] — audit
// §8 L1292-1393). PMR is written synchronously (small); AUD is written in
// <=4KB chunks driven from patchTick so a multi-second sampler save never
// stalls the UI (audit's audSaveTick/audBeginSave pattern, L1309-1352).
//
// Degrades completely without an SD card: patchInit() still loads Globals
// from QSPI and applies them; every patch op becomes a silent no-op
// (patchLoad/patchSave return false, patchSaving() stays false) — nothing
// here can block or crash while the user's card is missing or unmountable.
#include "../pmor_core/aud_loader.h"
#include "patch.h"
#include "files.h"
#include "sdlayout_fatfs.h"
#include "../hw/bootguard.h"
#include "../pmor_core/patch_format.h"
#include "fatfs.h"
#include "../config.h"
#include "../hw/hw.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/sampler.h"
#include "../pmor_core/sequencer.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/output.h"
#include "../pmor_core/scale.h"
#include "../pmor_core/clock.h"
extern "C" {
#include "../usb/clock_engine.h"
}
#include "../ui/render.h"    // g_trellis_brightness
#include "daisy_seed.h"
#include "sys/dma.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

using namespace daisy;

Globals glob;

// ============================================================ Globals (QSPI)
#define GLOB_MAGIC   0x474C4F42u   // 'GLOB'
#define GLOB_VERSION 3u   // 2: + bpm; 3: oled_contrast field removed (2026-10, TASK 9 correction) —
                          // same "bump version, let the old blob fall back to defaults" path as
                          // the v2 change; patchInit() checks version BEFORE reading any field,
                          // so an old v2 blob is never reinterpreted with the new layout.

struct GlobalsRecord {
    uint32_t magic;
    uint32_t version;
    Globals  g;
    bool operator==(const GlobalsRecord& o) const { return memcmp(this, &o, sizeof(GlobalsRecord)) == 0; }
    bool operator!=(const GlobalsRecord& o) const { return !(*this == o); }
};
static PersistentStorage<GlobalsRecord> gstore(hw.qspi);
// QSPI offset for the globals record: the last 64 KB of the 8 MB chip, far above the
// bootloader's reserved first 256 KB and the app image at 0x90040000.
#define GLOB_QSPI_OFFSET 0x7F0000u

static Globals defaultGlobals() {
    Globals d{};
    d.key_semis          = 0.f;
    d.scale_mode         = 0;
    d.input_gain         = 1.f;
    d.trellis_brightness = 0.45f;
    d.clock_mode         = 0;
    d.last_patch         = 0;
    d.master_fader       = 1.f;
    d.swing              = 0.f;
    d.bpm                = 120.f;
    d.clock_lead_ms      = 0.f;
    return d;
}

void globalsSave() {
    GlobalsRecord& r = gstore.GetSettings();
    r.magic   = GLOB_MAGIC;
    r.version = GLOB_VERSION;
    r.g       = glob;
    gstore.Save();
}

static void globalsApply() {
    scale.key_semis = glob.key_semis;
    scale.mode      = glob.scale_mode;
    scaleRebuild();
    outp.fader      = glob.master_fader;
    outp.input_gain = glob.input_gain;
    // TASK 9 (2026-10): g_trellis_brightness_ctrl is the linear control (what's persisted,
    // what GLOBALS shows); g_trellis_brightness is the gamma-corrected value hw/trellis.cpp's
    // single multiply actually uses. Both set here, same as the live-edit path in ui/nav.cpp's
    // globalsApply() — one gamma curve, applied at every site that derives the value, never
    // twice on the same number. OLEDBRT/oledSetContrast call removed with the field (2026-10):
    // the OLED contrast control is gone, not just hidden.
    g_trellis_brightness_ctrl = glob.trellis_brightness;
    g_trellis_brightness      = trellisBrightnessGamma(glob.trellis_brightness);
    clock_engine_set_sync_mode((sync_mode_t)glob.clock_mode);
    clk.swing       = glob.swing;
    if (glob.bpm >= 30.f && glob.bpm <= 300.f) clockSetBpm(glob.bpm);
    clk.follow_lead_ms = (glob.clock_lead_ms >= -20.f && glob.clock_lead_ms <= 40.f) ? glob.clock_lead_ms : 0.f;   // NaN (old record) fails both compares
}

// GLOBAL settings transcend patches: fader, input gain, brightness, contrast, key/scale, swing.
// Any live change is mirrored into glob and written to QSPI after a 3 s quiet period.
static uint32_t g_glob_save_at = 0;
static bool     g_glob_dirty   = false;
static void globalsSyncTick(uint32_t now_ms) {
    bool ch = false;
    if (glob.master_fader != outp.fader)               { glob.master_fader = outp.fader; ch = true; }
    if (glob.input_gain   != outp.input_gain)          { glob.input_gain = outp.input_gain; ch = true; }
    // compare against the LINEAR control, not the gamma-corrected g_trellis_brightness — see
    // render.h's comment on g_trellis_brightness_ctrl for why (avoids compounding the gamma
    // curve on every boot).
    if (glob.trellis_brightness != g_trellis_brightness_ctrl){ glob.trellis_brightness = g_trellis_brightness_ctrl; ch = true; }
    if (glob.key_semis    != scale.key_semis)          { glob.key_semis = scale.key_semis; ch = true; }
    if (glob.scale_mode   != scale.mode)               { glob.scale_mode = scale.mode; ch = true; }
    if (glob.swing        != clk.swing)                { glob.swing = clk.swing; ch = true; }
    if (glob.bpm          != clk.bpm)                  { glob.bpm = clk.bpm; ch = true; }
    if (!(glob.clock_lead_ms == clk.follow_lead_ms))   { glob.clock_lead_ms = clk.follow_lead_ms; ch = true; }
    if (ch) { g_glob_dirty = true; g_glob_save_at = now_ms + 3000; }
    if (g_glob_dirty && now_ms >= g_glob_save_at) { g_glob_dirty = false; globalsSave(); }
}

// ============================================================ Patch (SD, PATCHnn.PMR / .AUD)

#define AUD_MAGIC 0x44554150u   // 'PAUD' — same sidecar layout as v2

// v3 files live in their own folder with their own names so nothing ever collides with
// the v2 PATCHnn.* files on the same card.
#define PMOR3_DIR "PMOR3"
static bool s_use_dir = true;   // false = the folder could not be created: files go in the root as V3Pnn.*
static void patchName(char* o, uint8_t slot) {
    unsigned n = (unsigned)(slot & (PATCH_SLOTS - 1));
    if (s_use_dir) sprintf(o, PMOR3_DIR "/P%02u.PMR", n); else sprintf(o, "V3P%02u.PMR", n);
}
static void audName(char* o, uint8_t slot) {
    unsigned n = (unsigned)(slot & (PATCH_SLOTS - 1));
    if (s_use_dir) sprintf(o, PMOR3_DIR "/P%02u.AUD", n); else sprintf(o, "V3P%02u.AUD", n);
}

static void extName(char* o, uint8_t slot) {
    unsigned n = (unsigned)(slot & (PATCH_SLOTS - 1));
    if (s_use_dir) sprintf(o, PMOR3_DIR "/P%02u.EXT", n); else sprintf(o, "V3P%02u.EXT", n);
}
// Patch extension record (patch_format.h PatchExt): sampler START/END/LOOP/ATTACK/RELEASE + first-hit offsets. Additive sidecar; old firmware never
// opens it. Written with the patch only when something is non-default or a file already exists (an all-default patch leaves no file behind).
static PatchExt SDRAM_BSS s_ext;
static uint8_t SDRAM_BSS s_ext_raw[sizeof(PatchExt) + 256];
static void extSave(uint8_t slot) {
    char nm[24]; extName(nm, slot);
    FILINFO fi; bool have = (f_stat(nm, &fi) == FR_OK);
    if (!have && patchExtLiveCrc() == 0) return;
    patchExtSnapshot(s_ext);
    fileWriteAtomic(nm, &s_ext, patchExtBytes(s_ext));   // a failed write leaves the previous file (and its .BAK); the PMR save already succeeded
}
static void extLoad(uint8_t slot) {    // after patchApply (which put every field at its default): the record, else its .BAK, else defaults
    char nm[24]; extName(nm, slot);
    int n = fileRead(nm, s_ext_raw, sizeof s_ext_raw);
    if (n <= 0 || !patchExtParse(s_ext_raw, (size_t)n, s_ext)) {
        char bk[32]; snprintf(bk, sizeof bk, "%s.BAK", nm);
        n = fileRead(bk, s_ext_raw, sizeof s_ext_raw);
        if (n <= 0 || !patchExtParse(s_ext_raw, (size_t)n, s_ext)) return;
    }
    patchExtApply(s_ext);
}

// diagnostics for the DASH page
PatchDiag patch_diag = { 0, 0, 0, 0, 0 };
static uint8_t  g_cur_slot = 0;
static uint32_t s_job_after = 0;     // background export may not start before this (ms)
static uint8_t  s_aud_slot  = 0;     // slot of the in-flight AUD save
static Patch SDRAM_BSS s_default;   // the boot defaults (before any patch loads) = a "new" patch

// ---- QSPI mirror of the CURRENT patch ------------------------------------------------
// The live state is also written to on-chip QSPI flash on every autosave (only when it
// actually changed), so a reboot restores exactly what was playing even if the SD card
// write path is broken, slow, or the card is out. The SD library of 64 patches stays the
// source for switching slots; the mirror is only ever "the current one".
#define QMIRROR_MAGIC       0x334D5051u   // 'QPM3'
#define PATCH_QSPI_OFFSET   0x7C0000u     // 8 MB chip: 256 KB below the end; globals live at 0x7F0000
struct QspiPatchRec { uint32_t magic; uint32_t slot; Patch p; };
static QspiPatchRec SDRAM_BSS s_qrec;     // static: ~5 KB, never on the stack (SDRAM)

static void qspiMirrorWrite() {
    s_qrec.magic = QMIRROR_MAGIC;
    s_qrec.slot  = g_cur_slot;
    patchSnapshot(s_qrec.p);
    uint8_t* cur = (uint8_t*)hw.qspi.GetData(PATCH_QSPI_OFFSET);
    dsy_dma_invalidate_cache_for_buffer(cur, sizeof s_qrec);
    if (memcmp(cur, &s_qrec, sizeof s_qrec) == 0) return;         // unchanged: no erase cycle
    hw.qspi.Erase(PATCH_QSPI_OFFSET, PATCH_QSPI_OFFSET + sizeof s_qrec);
    hw.qspi.Write(PATCH_QSPI_OFFSET, sizeof s_qrec, (uint8_t*)&s_qrec);
    patch_diag.qspi_saves++;
}

static bool qspiMirrorValid(uint8_t* out_slot) {
    uint8_t* cur = (uint8_t*)hw.qspi.GetData(PATCH_QSPI_OFFSET);
    dsy_dma_invalidate_cache_for_buffer(cur, sizeof s_qrec);
    memcpy(&s_qrec, cur, sizeof s_qrec);
    if (s_qrec.magic != QMIRROR_MAGIC) return false;
    if (s_qrec.p.magic == PATCH_MAGIC_V7) {          // mirror written by v7 firmware: same prefix, convert
        static Patch SDRAM_BSS q;
        if (!patchFromBlob(&s_qrec.p, sizeof(Patch7), q)) return false;
        s_qrec.p = q;
    } else if (s_qrec.p.magic != PATCH_MAGIC || !patchCrcOk(s_qrec.p)) return false;
    if (s_qrec.slot >= PATCH_SLOTS) return false;
    if (out_slot) *out_slot = (uint8_t)s_qrec.slot;
    return true;
}

static uint32_t patchLiveCrc();
static uint32_t s_last_saved_crc = 0, s_last_seen_crc = 0, s_next_scan = 0;

// ---- existence bitmap, scanned once at boot -------------------------------
static uint64_t g_exists_mask = 0;

static void scanExists() {
    g_exists_mask = 0;
    if (!sdOk()) return;
    for (int s = 0; s < PATCH_SLOTS; s++) {
        char nm[16]; patchName(nm, (uint8_t)s);
        FILINFO fi;
        if (f_stat(nm, &fi) == FR_OK) g_exists_mask |= (1ull << s);
    }
}

bool patchExists(uint8_t slot) {
    if (slot >= PATCH_SLOTS) return false;
    return (g_exists_mask & (1ull << slot)) != 0;
}

// ---- current slot / dirty / saving state -----------------------------------
static bool     g_dirty    = false;
static uint32_t g_save_at  = 0;

uint8_t patchCurrent() { return g_cur_slot; }
bool    patchDirty()   { return g_dirty; }

// ---- chunked AUD (sampler audio) save, driven from patchTick --------------
static ChunkedWriter aud_writer;
static bool          aud_saving  = false;
static int           aud_slot_ix = 0;   // 0..SMP_SLOTS-1, which sampler slot we're writing
static uint32_t      aud_pos     = 0;   // samples written of the current slot
static int16_t       aud_buf[1024];     // 1024 * int16 = 2KB <= the 4KB-per-tick budget

bool patchSaving() { return aud_saving; }

static bool audBeginSave(uint8_t slot) {
    char nm[16]; audName(nm, slot);
    storeJobAbort(storeDevice()); s_aud_slot = slot;
    if (!aud_writer.begin(nm)) return false;
    uint32_t hdr[2] = { AUD_MAGIC, 1u };
    if (!aud_writer.tick(hdr, sizeof(hdr))) { aud_writer.finish(); return false; }
    uint32_t lens[SMP_SLOTS];
    for (int i = 0; i < SMP_SLOTS; i++) lens[i] = smp.len[i];
    if (!aud_writer.tick(lens, sizeof(lens))) { aud_writer.finish(); return false; }
    aud_slot_ix = 0; aud_pos = 0;
    aud_saving  = true;
    return true;
}

static void audSaveTick() {   // one <=2KB chunk — call once per patchTick pass
    if (!aud_saving) return;
    while (aud_slot_ix < SMP_SLOTS && (smp.len[aud_slot_ix] == 0 || aud_pos >= smp.len[aud_slot_ix])) {
        aud_slot_ix++; aud_pos = 0;
    }
    if (aud_slot_ix >= SMP_SLOTS) {            // done — trailing magic marks it complete
        uint32_t tail = AUD_MAGIC;
        aud_writer.tick(&tail, sizeof(tail));
        aud_saving = false;
        bool okf = aud_writer.finish();
        smp.audio_dirty = false;
        if (okf) { storeQueue(storeDevice(), s_aud_slot); s_job_after = System::GetNow() + 1500; }
        return;
    }
    uint32_t n = smp.len[aud_slot_ix] - aud_pos;
    if (n > 1024) n = 1024;
    for (uint32_t i = 0; i < n; i++) {
        float v = smp_buf[aud_slot_ix][aud_pos + i];
        if (v >  1.f) v =  1.f;
        if (v < -1.f) v = -1.f;
        aud_buf[i] = (int16_t)(v * 32767.f);
    }
    if (!aud_writer.tick(aud_buf, n * 2)) {   // a mid-save failure just stops — .TMP is
        aud_saving = false;                   // discarded on finish(), live .AUD untouched
        aud_writer.finish();
        return;
    }
    aud_pos += n;
}

// ---- AUD load (user-initiated: patchLoad, chunked read is fine to block) --
// TASK 2 audit (2026-10): rapid pad-press patch switching can abort an in-flight AUD save
// mid-file (patchNew/patchLoad/patchDelete all call ChunkedWriter::abort() on aud_writer to
// never race a load/delete against a save — TASK 7, below). Before TASK 7, that abort went
// through finish() instead, which only distinguishes "a tick() actually failed" (ok_ false,
// discards the .TMP) from everything else; a caller-initiated stop mid-save still had
// ok_ == true, so finish() PROMOTED the incomplete .TMP as the live .AUD and demoted the
// previous, complete file to .BAK — exactly backwards. The promoted file still opened fine
// and its header/lens[] were intact (those are written synchronously up front in
// audBeginSave, before the chunked PCM loop even starts), so without the check below the old
// per-slot short-read fallback would silently accept it: the in-progress slot loaded
// truncated and every slot after it (which had real, complete audio one save-cycle ago, now
// sitting untouched in .BAK) loaded as EMPTY. That was the data loss the user described,
// confirmed here by code, not guesswork.
//
// TASK 7 (2026-10) added ChunkedWriter::abort() (store/files.cpp, now owned by this agent) so
// the write side no longer promotes a partial save in the first place. This read-side check
// stays anyway — belt and braces, per instruction, and correctly so: a .TMP can also be
// orphaned by a power cut mid-save, which no amount of abort-handling on the normal code path
// covers. audSaveTick() writes a trailing AUD_MAGIC sentinel only when a save reaches natural
// completion (aud_slot_ix >= SMP_SLOTS); an aborted OR interrupted save never gets that far,
// so its absence is a reliable "this file is not a complete save" signal regardless of why it
// stopped. Require it before trusting ANY of the per-slot audio, and reject a short read the
// same way (previously it kept the partial slot and stopped) — both fall through to the .BAK
// the caller (audLoad) already tries next.
// ---- background sampler-audio restore (pmor_core/aud_loader.h). Boot used to block here for ~12 s on a slow card.
static AudLoader s_aud_ld; static bool s_ld_ids = false; static uint8_t s_ld_slot = 0;
static bool s_boot_defer = false;                       // patchInit(): audio restore is armed, not run
static int32_t ldFileRead(void*, const char* nm, uint32_t off, void* buf, uint32_t n) {
    FIL f; UINT br = 0;
    if (f_open(&f, nm, FA_READ) != FR_OK) return -1;
    FRESULT r = off ? f_lseek(&f, off) : FR_OK;
    if (r == FR_OK) r = f_read(&f, buf, n, &br);
    f_close(&f);
    return r == FR_OK ? (int32_t)br : 0;
}
static int32_t ldIdRead(void*, uint16_t id, uint32_t off, int16_t* buf, uint32_t n) { return storeSampleRead(storeDevice(), id, off, buf, n); }
static uint32_t ldIdLen(void*, uint16_t id) { const StoreSample* ss = storeFindSample(storeDevice(), id); return ss ? ss->len : 0; }
static void ldPut(void*, uint8_t sl, uint32_t off, const int16_t* s, uint32_t n) { for (uint32_t i = 0; i < n && off + i < SMP_LEN; i++) smp_buf[sl][off + i] = (float)s[i] / 32767.f; }
static void ldLen(void*, uint8_t sl, uint32_t len) { smp.len[sl] = len; }
static AudSrc ldSrc() { AudSrc s{}; s.file_read = ldFileRead; s.id_read = ldIdRead; s.id_len = ldIdLen; s.put = ldPut; s.set_len = ldLen; return s; }
static void audLoadSync() { while (s_aud_ld.step()) {} }                 // user-initiated paths: finish now (old behaviour)
static void audArmFile(uint8_t slot);
static void audBootTick() {                                               // patchTick: one bounded step (<= 4 KB of reads) per pass
    if (!s_aud_ld.busy()) return;
    s_aud_ld.step();
    if (!s_aud_ld.busy() && s_aud_ld.result == AudLoader::FAILED && s_ld_ids) { s_ld_ids = false; audArmFile(s_ld_slot); }   // pool read failed: the legacy .AUD is the fallback, as before
}

static void audLoad(uint8_t slot, const uint16_t* ids = nullptr) {
    s_aud_ld.cancel();                                    // a newer load supersedes a pending boot restore
    for (int i = 0; i < SMP_SLOTS; i++) smp.len[i] = 0;
    if (!sdOk()) return;
    s_ld_slot = slot;
    if (ids) {
        s_aud_ld.beginIds(ldSrc(), ids); s_ld_ids = true;
        if (s_boot_defer) return;
        audLoadSync(); s_ld_ids = false;
        if (s_aud_ld.result == AudLoader::OK) return;
        for (int i = 0; i < SMP_SLOTS; i++) smp.len[i] = 0;
    }
    audArmFile(slot);
    if (!s_boot_defer) audLoadSync();
}
static void audArmFile(uint8_t slot) { char nm[16]; audName(nm, slot); s_aud_ld.beginFile(ldSrc(), nm, AUD_MAGIC); }
static void audLoadFinish() { if (s_aud_ld.busy()) { audLoadSync(); if (!s_aud_ld.busy() && s_aud_ld.result == AudLoader::FAILED && s_ld_ids) { s_ld_ids = false; audArmFile(s_ld_slot); audLoadSync(); } } }   // a save/new/assign must never see half-restored audio

// ============================================================ public API
void patchInit() {
    s_boot_defer = true;   // sampler audio restore runs from patchTick, not here (cleared at the end of patchInit)
    // Globals from QSPI first — GLOBALS must be usable even with a dead/missing card.
    Globals def = defaultGlobals();
    GlobalsRecord defrec{ GLOB_MAGIC, GLOB_VERSION, def };
    gstore.Init(defrec, GLOB_QSPI_OFFSET);
    GlobalsRecord& r = gstore.GetSettings();
    if (r.magic != GLOB_MAGIC || r.version != GLOB_VERSION) {   // new layout -> defaults once
        r.magic = GLOB_MAGIC; r.version = GLOB_VERSION; r.g = def;
        gstore.Save();
    }
    glob = r.g;
    globalsApply();

    patchSnapshot(s_default);   // engines are at their init defaults here: keep them as "new patch"

    if (sdOk()) {
        FRESULT mr = f_mkdir(PMOR3_DIR);
        if (mr != FR_OK && mr != FR_EXIST) { fileDiagNote('m', (uint8_t)mr); s_use_dir = false; }
    }
    scanExists();
    if (sdOk() && !bootguardSkipStorage()) storeQueueAll(storeDevice());      // boot migration/mirror refresh runs from patchTick, never the audio callback
    g_cur_slot = glob.last_patch < PATCH_SLOTS ? glob.last_patch : 0;
    patch_diag.boot_load = 0;                       // 0 = no file for the boot slot

    // The QSPI mirror is written on every autosave whether or not the SD write worked, so it
    // is never older than the SD copy of the same slot: it wins for the boot slot. The SD
    // file is only used when the mirror is missing/invalid or belongs to a different slot.
    uint8_t qslot = 0;
    bool qok = qspiMirrorValid(&qslot);
    if (qok && qslot == g_cur_slot) {
        patchApply(s_qrec.p);
        extLoad(g_cur_slot);
        clockSetLoopLen(seqLongestLoop());
        audLoad(g_cur_slot);                         // sampler audio still comes from the card
        s_last_saved_crc = 0;                        // force one SD write of this state (keeps the card current)
        s_last_seen_crc  = patchLiveCrc();
        patch_diag.boot_load = 5;                    // 5 = restored from the QSPI mirror
    } else if (patchExists(g_cur_slot)) {
        patch_diag.boot_load = patchLoad(g_cur_slot) ? 1 : patch_diag.boot_load;
    } else if (qok) {                                // mirror is for another slot: still better than nothing
        g_cur_slot = qslot;
        patchApply(s_qrec.p);
        extLoad(g_cur_slot);
        clockSetLoopLen(seqLongestLoop());
        audLoad(g_cur_slot);
        s_last_saved_crc = 0;
        s_last_seen_crc  = patchLiveCrc();
        patch_diag.boot_load = 5;
    }
    if (patch_diag.boot_load != 5) qspiMirrorWrite();   // mirror now reflects whatever we booted into
    s_boot_defer = false;
}

const PatchDiag& patchDiag() { return patch_diag; }

// Fresh patch in `slot` (v2 sessionNew): flush what we were editing, apply the boot
// defaults, drop the sampler audio, make `slot` current and write it right away.
bool patchNew(uint8_t slot) {
    if (slot >= PATCH_SLOTS || !sdOk()) return false;
    s_aud_ld.cancel();   // a fresh patch drops the audio anyway
    if (aud_saving) { aud_saving = false; aud_writer.abort(); }   // TASK 7: caller-initiated stop, never promote a partial .AUD
    if (g_dirty) patchSave(g_cur_slot);
    patchApply(s_default);
    for (int i = 0; i < SMP_SLOTS; i++) smp.len[i] = 0;
    smp.audio_dirty = false;
    clockSetLoopLen(seqLongestLoop());
    g_cur_slot = slot;
    bool ok = patchSave(slot);
    g_dirty = false;
    s_last_saved_crc = s_last_seen_crc = patchLiveCrc();
    if (glob.last_patch != slot) { glob.last_patch = slot; globalsSave(); }
    qspiMirrorWrite();
    return ok;
}

bool patchLoad(uint8_t slot) {
    if (slot >= PATCH_SLOTS || !sdOk()) return false;
    if (aud_saving) { aud_saving = false; aud_writer.abort(); }   // never race a load against a save; TASK 7: never promote a partial .AUD
    if (g_dirty && slot != g_cur_slot) patchSave(g_cur_slot);      // never lose edits still inside the debounce

    char nm[16]; patchName(nm, slot);
    static Patch SDRAM_BSS p;   // ~5 KB: keep off the stack
    static uint8_t SDRAM_BSS raw[sizeof(Patch)];
    int n = fileRead(nm, raw, sizeof raw);
    bool good = n > 0 && patchFromBlob(raw, (size_t)n, p);   // v8, or legacy v7 (drum_eng = 0)
    uint32_t pmr_crc = good ? pmorCrc32(raw, (size_t)n) : 0, pmr_n = good ? (uint32_t)n : 0;
    bool recovered = false;
    uint16_t ids[SMP_SLOTS]; bool have_ids = false;
    if (!good) {
        // fall back to the atomic backup (stays v7 until the next v8 save rotates it in)
        char bk[24]; snprintf(bk, sizeof bk, "%s.BAK", nm);
        int n2 = fileRead(bk, raw, sizeof raw);
        good = n2 > 0 && patchFromBlob(raw, (size_t)n2, p);
        if (!good && n >= 0) {                                  // legacy file present but unreadable: the new-layout copy is a third source
            good = storeRecoverPatch(storeDevice(), slot, p, ids); recovered = good; have_ids = good;
        }
        if (!good) {
            bool sized = (n == (int)sizeof(Patch) || n == (int)sizeof(Patch7));
            patch_diag.boot_load = (n < 0) ? 2 : (sized ? 4 : 3);   // 2 unreadable, 3 wrong size/magic, 4 CRC
            patch_diag.load_size = (n < 0) ? 0 : (uint16_t)n;
            return false;
        }
    } else {
        have_ids = storeFreshIds(storeDevice(), slot, pmr_n, pmr_crc, ids);   // new layout prefers itself only when provably in sync
        if (!have_ids) storeQueue(storeDevice(), slot);                        // out of sync (older firmware wrote last): re-export
    }

    patchApply(p);                       // sound only — nav page untouched
    extLoad(slot);                       // additive sidecar: sampler region/envelope extras + first-hit offsets (defaults when absent)
    clockSetLoopLen(seqLongestLoop());
    audLoad(slot, have_ids ? ids : nullptr);   // sampler audio: pool WAVs when in sync, else legacy .AUD (user-initiated, ok to block)

    g_cur_slot = slot;
    g_dirty    = false;
    s_last_saved_crc = s_last_seen_crc = patchLiveCrc();
    if (glob.last_patch != slot) { glob.last_patch = slot; globalsSave(); }
    qspiMirrorWrite();
    if (recovered) patchMarkDirty();       // legacy file was bad: rewrite it from the recovered state
    return true;
}

bool patchSave(uint8_t slot) {
    audLoadFinish();   // never write a patch while the sampler audio is half restored
    if (slot >= PATCH_SLOTS || !sdOk() || storeArchiveActive(storeDevice())) return false;   // ARCHIVE_PATCHES running: no patch file is written
    static Patch SDRAM_BSS p;   // ~5 KB: keep off the stack
    patchSnapshot(p);
    char nm[16]; patchName(nm, slot);
    bool ok = fileWriteAtomic(nm, &p, sizeof(p));
    if (ok) {
        extSave(slot);
        g_exists_mask |= (1ull << slot);
        patchPoolInvalidate();
        storeJobAbort(storeDevice()); storeQueue(storeDevice(), slot); s_job_after = System::GetNow() + 1500;   // dual-write: new layout follows, in the background
        if (slot == g_cur_slot) g_dirty = false;
        patch_diag.saves_ok++;
    } else {
        patch_diag.saves_fail++;
    }
    return ok;
}

void patchDelete(uint8_t slot) {
    if (slot >= PATCH_SLOTS || !sdOk() || storeArchiveActive(storeDevice())) return;
    if (aud_saving) { aud_saving = false; aud_writer.abort(); }   // TASK 7: caller-initiated stop, never promote a partial .AUD
    char nm[16]; patchName(nm, slot);
    char bk[24]; snprintf(bk, sizeof bk, "%s.BAK", nm);
    f_unlink(nm); f_unlink(bk);
    char an[16]; audName(an, slot);
    char ab[24]; snprintf(ab, sizeof ab, "%s.BAK", an);
    f_unlink(an); f_unlink(ab);
    { char en[24], eb[32]; extName(en, slot); snprintf(eb, sizeof eb, "%s.BAK", en); f_unlink(en); f_unlink(eb); }
    g_exists_mask &= ~(1ull << slot);
    storeDropPatch(storeDevice(), slot);      // the new-layout mirror must not resurrect a deleted patch

    // TASK 2 (2026-10): erasing the CURRENT slot must not come back from the dead. Pad-press
    // now loads a patch instantly and pad-hold erases it 5 s later (ui/nav.cpp), so a held
    // pad means the patch loads live at t=0 then gets deleted at t=5s while still being
    // g_cur_slot. patchTick's 4 Hz CRC autosave (and the sibling smp.audio_dirty check right
    // beside it) would otherwise see "live state differs from what was last saved" on its very
    // next 250 ms scan and write the patch straight back into the file we just removed — the
    // erase would silently not erase. Quiesce both autosave triggers to the live state as it
    // stands right now, same as patchLoad/patchNew do once they settle on a slot. The sound
    // stays live in RAM (deleting a patch doesn't silence the instrument) but the SLOT is empty
    // until the player changes something new; any further edit naturally re-dirties from here
    // and saves a fresh patch into this slot, which is the expected PO-style behaviour.
    if (slot == g_cur_slot) {
        g_dirty = false;
        s_last_saved_crc = s_last_seen_crc = patchLiveCrc();
        smp.audio_dirty = false;
    }
}

void patchMarkDirty() {
    g_dirty   = true;
    g_save_at = System::GetNow() + AUTOSAVE_DEBOUNCE_MS;
}

// Central change detection: instead of trusting every code path to call patchMarkDirty(),
// the whole patch is snapshotted and CRC'd 4x a second. ANY change (sound swipe, sequence
// hit, gesture, bars/steps, volumes, automation...) arms the 2 s debounce; each further
// change restarts it; then it is written. Sampler audio has its own dirty flag.
static Patch SDRAM_BSS s_scan;         // static, not stack (several KB) — SDRAM
static uint32_t patchLiveCrc() { patchSnapshot(s_scan); return s_scan.crc32 ^ patchExtLiveCrc(); }   // + the extension fields (0 while default: the scan value is then bit-identical to before)

static void assignTick();
static bool s_asg_active();
static bool s_nosave = false;
void patchSetNoSave(bool on) { s_nosave = on; }
void patchFileIdleClose(uint32_t now_ms);
void patchArchiveTick();
void patchTick(uint32_t now_ms) {
    patchFileIdleClose(now_ms);
    if (storeArchiveActive(storeDevice())) { patchArchiveTick(); return; }   // ARCHIVE_PATCHES owns the card: no autosave, no export, one file per pass
    if (s_aud_ld.busy()) { audBootTick(); return; }   // restoring the sampler audio in the background: nothing is saved or reassigned until it finished
    assignTick();
    if (s_nosave) return;   // protocol test session: RAM edits are never written to the card / QSPI
    globalsSyncTick(now_ms);
    if (aud_saving) { audSaveTick(); return; }   // finish an in-flight audio save before anything else

    if (now_ms >= s_next_scan) {
        s_next_scan = now_ms + 250;
        uint32_t c = patchLiveCrc();
        if (c != s_last_seen_crc) { s_last_seen_crc = c; if (c != s_last_saved_crc) patchMarkDirty(); }
        if (smp.audio_dirty && !g_dirty) patchMarkDirty();
    }

    if (g_dirty && now_ms >= g_save_at) {
        qspiMirrorWrite();                                                                  // on-chip copy first: survives any SD failure
        if (patchSave(g_cur_slot)) s_last_saved_crc = s_last_seen_crc = patchLiveCrc();   // fast: PMR only
        else g_save_at = now_ms + 5000;                                                     // no card: retry later, never spin
        if (smp.audio_dirty) audBeginSave(g_cur_slot);   // AUD chunks now drive from here
    }

    // background export of the new SD layout: one bounded chunk per pass, only when nothing else is being saved
    if (!g_dirty && !aud_saving && !s_asg_active() && now_ms >= s_job_after && sdOk() && !bootguardSkipStorage()) {
        storeJobTick(storeDevice());
        StoreCtx& sc = storeDevice(); if (sc.state == STORE_DONE || sc.state == STORE_ERRORS) bootguardStorageDone();   // watchdog flag: this boot's storage tasks finished
    }
}

void patchFactoryReset() {
    if (sdOk()) {
        if (aud_saving) { aud_saving = false; aud_writer.abort(); }   // TASK 7: caller-initiated stop, never promote a partial .AUD
        for (int s = 0; s < PATCH_SLOTS; s++) patchDelete((uint8_t)s);
    }
    glob = defaultGlobals();
    globalsSave();
    globalsApply();
}

// ============================================================ sample pool for the CDC protocol (docs/PROTOCOL.md section 8)
// id = patch*16 + slot. The CURRENT patch is served from live RAM; every other patch from its .AUD sidecar:
// [magic][ver][lens[16]][pcm of the non-empty slots in order][magic]  (72-byte header, int16 mono 48k).
// All SD access is control-plane/background; nothing here is reachable from the audio callback.
static uint32_t SDRAM_BSS s_pool_len[PATCH_SLOTS][SMP_SLOTS];
static uint32_t s_pool_stamp[PATCH_SLOTS];
static uint8_t  s_pool_state[PATCH_SLOTS];     // 0 unknown, 1 ready, 2 no audio file

static int32_t s_blk_id = -1;   // sample-pool read cache owner (id), -1 = empty
void patchPoolInvalidate() { s_blk_id = -1; for (int i = 0; i < PATCH_SLOTS; i++) s_pool_state[i] = 0; }

int patchPoolScan(uint8_t p) {
    if (p >= PATCH_SLOTS) return -1;
    if (p == g_cur_slot) return 1;
    if (s_pool_state[p] == 1) return 1;
    if (s_pool_state[p] == 2) return -1;
    if (!sdOk() || !patchExists(p)) { s_pool_state[p] = 2; return -1; }
    char nm[16]; audName(nm, p);
    FIL f; UINT br; uint32_t hdr[2];
    if (f_open(&f, nm, FA_READ) != FR_OK) { s_pool_state[p] = 2; return -1; }
    bool ok = f_read(&f, hdr, sizeof hdr, &br) == FR_OK && br == sizeof hdr && hdr[0] == AUD_MAGIC &&
              f_read(&f, s_pool_len[p], sizeof s_pool_len[p], &br) == FR_OK && br == sizeof s_pool_len[p];
    f_close(&f);
    if (!ok) { s_pool_state[p] = 2; return -1; }
    FILINFO fi; s_pool_stamp[p] = (f_stat(nm, &fi) == FR_OK) ? (((uint32_t)fi.fdate << 16) | fi.ftime) : 0;
    for (int s = 0; s < SMP_SLOTS; s++) if (s_pool_len[p][s] > SMP_LEN) s_pool_len[p][s] = SMP_LEN;
    s_pool_state[p] = 1;
    return 1;
}
uint32_t patchPoolLen(uint8_t p, uint8_t s) {
    if (p >= PATCH_SLOTS || s >= SMP_SLOTS) return 0;
    if (p == g_cur_slot) return smp.len[s];
    return s_pool_state[p] == 1 ? s_pool_len[p][s] : 0;
}
uint32_t patchPoolVer(uint8_t p, uint8_t s) {
    if (p == g_cur_slot) {                       // live: cheap content hash (length + a stride through the samples)
        uint32_t h = 2166136261u ^ smp.len[s];
        for (uint32_t i = 0; i < smp.len[s]; i += 7) { h ^= (uint32_t)(int32_t)(smp_buf[s][i] * 32767.f); h *= 16777619u; }
        return h;
    }
    return s_pool_stamp[p & (PATCH_SLOTS - 1)];
}
// ---- new layout (docs/STORAGE.md) reads for the protocol: ids >= STORE_ID_BASE are pool ids, ids < 1024 are aliases (patchStoreAlias).
static int32_t newPoolRead(uint16_t id, uint32_t off, uint8_t* buf, uint32_t max) {   // bytes, 0 end, -1 err; small block cache (SD open per chunk is ~30 ms)
    const StoreSample* m = storeFindSample(storeDevice(), id); if (!m) return -1;
    uint32_t total = m->len * 2; if (off >= total) return 0;
    uint32_t n = total - off; if (n > max) n = max;
    static int16_t SDRAM_BSS blk[8192]; static int32_t bid = -1; static uint32_t boff = 0, bn = 0;   // 16 KB read-ahead
    if (!(bid == (int32_t)id && off >= boff && off + n <= boff + bn * 2u)) {
        uint32_t s0 = off / 2u, want = m->len - s0; if (want > 8192u) want = 8192u;
        int32_t got = storeSampleRead(storeDevice(), id, s0, blk, want); if (got <= 0) { bid = -1; return -1; }
        bid = (int32_t)id; boff = s0 * 2u; bn = (uint32_t)got;
        if (off + n > boff + bn * 2u) return -1;
    }
    memcpy(buf, (const uint8_t*)blk + (off - boff), n); return (int32_t)n;
}
// bytes read (0 = end), -1 error
int32_t patchPoolRead(uint16_t id, uint32_t off, uint8_t* buf, uint32_t max) {
    if (id >= STORE_ID_BASE) return newPoolRead(id, off, buf, max);
    if (uint16_t nid = patchStoreAlias(id)) return newPoolRead(nid, off, buf, max);   // old patch*16+slot id that the migration exported: serve the pool copy
    uint8_t p = (uint8_t)(id / 16), s = (uint8_t)(id % 16);
    if (p >= PATCH_SLOTS || patchPoolScan(p) < 0) return -1;
    uint32_t len = patchPoolLen(p, s), total = len * 2;
    if (len == 0) return -1;
    if (off >= total) return 0;
    uint32_t n = total - off; if (n > max) n = max;
    if (p == g_cur_slot) {
        for (uint32_t i = 0; i < n; i += 2) {
            float v = smp_buf[s][(off + i) / 2]; if (v > 1.f) v = 1.f; if (v < -1.f) v = -1.f;
            int16_t q = (int16_t)(v * 32767.f); buf[i] = (uint8_t)q; buf[i + 1] = (uint8_t)((uint16_t)q >> 8);
        }
        return (int32_t)n;
    }
    // card-resident sample: one SD open+seek+read fetches a 2 KB block that serves ~10 consecutive 192-byte chunks
    // (a per-chunk open/seek/close on the bit-banged SD cost ~30 ms each, i.e. ~28 chunks/s)
    static uint8_t SDRAM_BSS blk[4096]; static uint32_t blk_off = 0, blk_n = 0; static uint32_t blk_stamp = 0;
    if (!(s_blk_id == (int32_t)id && off >= blk_off && off + n <= blk_off + blk_n && blk_stamp == s_pool_stamp[p])) {
        uint32_t before = 0; for (int k = 0; k < s; k++) before += s_pool_len[p][k];
        uint32_t want = total - off; if (want > sizeof blk) want = sizeof blk;
        char nm[16]; audName(nm, p);
        FIL f; UINT br;
        if (f_open(&f, nm, FA_READ) != FR_OK) return -1;
        bool ok = f_lseek(&f, 72u + before * 2u + off) == FR_OK && f_read(&f, blk, want, &br) == FR_OK && br == want;
        f_close(&f);
        if (!ok) { s_blk_id = -1; return -1; }
        s_blk_id = (int32_t)id; blk_off = off; blk_n = want; blk_stamp = s_pool_stamp[p];
    }
    memcpy(buf, blk + (off - blk_off), n);
    return (int32_t)n;
}

// ASSIGN_SAMPLE: background copy of pool sample `id` into the CURRENT patch's slot, 1024 samples per patchTick pass
static struct { bool active; uint8_t slot; uint16_t id; uint32_t pos, len; bool done; int err; } s_asg;
int patchAssignSample(uint8_t slot, uint16_t id) {
    if (s_asg.active || slot >= SMP_SLOTS) return -2;
    if (id >= STORE_ID_BASE) {
        const StoreSample* ss = storeFindSample(storeDevice(), id); if (!ss || ss->len == 0) return -1;
        smp.len[slot] = 0;
        s_asg = { true, slot, id, 0, ss->len > SMP_LEN ? SMP_LEN : ss->len, false, 0 };
        return 0;
    }
    uint8_t p = (uint8_t)(id / 16), s = (uint8_t)(id % 16);
    if (p >= PATCH_SLOTS || patchPoolScan(p) < 0) return -1;
    uint32_t len = patchPoolLen(p, s); if (len == 0) return -1;
    smp.len[slot] = 0;                     // silent while it fills
    s_asg = { true, slot, id, 0, len, false, 0 };
    return 0;
}
static void assignTick() {
    if (!s_asg.active) return;
    uint8_t p = s_asg.id >= STORE_ID_BASE ? 255 : (uint8_t)(s_asg.id / 16), s = (uint8_t)(s_asg.id % 16);
    uint32_t n = s_asg.len - s_asg.pos; if (n > 1024) n = 1024;
    if (p == g_cur_slot) {
        if (s != s_asg.slot) for (uint32_t i = 0; i < n; i++) smp_buf[s_asg.slot][s_asg.pos + i] = smp_buf[s][s_asg.pos + i];
    } else {
        int16_t b[1024];
        int32_t got = patchPoolRead(s_asg.id, s_asg.pos * 2u, (uint8_t*)b, n * 2u);
        if (got != (int32_t)(n * 2u)) { s_asg.active = false; s_asg.done = true; s_asg.err = 1; return; }
        for (uint32_t i = 0; i < n; i++) smp_buf[s_asg.slot][s_asg.pos + i] = (float)b[i] / 32767.f;
    }
    s_asg.pos += n;
    if (s_asg.pos >= s_asg.len) { smp.len[s_asg.slot] = s_asg.len; smp.audio_dirty = true; s_asg.active = false; s_asg.done = true; }
}
static bool s_asg_active() { return s_asg.active; }

// ---- new-layout pool for the protocol / UI (background loop only)
int patchStoreSampleCount() { return storeDevice().n_smp; }
bool patchStoreSampleAt(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* crc) {
    StoreCtx& sc = storeDevice(); if (i < 0 || i >= sc.n_smp) return false;
    const StoreSample& q = sc.smp[i]; *id = q.id; *len = q.len; *rate = q.rate; *ch = q.ch; *crc = q.crc; return true;
}
uint16_t patchStoreAlias(uint16_t old_id) { return storeAliasToId(storeDevice(), old_id); }
void patchStoreStatus(PatchStoreStatus* o) {
    StoreCtx& sc = storeDevice(); o->state = sc.state; o->pending = (uint8_t)(__builtin_popcountll(sc.pending) + (sc.job.phase != J_IDLE ? 1 : 0));
    o->exported = sc.exported; o->written = sc.written; o->verified = sc.verified; o->skipped = sc.skipped; o->errors = sc.errors; o->samples = sc.n_smp;
}

bool patchAssignPoll(uint8_t* slot, int* err) {
    if (!s_asg.done) return false;
    *slot = s_asg.slot; *err = s_asg.err; s_asg.done = false; return true;
}

// CLEAR_PATCH: live state back to the boot defaults WITHOUT arming autosave (nothing on the card is overwritten
// unless the player edits afterwards). Sampler audio of the live patch is dropped from RAM only.
void patchClearLive() {
    if (aud_saving) { aud_saving = false; aud_writer.abort(); }
    patchApply(s_default);
    for (int i = 0; i < SMP_SLOTS; i++) smp.len[i] = 0;
    smp.audio_dirty = false;
    clockSetLoopLen(seqLongestLoop());
    g_dirty = false;
    s_last_saved_crc = s_last_seen_crc = patchLiveCrc();
}

// Raw read-only access to the card files for the protocol's backup tool (PULL_FILE). kind: P .PMR, K .PMR.BAK, A .AUD, L .AUD.BAK
volatile uint32_t g_ff_opens = 0, g_ff_reads = 0, g_ff_why[4] = {0, 0, 0, 0};   // why a read reopened: [0] off==0 [1] not open [2] pos mismatch [3] name   // PULL_FILE handle statistics (STATUS ffo=opens/reads)
static uint32_t s_ffile_last = 0;
void patchFileIdleClose(uint32_t) {}   // (read-ahead block cache replaced the kept-open handle)
int32_t patchFileRead(char kind, uint16_t slot, uint32_t off, uint8_t* buf, uint32_t max, uint32_t* total) {
    if (!sdOk()) return -1;
    char nm[48];
    if (kind == 'M') snprintf(nm, sizeof nm, "PMOR3/MANIFEST.TXT");
    else if (kind == 'T') snprintf(nm, sizeof nm, "PMOR3/PATCHES/P%02u.TXT", (unsigned)(slot & 63));
    else if (kind == 'R') { const char* e = storePresetEngineName(slot); if (!e) return -1; snprintf(nm, sizeof nm, "PMOR3/PRESETS/%s.TXT", e); }
    else if (kind == 'W') snprintf(nm, sizeof nm, "PMOR3/SAMPLES/S%04u.WAV", (unsigned)slot);
    else {
        if (slot >= PATCH_SLOTS) return -1;
        if (kind == 'P' || kind == 'K') patchName(nm, (uint8_t)slot); else audName(nm, (uint8_t)slot);
        if (kind == 'K' || kind == 'L') { size_t l = strlen(nm); strcpy(nm + l, ".BAK"); }
    }
    // 16 KB read-ahead: one open+seek+read serves 16 consecutive 1 KB chunks, so the per-open FAT/directory walk (~4 sectors) is <12 % of the
    // traffic. (A kept-open handle was tried: after it, the loop stalled for seconds on the next request; per-call open + big block is robust.)
    // A request at offset 0 always reloads, so a file that changed between two dumps is never served stale.
    static uint8_t SDRAM_BSS fb[16384]; static char fname[48]; static uint32_t foff = 0, fn = 0, fsz = 0; static bool fok = false;
    if (!(fok && off != 0 && !strcmp(fname, nm) && off >= foff && off < foff + fn)) {
        FIL f; UINT br;
        fok = false;
        if (f_open(&f, nm, FA_READ) != FR_OK) return -1;
        g_ff_opens++;
        fsz = (uint32_t)f_size(&f);
        if (off >= fsz) { f_close(&f); *total = fsz; return 0; }
        uint32_t want = fsz - off; if (want > sizeof fb) want = sizeof fb;
        bool ok = f_lseek(&f, off) == FR_OK && f_read(&f, fb, want, &br) == FR_OK && br == want;
        f_close(&f);
        if (!ok) return -1;
        strncpy(fname, nm, sizeof fname - 1); fname[sizeof fname - 1] = 0; foff = off; fn = want; fok = true;
    }
    g_ff_reads++;
    *total = fsz;
    uint32_t avail = foff + fn - off; uint32_t n = avail < max ? avail : max;
    memcpy(buf, fb + (off - foff), n);
    return (int32_t)n;
}

uint32_t patchStoreSampleLen(uint16_t id) {      // samples of a pool id or alias; 0 = unknown (caller falls back to the legacy tables)
    if (id < STORE_ID_BASE) id = patchStoreAlias(id);
    if (!id) return 0; const StoreSample* m = storeFindSample(storeDevice(), id); return m ? m->len : 0;
}

// SAVE_SCRATCH: write the LIVE patch into an EMPTY slot through the normal save path (legacy .PMR now, new-layout TXT queued), without
// switching slots. Refuses an occupied slot or the current one: it can never overwrite a user's patch.
int patchSaveScratch(uint8_t slot) {
    if (slot >= PATCH_SLOTS || !sdOk() || slot == g_cur_slot || patchExists(slot)) return -1;
    if (!patchSave(slot)) return -2;
    if (!bootguardSkipStorage()) storeQueue(storeDevice(), slot);
    return 0;
}

bool patchStoreSkipped() { return bootguardSkipStorage(); }

// STORE_RETRY / menu entry: undo a skip-storage boot without a power cycle. Re-queues every patch + preset; the existing time-sliced job does the work.
int patchStoreRetry() {
    if (!sdOk()) return -1;
    bootguardRetry();
    storeQueueAll(storeDevice());
    return 0;
}

// ---- user presets: next free slot of the engine's file. The export job may hold the one write handle: abort it (it re-queues itself).
bool patchNoSave() { return s_nosave; }
int patchRefsScan(uint16_t id, unsigned first, unsigned n) { return sdOk() ? storeRefsScan(storeDevice(), id, first, n) : 0; }
int patchBinDelete(uint16_t id) {
    if (s_nosave) return -5;
    if (!sdOk()) return -3;
    StoreCtx& sc = storeDevice(); storeJobAbort(sc);                     // the export job may hold the one write handle: it re-queues itself
    return storeBinDelete(sc, id, System::GetNow() / 1000u);             // deleted_at = uptime seconds (the card has no RTC)
}
int  patchBinCount() { return sdOk() ? storeBinCount(storeDevice()) : -1; }
bool patchBinAt(int i, uint16_t* id, uint32_t* len, uint32_t* at) { StoreBinEntry e; if (!sdOk() || !storeBinAt(storeDevice(), i, &e)) return false; *id = e.id; *len = e.len; *at = e.at; return true; }
uint32_t patchBinToken() { return sdOk() ? storeBinToken(storeDevice()) : 0; }
int patchBinRestore(uint16_t id) { if (s_nosave) return -3; if (!sdOk()) return -3; StoreCtx& sc = storeDevice(); storeJobAbort(sc); return storeBinRestore(sc, id); }
int patchBinEmpty(uint32_t token) { if (s_nosave) return -3; if (!sdOk()) return -3; StoreCtx& sc = storeDevice(); storeJobAbort(sc); return storeBinEmpty(sc, token); }
// ARCHIVE_PATCHES (protocol): move every patch file into PMOR3/ARCHIVE_<k>/ (rename only), then the live patch becomes the boot default.
int patchArchiveScan(unsigned first, unsigned n, uint32_t* hash, uint16_t* patches) { return sdOk() ? storeArchiveScan(storeDevice(), first, n, hash, patches) : -3; }
int patchArchiveBegin() {
    if (s_nosave) return -5;
    if (!sdOk()) return -3;
    if (aud_saving) { aud_saving = false; aud_writer.abort(); }          // the AUD writer holds the write handle too: never promote a partial .AUD
    s_aud_ld.cancel();
    return storeArchiveStart(storeDevice());                              // aborts the export job; resumes the unfinished ARCHIVE_<k>; patchTick then does one file per pass
}
static void patchArchiveFinish();
void patchArchiveTick() {                  // one file per main-loop pass, independent of any host session
    int r = storeArchiveNext(storeDevice());
    if (r == 0) patchArchiveFinish();
}
int patchArchiveList(unsigned slot, char* out, size_t n) { return sdOk() ? storeArchiveList(storeDevice(), slot, out, n) : -3; }
int patchArchiveState(char* out, size_t n) {   // 1 running, 0 idle; "k= moved= failed= fail=a,b"
    const StoreArch& a = storeDevice().arch; int o = snprintf(out, n, "state=%s k=%u moved=%u failed=%u", a.active ? "running" : "idle", (unsigned)a.k, (unsigned)a.moved, (unsigned)a.failed);
    if (o > 0 && (size_t)o < n && a.failed) { o += snprintf(out + o, n - (size_t)o, " fail="); for (unsigned i = 0; i < a.failed && i < 6 && (size_t)o < n; i++) o += snprintf(out + o, n - (size_t)o, "%s%s", i ? "," : "", a.fail[i]); }
    return a.active ? 1 : 0;
}
static void patchArchiveFinish() {         // patchNew without the save: nothing is reloaded from the archive and no file is written
    patchApply(s_default);
    for (int i = 0; i < SMP_SLOTS; i++) smp.len[i] = 0;
    smp.audio_dirty = false;
    clockSetLoopLen(seqLongestLoop());
    g_exists_mask = 0; patchPoolInvalidate();
    g_dirty = false; s_last_saved_crc = s_last_seen_crc = patchLiveCrc();
    qspiMirrorWrite();
}
// SAVE_PRESET <engine 0..11> <n> <name>: drums engines 0..3 (group order) n 0..31 = DrumUser (base sound of the group's first pad + the 4 depth knobs of
// that engine); synth engines 4..11 = SYN0..SYN7, idx 5..31 (SYN_PRESETS..) = the algorithm's current SynPreset. Verified write, .BAK kept by the fs.
int patchSavePresetAt(int engine, int n, const char* name, int pad) {
    if (engine < 0 || engine >= STORE_PRESET_ENGINES || n < 0 || n > 31 || !name || !*name) return -1;
    if (pad >= 0 && (engine >= 4 || pad >= N_PADS || drumPadGroup((uint8_t)pad) != (uint8_t)engine)) return -1;   // pad: drums only, and it must belong to that engine's group
    if (s_nosave) return -3;
    if (!sdOk() || bootguardSkipStorage()) return -2;
    StoreCtx& sc = storeDevice(); storeJobAbort(sc);
    if (engine < 4) {
        uint8_t group = (uint8_t)engine; DrumUser du; memset(&du, 0, sizeof du);
        du.base = drums.pad[pad >= 0 ? (uint8_t)pad : drumGroupFirstPad(group)].sound; for (int i = 0; i < DENG_PARAMS; i++) du.k[i] = drums.eng[group].x[i];
        return storeDrumUserSave(sc, group, (uint8_t)n, name, du) ? 0 : -2;
    }
    if (n < SYN_PRESETS) return -1;
    uint8_t algo = (uint8_t)(engine - 4);
    return storeSynUserSave(sc, algo, (uint8_t)n, name, syn.preset[algo]) ? 0 : -2;
}
int patchUserSaveSynth(uint8_t algo) {
    if (!sdOk() || bootguardSkipStorage()) return -2;
    int idx = synFileNextFree(algo); if (idx < 0) return -1;
    char nm[10]; snprintf(nm, sizeof nm, "U%02d", idx);
    StoreCtx& sc = storeDevice(); storeJobAbort(sc);
    return storeSynUserSave(sc, algo, (uint8_t)idx, nm, syn.preset[algo]) ? idx : -2;
}
int patchUserSaveDrum(uint8_t group, uint8_t base_sound) {
    if (!sdOk() || bootguardSkipStorage() || group >= DG_COUNT) return -2;
    int n = drumsUserNextFree(group); if (n < 0) return -1;
    char nm[10]; snprintf(nm, sizeof nm, "U%d", n + 1);
    DrumUser du; memset(&du, 0, sizeof du); du.base = base_sound; for (int i = 0; i < DENG_PARAMS; i++) du.k[i] = drums.eng[group].x[i];
    StoreCtx& sc = storeDevice(); storeJobAbort(sc);
    return storeDrumUserSave(sc, group, (uint8_t)n, nm, du) ? n : -2;
}
