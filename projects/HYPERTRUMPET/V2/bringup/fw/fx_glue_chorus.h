// Glue for HYPER_CORE FxChorus : only the FxDef below (arena 24000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_chorus = { "chorus", hx::fx_create_chorus, 24000, 0.038f, 2, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
