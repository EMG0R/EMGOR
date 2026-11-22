// pmor_share.h -- one core, many readers, any sample rate. Max-independent so it can be tested natively.
// The core renders in fixed BLOCKs at 48 kHz into a shared ring; every Reader (one per pmor.core~ instance)
// consumes the ring at its own cursor and linearly resamples from 48 kHz to the host rate. The core renders
// a block only when some reader needs frames that do not exist yet, so N instances cost one core.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <atomic>
#include <functional>

namespace pmorshare {

constexpr size_t RING_FR = 1u << 15;       // frames (power of two not required)
constexpr size_t BLOCK_FR = 64;

struct Ring {
    float L[RING_FR], R[RING_FR];
    uint64_t written = 0;                    // frames rendered so far (absolute)
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
    uint64_t blocksRendered = 0;
    void acquire() { while (lock.test_and_set(std::memory_order_acquire)) {} }
    void release() { lock.clear(std::memory_order_release); }
};

// render(l, r, BLOCK_FR): produce the next BLOCK_FR frames of the core
using RenderFn = std::function<void(float*, float*, size_t)>;

struct Reader {
    double cursor = 0.0;                     // absolute position in core frames
    double step = 1.0;                       // core frames per output frame = 48000 / host_sr
    void setRate(double hostSr) { step = 48000.0 / hostSr; }
    void join(const Ring& r) { cursor = (double)r.written; }

    // Fill n output frames (double buffers, as Max hands them over). Caller must NOT hold the ring lock.
    void pull(Ring& ring, const RenderFn& render, double* L, double* R, size_t n) {
        ring.acquire();
        const double lastPos = cursor + step * (double)(n > 0 ? n - 1 : 0);
        const uint64_t need = (uint64_t)lastPos + 2;            // +1 frame for interpolation
        while (ring.written < need) {
            float l[BLOCK_FR], r[BLOCK_FR]; render(l, r, BLOCK_FR);
            for (size_t i = 0; i < BLOCK_FR; ++i) { size_t k = (size_t)((ring.written + i) % RING_FR); ring.L[k] = l[i]; ring.R[k] = r[i]; }
            ring.written += BLOCK_FR; ring.blocksRendered++;
        }
        // this reader was idle for long enough that its history was overwritten: rejoin just behind the head
        if ((double)ring.written - cursor > (double)(RING_FR - 2 * BLOCK_FR)) cursor = (double)ring.written - (double)BLOCK_FR;
        for (size_t i = 0; i < n; ++i) {
            const uint64_t i0 = (uint64_t)cursor; const double fr = cursor - (double)i0;
            const size_t k0 = (size_t)(i0 % RING_FR), k1 = (size_t)((i0 + 1) % RING_FR);
            L[i] = ring.L[k0] + (ring.L[k1] - ring.L[k0]) * fr;
            R[i] = ring.R[k0] + (ring.R[k1] - ring.R[k0]) * fr;
            cursor += step;
        }
        ring.release();
    }
};

}  // namespace pmorshare
