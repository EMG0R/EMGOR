// Glue for HYPER_CORE FxRingmod : only the FxDef below (arena 4096 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_ring = { "ring", hx::fx_create_ringmod, 4096, 0.023f, 2, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
