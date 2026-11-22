// Glue for HYPER_CORE FxDelay (stereo delay, stochastic hold) : only the FxDef below (arena 1080000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_delay = { "delay", hx::fx_create_delay, 1080000, 0.045f, 3, { 79, 192, 234 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
