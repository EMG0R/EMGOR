// sdlayout.h -- the NEW SD layout (docs/STORAGE.md) and the additive migration into it. Hardware-free:
// all I/O goes through StoreFs, so it runs on the host against a directory (tests) and on the device
// over FatFS (sdlayout_fatfs.cpp). CONTROL PLANE ONLY: blocking SD I/O, call from the background loop,
// never from the audio callback.
//
// Layout, all under <root> (device: "PMOR3"), ALONGSIDE the legacy P%02u.PMR/.AUD/.BAK files:
//   MANIFEST.TXT              one line per sample: "M <id> <len_samples> <rate> <ch> <crc32 hex> <file>"
//   SAMPLES/S%04u.WAV         16-bit PCM WAV (44-byte header + data); crc32 is over the PCM data bytes
//   PATCHES/P%02u.TXT         one text file per patch (header, source stamps, sample refs, v8 blob as hex)
//   PRESETS/<ENGINE>.TXT      one file per engine (KICK SNARE HAT PERC, SYN0..SYN7): user presets
// Rules: legacy files are never opened for write, renamed or deleted (every write goes through
// storeWritePath(), which refuses anything outside the new tree). Migration is idempotent and every
// written file is read back and CRC-verified before it counts as done.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pmor_core/patch_format.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/drums.h"

struct StoreFs {
    void* u;
    int32_t  (*size)(void* u, const char* path);                                  // bytes, -1 if missing
    int      (*read)(void* u, const char* path, uint32_t off, void* buf, uint32_t n);   // bytes read, -1 error
    uint32_t (*stamp)(void* u, const char* path);                                 // mtime-ish 32-bit stamp, 0 unknown
    bool     (*mkdir)(void* u, const char* path);                                 // true if created or already there
    bool     (*wbegin)(void* u, const char* path);                                // opens <path>.TMP
    bool     (*wappend)(void* u, const void* data, uint32_t n);
    bool     (*wend)(void* u, bool commit);                                       // commit: .TMP -> path (replaces a NEW-tree file); else discard
    bool     (*remove)(void* u, const char* path);                                // new-tree files only (guarded in sdlayout)
    bool     (*rename)(void* u, const char* from, const char* to);                // new-tree files only; false if `to` exists. NULL = no recycle bin
};

#define STORE_MAX_SAMPLES 1024            // 64 patches x 16 slots
#define STORE_PATCHES     64
#define STORE_ID_BASE 1024                // new sample ids are >= this; ids < 1024 are the old patch*16+slot aliases
struct StoreSample { uint16_t id; uint32_t len, rate; uint8_t ch; uint32_t crc; };   // file = S%04u.WAV
struct StoreBinEntry { uint16_t id; uint32_t len, rate; uint8_t ch; uint32_t crc, at; };   // BIN/S%04u.WAV + a line of BIN/INDEX.TXT (at = uptime s, no RTC)
#define STORE_BIN_MAX 256

#define PF_N 32
#define PF_BLOB 640
struct PfEntry { bool used; char name[10]; uint16_t len; uint8_t blob[PF_BLOB]; };
struct PfFile { PfEntry f[PF_N], u[PF_N]; };      // one engine's preset file in RAM
struct StorePatchTxt { uint32_t pmr_size, pmr_stamp, pmr_crc, aud_size, aud_stamp; char aud_src[8]; uint16_t ids[16]; bool blob_ok; };
struct StoreWr { bool ok; uint32_t crc, total; char path[48]; };

// background export job (one patch at a time, a bounded chunk of SD work per storeJobTick call)
enum { J_IDLE, J_BEGIN, J_CRC, J_WAV_W, J_WAV_V, J_MANIFEST, J_TXT_H, J_TXT_B, J_TXT_E, J_TXT_V };
struct StoreJob {
    uint8_t  phase, slot, si;
    uint32_t off, pos, crc, len, pcrc;       // off = AUD byte offset of slot si's PCM; pos = progress in bytes
    uint16_t id; bool is_new;
    uint32_t lens[16]; char aud[48];
    StorePatchTxt cur;
};
enum { STORE_IDLE = 0, STORE_RUNNING = 1, STORE_DONE = 2, STORE_ERRORS = 3 };

// ARCHIVE_PATCHES state: a resumable, session-independent background job (one file per storeArchiveNext call).
struct StoreArch {
    uint8_t  active, k, slot, ext;       // running; ARCHIVE_<k>; cursor (slot, extension index)
    uint16_t moved, failed;              // files renamed in / skipped (rename failed) this run
    char     fail[6][24];                // first failed file names (relative to the root, truncated)
};

struct StoreCtx {
    StoreFs fs;
    char    root[16];                      // "PMOR3"
    StoreSample smp[STORE_MAX_SAMPLES];
    uint16_t    n_smp;
    bool        man_dirty;                 // table changed since the manifest file was written
    uint64_t    pending;                   // patch slots queued for (re-)export
    uint8_t     state;                     // STORE_*
    uint16_t    written, verified, skipped, errors;   // files, since storeInit
    uint16_t    exported;                  // patches exported this run
    StoreJob    job; StoreWr w;
    Patch       jpb;                       // job's patch blob (kept across ticks)
    Patch       pb, pb2;                   // working blobs for the synchronous API
    PfFile      pf;
    char        ln[1200];                  // text line buffer
    uint8_t     preset_next;               // preset engine cursor of the boot job (STORE_PRESET_ENGINES = done)
    uint16_t    presets_written, presets_loaded, presets_ignored;
    StoreBinEntry bin[STORE_BIN_MAX]; uint16_t n_bin; bool bin_loaded;   // recycle bin index (lazy)
    uint8_t     scratch[8192];             // I/O staging (SDRAM in firmware)
    bool        started;
    StoreArch   arch;                      // patch ARCHIVE in progress (see storeArchiveStart)
};

void storeInit(StoreCtx& c, const StoreFs& fs, const char* root);
bool storeLoadManifest(StoreCtx& c);               // reads MANIFEST.TXT into c.smp; false if absent/corrupt (then empty)

// Export job. Queue slots, then call storeJobTick() from the background loop: ONE bounded chunk (<= 2 KB of SD
// traffic, or one small file) per call; returns true while work remains. Never from the audio callback.
// A patch whose TXT still matches its legacy files (size/mtime/CRC, all sample ids present) is skipped.
void storeQueue(StoreCtx& c, uint8_t slot);        // (re-)export this patch after its legacy files changed
void storeQueueAll(StoreCtx& c);                   // every slot (boot migration)
void storeJobAbort(StoreCtx& c);                   // drop the in-flight patch (a legacy write is starting); it is re-queued
bool storeJobTick(StoreCtx& c);
bool storeMigrateAll(StoreCtx& c);                 // blocking convenience for tests/tools: queue all, tick to idle, errors == 0
// ---- recycle bin (data is precious: delete never erases). PMOR3/BIN/S<id>.WAV + BIN/INDEX.TXT ("B id len rate ch crc32 at", END line).
// The only erase path is storeBinEmpty(token), token = storeBinToken() (changes whenever the bin changes). Bounded steps, background loop only.
int      storeRefsScan(StoreCtx& c, uint16_t id, unsigned first, unsigned n);      // patches in [first, first+n) whose TXT references the pool id
int      storeBinDelete(StoreCtx& c, uint16_t id, uint32_t at_s);                  // 0 ok, -1 no such sample, -2 bin full, -3 io, -4 unsupported
int      storeBinCount(StoreCtx& c);                                               // -1 io
bool     storeBinAt(StoreCtx& c, int i, StoreBinEntry* out);
int      storeBinRestore(StoreCtx& c, uint16_t id);                                // 0 ok, -1 not in the bin, -2 id taken in the pool, -3 io
uint32_t storeBinToken(StoreCtx& c);
int      storeBinEmpty(StoreCtx& c, uint32_t token);                               // files erased, -1 token mismatch, -3 io
// One-time archive of every patch file into PMOR3/ARCHIVE_<k>/ (rename only; samples/manifest/bin/presets untouched).
// Resumable: storeArchiveStart reuses the highest ARCHIVE_<k> that has no DONE.TXT marker (else makes the next free k); storeArchiveNext does ONE
// file (<= 8 stats + <= 1 rename) per call and never aborts on a file error (skip + record). The marker is written only when nothing failed.
int  storeArchiveScan(StoreCtx& c, unsigned first, unsigned n, uint32_t* hash, uint16_t* patches);   // accumulates hash + count of slots with any patch file
int  storeArchiveStart(StoreCtx& c);                                            // aborts the export job; k (new or resumed), -3 io, -4 unsupported
int  storeArchiveNext(StoreCtx& c);                                             // 1 = a file moved/failed, 2 = still scanning, 0 = finished (idle again)
bool storeArchiveActive(const StoreCtx& c);
int  storeArchiveList(StoreCtx& c, unsigned slot, char* out, size_t n);         // read-only: names of the patch files still present for the slot ("PMR TXT ..."), count
bool storeDropPatch(StoreCtx& c, uint8_t slot);    // patch deleted on the device: remove its PATCHES/Pnn.TXT mirror (samples stay in the pool)

// Reading the new layout (background loop only).
const StoreSample* storeFindSample(const StoreCtx& c, uint16_t id);
int32_t storeSampleRead(StoreCtx& c, uint16_t id, uint32_t off_samples, int16_t* buf, uint32_t n_samples);   // samples read, -1 err; CRC not rechecked here
bool    storeSampleVerify(StoreCtx& c, uint16_t id);                                     // re-reads the WAV, checks length + CRC
// Patch from the new layout. Returns false if missing/invalid/STALE (the legacy file changed after the export:
// then the caller must use the legacy files). ids[16] = sample id per sampler slot (0 = empty).
bool storeLoadPatch(StoreCtx& c, uint8_t slot, Patch& out, uint16_t ids[16]);
// Cheap freshness check for patchLoad, which already read the legacy .PMR (size, CRC32 of its bytes): reads only the TXT header
// and stats the AUD. true => ids[16] are valid for the legacy audio as it is now.
bool storeFreshIds(StoreCtx& c, uint8_t slot, uint32_t pmr_size, uint32_t pmr_crc, uint16_t ids[16]);
// Recovery: the TXT's own blob (fully parsed + CRC checked), no freshness test. For a corrupt legacy .PMR/.BAK.
bool storeRecoverPatch(StoreCtx& c, uint8_t slot, Patch& out, uint16_t ids[16]);
// old alias (patch*16+slot) -> new sample id via that patch's TXT, 0 if none
uint16_t storeAliasToId(StoreCtx& c, uint16_t old_id);

// Presets: PRESETS/<ENGINE>.TXT, ENGINES = KICK SNARE HAT PERC (24 built-in sounds each) and SYN0..SYN7 (5 built-in presets per algorithm).
//   PMORPRESET 2 / engine <E> / blob <bytes>
//   f <n> <name|-> <hex>   factory entry n: the firmware's recipe (drums: SoundDef bytes, synth: SynPreset bytes)
//   u <n> <name|-> <hex>   user entry n (synth: a SynPreset, index SYN_PRESETS+.. <= 31; drums: DrumUser = base sound + 4 depth knobs)
//   end <crc>
// Boot job (storeQueuePresets, run by storeJobTick after the patches): a missing file is written from the firmware tables (never
// overwrites an existing file, edited or not); an existing valid file is installed over the firmware tables (drums: recipe override,
// synth: file table consulted by synLoadPreset). A file whose blob size does not match this firmware (struct changed), or that fails its
// CRC, is ignored: the firmware tables stay in force.
#define STORE_PRESET_ENGINES 12
const char* storePresetEngineName(int i);              // 0..3 drums (group order), 4..11 SYN0..SYN7
void storeQueuePresets(StoreCtx& c);
struct DrumUser { uint8_t base; uint8_t pad_[3]; float k[4]; };
bool storeSynUserSave(StoreCtx& c, uint8_t algo, uint8_t idx, const char* name, const SynPreset& p);   // idx SYN_PRESETS..31; verified, .BAK kept by the fs
bool storeDrumUserSave(StoreCtx& c, uint8_t group, uint8_t n, const char* name, const DrumUser& u);    // n 0..31
bool storeDrumUserLoad(StoreCtx& c, uint8_t group, uint8_t n, char name[10], DrumUser* u);

// CRC32 helper shared with the protocol (same polynomial as pmorCrc32).
static inline uint32_t storeCrc(const void* d, size_t n) { return pmorCrc32(d, n); }
