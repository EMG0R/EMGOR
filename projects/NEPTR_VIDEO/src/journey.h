// The endless zoom journey: the one number the picture ADVANCES through.
//
// GL-free and header-only on purpose, so every continuity claim below is
// checked by a test binary rather than by looking at frames and believing
// them (tests/test_journey.cpp).
//
// ---- why this exists -------------------------------------------------------
//
// MotionClock (motion.h) moves the map's PARAMETERS along a closed path, and a
// closed path cannot advance -- it returns to its start every 12*pi of phase,
// forever. That reads exactly as what it is: the fractal dancing in place.
//
// The fix is not to open that path. A bounded monotone function must converge,
// so a map parameter that only ever increases -- an exponent, a seed, a
// relaxation -- always ends up somewhere the fractal has stopped being
// interesting. The original note at core/kernels.cpp is right about that.
//
// The one quantity a fractal can advance through forever is SCALE. Self-
// similarity is the single place where the structure genuinely regenerates
// instead of repeating, so the journey is a zoom and nothing else.
//
// ---- why it is a pure function --------------------------------------------
//
// Everything below is a function of ONE accumulated number, travel_. There is
// no state machine, no "currently recycling" flag, no saved previous dive.
// That is the whole trick: reversibility is not implemented, it is structural.
// Running travel_ backwards through a recycle boundary reproduces the previous
// dive EXACTLY, because the dive was never stored -- it is recomputed from the
// dive index, which is just floor(travel / span). A state machine here would
// have to remember what it left behind in order to reverse into it, and would
// get it subtly wrong the first time two recycles happened close together.
//
// travel_ is integrated (travel += dtScene * rate), never wallClock * rate, for
// the same reason sceneTime is: changing the rate then changes only the
// derivative, so a JOURNEY move -- including through zero and into reverse --
// cannot make the picture jump.
#pragma once

#include <cmath>
#include <cstdint>

namespace nv {

// One view of the plane: where to look and how far in.
struct JourneyView {
    double cx = 0.0;      // centre, full double precision
    double cy = 0.0;
    double logZoom = 0.0; // decades of zoom; the linear factor is 10^logZoom
    // This dive's own rotation, in radians, added to the ROT knob.
    //
    // It is what makes the journey endless with a FINITE aim table. Each
    // kernel has at most six measured aims, so dives eventually reuse one --
    // and reusing an aim reproduces the frame exactly, which is a return, and
    // a return is the loop this design exists to eliminate. Measured: at rate
    // 1.0 a dive is exactly 300 frames, so a 2400-frame probe is exactly 8
    // dives, and dive 8 drew dive 0's aim and its opening frame again
    // (difference 1.256 against a threshold of 5).
    //
    // The angle is n * the GOLDEN ANGLE, an irrational fraction of a turn, so
    // the sequence of orientations never repeats however long it runs. The
    // aim itself is untouched -- the view turns about the measured point
    // rather than moving off it, so the precision that took a beam search to
    // find is not spent on making dives look different.
    double rot = 0.0;
};

// What a frame needs in order to draw the journey: normally one view, and
// during a recycle two views plus the weight that dissolves between them.
struct JourneyFrame {
    JourneyView a;
    JourneyView b;
    float xfade = 0.0f;   // 0 = a only. Never a cut; always a dissolve.
};

class Journey {
public:
    // How deep one dive goes before it recycles.
    //
    // Two different walls, and the one that actually binds is NOT the obvious
    // one. Precision was the expected limit: `p / uZoom + uCentre` needs the
    // centre resolved to about a pixel span, a single float near magnitude 1
    // holds ~6e-8 (hence the old four-decade ZOOM knob), and the hi/lo pair
    // added here carries ~48 bits, or ~1e-14.
    //
    // But ITERATIONS run out first, by a wide margin. The escape count needed
    // to resolve the boundary grows with depth, and the loop is capped at 512
    // (kIterCap). Measured, at DETAIL pinned to that cap, the fraction of the
    // frame that is not interior-black goes:
    //
    //     1 decade  23%    4 decades  99%    7 decades  0%
    //     2 decades  66%    5 decades  97%    8 decades  0%
    //     3 decades 100%    6 decades  21%
    //
    // So a dive is 5 decades: the whole of it is resolved, and it stops one
    // decade short of the cliff rather than descending into a black screen.
    // Raising this without raising kIterCap re-introduces exactly that.
    // 2*pi*(1 - 1/phi): the golden angle.
    static constexpr double kTwoPi = 6.283185307179586;
    static constexpr double kGoldenAngle = 2.399963229728653;
    // How much TRAVEL one dive takes -- which is how LONG it lasts, not how
    // far down it reaches. Those are separate: `depth` (the generator's own
    // journeyDepth) is the decades a dive covers, and scale = depth/kSpan
    // maps one onto the other.
    //
    // That separation is why this number moved. A dive takes kSpan/rate
    // seconds whatever its depth, so at the old kSpan of 5 and the default
    // JOURNEY of 0.25 decades/s EVERY dive recycled after twenty seconds --
    // and twenty seconds is short enough that the recycle, not the descent,
    // is what you notice. Raising the depth alone would only have made it
    // dive faster to the same clock.
    //
    // At 20 a dive runs eighty seconds, and on MANDELBROT it spends them
    // descending ten decades continuously. The dissolve is still there, and
    // still necessary -- precision ends somewhere -- but it arrives once the
    // descent has actually gone somewhere.
    static constexpr double kSpan = 20.0;
    // The dissolve at the end of a dive, in DECADES of travel rather than in
    // seconds. Decades is the unit that makes the fade reverse symmetrically
    // and stay the same length whatever JOURNEY rate or SPEED is set to -- and
    // it makes a freeze (speed 0) freeze the dissolve too, which is what
    // freezing should mean.
    static constexpr double kFade = 0.6;

    // Signed, unbounded, and the only mutable state in the class.
    void advance(double dtScene, double rate) { travel_ += dtScene * rate; }

    double travel() const { return travel_; }
    void setTravel(double t) { travel_ = t; }

    // Pin every dive to one entry in the target table. Only tooling sets
    // this: it is how tests/target_sweep.py asks "is there anything at
    // target 7", which is a question that cannot be answered by waiting for
    // the hash to land there.
    void forceTarget(int i) { forced_ = i; }

    // Pin every dive to an ARBITRARY coordinate, rather than to a table
    // entry. This is what lets tooling search a kernel's own plane for the
    // point worth descending into: the table holds MANDELBROT coordinates,
    // and 21 of the 22 escape kernels are not that map.
    void forceAim(double x, double y) { aimX_ = x; aimY_ = y; hasAim_ = true; }

    // This kernel's measured aims, in plane coordinates (x, y, x, y, ...).
    //
    // Set when the generator changes, not per frame: the aims decide where
    // every dive lands, and swapping them mid-dive would move the picture.
    // Empty restores the seed table.
    void setAims(const double* xy, int count) {
        aimCount_ = count < kMaxAims ? count : kMaxAims;
        for (int i = 0; i < aimCount_ * 2; ++i) aims_[i] = xy[i];
    }
    int aimCount() const { return aimCount_; }
    static int targetCount();

    // The dive index this travel lands in, and how far into it we are.
    // floor() rather than truncation so negative travel keeps going, rather
    // than reflecting around zero back into dive 0.
    long long diveIndex(double t) const {
        return static_cast<long long>(std::floor(t / kSpan));
    }

    // `depth` is how far THIS GENERATOR rewards a zoom, in decades per dive
    // (KernelDef::journeyDepth). It is not a second rate knob: travel still
    // accumulates in the same units at the same speed, so a dive always takes
    // the same wall-clock time and JOURNEY still means the same thing on every
    // fractal. What changes is how far down that dive reaches.
    //
    // depth == 0 is the important case and it is not "a very slow zoom": it
    // returns the identity view -- normal position, zoom 1, no recycle, no
    // dissolve, no centre hop. A generator with no self-similar structure to
    // descend into must HOLD STILL and let EVOLVE animate it, because a zoom
    // it cannot support is not a chill version of the journey, it is a slow
    // fade to a flat colour. Suppressing the recycle matters as much as
    // suppressing the zoom: at depth 0 both views would sit at zoom 1 and the
    // dissolve would be a pointless hop between two centres, which reads as
    // the exact frame-jump this engine exists not to do.
    JourneyFrame frame(double depth) const {
        JourneyFrame f;
        if (!(depth > 0.0)) return f;  // parked; also catches NaN

        const long long n = diveIndex(travel_);
        const double u = travel_ - static_cast<double>(n) * kSpan;  // [0, kSpan)
        const double scale = depth / kSpan;

        f.a = viewOf(n, u, scale);

        // The last kFade decades of a dive are shared with the next one. The
        // next dive is drawn at NEGATIVE depth (u - kSpan), i.e. zoomed out
        // beyond its own start, so what appears through the dissolve is a wide
        // view opening up rather than a second deep view cutting in.
        const double fadeStart = kSpan - kFade;
        if (u > fadeStart) {
            f.b = viewOf(n + 1, u - kSpan, scale);
            f.xfade = static_cast<float>((u - fadeStart) / kFade);
        }
        return f;
    }

    // The full-depth dive, i.e. what MANDELBROT gets. Kept as the no-argument
    // form because it is the reference behaviour every continuity proof in
    // tests/test_journey.cpp is written against.
    JourneyFrame frame() const { return frame(kSpan); }

    // Where dive n looks, at depth u.
    //
    // The centre is a function of n ALONE -- it does not drift during a dive.
    // A deep zoom has to converge on a point with detail all the way down, and
    // the table below is exactly such points; drifting off one mid-dive would
    // arrive at 1e12 looking at a solid region, which is the failure mode that
    // makes deep-zoom renderers look broken.
    JourneyView viewOf(long long n, double u, double scale = 1.0) const {
        JourneyView v;
        const Target t = targetOf(n);
        v.cx = t.x;
        v.cy = t.y;
        v.logZoom = u * scale;
        // Wrapped into one turn: the uniform is a float, and n grows without
        // bound.
        const double a = static_cast<double>(n) * kGoldenAngle;
        v.rot = a - kTwoPi * std::floor(a / kTwoPi);
        sampleSteer(n, u, v.cx, v.cy);
        return v;
    }

    // Record where the fractal's BOUNDARY actually is, measured from a
    // rendered frame (see focus.h). x, y are plane coordinates.
    //
    // This is what stops a dive ending in the void. The seed targets are
    // Mandelbrot coordinates and the bank holds 22 other escape kernels --
    // ten of them Julia-seeded, where the set's shape depends on a knob the
    // performer is holding, so no fixed coordinate can be right. Measuring
    // the boundary each time makes the aim the fractal's own.
    //
    // APPENDED to a path keyed by travel, never applied to a mutable centre.
    // That is what keeps the instrument exactly reversible: rewinding re-reads
    // the recorded path at the same travel and gets the same numbers back, so
    // the rewound frame is still byte-identical. A centre nudged each frame
    // could not do that -- the rewind would re-steer from wherever it happened
    // to be looking, and the reversibility proof would fail.
    void steerTo(double travel, double x, double y) {
        // Forward only, and never closer than kMinStep in TRAVEL to the last
        // sample. Both halves matter, and the second one is not an
        // optimisation:
        //
        // The path is interpolated BY TRAVEL, so the span between two samples
        // is how far the centre has to slide from one measurement to the
        // next. Sampling on a frame timer instead means that as SPEED
        // approaches zero the samples land at nearly identical travel, the
        // span collapses, and the slide becomes a SNAP. Measured as a step of
        // 14.65 against a median of 3.51 at the moment SPEED crosses zero --
        // a visible jump at exactly the gesture that is supposed to be
        // seamless. Requiring real travel between samples also means a frozen
        // picture stops steering, which is what freezing should mean.
        if (count_ > 0) {
            const double last = path_[(head_ + kPath - 1) % kPath].t;
            if (travel <= last + kMinStep) return;
        }
        path_[head_] = {travel, x, y};
        head_ = (head_ + 1) % kPath;
        if (count_ < kPath) ++count_;
    }

    bool steering() const { return count_ > 0; }

    // How many iterations this depth deserves. Detail that stays fixed while
    // the zoom descends is the other way a deep zoom goes flat: the escape
    // count needed to resolve the boundary grows with depth, so a frame at
    // 1e10 drawn with the wide view's iteration budget is a smooth blob.
    // Returned as a MULTIPLIER on the DETAIL knob so the knob keeps meaning
    // what it meant, and clamped because the governor still has to be able to
    // defend the framerate.
    static float detailBoost(double logZoom) {
        const double d = logZoom > 0.0 ? logZoom : 0.0;
        // Slope from the measurement, not from taste. At 512 iterations the
        // boundary is fully resolved to ~5 decades and gone by 7, so the
        // escape count the boundary needs runs at roughly a hundred
        // iterations per decade. Expressed as a multiplier on the DETAIL
        // knob's 96 default, that is ~1.2 per decade, which keeps the knob
        // meaning what it meant while letting a ten-decade dive ask for the
        // ~1250 iterations it actually needs.
        //
        // The clamp is still here for the same reason: the governor has to be
        // able to defend the framerate, and an unbounded ramp would let one
        // deep frame ask for more than the loop's ceiling.
        const double k = 1.0 + 1.2 * d;
        return static_cast<float>(k < 16.0 ? k : 16.0);
    }

private:
    struct Target { double x, y; };

    // The recorded steer path, as a ring. Reversing further back than this
    // falls back to the seed target -- a bounded infidelity, and far more
    // rewind than a performer reaches for.
    static constexpr int kPath = 4096;
    // Minimum travel between steer samples, in decades. Well under the
    // ~0.06 decades a normal sample interval covers at the default rate, so
    // it only bites when travel has all but stopped.
    static constexpr double kMinStep = 0.02;
    // Slew of the steer, in decades of travel. Comfortably longer than the
    // sample spacing so the centre glides between measurements rather than
    // stepping to each one.
    static constexpr double kTau = 0.08;
    // How much of the way to the measured point the centre is allowed to go.
    static constexpr double kAuthority = 0.35;
    struct Steer { double t, x, y; };

    // Interpolate the recorded path at this travel, if it covers it.
    //
    // Faded in by DEPTH rather than applied outright: at the top of a dive the
    // seed target and the measured boundary can be far apart, and snapping
    // between them is exactly the jump this design exists to avoid. By one
    // decade down the measurement has full authority, which is also where its
    // precision starts to matter.
    void sampleSteer(long long n, double u, double& cx, double& cy) const {
        if (count_ == 0) return;
        const double t = static_cast<double>(n) * kSpan + u;
        const auto at = [&](int i) -> const Steer& {
            return path_[(head_ - count_ + i + kPath) % kPath];
        };
        if (t <= at(0).t) return;   // before any measurement: seed target

        // Replay an exponential approach through the recorded samples, using
        // ONLY those at or behind t.
        //
        // Three requirements collide here and this is the one shape that
        // satisfies all of them:
        //
        //   CAUSAL   the view at travel t must not depend on a sample
        //            recorded later, or forward and rewind disagree -- which
        //            they did, and the byte-identical reversibility proof
        //            failed with mean|d| 26.26.
        //   SMOOTH   holding the newest sample instead is causal but
        //            piecewise-CONSTANT, so every new measurement arrives as
        //            a step.
        //   EXACT    a filter carried in a mutable member would drift with
        //            frame rate and could not be rewound at all. Replaying it
        //            from the recorded samples makes the value a pure
        //            function of (t, path), so rewinding recomputes the
        //            identical number and the frame stays byte-identical.
        //
        // Cost is one pass over at most kPath samples, twice a frame. That is
        // a few thousand flops against a full-screen fractal.
        // Start from the SEED target, not from the first measurement.
        //
        // Seeding the filter with at(0) means the centre leaps from the seed
        // coordinate to the first measured boundary the instant that sample
        // lands -- and if it lands when the dive is already deep, the depth
        // fade below is near 1 and does nothing to soften it. That was a
        // single hard jump per dive: step 20.74 against a median of 4.35.
        // Starting at the seed makes the first measurement something the
        // centre GLIDES towards, like every one after it.
        double vx = cx, vy = cy, cur = at(0).t;
        for (int i = 0; i + 1 < count_ && cur < t; ++i) {
            const double segEnd = at(i + 1).t < t ? at(i + 1).t : t;
            const double dT = segEnd - cur;
            if (dT <= 0.0) continue;
            const double k = 1.0 - std::exp(-dT / kTau);
            vx += (at(i).x - vx) * k;
            vy += (at(i).y - vy) * k;
            cur = segEnd;
        }
        if (cur < t) {   // past the last sample: it stays the target
            const double k = 1.0 - std::exp(-(t - cur) / kTau);
            vx += (at(count_ - 1).x - vx) * k;
            vy += (at(count_ - 1).y - vy) * k;
        }

        // Faded in by DEPTH. At the top of a dive the seed target and the
        // measured boundary can be far apart, and snapping between them is
        // the jump this design exists to avoid. By one decade down the
        // measurement has full authority, which is also where its precision
        // starts to matter.
        // Capped well below 1. Steering CORRECTS the seed aim, it does not
        // replace it: at full authority the measured point is the centre, and
        // a dive then follows wherever the last measurement pointed rather
        // than descending into one place. Measured, that cost MANDELBROT its
        // whole dive -- edge energy 22.7 -> 0.0 by three decades.
        const double ramp = u < 0.0 ? 0.0 : (u < 1.0 ? u : 1.0);
        const double auth = ramp * kAuthority;
        cx += (vx - cx) * auth;
        cy += (vy - cy) * auth;
    }

    Steer path_[kPath] = {};
    int head_ = 0;
    int count_ = 0;

    // Points where a five-decade dive was MEASURED to stay alive all the way
    // down -- tests/target_sweep.py, on the Pi, at MANDELBROT.
    //
    // "Chosen because zooming at them stays interesting" is what the first
    // version of this table said, and for six of its sixteen entries it was
    // simply not true. That is not a small error: a bad coordinate does not
    // degrade gracefully, it ends the dive on a solid screen. A 48 s run at
    // the default rate passes through three dives, so a table that is
    // three-eighths bad puts a dead dive on the panel about every minute --
    // which is exactly what the first hardware capture of this feature
    // recorded, one sampled frame at 0.00% non-black.
    //
    // Why a plausible-looking coordinate fails: being on the boundary at 1e0
    // says nothing about 1e4. A point a whisker inside the set is
    // indistinguishable from one a whisker outside it at the surface, and the
    // dive resolves the difference -- into solid interior, or into empty
    // exterior. The only way to know is to fly it and look, which is what the
    // sweep does.
    //
    // Dropped, with the depth at which each stopped having anything in it:
    //   (-1.7492046334590113, 0)     dies at 1.0   live 0.97 -> edge 0.5
    //   ( 0.2500100000, 0)           dies at 2.0   -> 0.00/0.0, the black one
    //   (-1.2568855, 0.3803017)      dies at 5.0   -> 0.00/0.0
    //   (-1.94158137168130, 0)       flat from 0   1.00/0.3, a wash throughout
    //   ( 0.3750001200, 0.2166393860) dies at 3.5  -> 0.00/0.0
    //   (-1.7683030, 0.0016887)      dies at 1.0
    //
    // (0.25001, 0) is worth naming: it sits a ten-thousandth outside the
    // cusp of the main cardioid, which is the most famous "on the boundary"
    // coordinate there is. It still dives straight into solid interior. That
    // is how little the surface tells you.
    // Also dropped as REDUNDANT rather than dead, because an instrument that
    // shows you the same view twice is repeating even if both are valid:
    //   (-0.7436447860, 0.1318252536) is the first entry to sub-pixel
    //     precision at 1e5 -- the two scored identically at all eight depths.
    //   (-0.1010963638, -0.9562865108) is the exact complex conjugate of
    //     entry 5, and the Mandelbrot set is symmetric about the real axis,
    //     so it is the same picture mirrored. Also identical at all depths.
    //
    // ADDING a point here means running target_sweep.py on it first. An
    // unverified entry is a black screen with a delay fuse on it.
    static const Target* targets(int& count) {
        static const Target kTargets[] = {
            // measured live/edge at 0,1,2,3,3.5,4,4.5,5 decades
            // .48/4.3 .19/4.2 .17/8.0 .42/22.7 .62/23.3 .61/16 .52/11.2 .65/14
            {-0.743643887037158704752191506114774, 0.131825904205311970493132056385139},
            // .66/2.1 .67/4.7 .72/6.0 .72/2.9 .61/3.1 .53/2.7 .46/2.1 .34/2.7
            { 0.2929859127507, 0.6117848324958},
            // .70/2.4 .85/2.3 .84/1.6 .35/3.7 .21/3.9 .25/4.4 .48/6.4 .73/7.9
            {-0.16070135, 1.0375665},
            // .48/4.3 .16/4.1 .20/7.3 .33/16 .53/23.8 .75/18.4 .72/11.6 .87/16.2
            {-0.7453, 0.1127},
            // .65/2.5 .69/4.0 .68/2.2 .65/1.4 .59/1.3 .54/1.2 .49/1.1 .42/1.1
            {-0.10109636384562, 0.95628651080914},
            // .65/2.7 .69/5.2 .82/4.1 .54/5.2 .38/6.0 .20/4.6 .19/3.0 .11/1.7
            { 0.4323029960, 0.2263880183},
            // .50/4.2 .23/4.6 .19/6.2 .27/6.5 .42/4.9 .63/7.3 .57/5.8 .72/4.4
            {-0.77568377, 0.13646737},
            // .55/2.5 .71/5.5 .57/6.3 .38/5.0 .45/4.8 .52/4.3 .55/3.8 .47/2.9
            {-0.5626805, 0.6428020},
        };
        count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
        return kTargets;
    }

    Target targetOf(long long n) const {
        int count = 0;
        const Target* t = targets(count);
        if (hasAim_) return Target{aimX_, aimY_};
        // This kernel's own measured aims come first: the table below is
        // MANDELBROT coordinates and means nothing on another map.
        if (aimCount_ > 0) {
            uint64_t h = static_cast<uint64_t>(n) * 0x9E3779B97F4A7C15ull;
            h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
            const int i = static_cast<int>(h % static_cast<uint64_t>(aimCount_));
            return Target{aims_[2 * i], aims_[2 * i + 1]};
        }
        if (forced_ >= 0) return t[forced_ % count];
        // A hash rather than n % count: consecutive dives should not walk the
        // table in order, or the instrument visibly repeats a tour after
        // sixteen dives. Deterministic in n, which is what keeps reversal
        // exact.
        uint64_t h = static_cast<uint64_t>(n) * 0x9E3779B97F4A7C15ull;
        h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
        return t[h % static_cast<uint64_t>(count)];
    }

    double travel_ = 0.0;
    int forced_ = -1;
    static constexpr int kMaxAims = 6;
    double aims_[kMaxAims * 2] = {};
    int aimCount_ = 0;
    double aimX_ = 0.0, aimY_ = 0.0;
    bool hasAim_ = false;
};

inline int Journey::targetCount() {
    int n = 0;
    targets(n);
    return n;
}

}  // namespace nv
