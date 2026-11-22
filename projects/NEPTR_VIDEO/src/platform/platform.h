// The platform seam.
//
// Everything above this line is one codebase. Below it there are exactly two
// implementations: GLFW+GL4.1 for Mac development, and libdrm/GBM/EGL for the
// Pi, where we hold DRM master and drive the page flip ourselves.
//
// The seam is deliberately narrow. It knows about outputs and frames. It does
// not know about fractals, effects, or state.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nv {

// Role decides what gets drawn, never how big it is. Sizes always come from
// the mode we actually detected at runtime -- there is no such thing as a
// hardcoded resolution anywhere above this seam.
enum class OutputRole {
    Scene,  // the projector / big screen: the synth output, nothing else
    Panel,  // the onboard touch panel: control surface only
};

struct Output {
    std::string name;          // "HDMI-A-1", "HDMI-A-2", "window"
    int width = 0;
    int height = 0;
    double refreshHz = 60.0;
    OutputRole role = OutputRole::Scene;
    bool connected = false;

    float aspect() const {
        return height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    }
};

struct KeyEvent {
    int key = 0;        // platform-independent: ASCII for printable, >255 for special
    bool pressed = false;
    bool shift = false;
    bool ctrl = false;
};

// Special keys, above the ASCII range.
enum : int {
    KeyEscape = 256, KeyLeft, KeyRight, KeyUp, KeyDown, KeyTab,
};

struct PointerEvent {
    int outputIndex = 0;
    float x = 0.0f;  // normalized 0..1 across that output, so panel size is irrelevant
    float y = 0.0f;
    bool down = false;
    bool moved = false;
};

class Platform {
public:
    virtual ~Platform() = default;

    // Brings up the GL context and enumerates outputs. Returns false with a
    // message on stderr if the machine cannot render at all.
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Current outputs. Re-read after pollEvents() reports a hotplug: the
    // projector coming and going is a normal, expected event, not an error.
    virtual const std::vector<Output>& outputs() const = 0;

    // Largest width/height across all outputs, present or plausibly future.
    // Every framebuffer is allocated once at this size and sub-viewported
    // below it, which is what makes the resolution control hitch-free.
    virtual void maxSurface(int& w, int& h) const = 0;

    // Index of the GL context currently bound on this thread.
    //
    // Textures, buffers and programs are shared between contexts; VERTEX
    // ARRAY OBJECTS ARE NOT. So anything holding a VAO needs one per context,
    // and this is how it knows which. The Pi drives both outputs from a
    // single context (one context, two surfaces) and always returns 0; two
    // windows on the desktop are two contexts.
    virtual int currentContext() const = 0;
    virtual int contextCount() const = 0;

    // Binds output i's default framebuffer and sets the full-surface viewport.
    virtual void beginFrame(int outputIndex) = 0;
    // Presents. On the Pi this is a real vblank-synced page flip.
    virtual void endFrame(int outputIndex) = 0;

    // Returns false when the user asked to quit. Sets hotplugged if the set of
    // connected outputs changed this tick.
    virtual bool pollEvents(bool& hotplugged) = 0;

    // Makes a context that SHARES objects with the render context current on
    // the CALLING thread. Called once from the compiler worker so a shader
    // can be built without the render loop waiting on it. Returns false if
    // the platform cannot provide one, in which case compilation falls back
    // to happening inline.
    virtual bool makeWorkerContextCurrent() = 0;

    // Screen rotation, in DEGREES CLOCKWISE (0/90/180/270).
    //
    // THE CONVENTION IS CLOCKWISE, stated here because every layer in this
    // stack expresses it that way (the kernel's `video=...,rotate=`,
    // config.txt's quarter-turn `display_hdmi_rotate`) and a reader who
    // assumes counter-clockwise gets a 180-degree error that looks like a
    // completely different bug. A panel mounted 90 degrees COUNTER-clockwise
    // therefore wants 270.
    //
    // This is a property of how the panel is BOLTED DOWN, not of the software,
    // so it defaults to 0 and is set per-machine (NV_ROTATE). Above this seam
    // nothing knows about it: outputs() already reports the ROTATED logical
    // size, so all layout is computed for the screen the user is looking at.
    // Only the two passes that reach the physical framebuffer -- the scene
    // present and the panel -- apply the turn, and the touch reader applies
    // its inverse so a tap lands on what is under the finger.
    virtual int rotation() const { return 0; }

    // Hands the display to another process, and takes it back.
    //
    // KMS allows exactly ONE master, so a child that needs the screen (the
    // audio UI, RetroArch) cannot have it while we hold it. HOME drops master
    // for the duration of the child and re-takes it after. acquireDisplay()
    // must also invalidate any modeset we were relying on: the child sets its
    // own mode on our CRTC, so resuming with a page flip would present onto a
    // CRTC that is no longer programmed the way we left it.
    //
    // Both return false on a platform with no such concept (the desktop, where
    // a compositor owns the screen and the child gets its own window). That is
    // not an error -- it means "nothing to hand over".
    virtual bool releaseDisplay() { return false; }
    virtual bool acquireDisplay() { return false; }

    virtual void onKey(std::function<void(const KeyEvent&)> fn) = 0;
    virtual void onPointer(std::function<void(const PointerEvent&)> fn) = 0;

    // Seconds since init, monotonic. The single time source for the whole
    // engine so audio-rate modulation can be phase-locked to it later.
    virtual double now() const = 0;
};

// Built for whichever backend was compiled in.
Platform* createPlatform();

}  // namespace nv
