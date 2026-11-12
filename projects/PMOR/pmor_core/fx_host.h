// fx_host.h — PRIVATE contract between fx.cpp (the HOST) and fx_home.cpp's flag-setter
// effects (ST4/ST8/ST16/ST32/BZZ grid-locked retrigger family; HALF/DBL/STOP shadow-clock
// family). Not one of the fixed interface headers — only fx.cpp and fx_home.cpp include it.
//
// These HOME effects have empty process() bodies: they just flip flags here on
// onEngage()/onRelease(). fx.cpp's fxProcess() reads the flags once per block and owns all
// the actual DSP/timing (grid-locked retrigger scheduling, varispeed request arbitration).
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- grid-locked retrigger
enum StutRate : uint8_t { STUT_NONE = 0, STUT_4, STUT_8, STUT_16, STUT_32 };

struct FxHostStut {
    volatile uint8_t rate;   // StutRate; 0 = none of ST4/8/16/32 held
    volatile bool    bzz;    // BZZ held (independent of rate)
};
extern FxHostStut fx_host_stut;

static inline void fxHostSetRate(StutRate r)   { fx_host_stut.rate = (uint8_t)r; }
static inline void fxHostClearRate(StutRate r) { if (fx_host_stut.rate == (uint8_t)r) fx_host_stut.rate = STUT_NONE; }
static inline void fxHostBzzOn()  { fx_host_stut.bzz = true; }
static inline void fxHostBzzOff() { fx_host_stut.bzz = false; }

// ---------------------------------------------------------------- shadow-clock (varispeed)
// HALF/DBL/STOP each flip their own bit; fx.cpp recomputes fx.varispeed_req every block from
// these with STOP > DBL > HALF priority (most dramatic wins when more than one is held), and
// falls back to 1.0 when none are held.
struct FxHostVs {
    volatile bool half, dbl, stop;
};
extern FxHostVs fx_host_vs;
