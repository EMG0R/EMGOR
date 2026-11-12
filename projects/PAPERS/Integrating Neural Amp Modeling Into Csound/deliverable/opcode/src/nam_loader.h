// nam_loader.h — background NAM profile loader shared by the csound-nam
// opcodes.
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// forward-declare nam::DSP so callers don't need NAM headers just to hold a
// NamLoader.
namespace nam {
class DSP;
}

// NamLoader owns a background thread that watches for profile index changes
// and loads new NAM models off the audio thread.
//
// stereo correctness: NAM models are stateful (WaveNet conv ring buffers,
// LSTM hidden state carry across process() calls). one instance must only
// ever see one continuous audio stream, so the loader holds two independent
// instances per profile — one for L, one for R — loaded from the same .nam
// file and swapped atomically as a pair.
//
// thread-safety contract:
//   - set_index() and get_active_pair() may be called from any thread,
//     concurrently.
//   - the audio thread calls get_active_pair() every ksmps block — it
//     acquires a mutex but only for two shared_ptr copies.
//   - the loader thread holds the mutex only while swapping the shared_ptrs,
//     never during the blocking nam::get_dsp() call.
class NamLoader {
public:
    NamLoader();
    ~NamLoader();  // signals stop, joins loader thread

    // scan lib_dir recursively for *.nam files, build a sorted path list,
    // and start the background loader thread. sample_rate and max_buffer
    // are forwarded to DSP::Reset() after each load. returns true if at
    // least one .nam file was found.
    bool init(const std::string& lib_dir, int initial_idx,
              double sample_rate, int max_buffer);

    // request a profile switch. returns immediately — the loader thread
    // picks it up within ~10 ms and does the blocking I/O off the audio
    // thread.
    void set_index(int idx);

    // request loading one specific .nam file by absolute path (e.g. a file
    // dragged onto a plugin UI). returns immediately; the previous model
    // keeps running until the swap.
    void set_path(const std::string& path);

    // basename (no dir, no .nam) of the most recently loaded profile —
    // reflects completed loads only, never the pending request.
    std::string get_active_name() const;

    // full path of the most recently loaded profile ("" until first load).
    std::string get_active_path() const;

    // step through the sibling .nam files of the active profile's folder
    // (sorted by name): delta -1 = previous, +1 = next. clamped at the
    // ends. no-op until a file has been loaded via set_path()/step().
    void step(int delta);

    // copy the currently active per-channel models plus the gain staging
    // computed for them at load time. both models are nullptr until the
    // first load completes; they always refer to the same profile.
    // callers must handle nullptr gracefully (passthrough).
    //   in_gain  — linear pre-model pad (from input_pad_db, dB) so a
    //              full-scale chain doesn't slam the amp.
    //   out_gain — linear post-model makeup normalising measured loudness
    //              to target_loudness_db so profiles sit at one volume.
    void get_active_pair(std::shared_ptr<nam::DSP>& outL,
                         std::shared_ptr<nam::DSP>& outR,
                         double& in_gain, double& out_gain) const;

    // legacy single-model accessor (returns the L instance).
    std::shared_ptr<nam::DSP> get_active() const;

    // number of .nam files discovered during init(). stable after init().
    int get_count() const;

    // basename of the file at idx ("" if out of range).
    std::string get_name(int idx) const;

private:
    std::vector<std::string> paths_;  // sorted, stable after init()

    double sample_rate_{48000.0};
    int    max_buffer_{128};

    // index currently being targeted by the audio thread / UI.
    std::atomic<int> requested_idx_{-1};

    // direct-path load request (set_path). guarded by path_mutex_;
    // path_pending_ flags the loader thread to pick it up.
    mutable std::mutex path_mutex_;
    std::string requested_path_;
    std::atomic<bool> path_pending_{false};
    std::string active_name_;   // guarded by path_mutex_
    std::string active_path_;   // guarded by path_mutex_

    // folder-navigation list: sorted .nam siblings of the active profile.
    // rebuilt by the loader thread after each successful load; read by
    // step() from any thread. guarded by path_mutex_.
    std::vector<std::string> nav_paths_;
    int nav_idx_{-1};

    // index actually loaded in the active models. loader thread only.
    int current_idx_{-1};

    // active per-channel models — written by the loader thread, read by the
    // audio thread. protected by model_mutex_; always swapped as a pair so
    // L and R can never briefly point at different profiles.
    mutable std::mutex model_mutex_;
    std::shared_ptr<nam::DSP> active_model_L_;
    std::shared_ptr<nam::DSP> active_model_R_;
    double active_in_gain_{1.0};   // linear; swapped together with the pair
    double active_out_gain_{1.0};

    // level config, re-read from <lib_dir>/nam_levels.conf before each load
    // so values can be tuned live (edit file, toggle profile — no rebuild).
    std::string lib_dir_;
    double input_pad_db_{0.0};
    double target_loudness_db_{-14.0};   // dB
    bool   loudness_norm_{true};   // conf "loudness_norm = on|off"
    void read_levels_conf();

    // per-profile calibrated pregain — <lib_dir>/nam_pregain.conf, lines of
    // "<profile basename without .nam> = <dB>". a profile with an entry uses
    // it as its input gain with unity output gain (overrides input_pad_db
    // and loudness normalisation).
    std::map<std::string, double> pregain_db_;
    void read_pregain_conf();

    // measure the model's actual output loudness (dB, K-weighted) with a
    // reference DI probe. community captures carry wildly inconsistent
    // `loudness` metadata, so it is never trusted — measured loudness makes
    // volume matching exact for any capture. loader thread only; leaves the
    // instance Reset() afterwards.
    double measure_loudness_db(nam::DSP* model) const;

    std::thread loader_thread_;
    std::atomic<bool> stop_{false};

    // entry point for loader_thread_.
    void loader_loop();

    // blocking load of one .nam file (both channel instances + gain staging)
    // followed by an atomic swap-in. loader thread only. returns false and
    // keeps the previous pair on failure.
    bool load_and_install(const std::string& path);

    // synchronously scan lib_dir for *.nam files.
    static std::vector<std::string> scan_dir(const std::string& lib_dir);
};
