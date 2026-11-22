// test_mapping.cpp -- scripted-gesture tests for HxMapper (mapping/fx_mapping.cpp). Spec: HYPERTRUMPET_V2/docs/FX_MAP.md.
// Build + run from HYPER_CORE/src/dsp/fx:
//   mkdir -p out && c++ -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
//       tests/test_mapping.cpp mapping/fx_mapping.cpp fx_engine.cpp fx_registry.cpp -o out/test_mapping && ./out/test_mapping
// (This file is test_mapping.cpp, not test_fx_*.cpp, so `make test` does not pick it up; `make engine` style rule is not needed.)
#include "../mapping/fx_mapping.h"
#include "fx_testutil.h"
using namespace hx;

static std::vector<uint8_t> g_mem((size_t)HX_NSLOTS * HX_SLOT_ARENA_BYTES, 0xCD);
static const float DT = 0.005f;   // 200 Hz control rate

struct Rig {
    FxEngine eng; HxMapper map; HxControls c;
    Rig() { eng.init(48000.f, g_mem.data(), g_mem.size()); map.init(&eng); }
    void run(float secs) { int n = (int)(secs / DT + 0.5f); for (int i = 0; i < n; i++) { map.update(c, DT); c.encDelta[0] = c.encDelta[1] = 0; } }
    void step() { map.update(c, DT); c.encDelta[0] = c.encDelta[1] = 0; }
    void pad(int n, int steps = 2) { c.pads |= (uint16_t)(1u << n); for (int i = 0; i < steps; i++) step(); c.pads &= (uint16_t)~(1u << n); step(); }
    Fx* fx(int s) { return eng.fx(s); }
    float P(int s, const char* nm) { Fx* f = fx(s); int i = f ? f->findParam(nm) : -1; return i < 0 ? -999.f : f->v[i]; }
};
static int T(const char* id) { return fxTypeById(id); }
static int lum(const HxLeds& l, int p) { return l.rgb[p][0] + l.rgb[p][1] + l.rgb[p][2]; }

int main() {
    // ---------------------------------------------------------------- default patch + status
    {
        Rig r;
        HX_CHECK(r.eng.effectType(0) == T("harm") && r.eng.effectType(1) == T("grain") && r.eng.effectType(2) == T("reverb") && r.eng.effectType(3) == T("delay"),
                 "default patch is harm/grain/reverb/delay, got %d %d %d %d", r.eng.effectType(0), r.eng.effectType(1), r.eng.effectType(2), r.eng.effectType(3));
        HX_CHECK(r.map.page() == 0 && r.map.selectedSlot() == 0, "start on PLAY page, slot 0");
        r.run(0.1f);
        int nz = 0; for (int p = 0; p < 16; p++) nz += lum(r.map.leds(), p) > 0;
        HX_CHECK(nz >= 12, "LEDs set on PLAY page (%d lit)", nz);
    }
    // ---------------------------------------------------------------- smoothing converges (fader -> MIX), rate aware
    {
        Rig r; r.c.fader[1] = 0.8f; r.step();
        float first = r.P(1, "MIX");
        r.run(1.0f);
        float fin = r.P(1, "MIX");
        HX_CHECK(fabsf(fin - 0.8f) < 0.02f, "fader 0.8 -> grain MIX converges (%.3f)", fin);
        HX_CHECK(first > 0.f && first <= 0.8f, "first update value within range (%.3f)", first);
        // rate-aware: same wall time at 1 kHz control rate lands in the same place
        Rig r2; r2.c.fader[1] = 0.8f; for (int i = 0; i < 100; i++) r2.map.update(r2.c, 0.001f);      // 0.1 s
        Rig r3; r3.c.fader[1] = 0.8f; for (int i = 0; i < 20; i++) r3.map.update(r3.c, 0.005f);       // 0.1 s
        HX_CHECK(fabsf(r2.P(1, "MIX") - r3.P(1, "MIX")) < 0.02f, "smoothing is dt-aware (%.3f vs %.3f)", r2.P(1, "MIX"), r3.P(1, "MIX"));
    }
    // ---------------------------------------------------------------- tilt, lidar, FSR, pot -> roles on the right slots
    {
        Rig r; r.run(0.05f);
        r.c.tiltX = 1.f; r.run(1.f);
        HX_CHECK(fabsf(r.P(2, "WARP") - 1.f) < 0.03f, "tiltX=1 saturates reverb WARP (%.3f)", r.P(2, "WARP"));
        r.c.tiltX = 0.f; r.run(1.f);
        HX_CHECK(fabsf(r.P(2, "WARP") - 0.5f) < 0.03f, "tiltX=0 centres WARP (%.3f)", r.P(2, "WARP"));
        // FSR -> S4 AMT (delay MIX), fast attack slow release
        r.c.fsr = 1.f; r.run(0.3f);
        HX_CHECK(r.P(3, "MIX") > 0.95f, "FSR 1 -> delay MIX high (%.3f)", r.P(3, "MIX"));
        r.c.fsr = 0.f; r.run(0.03f);
        float rel30 = r.P(3, "MIX");
        HX_CHECK(rel30 > 0.5f && rel30 < 0.95f, "FSR release is slower than attack (%.3f at 30 ms)", rel30);
        r.run(1.5f);
        HX_CHECK(r.P(3, "MIX") < 0.03f, "FSR 0 -> delay MIX back to ~0 (%.3f)", r.P(3, "MIX"));
        // lidar close = dark (TONE low), far = open (TONE high)
        Fx* d = r.fx(3); int ti = d ? d->findParam("TONE") : -1;
        if (ti >= 0) {
            r.c.lidar = 0.f; r.run(1.f); float open = d->v[ti];
            r.c.lidar = 1.f; r.run(1.f); float dark = d->v[ti];
            HX_CHECK(open > dark, "lidar close darkens delay TONE (%.3f far vs %.3f close)", open, dark);
        } else HX_CHECK(false, "delay has no TONE param");
        // pot1 -> S4 TIME
        Fx* dd = r.fx(3); int tm = dd ? dd->findParam("TIME") : -1;
        if (tm >= 0) { r.c.pot[0] = 0.f; r.run(0.5f); float lo = dd->v[tm]; r.c.pot[0] = 1.f; r.run(0.5f); float hi = dd->v[tm];
            HX_CHECK(hi > lo, "pot1 raises delay TIME (%.3f -> %.3f)", lo, hi); }
    }
    // ---------------------------------------------------------------- pads: latch bypass, per-slot; hold vs trigger LED state
    {
        Rig r; r.run(0.05f);
        HX_CHECK(!r.eng.bypassed(1), "slot 2 starts active");
        r.pad(1);
        HX_CHECK(r.eng.bypassed(1), "pad 1 latches slot 2 bypass ON");
        HX_CHECK(!r.eng.bypassed(0) && !r.eng.bypassed(2), "only slot 2 bypassed");
        int dimmed = lum(r.map.leds(), 1);
        r.pad(1);
        HX_CHECK(!r.eng.bypassed(1), "second press releases the bypass latch");
        HX_CHECK(lum(r.map.leds(), 1) > dimmed, "bypassed pad LED is dimmer than active (%d vs %d)", dimmed, lum(r.map.leds(), 1));
        // hold-type MAIN on the delay (S4, pad 7): LED bright only while held; trigger-type MAIN on grain (pad 5) is not held
        r.c.pads = (uint16_t)(1u << 7); r.run(0.01f);
        int heldL = lum(r.map.leds(), 7);
        r.c.pads = 0; r.run(0.8f);
        int relL = lum(r.map.leds(), 7);
        HX_CHECK(heldL > relL + 100, "hold pad LED bright while held (%d) then dim after release (%d)", heldL, relL);
        // grain MAIN cycles window: trigger fires once per press, not on release (grain action() has no observable here; check no crash + LED flash decays)
        r.pad(5); r.run(0.8f);
        HX_CHECK(lum(r.map.leds(), 5) < 120, "trigger pad returns to idle brightness");
        // row 3 col 0 is the overlay key, others are CLEAR: pressing CLEAR must not toggle bypass
        r.pad(14); HX_CHECK(!r.eng.bypassed(2), "CLEAR pad does not bypass");
    }
    // ---------------------------------------------------------------- joystick latch (v1: click enables), effect-aware pitch centre
    {
        Rig r; r.run(0.05f);
        int iv = r.fx(0) ? r.fx(0)->findParam("INTVL") : -1;
        if (iv >= 0) {
            float base = r.fx(0)->v[iv];
            r.c.joyX = 1.f; r.run(0.3f);
            HX_CHECK(r.fx(0)->v[iv] == base, "stick ignored until engaged (INTVL %.1f)", r.fx(0)->v[iv]);
            r.c.joyClick = true; r.step(); r.c.joyClick = false; r.run(0.3f);
            HX_CHECK(r.fx(0)->v[iv] >= 12.f, "engaged: stick full right -> top of ladder (INTVL %.1f)", r.fx(0)->v[iv]);
            r.c.joyX = 0.f; r.run(0.3f);
            HX_CHECK(fabsf(r.fx(0)->v[iv] - 5.f) < 0.51f, "stick centre = harmonizer unison idx 5 (INTVL %.1f)", r.fx(0)->v[iv]);
            r.c.joyX = -1.f; r.run(0.3f);
            HX_CHECK(r.fx(0)->v[iv] < 0.51f, "stick full left -> bottom of ladder (INTVL %.1f)", r.fx(0)->v[iv]);
            // MAIN pad on S1 resets to unison (harm A_MAIN) and the stick does not fight it while still
            r.c.joyX = 1.f; r.run(0.3f); r.c.joyX = 1.f;
            r.pad(4); r.run(0.2f);
            HX_CHECK(fabsf(r.fx(0)->v[iv] - 5.f) < 0.01f, "MAIN pad -> harmonizer reset to unison (INTVL %.1f)", r.fx(0)->v[iv]);
            r.c.joyClick = true; r.step(); r.c.joyClick = false;   // disengage
            r.c.joyX = -1.f; r.run(0.3f);
            HX_CHECK(fabsf(r.fx(0)->v[iv] - 5.f) < 0.01f, "disengaged stick is ignored again");
        } else HX_CHECK(false, "harm has no INTVL param");
    }
    // ---------------------------------------------------------------- half-valve smear -> S1 MOD, quick valve strokes ignored
    {
        Rig r; r.run(0.05f);
        int dt = r.fx(0) ? r.fx(0)->findParam("DETUNE") : -1;
        if (dt >= 0) {
            for (int k = 0; k < 40; k++) { r.c.valve[0] = (k & 1) ? 1.f : 0.f; r.run(0.02f); }   // fast full strokes
            float fast = r.fx(0)->v[dt];
            r.c.valve[0] = 0.5f; r.run(0.6f);
            float half = r.fx(0)->v[dt];
            r.c.valve[0] = 1.f; r.run(0.6f);
            float full = r.fx(0)->v[dt];
            HX_CHECK(half > 20.f && half > fast + 5.f, "half-valve dwell raises DETUNE (%.1f vs fast strokes %.1f)", half, fast);
            HX_CHECK(full < 2.f, "fully pressed valve releases DETUNE (%.1f)", full);
        } else HX_CHECK(false, "harm has no DETUNE param");
    }
    // ---------------------------------------------------------------- encoders: slot select, browse (hold + turn, commit on release), bypass click, param edit/reset
    {
        Rig r; r.run(0.05f);
        r.c.encDelta[0] = 1; r.step();
        HX_CHECK(r.map.selectedSlot() == 1, "enc1 turn selects slot 2");
        r.c.encDelta[0] = -2; r.step();
        HX_CHECK(r.map.selectedSlot() == 3, "enc1 turn wraps (slot 4)");
        r.map.selectSlot(1);
        int was = r.eng.effectType(1);
        r.c.encClick[0] = true; r.step();
        r.c.encDelta[0] = 1; r.step();
        HX_CHECK(r.eng.effectType(1) == was, "browse is staged: engine unchanged while enc1 is held");
        r.c.encClick[0] = false; r.step();
        HX_CHECK(r.eng.effectType(1) == (was + 1) % fxTypeCount(), "enc1 hold+turn+release commits next effect (%d -> %d)", was, r.eng.effectType(1));
        HX_CHECK(r.map.selectedSlot() == 1, "browse keeps the focus slot");
        // short click toggles focus-slot bypass
        bool b0 = r.eng.bypassed(1);
        r.c.encClick[0] = true; r.step(); r.c.encClick[0] = false; r.step();
        HX_CHECK(r.eng.bypassed(1) != b0, "enc1 short click toggles bypass");
        // enc2: edit + reset (use slot 2 = reverb)
        r.map.selectSlot(2);
        Fx* f = r.fx(2); int sp = r.map.selectedParam();
        HX_CHECK(f && sp >= 0 && sp < f->np, "selected param valid (%d)", sp);
        if (f) {
            // pick SIZE (not fader/role driven) with enc2 hold+turn
            int sz = f->findParam("SIZE");
            r.c.encClick[1] = true; r.step();
            for (int k = 0; k < f->np && sz >= 0 && r.map.selectedParam() != sz; k++) { r.c.encDelta[1] = 1; r.step(); }
            r.c.encClick[1] = false; r.step();
            sp = r.map.selectedParam();
            HX_CHECK(sz >= 0 && sp == sz, "enc2 hold+turn reached SIZE (%d)", sp);
            float v0 = f->v[sp];
            r.c.encDelta[1] = 4; r.step();
            float v1 = f->v[sp];
            HX_CHECK(v1 != v0, "enc2 turn edits the selected param (%.3f -> %.3f)", v0, v1);
            r.c.encClick[1] = true; r.step(); r.c.encClick[1] = false; r.step();
            HX_CHECK(fabsf(f->v[sp] - v0) < 1e-4f, "enc2 short click resets the param to its default (%.3f)", f->v[sp]);
            int p0 = r.map.selectedParam();
            r.c.encClick[1] = true; r.step(); r.c.encDelta[1] = 1; r.step(); r.c.encClick[1] = false; r.step();
            HX_CHECK(r.map.selectedParam() != p0, "enc2 hold+turn picks another param (%d -> %d)", p0, r.map.selectedParam());
        }
    }
    // ---------------------------------------------------------------- BANK overlay page (hold pad 12): banks, prev/next effect, slot select, dry, reset
    {
        Rig r; r.run(0.05f);
        r.c.pads = (uint16_t)(1u << 12); r.run(0.05f);
        HX_CHECK(r.map.page() == 1, "holding pad 12 shows the BANK page");
        HX_CHECK(lum(r.map.leds(), 0) > 3 * lum(r.map.leds(), 1), "current bank pad is the brightest on row 0");
        r.c.pads = (uint16_t)((1u << 12) | (1u << 1)); r.run(0.02f); r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
        HX_CHECK(r.eng.effectType(0) == T("mood") && r.eng.effectType(1) == T("freeze") && r.eng.effectType(2) == T("blur") && r.eng.effectType(3) == T("flange"),
                 "bank B loaded (mood/freeze/blur/flange)");
        // select slot 3 then next effect
        r.c.pads = (uint16_t)((1u << 12) | (1u << 10)); r.run(0.02f); r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
        HX_CHECK(r.map.selectedSlot() == 2, "overlay row 2 selects focus slot");
        int was = r.eng.effectType(2);
        r.c.pads = (uint16_t)((1u << 12) | (1u << 5)); r.run(0.02f); r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
        HX_CHECK(r.eng.effectType(2) == (was + 1) % fxTypeCount(), "overlay next-effect pad advances the focus slot");
        // DRY toggles all bypass and restores
        r.c.pads = (uint16_t)((1u << 12) | (1u << 13)); r.run(0.02f); r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
        HX_CHECK(r.eng.bypassed(0) && r.eng.bypassed(1) && r.eng.bypassed(2) && r.eng.bypassed(3), "DRY bypasses every slot");
        r.c.pads = (uint16_t)((1u << 12) | (1u << 13)); r.run(0.02f); r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
        HX_CHECK(!r.eng.bypassed(0) && !r.eng.bypassed(3), "DRY again restores");
        // hold pad 15 one second -> default patch
        r.c.pads = (uint16_t)((1u << 12) | (1u << 15)); r.run(1.1f);
        HX_CHECK(r.eng.effectType(0) == T("harm") && r.eng.effectType(3) == T("delay"), "1 s hold on pad 15 reloads the default patch");
        r.c.pads = 0; r.run(0.05f);
        HX_CHECK(r.map.page() == 0, "release returns to PLAY");
    }
    // ---------------------------------------------------------------- shake gesture + effect-type dependent actions do not crash
    {
        Rig r; r.run(0.05f);
        r.c.shake = 1.f; r.run(0.3f); r.c.shake = 0.f; r.run(0.1f);
        r.c.shake = 1.f; r.run(2.5f); r.c.shake = 0.f;
        HX_CHECK(true, "shake ok");
    }
    // ---------------------------------------------------------------- full-rig soak: audio + random gestures, no NaN, bounded output
    {
        Rig r; hxt::Stats st; uint32_t seed = 4242; long tt = 0; float in[48], L[48], R[48];
        for (int bank = 0; bank < 4; bank++) {
            r.c.pads = (uint16_t)(1u << 12); r.run(0.02f);
            r.c.pads = (uint16_t)((1u << 12) | (1u << bank)); r.run(0.02f); r.c.pads = 0; r.run(0.02f);
            for (int blk = 0; blk < 800; blk++) {
                for (int i = 0; i < 48; i++, tt++) in[i] = 0.35f * sinf(6.2831853f * 311.f * tt / 48000.f) * (0.5f + 0.5f * sinf(tt * 0.00007f));
                if (blk % 8 == 0) {   // control tick (~6 ms)
                    r.c.valve[0] = rnd01(seed); r.c.valve[1] = rnd01(seed) > 0.5f; r.c.valve[2] = rnd01(seed);
                    for (int k = 0; k < 3; k++) r.c.fader[k] = rnd01(seed);
                    r.c.pot[0] = rnd01(seed); r.c.pot[1] = rnd01(seed); r.c.fsr = rnd01(seed);
                    r.c.joyX = rnd01(seed) * 2.f - 1.f; r.c.joyY = rnd01(seed) * 2.f - 1.f; r.c.tiltX = rnd01(seed) * 2.f - 1.f; r.c.tiltY = rnd01(seed) * 2.f - 1.f;
                    r.c.lidar = rnd01(seed); r.c.shake = rnd01(seed);
                    r.c.pads = (uint16_t)((rnd01(seed) > 0.8f ? (1u << (int)(rnd01(seed) * 15.99f)) : 0u) & ~(1u << 12) & ~(1u << 15));
                    r.c.joyClick = rnd01(seed) > 0.97f;
                    r.map.update(r.c, 0.006f);
                }
                r.eng.process(in, L, R, 48); hxt::scan(st, L, 48); hxt::scan(st, R, 48);
            }
        }
        HX_CHECK(!st.bad, "NaN/inf in output during the soak");
        HX_CHECK(st.peak < 40.f, "output peak %.2f during the soak", st.peak);
        // hostile inputs: NaN/inf/out-of-range, dt extremes
        r.c.fader[0] = NAN; r.c.fsr = INFINITY; r.c.tiltX = -INFINITY; r.c.lidar = 5.f; r.c.joyX = NAN; r.c.pot[0] = -3.f;
        r.map.update(r.c, 0.f); r.map.update(r.c, 10.f); r.map.update(r.c, NAN);
        bool fin = true;
        for (int s = 0; s < HX_NSLOTS; s++) { Fx* f = r.fx(s); if (f) for (int i = 0; i < f->np; i++) if (!(f->v[i] == f->v[i]) || fabsf(f->v[i]) > 1e9f) fin = false; }
        HX_CHECK(fin, "no NaN in any effect param after hostile inputs");
        int lit = 0; for (int p = 0; p < 16; p++) lit += lum(r.map.leds(), p) > 0;
        HX_CHECK(lit > 0, "LED frame set after hostile inputs");
    }
    printf("[mapping] %s\n", hxt::failCount() ? "FAIL" : "PASS");
    return hxt::failCount() ? 1 : 0;
}
