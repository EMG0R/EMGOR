// engine_util.cpp — compile-check TU for the header-only engine_util.h.
// Nothing here is called; it only guarantees the header builds on its own.
#include "engine_util.h"

float engineUtilSelfTest(float x) {
    static uint32_t s = 1u;
    OnePole op; op.setMs(5.f);
    ADSR    env; env.setTimes(0.01f, 0.1f, 0.5f, 0.2f); env.retrigger(); env.scaleTimes(1.5f);
    Env     pk;  pk.setReleaseMs(150.f);
    return op.process(x) + env.process() + pk.process(x) + fastExp(x) + lcg01(s);
}
