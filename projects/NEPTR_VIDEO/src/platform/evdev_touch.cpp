#if defined(NV_PLATFORM_DRM)

#include "platform/evdev_touch.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace nv {
namespace {

bool bitSet(const unsigned long* bits, int bit) {
    const int perLong = 8 * static_cast<int>(sizeof(unsigned long));
    return (bits[bit / perLong] >> (bit % perLong)) & 1ul;
}

constexpr int kBitWords =
    (KEY_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long));

}  // namespace

EvdevTouch::~EvdevTouch() { close(); }

void EvdevTouch::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
}

bool EvdevTouch::openPath(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;

    unsigned long ev[kBitWords]{};
    unsigned long abs[kBitWords]{};
    unsigned long key[kBitWords]{};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev)), ev) < 0 ||
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0 ||
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key)), key) < 0) {
        ::close(fd);
        return false;
    }

    // A touchscreen is: absolute X and Y, plus BTN_TOUCH. The BTN_TOUCH test
    // is what separates it from a tablet-mode switch or an accelerometer,
    // both of which also carry EV_ABS.
    bool isTouch = bitSet(ev, EV_ABS) && bitSet(ev, EV_KEY) &&
                   bitSet(abs, ABS_X) && bitSet(abs, ABS_Y) &&
                   bitSet(key, BTN_TOUCH);
    if (!isTouch) {
        ::close(fd);
        return false;
    }

    input_absinfo ai{};
    if (ioctl(fd, EVIOCGABS(ABS_X), &ai) == 0 && ai.maximum > ai.minimum) {
        minX_ = ai.minimum;
        maxX_ = ai.maximum;
    }
    if (ioctl(fd, EVIOCGABS(ABS_Y), &ai) == 0 && ai.maximum > ai.minimum) {
        minY_ = ai.minimum;
        maxY_ = ai.maximum;
    }

    char name[128] = {0};
    ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);

    fd_ = fd;
    path_ = path;
    // Mount orientation is a property of the hardware, not of the event
    // stream, so there is nothing to detect -- only something to state. The
    // live panel needs none of these; they exist so a re-mounted or swapped
    // panel is one env var rather than a rebuild.
    swapXY_ = std::getenv("NV_TOUCH_SWAP_XY") != nullptr;
    flipX_ = std::getenv("NV_TOUCH_FLIP_X") != nullptr;
    flipY_ = std::getenv("NV_TOUCH_FLIP_Y") != nullptr;

    std::printf("[touch] %s \"%s\"  X %d..%d  Y %d..%d%s%s%s\n", path.c_str(),
                name, minX_, maxX_, minY_, maxY_, swapXY_ ? "  swapXY" : "",
                flipX_ ? "  flipX" : "", flipY_ ? "  flipY" : "");
    std::fflush(stdout);
    return true;
}

bool EvdevTouch::open() {
    if (const char* off = std::getenv("NV_TOUCH")) {
        if (off[0] == '0') {
            std::printf("[touch] disabled by NV_TOUCH=0\n");
            reopen_ = false;
            return false;
        }
    }
    if (const char* want = std::getenv("NV_TOUCH_DEVICE")) {
        if (openPath(want)) {
            reopen_ = true;
            return true;
        }
        std::fprintf(stderr, "[touch] NV_TOUCH_DEVICE=%s is not a touchscreen "
                             "(or cannot be opened: %s)\n",
                     want, std::strerror(errno));
        return false;
    }

    // Scan in order so the choice is deterministic across boots rather than
    // whatever readdir happens to return first.
    std::vector<std::string> nodes;
    if (DIR* d = opendir("/dev/input")) {
        while (dirent* e = readdir(d)) {
            if (std::strncmp(e->d_name, "event", 5) == 0) {
                nodes.push_back(std::string("/dev/input/") + e->d_name);
            }
        }
        closedir(d);
    }
    std::sort(nodes.begin(), nodes.end());
    for (const auto& n : nodes) {
        if (openPath(n)) {
            reopen_ = true;
            return true;
        }
    }

    if (!reopen_) {
        // Said once. If the retry is armed we are looking for a device that was
        // there a moment ago, and a line a second about it is noise.
        std::fprintf(stderr,
                     "[touch] no touchscreen found under /dev/input. The panel "
                     "will not respond to a finger.\n"
                     "        If one is attached, this process needs to be in "
                     "the 'input' group.\n");
    }
    return false;
}

void EvdevTouch::emit(const std::function<void(const PointerEvent&)>& fn,
                      bool moved) {
    if (!haveXY_) return;

    float nx = static_cast<float>(rawX_ - minX_) /
               static_cast<float>(std::max(1, maxX_ - minX_));
    float ny = static_cast<float>(rawY_ - minY_) /
               static_cast<float>(std::max(1, maxY_ - minY_));
    if (swapXY_) std::swap(nx, ny);
    if (flipX_) nx = 1.0f - nx;
    if (flipY_) ny = 1.0f - ny;

    // The glass does not turn when the picture does, so a finger's PHYSICAL
    // position has to be read back through the inverse of the display
    // rotation. Shared with the display side via rotate.h so a test can hold
    // the two against each other -- they are in a shader and in a Linux-only
    // reader, and there is nowhere else the pair can be checked.
    Pt logical = rotateInverse(rotation_, {nx, ny});
    nx = logical.x;
    ny = logical.y;

    PointerEvent e;
    e.outputIndex = outputIndex_;
    e.x = std::clamp(nx, 0.0f, 1.0f);
    e.y = std::clamp(ny, 0.0f, 1.0f);
    e.down = down_;
    e.moved = moved;
    fn(e);
}

void EvdevTouch::poll(const std::function<void(const PointerEvent&)>& fn) {
    if (!fn) return;

    // The panel's node is a uinput device created by demiurge-touch-poll, and
    // that daemon can restart -- when it does the old node disappears and a NEW
    // one takes its place. Without this the engine would notice the read error,
    // give up, and have no touch until the whole process was restarted, which
    // on stage is indistinguishable from the touchscreen dying. Retried at ~1 Hz
    // rather than every frame: a failed open is a syscall and a scan of
    // /dev/input, and there is nothing to gain from doing it 60 times a second.
    if (fd_ < 0) {
        if (!reopen_) return;   // never had one to begin with; nothing to retry
        if (++reopenTick_ < 60) return;
        reopenTick_ = 0;
        if (open()) {
            std::printf("[touch] re-opened after the device came back\n");
            std::fflush(stdout);
        }
        return;
    }

    input_event evs[64];
    for (;;) {
        ssize_t n = read(fd_, evs, sizeof(evs));
        if (n <= 0) {
            // EAGAIN is the normal "no finger" case. Anything else means the
            // node went away (a re-flashed poll driver re-creates its uinput
            // device), so drop it rather than spin on a dead fd.
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                // Not fatal, and usually not even a fault: the poll daemon
                // restarting destroys its uinput node and makes a new one.
                std::fprintf(stderr, "[touch] %s closed: %s -- will retry\n",
                             path_.c_str(), std::strerror(errno));
                close();
                reopenTick_ = 0;
            }
            return;
        }
        size_t count = static_cast<size_t>(n) / sizeof(input_event);
        for (size_t i = 0; i < count; ++i) {
            const input_event& e = evs[i];
            switch (e.type) {
                case EV_ABS:
                    if (e.code == ABS_X) { rawX_ = e.value; haveXY_ = true; dirty_ = true; }
                    else if (e.code == ABS_Y) { rawY_ = e.value; haveXY_ = true; dirty_ = true; }
                    break;
                case EV_KEY:
                    if (e.code == BTN_TOUCH) {
                        bool nowDown = e.value != 0;
                        if (nowDown != down_) {
                            down_ = nowDown;
                            // A press/release is reported as a NON-moved event,
                            // which is the distinction Panel uses to decide
                            // between capturing a slider and dragging it.
                            emit(fn, false);
                        }
                        dirty_ = false;
                    }
                    break;
                case EV_SYN:
                    // Coalesce to the packet boundary. Emitting per axis would
                    // send an event with a stale Y every time X moved.
                    if (e.code == SYN_REPORT && dirty_ && down_) emit(fn, true);
                    dirty_ = false;
                    break;
                default:
                    break;
            }
        }
    }
}

}  // namespace nv

#endif  // NV_PLATFORM_DRM
