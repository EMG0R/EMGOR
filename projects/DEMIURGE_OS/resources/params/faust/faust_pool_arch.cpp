// faust_pool_arch.cpp -- minimal reference architecture: Faust's built-in OSC (OSCUI) in,
// /pout out. NO audio I/O (compute runs on a timer into a dummy buffer): used by the selftest
// and as a template for a real arch. Real deployments can instead use `faust2jackconsole -osc`
// and let the pool's osc sink talk to Faust's own addresses (README.md).
//
// Faust OSC receive:  -port N        (we pass DEMIURGE_PARAM_PORT)
// Reporting:          poll the zones and send /pout <pool path> <normalized value> on change.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <vector>
#include <lo/lo.h>
#include "faust/dsp/dsp.h"
#include "faust/gui/meta.h"
#include "faust/gui/UI.h"
#include "faust/gui/MapUI.h"
#include "faust/gui/OSCUI.h"
#include "faust/misc.h"

// Faust GUI statics (every Faust architecture defines these once)
std::list<GUI*> GUI::fGuiList;
ztimedmap GUI::gTimedZoneMap;

<<includeIntrinsic>>
<<includeclass>>

// MapUI + remember each slider's range (MapUI has no min/max getter)
struct RangeUI : public MapUI {
    std::map<std::string, std::pair<float, float>> range;
    void rec(const char* l, float lo, float hi) { range[buildPath(l)] = {lo, hi}; }
    void addHorizontalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT i, FAUSTFLOAT lo, FAUSTFLOAT hi, FAUSTFLOAT s) override { MapUI::addHorizontalSlider(l, z, i, lo, hi, s); rec(l, lo, hi); }
    void addVerticalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT i, FAUSTFLOAT lo, FAUSTFLOAT hi, FAUSTFLOAT s) override { MapUI::addVerticalSlider(l, z, i, lo, hi, s); rec(l, lo, hi); }
    void addNumEntry(const char* l, FAUSTFLOAT* z, FAUSTFLOAT i, FAUSTFLOAT lo, FAUSTFLOAT hi, FAUSTFLOAT s) override { MapUI::addNumEntry(l, z, i, lo, hi, s); rec(l, lo, hi); }
};

int main(int argc, char* argv[]) {
    const char* pp = getenv("DEMIURGE_PARAM_PORT");
    const char* ph = getenv("DEMIURGE_POOL_HOST");
    const char* pq = getenv("DEMIURGE_POOL_PORT");
    const char* stage = getenv("DEMIURGE_STAGE");
    std::string port = pp ? pp : "0";
    char a0[] = "faust", a1[] = "-port", a3[] = "-xmit", a4[] = "0";
    std::string sport = port;
    char* av[] = {a0, a1, (char*)sport.c_str(), a3, a4, nullptr};
    int ac = 5;
    mydsp d; d.init(48000);
    RangeUI map; d.buildUserInterface(&map);
    OSCUI osc((char*)"faust", ac, av); d.buildUserInterface(&osc); osc.run();
    lo_address pool = lo_address_new(ph ? ph : "127.0.0.1", pq ? pq : "9102");
    std::map<std::string, float> last;
    std::string st = stage ? stage : "test";
    // zeroed input buffers (effects with inputs segfault on nullptr) and a scratch buffer per output
    int nin = d.getNumInputs(), nout = d.getNumOutputs();
    std::vector<std::vector<float>> inb(nin > 0 ? nin : 1, std::vector<float>(64, 0.f));
    std::vector<std::vector<float>> outb(nout > 0 ? nout : 1, std::vector<float>(64, 0.f));
    std::vector<float*> ins, outs;
    for (int k = 0; k < nin; k++) ins.push_back(inb[k].data());
    for (int k = 0; k < nout; k++) outs.push_back(outb[k].data());
    for (;;) {
        d.compute(64, ins.empty() ? nullptr : ins.data(), outs.empty() ? nullptr : outs.data());
        for (int i = 0; i < map.getParamsCount(); i++) {
            std::string addr = map.getParamAddress(i);       // /dparam_example/level
            std::string name = addr.substr(addr.rfind('/') + 1);
            float v = map.getParamValue(addr);
            float lo = map.range[addr].first, hi = map.range[addr].second;
            float n = (hi > lo) ? (v - lo) / (hi - lo) : 0.f;
            if (!last.count(addr) || std::fabs(last[addr] - n) > 1e-6f) {
                last[addr] = n;
                lo_send(pool, "/pout", "sf", ("/" + st + "/" + name).c_str(), n);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
