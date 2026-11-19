// Glue for HYPER_CORE FxMood (random pitch ladder) : only the FxDef below (arena 16000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_mood = { "mood", hx::fx_create_mood, 16000, 0.039f, 0, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
