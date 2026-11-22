// patch.h — 64 patches on SD (4 pages × 16), each a file PATCHnn.PMR (+ PATCHnn.AUD audio sidecar).
// QSPI holds only a tiny "boot" record (last patch, globals). Autosave debounced.
#pragma once
#include <stdint.h>
#include "../config.h"

struct Globals {                         // not per-patch
    float   key_semis;                   // continuous
    uint8_t scale_mode;
    float   input_gain;
    float   trellis_brightness;          // 0..1, LINEAR control value (gamma applied at the
                                          // point of use — ui/render.h's g_trellis_brightness_ctrl
                                          // / g_trellis_brightness, TASK 9 2026-10)
    uint8_t clock_mode;                  // PO SY0..SY5
    uint8_t last_patch;                  // boots here
    float   master_fader;
    // OLEDBRT / oled_contrast field REMOVED (2026-10, GLOB_VERSION bumped 2->3): the OLED
    // contrast control did nothing useful and is gone, not hidden (see store/patch.cpp's
    // defaultGlobals()/globalsApply()). GlobalsRecord (store/patch.cpp) already wraps this
    // struct in {magic, version, Globals} and patchInit() already discards anything whose
    // version doesn't match to defaults BEFORE ever reading a field out of it — that's the
    // same mechanism the "+ bpm" layout change used (see GLOB_VERSION's history comment), so
    // dropping this field outright and bumping the version is the established, safe way to
    // change this struct's layout: an old on-device blob is never reinterpreted with the new
    // layout, it's reset to defaults once, cleanly. No field needs to stay as padding.
    float   swing;                       // 0..1
    float   bpm;                         // master tempo, survives reboots (v2 of the record)
    float   clock_lead_ms;               // follower clock lead (APPENDED, GLOB_VERSION unchanged: an old record's erased tail reads NaN -> 0)
};
extern Globals glob;

struct PatchMeta { bool exists; };

void  patchInit();                        // mounts (via hw/sd), loads globals from QSPI, loads last patch
bool  patchLoad(uint8_t slot);            // sound only — nav page unchanged
bool  patchNew(uint8_t slot);             // fresh (boot-default) patch in an empty slot, saved at once, becomes current
bool  patchSave(uint8_t slot);            // full write (blocking, control plane) — prefer markDirty
void  patchDelete(uint8_t slot);
bool  patchExists(uint8_t slot);
uint8_t patchCurrent();
// autosave
void  patchMarkDirty();                   // any edit → save AUTOSAVE_DEBOUNCE_MS after the last one
void  patchTick(uint32_t now_ms);         // run the debounce + chunked audio save (never stalls UI > 1 sector)
bool  patchSaving();                      // for the star (busy spinner)
bool  patchDirty();
void  globalsSave();                      // QSPI
void  patchFactoryReset();

// DASH diagnostics: saves that succeeded / failed this session, how the boot-slot load went
// (0 none, 1 ok from SD, 2 unreadable, 3 wrong size/magic, 4 CRC, 5 restored from the QSPI mirror),
// bytes read on a size mismatch, QSPI mirror writes this session.
struct PatchDiag { uint16_t saves_ok, saves_fail; uint8_t boot_load; uint16_t load_size; uint16_t qspi_saves; };
const PatchDiag& patchDiag();                // store/patch.cpp addition: delete all 64 patches + reset globals
// ---- sample pool + live-patch services for the CDC protocol (store/patch.cpp, bottom)
int      patchPoolScan(uint8_t patch);                 // 1 ready, -1 none (control plane, one small SD read)
uint32_t patchPoolLen(uint8_t patch, uint8_t slot);    // samples
uint32_t patchPoolVer(uint8_t patch, uint8_t slot);
int32_t  patchPoolRead(uint16_t id, uint32_t off, uint8_t* buf, uint32_t max);   // bytes, 0 end, -1 err
void     patchPoolInvalidate();
int      patchAssignSample(uint8_t slot, uint16_t id); // 0 queued, -1 unknown, -2 busy
bool     patchAssignPoll(uint8_t* slot, int* err);     // finished assign? (one-shot)
void     patchSetNoSave(bool on);                      // NOSAVE: no autosave / QSPI writes (protocol test sessions)
int32_t  patchFileRead(char kind, uint16_t slot, uint32_t off, uint8_t* buf, uint32_t max, uint32_t* total);   // raw card file, read-only
bool     patchStoreSkipped();                          // true when this boot skipped storage (the UI offers RETRYSTO only then)
int      patchStoreRetry();                            // STORE_RETRY (also for a UI menu entry): re-enable + rerun storage tasks after a skip boot
int      patchSaveScratch(uint8_t slot);               // SAVE_SCRATCH (empty slots only)
void     patchClearLive();                             // CLEAR_PATCH

// ---- new SD layout (docs/STORAGE.md). Sample ids >= 1024 are the new global pool ids; ids < 1024 stay the old patch*16+slot
// aliases (patchPoolRead / patchAssignSample take both; patchStoreAlias maps an alias to its new id, 0 if none).
int      patchStoreSampleCount();
bool     patchStoreSampleAt(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* crc);   // MANIFEST rows
uint16_t patchStoreAlias(uint16_t old_id);
uint32_t patchStoreSampleLen(uint16_t id);   // pool id or alias -> samples (0 unknown)
// Background export/migration status for the UI / STATUS: state 0 idle, 1 running, 2 done, 3 done with errors.
struct PatchStoreStatus { uint8_t state, pending; uint16_t exported, written, verified, skipped, errors, samples; };
void     patchStoreStatus(PatchStoreStatus* out);
// User presets (docs/STORAGE.md): save the CURRENT sound into the next free user slot of its engine file (verified, .BAK kept).
// Blocking SD (user-initiated, ~0.5 s). Return the slot (synth: SYN_PRESETS..31, drums: 0..DRUM_USER_SLOTS-1) or -1 full, -2 write failed / no card.
int      patchSavePresetAt(int engine, int n, const char* name, int pad);   // protocol SAVE_PRESET: 0 ok, -1 arg, -2 io/skip, -3 NOSAVE session
bool     patchNoSave();
// recycle bin (protocol DELETE_POOL / BIN_*): wrappers over storeBin* on the device context; nothing here ever erases except patchBinEmpty(token)
int      patchRefsScan(uint16_t id, unsigned first, unsigned n);
int      patchBinDelete(uint16_t id);
int      patchBinCount();
bool     patchBinAt(int i, uint16_t* id, uint32_t* len, uint32_t* at);
uint32_t patchBinToken();
int      patchBinRestore(uint16_t id);
int      patchBinEmpty(uint32_t token);
int      patchArchiveScan(unsigned first, unsigned n, uint32_t* hash, uint16_t* patches);   // ARCHIVE_PATCHES: rename-only move of every patch file into PMOR3/ARCHIVE_<k>/
int      patchArchiveBegin();                    // starts the background job (resumes the unfinished ARCHIVE_<k>): k (>=1), -3 io, -5 nosave
void     patchArchiveTick();                     // one file per main-loop pass (called by patchTick while active); finishes -> live patch = boot default, nothing written or reloaded
int      patchArchiveList(unsigned slot, char* out, size_t n);   // read-only: patch files still present for the slot, count
int      patchArchiveState(char* out, size_t n); // 1 running / 0 idle; fills "state= k= moved= failed= fail="
int      patchUserSaveSynth(uint8_t algo);
int      patchUserSaveDrum(uint8_t group, uint8_t base_sound);
