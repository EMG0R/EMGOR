// files.cpp — atomic-write / chunked-write helpers over FatFS.
// Pattern lifted from PMOR_V2's patchSaveSD/audBeginSave/audSaveTick
// (v2-audit.md §8, L1262/1329/1402/1441 — temp -> rename -> keep-as-backup,
// repeated identically for every sidecar file in v2). See store/files.h.
#include "files.h"
#include "../hw/hw.h"
#include <string.h>

// Build NAME's companion in `out`: the full name with ".EXT3" appended
// (e.g. "PATCH00.PMR" + "TMP" -> "PATCH00.PMR.TMP"). Long filenames are
// enabled (_USE_LFN 1), so this stays distinct per caller even when two
// files share a base name (PATCH00.PMR vs PATCH00.AUD each get their own
// .TMP/.BAK) — swapping just the 8.3 extension would collide between them.
static void fileVariant(const char* name, char* out, size_t outsz, const char* ext3) {
    size_t n = strlen(name);
    if (n + 5 >= outsz) n = (outsz > 5) ? outsz - 5 : 0;   // defensive truncation
    memcpy(out, name, n);
    out[n] = '.'; out[n + 1] = ext3[0]; out[n + 2] = ext3[1]; out[n + 3] = ext3[2]; out[n + 4] = 0;
}

FileDiag file_diag = { 0, 0, 0, 0 };
void fileDiagNote(char op, uint8_t fr) { file_diag.op = op; file_diag.fr = fr; file_diag.fails++; }

// Write NAME.TMP fully, then read it back and compare byte-for-byte. A bit-bang
// SPI write that the card ACKs can still land corrupted on marginal wiring; the
// read-back is the only proof the bytes are really on the card.
static bool writeTmpVerified(const char* tmp, const void* data, size_t n) {
    FIL f;
    FRESULT r = f_open(&f, tmp, FA_WRITE | FA_CREATE_ALWAYS);
    if (r != FR_OK) { fileDiagNote('o', (uint8_t)r); return false; }
    UINT bw = 0;
    FRESULT wr = f_write(&f, data, (UINT)n, &bw);
    if (wr == FR_OK) wr = f_sync(&f);
    f_close(&f);
    if (wr != FR_OK || bw != n) { fileDiagNote('w', (uint8_t)wr); f_unlink(tmp); return false; }

    r = f_open(&f, tmp, FA_READ);
    if (r != FR_OK) { fileDiagNote('d', (uint8_t)r); f_unlink(tmp); return false; }
    static uint8_t chk[512];
    const uint8_t* src = (const uint8_t*)data;
    size_t off = 0;
    bool same = true;
    while (off < n && same) {
        UINT want = (UINT)((n - off) > sizeof chk ? sizeof chk : (n - off)), br = 0;
        r = f_read(&f, chk, want, &br);
        if (r != FR_OK || br != want) { fileDiagNote('d', (uint8_t)r); same = false; break; }
        if (memcmp(chk, src + off, want) != 0) { fileDiagNote('v', 0); same = false; break; }
        off += want;
    }
    f_close(&f);
    if (!same) f_unlink(tmp);
    return same;
}

bool fileWriteAtomic(const char* name, const void* data, size_t n) {
    if (!sdOk()) return false;
    char tmp[24], bak[24];
    fileVariant(name, tmp, sizeof tmp, "TMP");
    fileVariant(name, bak, sizeof bak, "BAK");

    if (!writeTmpVerified(tmp, data, n)) {
        // once: fall back to the slow SPI clock for the rest of the session and retry
        if (sdIsFast()) {
            sdSetFast(false);
            file_diag.verify_retries++;
            if (!writeTmpVerified(tmp, data, n)) return false;
        } else {
            return false;
        }
    }

    f_unlink(bak);
    f_rename(name, bak);                          // keep the prior good file (ok if none existed)
    FRESULT r = f_rename(tmp, name);
    if (r != FR_OK) {
        fileDiagNote('r', (uint8_t)r);
        f_rename(bak, name);                       // promotion failed -> restore the backup
        return false;
    }
    return true;
}

int fileRead(const char* name, void* buf, size_t max) {
    if (!sdOk()) return -1;
    FIL f;
    if (f_open(&f, name, FA_READ) != FR_OK) return -1;
    UINT br = 0;
    FRESULT r = f_read(&f, buf, (UINT)max, &br);
    f_close(&f);
    if (r != FR_OK) return -1;
    return (int)br;
}

bool ChunkedWriter::begin(const char* name) {
    active_ = false; ok_ = false;
    if (!sdOk()) return false;
    strncpy(name_, name, sizeof(name_) - 1);
    name_[sizeof(name_) - 1] = 0;
    char tmp[24];
    fileVariant(name_, tmp, sizeof tmp, "TMP");
    if (f_open(&fil_, tmp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    active_ = true; ok_ = true;
    return true;
}

bool ChunkedWriter::tick(const void* data, size_t n) {
    if (!active_ || !ok_) return false;
    UINT bw = 0;
    FRESULT wr = f_write(&fil_, data, (UINT)n, &bw);
    if (wr != FR_OK || bw != n) { ok_ = false; return false; }
    return true;
}

bool ChunkedWriter::finish() {
    if (!active_) return false;
    f_close(&fil_);
    active_ = false;
    char tmp[24];
    fileVariant(name_, tmp, sizeof tmp, "TMP");
    if (!ok_) { f_unlink(tmp); return false; }      // a failed tick() -> discard, live file untouched

    char bak[24];
    fileVariant(name_, bak, sizeof bak, "BAK");
    f_unlink(bak);
    f_rename(name_, bak);
    if (f_rename(tmp, name_) != FR_OK) {
        f_rename(bak, name_);
        return false;
    }
    return true;
}

// Caller-initiated stop (2026-10): unlike finish(), this NEVER promotes — ok_ being true just
// means every tick() so far wrote successfully, not that the save is complete, and only the
// caller knows which. Close and discard the .TMP; the live file and its .BAK are never
// touched, so a switch-patches-mid-save never costs a write cycle or a .BAK rotation the way
// routing the abort through finish() used to.
void ChunkedWriter::abort() {
    if (!active_) return;
    f_close(&fil_);
    active_ = false;
    char tmp[24];
    fileVariant(name_, tmp, sizeof tmp, "TMP");
    f_unlink(tmp);
}
