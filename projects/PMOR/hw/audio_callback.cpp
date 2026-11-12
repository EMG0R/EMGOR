// audio_callback.cpp -- the firmware's thin hardware layer around pmor_core's pmorRenderBlock().
// Hardware-only duties here: joystick/encoder ADC poll, USB clock engine's sample clock, PO-sync click
// (in place on the finished block), CPU meter. Never blocks, never touches I2C.
#include <string.h>
#include "../config.h"
#include "../hw/hw.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/clock.h"
#include "../pmor_core/render.h"
#include "studio.h"
#include "daisy_seed.h"
extern "C" {
#include "usb/clock_engine.h"
#include "usb/audio_format.h"
}
using namespace daisy;

// TASK 2 (2026-10): C-linkage adapter so usb/clock_engine.c (pure C, can't call a
// C++-mangled symbol) can read engine/clock.h's exact grid period and derive its
// MIDI tick scheduling from the SAME fixed-point source clock.cpp's own grid
// accumulator uses -- see usb/clock_engine.h's declaration for the full rationale.
extern "C" uint64_t clock_engine_grid_step_q32(void) { return clockStepSamplesQ32(); }



// clock_engine_process_audio() requires n_frames <= AUDIO_BLOCK_SAMPLES (48):
// its beat scratch buffer is that size. Our block is 64, so feed it in ≤48
// chunks — the routine is per-sample stateful and advances its own sample
// clock by n_frames, so chunking is exactly equivalent. Run IN PLACE on the
// final stereo block: SY0 passes audio through, SY1/SY2/SY5 replace one
// channel with the PO-sync click, SY3/SY4 consume sync-in from `in`.
static void poSyncProcess(const float* codecL, const float* codecR, float* L, float* R, size_t n) {
    // The routine's `in` is what it ROUTES to the outputs (SY0 stereo, SY1/2
    // mono on the free channel) and what it WATCHES for sync pulses (SY3/SY4/
    // SY5). So: the sync-in channel gets the codec input, everything else gets
    // our own mix (mono where the mode collapses to one channel), and the
    // result lands in L/R in place.
    static float sL[BLOCK], sR[BLOCK];
    if(n > BLOCK) n = BLOCK;
    sync_mode_t mode = clock_engine_get_sync_mode();
    switch(mode) {
        case SY3_IN_L:   // watch codec L; audio (from `in` R) → both outs
            for(size_t i = 0; i < n; i++) { sL[i] = codecL[i]; sR[i] = 0.5f * (L[i] + R[i]); }
            break;
        case SY4_IN_R:   // watch codec R; audio (from `in` L) → both outs
            for(size_t i = 0; i < n; i++) { sL[i] = 0.5f * (L[i] + R[i]); sR[i] = codecR[i]; }
            break;
        case SY5_PASS:   // watch codec L, repeat click on L; audio (from `in` R) on R
            for(size_t i = 0; i < n; i++) { sL[i] = codecL[i]; sR[i] = 0.5f * (L[i] + R[i]); }
            break;
        default:         // SY0/SY1/SY2: our mix is the audio source
            for(size_t i = 0; i < n; i++) { sL[i] = L[i]; sR[i] = R[i]; }
            break;
    }
    size_t off = 0;
    while(off < n) {
        size_t c = n - off; if(c > AUDIO_BLOCK_SAMPLES) c = AUDIO_BLOCK_SAMPLES;
        const float* ip[2] = { sL + off, sR + off };
        float*       op[2] = { L + off, R + off };
        clock_engine_process_audio(ip, op, c);
        off += c;
    }
}

volatile uint32_t g_audio_overruns = 0; volatile uint32_t g_cpu_max_permille = 0;
void AudioCallback(float** in, float** out, size_t n)
{
    uint32_t t_tick0 = System::GetTick();

    joyPollFromIsr(pmorSampleNow());                    // enc1 + joystick ADC, 750 Hz; pmorSampleNow() = this block's first sample (stamps encoder presses)
    // Advance the clock engine's sample clock. Required or the boot-listen
    // window never elapses and the device never emits MIDI clock (2026-08-16).
    clock_engine_note_audio_block((uint32_t)n);

    pmorRenderBlock(in[0], out[0], out[1], n);          // clock -> sequencer -> engines -> FX -> metro -> master output
    if(studioMuted(pmorSampleNow())) { memset(out[0], 0, n * sizeof(float)); memset(out[1], 0, n * sizeof(float)); }   // STUDIO/EDIT: Live is the monitor
    poSyncProcess(in[0], in[1], out[0], out[1], n);     // PO-sync click / sync-in (never wired in v2)

    // Timer TICKS, not GetUs(): the 32-bit timer wraps every ~21 s at its tick rate and GetUs() (ticks / divisor) jumps
    // backwards at the wrap, which produced a bogus multi-second "block time" (a phantom overrun / cpu_max garbage).
    uint32_t dt = System::GetTick() - t_tick0;                         // wrap-safe unsigned difference
    const float block_ticks = (float)n * ((float)System::GetTickFreq() / SR);
    g.cpu_load = (float)dt / block_ticks;
    if((float)dt > block_ticks) g_audio_overruns = g_audio_overruns + 1;
    uint32_t pm = (uint32_t)(g.cpu_load * 1000.f); if(pm > g_cpu_max_permille) g_cpu_max_permille = pm;
}
