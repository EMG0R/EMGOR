// test_boottempo.cpp -- pmor_host.h: HELLO `booting=1` (wait + re-HELLO, never fail) and TEMPO <bpm_milli> (validation, READY only, resend after resync, old firmware ignored). Uses fakepmor.h.
#include <stdio.h>
#include "fakepmor.h"
static int pass = 0, fail = 0;
#define CHECK(c, ...) do { if (c) pass++; else { fail++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    {   // boot: 3 HELLOs answer booting=1 (~3 s), the client keeps retrying and then syncs
        FakePmor d; pmorhost::Client C; FakeLink L(d, C); d.add(1100, 2000, 1); d.bootLeft = 3; int synced = 0; C.onSynced = [&]() { synced++; };
        C.connect("STUDIO"); L.run(2200);
        CHECK(C.state() == pmorhost::Client::HELLO && C.wantsConnection(), "still waiting, not failed (state %s)", C.stateName());
        CHECK(L.until([&] { return C.state() == pmorhost::Client::READY; }, 6000) && synced == 1, "READY after the boot finished");
        CHECK(!L.info("HELLO refused") && !L.info("timeout") && L.info("booting"), "no failure reported");
        int hellos = 0; for (auto& c : d.cmds) if (c.rfind("HELLO", 0) == 0) hellos++; CHECK(hellos == 4, "HELLO retried about once a second (%d)", hellos);
    }
    {   // a GET_* answered `ERR booting` mid-sync is retried
        FakePmor d; pmorhost::Client C; FakeLink L(d, C); d.add(1100, 2000, 1);
        C.connect("STUDIO"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 4000); d.bootLeft = 1; d.mode = "STUDIO"; C.getPatch(); L.run(300);
        d.bootLeft = 0; d.mode = "STUDIO"; L.run(1500); CHECK(C.slotsOk(), "GET_PATCH retried after `ERR booting`");
    }
    {   // tempo
        FakePmor d; pmorhost::Client C; FakeLink L(d, C);
        C.setTempo(120); CHECK(d.tempos.empty(), "not READY: nothing sent");
        C.connect("STUDIO"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 4000); L.run(100);
        CHECK(d.tempos.size() == 1 && d.tempos[0] == 120000, "tempo stored before READY is sent when READY (%zu)", d.tempos.size());
        C.setTempo(123.4567); L.run(100); CHECK(d.tempos.back() == 123457, "round(bpm*1000) (%ld)", d.tempos.back());
        size_t n = d.tempos.size(); C.setTempo(123.4567); L.run(50); CHECK(d.tempos.size() == n, "same value not resent");
        C.setTempo(39.9); C.setTempo(400.1); C.setTempo(0); C.setTempo(-5); C.setTempo(0.0 / 0.0); L.run(100);
        CHECK(d.tempos.size() == n && C.tempoMilli() == 123457 && !L.info("TEMPO"), "out-of-range / NaN ignored silently, last valid kept");
        C.setTempo(40); C.setTempo(400); L.run(100); CHECK(d.tempos.size() == n + 2 && d.tempos[n] == 40000 && d.tempos[n + 1] == 400000, "limits 40 and 400 accepted");
        C.setTempo(98.5); L.run(100);
        C.portClosed(); d.tempos.clear(); C.connect("EDIT"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 4000); L.run(100);
        CHECK(d.tempos.size() == 1 && d.tempos[0] == 98500, "last tempo resent after the resync (%zu)", d.tempos.size());
    }
    {   // old firmware
        FakePmor d; pmorhost::Client C; FakeLink L(d, C); d.hasTempo = false; C.connect("STUDIO"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 4000);
        size_t ni = L.infos.size(); C.setTempo(120); L.run(200);
        CHECK(C.state() == pmorhost::Client::READY && L.infos.size() == ni && C.slotsOk(), "ERR TEMPO unknown ignored, no state change, no info");
    }
    printf("test_boottempo: %d passed, %d failed\n", pass, fail); return fail ? 1 : 0;
}
