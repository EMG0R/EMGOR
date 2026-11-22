#include "nam_loader.h"

#include <NAM/get_dsp.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

NamLoader::NamLoader() = default;

NamLoader::~NamLoader() {
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
    sample_rate_ = sample_rate;
    max_buffer_  = max_buffer;

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
}

std::shared_ptr<nam::DSP> NamLoader::get_active() const {
    std::lock_guard<std::mutex> lock(model_mutex_);
    return active_model_;
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

void NamLoader::loader_loop() {
    while (!stop_.load(std::memory_order_relaxed)) {
        int wanted = requested_idx_.load(std::memory_order_relaxed);

        if (wanted != current_idx_ && wanted >= 0 &&
            wanted < static_cast<int>(paths_.size()))
        {
            const std::string& path = paths_[static_cast<size_t>(wanted)];

            std::shared_ptr<nam::DSP> new_model;
            try {
                // nam::get_dsp() is the canonical factory in NeuralAmpModelerCore.
                std::unique_ptr<nam::DSP> raw = nam::get_dsp(std::filesystem::path(path));
                if (raw) {
                    raw->Reset(sample_rate_, max_buffer_);
                    new_model = std::move(raw);
                } else {
                    std::cerr << "[NAMLoader] get_dsp returned null for: "
                              << path << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "[NAMLoader] Failed to load '" << path
                          << "': " << e.what() << "\n";
                current_idx_ = wanted;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } catch (...) {
                std::cerr << "[NAMLoader] Unknown exception loading: "
                          << path << "\n";
                current_idx_ = wanted;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (new_model) {
                {
                    std::lock_guard<std::mutex> lock(model_mutex_);
                    active_model_ = std::move(new_model);
                }
                current_idx_ = wanted;
                std::cerr << "[NAMLoader] Loaded profile [" << wanted
                          << "] " << get_name(wanted) << "\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
