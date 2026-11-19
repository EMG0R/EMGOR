// Glue for HYPER_CORE FxGrain (McCurdy grain delay) : only the FxDef below (arena 1100000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_grain = { "grain", hx::fx_create_grain, 1100000, 0.061f, 3, { 30, 230, 170 }, nullptr, nullptr, nullptr, false, &g_extmix[GI_GRAIN], nullptr };
