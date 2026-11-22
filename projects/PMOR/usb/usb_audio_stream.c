// usb_audio_stream.c — float -> packed-PCM conversion and hand-off to
// TinyUSB's UAC2 capture FIFO. See usb_audio_stream.h for the contract.
//
// STRUCTURE (2026-08-16 rework): the audio ISR never calls TinyUSB.
// It only packs samples into a private SPSC ring; the main loop drains the
// ring into tud_audio_write() via usb_audio_stream_task(), called right
// beside tud_task() in thread context.
//
// WHY: tud_audio_write() is an unlocked tu_fifo_write_n() into ep_in_ff --
// the same FIFO the OTG ISR streams out of via epin_write_tx_fifo(). The
// audio callback runs in the SAI DMA ISR at NVIC preempt priority 0;
// the OTG ISR runs lower. So calling tud_audio_write() from the audio ISR
// let a priority-0 ISR mutate FIFO cursors in the middle of a hardware
// TXFIFO copy, which wedges the DWC2 packet state machine permanently
// (device off the bus until power cycle). The earlier dcd_int_disable()
// guard around the write could NOT fix that direction of the race: by the
// time the preempting ISR masks the OTG line, it has already interrupted
// the copy. The only correct shape is to keep every TinyUSB call in one
// context. This bug was the nondeterministic USB killer: two byte-identical
// builds (halt-after-StartAudio) enumerated / wedged on different boots.
#include "usb_audio_stream.h"

#include <string.h>
#include "tusb.h"
#include "device/dcd.h"
#include "audio_format.h"

// No-UAC build (CFG_TUD_AUDIO 0, the default since 2026-11-05): the class
// driver is not compiled, so the stream is never mounted and push/task are
// no-ops. Callers in main/boot/oled stay unchanged.
#if !CFG_TUD_AUDIO
#define tud_audio_mounted()       false
#define tud_audio_write(buf, len) ((void)(buf), (void)(len), 0)
#endif

#define PACK_BYTES (AUDIO_BLOCK_SAMPLES * AUDIO_N_CHANNELS * AUDIO_BYTES_PER_SAMPLE)

// SPSC ring: audio ISR is the only producer, main loop the only consumer.
// 4 blocks (~5ms of audio) rides out main-loop hiccups (OLED page pushes)
// without adding meaningful capture latency.
#define RING_BLOCKS 16
static uint8_t          s_ring[RING_BLOCKS][PACK_BYTES];
static volatile uint8_t s_ring_w = 0;   // written only by the audio ISR
static volatile uint8_t s_ring_r = 0;   // written only by the main loop

void usb_audio_stream_init(void) {
  memset(s_ring, 0, sizeof(s_ring));
  s_ring_w = s_ring_r = 0;
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

// ISR side: pack + enqueue only. NO TinyUSB calls of any kind in here.
void usb_audio_stream_push_block(float const *const *in, size_t n_channels, size_t n_frames) {
  if (!usb_audio_stream_is_active() || !tud_audio_mounted()) return;
  if (n_frames != AUDIO_BLOCK_SAMPLES) return;  // caller must set hw block size to 48

  uint8_t const w    = s_ring_w;
  uint8_t const next = (uint8_t) ((w + 1) % RING_BLOCKS);
  if (next == s_ring_r) return;  // ring full: drop the block. Isochronous
                                 // capture has no retransmission; a dropped
                                 // block is just a dropout, and blocking in
                                 // the audio ISR is never acceptable.

  // Interleave to L,R,L,R... regardless of how many channels the Daisy
  // callback actually hands us -- if only channel 0 is wired (mono board
  // variant) both L and R get the same signal rather than silence, since a
  // dropped channel reads worse to the ear than a doubled one.
  float const *ch_l = in[0];
  float const *ch_r = (n_channels > 1) ? in[1] : in[0];

  uint8_t *p = s_ring[w];
  for (size_t i = 0; i < n_frames; i++) {
    audio_pack_sample(p, ch_l[i]);
    p += AUDIO_BYTES_PER_SAMPLE;
    audio_pack_sample(p, ch_r[i]);
    p += AUDIO_BYTES_PER_SAMPLE;
  }
  s_ring_w = next;   // publish AFTER the block is fully written
}

// Thread side: drain the ring into TinyUSB. Call from the main loop right
// next to tud_task(). The dcd_int guard here IS valid in thread context --
// it keeps the OTG ISR from starting a TXFIFO copy mid-write, and nothing
// can preempt us into the FIFO because the audio ISR no longer touches it.
void usb_audio_stream_task(void) {
  if (!tud_audio_mounted()) { s_ring_r = s_ring_w; return; }  // flush stale
  while (s_ring_r != s_ring_w) {
    uint8_t const r = s_ring_r;
    dcd_int_disable(0);
    tud_audio_write(s_ring[r], (uint16_t) PACK_BYTES);
    dcd_int_enable(0);
    s_ring_r = (uint8_t) ((r + 1) % RING_BLOCKS);
  }
}
