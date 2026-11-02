/* galaxy-layout.js — pure (no THREE, no DOM) tree layout + pilot clearance math
   shared by galaxy3d.js and tests/node/pilot-clearance.test.mjs. Moved out of
   galaxy3d.js verbatim; galaxy3d.js imports it (one definition, no copy). */

export var ROOT_SYS_R = 1200;          // world radius of the root system
export var SHRINK = 0.2;               // child system radius = parent * SHRINK
export var BODY_F = 0.53;              // body radius = own system radius * BODY_F
export var ORBIT_MIN = 0.493, ORBIT_MAX = 0.85;
export var BH_CORE_BOOST = 1.3, BH_PILOT_SCALE = 8;
export var BH_CORE_R = ROOT_SYS_R * 0.14 * BH_CORE_BOOST;   // black hole core radius (world)
export var PILOT_CLEAR0 = 1.65;   // pilot orbit clearance (x parent rendered radius) at spread t=0, +0.55 t; keeps a 1.45R landing approach outside the parent's 1.6R sphere
var TAU = Math.PI * 2;
var SIZE_FALLBACK = { 'emgor.papers': 2.0, 'emgor.web-synth': 1.4 };

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function lerp(a, b, t) { return a + (b - a) * t; }
export function hash32(str) {
    var h = 2166136261;
    for (var i = 0; i < str.length; i++) {
        h ^= str.charCodeAt(i);
        h = Math.imul(h, 16777619);
    }
    return h >>> 0;
}
export function mulberry32(a) {
    return function () {
        a |= 0; a = (a + 0x6D2B79F5) | 0;
        var t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

// galaxy.json -> { root, byId, byRoute, drawOrder }. onKid(node, rng) runs at the
// exact point the per-node identity used to be seeded (rng stream order is a contract).
export function buildTree(data, onKid) {
    var root = {
        id: 'emgor', title: 'EMGOR', route: '/', depth: 0, kids: [],
        parentNode: null, sysR: ROOT_SYS_R, wx: 0, wy: 0, sizeF: 1
    };
    var byId = { emgor: root };
    var byRoute = { '/': root };

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
            read: typeof n.read === 'string' ? n.read : '',
            zip: typeof n.zip === 'string' ? n.zip : '',
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

    var drawOrder = [];
    (function walk(node) {
        if (node !== root) drawOrder.push(node);
        node.sysR = node === root ? ROOT_SYS_R : node.parentNode.sysR * SHRINK;
        node.bodyR = node.sysR * BODY_F * node.sizeF;
        var kids = node.kids;
        var N = kids.length;
        var sysRot = mulberry32(hash32(node.id + '::rot'))() * TAU;
        // world-unit gap between two adjacent (by orbit radius) siblings'
        // shells — caps the radial "breathing" wobble so it can never nudge
        // one orbit shell into its neighbour's.
        var orbitGapWorld = N > 1 ? ((ORBIT_MAX - ORBIT_MIN) / (N - 1)) * node.sysR : node.sysR * 0.1;
        var i, k;
        for (i = 0; i < N; i++) {
            k = kids[i];
            var t = N === 1 ? 0.6 : i / (N - 1);
            k.orbF = ORBIT_MIN + (ORBIT_MAX - ORBIT_MIN) * t;
            k.spreadT = t;   // even spread, not center-bunched
            var rng = mulberry32(hash32(k.id));
            k.homeA = sysRot + (i / N) * TAU;
            // RADIAL breathing only (never angular): see galaxy3d.js computePositions.
            var r = k.orbF * node.sysR;
            var maxRadAmp = Math.min(orbitGapWorld * 0.3, r * 0.06);
            k.vibDepth = (maxRadAmp / Math.max(1, r)) * (0.5 + rng() * 0.5);
            k.vibF = 0.35 + rng() * 0.45;
            k.vibPh = rng() * TAU;
            if (onKid) onKid(k, rng);
            walk(k);
        }
    })(root);
    return { root: root, byId: byId, byRoute: byRoute, drawOrder: drawOrder };
}

// ─── rendered size: mostly uniform, role-based (see galaxy3d.js for the history) ───
var UNIFORM_MIX = 0.7;          // 0 = true hierarchy size, 1 = fully uniform by role
var CHILD_ROLE = 0.133;         // nav-ring children when zoomed into a sub-system
var ROOT_CHILD_ROLE = 0.06;     // root-level planets around the black hole
var CROWD_C_ROOT = 0.07;
function crowdAdjRoot(n) { return 1 / (1 + Math.max(0, n - 5) * CROWD_C_ROOT); }
var CHILD_CROWD_K = 10;
function crowdAdjChild(n) { return CHILD_CROWD_K / (CHILD_CROWD_K + Math.max(0, n - 1)); }
var SUN_ROLE = 0.36;
export function uniformSizeFor(n, f, root) {
    if (n === f) return f.sysR * SUN_ROLE;
    var role, isChildTier = false, crowdN = 0, crowdFn = null;
    if (n.parentNode === f) {                                   // nav-ring children — main tier
        role = (f === root) ? ROOT_CHILD_ROLE : CHILD_ROLE;
        crowdN = f.kids.length;
        crowdFn = (f === root) ? crowdAdjRoot : crowdAdjChild;
        isChildTier = true;
    }
    else if (n.parentNode && n.parentNode.parentNode === f) role = 0.02; // hinted grandkids
    else if (f !== root && n === f.parentNode) role = 0.30;          // ambient background giant
    else if (f !== root && n.parentNode === f.parentNode) {          // siblings
        role = CHILD_ROLE;
        crowdN = f.parentNode.kids.length;
        crowdFn = crowdAdjChild;
        isChildTier = true;
    }
    else role = n.bodyR / Math.max(1, f.sysR);
    if (isChildTier) role *= crowdFn(crowdN);
    var uniformR = f.sysR * role;
    var sizeFVar = clamp(0.85 + (n.sizeF - 1) * 0.15, 0.8, 1.2);     // gentle per-node variation only
    var varMix = (n !== f && isChildTier) ? n.sizeVar : 1;
    return lerp(n.bodyR, uniformR, UNIFORM_MIX) * sizeFVar * varMix;
}

// pilot pre-pass: _pF (full-pilot rendered radius, children capped to 0.3x their
// inflated parent) top-down, then _ext (radius of the node's whole system incl. a
// landing margin) bottom-up. drawOrder is parent-first. Allocation-free.
export function pilotPrepass(drawOrder, root, scale, minR) {
    var i, n, p, base;
    for (i = 0; i < drawOrder.length; i++) {
        n = drawOrder[i]; p = n.parentNode;
        base = uniformSizeFor(n, p, root);
        n._pF = p === root ? base * scale.root : Math.min(base * scale.deep, 0.3 * p._pF);
        if (n._pF < minR) n._pF = minR;   // floor applied BEFORE _ext/orbit layout so nothing embeds
        n._ext = n._pF * 1.5;
    }
    for (i = drawOrder.length - 1; i >= 0; i--) {
        n = drawOrder[i]; p = n.parentNode;
        if (p === root) continue;
        var rk = Math.max(n.orbF * p.sysR * (1 + n.vibDepth), p._pF * (PILOT_CLEAR0 + 0.55 * (n.spreadT || 0)), p._pF + 1.15 * n._pF, n.kids.length ? 1.65 * p._pF + n._ext : 0) + n._ext;
        if (rk > p._ext) p._ext = rk;
    }
}

// pilot: whole root-level systems (planet + every moon shell) must not overlap each
// other, so spread the root ring radially just enough. Returns rootS (>= 1).
// orbAngle(n, pb) / orbVib(n, pb) are the engine's own time-dependent evaluators.
export function pilotRootSpread(root, pb, bhCoreR, orbAngle, orbVib) {
    var rootS = 1, need;
    var rk0 = root.kids, ia, ib, ka, kb, ra, dx0, dy0;
    for (ia = 0; ia < rk0.length; ia++) {
        ka = rk0[ia];
        ra = ka.orbF * root.sysR * (1 + orbVib(ka, pb) * ka.vibDepth);
        var ua = orbAngle(ka, pb);
        ka._ux = Math.cos(ua) * ra; ka._uy = Math.sin(ua) * ra;
        if (ka._ext < 1.6 * ka._pF) ka._ext = 1.6 * ka._pF;
        // rev 22: innermost orbit clears the x8 black hole + station (2.2 coreR) + dock margin
        need = (bhCoreR * BH_PILOT_SCALE * 3.2 + 1.6 * ka._pF) / Math.max(1, Math.sqrt(ka._ux * ka._ux + ka._uy * ka._uy));
        if (need > rootS) rootS = need;
    }
    for (ia = 0; ia < rk0.length; ia++) {
        ka = rk0[ia];
        for (ib = ia + 1; ib < rk0.length; ib++) {
            kb = rk0[ib]; dx0 = ka._ux - kb._ux; dy0 = ka._uy - kb._uy;
            need = (ka._ext + kb._ext) * 1.05 / Math.max(1, Math.sqrt(dx0 * dx0 + dy0 * dy0));
            if (need > rootS) rootS = need;
        }
    }
    return 1 + (rootS - 1) * pb;
}

// pilot layout: planets render inflated, so a child's orbit must clear its parent's
// inflated globe. r = the unscaled orbit radius; returns the (blended) pilot radius.
export function pilotOrbitRadius(n, p, r, pb) {
    var rp = p._pF * (PILOT_CLEAR0 + 0.55 * (n.spreadT || 0));
    if (rp < p._pF + 1.15 * n._pF) rp = p._pF + 1.15 * n._pF;   // floored children can be as big as the parent: never embed
    // a node carrying moons must also carry them clear of its parent's globe
    if (n.kids.length) rp = Math.max(rp, 1.65 * p._pF + n._ext);
    if (rp > r) r += (rp - r) * pb;
    return r;
}
