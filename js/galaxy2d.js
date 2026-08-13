/* galaxy2d.js — EMGOR fractal galaxy engine.
   Dependency-free Canvas-2D. Renders galaxy.json (falls back to js/galaxy-stub.json).
   One continuous 2D camera; every node is a procedurally-seeded planet; every
   planet label is a real DOM <a>. See docs/universe-schema.md (rendering contract). */
(function () {
    'use strict';

    // ─── constants ─────────────────────────────────────────────
    var VOID = '#0D0221';
    var ROOT_SYS_R = 1200;          // world radius of the root system
    var SHRINK = 0.2;               // child system radius = parent * SHRINK
    var BODY_F = 0.3;               // body radius = own system radius * BODY_F
    var ORBIT_MIN = 0.46, ORBIT_MAX = 1.0; // orbit radii as fraction of parent sysR
    var MARGIN_X = 44, MARGIN_Y = 76;      // screen margin so labels never clip
    var FLY_DUR = 1.15;             // seconds, fractal zoom flight
    var CHILD_BOOST = 1.9;          // visual size boost for the focused nav ring
    var TAU = Math.PI * 2;

    // per-node size multiplier fallback for galaxy.json snapshots without `size`
    var SIZE_FALLBACK = {
        'emgor.papers': 2.0,
        'emgor.web-synth': 1.4
    };

    var BASE_ROT = 0.022;           // rad/s — slow chill shared orbital drift

    // vivid-but-dark jewel palette families (hue range, sat range)
    var FAMILIES = [
        [352, 372, 60, 80],    // deep crimson
        [24, 42, 55, 75],      // burnt amber
        [214, 236, 52, 72],    // cobalt
        [164, 186, 50, 68],    // emerald teal
        [262, 286, 50, 68],    // royal violet
        [196, 214, 34, 50]     // sapphire slate
    ];

    var reducedMotion = window.matchMedia &&
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    // ─── seeded randomness ─────────────────────────────────────
    function hash32(str) {
        var h = 2166136261;
        for (var i = 0; i < str.length; i++) {
            h ^= str.charCodeAt(i);
            h = Math.imul(h, 16777619);
        }
        return h >>> 0;
    }
    function mulberry32(a) {
        return function () {
            a |= 0; a = (a + 0x6D2B79F5) | 0;
            var t = Math.imul(a ^ (a >>> 15), 1 | a);
            t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        };
    }

    function easeInOut(t) { return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2; }
    function easeOut(t) { return 1 - Math.pow(1 - t, 3); }
    function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
    function lerp(a, b, t) { return a + (b - a) * t; }
    // keep free-spinning angles bounded to (-PI, PI] so they never lose
    // float precision over a very long idle session
    function wrapAngle(a) {
        a = a % TAU;
        if (a > Math.PI) a -= TAU;
        else if (a < -Math.PI) a += TAU;
        return a;
    }

    // ─── state ─────────────────────────────────────────────────
    var canvas, ctx, labelsEl, crumbEl, homeBtn;
    var W = 0, H = 0, DPR = 1;
    var availX = 0, availY = 0, stretchX = 1, stretchY = 1;
    var time = 0, lastTs = 0, frame = 0;

    var root = null;                // virtual node id 'emgor'
    var byId = {};
    var byRoute = {};
    var drawOrder = [];             // nodes sorted by depth asc

    var focus = null;               // node whose system fills the screen
    var trans = null;               // {from, to, t, sFrom, sTo}
    var cam = { x: 0, y: 0, scale: 1 };

    // 3D scene rotation (trackball): orbits live in the x,z plane (y up).
    // yaw spins the model around the vertical axis, pitch tilts the plane.
    // Both angles are full 360°: this is a genuine rotation-matrix composition
    // (yaw about Y, then pitch about the yawed X axis), not a hacky 2D tilt, so
    // depth (sz/z2) stays mathematically correct through every orientation,
    // including tumbling fully over the top or underneath.
    var PITCH_REST = 0.62;          // rest pose ~35°: reads as 3D immediately
    var PERSP = 0.35;               // perspective strength (normalized by view radius)
    var yaw = 0, pitch = PITCH_REST;
    var yawV = 0, pitchV = 0;       // trackball momentum (rad/s)
    var cosYaw = 1, sinYaw = 0, cosPit = 1, sinPit = 0, perspK = 0;
    var sortOrder = [];             // drawOrder clone, insertion-sorted by depth

    var intro = null;               // big-bang state
    var overlayNode = null;
    var suppressHash = false;

    // stars — anchored background, fully decoupled from scene rotation/zoom;
    // they shimmer in place instead of moving
    var starN = 0;
    var starX, starY, starR, starPh, starW, starTw, starLayer, starTint;

    // sprites
    var nebulaSprites = [];
    var accretionSprite = null;    // base 512px tier
    var accretionHi = null;        // lazily-baked 1024px tier, once, kept forever
    var vignette = null;

    // big-bang particles
    var PN = 420;
    var px = new Float32Array(PN), py = new Float32Array(PN);
    var pvx = new Float32Array(PN), pvy = new Float32Array(PN);
    var plife = new Float32Array(PN), phue = new Float32Array(PN);

    // label pool
    var labelPool = {};             // id -> <a>

    // ─── data loading ──────────────────────────────────────────
    function loadData() {
        return fetch('galaxy.json', { cache: 'no-cache' })
            .then(function (r) { if (!r.ok) throw new Error('no galaxy.json'); return r.json(); })
            .catch(function () {
                return fetch('js/galaxy-stub.json').then(function (r) { return r.json(); });
            });
    }

    function buildTree(data) {
        root = {
            id: 'emgor', title: 'EMGOR', route: '/', depth: 0, kids: [],
            parentNode: null, sysR: ROOT_SYS_R, wx: 0, wy: 0, sizeF: 1
        };
        byId = { emgor: root };
        byRoute = { '/': root };

        var nodes = (data.nodes || []).filter(function (n) {
            // the galactic center may appear as a data node; the engine owns it
            if (n.id === 'emgor' || !n.parent) {
                if (n.path) root.path = n.path;
                if (n.blurb) root.blurb = n.blurb;
                return false;
            }
            return true;
        });
        nodes.forEach(function (n) {
            var node = {
                id: n.id, title: n.title || n.id, blurb: n.blurb || '',
                parentId: n.parent || 'emgor', path: n.path || '',
                route: n.route || ('/' + n.id.split('.').slice(1).join('/')),
                depth: n.depth || 1, kids: [],
                downloads: n.downloads || [], links: n.links || [],
                tags: n.tags || [], updated: n.updated || '',
                launch: typeof n.launch === 'string' ? n.launch : '',
                wx: 0, wy: 0, sx: 0, sy: 0, sr: 0, alpha: 0, rev: 1
            };
            var sz = typeof n.size === 'number' ? n.size : (SIZE_FALLBACK[node.id] || 1);
            node.sizeF = clamp(sz, 0.5, 2.5);
            byId[node.id] = node;
            byRoute[node.route] = node;
        });
        nodes.forEach(function (n) {
            var node = byId[n.id];
            var p = byId[node.parentId] || root;
            node.parentNode = p;
            p.kids.push(node);
        });

        // layout + identity, depth-first
        drawOrder = [];
        (function walk(node) {
            if (node !== root) drawOrder.push(node);
            node.sysR = node === root ? ROOT_SYS_R : node.parentNode.sysR * SHRINK;
            node.bodyR = node.sysR * BODY_F * node.sizeF;
            var kids = node.kids;
            var N = kids.length;
            var minGap = N ? TAU / N : 0;
            var boostEff = CHILD_BOOST / (1 + Math.max(0, N - 5) * 0.045);
            var sysRot = mulberry32(hash32(node.id + '::rot'))() * TAU;
            var kBody = node.sysR * SHRINK * BODY_F;
            var i, k;

            // max-separation home slots: even angular spacing (+per-system twist),
            // one ring each — min pairwise separation is TAU/N by construction
            var maxHalf = 0;
            for (i = 0; i < N; i++) {
                k = kids[i];
                var t = N === 1 ? 0.6 : i / (N - 1);
                k.orbF = ORBIT_MIN + (ORBIT_MAX - ORBIT_MIN) * (t * 0.75 + t * t * 0.25);
                // angular half-extent of the body at a mean ring radius —
                // bounds how far the vibrato may swing without any contact
                k._half = (kBody * k.sizeF * boostEff) / (0.73 * node.sysR);
                if (k._half > maxHalf) maxHalf = k._half;
            }
            for (i = 0; i < N; i++) {
                k = kids[i];
                var rng = mulberry32(hash32(k.id));
                k.homeA = sysRot + (i / N) * TAU;
                // vibrato: slow sinusoidal wander around the home slot; depth is
                // capped so two neighbours plus their bodies can never touch
                var depthCap = Math.max(0.015, 0.5 * (minGap - (maxHalf + k._half) * 1.15));
                k.vibDepth = Math.min(minGap * (0.08 + rng() * 0.1), depthCap);
                k.vibF = 0.35 + rng() * 0.45;       // period ~8–18 s
                k.vibPh = rng() * TAU;
                seedIdentity(k, rng);
                walk(k);
            }
        })(root);
        sortOrder = drawOrder.slice();
    }

    // ─── procedural planet identity ────────────────────────────
    // Sprites are baked at a base 192px tier here; the LOD system (below)
    // lazily re-bakes the same deterministic art at higher tiers when a
    // body's on-screen radius outgrows what's cached, so a fresh visit
    // always starts cheap.
    function seedIdentity(node, rng) {
        var fam = FAMILIES[Math.floor(rng() * FAMILIES.length)];
        node.hue = Math.floor(fam[0] + rng() * (fam[1] - fam[0]));
        node.sat = fam[2] + rng() * (fam[3] - fam[2]);
        node.hasRing = rng() < 0.42;
        node.ringAngle = (rng() - 0.5) * 0.9;
        node.ringTilt = 0.22 + rng() * 0.2;
        node.pulseRate = 0.4 + rng() * 1.1;
        node.pulsePhase = rng() * TAU;
        node.noiseKind = rng() < 0.5 ? 'bands' : 'speckle';
        node.spriteSeed = hash32(node.id + '::surface');
        node.spriteRes = 192;
        node._tiers = {};
        node.sprite = node._tiers[192] = makePlanetSprite(node, 192);
        node.glowRes = 256;
        node._glowTiers = {};
        node.glow = node._glowTiers[256] = makeGlowSprite(node.hue, node.sat, 256);
        node._lodTouch = 0;
    }

    // consistent global light direction for every body (fraction of R,
    // upper-left) — one light source, used for the base gradient, the
    // terminator, the specular sheen and the rim glow alike
    var LIGHT_X = -0.42, LIGHT_Y = -0.42;

    function makePlanetSprite(node, S) {
        S = S || 192;                             // sprite canvas size (LOD tier)
        var c = document.createElement('canvas');
        c.width = c.height = S;
        var g = c.getContext('2d');
        var cx = S / 2, cy = S / 2;
        var R = S * 0.30;                        // body radius, room left for ring
        var rng = mulberry32(node.spriteSeed);   // same seed every tier -> identical art
        var hue = node.hue, sat = node.sat;
        var lx = LIGHT_X, ly = LIGHT_Y;

        // ring behind
        if (node.hasRing) drawRing(g, cx, cy, R, node, 0.55, true);

        g.save();
        g.beginPath();
        g.arc(cx, cy, R, 0, TAU);
        g.clip();

        // base sphere gradient, lit toward the global light — deep
        // desaturated albedo, real-rock dark, with an extra mid stop for a
        // softer Lambertian falloff instead of a hard band transition
        var satA = Math.max(14, sat - 18);       // muted surface saturation, hue kept
        var lg = g.createRadialGradient(cx + lx * R, cy + ly * R, R * 0.06, cx, cy, R * 1.15);
        lg.addColorStop(0, 'hsl(' + hue + ',' + satA + '%,43%)');
        lg.addColorStop(0.3, 'hsl(' + hue + ',' + satA + '%,32%)');
        lg.addColorStop(0.6, 'hsl(' + hue + ',' + satA + '%,21%)');
        lg.addColorStop(0.85, 'hsl(' + ((hue + 14) % 360) + ',' + satA + '%,12%)');
        lg.addColorStop(1, 'hsl(' + ((hue + 20) % 360) + ',' + Math.min(70, satA + 8) + '%,7%)');
        g.fillStyle = lg;
        g.fillRect(0, 0, S, S);

        // multi-octave surface detail: more octaves unlock at higher LOD
        // tiers so texture keeps reading as detail (not a smear) when the
        // camera flies in close
        var octaves = S >= 1200 ? 4 : (S >= 700 ? 3 : (S >= 350 ? 2 : 1));
        if (node.noiseKind === 'bands') {
            drawBandOctaves(g, cx, cy, R, rng, hue, sat, octaves);
        } else {
            drawSpeckleOctaves(g, cx, cy, R, rng, hue, sat, octaves);
        }

        // terminator — soft multi-stop falloff toward the night side; capped
        // short of full black so the dark hemisphere never goes flat
        var sh = g.createRadialGradient(cx + lx * R, cy + ly * R, R * 0.22, cx, cy, R * 1.3);
        sh.addColorStop(0, 'rgba(0,0,0,0)');
        sh.addColorStop(0.42, 'rgba(3,1,10,0.10)');
        sh.addColorStop(0.65, 'rgba(3,1,10,0.32)');
        sh.addColorStop(0.85, 'rgba(3,1,10,0.6)');
        sh.addColorStop(1, 'rgba(2,0,10,0.84)');
        g.fillStyle = sh;
        g.fillRect(0, 0, S, S);

        // faint ambient bounce opposite the light — a low-sat hue tint so
        // the night side reads as lit-by-starlight rather than a void
        var ax = cx - lx * R * 0.85, ay = cy - ly * R * 0.85;
        var amb = g.createRadialGradient(ax, ay, 0, ax, ay, R * 0.95);
        var ambHue = (hue + 200) % 360;
        amb.addColorStop(0, 'hsla(' + ambHue + ',' + Math.max(10, satA - 10) + '%,17%,0.16)');
        amb.addColorStop(1, 'hsla(' + ambHue + ',' + Math.max(10, satA - 10) + '%,17%,0)');
        g.fillStyle = amb;
        g.fillRect(0, 0, S, S);

        // limb darkening — the whole disc edge falls off like a real photosphere
        var ld = g.createRadialGradient(cx, cy, R * 0.78, cx, cy, R);
        ld.addColorStop(0, 'rgba(0,0,4,0)');
        ld.addColorStop(1, 'rgba(0,0,4,0.4)');
        g.fillStyle = ld;
        g.fillRect(0, 0, S, S);

        // subtle specular sheen, offset toward the light and biased near the
        // limb like a real curved surface — deliberately faint (additive,
        // low alpha): a hint of gloss, not a glossy 3D render
        g.globalCompositeOperation = 'lighter';
        var specX = cx + lx * R * 1.05, specY = cy + ly * R * 1.05;
        var sp = g.createRadialGradient(specX, specY, 0, specX, specY, R * 0.32);
        sp.addColorStop(0, 'hsla(' + hue + ',18%,92%,0.15)');
        sp.addColorStop(0.5, 'hsla(' + hue + ',28%,80%,0.05)');
        sp.addColorStop(1, 'hsla(' + hue + ',28%,80%,0)');
        g.fillStyle = sp;
        g.fillRect(0, 0, S, S);
        g.globalCompositeOperation = 'source-over';

        // ring shadow cast on the planet, where the front ring band crosses
        // the sphere — read together with the front ring it sells the 3D
        if (node.hasRing) drawRingShadow(g, cx, cy, R, node);

        g.restore();

        // limb light: bright hue-tinted crescent on the lit side — a thin
        // crisp core plus (at tiers that can afford it) a soft blurred
        // outer halo, reading as thin scattered atmosphere
        g.save();
        g.lineCap = 'round';
        g.strokeStyle = 'hsla(' + hue + ',34%,76%,0.32)';
        g.lineWidth = Math.max(1, R * 0.014);
        g.beginPath();
        g.arc(cx, cy, R + g.lineWidth * 0.5, Math.PI * 0.86, Math.PI * 1.68);
        g.stroke();
        if (S >= 350) {
            g.filter = 'blur(' + Math.max(1, R * 0.035) + 'px)';
            g.strokeStyle = 'hsla(' + hue + ',40%,80%,0.20)';
            g.lineWidth = Math.max(1.5, R * 0.05);
            g.beginPath();
            g.arc(cx, cy, R + g.lineWidth * 0.6, Math.PI * 0.82, Math.PI * 1.72);
            g.stroke();
            g.filter = 'none';
        }
        g.restore();

        // ring front half — the planet occludes the far half purely by draw
        // order (behind half was painted before the sphere clip above)
        if (node.hasRing) drawRing(g, cx, cy, R, node, 0.9, false);

        node.spriteR = R / S;       // body radius as fraction of sprite size (tier-invariant)
        return c;
    }

    // layered band noise for gas giants: each octave adds smaller, fainter
    // belts; every belt is domain-warped (two summed sine waves) instead of
    // a plain ellipse, so it drifts organically along its length
    function drawBandOctaves(g, cx, cy, R, rng, hue, sat, octaves) {
        var rot = (rng() - 0.5) * 0.8;
        g.save();
        g.translate(cx, cy); g.rotate(rot); g.translate(-cx, -cy);
        var steps = 22;
        for (var o = 0; o < octaves; o++) {
            var bands = (3 + Math.floor(rng() * 4)) + o * 2;
            for (var b = 0; b < bands; b++) {
                var by = cy - R + rng() * R * 2;
                var bh = R * (0.07 + rng() * 0.18) / (1 + o * 0.55);
                var freqA = 1.5 + rng() * 2.5, freqB = 3 + rng() * 4;
                var ampA = R * (0.05 + rng() * 0.05) / (o + 1);
                var ampB = R * (0.02 + rng() * 0.03) / (o + 1);
                var phA = rng() * TAU, phB = rng() * TAU;
                var hueJ = (hue + (rng() - 0.5) * 26 + 360) % 360;
                var lightness = rng() < 0.55 ? 11 : 30;
                var alpha = (0.1 + rng() * 0.14) / (1 + o * 0.4);
                g.fillStyle = 'hsla(' + hueJ + ',' + Math.max(8, sat - 24) + '%,' + lightness + '%,' + alpha + ')';
                g.beginPath();
                var s, t, x, warp;
                for (s = 0; s <= steps; s++) {
                    t = s / steps; x = cx - R * 1.3 + t * R * 2.6;
                    warp = Math.sin(t * TAU * freqA + phA) * ampA + Math.sin(t * TAU * freqB + phB) * ampB;
                    if (s === 0) g.moveTo(x, by - bh + warp); else g.lineTo(x, by - bh + warp);
                }
                for (s = steps; s >= 0; s--) {
                    t = s / steps; x = cx - R * 1.3 + t * R * 2.6;
                    warp = Math.sin(t * TAU * freqA + phA) * ampA + Math.sin(t * TAU * freqB + phB) * ampB;
                    g.lineTo(x, by + bh + warp);
                }
                g.closePath();
                g.fill();
            }
        }
        g.restore();
    }

    // layered blotch noise for rocky bodies: octave 0 is a handful of large
    // craters with a raised-relief highlight rim, later octaves add medium
    // and fine speckle so detail holds up at high LOD tiers
    function drawSpeckleOctaves(g, cx, cy, R, rng, hue, sat, octaves) {
        for (var o = 0; o < octaves; o++) {
            var scale = Math.pow(0.42, o);
            var n = Math.floor((22 + rng() * 34) * (o === 0 ? 1 : 1.6 + o * 0.8));
            for (var s = 0; s < n; s++) {
                var a = rng() * TAU, d = Math.sqrt(rng()) * R * 0.94;
                var sx2 = cx + Math.cos(a) * d, sy2 = cy + Math.sin(a) * d;
                var sr2 = R * (0.03 + rng() * 0.1) * scale;
                var hueJ = (hue + (rng() - 0.5) * 32 + 360) % 360;
                var dark = rng() < 0.62;
                g.fillStyle = 'hsla(' + hueJ + ',' + Math.max(8, sat - 22) + '%,' + (dark ? 10 : 32) + '%,' +
                    (0.1 + rng() * 0.16) + ')';
                g.beginPath();
                g.ellipse(sx2, sy2, sr2 * (0.7 + rng()), sr2, rng() * TAU, 0, TAU);
                g.fill();
                if (o === 0 && dark && sr2 > R * 0.045) {
                    g.strokeStyle = 'hsla(' + hueJ + ',' + Math.max(6, sat - 30) + '%,46%,0.14)';
                    g.lineWidth = Math.max(0.6, sr2 * 0.22);
                    g.beginPath();
                    g.arc(sx2 - sr2 * 0.28, sy2 - sr2 * 0.28, sr2 * 0.82, Math.PI * 0.9, Math.PI * 1.7);
                    g.stroke();
                }
            }
        }
    }

    function drawRingShadow(g, cx, cy, R, node) {
        g.save();
        g.translate(cx, cy);
        g.rotate(node.ringAngle);
        g.fillStyle = 'rgba(2,0,8,0.28)';
        g.beginPath();
        g.ellipse(0, R * node.ringTilt * 0.05, R * 1.04, R * 0.15 * node.ringTilt + R * 0.03, 0, 0, TAU);
        g.fill();
        g.restore();
    }

    // gradient-banded ring: several concentric strokes of varying width /
    // saturation instead of two flat bands, for a more believable particle
    // disc; the planet occludes the far half purely by draw order (the
    // caller paints "behind" before the sphere clip, "front" after)
    function drawRing(g, cx, cy, R, node, alpha, behind) {
        g.save();
        g.translate(cx, cy);
        g.rotate(node.ringAngle);
        var hue2 = (node.hue + 24) % 360;
        var bands = [
            { rr: 1.40, w: 0.16, sat: 14, light: 38, a: 0.22 },
            { rr: 1.56, w: 0.06, sat: 20, light: 56, a: 0.30 },
            { rr: 1.68, w: 0.10, sat: 15, light: 38, a: 0.16 },
            { rr: 1.80, w: 0.045, sat: 22, light: 60, a: 0.14 }
        ];
        for (var i = 0; i < bands.length; i++) {
            var bd = bands[i];
            g.strokeStyle = 'hsla(' + hue2 + ',' + bd.sat + '%,' + bd.light + '%,' + (alpha * bd.a) + ')';
            g.lineWidth = R * bd.w;
            g.beginPath();
            g.ellipse(0, 0, R * bd.rr, R * bd.rr * node.ringTilt, 0,
                behind ? Math.PI : 0, behind ? TAU : Math.PI);
            g.stroke();
        }
        g.restore();
    }

    function makeGlowSprite(hue, sat, S) {
        S = S || 256;
        var c = document.createElement('canvas');
        c.width = c.height = S;
        var g = c.getContext('2d');
        var gr = g.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
        gr.addColorStop(0, 'hsla(' + hue + ',' + Math.max(20, sat - 12) + '%,54%,0.11)');
        gr.addColorStop(0.35, 'hsla(' + hue + ',' + Math.max(16, sat - 18) + '%,44%,0.04)');
        gr.addColorStop(1, 'hsla(' + hue + ',' + sat + '%,40%,0)');
        g.fillStyle = gr;
        g.fillRect(0, 0, S, S);
        return c;
    }

    // ─── LOD (level-of-detail) sprite re-baking ─────────────────
    var LOD_TIERS = [192, 384, 768, 1536];
    var LOD_GLOW_TIERS = [256, 512, 1024];
    var lodQueue = [];
    var lodActive = {};
    var hiResNodes = {};

    function pickTier(tiers, px) {
        for (var i = 0; i < tiers.length; i++) if (tiers[i] >= px) return tiers[i];
        return tiers[tiers.length - 1];
    }

    // Scan only the handful of bodies that are actually large on screen
    // right now — the focused nav ring, the sun body, and (mid-flight) the
    // system being left — and queue a higher-res bake if the sprite would
    // visibly upscale. At most one bake happens per frame, and never while
    // a flight transition is still mid-flight, so FLY_DUR zooms never
    // stutter. Anything that stops being large gets its extra tiers evicted
    // after a short grace period so cached canvases can't grow unbounded.
    function lodUpdate() {
        var cands = [], i;
        for (i = 0; i < focus.kids.length; i++) cands.push(focus.kids[i]);
        if (focus !== root) cands.push(focus);
        if (trans) {
            for (i = 0; i < trans.from.kids.length; i++) cands.push(trans.from.kids[i]);
            if (trans.from !== root) cands.push(trans.from);
        }
        for (i = 0; i < cands.length; i++) {
            var n = cands[i];
            n._lodTouch = frame;
            var needPx = n.sr * DPR * 2.3;
            if (pickTier(LOD_TIERS, needPx) > n.spriteRes && !lodActive[n.id]) {
                lodActive[n.id] = true;
                lodQueue.push(n);
            }
        }

        var okToBake = !intro && !(trans && trans.t < 0.85);
        if (okToBake && lodQueue.length) {
            var job = lodQueue.shift();
            lodActive[job.id] = false;
            var needPx2 = job.sr * DPR * 2.3;
            var tier = pickTier(LOD_TIERS, needPx2);
            if (tier > job.spriteRes) bakeSpriteTier(job, tier);
            var gTier = pickTier(LOD_GLOW_TIERS, needPx2 * 1.4);
            if (gTier > job.glowRes) bakeGlowTier(job, gTier);
        }

        for (var id in hiResNodes) {
            var hn = hiResNodes[id];
            if (frame - hn._lodTouch > 90) {
                evictTiers(hn);
                delete hiResNodes[id];
            }
        }
    }

    function bakeSpriteTier(node, size) {
        node.sprite = node._tiers[size] || (node._tiers[size] = makePlanetSprite(node, size));
        node.spriteRes = size;
        hiResNodes[node.id] = node;
    }

    function bakeGlowTier(node, size) {
        node.glow = node._glowTiers[size] || (node._glowTiers[size] = makeGlowSprite(node.hue, node.sat, size));
        node.glowRes = size;
        hiResNodes[node.id] = node;
    }

    // drop everything above the base tier — memory reclaimed, next visit
    // just re-bakes lazily again
    function evictTiers(node) {
        var sz;
        for (sz in node._tiers) if (Number(sz) !== 192) delete node._tiers[sz];
        for (sz in node._glowTiers) if (Number(sz) !== 256) delete node._glowTiers[sz];
        node.sprite = node._tiers[192];
        node.spriteRes = 192;
        node.glow = node._glowTiers[256];
        node.glowRes = 256;
    }

    // ─── ambient sprites ───────────────────────────────────────
    function makeNebulaSprites() {
        nebulaSprites = [];
        var hues = [268, 288, 248, 195];
        for (var i = 0; i < hues.length; i++) {
            var S = 512;
            var c = document.createElement('canvas');
            c.width = c.height = S;
            var g = c.getContext('2d');
            var gr = g.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
            gr.addColorStop(0, 'hsla(' + hues[i] + ',80%,42%,' + (i === 3 ? 0.05 : 0.1) + ')');
            gr.addColorStop(0.55, 'hsla(' + hues[i] + ',75%,30%,' + (i === 3 ? 0.025 : 0.05) + ')');
            gr.addColorStop(1, 'hsla(' + hues[i] + ',70%,25%,0)');
            g.fillStyle = gr;
            g.fillRect(0, 0, S, S);
            nebulaSprites.push(c);
        }
    }

    function makeAccretionSprite(S) {
        S = S || 512;
        var c = document.createElement('canvas');
        c.width = c.height = S;
        var g = c.getContext('2d');
        var cx = S / 2, cy = S / 2;
        var rng = mulberry32(hash32('emgor::accretion'));
        g.globalCompositeOperation = 'lighter';
        // haze
        var gr = g.createRadialGradient(cx, cy, S * 0.06, cx, cy, S * 0.5);
        gr.addColorStop(0, 'rgba(224,170,255,0.55)');
        gr.addColorStop(0.22, 'rgba(168,85,247,0.30)');
        gr.addColorStop(0.5, 'rgba(123,47,190,0.12)');
        gr.addColorStop(1, 'rgba(61,29,142,0)');
        g.fillStyle = gr;
        g.fillRect(0, 0, S, S);
        // streaks
        for (var i = 0; i < 46; i++) {
            var a0 = rng() * TAU;
            var len = 0.5 + rng() * 1.6;
            var r = S * (0.09 + rng() * 0.3);
            var hue = rng() < 0.16 ? 188 : 268 + rng() * 30;
            g.strokeStyle = 'hsla(' + hue + ',90%,' + (60 + rng() * 25) + '%,' + (0.05 + rng() * 0.12) + ')';
            g.lineWidth = 1 + rng() * 3.2;
            g.beginPath();
            g.arc(cx, cy, r, a0, a0 + len);
            g.stroke();
        }
        return c;
    }

    function makeVignette() {
        var c = document.createElement('canvas');
        c.width = Math.max(2, Math.floor(W / 4));
        c.height = Math.max(2, Math.floor(H / 4));
        var g = c.getContext('2d');
        var gr = g.createRadialGradient(
            c.width / 2, c.height / 2, Math.min(c.width, c.height) * 0.32,
            c.width / 2, c.height / 2, Math.max(c.width, c.height) * 0.72);
        gr.addColorStop(0, 'rgba(13,2,33,0)');
        gr.addColorStop(1, 'rgba(4,0,12,0.55)');
        g.fillStyle = gr;
        g.fillRect(0, 0, c.width, c.height);
        return c;
    }

    // ─── stars ─────────────────────────────────────────────────
    function makeStars() {
        var target = clamp(Math.floor((W * H) / 5200), 140, 460);
        starN = target;
        starX = new Float32Array(starN);
        starY = new Float32Array(starN);
        starR = new Float32Array(starN);
        starPh = new Float32Array(starN);
        starW = new Float32Array(starN);
        starTw = new Uint8Array(starN);
        starLayer = new Uint8Array(starN);
        starTint = new Uint8Array(starN);
        var rng = mulberry32(hash32('emgor::stars'));
        var cx = W / 2, cy = H / 2;
        var clearR = Math.min(W, H) * 0.36;   // keep the planet zone clearer
        for (var i = 0; i < starN; i++) {
            var x = rng() * W, y = rng() * H, tries = 0;
            while (tries < 3 && Math.hypot(x - cx, y - cy) < clearR && rng() < 0.68) {
                x = rng() * W; y = rng() * H; tries++;
            }
            starX[i] = x;
            starY[i] = y;
            var l = rng();
            starLayer[i] = l < 0.5 ? 0 : (l < 0.83 ? 1 : 2);
            starR[i] = 0.4 + starLayer[i] * 0.45 + rng() * 0.7;
            // shimmer: long fade-in/out cycles, 6–20 s, staggered phases
            starPh[i] = rng() * TAU;
            starW[i] = TAU / (6 + rng() * 14);
            starTw[i] = rng() < 0.3 ? 1 : 0;   // subset twinkles subtly faster
            var t = rng();
            starTint[i] = t < 0.07 ? 2 : (t < 0.26 ? 1 : 0); // white / violet / cyan
        }
    }

    var STAR_COLORS = ['rgba(235,225,255,', 'rgba(192,132,252,', 'rgba(110,235,255,'];

    // ─── viewport ──────────────────────────────────────────────
    function resize() {
        W = window.innerWidth;
        H = window.innerHeight;
        DPR = Math.min(window.devicePixelRatio || 1, 2);
        canvas.width = Math.floor(W * DPR);
        canvas.height = Math.floor(H * DPR);
        canvas.style.width = W + 'px';
        canvas.style.height = H + 'px';

        availX = Math.max(120, W / 2 - MARGIN_X);
        availY = Math.max(120, H / 2 - MARGIN_Y);
        var base = Math.min(availX, availY);
        stretchX = clamp(availX / base, 1, 2.2);
        stretchY = clamp(availY / base, 1, 1.5);

        makeStars();
        vignette = makeVignette();
    }

    function targetScaleFor(node) {
        return Math.min(
            availX / (stretchX * node.sysR * ORBIT_MAX),
            availY / (stretchY * node.sysR * ORBIT_MAX));
    }

    // ─── world positions (per-frame) ───────────────────────────
    // wx/wy are world x/z on the flat orbital plane; drag never touches
    // orbital angles — the whole scene is rotated at projection time.
    function computePositions() {
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var p = n.parentNode;
            // slow shared orbital rotation + per-body vibrato wander on top;
            // same base rate for all siblings keeps the separation guarantee
            var a = n.homeA + time * BASE_ROT +
                Math.sin(time * n.vibF + n.vibPh) * n.vibDepth;
            var r = n.orbF * p.sysR;
            n.wx = p.wx + Math.cos(a) * r;
            n.wy = p.wy + Math.sin(a) * r;
        }
    }

    // rotate the scene (yaw then pitch) around the camera center and
    // project with mild perspective; fills n.sx/sy/sr/sz/persp/dim
    function projectAll() {
        cosYaw = Math.cos(yaw); sinYaw = Math.sin(yaw);
        cosPit = Math.cos(pitch); sinPit = Math.sin(pitch);
        var viewR = availY / (stretchY * cam.scale);       // ~world radius on screen
        perspK = PERSP / Math.max(1e-6, viewR);
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var rx = n.wx - cam.x, rz = n.wy - cam.y;
            var x1 = rx * cosYaw + rz * sinYaw;
            var z1 = -rx * sinYaw + rz * cosYaw;
            var y2 = -z1 * sinPit;
            var z2 = z1 * cosPit;
            var pr = 1 / Math.max(0.3, 1 + z2 * perspK);
            n.persp = pr;
            n.sz = z2 * perspK;                            // normalized depth
            n.dim = clamp(1 - n.sz * 1.05, 0.45, 1.22);    // far dimmer, near brighter
            n.sx = W / 2 + x1 * stretchX * cam.scale * pr;
            n.sy = H / 2 + y2 * stretchY * cam.scale * pr;
            n.sr = n.bodyR * cam.scale * pr;
        }
        // root's projected position (label radial math needs every parent's)
        var rp = projectPoint(root.wx, root.wy);
        root.sx = rp.x; root.sy = rp.y;
    }

    // project an arbitrary flat-plane world point (for rings, black hole)
    var PROJ = { x: 0, y: 0, z: 0, p: 1 };
    function projectPoint(wx, wz) {
        var rx = wx - cam.x, rz = wz - cam.y;
        var x1 = rx * cosYaw + rz * sinYaw;
        var z1 = -rx * sinYaw + rz * cosYaw;
        var y2 = -z1 * sinPit;
        var z2 = z1 * cosPit;
        var pr = 1 / Math.max(0.3, 1 + z2 * perspK);
        PROJ.x = W / 2 + x1 * stretchX * cam.scale * pr;
        PROJ.y = H / 2 + y2 * stretchY * cam.scale * pr;
        PROJ.z = z2 * perspK;
        PROJ.p = pr;
        return PROJ;
    }

    // visual size boost by navigational role (planets are the main event)
    function boostFor(n, f) {
        if (n.parentNode === f)
            return CHILD_BOOST / (1 + Math.max(0, f.kids.length - 5) * 0.045);
        if (n === f) return 1.12;
        if (n.parentNode && n.parentNode.parentNode === f) return 1.45;
        return 1;
    }

    // relevance/alpha of a node for a given focus
    function alphaFor(n, f) {
        if (n === f) return f === root ? 0 : 0.95;          // central sun
        if (n.parentNode === f) return 1;                    // children: the nav ring
        if (n.parentNode && n.parentNode.parentNode === f) return 0.55; // grandkids: hints
        if (f !== root && n === f.parentNode) return 0.3;    // ambient background giant
        if (f !== root && n.parentNode === f.parentNode) return 0.16;   // siblings
        return 0;
    }

    // ─── navigation ────────────────────────────────────────────
    function focusTo(node, animate) {
        if (node === focus) return;
        if (animate && !reducedMotion) {
            trans = {
                from: focus, to: node, t: 0,
                sFrom: cam.scale, sTo: targetScaleFor(node)
            };
        } else {
            trans = null;
            cam.scale = targetScaleFor(node);
        }
        focus = node;
        updateCrumb();
    }

    function updateCrumb() {
        var parts = ['emgor'];
        var chain = [];
        var n = focus;
        while (n && n !== root) { chain.unshift(n.title.toLowerCase()); n = n.parentNode; }
        crumbEl.textContent = parts.concat(chain).join(' / ');
        homeBtn.classList.toggle('is-dim', focus === root && !overlayNode);
    }

    // direct launch: fade to black, then jump straight to the node's page
    var warpEl = null;
    function launchTo(node) {
        if (!node.launch) return;
        if (warpEl) warpEl.classList.add('is-on');
        setTimeout(function () { window.location.href = node.launch; },
            reducedMotion || !warpEl ? 0 : 320);
    }

    function go(route) {
        var target = '#' + route;
        if (location.hash === target) { applyRoute(route, true); return; }
        suppressHash = false;
        location.hash = target;      // triggers hashchange -> applyRoute
    }

    function applyRoute(route, animate) {
        route = route || '/';
        if (route.charAt(0) !== '/') route = '/' + route;
        var node = byRoute[route];
        if (!node) node = root;
        if (node === root || node.kids.length > 0) {
            closeOverlay(false);
            focusTo(node, animate);
        } else if (node.launch) {
            // deep-linked launch node: show its system, never auto-redirect
            // (would trap the back button); the click is what launches
            closeOverlay(false);
            focusTo(node.parentNode, animate);
        } else {
            focusTo(node.parentNode, animate);
            openOverlay(node);
        }
    }

    function zoomOut() {
        if (overlayNode) { closeOverlay(true); return; }
        if (focus !== root && focus.parentNode) {
            go(focus.parentNode === root ? '/' : focus.parentNode.route);
        }
    }

    function onHashChange() {
        if (suppressHash) { suppressHash = false; return; }
        applyRoute(location.hash.replace(/^#/, ''), true);
    }

    // ─── overlay ───────────────────────────────────────────────
    var ovRoot, ovPanel, ovTitle, ovBlurb, ovMeta, ovDl, ovLinks, ovBody, ovClose;
    var lastFocusedEl = null;

    function buildOverlay() {
        ovRoot = document.getElementById('overlay');
        ovRoot.innerHTML =
            '<div class="ov-backdrop" data-ov-close></div>' +
            '<article class="ov-panel" role="dialog" aria-modal="true" aria-labelledby="ovTitle">' +
            '  <button class="ov-close" data-ov-close aria-label="close panel">×</button>' +
            '  <header class="ov-head">' +
            '    <h1 id="ovTitle" class="ov-title"></h1>' +
            '    <p class="ov-blurb"></p>' +
            '    <div class="ov-meta"></div>' +
            '  </header>' +
            '  <div class="ov-downloads"></div>' +
            '  <div class="ov-links"></div>' +
            '  <div class="ov-body md"></div>' +
            '</article>';
        ovPanel = ovRoot.querySelector('.ov-panel');
        ovTitle = ovRoot.querySelector('.ov-title');
        ovBlurb = ovRoot.querySelector('.ov-blurb');
        ovMeta = ovRoot.querySelector('.ov-meta');
        ovDl = ovRoot.querySelector('.ov-downloads');
        ovLinks = ovRoot.querySelector('.ov-links');
        ovBody = ovRoot.querySelector('.ov-body');
        ovClose = ovRoot.querySelector('.ov-close');
        ovRoot.addEventListener('click', function (e) {
            if (e.target.hasAttribute('data-ov-close')) closeOverlay(true);
        });
    }

    function renderMeta(node, meta) {
        var bits = [];
        var updated = (meta && meta.updated) || node.updated;
        if (updated) bits.push('<span class="ov-updated">updated ' + updated + '</span>');
        var tags = (meta && meta.tags && meta.tags.length ? meta.tags : node.tags) || [];
        tags.forEach(function (t) { bits.push('<span class="ov-tag">#' + String(t) + '</span>'); });
        ovMeta.innerHTML = bits.join('');
    }

    function fileName(u) {
        var s = String(u); var i = s.lastIndexOf('/');
        return i === -1 ? s : s.slice(i + 1);
    }

    function renderDownloadsLinks(node) {
        ovDl.innerHTML = '';
        (node.downloads || []).forEach(function (d) {
            var url = typeof d === 'string' ? d : d.url;
            var name = typeof d === 'string' ? fileName(d) : (d.name || fileName(d.url));
            var a = document.createElement('a');
            a.className = 'ov-dl';
            a.href = url;
            a.setAttribute('download', '');
            a.innerHTML = '<span class="ov-dl-arrow">↓</span> ' + name;
            ovDl.appendChild(a);
        });
        ovLinks.innerHTML = '';
        (node.links || []).forEach(function (l) {
            var a = document.createElement('a');
            a.className = 'ov-pill';
            a.href = l.url;
            if (/^https?:\/\//.test(l.url)) { a.target = '_blank'; a.rel = 'noopener'; }
            a.textContent = l.label || l.url;
            ovLinks.appendChild(a);
        });
    }

    function openOverlay(node) {
        overlayNode = node;
        lastFocusedEl = document.activeElement;
        ovTitle.textContent = node.title.toLowerCase();
        ovBlurb.textContent = node.blurb;
        renderMeta(node, null);
        renderDownloadsLinks(node);
        ovBody.innerHTML = '<p class="ov-loading">receiving transmission…</p>';
        ovRoot.hidden = false;
        requestAnimationFrame(function () { ovRoot.classList.add('is-open'); });
        ovClose.focus();
        updateCrumb();

        if (!node.path) {
            ovBody.innerHTML = '<p class="ov-missing">no archive at this coordinate yet.</p>';
            return;
        }
        fetch(node.path)
            .then(function (r) { if (!r.ok) throw new Error(r.status); return r.text(); })
            .then(function (text) {
                if (overlayNode !== node) return;
                var fm = window.MDLITE.parseFrontmatter(text);
                if (fm.meta.title) ovTitle.textContent = String(fm.meta.title).toLowerCase();
                if (fm.meta.blurb) ovBlurb.textContent = fm.meta.blurb;
                renderMeta(node, fm.meta);
                ovBody.innerHTML = window.MDLITE.render(fm.body);
            })
            .catch(function () {
                if (overlayNode !== node) return;
                ovBody.innerHTML =
                    '<p class="ov-missing">transmission pending — this sector\'s archive ' +
                    'hasn\'t been synced yet.</p>';
            });
    }

    function closeOverlay(navigate) {
        if (!overlayNode) return;
        var node = overlayNode;
        overlayNode = null;
        ovRoot.classList.remove('is-open');
        setTimeout(function () { if (!overlayNode) ovRoot.hidden = true; }, 260);
        if (lastFocusedEl && lastFocusedEl.focus) lastFocusedEl.focus();
        updateCrumb();
        if (navigate) {
            var p = node.parentNode;
            go(p === root ? '/' : p.route);
        }
    }

    // ─── labels (real DOM <a> elements) ────────────────────────
    function labelFor(node) {
        var el = labelPool[node.id];
        if (el) return el;
        el = document.createElement('a');
        el.className = 'planet-label';
        // launch nodes link straight at their destination (crawler/middle-click safe)
        el.href = node.launch ? node.launch : '#' + node.route;
        el.draggable = false;
        el.innerHTML =
            '<span class="pl-name">' + node.title.toLowerCase() + '</span>' +
            (node.kids.length
                ? '<span class="pl-sub">' + node.kids.length + ' bodies</span>'
                : (node.launch
                    ? '<span class="pl-sub pl-leaf">launch ↗</span>'
                    : '<span class="pl-sub pl-leaf">landing</span>'));
        el.title = node.blurb;
        el.addEventListener('click', function (e) {
            if (e.metaKey || e.ctrlKey || e.shiftKey) return;
            e.preventDefault();
            if (node.launch) { launchTo(node); return; }
            go(node.route);
        });
        labelsEl.appendChild(el);
        labelPool[node.id] = el;
        return el;
    }

    var labArr = [];                // visible labels this frame (reused)

    function syncLabels() {
        // labeled set: children of focus (+ children of trans.from while flying)
        var id;
        for (id in labelPool) labelPool[id]._keep = false;

        labArr.length = 0;
        var e = trans ? easeInOut(clamp(trans.t, 0, 1)) : 1;
        collectLabels(focus.kids, trans ? e : 1);
        if (trans) collectLabels(trans.from.kids, 1 - e, focus.kids);
        layoutLabels();

        for (id in labelPool) {
            var el = labelPool[id];
            if (!el._keep && el.style.display !== 'none') el.style.display = 'none';
        }
    }

    function collectLabels(kids, alpha, excludeIfIn) {
        for (var i = 0; i < kids.length; i++) {
            var n = kids[i];
            if (excludeIfIn && excludeIfIn.indexOf(n) !== -1) continue;
            var el = labelFor(n);
            el._keep = true;
            var a = alpha * (intro ? n.rev : 1) * clamp(n.dim, 0.6, 1);
            if (a <= 0.02) { el.style.display = 'none'; continue; }

            var fs = clamp(n.sr * 0.3, 13, 22);
            var lw = n.title.length * fs * 0.66 + 22;
            var lh = fs * 2.15;

            // far-side placement: offset outward along the radial from the
            // projected system center so text never crowds the sun/black hole
            var p = n.parentNode;
            var dx = n.sx - p.sx, dy = n.sy - p.sy;
            var L = Math.sqrt(dx * dx + dy * dy);
            var ux = L < 1 ? 0 : dx / L, uy = L < 1 ? -1 : dy / L;
            var off = n.sr + lh * 0.62 + 6;
            n._lx = clamp(n.sx + ux * off, lw / 2 + 6, W - lw / 2 - 6);
            n._ly = clamp(n.sy + uy * off, lh / 2 + 6, H - lh / 2 - 6);
            n._lw = lw; n._lh = lh; n._fs = fs; n._la = a; n._el = el;
            labArr.push(n);
        }
    }

    // light relaxation: labels never overlap each other, other planets,
    // or leave the viewport — layout is near-collision-free by construction,
    // this is the per-frame guarantee
    function layoutLabels() {
        var i, j, a, b;
        for (var it = 0; it < 2; it++) {
            for (i = 0; i < labArr.length; i++) {
                a = labArr[i];
                // vs other labels (AABB, 4px pad)
                for (j = i + 1; j < labArr.length; j++) {
                    b = labArr[j];
                    var ox = (a._lw + b._lw) / 2 + 4 - Math.abs(a._lx - b._lx);
                    var oy = (a._lh + b._lh) / 2 + 4 - Math.abs(a._ly - b._ly);
                    if (ox > 0 && oy > 0) {
                        if (ox < oy) {
                            var sx = a._lx < b._lx ? -1 : 1;
                            a._lx += sx * ox / 2; b._lx -= sx * ox / 2;
                        } else {
                            var sy = a._ly < b._ly ? -1 : 1;
                            a._ly += sy * oy / 2; b._ly -= sy * oy / 2;
                        }
                    }
                }
                // vs other planets' discs
                for (j = 0; j < labArr.length; j++) {
                    b = labArr[j];
                    if (b === a) continue;
                    var cx2 = clamp(b.sx, a._lx - a._lw / 2, a._lx + a._lw / 2);
                    var cy2 = clamp(b.sy, a._ly - a._lh / 2, a._ly + a._lh / 2);
                    var ddx = cx2 - b.sx, ddy = cy2 - b.sy;
                    var dd = Math.sqrt(ddx * ddx + ddy * ddy);
                    var need = b.sr + 4;
                    if (dd < need) {
                        var pdx = a._lx - b.sx, pdy = a._ly - b.sy;
                        var pl = Math.sqrt(pdx * pdx + pdy * pdy) || 1;
                        a._lx += (pdx / pl) * (need - dd);
                        a._ly += (pdy / pl) * (need - dd);
                    }
                }
                a._lx = clamp(a._lx, a._lw / 2 + 6, W - a._lw / 2 - 6);
                a._ly = clamp(a._ly, a._lh / 2 + 6, H - a._lh / 2 - 6);
            }
        }
        for (i = 0; i < labArr.length; i++) {
            a = labArr[i];
            var el = a._el;
            var zi = a.sz < 0 ? 15 : 3;
            if (el._zi !== zi) { el._zi = zi; el.style.zIndex = zi; }
            el.style.display = 'flex';
            el.style.width = a._lw + 'px';
            el.style.height = a._lh + 'px';
            el.style.fontSize = a._fs + 'px';
            el.style.opacity = a._la.toFixed(3);
            el.style.transform = 'translate3d(' + (a._lx - a._lw / 2) + 'px,' +
                (a._ly - a._lh / 2) + 'px,0)';
        }
    }

    // ─── big bang ──────────────────────────────────────────────
    function startIntro() {
        intro = { t: 0, seeded: false };
        var kids = root.kids;
        for (var i = 0; i < kids.length; i++) kids[i].rev = 0;
    }

    function seedParticles() {
        var rng = mulberry32(hash32('emgor::bang'));
        for (var i = 0; i < PN; i++) {
            var a = rng() * TAU;
            var sp = 90 + Math.pow(rng(), 1.6) * 780;
            px[i] = W / 2; py[i] = H / 2;
            pvx[i] = Math.cos(a) * sp; pvy[i] = Math.sin(a) * sp * 0.8;
            plife[i] = 0.65 + rng() * 0.6;
            var roll = rng();
            phue[i] = roll < 0.12 ? 188 : (roll < 0.62 ? 268 : 285);
        }
    }

    function drawIntro(dt) {
        var t = intro.t;
        ctx.globalCompositeOperation = 'lighter';
        if (t < 0.55) {                       // the seed, swelling
            var p = t / 0.55;
            var r = 8 + p * p * 110;
            var gr = ctx.createRadialGradient(W / 2, H / 2, 0, W / 2, H / 2, r);
            gr.addColorStop(0, 'rgba(255,245,255,' + (0.5 + p * 0.5) + ')');
            gr.addColorStop(0.4, 'rgba(192,132,252,' + (0.35 + p * 0.4) + ')');
            gr.addColorStop(1, 'rgba(123,47,190,0)');
            ctx.fillStyle = gr;
            ctx.beginPath(); ctx.arc(W / 2, H / 2, r, 0, TAU); ctx.fill();
        } else {
            if (!intro.seeded) { intro.seeded = true; seedParticles(); }
            var flash = clamp(1 - (t - 0.55) / 0.5, 0, 1);
            if (flash > 0) {
                ctx.fillStyle = 'rgba(224,170,255,' + (flash * flash * 0.5).toFixed(3) + ')';
                ctx.fillRect(0, 0, W, H);
            }
            var drag = Math.pow(0.5, dt * 2.2);
            for (var i = 0; i < PN; i++) {
                if (plife[i] <= 0) continue;
                plife[i] -= dt * 0.55;
                px[i] += pvx[i] * dt;
                py[i] += pvy[i] * dt;
                pvx[i] *= drag; pvy[i] *= drag;
                var a = clamp(plife[i], 0, 1) * 0.85;
                if (a <= 0.01) continue;
                ctx.fillStyle = 'hsla(' + phue[i] + ',95%,72%,' + a.toFixed(3) + ')';
                ctx.fillRect(px[i] - 1, py[i] - 1, 2.4, 2.4);
            }
        }
        ctx.globalCompositeOperation = 'source-over';

        // planets resolve outward, staggered
        var kids = root.kids;
        for (var k = 0; k < kids.length; k++) {
            kids[k].rev = easeOut(clamp((t - 0.75 - k * 0.09) / 0.7, 0, 1));
        }
        intro.t += dt;
        if (intro.t > 0.75 + kids.length * 0.09 + 0.9) {
            for (var k2 = 0; k2 < kids.length; k2++) kids[k2].rev = 1;
            intro = null;
        }
    }

    // ─── render ────────────────────────────────────────────────
    function draw(dt) {
        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);

        // deep space base
        ctx.fillStyle = VOID;
        ctx.fillRect(0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        drawNebulae();
        drawStars();
        ctx.globalCompositeOperation = 'source-over';

        drawWorld();

        if (intro) drawIntro(dt);

        if (vignette) {
            ctx.drawImage(vignette, 0, 0, W, H);
        }
    }

    function drawNebulae() {
        var n = nebulaSprites.length;
        var d = Math.max(W, H);
        for (var i = 0; i < n; i++) {
            // anchored like the stars — only an extremely faint autonomous drift
            var ph = time * 0.012 + i * 2.1;
            var bx = W * (0.18 + 0.64 * (0.5 + 0.5 * Math.sin(ph + i * 1.7)));
            var by = H * (0.2 + 0.6 * (0.5 + 0.5 * Math.cos(ph * 0.8 + i * 2.3)));
            var s = d * (0.7 + i * 0.22);
            ctx.drawImage(nebulaSprites[i], bx - s / 2, by - s / 2, s, s);
        }
    }

    function drawStars() {
        // anchored: fixed positions, no rotation/zoom coupling — the distant
        // universe. Stars shimmer in and out of existence on long cycles.
        for (var i = 0; i < starN; i++) {
            var a;
            if (reducedMotion) {
                a = 0.4;
            } else {
                var s = 0.5 + 0.5 * Math.sin(time * starW[i] + starPh[i]);
                a = Math.pow(s, 1.6) * (0.55 + starLayer[i] * 0.15);
                if (starTw[i]) a *= 0.82 + 0.18 * Math.sin(time * 2.7 + starPh[i] * 3.1);
            }
            if (a <= 0.015) continue;
            ctx.fillStyle = STAR_COLORS[starTint[i]] + a.toFixed(3) + ')';
            var r = starR[i];
            ctx.fillRect(starX[i] - r, starY[i] - r, r * 2, r * 2);
        }
    }

    var DASH = [6, 7];
    var NO_DASH = [];

    // far bodies first; insertion sort — cheap on mostly-sorted data
    function sortByDepth() {
        for (var i = 1; i < sortOrder.length; i++) {
            var n = sortOrder[i], j = i - 1;
            while (j >= 0 && sortOrder[j].sz < n.sz) {
                sortOrder[j + 1] = sortOrder[j];
                j--;
            }
            sortOrder[j + 1] = n;
        }
    }

    function drawWorld() {
        var e = trans ? easeInOut(clamp(trans.t, 0, 1)) : 1;
        var i, n;

        // orbit rings for focus system (and previous during flight)
        drawOrbitRings(focus, trans ? e : 1);
        if (trans) drawOrbitRings(trans.from, 1 - e);

        // pass 1: alpha + boosted size (drawing happens in depth order below)
        for (i = 0; i < drawOrder.length; i++) {
            n = drawOrder[i];
            var a = alphaFor(n, focus);
            if (trans) a = lerp(alphaFor(n, trans.from), a, e);
            if (intro) {
                var anc = n;
                while (anc.parentNode && anc.parentNode !== root) anc = anc.parentNode;
                a *= anc.rev;
            }
            n.alpha = a;
            var b = boostFor(n, focus);
            if (trans) b = lerp(boostFor(n, trans.from), b, e);
            n.sr = n.bodyR * cam.scale * n.persp * b;
            if (intro && n.parentNode === root) {
                // resolve out of the center during the big bang
                n.sx = lerp(W / 2, n.sx, n.rev);
                n.sy = lerp(H / 2, n.sy, n.rev);
            }
        }

        lodUpdate();
        sortByDepth();

        // the center (black hole) slots into the depth order at its own z
        var bp = projectPoint(0, 0);
        var bx = bp.x, by = bp.y, bz = bp.z;
        var bcore = ROOT_SYS_R * 0.075 * cam.scale * bp.p;
        var centerOK = bcore < 2600 && bx > -W && bx < 2 * W && by > -H && by < 2 * H;
        var centerDrawn = !centerOK;

        // pass 2: far → near
        for (i = 0; i < sortOrder.length; i++) {
            n = sortOrder[i];
            if (!centerDrawn && n.sz < bz) {
                drawBlackHole(bx, by, bcore);
                centerDrawn = true;
            }
            var al = n.alpha;
            if (al <= 0.01) continue;
            var r = n.sr;
            if (r < 0.5 || r > 6000) continue;
            if (n.sx < -r * 4 || n.sx > W + r * 4 || n.sy < -r * 4 || n.sy > H + r * 4) continue;
            drawPlanet(n, al);
        }
        if (!centerDrawn) drawBlackHole(bx, by, bcore);
    }

    function drawOrbitRings(f, alpha) {
        if (alpha <= 0.02) return;
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        ctx.setLineDash(DASH);
        ctx.lineWidth = 1;
        var SEG = 56;
        for (var i = 0; i < f.kids.length; i++) {
            var k = f.kids[i];
            var r = k.orbF * f.sysR;
            var rpx = r * cam.scale;
            if (rpx < 8 || rpx > W * 4) continue;
            var a = alpha * 0.14 * (intro ? k.rev : 1);
            if (a <= 0.01) continue;
            ctx.strokeStyle = 'rgba(168,85,247,' + a.toFixed(3) + ')';
            ctx.beginPath();
            for (var s = 0; s <= SEG; s++) {
                var th = (s / SEG) * TAU;
                var p = projectPoint(f.wx + Math.cos(th) * r, f.wy + Math.sin(th) * r);
                if (s === 0) ctx.moveTo(p.x, p.y);
                else ctx.lineTo(p.x, p.y);
            }
            ctx.stroke();
        }
        ctx.setLineDash(NO_DASH);
        ctx.restore();
    }

    function drawBlackHole(x, y, core) {
        var t = reducedMotion ? 0 : time;
        var ds = core * 7;
        // lazily bake a 1024px tier once the disc genuinely outgrows the
        // base 512px sprite; a single one-time asset, kept forever (never
        // evicted — there is exactly one black hole), skipped mid-flight so
        // the zoom transition doesn't stutter
        if (!accretionHi && ds * DPR > 640 && !(trans && trans.t < 0.85)) {
            accretionHi = makeAccretionSprite(1024);
        }
        var disk = accretionHi || accretionSprite;
        ctx.save();
        // accretion disk (rotating sprite, additive)
        ctx.globalCompositeOperation = 'lighter';
        ctx.translate(x, y);
        ctx.rotate(t * 0.12);
        ctx.globalAlpha = 0.85 + 0.15 * Math.sin(t * 1.7);
        ctx.drawImage(disk, -ds / 2, -ds / 2, ds, ds);
        ctx.rotate(-t * 0.26);
        ctx.globalAlpha = 0.4;
        ctx.drawImage(disk, -ds * 0.35, -ds * 0.35, ds * 0.7, ds * 0.7);
        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalAlpha = 1;
        // event horizon
        ctx.globalCompositeOperation = 'source-over';
        var gr = ctx.createRadialGradient(x, y, core * 0.2, x, y, core * 1.9);
        gr.addColorStop(0, 'rgba(2,0,8,1)');
        gr.addColorStop(0.55, 'rgba(4,0,12,0.95)');
        gr.addColorStop(1, 'rgba(13,2,33,0)');
        ctx.fillStyle = gr;
        ctx.beginPath(); ctx.arc(x, y, core * 1.9, 0, TAU); ctx.fill();
        // photon ring
        ctx.globalCompositeOperation = 'lighter';
        ctx.strokeStyle = 'rgba(224,170,255,' + (0.5 + 0.2 * Math.sin(t * 2.3)).toFixed(3) + ')';
        ctx.lineWidth = Math.max(1, core * 0.045);
        ctx.beginPath(); ctx.arc(x, y, core, 0, TAU); ctx.stroke();
        ctx.restore();
        ctx.globalCompositeOperation = 'source-over';
    }

    function drawPlanet(n, alpha) {
        var t = reducedMotion ? 0 : time;
        var pulse = reducedMotion ? 0.5 : 0.5 + 0.5 * Math.sin(t * n.pulseRate + n.pulsePhase);
        var r = n.sr;
        var dim = n.dim;
        // glow (additive) — radially symmetric, so it never needs rotation
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = Math.min(1, alpha * dim * (0.26 + pulse * 0.16));
        var gs = r * (2.9 + pulse * 0.35);
        ctx.drawImage(n.glow, n.sx - gs / 2, n.sy - gs / 2, gs, gs);
        // body sprite (ring included) — sprite body radius is spriteR of its size.
        // The sprite is a flat pre-baked raster (fixed light dir + fixed ring
        // tilt), so on its own it would stay screen-locked as the camera orbits
        // — most obvious on the focused/central body, whose screen position
        // never moves at all. Apply a camera-driven screen-space spin so every
        // body (central and orbiting alike, applied uniformly — no special
        // casing) visibly turns with the scene instead of reading as a decal.
        // Spin tracks yaw, scaled by sinPit: viewed edge-on (pitch≈0, ring
        // edge-on) a yaw orbit produces no in-plane rotation of a flat disc —
        // physically correct — while viewed from near-top (pitch≈π/2, ring
        // reads circular) yaw maps ~1:1 onto screen rotation, exactly like
        // watching a disc spin from above. This is a deliberate 2D approximation
        // (no per-frame relight/re-bake — drawImage-only stays intact) but it's
        // the natural yaw/pitch-only source of "spin" available without one.
        // Driven directly by yaw/pitch state (set by drag, or by momentum which
        // is itself gated off under prefers-reduced-motion), so this rotation
        // automatically respects the existing reduced-motion convention.
        var rot = yaw * sinPit;
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = Math.min(1, alpha * Math.min(1.06, dim));
        var ss = r / n.spriteR;
        ctx.save();
        ctx.translate(n.sx, n.sy);
        ctx.rotate(rot);
        ctx.drawImage(n.sprite, -ss / 2, -ss / 2, ss, ss);
        ctx.restore();
        ctx.globalAlpha = 1;
    }

    // ─── frame loop ────────────────────────────────────────────
    function tick(ts) {
        requestAnimationFrame(tick);
        var dt = lastTs ? Math.min((ts - lastTs) / 1000, 0.05) : 0.016;
        lastTs = ts;
        frame++;
        if (!reducedMotion) time += dt;

        computePositions();

        // camera
        if (trans) {
            trans.t += dt / FLY_DUR;
            var e = easeInOut(clamp(trans.t, 0, 1));
            cam.x = lerp(trans.from.wx, trans.to.wx, e);
            cam.y = lerp(trans.from.wy, trans.to.wy, e);
            cam.scale = Math.exp(lerp(Math.log(trans.sFrom), Math.log(trans.sTo), e));
            if (trans.t >= 1) trans = null;
        } else {
            cam.x = focus.wx;
            cam.y = focus.wy;
            var tScale = targetScaleFor(focus);
            cam.scale += (tScale - cam.scale) * Math.min(1, dt * 5);
        }

        // trackball momentum: fling keeps the model turning, friction slows it —
        // full 360° now, so it wraps smoothly through the poles instead of
        // hitting a clamp wall
        if (!dragging && !reducedMotion && (yawV !== 0 || pitchV !== 0)) {
            yaw = wrapAngle(yaw + yawV * dt);
            pitch = wrapAngle(pitch + pitchV * dt);
            var sfr = Math.pow(0.5, dt * 1.6);
            yawV *= sfr; pitchV *= sfr;
            if (Math.abs(yawV) + Math.abs(pitchV) < 0.01) { yawV = 0; pitchV = 0; }
        }

        projectAll();
        draw(dt);
        syncLabels();
    }

    // ─── input ─────────────────────────────────────────────────
    var dragging = false;
    var pointers = {};
    var pinchStart = 0, pinchFired = false;
    var dragSX = 0, dragSY = 0, dragMoved = false, dragConsumed = false;
    var dragLX = 0, dragLY = 0, dragT = 0;
    var ROT_V_MAX = 8;              // rad/s fling cap per axis

    function pointerCount() {
        var c = 0; for (var k in pointers) c++; return c;
    }

    function grabbable(t) {
        if (t === canvas) return true;
        return !!(t && t.closest && t.closest('.planet-label'));
    }

    function setupInput() {
        document.addEventListener('pointerdown', function (e) {
            if (!grabbable(e.target)) return;
            dragConsumed = false;   // stale guard must never eat a fresh tap
            pointers[e.pointerId] = { x: e.clientX, y: e.clientY };
            var n = pointerCount();
            if (n === 1) {
                dragging = true; dragMoved = false;
                dragSX = e.clientX; dragSY = e.clientY;
                dragLX = e.clientX; dragLY = e.clientY;
                dragT = performance.now();
                yawV = 0; pitchV = 0;
                document.body.classList.add('is-grabbing');
            } else if (n === 2) {
                dragging = false;
                pinchStart = pinchDist();
                pinchFired = false;
            }
        });

        window.addEventListener('pointermove', function (e) {
            var p = pointers[e.pointerId];
            if (!p) return;
            p.x = e.clientX; p.y = e.clientY;
            var n = pointerCount();
            if (n === 1 && dragging) {
                // trackball: horizontal drag yaws the model, vertical drag
                // tilts the orbital plane — orbital motion itself is untouched.
                // Past the pole (cosPit < 0, camera flipped upside-down) the
                // near/far sides of the disc have swapped, so a horizontal
                // drag needs its sign flipped to keep tracking the cursor —
                // otherwise the model appears to spin backwards once you tip
                // it past vertical.
                var dx = e.clientX - dragLX, dy = e.clientY - dragLY;
                var yawInv = cosPit < 0 ? -1 : 1;
                var dYaw = -dx * (4.6 / Math.max(320, W)) * yawInv;
                var dPit = dy * (3.2 / Math.max(320, H));
                yaw = wrapAngle(yaw + dYaw);
                pitch = wrapAngle(pitch + dPit);
                var now = performance.now();
                var dts = Math.max(0.008, (now - dragT) / 1000);
                yawV = clamp(0.75 * (dYaw / dts) + 0.25 * yawV, -ROT_V_MAX, ROT_V_MAX);
                pitchV = clamp(0.75 * (dPit / dts) + 0.25 * pitchV, -ROT_V_MAX, ROT_V_MAX);
                dragLX = e.clientX; dragLY = e.clientY; dragT = now;
                if (Math.abs(e.clientX - dragSX) + Math.abs(e.clientY - dragSY) > 6)
                    dragMoved = true;
            } else if (n === 2 && pinchStart > 0 && !pinchFired) {
                var ratio = pinchDist() / pinchStart;
                if (ratio < 0.72) { pinchFired = true; zoomOut(); }
                else if (ratio > 1.4) { pinchFired = true; pinchZoomIn(); }
            }
        });

        function endPointer(e) {
            if (!pointers[e.pointerId]) return;
            delete pointers[e.pointerId];
            if (pointerCount() === 0) {
                if (dragMoved) dragConsumed = true;
                dragging = false; pinchStart = 0;
                document.body.classList.remove('is-grabbing');
            }
        }
        window.addEventListener('pointerup', endPointer);
        window.addEventListener('pointercancel', endPointer);

        // a drag must never fire the link/canvas click underneath it
        document.addEventListener('click', function (e) {
            if (dragConsumed) {
                e.preventDefault();
                e.stopPropagation();
                dragConsumed = false;
            }
        }, true);

        // wheel: down = zoom out, up = dive into nearest child under cursor
        var wheelAcc = 0, wheelCooldown = 0;
        canvas.addEventListener('wheel', function (e) {
            e.preventDefault();
            var now = performance.now();
            if (now < wheelCooldown) return;
            wheelAcc += e.deltaY;
            if (wheelAcc > 240) {
                wheelAcc = 0; wheelCooldown = now + 650; zoomOut();
            } else if (wheelAcc < -240) {
                wheelAcc = 0; wheelCooldown = now + 650;
                var n = nearestChild(e.clientX, e.clientY);
                if (n) go(n.route);
            }
        }, { passive: false });

        // tap on canvas near a planet counts as click (labels usually catch it first)
        canvas.addEventListener('click', function (e) {
            if (dragMoved) return;
            var n = nearestChild(e.clientX, e.clientY);
            if (n) {
                var d = Math.hypot(n.sx - e.clientX, n.sy - e.clientY);
                if (d < Math.max(34, n.sr * 1.6)) {
                    if (n.launch) launchTo(n);
                    else go(n.route);
                }
            }
        });

        window.addEventListener('keydown', function (e) {
            if (e.key === 'Escape') {
                if (overlayNode) closeOverlay(true);
                else zoomOut();
            }
        });

        homeBtn.addEventListener('click', function (e) {
            e.preventDefault();
            if (overlayNode) { closeOverlay(true); return; }
            if (focus !== root) go('/');
        });
    }

    function pinchDist() {
        var pts = [];
        for (var k in pointers) pts.push(pointers[k]);
        if (pts.length < 2) return 0;
        return Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
    }

    function pinchZoomIn() {
        var pts = [];
        for (var k in pointers) pts.push(pointers[k]);
        var mx = pts.length >= 2 ? (pts[0].x + pts[1].x) / 2 : W / 2;
        var my = pts.length >= 2 ? (pts[0].y + pts[1].y) / 2 : H / 2;
        var n = nearestChild(mx, my);
        if (n && n.kids.length) go(n.route);
    }

    function nearestChild(x, y) {
        var best = null, bd = Infinity;
        for (var i = 0; i < focus.kids.length; i++) {
            var k = focus.kids[i];
            var d = Math.hypot(k.sx - x, k.sy - y);
            if (d < bd) { bd = d; best = k; }
        }
        return best;
    }

    // ─── boot ──────────────────────────────────────────────────
    function boot() {
        canvas = document.getElementById('galaxy');
        labelsEl = document.getElementById('labels');
        crumbEl = document.getElementById('crumb');
        homeBtn = document.getElementById('homeBtn');
        ctx = canvas && canvas.getContext && canvas.getContext('2d');

        if (!ctx) {
            var fb = document.getElementById('fallback');
            if (fb) fb.hidden = false;
            return;
        }

        buildOverlay();
        warpEl = document.createElement('div');
        warpEl.className = 'warp';
        document.body.appendChild(warpEl);
        // returning via bfcache must never leave the fade stuck on
        window.addEventListener('pageshow', function () {
            warpEl.classList.remove('is-on');
        });
        makeNebulaSprites();
        accretionSprite = makeAccretionSprite();
        resize();
        window.addEventListener('resize', function () {
            resize();
            if (!trans) cam.scale = targetScaleFor(focus);
        });

        loadData().then(function (data) {
            buildTree(data);
            focus = root;
            cam.scale = targetScaleFor(root);
            setupInput();
            window.addEventListener('hashchange', onHashChange);

            var hash = location.hash.replace(/^#/, '');
            var deepLinked = hash && hash !== '/' && byRoute[hash];
            if (deepLinked) {
                applyRoute(hash, false);
            } else if (!reducedMotion && !sessionStorage.getItem('emgor-bigbang')) {
                sessionStorage.setItem('emgor-bigbang', '1');
                startIntro();
            }
            updateCrumb();
            requestAnimationFrame(tick);
            document.body.classList.add('galaxy-ready');
        }).catch(function (err) {
            var fb = document.getElementById('fallback');
            if (fb) fb.hidden = false;
            if (window.console) console.error('galaxy2d: failed to load data', err);
        });
    }

    // debug/inspection hook (read-only)
    window.EMGOR_GALAXY = {
        // debug: advance the animation clock (vibrato etc.) by s seconds —
        // lets throttled tabs (automation, background) inspect motion
        nudge: function (s) { if (!reducedMotion) time += +s || 0; },
        get state() {
            return {
                focus: focus && focus.id, kids: focus && focus.kids.length,
                cam: { x: cam.x, y: cam.y, scale: cam.scale },
                intro: !!intro, trans: !!trans, frame: frame,
                overlay: overlayNode && overlayNode.id
            };
        }
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', boot);
    } else {
        boot();
    }
})();
