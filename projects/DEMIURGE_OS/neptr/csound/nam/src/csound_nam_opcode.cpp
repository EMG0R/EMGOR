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
    AUXCH      scratch;   // 2 * ksmps NAM_SAMPLEs (L then R work buffers)
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

    // Work buffers for gain-staged processing (csound-managed, freed with
    // the instrument — never allocate on the audio thread).
    const int iksmps = static_cast<int>(cs->GetKsmps(cs));
    cs->AuxAlloc(cs, 2 * static_cast<size_t>(iksmps) * sizeof(NAM_SAMPLE),
                 &p->scratch);

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
// NAMProcess — kperf (k-rate dispatch, processes one ksmps block per cycle).
// CSound 6.18 LINKAGE maps ep->thread → flags, so thread=5 (aperf) silently
// breaks scheduling — the audio callback never fires and outputs stay zero.
// thread=3 (kperf) is scheduled correctly; function body is identical since
// it processes one ksmps block regardless of which slot calls it.
// ---------------------------------------------------------------------------
static int nam_kperf(CSOUND* cs, NAMOpcode* p) {
    const int ksmps = static_cast<int>(cs->GetKsmps(cs));

    // Update profile if kProfileIdx changed.
    int new_idx = p->kProfileIdx ? static_cast<int>(*p->kProfileIdx) : 0;
    if (new_idx != p->last_idx) {
        p->loader->set_index(new_idx);
        p->last_idx = new_idx;
    }

    // Per-channel model instances: NAM models carry state (conv ring buffers,
    // LSTM hidden state) across process() calls, so each channel needs its
    // own instance — sharing one corrupts both streams at every block edge.
    std::shared_ptr<nam::DSP> modelL, modelR;
    double in_gain, out_gain;
    p->loader->get_active_pair(modelL, modelR, in_gain, out_gain);

    MYFLT* inL  = p->ainL;
    MYFLT* inR  = p->ainR;
    MYFLT* outL = p->aoutL;
    MYFLT* outR = p->aoutR;

    if (!modelL || !modelR || !p->scratch.auxp) {
        // No profile loaded yet — passthrough.
        if (inL != outL) std::memcpy(outL, inL, sizeof(MYFLT) * ksmps);
        if (inR != outR) std::memcpy(outR, inR, sizeof(MYFLT) * ksmps);
        return CSOUND_SUCCESS;
    }

    // Gain-staged path: pad input into the scratch buffers, run each channel
    // through its own model in place, then write out with loudness makeup.
    // nam::DSP::process(NAM_SAMPLE** input, NAM_SAMPLE** output, int num_frames)
    NAM_SAMPLE* sL = reinterpret_cast<NAM_SAMPLE*>(p->scratch.auxp);
    NAM_SAMPLE* sR = sL + ksmps;

    for (int i = 0; i < ksmps; ++i) {
        sL[i] = static_cast<NAM_SAMPLE>(inL[i]) * in_gain;
        sR[i] = static_cast<NAM_SAMPLE>(inR[i]) * in_gain;
    }

    NAM_SAMPLE* ch[1];
    ch[0] = sL;
    modelL->process(ch, ch, ksmps);
    ch[0] = sR;
    modelR->process(ch, ch, ksmps);

    for (int i = 0; i < ksmps; ++i) {
        outL[i] = static_cast<MYFLT>(sL[i] * out_gain);
        outR[i] = static_cast<MYFLT>(sR[i] * out_gain);
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
// thread: 1=init, 2=kperf, 4=aperf, 5=init+aperf
// ---------------------------------------------------------------------------
static OENTRY oentries[] = {
    {
        (char*)"NAMProcess",
        (uint16_t)sizeof(NAMOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf (aperf/thread=5 broken in CSound 6.18)
        (char*)"aa",          // outypes: aoutL, aoutR
        (char*)"aakS",        // intypes: ainL, ainR, kProfileIdx, SLibDir
        (SUBR)nam_init,
        (SUBR)nam_kperf,
        nullptr,
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
