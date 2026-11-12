// Host stub of hw/hw.h for the reference build: audio_callback.cpp only needs joyPollFromIsr.
#pragma once
#include <stdint.h>
#include "daisy_seed.h"
void joyPollFromIsr(uint32_t block_start_sample);
