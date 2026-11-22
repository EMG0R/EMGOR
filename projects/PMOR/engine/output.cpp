// output.cpp — master fader + always-on Fruity-style soft clipper (research/
// clipper-and-posync.md §A.5) + hard ±MASTER_UNITY clamp + NaN guard + peak
// meter into g.out_amp. AUDIO plane; outputInit() is CONTROL plane (boot only).
#include <string.h>
#include <math.h>
#include "output.h"
#include "shared_state.h"
#include "engine_util.h"

OutputState outp;

// file-scope AUDIO-plane state: the clipper coefficients (rebuilt only when
// clip_thresh_db actually changes, never per-sample) and the peak follower.
static FlClip s_clip;
static float  s_clip_cached_db = 1.0e9f;
static Env    s_peak;

void outputInit() {
    memset(&outp, 0, sizeof(outp));
    outp.fader          = 1.f;
    outp.clip_thresh_db = CLIP_THRESH_DB;
    outp.input_gain     = 1.f;
    flclipSet(s_clip, outp.clip_thresh_db, 0.f);   // post gain lives in the fader/drive stage below, not here
    s_clip_cached_db = outp.clip_thresh_db;
    s_peak.setReleaseMs(150.f);                     // instant attack (built into Env), ~150 ms release
    outp.peak = 0.f;
}

void flclipSet(FlClip& c, float thresh_db, float post_db) {
    float t = powf(10.f, thresh_db * 0.05f);
    if (t > 0.999f)  t = 0.999f;                     // 0 dB -> hard clip
    if (t < 0.0025f) t = 0.0025f;                     // ~ -52 dB floor
    c.t      = t;
    c.inv4w  = 1.f / (4.f * (1.f - t));
    c.kneeEnd = 2.f - t;
    c.post   = powf(10.f, post_db * 0.05f);
}

void outputProcess(float* L, float* R, size_t n) {
    // rebuild the clipper only on an actual threshold change (control-plane
    // edit) — no divisions/powf in the per-sample loop below.
    if (outp.clip_thresh_db != s_clip_cached_db) {
        flclipSet(s_clip, outp.clip_thresh_db, 0.f);
        s_clip_cached_db = outp.clip_thresh_db;
    }

    // Gain staging (spec §9, corrected 2026-09-14):
    //   0..100 % of the fader is a plain linear volume applied BEFORE the clipper, so at
    //   half volume the clipper sees half the signal and stays transparent.
    //   Above 100 % (up to 133 %) the level does not rise: the extra only drives the
    //   Fruity-style soft clipper harder. The final scale MASTER_UNITY (= the old 75 %)
    //   is the absolute ceiling the speakers ever see.
    float fader = outp.fader;
    if (fader < 0.f) fader = 0.f;
    float pre   = (fader < 1.f) ? fader : 1.f;
    float over  = fader - 1.f;
    if (over < 0.f) over = 0.f;
    float drive = pre * (1.f + over * 3.f);
    const float post = MASTER_UNITY;

    for (size_t i = 0; i < n; i++) {
        float xl = L[i] * drive;
        float xr = R[i] * drive;
        float yl = flclipSample(s_clip, xl) * post;
        float yr = flclipSample(s_clip, xr) * post;

        yl = flushBad(yl);                                    // NaN/inf guard
        yr = flushBad(yr);

        if (yl >  MASTER_UNITY) yl =  MASTER_UNITY;            // hard clamp, last line of defence
        else if (yl < -MASTER_UNITY) yl = -MASTER_UNITY;
        if (yr >  MASTER_UNITY) yr =  MASTER_UNITY;
        else if (yr < -MASTER_UNITY) yr = -MASTER_UNITY;

        L[i] = yl;
        R[i] = yr;

        float m = fabsf(yl) > fabsf(yr) ? fabsf(yl) : fabsf(yr);
        s_peak.process(m);                                     // instant attack, ~150 ms release
    }

    float pk = s_peak.value();
    pk = (MASTER_UNITY > 0.f) ? (pk / MASTER_UNITY) : pk;       // normalise so MASTER_UNITY == 1.0
    outp.peak  = pk;
    g.out_amp  = pk;
}
