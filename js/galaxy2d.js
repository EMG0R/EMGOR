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
    var BODY_F = 0.26;              // body radius = own system radius * BODY_F
    var ORBIT_MIN = 0.46, ORBIT_MAX = 1.0; // orbit radii as fraction of parent sysR
    var MARGIN_X = 44, MARGIN_Y = 76;      // screen margin so labels never clip
    var FLY_DUR = 1.15;             // seconds, fractal zoom flight
    var CHILD_BOOST = 1.85;         // visual size boost for the focused nav ring
    var TAU = Math.PI * 2;

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
    var spin = 0;                   // grab-rotation: rad added to every orbital angle
    var spinV = 0;                  // angular momentum (rad/s)

    var intro = null;               // big-bang state
    var overlayNode = null;
    var suppressHash = false;

    // stars
    var starN = 0;
    var starX, starY, starR, starPh, starLayer, starTint;
    var LAYER_PAR = [0.018, 0.05, 0.11];
    var STAR_SPIN = [0.05, 0.13, 0.26]; // per-layer spin parallax (depth grid feel)

    // sprites
    var nebulaSprites = [];
    var accretionSprite = null;
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
            parentNode: null, sysR: ROOT_SYS_R, wx: 0, wy: 0
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
                wx: 0, wy: 0, sx: 0, sy: 0, sr: 0, alpha: 0, rev: 1
            };
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
            node.bodyR = node.sysR * BODY_F;
            var kids = node.kids;
            for (var i = 0; i < kids.length; i++) {
                var k = kids[i];
                var rng = mulberry32(hash32(k.id));
                var t = kids.length === 1 ? 0.6 : i / (kids.length - 1);
                k.orbF = ORBIT_MIN + (ORBIT_MAX - ORBIT_MIN) * (t * 0.75 + t * t * 0.25);
                k.a0 = (i / kids.length) * TAU + rng() * 0.9;
                k.spd = (0.055 + 0.09 / (1 + i * 0.6)) * (rng() > 0.85 ? -1 : 1);
                seedIdentity(k, rng);
                walk(k);
            }
        })(root);
    }

    // ─── procedural planet identity ────────────────────────────
    function seedIdentity(node, rng) {
        node.hue = Math.floor(rng() * 360);
        node.sat = 55 + rng() * 30;
        node.hasRing = rng() < 0.42;
        node.ringAngle = (rng() - 0.5) * 0.9;
        node.ringTilt = 0.22 + rng() * 0.2;
        node.pulseRate = 0.4 + rng() * 1.1;
        node.pulsePhase = rng() * TAU;
        node.noiseKind = rng() < 0.5 ? 'bands' : 'speckle';
        node.spriteSeed = hash32(node.id + '::surface');
        node.sprite = makePlanetSprite(node);
        node.glow = makeGlowSprite(node.hue, node.sat);
    }

    function makePlanetSprite(node) {
        var S = 192;                             // sprite canvas size
        var c = document.createElement('canvas');
        c.width = c.height = S;
        var g = c.getContext('2d');
        var cx = S / 2, cy = S / 2;
        var R = S * 0.30;                        // body radius, room left for ring
        var rng = mulberry32(node.spriteSeed);
        var hue = node.hue, sat = node.sat;

        // ring behind
        if (node.hasRing) drawRing(g, cx, cy, R, node, 0.55, true);

        g.save();
        g.beginPath();
        g.arc(cx, cy, R, 0, TAU);
        g.clip();

        // base sphere gradient, lit upper-left
        var lg = g.createRadialGradient(cx - R * 0.38, cy - R * 0.38, R * 0.08, cx, cy, R * 1.15);
        lg.addColorStop(0, 'hsl(' + hue + ',' + sat + '%,72%)');
        lg.addColorStop(0.45, 'hsl(' + hue + ',' + sat + '%,46%)');
        lg.addColorStop(0.85, 'hsl(' + ((hue + 24) % 360) + ',' + sat + '%,20%)');
        lg.addColorStop(1, 'hsl(' + ((hue + 30) % 360) + ',' + Math.min(90, sat + 10) + '%,10%)');
        g.fillStyle = lg;
        g.fillRect(0, 0, S, S);

        // surface noise
        if (node.noiseKind === 'bands') {
            var bands = 3 + Math.floor(rng() * 4);
            var rot = (rng() - 0.5) * 0.8;
            g.save();
            g.translate(cx, cy); g.rotate(rot); g.translate(-cx, -cy);
            for (var b = 0; b < bands; b++) {
                var by = cy - R + rng() * R * 2;
                var bh = R * (0.08 + rng() * 0.22);
                g.fillStyle = 'hsla(' + ((hue + (rng() - 0.5) * 60 + 360) % 360) + ',' +
                    sat + '%,' + (rng() < 0.5 ? 30 : 62) + '%,' + (0.08 + rng() * 0.14) + ')';
                g.beginPath();
                g.ellipse(cx, by, R * 1.3, bh, 0, 0, TAU);
                g.fill();
            }
            g.restore();
        } else {
            var n = 26 + Math.floor(rng() * 40);
            for (var s = 0; s < n; s++) {
                var a = rng() * TAU, d = Math.sqrt(rng()) * R * 0.94;
                var sx2 = cx + Math.cos(a) * d, sy2 = cy + Math.sin(a) * d;
                var sr2 = R * (0.03 + rng() * 0.1);
                g.fillStyle = 'hsla(' + ((hue + (rng() - 0.5) * 80 + 360) % 360) + ',' +
                    sat + '%,' + (rng() < 0.6 ? 26 : 68) + '%,' + (0.1 + rng() * 0.16) + ')';
                g.beginPath();
                g.ellipse(sx2, sy2, sr2 * (0.7 + rng()), sr2, rng() * TAU, 0, TAU);
                g.fill();
            }
        }

        // terminator shadow (lower-right)
        var sh = g.createRadialGradient(cx - R * 0.4, cy - R * 0.4, R * 0.3, cx, cy, R * 1.35);
        sh.addColorStop(0, 'rgba(0,0,0,0)');
        sh.addColorStop(0.72, 'rgba(6,1,16,0.05)');
        sh.addColorStop(1, 'rgba(6,1,16,0.72)');
        g.fillStyle = sh;
        g.fillRect(0, 0, S, S);
        g.restore();

        // atmosphere rim
        g.strokeStyle = 'hsla(' + hue + ',90%,72%,0.5)';
        g.lineWidth = 1.6;
        g.beginPath();
        g.arc(cx, cy, R + 0.8, 0, TAU);
        g.stroke();

        // ring front half
        if (node.hasRing) drawRing(g, cx, cy, R, node, 0.9, false);

        node.spriteR = R / S;       // body radius as fraction of sprite size
        return c;
    }

    function drawRing(g, cx, cy, R, node, alpha, behind) {
        g.save();
        g.translate(cx, cy);
        g.rotate(node.ringAngle);
        g.strokeStyle = 'hsla(' + ((node.hue + 40) % 360) + ',80%,70%,' + (alpha * 0.5) + ')';
        g.lineWidth = R * 0.13;
        g.beginPath();
        g.ellipse(0, 0, R * 1.5, R * 1.5 * node.ringTilt, 0,
            behind ? Math.PI : 0, behind ? TAU : Math.PI);
        g.stroke();
        g.strokeStyle = 'hsla(' + ((node.hue + 40) % 360) + ',90%,80%,' + (alpha * 0.35) + ')';
        g.lineWidth = R * 0.04;
        g.beginPath();
        g.ellipse(0, 0, R * 1.72, R * 1.72 * node.ringTilt, 0,
            behind ? Math.PI : 0, behind ? TAU : Math.PI);
        g.stroke();
        g.restore();
    }

    function makeGlowSprite(hue, sat) {
        var S = 256;
        var c = document.createElement('canvas');
        c.width = c.height = S;
        var g = c.getContext('2d');
        var gr = g.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
        gr.addColorStop(0, 'hsla(' + hue + ',' + Math.min(95, sat + 25) + '%,68%,0.5)');
        gr.addColorStop(0.35, 'hsla(' + hue + ',' + sat + '%,55%,0.18)');
        gr.addColorStop(1, 'hsla(' + hue + ',' + sat + '%,50%,0)');
        g.fillStyle = gr;
        g.fillRect(0, 0, S, S);
        return c;
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

    function makeAccretionSprite() {
        var S = 512;
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
        starLayer = new Uint8Array(starN);
        starTint = new Uint8Array(starN);
        var rng = mulberry32(hash32('emgor::stars'));
        for (var i = 0; i < starN; i++) {
            starX[i] = rng() * W;
            starY[i] = rng() * H;
            var l = rng();
            starLayer[i] = l < 0.5 ? 0 : (l < 0.83 ? 1 : 2);
            starR[i] = 0.4 + starLayer[i] * 0.45 + rng() * 0.7;
            starPh[i] = rng() * TAU;
            var t = rng();
            starTint[i] = t < 0.07 ? 2 : (t < 0.26 ? 1 : 0); // white / violet / cyan
        }
    }

    var STAR_COLORS = ['rgba(235,225,255,', 'rgba(192,132,252,', 'rgba(110,235,255,'];
    var LCOS = [1, 1, 1], LSIN = [0, 0, 0];   // per-layer spin rotation, per frame

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
    function computePositions() {
        for (var i = 0; i < drawOrder.length; i++) {
            var n = drawOrder[i];
            var p = n.parentNode;
            var a = n.a0 + time * n.spd + spin;
            var r = n.orbF * p.sysR;
            n.wx = p.wx + Math.cos(a) * r * stretchX;
            n.wy = p.wy + Math.sin(a) * r * stretchY;
        }
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
        el.href = '#' + node.route;
        el.draggable = false;
        el.innerHTML =
            '<span class="pl-name">' + node.title.toLowerCase() + '</span>' +
            (node.kids.length
                ? '<span class="pl-sub">' + node.kids.length + ' bodies</span>'
                : '<span class="pl-sub pl-leaf">landing</span>');
        el.title = node.blurb;
        el.addEventListener('click', function (e) {
            if (e.metaKey || e.ctrlKey || e.shiftKey) return;
            e.preventDefault();
            go(node.route);
        });
        labelsEl.appendChild(el);
        labelPool[node.id] = el;
        return el;
    }

    function syncLabels() {
        // labeled set: children of focus (+ children of trans.from while flying)
        var id;
        for (id in labelPool) labelPool[id]._keep = false;

        var e = trans ? easeInOut(clamp(trans.t, 0, 1)) : 1;
        placeLabels(focus.kids, trans ? e : 1);
        if (trans) placeLabels(trans.from.kids, 1 - e, focus.kids);

        for (id in labelPool) {
            var el = labelPool[id];
            if (!el._keep && el.style.display !== 'none') el.style.display = 'none';
        }
    }

    function placeLabels(kids, alpha, excludeIfIn) {
        for (var i = 0; i < kids.length; i++) {
            var n = kids[i];
            if (excludeIfIn && excludeIfIn.indexOf(n) !== -1) continue;
            var el = labelFor(n);
            el._keep = true;
            var a = alpha * (intro ? n.rev : 1);
            if (a <= 0.02) { el.style.display = 'none'; continue; }
            var d = clamp(n.sr * 2.7, 80, 250);
            var lw = clamp(d * 1.65, 122, 330);  // wider than tall: long names breathe
            var x = clamp(n.sx, lw / 2 + 6, W - lw / 2 - 6);
            var y = clamp(n.sy, d / 2 + 6, H - d / 2 - 6);
            el.style.display = 'flex';
            el.style.width = lw + 'px';
            el.style.height = d + 'px';
            el.style.fontSize = clamp(d * 0.115, 12, 19) + 'px';
            el.style.opacity = a.toFixed(3);
            el.style.transform = 'translate3d(' + (x - lw / 2) + 'px,' + (y - d / 2) + 'px,0)';
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
            var ph = time * 0.012 + i * 2.1 + spin * 0.12;
            var bx = W * (0.18 + 0.64 * (0.5 + 0.5 * Math.sin(ph + i * 1.7)));
            var by = H * (0.2 + 0.6 * (0.5 + 0.5 * Math.cos(ph * 0.8 + i * 2.3)));
            bx -= cam.x * 0.004;
            by -= cam.y * 0.004;
            var s = d * (0.7 + i * 0.22);
            ctx.drawImage(nebulaSprites[i], bx - s / 2, by - s / 2, s, s);
        }
    }

    function drawStars() {
        var tw = reducedMotion ? 0 : time;
        // star layers counter-rotate at different rates while the plane spins
        for (var l = 0; l < 3; l++) {
            LCOS[l] = Math.cos(-spin * STAR_SPIN[l]);
            LSIN[l] = Math.sin(-spin * STAR_SPIN[l]);
        }
        var cx = W / 2, cy = H / 2;
        for (var i = 0; i < starN; i++) {
            var ly = starLayer[i];
            var par = LAYER_PAR[ly];
            var x0 = starX[i] - cam.x * par - cx;
            var y0 = starY[i] - cam.y * par - cy;
            var x = x0 * LCOS[ly] - y0 * LSIN[ly] + cx;
            var y = x0 * LSIN[ly] + y0 * LCOS[ly] + cy;
            x = ((x % W) + W) % W;
            y = ((y % H) + H) % H;
            var a = 0.35 + 0.45 * (0.5 + 0.5 * Math.sin(tw * (0.6 + starLayer[i] * 0.5) + starPh[i]));
            ctx.fillStyle = STAR_COLORS[starTint[i]] + a.toFixed(3) + ')';
            var r = starR[i];
            ctx.fillRect(x - r, y - r, r * 2, r * 2);
        }
    }

    function toScreen(n) {
        n.sx = (n.wx - cam.x) * cam.scale + W / 2;
        n.sy = (n.wy - cam.y) * cam.scale + H / 2;
        n.sr = n.bodyR * cam.scale;
    }

    var DASH = [6, 7];
    var NO_DASH = [];

    function drawWorld() {
        var e = trans ? easeInOut(clamp(trans.t, 0, 1)) : 1;
        var i, n;

        // black hole (root center)
        var bx = (0 - cam.x) * cam.scale + W / 2;
        var by = (0 - cam.y) * cam.scale + H / 2;
        var bcore = ROOT_SYS_R * 0.075 * cam.scale;
        if (bcore < 2600 && bx > -W && bx < 2 * W && by > -H && by < 2 * H) {
            drawBlackHole(bx, by, bcore);
        }

        // orbit rings for focus system (and previous during flight)
        drawOrbitRings(focus, trans ? e : 1);
        if (trans) drawOrbitRings(trans.from, 1 - e);

        // bodies, depth order (parents behind children)
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
            if (a <= 0.01) continue;
            toScreen(n);
            var b = boostFor(n, focus);
            if (trans) b = lerp(boostFor(n, trans.from), b, e);
            n.sr *= b;
            if (intro && n.parentNode === root) {
                // resolve out of the center during the big bang
                n.sx = lerp(W / 2, n.sx, n.rev);
                n.sy = lerp(H / 2, n.sy, n.rev);
            }
            var r = n.sr;
            if (r < 0.5 || r > 6000) continue;
            if (n.sx < -r * 4 || n.sx > W + r * 4 || n.sy < -r * 4 || n.sy > H + r * 4) continue;
            drawPlanet(n, a);
        }
    }

    function drawOrbitRings(f, alpha) {
        if (alpha <= 0.02) return;
        var pxc = (f.wx - cam.x) * cam.scale + W / 2;
        var pyc = (f.wy - cam.y) * cam.scale + H / 2;
        ctx.save();
        ctx.globalCompositeOperation = 'lighter';
        ctx.setLineDash(DASH);
        ctx.lineWidth = 1;
        for (var i = 0; i < f.kids.length; i++) {
            var k = f.kids[i];
            var rx = k.orbF * f.sysR * stretchX * cam.scale;
            var ry = k.orbF * f.sysR * stretchY * cam.scale;
            if (rx < 8 || rx > W * 4) continue;
            var a = alpha * 0.14 * (intro ? k.rev : 1);
            if (a <= 0.01) continue;
            ctx.strokeStyle = 'rgba(168,85,247,' + a.toFixed(3) + ')';
            ctx.beginPath();
            ctx.ellipse(pxc, pyc, rx, ry, 0, 0, TAU);
            ctx.stroke();
        }
        ctx.setLineDash(NO_DASH);
        ctx.restore();
    }

    function drawBlackHole(x, y, core) {
        var t = reducedMotion ? 0 : time;
        ctx.save();
        // accretion disk (rotating sprite, additive)
        ctx.globalCompositeOperation = 'lighter';
        var ds = core * 7;
        ctx.translate(x, y);
        ctx.rotate(t * 0.12);
        ctx.globalAlpha = 0.85 + 0.15 * Math.sin(t * 1.7);
        ctx.drawImage(accretionSprite, -ds / 2, -ds / 2, ds, ds);
        ctx.rotate(-t * 0.26);
        ctx.globalAlpha = 0.4;
        ctx.drawImage(accretionSprite, -ds * 0.35, -ds * 0.35, ds * 0.7, ds * 0.7);
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
        // glow (additive)
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = alpha * (0.5 + pulse * 0.45);
        var gs = r * (5.2 + pulse * 1.1);
        ctx.drawImage(n.glow, n.sx - gs / 2, n.sy - gs / 2, gs, gs);
        // body sprite (ring included) — sprite body radius is spriteR of its size
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = alpha;
        var ss = r / n.spriteR;
        ctx.drawImage(n.sprite, n.sx - ss / 2, n.sy - ss / 2, ss, ss);
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

        // grab-rotation momentum: fling keeps the plane spinning, friction slows it
        if (!dragging && spinV !== 0) {
            spin += spinV * dt;
            spinV *= Math.pow(0.5, dt * 1.6);
            if (Math.abs(spinV) < 0.01) spinV = 0;
        }

        draw(dt);
        syncLabels();
    }

    // ─── input ─────────────────────────────────────────────────
    var dragging = false;
    var pointers = {};
    var pinchStart = 0, pinchFired = false;
    var dragSX = 0, dragSY = 0, dragMoved = false, dragConsumed = false;
    var dragA = 0, dragT = 0;
    var SPIN_V_MAX = 7;             // rad/s fling cap
    var DEAD_R = 44;                // px around center where the angle is unstable

    function pointerAngle(x, y) {
        return Math.atan2(y - H / 2, x - W / 2);
    }
    function normAngle(a) {
        while (a > Math.PI) a -= TAU;
        while (a < -Math.PI) a += TAU;
        return a;
    }

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
                dragA = pointerAngle(e.clientX, e.clientY);
                dragT = performance.now();
                spinV = 0;
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
                // spin the plane: angular delta of the pointer's arc around center
                if (Math.hypot(e.clientX - W / 2, e.clientY - H / 2) > DEAD_R) {
                    var na = pointerAngle(e.clientX, e.clientY);
                    var da = normAngle(na - dragA);
                    dragA = na;
                    spin += da;
                    var now = performance.now();
                    var dts = Math.max(0.008, (now - dragT) / 1000);
                    spinV = clamp(0.75 * (da / dts) + 0.25 * spinV,
                        -SPIN_V_MAX, SPIN_V_MAX);
                    dragT = now;
                } else {
                    dragA = pointerAngle(e.clientX, e.clientY);
                }
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
                if (d < Math.max(34, n.sr * 1.6)) go(n.route);
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
