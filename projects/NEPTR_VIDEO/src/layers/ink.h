// The drawing layer.
//
// A persistent buffer you paint into with the pointer. It is composited
// before the effect chain, which is the whole point: the feedback, warp and
// kaleido stages already downstream then process the strokes, so one dab
// smears into structure instead of sitting there as a dab.
//
// The fractal modulates the brush -- colour is sampled from the field under
// the cursor -- so drawing on a Burning Ship gives you the Burning Ship's
// colours without picking any.
#pragma once

#include <vector>

#include "gfx/glcommon.h"
#include "gfx/quad.h"
#include "gfx/shader.h"

namespace nv {

struct InkParams {
    float size = 0.04f;      // brush radius, fraction of the surface
    float flow = 0.6f;       // how much each dab deposits
    float softness = 0.6f;
    float fade = 0.0f;       // per-second decay; 0 keeps strokes forever
    float fieldColor = 1.0f; // how much the fractal under the brush tints it
};

class Ink {
public:
    bool init(int maxW, int maxH, int contextCount);
    void shutdown();
    void clear();

    // Queues a dab at a normalized position. Called from pointer events, which
    // arrive between frames, so the stroke is accumulated and stamped during
    // rendering when a framebuffer is safe to bind.
    void addDab(float nx, float ny);

    // Lifting the pointer must break interpolation: otherwise the next dab
    // draws a line across the canvas from wherever the last stroke ended.
    void endStroke() { haveLast_ = false; }

    // Stamps queued dabs and applies fade. `field` supplies the brush colour.
    void stamp(int w, int h, GLuint field, float srcScaleX, float srcScaleY,
               GLuint palette, float paletteShift, float paletteMix,
               const InkParams& p, float dt, int ctx);

    // Binds the canvas as the render target so ANOTHER layer can write into
    // it -- the particles depositing is the case this exists for. The brush
    // is not the only thing allowed to touch the world.
    void beginDeposit(int w, int h);

    GLuint texture() const { return fboTex_; }
    bool hasContent() const { return hasContent_; }

private:
    void ensureCleared();

    GLuint fbo_ = 0;
    GLuint fboTex_ = 0;
    int maxW_ = 0, maxH_ = 0;
    bool hasContent_ = false;
    bool cleared_ = false;

    Program brush_;
    Program fade_;
    FullscreenTri tri_;

    // Dabs queued since the last stamp. Interpolating between successive
    // pointer positions is what turns a 60Hz sample of a fast drag into a
    // continuous line rather than a dotted one.
    std::vector<float> dabs_;  // x,y pairs
    bool haveLast_ = false;
    float lastX_ = 0.0f, lastY_ = 0.0f;
};

}  // namespace nv
