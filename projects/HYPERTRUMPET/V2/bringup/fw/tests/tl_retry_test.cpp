// Hot-plug test of the Trellis init scheduling. Build: c++ -std=c++14 -I. tests/tl_retry_test.cpp -o /tmp/tl_retry_test (from bringup/fw)
#include <stdio.h>
#include "tl_retry.h"
static int fails = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while(0)
int main() {
    TlRetry t; uint32_t now = 0, plug = 20000, found = 0; uint32_t probes = 0, lastProbe = 0, maxGap = 0; bool redrawSeen = false, ok = false;
    for(now = 0; now < 60000 && !ok; now++) {
        if(!t.due(now)) continue;
        probes++; if(lastProbe && now - lastProbe > maxGap) maxGap = now - lastProbe; lastProbe = now;
        ok = now >= plug;                                           // absent until 20 s, then present
        t.result(now, ok); if(ok) { found = now; redrawSeen = t.redraw; }
    }
    CHECK(ok, "an absent Trellis is found after it appears");
    CHECK(found - plug <= 5000, "found within 5 s of power returning (backoff capped at 5 s)");
    CHECK(maxGap <= 5000, "probe interval never above 5 s");
    CHECK(probes < 30, "absent for 20 s costs fewer than 30 probes (backoff, not a busy retry)");
    CHECK(redrawSeen && t.successes == 1, "success schedules a full LED redraw and exactly one re-init");
    TlRetry u; for(uint32_t n = 0; n < 3000; n += 150) { if(u.due(n)) u.result(n, false); } CHECK(u.backoff == 150, "first 3 s of uptime: 150 ms retries (seesaw power-up)");
    for(uint32_t n = 3000; n < 20000; n++) if(u.due(n)) u.result(n, false); CHECK(u.backoff == 5000, "long absence: backoff reaches the 5 s cap");
    return fails;
}
