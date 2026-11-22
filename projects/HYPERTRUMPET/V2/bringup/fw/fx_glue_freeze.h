// Glue for HYPER_CORE FxFreeze (spectral freeze): the FxDef below + a creator that builds the effect in a static AXI SRAM arena.
// The whole effect (object, per-bin arrays, STFT rings, windows, ~66 KB) lives in AXI SRAM (.axi_bss): the SDRAM pool was memory-bound on the device (freeze alone measured 22.9% avg); the pool only gets a token 64-byte slot.
// CPU estimate = host ns per sample x the device factor (docs/FX_BUDGET.md); everything else is the generic template.
#pragma once
#include "fx_glue_tmpl.h"
static uint8_t g_freeze_mem[68 * 1024] __attribute__((section(".axi_bss"), aligned(32)));
static hx::Arena g_freeze_arena;
static hx::Fx* freezeCreate(hx::Arena&, float sr) { g_freeze_arena.bind(g_freeze_mem, sizeof(g_freeze_mem)); return hx::fx_create_freeze(g_freeze_arena, sr); }
static const FxDef FXD_freeze = { "freeze", freezeCreate, 64, 0.05f, 4, { 63, 207, 156 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
