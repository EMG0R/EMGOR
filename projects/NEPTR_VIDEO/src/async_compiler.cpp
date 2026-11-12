#include "async_compiler.h"

#include <cstdio>

#include "gfx/shader.h"

namespace nv {

AsyncCompiler::~AsyncCompiler() { stop(); }

bool AsyncCompiler::start(Platform* plat) {
    if (running_.load()) return true;

    // Probe on the worker itself: whether a shared context can be made
    // current is a property of the calling thread, so asking from here would
    // answer the wrong question.
    std::atomic<bool> ready{false};
    std::atomic<bool> ok{false};

    quit_ = false;
    thread_ = std::thread([this, plat, &ready, &ok]() {
        if (!plat->makeWorkerContextCurrent()) {
            ok.store(false);
            ready.store(true);
            return;
        }
        ok.store(true);
        running_.store(true);
        ready.store(true);
        workerMain(plat);
    });

    while (!ready.load()) {
        std::this_thread::yield();
    }
    if (!ok.load()) {
        if (thread_.joinable()) thread_.join();
        std::fprintf(stderr,
                     "[compile] no shared context available; compiling inline\n");
        return false;
    }
    return true;
}

void AsyncCompiler::stop() {
    if (!thread_.joinable()) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        quit_ = true;
    }
    cv_.notify_all();
    thread_.join();
    running_.store(false);
}

void AsyncCompiler::submit(uint64_t token, std::string vert,
                           std::vector<std::string> frags) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        // Overwrite rather than queue. Sweeping through effects must not
        // build a backlog of chains nobody will ever see.
        hasRequest_ = true;
        reqToken_ = token;
        reqVert_ = std::move(vert);
        reqFrags_ = std::move(frags);
    }
    cv_.notify_one();
}

bool AsyncCompiler::poll(uint64_t& token, std::vector<GLuint>& programs) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!hasResult_) return false;
    hasResult_ = false;
    token = resToken_;
    programs = std::move(resPrograms_);
    resPrograms_.clear();
    return true;
}

void AsyncCompiler::workerMain(Platform* plat) {
    (void)plat;
    for (;;) {
        uint64_t token = 0;
        std::string vert;
        std::vector<std::string> frags;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this]() { return quit_ || hasRequest_; });
            if (quit_) return;
            hasRequest_ = false;
            token = reqToken_;
            vert = std::move(reqVert_);
            frags = std::move(reqFrags_);
        }

        std::vector<GLuint> built;
        built.reserve(frags.size());
        bool allOk = true;
        for (size_t i = 0; i < frags.size(); ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "chain_pass_%zu", i);
            GLuint p = buildProgramRaw(vert, frags[i], name);
            if (!p) {
                allOk = false;
                break;
            }
            built.push_back(p);
        }

        if (!allOk) {
            for (GLuint p : built) glDeleteProgram(p);
            // A chain that will not compile must leave the picture alone, so
            // simply publish nothing and let the old programs keep drawing.
            continue;
        }

        // Object sharing between contexts is only guaranteed to be visible
        // after the producing context has finished. Without this the render
        // thread can bind a program the driver has not published yet.
        glFinish();

        {
            std::lock_guard<std::mutex> lk(mu_);
            // If a newer request arrived while we were compiling, drop this
            // result: it is already stale.
            if (hasRequest_) {
                for (GLuint p : built) glDeleteProgram(p);
                continue;
            }
            // Anything the render thread never collected must not leak.
            for (GLuint p : resPrograms_) glDeleteProgram(p);
            resPrograms_ = std::move(built);
            resToken_ = token;
            hasResult_ = true;
        }
    }
}

}  // namespace nv
