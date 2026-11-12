// "Wait for first hit" recording: REC armed on an empty lane -> the first pad hit is step 0 of the loop.
// Own clock: the transport restarts at the hit (hit = exact downbeat). Followed / other lanes playing: the hit snaps to the nearest 16th of the
// existing grid and the lane keeps a start offset. Non-empty lane: overdub exactly as before. Clearing returns to waiting.
// (The followed grid over many bars is in test_follower.cpp "first-hit follow".)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <initializer_list>
#include "clock.h"
#include "sequencer.h"
#include "shared_state.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
static uint32_t T = 0;
static int blocks(int n) { int steps = 0; for (int i = 0; i < n; i++, T += BLOCK) steps += clockTick(T); return steps; }
static void boot(bool followed) {
    memset(&g, 0, sizeof g); clockInit(120.f); seqInit(); T = 0;
    seq.lane[ENG_DRUMS].mode_f = (float)SEQ_PLAY; seq.lane[ENG_DRUMS].mode = SEQ_PLAY; seq.lane[ENG_DRUMS].rec_on = 1.f;
    clockSetLoopLen(seqLongestLoop()); seqSetGridFollowed(followed);
    blocks(300);
}
int main() {
    const double S = SR * 15.0 / 120.0;   // 3000 samples per 16th at 120 BPM
    {   // standalone: the first hit restarts the transport; step 0 fires at the hit and every 16th after is exact
        boot(false);
        EXPECT(seqWaitingFirstHit(ENG_DRUMS), "REC armed on an empty lane waits for the first hit");
        blocks(7);
        uint32_t press = T - 40;   // arbitrary instant, not on the grid, already in the past like a pad stamp
        seqRecordHit(ENG_DRUMS, 5, press);
        EXPECT(seq.lane[ENG_DRUMS].step[0] == (1u << 5) && seqStartOffset(ENG_DRUMS) == 0 && !seqWaitingFirstHit(ENG_DRUMS), "first hit = step 0, offset 0, no longer waiting");
        uint32_t fired_at = 0; uint32_t gs = 99;
        for (int i = 0; i < 4 && !fired_at; i++) { uint32_t nb = clk.next_step_whole; (void)nb; int f = clockTick(T); T += BLOCK; if (f && clk.gstep == 0) { fired_at = 1; gs = clk.gstep; } }
        double err = (double)(int32_t)(clk.next_step_whole - press) + clk.next_step_frac / 4294967296.0 - S;
        EXPECT(fired_at && gs == 0, "transport restarted: step 0 fired (gstep %u)", gs);
        EXPECT(fabs(err) < 1.5, "the next boundary is exactly one 16th after the hit (error %.3f samples)", err);
    }
    {   // followed grid: grid untouched, hit snapped to the nearest 16th, lane offset set
        for (int frac : {10, 40, 60, 90}) {
            boot(true);
            uint32_t nbw = clk.next_step_whole; uint32_t gsb = clk.gstep;       // grid state before
            uint32_t press = nbw + (uint32_t)(S * frac / 100.0) + (uint32_t)(S * 3);   // 3 boundaries ahead + frac of a step past the boundary
            // boundary index of the last boundary at/before press: gsb+1 is at nbw
            long j = (long)gsb + 1 + 3 + (frac >= 50 ? 1 : 0);                     // nearest boundary
            seqRecordHit(ENG_DRUMS, 2, press);
            long want = j % 16;
            EXPECT(!clk.phase_reanchor_pending, "followed: the clock is NOT re-anchored (%d%% into a 16th)", frac);
            EXPECT(seq.lane[ENG_DRUMS].step[0] == (1u << 2) && seqStartOffset(ENG_DRUMS) == (uint16_t)want, "followed %d%% into a 16th: offset %u (want %ld), recorded on lane step 0", frac, seqStartOffset(ENG_DRUMS), want);
            // lane step 0 plays on every global step == offset (mod 16) and on no other
            bool ok = true; for (uint32_t gst = 0; gst < 64; gst++) { uint16_t m = seqFire(ENG_DRUMS, gst, 0, 0); if (((m & 4) != 0) != ((gst % 16) == (uint32_t)want)) ok = false; }
            EXPECT(ok, "lane plays on global steps = offset mod 16 only");
            // a following hit records relative to the first one
            seqRecordHit(ENG_DRUMS, 2, press + (uint32_t)(4 * S));
            EXPECT((seq.lane[ENG_DRUMS].step[4] & 4) != 0, "second hit 4 steps later lands on lane step 4");
        }
    }
    {   // other lanes playing (own clock): the transport is not restarted, the lane snaps with an offset
        boot(false); seq.lane[ENG_SYNTH].step[3] = 1;
        seqRecordHit(ENG_DRUMS, 1, T + 100);
        EXPECT(!clk.phase_reanchor_pending, "another lane has content: no transport restart");
        EXPECT((seq.lane[ENG_DRUMS].step[0] & 2) != 0, "still recorded as step 0");
    }
    {   // non-empty lane: overdub is the old behaviour (nearest boundary, offset 0, no re-anchor)
        boot(false); seq.lane[ENG_DRUMS].step[7] = 1;
        uint32_t nbw = clk.next_step_whole; uint32_t gsb = clk.gstep;
        uint32_t press = nbw + (uint32_t)(S * 2) + 100;                         // just after boundary gsb+3
        seqRecordHit(ENG_DRUMS, 6, press);
        long j = ((long)gsb + 3) % 16;
        EXPECT(!clk.phase_reanchor_pending && seqStartOffset(ENG_DRUMS) == 0, "overdub: no restart, offset 0");
        EXPECT((seq.lane[ENG_DRUMS].step[j] & 64) != 0 && (seq.lane[ENG_DRUMS].step[7] & 1) != 0, "overdub lands on the nearest boundary step %ld, old content kept", j);
    }
    {   // clearing returns to waiting; offset reset
        boot(true); seqRecordHit(ENG_DRUMS, 0, clk.next_step_whole + 5000);
        EXPECT(!seqWaitingFirstHit(ENG_DRUMS), "recorded -> not waiting");
        seqClear(ENG_DRUMS);
        EXPECT(seqWaitingFirstHit(ENG_DRUMS) && seqStartOffset(ENG_DRUMS) == 0, "cleared -> waits again, offset 0");
        seq.lane[ENG_DRUMS].rec_on = 0.f;
        EXPECT(!seqWaitingFirstHit(ENG_DRUMS), "REC off -> not waiting");
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
