// Host-only stand-in for libDaisy's daisy_seed.h so pmor_core/clock.cpp + pmor_core/sequencer.cpp can be
// compiled and MEASURED on the Mac (tools/host_clock_test.cpp). Provides only what those two files touch.
#pragma once
#include <stdint.h>
namespace daisy {
struct System {
    static uint32_t s_now_ms;
    static uint32_t GetNow() { return s_now_ms; }
};
}
