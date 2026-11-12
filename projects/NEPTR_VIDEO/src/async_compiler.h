// Off-thread shader compilation.
//
// A chain edit generates new shader source, and a GLSL compile is not fast
// enough to sit on the critical path to a page flip -- one edit would drop
// frames. So the worker holds a context that SHARES objects with the render
// context, builds the programs there, and hands the linked ids back. The
// render loop keeps drawing the old chain until the new one is ready.
//
// Only the newest request matters: if you sweep through effects quickly, the
// intermediate chains are superseded, never queued up.
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gfx/glcommon.h"
#include "platform/platform.h"

namespace nv {

class AsyncCompiler {
public:
    ~AsyncCompiler();

    // Returns false if the platform cannot give the worker a shared context.
    // The caller must then compile inline -- correctness never depends on
    // this succeeding.
    bool start(Platform* plat);
    void stop();
    bool running() const { return running_.load(); }

    // Supersedes any pending request. `token` identifies the result.
    void submit(uint64_t token, std::string vert, std::vector<std::string> frags);

    // Non-blocking. True exactly once per completed request.
    bool poll(uint64_t& token, std::vector<GLuint>& programs);

private:
    void workerMain(Platform* plat);

    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;

    std::atomic<bool> running_{false};
    bool quit_ = false;

    bool hasRequest_ = false;
    uint64_t reqToken_ = 0;
    std::string reqVert_;
    std::vector<std::string> reqFrags_;

    bool hasResult_ = false;
    uint64_t resToken_ = 0;
    std::vector<GLuint> resPrograms_;
};

}  // namespace nv
