// Desktop backend.
//
// Not the performance target -- the Pi is. This exists so the whole engine
// above the seam can be iterated on without a deploy, and so the same shaders
// that will run on V3D get compiled by a second, stricter driver first, which
// catches a surprising amount.
//
// It opens TWO windows by default, mirroring the Pi's two outputs:
//
//   SCENE    pure visuals, vsync-locked. Nothing is ever drawn over it.
//   CONTROL  the fader surface.
//
// So the desktop exercises the same dual-surface path that runs on the device,
// rather than a single-window arrangement that only exists here.
#if defined(NV_PLATFORM_GLFW)

// glcommon first: GLFW must not pull in its own GL headers on top of ours.
#include "gfx/glcommon.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

#include "platform/platform.h"

namespace nv {
namespace {

class GlfwPlatform : public Platform {
public:
    bool init() override {
        glfwSetErrorCallback([](int code, const char* msg) {
            std::fprintf(stderr, "[glfw] %d: %s\n", code, msg);
        });
        if (!glfwInit()) return false;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        // HEADLESS: render without ever putting a window on the developer's
        // screen. A test run that captures frames still needs a real GL
        // context and a real swap chain -- but it has no business stealing
        // focus from whoever is using the machine, and a suite that renders
        // for two minutes (motion_bank alone is 78s) makes the desktop
        // unusable while it runs.
        //
        // Implied by NV_SHOT and NV_SELFTEST, because every caller of those is
        // a capture or a test, never someone watching. NV_WINDOW=1 forces a
        // visible window back for the times you DO want to watch.
        const bool wantWindow = std::getenv("NV_WINDOW") != nullptr;
        headless_ = !wantWindow && (std::getenv("NV_SHOT") ||
                                    std::getenv("NV_SELFTEST") ||
                                    std::getenv("NV_HEADLESS"));
        if (headless_) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            // Belt and braces: an unmapped window cannot be focused, but the
            // hints are free and macOS has surprised us before.
            glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
            glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        }

        // NV_SINGLE_WINDOW collapses to one surface, where TAB toggles the
        // panel as an overlay.
        singleWindow_ = std::getenv("NV_SINGLE_WINDOW") != nullptr;
        if (const char* r = std::getenv("NV_ROTATE")) {
            int v = std::atoi(r);
            if (v == 0 || v == 90 || v == 180 || v == 270) rotation_ = v;
        }
        // Force a specific window size, for reproducing a device's panel
        // geometry on the desktop without the device.
        int forceW = 0, forceH = 0;
        if (const char* g = std::getenv("NV_GEOM")) sscanf(g, "%dx%d", &forceW, &forceH);

        // Size the pair to the actual work area rather than to fixed numbers,
        // so both windows land fully on screen on a laptop panel as well as
        // on a large display.
        int wax = 0, way = 0, waw = 1440, wah = 900;
        glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &wax, &way, &waw, &wah);

        const int gap = 16;
        const int margin = 40;
        int panelW = singleWindow_ ? 0 : 460;
        int sceneW = waw - margin * 2 - (singleWindow_ ? 0 : panelW + gap);
        int sceneH = wah - margin * 2;
        // Keep the scene 16:9 unless the work area is too short for it.
        if (sceneW * 9 / 16 < sceneH) sceneH = sceneW * 9 / 16;
        if (sceneW < 640) sceneW = 640;
        if (sceneH < 360) sceneH = 360;

        if (forceW > 0 && forceH > 0) { sceneW = forceW; sceneH = forceH; }
        win_ = glfwCreateWindow(sceneW, sceneH, "NEPTR VIDEO - SCENE", nullptr,
                                nullptr);
        if (!win_) {
            glfwTerminate();
            return false;
        }
        glfwSetWindowPos(win_, wax + margin, way + margin);
        attach(win_);
        glfwMakeContextCurrent(win_);
        // Only the scene window waits for vblank. Two windows both syncing
        // would be two waits per iteration and a hard 30fps ceiling.
        glfwSwapInterval(1);

        if (!singleWindow_) {
            // Shares objects with the scene context: textures, buffers and
            // programs are common. VAOs are NOT, which is why every drawable
            // keeps one per context.
            panelWin_ = glfwCreateWindow(panelW, sceneH, "NEPTR VIDEO - CONTROL",
                                         nullptr, win_);
            if (panelWin_) {
                attach(panelWin_);
                glfwMakeContextCurrent(panelWin_);
                glfwSwapInterval(0);
                // Immediately right of the scene, inside the work area.
                glfwSetWindowPos(panelWin_, wax + margin + sceneW + gap,
                                 way + margin);
                glfwMakeContextCurrent(win_);
            } else {
                std::fprintf(stderr,
                             "[glfw] control window failed; using one surface\n");
                singleWindow_ = true;
            }
        }

        // The worker's context. GLFW has no bare shared-context call, but an
        // invisible window is exactly that and is never shown or swapped.
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        workerWin_ = glfwCreateWindow(1, 1, "nv-worker", nullptr, win_);
        // Restore to whatever this run's visibility actually is -- not
        // unconditionally TRUE, which would un-hide anything created later.
        glfwWindowHint(GLFW_VISIBLE, headless_ ? GLFW_FALSE : GLFW_TRUE);

        // Swap interval is set LAST, once every context exists. Creating a
        // window re-queries context attributes and makes contexts current
        // behind our back, so an interval set earlier is not reliably the one
        // still in force.
        glfwMakeContextCurrent(win_);
        // NV_VSYNC=0 unlocks the scene. Only for measurement: under vsync a
        // frame takes a whole refresh interval whether the GPU finished in
        // 2 ms or 15 ms, so a frame-time comparison between two builds reads
        // as identical however different they are.
        {
            const char* v = std::getenv("NV_VSYNC");
            glfwSwapInterval((v && std::atoi(v) == 0) ? 0 : 1);
        }
        currentCtx_ = 0;

        // Tests and captures must never steal the user's screen or focus.
        // NV_WINDOW=1 is the explicit opt-in for watching the scene.
        if (!headless_) {
            glfwShowWindow(win_);
            glfwFocusWindow(win_);
        }

        refreshOutputs();
        start_ = glfwGetTime();
        return true;
    }

    void shutdown() override {
        if (workerWin_) glfwDestroyWindow(workerWin_);
        if (panelWin_) glfwDestroyWindow(panelWin_);
        if (win_) glfwDestroyWindow(win_);
        workerWin_ = panelWin_ = win_ = nullptr;
        glfwTerminate();
    }

    const std::vector<Output>& outputs() const override { return outputs_; }

    // Rotation is a device property, but honouring NV_ROTATE here too means
    // the orientation can be checked on the desktop -- with NV_GEOM to match
    // the panel's geometry -- instead of only on hardware.
    int rotation() const override { return rotation_; }

    int currentContext() const override { return currentCtx_; }
    int contextCount() const override { return panelWin_ ? 2 : 1; }

    bool makeWorkerContextCurrent() override {
        if (!workerWin_) return false;
        // Called on the compiler thread, so it deliberately does not touch
        // currentCtx_, which belongs to the render thread.
        glfwMakeContextCurrent(workerWin_);
        return true;
    }

    void maxSurface(int& w, int& h) const override {
        // Allocate for the largest monitor attached, not the current window,
        // so resizing or going fullscreen never triggers a reallocation.
        w = 1920;
        h = 1080;
        int count = 0;
        GLFWmonitor** mons = glfwGetMonitors(&count);
        for (int i = 0; i < count; ++i) {
            const GLFWvidmode* m = glfwGetVideoMode(mons[i]);
            if (!m) continue;
            // A video mode is in points; the framebuffer we are handed is in
            // pixels, and on a Retina display those differ by 2x. Allocating
            // for points would silently cap the render at half resolution --
            // which looks exactly like the governor throttling.
            float sx = 1.0f, sy = 1.0f;
            glfwGetMonitorContentScale(mons[i], &sx, &sy);
            int mw = static_cast<int>(m->width * (sx > 0.0f ? sx : 1.0f));
            int mh = static_cast<int>(m->height * (sy > 0.0f ? sy : 1.0f));
            if (mw > w) w = mw;
            if (mh > h) h = mh;
        }
    }

    void beginFrame(int i) override {
        GLFWwindow* w = windowFor(i);
        if (!w) return;
        glfwMakeContextCurrent(w);
        currentCtx_ = (w == panelWin_) ? 1 : 0;
        int fw = 0, fh = 0;
        glfwGetFramebufferSize(w, &fw, &fh);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fw, fh);
    }

    void endFrame(int i) override {
        GLFWwindow* w = windowFor(i);
        // A hidden test context has no compositor surface to present. Swapping
        // it can block indefinitely on macOS even with swap interval 0, which
        // makes headless capture look like a shader hang. The render target
        // is still current for glReadPixels; only presentation is skipped.
        if (w && !headless_) glfwSwapBuffers(w);
    }

    bool pollEvents(bool& hotplugged) override {
        hotplugged = false;
        glfwPollEvents();
        // Closing either window ends the session: they are two views of one
        // instrument, not two documents.
        if (glfwWindowShouldClose(win_)) return false;
        if (panelWin_ && glfwWindowShouldClose(panelWin_)) return false;

        for (size_t i = 0; i < outputs_.size(); ++i) {
            GLFWwindow* w = windowFor(static_cast<int>(i));
            if (!w) continue;
            int fw = 0, fh = 0;
            glfwGetFramebufferSize(w, &fw, &fh);
            if (outputs_[i].width != fw || outputs_[i].height != fh) {
                outputs_[i].width = fw;
                outputs_[i].height = fh;
                hotplugged = true;  // a resize is the same event as a mode change
            }
        }
        return true;
    }

    void onKey(std::function<void(const KeyEvent&)> fn) override {
        keyFn_ = std::move(fn);
    }
    void onPointer(std::function<void(const PointerEvent&)> fn) override {
        ptrFn_ = std::move(fn);
    }

    double now() const override { return glfwGetTime() - start_; }

private:
    GLFWwindow* windowFor(int outputIndex) const {
        if (outputIndex <= 0) return win_;
        return panelWin_ ? panelWin_ : win_;
    }

    int outputIndexOf(GLFWwindow* w) const {
        return (panelWin_ && w == panelWin_) ? 1 : 0;
    }

    void attach(GLFWwindow* w) {
        glfwSetWindowUserPointer(w, this);
        glfwSetKeyCallback(w, &GlfwPlatform::keyCb);
        glfwSetCursorPosCallback(w, &GlfwPlatform::cursorCb);
        glfwSetMouseButtonCallback(w, &GlfwPlatform::buttonCb);
    }

    static GlfwPlatform* self(GLFWwindow* w) {
        return static_cast<GlfwPlatform*>(glfwGetWindowUserPointer(w));
    }

    // Keys work from EITHER window, so you never have to hunt for focus
    // mid-performance.
    static void keyCb(GLFWwindow* w, int key, int, int action, int mods) {
        GlfwPlatform* p = self(w);
        if (!p || !p->keyFn_ || action == GLFW_REPEAT) return;
        KeyEvent e;
        e.pressed = (action == GLFW_PRESS);
        e.shift = (mods & GLFW_MOD_SHIFT) != 0;
        e.ctrl = (mods & GLFW_MOD_CONTROL) != 0;
        switch (key) {
            case GLFW_KEY_ESCAPE: e.key = KeyEscape; break;
            case GLFW_KEY_LEFT:   e.key = KeyLeft; break;
            case GLFW_KEY_RIGHT:  e.key = KeyRight; break;
            case GLFW_KEY_UP:     e.key = KeyUp; break;
            case GLFW_KEY_DOWN:   e.key = KeyDown; break;
            case GLFW_KEY_TAB:    e.key = KeyTab; break;
            default:              e.key = key; break;  // GLFW keys are ASCII here
        }
        p->keyFn_(e);
    }

    static void cursorCb(GLFWwindow* w, double x, double y) {
        GlfwPlatform* p = self(w);
        if (!p || !p->ptrFn_) return;
        int ww = 1, wh = 1;
        glfwGetWindowSize(w, &ww, &wh);
        PointerEvent e;
        e.outputIndex = p->outputIndexOf(w);
        e.x = static_cast<float>(x) / static_cast<float>(ww > 0 ? ww : 1);
        e.y = static_cast<float>(y) / static_cast<float>(wh > 0 ? wh : 1);
        e.moved = true;
        e.down = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        p->ptrFn_(e);
    }

    static void buttonCb(GLFWwindow* w, int button, int action, int) {
        GlfwPlatform* p = self(w);
        if (!p || !p->ptrFn_ || button != GLFW_MOUSE_BUTTON_LEFT) return;
        double x = 0, y = 0;
        glfwGetCursorPos(w, &x, &y);
        int ww = 1, wh = 1;
        glfwGetWindowSize(w, &ww, &wh);
        PointerEvent e;
        e.outputIndex = p->outputIndexOf(w);
        e.x = static_cast<float>(x) / static_cast<float>(ww > 0 ? ww : 1);
        e.y = static_cast<float>(y) / static_cast<float>(wh > 0 ? wh : 1);
        // A press is NOT a move: the handler distinguishes them so a click
        // grabs the slider under the cursor before any dragging begins.
        e.moved = false;
        e.down = (action == GLFW_PRESS);
        p->ptrFn_(e);
    }

    void refreshOutputs() {
        outputs_.clear();
        const GLFWvidmode* m = glfwGetVideoMode(glfwGetPrimaryMonitor());
        double hz = (m && m->refreshRate > 0) ? m->refreshRate : 60.0;

        int w = 0, h = 0;
        glfwGetFramebufferSize(win_, &w, &h);
        Output scene;
        scene.name = "scene";
        scene.width = w;
        scene.height = h;
        scene.refreshHz = hz;
        scene.role = OutputRole::Scene;
        scene.connected = true;
        outputs_.push_back(scene);

        if (panelWin_) {
            glfwGetFramebufferSize(panelWin_, &w, &h);
            Output panel;
            panel.name = "control";
            panel.width = w;
            panel.height = h;
            panel.refreshHz = hz;
            panel.role = OutputRole::Panel;
            panel.connected = true;
            outputs_.push_back(panel);
        }
    }

    GLFWwindow* win_ = nullptr;
    GLFWwindow* panelWin_ = nullptr;
    GLFWwindow* workerWin_ = nullptr;
    bool singleWindow_ = false;
    // True when the run must never show a window. See init().
    bool headless_ = false;
    int currentCtx_ = 0;
    int rotation_ = 0;
    std::vector<Output> outputs_;
    std::function<void(const KeyEvent&)> keyFn_;
    std::function<void(const PointerEvent&)> ptrFn_;
    double start_ = 0.0;
};

}  // namespace

Platform* createPlatform() { return new GlfwPlatform(); }

}  // namespace nv

#endif  // NV_PLATFORM_GLFW
