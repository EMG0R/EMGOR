// hyper_config.h -- NEW (HYPER_CORE, 2026-11-13): the three constants engine_util.h takes from config.h.
// Values match PMOR_V3_live/config.h (SR 48000, BLOCK 64). Per-instrument constants stay out of here.
#pragma once
#define SR       48000.0f
#define BLOCK    64
#define BLOCK_HZ (SR / BLOCK)
