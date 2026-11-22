// scenario.h -- offline scenario driver shared by the PRE-extraction reference build and the
// POST-extraction pmor_render tool. It only touches the public engine headers (synth.h, drums.h,
// sampler.h, sequencer.h, fx.h, clock.h, ...), found through -I, so the identical driver source
// runs against engine/ (ff86e4c) and pmor_core/ (after extraction).
#pragma once
#include <stddef.h>
// One audio block: in0 = mono line-in (sampler monitor), L/R = stereo out (zeroed by the callee).
typedef void (*ScenarioBlockFn)(const float* in0, float* L, float* R, size_t n);
// Runs a .scn script (see tests/cases/*.scn), writes interleaved L,R float32 little-endian to out_path.
// Returns 0 on success.
int scenarioRun(const char* script_path, const char* out_path, ScenarioBlockFn fn);
