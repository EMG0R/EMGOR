// sdlayout_fatfs.cpp -- StoreFs over FatFS for the device, plus the single shared StoreCtx. Control plane
// only (background loop); NEVER call from the audio callback. See sdlayout.h / docs/STORAGE.md.
#include "sdlayout_fatfs.h"
#include "../hw/hw.h"
#include <string.h>

static FIL s_wf; static char s_wpath[64]; static bool s_wopen = false;

static int32_t f_size_(void*, const char* p) { FILINFO fi; return f_stat(p, &fi) == FR_OK ? (int32_t)fi.fsize : -1; }
static int f_read_(void*, const char* p, uint32_t off, void* b, uint32_t n) {
    FIL f; if (f_open(&f, p, FA_READ) != FR_OK) return -1;
    UINT br = 0; FRESULT r = f_lseek(&f, off);
    if (r == FR_OK) r = f_read(&f, b, n, &br);
    f_close(&f); return r == FR_OK ? (int)br : -1;
}
void storeReadHandleClose() {}
static uint32_t f_stamp_(void*, const char* p) { FILINFO fi; return f_stat(p, &fi) == FR_OK ? (((uint32_t)fi.fdate << 16) | fi.ftime) : 0; }
static bool f_mkdir_(void*, const char* p) { FRESULT r = f_mkdir(p); return r == FR_OK || r == FR_EXIST; }
static bool f_wbegin_(void*, const char* p) {
    if (s_wopen || strlen(p) + 5 >= sizeof s_wpath) return false;
    strcpy(s_wpath, p);
    char t[72]; strcpy(t, p); strcat(t, ".TMP");
    if (f_open(&s_wf, t, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    s_wopen = true; return true;
}
static bool f_wappend_(void*, const void* d, uint32_t n) { UINT bw = 0; return s_wopen && f_write(&s_wf, d, n, &bw) == FR_OK && bw == n; }
static bool f_wend_(void*, bool commit) {
    if (!s_wopen) return false;
    s_wopen = false; bool ok = f_sync(&s_wf) == FR_OK; f_close(&s_wf);
    char t[72]; strcpy(t, s_wpath); strcat(t, ".TMP");
    if (!commit || !ok) { f_unlink(t); return false; }
    // new-tree file only (sdlayout's path guard); legacy files are never reachable here. Keep the previous copy as .BAK.
    char bk[72]; strcpy(bk, s_wpath); strcat(bk, ".BAK");
    FILINFO fi; bool keep = !strstr(s_wpath, "/SAMPLES/");     // pool WAVs are content-addressed: no .BAK
    if (f_stat(s_wpath, &fi) == FR_OK) { if (keep) { f_unlink(bk); f_rename(s_wpath, bk); } else f_unlink(s_wpath); }
    if (f_rename(t, s_wpath) != FR_OK) { if (keep) f_rename(bk, s_wpath); return false; }
    return true;
}

static bool f_rename_(void*, const char* a, const char* b) { return f_rename(a, b) == FR_OK; }   // FatFS refuses an existing target (FR_EXIST)
static bool f_remove_(void*, const char* p) { return f_unlink(p) == FR_OK; }   // sdlayout only passes new-tree paths

static StoreCtx SDRAM_BSS s_ctx;
static bool s_inited = false;
StoreCtx& storeDevice() {
    if (!s_inited) {
        StoreFs fs{ nullptr, f_size_, f_read_, f_stamp_, f_mkdir_, f_wbegin_, f_wappend_, f_wend_, f_remove_, f_rename_ };
        storeInit(s_ctx, fs, "PMOR3"); s_inited = true;
    }
    return s_ctx;
}
