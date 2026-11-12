#include "nam_loader.h"

#include <NAM/get_dsp.h>
#include <NAM/slimmable.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

// Live loader registry for shutdown_all() (see nam_loader.h).
static std::mutex g_loaders_mu;
static std::vector<NamLoader*>& g_loaders() {
    static auto* v = new std::vector<NamLoader*>();   // intentionally leaked
    return *v;
}

NamLoader::NamLoader() {
    std::lock_guard<std::mutex> lk(g_loaders_mu);
    g_loaders().push_back(this);
}

void NamLoader::shutdown_all() {
    std::vector<NamLoader*> v;
    { std::lock_guard<std::mutex> lk(g_loaders_mu); v = g_loaders(); }
    for (auto* l : v) l->stop_.store(true, std::memory_order_relaxed);
    for (auto* l : v)
        if (l->loader_thread_.joinable() &&
            l->loader_thread_.get_id() != std::this_thread::get_id())
            l->loader_thread_.join();
}

NamLoader::~NamLoader() {
    {
        std::lock_guard<std::mutex> lk(g_loaders_mu);
        auto& v = g_loaders();
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
    stop_.store(true, std::memory_order_relaxed);
    if (loader_thread_.joinable()) {
        loader_thread_.join();
    }
}

// ---------------------------------------------------------------------------
// init()
// ---------------------------------------------------------------------------

bool NamLoader::init(const std::string& lib_dir, int initial_idx,
                     double sample_rate, int max_buffer) {
    static std::once_flag once;
    std::call_once(once, [] { std::atexit([] { NamLoader::shutdown_all(); }); });
    sample_rate_ = sample_rate;
    max_buffer_  = max_buffer;
    lib_dir_     = lib_dir;

    paths_ = scan_dir(lib_dir);
    std::sort(paths_.begin(), paths_.end());

    if (paths_.empty()) {
        loader_thread_ = std::thread(&NamLoader::loader_loop, this);
        return false;
    }

    if (initial_idx < 0) initial_idx = 0;
    if (initial_idx >= static_cast<int>(paths_.size()))
        initial_idx = static_cast<int>(paths_.size()) - 1;

    requested_idx_.store(initial_idx, std::memory_order_relaxed);

    loader_thread_ = std::thread(&NamLoader::loader_loop, this);
    return true;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void NamLoader::set_index(int idx) {
    if (paths_.empty()) return;
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(paths_.size()))
        idx = static_cast<int>(paths_.size()) - 1;
    requested_idx_.store(idx, std::memory_order_relaxed);
    idx_touched_.store(true, std::memory_order_relaxed);
}

void NamLoader::set_size(int size) {
    requested_size_.store(size == 1 ? 1 : 0, std::memory_order_relaxed);
}

void NamLoader::get_active_pair(std::shared_ptr<nam::DSP>& outL,
                                std::shared_ptr<nam::DSP>& outR,
                                double& in_gain, double& out_gain) const {
    std::lock_guard<std::mutex> lock(model_mutex_);
    outL     = active_model_L_;
    outR     = active_model_R_;
    in_gain  = active_in_gain_;
    out_gain = active_out_gain_;
}

std::shared_ptr<nam::DSP> NamLoader::get_active() const {
    std::lock_guard<std::mutex> lock(model_mutex_);
    return active_model_L_;
}

int NamLoader::get_count() const {
    return static_cast<int>(paths_.size());
}

std::string NamLoader::get_name(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(paths_.size())) return "";
    const std::string& p = paths_[static_cast<size_t>(idx)];

    size_t slash = p.rfind('/');
    std::string base = (slash == std::string::npos) ? p : p.substr(slash + 1);

    if (base.size() > 4) {
        std::string ext = base.substr(base.size() - 4);
        for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".nam") base = base.substr(0, base.size() - 4);
    }
    return base;
}

// ---------------------------------------------------------------------------
// Background loader thread
// ---------------------------------------------------------------------------

// Move the calling (loader) thread off the audio core and out of the realtime
// class. The loader is spawned from a csound thread, so it INHERITS that
// thread's affinity (the single isolated audio core) and SCHED_FIFO priority.
// Profile loads (file read, JSON parse, weight allocation, prewarm, optional
// 1.5 s loudness probe) are memory-bandwidth heavy; run at FIFO on the audio
// core they pollute that core's L1/L2 and contend for memory with the audio
// thread, which is the "audio drops when a NAM profile switches/activates"
// mechanism. Best effort: any failure leaves the previous behaviour. Does not
// touch the model, its gains, or the audio path.
static void demote_loader_thread() {
    cpu_set_t all;
    CPU_ZERO(&all);
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 2) return;                       // single core: nothing to move to
    cpu_set_t cur;
    CPU_ZERO(&cur);
    sched_getaffinity(0, sizeof(cur), &cur);
    // only act when pinned (isolated audio core); leave an unpinned process alone
    if (CPU_COUNT(&cur) >= ncpu) return;
    for (long c = 0; c < ncpu; ++c) {
        if (!CPU_ISSET(static_cast<int>(c), &cur)) CPU_SET(static_cast<int>(c), &all);
    }
    if (CPU_COUNT(&all) == 0) return;
    sched_param sp{};
    sp.sched_priority = 0;
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &sp);
    sched_setaffinity(0, sizeof(all), &all);    // pid 0 = calling thread
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 19);
}

void NamLoader::loader_loop() {
    demote_loader_thread();
    // Defer the first load until the persisted profile has been requested
    // (or ~1 s), so boot loads once instead of default-then-persisted.
    for (int w = 0; w < 100 && !idx_touched_.load(std::memory_order_relaxed) &&
                    !stop_.load(std::memory_order_relaxed); ++w)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    while (!stop_.load(std::memory_order_relaxed)) {
        int wanted = requested_idx_.load(std::memory_order_relaxed);
        const int want_size = requested_size_.load(std::memory_order_relaxed);

        // A size change is a reload of the current profile (same path, same
        // atomic pair swap as a profile change), all on this thread.
        if ((wanted != current_idx_ || want_size != current_size_) && wanted >= 0 &&
            wanted < static_cast<int>(paths_.size()))
        {
            const std::string& path = paths_[static_cast<size_t>(wanted)];

            // Pick up any tuning edits before computing gains.
            read_levels_conf();
            read_pregain_conf();

            // Load two independent instances of the same profile — the model
            // is stateful, so L and R must never share one instance.
            auto load_one = [&](const char* ch) -> std::shared_ptr<nam::DSP> {
                try {
                    // nam::get_dsp() is the canonical factory in NeuralAmpModelerCore.
                    std::unique_ptr<nam::DSP> raw =
                        nam::get_dsp(std::filesystem::path(path));
                    if (!raw) {
                        std::cerr << "[NAMLoader] get_dsp returned null (" << ch
                                  << ") for: " << path << "\n";
                        return nullptr;
                    }
                    // Select the submodel. SlimmableContainer models default to
                    // submodel 0 (lo-fi). FULL = 1.0 picks the highest-quality
                    // submodel; MICRO = 0.0 picks the smallest (the container
                    // takes the first submodel whose max_value > val). Safe
                    // here: this is the loader thread, never the audio thread.
                    if (auto* slim = dynamic_cast<nam::SlimmableModel*>(raw.get())) {
                        slim->SetSlimmableSize(want_size == 1 ? 0.0 : 1.0);
                    }
                    // Reset also prewarms (flushes conv/LSTM state) off-thread.
                    raw->Reset(sample_rate_, max_buffer_);
                    double want = raw->GetExpectedSampleRate();
                    if (want > 0 && want != sample_rate_) {
                        std::cerr << "[NAMLoader] WARNING: '" << path
                                  << "' trained at " << want << " Hz but engine runs "
                                  << sample_rate_ << " Hz — tone will be off\n";
                    }
                    return std::shared_ptr<nam::DSP>(std::move(raw));
                } catch (const std::exception& e) {
                    std::cerr << "[NAMLoader] Failed to load '" << path
                              << "' (" << ch << "): " << e.what() << "\n";
                    return nullptr;
                } catch (...) {
                    std::cerr << "[NAMLoader] Unknown exception loading (" << ch
                              << "): " << path << "\n";
                    return nullptr;
                }
            };

            std::shared_ptr<nam::DSP> new_L = load_one("L");
            std::shared_ptr<nam::DSP> new_R = new_L ? load_one("R") : nullptr;

            if (new_L && new_R) {
                // Gain staging, in priority order:
                //  1. Calibrated per-profile pregain (nam_pregain.conf): input
                //     gain measured for unity through-amp level; output unity.
                //  2. Otherwise: optional input pad + makeup from MEASURED
                //     loudness (test tone through the model — metadata is
                //     untrusted) to land at target_loudness_db.
                double in_gain, out_gain;
                char   gbuf[160];
                auto   cal = pregain_db_.find(get_name(wanted));
                if (cal != pregain_db_.end()) {
                    in_gain  = std::pow(10.0, cal->second / 20.0);
                    out_gain = 1.0;
                    std::snprintf(gbuf, sizeof(gbuf),
                                  " (L+R, calibrated pregain %+.1f dB, unity out)",
                                  cal->second);
                } else {
                    in_gain = std::pow(10.0, input_pad_db_ / 20.0);
                    double makeup_db = 0.0;
                    double meas_db   = 0.0;
                    if (loudness_norm_) {
                        meas_db   = measure_loudness_db(new_R.get());
                        makeup_db = target_loudness_db_ - meas_db;
                        new_R->Reset(sample_rate_, max_buffer_);  // clear probe state
                    }
                    out_gain = std::pow(10.0, makeup_db / 20.0);
                    if (loudness_norm_) {
                        std::snprintf(gbuf, sizeof(gbuf),
                                      " (L+R, pad %.1f dB, measured %.1f dB, makeup %+.1f dB)",
                                      input_pad_db_, meas_db, makeup_db);
                    } else {
                        std::snprintf(gbuf, sizeof(gbuf),
                                      " (L+R, pad %.1f dB, no loudness norm)",
                                      input_pad_db_);
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(model_mutex_);
                    active_model_L_  = std::move(new_L);
                    active_model_R_  = std::move(new_R);
                    active_in_gain_  = in_gain;
                    active_out_gain_ = out_gain;
                }
                current_idx_ = wanted;
                current_size_ = want_size;
                std::cerr << "[NAMLoader] Loaded profile [" << wanted
                          << "] " << get_name(wanted)
                          << (want_size == 1 ? " [MICRO]" : " [FULL]") << gbuf << "\n";
            } else {
                // Load failed — keep the previous pair (or passthrough if none;
                // the audio thread bypasses when no pair is active). With no
                // model at all, retry up to 3x at 500 ms before giving up;
                // never a hot loop.
                bool have_model;
                { std::lock_guard<std::mutex> lock(model_mutex_);
                  have_model = (active_model_L_ && active_model_R_); }
                if (fail_idx_ != wanted) { fail_idx_ = wanted; fail_count_ = 0; }
                if (!have_model && fail_count_ < 3) {
                    ++fail_count_;
                    std::cerr << "[NAMLoader] retry " << fail_count_ << "/3 in 500 ms\n";
                    for (int w = 0; w < 50 && !stop_.load(std::memory_order_relaxed); ++w)
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    current_idx_ = wanted;
                    current_size_ = want_size;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ---------------------------------------------------------------------------
// Loudness measurement — 1 kHz sine at -18 dBFS RMS, 1.5 s through the model,
// RMS taken over the last 0.5 s (lets conv/LSTM state and prewarm settle).
// A unity-gain "wire" model measures -18; makeup = target - measured.
// ---------------------------------------------------------------------------

double NamLoader::measure_loudness_db(nam::DSP* model) const {
    const double kRefRmsDb = -18.0;
    const double amp   = std::sqrt(2.0) * std::pow(10.0, kRefRmsDb / 20.0);
    const double freq  = 1000.0;
    const int    block = max_buffer_ > 0 ? max_buffer_ : 128;
    const long   total = static_cast<long>(sample_rate_ * 1.5);
    const long   tail  = static_cast<long>(sample_rate_ * 0.5);

    std::vector<NAM_SAMPLE> in(block), out(block);
    double sumsq = 0.0;
    long   count = 0;
    long   n     = 0;

    while (n < total) {
        int frames = static_cast<int>(std::min<long>(block, total - n));
        for (int i = 0; i < frames; ++i) {
            in[static_cast<size_t>(i)] = static_cast<NAM_SAMPLE>(
                amp * std::sin(2.0 * M_PI * freq * ((n + i) / sample_rate_)));
        }
        NAM_SAMPLE* ip = in.data();
        NAM_SAMPLE* op = out.data();
        model->process(&ip, &op, frames);
        for (int i = 0; i < frames; ++i) {
            if (n + i >= total - tail) {
                double v = static_cast<double>(out[static_cast<size_t>(i)]);
                sumsq += v * v;
                ++count;
            }
        }
        n += frames;
    }

    double rms = (count > 0) ? std::sqrt(sumsq / count) : 0.0;
    if (rms < 1e-8) return kRefRmsDb;   // dead/silent model: no makeup
    return 20.0 * std::log10(rms);
}

// ---------------------------------------------------------------------------
// Level config — <lib_dir>/nam_levels.conf, "key = value" lines:
//   input_pad_db       = -12    ; pre-model pad (more negative = cleaner)
//   target_loudness_db = -18    ; common output loudness for all profiles
// Missing file / keys keep the defaults.  Re-read before every profile load.
// ---------------------------------------------------------------------------

void NamLoader::read_levels_conf() {
    std::ifstream f(lib_dir_ + "/nam_levels.conf");
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        // strip comments (# or ;) and whitespace
        size_t cut = line.find_first_of("#;");
        if (cut != std::string::npos) line = line.substr(0, cut);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r");
            size_t b = s.find_last_not_of(" \t\r");
            return (a == std::string::npos) ? std::string()
                                            : s.substr(a, b - a + 1);
        };
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        if (key == "loudness_norm") {
            loudness_norm_ = !(val == "off" || val == "0" || val == "false");
            continue;
        }
        try {
            double d = std::stod(val);
            if (key == "input_pad_db")            input_pad_db_       = d;
            else if (key == "target_loudness_db") target_loudness_db_ = d;
        } catch (...) {
            // ignore malformed values, keep current setting
        }
    }
}

// ---------------------------------------------------------------------------
// Per-profile calibrated pregain — <lib_dir>/nam_pregain.conf, lines of
// "<profile basename without .nam> = <dB>".  Generated by
// testing/calibrate_pregains.sh.  Re-read before every profile load so a
// re-calibration applies on the next toggle.
// ---------------------------------------------------------------------------

void NamLoader::read_pregain_conf() {
    pregain_db_.clear();
    std::ifstream f(lib_dir_ + "/nam_pregain.conf");
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        size_t cut = line.find_first_of("#;");
        if (cut != std::string::npos) line = line.substr(0, cut);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r");
            size_t b = s.find_last_not_of(" \t\r");
            return (a == std::string::npos) ? std::string()
                                            : s.substr(a, b - a + 1);
        };
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        try {
            pregain_db_[key] = std::stod(val);
        } catch (...) {
            // ignore malformed values
        }
    }
}

// ---------------------------------------------------------------------------
// Directory scanner
// ---------------------------------------------------------------------------

std::vector<std::string> NamLoader::scan_dir(const std::string& lib_dir) {
    std::vector<std::string> result;

#if defined(__cpp_lib_filesystem) || defined(__cpp_lib_experimental_filesystem)
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(lib_dir, ec) || !fs::is_directory(lib_dir, ec)) {
        std::cerr << "[NAMLoader] Library directory not found: " << lib_dir << "\n";
        return result;
    }
    for (auto& entry : fs::recursive_directory_iterator(lib_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        std::string ext = p.extension().string();
        for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".nam") {
            result.push_back(p.string());
        }
    }
#else
    std::function<void(const std::string&)> walk = [&](const std::string& dir) {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            std::string full = dir + "/" + ent->d_name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                walk(full);
            } else if (S_ISREG(st.st_mode)) {
                const char* nm = ent->d_name;
                size_t len = strlen(nm);
                if (len > 4) {
                    char ext[5];
                    for (int i = 0; i < 4; ++i)
                        ext[i] = static_cast<char>(::tolower(static_cast<unsigned char>(nm[len-4+i])));
                    ext[4] = '\0';
                    if (strcmp(ext, ".nam") == 0) {
                        result.push_back(full);
                    }
                }
            }
        }
        closedir(d);
    };
    walk(lib_dir);
#endif

    return result;
}
