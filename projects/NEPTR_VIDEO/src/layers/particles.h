// GPU particle system, steered by the fractal.
//
// State lives in ping-ponged textures and never leaves the GPU. A readback to
// move particles on the CPU would stall the pipeline every frame -- the
// driver has to finish all outstanding work before glReadPixels returns --
// and 65k particles would be dead on arrival at 60fps.
//
//   update pass   full-screen over the STATE texture: one texel per particle.
//                 Reads the fractal field, integrates, writes new state.
//   draw pass     GL_POINTS with no vertex buffer at all: the vertex shader
//                 fetches its own position by gl_VertexID. Nothing is
//                 uploaded per frame.
//
// The fractal drives the motion rather than decorating it: particles are
// advected along the field's gradient (so they trace its contours), or along
// the perpendicular (so they orbit the structure instead of falling into it).
#pragma once

#include <vector>

#include "gfx/fbo.h"
#include "gfx/glcommon.h"
#include "gfx/quad.h"
#include "gfx/shader.h"
#include "state.h"

namespace nv {

struct ParticleParams {
    float count = 1.0f;      // fraction of the pool that is alive
    float size = 2.0f;       // point size in pixels
    float speed = 1.0f;      // rate: how fast everything below plays out
    // How hard the generator's structure pulls them in. This is what gives
    // the swarm its shape -- without it they are just noise moving.
    float gravity = 1.0f;
    float swirl = 0.5f;      // orbit the structure instead of falling into it
    // Undirected energy: how much they shoot about on their own. Gravity
    // gives structure, motion fights it, and the balance is the character.
    float motion = 0.0f;
    float damping = 0.92f;
    float life = 4.0f;       // seconds before respawn

    // Causes of death, all off by default so the swarm behaves exactly as it
    // did when it only respawned on the timer. Rates per second, not
    // durations: see layers/lifecycle.h.
    float dieInterior = 0.0f;  // dwelling where the field is zero consumes life
    float dieEdge = 0.0f;      // 0 wraps at the frame edge, 1 dies there
    float dieExhaust = 0.0f;   // being thrown hard spends a particle
    // How much of the swarm's colour is written into the INK layer each
    // frame. This is the loop that makes the actors able to touch the world:
    // the generator steers them, they write into the ink, the ink is
    // composited into the field they read. 0 is exactly free.
    float deposit = 0.0f;
    float brightness = 1.0f;
    float fieldColor = 1.0f; // how much the field under the particle tints it

    // COHESION is the whole idea: a spring pulling each particle back to its
    // HOME -- its slot in a grid covering the screen. At 1, with size set to
    // the cell size, the pool tiles exactly and reconstructs the image, so
    // what you are looking at IS the particles. Wind it down and the same
    // swarm disperses. No crossfade, no second renderer, no "tight mode".
    float cohesion = 1.0f;
    // Whether a particle keeps the colour of where it CAME FROM (the image
    // shatters and the pieces carry their colour) or takes the colour of
    // where it IS (dust drifting over the image).
    float carry = 1.0f;
    float aspect = 1.0f;
    // The swarm reads the SAME palette as the generator, so a colour change
    // moves the whole instrument at once rather than the fractal and the
    // particles drifting apart.
    float paletteShift = 0.0f;
    float paletteMix = 0.0f;
};

class Particles {
public:
    // texW*texH is the pool size. Returns false if the GL cannot render to a
    // float target, in which case particle mode is unavailable rather than
    // silently wrong -- 8-bit positions would quantise motion to a coarse
    // grid and look broken.
    bool init(int texW, int texH, int contextCount);
    void shutdown();
    bool available() const { return available_; }
    int poolSize() const { return texW_ * texH_; }

    // Scatters every particle to a fresh random position.
    void seed();

    // One integration step. `field` is the core render; its luma is the
    // scalar field. srcScale maps a 0..1 uv onto the field's sub-rect.
    void update(GLuint field, float srcScaleX, float srcScaleY, float dt,
                double time, const ParticleParams& p, int ctx);

    // Draws the pool into the currently bound framebuffer.
    void draw(int outW, int outH, GLuint field, float srcScaleX, float srcScaleY,
              GLuint palette, const ParticleParams& p, int ctx);

    // The same draw, aimed at the ink canvas instead of the scene, at DEPOSIT
    // strength. Nothing happens at all when deposit is 0.
    void deposit(int outW, int outH, GLuint field, float srcScaleX,
                 float srcScaleY, GLuint palette, const ParticleParams& p,
                 int ctx);

private:
    void drawPoints(int outW, int outH, GLuint field, float srcScaleX,
                    float srcScaleY, GLuint palette, const ParticleParams& p,
                    int ctx, float alpha, bool lerpIntoTarget);

    bool available_ = false;
    int texW_ = 0, texH_ = 0;
    int cur_ = 0;              // which state texture holds the current frame

    GLuint tex_[2] = {0, 0};
    GLuint fbo_[2] = {0, 0};

    Program update_;
    Program draw_;
    FullscreenTri tri_;
    std::vector<GLuint> vaos_;  // point draws need a VAO, and VAOs are per-context
    bool seeded_ = false;
    double seedTime_ = 0.0;
};

}  // namespace nv
