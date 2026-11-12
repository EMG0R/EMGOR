// patch_format.h -- the PMOR patch blob (PATCHnn.PMR / QSPI mirror): layout, CRC, snapshot of the live
// engine state into it, and apply back. Hardware-free; store/patch.cpp does the SD/QSPI I/O around it.
// The byte layout is frozen (existing cards must keep loading): changing a field means bumping
// PATCH_MAGIC/version, never silently reshaping.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "core_config.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "sequencer.h"
#include "fx.h"

struct Patch {
    uint32_t magic, version;
    // syn
    uint8_t    syn_algo;
    SynPreset  syn_preset[ALG_COUNT];
    float      syn_volume;
    // drums
    DrumPadCfg drum_pad[N_PADS];
    float      drum_group_vol[DG_COUNT];
    float      drum_volume;
    // smp
    SmpSlotCfg smp_slot[SMP_SLOTS];
    uint32_t   smp_len[SMP_SLOTS];
    float      smp_volume, smp_width, smp_width_prob, smp_polyphony;
    // seq (all three lane sets, verbatim)
    LaneSet    seq_lane[ENG_COUNT];
    // fx
    uint16_t   fx_auto_mask[MAXSTEPS][FX_BANKS];
    uint8_t    fx_auto_fsr[MAXSTEPS];
    uint8_t    fx_dist_algo;
    uint8_t    fx_auto_enabled;
    // output
    float      outp_fader;
    // drum engine depth knobs (v8: persisted; v7 patches load with all 0 = neutral)
    float      drum_eng[DG_COUNT][DENG_PARAMS];
    uint32_t   crc32;          // over everything above
};

// The v7 blob exactly as it sits on existing cards (magic 'OPG7', 7476 bytes). Read-only legacy:
// v8 firmware loads it (drum_eng = 0) and writes v8; the .BAK it leaves behind is the untouched v7 file.
struct Patch7 {
    uint32_t magic, version;
    uint8_t    syn_algo;
    SynPreset  syn_preset[ALG_COUNT];
    float      syn_volume;
    DrumPadCfg drum_pad[N_PADS];
    float      drum_group_vol[DG_COUNT];
    float      drum_volume;
    SmpSlotCfg smp_slot[SMP_SLOTS];
    uint32_t   smp_len[SMP_SLOTS];
    float      smp_volume, smp_width, smp_width_prob, smp_polyphony;
    LaneSet    seq_lane[ENG_COUNT];
    uint16_t   fx_auto_mask[MAXSTEPS][FX_BANKS];
    uint8_t    fx_auto_fsr[MAXSTEPS];
    uint8_t    fx_dist_algo;
    uint8_t    fx_auto_enabled;
    float      outp_fader;
    uint32_t   crc32;
};
static_assert(offsetof(Patch, drum_eng) == offsetof(Patch7, crc32), "v8 = v7 prefix + drum_eng");
static_assert(sizeof(Patch7) == 7476, "v7 blob layout is frozen");

#define PATCH_VERSION 3   // v2: SynPreset gained pitch_amt; v3 (magic OPG8): + drum_eng

uint32_t pmorCrc32(const void* data, size_t n);
void     patchSnapshot(Patch& p);    // live engine state -> blob (CRC filled)
bool     patchCrcOk(Patch& p);       // accepts the current CRC and the pre-6w one
void     patchApply(const Patch& p); // blob -> live engine state (master fader deliberately not applied)
// Parse a file/mirror blob of n bytes: v8 (sizeof(Patch), 'OPG8') or legacy v7 (sizeof(Patch7), 'OPG7',
// converted with drum_eng = 0, CRC refreshed). false = wrong size/magic/CRC; out untouched then.
bool     patchFromBlob(const void* data, size_t n, Patch& out);

// ---- patch EXTENSION record (additive; the blob above stays frozen). A sidecar file PMOR3/Pnn.EXT beside Pnn.PMR holds state that has no room in
// the blob: the sampler per-slot START/END/LOOP/ATTACK/RELEASE and the first-hit record start offsets. Old firmware never opens it; absent,
// unreadable or CRC-bad = every field at its default (what a patch had before this existed). Layout: header {magic 'PEXT', version, body_len,
// crc32 of the body} then the body; a reader uses the prefix of the body it knows, a longer body from a newer firmware is accepted (fields it
// does not know are ignored), so fields can be appended later without a new file name. A shorter (older) body is accepted too: missing fields = defaults.
#define PEXT_MAGIC   0x54584550u   // 'PEXT' little endian
#define PEXT_VERSION 2u            // 2 = + the drum synth tail (per-pad params); a v1 record (shorter body) is accepted: pads then load their preset's params
#define DSV_FLOATS 136                                  // = SDEF_FLOATS (drum_super.h; static_assert in patch_format.cpp)
struct PatchExt {
    uint32_t magic, version, body_len, crc;       // crc = CRC-32 of the body (the bytes after this header)
    SmpSlotExt smp[SMP_SLOTS];                    // start, end, loop_f, attack_ms, release_ms per slot
    uint16_t   seq_off[ENG_COUNT];                // first-hit start offset per lane (sequencer.h)
    uint16_t   pad_;
    float      lfo[4];                            // synth LFO: rate_hz, depth, shape, dest (synth.h)
    float      deep[DG_COUNT][DDEEP_PARAMS];      // drum deep recipe multipliers (drums.h), 1 = neutral
    // ---- v2: drum synth engine. Only pads whose params differ from their preset are stored (a pad with no entry loads its preset's params: what every
    // pre-v2 patch does). dsv_mask bit p = pad p has a block; blocks are packed in ascending pad order in dsv[0..dsv_n). The record's body_len ends after the last used block.
    uint16_t   dsv_mask, dsv_n;
    float      dsv[N_PADS][DSV_FLOATS];
};
static_assert(sizeof(SmpSlotExt) == 20, "SmpSlotExt is part of the on-card extension layout");
#define PEXT_BODY_V2_BASE (SMP_SLOTS * 20 + 8 + 16 + DG_COUNT * DDEEP_PARAMS * 4 + 4)   /* up to and including dsv_mask/dsv_n; blocks follow */
static_assert(sizeof(PatchExt) == 16 + PEXT_BODY_V2_BASE + N_PADS * DSV_FLOATS * 4, "PatchExt layout is on the card");
// Body length of each version of the record; a body SHORTER than the current one (an older firmware's file) is accepted, the fields it lacks read as defaults.
#define PEXT_BODY_V1 (SMP_SLOTS * 20 + 8)
size_t   patchExtBytes(const PatchExt& e);             // bytes of the record to write: header + body_len (shrinks with the number of customised drum pads)
void     patchExtSnapshot(PatchExt& e);                 // live state -> record (CRC filled)
bool     patchExtParse(const void* data, size_t n, PatchExt& out);   // false = absent/short/wrong magic/CRC; out untouched then
void     patchExtApply(const PatchExt& e);              // record -> live state (values clamped to the editor ranges)
uint32_t patchExtLiveCrc(void);                         // 0 while every field is at its default, else a CRC of the live fields (folded into the autosave change scan)
uint32_t patchApplyCount(void);  // bumps on every apply (patch load / new / clear): the protocol's "patch changed" epoch
