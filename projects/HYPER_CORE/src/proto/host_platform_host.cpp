/* HYPER_CORE import
 * origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_max/pmor_core/host/platform_host.cpp
 * commit : PMOR_V3_max 9fd83ba
 * changes: none (verbatim). Host-build implementation of platform.h
 * licence: Emory (own work) */
// platform_host.cpp -- pmor_core/platform.h for host builds (offline render, tests, Max external).
// Wall clock is only mirrored into clk.ms_at_block; offline renders are sample-timed, so 0 is exact.
#include "../platform.h"
uint32_t pmorNowMs(void) { return 0; }
#include <chrono>
uint32_t pmorCycles(void) { return (uint32_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
