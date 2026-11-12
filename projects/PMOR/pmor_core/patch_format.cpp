// patch_format.cpp -- see patch_format.h. Bodies moved verbatim from store/patch.cpp.
#include "patch_format.h"
#include "output.h"
#include "drum_super.h"
#include <string.h>

// Table-driven (same reflected CRC-32/0xEDB88320 as the bitwise loop it replaces, bit-identical output). The live-patch change scan
// (patchTick, every 250 ms) CRCs the whole snapshot; the bit loop was ~1 ms of main-loop time per scan, the table is ~5x faster.
static const uint32_t kCrcTab[256] = {
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu,
    0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u,
    0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u,
    0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u,
    0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
    0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u,
    0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu, 0x35B5A8FAu, 0x42B2986Cu,
    0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
    0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u,
    0xCFBA9599u, 0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
    0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u, 0x01DB7106u,
    0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u,
    0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du,
    0x91646C97u, 0xE6635C01u, 0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu,
    0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
    0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
    0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u,
    0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u,
    0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu,
    0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
    0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u,
    0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au,
    0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u, 0xE3630B12u, 0x94643B84u,
    0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
    0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
    0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu,
    0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u, 0xD6D6A3E8u, 0xA1D1937Eu,
    0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu,
    0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u,
    0x316E8EEFu, 0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
    0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu, 0xB2BD0B28u,
    0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
    0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu,
    0x72076785u, 0x05005713u, 0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u,
    0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
    0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u,
    0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u,
    0x616BFFD3u, 0x166CCF45u, 0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u,
    0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu,
    0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
    0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u,
    0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u,
    0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du
};
uint32_t pmorCrc32(const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) crc = (crc >> 8) ^ kCrcTab[(crc ^ p[i]) & 0xFFu];
    return ~crc;
}


void patchSnapshot(Patch& p) {
    memset(&p, 0, sizeof(p));
    p.magic   = PATCH_MAGIC;
    p.version = PATCH_VERSION;   // v2: SynPreset gained pitch_amt

    p.syn_algo = syn.algo;
    memcpy(p.syn_preset, syn.preset, sizeof(p.syn_preset));
    p.syn_volume = syn.volume;

    memcpy(p.drum_pad, drums.pad, sizeof(p.drum_pad));
    memcpy(p.drum_group_vol, drums.group_vol, sizeof(p.drum_group_vol));
    p.drum_volume = drums.volume;

    memcpy(p.smp_slot, smp.slot, sizeof(p.smp_slot));
    memcpy(p.smp_len, smp.len, sizeof(p.smp_len));
    p.smp_volume      = smp.volume;
    p.smp_width       = smp.width;
    p.smp_width_prob  = smp.width_prob;
    p.smp_polyphony   = smp.polyphony;

    memcpy(p.seq_lane, seq.lane, sizeof(p.seq_lane));

    memcpy(p.fx_auto_mask, (const void*)fx.auto_mask, sizeof(p.fx_auto_mask));
    memcpy(p.fx_auto_fsr, (const void*)fx.auto_fsr, sizeof(p.fx_auto_fsr));
    p.fx_dist_algo    = fx.dist_algo;
    p.fx_auto_enabled = fx.auto_enabled ? 1 : 0;

    memcpy(p.drum_eng, drums.eng, sizeof(p.drum_eng));

    p.outp_fader = 0.f;   // master fader is GLOBAL (glob.master_fader), never per patch

    // CRC over everything BEFORE crc32 — offsetof, not sizeof-4: the struct has 4 bytes of
    // trailing padding after crc32 (8-byte alignment from fx_auto_mask), so sizeof-4 covered the
    // crc field itself -> every load since 6e failed its CRC check. THE "never saves" bug.
    p.crc32 = pmorCrc32(&p, offsetof(Patch, crc32));
}

// accepts the current CRC and the pre-6w one (computed with the crc field zeroed over sizeof-4)
bool patchCrcOk(Patch& p) {
    if (pmorCrc32(&p, offsetof(Patch, crc32)) == p.crc32) return true;
    uint32_t keep = p.crc32; p.crc32 = 0;
    uint32_t old = pmorCrc32(&p, sizeof(p) - sizeof(p.crc32));
    p.crc32 = keep;
    return old == keep;
}

static_assert(DSV_FLOATS == SDEF_FLOATS, "PatchExt drum synth block is one SuperDef");
static void extDefaults(PatchExt& e) {
    memset(&e, 0, sizeof e);
    for (int i = 0; i < SMP_SLOTS; i++) e.smp[i] = SmpSlotExt{ 0.f, 1.f, 0.f, SMP_ATK_DEF_MS, SMP_REL_DEF_MS };
    e.lfo[0] = 3.f;                                    // = synLfoReset()
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DDEEP_PARAMS; k++) e.deep[g2][k] = 1.f;
}
static bool extIsDefault() {
    for (int i = 0; i < SMP_SLOTS; i++) { const SmpSlotExt& x = smp_ext[i]; if (x.start != 0.f || x.end != 1.f || x.loop_f != 0.f || x.attack_ms != SMP_ATK_DEF_MS || x.release_ms != SMP_REL_DEF_MS) return false; }
    for (int e = 0; e < ENG_COUNT; e++) if (seqStartOffset((Engine)e) != 0) return false;
    if (synLfo.rate_hz != 3.f || synLfo.depth != 0.f || synLfo.shape != 0.f || synLfo.dest != 0.f) return false;
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DDEEP_PARAMS; k++) if (drum_deep[g2][k] != 1.f) return false;
    for (uint8_t p = 0; p < N_PADS; p++) if (drumsPadCustom(p)) return false;
    return true;
}
void patchExtSnapshot(PatchExt& e) {
    memset(&e, 0, sizeof e);
    e.magic = PEXT_MAGIC; e.version = PEXT_VERSION;
    memcpy(e.smp, smp_ext, sizeof e.smp);
    for (int i = 0; i < ENG_COUNT; i++) e.seq_off[i] = seqStartOffset((Engine)i);
    e.lfo[0] = synLfo.rate_hz; e.lfo[1] = synLfo.depth; e.lfo[2] = synLfo.shape; e.lfo[3] = synLfo.dest;
    memcpy(e.deep, drum_deep, sizeof e.deep);
    uint16_t n = 0; e.dsv_mask = 0;
    for (uint8_t p = 0; p < N_PADS; p++) if (drumsPadCustom(p)) { e.dsv_mask |= (uint16_t)(1u << p); memcpy(e.dsv[n++], drumsPadSynthGet(p), sizeof e.dsv[0]); }
    e.dsv_n = n;
    e.body_len = (uint32_t)(PEXT_BODY_V2_BASE + (size_t)n * DSV_FLOATS * 4);
    e.crc = pmorCrc32((const uint8_t*)&e + 16, e.body_len);
}
size_t patchExtBytes(const PatchExt& e) { return 16 + e.body_len; }
uint32_t patchExtLiveCrc(void) {
    if (extIsDefault()) return 0;
    uint32_t c = pmorCrc32(smp_ext, sizeof smp_ext);
    uint16_t o[ENG_COUNT]; for (int i = 0; i < ENG_COUNT; i++) o[i] = seqStartOffset((Engine)i);
    c ^= pmorCrc32(o, sizeof o);
    c ^= pmorCrc32(&synLfo, sizeof synLfo) * 3u;
    c ^= pmorCrc32(drum_deep, sizeof drum_deep) * 5u;
    for (uint8_t p = 0; p < N_PADS; p++) if (drumsPadCustom(p)) c ^= pmorCrc32(drumsPadSynthGet(p), SDEF_FLOATS * 4) * (7u + 2u * p);
    return c | 1u;   // never 0 for a non-default state
}
bool patchExtParse(const void* data, size_t n, PatchExt& out) {
    if (!data || n < 16) return false;
    uint32_t h[4]; memcpy(h, data, 16);
    if (h[0] != PEXT_MAGIC || h[1] < PEXT_VERSION || h[2] < PEXT_BODY_V1 || h[2] > n - 16) return false;   // the first version's body is the minimum; a longer (newer) body is fine
    if (pmorCrc32((const uint8_t*)data + 16, h[2]) != h[3]) return false;
    PatchExt t; extDefaults(t);                                           // fields an older (shorter) record lacks stay at their defaults
    size_t take = h[2] < sizeof t - 16 ? h[2] : sizeof t - 16;            // the prefix this firmware knows; appended fields of a newer version are ignored
    memcpy((uint8_t*)&t + 16, (const uint8_t*)data + 16, take);
    t.magic = h[0]; t.version = h[1]; t.body_len = h[2]; t.crc = h[3];
    out = t; return true;
}
static float clampTo(float v, float lo, float hi, float dflt) { if (!(v >= lo && v <= hi)) return dflt; return v; }   // NaN / out of range -> default
void patchExtApply(const PatchExt& e) {
    for (int i = 0; i < SMP_SLOTS; i++) {
        const SmpSlotExt& x = e.smp[i];
        smp_ext[i].start      = clampTo(x.start, 0.f, 1.f, 0.f);
        smp_ext[i].end        = clampTo(x.end, 0.f, 1.f, 1.f);
        smp_ext[i].loop_f     = x.loop_f >= 0.5f ? 1.f : 0.f;
        smp_ext[i].attack_ms  = clampTo(x.attack_ms, 1.f, 2000.f, SMP_ATK_DEF_MS);
        smp_ext[i].release_ms = clampTo(x.release_ms, 1.f, 4000.f, SMP_REL_DEF_MS);
    }
    for (int i = 0; i < ENG_COUNT; i++) seqSetStartOffset((Engine)i, e.seq_off[i]);
    synLfo.rate_hz = clampTo(e.lfo[0], 0.05f, 20.f, 3.f);
    synLfo.depth   = clampTo(e.lfo[1], 0.f, 1.f, 0.f);
    synLfo.shape   = clampTo(e.lfo[2], 0.f, (float)(LFO_SHAPES - 1), 0.f);
    synLfo.dest    = clampTo(e.lfo[3], 0.f, (float)(LFOD_DESTS - 1), 0.f);
    for (int g2 = 0; g2 < DG_COUNT; g2++) for (int k = 0; k < DDEEP_PARAMS; k++) drum_deep[g2][k] = clampTo(e.deep[g2][k], 0.25f, 4.f, 1.f);
    for (uint8_t p = 0; p < N_PADS; p++) drumsPadReload(p);                    // pads with no block (every pre-v2 record): the preset's params
    { uint16_t i = 0; for (uint8_t p = 0; p < N_PADS && i < e.dsv_n && i < N_PADS; p++) if (e.dsv_mask & (1u << p)) drumsPadSynthSet(p, e.dsv[i++]); }
}

static uint32_t s_apply_count = 0;
uint32_t patchApplyCount(void) { return s_apply_count; }
void patchApply(const Patch& p) {
    s_apply_count++;
    for (int e = 0; e < ENG_COUNT; e++) seqSetStartOffset((Engine)e, 0);   // first-hit start offsets are not in the blob: 0 on every load
    synLfoReset(); drumsDeepReset();   // LFO / deep recipe knobs: defaults on every load (the extension record carries them)
    synLfoReset(); drumsDeepReset();   // synth LFO / drum deep recipe knobs: not in the blob, defaults on every load (the extension record restores them)
    for (uint8_t pp = 0; pp < N_PADS; pp++) drumsPadReload(pp);   // drum synth params: the preset's, until the extension record says a pad is customised
    samplerExtReset();   // sampler extras (START/END/LOOP/ATTACK/RELEASE) likewise
    syn.algo = p.syn_algo;
    memcpy(syn.preset, p.syn_preset, sizeof(syn.preset));
    syn.volume = p.syn_volume;

    memcpy(drums.pad, p.drum_pad, sizeof(drums.pad));
    memcpy(drums.group_vol, p.drum_group_vol, sizeof(drums.group_vol));
    drums.volume = p.drum_volume;

    memcpy(smp.slot, p.smp_slot, sizeof(smp.slot));
    memcpy(smp.len, p.smp_len, sizeof(smp.len));
    smp.volume     = p.smp_volume;
    smp.width      = p.smp_width;
    smp.width_prob = p.smp_width_prob;
    smp.polyphony  = p.smp_polyphony;

    memcpy(seq.lane, p.seq_lane, sizeof(seq.lane));

    memcpy((void*)fx.auto_mask, p.fx_auto_mask, sizeof(p.fx_auto_mask));
    memcpy((void*)fx.auto_fsr, p.fx_auto_fsr, sizeof(p.fx_auto_fsr));
    fx.dist_algo    = p.fx_dist_algo;
    fx.auto_enabled = p.fx_auto_enabled != 0;

    memcpy(drums.eng, p.drum_eng, sizeof(drums.eng));

    // outp.fader deliberately NOT applied: master volume transcends patches
}

bool patchFromBlob(const void* data, size_t n, Patch& out) {
    if (n == sizeof(Patch)) {
        static Patch SDRAM_BSS t; memcpy(&t, data, sizeof t);   // SDRAM: 7.5 KB each in DTCM left 3 KB for stack+heap and bricked f7a6fe1
        if (t.magic != PATCH_MAGIC || !patchCrcOk(t)) return false;
        out = t; return true;
    }
    if (n == sizeof(Patch7)) {
        static Patch7 SDRAM_BSS o; memcpy(&o, data, sizeof o);
        if (o.magic != PATCH_MAGIC_V7) return false;
        bool ok = pmorCrc32(&o, offsetof(Patch7, crc32)) == o.crc32;
        if (!ok) { uint32_t keep = o.crc32; o.crc32 = 0; ok = pmorCrc32(&o, sizeof(o) - sizeof(o.crc32)) == keep; o.crc32 = keep; }
        if (!ok) return false;
        memset(&out, 0, sizeof out);
        memcpy(&out, &o, offsetof(Patch7, crc32));
        out.magic = PATCH_MAGIC; out.version = PATCH_VERSION;
        out.crc32 = pmorCrc32(&out, offsetof(Patch, crc32));
        return true;
    }
    return false;
}
