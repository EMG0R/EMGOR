// TEST 12 control map: ONE fader drives the drone MIX (the fader's calibrated value g_fs: exact 0 at rest, exact 1.0 at the top). Nothing else is mapped.
//
// FADER GROUND-FAULT GUARD (2026-11-17): all three faders reading over 97 % at the same time, either within 2 s of boot or reached by a jump faster than 50 ms (a real hand cannot move three
// faders to the top at once), is treated as a wiring / ground fault, not as three faders at the top. While it holds, the drone mix is forced to 0 (st.ffault = 1); (telemetry only since 1.3: the drone is NEVER muted by it). `lin` is the linearised channel, 0..65535.
#pragma once
static const int DRONE_FADER = 2;                          // 1-based: 1 = A3, 2 = A4, 3 = A5 (the only constant to change to move the drone to another fader)
static const float FF_HI = 0.97f, FF_LO = 0.90f; static const uint32_t FF_BOOT_MS = 2000, FF_JUMP_MS = 50;
static uint32_t g_ffLastBelow = 0; static bool g_ffPrevAllHi = false;
static void fxControlsTick(const uint16_t* lin, uint32_t now) {
    float lo = 1.0f, hi = 1.0f; for(int i = 0; i < 3; i++) { const float x = (float)lin[i] / 65535.0f; if(x < lo) lo = x; if(x < hi) hi = x; }   // lo = the lowest of the three
    if(g_simFader[0] >= 0 || g_simFader[1] >= 0 || g_simFader[2] >= 0) { g_ffault = 0; g_ffPrevAllHi = false; drControls(g_fs[DRONE_FADER - 1]); return; }   // `sim fader` overrides the hardware (tests): the guard is for the real faders
    const bool allHi = lo > FF_HI, anyBelow = lo < FF_LO;
    if(anyBelow) { g_ffLastBelow = now; g_ffault = 0; }                                                                                  // seen below 90 %: the fault clears
    if(allHi && !g_ffPrevAllHi && !g_ffault) { if(now < FF_BOOT_MS || now - g_ffLastBelow < FF_JUMP_MS) g_ffault = 1; }               // all three at the top at boot, or reached in < 50 ms
    g_ffPrevAllHi = allHi;
    drControls(g_fs[DRONE_FADER - 1]);                                                                                                    // DETECT ONLY: st.ffault is telemetry, the fader is never muted because of it (faders up is the normal state)
}
