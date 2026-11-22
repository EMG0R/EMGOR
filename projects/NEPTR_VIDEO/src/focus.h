// Where, in the frame just drawn, is the fractal's BOUNDARY?
//
// GL-free so the scoring can be tested against synthetic frames rather than
// against the renderer (tests/test_focus.cpp).
//
// ---- why the boundary, and not "the interesting bit" ----------------------
//
// A dive has to end somewhere with structure at the bottom, and the two ways
// to fail are symmetric:
//
//   * descend into the INTERIOR and the frame goes solid black -- every
//     sample hits the iteration ceiling without escaping;
//   * descend into the far EXTERIOR and the frame goes to a flat wash --
//     everything escapes on the first iteration or two.
//
// Both are "the void". The set's boundary is exactly the frontier between
// them, and for an escape-time fractal it is the ONLY place with detail at
// every scale -- that is what self-similarity means here. So the target is
// not a heuristic about where the picture looks busy: it is the mathematical
// object the fractal is made of, and aiming at it is what makes a zoom
// endless rather than merely long.
//
// The classification needs no new machinery, because the renderer already
// draws it: a point that never escapes is written as pure black (see the
// `if (!done)` line in shader_source.cpp). Interior is therefore exactly
// "black", not an estimate of it. A cell that has BOTH black and non-black
// neighbours straddles the boundary.
//
// Among boundary cells, the tie-break is local contrast, because a smooth arc
// of boundary is technically on the set and still dull -- the filaments,
// spirals and mini-sets all read as high contrast. Boundary-ness comes first
// and contrast only sorts within it: a high-contrast cell that is NOT on the
// boundary is usually just palette banding in the exterior, and diving at one
// runs out of structure within a decade.
#pragma once

#include <cstdint>

namespace nv {

// Half-width of the transition-counting window.
static constexpr int kWin = 4;

struct FocusScore {
    bool valid = false;
    int cx = 0, cy = 0;   // best cell, in grid coordinates
    float score = 0.0f;
    float liveFraction = 0.0f;  // not-interior share of the frame
};

// rgba is dim*dim RGBA8. Returns the cell to steer towards.
inline FocusScore scoreFocus(const uint8_t* rgba, int dim) {
    FocusScore best;
    if (!rgba || dim < 3) return best;

    const auto lum = [&](int x, int y) -> int {
        const uint8_t* p = rgba + 4 * (y * dim + x);
        return p[0] + p[1] + p[2];
    };
    // The renderer writes interior as exact black. A small threshold rather
    // than == 0 because the frame has been through a linear-filtered
    // downsample by the time it gets here.
    const auto interior = [&](int x, int y) { return lum(x, y) <= 12; };

    int live = 0;
    for (int y = 0; y < dim; ++y)
        for (int x = 0; x < dim; ++x)
            if (!interior(x, y)) ++live;
    best.liveFraction = static_cast<float>(live) / static_cast<float>(dim * dim);

    // Only the CENTRAL region is a candidate. Steering is a correction to
    // an aim that is already roughly right, not a relocation: allowing the
    // target to be any cell lets a single measurement yank the view across
    // the whole frame, and repeated across a dive that is a random walk along
    // the boundary rather than a descent into one point.
    const int lo = dim / 4, hi = dim - dim / 4;
    for (int y = lo; y < hi; ++y) {
        for (int x = lo; x < hi; ++x) {
            int black = 0, lit = 0, contrast = 0;
            const int here = lum(x, y);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    if (interior(x + dx, y + dy)) ++black; else ++lit;
                    const int d = lum(x + dx, y + dy) - here;
                    contrast += d < 0 ? -d : d;
                }
            }
            // Straddles the frontier: both kinds of neighbour present.
            if (!black || !lit) continue;

            // How many times does the boundary CROSS this neighbourhood?
            //
            // This is the term that decides whether a dive stays alive, and
            // leaving it out is a measured regression, not a theoretical one:
            // scoring boundary-ness alone walked MANDELBROT off its target
            // and its edge energy at 3-5 decades fell to 0.0 -- a flat wash.
            //
            // The reason is that not every boundary point is worth zooming
            // into. A generic point on the frontier sits on a smooth arc, and
            // descending at one converges on a straight edge with nothing in
            // it. The points that stay interesting forever are the ones the
            // set is asymptotically SELF-SIMILAR about -- dendrites, spiral
            // centres, mini-set necks -- and what those have locally is many
            // interior/exterior alternations, not one. Counting transitions
            // across the window separates the two: a smooth arc gives one per
            // scanline, a dendritic neighbourhood gives several.
            int transitions = 0;
            for (int dy = -kWin; dy <= kWin; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= dim) continue;
                bool prev = interior(x - kWin < 0 ? 0 : x - kWin, yy);
                for (int dx = -kWin + 1; dx <= kWin; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= dim) continue;
                    const bool cur = interior(xx, yy);
                    if (cur != prev) ++transitions;
                    prev = cur;
                }
            }

            const float balance =
                1.0f - (black > lit ? black - lit : lit - black) / 8.0f;
            const float s = static_cast<float>(transitions) * 400.0f +
                            balance * 200.0f + static_cast<float>(contrast);
            if (s > best.score) {
                best.score = s;
                best.cx = x;
                best.cy = y;
                best.valid = true;
            }
        }
    }
    return best;
}

}  // namespace nv
