// usb_audio_stream.c — float -> packed-PCM conversion and hand-off to
// TinyUSB's UAC2 capture FIFO. See usb_audio_stream.h for the contract.
#include "usb_audio_stream.h"

#include <string.h>
#include "tusb.h"
#include "audio_format.h"

// Scratch pack buffer: CPU-side source for tud_audio_write()'s internal
// memcpy into the class driver's own FIFO (which IS the buffer TinyUSB's
// DMA/DWC2 code touches directly, and which lives in D2 RAM via
// CFG_TUSB_MEM_SECTION in tusb_config.h). This buffer itself is never
// touched by DMA -- it's a plain CPU write-then-read within the same
// function call -- so it does NOT need USB_DMA_MEM_SECTION placement.
static uint8_t s_pack_buf[AUDIO_BLOCK_SAMPLES * AUDIO_N_CHANNELS * AUDIO_BYTES_PER_SAMPLE];

void usb_audio_stream_init(void) {
  memset(s_pack_buf, 0, sizeof(s_pack_buf));
}

#if AUDIO_BYTES_PER_SAMPLE == 3
// Pack a float sample to 24-bit signed PCM, 3 bytes little-endian. Daisy's
// own internal audio path is 24-bit-in-32 (see libDaisy's AudioHandle), so
// this is the same conversion libDaisy does internally, just truncated to
// 3 bytes instead of sign-extended into a 4th.
static inline void audio_pack_sample(uint8_t *dst, float sample) {
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  int32_t const s24 = (int32_t) (sample * 8388607.0f);  // 2^23 - 1
  dst[0] = (uint8_t) (s24 & 0xFF);
  dst[1] = (uint8_t) ((s24 >> 8) & 0xFF);
  dst[2] = (uint8_t) ((s24 >> 16) & 0xFF);
}
#else
// 16-bit fallback: 2 bytes little-endian.
static inline void audio_pack_sample(uint8_t *dst, float sample) {
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  int16_t const s16 = (int16_t) (sample * 32767.0f);
  dst[0] = (uint8_t) (s16 & 0xFF);
  dst[1] = (uint8_t) ((s16 >> 8) & 0xFF);
}
#endif

void usb_audio_stream_push_block(float const *const *in, size_t n_channels, size_t n_frames) {
  if (!usb_audio_stream_is_active() || !tud_audio_mounted()) return;
  if (n_frames != AUDIO_BLOCK_SAMPLES) return;  // caller must set hw block size to 48

  // Interleave to L,R,L,R... regardless of how many channels the Daisy
  // callback actually hands us -- if only channel 0 is wired (mono board
  // variant) both L and R get the same signal rather than silence, since a
  // dropped channel reads worse to the ear than a doubled one.
  float const *ch_l = in[0];
  float const *ch_r = (n_channels > 1) ? in[1] : in[0];

  uint8_t *p = s_pack_buf;
  for (size_t i = 0; i < n_frames; i++) {
    audio_pack_sample(p, ch_l[i]);
    p += AUDIO_BYTES_PER_SAMPLE;
    audio_pack_sample(p, ch_r[i]);
    p += AUDIO_BYTES_PER_SAMPLE;
  }

  // tud_audio_write() copies into the class driver's own FIFO (D2 RAM,
  // flow-controlled drain at each SOF -- see tusb_config.h /
  // audio_format.h). If the FIFO is already full (host stalled / not
  // draining fast enough) the excess is simply dropped rather than
  // blocking the audio ISR -- an isochronous capture stream has no
  // retransmission concept anyway, a dropped block is just a dropout, and
  // blocking here would blow the Daisy's audio callback deadline.
  tud_audio_write(s_pack_buf, (uint16_t) sizeof(s_pack_buf));
}
