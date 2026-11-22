// Host stand-in for libDaisy's daisy_seed.h, just enough for ui/nav.cpp + ui/input.cpp + hw/hw.h to compile in tests/test_ui.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace daisy {
struct System { static uint32_t s_now_ms; static uint32_t GetNow() { return s_now_ms; } };
struct DaisySeed {};
}
