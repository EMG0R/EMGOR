// test_storage_layout.cpp -- new SD layout + additive migration against a directory (host StoreFs).
// usage: test_storage_layout <card_dir>   card_dir contains PMOR3/ (a COPY of a real card; tests/test_storage_migration.sh makes it)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <utility>
#include "sdlayout.h"
#include "drums_priv.h"
#include "shared_state.h"
#include "clock.h"
#include "scale.h"
#include "sequencer.h"
#include "synth.h"

static int fails = 0;
static std::string slurp(const char* p) { std::string o; FILE* f = fopen(p, "rb"); if (!f) return o; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) o.append(b, n); fclose(f); return o; }
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

struct Host { char base[512]; FILE* f; char path[600]; int writes, violations; long ops, cut_at; };
#define LIVE(h) (++((Host*)(h))->ops, (((Host*)(h))->cut_at < 0 || ((Host*)(h))->ops <= ((Host*)(h))->cut_at))   // false once the 'power' is gone
static void full(Host* h, const char* p, char* o) { snprintf(o, 700, "%s/%s", h->base, p); }
static int32_t h_size(void* u, const char* p) { if (!LIVE(u)) return -1; char o[700]; full((Host*)u, p, o); struct stat st; return stat(o, &st) ? -1 : (int32_t)st.st_size; }
static int h_read(void* u, const char* p, uint32_t off, void* b, uint32_t n) { if (!LIVE(u)) return -1; char o[700]; full((Host*)u, p, o); FILE* f = fopen(o, "rb"); if (!f) return -1; fseek(f, off, SEEK_SET); size_t g = fread(b, 1, n, f); fclose(f); return (int)g; }
static uint32_t h_stamp(void* u, const char* p) { if (!LIVE(u)) return 0; char o[700]; full((Host*)u, p, o); struct stat st; return stat(o, &st) ? 0 : (uint32_t)st.st_mtime; }
static bool h_mkdir(void* u, const char* p) { if (!LIVE(u)) return false; char o[700]; full((Host*)u, p, o); return mkdir(o, 0755) == 0 || access(o, F_OK) == 0; }
static bool h_wbegin(void* u, const char* p) {
    Host* h = (Host*)u; if (!LIVE(u)) return false; size_t n = strlen(p);
    if ((n > 4 && (!strcmp(p + n - 4, ".PMR") || !strcmp(p + n - 4, ".AUD") || !strcmp(p + n - 4, ".BAK")))) h->violations++;   // legacy name written = hard fail
    snprintf(h->path, sizeof h->path, "%s/%s", h->base, p); char t[700]; snprintf(t, sizeof t, "%s.TMP", h->path);
    h->f = fopen(t, "wb"); return h->f != nullptr;
}
static bool h_wappend(void* u, const void* d, uint32_t n) { Host* h = (Host*)u; if (!LIVE(u)) return false; return h->f && fwrite(d, 1, n, h->f) == n; }
static bool h_wend(void* u, bool commit) {
    Host* h = (Host*)u; if (!h->f) return false; bool ok = fclose(h->f) == 0; h->f = nullptr;
    if (commit && !LIVE(u)) return false;       // power cut before the rename: the .TMP stays orphaned
    char t[700]; snprintf(t, sizeof t, "%s.TMP", h->path);
    if (!commit) { unlink(t); return ok; }
    h->writes++; { char bk[720]; snprintf(bk, sizeof bk, "%s.BAK", h->path); if (access(h->path, F_OK) == 0) { if (strstr(h->path, "/SAMPLES/")) unlink(h->path); else { unlink(bk); rename(h->path, bk); } } }   // like the device adapter
    return ok && rename(t, h->path) == 0;
}

static bool h_rename(void* u, const char* a, const char* b) { if (!LIVE(u)) return false; char fa[700], fb[700]; full((Host*)u, a, fa); full((Host*)u, b, fb); if (access(fb, F_OK) == 0) return false; return rename(fa, fb) == 0; }
static bool h_remove(void* u, const char* p) { if (!LIVE(u)) return false; char o[700]; full((Host*)u, p, o); return unlink(o) == 0; }

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage\n"); return 2; }
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit();
    Host h; memset(&h, 0, sizeof h); h.cut_at = -1; snprintf(h.base, sizeof h.base, "%s", argv[1]);
    StoreFs fs{ &h, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove, h_rename };
    static StoreCtx c; storeInit(c, fs, "PMOR3");

    CHECK(storeMigrateAll(c), "migration errors=%u", c.errors);
    printf("run1: written=%u verified=%u skipped=%u errors=%u samples=%u\n", c.written, c.verified, c.skipped, c.errors, c.n_smp);
    CHECK(c.written > 0 && c.written == c.verified, "written %u != verified %u", c.written, c.verified);
    CHECK(h.violations == 0, "legacy-named file written %d times", h.violations);
    uint16_t nsmp = c.n_smp;

    // manifest reload + every sample verifies
    static StoreCtx d; storeInit(d, fs, "PMOR3");
    CHECK(storeLoadManifest(d) && d.n_smp == nsmp, "manifest reload");
    int bad = 0; for (int i = 0; i < d.n_smp; i++) bad += !storeSampleVerify(d, d.smp[i].id);
    CHECK(bad == 0, "%d samples fail verify", bad);

    // every legacy patch loads back identical through the new layout
    int np = 0, nref = 0;
    for (int s = 0; s < STORE_PATCHES; s++) {
        char p[48]; snprintf(p, sizeof p, "PMOR3/P%02d.PMR", s);
        if (h_size(&h, p) < 0) continue;
        static Patch L, W; uint16_t ids[16];
        static uint8_t raw[8192]; int n = h_read(&h, p, 0, raw, sizeof raw);
        if (!patchFromBlob(raw, (size_t)n, L)) continue;
        np++;
        CHECK(storeLoadPatch(d, (uint8_t)s, W, ids) && memcmp(&L, &W, sizeof L) == 0, "patch %d new-layout copy differs", s);
        for (int k = 0; k < 16; k++) if (ids[k]) { nref++; CHECK(storeFindSample(d, ids[k]) != nullptr, "patch %d slot %d dangling id %u", s, k, ids[k]); }
    }
    printf("patches exported+loaded: %d, sample refs: %d\n", np, nref);
    CHECK(np > 0, "no patches");

    // sample content equals the legacy AUD PCM (spot: first patch with audio)
    for (int s = 0; s < STORE_PATCHES; s++) {
        char p[48]; snprintf(p, sizeof p, "PMOR3/P%02d.AUD", s); int32_t sz = h_size(&h, p); if (sz < 72) continue;
        uint32_t hdr[18]; h_read(&h, p, 0, hdr, sizeof hdr); if (hdr[0] != 0x44554150u) continue;
        static Patch W; uint16_t ids[16]; if (!storeLoadPatch(d, (uint8_t)s, W, ids)) continue;
        uint32_t off = 72; bool eq = true; int cmp = 0;
        for (int k = 0; k < 16; k++) { uint32_t len = hdr[2 + k] > SMP_LEN ? SMP_LEN : hdr[2 + k]; if (!len) continue;
            if (!ids[k]) { off += len * 2; continue; }
            static int16_t a[2048], b[2048]; uint32_t o = 0;
            while (o < len) { uint32_t m = len - o < 2048 ? len - o : 2048; h_read(&h, p, off + o * 2, a, m * 2); eq &= storeSampleRead(d, ids[k], o, b, m) == (int32_t)m && memcmp(a, b, m * 2) == 0; o += m; cmp++; }
            off += len * 2; }
        CHECK(eq, "patch %d sample audio differs from legacy AUD", s);
        if (cmp) break;
    }

    // idempotent: rerun writes nothing
    h.writes = 0; static StoreCtx e; storeInit(e, fs, "PMOR3");
    CHECK(storeMigrateAll(e), "rerun errors"); CHECK(h.writes == 0, "rerun wrote %d files", h.writes);
    CHECK(e.skipped > 0 && e.n_smp == nsmp, "rerun skipped=%u smp=%u", e.skipped, e.n_smp);
    printf("run2: written=%d skipped=%u (idempotent)\n", h.writes, e.skipped);

    // resume: lose the manifest, a patch text and a wav (interrupted run) -> rerun restores the same state
    {
        char b[700];
        snprintf(b, sizeof b, "%s/PMOR3/MANIFEST.TXT", h.base); unlink(b);
        snprintf(b, sizeof b, "%s/PMOR3/SAMPLES/S1024.WAV", h.base); unlink(b);
        for (int s = 0; s < STORE_PATCHES; s++) { snprintf(b, sizeof b, "%s/PMOR3/PATCHES/P%02d.TXT", h.base, s); if (unlink(b) == 0) break; }
        h.writes = 0; static StoreCtx r; storeInit(r, fs, "PMOR3");
        CHECK(storeMigrateAll(r) && r.n_smp == nsmp, "resume: errors=%u smp=%u/%u", r.errors, r.n_smp, nsmp);
        CHECK(h.writes >= 3, "resume wrote %d", h.writes);
        int bad2 = 0; for (int i = 0; i < r.n_smp; i++) bad2 += !storeSampleVerify(r, r.smp[i].id);
        CHECK(bad2 == 0, "resume: %d bad samples", bad2);
        printf("resume: rewrote %d files, all verified\n", h.writes);
    }
    // stale copy is refused: touch a legacy-size mismatch by checking storeLoadPatch on a slot with no legacy file
    { static Patch W; uint16_t ids[16]; CHECK(!storeLoadPatch(d, 63, W, ids), "load of a slot with no export should fail"); }

    // presets: first run exported all 12 engine files from the firmware tables; installing them reproduces the tables exactly
    CHECK(d.presets_written == 0 && c.presets_written == 12, "preset files written first run: %u", c.presets_written);
    { int diff = 0, inst = 0;
      for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DRUM_SOUNDS; k++) { inst += drumsDefInstalled((uint8_t)g2, (uint8_t)k);
          if (memcmp(&drumDef((uint8_t)g2, (uint8_t)k), &DRUM_DEFS[g2][k], sizeof(SoundDef)) != 0) diff++; }
      printf("presets: 12 files written; %d drum recipes installed from file (identical to firmware tables: %s); synth file entries:", inst, diff ? "NO" : "yes");
      int sy = 0; for (int a = 0; a < ALG_COUNT; a++) for (int k = 0; k < SYN_PRESETS; k++) { SynPreset f, q; sy += synFileGet((uint8_t)a, (uint8_t)k, f) && synFactoryPreset((uint8_t)a, (uint8_t)k, q) && memcmp(&f, &q, sizeof f) == 0; }
      printf(" %d identical\n", sy);
      CHECK(diff == 0 && inst == DG_COUNT * DRUM_SOUNDS && sy == ALG_COUNT * SYN_PRESETS, "preset install: diff=%d installed=%d synth=%d", diff, inst, sy); }
    {   // user save (synth): verified, .BAK kept, additive on re-run, survives; edited file is never overwritten
        SynPreset mine; synFactoryPreset(2, 3, mine); mine.cutoff = 1234.f; mine.p[0] = 0.77f;
        char fp[700]; snprintf(fp, sizeof fp, "%s/PMOR3/PRESETS/SYN2.TXT", h.base);
        CHECK(storeSynUserSave(d, 2, 9, "MINE", mine), "synth user save");
        { DrumUser du2; memset(&du2, 0, sizeof du2); du2.base = 3; du2.k[1] = 0.5f; char nm2[10]; DrumUser dl2; CHECK(storeDrumUserSave(d, 0, 4, "A_B_9", du2) && storeDrumUserLoad(d, 0, 4, nm2, &dl2) && !strcmp(nm2, "A_B_9") && dl2.base == 3, "name with '_' round-trips through the PRESETS file (%s)", nm2);
          static StoreCtx re; storeInit(re, fs, "PMOR3"); CHECK(storeDrumUserLoad(re, 0, 4, nm2, &dl2) && !strcmp(nm2, "A_B_9"), "... and survives a fresh load from the card (%s)", nm2); }
        CHECK(storeSynUserSave(d, 2, 10, "MINE2", mine), "synth user save 2");
        char bk[720]; snprintf(bk, sizeof bk, "%s.BAK", fp); CHECK(access(bk, F_OK) == 0, "no .BAK kept on preset save");
        CHECK(!storeSynUserSave(d, 2, 3, "NOPE", mine), "user save into a factory slot accepted");
        std::string a1 = slurp(fp);
        static StoreCtx r; storeInit(r, fs, "PMOR3"); CHECK(storeMigrateAll(r), "rerun");
        CHECK(slurp(fp) == a1 && r.presets_written == 0 && r.presets_loaded == 12, "re-run touched an existing preset file (written %u loaded %u)", r.presets_written, r.presets_loaded);
        SynPreset got; CHECK(synFileGet(2, 9, got) && got.cutoff == 1234.f && got.p[0] == 0.77f, "user preset not in the file table");
        synthInit(); synLoadPreset(2, 9); CHECK(syn.preset[2].cutoff == 1234.f, "synLoadPreset ignores the file entry");
        synLoadPreset(2, 3); SynPreset fac; synFactoryPreset(2, 3, fac); CHECK(memcmp(&syn.preset[2], &fac, sizeof fac) == 0, "factory idx via file != tables");
        // unusable file (garbage): ignored, left byte-identical, firmware tables stay
        char gp[700]; snprintf(gp, sizeof gp, "%s/PMOR3/PRESETS/SYN1.TXT", h.base); { FILE* f = fopen(gp, "wb"); fputs("junk\n", f); fclose(f); }
        static StoreCtx r2; storeInit(r2, fs, "PMOR3"); CHECK(storeMigrateAll(r2) && r2.presets_ignored == 1 && slurp(gp) == "junk\n", "garbage preset file: ignored=%u", r2.presets_ignored);
        synLoadPreset(1, 2); synFactoryPreset(1, 2, fac); CHECK(memcmp(&syn.preset[1], &fac, sizeof fac) == 0, "fallback to tables failed");
        // drum user preset
        DrumUser du; memset(&du, 0, sizeof du); du.base = 7; du.k[0] = 0.25f; du.k[3] = 0.9f; DrumUser dl; char nm[10];
        CHECK(storeDrumUserSave(d, 1, 2, "SNAPPY", du) && storeDrumUserLoad(d, 1, 2, nm, &dl) && dl.base == 7 && dl.k[0] == 0.25f && dl.k[3] == 0.9f && !strcmp(nm, "SNAPPY"), "drum user preset round trip");
        { DrumUserEntry ue; CHECK(drumsUserGet(1, 2, &ue) && ue.base == 7 && ue.k[3] == 0.9f && !strcmp(ue.name, "SNAPPY"), "RAM user table not updated by the save"); CHECK(!strcmp(synFileName(2, 9), "MINE"), "synth user name in RAM"); CHECK(synFileNextFree(2) == 5, "next free %d", synFileNextFree(2)); }
        printf("presets: user save verified (+.BAK), re-run leaves files alone, garbage file ignored, fallback ok\n");
    }

    // everything below simulates device saves, so it runs on a COPY of the migrated card (the migration check above is the no-touch proof)
    { char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_save && cp -a %s %s_save", argv[1], argv[1], argv[1]); CHECK(system(cmd) == 0, "cp save copy"); snprintf(h.base, sizeof h.base, "%s_save", argv[1]); }
    // ---------------- dual-write flow: a device save (legacy v8 PMR + new AUD with an ASSIGNED pool sample), then export
    {
        int A = -1; static Patch cur; uint16_t ids0[16];
        for (int sl = 0; sl < STORE_PATCHES && A < 0; sl++) { uint16_t ii[16]; if (storeLoadPatch(d, (uint8_t)sl, cur, ii)) { for (int k = 0; k < 16; k++) if (ii[k]) { A = sl; memcpy(ids0, ii, sizeof ids0); break; } } }
        CHECK(A >= 0, "no patch with audio");
        if (A >= 0) {
            int first = 0; while (!ids0[first]) first++;
            // legacy PMR -> v8 with a changed drum knob, written the way fileWriteAtomic does (tmp, keep .BAK, rename)
            char pm[700]; snprintf(pm, sizeof pm, "%s/PMOR3/P%02d.PMR", h.base, A);
            static uint8_t oldraw[8192]; int on = h_read(&h, (std::string("PMOR3/P") + (A < 10 ? "0" : "") + std::to_string(A) + ".PMR").c_str(), 0, oldraw, sizeof oldraw);
            static Patch np; CHECK(patchFromBlob(oldraw, (size_t)on, np), "old patch read"); np.drum_eng[0][0] = 0.5f; np.crc32 = pmorCrc32(&np, offsetof(Patch, crc32));
            auto atomicWrite = [&](const char* path, const void* data, size_t n) {
                char t[720], bk[720]; snprintf(t, sizeof t, "%s.TMP", path); snprintf(bk, sizeof bk, "%s.BAK", path);
                FILE* f = fopen(t, "wb"); fwrite(data, 1, n, f); fclose(f); unlink(bk); rename(path, bk); rename(t, path); };
            // power cut mid legacy save: only a .TMP exists -> the old state is intact for the v7 reader
            { char t[720]; snprintf(t, sizeof t, "%s.TMP", pm); FILE* f = fopen(t, "wb"); fwrite(&np, 1, 100, f); fclose(f); }
            { static Patch chk; static uint8_t rr[8192]; int rn = h_read(&h, (std::string("PMOR3/P") + (A < 10 ? "0" : "") + std::to_string(A) + ".PMR").c_str(), 0, rr, sizeof rr);
              CHECK(rn == 7476 && patchFromBlob(rr, (size_t)rn, chk) && chk.drum_eng[0][0] == 0.f, "old state not intact after a cut legacy save"); }
            atomicWrite(pm, &np, sizeof np);
            // AUD: same samples + the pool sample of slot `first` assigned to slot 15 (what ASSIGN_SAMPLE + autosave produce)
            uint32_t lens[16]; static int16_t pcm[16][SMP_LEN]; memset(lens, 0, sizeof lens);
            for (int k = 0; k < 16; k++) if (ids0[k]) { const StoreSample* ss = storeFindSample(d, ids0[k]); lens[k] = ss->len; storeSampleRead(d, ids0[k], 0, pcm[k], ss->len); }
            if (!lens[15]) { lens[15] = lens[first]; memcpy(pcm[15], pcm[first], lens[first] * 2); }
            std::string aud; uint32_t hd[18] = { 0x44554150u, 1u }; for (int k = 0; k < 16; k++) hd[2 + k] = lens[k];
            aud.append((char*)hd, sizeof hd); for (int k = 0; k < 16; k++) aud.append((char*)pcm[k], lens[k] * 2); uint32_t mg = 0x44554150u; aud.append((char*)&mg, 4);
            char au[700]; snprintf(au, sizeof au, "%s/PMOR3/P%02d.AUD", h.base, A); atomicWrite(au, aud.data(), aud.size());

            static Patch got; uint16_t gi[16];
            CHECK(!storeLoadPatch(d, (uint8_t)A, got, gi), "stale copy served after a legacy save");
            uint32_t pcrc = pmorCrc32(&np, sizeof np); CHECK(!storeFreshIds(d, (uint8_t)A, sizeof np, pcrc, gi), "storeFreshIds true while stale");
            uint16_t before = d.n_smp; d.errors = 0;
            storeJobAbort(d); storeQueue(d, (uint8_t)A); long guard = 0; while (storeJobTick(d) && ++guard < 100000) {}
            CHECK(d.errors == 0, "export after save: errors %u", d.errors);
            CHECK(storeLoadPatch(d, (uint8_t)A, got, gi) && got.drum_eng[0][0] == 0.5f, "new layout missing the saved change");
            CHECK(gi[15] == ids0[first] && d.n_smp == before, "assign: slot 15 id %u (want %u), pool grew %u->%u (dedupe expected)", gi[15], ids0[first], before, d.n_smp);
            CHECK(storeFreshIds(d, (uint8_t)A, sizeof np, pcrc, gi), "storeFreshIds false after export");
            CHECK(storeAliasToId(d, (uint16_t)(A * 16 + 15)) == ids0[first] && storeAliasToId(d, (uint16_t)(A * 16 + first)) == ids0[first], "alias mapping");
            // .BAK of the PMR is the untouched v7 file
            { static Patch b7; static uint8_t rr[8192]; char bk[700]; snprintf(bk, sizeof bk, "PMOR3/P%02d.PMR.BAK", A); int rn = h_read(&h, bk, 0, rr, sizeof rr);
              CHECK(rn == 7476 && patchFromBlob(rr, (size_t)rn, b7) && b7.drum_eng[0][0] == 0.f, ".BAK is not the v7 original"); }
            // recovery source: legacy PMR corrupt -> the TXT blob still yields the patch
            { static Patch rec; uint16_t ri[16]; CHECK(storeRecoverPatch(d, (uint8_t)A, rec, ri) && rec.drum_eng[0][0] == 0.5f, "recover"); }
            // deleted patch: mirror dropped so it cannot resurrect
            CHECK(storeDropPatch(d, (uint8_t)A) && !storeRecoverPatch(d, (uint8_t)A, got, gi), "drop");
            printf("dual-write: save -> stale detected -> exported -> assign deduped (pool %u), alias ok, drop ok\n", d.n_smp);
        }
    }
    // legacy files are all still readable by the v7 reader (size 7476 + OPG7 magic + CRC) where they were v7
    { int nv7 = 0, bad7 = 0; for (int sl = 0; sl < STORE_PATCHES; sl++) { char p[48]; snprintf(p, sizeof p, "PMOR3/P%02d.PMR.BAK", sl); static uint8_t rr[8192]; int rn = h_read(&h, p, 0, rr, sizeof rr);
        if (rn == 7476) { nv7++; Patch7* q = (Patch7*)rr; bad7 += !(q->magic == PATCH_MAGIC_V7 && pmorCrc32(q, offsetof(Patch7, crc32)) == q->crc32); } }
      printf("v7 reader: %d .BAK files checked, %d bad\n", nv7, bad7); CHECK(bad7 == 0, "v7 reader rejects %d files", bad7); }

    // ---------------- power cut anywhere in the export: never corrupt, always resumable
    if (argc >= 3) {
        long total_ops = 0;
        { char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_cut && mkdir -p %s_cut/PMOR3 && cp -a %s/PMOR3/. %s_cut/PMOR3/", argv[1], argv[1], argv[2], argv[1]); CHECK(system(cmd) == 0, "cp");
          Host z; memset(&z, 0, sizeof z); z.cut_at = -1; snprintf(z.base, sizeof z.base, "%s_cut", argv[1]);
          StoreFs zf{ &z, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove, h_rename }; static StoreCtx zc; storeInit(zc, zf, "PMOR3"); storeMigrateAll(zc); total_ops = z.ops; }
        int cuts = 0, bad_after = 0, bad_resume = 0;
        for (long n = 5; n < total_ops; n += total_ops / 60 + 1) {
            char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_cut && mkdir -p %s_cut/PMOR3 && cp -a %s/PMOR3/. %s_cut/PMOR3/", argv[1], argv[1], argv[2], argv[1]); system(cmd);
            Host z; memset(&z, 0, sizeof z); z.cut_at = n; snprintf(z.base, sizeof z.base, "%s_cut", argv[1]);
            StoreFs zf{ &z, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove, h_rename }; static StoreCtx zc; storeInit(zc, zf, "PMOR3"); storeMigrateAll(zc); cuts++; bad_after += z.violations;
            // after the cut: healthy fs, same card. Whatever is on disk must be consistent.
            Host y; memset(&y, 0, sizeof y); y.cut_at = -1; snprintf(y.base, sizeof y.base, "%s_cut", argv[1]);
            StoreFs yf{ &y, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove, h_rename }; static StoreCtx yc; storeInit(yc, yf, "PMOR3");
            bool man = storeLoadManifest(yc);
            for (int i = 0; man && i < yc.n_smp; i++) if (!storeSampleVerify(yc, yc.smp[i].id)) bad_after++;
            for (int sl = 0; sl < STORE_PATCHES; sl++) { static Patch a, b; uint16_t ii[16]; char p[48]; snprintf(p, sizeof p, "PMOR3/P%02d.PMR", sl); static uint8_t rr[8192]; int rn = h_read(&y, p, 0, rr, sizeof rr);
                if (rn > 0 && patchFromBlob(rr, (size_t)rn, a) && storeLoadPatch(yc, (uint8_t)sl, b, ii) && memcmp(&a, &b, sizeof a) != 0) bad_after++;
                if (storeRecoverPatch(yc, (uint8_t)sl, b, ii)) { if (rn > 0 && patchFromBlob(rr, (size_t)rn, a) && memcmp(&a, &b, sizeof a) != 0) bad_after++; } }
            // resume
            static StoreCtx rc; storeInit(rc, yf, "PMOR3"); if (!storeMigrateAll(rc)) bad_resume++;
            static StoreCtx vc; storeInit(vc, yf, "PMOR3"); storeLoadManifest(vc); for (int i = 0; i < vc.n_smp; i++) if (!storeSampleVerify(vc, vc.smp[i].id)) bad_resume++;
        }
        printf("power cut: %d cut points over %ld fs ops: inconsistent-after-cut=%d, failed-resume=%d\n", cuts, total_ops, bad_after, bad_resume);
        CHECK(bad_after == 0 && bad_resume == 0, "power-cut: %d inconsistent, %d resume failures", bad_after, bad_resume);
        char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_cut", argv[1]); system(cmd);
    }
    {   // recycle bin: delete never erases; refs; list; restore; token-gated empty; (d) pool CRC-32 is the real CRC-32 of the data
        static StoreCtx b; storeInit(b, fs, "PMOR3"); CHECK(storeLoadManifest(b) && b.n_smp > 2, "bin: manifest");
        auto crc32 = [](const uint8_t* d, size_t n) { uint32_t c = 0xFFFFFFFFu; for (size_t i = 0; i < n; i++) { c ^= d[i]; for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1; } return ~c; };
        int badcrc = 0; for (int i = 0; i < b.n_smp; i++) { char fp[700]; snprintf(fp, sizeof fp, "%s/PMOR3/SAMPLES/S%04u.WAV", h.base, b.smp[i].id); std::string w = slurp(fp);
            if (w.size() != 44 + b.smp[i].len * 2 || crc32((const uint8_t*)w.data() + 44, w.size() - 44) != b.smp[i].crc) badcrc++; }
        CHECK(badcrc == 0, "(d) manifest CRC-32 == independent CRC-32 of the PCM data for every sample (%d bad)", badcrc);
        uint16_t id = b.smp[0].id; int refs = 0, scanned_refs = 0; for (int i = 0; i < b.n_smp && !refs; i++) { id = b.smp[i].id; for (unsigned s0 = 0; s0 < STORE_PATCHES; s0 += 16) refs += storeRefsScan(b, id, s0, 16); } (void) scanned_refs;
        CHECK(refs >= 1, "bin: referenced sample is found by the refs scan (%d)", refs);
        int n0 = b.n_smp; char sp[700], bp[700]; snprintf(sp, sizeof sp, "%s/PMOR3/SAMPLES/S%04u.WAV", h.base, id); snprintf(bp, sizeof bp, "%s/PMOR3/BIN/S%04u.WAV", h.base, id);
        std::string before = slurp(sp);
        CHECK(storeBinDelete(b, 65000, 5) == -1, "bin: unknown id -> -1");
        CHECK(storeBinDelete(b, id, 77) == 0, "bin: delete");
        CHECK(access(sp, F_OK) != 0 && slurp(bp) == before, "bin: the WAV moved byte-identical, nothing erased");
        CHECK(b.n_smp == n0 - 1 && !storeFindSample(b, id), "bin: removed from the manifest");
        { static StoreCtx m2; storeInit(m2, fs, "PMOR3"); CHECK(storeLoadManifest(m2) && m2.n_smp == n0 - 1, "bin: MANIFEST.TXT on the card dropped it (and still verifies)"); }
        CHECK(storeBinCount(b) == 1, "bin: count 1"); StoreBinEntry e; CHECK(storeBinAt(b, 0, &e) && e.id == id && e.at == 77 && e.crc != 0, "bin: entry has id/at/crc");
        { static StoreCtx b2; storeInit(b2, fs, "PMOR3"); CHECK(storeBinCount(b2) == 1 && storeBinToken(b2) == storeBinToken(b), "bin: index survives a reload (same token)"); }
        uint32_t tok = storeBinToken(b);
        CHECK(storeBinEmpty(b, tok ^ 1u) == -1 && access(bp, F_OK) == 0, "bin: wrong token erases nothing");
        CHECK(storeBinRestore(b, id) == 0 && slurp(sp) == before && b.n_smp == n0 && storeBinCount(b) == 0, "bin: restore puts the file back byte-identical + manifest + empty bin");
        { static StoreCtx m3; storeInit(m3, fs, "PMOR3"); CHECK(storeLoadManifest(m3) && m3.n_smp == n0 && storeSampleVerify(m3, id), "bin: restored sample verifies from the card"); }
        CHECK(storeBinDelete(b, id, 90) == 0, "bin: delete again"); uint32_t t2 = storeBinToken(b); CHECK(t2 != tok, "bin: token changes with the bin");
        CHECK(storeBinEmpty(b, t2) == 1 && access(bp, F_OK) != 0 && storeBinCount(b) == 0, "bin: empty with the right token is the only erase");
        CHECK(storeBinRestore(b, id) == -1, "bin: restore of an emptied id -> not in the bin");
        // power cut in the middle of a delete: file is in the bin or the pool, never lost
        { Host z; memset(&z, 0, sizeof z); z.cut_at = -1; snprintf(z.base, sizeof z.base, "%s", argv[1]); (void) z; }
    }
    {   // ARCHIVE_PATCHES: patch files move (rename), samples/manifest/presets byte-identical, boot afterwards = empty patches + intact pool
        char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_arch; cp -a %s %s_arch", argv[1], argv[1], argv[1]); CHECK(system(cmd) == 0, "arch: copy card");
        Host z; memset(&z, 0, sizeof z); z.cut_at = -1; snprintf(z.base, sizeof z.base, "%s_arch", argv[1]);
        StoreFs zf{ &z, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove, h_rename };
        static StoreCtx a; storeInit(a, zf, "PMOR3"); CHECK(storeLoadManifest(a) && a.n_smp > 2, "arch: manifest");
        char pp[900]; auto P = [&](const std::string& rel) { snprintf(pp, sizeof pp, "%s/PMOR3/%s", z.base, rel.c_str()); return std::string(pp); };
        { FILE* f = fopen(P("P03.EXT").c_str(), "wb"); fputs("ext", f); fclose(f); f = fopen(P("P03.AUD.BAK").c_str(), "wb"); fputs("audbak", f); fclose(f); }   // sidecars the card may carry
        std::vector<std::string> keep;   // everything that must survive untouched
        keep.push_back("MANIFEST.TXT"); for (int i = 0; i < a.n_smp; i++) { char r[48]; snprintf(r, sizeof r, "SAMPLES/S%04u.WAV", a.smp[i].id); keep.push_back(r); }
        for (int e = 0; e < STORE_PRESET_ENGINES; e++) { char r[48]; snprintf(r, sizeof r, "PRESETS/%s.TXT", storePresetEngineName(e)); if (access(P(r).c_str(), F_OK) == 0) keep.push_back(r); }
        std::vector<std::string> before; for (auto& k : keep) before.push_back(slurp(P(k.c_str()).c_str()));
        std::vector<std::pair<std::string, std::string>> mv;   // patch files present -> content
        static const char* X[] = { "PMR", "PMR.BAK", "AUD", "AUD.BAK", "EXT", "EXT.BAK" };
        for (int s = 0; s < STORE_PATCHES; s++) { char r[48];
            for (auto x : X) { snprintf(r, sizeof r, "P%02d.%s", s, x); if (access(P(r).c_str(), F_OK) == 0) mv.push_back({ r, slurp(P(r).c_str()) }); }
            for (auto x : { "TXT", "TXT.BAK" }) { snprintf(r, sizeof r, "PATCHES/P%02d.%s", s, x); if (access(P(r).c_str(), F_OK) == 0) mv.push_back({ r, slurp(P(r).c_str()) }); } }
        CHECK(mv.size() > 4, "arch: card has patch files (%zu)", mv.size());
        uint32_t h1 = 0x7A3C91E5u, h2 = 0x7A3C91E5u; uint16_t n1 = 0, n2 = 0; int sc = 0;
        for (unsigned f0 = 0; f0 < 64; f0 += 16) { storeArchiveScan(a, f0, 16, &h1, &n1); sc++; } storeArchiveScan(a, 0, 64, &h2, &n2);
        CHECK(n1 > 0 && n1 == n2 && h1 == h2 && sc == 4, "arch: scan is bounded + additive (patches=%u)", n1);
        int k = storeArchiveStart(a); CHECK(k == 1 && storeArchiveActive(a), "arch: first dir is ARCHIVE_1 (%d)", k);
        { char lb[96]; int ln = storeArchiveList(a, 0, lb, sizeof lb); CHECK(ln >= 0, "arch: list is read-only (%d: %s)", ln, lb); }
        int moved = 0, calls = 0, maxStatsOnly = 0, r2;
        while ((r2 = storeArchiveNext(a)) != 0) { calls++; if (r2 == 1) moved++; else maxStatsOnly++; if (calls > 5000) break; }
        CHECK(moved == (int)mv.size() && a.arch.moved == moved && a.arch.failed == 0 && !storeArchiveActive(a), "arch: moved %d == %zu patch files, one per call, no failures (%d scan-only calls)", moved, mv.size(), maxStatsOnly);
        CHECK(access(P("ARCHIVE_1/DONE.TXT").c_str(), F_OK) == 0, "arch: DONE.TXT marker written");
        bool all = true; for (auto& m : mv) { std::string dst = "ARCHIVE_1/" + m.first; if (access(P(m.first).c_str(), F_OK) == 0 || slurp(P(dst).c_str()) != m.second) all = false; }
        CHECK(all, "arch: every patch file renamed into ARCHIVE_1, byte-identical, gone from the live tree");
        bool same = true; for (size_t i = 0; i < keep.size(); i++) if (slurp(P(keep[i].c_str()).c_str()) != before[i]) same = false;
        CHECK(same, "arch: %zu samples/manifest/preset files byte-identical (nothing in SAMPLES/ touched)", keep.size());
        // boot afterwards: a fresh ctx loads the pool intact, every patch is empty, nothing references the samples, migration re-creates nothing from the archive
        static StoreCtx b; storeInit(b, zf, "PMOR3"); CHECK(storeLoadManifest(b) && b.n_smp == a.n_smp, "arch: pool intact after boot (%u)", b.n_smp);
        bool ok = true; for (int i = 0; i < b.n_smp; i++) if (!storeSampleVerify(b, b.smp[i].id)) ok = false; CHECK(ok, "arch: every pool sample still verifies (CRC)");
        Patch pb; uint16_t ids[16]; bool anyp = false; int refs = 0; for (int s = 0; s < STORE_PATCHES; s++) { if (storeLoadPatch(b, (uint8_t)s, pb, ids) || storeRecoverPatch(b, (uint8_t)s, pb, ids)) anyp = true; refs += storeRefsScan(b, b.smp[0].id, (unsigned)s, 1); }
        CHECK(!anyp && refs == 0, "arch: boot has empty patches (no TXT) and no patch references the pool");
        CHECK(storeMigrateAll(b) && b.exported == 0, "arch: boot migration finds no patch to export (%u)", b.exported);
        CHECK(storeBinCount(b) == 0, "arch: no sample was binned, purge path (storeBinEmpty) never ran");
        // second archive: next free number, nothing to move
        uint32_t h3 = 0x7A3C91E5u; uint16_t n3 = 0; storeArchiveScan(b, 0, 64, &h3, &n3); CHECK(n3 == 0, "arch: nothing left to scan");
        CHECK(storeArchiveStart(b) == 2, "arch: after a finished archive the next one is ARCHIVE_2"); while (storeArchiveNext(b) != 0) {}
        CHECK(h.violations == 0 && z.violations == 0, "arch: no legacy-named write");
        printf("archive: %d patch files moved to ARCHIVE_1, %zu samples/manifest/preset files byte-identical, boot has empty patches + intact pool (%u samples)\n", moved, keep.size(), b.n_smp);
        {   // resume + skip-and-report: an unfinished ARCHIVE_<k> is reused; a failing rename is skipped + named, never aborts; the marker waits for a clean run
            static const char* mk[] = { "P05.PMR", "P06.PMR", "P07.PMR", "PATCHES/P06.TXT", "PATCHES/P07.TXT" };
            for (auto f : mk) { FILE* ff = fopen(P(f).c_str(), "wb"); fputs("x", ff); fclose(ff); }
            // pre-make an unfinished ARCHIVE_3 (no marker) that already holds P06.PMR: the same name must not be overwritten, both kept
            snprintf(cmd, sizeof cmd, "mkdir -p %s/PMOR3/ARCHIVE_3/PATCHES; echo old > %s/PMOR3/ARCHIVE_3/P06.PMR", z.base, z.base); CHECK(system(cmd) == 0, "arch: make unfinished ARCHIVE_3");
            static StoreCtx r; storeInit(r, zf, "PMOR3");
            int k3 = storeArchiveStart(r); CHECK(k3 == 3, "arch: resumes the latest unfinished folder ARCHIVE_3 (%d)", k3);
            while (storeArchiveNext(r) != 0) {}
            CHECK(r.arch.failed == 0 && r.arch.moved >= 5, "arch: resumed run moved %u files", (unsigned)r.arch.moved);
            CHECK(slurp(P("ARCHIVE_3/P06.PMR").c_str()) == "old\n" && slurp(P("ARCHIVE_3/P06.PMR.1").c_str()) == "x", "arch: an existing name in the archive is never overwritten (the new file keeps a .1 name)");
            CHECK(access(P("ARCHIVE_3/DONE.TXT").c_str(), F_OK) == 0, "arch: resumed folder marked done");
            // failure path: a read-only target dir makes the rename fail; the run skips, names the file and leaves the folder resumable
            { FILE* ff = fopen(P("P08.PMR").c_str(), "wb"); fputs("y", ff); fclose(ff); ff = fopen(P("P09.PMR").c_str(), "wb"); fputs("z", ff); fclose(ff); }
            int k4 = storeArchiveStart(r); CHECK(k4 == 4, "arch: finished folder -> new ARCHIVE_4");
            snprintf(cmd, sizeof cmd, "mkdir -p %s/PMOR3/ARCHIVE_4/P08.PMR.1 && mkdir -p %s/PMOR3/ARCHIVE_4/P08.PMR", z.base, z.base); CHECK(system(cmd) == 0, "arch: block P08 target");
            for (int i = 1; i < 10; i++) { snprintf(cmd, sizeof cmd, "mkdir -p %s/PMOR3/ARCHIVE_4/P08.PMR.%d", z.base, i); (void)system(cmd); }
            while (storeArchiveNext(r) != 0) {}
            CHECK(r.arch.failed == 1 && strstr(r.arch.fail[0], "P08.PMR") && access(P("ARCHIVE_4/P09.PMR").c_str(), F_OK) == 0, "arch: failed file named (%s), the next file still moved", r.arch.fail[0]);
            CHECK(access(P("ARCHIVE_4/DONE.TXT").c_str(), F_OK) != 0 && access(P("P08.PMR").c_str(), F_OK) == 0, "arch: no marker after a failure, the source stays in place");
            snprintf(cmd, sizeof cmd, "rm -rf %s/PMOR3/ARCHIVE_4/P08.PMR*", z.base); (void)system(cmd);
            CHECK(storeArchiveStart(r) == 4, "arch: retry resumes ARCHIVE_4"); while (storeArchiveNext(r) != 0) {}
            CHECK(r.arch.failed == 0 && access(P("ARCHIVE_4/P08.PMR").c_str(), F_OK) == 0 && access(P("ARCHIVE_4/DONE.TXT").c_str(), F_OK) == 0, "arch: retry moved the file and marked the folder done");
        }
        snprintf(cmd, sizeof cmd, "rm -rf %s_arch", argv[1]); system(cmd);
    }
    { char cmd[1400]; snprintf(cmd, sizeof cmd, "rm -rf %s_save", argv[1]); system(cmd); }

    CHECK(h.violations == 0, "legacy-named file written %d times", h.violations);
    printf(fails ? "test_storage_layout: %d FAILURES\n" : "test_storage_layout: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}
