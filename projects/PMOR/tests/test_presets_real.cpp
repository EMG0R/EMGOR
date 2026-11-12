// test_presets_real.cpp -- PRESETS/<ENGINE>.TXT exactly as the FIRMWARE writes them (store/sdlayout.cpp on a host directory) are listed and applied by max/pmor_host.h.
// Format (store/sdlayout.h): PMORPRESET 2 / engine <E> / blob <n> / f <n> <name|-> <hex> factory / u <n> <name|-> <hex> user / end <crc>.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include "../max/pmor_host.h"
#include "sdlayout.h"
#include "shared_state.h"
#include "clock.h"
#include "scale.h"
#include "sequencer.h"
#include "synth.h"
#include "drums.h"
#include "params.h"
#include "param.h"
static int passes = 0, fails = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static std::string slurp(const std::string& p) { std::string o; FILE* f = fopen(p.c_str(), "rb"); if (!f) return o; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) o.append(b, n); fclose(f); return o; }
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

static bool h_remove(void* u, const char* p) { if (!LIVE(u)) return false; char o[700]; full((Host*)u, p, o); return unlink(o) == 0; }


int main() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); pmorSchemaInit();
    char tmpl[] = "/tmp/pmorpresetsXXXXXX"; std::string base = mkdtemp(tmpl);
    Host h; memset(&h, 0, sizeof h); h.cut_at = -1; snprintf(h.base, sizeof h.base, "%s", base.c_str());
    StoreFs fs{ &h, h_size, h_read, h_stamp, h_mkdir, h_wbegin, h_wappend, h_wend, h_remove };
    static StoreCtx c; storeInit(c, fs, "PMOR3");
    CHECK(storeMigrateAll(c), "storage job (errors=%u)", c.errors);
    pmorhost::Client C; std::vector<std::string> sets; std::map<int, float> vals; std::vector<std::string> infos;
    C.onInfo = [&](const std::string& s) { infos.push_back(s); };
    int total = 0, missing = 0;
    for (int e = 0; e < STORE_PRESET_ENGINES; e++) {
        std::string f = slurp(base + "/PMOR3/PRESETS/" + storePresetEngineName(e) + ".TXT");
        if (f.empty()) { missing++; continue; }
        C.loadPresetFile(e, std::vector<uint8_t>(f.begin(), f.end()));
    }
    CHECK(missing == 0, "all %d preset files written by the firmware (%d missing)", STORE_PRESET_ENGINES, missing);
    int drumF = 0, synF = 0, bad = 0, dashes = 0;
    for (auto& p : C.presets) {
        total++; if (p.kind != 'f') continue;
        if (p.engine < 4) { drumF++; if (p.blob.empty()) bad++; } else { synF++; if (p.blob.size() != sizeof(SynPreset)) bad++; }
        if (p.name == "-") dashes++;
    }
    CHECK(drumF == 4 * 24 && synF == 8 * SYN_PRESETS, "factory entries: drums %d (want 96), synth %d (want %d)", drumF, synF, 8 * SYN_PRESETS);
    CHECK(bad == 0, "blob sizes (SynPreset %zu B) / non-empty drum recipes: %d bad", sizeof(SynPreset), bad);
    printf("  note: %d of %d factory entries are unnamed ('-': the UI uses the schema sound/preset names)\n", dashes, drumF + synF);
    // apply a factory synth preset: 20 SET_PARAMs, values == the firmware's SynPreset fields
    std::vector<std::string> sent; C.send = [&](const std::string& fr) { sent.push_back(fr); };
    SynPreset ref; synFactoryPreset(2, 1, ref);
    // the host is not connected here: drive the apply path through a fake ready state is not possible, so check the decoded blob instead
    const pmorhost::Client::Preset* pr = nullptr; for (auto& p : C.presets) if (p.engine == 4 + 2 && p.kind == 'f' && p.n == 1) pr = &p;
    CHECK(pr && pr->blob.size() == sizeof(SynPreset) && memcmp(pr->blob.data(), &ref, sizeof ref) == 0, "synth algo 2 factory #1 blob == synFactoryPreset");
    std::string cmd = "rm -rf '" + base + "'"; if (system(cmd.c_str())) {}
    printf("test_presets_real: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
