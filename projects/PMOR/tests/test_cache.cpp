// test_cache.cpp -- max/pmor_cache.h: session dir (<Live set>/PMOR) first, then ~/Music/PMOR_Cache; write to the session dir when usable; find() verifies size AND content CRC-32
// (wrong-crc files are quarantined as .bad and never used; two crc variants of one id coexist).
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../max/pmor_cache.h"
static int passes = 0, fails = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool exists(const std::string& f) { struct stat st; return stat(f.c_str(), &st) == 0; }
int main() {
    char tmpl[] = "/tmp/pmorcacheXXXXXX"; std::string root = mkdtemp(tmpl);
    std::string setdir = root + "/My Set Project/PMOR", cachedir = root + "/home/Music/PMOR_Cache";
    std::vector<uint8_t> pcm(2000 * 2); for (size_t i = 0; i < pcm.size(); i++) pcm[i] = (uint8_t)(i * 31 + 7);
    const uint32_t K = pmorcache::crc32(pcm.data(), pcm.size());          // the CRC-32 a PMOR manifest would carry for this PCM
    char kn[32]; snprintf(kn, sizeof kn, "S1024_%08X.aud", (unsigned)K);
    {   // saved set: session dir is created and used first
        pmorcache::Cache c; c.session = setdir; c.cache = cachedir; bool used = false;
        CHECK(c.find("S1024", K, 2000).empty(), "nothing cached yet");
        std::string f = c.store("S1024", K, pcm, &used);
        CHECK(!f.empty() && used && f.rfind(setdir, 0) == 0, "stored in the Live set folder (%s)", f.c_str());
        CHECK(!exists(cachedir + "/" + kn), "not duplicated into the shared cache");
        CHECK(c.find("S1024", K, 2000) == f, "found again (session first)");
        CHECK(c.find("S1024", K, 1999).empty(), "wrong length is not a hit");
        CHECK(c.find("S1024", 0xBEEF, 2000).empty(), "other crc is not a hit");
    }
    {   // unsaved set (no session dir): shared cache, created on demand
        pmorcache::Cache c; c.cache = cachedir; bool used = true;
        std::string f = c.store("S1025", K, pcm, &used);
        CHECK(!f.empty() && !used && f.rfind(cachedir, 0) == 0, "no session dir -> ~/Music/PMOR_Cache-style fallback (%s)", f.c_str());
        CHECK(c.find("S1025", K, 2000) == f, "found in the cache");
    }
    {   // read order: a sample present in both comes from the session dir; a cache-only sample is still found
        pmorcache::Cache c; c.session = setdir; c.cache = cachedir;
        c.store("S1025", K, pcm);                                   // goes to session (usable)
        CHECK(c.find("S1025", K, 2000).rfind(setdir, 0) == 0, "session wins over cache when both have the file");
        pmorcache::Cache c2; c2.session = root + "/other set/PMOR"; c2.cache = cachedir;
        CHECK(c2.find("S1025", K, 2000).rfind(c2.session, 0) == 0 && exists(cachedir + "/S1025_" + std::string(kn + 6)), "cache-only sample found from another set (copied into that set, cache kept)");
    }
    {   // unusable session dir (a file is in the way): falls back to the cache
        FILE* f = fopen((root + "/blocker").c_str(), "w"); fclose(f);
        pmorcache::Cache c; c.session = root + "/blocker/PMOR"; c.cache = cachedir; bool used = true;
        std::string out = c.store("S1026", K, pcm, &used);
        CHECK(!out.empty() && !used && out.rfind(cachedir, 0) == 0, "unwritable session dir -> cache (%s)", out.c_str());
        CHECK(!exists(out + ".part"), "no .part file left behind");
    }
    {   // content CRC is enforced: a same-size file under the right name but other content is quarantined and never used (the stale-sample bug)
        pmorcache::Cache c; c.session = root + "/v/PMOR"; c.cache = root + "/v/cache"; pmorcache::Cache::makeDirs(c.session); pmorcache::Cache::makeDirs(c.cache);
        std::vector<uint8_t> wrong(pcm.size(), 0x11); std::string n = "/" + std::string(kn);
        FILE* f = fopen((c.session + n).c_str(), "wb"); fwrite(wrong.data(), 1, wrong.size(), f); fclose(f);
        CHECK(c.find("S1024", K, 2000).empty(), "equal-size wrong-content file is not a hit");
        CHECK(!exists(c.session + n) && exists(c.session + n + ".bad") && c.quarantined.size() == 1, "quarantined as .bad");
        CHECK(!c.find("S1024", K, 2000, false).empty() == false, "(file is gone, size-only lookup finds nothing either)");
        // wrong copy in the session dir, good copy in the cache dir: the good one is used whatever the order
        f = fopen((c.session + n).c_str(), "wb"); fwrite(wrong.data(), 1, wrong.size(), f); fclose(f);
        f = fopen((c.cache + n).c_str(), "wb"); fwrite(pcm.data(), 1, pcm.size(), f); fclose(f);
        CHECK(c.find("S1024", K, 2000) == c.session + n && exists(c.cache + n), "good copy in the cache dir replaces a wrong-crc session copy (copied into the session dir, cache kept)");
        // two crc variants of one id coexist, each found only by its exact crc
        std::vector<uint8_t> pcm2(pcm.size(), 0x42); uint32_t K2 = pmorcache::crc32(pcm2.data(), pcm2.size());
        std::string f2 = c.store("S1024", K2, pcm2); std::string f1 = c.find("S1024", K, 2000);
        CHECK(!f2.empty() && !f1.empty() && f1 != f2 && c.find("S1024", K2, 2000) == f2, "two crc variants of one id coexist (%s / %s)", f1.c_str(), f2.c_str());
        CHECK(c.find("S1024", K ^ 1u, 2000).empty(), "an unknown crc finds nothing");
        // truncated file under the right name (not a hit; storing again replaces it)
        std::string t = c.session + "/S1030_" + std::string("0000ABCD.aud"); f = fopen(t.c_str(), "wb"); fwrite(pcm.data(), 1, 100, f); fclose(f);
        CHECK(c.find("S1030", 0xABCD, 2000).empty(), "truncated file is not a hit");
        CHECK(pmorcache::crc32((const uint8_t*)"123456789", 9) == 0xCBF43926u, "crc32 check value");
    }
    {   // pulled while the set was unsaved (cache), the set is saved later: find() copies the sample into <set>/PMOR; the cache keeps its file
        pmorcache::Cache u; u.cache = root + "/u/cache"; std::string cf = u.store("S2000", K, pcm);
        pmorcache::Cache c; c.cache = u.cache; c.session = root + "/u/My Set/PMOR";
        std::string got = c.find("S2000", K, 2000);
        std::string want = c.session + "/S2000_" + std::string(kn + 6);
        CHECK(got == want && exists(want) && exists(cf), "cache hit copied into the session dir (%s)", got.c_str());
        std::vector<uint8_t> b; pmorcache::Cache::readAll(want, b); CHECK(b == pcm, "copy is byte identical");
        CHECK(!exists(want + ".part") && c.copied.size() == 1, "no .part left, one copy reported");
        CHECK(c.find("S2000", K, 2000) == want && c.copied.size() == 1, "second lookup is a session hit, no second copy");
        // size-only lookups (the pool listing) never write
        pmorcache::Cache l; l.cache = u.cache; l.session = root + "/u/Other Set/PMOR";
        CHECK(!l.find("S2000", K, 2000, false).empty() && !exists(l.session), "unverified lookup does not copy");
        // a cache file with the wrong content is quarantined and nothing is copied
        pmorcache::Cache w; w.cache = root + "/u/wcache"; w.session = root + "/u/W Set/PMOR"; pmorcache::Cache::makeDirs(w.cache);
        std::vector<uint8_t> wrong(pcm.size(), 0x33); FILE* f = fopen((w.cache + "/" + kn).c_str(), "wb"); sizeof(wrong); fwrite(wrong.data(), 1, wrong.size(), f); fclose(f);
        std::string nm = std::string("S2000_") + (kn + 6);
        CHECK(w.find("S2000", K, 2000).empty() && !exists(w.session + "/" + nm), "wrong-crc cache file: not used, not copied");
        // unusable session dir: the cache path is returned, nothing breaks
        pmorcache::Cache x; x.cache = u.cache; x.session = root + "/blocker/PMOR";
        CHECK(x.find("S2000", K, 2000) == cf && x.copied.empty(), "unusable session dir: the cache file is used as before");
        // legacy path helper: any verified file outside the session dir
        pmorcache::Cache y; y.session = root + "/u/Legacy Set/PMOR"; std::string g = y.intoSession(cf, K);
        CHECK(g == y.session + "/S2000_" + std::string(kn + 6) && exists(cf), "intoSession copies by name and keeps the source");
        CHECK(y.intoSession(cf, K ^ 1u).empty(), "intoSession refuses a CRC mismatch");
        CHECK(y.intoSession(g, K).empty(), "a file already inside the session dir is not copied onto itself");
    }
    {   // default cache root is ~/Music/PMOR_Cache
        setenv("HOME", "/tmp/fakehome", 1); pmorcache::Cache c;
        CHECK(c.cacheRoot() == "/tmp/fakehome/Music/PMOR_Cache", "default cache root %s", c.cacheRoot().c_str());
        CHECK(pmorcache::Cache::expand("~/x") == "/tmp/fakehome/x", "~ expands");
    }
    std::string cmd = "rm -rf '" + root + "'"; if (system(cmd.c_str())) {}
    printf("test_cache: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
