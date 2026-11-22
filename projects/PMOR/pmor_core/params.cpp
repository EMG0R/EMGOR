// params.cpp -- see params.h.
#include "core_config.h"
#include "params.h"
#include "param.h"
#include "synth.h"
#include "drums.h"
#include "drum_super.h"
#include "sampler.h"
#include "sequencer.h"
#include "fx.h"
#include "fx_v2.h"
#include "clock.h"
#include "output.h"
#include "metro.h"
#include "scale.h"
#include <string.h>

#define SCHEMA_MAX   3072     // 612 engine params + 16 pads x 136 drum synth params = 2788, +headroom
#define GROUP_STRINGS 512

namespace {
struct Entry { PmorParamDesc d; float* f; uint8_t* u8; };
Entry  SDRAM_BSS s_tab[SCHEMA_MAX];      // ~30 KB: SDRAM, not the 128 KB DTCM (rebuilt by pmorSchemaInit at boot)
size_t s_n = 0;
char   SDRAM_BSS s_gs[GROUP_STRINGS][22];     // group labels built at init (no snprintf: keeps the firmware lean)
size_t s_ng = 0;

const char* label(const char* base, int num) {      // "DRUM.PAD" + 3 -> "DRUM.PAD3"
    if (s_ng >= GROUP_STRINGS) return base;
    char* o = s_gs[s_ng++];
    size_t i = 0;
    while (base[i] && i < 10) { o[i] = base[i]; i++; }
    if (num >= 100) o[i++] = (char)('0' + num / 100);
    if (num >= 10)  o[i++] = (char)('0' + (num / 10) % 10);
    o[i++] = (char)('0' + num % 10);
    o[i] = 0;
    return o;
}
const char* label2(int pad, const char* eng, const char* layer) {     // "KICK.P3.RES"
    if (s_ng >= GROUP_STRINGS) return layer;
    char* o = s_gs[s_ng++]; size_t i = 0;
    for (size_t j = 0; eng[j]; j++) o[i++] = eng[j];
    o[i++] = '.'; o[i++] = 'P'; if (pad >= 10) o[i++] = (char)('0' + pad / 10); o[i++] = (char)('0' + pad % 10); o[i++] = '.';
    for (size_t j = 0; layer[j] && i < 21; j++) o[i++] = layer[j];
    o[i] = 0; return o;
}
void add(PmorDomain dom, uint16_t idx, const char* name, float mn, float mx, float def, uint8_t flags,
         const char* group, float* f, uint8_t* u8) {
    if (s_n >= SCHEMA_MAX) return;
    Entry& e = s_tab[s_n++];
    e.d.id = pmorParamId(dom, idx); e.d.name = name; e.d.min = mn; e.d.max = mx; e.d.def = def;
    e.d.flags = flags; e.d.group = group; e.f = f; e.u8 = u8;
}
// page walk: every Param of a ParamPage becomes one entry (default = its value right now)
void addPage(PmorDomain dom, uint16_t base, const ParamPage* pg, const char* group) {
    if (!pg) return;
    for (uint8_t i = 0; i < pg->count; i++) {
        const Param& p = pg->items[i];
        add(dom, (uint16_t)(base + i), p.name, p.min, p.max, *p.value, p.flags, group, p.value, nullptr);
    }
}
}  // namespace

void pmorSchemaInit(void) {
    s_n = 0; s_ng = 0;

    // ---- SYNTH: index = algo*32 + k (algo knobs 0-3, env 4-7, filter 8-14, pitch 15-17, width 18-19); 256 = ALGO, 257 = VOLUME
    const uint8_t keep_algo = syn.algo;
    for (uint8_t a = 0; a < ALG_COUNT; a++) {
        syn.algo = a;                                  // the env/filter/pitch/width accessors follow syn.algo
        const char* grp = label("SYN.A", a);
        addPage(PD_SYNTH, (uint16_t)(a * 32 + 0),  synAlgoParamPage(a), grp);
        addPage(PD_SYNTH, (uint16_t)(a * 32 + 4),  synEnvPage(),    grp);
        addPage(PD_SYNTH, (uint16_t)(a * 32 + 8),  synFilterPage(), grp);
        addPage(PD_SYNTH, (uint16_t)(a * 32 + 15), synPitchPage(),  grp);
        addPage(PD_SYNTH, (uint16_t)(a * 32 + 18), synWidthPage(),  grp);
    }
    syn.algo = keep_algo;
    {
        add(PD_SYNTH, 256, "ALGO",   0.f, (float)(ALG_COUNT - 1), (float)syn.algo, PF_QUANT | PF_ENUM, "SYN", nullptr, &syn.algo);
        add(PD_SYNTH, 257, "VOLUME", 0.f, ENGINE_VOL_MAX, syn.volume, PF_PERCENT, "SYN", &syn.volume, nullptr);
        addPage(PD_SYNTH, 258, synLfoPage(), "SYN.LFO");   // 258 LFORATE 259 LFODEPTH 260 LFOSHAPE 261 LFODEST (engine-wide LFO; depth 0 = off)
    }

    // ---- DRUMS: index = pad*8 + k (length, pitch, tone, rev, vol = 0-4; sound = 5); 256+g group vol; 260 volume
    for (uint8_t p = 0; p < N_PADS; p++) {
        const char* grp = label("DRUM.PAD", p);
        addPage(PD_DRUMS, (uint16_t)(p * 8), drumsPadPage(p), grp);
        add(PD_DRUMS, (uint16_t)(p * 8 + 5), "SOUND", 0.f, (float)(DRUM_SOUNDS - 1), (float)drums.pad[p].sound,
            PF_QUANT | PF_ENUM, grp, nullptr, &drums.pad[p].sound);
    }
    // drum synth engine: every param of every pad's voice, grouped by layer (P<pad>.<LAYER>)
    for (uint8_t p = 0; p < N_PADS; p++) {
        const char* lg[SL_COUNT]; for (int l = 0; l < SL_COUNT; l++) lg[l] = label2(p, SDEF_ENGINE_NAMES[drumPadGroup(p)], SDEF_LAYER_NAMES[l]);
        for (size_t k = 0; k < SDEF_NPARAMS; k++) {
            if (!sdefUsed(drumPadGroup(p), k)) continue;   // engine = the group's parameter set
            const SdParamInfo& q = SDEF_PARAMS[k]; float* f = drumsSynthParamPtr(p, (uint16_t)k);
            add((PmorDomain)(PD_DRUMSYN0 + p / 4), (uint16_t)((p % 4) * 256 + k), sdefName(drumPadGroup(p), k), q.mn, q.mx, *f, q.flags, lg[q.layer], f, nullptr);
        }
    }
    addPage(PD_DRUMS, 256, drumsMixPage(), "DRUM");
    for (uint8_t gq = 0; gq < DG_COUNT; gq++) addPage(PD_DRUMS, (uint16_t)(272 + gq * 8), drumsEnginePage(gq), label("DRUM.ENG", gq));   // engine depth knobs
    for (uint8_t gq = 0; gq < DG_COUNT; gq++) addPage(PD_DRUMS, (uint16_t)(320 + gq * 16), drumsDeepPage(gq), label("DRUM.DEEP", gq));   // recipe multipliers: PITCH DECAY LAYER2 NOISEF NOISEQ NOISEDEC SWEEP FM (1 = neutral)
    add(PD_DRUMS, 260, "VOLUME", 0.f, ENGINE_VOL_MAX, drums.volume, PF_PERCENT, "DRUM", &drums.volume, nullptr);

    // ---- SAMPLER: index = slot*8 + k; 256.. mix page
    for (uint8_t s = 0; s < SMP_SLOTS; s++) addPage(PD_SAMPLER, (uint16_t)(s * 8), samplerSlotPage(s), label("SMP.SLOT", s));
    addPage(PD_SAMPLER, 256, samplerMixPage(), "SMP");
    for (uint8_t s = 0; s < SMP_SLOTS; s++) addPage(PD_SAMPLER, (uint16_t)(512 + s * 8), samplerExtPage(s), label("SMP.SLOT", s));   // START END LOOP ATTACK RELEASE (session-live)

    // ---- SEQUENCER: index = engine*32 + k
    {
        static const char* const sg[ENG_COUNT] = { "SEQ.DRUM", "SEQ.SYN", "SEQ.SMP" };
        for (int e = 0; e < ENG_COUNT; e++) addPage(PD_SEQ, (uint16_t)(e * 32), seqParamPage((Engine)e), sg[e]);
    }

    // ---- FX: index = effect id (bank*16 + pad) = engage amount 0..1 (held/released; depth rides the FSR);
    //          128 = dist algo, 129 = automation enabled
    for (int id = 0; id < FX_TOTAL; id++)
        add(PD_FX, (uint16_t)id, FX_V2_NAMES[id >> 4][id & 15], 0.f, 1.f, 0.f, PF_QUANT, label("FX", id >> 4), nullptr, nullptr);
    // 130+k AMOUNT: floor under the FSR macro bank k's effects read (0 = follow the live FSR); 140+k LIDAR: floor under the lidar macro;
    // 150+k MIX: bank wet/dry (k = 1..7; bank 1 = k 0 has no MIX: its chain is spread over the master chain). Linear 0..1, shown as %.
    for (int k = 0; k < FX_BANKS; k++) add(PD_FX, (uint16_t)(130 + k), "AMOUNT", 0.f, 1.f, 0.f, PF_PERCENT, label("FX", k), &fx.bank_amt[k], nullptr);
    for (int k = 0; k < FX_BANKS; k++) add(PD_FX, (uint16_t)(140 + k), "LIDAR", 0.f, 1.f, 0.f, PF_PERCENT, label("FX", k), &fx.bank_lid[k], nullptr);
    for (int k = 1; k < FX_BANKS; k++) add(PD_FX, (uint16_t)(150 + k), "MIX", 0.f, 1.f, 1.f, PF_PERCENT, label("FX", k), &fx.bank_mix[k], nullptr);
    add(PD_FX, 128, "DISTALG", 0.f, (float)(DIST_COUNT - 1), (float)fx.dist_algo, PF_QUANT | PF_ENUM, "FX", nullptr, &fx.dist_algo);

    // ---- GLOBAL: master clock, output, scale, metronome
#ifdef PMOR_DRUMBENCH
    add(PD_GLOBAL, 900, "DBENCH", 0.f, 9999999.f, 0.f, PF_NONE, "BENCH", &g_drumBenchF, nullptr);   // measurement builds only (make DRUMBENCH=1): see drumsBenchBlock
#endif
    add(PD_GLOBAL, 0, "BPM",     30.f, 300.f, clk.bpm,        PF_NONE,    "CLOCK",  &clk.bpm, nullptr);
    add(PD_GLOBAL, 1, "SWING",   0.f,  1.f,   clk.swing,      PF_PERCENT, "CLOCK",  &clk.swing, nullptr);
    add(PD_GLOBAL, 2, "FADER",   0.f,  1.3333f, outp.fader,   PF_PERCENT, "OUTPUT", &outp.fader, nullptr);
    add(PD_GLOBAL, 3, "INGAIN",  0.f,  2.f,   outp.input_gain, PF_NONE,   "OUTPUT", &outp.input_gain, nullptr);
    add(PD_GLOBAL, 4, "KEY",    -12.f, 12.f,  scale.key_semis, PF_BIPOLAR, "SCALE", &scale.key_semis, nullptr);
    add(PD_GLOBAL, 5, "SCALE",   0.f, (float)(SCALE_MODES - 1), (float)scale.mode, PF_QUANT | PF_ENUM, "SCALE", nullptr, &scale.mode);
    add(PD_GLOBAL, 6, "METRO",   0.f,  1.f,   metro.level,    PF_PERCENT, "METRO",  &metro.level, nullptr);
    add(PD_GLOBAL, 7, "CLKLEAD", -20.f, 40.f, 0.f, PF_QUANT | PF_BIPOLAR, "CLOCK", &clk.follow_lead_ms, nullptr);   // ms; follower only; persisted in QSPI globals
}

size_t pmorSchemaCount(void) { return s_n; }
const PmorParamDesc* pmorSchemaAt(size_t i) { return i < s_n ? &s_tab[i].d : nullptr; }
const PmorParamDesc* pmorSchemaFind(uint16_t id) {
    for (size_t i = 0; i < s_n; i++) if (s_tab[i].d.id == id) return &s_tab[i].d;
    return nullptr;
}
static Entry* findEntry(uint16_t id) {
    for (size_t i = 0; i < s_n; i++) if (s_tab[i].d.id == id) return &s_tab[i];
    return nullptr;
}
bool pmorParamGet(uint16_t id, float* out) {
    Entry* e = findEntry(id);
    if (!e) return false;
    if (e->f) *out = *e->f;
    else if (e->u8) *out = (float)*e->u8;
    else *out = ((fx.held[(id & 0x3FF) >> 4] >> ((id & 0x3FF) & 15)) & 1u) ? 1.f : 0.f;   // FX engage = held bit
    return true;
}
bool pmorParamSet(uint16_t id, float v) {
    Entry* e = findEntry(id);
    if (!e) return false;
    if (v < e->d.min) v = e->d.min;
    if (v > e->d.max) v = e->d.max;
    if (e->d.flags & PF_QUANT) v = (float)(int)(v + (v >= 0.f ? 0.5f : -0.5f));
    if (id == pmorParamId(PD_GLOBAL, 0)) { /* BPM: only the grid setter, clk.bpm is derived */ }
    else if (e->f) *e->f = v;
    else if (e->u8) *e->u8 = (uint8_t)v;
    else if ((id >> 10) == PD_FX) { if (v >= 0.5f) fxPress((uint8_t)(id & 0x3FF)); else fxRelease((uint8_t)(id & 0x3FF)); }
    pmorParamAfterSet(id, v);
    return true;
}

int pmorSchemaIndexOf(uint16_t id) {
    for (size_t i = 0; i < s_n; i++) if (s_tab[i].d.id == id) return (int)i;
    return -1;
}
bool pmorParamGetAt(size_t i, float* out) {
    if (i >= s_n) return false;
    const Entry& e = s_tab[i];
    if (e.f) *out = *e.f;
    else if (e.u8) *out = (float)*e.u8;
    else { unsigned k = e.d.id & 0x3FF; *out = ((fx.held[k >> 4] >> (k & 15)) & 1u) ? 1.f : 0.f; }
    return true;
}
void pmorParamAfterSet(uint16_t id, float v) {
    if (id == pmorParamId(PD_GLOBAL, 0)) clockSetBpm(v);                                            // tempo goes through the grid setter
    else if (id == pmorParamId(PD_GLOBAL, 4) || id == pmorParamId(PD_GLOBAL, 5)) scaleRebuild();    // key / scale -> note table
    else if ((id >> 10) == PD_DRUMS && (id & 0x3FF) < 128 && (id & 7) == 5) drumsPadReload((uint8_t)((id & 0x3FF) >> 3));   // SOUND: selecting a preset (even the same one) loads its params into the pad
}
