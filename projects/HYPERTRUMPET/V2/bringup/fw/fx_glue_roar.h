// Glue for HYPER_CORE FxRoar (saturator) : only the FxDef below (arena 4096 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_roar = { "roar", hx::fx_create_roar, 4096, 0.043f, 1, { 255, 154, 77 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
