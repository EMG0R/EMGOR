// Running another program on our display.
//
// This is the crux of HOME, and it is entirely about ONE fact: KMS allows
// exactly one DRM master. While neptr-video holds it, the audio UI and
// RetroArch cannot open the screen at all -- they fail at their own
// drmSetMaster / KMSDRM init and either exit or draw nowhere. So the sequence
// is not "start the child", it is:
//
//   drmDropMaster  ->  fork/exec  ->  wait  ->  drmSetMaster  ->  re-modeset
//
// The re-modeset matters as much as the drop: the child sets its own mode on
// our CRTC, so the framebuffer we were flipping is no longer what is scanning
// out. Coming back with a page flip onto a CRTC someone else re-programmed
// shows the child's last frame, or nothing.
//
// Taking master back is a RACE, not an instruction. The kernel reaps the
// child's DRM fd asynchronously after exit, so drmSetMaster can legitimately
// return EBUSY/EPERM for a moment -- the same reclaim race the arcade
// supervisor documents around ui-restore. Hence the retry rather than one
// attempt and a failure message.
#pragma once

#include <string>

namespace nv {

class Platform;

// A child holding the display. Non-blocking so the engine can keep reading
// the control bus while the child runs -- which is what makes "hold the
// joystick to come back" work even from a mode that owns the screen.
class Launcher {
public:
    ~Launcher();

    // Runs `cmd` through /bin/sh -c so a command line out of live.conf works
    // verbatim, quotes and all. Returns false if the fork failed.
    bool start(const std::string& cmd);

    // Reaps without blocking. False once the child is gone.
    bool running();

    // SIGTERM, then SIGKILL after `graceSeconds`. Terminating politely is not
    // manners: the child has to run its own shutdown far enough to drop DRM
    // master and close its card fd, or we inherit the reclaim race.
    void terminate(double graceSeconds = 8.0);

    int exitCode() const { return exitCode_; }

private:
    int pid_ = -1;
    int exitCode_ = -1;
};

// Hands the display over, runs the command to completion, takes it back.
//
// Blocks, but calls `pump` (if given) roughly every 20 ms while waiting, so
// the caller can drain its control socket and ask to come back early. `pump`
// returning false means "kill the child now".
//
// Returns the child's exit code, or -1 if it could not be started. The display
// is re-acquired on EVERY path out, including failure -- a menu that loses the
// screen because the thing it launched was missing is worse than one that
// never launched anything.
int runModeWithDisplay(Platform& plat, const std::string& cmd,
                       bool (*pump)(void* user), void* user);

}  // namespace nv
