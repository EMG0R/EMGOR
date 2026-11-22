// usb_audio_stream.h — bridges the Daisy Seed audio callback (float,
// non-interleaved, 48-sample blocks) into the TinyUSB UAC2 capture FIFO.
#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once from main() before the audio callback starts.
void usb_audio_stream_init(void);

// Call once per Daisy audio callback invocation, from interrupt/callback
// context. `in` follows libDaisy's AudioHandle::AudioCallback convention:
// in[0..n_channels-1] are per-channel float arrays, each n_frames long,
// samples in [-1.0, 1.0]. n_frames MUST equal AUDIO_BLOCK_SAMPLES (48) --
// that 1:1 mapping between "one Daisy callback" and "one nominal 1ms USB
// frame" is what keeps the drift-compensation math in audio_format.h
// meaningful; see README.md "48-sample blocks" for why.
//
// Packs to interleaved L/R at the build's configured bit depth (24-bit/3
// bytes or 16-bit/2 bytes, see audio_format.h) and hands the result to
// TinyUSB's tud_audio_write(), which queues it into the class driver's own
// software FIFO. TinyUSB's flow-control layer (CFG_TUD_AUDIO_EP_IN_FLOW_
// CONTROL) is what actually drains that FIFO into 47/48/49-sample IN
// packets at each SOF -- this function only ever pushes exactly 48 samples
// worth per call; it does not itself decide packet sizes.
//
// No-ops (and returns immediately) if the host hasn't opened the capture
// stream yet (see usb_audio_stream_is_active()) or if the device isn't
// mounted -- cheap enough to call unconditionally from the audio ISR.
void usb_audio_stream_push_block(float const *const *in, size_t n_channels, size_t n_frames);

// Defined in usb_descriptors.c, driven by tud_audio_set_itf_cb(): true once
// the host has selected AS alternate-setting 1 (i.e. opened the capture
// device), false again on alt 0 / interface close.
bool usb_audio_stream_is_active(void);

#ifdef __cplusplus
}
#endif
