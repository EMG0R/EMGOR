// csound_nam_opcode.cpp — Csound plugin exposing NAM (Neural Amp Modeler)
// opcodes: NAMProcess, NAMCount, NAMActiveName, NAMActivePath, NAMStep,
// NAMWriteFile.
//
//   aoutL, aoutR  NAMProcess  ainL, ainR, kProfileIdx, SLibDir[, kAutoVol, kDriveDb]
//
// NAMProcess starts a background thread (NamLoader) that watches kProfileIdx
// for changes and swaps .nam profiles without blocking the audio thread.

#include "nam_loader.h"

// Csound plugin API. plugin.h in Csound 6.18 has a typo that breaks
// compilation with AppleClang 17+; CSOUND_USE_CSDL_H is set by CMakeLists.txt
// on macOS to force the csdl.h path.
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

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <strings.h>   // strcasecmp

#include "kweight.h"

// ---------------------------------------------------------------------------
// live dry-referenced volume compensation.
//
// the dry input (before any drive) is the loudness reference: the wet output
// gets a slow trim so it lands at the dry signal's perceived level. driving
// kDriveDb harder adds saturation while the compensator pulls the output back
// down — more distortion, no volume jump.
//
// deliberately NOT a compressor: loudness is a ~2.5 s K-weighted average, the
// trim slews at most 2 dB/s, holds inside a 0.75 dB deadband, freezes when
// the input is silent, and each model's converged trim is remembered so
// switching back to a model is instantly correct.
//
// heap-allocated (the opcode struct is csound-allocated, no C++ ctors run).
// ---------------------------------------------------------------------------
struct NamCompState {
    double  sr{48000.0};
    KwChain kw_dry, kw_wet;
    double  dry_ms{0.0}, wet_ms{0.0};   // EMAs of K-weighted mean square
    double  dry_warm{0.0}, wet_warm{0.0};  // seconds of active signal seen
    double  trim_db{0.0};
    const void* model_tag{nullptr};     // active model identity (raw ptr)
    std::string cur_path;
    std::map<std::string, double> remembered;

    static constexpr double kTauSec      = 2.5;   // loudness averaging (s)
    static constexpr double kSlewDbPerS  = 2.0;   // max trim movement (dB/s)
    static constexpr double kDeadbandDb  = 0.75;  // hold region (dB)
    static constexpr double kTrimLimitDb = 18.0;
    static constexpr double kActiveMs    = 3.16e-6;  // (-55 dBFS)^2 silence gate
    static constexpr double kWarmupSec   = 1.0;   // before trim may move
};

// per-opcode instance data. field order must match the OENTRY signature:
// OPDS header first, then outputs, then inputs, then private state.
struct NAMOpcode {
    OPDS      h;
    // outputs
    MYFLT*    aoutL;
    MYFLT*    aoutR;
    // inputs (match intypes "aakSPO" order)
    MYFLT*    ainL;
    MYFLT*    ainR;
    MYFLT*    kProfileIdx;
    STRINGDAT* SLibDir;
    MYFLT*    kAutoVol;   // optional (default 1): 0 = raw output, no comp
    MYFLT*    kDriveDb;   // optional (default 0): pre-model drive in dB,
                          //   loudness-compensated at the output
    // per-instance state (not part of the OPDS signature)
    NamCompState* comp;   // heap-allocated C++ state (new/delete in init/deinit)
    NamLoader* loader;
    int        last_idx;
    AUXCH      scratch;   // 2 * ksmps NAM_SAMPLEs (L then R work buffers)
    // cached copy of SLibDir as of the last kperf — SLibDir is watched at
    // k-rate so a UI (e.g. Cabbage drag-and-drop) can retarget the profile
    // by writing a new .nam path into the string channel feeding it.
    // fixed buffer: this struct is csound-allocated (no C++ ctor runs).
    char       last_path[1024];
};

// NAMCount reads the profile count from the shared loader.
struct NAMCountOpcode {
    OPDS  h;
    MYFLT* icount;
};

// NAMActiveName returns the basename of the currently loaded profile
// (k-rate string) — empty until the first load completes, so a UI can
// distinguish "loading" from "running". NAMActivePath is the same but
// returns the full file path (for session persistence).
struct NAMActiveNameOpcode {
    OPDS       h;
    STRINGDAT* Sname;
};

// NAMStep — on kTrig != 0, step through the active profile's folder
// (kDelta -1 = previous file, +1 = next file, sorted by name).
struct NAMStepOpcode {
    OPDS   h;
    MYFLT* kTrig;
    MYFLT* kDelta;
};

// NAMWriteFile — i-rate: truncate-write Stext into Sfile (Csound's fprints
// can only append).
struct NAMWriteFileOpcode {
    OPDS       h;
    STRINGDAT* Sfile;
    STRINGDAT* Stext;
};

static int nam_deinit(CSOUND* cs, void* p) {
    NAMOpcode* op = reinterpret_cast<NAMOpcode*>(p);
    if (op->comp) {
        delete op->comp;
        op->comp = nullptr;
    }
    if (op->loader) {
        // if the shared slot still points at this instance's loader, clear it
        // so NAMCount/NAMActiveName can't dereference freed memory after an
        // instance ends (e.g. a MIDI-spawned duplicate instance going away).
        void* slot = cs->QueryGlobalVariable(cs, "__nam_loader_ptr__");
        if (slot && *reinterpret_cast<NamLoader**>(slot) == op->loader) {
            *reinterpret_cast<NamLoader**>(slot) = nullptr;
        }
        delete op->loader;
        op->loader = nullptr;
    }
    return CSOUND_SUCCESS;
}

// loader pointer sharing via Csound global storage: NAMProcess stores its
// NamLoader* so the query opcodes (NAMCount etc.) can read it.
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

// NAMProcess — init
static int nam_init(CSOUND* cs, NAMOpcode* p) {
    p->loader   = nullptr;
    p->last_idx = -1;

    p->comp = new NamCompState();
    p->comp->sr = static_cast<double>(cs->GetSr(cs));
    p->comp->kw_dry.init(p->comp->sr);
    p->comp->kw_wet.init(p->comp->sr);

    // work buffers for gain-staged processing (csound-managed, freed with
    // the instrument — never allocate on the audio thread).
    const int iksmps = static_cast<int>(cs->GetKsmps(cs));
    cs->AuxAlloc(cs, 2 * static_cast<size_t>(iksmps) * sizeof(NAM_SAMPLE),
                 &p->scratch);

    // no default library: an empty SLibDir means passthrough until the user
    // supplies a profile directory or .nam file path.
    const char* lib_dir_c = p->SLibDir ? p->SLibDir->data : nullptr;
    std::string lib_dir = (lib_dir_c && lib_dir_c[0]) ? lib_dir_c : "";

    int initial_idx    = p->kProfileIdx ? static_cast<int>(*p->kProfileIdx) : 0;
    double sample_rate = static_cast<double>(cs->GetSr(cs));
    int ksmps          = static_cast<int>(cs->GetKsmps(cs));

    p->loader = new NamLoader();
    bool ok = p->loader->init(lib_dir, initial_idx, sample_rate, ksmps);
    if (!ok) {
        cs->Message(cs,
            "[NAMProcess] no .nam files in '%s' — passthrough active "
            "(pass a profile directory or .nam file as SLibDir)\n",
            lib_dir.c_str());
    } else {
        cs->Message(cs,
            "[NAMProcess] %s (%d profiles)\n",
            lib_dir.c_str(), p->loader->get_count());
    }
    p->last_idx = initial_idx;

    // seed the k-rate path watcher with the init-time string.
    p->last_path[0] = '\0';
    if (lib_dir_c) {
        std::strncpy(p->last_path, lib_dir_c, sizeof(p->last_path) - 1);
        p->last_path[sizeof(p->last_path) - 1] = '\0';
    }

    publish_loader_ptr(cs, p->loader);
    cs->RegisterDeinitCallback(cs, p, nam_deinit);

    return CSOUND_SUCCESS;
}

// NAMProcess — kperf (k-rate dispatch, processes one ksmps block per cycle).
// Csound 6.18 LINKAGE maps ep->thread → flags, so thread=5 (aperf) silently
// breaks scheduling — the audio callback never fires and outputs stay zero.
// thread=3 (kperf) is scheduled correctly; the body is the same either way
// since it processes one ksmps block per call.
static int nam_kperf(CSOUND* cs, NAMOpcode* p) {
    const int ksmps = static_cast<int>(cs->GetKsmps(cs));

    int new_idx = p->kProfileIdx ? static_cast<int>(*p->kProfileIdx) : 0;
    if (new_idx != p->last_idx) {
        p->loader->set_index(new_idx);
        p->last_idx = new_idx;
    }

    // watch the path string at k-rate: a UI can push a new .nam file path
    // (e.g. Cabbage LAST_FILE_DROPPED) into the S input; a change queues a
    // background load of that file — no re-init, no audio dropout.
    const char* cur_path = p->SLibDir ? p->SLibDir->data : nullptr;
    if (cur_path && cur_path[0] &&
        std::strncmp(cur_path, p->last_path, sizeof(p->last_path)) != 0) {
        std::strncpy(p->last_path, cur_path, sizeof(p->last_path) - 1);
        p->last_path[sizeof(p->last_path) - 1] = '\0';
        size_t len = std::strlen(cur_path);
        if (len > 4 && strcasecmp(cur_path + len - 4, ".nam") == 0) {
            p->loader->set_path(cur_path);
        }
    }

    // per-channel model instances: NAM models carry state (conv ring buffers,
    // LSTM hidden state) across process() calls, so each channel needs its
    // own instance — sharing one corrupts both streams at every block edge.
    std::shared_ptr<nam::DSP> modelL, modelR;
    double in_gain, out_gain;
    p->loader->get_active_pair(modelL, modelR, in_gain, out_gain);
    const bool auto_vol = !(p->kAutoVol && *p->kAutoVol < FL(0.5));
    // auto volume off -> keep the input staging but skip loudness makeup
    if (!auto_vol) out_gain = 1.0;

    // pre-model drive (dB) — loudness-compensated at the output below, so
    // turning it up adds saturation, not volume
    const double drive_db = p->kDriveDb ? static_cast<double>(*p->kDriveDb)
                                        : 0.0;
    in_gain *= std::pow(10.0, drive_db / 20.0);

    MYFLT* inL  = p->ainL;
    MYFLT* inR  = p->ainR;
    MYFLT* outL = p->aoutL;
    MYFLT* outR = p->aoutR;

    if (!modelL || !modelR || !p->scratch.auxp) {
        // no profile loaded yet — passthrough.
        if (inL != outL) std::memcpy(outL, inL, sizeof(MYFLT) * ksmps);
        if (inR != outR) std::memcpy(outR, inR, sizeof(MYFLT) * ksmps);
        return CSOUND_SUCCESS;
    }

    // pad input into the scratch buffers, run each channel through its own
    // model in place, then write out with loudness makeup.
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

    // live dry-referenced volume compensation (see NamCompState).
    NamCompState* c = p->comp;
    double comp_gain = 1.0;
    if (c) {
        // model swap: remember the converged trim for the old model and
        // restore the remembered trim for the new one (or keep the current
        // trim as the best available guess for an unseen model).
        const void* tag = modelL.get();
        if (tag != c->model_tag) {
            if (!c->cur_path.empty()) c->remembered[c->cur_path] = c->trim_db;
            c->model_tag = tag;
            c->cur_path  = p->loader->get_active_path();
            auto it = c->remembered.find(c->cur_path);
            if (it != c->remembered.end()) c->trim_db = it->second;
            c->wet_ms   = 0.0;
            c->wet_warm = 0.0;
            c->kw_wet.reset();
        }

        // block loudness (K-weighted mean square of the mono mix):
        // dry = the raw input (no drive, no pad) — the reference the player
        // hears without the amp; wet = model output incl. static makeup.
        double dry_blk = 0.0, wet_blk = 0.0;
        for (int i = 0; i < ksmps; ++i) {
            double d = 0.5 * (static_cast<double>(inL[i]) +
                              static_cast<double>(inR[i]));
            double w = 0.5 * (sL[i] + sR[i]) * out_gain;
            double dk = c->kw_dry.tick(d);
            double wk = c->kw_wet.tick(w);
            dry_blk += dk * dk;
            wet_blk += wk * wk;
        }
        dry_blk /= ksmps;
        wet_blk /= ksmps;

        const double blk_sec = ksmps / c->sr;
        const double alpha   = blk_sec / NamCompState::kTauSec;
        // freeze everything while the input is silent — silence must never
        // decay the dry reference or drag the trim around.
        if (dry_blk > NamCompState::kActiveMs) {
            c->dry_ms  += alpha * (dry_blk - c->dry_ms);
            c->wet_ms  += alpha * (wet_blk - c->wet_ms);
            c->dry_warm = std::min(c->dry_warm + blk_sec, 10.0);
            c->wet_warm = std::min(c->wet_warm + blk_sec, 10.0);

            if (auto_vol &&
                c->dry_warm >= NamCompState::kWarmupSec &&
                c->wet_warm >= NamCompState::kWarmupSec &&
                c->wet_ms > 1e-14 && c->dry_ms > 1e-14) {
                double target = 10.0 * std::log10(c->dry_ms / c->wet_ms);
                if (target >  NamCompState::kTrimLimitDb) target =  NamCompState::kTrimLimitDb;
                if (target < -NamCompState::kTrimLimitDb) target = -NamCompState::kTrimLimitDb;
                double diff = target - c->trim_db;
                if (std::fabs(diff) > NamCompState::kDeadbandDb) {
                    double step = NamCompState::kSlewDbPerS * blk_sec;
                    if (diff >  step) diff =  step;
                    if (diff < -step) diff = -step;
                    c->trim_db += diff;
                }
            }
        }
        if (auto_vol) comp_gain = std::pow(10.0, c->trim_db / 20.0);
    }

    const double og = out_gain * comp_gain;
    for (int i = 0; i < ksmps; ++i) {
        outL[i] = static_cast<MYFLT>(sL[i] * og);
        outR[i] = static_cast<MYFLT>(sR[i] * og);
    }

    return CSOUND_SUCCESS;
}

// NAMCount — i-rate: number of .nam files in the active library.
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

static NamLoader* fetch_loader(CSOUND* cs) {
    void* slot = cs->QueryGlobalVariable(cs, "__nam_loader_ptr__");
    return slot ? *reinterpret_cast<NamLoader**>(slot) : nullptr;
}

static void write_sout(CSOUND* cs, STRINGDAT* out, const std::string& s) {
    const int needed = static_cast<int>(s.size()) + 1;
    if (out->size < needed || !out->data) {
        if (out->data) cs->Free(cs, out->data);
        out->data = static_cast<char*>(cs->Malloc(cs, needed));
        out->size = needed;
    }
    std::memcpy(out->data, s.c_str(), needed);
}

static int nam_name_copy(CSOUND* cs, NAMActiveNameOpcode* p) {
    std::string name;
    if (NamLoader* loader = fetch_loader(cs)) name = loader->get_active_name();
    write_sout(cs, p->Sname, name);
    return CSOUND_SUCCESS;
}

static int nam_path_copy(CSOUND* cs, NAMActiveNameOpcode* p) {
    std::string path;
    if (NamLoader* loader = fetch_loader(cs)) path = loader->get_active_path();
    write_sout(cs, p->Sname, path);
    return CSOUND_SUCCESS;
}

static int nam_writefile_init(CSOUND*, NAMWriteFileOpcode* p) {
    if (!p->Sfile || !p->Sfile->data || !p->Sfile->data[0])
        return CSOUND_SUCCESS;
    if (FILE* f = fopen(p->Sfile->data, "w")) {
        if (p->Stext && p->Stext->data) fputs(p->Stext->data, f);
        fclose(f);
    }
    return CSOUND_SUCCESS;
}

static int nam_step_init(CSOUND*, NAMStepOpcode*) { return CSOUND_SUCCESS; }
static int nam_step_kperf(CSOUND* cs, NAMStepOpcode* p) {
    if (p->kTrig && *p->kTrig != FL(0.0)) {
        if (NamLoader* loader = fetch_loader(cs)) {
            int delta = p->kDelta ? static_cast<int>(*p->kDelta) : 1;
            if (delta == 0) delta = 1;
            loader->step(delta);
        }
    }
    return CSOUND_SUCCESS;
}

// OENTRY table
// fields: opname, dsblksiz, flags, thread, outypes, intypes,
//         iopadr (init), kopadr (k-perf), aopadr (a-perf), useropinfo
// thread: 1=init, 2=kperf, 4=aperf, 5=init+aperf
static OENTRY oentries[] = {
    {
        (char*)"NAMProcess",
        (uint16_t)sizeof(NAMOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf (aperf/thread=5 broken in Csound 6.18)
        (char*)"aa",          // aoutL, aoutR
        (char*)"aakSPO",      // ainL, ainR, kProfileIdx, SLibDir[, kAutoVol=1, kDriveDb=0]
        (SUBR)nam_init,
        (SUBR)nam_kperf,
        nullptr,
        nullptr
    },
    {
        (char*)"NAMCount",
        (uint16_t)sizeof(NAMCountOpcode),
        (uint16_t)0,
        (uint8_t)1,           // init only
        (char*)"i",
        (char*)"",
        (SUBR)nam_count_init,
        nullptr,
        nullptr,
        nullptr
    },
    {
        (char*)"NAMActiveName",
        (uint16_t)sizeof(NAMActiveNameOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf
        (char*)"S",
        (char*)"",
        (SUBR)nam_name_copy,
        (SUBR)nam_name_copy,
        nullptr,
        nullptr
    },
    {
        (char*)"NAMActivePath",
        (uint16_t)sizeof(NAMActiveNameOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf
        (char*)"S",
        (char*)"",
        (SUBR)nam_path_copy,
        (SUBR)nam_path_copy,
        nullptr,
        nullptr
    },
    {
        (char*)"NAMStep",
        (uint16_t)sizeof(NAMStepOpcode),
        (uint16_t)0,
        (uint8_t)3,           // init + kperf
        (char*)"",
        (char*)"kk",          // kTrig, kDelta
        (SUBR)nam_step_init,
        (SUBR)nam_step_kperf,
        nullptr,
        nullptr
    },
    {
        (char*)"NAMWriteFile",
        (uint16_t)sizeof(NAMWriteFileOpcode),
        (uint16_t)0,
        (uint8_t)1,           // init only
        (char*)"",
        (char*)"SS",          // Sfile, Stext
        (SUBR)nam_writefile_init,
        nullptr,
        nullptr,
        nullptr
    }
};

// plugin entry point and API version info (required by Csound); LINKAGE
// expands to both functions.
#define localops oentries
LINKAGE
