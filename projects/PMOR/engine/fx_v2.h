// fx_v2.h — the verbatim PMOR_V2 punch-in FX chain (see fx_v2.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>

struct FxV2Blk {                // per-block inputs (v2's globals)
    uint16_t bank[8];           // effective held masks FX1..FX8 (live | automation)
    float    fsr;               // 0..1
    float    lid;               // lidar 0..1
    uint32_t gstep;             // master 16th counter
    uint32_t step_ms;           // ms per 16th
    uint32_t step_rw;           // capture-ring head at the last 16th boundary (REV/SCAT anchor)
};
void     fxV2Init();
void     fxV2Block(const FxV2Blk& b, float* L, float* R, size_t n);
uint32_t fxV2RingHead();        // the capture ring write head (for step_rw latching)
extern const char* const FX_V2_NAMES[8][16];
