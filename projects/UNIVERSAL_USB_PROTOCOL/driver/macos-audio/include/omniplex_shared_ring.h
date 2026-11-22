// omniplex_shared_ring.h
//
// IPC contract between the Omniplex Audio HAL plugin (runs inside coreaudiod,
// as root/_coreaudiod) and the omniplex-cli Rust process (runs as the login
// user). Shared via a single POSIX shared-memory object so both sides can
// be built independently -- this header is the single source of truth for
// the layout and MUST be kept in sync between the two implementations
// (the C plugin here, and a future Rust rewrite of this struct in
// crates/omniplex-cli/src/bridge/audio.rs via a matching #[repr(C)] type or raw
// byte offsets).
//
// Transport: POSIX shared memory, shm_open(2), name below. World-readable
// and world-writable (0666) because the two peers run as different UIDs
// (coreaudiod vs. the login user) and there is no other practical way for
// them to share a segment. This is inherently *local-machine-trust* IPC:
// anything on the box can attach to this segment. That's an accepted
// tradeoff for a single-user music workstation use case; do not reuse this
// mechanism verbatim in a multi-tenant context.
//
// Two independent single-producer/single-consumer (SPSC) lock-free ring
// buffers live in the segment:
//
//   playback ring:  HAL plugin (producer) -> Rust bridge (consumer)
//                    Audio a DAW/app plays OUT to "Omniplex Audio" (the HAL
//                    output stream) lands here. Rust reads it and sends it
//                    over the network to the Pi's speaker.
//
//   capture ring:   Rust bridge (producer) -> HAL plugin (consumer)
//                    Audio arriving over the network from the Pi's mic is
//                    written here by Rust. The HAL plugin reads it to
//                    satisfy the device's input stream (what a DAW sees
//                    when it records FROM "Omniplex Audio").
//
// Both rings carry interleaved 32-bit float samples at SAMPLE_RATE / CHANNELS
// below (must match crates/omniplex-cli/src/bridge/audio_core.rs SAMPLE_RATE=48000,
// CHANNELS=2).
//
// Each ring is a classic power-of-two-capacity SPSC ring: monotonically
// increasing 64-bit frame counters for write/read position (never reset,
// only ever incremented), with the actual storage index computed as
// `counter % capacity_frames`. This avoids the empty/full ambiguity of a
// single wraparound index and requires no locks -- only the single
// designated producer ever writes `write_frames`, only the single
// designated consumer ever writes `read_frames`. Memory ordering: the
// producer must publish sample data before bumping `write_frames`; the
// consumer must read `write_frames` before reading sample data. On the C
// side this is done with C11 atomics (acquire/release). On the Rust side,
// use `std::sync::atomic::{AtomicU64, Ordering::Release/Acquire}` on a
// pointer into the mmap'd region.
//
// Overflow/underflow policy (no blocking ever -- this runs on a real-time
// audio thread that must not stall):
//   - Producer, ring full (write_frames - read_frames >= capacity_frames):
//     the new frame(s) are DROPPED (producer does not advance read_frames;
//     it just declines to write and advances write_frames anyway so the
//     consumer can detect the gap via read_frames lagging by >= capacity,
//     OR -- as implemented here -- the producer simply does not write past
//     capacity and the consumer will observe a stall). See README for the
//     exact policy each side implements.
//   - Consumer, ring empty (read_frames >= write_frames): consumer emits
//     silence for the missing frames and does not advance read_frames
//     past write_frames.
//
// Segment layout (all integers native-endian, this is macOS-only local
// IPC so no wire-format endianness concerns):
//
//   [0]                                 SharedHeader   (128 bytes, see below)
//   [128]                               RingControl playback_ctrl (64 bytes)
//   [192]                               RingControl capture_ctrl  (64 bytes)
//   [256]                               float playback_data[capacity_frames * channels]
//   [256 + playback_bytes]              float capture_data[capacity_frames * channels]
//
// Total segment size = OMNIPLEX_RING_HEADER_BYTES (256) + 2 * capacity_frames * channels * 4.
// With the defaults below (capacity_frames=65536, channels=2): 256 + 2*65536*2*4
// = 256 + 1,048,576 = 1,048,832 bytes (~1 MiB), giving ~1.37s of buffering per
// direction at 48kHz -- generous headroom for scheduling jitter between the
// two processes, trimmed down by the jitter buffer/ASRC already present on
// the Rust receive path (audio_core.rs) before it ever reaches this ring.

#ifndef OMNIPLEX_SHARED_RING_H
#define OMNIPLEX_SHARED_RING_H

#include <stdatomic.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// POSIX shared memory object name (passed to shm_open). Leading slash is
// required by the POSIX shm API on Darwin.
#define OMNIPLEX_SHM_NAME "/omniplex_audio_ring_v1"

// Bump if the layout below changes incompatibly.
#define OMNIPLEX_RING_MAGIC   0x4F4D5058u /* 'OMPX' */
#define OMNIPLEX_RING_VERSION 1u

#define OMNIPLEX_SAMPLE_RATE       48000u
#define OMNIPLEX_CHANNELS          2u
#define OMNIPLEX_RING_CAPACITY_FRAMES (1u << 16) /* 65536 frames, ~1.37s @ 48kHz */
#define OMNIPLEX_FORMAT_FLOAT32_INTERLEAVED 1u

typedef struct {
    uint32_t magic;             // OMNIPLEX_RING_MAGIC
    uint32_t version;           // OMNIPLEX_RING_VERSION
    uint32_t sample_rate;       // OMNIPLEX_SAMPLE_RATE
    uint32_t channels;          // OMNIPLEX_CHANNELS
    uint32_t capacity_frames;   // OMNIPLEX_RING_CAPACITY_FRAMES
    uint32_t format;            // OMNIPLEX_FORMAT_FLOAT32_INTERLEAVED
    uint32_t plugin_alive;      // 1 while the HAL plugin has the segment open (heartbeat-ish flag)
    uint32_t reserved0;
    uint8_t  reserved[96];      // pad header to 128 bytes total
} SharedHeader;

_Static_assert(sizeof(SharedHeader) == 128, "SharedHeader must be exactly 128 bytes");

typedef struct {
    _Atomic uint64_t write_frames; // monotonic total frames written (producer-owned)
    _Atomic uint64_t read_frames;  // monotonic total frames read (consumer-owned)
    uint8_t reserved[48];          // pad to 64 bytes (cache-line sized)
} RingControl;

_Static_assert(sizeof(RingControl) == 64, "RingControl must be exactly 64 bytes");

#define OMNIPLEX_RING_HEADER_BYTES 256 /* sizeof(SharedHeader) + 2 * sizeof(RingControl) */

static inline uint64_t omniplex_ring_data_bytes(uint32_t capacity_frames, uint32_t channels) {
    return (uint64_t)capacity_frames * (uint64_t)channels * sizeof(float);
}

static inline uint64_t omniplex_segment_size(uint32_t capacity_frames, uint32_t channels) {
    return OMNIPLEX_RING_HEADER_BYTES + 2ull * omniplex_ring_data_bytes(capacity_frames, channels);
}

// Layout helpers -- given a base pointer to the mmap'd segment, return
// pointers to each region. Callers on the Rust side should replicate this
// arithmetic exactly (or bind to this header via bindgen).
typedef struct {
    SharedHeader*  header;
    RingControl*   playback_ctrl;   // app -> network
    float*         playback_data;   // capacity_frames * channels floats
    RingControl*   capture_ctrl;    // network -> app
    float*         capture_data;    // capacity_frames * channels floats
} OmniplexRingLayout;

static inline OmniplexRingLayout omniplex_layout(void* base) {
    OmniplexRingLayout l;
    uint8_t* b = (uint8_t*)base;
    l.header = (SharedHeader*)b;
    l.playback_ctrl = (RingControl*)(b + sizeof(SharedHeader));
    l.capture_ctrl  = (RingControl*)(b + sizeof(SharedHeader) + sizeof(RingControl));
    uint64_t data_bytes = omniplex_ring_data_bytes(OMNIPLEX_RING_CAPACITY_FRAMES, OMNIPLEX_CHANNELS);
    l.playback_data = (float*)(b + OMNIPLEX_RING_HEADER_BYTES);
    l.capture_data  = (float*)(b + OMNIPLEX_RING_HEADER_BYTES + data_bytes);
    return l;
}

#ifdef __cplusplus
}
#endif

#endif // OMNIPLEX_SHARED_RING_H
