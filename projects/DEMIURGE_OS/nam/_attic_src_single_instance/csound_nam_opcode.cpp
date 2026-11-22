// csound_nam_opcode.cpp
//
// CSound external plugin — NAMProcess and NAMCount opcodes.
//
//   aoutL, aoutR  NAMProcess  ainL, ainR, kProfileIdx, SLibDir
//   icount        NAMCount
//
// NAMProcess loads a background thread (NamLoader) that watches kProfileIdx
// for changes and swaps .nam profiles without blocking the audio thread.

#include "nam_loader.h"

// CSound plugin API (csdl.h is compatible with CSound 6.18 / AppleClang 17+).
// plugin.h in 6.18 has a typo that breaks compilation; CSOUND_USE_CSDL_H is
// set by CMakeLists.txt on macOS to force this path.
#if defined(CSOUND_USE_CSDL_H)
#  include <csdl.h>
#elif defined(__has_include) && __has_include(<csound/csdl.h>)
#  include <csound/csdl.h>
#elif defined(__has_include) && __has_include(<csdl.h>)
#  include <csdl.h>
#else
#  include <csound/csdl.h>
#endif

#include <NAM/dsp.h>

#include <cstring>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Per-opcode instance data.
// Field order: outputs first, then inputs, then per-instance state.
// The OPDS header MUST be first.
// ---------------------------------------------------------------------------
struct NAMOpcode {
    OPDS      h;
    // --- outputs ---
    MYFLT*    aoutL;
    MYFLT*    aoutR;
    // --- inputs (match intypes "aakS" order) ---
    MYFLT*    ainL;
    MYFLT*    ainR;
    MYFLT*    kProfileIdx;
    STRINGDAT* SLibDir;
    // --- per-instance state (not part of OPDS signature) ---
    NamLoader* loader;
    int        last_idx;
};

// NAMCount reads the profile count from the shared loader.
struct NAMCountOpcode {
    OPDS  h;
    MYFLT* icount;  // output
    // no inputs
};

// ---------------------------------------------------------------------------
// Deinit callback
// ---------------------------------------------------------------------------
static int nam_deinit(CSOUND* /*cs*/, void* p) {
    NAMOpcode* op = reinterpret_cast<NAMOpcode*>(p);
    if (op->loader) {
        delete op->loader;
        op->loader = nullptr;
    }
    return CSOUND_SUCCESS;
}

// ---------------------------------------------------------------------------
// Loader pointer sharing via CSound global storage.
// NAMProcess stores its NamLoader* here so NAMCount can read it.
// ---------------------------------------------------------------------------
static void publish_loader_ptr(CSOUND* cs, NamLoader* loader) {
    void* slot = cs->QueryGlobalVariable(cs, "__nam_loader_ptr__");
    if (!slot) {
        cs->CreateGlobalVariable(cs, "__nam_loader_ptr__", sizeof(NamLoader*));
        slot = cs->QueryGlobalVariable(cs, "__nam_loader_ptr__");
    }
    if (slot) {
        *reinterpret_cast<NamLoader**>(slot) = loader;
    }
}

// ---------------------------------------------------------------------------
// NAMProcess — init
// ---------------------------------------------------------------------------
static int nam_init(CSOUND* cs, NAMOpcode* p) {
    p->loader   = nullptr;
    p->last_idx = -1;

    // Read lib dir from string input.  STRINGDAT::data is the char*.
    const char* lib_dir_c = p->SLibDir ? p->SLibDir->data : nullptr;
    std::string lib_dir = (lib_dir_c && lib_dir_c[0]) ? lib_dir_c
                          : "/home/gorpi7/NEPTR_phase4/csound/nam/profiles";

    int initial_idx    = p->kProfileIdx ? static_cast<int>(*p->kProfileIdx) : 0;
    double sample_rate = static_cast<double>(cs->GetSr(cs));
    int ksmps          = static_cast<int>(cs->GetKsmps(cs));

    p->loader = new NamLoader();
    bool ok = p->loader->init(lib_dir, initial_idx, sample_rate, ksmps);
    if (!ok) {
        cs->Message(cs,
            "[NAMProcess] no .nam files in '%s' — passthrough active\n",
            lib_dir.c_str());
    } else {
        cs->Message(cs,
            "[NAMProcess] %s (%d profiles)\n",
            lib_dir.c_str(), p->loader->get_count());
    }
    p->last_idx = initial_idx;

    publish_loader_ptr(cs, p->loader);
    cs->RegisterDeinitCallback(cs, p, nam_deinit);

    return CSOUND_SUCCESS;
}

// ---------------------------------------------------------------------------
// NAMProcess — kperf (called once per ksmps block, fills a-rate output arrays)
//
// CSound 6.18 AppendOpcode/LINKAGE maps ep->thread→flags, so thread=5
// (init+aperf) silently mis-schedules the aperf callback.  Using thread=3
// (init+kperf) with kopadr avoids that path; a-rate output buffers are
// allocated by CSound based on outypes="aa" regardless of thread value.
// ---------------------------------------------------------------------------
static int nam_kperf(CSOUND* cs, NAMOpcode* p) {
    const int ksmps = static_cast<int>(cs->GetKsmps(cs));

    // Update profile if kProfileIdx changed.
    int new_idx = p->kProfileIdx ? static_cast<int>(*p->kProfileIdx) : 0;
    if (new_idx != p->last_idx) {
        p->loader->set_index(new_idx);
        p->last_idx = new_idx;
    }

    std::shared_ptr<nam::DSP> model = p->loader->get_active();

    MYFLT* inL  = p->ainL;
    MYFLT* inR  = p->ainR;
    MYFLT* outL = p->aoutL;
    MYFLT* outR = p->aoutR;

    if (!model) {
        // No profile loaded yet — passthrough.
        if (inL != outL) std::memcpy(outL, inL, sizeof(MYFLT) * ksmps);
        if (inR != outR) std::memcpy(outR, inR, sizeof(MYFLT) * ksmps);
        return CSOUND_SUCCESS;
    }

    // nam::DSP::process(NAM_SAMPLE** input, NAM_SAMPLE** output, int num_frames)
    // NAM_SAMPLE is double by default (same as MYFLT in csound64).
    if constexpr (std::is_same_v<MYFLT, NAM_SAMPLE>) {
        NAM_SAMPLE* in_ch[1];
        NAM_SAMPLE* out_ch[1];

        in_ch[0]  = reinterpret_cast<NAM_SAMPLE*>(inL);
        out_ch[0] = reinterpret_cast<NAM_SAMPLE*>(outL);
        model->process(in_ch, out_ch, ksmps);

        in_ch[0]  = reinterpret_cast<NAM_SAMPLE*>(inR);
        out_ch[0] = reinterpret_cast<NAM_SAMPLE*>(outR);
        model->process(in_ch, out_ch, ksmps);
    } else {
        std::vector<NAM_SAMPLE> bufL(ksmps), bufR(ksmps);
        for (int i = 0; i < ksmps; ++i) {
            bufL[i] = static_cast<NAM_SAMPLE>(inL[i]);
            bufR[i] = static_cast<NAM_SAMPLE>(inR[i]);
        }
        NAM_SAMPLE* in_ch[1];
        NAM_SAMPLE* out_ch[1];

        in_ch[0]  = bufL.data();
        out_ch[0] = bufL.data();
        model->process(in_ch, out_ch, ksmps);

        in_ch[0]  = bufR.data();
        out_ch[0] = bufR.data();
        model->process(in_ch, out_ch, ksmps);

        for (int i = 0; i < ksmps; ++i) {
            outL[i] = static_cast<MYFLT>(bufL[i]);
            outR[i] = static_cast<MYFLT>(bufR[i]);
        }
    }

    return CSOUND_SUCCESS;
}

// ---------------------------------------------------------------------------
// NAMCount — i-rate: returns number of .nam files in the active library.
// Reads the loader ptr published by NAMProcess.
// ---------------------------------------------------------------------------
static int nam_count_init(CSOUND* cs, NAMCountOpcode* p) {
    void* slot = cs->QueryGlobalVariable(cs, "__nam_loader_ptr__");
    if (slot) {
        NamLoader* loader = *reinterpret_cast<NamLoader**>(slot);
        if (loader) {
            *p->icount = static_cast<MYFLT>(loader->get_count());
            return CSOUND_SUCCESS;
        }
    }
    *p->icount = static_cast<MYFLT>(0);
    cs->Message(cs, "[NAMCount] WARNING: NAMProcess not yet initialised\n");
    return CSOUND_SUCCESS;
}

// ---------------------------------------------------------------------------
// OENTRY table
// Fields: opname, dsblksiz, flags, thread, outypes, intypes,
//         iopadr (init), kopadr (k-perf), aopadr (a-perf), useropinfo
//
// thread=3 (init+kperf): CSound 6.18 LINKAGE/AppendOpcode maps ep->thread to
// the 'flags' slot, so thread=5 (init+aperf) silently breaks aperf scheduling.
// thread=3 uses kopadr which CSound always calls regardless of output type.
// outypes="aa" still allocates a-rate arrays; kperf fills them per k-block.
// ---------------------------------------------------------------------------
static OENTRY oentries[] = {
    {
        (char*)"NAMProcess",
        (uint16_t)sizeof(NAMOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf (see comment above)
        (char*)"aa",          // outypes: aoutL, aoutR (a-rate arrays)
        (char*)"aakS",        // intypes: ainL, ainR, kProfileIdx, SLibDir
        (SUBR)nam_init,
        (SUBR)nam_kperf,      // kopadr: called once per ksmps block
        nullptr,              // aopadr: unused
        nullptr               // useropinfo
    },
    {
        (char*)"NAMCount",
        (uint16_t)sizeof(NAMCountOpcode),
        (uint16_t)0,
        (uint8_t)1,           // init only (i-rate)
        (char*)"i",           // outypes: icount
        (char*)"",            // no inputs
        (SUBR)nam_count_init,
        nullptr,
        nullptr,
        nullptr
    }
};

// ---------------------------------------------------------------------------
// Plugin entry point and API version info (required by CSound).
// LINKAGE macro expands to both functions.
// ---------------------------------------------------------------------------
#define localops oentries
LINKAGE
