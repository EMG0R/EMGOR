// Glue for HYPER_CORE FxHarmony (HARMONY page: up to 3 shifted voices over the dry signal). On/off by the pad logic (any voice selected), MIX = encoder in harmony mode.
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_harmony = { "harmony", hx::fx_create_harmony, 20480, 0.15f, 0, { 255, 190, 60 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
