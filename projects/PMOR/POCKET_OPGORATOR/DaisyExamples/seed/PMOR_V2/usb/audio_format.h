// audio_format.h — Pocket OpGorator TinyUSB composite firmware
//
// Single source of truth for the UAC2 capture sample format. Both
// usb_descriptors.c (what we tell the host) and tusb_config.h /
// usb_audio_stream.c (what we actually push into the endpoint FIFO) include
// this header so the two can never drift apart.
//
// Build-flag fallback: define POCKET_OPGORATOR_AUDIO_16BIT (via `make
// AUDIO_BITS=16`) to drop from the primary 24-bit format to the
// community-tested 16-bit fallback. See README.md "16-bit fallback" section
// for why 24-bit is the one that gets fragile on some hosts/hubs.
#pragma once

#include <stdint.h>

#if defined(POCKET_OPGORATOR_AUDIO_16BIT)
#define AUDIO_BYTES_PER_SAMPLE 2u
#define AUDIO_BIT_RESOLUTION   16u
typedef int16_t audio_sample_t;
#else
#define AUDIO_BYTES_PER_SAMPLE 3u
#define AUDIO_BIT_RESOLUTION   24u
/* No native 24-bit C type; samples are packed 3 bytes/sample (little-endian,
 * sign-extended from the low 24 bits of a 32-bit int) by
 * audio_pack24() in usb_audio_stream.c. */
#endif

#define AUDIO_N_CHANNELS   2u              /* stereo capture: PCM3060 L/R */
#define AUDIO_SAMPLE_RATE  48000u          /* fixed; firmware never negotiates rate */

/* Daisy audio callback block size. 48 samples @ 48kHz == exactly 1ms, i.e.
 * exactly one USB full-speed frame's worth of audio. This is what lets the
 * UAC2 IN endpoint and the Daisy audio callback stay in lock-step: one
 * callback == one nominal USB frame payload. */
#define AUDIO_BLOCK_SAMPLES 48u

/* Isochronous IN endpoints on an asynchronous (unclocked-by-host) source
 * must be sized for the worst case of the drift-compensation scheme: one
 * extra sample's worth of headroom per packet (see README "47/49-sample
 * flow control"). TinyUSB's own audio class driver (CFG_TUD_AUDIO_EP_IN_
 * FLOW_CONTROL, default on) picks 47/48/49 automatically from FIFO fill
 * level -- we just have to size the endpoint/FIFO for the 49-sample case. */
#define AUDIO_MAX_SAMPLES_PER_PACKET (AUDIO_BLOCK_SAMPLES + 1u)

#define AUDIO_EP_IN_SZ_MAX \
  (AUDIO_MAX_SAMPLES_PER_PACKET * AUDIO_N_CHANNELS * AUDIO_BYTES_PER_SAMPLE)

/* Nominal (non-drift) bytes per 1ms block -- what the spec's "288 vs 192
 * bytes per 1ms frame" figures refer to. */
#define AUDIO_NOMINAL_BLOCK_BYTES \
  (AUDIO_BLOCK_SAMPLES * AUDIO_N_CHANNELS * AUDIO_BYTES_PER_SAMPLE)
