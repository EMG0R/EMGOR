// Glue for HYPER_CORE FxHarmonizer (2-voice interval harmonizer) : only the FxDef below (arena 12288 bytes, CPU estimate = host ns per sample x the reverb-derived factor (docs/FX_BUDGET.md), pad colour); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_harm = { "harm", hx::fx_create_harm, 12288, 0.023f, 0, { 255, 64, 190 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
