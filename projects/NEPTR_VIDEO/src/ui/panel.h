// The control surface.
//
// Draws every parameter in the bank as a slider, plus a status header. Layout
// is computed from the surface size every frame and flows into as many columns
// as fit, so an 800x480 panel and a 1080p monitor both get a usable page
// without a single hardcoded coordinate.
//
// Everything is drawn from ONE texture (the font atlas, whose last cell is
// solid) in ONE draw call: bars, backgrounds and text share a vertex buffer.
// A control surface that costs more than a fraction of a millisecond is a
// control surface that is stealing frames from the scene.
#pragma once

#include <string>
#include <vector>

#include "chain/effects.h"
#include "gfx/glcommon.h"
#include "gfx/shader.h"
#include "home/home.h"
#include "state.h"
#include "ui/theme.h"

namespace nv {

struct PanelStatus {
    std::string mode;
    std::string status;   // last preset action
    std::string kernelName;
    std::string colorMode;
    std::string trapShape;
    std::string paletteA;
    std::string paletteB;
    int fps = 0;
    int renderWidth = 0;
    int renderHeight = 0;
    int passes = 0;
    int iterations = 0;
    bool governed = false;

    // Control ingress, for the monitor page.
    int oscPort = 0;
    bool oscListening = false;
    int mappingCount = 0;
};

// One row of the input monitor: a source seen on the wire, its latest value,
// and where it is going. Sources with no mapping still appear -- watching
// unmapped traffic is the only way to discover what a new sensor emits.
struct MonitorRow {
    std::string source;
    float value = 0.0f;
    std::string target;   // parameter id, or empty when unmapped
    double lastSeen = 0.0;
};

// What one LFO drives, reduced to what the panel actually has to draw. The
// panel deals in floats; routing is a set of names, and a MOD page that showed
// a DEPTH knob without saying what it modulates would be a page of mysteries.
//
// `count` rather than the whole list because an LFO with three destinations
// cannot say all three on a 480px button -- it says how many, and the MAP page
// is where the list lives.
struct LfoInfo {
    int count = 0;
    std::string first;    // the one name, when there is exactly one
    // At least one destination names a parameter the bank does not currently
    // have. Normal -- switch kernels and a knob leaves -- but it must be
    // VISIBLE, or a silent modulator reads as a fault.
    bool dangling = false;
};

// One row of the mapping table, flattened for drawing. The panel is a renderer
// and a hit-tester; the table itself stays with the engine, so what crosses
// the boundary is this and a row index back.
struct PanelMapRow {
    std::string source;
    std::string param;
    float lo = 0.0f;
    float hi = 1.0f;
    bool mod = false;        // ADDS rather than SETS
    bool relative = false;
    bool dangling = false;   // parameter not in the bank right now
};

// The panel is paged. Forty-odd parameters in one flat list is a wall of
// sliders, not an instrument: pages group them the way you actually reach for
// them, and each page is small enough to read at a glance and hit with a
// finger.
enum class Page {
    Main = 0,   // speed, zoom, position, detail, resolution, morph
    Colour,     // palette selection and shaping
    Shape,      // the generator's own knobs, and the morph target's
    // The bank itself, one cell per generator. `[` and `]` step through
    // thirty entries, which is fine at a desk and useless with a projector
    // running: you know which one you want and you want it now. Same reason
    // the remote surface grew a grid.
    Gen,
    Swarm,      // particles
    Draw,       // brush
    Mod,        // LFOs and macros
    Map,        // the mapping table: every source and where it goes
    Fx,         // the effect chain
    Input,      // the raw-input monitor
    kCount
};

const char* pageName(Page p);

class Panel {
public:
    bool init(int contextCount);
    void shutdown();

    // Degrees CLOCKWISE (0/90/180/270), for a panel bolted on sideways.
    void setRotation(int deg) { rotation_ = deg; }

    // What each LFO drives, and how many parameters each macro drives. Both
    // are now the same question asked of the same table -- one is just drawn
    // on a button and the other in a label.
    void setModInfo(const std::vector<LfoInfo>& lfoTargets,
                    const std::vector<int>& macroFanout) {
        lfoTargets_ = lfoTargets;
        macroFanout_ = macroFanout;
    }

    // The whole mapping table, for the MAP page. Pushed rather than pulled,
    // and only when it changes, for the same reason setModInfo is: rebuilding
    // this every frame would be work the scene pays for to say the same thing.
    void setMappings(const std::vector<PanelMapRow>& rows) { mapRows_ = rows; }

    // Which LFO is armed for learn (-1 for none) and how long it has left.
    // Drawn rather than owned: the state machine lives in ModLearn, because
    // the keyboard can arm it too and a panel that owned the mode would be a
    // second copy of it.
    void setLearn(int lfoSlot, float secondsLeft) {
        learnSlot_ = lfoSlot;
        learnLeft_ = secondsLeft;
    }

    // The generator bank, for the GEN page. Pushed, and only when it changes,
    // like the mapping table: the names are constant for the life of the
    // process and rebuilding the list every frame would be work the scene pays
    // for to say the same thing.
    void setGenerators(const std::vector<std::string>& names, int current,
                       int morphTarget) {
        genNames_ = names;
        genCurrent_ = current;
        genB_ = morphTarget;
    }

    // Which two are current. Separate from the names because the names never
    // change and this does, every time anything selects a generator.
    void setGeneratorSelection(int current, int morphTarget) {
        genCurrent_ = current;
        genB_ = morphTarget;
    }

    // Which generator cell is under the point, or -1. Uses the layout from the
    // last draw(), so what you touch is exactly what you saw.
    int genCellAt(float nx, float ny) const;

    void setPage(Page p) { page_ = p; }
    Page page() const { return page_; }
    void cyclePage(int delta);

    // Draws the current page into the currently bound framebuffer.
    void draw(const ParamBank& params, const PanelStatus& status,
              const std::vector<MonitorRow>& monitor, double now, int w, int h,
              int ctx);

    // The HOME mode selector. Drawn by the Panel rather than by a view of its
    // own because everything it needs -- the font atlas, the program, the
    // vertex buffer, the one-draw-call flush -- already lives here. A second
    // renderer to draw four words would be four hundred duplicated lines.
    void drawHome(const std::vector<HomeItem>& items, int selected,
                  const std::string& message, float homeHold, int w, int h,
                  int ctx);

    // Entry index under a normalized point, or -1. Uses the layout from the
    // last drawHome(), so what you touch is exactly what you saw.
    int homeEntryAt(float nx, float ny) const;

    // Hit test in normalized surface coordinates (0..1, y down). Returns the
    // parameter index under the point, or -1. Uses the layout from the last
    // draw(), so what you touch is exactly what you saw.
    int paramAt(float nx, float ny) const;

    // The LFO whose TARGET button is under the point, or -1. A separate hit
    // test from paramAt() because these are buttons, not sliders: they take a
    // tap, they must never take a drag, and they exist only on the MOD page.
    int modTargetAt(float nx, float ny) const;

    // ---- the MAP page ----------------------------------------------------
    //
    // A row has two zones, and they are two gestures rather than one modal
    // one: the NAME half takes a tap (remove this destination), the AMOUNT
    // half takes a drag (how much of the parameter's travel it gets). Both
    // report an index into the rows last given to setMappings(), and the
    // engine -- which owns the table -- does the editing. The panel never
    // holds a mapping it can change behind the engine's back.
    //
    // `onAmount` says which half was hit.
    int mapRowAt(float nx, float ny, bool* onAmount) const;
    // Where a point falls across a row's amount bar, 0..1. Bipolar: 0.5 is
    // "no depth", and dragging through the middle flips the direction, which
    // is the same detent the DEPTH knob has.
    float mapAmountAt(int row, float nx) const;

    // Drag handling: which parameter is captured, and where the finger is.
    void onPointerDown(float nx, float ny);
    void onPointerMove(float nx, float ny, ParamBank& params);
    void onPointerUp();

private:
    struct Rect {
        float x, y, w, h;
    };
    struct SliderRect {
        int param;
        Rect bar;
    };

    void pushRect(const Rect& r, float cr, float cg, float cb, float ca);
    void pushText(const std::string& s, float x, float y, float scale,
                  float cr, float cg, float cb, float ca);

    // Colour-struct overloads. Every call site names a theme colour instead of
    // three literals, so the instrument can be re-skinned by editing theme.h
    // and nothing else.
    void pushRect(const Rect& r, const theme::Colour& c, float a = 1.0f) {
        pushRect(r, c.r, c.g, c.b, a);
    }
    void pushText(const std::string& s, float x, float y, float scale,
                  const theme::Colour& c, float a = 1.0f) {
        pushText(s, x, y, scale, c.r, c.g, c.b, a);
    }
    float pushTextFit(const std::string& s, float x, float y, float maxW,
                      float scale, const theme::Colour& c, float a = 1.0f) {
        return pushTextFit(s, x, y, maxW, scale, c.r, c.g, c.b, a);
    }
    // Fades a colour towards the background rather than towards black: on a
    // purple ground, multiplying by a brightness turns everything grey.
    static theme::Colour fade(const theme::Colour& c, float lit) {
        return {theme::kBg.r + (c.r - theme::kBg.r) * lit,
                theme::kBg.g + (c.g - theme::kBg.g) * lit,
                theme::kBg.b + (c.b - theme::kBg.b) * lit};
    }
    float textWidth(const std::string& s, float scale) const;
    // Draws at the largest scale up to `scale` that fits inside maxW. The
    // panel's scale is chosen to fit PARAMETERS; the header is a different
    // width problem and on a 480px panel it simply ran off the edge.
    float pushTextFit(const std::string& s, float x, float y, float maxW,
                      float scale, float cr, float cg, float cb, float ca);

    // Uploads the accumulated geometry and issues the single draw call.
    void flush(int ctx);

    void drawGenerators(const PanelStatus& status, int w, int h, int ctx);
    void drawMonitor(const std::vector<MonitorRow>& rows, const PanelStatus& status,
                     double now, int w, int h, int ctx);
    void drawMappings(const ParamBank& params, const PanelStatus& status, int w,
                      int h, int ctx);
    // True when this parameter belongs on the page being drawn.
    static bool onPage(Page pg, const std::string& group);

    Page page_ = Page::Main;
    int rotation_ = 0;
    Program prog_;
    // The VBO is a shared object; the VAO is not, so there is one per context.
    std::vector<GLuint> vaos_;
    GLuint vbo_ = 0;
    GLuint atlas_ = 0;
    std::vector<float> verts_;

    // Pixel dimensions of the surface the current layout was computed for.
    float surfW_ = 1.0f;
    float surfH_ = 1.0f;
    std::vector<LfoInfo> lfoTargets_;
    std::vector<int> macroFanout_;
    std::vector<PanelMapRow> mapRows_;
    // The MAP page's rows, in the order they were drawn, split into the two
    // halves the two gestures land on.
    struct MapRect {
        Rect name;
        Rect amount;
    };
    std::vector<MapRect> mapRects_;
    std::vector<std::string> genNames_;
    int genCurrent_ = -1;
    int genB_ = -1;
    std::vector<Rect> genRects_;
    std::vector<SliderRect> sliders_;
    int captured_ = -1;
    std::vector<Rect> homeRects_;
    // The MOD page's TARGET buttons, in slot order.
    std::vector<Rect> modButtons_;
    int learnSlot_ = -1;
    float learnLeft_ = 0.0f;
};

}  // namespace nv
