// Glue for HYPER_CORE FxAutopan (v1 p autopan: lidar sets depth and rate). Pad / web toggled (it was never on the v1 audio path); DEPTH and PERIOD are lidar-locked (fx_controls.h).
#pragma once
#include "fx_glue_tmpl.h"
static const FxDef FXD_autopan = { "autopan", hx::fx_create_autopan, 4096, 0.02f, 2, { 255, 111, 176 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
