// Touch input on the Pi, read straight from evdev.
//
// WHY EVDEV AND NOT THE EXISTING demiurge-touch DAEMON
//
// demiurge-touch already reads this panel -- and it emits ch16 CC80-95, a
// four-zone abstraction for csound. Those messages carry a ZONE, not a
// coordinate, and this engine needs a coordinate: the panel's sliders and the
// HOME menu are laid out from the surface size at runtime, so a quadrant is
// not enough to know which of four rows a finger landed on. There is also no
// ALSA-seq client here at all (README gap 3), so those CCs do not reach this
// process by any route today.
//
// So this is not a second mechanism doing the daemon's job. The daemon turns
// touch into MIDI for the audio engine; this turns touch into pointer
// coordinates for the GL surface, which is the seam every other input in this
// codebase already goes through (Platform::onPointer). Filling it fixes the
// panel for the whole engine, not just HOME.
//
// Two readers on one evdev node do NOT conflict: the kernel gives each open fd
// its own event queue and delivers a copy to all of them. That is already
// relied on in production here -- NEPTR_phase4/touch_evdev.py reads the same
// node demiurge-touch reads, and engine-ui.md states it outright. Verified on
// the device: demiurge-touch was left running while this reader was open.
//
// CALIBRATION comes free. The panel's node is created by demiurge-touch-poll
// (a uinput device, because the ADS7846's PENIRQ pin is dead and it is polled
// over SPI instead), and that driver stages the saved calibration into the
// device's ADVERTISED ABS range. So EVIOCGABS returns the calibrated min/max
// and normalising against it lands on the right pixel with no second config
// reader and nothing to keep in sync.
#pragma once

#if defined(NV_PLATFORM_DRM)

#include <functional>
#include <string>

#include "platform/platform.h"
#include "platform/rotate.h"

namespace nv {

class EvdevTouch {
public:
    ~EvdevTouch();

    // Finds and opens the touchscreen. NV_TOUCH_DEVICE forces a node;
    // NV_TOUCH=0 disables it entirely. Returns false, with one line on stderr,
    // when there is nothing to open -- a missing panel is not fatal.
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    const std::string& devicePath() const { return path_; }

    // Which output a touch belongs to. The touchscreen IS the small panel, so
    // this is the Panel-role output when there is one and output 0 otherwise.
    void setOutputIndex(int i) { outputIndex_ = i; }

    // Degrees CLOCKWISE that the IMAGE is turned by. The touch panel is bolted
    // to the glass, so its axes do NOT turn with the picture -- this reader
    // therefore applies the INVERSE, and a tap lands on what is under the
    // finger. Skipping this is the failure the user reads as "the touchscreen
    // is broken" rather than "the screen is sideways".
    void setRotation(int deg) { rotation_ = deg; }

    // Drains everything pending and calls `fn` per gesture change. Never
    // blocks: the render loop cannot wait on a finger.
    void poll(const std::function<void(const PointerEvent&)>& fn);

private:
    bool openPath(const std::string& path);
    void emit(const std::function<void(const PointerEvent&)>& fn, bool moved);

    int fd_ = -1;
    std::string path_;

    int rawX_ = 0, rawY_ = 0;
    int minX_ = 0, maxX_ = 4095;
    int minY_ = 0, maxY_ = 4095;
    int rotation_ = 0;
    bool swapXY_ = false;
    bool flipX_ = false;
    bool flipY_ = false;

    bool down_ = false;
    bool haveXY_ = false;
    bool dirty_ = false;   // something changed since the last SYN_REPORT
    int outputIndex_ = 0;
    // Armed once a device has been seen, so a node that VANISHES is retried
    // while a machine that simply has no touchscreen is not scanned forever.
    bool reopen_ = false;
    int reopenTick_ = 0;
};

}  // namespace nv

#endif  // NV_PLATFORM_DRM
