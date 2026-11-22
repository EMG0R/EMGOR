#pragma once

#include <atomic>
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
// Thread-safety contract:
//   - set_index() and get_active() may be called from any thread, concurrently.
//   - The audio thread calls get_active() every ksmps block — it acquires a
//     mutex but only for a shared_ptr copy (< 100 ns on uncontended path).
//   - The loader thread holds the mutex only while swapping the shared_ptr,
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

    // Lock-free read of the currently active model.  Returns nullptr if no
    // model is loaded yet.  Callers must handle nullptr gracefully.
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

    // Index that is actually loaded in active_model_.  Only written by the
    // loader thread.
    int current_idx_{-1};

    // The active model — written by loader thread, read by audio thread.
    // Protected by model_mutex_ for the shared_ptr copy.
    mutable std::mutex model_mutex_;
    std::shared_ptr<nam::DSP> active_model_;

    std::thread loader_thread_;
    std::atomic<bool> stop_{false};

    // Entry point for loader_thread_.
    void loader_loop();

    // Synchronously scan lib_dir for *.nam files, populate paths_.
    static std::vector<std::string> scan_dir(const std::string& lib_dir);
};
