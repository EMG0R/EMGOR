// nam_loader.cpp — background profile loading, gain staging, and load-time
// loudness measurement for the csound-nam opcodes.

#include "nam_loader.h"

#include <NAM/get_dsp.h>
#include <NAM/slimmable.h>

#include <algorithm>
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

#include "di_probe.h"
#include "kweight.h"

// AudioDSPTools' ResamplingContainer is lifted from iPlug2 and expects two
// identifiers from that framework; provided here standalone.
namespace iplug {
inline constexpr double PI = 3.14159265358979323846;
}
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 512
#endif
#include <dsp/ResamplingContainer/ResamplingContainer.h>

// ResampledModel — wraps a nam::DSP whose trained sample rate differs from
// the engine rate inside AudioDSPTools' Lanczos ResamplingContainer (the
// same mechanism the official NAM plugin uses): the model always runs at its
// native rate, the engine talks to it at the engine rate. only built when
// the rates differ; matched-rate models are used directly.
namespace {
class ResampledModel : public nam::DSP {
public:
    ResampledModel(std::unique_ptr<nam::DSP> inner, double engine_sr,
                   int max_block)
        : nam::DSP(1, 1, engine_sr),
          inner_(std::move(inner)),
          rc_(inner_->GetExpectedSampleRate())
    {
        // give the inner model headroom for the resampled block size, then
        // prime the container (also computes its latency).
        const double ratio =
            inner_->GetExpectedSampleRate() / engine_sr;
        const int inner_block =
            static_cast<int>(std::ceil(max_block * ratio)) + 8;
        inner_->Reset(inner_->GetExpectedSampleRate(), inner_block);
        rc_.Reset(engine_sr, max_block);
    }

    void process(NAM_SAMPLE** input, NAM_SAMPLE** output,
                 const int num_frames) override {
        rc_.ProcessBlock(input, output, num_frames,
                         [this](NAM_SAMPLE** in, NAM_SAMPLE** out, int n) {
                             inner_->process(in, out, n);
                         });
    }

    void Reset(const double sampleRate, const int maxBufferSize) override {
        const double ratio =
            inner_->GetExpectedSampleRate() / sampleRate;
        const int inner_block =
            static_cast<int>(std::ceil(maxBufferSize * ratio)) + 8;
        inner_->Reset(inner_->GetExpectedSampleRate(), inner_block);
        rc_.Reset(sampleRate, maxBufferSize);
    }

private:
    std::unique_ptr<nam::DSP> inner_;
    dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> rc_;
};
} // namespace

NamLoader::NamLoader() = default;

NamLoader::~NamLoader() {
    stop_.store(true, std::memory_order_relaxed);
    if (loader_thread_.joinable()) {
        loader_thread_.join();
    }
}

bool NamLoader::init(const std::string& lib_dir, int initial_idx,
                     double sample_rate, int max_buffer) {
    sample_rate_ = sample_rate;
    max_buffer_  = max_buffer;
    lib_dir_     = lib_dir;

    // a path ending in ".nam" that is a regular file is a single-profile
    // library (drag-and-drop / direct-file use); otherwise scan as a dir.
    {
        std::error_code ec;
        if (lib_dir.size() > 4 &&
            std::filesystem::is_regular_file(lib_dir, ec)) {
            std::string ext = std::filesystem::path(lib_dir).extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (ext == ".nam") {
                paths_.push_back(lib_dir);
                lib_dir_ = std::filesystem::path(lib_dir).parent_path().string();
            }
        }
    }
    if (paths_.empty()) {
        paths_ = scan_dir(lib_dir);
        std::sort(paths_.begin(), paths_.end());
    }

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

void NamLoader::set_index(int idx) {
    if (paths_.empty()) return;
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(paths_.size()))
        idx = static_cast<int>(paths_.size()) - 1;
    requested_idx_.store(idx, std::memory_order_relaxed);
}

void NamLoader::set_path(const std::string& path) {
    if (path.empty()) return;
    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        requested_path_ = path;
    }
    path_pending_.store(true, std::memory_order_relaxed);
}

std::string NamLoader::get_active_name() const {
    std::lock_guard<std::mutex> lock(path_mutex_);
    return active_name_;
}

std::string NamLoader::get_active_path() const {
    std::lock_guard<std::mutex> lock(path_mutex_);
    return active_path_;
}

void NamLoader::step(int delta) {
    std::string target;
    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        if (nav_paths_.empty() || nav_idx_ < 0) return;
        int idx = nav_idx_ + delta;
        if (idx < 0) idx = 0;
        if (idx >= static_cast<int>(nav_paths_.size()))
            idx = static_cast<int>(nav_paths_.size()) - 1;
        if (idx == nav_idx_) return;
        target = nav_paths_[static_cast<size_t>(idx)];
        requested_path_ = target;
    }
    path_pending_.store(true, std::memory_order_relaxed);
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

void NamLoader::loader_loop() {
    while (!stop_.load(std::memory_order_relaxed)) {
        // direct-path requests (drag-and-drop) take priority over index ones.
        if (path_pending_.load(std::memory_order_relaxed)) {
            std::string path;
            {
                std::lock_guard<std::mutex> lock(path_mutex_);
                path = requested_path_;
            }
            path_pending_.store(false, std::memory_order_relaxed);
            // read level confs from the dropped file's own directory so a
            // nam_levels.conf beside the file still applies.
            std::string parent =
                std::filesystem::path(path).parent_path().string();
            if (!parent.empty()) lib_dir_ = parent;
            load_and_install(path);
            // invalidate the index so a later set_index reloads cleanly.
            current_idx_ = -1;
            requested_idx_.store(-1, std::memory_order_relaxed);
            continue;
        }

        int wanted = requested_idx_.load(std::memory_order_relaxed);

        if (wanted != current_idx_ && wanted >= 0 &&
            wanted < static_cast<int>(paths_.size()))
        {
            load_and_install(paths_[static_cast<size_t>(wanted)]);
            // on load failure keep the previous pair; marking the index as
            // current avoids retrying in a hot loop.
            current_idx_ = wanted;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool NamLoader::load_and_install(const std::string& path) {
    // pick up any tuning edits before computing gains.
    read_levels_conf();
    read_pregain_conf();

    // basename without directory or .nam extension — used for the pregain
    // lookup and the UI-facing active-name string.
    std::string base = std::filesystem::path(path).stem().string();

    // load two independent instances of the same profile — the model is
    // stateful, so L and R must never share one instance.
    auto load_one = [&](const char* ch) -> std::shared_ptr<nam::DSP> {
        try {
            std::unique_ptr<nam::DSP> raw =
                nam::get_dsp(std::filesystem::path(path));
            if (!raw) {
                std::cerr << "[NAMLoader] get_dsp returned null (" << ch
                          << ") for: " << path << "\n";
                return nullptr;
            }
            // SlimmableContainer models default to submodel 0 (lo-fi);
            // 1.0 selects the highest-quality submodel. safe here — off
            // the audio thread.
            if (auto* slim = dynamic_cast<nam::SlimmableModel*>(raw.get())) {
                slim->SetSlimmableSize(1.0);
            }
            // Reset also prewarms (flushes conv/LSTM state) off-thread.
            raw->Reset(sample_rate_, max_buffer_);
            double want = raw->GetExpectedSampleRate();
            if (want > 0 && want != sample_rate_) {
                // rate mismatch: run the model at its trained rate inside a
                // Lanczos resampling container instead of feeding it wrong-
                // clocked audio (which skews its whole learned tone).
                std::cerr << "[NAMLoader] '" << path << "' trained at "
                          << want << " Hz, engine at " << sample_rate_
                          << " Hz — resampling around the model (" << ch
                          << ")\n";
                return std::shared_ptr<nam::DSP>(
                    new ResampledModel(std::move(raw), sample_rate_,
                                       max_buffer_));
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

    if (!new_L || !new_R) return false;

    // gain staging, in priority order:
    //  1. calibrated per-profile pregain (nam_pregain.conf): input gain
    //     measured for unity through-amp level; output unity.
    //  2. otherwise: optional input pad + makeup from MEASURED loudness
    //     (probe through the model — profile metadata is untrusted) to
    //     land at target_loudness_db.
    double in_gain, out_gain;
    char   gbuf[160];
    auto   cal = pregain_db_.find(base);
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
    // rebuild the folder-navigation list: sorted .nam siblings of this file,
    // with nav_idx_ pointing at the file itself (for step(±1)).
    std::vector<std::string> nav;
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path parent = fs::path(path).parent_path();
        for (auto& entry : fs::directory_iterator(parent, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (ext == ".nam") nav.push_back(entry.path().string());
        }
        std::sort(nav.begin(), nav.end());
    }
    int nav_idx = -1;
    for (size_t i = 0; i < nav.size(); ++i) {
        if (nav[i] == path) { nav_idx = static_cast<int>(i); break; }
    }
    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        active_name_ = base;
        active_path_ = path;
        nav_paths_   = std::move(nav);
        nav_idx_     = nav_idx;
    }
    std::cerr << "[NAMLoader] Loaded " << base << gbuf << "\n";
    return true;
}

// loudness measurement — perceptual (ITU-R BS.1770 K-weighted), not raw RMS:
// K-weighting tracks perceived loudness across bright vs dark captures far
// better than RMS. makeup = target - measured.
double NamLoader::measure_loudness_db(nam::DSP* model) const {
    const double kRefRmsDb = -18.0;

    // probe: the standardized NAM reference DI (real guitar, embedded from
    // NeuralAmpModelerCore/example_audio/input.wav), scaled to kRefRmsDb —
    // matching a typical playing level matters, because how much an amp
    // compresses (and therefore how loud it measures) depends strongly on
    // drive level. linearly resampled when the engine rate differs from
    // 48 kHz (loudness probing only; live audio is untouched).
    double di_ms = 0.0;
    for (int i = 0; i < kDIProbeLen; ++i)
        di_ms += static_cast<double>(kDIProbe[i]) * kDIProbe[i];
    const double di_rms  = std::sqrt(di_ms / kDIProbeLen);
    const double di_gain = std::pow(10.0, kRefRmsDb / 20.0) /
                           std::max(di_rms, 1e-9);
    const double ratio = kDIProbeRate / sample_rate_;
    const long   di_frames = static_cast<long>(kDIProbeLen / ratio);

    const int  block = max_buffer_ > 0 ? max_buffer_ : 128;
    const long total = di_frames;
    const long skip  = static_cast<long>(sample_rate_ * 0.1);  // prewarm (s * sr)

    KwChain kw;
    kw.init(sample_rate_);

    std::vector<NAM_SAMPLE> in(static_cast<size_t>(block)),
                            out(static_cast<size_t>(block));

    // 100 ms measurement blocks; final loudness is the mean of blocks within
    // 10 dB of the loudest block (BS.1770-style relative gating), so decay
    // tails don't drag quiet-but-clean captures down.
    const long blk_len = static_cast<long>(sample_rate_ * 0.1);
    std::vector<double> blk_ms;
    double cur_sum = 0.0;
    long   cur_cnt = 0;

    long n = 0;
    while (n < total) {
        int frames = static_cast<int>(std::min<long>(block, total - n));
        for (int i = 0; i < frames; ++i) {
            double srcpos = (n + i) * ratio;
            long   s0 = static_cast<long>(srcpos);
            double fr = srcpos - s0;
            long   s1 = std::min<long>(s0 + 1, kDIProbeLen - 1);
            double v = (1.0 - fr) * kDIProbe[s0] + fr * kDIProbe[s1];
            in[static_cast<size_t>(i)] = static_cast<NAM_SAMPLE>(v * di_gain);
        }
        NAM_SAMPLE* ip = in.data();
        NAM_SAMPLE* op = out.data();
        model->process(&ip, &op, frames);
        for (int i = 0; i < frames; ++i) {
            double w = kw.tick(static_cast<double>(out[static_cast<size_t>(i)]));
            if (n + i >= skip) {
                cur_sum += w * w;
                if (++cur_cnt >= blk_len) {
                    blk_ms.push_back(cur_sum / cur_cnt);
                    cur_sum = 0.0;
                    cur_cnt = 0;
                }
            }
        }
        n += frames;
    }
    if (cur_cnt > blk_len / 2) blk_ms.push_back(cur_sum / cur_cnt);

    if (blk_ms.empty()) return kRefRmsDb;
    double peak_ms = 0.0;
    for (double m : blk_ms) peak_ms = std::max(peak_ms, m);
    if (peak_ms < 1e-16) return kRefRmsDb;  // dead/silent model: no makeup
    const double gate = peak_ms * 0.1;      // -10 dB relative gate
    double sum = 0.0;
    int    cnt = 0;
    for (double m : blk_ms) {
        if (m >= gate) { sum += m; ++cnt; }
    }
    double ms = (cnt > 0) ? sum / cnt : peak_ms;
    return 10.0 * std::log10(ms);
}

// level config — <lib_dir>/nam_levels.conf, "key = value" lines:
//   input_pad_db       = -12    ; pre-model pad, dB (more negative = cleaner)
//   target_loudness_db = -18    ; common output loudness for all profiles, dB
// missing file / keys keep the defaults. re-read before every profile load.
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

// per-profile calibrated pregain — <lib_dir>/nam_pregain.conf, lines of
// "<profile basename without .nam> = <dB>". re-read before every profile
// load so a re-calibration applies on the next toggle.
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
