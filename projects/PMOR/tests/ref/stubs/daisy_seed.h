// Host stand-in for libDaisy's daisy_seed.h: only what engine/*.cpp and audio_callback.cpp touch.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace daisy {
struct System {
    static uint32_t GetNow() { return 0; }   // fixed: clock.cpp only mirrors it into clk.ms_at_block
    static uint32_t GetUs()  { return 0; }
};
}
