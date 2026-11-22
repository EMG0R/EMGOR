// fx_registry.cpp -- the one table of effects. Each effect header defines inline hx::fx_create_<id>(Arena&, float).
#include "fx_registry.h"
#include "fx_harmonizer.h"
#include "fx_looper.h"
#include "fx_reverb.h"
#include "fx_grain.h"
#include "fx_delay.h"
#include "fx_flanger.h"
#include "fx_chorus.h"
#include "fx_mood.h"
#include "fx_freeze.h"
#include "fx_blur.h"
#include "fx_drone.h"
#include "fx_roar.h"
#include "fx_ringmod.h"
#include "fx_wah.h"
namespace hx {
static const FxTypeInfo kTypes[] = {
    { "harm",   "HARMONIZ", "p shiftr: 2-voice pfft~ gizmo harmonizer (enc1)",        fx_create_harm },
    { "looper", "LOOPER",   "rnbo~ 12-layer looper (pads 13/14)",                     fx_create_looper },
    { "reverb", "REVERB",   "scatter-FDN reverb (slider3 + tilt warp)",          fx_create_reverb },
    { "grain",  "GRAIN",    "McCurdy grain delay + octave-up AU (slider2)",           fx_create_grain },
    { "delay",  "DELAY",    "abl.device.delay~ + tanh, stochastic mode (FSR, pads)",  fx_create_delay },
    { "flange", "FLANGER",  "McCurdy Flanger.csd (lidar)",                            fx_create_flanger },
    { "chorus", "CHORUS",   "abl.dsp.chorus~ (pad 8)",                                fx_create_chorus },
    { "mood",   "MOOD",     "supervp.trans~ random pitch ladder (joystick)",          fx_create_mood },
    { "freeze", "FREEZE",   "Spectral Freezing AU (pads 3/4)",                        fx_create_freeze },
    { "blur",   "BLUR",     "Spectral Blurring AU (pad 7)",                           fx_create_blur },
    { "drone",  "DRONE",    "Spectral DroneMaker AU (slider1)",                       fx_create_drone },
    { "roar",   "ROAR",     "abl.device.roar~ saturator (pot1)",                      fx_create_roar },
    { "ring",   "RINGMOD",  "abl.dsp.ringmod~ (FSR + pad 5)",                         fx_create_ringmod },
    { "wah",    "WAH",      "NEW in v2 (lidar hand-wah, per IO_LIST)",                fx_create_wah },
};
int fxTypeCount() { return (int)(sizeof(kTypes) / sizeof(kTypes[0])); }
const FxTypeInfo* fxType(int i) { return (i >= 0 && i < fxTypeCount()) ? &kTypes[i] : nullptr; }
int fxTypeById(const char* id) { for (int i = 0; i < fxTypeCount(); i++) if (!strcmp(kTypes[i].id, id)) return i; return -1; }
Fx* fxCreate(int t, Arena& a, float sr) { const FxTypeInfo* ti = fxType(t); return ti ? ti->create(a, sr) : nullptr; }
}
