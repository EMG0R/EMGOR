#include "layers/particles.h"

#include <cstdio>

#include "layers/lifecycle.h"
#include "shader_source.h"

namespace nv {

// State packing, one texel per particle:
//   rg = position in 0..1 screen space
//   ba = velocity
// Age is deliberately NOT stored. Each particle respawns on a schedule
// derived from a hash of its own id, so lifetimes are staggered without
// spending a channel -- and without a second render target.
static const char* kUpdateFrag =
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uState;\n"
    "uniform sampler2D uField;\n"
    "uniform vec2  uSrcScale;\n"
    "uniform vec2  uDim;\n"
    "uniform float uDt;\n"
    "uniform float uTime;\n"
    "uniform float uSpeed;\n"
    "uniform float uGravity;\n"
    "uniform float uSwirl;\n"
    "uniform float uMotion;\n"
    "uniform float uDamping;\n"
    "uniform float uLife;\n"
    "uniform float uCount;\n"
    "uniform float uCohesion;\n"
    // Causes of death, each a rate per second and each OFF at zero. See
    // layers/lifecycle.h for why these are hazards rather than a life budget.
    "uniform float uDieInterior;\n"
    "uniform float uDieEdge;\n"
    "uniform float uDieExhaust;\n"
    "\n"
    "float fieldAt(vec2 p) {\n"
    "  vec3 c = texture(uField, clamp(p, 0.0, 1.0) * uSrcScale).rgb;\n"
    "  return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "}\n"
    "\n"
    "vec2 hash2(vec2 p) {\n"
    "  vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
    "  q += dot(q, q.yzx + 33.33);\n"
    "  return fract((q.xx + q.yz) * q.zy);\n"
    "}\n"
    "\n"
    // A fresh particle. Respawn AT HOME when cohesive, so a tight image never
    // flickers as particles recycle; scatter only as cohesion drops. This one
    // line is also what keeps every cause of death INVISIBLE at cohesion 1 --
    // the replacement lands exactly where the dead one was standing, so the
    // reconstruction never develops holes however lethal the world is set.
    "vec4 spawnState(float id, vec2 home, float epoch) {\n"
    "  vec2 r = hash2(vec2(id, epoch + 3.7));\n"
    "  return vec4(mix(r, home, uCohesion), 0.0, 0.0);\n"
    "}\n"
    "\n"
    // Mirrors of lifecycle.h. The thresholds arrive as #defines generated from
    // the same constants the C++ side tests, so only the shape is written
    // twice and never the numbers.
    "float interiorWeight(float lum) {\n"
    "  return 1.0 - smoothstep(0.0, NV_INTERIOR_CEIL, lum);\n"
    "}\n"
    "float exhaustWeight(float sp) {\n"
    "  return smoothstep(NV_CALM_SPEED, NV_SPENT_SPEED, sp);\n"
    "}\n"
    "float hazard(float rate, float dt) {\n"
    "  return 1.0 - exp(-rate * dt);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "  vec4 st = texture(uState, vUV);\n"
    "  vec2 pos = st.xy;\n"
    "  vec2 vel = st.zw;\n"
    "\n"
    "  vec2 cell = floor(vUV * uDim);\n"
    "  float id = cell.x + cell.y * uDim.x;\n"
    "  vec2 h = hash2(vec2(id, 17.0));\n"
    // The ALIVE grid. COUNT is an area fraction, so the side scales by its
    // square root, and the alive particles are the top-left rectangle of the
    // pool rather than a contiguous run of ids.
    //
    // This is what makes COUNT a density control instead of a crop. Parking
    // the TAIL of a raster-ordered pool deletes a band of the image -- at
    // COUNT 0.5 exactly half the frame went empty, which reads as a broken
    // renderer rather than as fewer particles.
    "  vec2 adim = max(floor(uDim * sqrt(max(uCount, 0.0001))), vec2(1.0));\n"
    // HOME: the centre of this particle's cell in the ALIVE grid, so whatever
    // is alive always tiles the whole frame. Still derived from the id alone,
    // so it costs no storage and never has to be initialised.
    "  vec2 home = (cell + 0.5) / adim;\n"
    "\n"
    // Respawn on a per-particle phase so the pool does not blink all at once.
    "  float phase = h.x;\n"
    "  float age = fract(uTime / max(uLife, 0.05) + phase);\n"
    "  bool born = age < uDt / max(uLife, 0.05);\n"
    // Particles past the active fraction are parked off-screen instead of
    // being drawn at zero size: a degenerate point still costs a fragment.
    "  bool idle = any(greaterThanEqual(cell, adim));\n"
    "\n"
    "  if (born || idle || pos.x < -0.5) {\n"
    "    fragColor = idle\n"
    "        ? vec4(-1.0, -1.0, 0.0, 0.0)\n"
    "        : spawnState(id, home, floor(uTime / max(uLife, 0.05)));\n"
    "    return;\n"
    "  }\n"
    "\n"
    // The field's gradient, by central difference. This is what makes the
    // motion belong to the fractal: particles run along its contours instead
    // of drifting over a picture of it.
    "  vec2 e = vec2(1.5) / uDim;\n"
    "  e = max(e, vec2(0.002));\n"
    "  float gx = fieldAt(pos + vec2(e.x, 0.0)) - fieldAt(pos - vec2(e.x, 0.0));\n"
    "  float gy = fieldAt(pos + vec2(0.0, e.y)) - fieldAt(pos - vec2(0.0, e.y));\n"
    "  vec2 grad = vec2(gx, gy);\n"
    // Perpendicular to the gradient: this is what makes them ORBIT the
    // structure rather than fall into its minima and stop.
    "  vec2 perp = vec2(-grad.y, grad.x);\n"
    "\n"
    // Gravity climbs the field, so particles gather on the generator's
    // structure; swirl runs along the contour so they circulate rather than
    // pile up in the minima.
    "  vec2 acc = grad * uGravity + perp * uSwirl;\n"
    // Undirected energy, re-rolled every frame so it never settles into a
    // pattern of its own.
    "  vec2 jit = hash2(vec2(id, floor(uTime * 60.0))) - 0.5;\n"
    "  acc += jit * uMotion * 4.0;\n"
    // The spring home. Stiffness rises steeply with cohesion so the top of
    // the knob's travel is genuinely locked rather than merely tight -- a
    // linear ramp leaves a visible wobble at 0.9 that reads as a bug.
    "  float k = uCohesion * uCohesion * 60.0;\n"
    "  acc += (home - pos) * k;\n"
    // Drift is scaled DOWN by cohesion so ONE knob runs the whole range:
    // locked at 1, chaotic at 0, without having to back off the others.
    // Fully, not partly. Leaving even 15% of the drive in place at cohesion 1
    // let particles settle slightly off-home wherever the gradient was
    // strong, which showed up as faint streaks across the flat areas.
    "  acc *= (1.0 - uCohesion);\n"
    "  vel = vel * uDamping + acc * uDt * uSpeed * 10.0;\n"
    // Keep the un-wrapped position: leaving the frame is only visible for the
    // one step before the wrap puts it back, so the edge test has to happen
    // here and not after.
    "  vec2 raw = pos + vel * uDt * uSpeed;\n"
    "\n"
    // ---- causes of death ------------------------------------------------
    // Each is a hazard: a rate of dying while the condition holds. Every one
    // of them is skipped entirely at zero, including the field fetch the
    // first one needs -- an unused cause costs nothing at all.
    "  bool dead = false;\n"
    "  vec2 dice = hash2(vec2(id + 0.5, floor(uTime * 60.0) + 91.7));\n"
    "\n"
    // FALLING IN. The set interior is black: no field, no gradient, nothing
    // to move a particle that wanders in, so they pool there permanently.
    // That was a bug that looked like a feature; this makes it the feature,
    // and the interior becomes somewhere you can be lost rather than parked.
    "  if (uDieInterior > 0.0) {\n"
    "    float w = interiorWeight(fieldAt(raw));\n"
    "    if (dice.x < hazard(uDieInterior * w, uDt)) dead = true;\n"
    "  }\n"
    // LEAVING. A probability rather than a switch, so the knob crossfades
    // between a world that wraps and a world with a lethal edge instead of
    // snapping between two looks.
    "  if (uDieEdge > 0.0 &&\n"
    "      (min(raw.x, raw.y) < 0.0 || max(raw.x, raw.y) > 1.0)) {\n"
    "    if (dice.y < uDieEdge) dead = true;\n"
    "  }\n"
    // EXHAUSTION. Speed is the only evidence of how hard a particle has been
    // thrown that survives without an age channel, and it is the right
    // evidence: MOTION spends a particle, drifting does not.
    "  if (uDieExhaust > 0.0) {\n"
    "    vec2 d2 = hash2(vec2(id + 1.5, floor(uTime * 60.0) + 7.3));\n"
    "    float w = exhaustWeight(length(vel));\n"
    "    if (d2.x < hazard(uDieExhaust * w, uDt)) dead = true;\n"
    "  }\n"
    "\n"
    "  if (dead) {\n"
    "    fragColor = spawnState(id, home, floor(uTime * 60.0) + 5.1);\n"
    "    return;\n"
    "  }\n"
    "\n"
    // Wrap rather than clamp: clamping piles every escaped particle onto the
    // border in a bright line.
    "  pos = mix(fract(raw + 1.0), clamp(raw, 0.0, 1.0), step(0.5, uCohesion));\n"
    "  fragColor = vec4(pos, vel);\n"
    "}\n";

static const char* kDrawVert =
    "uniform sampler2D uState;\n"
    "uniform sampler2D uField;\n"
    "uniform vec2  uDim;\n"
    "uniform vec2  uSrcScale;\n"
    "uniform float uSize;\n"
    "uniform float uCohesion;\n"
    "uniform float uCarry;\n"
    "uniform float uCount;\n"
    "uniform vec2  uCell;\n"
    "out float vField;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    // No vertex buffer: each point fetches its own state by its index. That
    // is the whole reason nothing is uploaded per frame.
    "  int id = gl_VertexID;\n"
    "  ivec2 t = ivec2(id % int(uDim.x), id / int(uDim.x));\n"
    "  vec4 st = texelFetch(uState, t, 0);\n"
    // Must derive HOME exactly as the update pass does, from the same COUNT.
    // If these two disagree, CARRY samples the field at a point the particle
    // is not being pulled towards and the reconstruction smears.
    "  vec2 adim = max(floor(uDim * sqrt(max(uCount, 0.0001))), vec2(1.0));\n"
    "  vec2 home = (vec2(t) + 0.5) / adim;\n"
    // Sampling at HOME makes the image shatter and the pieces keep their
    // colour; sampling at the current position makes them dust drifting over
    // it. CARRY chooses.
    "  vec2 sp = mix(st.xy, home, uCarry);\n"
    // Carry the field's ACTUAL COLOUR. Re-mapping its luma through the
    // palette would be a second colouring of an already-coloured image: a
    // black interior came back as palette index 0, which is bright red, and
    // a cohesive swarm could never reproduce the picture it is made of.
    "  vColor = texture(uField, clamp(sp, 0.0, 1.0) * uSrcScale).rgb;\n"
    "  vField = dot(vColor, vec3(0.2126, 0.7152, 0.0722));\n"
    "  gl_Position = vec4(st.xy * 2.0 - 1.0, 0.0, 1.0);\n"
    // Parked particles are pushed outside clip space so they are culled
    // before rasterisation.
    "  if (st.x < -0.5) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);\n"
    // At full cohesion the sprite is exactly one cell, so the pool tiles the
    // screen with no gaps and the image is reconstructed. Below that it
    // shrinks to the free size and the grains separate.
    "  float cell = max(uCell.x, uCell.y);\n"
    "  gl_PointSize = mix(uSize, cell, uCohesion);\n"
    "}\n";

static const char* kDrawFrag =
    "in float vField;\n"
    "in vec3 vColor;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uPalette;\n"
    "uniform float uPaletteShift;\n"
    "uniform float uPaletteMix;\n"
    "uniform float uBrightness;\n"
    "uniform float uFieldColor;\n"
    "uniform float uCohesion;\n"
    // 1 for the scene. Below that when the same pass is aimed at the ink
    // canvas, where it is how much of the swarm's colour each frame writes.
    "uniform float uAlpha;\n"
    "void main() {\n"
    // Round, soft-edged sprite. gl_PointCoord is the only thing that makes a
    // point look like anything other than a square.
    "  vec2 d = gl_PointCoord - 0.5;\n"
    "  float r = dot(d, d);\n"
    // The cut is a CIRCLE inscribed in the point's square, so at full
    // cohesion the discarded corners left a regular grid of holes between
    // neighbouring cells. Open the cut up as cohesion rises so the sprites
    // become squares that tile without gaps.
    "  if (r > mix(0.25, 0.5, uCohesion)) discard;\n"
    // Square-edged and opaque when cohesive so neighbours meet without a
    // seam; round and soft when loose so the swarm reads as grains.
    "  float a = mix(1.0 - smoothstep(0.0, 0.25, r), 1.0, uCohesion) * uAlpha;\n"
    "\n"
    // Default is the field's own colour, so a cohesive swarm reproduces the
    // image exactly. TINT re-reads the palette by brightness instead, which
    // is a stylisation you ask for rather than one you cannot avoid.
    "  float t = fract(vField + uPaletteShift);\n"
    "  vec3 c1 = texture(uPalette, vec2(t, 0.25)).rgb;\n"
    "  vec3 c2 = texture(uPalette, vec2(t, 0.75)).rgb;\n"
    "  vec3 tinted = mix(c1, c2, uPaletteMix);\n"
    "  vec3 col = mix(vColor, tinted, clamp(uFieldColor, 0.0, 1.0));\n"
    "  fragColor = vec4(col * a * uBrightness, a);\n"
    "}\n";

bool Particles::init(int texW, int texH, int contextCount) {
    texW_ = texW;
    texH_ = texH;
    tri_.create(contextCount);
    vaos_.assign(contextCount > 0 ? contextCount : 1, 0);

    glGenTextures(2, tex_);
    glGenFramebuffers(2, fbo_);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex_[i]);
        // RGBA32F: half precision would quantise a 0..1 position to roughly
        // a 2048-step grid, which is coarser than the pixels the particle is
        // meant to move between.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texW, texH, 0, GL_RGBA,
                     GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex_[i], 0);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            // Almost always a driver without float colour targets. Say so
            // plainly; the mode is then unavailable rather than broken.
            std::fprintf(stderr,
                         "[particles] no float render target (0x%04x); "
                         "particle mode unavailable\n", st);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            shutdown();
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::string vert = std::string(glslPreamble()) + glslFullscreenVert();
    if (!update_.build(vert,
                       std::string(glslPreamble()) + lifecycleDefines() +
                           kUpdateFrag,
                       "particle_update")) {
        shutdown();
        return false;
    }
    if (!draw_.build(std::string(glslPreamble()) + kDrawVert,
                     std::string(glslPreamble()) + kDrawFrag, "particle_draw")) {
        shutdown();
        return false;
    }

    available_ = true;
    seed();
    return true;
}

void Particles::shutdown() {
    if (tex_[0]) glDeleteTextures(2, tex_);
    if (fbo_[0]) glDeleteFramebuffers(2, fbo_);
    tex_[0] = tex_[1] = fbo_[0] = fbo_[1] = 0;
    for (GLuint v : vaos_) {
        if (v) glDeleteVertexArrays(1, &v);
    }
    vaos_.clear();
    update_.destroy();
    draw_.destroy();
    tri_.destroy();
    available_ = false;
}

void Particles::seed() {
    // The update shader respawns anything at x < -0.5, so clearing to that
    // scatters the whole pool on the next step without a separate seed pass.
    seeded_ = false;
}

void Particles::update(GLuint field, float sx, float sy, float dt, double time,
                       const ParticleParams& p, int ctx) {
    if (!available_) return;

    if (!seeded_) {
        for (int i = 0; i < 2; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
            glViewport(0, 0, texW_, texH_);
            glClearColor(-1.0f, -1.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        seeded_ = true;
        seedTime_ = time;
    }

    int dst = 1 - cur_;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[dst]);
    glViewport(0, 0, texW_, texH_);
    glDisable(GL_BLEND);

    update_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_[cur_]);
    update_.set1i("uState", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, field);
    update_.set1i("uField", 1);

    update_.set2f("uSrcScale", sx, sy);
    update_.set2f("uDim", static_cast<float>(texW_), static_cast<float>(texH_));
    update_.set1f("uDt", dt);
    update_.set1f("uTime", static_cast<float>(time - seedTime_));
    update_.set1f("uSpeed", p.speed);
    update_.set1f("uGravity", p.gravity);
    update_.set1f("uSwirl", p.swirl);
    update_.set1f("uMotion", p.motion);
    update_.set1f("uDamping", p.damping);
    update_.set1f("uLife", p.life);
    update_.set1f("uCount", p.count);
    update_.set1f("uCohesion", p.cohesion);
    update_.set1f("uDieInterior", p.dieInterior);
    update_.set1f("uDieEdge", p.dieEdge);
    update_.set1f("uDieExhaust", p.dieExhaust);

    tri_.draw(ctx);
    cur_ = dst;
}

void Particles::draw(int outW, int outH, GLuint field, float sx, float sy,
                     GLuint palette, const ParticleParams& p, int ctx) {
    drawPoints(outW, outH, field, sx, sy, palette, p, ctx, 1.0f, false);
}

void Particles::deposit(int outW, int outH, GLuint field, float sx, float sy,
                        GLuint palette, const ParticleParams& p, int ctx) {
    // Exactly free at zero: no pass, no state change, no ink marked as having
    // content. "Nearly nothing" would still put the canvas into the composite
    // and the loop would be running whether or not you asked for it.
    if (p.deposit <= 0.0f) return;
    // Deliberately the SAME pass, into a different framebuffer, with ONE
    // difference: the blend is a LERP of the canvas towards the swarm rather
    // than the OVER the scene draw uses. That difference is the whole reason
    // this loop is safe, and it was found by running it: the loop closes
    // through the composite, so the ink is part of the field the particles
    // read, and anything that can only climb runs away exactly like FEEDBACK
    // did under max().
    //
    // OVER already mixes the COLOUR -- dst*(1-a) + col*a is bounded by its
    // inputs. But it ratchets the ALPHA: a' = a + dst*(1-a) can only rise, so
    // a canvas deposited into every frame reaches full opacity whatever the
    // amount, hides the generator completely, and leaves the swarm reading a
    // flat field with no structure left to steer it. Measured: a loose swarm
    // at DEPOSIT 0.6 collapsed to one uniform magenta in a few hundred frames.
    //
    // Lerping all four channels instead makes the canvas converge ON the
    // deposit amount rather than through it: at 0.6 the ink settles at 0.6
    // opacity and the generator keeps showing through the other 0.4, which is
    // what keeps the loop a coupling instead of a takeover.
    drawPoints(outW, outH, field, sx, sy, palette, p, ctx, p.deposit, true);
}

void Particles::drawPoints(int outW, int outH, GLuint field, float sx, float sy,
                           GLuint palette, const ParticleParams& p, int ctx,
                           float alpha, bool lerpIntoTarget) {
    if (!available_) return;
    if (ctx < 0 || ctx >= static_cast<int>(vaos_.size())) ctx = 0;
    if (vaos_.empty()) return;
    if (!vaos_[ctx]) glGenVertexArrays(1, &vaos_[ctx]);

    glViewport(0, 0, outW, outH);
    // Premultiplied OVER, not additive. Additive looks better for a loose
    // swarm, but it double-brightens wherever sprites overlap -- and at full
    // cohesion they tile edge to edge, so the reconstructed image would come
    // out blown out. Correct at the tight end matters more; BRIGHT covers the
    // glow at the loose end.
    glEnable(GL_BLEND);
    // The fragment is premultiplied either way. GL_ONE is OVER -- correct for
    // the scene, where the target is thrown away every frame. GL_SRC_ALPHA
    // makes the same fragment a lerp of the target towards it, alpha
    // included, which is what a target that is READ BACK needs: see deposit().
    glBlendFunc(lerpIntoTarget ? GL_SRC_ALPHA : GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#if defined(NV_PLATFORM_GLFW)
    // On desktop GL the vertex shader may only set gl_PointSize when this is
    // enabled; on ES it is always in effect.
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif

    draw_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_[cur_]);
    draw_.set1i("uState", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, field);
    draw_.set1i("uField", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, palette);
    draw_.set1i("uPalette", 2);

    draw_.set2f("uDim", static_cast<float>(texW_), static_cast<float>(texH_));
    draw_.set2f("uSrcScale", sx, sy);
    draw_.set1f("uSize", p.size);
    draw_.set1f("uBrightness", p.brightness);
    draw_.set1f("uPaletteShift", p.paletteShift);
    draw_.set1f("uPaletteMix", p.paletteMix);
    draw_.set1f("uFieldColor", p.fieldColor);
    draw_.set1f("uCohesion", p.cohesion);
    draw_.set1f("uCarry", p.carry);
    draw_.set1f("uCount", p.count);
    draw_.set1f("uAlpha", alpha);
    // The cell this particle owns, in pixels. At full cohesion the sprite is
    // exactly this big, so the pool tiles the surface with no gaps.
    draw_.set2f("uCell", static_cast<float>(outW) / texW_,
                static_cast<float>(outH) / texH_);

    glBindVertexArray(vaos_[ctx]);
    glDrawArrays(GL_POINTS, 0, texW_ * texH_);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

}  // namespace nv
