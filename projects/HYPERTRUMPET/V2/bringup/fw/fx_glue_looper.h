// Glue for HYPER_CORE FxLooper (4 x 12 s layers; keeps its layers across off/on) : only the FxDef below (arena 9400000 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_looper = { "looper", hx::fx_create_looper, 9400000, 0.03f, 3, { 232, 195, 74 }, nullptr, nullptr, nullptr, true, nullptr, nullptr };
