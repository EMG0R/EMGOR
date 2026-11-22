// Glue for HYPER_CORE FxFlanger : only the FxDef below (arena 70000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_flange = { "flange", hx::fx_create_flanger, 70000, 0.043f, 2, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, &g_extmix[GI_FLANGE], nullptr };
