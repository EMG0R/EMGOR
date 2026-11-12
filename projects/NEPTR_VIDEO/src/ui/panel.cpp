#include "ui/panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "shader_source.h"
#include "ui/font.h"

namespace nv {

// x, y, u, v, r, g, b, a
static constexpr int kFloatsPerVert = 8;

static const char* kPanelVert =
    "layout(location = 0) in vec2 aPos;\n"   // pixels
    "layout(location = 1) in vec2 aUV;\n"
    "layout(location = 2) in vec4 aCol;\n"
    "uniform vec2 uSurface;\n"
    // Degrees CLOCKWISE. uSurface is the LOGICAL size, so the layout above is
    // already correct for what the user sees; only the final placement turns.
    "uniform int uRot;\n"
    "out vec2 vUV;\n"
    "out vec4 vCol;\n"
    "void main() {\n"
    "  vec2 p = aPos / uSurface;\n"
    "  vec2 n = vec2(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0);\n"
    "  if (uRot == 90)       n = vec2(n.y, -n.x);\n"
    "  else if (uRot == 180) n = -n;\n"
    "  else if (uRot == 270) n = vec2(-n.y, n.x);\n"
    "  gl_Position = vec4(n, 0.0, 1.0);\n"
    "  vUV = aUV;\n"
    "  vCol = aCol;\n"
    "}\n";

static const char* kPanelFrag =
    "in vec2 vUV;\n"
    "in vec4 vCol;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uAtlas;\n"
    "void main() {\n"
    "  float a = texture(uAtlas, vUV).r;\n"
    "  if (a < 0.5) discard;\n"
    "  fragColor = vCol;\n"
    "}\n";

bool Panel::init(int contextCount) {
    std::string vs = std::string(glslPreamble()) + kPanelVert;
    std::string fs = std::string(glslPreamble()) + kPanelFrag;
    if (!prog_.build(vs, fs, "panel")) return false;

    vaos_.assign(contextCount > 0 ? contextCount : 1, 0);
    glGenBuffers(1, &vbo_);

    std::vector<uint8_t> px = font::buildAtlas();
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font::atlasWidth(), font::atlasHeight(),
                 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
    // NEAREST: a bitmap font scaled with LINEAR turns to grey mush, and the
    // solid cell would develop soft edges.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return glCheck("panel init");
}

void Panel::shutdown() {
    prog_.destroy();
    if (vbo_) glDeleteBuffers(1, &vbo_);
    for (GLuint v : vaos_) {
        if (v) glDeleteVertexArrays(1, &v);
    }
    vaos_.clear();
    if (atlas_) glDeleteTextures(1, &atlas_);
    vbo_ = atlas_ = 0;
}

void Panel::flush(int ctx) {
    if (verts_.empty()) return;
    if (ctx < 0 || ctx >= static_cast<int>(vaos_.size())) ctx = 0;
    if (vaos_.empty()) return;

    // Each context needs its own VAO, and the attribute pointers live IN the
    // VAO -- so the format has to be declared once per context, not once per
    // program. Created on first use because only one context is current at
    // init time.
    if (!vaos_[ctx]) {
        glGenVertexArrays(1, &vaos_[ctx]);
        glBindVertexArray(vaos_[ctx]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        const GLsizei stride = kFloatsPerVert * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                              (void*)(2 * sizeof(float)));
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                              (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }

    // Same reason as Renderer::present: whatever ran before may have left an
    // offscreen target bound, and the panel belongs on the screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // The VIEWPORT is physical; surfW_/surfH_ are logical. Under a quarter
    // turn they are swapped, so the viewport has to be too or the panel is
    // drawn into a corner of the screen.
    bool quarter = (rotation_ == 90 || rotation_ == 270);
    glViewport(0, 0,
               static_cast<GLsizei>(quarter ? surfH_ : surfW_),
               static_cast<GLsizei>(quarter ? surfW_ : surfH_));

    prog_.use();
    prog_.set2f("uSurface", surfW_, surfH_);
    prog_.set1i("uRot", rotation_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    prog_.set1i("uAtlas", 0);

    glBindVertexArray(vaos_[ctx]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(float), verts_.data(),
                 GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(verts_.size() / kFloatsPerVert));
    glBindVertexArray(0);
}

void Panel::pushRect(const Rect& r, float cr, float cg, float cb, float ca) {
    // Sample the middle of the solid cell so NEAREST can never pick up a
    // neighbouring glyph's edge column.
    const float u0 = (font::kSolidCell * font::kCellW + 1.5f) / font::atlasWidth();
    const float u1 = (font::kSolidCell * font::kCellW + 4.5f) / font::atlasWidth();
    const float v0 = 1.5f / font::atlasHeight();
    const float v1 = 5.5f / font::atlasHeight();

    const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    const float quad[6][8] = {
        {x0, y0, u0, v0, cr, cg, cb, ca}, {x1, y0, u1, v0, cr, cg, cb, ca},
        {x1, y1, u1, v1, cr, cg, cb, ca}, {x0, y0, u0, v0, cr, cg, cb, ca},
        {x1, y1, u1, v1, cr, cg, cb, ca}, {x0, y1, u0, v1, cr, cg, cb, ca},
    };
    for (auto& v : quad) verts_.insert(verts_.end(), v, v + 8);
}

float Panel::textWidth(const std::string& s, float scale) const {
    return static_cast<float>(s.size()) * font::kCellW * scale;
}

void Panel::pushText(const std::string& s, float x, float y, float scale,
                     float cr, float cg, float cb, float ca) {
    const float gw = font::kGlyphW * scale;
    const float gh = font::kGlyphH * scale;
    for (size_t i = 0; i < s.size(); ++i) {
        int cell = font::cellFor(s[i]);
        if (cell == 0) continue;  // space: nothing to draw
        float cx = x + static_cast<float>(i) * font::kCellW * scale;

        float u0 = static_cast<float>(cell * font::kCellW) / font::atlasWidth();
        float u1 = static_cast<float>(cell * font::kCellW + font::kGlyphW) /
                   font::atlasWidth();
        float v0 = 0.0f;
        float v1 = static_cast<float>(font::kGlyphH) / font::atlasHeight();

        const float quad[6][8] = {
            {cx, y, u0, v0, cr, cg, cb, ca},
            {cx + gw, y, u1, v0, cr, cg, cb, ca},
            {cx + gw, y + gh, u1, v1, cr, cg, cb, ca},
            {cx, y, u0, v0, cr, cg, cb, ca},
            {cx + gw, y + gh, u1, v1, cr, cg, cb, ca},
            {cx, y + gh, u0, v1, cr, cg, cb, ca},
        };
        for (auto& v : quad) verts_.insert(verts_.end(), v, v + 8);
    }
}

float Panel::pushTextFit(const std::string& s, float x, float y, float maxW,
                         float scale, float cr, float cg, float cb, float ca) {
    float w = textWidth(s, scale);
    if (w > maxW && w > 0.0f) {
        // Shrink rather than clip: a truncated readout is worse than a small
        // one, because you cannot tell it has been truncated.
        scale = std::max(1.0f, scale * (maxW / w));
        w = textWidth(s, scale);
    }
    pushText(s, x, y, scale, cr, cg, cb, ca);
    return w;
}

const char* pageName(Page p) {
    switch (p) {
        case Page::Main:   return "MAIN";
        case Page::Colour: return "COLOUR";
        case Page::Shape:  return "SHAPE";
        case Page::Gen:    return "GEN";
        case Page::Swarm:  return "SWARM";
        case Page::Draw:   return "DRAW";
        case Page::Mod:    return "MOD";
        case Page::Map:    return "MAP";
        case Page::Fx:     return "FX";
        case Page::Input:  return "INPUT";
        default:           return "?";
    }
}

void Panel::cyclePage(int delta) {
    int n = static_cast<int>(Page::kCount);
    page_ = static_cast<Page>(((static_cast<int>(page_) + delta) % n + n) % n);
}

bool Panel::onPage(Page pg, const std::string& g) {
    switch (pg) {
        case Page::Main:   return g == "global" || g == "layer";
        case Page::Colour: return g == "palette";
        case Page::Shape:  return g == "patch" || g == "morph";
        // GEN draws the bank itself, not a bank of knobs: no parameters.
        case Page::Gen:    return false;
        case Page::Swarm:  return g == "particles";
        case Page::Draw:   return g == "draw";
        case Page::Mod:    return g == "lfo" || g == "macro";
        // MAP draws the table, not the bank: it has no parameters of its own.
        case Page::Map:    return false;
        // Effects name their own group, so FX is "everything that is not one
        // of the fixed sections" -- adding an effect never needs a page.
        case Page::Fx:
            return g != "global" && g != "layer" && g != "palette" &&
                   g != "patch" && g != "morph" && g != "particles" &&
                   g != "draw" && g != "lfo" && g != "macro";
        default: return false;
    }
}

void Panel::draw(const ParamBank& params, const PanelStatus& st,
                 const std::vector<MonitorRow>& monitor, double now, int w, int h,
                 int ctx) {
    if (w <= 0 || h <= 0) return;
    verts_.clear();
    sliders_.clear();
    // Symmetric with drawHome() clearing sliders_: whichever view drew last
    // owns the hit-test surface entirely, so a tap can never reach a target
    // belonging to a screen that is no longer up.
    homeRects_.clear();
    modButtons_.clear();
    mapRects_.clear();
    genRects_.clear();
    surfW_ = static_cast<float>(w);
    surfH_ = static_cast<float>(h);

    if (page_ == Page::Input) {
        drawMonitor(monitor, st, now, w, h, ctx);
        return;
    }
    if (page_ == Page::Map) {
        drawMappings(params, st, w, h, ctx);
        return;
    }
    if (page_ == Page::Gen) {
        drawGenerators(st, w, h, ctx);
        return;
    }

    // Only visible parameters ON THIS PAGE take a row.
    std::vector<int> rows;
    rows.reserve(params.count());
    for (int i = 0; i < params.count(); ++i) {
        if (params.def(i).hidden) continue;
        if (!onPage(page_, params.def(i).group)) continue;
        rows.push_back(i);
    }
    const int n = static_cast<int>(rows.size());

    // Pick the largest scale at which EVERY parameter fits. Silently dropping
    // controls off the bottom of the surface is worse than drawing them
    // small: a control you cannot see is a control you cannot reach.
    // The MOD page carries a band of TARGET buttons under the header, so it
    // has that much less room for sliders. Folded into the fitting loop rather
    // than subtracted afterwards: the loop's whole job is "does everything
    // fit", and a band it did not know about would push the last LFO's knobs
    // off the bottom of the surface.
    const int lfoSlots = static_cast<int>(lfoTargets_.size());
    const bool modBand = (page_ == Page::Mod && lfoSlots > 0);

    float scale = 1.0f;
    int cols = 1;
    int rowsPerCol = 1;
    for (float s = 6.0f; s >= 1.0f; s -= 1.0f) {
        float lh = font::kCellH * s * 2.2f;
        float head = lh * 3.1f;
        float band = modBand ? lh * 1.9f : 0.0f;
        float rh = lh * 1.35f;
        int c = std::max(1, static_cast<int>(surfW_ / (190.0f * s)));
        int r = std::max(1, static_cast<int>((surfH_ - head - band - 6.0f * s) / rh));
        if (c * r >= n || s == 1.0f) {
            scale = s;
            cols = c;
            rowsPerCol = r;
            break;
        }
    }

    const float lineH = font::kCellH * scale * 2.2f;
    const float pad = 6.0f * scale;
    // Taller header now that it carries the page tabs.
    const float headerH = lineH * 3.1f;
    const float bandH = modBand ? lineH * 1.9f : 0.0f;

    pushRect({0, 0, surfW_, surfH_}, theme::kBg);
    pushRect({0, 0, surfW_, headerH}, theme::kBgBot);

    char line[256];
    std::snprintf(line, sizeof(line), "%s  %s  %s", st.kernelName.c_str(),
                  st.colorMode.c_str(), st.trapShape.c_str());
    pushTextFit(line, pad, pad, surfW_ - pad * 2.0f, scale * 1.6f, theme::kFg);

    std::snprintf(line, sizeof(line), "%s  %dFPS  %dX%d  IT%d  %dPASS%s",
                  st.mode.c_str(), st.fps, st.renderWidth, st.renderHeight,
                  st.iterations, st.passes, st.governed ? "  GOV" : "");
    // ALERT is spent only on things that are actually wrong -- here, the
    // governor having given up detail to hold the frame rate.
    float statusW = pushTextFit(line, pad, pad + lineH * 1.05f,
                                surfW_ - pad * 2.0f, scale,
                                st.governed ? theme::kAlert : theme::kOffText);

    // The palette line shares a row with the status line above. On a narrow
    // surface (the 480px onboard panel at scale 2) the two ran into each
    // other and rendered as unreadable overlapping glyphs, so it is drawn
    // only when it actually fits. It is never the only place this
    // information appears -- the COLOUR page carries it too.
    float statusEnd = pad + statusW;
    char palLine[128];
    std::snprintf(palLine, sizeof(palLine), "PAL %s > %s", st.paletteA.c_str(),
                  st.paletteB.c_str());
    float palW = textWidth(palLine, scale);
    if (surfW_ - pad - palW > statusEnd + pad) {
        pushText(palLine, surfW_ - pad - palW, pad + lineH * 1.05f, scale,
                 theme::kOffText);
    }

    // Page tabs: the current page lit, the rest dim, so TAB has somewhere
    // visible to go and you can tell what the instrument even has.
    {
        float ty = pad + lineH * 1.75f;
        // Every tab has to be visible: a page you cannot see is a page you
        // will not know to reach for.
        float need = 0.0f;
        for (int i = 0; i < static_cast<int>(Page::kCount); ++i) {
            need += textWidth(pageName(static_cast<Page>(i)), scale) + 7.0f * scale;
        }
        float tabScale = scale;
        if (need > surfW_ - pad * 2.0f) {
            tabScale = std::max(1.0f, scale * ((surfW_ - pad * 2.0f) / need));
        }
        float tx = pad;
        for (int i = 0; i < static_cast<int>(Page::kCount); ++i) {
            bool on = (i == static_cast<int>(page_));
            const char* nm = pageName(static_cast<Page>(i));
            float w2 = textWidth(nm, tabScale);
            if (on) {
                pushRect({tx - 2.0f * tabScale, ty - 2.0f * tabScale,
                          w2 + 4.0f * tabScale,
                          font::kGlyphH * tabScale + 4.0f * tabScale},
                         theme::kAccent);
            }
            pushText(nm, tx, ty, tabScale, on ? theme::kBg : theme::kOffText);
            tx += w2 + 7.0f * tabScale;
        }
        // tx is where the tabs ended; only place the chip if it clears them.
        if (!st.status.empty()) {
            float sw = textWidth(st.status, scale);
            if (surfW_ - pad - sw > tx) {
                pushText(st.status, surfW_ - pad - sw, ty, scale,
                         theme::kTrackActive);
            }
        }
    }

    // ---- MOD: the TARGET buttons ----------------------------------------
    // One button per LFO, showing what it drives. Tapping it arms LEARN; the
    // next parameter you touch -- on any page -- becomes the target.
    //
    // A button rather than a list of every id in the bank: the bank's tail
    // changes with the kernel and the chain, so a list would be a second place
    // parameters are named and a second thing to keep in step with the
    // sliders. The slider IS the name.
    if (modBand) {
        const float bw = surfW_ / static_cast<float>(lfoSlots);
        for (int n = 0; n < lfoSlots; ++n) {
            Rect r{static_cast<float>(n) * bw + pad * 0.5f, headerH + pad * 0.5f,
                   bw - pad, bandH - pad};
            modButtons_.push_back(r);

            const bool armed = (learnSlot_ == n);
            const LfoInfo& info = lfoTargets_[static_cast<size_t>(n)];
            // Present, absent, or assigned to something this patch does not
            // have. The third is normal -- switch kernels and an LFO can
            // legitimately point at a knob that is not here right now -- but
            // it has to be VISIBLE, or a silent modulator reads as a fault.
            bool missing = info.dangling;

            pushRect(r, armed ? theme::kAccent : theme::kBgBot);
            if (!armed) {
                // A hairline so an unlit button still reads as a button.
                pushRect({r.x, r.y + r.h - scale, r.w, scale},
                         missing ? theme::kAlert : theme::kAccentDim);
            }

            char label[96];
            if (armed) {
                std::snprintf(label, sizeof(label), "L%d TOUCH A KNOB %.0f",
                              n + 1, std::ceil(learnLeft_));
            } else if (info.count == 0) {
                std::snprintf(label, sizeof(label), "L%d>-", n + 1);
            } else if (info.count == 1) {
                std::snprintf(label, sizeof(label), "L%d>%s", n + 1,
                              info.first.c_str());
            } else {
                // Several destinations cannot be read on a button this wide,
                // and half of them listed would be worse than a count: it
                // would look like all of them. The MAP page carries the list.
                std::snprintf(label, sizeof(label), "L%d>%d DESTS", n + 1,
                              info.count);
            }
            theme::Colour col = armed ? theme::kBg
                                      : (info.count == 0 ? theme::kOffText
                                                     : (missing ? theme::kAlert
                                                                : theme::kTrackActive));
            pushTextFit(label, r.x + pad * 0.5f,
                        r.y + (r.h - font::kGlyphH * scale) * 0.5f,
                        r.w - pad, scale, col);
        }
    }

    // ---- slider grid ----------------------------------------------------
    // Flows into as many columns as the surface holds. Nothing here is a
    // fixed coordinate: the same code lays out 800x480 and 2560x1440.
    const float colW = surfW_ / static_cast<float>(cols);
    const float rowH = lineH * 1.35f;
    std::string lastGroup;

    for (int r = 0; r < n; ++r) {
        int col = r / rowsPerCol;
        int row = r % rowsPerCol;
        if (col >= cols) break;

        const int i = rows[r];
        float x = col * colW + pad;
        float y = headerH + bandH + pad + row * rowH;
        float innerW = colW - pad * 2.0f;

        const ParamDef& d = params.def(i);
        // The bar shows what the parameter IS -- slewed and modulated -- so a
        // glide is visible as a bar sliding and an LFO as a bar breathing. The
        // target is drawn separately below; showing only the target would make
        // the panel disagree with the screen, and showing only the modulated
        // value would lose where you actually set the control.
        float t = params.normMod(i);

        // Group changes get a tint on the label so the chain reads as
        // sections without spending a row on a heading.
        bool groupHead = (d.group != lastGroup);
        lastGroup = d.group;

        float labelScale = scale;
        // Only prefix with the group where it disambiguates -- inside a page
        // that IS the group, repeating it is noise.
        bool bare = (d.group == "global" || d.group == "layer" ||
                     d.group == "palette" || d.group == "particles" ||
                     d.group == "draw" || d.group == "patch" ||
                     d.group == "lfo" || d.group == "macro");
        std::string label = bare ? d.label : (d.group + " " + d.label);
        // A modulator's routing, on the row you reach for. The DEPTH row is
        // where a hand goes to bring an LFO in, so it is where "in to WHAT"
        // has to be readable; the macro carries its fan-out for the same
        // reason -- a macro wired to nothing looks identical to a broken one.
        std::string routing;
        theme::Colour routingCol = theme::kOffText;
        if (d.group == "lfo" && d.id.size() > 6 &&
            d.id.compare(d.id.size() - 6, 6, ".depth") == 0) {
            int slot = std::atoi(d.id.c_str() + 3);
            if (slot >= 0 && slot < static_cast<int>(lfoTargets_.size())) {
                const LfoInfo& info = lfoTargets_[slot];
                if (info.count == 0) routing = " > -";
                else if (info.count == 1) routing = " > " + info.first;
                else routing = " > " + std::to_string(info.count) + " DESTS";
                // Same three states as the TARGET button above, in the same
                // three colours: unassigned, live, or pointed at a knob this
                // patch does not currently have.
                routingCol = info.count == 0
                                 ? theme::kOffOutline
                                 : (info.dangling ? theme::kAlert
                                                  : theme::kAccent);
            }
        } else if (d.group == "macro") {
            int slot = std::atoi(d.id.c_str() + 5);
            if (slot >= 0 && slot < static_cast<int>(macroFanout_.size())) {
                char n[24];
                std::snprintf(n, sizeof(n), " (%d)", macroFanout_[slot]);
                label += n;
            }
        }
        pushText(label, x, y, labelScale,
                 groupHead ? theme::kFg : theme::kOffText);
        // Drawn as its own run so the routing can carry its own colour -- the
        // label is what the knob IS and the suffix is where it GOES, and only
        // the second of those can be broken.
        if (!routing.empty()) {
            pushText(routing, x + textWidth(label, labelScale), y, labelScale,
                     routingCol);
        }

        // Value, right-aligned against the column edge. A bipolar parameter
        // is drawn SIGNED and always with its sign, because the one thing a
        // reversible control has to say is which way it is going: "1.00" and
        // "-1.00" differ by one glyph and a performer glancing down needs the
        // sign to be there rather than inferred from its absence.
        const bool bipolar = (d.curve == Curve::Bipolar ||
                              d.curve == Curve::BipolarCube);
        float v = params.value(i);
        const char* fmt = std::fabs(v) >= 100.0f ? (bipolar ? "%+.0f" : "%.0f")
                        : std::fabs(v) >= 1.0f   ? (bipolar ? "%+.2f" : "%.2f")
                                                 : (bipolar ? "%+.3f" : "%.3f");
        std::snprintf(line, sizeof(line), fmt, v);
        pushText(line, x + innerW - textWidth(line, labelScale), y, labelScale,
                 theme::kTrackActive);

        Rect bar{x, y + font::kGlyphH * labelScale + 3.0f * scale, innerW,
                 5.0f * scale};
        pushRect(bar, theme::kTrackDim);
        Rect fill = bar;
        if (bipolar) {
            // Grows OUT OF THE CENTRE, both ways. A bipolar quantity drawn as
            // a bar filling from the left says the wrong thing twice: that
            // zero is at one end, and that the two directions are the same
            // gesture with different amounts. The detent gets a tick so it can
            // be found without reading the number.
            float cx = bar.x + bar.w * 0.5f;
            float half = (t - 0.5f) * bar.w;
            fill.x = half >= 0.0f ? cx : cx + half;
            fill.w = std::max(1.5f * scale, std::fabs(half));
            pushRect(fill, theme::kTrackActive);
            pushRect({cx - 0.5f * scale, bar.y - 1.5f * scale, scale,
                      bar.h + 3.0f * scale}, theme::kAccentDim);
        } else {
            fill.w = std::max(2.0f * scale, bar.w * t);
            pushRect(fill, theme::kTrackActive);
        }

        // Where the control is SET, when that is not where the parameter
        // currently is -- mid-glide, or under a modulator. Without it the
        // instrument appears to have moved a knob you did not touch, and the
        // knob you did touch appears to have failed.
        float tgt = params.target(i);
        if (std::fabs(tgt - t) > 0.004f) {
            pushRect({bar.x + bar.w * tgt - scale, bar.y - scale,
                      2.0f * scale, bar.h + 2.0f * scale},
                     theme::kFg);
        }

        // Touch target is deliberately taller than the bar: a 5px bar is not
        // something you can hit with a finger.
        sliders_.push_back({i, {x, y, innerW, rowH * 0.9f}});
    }

    // ---- LEARN banner ---------------------------------------------------
    // On EVERY page, not only MOD: the whole point of the gesture is that you
    // arm it here and then walk to the page carrying the knob you want, and a
    // mode that is invisible from where you are using it is a trap. It says
    // what will happen, to which LFO, how long it has left, and how to get out.
    if (learnSlot_ >= 0) {
        float h = lineH * 1.5f;
        Rect r{0.0f, surfH_ - h, surfW_, h};
        pushRect(r, theme::kAccent);
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "L%d LEARN %.0fS - TOUCH ANY PARAMETER   ESC CANCELS",
                      learnSlot_ + 1, std::ceil(learnLeft_));
        pushTextFit(msg, pad, r.y + (h - font::kGlyphH * scale * 1.2f) * 0.5f,
                    surfW_ - pad * 2.0f, scale * 1.2f, theme::kBg);
    }

    // ---- one upload, one draw ------------------------------------------
    flush(ctx);
}

// ---- MAP: the mapping table, on the device --------------------------------
//
// The zone where routing is visible and editable without a keyboard. Before
// this page an LFO's fan-out could be built by gesture but only READ from a
// file, which is the half-feature that makes a performer distrust the whole
// mechanism -- you cannot undo what you cannot see.
//
// It is the same renderer, the same atlas and the same single draw call as
// every other page. A routing editor is a list of short strings and a few
// bars; a second renderer for that would be four hundred duplicated lines.
// The bank, as buttons. One tap is one generator, wherever it sits in the
// thirty -- the same gesture the browser grid gets, from the same list,
// because there is one bank and it names itself.
//
// Still one draw call and still one texture: a cell is a rect and a run of
// glyphs out of the same atlas as every other page, so thirty cells cost what
// thirty sliders cost, which is nothing measurable.
void Panel::drawGenerators(const PanelStatus& st, int w, int h, int ctx) {
    (void)w;
    (void)h;
    const int n = static_cast<int>(genNames_.size());

    // Same fitting rule as the slider grid: the largest scale at which EVERY
    // entry is on the surface. A generator you cannot see is a generator you
    // will never play.
    float scale = 1.0f;
    int cols = 1, rowsPerCol = 1;
    for (float sc = 6.0f; sc >= 1.0f; sc -= 1.0f) {
        float lh = font::kCellH * sc * 2.2f;
        float head = lh * 3.1f;
        float rh = lh * 1.5f;
        int c = std::max(1, static_cast<int>(surfW_ / (150.0f * sc)));
        int r = std::max(1, static_cast<int>((surfH_ - head - 6.0f * sc) / rh));
        if (c * r >= n || sc == 1.0f) {
            scale = sc;
            cols = c;
            rowsPerCol = r;
            break;
        }
    }

    const float lineH = font::kCellH * scale * 2.2f;
    const float pad = 6.0f * scale;
    const float headerH = lineH * 3.1f;
    const float rowH = lineH * 1.5f;
    const float colW = surfW_ / static_cast<float>(cols);

    pushRect({0, 0, surfW_, surfH_}, theme::kBg);
    pushRect({0, 0, surfW_, headerH}, theme::kBgBot);
    pushTextFit("GENERATOR", pad, pad, surfW_ - pad * 2.0f, scale * 1.6f,
                theme::kFg);
    char line[192];
    std::snprintf(line, sizeof(line), "%d IN BANK   TAP TO SELECT   %s", n,
                  st.kernelName.c_str());
    pushTextFit(line, pad, pad + lineH * 1.05f, surfW_ - pad * 2.0f, scale,
                theme::kOffText);
    // The B slot is marked too, because a morph pair is two selections and a
    // page showing only one would make the other unfindable from here.
    pushTextFit("LIT = CURRENT     UNDERLINE = MORPH TARGET B", pad,
                pad + lineH * 1.95f, surfW_ - pad * 2.0f, scale,
                theme::kOffOutline);

    for (int i = 0; i < n; ++i) {
        int col = i / rowsPerCol;
        int row = i % rowsPerCol;
        if (col >= cols) break;
        Rect r{col * colW + pad * 0.5f, headerH + pad + row * rowH,
               colW - pad, rowH - pad * 0.6f};
        genRects_.push_back(r);

        const bool isA = (i == genCurrent_);
        const bool isB = (i == genB_);
        pushRect(r, isA ? theme::kAccent : theme::kBgBot);
        if (!isA) {
            pushRect({r.x, r.y + r.h - scale, r.w, scale},
                     isB ? theme::kAccent : theme::kAccentDim);
        }
        pushTextFit(genNames_[static_cast<size_t>(i)], r.x + pad * 0.5f,
                    r.y + (r.h - font::kGlyphH * scale) * 0.5f, r.w - pad,
                    scale, isA ? theme::kBg : theme::kTrackActive);
    }

    flush(ctx);
}

int Panel::genCellAt(float nx, float ny) const {
    float x = nx * surfW_;
    float y = ny * surfH_;
    for (size_t i = 0; i < genRects_.size(); ++i) {
        const Rect& r = genRects_[i];
        if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Panel::drawMappings(const ParamBank& params, const PanelStatus& st, int w,
                         int h, int ctx) {
    if (w <= 0 || h <= 0) return;
    verts_.clear();
    // Same rule as every other view: whoever drew last owns the hit-test
    // surface completely, so a tap cannot reach a control that is not up.
    sliders_.clear();
    homeRects_.clear();
    modButtons_.clear();
    mapRects_.clear();
    genRects_.clear();
    surfW_ = static_cast<float>(w);
    surfH_ = static_cast<float>(h);

    const int n = static_cast<int>(mapRows_.size());

    // Width is driven by the longest ids actually in the table rather than by
    // a guess: `sensor:tof2 > g.palette_shift` is a very different row from
    // `lfo:0 + a`, and sizing for the worst case that is not present would
    // shrink the page for nothing.
    size_t srcChars = 6, parChars = 6;
    for (const auto& r : mapRows_) {
        srcChars = std::max(srcChars, r.source.size());
        parChars = std::max(parChars, r.param.size());
    }
    const float textChars = static_cast<float>(srcChars + parChars + 3);

    float scale = 1.0f;
    int cols = 1;
    int rowsPerCol = 1;
    for (float s = 4.0f; s >= 1.0f; s -= 1.0f) {
        float lh = font::kCellH * s * 2.2f;
        float head = lh * 2.9f;
        float rh = lh * 1.35f;
        // Text, plus the amount zone, plus padding. The amount zone is a
        // third of the column, so the text gets the other two thirds.
        float colNeed = (textChars * font::kCellW * s) * 1.55f + 18.0f * s;
        int c = std::max(1, static_cast<int>(surfW_ / colNeed));
        int r = std::max(1, static_cast<int>((surfH_ - head - 6.0f * s) / rh));
        if (c * r >= n || s == 1.0f) {
            scale = s;
            cols = c;
            rowsPerCol = r;
            break;
        }
    }

    const float lineH = font::kCellH * scale * 2.2f;
    const float pad = 6.0f * scale;
    const float headerH = lineH * 2.9f;
    const float rowH = lineH * 1.35f;

    pushRect({0, 0, surfW_, surfH_}, theme::kBg);
    pushRect({0, 0, surfW_, headerH}, theme::kBgBot);

    char line[256];
    pushTextFit("MAPPING", pad, pad, surfW_ - pad * 2.0f, scale * 1.6f,
                theme::kFg);
    int modRows = 0;
    for (const auto& r : mapRows_) {
        if (r.mod) ++modRows;
    }
    // Split the count the way ownership splits it, so "how much of this is
    // mine to edit" is answered before scrolling.
    std::snprintf(line, sizeof(line), "%d ROWS   %d PATCH   %d FILE   OSC %d",
                  n, modRows, n - modRows, st.oscPort);
    pushTextFit(line, pad, pad + lineH * 1.05f, surfW_ - pad * 2.0f, scale,
                theme::kOffText);
    // The two things a performer has to know before touching anything here:
    // which rows are theirs to edit, and what the two marks mean.
    pushTextFit("+ ADDS (PATCH)  > SETS  >> DETENTS  TAP NAME REMOVES",
                pad, pad + lineH * 1.95f, surfW_ - pad * 2.0f, scale,
                theme::kOffOutline);

    if (n == 0) {
        pushText("NO MAPPINGS", pad, headerH + pad * 2.0f, scale * 1.2f,
                 theme::kOffText);
        flush(ctx);
        return;
    }

    const float colW = surfW_ / static_cast<float>(cols);

    for (int i = 0; i < n; ++i) {
        int col = i / rowsPerCol;
        int row = i % rowsPerCol;
        if (col >= cols) break;

        const PanelMapRow& m = mapRows_[static_cast<size_t>(i)];
        float x = col * colW + pad;
        float y = headerH + pad + row * rowH;
        float innerW = colW - pad * 2.0f;

        // Two zones, two gestures. Everything left of the split is the NAME
        // (a tap removes the row); everything right of it is the AMOUNT (a
        // drag retunes it). One split, so a finger never has to be precise
        // about which of several small targets it found.
        MapRect rects;
        rects.name = {x, y, innerW * 0.66f, rowH * 0.92f};
        rects.amount = {x + innerW * 0.68f, y, innerW * 0.32f, rowH * 0.92f};
        mapRects_.push_back(rects);

        // A modulator's arrow is deliberately a DIFFERENT character rather
        // than a different colour alone: this is the one distinction on the
        // page that changes what a row does, and colour is the first thing a
        // stage light takes away.
        // `>>` for a relative row rather than a tilde: the built-in font stops
        // at '_' (ASCII 95), so a tilde silently renders as a SPACE -- which
        // is how an encoder row came out looking like it had no destination at
        // all. Every mark on this page has to exist in the atlas.
        const char* arrow = m.mod ? "+" : (m.relative ? ">>" : ">");
        std::string text = m.source + " " + arrow + " ";
        pushText(text, x, y, scale, m.mod ? theme::kAccent : theme::kOffText);
        // The parameter, in ALERT when the bank does not have it right now.
        // Not an error -- an effect out of the chain, a kernel without that
        // knob -- but a row doing nothing has to say so.
        pushText(m.param, x + textWidth(text, scale), y, scale,
                 m.dangling ? theme::kAlert : theme::kTrackActive);

        // The amount, as a number and as a bar. The number carries the
        // DIRECTION, which a bar cannot: `1.00>0.00` and `0.00>1.00` cover the
        // same ground and mean opposite things.
        char amt[32];
        if (m.mod) std::snprintf(amt, sizeof(amt), "%+.2f", m.hi - m.lo);
        else std::snprintf(amt, sizeof(amt), "%.2f>%.2f", m.lo, m.hi);
        float aw = textWidth(amt, scale);
        pushText(amt, rects.amount.x + std::max(0.0f, rects.amount.w - aw), y,
                 scale, m.mod ? theme::kTrackActive : theme::kOffText);

        Rect bar{rects.amount.x, y + font::kGlyphH * scale + 3.0f * scale,
                 rects.amount.w, 5.0f * scale};
        pushRect(bar, theme::kTrackDim);
        if (m.mod) {
            // Bipolar, drawn from the centre, because that is what the number
            // means and where the detent is: an LFO row at zero span is off,
            // and either side of it is the same shape in opposite directions.
            float span = std::clamp(m.hi - m.lo, -1.0f, 1.0f);
            float mid = bar.x + bar.w * 0.5f;
            float half = bar.w * 0.5f * std::fabs(span);
            pushRect({span >= 0.0f ? mid : mid - half, bar.y,
                      std::max(1.0f * scale, half), bar.h},
                     theme::kAccent);
            pushRect({mid - scale * 0.5f, bar.y - scale, scale,
                      bar.h + 2.0f * scale}, theme::kFg);
        } else {
            // A file row: shown, never edited here. Dim, because "you cannot
            // drag this" has to be legible before the finger lands.
            float a = std::clamp(std::min(m.lo, m.hi), 0.0f, 1.0f);
            float b = std::clamp(std::max(m.lo, m.hi), 0.0f, 1.0f);
            pushRect({bar.x + bar.w * a, bar.y,
                      std::max(1.0f * scale, bar.w * (b - a)), bar.h},
                     theme::kTrackDim);
            pushRect({bar.x + bar.w * a, bar.y,
                      std::max(1.0f * scale, bar.w * (b - a)), bar.h},
                     theme::kOffOutline);
        }
    }

    // Rows that did not fit. Never silently dropped: the whole reason this
    // page exists is that routing you cannot see is routing you distrust.
    int shown = std::min(n, cols * rowsPerCol);
    if (shown < n) {
        std::snprintf(line, sizeof(line), "+%d MORE", n - shown);
        pushText(line, pad, surfH_ - font::kGlyphH * scale - pad, scale,
                 theme::kAlert);
    }

    (void)params;
    flush(ctx);
}

void Panel::drawMonitor(const std::vector<MonitorRow>& rows,
                        const PanelStatus& st, double now, int w, int h, int ctx) {
    if (w <= 0 || h <= 0) return;
    verts_.clear();
    // The monitor owns no sliders; clearing means a stray touch cannot drag a
    // parameter belonging to the page underneath.
    sliders_.clear();
    homeRects_.clear();
    modButtons_.clear();
    mapRects_.clear();
    genRects_.clear();
    surfW_ = static_cast<float>(w);
    surfH_ = static_cast<float>(h);

    const float scale = std::max(1.0f, std::floor(static_cast<float>(h) / 260.0f));
    const float lineH = font::kCellH * scale * 2.2f;
    const float pad = 6.0f * scale;
    const float headerH = lineH * 2.4f;

    pushRect({0, 0, surfW_, surfH_}, theme::kBg);
    pushRect({0, 0, surfW_, headerH}, theme::kBgBot);

    char line[256];
    pushText("INPUT MONITOR", pad, pad, scale * 1.6f, theme::kFg);
    if (st.oscListening) {
        std::snprintf(line, sizeof(line), "OSC %d   %d MAPPINGS   %zu SOURCES",
                      st.oscPort, st.mappingCount, rows.size());
    } else {
        std::snprintf(line, sizeof(line), "OSC PORT CLOSED   %d MAPPINGS",
                      st.mappingCount);
    }
    pushText(line, pad, pad + lineH * 1.05f, scale,
             st.oscListening ? theme::kOffText : theme::kAlert);

    if (rows.empty()) {
        pushText("NO INPUT SEEN YET", pad, headerH + pad * 2.0f, scale * 1.2f,
                 theme::kOffText);
    }

    const float rowH = lineH * 1.35f;
    const int maxRows = std::max(1, static_cast<int>((surfH_ - headerH - pad) / rowH));

    for (size_t i = 0; i < rows.size() && i < static_cast<size_t>(maxRows); ++i) {
        const MonitorRow& r = rows[i];
        float y = headerH + pad + static_cast<float>(i) * rowH;

        // Fade a source out as it goes quiet, so a live signal is obvious at
        // a glance and a stale one does not read as current.
        float age = static_cast<float>(now - r.lastSeen);
        float lit = std::clamp(1.0f - age / 2.0f, 0.25f, 1.0f);

        pushText(r.source, pad, y, scale, fade(theme::kFg, lit));

        std::snprintf(line, sizeof(line), "%.3f", r.value);
        float valX = pad + surfW_ * 0.32f;
        pushText(line, valX, y, scale, fade(theme::kTrackActive, lit));

        // A bar, because a number alone does not show a sensor's jitter or
        // the extent of its travel.
        Rect bar{pad + surfW_ * 0.44f, y + 1.0f * scale, surfW_ * 0.22f, 5.0f * scale};
        pushRect(bar, theme::kTrackDim);
        Rect fill = bar;
        fill.w = std::max(2.0f * scale, bar.w * std::clamp(r.value, 0.0f, 1.0f));
        pushRect(fill, fade(theme::kTrackActive, lit));

        if (r.target.empty()) {
            pushText("UNMAPPED", pad + surfW_ * 0.70f, y, scale, theme::kOffOutline);
        } else {
            pushText(r.target, pad + surfW_ * 0.70f, y, scale,
                     fade(theme::kAccent, lit));
        }
    }

    flush(ctx);
}

void Panel::drawHome(const std::vector<HomeItem>& items, int selected,
                     const std::string& message, float homeHold, int w, int h,
                     int ctx) {
    if (w <= 0 || h <= 0 || items.empty()) return;
    verts_.clear();
    // HOME owns the surface completely: leaving the slider rects behind would
    // let a tap on a menu entry drag whatever parameter used to be there.
    sliders_.clear();
    homeRects_.clear();
    modButtons_.clear();
    mapRects_.clear();
    genRects_.clear();
    surfW_ = static_cast<float>(w);
    surfH_ = static_cast<float>(h);

    const int n = static_cast<int>(items.size());

    // The entries fill the surface. This is the one screen where a finger has
    // to land first time in the dark, so rows are as tall as the display
    // allows rather than as tall as the text needs.
    // No instrument title. NEPTR is the thing you are already holding, and the
    // band it used to occupy is worth more as menu row: five entries need the
    // height that one redundant word was spending. What stays is the one line
    // that tells you what this screen is FOR.
    const float titleBand = surfH_ * 0.07f;
    const float footBand = surfH_ * 0.08f;
    const float listTop = titleBand;
    const float listH = surfH_ - titleBand - footBand;
    const float rowH = listH / static_cast<float>(n);
    const float pad = std::max(4.0f, surfH_ * 0.008f);

    pushRect({0, 0, surfW_, surfH_}, theme::kBg);

    // Bounded by the BAND as well as by the width: fitting text to the surface
    // width alone made it overrun the first entry on a landscape display, and
    // the first entry is the one thing on this screen that must always be
    // readable.
    {
        const std::string t = "SELECT A MODE";
        float s = std::floor(std::min(surfW_ * 0.34f / (t.size() * font::kCellW),
                                      titleBand * 0.52f / font::kGlyphH));
        s = std::max(1.0f, s);
        pushText(t, pad * 2.0f, titleBand * 0.22f, s, theme::kOffText);
    }

    for (int i = 0; i < n; ++i) {
        const HomeItem& it = items[static_cast<size_t>(i)];
        Rect row{pad * 2.0f, listTop + static_cast<float>(i) * rowH,
                 surfW_ - pad * 4.0f, rowH - pad};
        homeRects_.push_back(row);

        const bool sel = (i == selected);
        if (sel) {
            // ACCENT for a mode you can enter; ALERT for one you cannot, so
            // the refusal is legible before you press anything.
            pushRect(row, it.available ? theme::kAccent : theme::kAlert);
        } else {
            pushRect(row, theme::kBgBot);
        }

        // Name scale is chosen per row from the row's own box, so the same
        // code lays out a 480x800 portrait panel and a 1080p projector.
        float nameScale = std::max(
            1.0f, std::floor(std::min(row.h * 0.42f / font::kGlyphH,
                                      (row.w - pad * 4.0f) /
                                          (it.name.size() * font::kCellW))));
        float noteScale = std::max(1.0f, std::floor(nameScale * 0.45f));

        float tx = row.x + pad * 2.0f;
        float ty = row.y + row.h * 0.22f;

        theme::Colour nameCol = sel ? theme::kBg
                                    : (it.available ? theme::kFg
                                                    : theme::kOffOutline);
        pushText(it.name, tx, ty, nameScale, nameCol);

        std::string note = it.available ? it.note : (it.note + "  [UNAVAILABLE]");
        pushText(note, tx, ty + font::kGlyphH * nameScale + pad * 1.5f, noteScale,
                 sel ? theme::kAccentDim : theme::kOffText);
    }

    // Footer: what the controls are, then the message if there is one. A menu
    // reached in the dark has to say how it is driven.
    {
        float s = std::max(1.0f, std::floor(surfW_ / 320.0f));
        float fy = surfH_ - footBand + pad;
        if (!message.empty()) {
            pushTextFit(message, pad * 2.0f, fy, surfW_ - pad * 4.0f, s * 1.2f,
                        theme::kAlert);
        } else {
            // Names the buttons nav.cpp actually binds. It said "B1 SELECT",
            // and B1 is the one face button HOME deliberately leaves unbound —
            // a hint that names the wrong control is worse than no hint, on
            // the exact screen someone reaches in the dark.
            pushTextFit("UP/DOWN MOVE   A OR JOY-CLICK SELECT   HOLD = HOME",
                        pad * 2.0f, fy, surfW_ - pad * 4.0f, s, theme::kOffText);
        }
    }

    // The hold-to-return gesture is invisible until it fires, which makes a
    // 1.5 s wait feel broken. A bar along the bottom edge shows it working.
    if (homeHold > 0.0f) {
        pushRect({0.0f, surfH_ - 4.0f, surfW_ * std::clamp(homeHold, 0.0f, 1.0f),
                  4.0f},
                 theme::kAccent);
    }

    flush(ctx);
}

int Panel::homeEntryAt(float nx, float ny) const {
    float x = nx * surfW_;
    float y = ny * surfH_;
    for (size_t i = 0; i < homeRects_.size(); ++i) {
        const Rect& r = homeRects_[i];
        if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Panel::paramAt(float nx, float ny) const {
    float x = nx * surfW_;
    float y = ny * surfH_;
    for (const auto& s : sliders_) {
        if (x >= s.bar.x && x <= s.bar.x + s.bar.w && y >= s.bar.y &&
            y <= s.bar.y + s.bar.h) {
            return s.param;
        }
    }
    return -1;
}

int Panel::modTargetAt(float nx, float ny) const {
    float x = nx * surfW_;
    float y = ny * surfH_;
    for (size_t i = 0; i < modButtons_.size(); ++i) {
        const Rect& r = modButtons_[i];
        if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Panel::mapRowAt(float nx, float ny, bool* onAmount) const {
    float x = nx * surfW_;
    float y = ny * surfH_;
    for (size_t i = 0; i < mapRects_.size(); ++i) {
        const MapRect& r = mapRects_[i];
        auto in = [x, y](const Rect& b) {
            return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
        };
        if (in(r.amount)) {
            if (onAmount) *onAmount = true;
            return static_cast<int>(i);
        }
        if (in(r.name)) {
            if (onAmount) *onAmount = false;
            return static_cast<int>(i);
        }
    }
    return -1;
}

float Panel::mapAmountAt(int row, float nx) const {
    if (row < 0 || row >= static_cast<int>(mapRects_.size())) return 0.5f;
    const Rect& b = mapRects_[static_cast<size_t>(row)].amount;
    float t = (nx * surfW_ - b.x) / std::max(1.0f, b.w);
    return std::clamp(t, 0.0f, 1.0f);
}

void Panel::onPointerDown(float nx, float ny) { captured_ = paramAt(nx, ny); }

// ny is unused: once a slider is captured the drag is horizontal only.
void Panel::onPointerMove(float nx, [[maybe_unused]] float ny, ParamBank& params) {
    if (captured_ < 0 || captured_ >= params.count()) return;
    // Once a slider is captured the finger owns it until release, even if it
    // slides outside the row -- otherwise a fast drag jumps to a neighbour.
    for (const auto& s : sliders_) {
        if (s.param != captured_) continue;
        float t = (nx * surfW_ - s.bar.x) / std::max(1.0f, s.bar.w);
        // NOW, not glided. A drag is already a continuous stream of positions
        // from a hand that is itself the smoother; slewing it would add pure
        // latency and make the slider feel like it is on the end of a rope.
        // GLIDE is for sources that arrive as a STEP -- a preset recall, a CC
        // jumping across its range -- not for a finger.
        params.setNormNow(captured_, std::clamp(t, 0.0f, 1.0f));
        return;
    }
}

void Panel::onPointerUp() { captured_ = -1; }

}  // namespace nv
