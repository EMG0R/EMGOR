/* galaxy3d.js — EMGOR fractal galaxy engine, true-3D edition.
   Renders galaxy.json (falls back to js/galaxy-stub.json) as one real
   THREE.Scene: every body — including the black hole at the virtual root
   "emgor" — is a real Object3D at a real absolute (x, y, z) world position,
   sharing one coordinate space. The camera is the only thing that moves;
   nothing is rescaled or re-projected per-node the way the old fake-3D
   Canvas engine (js/galaxy2d.js) had to. See that file for the full
   behavioral spec this file reproduces — this file is a renderer swap,
   not a behavior change.

   Stack: three.js r160, loaded as a pinned ESM build straight from a CDN.
   No bundler, no build step — this *is* the shipped file. */

import * as THREE from 'https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.js';

(function () {
    'use strict';

    // ─── constants (ported 1:1 from galaxy2d.js — same galaxy shape) ──
    var VOID = '#060112';   // near-black, faint violet — deep cinematic void
    var ROOT_SYS_R = 1200;          // world radius of the root system
    var SHRINK = 0.2;               // child system radius = parent * SHRINK
    var BODY_F = 0.3;               // body radius = own system radius * BODY_F
    var ORBIT_MIN = 0.46, ORBIT_MAX = 1.0;
    var MARGIN_X = 44, MARGIN_Y = 76;
    var FLY_DUR = 1.15;             // seconds, fractal zoom flight
    var CHILD_BOOST = 1.9;          // visual size boost for the focused nav ring
    var TAU = Math.PI * 2;
    var BASE_ROT = 0.022;           // rad/s — slow chill shared orbital drift

    var SIZE_FALLBACK = { 'emgor.papers': 2.0, 'emgor.web-synth': 1.4 };

    var reducedMotion = window.matchMedia &&
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    // ─── seeded randomness (identical to galaxy2d.js — determinism is the
    // contract: same node id -> same hash -> same PRNG stream -> same look,
    // forever, in every renderer that ever reads this file) ───────────
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
    function wrapAngle(a) {
        a = a % TAU;
        if (a > Math.PI) a -= TAU;
        else if (a < -Math.PI) a += TAU;
        return a;
    }

    // ─── LOCKED color scheme: curated NMS-style duotone/tritone palettes ─
    // Ten hand-picked palettes, each a small world-recipe: two terrain
    // tones (hi/lo), a contrasting sea/band color, an atmosphere glow
    // (often deliberately CONTRASTING with the terrain — red world, cyan
    // halo) and a bright accent for speckle/rings. Saturated but SOFT —
    // pastel-leaning, never pure RGB primaries, never muddy dark. A node
    // picks its palette deterministically from its id; children inherit
    // the parent's palette FAMILY but re-roll hue/detail variation from
    // their own id, so each subsystem reads coherent yet every body is
    // distinct. This is the permanent look going forward.
    var PALETTES = [
        { name: 'coral-sea',   hi: 0xF4A28C, lo: 0xD96D5A, sea: 0x2E8C8C, atmo: 0x7DE8F0, acc: 0xFFE3C8 },
        { name: 'gilded',      hi: 0xE8B84B, lo: 0xB7852F, sea: 0x6D4FA3, atmo: 0xF26BD8, acc: 0xFFE9A8 },
        { name: 'mint-rose',   hi: 0x9FE6C5, lo: 0x5FBF9A, sea: 0xB5496B, atmo: 0xFFA7C4, acc: 0xEAFFF2 },
        { name: 'tide-fire',   hi: 0xE08B4F, lo: 0xB25A33, sea: 0x2FB3A8, atmo: 0x63F2DC, acc: 0xFFC894 },
        { name: 'lav-heath',   hi: 0xB9A5E3, lo: 0x8B6FC2, sea: 0x7FA893, atmo: 0xD9B8FF, acc: 0xF2E8FF },
        { name: 'ember-ice',   hi: 0xD6455C, lo: 0xA32B44, sea: 0x3E6C99, atmo: 0xA8D8FF, acc: 0xFF9AA8 },
        { name: 'absinthe',    hi: 0xC4E060, lo: 0x8FB03A, sea: 0x7A4A8C, atmo: 0xE0A8F2, acc: 0xF4FFB8 },
        { name: 'orchard',     hi: 0xF2B98F, lo: 0xDB8E63, sea: 0x2F8C66, atmo: 0x8FF2C2, acc: 0xFFE0C2 },
        { name: 'dune-sky',    hi: 0xE3C08F, lo: 0xC29455, sea: 0x3573B5, atmo: 0x7FB8FF, acc: 0xFFEFC4 },
        { name: 'rose-quartz', hi: 0xE8A8B8, lo: 0xC97A94, sea: 0x5C6FA8, atmo: 0x93A8F2, acc: 0xFFD8E8 }
    ];

    // one consistent, fixed world-space light direction for every planet
    // shader — stylized in-shader lighting, deliberately NOT physical.
    var LIGHT_DIR = new THREE.Vector3(-0.62, 0.52, 0.4).normalize();

    // ─── three.js state ────────────────────────────────────────
    var renderer, scene, camera;
    var composer = null, bloomPass = null;  // post chain (null until addons load)
    var bgCanvas, bgCtx, fxCanvas, fxCtx;   // 2D backdrop / foreground fx layers
    var labelsEl, crumbEl, homeBtn;
    var W = 0, H = 0, DPR = 1;
    var availX = 0, availY = 0, stretchX = 1, stretchY = 1;
    var time = 0, lastTs = 0, frame = 0;
    var FOV = 42, fovRad = FOV * Math.PI / 180;

    var root = null;
    var byId = {};
    var byRoute = {};
    var drawOrder = [];             // every non-root node, depth-first

    var focus = null;
    var trans = null;               // {from, to, t, dFrom, dTo}
    var camDist = 1000;             // current camera distance from target

    // orbit-camera angles — full 360° in both, tracked as independent state
    // (never re-derived from a matrix/quaternion, so there is no gimbal-lock
    // *interpretation* problem — see buildCameraTransform()).
    var PITCH_REST = 0.62;
    var yaw = 0, pitch = PITCH_REST;
    var yawV = 0, pitchV = 0;
    var ROT_V_MAX = 8;

    var intro = null;
    var overlayNode = null;
    var suppressHash = false;
    var lastFocusedEl = null;

    // shared geometry/texture pools — reused across every body of the same
    // LOD tier so we never allocate per-frame or per-duplicate-tier GPU data
    var SPHERE_GEOM = {};           // tier(segments) -> BufferGeometry
    var RING_TEX = null;            // one shared radial-alpha ring texture
    var GLOW_TEX = null;            // one shared radial glow texture
    var orbitLinePool = {};         // parentId -> THREE.Line (built lazily)

    // ─── data loading (unchanged contract) ────────────────────
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

            var maxHalf = 0;
            for (i = 0; i < N; i++) {
                k = kids[i];
                var t = N === 1 ? 0.6 : i / (N - 1);
                k.orbF = ORBIT_MIN + (ORBIT_MAX - ORBIT_MIN) * (t * 0.75 + t * t * 0.25);
                k._half = (kBody * k.sizeF * boostEff) / (0.73 * node.sysR);
                if (k._half > maxHalf) maxHalf = k._half;
            }
            for (i = 0; i < N; i++) {
                k = kids[i];
                var rng = mulberry32(hash32(k.id));
                k.homeA = sysRot + (i / N) * TAU;
                var depthCap = Math.max(0.015, 0.5 * (minGap - (maxHalf + k._half) * 1.15));
                k.vibDepth = Math.min(minGap * (0.08 + rng() * 0.1), depthCap);
                k.vibF = 0.35 + rng() * 0.45;
                k.vibPh = rng() * TAU;
                seedIdentity(k, rng);
                walk(k);
            }
        })(root);
    }

    // ─── locked color + identity seeding ───────────────────────
    // Palette selection is deterministic and *inherited down the tree*:
    // top-level nodes (children of root) roll a palette from their own id;
    // every descendant inherits its parent's palette FAMILY unchanged, but
    // re-rolls hue jitter + all surface/detail seeds from its own id — so
    // a subsystem reads as one coherent world-family while every body in
    // it stays distinct. This rule is permanent (see report).
    function familyIndexFor(node) {
        if (node._famIdx !== undefined) return node._famIdx;
        var idx;
        if (node.parentNode === root || !node.parentNode) {
            var rng = mulberry32(hash32(node.id + '::family'));
            idx = Math.floor(rng() * PALETTES.length);
        } else {
            idx = familyIndexFor(node.parentNode);
        }
        node._famIdx = idx;
        return idx;
    }

    // small seeded hue/lightness wobble around a palette color — enough
    // that siblings sharing a palette never render bit-identical, small
    // enough that the family read survives.
    function jitterColor(hex, rng, hAmt, lAmt) {
        var c = new THREE.Color(hex);
        var o = { h: 0, s: 0, l: 0 };
        c.getHSL(o);
        c.setHSL(
            ((o.h + (rng() - 0.5) * hAmt) % 1 + 1) % 1,
            clamp(o.s + (rng() - 0.5) * 0.08, 0, 1),
            clamp(o.l + (rng() - 0.5) * lAmt, 0.08, 0.92));
        return c;
    }

    function seedIdentity(node, rng) {
        var famIdx = familyIndexFor(node);
        var fam = PALETTES[famIdx];
        node.famIdx = famIdx;
        // per-node tint stream (own id) — inherits the FAMILY, re-rolls
        // the exact hues, per the coherent-but-distinct subsystem rule.
        var crng = mulberry32(hash32(node.id + '::tint'));
        node.pal = {
            hi: jitterColor(fam.hi, crng, 0.05, 0.10),
            lo: jitterColor(fam.lo, crng, 0.05, 0.08),
            sea: jitterColor(fam.sea, crng, 0.05, 0.08),
            atmo: jitterColor(fam.atmo, crng, 0.03, 0.06),
            acc: jitterColor(fam.acc, crng, 0.04, 0.06)
        };
        node.pal.ring = node.pal.atmo.clone().lerp(new THREE.Color(0xffffff), 0.35);
        node.hasRing = rng() < 0.42;
        node.ringAngle = (rng() - 0.5) * 0.9;      // radial rotation of the ring plane
        node.ringTilt = 0.22 + rng() * 0.2;         // tilt off the orbital plane
        node.pulseRate = 0.4 + rng() * 1.1;
        node.pulsePhase = rng() * TAU;
        // slow self-rotation (axial spin) — every body spins gently on its
        // own axis, seeded so the rate/phase/tilt is a permanent per-node
        // trait rather than random noise each session.
        node.spinRate = 0.045 + rng() * 0.09;
        node.spinPhase = rng() * TAU;
        node.spinTilt = (rng() - 0.5) * 0.5;
        node.spriteSeed = hash32(node.id + '::surface');
        // shader-surface seeds (own stream so tweaking one knob never
        // reshuffles the others): biome mode, noise domain offset, scale,
        // domain-warp strength, gas band count, sea level, accent speckle.
        var srng = mulberry32(node.spriteSeed);
        node.biome = srng() < 0.38 ? 'gas' : 'terra';
        node.shSeed = new THREE.Vector3(srng() * 43.0, srng() * 43.0, srng() * 43.0);
        node.shFreq = 2.1 + srng() * 1.7;
        node.shWarp = 0.55 + srng() * 0.95;
        node.shBands = 5.0 + srng() * 8.0;
        node.shSea = 0.43 + srng() * 0.12;
        node.shSpeckle = srng() < 0.3 ? 0.4 + srng() * 0.6 : 0.0;
        // geometry LOD tier from navigational depth — shallow nodes are
        // seen large & close, deep leaves small, so coarser is plenty.
        node.geomTier = node.depth <= 1 ? 48 : (node.depth === 2 ? 32 : 20);
    }

    // ─── planet surface + atmosphere shaders (the NMS look) ────────────
    // One ShaderMaterial per planet (~60 nodes — trivial). Fragment does
    // 4-octave value-noise fbm with domain warping over the OBJECT-space
    // sphere position (seeded offset per planet, so the pattern is welded
    // to the surface and spins with the body), split into two biome modes:
    // terra (chunky smoothstepped continents over a contrasting sea) and
    // gas (flowing warped latitude bands). Lighting is stylized in-shader
    // wrap diffuse — high-key like NMS, night side only mildly darkened —
    // plus an inner Fresnel limb tint in the atmosphere color.
    var PLANET_VERT = [
        'varying vec3 vObjPos;',
        'varying vec3 vWorldNormal;',
        'varying vec3 vViewDir;',
        'void main() {',
        '    vObjPos = position;',
        '    vec4 wp = modelMatrix * vec4(position, 1.0);',
        '    vWorldNormal = normalize(mat3(modelMatrix) * normal);',
        '    vViewDir = cameraPosition - wp.xyz;',
        '    gl_Position = projectionMatrix * viewMatrix * wp;',
        '}'
    ].join('\n');

    var PLANET_NOISE = [
        'float nhash(vec3 p) {',
        '    p = fract(p * 0.3183099 + 0.1);',
        '    p *= 17.0;',
        '    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));',
        '}',
        'float vnoise(vec3 x) {',
        '    vec3 i = floor(x);',
        '    vec3 f = fract(x);',
        '    f = f * f * (3.0 - 2.0 * f);',
        '    return mix(mix(mix(nhash(i + vec3(0.0, 0.0, 0.0)), nhash(i + vec3(1.0, 0.0, 0.0)), f.x),',
        '                   mix(nhash(i + vec3(0.0, 1.0, 0.0)), nhash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),',
        '               mix(mix(nhash(i + vec3(0.0, 0.0, 1.0)), nhash(i + vec3(1.0, 0.0, 1.0)), f.x),',
        '                   mix(nhash(i + vec3(0.0, 1.0, 1.0)), nhash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);',
        '}',
        'float fbm(vec3 p) {',
        '    float v = 0.0;',
        '    float a = 0.5;',
        '    for (int i = 0; i < 4; i++) {',
        '        v += a * vnoise(p);',
        '        p = p * 2.07 + vec3(11.3, 7.9, 5.1);',
        '        a *= 0.5;',
        '    }',
        '    return v;',
        '}'
    ].join('\n');

    var PLANET_FRAG = [
        'uniform vec3 uSeed;',
        'uniform vec3 uColHigh;',
        'uniform vec3 uColLow;',
        'uniform vec3 uColSea;',
        'uniform vec3 uAtmo;',
        'uniform vec3 uAccent;',
        'uniform float uBiome;',
        'uniform float uFreq;',
        'uniform float uWarp;',
        'uniform float uBandFreq;',
        'uniform float uSeaLevel;',
        'uniform float uSpeckle;',
        'uniform vec3 uLightDir;',
        'varying vec3 vObjPos;',
        'varying vec3 vWorldNormal;',
        'varying vec3 vViewDir;',
        PLANET_NOISE,
        'void main() {',
        '    vec3 sp = normalize(vObjPos);',
        '    vec3 p = sp * uFreq + uSeed;',
        // domain warp field — this is what makes both biomes look
        // "grown", flowing/organic instead of raw noise blotches
        '    vec3 q = vec3(fbm(p + vec3(0.0, 3.1, 1.7)),',
        '                  fbm(p + vec3(5.2, 1.3, 2.8)),',
        '                  fbm(p + vec3(1.7, 9.2, 4.6)));',
        '    vec3 col;',
        '    if (uBiome < 0.5) {',
        // terra: fbm smoothstepped into discrete sea / lowland / highland
        // zones (chunky NMS landmasses), fine octave breaking the edges
        '        float n = fbm(p + (q - 0.5) * uWarp);',
        '        n += (fbm(p * 3.7 + q * 2.0) - 0.5) * 0.22;',
        '        float land = smoothstep(uSeaLevel - 0.035, uSeaLevel + 0.035, n);',
        '        float high = smoothstep(uSeaLevel + 0.12, uSeaLevel + 0.2, n);',
        '        col = mix(uColSea, uColLow, land);',
        '        col = mix(col, uColHigh, high);',
        '        col += (fbm(p * 6.3) - 0.5) * 0.07;',
        '    } else {',
        // gas: latitude bands warped by the same field, plus storm patches
        '        float lat = sp.y + (q.x - 0.5) * uWarp * 0.45 + (fbm(p * 2.3) - 0.5) * 0.3;',
        '        float s = sin(lat * uBandFreq + q.y * 2.4);',
        '        float band = smoothstep(-0.55, 0.55, s);',
        '        float storm = smoothstep(0.58, 0.8, fbm(p * 2.6 + q));',
        '        col = mix(uColLow, uColHigh, band);',
        '        col = mix(col, uColSea, storm * 0.65);',
        '        col += (fbm(p * 5.1) - 0.5) * 0.05;',
        '    }',
        // sparse glowing accent speckle on a seeded minority of worlds
        '    if (uSpeckle > 0.0) {',
        '        float spk = step(1.0 - uSpeckle * 0.012, vnoise(p * 26.0));',
        '        col += uAccent * spk * 1.7;',
        '    }',
        // stylized wrap lighting: bright and readable everywhere, night
        // side only mildly darkened, soft terminator — NOT physical
        '    vec3 N = normalize(vWorldNormal);',
        '    float ndl = dot(N, uLightDir) * 0.5 + 0.5;',
        '    float lightAmt = mix(0.24, 0.96, smoothstep(0.08, 0.92, ndl));',
        '    col *= lightAmt;',
        // inner Fresnel — the limb brightens toward the atmosphere color
        // (tight and modest now — a glowing edge, not a washed-out planet)
        '    vec3 V = normalize(vViewDir);',
        '    float fr = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 3.6);',
        '    col += uAtmo * fr * 0.3;',
        '    gl_FragColor = vec4(col, 1.0);',
        '    #include <tonemapping_fragment>',
        '    #include <colorspace_fragment>',
        '}'
    ].join('\n');

    // atmosphere halo: a slightly larger BackSide shell. Back faces behind
    // the planet are depth-occluded by the opaque planet mesh, so only the
    // thin annulus outside the limb survives — where -dot(N, V) runs from
    // ~0.36 at the planet's limb down to 0 at the shell's own silhouette.
    // Remapped + pow'd, that's a soft additive glow hugging the limb that
    // decays to nothing — no hard outline, no giant fog ball.
    var ATMO_FRAG = [
        'uniform vec3 uAtmo;',
        'uniform float uAlpha;',
        'varying vec3 vObjPos;',
        'varying vec3 vWorldNormal;',
        'varying vec3 vViewDir;',
        'void main() {',
        '    vec3 N = normalize(vWorldNormal);',
        '    vec3 V = normalize(vViewDir);',
        '    float d = clamp(-dot(N, V) * 2.9, 0.0, 1.0);',
        '    float glow = pow(d, 1.7);',
        '    gl_FragColor = vec4(uAtmo * glow * uAlpha, 1.0);',
        '    #include <tonemapping_fragment>',
        '    #include <colorspace_fragment>',
        '}'
    ].join('\n');

    function makePlanetMaterial(node) {
        return new THREE.ShaderMaterial({
            uniforms: {
                uSeed: { value: node.shSeed },
                uColHigh: { value: node.pal.hi },
                uColLow: { value: node.pal.lo },
                uColSea: { value: node.pal.sea },
                uAtmo: { value: node.pal.atmo },
                uAccent: { value: node.pal.acc },
                uBiome: { value: node.biome === 'gas' ? 1 : 0 },
                uFreq: { value: node.shFreq },
                uWarp: { value: node.shWarp },
                uBandFreq: { value: node.shBands },
                uSeaLevel: { value: node.shSea },
                uSpeckle: { value: node.shSpeckle },
                uLightDir: { value: LIGHT_DIR }
            },
            vertexShader: PLANET_VERT,
            fragmentShader: PLANET_FRAG,
            // tone mapping is handled once, at output time (OutputPass when
            // bloom is live, renderer otherwise) — never inside materials,
            // so the two render paths can't diverge or double-map.
            toneMapped: false
        });
    }

    function makeAtmoMaterial(node) {
        return new THREE.ShaderMaterial({
            uniforms: {
                uAtmo: { value: node.pal.atmo },
                uAlpha: { value: 0.7 }
            },
            vertexShader: PLANET_VERT,
            fragmentShader: ATMO_FRAG,
            side: THREE.BackSide,
            transparent: true,
            blending: THREE.AdditiveBlending,
            depthWrite: false,
            toneMapped: false
        });
    }

    function sphereGeom(tier) {
        var key = tier;
        if (!SPHERE_GEOM[key]) {
            SPHERE_GEOM[key] = new THREE.SphereGeometry(1, tier, Math.round(tier * 0.7));
        }
        return SPHERE_GEOM[key];
    }

    function buildRingTexture() {
        // three soft concentric bands separated by real gaps, baked as a
        // white radial-alpha gradient and tinted per planet — reads as a
        // translucent pastel ring system, not a single glowing washer.
        var S = 256;
        var c = document.createElement('canvas');
        c.width = S; c.height = S;
        var g = c.getContext('2d');
        var grad = g.createRadialGradient(S / 2, S / 2, S * 0.30, S / 2, S / 2, S * 0.5);
        grad.addColorStop(0.00, 'rgba(255,255,255,0)');
        grad.addColorStop(0.06, 'rgba(255,255,255,0.5)');
        grad.addColorStop(0.22, 'rgba(255,255,255,0.34)');
        grad.addColorStop(0.30, 'rgba(255,255,255,0.04)');   // gap
        grad.addColorStop(0.38, 'rgba(255,255,255,0.55)');
        grad.addColorStop(0.58, 'rgba(255,255,255,0.4)');
        grad.addColorStop(0.66, 'rgba(255,255,255,0.03)');   // gap
        grad.addColorStop(0.74, 'rgba(255,255,255,0.3)');
        grad.addColorStop(0.9, 'rgba(255,255,255,0.16)');
        grad.addColorStop(1.00, 'rgba(255,255,255,0)');
        g.fillStyle = grad;
        g.fillRect(0, 0, S, S);
        var tex = new THREE.CanvasTexture(c);
        return tex;
    }

    function buildGlowTexture() {
        var S = 128;
        var c = document.createElement('canvas');
        c.width = S; c.height = S;
        var g = c.getContext('2d');
        var grad = g.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
        grad.addColorStop(0, 'rgba(255,255,255,0.9)');
        grad.addColorStop(0.35, 'rgba(255,255,255,0.28)');
        grad.addColorStop(1, 'rgba(255,255,255,0)');
        g.fillStyle = grad;
        g.fillRect(0, 0, S, S);
        var tex = new THREE.CanvasTexture(c);
        return tex;
    }

    // ─── per-node three.js objects: one Object3D "anchor" at the node's
    // true absolute world position, holding a real sphere mesh, an
    // optional ring mesh (its own child, so ring tilt/rotation composes
    // with the anchor transform automatically — real 3D, no hacks), and a
    // camera-facing glow sprite. All parented directly under `scene`, in
    // one shared coordinate space with the black hole and everything else. ─
    function buildBodyObjects(node) {
        var anchor = new THREE.Object3D();
        anchor.name = node.id;

        var mat = makePlanetMaterial(node);
        var mesh = new THREE.Mesh(sphereGeom(node.geomTier), mat);
        mesh.scale.setScalar(node.bodyR);
        anchor.add(mesh);
        node.mesh = mesh;

        // atmosphere halo shell — a child of the planet mesh, so the
        // per-frame rendered-size scaling in updateBodies() (and the spin)
        // carries it automatically. Slightly larger, BackSide, additive.
        var atmoMat = makeAtmoMaterial(node);
        var atmo = new THREE.Mesh(sphereGeom(node.geomTier), atmoMat);
        atmo.scale.setScalar(1.07);
        mesh.add(atmo);
        node.atmoMat = atmoMat;

        if (node.hasRing) {
            var ringGeom = new THREE.RingGeometry(1.4, 1.9, 48);
            // RingGeometry is built in the XY plane; rotate it flat (XZ,
            // matching the orbital plane) then apply the seeded tilt/angle.
            var ringMat = new THREE.MeshBasicMaterial({
                map: RING_TEX,
                color: node.pal.ring,
                transparent: true,
                opacity: 0.3,
                side: THREE.DoubleSide,
                blending: THREE.AdditiveBlending,
                depthWrite: false
            });
            var ring = new THREE.Mesh(ringGeom, ringMat);
            ring.rotation.x = Math.PI / 2 + node.ringTilt;
            ring.rotation.z = node.ringAngle;
            ring.scale.setScalar(node.bodyR);
            anchor.add(ring);
            node.ringMesh = ring;
        }

        var glowMat = new THREE.SpriteMaterial({
            map: GLOW_TEX,
            color: node.pal.atmo,
            transparent: true,
            opacity: 0.14,
            depthWrite: false,
            blending: THREE.AdditiveBlending
        });
        var glow = new THREE.Sprite(glowMat);
        glow.scale.setScalar(node.bodyR * 2.2);
        anchor.add(glow);
        node.glowSprite = glow;

        node.anchor = anchor;
        scene.add(anchor);
    }

    // ─── black hole (also a real Object3D at true origin, same scene) ──
    var blackHole = null, accretionMesh = null, accretionMesh2 = null, photonRing = null;
    var bhCoreR = 0;   // real world radius of the black hole core (set in buildBlackHole)
    // The black hole is a small "solar system" of its own real 3D pieces —
    // a horizon sphere, three accretion disks at DIFFERENT tilts (not all
    // coplanar, so it doesn't read as a flat 2D ring seen edge-on/face-on),
    // a puffy volumetric particle swarm (real depth in every direction, not
    // just in-plane), a photon-ring torus, and a big soft omnidirectional
    // glow sprite — together giving it genuine 3D bulk from any camera angle.
    var accretionMesh3 = null, accretionParticles = null, haloSprite = null;

    // buckles a flat RingGeometry into a turbulent, non-planar disk — layered
    // sine displacement along its own normal, seeded per ring, so it reads as
    // real volume (a churning sheet of matter) from any camera angle instead
    // of a perfect flat 2D annulus.
    function warpRingGeometry(geom, amp, seed) {
        var pos = geom.attributes.position;
        var rng = mulberry32(hash32(seed));
        var f1 = 3 + Math.floor(rng() * 3), f2 = 5 + Math.floor(rng() * 5);
        var p1 = rng() * TAU, p2 = rng() * TAU;
        for (var i = 0; i < pos.count; i++) {
            var x = pos.getX(i), y = pos.getY(i);
            var r = Math.sqrt(x * x + y * y);
            var a = Math.atan2(y, x);
            var z = (Math.sin(a * f1 + p1) * 0.6 + Math.sin(a * f2 + r * 0.02 + p2) * 0.4) * amp;
            pos.setZ(i, z);
        }
        pos.needsUpdate = true;
        geom.computeVertexNormals();
        return geom;
    }

    // roughs up a TorusGeometry's tube along its own normal, seeded — turns
    // a perfect CAD-smooth ring into an irregular, faintly turbulent band of
    // plasma (used for the photon ring, which otherwise reads as a flat,
    // too-crisp 2D outline right at the horizon).
    function warpTorusGeometry(geom, amp, seed) {
        var pos = geom.attributes.position;
        var norm = geom.attributes.normal;
        var rng = mulberry32(hash32(seed));
        var f1 = 5 + Math.floor(rng() * 4), f2 = 9 + Math.floor(rng() * 6);
        var p1 = rng() * TAU, p2 = rng() * TAU;
        for (var i = 0; i < pos.count; i++) {
            var x = pos.getX(i), z = pos.getZ(i);
            var a = Math.atan2(z, x);
            var n = Math.sin(a * f1 + p1) * 0.6 + Math.sin(a * f2 + p2) * 0.4;
            pos.setXYZ(i, x + norm.getX(i) * n * amp,
                pos.getY(i) + norm.getY(i) * n * amp, z + norm.getZ(i) * n * amp);
        }
        pos.needsUpdate = true;
        geom.computeVertexNormals();
        return geom;
    }

    function buildBlackHole() {
        blackHole = new THREE.Object3D();
        blackHole.name = 'emgor';

        var core = ROOT_SYS_R * 0.14;      // bigger, more commanding presence
        bhCoreR = core;

        var horizonGeom = new THREE.SphereGeometry(core, 40, 28);
        var horizonMat = new THREE.MeshBasicMaterial({ color: 0x030008, transparent: true });
        var horizon = new THREE.Mesh(horizonGeom, horizonMat);
        blackHole.add(horizon);

        var accTex = buildAccretionTexture();
        var diskGeom = warpRingGeometry(
            new THREE.RingGeometry(core * 1.08, core * 3.2, 128, 3), core * 0.07, 'emgor::disk1');
        var diskMat = new THREE.MeshBasicMaterial({
            map: accTex, transparent: true, opacity: 0.6, side: THREE.DoubleSide,
            blending: THREE.AdditiveBlending, depthWrite: false
        });
        accretionMesh = new THREE.Mesh(diskGeom, diskMat);
        accretionMesh.rotation.x = Math.PI / 2;   // primary disk: flat in the orbital (XZ) plane
        blackHole.add(accretionMesh);

        // secondary + tertiary disks precess at different FIXED tilts off
        // the main plane (each in its own pivot group) while independently
        // spinning about their own local normal — this is what reads as
        // genuine 3D structure instead of a single flat ring, since no two
        // disks share an orientation and each still visibly turns. Each is
        // also buckled off-plane (warpRingGeometry) so it never reads as a
        // flat 2D annulus even face-on — it's a churning volume of matter.
        var disk2Mat = diskMat.clone();
        disk2Mat.opacity = 0.28;
        var disk2Geom = warpRingGeometry(
            new THREE.RingGeometry(core * 0.55, core * 1.5, 96, 2), core * 0.1, 'emgor::disk2');
        accretionMesh2 = new THREE.Mesh(disk2Geom, disk2Mat);
        var pivot2 = new THREE.Object3D();
        pivot2.rotation.set(Math.PI / 2 + 0.62, 0, 0.9);
        pivot2.add(accretionMesh2);
        blackHole.add(pivot2);

        var disk3Mat = diskMat.clone();
        disk3Mat.opacity = 0.2;
        var disk3Geom = warpRingGeometry(
            new THREE.RingGeometry(core * 1.6, core * 4.4, 112, 2), core * 0.14, 'emgor::disk3');
        accretionMesh3 = new THREE.Mesh(disk3Geom, disk3Mat);
        var pivot3 = new THREE.Object3D();
        pivot3.rotation.set(Math.PI / 2 - 0.4, 0, -1.3);
        pivot3.add(accretionMesh3);
        blackHole.add(pivot3);

        // photon ring: the tight, bright lensed edge right at the horizon —
        // deliberately warm violet rather than near-white so it reads as
        // superheated matter, not a flat pale outline
        var photonGeom = warpTorusGeometry(
            new THREE.TorusGeometry(core * 1.01, core * 0.045, 12, 96), core * 0.018, 'emgor::photon');
        var photonMat = new THREE.MeshBasicMaterial({ color: 0xb87cf0, transparent: true, opacity: 0.5 });
        photonRing = new THREE.Mesh(photonGeom, photonMat);
        photonRing.rotation.x = Math.PI / 2;
        blackHole.add(photonRing);

        accretionParticles = buildAccretionParticles(core);
        blackHole.add(accretionParticles);

        var haloMat = new THREE.SpriteMaterial({
            map: GLOW_TEX, color: 0x8a5fd9, transparent: true,
            opacity: 0.16, depthWrite: false, blending: THREE.AdditiveBlending
        });
        haloSprite = new THREE.Sprite(haloMat);
        haloSprite.scale.setScalar(core * 6.5);
        blackHole.add(haloSprite);

        scene.add(blackHole);
    }

    // Depth cueing for the black hole, NOT a hack around true 3D — its real
    // world position/size never change. This just fades its opacity like
    // atmospheric haze once it grows past a comfortable share of the view,
    // which happens legitimately when a true absolute-position camera flies
    // deep into a distant sub-system and ends up geometrically close to the
    // root. Without this the root would balloon and dominate every focused
    // system at depth, which reads as broken even though the geometry is
    // "correct." Applies uniformly to every material in the group.
    var BH_FADE_START = 0.16;   // angular-radius / (half-FOV) ratio where fade begins
    var BH_FADE_END = 0.34;     // ratio where it's fully faded out
    function updateBlackHoleFade() {
        if (!blackHole || !bhCoreR) return;
        var dist = camera.position.length();      // origin is always (0,0,0)
        var angR = Math.atan(bhCoreR / Math.max(1, dist));
        var ratio = angR / (fovRad / 2);
        var fade = 1 - clamp((ratio - BH_FADE_START) / (BH_FADE_END - BH_FADE_START), 0, 1);
        // smoothstep for a gentler transition than linear
        fade = fade * fade * (3 - 2 * fade);
        // Opacity alone doesn't work here: the horizon material is nearly
        // the same near-black as the void backdrop, so fading its alpha is
        // visually a no-op. Scale the whole group instead — this actually
        // shrinks its rendered footprint regardless of background color.
        // Its true world POSITION never moves or lies; only how large we
        // choose to render it does, exactly like an artist's-choice glow
        // radius, not a fake position. Floor above 0 so it never fully
        // vanishes — there's still a black hole out there, just small.
        var scale = 0.06 + 0.94 * fade;
        blackHole.scale.setScalar(scale);
        blackHole.traverse(function (obj) {
            if (!obj.material) return;
            var m = obj.material;
            if (m.userData.baseOpacity === undefined) m.userData.baseOpacity = m.opacity;
            m.opacity = m.userData.baseOpacity * (0.4 + 0.6 * fade);
            m.visible = true;
        });
    }

    // puffy, non-flat particle swarm around the disk — points are scattered
    // with vertical jitter that grows with radius (a torus with real
    // thickness, not a flat sheet), so from any camera angle — including
    // straight down the pole — the swarm still reads as a 3D volume, not a
    // 2D ring. Hot magenta-violet near the core fading to deep violet out —
    // deliberately no near-white, so it reads as superheated plasma, not fog.
    function buildAccretionParticles(core) {
        var N = 1500;
        var rng = mulberry32(hash32('emgor::accretion-particles'));
        var pos = new Float32Array(N * 3);
        var col = new Float32Array(N * 3);
        var hotColor = new THREE.Color(0xe8a8ff);
        var coolColor = new THREE.Color(0x4a1f8c);
        for (var i = 0; i < N; i++) {
            var a = rng() * TAU;
            var t = Math.pow(rng(), 0.7);                 // bias toward inner radii
            var r = core * (1.15 + t * 4.2);
            var thickness = core * (0.16 + t * 0.7);       // puffier further out
            var y = (rng() - 0.5) * 2 * thickness;
            pos[i * 3] = Math.cos(a) * r;
            pos[i * 3 + 1] = y;
            pos[i * 3 + 2] = Math.sin(a) * r;
            var c = hotColor.clone().lerp(coolColor, clamp(t, 0, 1));
            col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
        }
        var geom = new THREE.BufferGeometry();
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));
        var mat = new THREE.PointsMaterial({
            size: core * 0.055, vertexColors: true, transparent: true, opacity: 0.7,
            blending: THREE.AdditiveBlending, depthWrite: false, sizeAttenuation: true
        });
        return new THREE.Points(geom, mat);
    }

    function buildAccretionTexture() {
        var S = 512;
        var c = document.createElement('canvas');
        c.width = S; c.height = S;
        var g = c.getContext('2d');
        var cx = S / 2, cy = S / 2;
        var rng = mulberry32(hash32('emgor::accretion'));
        g.globalCompositeOperation = 'lighter';
        // hot core is rich magenta-violet, not white — a black hole's disk
        // reads as superheated plasma, never a pale/washed glow
        var gr = g.createRadialGradient(cx, cy, S * 0.06, cx, cy, S * 0.5);
        gr.addColorStop(0, 'rgba(203,110,255,0.5)');
        gr.addColorStop(0.22, 'rgba(150,70,230,0.28)');
        gr.addColorStop(0.5, 'rgba(105,40,170,0.12)');
        gr.addColorStop(1, 'rgba(50,22,120,0)');
        g.fillStyle = gr; g.fillRect(0, 0, S, S);
        for (var i = 0; i < 46; i++) {
            var a0 = rng() * TAU, len = 0.5 + rng() * 1.6, r = S * (0.09 + rng() * 0.3);
            var hue = rng() < 0.16 ? 188 : 268 + rng() * 30;
            g.strokeStyle = 'hsla(' + hue + ',85%,' + (52 + rng() * 20) + '%,' + (0.05 + rng() * 0.12) + ')';
            g.lineWidth = 1 + rng() * 3.2;
            g.beginPath(); g.arc(cx, cy, r, a0, a0 + len); g.stroke();
        }
        return new THREE.CanvasTexture(c);
    }

    // ─── background 2D layer (stars/nebula/vignette) — deliberately kept
    // screen-space & decoupled from the 3D camera, exactly like galaxy2d.js
    // ("stars — anchored background, fully decoupled from scene
    // rotation/zoom"). This is not a regression: it was always meant to be
    // an unrotated backdrop, not part of the navigable 3D scene. ─────────
    var starN = 0, starX, starY, starR, starPh, starW, starTw, starLayer, starTint;
    var nebulaSprites = [];
    var STAR_COLORS = ['rgba(235,225,255,', 'rgba(192,132,252,', 'rgba(110,235,255,'];

    function makeNebulaSprites() {
        nebulaSprites = [];
        var hues = [268, 288, 248, 195];
        for (var i = 0; i < hues.length; i++) {
            var S = 512;
            var c = document.createElement('canvas');
            c.width = c.height = S;
            var g = c.getContext('2d');
            var gr = g.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
            // moodier / subtler than before — the nebulae are an undertone
            // now; the bright planets carry the color in the new grade.
            gr.addColorStop(0, 'hsla(' + hues[i] + ',70%,34%,' + (i === 3 ? 0.03 : 0.06) + ')');
            gr.addColorStop(0.55, 'hsla(' + hues[i] + ',65%,24%,' + (i === 3 ? 0.015 : 0.03) + ')');
            gr.addColorStop(1, 'hsla(' + hues[i] + ',60%,20%,0)');
            g.fillStyle = gr; g.fillRect(0, 0, S, S);
            nebulaSprites.push(c);
        }
    }

    function makeStars() {
        var target = clamp(Math.floor((W * H) / 5200), 140, 460);
        starN = target;
        starX = new Float32Array(starN); starY = new Float32Array(starN);
        starR = new Float32Array(starN); starPh = new Float32Array(starN);
        starW = new Float32Array(starN); starTw = new Uint8Array(starN);
        starLayer = new Uint8Array(starN); starTint = new Uint8Array(starN);
        var rng = mulberry32(hash32('emgor::stars'));
        var cx = W / 2, cy = H / 2, clearR = Math.min(W, H) * 0.36;
        for (var i = 0; i < starN; i++) {
            var x = rng() * W, y = rng() * H, tries = 0;
            while (tries < 3 && Math.hypot(x - cx, y - cy) < clearR && rng() < 0.68) {
                x = rng() * W; y = rng() * H; tries++;
            }
            starX[i] = x; starY[i] = y;
            var l = rng();
            starLayer[i] = l < 0.5 ? 0 : (l < 0.83 ? 1 : 2);
            starR[i] = 0.4 + starLayer[i] * 0.45 + rng() * 0.7;
            starPh[i] = rng() * TAU;
            starW[i] = TAU / (6 + rng() * 14);
            starTw[i] = rng() < 0.3 ? 1 : 0;
            var t = rng();
            starTint[i] = t < 0.07 ? 2 : (t < 0.26 ? 1 : 0);
        }
    }

    function drawBackdrop() {
        bgCtx.fillStyle = VOID;
        bgCtx.fillRect(0, 0, W, H);
        bgCtx.globalCompositeOperation = 'lighter';
        var d = Math.max(W, H);
        for (var i = 0; i < nebulaSprites.length; i++) {
            var ph = time * 0.012 + i * 2.1;
            var bx = W * (0.18 + 0.64 * (0.5 + 0.5 * Math.sin(ph + i * 1.7)));
            var by = H * (0.2 + 0.6 * (0.5 + 0.5 * Math.cos(ph * 0.8 + i * 2.3)));
            var s = d * (0.7 + i * 0.22);
            bgCtx.drawImage(nebulaSprites[i], bx - s / 2, by - s / 2, s, s);
        }
        for (var j = 0; j < starN; j++) {
            var a;
            if (reducedMotion) { a = 0.4; }
            else {
                var sN = 0.5 + 0.5 * Math.sin(time * starW[j] + starPh[j]);
                a = Math.pow(sN, 1.6) * (0.55 + starLayer[j] * 0.15);
                if (starTw[j]) a *= 0.82 + 0.18 * Math.sin(time * 2.7 + starPh[j] * 3.1);
            }
            if (a <= 0.015) continue;
            bgCtx.fillStyle = STAR_COLORS[starTint[j]] + a.toFixed(3) + ')';
            var r = starR[j];
            bgCtx.fillRect(starX[j] - r, starY[j] - r, r * 2, r * 2);
        }
        bgCtx.globalCompositeOperation = 'source-over';
    }

    // ─── big-bang intro particles (drawn on the fx overlay layer, purely
    // decorative/screen-space; the real 3D bodies resolve outward
    // simultaneously via anchor position/scale lerp in updateBodies()) ──
    var PN = 420;
    var px = new Float32Array(PN), py = new Float32Array(PN);
    var pvx = new Float32Array(PN), pvy = new Float32Array(PN);
    var plife = new Float32Array(PN), phue = new Float32Array(PN);

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

    function drawIntroFx(dt) {
        var t = intro.t;
        fxCtx.clearRect(0, 0, W, H);
        fxCtx.globalCompositeOperation = 'lighter';
        if (t < 0.55) {
            var p = t / 0.55;
            var r = 8 + p * p * 110;
            var gr = fxCtx.createRadialGradient(W / 2, H / 2, 0, W / 2, H / 2, r);
            gr.addColorStop(0, 'rgba(255,245,255,' + (0.5 + p * 0.5) + ')');
            gr.addColorStop(0.4, 'rgba(192,132,252,' + (0.35 + p * 0.4) + ')');
            gr.addColorStop(1, 'rgba(123,47,190,0)');
            fxCtx.fillStyle = gr;
            fxCtx.beginPath(); fxCtx.arc(W / 2, H / 2, r, 0, TAU); fxCtx.fill();
        } else {
            if (!intro.seeded) { intro.seeded = true; seedParticles(); }
            var flash = clamp(1 - (t - 0.55) / 0.5, 0, 1);
            if (flash > 0) {
                fxCtx.fillStyle = 'rgba(224,170,255,' + (flash * flash * 0.5).toFixed(3) + ')';
                fxCtx.fillRect(0, 0, W, H);
            }
            var drag = Math.pow(0.5, dt * 2.2);
            for (var i = 0; i < PN; i++) {
                if (plife[i] <= 0) continue;
                plife[i] -= dt * 0.55;
                px[i] += pvx[i] * dt; py[i] += pvy[i] * dt;
                pvx[i] *= drag; pvy[i] *= drag;
                var a = clamp(plife[i], 0, 1) * 0.85;
                if (a <= 0.01) continue;
                fxCtx.fillStyle = 'hsla(' + phue[i] + ',95%,72%,' + a.toFixed(3) + ')';
                fxCtx.fillRect(px[i] - 1, py[i] - 1, 2.4, 2.4);
            }
        }
        fxCtx.globalCompositeOperation = 'source-over';

        var kids = root.kids;
        for (var k = 0; k < kids.length; k++) {
            kids[k].rev = easeOut(clamp((t - 0.75 - k * 0.09) / 0.7, 0, 1));
        }
        intro.t += dt;
        if (intro.t > 0.75 + kids.length * 0.09 + 0.9) {
            for (var k2 = 0; k2 < kids.length; k2++) kids[k2].rev = 1;
            intro = null;
            fxCtx.clearRect(0, 0, W, H);
        }
    }

    // vignette drawn once per resize onto the fx layer's background via CSS
    // radial-gradient (cheap, no per-frame canvas cost) instead of a canvas
    // draw — same visual effect, zero runtime overhead.
    function applyVignette() {
        fxCanvas.style.background =
            'radial-gradient(ellipse at center, rgba(6,1,18,0) 45%, rgba(2,0,8,0.6) 100%)';
    }

    // ─── world positions (per frame, ALL nodes, true absolute coords) ──
    // wx/wy map straight onto the world's X/Z (flat orbital plane, Y up) —
    // every node's position is always its true absolute place in the one
    // shared coordinate space. Nothing here depends on `focus`: the camera
    // is what moves through this space, not the space itself.
    function computePositions() {
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var p = n.parentNode;
            var a = n.homeA + time * BASE_ROT +
                Math.sin(time * n.vibF + n.vibPh) * n.vibDepth;
            var r = n.orbF * p.sysR;
            n.wx = p.wx + Math.cos(a) * r;
            n.wy = p.wy + Math.sin(a) * r;
        }
    }

    // ─── rendered size: mostly uniform, role-based, deliberately NOT the
    // true (very depth-dependent) bodyR. This only ever scales the
    // *rendered* mesh/ring/glow — the true anchor position (computePositions)
    // is untouched, so absolute orbital placement stays exact. Per Emory's
    // note: bodies should read as "almost the same size in general" within
    // a system, with only a gentle nudge from a node's own `size` override —
    // the real sense of scale/depth now comes from orbit motion, self-spin
    // and camera framing instead of a dramatic size hierarchy.
    var UNIFORM_MIX = 0.78;         // 0 = true hierarchy size, 1 = fully uniform by role
    var CHILD_ROLE = 0.072;         // nav-ring children — deliberately small vs. the focused "sun"
    function uniformSizeFor(n, f) {
        var role;
        if (n === f) role = 0.20;                                        // focused body ("sun")
        else if (n.parentNode === f) role = CHILD_ROLE;                  // nav-ring children — main tier
        else if (n.parentNode && n.parentNode.parentNode === f) role = 0.035; // hinted grandkids
        else if (f !== root && n === f.parentNode) role = 0.30;          // ambient background giant
        else if (f !== root && n.parentNode === f.parentNode) role = CHILD_ROLE; // siblings
        else role = n.bodyR / Math.max(1, f.sysR);
        var uniformR = f.sysR * role;
        var sizeFVar = clamp(0.85 + (n.sizeF - 1) * 0.15, 0.8, 1.2);     // gentle per-node variation only
        return lerp(n.bodyR, uniformR, UNIFORM_MIX) * sizeFVar;
    }
    function alphaFor(n, f) {
        if (n === f) return 0.95;
        if (n.parentNode === f) return 1;
        if (n.parentNode && n.parentNode.parentNode === f) return 0.55;
        if (f !== root && n === f.parentNode) return 0.3;
        if (f !== root && n.parentNode === f.parentNode) return 0.16;
        return 0;
    }

    function updateBodies() {
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var a = alphaFor(n, focus);
            if (trans) a = lerp(alphaFor(n, trans.from), a, easeInOut(clamp(trans.t, 0, 1)));
            var anc = n;
            while (anc.parentNode && anc.parentNode !== root) anc = anc.parentNode;
            a *= (intro ? anc.rev : 1);
            n.alpha = a;

            var targetR = trans
                ? lerp(uniformSizeFor(n, trans.from), uniformSizeFor(n, focus), easeInOut(clamp(trans.t, 0, 1)))
                : uniformSizeFor(n, focus);

            var visible = a > 0.01;
            n.anchor.visible = visible;
            if (!visible) continue;

            var wx = n.wx, wy = n.wy;
            var revScale = intro && anc !== root ? anc.rev : 1;
            if (intro && n.parentNode === root) {
                // resolve outward from the true origin during the big bang
                n.anchor.position.set(lerp(0, wx, n.rev), 0, lerp(0, wy, n.rev));
            } else {
                n.anchor.position.set(wx, 0, wy);
            }
            targetR *= revScale;
            n.mesh.scale.setScalar(targetR);
            if (n.ringMesh) n.ringMesh.scale.setScalar(targetR);
            if (n.glowSprite) n.glowSprite.scale.setScalar(targetR * 2.2);

            // slow axial self-rotation, on top of orbital motion — every
            // body spins gently in place, seeded per-node (rate/phase/tilt),
            // fully gated by prefers-reduced-motion via `time` itself.
            n.mesh.rotation.z = n.spinTilt;
            n.mesh.rotation.y = (reducedMotion ? 0 : time) * n.spinRate + n.spinPhase;

            var pulse = reducedMotion ? 0.5 : 0.5 + 0.5 * Math.sin(time * n.pulseRate + n.pulsePhase);
            n.glowSprite.material.opacity = clamp(a * (0.09 + pulse * 0.06), 0, 1);
            // atmosphere halo breathes very gently with the same pulse and
            // fades with the node's role alpha like every other adornment
            if (n.atmoMat) n.atmoMat.uniforms.uAlpha.value = clamp(a * (0.6 + pulse * 0.18), 0, 1);
            if (n.ringMesh) n.ringMesh.material.opacity = clamp(a * 0.32, 0, 1);
            n.mesh.visible = a > 0.04;
        }
    }

    // ─── camera: orbit rig built directly from (yaw, pitch, dist) with NO
    // lookAt()/up-vector step. We compose the camera's own rotation the
    // same way galaxy2d.js composed its rotation MATRIX — yaw about world
    // Y, then pitch about the *already-yawed* local X axis — using three's
    // Euler order 'YXZ', which performs exactly that composition. Because
    // yaw/pitch are angles WE track and only ever WRITE into this Euler
    // (never read back out of a matrix), there is no gimbal-lock
    // *ambiguity* — gimbal lock is a problem for extracting angles from an
    // orientation, not for constructing one from two known angles. This is
    // what makes full 360° in both axes, wrapping smoothly through the
    // poles, actually correct here (no lookAt()+up singularity at the
    // poles the way a naive OrbitControls-style camera would hit). ───────
    function buildCameraTransform(target, dist) {
        camera.rotation.order = 'YXZ';
        camera.rotation.set(pitch, yaw, 0);
        var back = new THREE.Vector3(0, 0, dist).applyEuler(camera.rotation);
        camera.position.set(target.x + back.x, target.y + back.y, target.z + back.z);
    }

    function targetScaleFor(node) {
        return Math.min(
            availX / (stretchX * node.sysR * ORBIT_MAX),
            availY / (stretchY * node.sysR * ORBIT_MAX));
    }
    // Convert the old engine's "pixels per world unit" framing scale into a
    // real perspective-camera distance that frames the same system: for a
    // vertical FOV f at distance d, pixels-per-world-unit = (H/2)/(d*tan(f/2)).
    // Solve for d.
    function distanceForScale(scaleVal) {
        return (H / 2) / (Math.max(1e-6, scaleVal) * Math.tan(fovRad / 2));
    }
    function distanceFor(node) { return distanceForScale(targetScaleFor(node)); }

    // ─── navigation (unchanged contract vs galaxy2d.js) ────────────────
    function focusTo(node, animate) {
        if (node === focus) return;
        if (animate && !reducedMotion) {
            trans = { from: focus, to: node, t: 0, dFrom: camDist, dTo: distanceFor(node) };
        } else {
            trans = null;
            camDist = distanceFor(node);
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
        location.hash = target;
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

    // ─── overlay (unchanged contract — DOM only, renderer-independent) ──
    var ovRoot, ovPanel, ovTitle, ovBlurb, ovMeta, ovDl, ovLinks, ovBody, ovClose;

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

    // ─── screen-space projection helper (true 3D world -> screen px) ───
    var _v3 = new THREE.Vector3();
    function project(wx, wy, wz) {
        _v3.set(wx, wy, wz).project(camera);
        return {
            x: (_v3.x * 0.5 + 0.5) * W,
            y: (-_v3.y * 0.5 + 0.5) * H,
            z: _v3.z,           // NDC depth: <1 in front of camera, roughly
            behind: _v3.z > 1
        };
    }
    // approximate on-screen pixel radius of a world-space sphere radius r
    // centered at (wx,wy,wz), for label sizing / hit-testing (no lookAt
    // dependency — pure perspective-projection math, correct from any angle)
    function projectRadius(wx, wy, wz, r) {
        var d = camera.position.distanceTo(new THREE.Vector3(wx, wy, wz));
        return (r / Math.max(1e-3, d * Math.tan(fovRad / 2))) * (H / 2);
    }

    function projectAllBodies() {
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var p = project(n.wx, 0, n.wy);
            n.sx = p.x; n.sy = p.y; n.sz = p.z; n.behind = p.behind;
            n.sr = projectRadius(n.wx, 0, n.wy, n.bodyR * (n.mesh ? n.mesh.scale.x / Math.max(1e-6, n.bodyR) : 1));
        }
        var rp = project(0, 0, 0);
        root.sx = rp.x; root.sy = rp.y;
    }

    // ─── labels (real DOM <a> elements, projected from true 3D world) ──
    var labelPool = {};
    function labelFor(node) {
        var el = labelPool[node.id];
        if (el) return el;
        el = document.createElement('a');
        el.className = 'planet-label';
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

    var labArr = [];
    function syncLabels() {
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
            if (n.behind) continue;
            var el = labelFor(n);
            el._keep = true;
            var a = alpha * (intro ? n.rev : 1);
            if (a <= 0.02) { el.style.display = 'none'; continue; }

            var fs = clamp(n.sr * 0.3, 13, 22);
            var lw = n.title.length * fs * 0.66 + 22;
            var lh = fs * 2.15;

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

    function layoutLabels() {
        var i, j, a, b;
        for (var it = 0; it < 2; it++) {
            for (i = 0; i < labArr.length; i++) {
                a = labArr[i];
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

    // ─── viewport ──────────────────────────────────────────────
    function resize() {
        W = window.innerWidth;
        H = window.innerHeight;
        DPR = Math.min(window.devicePixelRatio || 1, 2);

        availX = Math.max(120, W / 2 - MARGIN_X);
        availY = Math.max(120, H / 2 - MARGIN_Y);
        var base = Math.min(availX, availY);
        stretchX = clamp(availX / base, 1, 2.2);
        stretchY = clamp(availY / base, 1, 1.5);

        bgCanvas.width = Math.floor(W * DPR); bgCanvas.height = Math.floor(H * DPR);
        bgCanvas.style.width = W + 'px'; bgCanvas.style.height = H + 'px';
        bgCtx.setTransform(DPR, 0, 0, DPR, 0, 0);

        fxCanvas.width = Math.floor(W * DPR); fxCanvas.height = Math.floor(H * DPR);
        fxCanvas.style.width = W + 'px'; fxCanvas.style.height = H + 'px';
        fxCtx.setTransform(DPR, 0, 0, DPR, 0, 0);

        renderer.setPixelRatio(DPR);
        renderer.setSize(W, H, true);
        if (composer) {
            composer.setPixelRatio(DPR);
            composer.setSize(W, H);
        }
        camera.aspect = W / H;
        camera.fov = FOV;
        camera.updateProjectionMatrix();

        makeStars();
        applyVignette();
    }

    // ─── orbit-ring guides for the focused system (built lazily, one Line
    // per parent, reused; only visible for the current/previous focus) ──
    function orbitLineFor(parent) {
        if (orbitLinePool[parent.id]) return orbitLinePool[parent.id];
        var group = new THREE.Group();
        for (var i = 0; i < parent.kids.length; i++) {
            var k = parent.kids[i];
            var r = k.orbF * parent.sysR;
            var SEG = 72;
            var pts = [];
            for (var s = 0; s <= SEG; s++) {
                var th = (s / SEG) * TAU;
                pts.push(new THREE.Vector3(Math.cos(th) * r, 0, Math.sin(th) * r));
            }
            var geom = new THREE.BufferGeometry().setFromPoints(pts);
            var mat = new THREE.LineBasicMaterial({
                color: 0xa855f7, transparent: true, opacity: 0.14, depthWrite: false
            });
            var line = new THREE.Line(geom, mat);
            line.userData.baseOpacity = 0.14;
            group.add(line);
        }
        group.position.set(parent.wx, 0, parent.wy);
        scene.add(group);
        orbitLinePool[parent.id] = group;
        return group;
    }

    function updateOrbitRings() {
        var e = trans ? easeInOut(clamp(trans.t, 0, 1)) : 1;
        for (var id in orbitLinePool) orbitLinePool[id].visible = false;
        if (focus.kids.length) {
            var g = orbitLineFor(focus);
            g.visible = true;
            g.position.set(focus.wx, 0, focus.wy);
            setGroupOpacity(g, trans ? e : 1);
        }
        if (trans && trans.from.kids.length) {
            var g2 = orbitLineFor(trans.from);
            g2.visible = true;
            g2.position.set(trans.from.wx, 0, trans.from.wy);
            setGroupOpacity(g2, 1 - e);
        }
    }
    function setGroupOpacity(g, mult) {
        g.children.forEach(function (line) {
            line.material.opacity = line.userData.baseOpacity * mult;
        });
    }

    // ─── render loop ───────────────────────────────────────────
    function tick(ts) {
        requestAnimationFrame(tick);
        var dt = lastTs ? Math.min((ts - lastTs) / 1000, 0.05) : 0.016;
        lastTs = ts;
        frameStep(dt);
    }

    function frameStep(dt) {
        frame++;
        if (!reducedMotion) time += dt;

        computePositions();

        var targetVec;
        if (trans) {
            trans.t += dt / FLY_DUR;
            var e = easeInOut(clamp(trans.t, 0, 1));
            var tx = lerp(trans.from.wx, trans.to.wx, e);
            var tz = lerp(trans.from.wy, trans.to.wy, e);
            targetVec = new THREE.Vector3(tx, 0, tz);
            camDist = Math.exp(lerp(Math.log(trans.dFrom), Math.log(trans.dTo), e));
            if (trans.t >= 1) trans = null;
        } else {
            targetVec = new THREE.Vector3(focus.wx, 0, focus.wy);
            var tDist = distanceFor(focus);
            camDist += (tDist - camDist) * Math.min(1, dt * 5);
        }

        if (!dragging && !reducedMotion && (yawV !== 0 || pitchV !== 0)) {
            yaw = wrapAngle(yaw + yawV * dt);
            pitch = wrapAngle(pitch + pitchV * dt);
            var sfr = Math.pow(0.5, dt * 1.6);
            yawV *= sfr; pitchV *= sfr;
            if (Math.abs(yawV) + Math.abs(pitchV) < 0.01) { yawV = 0; pitchV = 0; }
        }

        buildCameraTransform(targetVec, camDist);
        updateBodies();
        updateOrbitRings();

        var bt = reducedMotion ? 0 : time;
        blackHole.rotation.y = bt * 0.03;
        if (accretionMesh) accretionMesh.rotation.z = bt * 0.12;
        if (accretionMesh2) accretionMesh2.rotation.z = -bt * 0.3;      // spins about its own tilted pivot
        if (accretionMesh3) accretionMesh3.rotation.z = bt * 0.19;
        if (accretionParticles) accretionParticles.rotation.y = bt * 0.045;
        if (photonRing) photonRing.material.opacity = 0.5 + 0.2 * Math.sin(bt * 2.3);
        if (haloSprite) haloSprite.material.opacity = 0.13 + 0.03 * Math.sin(bt * 0.9);
        updateBlackHoleFade();

        drawBackdrop();
        if (intro) drawIntroFx(dt);

        if (composer) composer.render();
        else renderer.render(scene, camera);

        projectAllBodies();
        syncLabels();
    }

    // ─── input ─────────────────────────────────────────────────
    var dragging = false;
    var pointers = {};
    var pinchStart = 0, pinchFired = false;
    var dragSX = 0, dragSY = 0, dragMoved = false, dragConsumed = false;
    var dragLX = 0, dragLY = 0, dragT = 0;

    function pointerCount() { var c = 0; for (var k in pointers) c++; return c; }
    function grabbable(t) {
        if (t === renderer.domElement) return true;
        return !!(t && t.closest && t.closest('.planet-label'));
    }

    function setupInput() {
        document.addEventListener('pointerdown', function (e) {
            if (!grabbable(e.target)) return;
            dragConsumed = false;
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
                var dx = e.clientX - dragLX, dy = e.clientY - dragLY;
                var dYaw = -dx * (4.6 / Math.max(320, W));
                var dPit = dy * (3.2 / Math.max(320, H));
                yaw = wrapAngle(yaw + dYaw);
                pitch = wrapAngle(pitch + dPit);
                var now = performance.now();
                var dts = Math.max(0.008, (now - dragT) / 1000);
                yawV = clamp(0.75 * (dYaw / dts) + 0.25 * yawV, -ROT_V_MAX, ROT_V_MAX);
                pitchV = clamp(0.75 * (dPit / dts) + 0.25 * pitchV, -ROT_V_MAX, ROT_V_MAX);
                dragLX = e.clientX; dragLY = e.clientY; dragT = now;
                if (Math.abs(e.clientX - dragSX) + Math.abs(e.clientY - dragSY) > 6) dragMoved = true;
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

        document.addEventListener('click', function (e) {
            if (dragConsumed) { e.preventDefault(); e.stopPropagation(); dragConsumed = false; }
        }, true);

        var wheelAcc = 0, wheelCooldown = 0;
        renderer.domElement.addEventListener('wheel', function (e) {
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

        renderer.domElement.addEventListener('click', function (e) {
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
    function startIntro() {
        intro = { t: 0, seeded: false };
        var kids = root.kids;
        for (var i = 0; i < kids.length; i++) kids[i].rev = 0;
    }

    function setupScene() {
        var canvas = document.getElementById('galaxy');

        bgCanvas = document.createElement('canvas');
        bgCanvas.style.cssText = 'position:fixed;inset:0;z-index:0;pointer-events:none;display:block;';
        canvas.parentNode.insertBefore(bgCanvas, canvas);
        bgCtx = bgCanvas.getContext('2d');

        renderer = new THREE.WebGLRenderer({
            canvas: canvas, alpha: true, antialias: true,
            logarithmicDepthBuffer: true, powerPreference: 'high-performance'
        });
        renderer.setClearColor(0x000000, 0);
        // filmic tonemapping compresses highlights instead of clipping them
        // flat — this is a large part of why real-time PBR reads as
        // "realistic" instead of "cartoony/plasticky" at these light levels.
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.toneMappingExposure = 1.15;
        renderer.outputColorSpace = THREE.SRGBColorSpace;

        fxCanvas = document.createElement('canvas');
        fxCanvas.style.cssText = 'position:fixed;inset:0;z-index:2;pointer-events:none;display:block;';
        canvas.parentNode.insertBefore(fxCanvas, canvas.nextSibling);
        fxCtx = fxCanvas.getContext('2d');

        scene = new THREE.Scene();
        camera = new THREE.PerspectiveCamera(FOV, 1, 0.05, 200000);

        // no scene lights: every planet is lit inside its own shader from
        // the one shared world-space LIGHT_DIR (stylized wrap diffuse, NMS
        // high-key), and the black hole is all Basic/emissive materials —
        // so real THREE lights would be dead weight here.

        RING_TEX = buildRingTexture();
        GLOW_TEX = buildGlowTexture();
        buildBlackHole();
        setupPost();
    }

    // ─── bloom post chain — loaded lazily from three's addons via the
    // importmap in index.html. Everything renders fine without it (the
    // additive atmosphere shells + glow sprites carry the look), so a CDN
    // hiccup or an old browser without importmap support just means "no
    // bloom", never a black page: frameStep falls back to a plain
    // renderer.render whenever `composer` is null. ─────────────────────
    function setupPost() {
        Promise.all([
            import('three/addons/postprocessing/EffectComposer.js'),
            import('three/addons/postprocessing/RenderPass.js'),
            import('three/addons/postprocessing/UnrealBloomPass.js'),
            import('three/addons/postprocessing/OutputPass.js')
        ]).then(function (mods) {
            var c = new mods[0].EffectComposer(renderer);
            c.setPixelRatio(DPR);
            c.setSize(W, H);
            c.addPass(new mods[1].RenderPass(scene, camera));
            // threshold high enough that only atmosphere limbs, accent
            // speckle and the accretion disk's hot core cross it — the
            // bloom is a soft breath on the bright bits, not a smear.
            bloomPass = new mods[2].UnrealBloomPass(new THREE.Vector2(W, H), 0.48, 0.45, 0.82);
            // UnrealBloomPass's final additive composite hard-codes alpha
            // 1.0 in its blur shaders, which would turn the whole canvas
            // opaque and hide the DOM star/nebula backdrop layered beneath
            // the WebGL canvas. Rewire that one blend to custom blending:
            // rgb stays ONE/ONE additive (identical bloom), alpha becomes
            // ZERO/ONE — i.e. "add the light, keep the scene's own
            // coverage" — so the void stays see-through to the stars.
            bloomPass.blendMaterial.blending = THREE.CustomBlending;
            bloomPass.blendMaterial.blendEquation = THREE.AddEquation;
            bloomPass.blendMaterial.blendSrc = THREE.OneFactor;
            bloomPass.blendMaterial.blendDst = THREE.OneFactor;
            bloomPass.blendMaterial.blendSrcAlpha = THREE.ZeroFactor;
            bloomPass.blendMaterial.blendDstAlpha = THREE.OneFactor;
            c.addPass(bloomPass);
            c.addPass(new mods[3].OutputPass());
            composer = c;
        }).catch(function (err) {
            composer = null;
            if (window.console) console.warn('galaxy3d: bloom addons unavailable, rendering direct', err);
        });
    }

    function boot() {
        labelsEl = document.getElementById('labels');
        crumbEl = document.getElementById('crumb');
        homeBtn = document.getElementById('homeBtn');
        var canvasEl = document.getElementById('galaxy');

        if (!canvasEl || !window.WebGLRenderingContext) {
            var fb = document.getElementById('fallback');
            if (fb) fb.hidden = false;
            return;
        }

        buildOverlay();
        warpEl = document.createElement('div');
        warpEl.className = 'warp';
        document.body.appendChild(warpEl);
        window.addEventListener('pageshow', function () { warpEl.classList.remove('is-on'); });

        setupScene();
        makeNebulaSprites();
        resize();
        window.addEventListener('resize', resize);

        loadData().then(function (data) {
            buildTree(data);
            drawOrder.forEach(buildBodyObjects);

            focus = root;
            camDist = distanceFor(root);
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
            if (window.console) console.error('galaxy3d: failed to load data', err);
        });
    }

    // ─── debug/inspection hook ─────────────────────────────────
    window.EMGOR_GALAXY = {
        nudge: function (s) { if (!reducedMotion) time += +s || 0; },
        setOrbit: function (y, p) {
            if (typeof y === 'number') yaw = wrapAngle(y);
            if (typeof p === 'number') pitch = wrapAngle(p);
            yawV = 0; pitchV = 0;
        },
        get state() {
            return {
                focus: focus && focus.id, kids: focus && focus.kids.length,
                cam: { x: focus && focus.wx, z: focus && focus.wy, dist: camDist, yaw: yaw, pitch: pitch },
                cameraPos: camera && { x: camera.position.x, y: camera.position.y, z: camera.position.z },
                intro: !!intro, trans: !!trans, frame: frame,
                overlay: overlayNode && overlayNode.id,
                bodyCount: drawOrder.length
            };
        },
        // manually advance one frame (camera + positions + render) without
        // waiting on requestAnimationFrame — useful when a tab is
        // backgrounded/throttled (e.g. headless automation) and rAF stalls.
        step: function (dt) { frameStep(+dt || 0.05); }
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', boot);
    } else {
        boot();
    }
})();
