// Glue for HYPER_CORE FxReverb (8-line FDN, WARP): the generic template plus three hooks. Mapping: MIX <- fader 1 (ext), WARP and MODDEPTH <- accel Y.
// Full 8-line version (estimated 8 to 10 % of the Seed, far below the 60 % limit for HX_REVERB_LITE); the real load is in telemetry.
#pragma once
#include "fx_glue_tmpl.h"
static volatile float g_rv_warp_t = 0.0f, g_rv_modd_t = 0.8f;
static int g_rv_modd_idx = -1;
static void rvSetup(hx::Fx* f) {
    using R = hx::FxReverb; hx::FxReverb* r = static_cast<hx::FxReverb*>(f);
    r->setParam(R::P_DECAY, 0.94f); r->setParam(R::P_SIZE, 1.6f); r->setParam(R::P_DAMP, 5500.0f); r->setParam(R::P_MODD, 0.8f);   // big, long-tail Supermassive-style defaults
    r->setParam(R::P_MODR, 0.4f); r->setParam(R::P_LOWCUT, 80.0f); r->setParam(R::P_WIDTH, 1.0f); r->setParam(R::P_PRE, 25.0f); r->setParam(R::P_WARP, 0.0f);
    g_rv_modd_idx = r->findParam("MODDEPTH");
}
static void rvPre(hx::Fx* f) { f->v[f->role[hx::R_MOD]] = g_rv_warp_t; if(g_rv_modd_idx >= 0) f->v[g_rv_modd_idx] = g_rv_modd_t; }
static void rvWake(hx::Fx*) {}   // the reverb now has its own wake() (clears every line: far too long for the audio thread); fxClearPoll resets it in the main loop while idle
static const FxDef FXD_reverb = { "reverb", hx::fx_create_reverb, 524288, 0.073f, 5, { 180, 140, 255 }, rvSetup, rvPre, rvWake, false, &g_extmix[GI_REVERB], nullptr };
static void rvGlueInit() {
    fxgInit(GI_REVERB, FXD_reverb); FxGlue& g = g_glue[GI_REVERB]; if(!g.ok) return;
    hx::Fx* f = g.fx; if(f->role[hx::R_MOD] != 0xFF) g.lock[f->role[hx::R_MOD]] = "accel Y";    // v1: accel drives DelayWarp only, feedback fixed; MODDEPTH stays at its default
}

