// fx_engine.h -- 4-slot FX engine: any registry effect in any slot, serial chain, per-slot bypass crossfade,
// shared analysis (pitch tracker + envelope), meters, per-slot timing hook. Portable C++17.
#pragma once
#include "fx_common.h"
#include "fx_registry.h"
#include "fx_pitchtrack.h"

namespace hx {

// Host-supplied monotonic clock in nanoseconds (optional; used only for the CPU meter). nullptr = no timing.
extern uint64_t (*g_nowNs)();

class FxEngine {
public:
    // sdram: memory for 4 slot arenas (+ nothing else). bytes >= HX_NSLOTS * 64 KB; each slot gets bytes/HX_NSLOTS.
    void init(float sampleRate, void* sdram, size_t bytes);
    // CONTROL THREAD ONLY (never from the audio callback): builds the effect in the slot arena; slot passes audio through while swapping.
    bool setEffect(int slot, int typeId);            // typeId -1 = empty slot
    int  effectType(int slot) const { return slots_[slot].type; }
    Fx*  fx(int slot) { return slots_[slot].ready ? slots_[slot].fx : nullptr; }
    void setBypass(int slot, bool b) { slots_[slot].bypassTarget = b; }
    bool bypassed(int slot) const { return slots_[slot].bypassTarget; }
    void setParam(int slot, int idx, float v) { if (Fx* f = fx(slot)) f->setParam(idx, v); }
    void action(int slot, int id, float v) { if (Fx* f = fx(slot)) f->action(id, v); }
    void setValveHint(float hz) { valveHz_ = hz; }
    float valveHint() const { return valveHz_; }

    // mono mic in -> stereo out, n <= HX_MAXBLOCK. Audio-thread safe.
    void process(const float* in, float* outL, float* outR, int n);

    // meters (peak since last read; reading resets)
    float readInPeak()  { float p = inPeak_;  inPeak_ = 0.f;  return p; }
    float readOutPeak() { float p = outPeak_; outPeak_ = 0.f; return p; }
    float f0() const { return ctx_.f0; }
    float env() const { return ctx_.env; }
    // average ns per block per slot (EMA); 0 if no clock
    float slotNs(int slot) const { return slots_[slot].ns; }
    float totalNs() const { return totalNs_; }
    float sampleRate() const { return sr_; }
private:
    struct Slot { Arena arena; Fx* fx = nullptr; int type = -1; volatile bool ready = false; bool bypassTarget = false; float g = 1.f; float ns = 0.f; };
    Slot slots_[HX_NSLOTS];
    PitchTrack pt_; FxCtx ctx_{}; float sr_ = 48000.f, envY_ = 0.f, inPeak_ = 0.f, outPeak_ = 0.f, valveHz_ = 0.f, totalNs_ = 0.f;
    float dryL_[HX_MAXBLOCK], dryR_[HX_MAXBLOCK];
    Arena ptArena_;
};

} // namespace hx
