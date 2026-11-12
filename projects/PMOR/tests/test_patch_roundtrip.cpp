// test_patch_roundtrip.cpp -- patch blob: layout frozen, snapshot -> apply -> snapshot byte-identical,
// CRC valid, file round trip byte-identical, a one-bit flip is caught.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "patch_format.h"
#include "shared_state.h"
#include "clock.h"
#include "scale.h"
#include "metro.h"
#include "output.h"
#include "engine_util.h"
#include "sequencer.h"
#include "drums.h"
#include "drum_super.h"
#include <math.h>

static float g_dummyL[64], g_dummyR[64];
static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

// Layout of the blob on existing SD cards, measured from the pre-extraction struct (ff86e4c store/patch.cpp).
#ifndef PATCH_EXPECT_SIZE
#define PATCH_EXPECT_SIZE 0
#endif

int main() {
    memset(&g, 0, sizeof(g));
    clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    printf("sizeof(Patch)=%zu offsetof(crc32)=%zu\n", sizeof(Patch), offsetof(Patch, crc32));
    if (PATCH_EXPECT_SIZE) CHECK(sizeof(Patch) == (size_t)PATCH_EXPECT_SIZE, "Patch size %zu != frozen %d", sizeof(Patch), (int)PATCH_EXPECT_SIZE);

    // scribble a deterministic non-default state across every serialised field
    uint32_t r = 0xC0FFEEu;
    for (int a = 0; a < ALG_COUNT; a++) { for (int k = 0; k < 4; k++) syn.preset[a].p[k] = lcg01(r); syn.preset[a].cutoff = 200.f + 8000.f * lcg01(r); syn.preset[a].pitch_amt = lcg01(r) * 12.f; }
    syn.algo = 5; syn.volume = 0.9f;
    for (int p = 0; p < N_PADS; p++) { drums.pad[p].sound = (uint8_t)(lcg(r) % 16); drums.pad[p].length = lcg01(r); drums.pad[p].pitch = lcg01(r) * 2.f - 1.f; drums.pad[p].tone = lcg01(r); drums.pad[p].reverse = (lcg(r) & 1) != 0; drums.pad[p].vol = lcg01(r) * 2.f; }
    for (int i = 0; i < DG_COUNT; i++) drums.group_vol[i] = lcg01(r);
    for (int s = 0; s < SMP_SLOTS; s++) { smp.slot[s].pitch = lcg01(r) - 0.5f; smp.slot[s].formant = lcg01(r) - 0.5f; smp.slot[s].cutoff = lcg01(r); smp.len[s] = lcg(r) % 90000u; }
    smp.width = 0.4f; smp.polyphony = 5.f;
    for (int e = 0; e < ENG_COUNT; e++) { for (int i = 0; i < MAXSTEPS; i++) seq.lane[e].step[i] = (uint16_t)lcg(r); seq.lane[e].bars = 4; seq.lane[e].density = lcg01(r); }
    for (int i = 0; i < MAXSTEPS; i++) { for (int b = 0; b < FX_BANKS; b++) fx.auto_mask[i][b] = (uint16_t)lcg(r); fx.auto_fsr[i] = (uint8_t)lcg(r); }
    fx.dist_algo = 3; fx.auto_enabled = true;
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DENG_PARAMS; k++) drums.eng[g2].x[k] = lcg01(r);

    static Patch a, b, c;
    patchSnapshot(a);
    CHECK(a.magic == PATCH_MAGIC, "magic");
    CHECK(patchCrcOk(a), "crc invalid after snapshot");
    patchApply(a);
    patchSnapshot(b);
    CHECK(memcmp(&a, &b, sizeof a) == 0, "snapshot -> apply -> snapshot differs");

    // file round trip
    const char* path = "/tmp/pmor_patch_roundtrip.pmr";
    FILE* f = fopen(path, "wb"); fwrite(&a, sizeof a, 1, f); fclose(f);
    f = fopen(path, "rb"); size_t got = fread(&c, 1, sizeof c, f); fclose(f);
    CHECK(got == sizeof c && memcmp(&a, &c, sizeof a) == 0, "file round trip differs");
    CHECK(patchCrcOk(c), "crc invalid after file round trip");

    // v8: drum depth knobs persisted
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DENG_PARAMS; k++) {
        CHECK(a.drum_eng[g2][k] == drums.eng[g2].x[k], "drum_eng snapshot %d/%d", g2, k);
        drums.eng[g2].x[k] = 0.f;
    }
    CHECK(patchFromBlob(&a, sizeof a, c) && memcmp(&a, &c, sizeof a) == 0, "patchFromBlob v8 differs");
    patchApply(c);
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DENG_PARAMS; k++)
        CHECK(drums.eng[g2].x[k] == a.drum_eng[g2][k], "drum_eng apply %d/%d", g2, k);
    CHECK(patchFromBlob(&a, sizeof a - 1, c) == false, "short v8 blob accepted");

    // v7 -> v8: build a v7 blob from the v8 one (identical prefix), load it, knobs must be 0, rest identical
    {
        static Patch7 o7; memcpy(&o7, &a, offsetof(Patch7, crc32));
        o7.magic = PATCH_MAGIC_V7; o7.version = 2;
        o7.crc32 = pmorCrc32(&o7, offsetof(Patch7, crc32));
        static Patch L; memset(&L, 0xAA, sizeof L);
        CHECK(sizeof(Patch7) == 7476, "v7 size");
        CHECK(patchFromBlob(&o7, sizeof o7, L), "v7 blob rejected");
        CHECK(L.magic == PATCH_MAGIC, "v7 not upgraded to v8 magic");
        bool z = true; for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DENG_PARAMS; k++) z &= (L.drum_eng[g2][k] == 0.f);
        CHECK(z, "v7 load: drum_eng not 0");
        CHECK(memcmp(&L.seq_lane, &a.seq_lane, sizeof a.seq_lane) == 0 && L.syn_algo == a.syn_algo && memcmp(L.drum_pad, a.drum_pad, sizeof a.drum_pad) == 0, "v7 load: fields differ");
        CHECK(patchCrcOk(L), "v7-upgraded CRC invalid");
        o7.seq_lane[0].step[3] ^= 1u;
        CHECK(!patchFromBlob(&o7, sizeof o7, L), "v7 bit flip accepted");
        // legacy v7 files from a real card (optional): PMOR_V7_DIR=/path/to/files
        const char* dir = getenv("PMOR_V7_DIR");
        int nfile = 0, nbad = 0;
        if (dir) for (int sl = 0; sl < 64; sl++) {
            char nm[512]; snprintf(nm, sizeof nm, "%s/P%02d.PMR", dir, sl);
            FILE* ff = fopen(nm, "rb"); if (!ff) continue;
            static uint8_t buf[8192]; size_t nn = fread(buf, 1, sizeof buf, ff); fclose(ff);
            nfile++;
            if (nn != 7476 || !patchFromBlob(buf, nn, L)) { nbad++; printf("  real v7 file %s: REJECTED (%zu bytes)\n", nm, nn); continue; }
            for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DENG_PARAMS; k++) if (L.drum_eng[g2][k] != 0.f) nbad++;
        }
        if (dir) { printf("real-card v7 patches loaded: %d files, %d bad\n", nfile, nbad); CHECK(nbad == 0 && nfile > 0, "real v7 files"); }
    }
    // ---- patch EXTENSION record (sampler START/END/LOOP/ATTACK/RELEASE + first-hit offsets): additive sidecar, blob untouched
    {
        samplerExtReset(); for (int e = 0; e < ENG_COUNT; e++) seqSetStartOffset((Engine)e, 0);
        CHECK(patchExtLiveCrc() == 0, "ext: defaults -> live crc 0 (autosave scan value unchanged for old patches)");
        Patch before; patchSnapshot(before);
        smp_ext[2].start = 0.25f; smp_ext[2].end = 0.75f; smp_ext[2].loop_f = 1.f; smp_ext[2].attack_ms = 40.f; smp_ext[2].release_ms = 900.f;
        smp_ext[15].end = 0.5f; seqSetStartOffset(ENG_SYNTH, 7);
        synLfo.rate_hz = 7.5f; synLfo.depth = 0.6f; synLfo.shape = 3.f; synLfo.dest = 2.f; drum_deep[DG_SNARE][5] = 1.75f; drum_deep[DG_PERC][0] = 0.5f;
        CHECK(patchExtLiveCrc() != 0, "ext: edit changes the live crc (autosave fires)");
        Patch after; patchSnapshot(after);
        CHECK(memcmp(&before, &after, sizeof before) == 0, "ext: the frozen patch blob does not carry the extras (byte-identical)");
        static PatchExt e1, e2; patchExtSnapshot(e1);
        const char* xp = "/tmp/pmor_patch_ext.ext"; FILE* xf = fopen(xp, "wb"); fwrite(&e1, patchExtBytes(e1), 1, xf); fclose(xf);
        static uint8_t xb[20000]; xf = fopen(xp, "rb"); size_t xn = fread(xb, 1, sizeof xb, xf); fclose(xf);
        CHECK(xn == patchExtBytes(e1) && xn == 16 + PEXT_BODY_V2_BASE && patchExtParse(xb, xn, e2), "ext: file round trip parses, no custom drum pad -> record has no drum synth blocks (%zu bytes)", xn);
        samplerExtReset(); synLfoReset(); drumsDeepReset(); for (int e = 0; e < ENG_COUNT; e++) seqSetStartOffset((Engine)e, 0);
        patchApply(after);                                    // a load of a patch WITHOUT the sidecar: every extra back to its default
        CHECK(smp_ext[2].start == 0.f && smp_ext[2].end == 1.f && smp_ext[2].attack_ms == 2.f && seqStartOffset(ENG_SYNTH) == 0 && patchExtLiveCrc() == 0, "ext: old patch (no record) loads with defaults");
        patchExtApply(e2);
        CHECK(smp_ext[2].start == 0.25f && smp_ext[2].end == 0.75f && smp_ext[2].loop_f == 1.f && smp_ext[2].attack_ms == 40.f && smp_ext[2].release_ms == 900.f && smp_ext[15].end == 0.5f && smp_ext[0].end == 1.f, "ext: sampler extras restored");
        CHECK(seqStartOffset(ENG_SYNTH) == 7 && seqStartOffset(ENG_DRUMS) == 0, "ext: first-hit start offset restored");
        CHECK(synLfo.rate_hz == 7.5f && synLfo.depth == 0.6f && synLfo.shape == 3.f && synLfo.dest == 2.f && drum_deep[DG_SNARE][5] == 1.75f && drum_deep[DG_PERC][0] == 0.5f && drum_deep[DG_KICK][0] == 1.f, "ext: synth LFO + drum deep knobs restored");
        {   // an OLDER (shorter, first-version) record is accepted: the fields it lacks are defaults
            static uint8_t old[16 + PEXT_BODY_V1]; memcpy(old, &e1, sizeof old); uint32_t bl = PEXT_BODY_V1; memcpy(old + 8, &bl, 4); uint32_t oc = pmorCrc32(old + 16, bl); memcpy(old + 12, &oc, 4);
            PatchExt e4; samplerExtReset(); synLfoReset(); drumsDeepReset();
            CHECK(patchExtParse(old, sizeof old, e4), "ext: shorter first-version record parses"); patchExtApply(e4);
            CHECK(smp_ext[2].attack_ms == 40.f && synLfo.depth == 0.f && synLfo.rate_hz == 3.f && drum_deep[DG_SNARE][5] == 1.f, "ext: shorter record: sampler fields restored, LFO/deep at defaults");
            patchExtApply(e2);
        }
        // corruption / garbage / truncation: rejected (the loader then keeps the defaults), never partially applied
        xb[40] ^= 0x10; CHECK(!patchExtParse(xb, xn, e2), "ext: bit flip rejected"); xb[40] ^= 0x10;
        CHECK(!patchExtParse(xb, xn - 1, e2), "ext: truncated record rejected");
        CHECK(!patchExtParse(xb, 8, e2) && !patchExtParse(nullptr, 0, e2), "ext: short/empty rejected");
        uint8_t junk[400]; for (int i = 0; i < 400; i++) junk[i] = (uint8_t)(i * 37u); CHECK(!patchExtParse(junk, sizeof junk, e2), "ext: garbage rejected");
        // a LONGER record from a newer firmware (fields appended) is accepted: this firmware reads the prefix it knows
        static uint8_t big[sizeof(PatchExt) + 64]; memset(big, 0xAB, sizeof big); memcpy(big, &e1, sizeof e1);
        uint32_t nl = (uint32_t)(sizeof big - 16); memcpy(big + 8, &nl, 4); uint32_t cr = pmorCrc32(big + 16, nl); memcpy(big + 12, &cr, 4);
        PatchExt e3; memset(&e3, 0, sizeof e3); CHECK(patchExtParse(big, sizeof big, e3) && e3.smp[2].attack_ms == 40.f, "ext: longer (newer) record accepted, known prefix used");
        // out-of-range / NaN values never reach the engine
        PatchExt bad = e1; bad.smp[4].start = 5.f; bad.smp[4].attack_ms = NAN; bad.smp[4].release_ms = -3.f; patchExtApply(bad);
        CHECK(smp_ext[4].start == 0.f && smp_ext[4].attack_ms == 2.f && smp_ext[4].release_ms == 2.f, "ext: out-of-range / NaN fall back to defaults");
        samplerExtReset(); synLfoReset(); drumsDeepReset(); for (int e = 0; e < ENG_COUNT; e++) seqSetStartOffset((Engine)e, 0);
    }
    // ---- drum synth engine params in the EXT record: only customised pads are stored; an old patch (no blocks) loads every pad's preset params
    {
        drumsInit(); for (int pp = 0; pp < N_PADS; pp++) drums.pad[pp].sound = (uint8_t)((pp * 5) % DRUM_SOUNDS); Patch pa; patchSnapshot(pa); patchApply(pa);   // pads loaded for their sounds
        CHECK(patchExtLiveCrc() == 0, "dsv: untouched pads -> ext live crc 0 (old patches keep their autosave scan value)");
        uint16_t kRT0 = 14, kNLVL = 0; for (size_t k = 0; k < SDEF_NPARAMS; k++) { if (!strcmp(SDEF_PARAMS[k].name, "RT0")) kRT0 = (uint16_t)k; if (!strcmp(SDEF_PARAMS[k].name, "NLVL")) kNLVL = (uint16_t)k; }
        float pre3 = *drumsSynthParamPtr(3, kRT0), pre9 = *drumsSynthParamPtr(9, kNLVL);
        *drumsSynthParamPtr(3, kRT0) = 1.234f; *drumsSynthParamPtr(9, kNLVL) = 0.777f;
        CHECK(drumsPadCustom(3) && drumsPadCustom(9) && !drumsPadCustom(4), "dsv: edited pads are custom, others not");
        CHECK(patchExtLiveCrc() != 0, "dsv: custom pad -> ext live crc non-zero (autosave fires)");
        static PatchExt d1, d2; patchExtSnapshot(d1);
        CHECK(d1.dsv_n == 2 && d1.dsv_mask == ((1u << 3) | (1u << 9)) && patchExtBytes(d1) == 16 + PEXT_BODY_V2_BASE + 2 * DSV_FLOATS * 4, "dsv: record holds exactly the 2 custom pads (%u blocks, %zu bytes)", (unsigned)d1.dsv_n, patchExtBytes(d1));
        static uint8_t fb[20000]; size_t fn = patchExtBytes(d1); memcpy(fb, &d1, fn);
        CHECK(patchExtParse(fb, fn, d2), "dsv: record parses");
        patchApply(pa);                                                       // a patch load resets every pad to its preset
        CHECK(*drumsSynthParamPtr(3, kRT0) == pre3 && *drumsSynthParamPtr(9, kNLVL) == pre9 && !drumsPadCustom(3), "dsv: patch load without the record -> preset params (old patch)");
        patchExtApply(d2);
        CHECK(*drumsSynthParamPtr(3, kRT0) == 1.234f && *drumsSynthParamPtr(9, kNLVL) == 0.777f && drumsPadCustom(3) && drumsPadCustom(9) && !drumsPadCustom(5), "dsv: custom params restored on exactly their pads");
        { PatchExt dv1; patchExtSnapshot(dv1); static uint8_t ob[16 + PEXT_BODY_V1]; memcpy(ob, &dv1, sizeof ob); uint32_t bl = PEXT_BODY_V1; memcpy(ob + 8, &bl, 4); uint32_t oc = pmorCrc32(ob + 16, bl); memcpy(ob + 12, &oc, 4);
          PatchExt dv2; CHECK(patchExtParse(ob, sizeof ob, dv2), "dsv: v1 record parses"); patchExtApply(dv2); CHECK(!drumsPadCustom(3) && *drumsSynthParamPtr(3, kRT0) == pre3, "dsv: v1 (pre-drum-synth) record -> preset params"); patchExtApply(d2); }
        { PatchExt bd = d1; bd.dsv[0][kRT0] = NAN; bd.dsv[1][kNLVL] = 9999.f; patchExtApply(bd);
          CHECK(*drumsSynthParamPtr(3, kRT0) == pre3 && *drumsSynthParamPtr(9, kNLVL) == pre9, "dsv: NaN / out-of-range values fall back to the preset's value"); }
        *drumsSynthParamPtr(3, kRT0) = 1.234f; drums.pad[3].sound = (uint8_t)((drums.pad[3].sound + 1) % DRUM_SOUNDS); drumsProcess(g_dummyL, g_dummyR, 64);
        CHECK(!drumsPadCustom(3), "dsv: changing the pad's sound loads the new preset's params (custom edits dropped)");
        patchExtApply(d1); patchApply(pa); drumsInit();
    }

    patchSnapshot(c); memcpy(&c, &a, sizeof a);

    // corruption is detected
    c.seq_lane[1].step[7] ^= 1u;
    CHECK(!patchCrcOk(c), "bit flip not detected");

    printf(fails ? "test_patch_roundtrip: %d FAILURES\n" : "test_patch_roundtrip: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}
