// Pi backend: direct scanout, no compositor.
//
// We open the DRM device, become DRM master, and drive the page flip
// ourselves. Nothing sits between the fragment shader and the HDMI connector.
// That is the only way to get a *deterministic* 60 -- a compositor in the
// path costs a frame of latency and takes the vsync guarantee away from us.
//
// Output policy, per the NEPTR display rule:
//   the smaller display   -> Panel  (the onboard touch LCD, control surface)
//   the larger display    -> Scene  (the projector)
// With only one connector live, that one becomes the Scene. See assignRoles()
// for why this is by size rather than by connector name.
#if defined(NV_PLATFORM_DRM)

#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "gfx/glcommon.h"
#include "platform/evdev_touch.h"
#include "platform/platform.h"

namespace nv {
namespace {

struct DrmOutput {
    uint32_t connectorId = 0;
    uint32_t crtcId = 0;
    drmModeModeInfo mode{};
    drmModeCrtc* savedCrtc = nullptr;

    gbm_surface* gbmSurface = nullptr;
    EGLSurface eglSurface = EGL_NO_SURFACE;

    gbm_bo* frontBo = nullptr;
    uint32_t frontFb = 0;
    bool modeSet = false;
};

uint32_t fbForBo(int fd, gbm_bo* bo) {
    // The framebuffer id is cached on the bo, so a buffer cycling through the
    // swap chain is only registered with KMS once.
    auto* cached = static_cast<uint32_t*>(gbm_bo_get_user_data(bo));
    if (cached) return *cached;

    uint32_t w = gbm_bo_get_width(bo);
    uint32_t h = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;

    uint32_t handles[4] = {handle, 0, 0, 0};
    uint32_t strides[4] = {stride, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};

    auto* id = new uint32_t(0);
    if (drmModeAddFB2(fd, w, h, GBM_FORMAT_XRGB8888, handles, strides, offsets,
                      id, 0) != 0) {
        std::fprintf(stderr, "[drm] drmModeAddFB2 failed: %s\n", std::strerror(errno));
        delete id;
        return 0;
    }

    gbm_bo_set_user_data(bo, id, [](gbm_bo* b, void* data) {
        (void)b;
        delete static_cast<uint32_t*>(data);
    });
    return *id;
}

class DrmPlatform : public Platform {
public:
    bool init() override {
        readRotation();
        if (!openDevice()) return false;
        if (!initGbmEgl()) return false;
        if (!enumerateOutputs()) return false;
        if (!createSurfaces()) return false;
        // Touch is not required to render, so a missing panel is a warning and
        // the engine comes up anyway -- exactly like a missing controller.
        if (touch_.open()) {
            touch_.setOutputIndex(touchOutputIndex());
            touch_.setRotation(rotation_);
        }
        clock_gettime(CLOCK_MONOTONIC, &start_);
        return true;
    }

    void shutdown() override {
        for (auto& o : drm_) {
            if (o.savedCrtc) {
                drmModeSetCrtc(fd_, o.savedCrtc->crtc_id, o.savedCrtc->buffer_id,
                               o.savedCrtc->x, o.savedCrtc->y, &o.connectorId, 1,
                               &o.savedCrtc->mode);
                drmModeFreeCrtc(o.savedCrtc);
                o.savedCrtc = nullptr;
            }
            if (o.eglSurface != EGL_NO_SURFACE) eglDestroySurface(dpy_, o.eglSurface);
            if (o.gbmSurface) gbm_surface_destroy(o.gbmSurface);
        }
        drm_.clear();
        if (workerCtx_ != EGL_NO_CONTEXT) eglDestroyContext(dpy_, workerCtx_);
        if (ctx_ != EGL_NO_CONTEXT) eglDestroyContext(dpy_, ctx_);
        if (dpy_ != EGL_NO_DISPLAY) eglTerminate(dpy_);
        if (gbm_) gbm_device_destroy(gbm_);
        if (fd_ >= 0) {
            drmDropMaster(fd_);
            close(fd_);
        }
        fd_ = -1;
    }

    const std::vector<Output>& outputs() const override { return outputs_; }

    void maxSurface(int& w, int& h) const override {
        w = 1920;
        h = 1080;
        for (const auto& o : outputs_) {
            if (o.width > w) w = o.width;
            if (o.height > h) h = o.height;
        }
    }

    int rotation() const override { return rotation_; }

    // One context drives both connectors (different surfaces, same context),
    // so VAOs are shared here and there is only ever context 0.
    int currentContext() const override { return 0; }
    int contextCount() const override { return 1; }

    void beginFrame(int i) override {
        if (i < 0 || i >= static_cast<int>(drm_.size())) return;
        eglMakeCurrent(dpy_, drm_[i].eglSurface, drm_[i].eglSurface, ctx_);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, drm_[i].mode.hdisplay, drm_[i].mode.vdisplay);
    }

    void endFrame(int i) override {
        if (i < 0 || i >= static_cast<int>(drm_.size())) return;
        DrmOutput& o = drm_[i];

        eglSwapBuffers(dpy_, o.eglSurface);
        gbm_bo* bo = gbm_surface_lock_front_buffer(o.gbmSurface);
        if (!bo) return;
        uint32_t fb = fbForBo(fd_, bo);
        if (!fb) {
            gbm_surface_release_buffer(o.gbmSurface, bo);
            return;
        }

        if (!o.modeSet) {
            if (drmModeSetCrtc(fd_, o.crtcId, fb, 0, 0, &o.connectorId, 1,
                               &o.mode) != 0) {
                std::fprintf(stderr, "[drm] SetCrtc failed: %s\n", std::strerror(errno));
                gbm_surface_release_buffer(o.gbmSurface, bo);
                return;
            }
            o.modeSet = true;
        } else {
            if (drmModePageFlip(fd_, o.crtcId, fb, DRM_MODE_PAGE_FLIP_EVENT,
                                nullptr) == 0) {
                waitForFlip();
            }
        }

        // Release only after the new buffer is on screen, or we would hand
        // back a buffer the scanout engine is still reading.
        if (o.frontBo) gbm_surface_release_buffer(o.gbmSurface, o.frontBo);
        o.frontBo = bo;
        o.frontFb = fb;
    }

    bool pollEvents(bool& hotplugged) override {
        hotplugged = false;
        if (ptrFn_) touch_.poll(ptrFn_);
        // Re-querying every connector every frame is a handful of ioctls we
        // do not need; the projector appearing is a human-timescale event.
        if (++hotplugTick_ >= 60) {
            hotplugTick_ = 0;
            hotplugged = connectorSetChanged();
        }
        return true;
    }

    // Hand the screen to a child (the audio UI, RetroArch) and take it back.
    // See home/launcher.h for why this sequence is the whole feature.
    bool releaseDisplay() override {
        if (fd_ < 0) return false;
        if (drmDropMaster(fd_) != 0) {
            std::fprintf(stderr, "[drm] drmDropMaster failed: %s\n",
                         std::strerror(errno));
            return false;
        }
        // The child will set its OWN mode on our CRTC. Whatever we had
        // programmed is gone, so the first frame back has to re-modeset rather
        // than page-flip onto a CRTC someone else re-programmed.
        for (auto& o : drm_) o.modeSet = false;
        return true;
    }

    bool acquireDisplay() override {
        if (fd_ < 0) return false;
        // Taking master back is a race, not an instruction: the kernel reaps
        // the child's DRM fd asynchronously after exit, so EBUSY/EPERM here is
        // a normal transient. (The arcade supervisor documents the same
        // reclaim race around ui-restore.)
        //
        // THIRTY seconds, not five. Five was sized for the reap alone, and the
        // measured worst case on this rig is far longer: leaving GAMES, the
        // arcade console sends RetroArch SIGTERM, waits its own ten-second
        // grace, then SIGKILLs — and RetroArch holds DRM master for every one
        // of those seconds. HOME's child (the mode script) has already exited
        // by then, so the engine arrives here while the display is still
        // genuinely owned. At five seconds it gave up and left a black screen
        // (observed 2026-08-16). The wait costs nothing when the display is
        // free: the first drmSetMaster succeeds and returns immediately.
        for (int i = 0; i < 600; ++i) {
            if (drmSetMaster(fd_) == 0) {
                for (auto& o : drm_) o.modeSet = false;
                return true;
            }
            timespec t{0, 50 * 1000 * 1000};
            nanosleep(&t, nullptr);
        }
        std::fprintf(stderr, "[drm] drmSetMaster did not come back: %s\n",
                     std::strerror(errno));
        return false;
    }

    bool makeWorkerContextCurrent() override {
        // Surfaceless: the worker only builds programs, it never draws, so it
        // needs no surface of its own. Mesa provides EGL_KHR_surfaceless_context
        // on v3d; if a driver ever lacks it, this returns false and the engine
        // falls back to compiling inline.
        if (workerCtx_ == EGL_NO_CONTEXT) {
            const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
            workerCtx_ = eglCreateContext(dpy_, config_, ctx_, attribs);
            if (workerCtx_ == EGL_NO_CONTEXT) return false;
        }
        return eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, workerCtx_) ==
               EGL_TRUE;
    }

    void onKey(std::function<void(const KeyEvent&)> fn) override { keyFn_ = std::move(fn); }
    void onPointer(std::function<void(const PointerEvent&)> fn) override { ptrFn_ = std::move(fn); }

    double now() const override {
        timespec t{};
        clock_gettime(CLOCK_MONOTONIC, &t);
        return (t.tv_sec - start_.tv_sec) + (t.tv_nsec - start_.tv_nsec) * 1e-9;
    }

private:
    // NV_ROTATE is DEGREES CLOCKWISE. See Platform::rotation() -- the
    // convention is stated there because getting it backwards is a 180-degree
    // error that reads as an entirely different bug.
    void readRotation() {
        const char* v = std::getenv("NV_ROTATE");
        if (!v) return;
        int r = std::atoi(v);
        r = ((r % 360) + 360) % 360;
        if (r != 0 && r != 90 && r != 180 && r != 270) {
            std::fprintf(stderr,
                         "[drm] NV_ROTATE=%s is not 0/90/180/270 -- ignoring\n", v);
            return;
        }
        rotation_ = r;
        if (r) std::printf("[drm] rotating %d degrees clockwise\n", r);
    }

    bool openDevice() {
        // card1 first: on a Pi 5 running the v3d/vc4 split, card1 is usually
        // the display device and card0 the render node's companion.
        const char* candidates[] = {"/dev/dri/card1", "/dev/dri/card0"};
        for (const char* path : candidates) {
            int fd = open(path, O_RDWR | O_CLOEXEC);
            if (fd < 0) continue;
            drmModeRes* res = drmModeGetResources(fd);
            if (res && res->count_connectors > 0) {
                drmModeFreeResources(res);
                fd_ = fd;
                devicePath_ = path;
                break;
            }
            if (res) drmModeFreeResources(res);
            close(fd);
        }
        if (fd_ < 0) {
            std::fprintf(stderr, "[drm] no usable DRM device\n");
            return false;
        }
        if (drmSetMaster(fd_) != 0) {
            // Almost always means a compositor or another KMS client already
            // holds the display. That is a real conflict, not a warning: only
            // one process can be DRM master.
            std::fprintf(stderr,
                         "[drm] cannot become DRM master on %s: %s\n"
                         "      Another process owns the display (a compositor, "
                         "or the NEPTR audio UI). Stop it first.\n",
                         devicePath_.c_str(), std::strerror(errno));
            return false;
        }
        return true;
    }

    bool initGbmEgl() {
        gbm_ = gbm_create_device(fd_);
        if (!gbm_) {
            std::fprintf(stderr, "[drm] gbm_create_device failed\n");
            return false;
        }

        dpy_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(gbm_));
        if (dpy_ == EGL_NO_DISPLAY) {
            std::fprintf(stderr, "[drm] eglGetDisplay failed\n");
            return false;
        }
        if (!eglInitialize(dpy_, nullptr, nullptr)) {
            std::fprintf(stderr, "[drm] eglInitialize failed\n");
            return false;
        }
        if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;

        const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 0,
            EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_NONE};

        EGLint count = 0;
        std::vector<EGLConfig> configs(64);
        if (!eglChooseConfig(dpy_, attribs, configs.data(),
                             static_cast<EGLint>(configs.size()), &count) ||
            count == 0) {
            std::fprintf(stderr, "[drm] no matching EGL config\n");
            return false;
        }

        // The config must match the GBM format exactly, or eglCreateWindow-
        // Surface succeeds and then scans out garbage.
        config_ = configs[0];
        for (EGLint i = 0; i < count; ++i) {
            EGLint id = 0;
            eglGetConfigAttrib(dpy_, configs[i], EGL_NATIVE_VISUAL_ID, &id);
            if (id == static_cast<EGLint>(GBM_FORMAT_XRGB8888)) {
                config_ = configs[i];
                break;
            }
        }

        const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        ctx_ = eglCreateContext(dpy_, config_, EGL_NO_CONTEXT, ctxAttribs);
        if (ctx_ == EGL_NO_CONTEXT) {
            std::fprintf(stderr, "[drm] eglCreateContext failed\n");
            return false;
        }
        return true;
    }

    static std::string connectorName(drmModeConnector* c) {
        const char* type = drmModeGetConnectorTypeName(c->connector_type);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s-%u", type ? type : "UNKNOWN",
                      c->connector_type_id);
        return buf;
    }

    bool enumerateOutputs() {
        drmModeRes* res = drmModeGetResources(fd_);
        if (!res) return false;

        drm_.clear();
        outputs_.clear();

        for (int i = 0; i < res->count_connectors; ++i) {
            drmModeConnector* conn = drmModeGetConnector(fd_, res->connectors[i]);
            if (!conn) continue;
            if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
                drmModeFreeConnector(conn);
                continue;
            }

            // Preferred mode if the display declares one, otherwise the
            // largest. Never a hardcoded resolution: whatever is plugged in
            // decides, and everything above the seam adapts to it.
            drmModeModeInfo best = conn->modes[0];
            for (int m = 0; m < conn->count_modes; ++m) {
                if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                    best = conn->modes[m];
                    break;
                }
                if (conn->modes[m].hdisplay * conn->modes[m].vdisplay >
                    best.hdisplay * best.vdisplay) {
                    best = conn->modes[m];
                }
            }

            uint32_t crtcId = findCrtc(res, conn);
            if (!crtcId) {
                drmModeFreeConnector(conn);
                continue;
            }

            DrmOutput d;
            d.connectorId = conn->connector_id;
            d.crtcId = crtcId;
            d.mode = best;
            d.savedCrtc = drmModeGetCrtc(fd_, crtcId);

            Output o;
            o.name = connectorName(conn);
            // The LOGICAL size: a quarter turn swaps it, so every layout above
            // this seam is computed for the screen as the user sees it and
            // nothing up there ever learns the panel is bolted on sideways.
            bool quarter = (rotation_ == 90 || rotation_ == 270);
            o.width = quarter ? best.vdisplay : best.hdisplay;
            o.height = quarter ? best.hdisplay : best.vdisplay;
            o.refreshHz = best.vrefresh > 0 ? best.vrefresh : 60.0;
            o.connected = true;
            o.role = OutputRole::Scene;

            drm_.push_back(d);
            outputs_.push_back(o);
            drmModeFreeConnector(conn);
        }
        drmModeFreeResources(res);

        if (outputs_.empty()) {
            std::fprintf(stderr, "[drm] no connected outputs\n");
            return false;
        }

        assignRoles();
        for (size_t i = 0; i < outputs_.size(); ++i) {
            std::printf("[drm] %s %dx%d@%.0f -> %s\n", outputs_[i].name.c_str(),
                        outputs_[i].width, outputs_[i].height, outputs_[i].refreshHz,
                        outputs_[i].role == OutputRole::Panel ? "PANEL" : "SCENE");
        }
        return true;
    }

    // One display -> it is the Scene, whatever it is. There is nowhere else
    // for the visuals to go, and a control surface with no output to control
    // is useless.
    //
    // Two displays -> the onboard touch panel is the control surface and the
    // other one is the scene. Which connector the panel lands on is NOT
    // something this code can know: micro-HDMI port numbering is a property of
    // how the machine was cabled, not of the software, and hardcoding
    // "HDMI-A-1 is the panel" silently puts the control surface on the
    // projector the day someone swaps the two leads.
    //
    // So the rule is by SIZE, not by name: the onboard panel is a small
    // portrait LCD and the scene display is a monitor or projector, so the
    // smaller pixel count is the panel. That holds for any cabling.
    // NV_PANEL_CONNECTOR overrides it by name for the case the heuristic
    // cannot cover -- two displays of identical size.
    void assignRoles() {
        for (auto& o : outputs_) o.role = OutputRole::Scene;
        if (outputs_.size() < 2) return;

        int panelIdx = -1;
        if (const char* want = std::getenv("NV_PANEL_CONNECTOR")) {
            for (size_t i = 0; i < outputs_.size(); ++i) {
                if (outputs_[i].name == want) panelIdx = static_cast<int>(i);
            }
            if (panelIdx < 0) {
                std::fprintf(stderr,
                             "[drm] NV_PANEL_CONNECTOR=%s matches no connected "
                             "output; falling back to the smallest display\n",
                             want);
            }
        }
        if (panelIdx < 0) {
            panelIdx = 0;
            for (size_t i = 1; i < outputs_.size(); ++i) {
                if (outputs_[i].width * outputs_[i].height <
                    outputs_[panelIdx].width * outputs_[panelIdx].height) {
                    panelIdx = static_cast<int>(i);
                }
            }
        }
        outputs_[panelIdx].role = OutputRole::Panel;
    }

    uint32_t findCrtc(drmModeRes* res, drmModeConnector* conn) {
        for (int e = 0; e < conn->count_encoders; ++e) {
            drmModeEncoder* enc = drmModeGetEncoder(fd_, conn->encoders[e]);
            if (!enc) continue;
            for (int c = 0; c < res->count_crtcs; ++c) {
                if (!(enc->possible_crtcs & (1u << c))) continue;
                uint32_t id = res->crtcs[c];
                if (usedCrtcs_.count(id)) continue;
                drmModeFreeEncoder(enc);
                usedCrtcs_.insert(id);
                return id;
            }
            drmModeFreeEncoder(enc);
        }
        return 0;
    }

    bool createSurfaces() {
        for (size_t i = 0; i < drm_.size(); ++i) {
            DrmOutput& o = drm_[i];
            o.gbmSurface = gbm_surface_create(
                gbm_, o.mode.hdisplay, o.mode.vdisplay, GBM_FORMAT_XRGB8888,
                GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
            if (!o.gbmSurface) {
                std::fprintf(stderr, "[drm] gbm_surface_create failed for %s\n",
                             outputs_[i].name.c_str());
                return false;
            }
            o.eglSurface = eglCreateWindowSurface(
                dpy_, config_, reinterpret_cast<EGLNativeWindowType>(o.gbmSurface),
                nullptr);
            if (o.eglSurface == EGL_NO_SURFACE) {
                std::fprintf(stderr, "[drm] eglCreateWindowSurface failed for %s\n",
                             outputs_[i].name.c_str());
                return false;
            }
        }
        eglMakeCurrent(dpy_, drm_[0].eglSurface, drm_[0].eglSurface, ctx_);
        // We pace on the page flip event, so EGL must not also throttle.
        eglSwapInterval(dpy_, 0);
        return true;
    }

    void waitForFlip() {
        drmEventContext ev{};
        ev.version = 2;
        ev.page_flip_handler = [](int, unsigned int, unsigned int, unsigned int,
                                  void*) {};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval tv{};
        tv.tv_sec = 1;  // never hang forever on a display that stopped flipping
        if (select(fd_ + 1, &fds, nullptr, nullptr, &tv) > 0) {
            drmHandleEvent(fd_, &ev);
        }
    }

    // The touchscreen IS the small onboard panel, so a touch belongs to the
    // Panel-role output when there is one. With a single display that display
    // is the Scene by policy, and it is also the thing being touched.
    int touchOutputIndex() const {
        for (size_t i = 0; i < outputs_.size(); ++i) {
            if (outputs_[i].role == OutputRole::Panel) return static_cast<int>(i);
        }
        return 0;
    }

    bool connectorSetChanged() {
        drmModeRes* res = drmModeGetResources(fd_);
        if (!res) return false;
        size_t live = 0;
        for (int i = 0; i < res->count_connectors; ++i) {
            drmModeConnector* c = drmModeGetConnector(fd_, res->connectors[i]);
            if (!c) continue;
            if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) ++live;
            drmModeFreeConnector(c);
        }
        drmModeFreeResources(res);
        return live != outputs_.size();
    }

    int fd_ = -1;
    std::string devicePath_;
    gbm_device* gbm_ = nullptr;
    EGLDisplay dpy_ = EGL_NO_DISPLAY;
    EGLContext ctx_ = EGL_NO_CONTEXT;
    EGLContext workerCtx_ = EGL_NO_CONTEXT;
    EGLConfig config_ = nullptr;

    std::vector<DrmOutput> drm_;
    std::vector<Output> outputs_;
    std::set<uint32_t> usedCrtcs_;
    int rotation_ = 0;

    std::function<void(const KeyEvent&)> keyFn_;
    std::function<void(const PointerEvent&)> ptrFn_;
    EvdevTouch touch_;
    timespec start_{};
    int hotplugTick_ = 0;
};

}  // namespace

Platform* createPlatform() { return new DrmPlatform(); }

}  // namespace nv

#endif  // NV_PLATFORM_DRM
