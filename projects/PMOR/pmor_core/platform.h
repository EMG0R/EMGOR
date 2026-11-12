// platform.h -- the only two things the core needs from the machine it runs on.
#pragma once
#include <stdint.h>

// Wall-clock milliseconds. Firmware: daisy::System::GetNow() (hw/platform_hw.cpp). Host: see pmor_core/host/.
uint32_t pmorNowMs(void);

// Free-running CPU cycle counter (firmware: DWT->CYCCNT at 480 MHz; host: steady_clock nanoseconds). Differences are wrap-safe; used by the drum cycle probe.
uint32_t pmorCycles(void);

// Publish-before-bump barrier for the seqlock in clock.cpp. On the Cortex-M7 this is exactly the
// original "dmb"; elsewhere a full fence (same ordering guarantee, any CPU).
#if defined(__arm__) && !defined(__aarch64__)
#define PMOR_MEM_BARRIER() __asm volatile("dmb" ::: "memory")
#else
#define PMOR_MEM_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif
