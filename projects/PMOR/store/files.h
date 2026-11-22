// files.h — small SD (FatFS) helpers shared by store/. Control plane only
// (blocking SD I/O; the audio plane must never call anything here).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "fatfs.h"

// Atomic write: fully writes NAME.TMP, then promotes it to NAME, keeping
// the previous good file as NAME.BAK — a failed/torn write can never
// corrupt or truncate the live file. `name` is the target filename (e.g.
// "PATCH00.PMR"); its companions are the full name with ".TMP"/".BAK"
// appended (e.g. "PATCH00.PMR.TMP") — long filenames are enabled in this
// project's FatFS config (_USE_LFN 1), so this stays distinct per caller
// even when several files share a base name (PATCH00.PMR vs PATCH00.AUD).
// Returns false (nothing written) if the SD isn't mounted or any step fails.
bool fileWriteAtomic(const char* name, const void* data, size_t n);

// Last SD failure, for the DASH page / CDC: which step ('o'pen 'w'rite 'v'erify
// 'r'ename 'm'kdir 'd'read) and its FRESULT. 0/0 = nothing has failed yet.
struct FileDiag { char op; uint8_t fr; uint16_t fails; uint16_t verify_retries; };
extern FileDiag file_diag;
void fileDiagNote(char op, uint8_t fr);

// Reads up to `max` bytes of `name` into buf. Returns the number of bytes
// read, or -1 if the SD isn't mounted, the file doesn't exist, or the read
// failed.
int fileRead(const char* name, void* buf, size_t max);

// Chunked writer: begin() opens NAME's temp companion; tick() appends one
// chunk (the caller keeps each chunk <= 4KB so a big save — e.g. sampler
// audio — never stalls the UI for more than one sector's worth of SD I/O);
// finish() closes and promotes the temp file the same atomic way as
// fileWriteAtomic (NAME.BAK keeps the previous good file). If any tick()
// fails, finish() discards the temp file and returns false — the live file
// (and its .BAK) are left untouched.
//
// abort() (2026-10) is the other way a save can stop: a CALLER decides, mid-save, that it
// doesn't want this file any more (e.g. the patch being saved is no longer the one on
// screen). That is NOT the same thing as finish() — finish() means "this is everything,
// promote it"; a caller-initiated stop only has SOME of the data. Calling finish() for that
// used to be the only option, and finish() can't tell the difference from a genuinely
// complete save (ok_ is still true — every tick() so far actually succeeded), so it would
// promote the partial .TMP as the live file and demote the previous, complete file to .BAK —
// exactly backwards. abort() closes and discards the .TMP unconditionally, promotes nothing,
// and leaves the live file (and its .BAK) exactly as they were, the same guarantee finish()
// gives on a tick() failure.
struct ChunkedWriter {
    bool begin(const char* name);
    bool tick(const void* data, size_t n);
    bool finish();
    void abort();           // caller-initiated stop: discard the .TMP, promote nothing
    bool active() const { return active_; }

private:
    FIL  fil_{};
    char name_[24] = {};   // target filename, e.g. "PATCH00.AUD"
    bool active_    = false;
    bool ok_        = false;
};
