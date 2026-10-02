// ship-perf.js — adaptive quality tiers + LOD helpers for ship mode (docs/ship-mode.md, rev 17 C).
//
// QUALITY MANAGER
//   import { createQualityManager, applyQualityCommand, lodFor, makeImpostor } from './ship-perf.js';
//   var q = createQualityManager(engine, { fx: fx, ps: ps });       // opts also: getFx/getPs getters, auto:false, key:'nmg-perf'
//   q.tier                      current tier 0 (lowest) .. 3 (full, == pre-rev-17 look)
//   q.setTier(n, manual)        apply a tier now; manual=true also turns auto-stepping off (and persists that)
//   q.auto                      true: the manager steps tiers itself. q.setAuto(bool)
//   q.attach({ fx, ps })        (re)bind live objects. CALL THIS whenever ship.js recreates them (fx.dispose(); fx = createFx(...)
//                               at rescale, makePs()): the current tier is re-applied immediately. Values or () => value getters.
//   q.update(dt, ms?)           evaluate the step rules. The engine's onFrame hook already calls this every frame, so ship.js only
//                               needs it if it wants to feed its own timing (pass ms to record a sample).
//   q.hooks.onTier(fn)          fn(tier, prevTier) on every change; returns unsubscribe
//   q.stats                     { tier, auto, p95, avg, workP95, fps, calls, tris, points, programs, stepMs, renderMs, pixelRatio, bloom }
//   q.dispose()
// Rules: rolling 2 s window of frame times. Step DOWN a tier when p95 > 20 ms over a full window. Step UP when work-time p95
//   < 9 ms for 10 s straight (work = CPU time in frameStep; wall time on a 60 Hz display is pinned at 16.7 ms and could never read
//   "fast"). An up-step that is undone by a down-step within 30 s bans further up-steps for 2 min (no flapping). Ignores hidden
//   tabs / gaps > 250 ms. Persists { tier, auto } in localStorage 'nmg-perf'. Sets globalThis.EMGOR_PERF_TIER / EMGOR_PERF_PR so
//   freshly created fx / planet surfaces start at the right tier.
// Tier table (3 == shipped look):
//   tier      0      1      2      3
//   DPR cap   1      1.25   1.5    min(devicePixelRatio, 2)
//   bloom     off    0.5x   0.75x  1x          (engine.setBloom)
//   stars     40%    60%    80%    100%        (engine.setStarFraction)
//   fx        fx.setQuality(t)  streaks 200/320/480/600, cone segments 5/6/8/8, sparks 6/8/12/12
//   planet    ps.setQuality(t)  grid 64/96/128/128, flora 150/300/500/500, creatures 10/20/40/40
//
// SLASH COMMAND (wire into ship.js runCommand):   var r = applyQualityCommand(str);  if (r !== null) say(r);
//   '/quality'        -> status string        '/quality 0..3' -> force tier (auto off)        '/quality auto' -> auto on
//   returns null when str is not a /quality command.
//
// LOD (no behaviour change until ship.js adopts it)
//   lodFor(distL)        'hi' (< 60 L) | 'lo' (< 200 L) | 'impostor' (>= 200 L); distL = distance in ship lengths
//   makeImpostor(THREE, color) -> THREE.Sprite: additive glow dot, ONE shared SpriteMaterial per colour (and one shared texture).
//                         Set sprite.scale to the body size in world units; add to the scene when lodFor() === 'impostor'.

export var LOD_LO = 60, LOD_IMPOSTOR = 200;
export function lodFor(distL) { return distL < LOD_LO ? 'hi' : (distL < LOD_IMPOSTOR ? 'lo' : 'impostor'); }

var _impTex = null, _impMats = {};
export function makeImpostor(THREE, color) {
    if (!_impTex) {
        var c = document.createElement('canvas'); c.width = c.height = 64;
        var g = c.getContext('2d'), gr = g.createRadialGradient(32, 32, 0, 32, 32, 32);
        gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.25, 'rgba(255,255,255,0.7)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
        g.fillStyle = gr; g.fillRect(0, 0, 64, 64);
        _impTex = new THREE.CanvasTexture(c);
    }
    var key = new THREE.Color(color === undefined ? 0xffffff : color).getHexString();
    var m = _impMats[key];
    if (!m) {
        m = _impMats[key] = new THREE.SpriteMaterial({
            map: _impTex, color: new THREE.Color(color === undefined ? 0xffffff : color), transparent: true, depthWrite: false,
            blending: THREE.AdditiveBlending, toneMapped: false, fog: false
        });
    }
    var sp = new THREE.Sprite(m);
    sp.matrixAutoUpdate = true;       // frustumCulled stays true (default)
    return sp;
}

var PR_CAP = [1, 1.25, 1.5, 2];
var BLOOM = [[false, 0.5], [true, 0.5], [true, 0.75], [true, 1]];
var STARS = [0.4, 0.6, 0.8, 1];
var WIN_MS = 2000, UP_MS = 10000, DOWN_P95 = 20, UP_P95 = 9, GAP_MS = 250, MAXS = 512;

var _active = null;

function readSaved(key) {
    try {
        var v = localStorage.getItem(key);
        if (v == null) return null;
        if (/^\d$/.test(v)) return { tier: +v, auto: true };
        var o = JSON.parse(v);
        return o && typeof o.tier === 'number' ? o : null;
    } catch (e) { return null; }
}

export function createQualityManager(engine, opts) {
    opts = opts || {};
    var key = opts.key || 'nmg-perf';
    var saved = readSaved(key);
    var fxRef = opts.getFx || opts.fx || null, psRef = opts.getPs || opts.ps || null;
    var tier = 3, auto = opts.auto !== false;
    if (saved) { tier = Math.max(0, Math.min(3, saved.tier | 0)); if (saved.auto === false) auto = false; }
    if (opts.tier !== undefined) tier = Math.max(0, Math.min(3, opts.tier | 0));
    var listeners = [];

    // ring buffers (time stamp, wall-or-work frame ms, work ms)
    var tS = new Float64Array(MAXS), fS = new Float32Array(MAXS), wS = new Float32Array(MAXS), head = 0, count = 0;
    var sortA = new Float32Array(MAXS), sortB = new Float32Array(MAXS);
    var lowSince = -1, lastUpAt = -1e9, upBanUntil = 0, lastChangeAt = 0;
    var stats = { tier: tier, auto: auto, p95: 0, avg: 0, workP95: 0, fps: 0, calls: 0, tris: 0, points: 0, programs: 0, stepMs: 0, renderMs: 0, pixelRatio: 1, bloom: true };

    function get(ref) { return typeof ref === 'function' ? ref() : ref; }
    function save() { try { localStorage.setItem(key, JSON.stringify({ tier: tier, auto: auto })); } catch (e) { /* private mode */ } }

    function applyAll() {
        var t = tier;
        try { globalThis.EMGOR_PERF_TIER = t; } catch (e) { /* ignore */ }
        engine.setPixelRatioCap(PR_CAP[t]);
        try { globalThis.EMGOR_PERF_PR = engine.pixelRatio; } catch (e) { /* ignore */ }
        engine.setBloom(BLOOM[t][0], BLOOM[t][1]);
        engine.setStarFraction(STARS[t]);
        var fx = get(fxRef), ps = get(psRef);
        if (fx) { if (fx.setQuality) fx.setQuality(t); if (fx.setPixelRatio) fx.setPixelRatio(engine.pixelRatio); }
        if (ps && ps.setQuality) ps.setQuality(t);
    }

    function pct95(arr, n, out) {
        for (var i = 0; i < n; i++) out[i] = arr[(head - 1 - i + MAXS) % MAXS];
        var v = out.subarray(0, n); v.sort();
        return v[Math.min(n - 1, Math.floor(n * 0.95))];
    }

    var mgr = {
        get tier() { return tier; },
        get auto() { return auto; },
        stats: stats,
        hooks: { onTier: function (fn) { if (typeof fn === 'function') listeners.push(fn); return function () { var i = listeners.indexOf(fn); if (i >= 0) listeners.splice(i, 1); }; } },
        setTier: function (n, manual) {
            n = Math.max(0, Math.min(3, Math.round(+n)));
            if (manual) auto = false;
            var prev = tier; tier = n; count = 0; lowSince = -1; lastChangeAt = performance.now();
            applyAll(); save();
            stats.tier = tier; stats.auto = auto;
            if (n !== prev) for (var i = 0; i < listeners.length; i++) { try { listeners[i](n, prev); } catch (e) { /* listener bug must not stop rendering */ } }
            return tier;
        },
        setAuto: function (on) { auto = !!on; lowSince = -1; stats.auto = auto; save(); },
        attach: function (o) {
            if (o && o.fx !== undefined) fxRef = o.fx; if (o && o.getFx) fxRef = o.getFx;
            if (o && o.ps !== undefined) psRef = o.ps; if (o && o.getPs) psRef = o.getPs;
            applyAll();
        },
        // ms (optional): record a frame sample taken by the caller; with no ms this only evaluates the rules
        update: function (dt, ms, workMs) {
            var now = performance.now();
            if (ms !== undefined) {
                if (document.hidden || ms > GAP_MS || !(ms > 0)) { count = 0; lowSince = -1; return; }
                tS[head] = now; fS[head] = ms; wS[head] = workMs === undefined ? ms : workMs; head = (head + 1) % MAXS; if (count < MAXS) count++;
            }
            // drop samples older than the window
            while (count > 0 && now - tS[(head - count + MAXS) % MAXS] > WIN_MS) count--;
            if (!auto || count < 30) return;
            var oldest = tS[(head - count + MAXS) % MAXS];
            if (now - oldest < WIN_MS * 0.9) return;                 // need ~a full window
            var p95 = pct95(fS, count, sortA), wp95 = pct95(wS, count, sortB);
            stats.p95 = p95; stats.workP95 = wp95;
            if (p95 > DOWN_P95) {
                lowSince = -1;
                if (tier > 0) {
                    if (now - lastUpAt < 30000) upBanUntil = now + 120000;
                    mgr.setTier(tier - 1, false);
                }
                return;
            }
            if (wp95 < UP_P95 && now >= upBanUntil && tier < 3) {
                if (lowSince < 0) lowSince = now;
                else if (now - lowSince >= UP_MS) { lastUpAt = now; mgr.setTier(tier + 1, false); }
            } else lowSince = -1;
        },
        dispose: function () { if (off) off(); if (_active === mgr) _active = null; }
    };

    // refresh the public stats object a few times a second (cheap), not per frame
    var lastStat = 0;
    function refreshStats(now) {
        if (now - lastStat < 250) return; lastStat = now;
        var n = 0, sum = 0, i;
        for (i = 0; i < count; i++) { sum += fS[(head - 1 - i + MAXS) % MAXS]; n++; }
        stats.avg = n ? sum / n : 0; stats.fps = stats.avg ? 1000 / stats.avg : 0;
        var p = engine.perf;
        if (p) { stats.calls = p.calls; stats.tris = p.tris; stats.points = p.points; stats.programs = p.programs; stats.stepMs = p.stepMs; stats.renderMs = p.renderMs; }
        stats.pixelRatio = engine.pixelRatio; stats.bloom = engine.bloom.on;
        stats.tier = tier; stats.auto = auto;
    }

    var off = engine.onFrame ? engine.onFrame(function (stepMs, wallMs, dt) {
        // a real rAF interval when we have one (includes GPU back-pressure), else the CPU step time (manual stepping)
        mgr.update(dt, wallMs > 0 ? Math.max(wallMs, stepMs) : stepMs, stepMs);
        refreshStats(performance.now());
    }) : null;

    _active = mgr;
    applyAll();
    return mgr;
}

// '/quality' chat command. Returns a message string, or null if `str` is not a /quality command.
export function applyQualityCommand(str) {
    var m = /^\s*\/?quality(?:\s+(\S+))?\s*$/i.exec(String(str == null ? '' : str));
    if (!m) return null;
    var q = _active;
    if (!q) return 'QUALITY · manager not running';
    var a = m[1];
    if (a === undefined) return 'QUALITY · tier ' + q.tier + (q.auto ? ' (auto)' : ' (manual)') + ' · ' + q.stats.fps.toFixed(0) + ' fps · usage /quality 0-3 | auto';
    if (/^auto$/i.test(a)) { q.setAuto(true); return 'QUALITY · auto · tier ' + q.tier; }
    if (/^[0-3]$/.test(a)) { q.setTier(+a, true); return 'QUALITY · tier ' + q.tier + ' (manual)'; }
    return 'USAGE · /quality 0-3 | auto';
}

export default createQualityManager;
