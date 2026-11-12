#include "home/launcher.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "platform/platform.h"

namespace nv {
namespace {

void sleepSeconds(double s) {
    if (s <= 0.0) return;
    timespec t{};
    t.tv_sec = static_cast<time_t>(s);
    t.tv_nsec = static_cast<long>((s - static_cast<double>(t.tv_sec)) * 1e9);
    nanosleep(&t, nullptr);
}

double monotonic() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

}  // namespace

Launcher::~Launcher() {
    if (pid_ > 0) terminate(2.0);
}

bool Launcher::start(const std::string& cmd) {
    if (cmd.empty()) return false;
    pid_t p = fork();
    if (p < 0) {
        std::fprintf(stderr, "[home] fork failed: %s\n", std::strerror(errno));
        return false;
    }
    if (p == 0) {
        // Own process group, so terminate() can reach a shell's children too:
        // `sh -c "python foo.py"` leaves the real program one level down, and
        // signalling only the shell would orphan it still holding the display.
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        std::fprintf(stderr, "[home] exec failed: %s\n", std::strerror(errno));
        _exit(127);
    }
    setpgid(p, p);  // also here: whichever call wins, the group exists
    pid_ = p;
    exitCode_ = -1;
    return true;
}

bool Launcher::running() {
    if (pid_ <= 0) return false;
    int status = 0;
    pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == 0) return true;
    if (r < 0 && errno == EINTR) return true;
    if (r > 0) {
        exitCode_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }
    pid_ = -1;
    return false;
}

void Launcher::terminate(double graceSeconds) {
    if (pid_ <= 0) return;
    kill(-pid_, SIGTERM);
    double deadline = monotonic() + graceSeconds;
    while (monotonic() < deadline) {
        if (!running()) return;
        sleepSeconds(0.05);
    }
    std::fprintf(stderr, "[home] child did not exit on SIGTERM -- SIGKILL\n");
    kill(-pid_, SIGKILL);
    for (int i = 0; i < 100 && running(); ++i) sleepSeconds(0.05);
}

int runModeWithDisplay(Platform& plat, const std::string& cmd,
                       bool (*pump)(void* user), void* user) {
    if (cmd.empty()) return -1;

    const bool hadMaster = plat.releaseDisplay();
    if (!hadMaster) {
        // On the desktop there is no master to drop and this is not an error;
        // on the Pi it means the child will get a black screen, and saying so
        // is far better than the child failing with its own opaque message.
        std::fprintf(stderr,
                     "[home] display not released (platform has no DRM master "
                     "to drop) -- launching anyway\n");
    } else {
        std::printf("[home] dropped DRM master\n");
        std::fflush(stdout);
    }

    Launcher child;
    int code = -1;
    if (!child.start(cmd)) {
        std::fprintf(stderr, "[home] could not start: %s\n", cmd.c_str());
    } else {
        std::printf("[home] running: %s\n", cmd.c_str());
        std::fflush(stdout);
        while (child.running()) {
            if (pump && !pump(user)) {
                std::printf("[home] return-to-HOME requested -- closing child\n");
                std::fflush(stdout);
                child.terminate();
                break;
            }
            sleepSeconds(0.02);
        }
        code = child.exitCode();
    }

    if (!plat.acquireDisplay()) {
        std::fprintf(stderr,
                     "[home] could NOT take DRM master back. The display is "
                     "gone until this process restarts.\n");
        // So restart. Carrying on means drawing into a CRTC we do not own:
        // every frame fails at drmModeSetCrtc, at frame rate, forever — a
        // black screen with no way back and 27,000 journal lines a minute
        // (observed 2026-08-16 leaving GAMES). Exiting non-zero hands the
        // problem to the one mechanism that can actually fix it:
        // neptr-video.service is Restart=on-failure/RestartSec=3, and a fresh
        // process takes master cleanly. The rig is back at HOME in three
        // seconds instead of needing a power cycle.
        //
        // Only when we genuinely HAD master: on the desktop backend
        // release/acquire are both no-ops that report false, and quitting the
        // app every time a mode exits would be a bug invented on the machine
        // this never runs on.
        if (hadMaster) {
            std::fflush(nullptr);
            _exit(70);
        }
    } else {
        std::printf("[home] re-took DRM master\n");
        std::fflush(stdout);
    }
    return code;
}

}  // namespace nv
