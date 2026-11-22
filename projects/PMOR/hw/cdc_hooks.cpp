// cdc_hooks.cpp — the remaining CDC test hooks usb/clock_engine.c (ported
// as-is) links against. v2's runtime subsystem gates (TRel/TKeys/TOled/TLoop/
// TIntro, PMOR_V2.cpp L3668–3836) are gone in v3 — the bring-up they served
// is finished — so pmor_probe_set() is a no-op that only answers the CDC
// line. pmor_audio_ctl() (L3817–3820) still stops/starts the audio engine;
// pmor_sine_set() (L133–135) keeps the USB-capture test-tone flag (unused:
// USB audio is off, g_en_usbaudio=false in v2).
#include "hw.h"
#include "hw_priv.h"
#include "daisy_seed.h"
using namespace daisy;

volatile bool g_usb_sine = false;
extern "C" void pmor_sine_set(int on) { g_usb_sine = (on != 0); }

extern "C" void pmor_probe_set(const char* what, int on) { (void)what; (void)on; }

extern "C" void pmor_audio_ctl(int on) {
    if(on) hwRestartAudio();
    else   hw.StopAudio();
}
