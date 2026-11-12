// Hardware stand-ins for the reference build of the ORIGINAL audio_callback.cpp.
#include <stdint.h>
#include <stddef.h>
extern "C" {
#include "usb/clock_engine.h"
}
void joyPollFromIsr(uint32_t) {}                       // no joystick on the host: g.joy_* stay 0
extern "C" {
void        clock_engine_note_audio_block(uint32_t) {}
sync_mode_t clock_engine_get_sync_mode(void) { return SY0_STEREO; }   // SY0: bit-exact passthrough
void clock_engine_process_audio(float const* const* in, float* const* out, size_t n) {   // SY0 semantics
    for (size_t i = 0; i < n; i++) { out[0][i] = in[0][i]; out[1][i] = in[1][i]; }
}
}
