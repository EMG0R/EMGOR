// Freeze and blur (the rig's two spectral "slot" effects) share ONE AXI SRAM slot (replaces fx_glue_freeze.h / fx_glue_blur.h, whose static arenas no longer fit with the new phase-cloud freeze). Their param OBJECTS stay
// resident (SDRAM), so params / pd / persistence work while one sleeps; the arrays live in the slot and the big cold buffers go to SDRAM through Arena::cold. Waking the effect that does not own the slot rebuilds it in
// the main loop (axAcquire: save params, reset the slot, placement-new at the same address, init, restore params); the audio thread never allocates. If the other one is on, the wake is REFUSED (red blink).
#pragma once
#include "fx_glue_tmpl.h"
static const size_t AX_SLOT = 68 * 1024, AX_COLD = 320 * 1024;
static uint8_t g_ax_mem[AX_SLOT] __attribute__((section(".axi_bss"), aligned(32)));
static uint8_t HX_SDRAM g_ax_cold_mem[AX_COLD] __attribute__((aligned(32)));
static uint8_t HX_SDRAM g_ax_obj[2][((sizeof(hx::FxFreeze) > sizeof(hx::FxBlur) ? sizeof(hx::FxFreeze) : sizeof(hx::FxBlur)) + 63) & ~(size_t)31] __attribute__((aligned(32)));
static hx::Arena g_ax_arena, g_ax_cold; static int g_ax_owner = -1; static size_t g_ax_used[2] = {0, 0};
static hx::Fx* axConstruct(int which, float sr) {
    g_ax_arena.bind(g_ax_mem, sizeof(g_ax_mem)); g_ax_cold.bind(g_ax_cold_mem, sizeof(g_ax_cold_mem)); g_ax_arena.cold = &g_ax_cold;
    hx::Fx* f = which == 0 ? static_cast<hx::Fx*>(new (g_ax_obj[0]) hx::FxFreeze(g_ax_arena, sr)) : static_cast<hx::Fx*>(new (g_ax_obj[1]) hx::FxBlur(g_ax_arena, sr));
    g_ax_owner = which; g_ax_used[which] = g_ax_arena.used; return f;
}
static hx::Fx* freezeCreate(hx::Arena&, float sr) { return axConstruct(0, sr); }
static hx::Fx* blurCreate(hx::Arena&, float sr) { return axConstruct(1, sr); }
static const FxDef FXD_freeze = { "freeze", freezeCreate, 64, 0.05f, 4, { 63, 207, 156 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
static const FxDef FXD_blur = { "blur", blurCreate, 64, 0.05f, 4, { 63, 207, 156 }, nullptr, nullptr, nullptr, false, nullptr, nullptr };
static const int GI_AXA = GI_FREEZE, GI_AXB = GI_BLUR;
// Main loop only. nullptr = the effect owns the slot (or needs none); otherwise the reason it cannot wake.
static const char* axAcquire(int gi) {
    if(gi != GI_AXA && gi != GI_AXB) return nullptr;
    const int w = gi == GI_AXA ? 0 : 1; if(g_ax_owner == w) return nullptr;
    FxG& o = g_fxg[w ? GI_AXA : GI_AXB]; if(o.latch || o.hold || o.gate.state != FXS_IDLE) return w ? "no AXI memory: freeze is using it" : "no AXI memory: blur is using it";
    FxG& g = g_fxg[gi]; if(!g.ok) return "effect unavailable";
    float vs[HX_MAXP]; const int np = g.fx->np; for(int i = 0; i < np && i < HX_MAXP; i++) vs[i] = g.fx->v[i];
    g.clearing = true; hx::Fx* f = axConstruct(w, 48000.0f);
    if(!f || !f->ok) { g.clearing = false; return "no AXI memory"; }
    f->init(48000.0f, 48.0f); for(int i = 0; i < np && i < HX_MAXP; i++) f->setParam(i, vs[i]); f->Fx::snap(); f->reset();
    g.clean = true; g.gate.state = FXS_IDLE; g.clearing = false; o.clean = true; return nullptr;   // the sleeping one is never reset: its arrays are no longer valid
}
