// FX automation over MIDI: record a loop -> emitted CC stream matches; inject CCs -> FX state follows; echo -> no double apply / re-record.
#include <stdio.h>
#include <string.h>
#include <vector>
#include "fx.h"
#include "fx_midi.h"
#include "clock.h"
#include "shared_state.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "output.h"
#include "metro.h"
#include "params.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
struct Cc { uint32_t t; uint8_t ch, cc, v; };
static std::vector<Cc> out; static uint32_t T;
static void sink(uint8_t ch, uint8_t cc, uint8_t v) { out.push_back({T, ch, cc, v}); }
static void boot() {
    memset(&g, 0, sizeof g); clockInit(120.f); clockSetLoopLen(16); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit(); pmorSchemaInit();
    fxMidiReset(); out.clear(); T = 1000;
}
static void step(uint32_t gs) { clk.gstep = gs; uint16_t m[ENG_COUNT] = {0}; fxOnStep(gs, m); }
static int count(uint8_t ch, uint8_t cc, int v = -1) { int n = 0; for (auto& e : out) if (e.ch == ch && e.cc == cc && (v < 0 || e.v == v)) n++; return n; }
int main() {
    const uint8_t REV = 4, CHOR = 5;   // bank 0 pads (ids 4, 5)
    {   boot(); printf("[record on PMOR, play back, emit]\n");
        fxMidiPoll(T, sink);                                          // arm baseline
        fx.auto_rec = true;
        for (uint32_t gs = 0; gs < 16; gs++) {                        // hold REV on steps 2..5, CHOR on 8..9
            if (gs == 2) fxPress(REV); if (gs == 6) fxRelease(REV);
            if (gs == 8) fxPress(CHOR); if (gs == 10) fxRelease(CHOR);
            step(gs); T += 125; fxMidiPoll(T, sink);
        }
        fx.auto_rec = false; fxReleaseAll();
        int liveOn = count(4, REV, 127), liveOff = count(4, REV, 0);
        EXPECT(liveOn == 1 && liveOff == 1 && count(4, CHOR, 127) == 1, "live play emitted engage edges once each (REV %d/%d, CHOR on %d)", liveOn, liveOff, count(4, CHOR, 127));
        out.clear();
        for (uint32_t gs = 0; gs < 16; gs++) { clk.gstep = gs; T += 125; fxMidiPoll(T, sink); }   // loop playback: nothing held
        bool ok = count(4, REV, 127) == 1 && count(4, REV, 0) == 1 && count(4, CHOR, 127) == 1 && count(4, CHOR, 0) == 1;
        uint32_t onAt = 0, offAt = 0; for (auto& e : out) if (e.cc == REV) { if (e.v) onAt = e.t; else offAt = e.t; }
        EXPECT(ok && offAt - onAt == 4 * 125, "loop playback emits the recorded edges (REV held %u ms = 4 steps)", (unsigned)(offAt - onAt));
        EXPECT(out.size() == 4, "and nothing else (%zu CCs)", out.size());
    }
    {   boot(); printf("[inject CCs: Live drives PMOR's FX]\n");
        fxMidiPoll(T, sink); out.clear();
        T += 500; fxMidiIn(T, 4, REV, 127); EXPECT(fxEffectiveBank(0) & (1u << REV), "CC ch5 %d=127 engages REV", REV);
        fxMidiPoll(T + 10, sink); EXPECT(out.empty(), "the received value is never re-sent");
        T += 500; fxMidiIn(T, 4, REV, 0); EXPECT(!(fxEffectiveBank(0) & (1u << REV)), "CC ch5 %d=0 releases it", REV);
        T += 500; fxMidiIn(T, 5, 1, 100); EXPECT(fxEffectiveFsr() > 0.78f && fxEffectiveFsr() < 0.8f, "ch6 CC1 sets the FX amount (%.3f)", fxEffectiveFsr());
        T += 500; fxMidiIn(T, 5, 16 + 1, 64); float v; pmorParamGet(pmorParamId(PD_FX, 131), &v); EXPECT(v > 0.5f && v < 0.51f, "ch6 CC17 sets bank 2 AMOUNT param (%.3f)", v);
        T += 500; fxMidiIn(T, 5, 32 + 2, 127); pmorParamGet(pmorParamId(PD_FX, 152), &v); EXPECT(v == 1.f, "ch6 CC34 sets bank 3 MIX (%.3f)", v);
        out.clear(); fxMidiPoll(T + 10, sink); EXPECT(out.empty(), "params set from outside are not echoed back (%zu CCs)", out.size());
        pmorParamSet(pmorParamId(PD_FX, 133), 0.25f); T += 500; fxMidiPoll(T, sink); EXPECT(count(5, 16 + 3, 32) == 1, "a SET_PARAM (bank 4 AMOUNT 0.25) is emitted as ch6 CC19 = 32");
    }
    {   boot(); printf("[echo: Live plays PMOR's own loop back]\n");
        fx.auto_enabled = true; fx.auto_mask[3][0] = 1u << REV; fx.auto_mask[4][0] = 1u << REV;
        fxMidiPoll(T, sink); out.clear();
        uint16_t am0[16], snap_n = 0; (void) am0; (void) snap_n;
        std::vector<Cc> sent; fx.auto_rec = true;                       // even with the record gesture armed: nothing recorded from MIDI
        for (uint32_t gs = 0; gs < 16; gs++) {
            clk.gstep = gs; T += 125; size_t n0 = out.size(); fxMidiPoll(T, sink);
            for (size_t i = n0; i < out.size(); i++) { T += 5; fxMidiIn(T, out[i].ch, out[i].cc, out[i].v); }   // Live echoes each CC 5 ms later
            uint16_t m[ENG_COUNT] = {0}; fxOnStep(gs, m);
        }
        EXPECT(count(4, REV, 127) == 1 && count(4, REV, 0) == 1 && out.size() == 2, "loop emitted once (on/off) and the echo added nothing (%zu CCs)", out.size());
        EXPECT(fx.ovr[0] == 0 && fx.ext[0] == 0, "echo did not take the FX over (ovr %04x ext %04x)", fx.ovr[0], fx.ext[0]);
        { bool none = true; for (int st = 0; st < 16; st++) if (fx.auto_mask[st][0] != 0) none = false; EXPECT(none, "record gesture armed: the MIDI-in state is never written into the loop"); }
        fx.auto_rec = false;
    }
    {   boot(); printf("[override: Live changes the FX while PMOR's loop plays]\n");
        fx.auto_enabled = true; for (int s = 0; s < 16; s++) fx.auto_mask[s][0] = 1u << REV;      // loop holds REV on for the whole bar
        clk.gstep = 0; fxMidiPoll(T, sink); out.clear();
        T += 1000; fxMidiIn(T, 4, REV, 0);                                                          // Live says OFF (genuinely different, long after our send)
        clk.gstep = 5; EXPECT(!(fxEffectiveBank(0) & (1u << REV)), "an outside OFF pauses PMOR's loop for that FX");
        fxMidiPoll(T + 50, sink); EXPECT(out.empty(), "and PMOR does not fight it by re-sending ON (%zu CCs)", out.size());
        fxPress(REV); fxRelease(REV); clk.gstep = 7; EXPECT((fxEffectiveBank(0) & (1u << REV)) != 0, "the next physical hold of that FX takes it back: the loop plays again");
    }
    {   boot(); printf("[rate limit]\n");
        fxMidiPoll(T, sink); out.clear();
        for (int ms = 0; ms < 1000; ms++) { T++; for (int id = 0; id < 128; id += 1) if (id % 3 == ms % 3) { if (ms & 1) fx.held[id >> 4] |= 1u << (id & 15); else fx.held[id >> 4] &= ~(1u << (id & 15)); } fxMidiPoll(T, sink); }
        EXPECT(out.size() <= 200 + 16 + 4, "flood of changes: %zu CCs in 1 s (<= ~200)", out.size());
    }
    {   boot(); printf("[perform state on ch6: tilt CC2, octave CC3+CC35 (14 bit), env CC4, throws CC8..11, engine CC12]\n");
        fxMidiPoll(T, sink); out.clear();
        fxMidiPoll(T += 10, sink); EXPECT(out.empty(), "baseline: nothing sent for the unchanged state");
        fx.tilt_t = 0.5f; syn.perf_octave = 1.f; syn.perf_env_macro = 1.f; fx.perf_up = 0.25f; fx.perf_right = 0.5f; fx.perf_down = 0.75f; fx.perf_left = 1.f; fx.perf_engine = 1;
        fxMidiPoll(T += 10, sink);
        EXPECT(count(5, 2) == 1 && count(5, 4, 127) == 1 && count(5, 8) == 1 && count(5, 9) == 1 && count(5, 10) == 1 && count(5, 11, 127) == 1 && count(5, 12, 1) == 1, "each perform CC sent once on change (%zu CCs)", out.size());
        { int msb = -1, lsb = -1; for (auto& e : out) { if (e.ch == 5 && e.cc == 3) msb = e.v; if (e.ch == 5 && e.cc == 35) lsb = e.v; }
          int o14 = (msb << 7) | lsb; EXPECT(msb >= 0 && lsb >= 0 && o14 == (int)(8192.f + 4095.5f + 0.5f), "octave +1 as 14 bit %d (want %d)", o14, (int)(8192.f + 4095.5f + 0.5f)); }
        out.clear(); fxMidiPoll(T += 10, sink); EXPECT(out.empty(), "unchanged state sends nothing");
        fx.perf_engine = 3; fxMidiPoll(T += 10, sink); EXPECT(count(5, 12, 127) == 1, "no engine = 127");
        out.clear(); fxMidiIn(T += 500, 5, 3, 0); fxMidiIn(T, 5, 35, 0); fxMidiIn(T, 5, 2, 0); fxMidiIn(T, 5, 12, 0); fxMidiPoll(T += 10, sink);
        EXPECT(out.empty() && fx.tilt_t == 0.5f && syn.perf_octave == 1.f, "host CCs 2/3/12/35 on ch6 are ignored by the firmware (no loopback writes)");
        syn.perf_octave = 0.f; fx.tilt_t = 0.f; syn.perf_env_macro = 0.f; fx.perf_up = fx.perf_right = fx.perf_down = fx.perf_left = 0.f; fx.perf_engine = 0;
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
