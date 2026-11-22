#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward-declare nam::DSP so callers don't need to include NAM headers
// just to hold a NamLoader — the .cpp will include <NAM/get_dsp.h>
namespace nam {
class DSP;
}

// NamLoader owns a background thread that watches for profile index changes
// and loads new NAM models off the audio thread.
//
// Stereo correctness: NAM models are STATEFUL (WaveNet conv ring buffers,
// LSTM hidden state carry across process() calls). One instance must only
// ever see one continuous audio stream, so the loader holds TWO independent
// instances per profile — one for L, one for R — loaded from the same .nam
// file and swapped atomically as a pair.
//
// Thread-safety contract:
//   - set_index() and get_active_pair() may be called from any thread,
//     concurrently.
//   - The audio thread calls get_active_pair() every ksmps block — it
//     acquires a mutex but only for two shared_ptr copies.
//   - The loader thread holds the mutex only while swapping the shared_ptrs,
//     never during the blocking nam::get_dsp() call.
//
// C++17 compatibility: std::atomic<std::shared_ptr<T>> is C++20.  We use a
// mutex + plain shared_ptr pair instead.  The audio-thread path takes one
// lock/unlock cycle for a shared_ptr copy — acceptable at ksmps = 128.
class NamLoader {
public:
    NamLoader();
    ~NamLoader();  // signals stop, joins loader thread

    // Scan lib_dir recursively for *.nam files, build sorted path list,
    // and start the background loader thread.  If initial_idx >= 0 and
    // there are profiles available it kicks off an immediate load.
    // sample_rate and max_buffer are forwarded to DSP::Reset() after load.
    // Returns true if at least one .nam file was found.
    bool init(const std::string& lib_dir, int initial_idx,
              double sample_rate, int max_buffer);

    // Request a profile switch.  Returns immediately — the loader thread
    // will pick this up within ~10 ms and do the blocking I/O off the
    // audio thread.
    void set_index(int idx);

    // Request loading one specific .nam file by absolute path (e.g. a file
    // dragged onto a plugin UI).  Returns immediately; the loader thread
    // does the blocking load off the audio thread and swaps the active pair
    // when ready.  The previous model keeps running until the swap.
    void set_path(const std::string& path);

    // Basename (no dir, no .nam) of the most recently LOADED profile —
    // reflects completed loads only, never the pending request.
    std::string get_active_name() const;

    // Full path of the most recently LOADED profile ("" until first load).
    std::string get_active_path() const;

    // Step through the sibling .nam files of the active profile's folder
    // (sorted by name): delta -1 = previous, +1 = next.  Clamped at the
    // ends.  No-op until a file has been loaded via set_path()/step().
    void step(int delta);

    // Copy the currently active per-channel models plus the gain staging
    // computed for them at load time.  Both models are nullptr until the
    // first load completes; they always refer to the same profile.
    // Callers must handle nullptr gracefully (passthrough).
    //   in_gain  — linear pre-model pad (from input_pad_db in nam_levels.conf,
    //              default -12 dB) so a full-scale chain doesn't slam the amp.
    //   out_gain — linear post-model makeup normalising the profile's
    //              loudness metadata to target_loudness_db (default -18 dB)
    //              so profiles sit at one volume.
    void get_active_pair(std::shared_ptr<nam::DSP>& outL,
                         std::shared_ptr<nam::DSP>& outR,
                         double& in_gain, double& out_gain) const;

    // Legacy single-model accessor (returns the L instance).
    std::shared_ptr<nam::DSP> get_active() const;

    // Number of .nam files discovered during init().  Stable after init().
    int get_count() const;

    // Basename of the file at idx (no directory, no extension).
    // Returns empty string if idx is out of range.
    std::string get_name(int idx) const;

private:
    std::vector<std::string> paths_;  // sorted, stable after init()

    double sample_rate_{48000.0};
    int    max_buffer_{128};

    // Index currently being targeted by the audio thread / UI.
    std::atomic<int> requested_idx_{-1};

    // Direct-path load request (set_path).  Guarded by path_mutex_;
    // path_pending_ flags the loader thread to pick it up.
    mutable std::mutex path_mutex_;
    std::string requested_path_;
    std::atomic<bool> path_pending_{false};
    std::string active_name_;   // guarded by path_mutex_ (cheap, UI-rate)
    std::string active_path_;   // guarded by path_mutex_

    // Folder-navigation list: sorted .nam siblings of the active profile.
    // Rebuilt by the loader thread after each successful direct-path load;
    // read by step() from any thread.  Guarded by path_mutex_.
    std::vector<std::string> nav_paths_;
    int nav_idx_{-1};

    // Index that is actually loaded in the active models.  Only written by
    // the loader thread.
    int current_idx_{-1};

    // The active per-channel models — written by loader thread, read by the
    // audio thread.  Protected by model_mutex_; always swapped as a pair so
    // L and R can never briefly point at different profiles.
    mutable std::mutex model_mutex_;
    std::shared_ptr<nam::DSP> active_model_L_;
    std::shared_ptr<nam::DSP> active_model_R_;
    double active_in_gain_{1.0};   // linear; swapped together with the pair
    double active_out_gain_{1.0};

    // Level config, re-read from <lib_dir>/nam_levels.conf before each load
    // so values can be tuned live (edit file, toggle profile — no rebuild).
    std::string lib_dir_;
    double input_pad_db_{0.0};
    double target_loudness_db_{-14.0};
    bool   loudness_norm_{true};   // conf "loudness_norm = on|off"
    void read_levels_conf();

    // Per-profile calibrated pregain — <lib_dir>/nam_pregain.conf, lines of
    // "<profile basename without .nam> = <dB>".  Generated by
    // testing/calibrate_pregains.sh: the input gain that makes the amp
    // unity-loudness at the Scarlett green-before-yellow reference level.
    // A profile with an entry uses it as its input gain with UNITY output
    // gain (overrides input_pad_db and loudness normalisation).
    std::map<std::string, double> pregain_db_;
    void read_pregain_conf();

    // Measure the model's actual output loudness (dB RMS) by running a
    // -18 dBFS-RMS 1 kHz sine through it.  Community captures carry wildly
    // inconsistent `loudness` metadata, so we never trust it — measured
    // loudness makes volume matching exact for any capture.  Called on the
    // loader thread only; leaves the instance Reset() afterwards.
    double measure_loudness_db(nam::DSP* model) const;

    std::thread loader_thread_;
    std::atomic<bool> stop_{false};

    // Entry point for loader_thread_.
    void loader_loop();

    // Blocking load of one .nam file (both channel instances + gain staging)
    // followed by an atomic swap-in.  Loader thread only.  Returns false and
    // keeps the previous pair on failure.
    bool load_and_install(const std::string& path);

    // Synchronously scan lib_dir for *.nam files, populate paths_.
    static std::vector<std::string> scan_dir(const std::string& lib_dir);
};
