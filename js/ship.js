/* ship.js — EMGOR galaxy "ship mode" game half. See docs/ship-mode.md (Revision 3).
   Lazy-loaded by galaxy3d.js after galaxy-ready; default export mount(engine).
   Everything here rides on the engine port (engine.THREE, setPilot...) so the
   ship flies through the very same scene, bodies and camera. Finite space:
   no procedural generation, no makeBody at runtime. */

import { createFx } from './ship-fx.js';
import { buildCreatureGeoms, makeCreatureMaterial, bossName, roleEyeZ, CREATURE_EYE, CREATURE_EYE_R, BOSS_ATTACKS, bossAttack } from './ship-enemies.js';

// ─── tuning (world units / seconds; L = ship length) ────────────────
var CRUISE = 1.5, REVERSE = 0.4, BOOST = 5, PULSE_SPEED = 60;   // world u/s (rev 5, NMS scale); pulse ramps exponentially to PULSE_SPEED
var PULSE_TAU = 3, PULSE_DECAY = 1;   // pulse(t) = 1 - e^(-t/tau) while Space is held; releasing bleeds it off over 1 s
var THROTTLE_MIN = -0.2;           // S past zero reverses down to this
var THROTTLE_RATE = 0.6;           // throttle integration per second
var PULSE_SPINUP = 1.5;            // seconds to full pulse charge
var MOUSE_SENS = 0.0019;           // rad per px of pointer travel
var MOUSE_WEIGHT = 6.5;            // rev 9: 1/s, how fast the turn rate chases the weighted mouse target (heavier than 11)
var BANK_K = 0.35, BANK_SPRING = 4, BANK_MAX = 0.6;   // rev 9: roll leans into yaw (rad per rad/s), spring 1/s, cap rad
var yawRate = 0, pitRate = 0, bank = 0, shake = 0;
var MAX_MOUSE_RATE = 14;           // rad/s: the only clamp on the mouse
var ROLL_RATE = 1.6;               // rad/s from A/D
var CAM_L = 3.2, CAM_UP = 0.3;     // chase distance / lift in ship lengths (rigid)
var BOARD_DUR = 1.6;               // rev 8: boarding / leaving cinematic (s)
var FOV_BOOST = 4, FOV_PULSE = 9;     // rev 9: halved (the streaks sell speed now)
var DOCK_R = 1.05, DOCK_Y = 0.45, DOCK_RATE = 0.03;   // rev 12: dock radius / height as x root.sysR: outside every orbit, high above the plane
var HARD_F = 1.15;                 // hard collision sphere, x renderedRadius
var PULSE_DROP_F = 1.5;            // pulse drops (and cannot start) inside this x R (+ speed lookahead)
var BOUNDARY_F = 3.5;              // soft edge, x root.sysR
var TARGET_CONE = 6 * Math.PI / 180;
// combat
var HP_MAX = 100, REGEN = 8 * 0.4;       // rev 9c: passive regen at 40 %, kills feed the rest
var P_BOLT_SPEED = 270, P_BOLT_RANGE = 120, P_BOLT_DMG = 8, P_FIRE_RATE = 6;   // L/s, L, hp, volleys/s (2 bolts each)
var E_BOLT_PER_LEN = 22, E_BOLT_MIN = 80, E_BOLT_MAX = 200;                    // enemy bolt speed = 22 L/s per L of body length, clamped (L/s)
var E_FIRE_PER_LEN = 18, E_RANGE_X = 1.5;                                      // fire range = 18 x body length; bolt range = 1.5 x fire range
var AGGRO_L = 120;
var E_L_CREATURE = 6, E_L_WAVE1 = 9;                                           // creature lengths in L
var SPAWN_MIN = 60, SPAWN_VAR = 40;                                            // spawn distance from player, L
var WAVE_SPEED_X = [0.6, 1.0, 0.9];                                            // x CRUISE (wave 1, wave 2, wave 3+)
var RUN_IN_MIN = 60, RUN_IN_VAR = 30, RUN_OFF_DEG = 30;   // run-in point 25-35 old L = 75-105 L (L is 3x smaller)
var PEEL_MIN = 60, PEEL_VAR = 30, RUN_NEAR = 8, RUN_NEAR_T = 1.5, DIVE_CLOSE = 3;
var AIM_CONE = 2.5 * Math.PI / 180, AIM_RATE = 0.9 * 0.4, AIM_YANK = 900;   // rad, rad/s, px/s of raw mouse = 'yanking'
var ENEMY_MAX = 12, BOLT_MAX = 120;
var FIRST_WAVE = 3, WAVE_MIN = 12, WAVE_VAR = 8;
var DEATH_TIME = 1.5;
// rev 9 C / 9b / 9c
var WAVE_X = 4;                                    // wave N uses the old curve's wave 4N
var FLEE_AT = 0.45, FLEE_TELE = 0.3, FLEE_RUN = 2.5, FLEE_REGEN = 3, FLEE_RATE = 0.15, FLEE_X = 3, FLEE_MAX = 2, FLEE_CAP = 0.85 * BOOST;   // flee state; burst speed capped so boost can still catch it
var SNIPER_DMG = 25, SNIPER_HOLD = 80, SNIPER_CHARGE = 1.2;
var ROLE_T = [                                     // speed, hp, dmg, turn, fireInt multipliers vs the wave curve
    { name: 'INTERCEPTOR', spd: 1.3, hp: 0.6, dmg: 0.45, turn: 1.1, fire: 0.8, mix: 60 },
    { name: 'SPITTER', spd: 0.6, hp: 1.5, dmg: 1.8, turn: 0.55, fire: 1.7, mix: 25 },
    { name: 'SNIPER', spd: 0.7, hp: 1.0, dmg: 1, turn: 0.8, fire: 1, mix: 15 }
];
var SPIT_SPEED = 45;                               // spitter orb speed, L/s (dodgeable)
var SPAWN_NEAR = 150, SPAWN_FAR = 250, GRACE_T = 3, BREATHER_MIN = 8, BREATHER_VAR = 4, ARROW_MAX = 10;
var ROLL_T = 0.45, ROLL_CD = 1.5, ROLL_CUT = 0.4, ROLL_SIDE = 6, TAP_MS = 260;
var DRIFT_SLIP = 0.4, DRIFT_YAW = 0.7, DRIFT_BOOST = 0.25;
var DROP_MAX = 24, DROP_VAL = 12, DROP_MAGNET = 20, DROP_COLLECT = 6, CHAIN_T = 4;
var GRAZE_R = 1.5, GRAZE_FULL = 10, OD_T = 4, OD_FIRE = 1.5, OD_DMG = 1.25;
var FOCUS_MAX = 1.5, FOCUS_RECHARGE = 8, FOCUS_TS = 0.35, Z_FOCUS_T = 8;
var PERIL_DETECT = 2, PERIL_SAVE = 10, PERIL_R = 14;
var LOCK_RANGE = 300, FAR2 = 120 * 120;
// revision 3: allies, bosses, persistence
var ALLY_MAX = 3, ALLY_HP = 50, A_SPEED = 2.5, A_TURN = 2.2, A_DMG = 7 / 3;
var BOSS_FIRST = 4, BOSS_EVERY = 2, BOSS_HP = 480, BOSS_L = 100, BOSS_SPEED = 0.25;        // rev 9: boss wave 4, 6, 8...; ~40 hits (rev 9c); length in L; x1.4 per boss
var ORB_MAX = 4, ORB_SPEED = 40, ORB_S = 2, ORB_DMG = 18, ORB_HP = 16;
var BAND_MIN = 60, BAND_MAX = 90;                 // rev 12: boss holds its carapace 60-90 L from the player (follows when you leave, lets you approach)
var SIG_DMG = [38, 35, 40, 36, 36], SIG_TELE = [1.5, 1.5, 1.5, 1.2, 1.5];   // beam sweep, orb ring, ram charge, mine field, gravity pull
var RAM_PLAYER = 45, RAM_BOSS = 70, RAM_ENEMY_FRAC = 0.25, RAM_INV = 1;
var SAVE_KEY = 'nmg', DEATH_RESPAWN_DELAY = 2.5;
var RESUME_WAVE = false;           // rev 6: always start at wave 1 (flip to restore the saved wave)

// rev 7: difficulty 1-3 multiplies the wave curve (hp, dmg, count delta, speed, regen delay delta, ally lag in waves)
var DIFFS = {
    1: { hp: 0.8, dmg: 0.7, n: -1, spd: 0.9, regen: -1, allyLag: 0 },
    2: { hp: 1, dmg: 1, n: 0, spd: 1, regen: 0, allyLag: 0 },
    3: { hp: 1.5, dmg: 1.6, n: 2, spd: 1.15, regen: 3, allyLag: 1 }
};

function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function easeInOut(t) { return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2; }
function wcurve(r) { var a = Math.abs(r) / 2; return (r < 0 ? -1 : 1) * Math.pow(a, 1.6) * 2; }
function damp(rate, dt) { return 1 - Math.exp(-rate * dt); }   // frame-rate independent lerp factor
function fmtDist(d) { return d >= 1000 ? (d / 1000).toFixed(1) + 'k' : String(Math.round(d)); }

// squared distance from point c to segment a-b (plain numbers: no allocation)
function segDistSq(ax, ay, az, bx, by, bz, cx, cy, cz) {
    var dx = bx - ax, dy = by - ay, dz = bz - az;
    var l2 = dx * dx + dy * dy + dz * dz, t = 0;
    if (l2 > 1e-9) t = clamp(((cx - ax) * dx + (cy - ay) * dy + (cz - az) * dz) / l2, 0, 1);
    var ex = ax + dx * t - cx, ey = ay + dy * t - cy, ez = az + dz * t - cz;
    return ex * ex + ey * ey + ez * ez;
}

// wave table: difficulty curve from the design doc
function waveParams(w) {
    if (w <= 1) return { n: 3, hp: 120, speed: WAVE_SPEED_X[0] * CRUISE, len: E_L_WAVE1, turn: 1.0, dmg: 3, fireInt: 1.8, err: 0.16, lead: 0, cone: 0.28, regenDelay: 0, dive: 1.25 };
    if (w === 2) return { n: 6, hp: 180, speed: WAVE_SPEED_X[1] * CRUISE, len: E_L_CREATURE, turn: 2.2, dmg: 8, fireInt: 0.9, err: 0.03, lead: 0.9, cone: 0.24, regenDelay: 1, dive: 1.6 };
    var n = Math.min(10, 4 + Math.floor((w - 2) / 2));
    return { n: n, hp: Math.round(240 * (1 + 0.1 * (w - 3))), speed: WAVE_SPEED_X[2] * CRUISE, len: E_L_CREATURE, turn: 1.6, dmg: 6, fireInt: 1.3, err: 0.07, lead: 0.6, cone: 0.24, regenDelay: 4, dive: 1.6 };
}

export default function mount(engine) {
    var THREE = engine.THREE, scene = engine.scene, camera = engine.camera;
    var root = engine.root;

    // sizes derive from the live tree so a growing galaxy keeps the ship tiny
    var refR = (root.kids && root.kids[0]) ? engine.renderedRadius(root.kids[0]) : 0;
    var L = (refR > 0 ? refR : 100) / 1000;     // rev 5: NMS ratio, planet radius : ship length = 1000 : 1
    var baseFov = camera.fov;
    var EDGE_R = BOUNDARY_F * root.sysR;
    var galS = 0.9 * (refR > 0 ? refR : 100);   // rev 8: galaxy-scale hull length of a remote ghost (0.9 x root-planet radius)
    var mineS = 1.35 * (refR > 0 ? refR : 100);  // rev 12: YOUR docked ship = 1.35 x root-planet radius (half the rev 8 size)
    var shipScale = mineS;

    // ─── css (injected at mount so first paint never loads it) ──────
    var link = document.createElement('link');
    link.rel = 'stylesheet'; link.href = 'style/ship.css';
    document.head.appendChild(link);
    var linkChat = document.createElement('link');      // rev 11: chat styles + Monocraft font, lazy like everything else here
    linkChat.rel = 'stylesheet'; linkChat.href = 'style/ship-chat.css';
    document.head.appendChild(linkChat);

    // ─── mesh building helpers ──────────────────────────────────────
    function Builder() { this.pos = []; this.col = []; }
    Builder.prototype.tri = function (a, b, c, k) {
        this.pos.push(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);
        for (var i = 0; i < 3; i++) this.col.push(k.r, k.g, k.b);
    };
    Builder.prototype.quad = function (a, b, c, d, k) { this.tri(a, b, c, k); this.tri(a, c, d, k); };
    // convex polygon extruded by offset vector (thin wing / fin)
    Builder.prototype.slab = function (poly, off, k, kEdge) {
        var n = poly.length, top = [], bot = [], i;
        for (i = 0; i < n; i++) {
            top.push([poly[i][0] + off[0] / 2, poly[i][1] + off[1] / 2, poly[i][2] + off[2] / 2]);
            bot.push([poly[i][0] - off[0] / 2, poly[i][1] - off[1] / 2, poly[i][2] - off[2] / 2]);
        }
        for (i = 1; i < n - 1; i++) { this.tri(top[0], top[i], top[i + 1], k); this.tri(bot[0], bot[i + 1], bot[i], kEdge || k); }
        for (i = 0; i < n; i++) { var j = (i + 1) % n; this.quad(top[i], bot[i], bot[j], top[j], kEdge || k); }
    };
    // lofted tube along Z: secs {z, w(half width), t(top), b(bottom)}; ks cycles per band
    Builder.prototype.loft = function (secs, sides, cx, cy, ks) {
        var rings = [], i, s;
        for (i = 0; i < secs.length; i++) {
            var sc = secs[i], ring = [];
            for (s = 0; s < sides; s++) {
                var a = s / sides * Math.PI * 2, sn = Math.sin(a);
                ring.push([cx + Math.cos(a) * sc.w, cy + sn * (sn > 0 ? sc.t : sc.b), sc.z]);
            }
            rings.push(ring);
        }
        for (i = 0; i < rings.length - 1; i++) {
            var k = ks[i % ks.length];
            for (s = 0; s < sides; s++) {
                var s2 = (s + 1) % sides;
                this.quad(rings[i][s], rings[i + 1][s], rings[i + 1][s2], rings[i][s2], k);
            }
        }
        // end caps (fan)
        var ends = [[0, ks[0]], [rings.length - 1, ks[ks.length - 1]]];
        for (i = 0; i < 2; i++) {
            var r = rings[ends[i][0]], ctr = [cx, cy, secs[ends[i][0]].z];
            for (s = 0; s < sides; s++) this.tri(ctr, r[s], r[(s + 1) % sides], ends[i][1]);
        }
    };
    Builder.prototype.geometry = function () {
        var g = new THREE.BufferGeometry();
        g.setAttribute('position', new THREE.Float32BufferAttribute(this.pos, 3));
        g.setAttribute('color', new THREE.Float32BufferAttribute(this.col, 3));
        g.computeVertexNormals();       // non-indexed => flat shading
        g.computeBoundingSphere();
        return g;
    };
    function hullMaterial() {
        // no scene lights: fixed light dir like the planets. Log-depth chunks keep
        // it sorting correctly with the renderer's logarithmic depth buffer.
        var mat = new THREE.ShaderMaterial({
            side: THREE.DoubleSide,
            vertexShader: [
                '#include <common>', '#include <logdepthbuf_pars_vertex>',
                'varying vec3 vN; varying vec3 vC;',
                'void main(){',
                '  vN = normalize(mat3(modelMatrix) * normal); vC = color;',
                '  gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0);',
                '  #include <logdepthbuf_vertex>',
                '}'
            ].join('\n'),
            fragmentShader: [
                '#include <common>', '#include <logdepthbuf_pars_fragment>',
                'varying vec3 vN; varying vec3 vC;',
                'void main(){',
                '  #include <logdepthbuf_fragment>',
                '  vec3 n = normalize(vN) * (gl_FrontFacing ? 1.0 : -1.0);',
                '  float l = max(dot(n, normalize(vec3(-0.62,0.52,0.4))), 0.0);',
                '  vec3 c = vC * (0.32 + 0.85 * l);',
                '  c += vec3(0.0,0.35,0.4) * pow(1.0 - abs(n.y), 6.0) * 0.12;',
                '  gl_FragColor = vec4(c, 1.0);',
                '}'
            ].join('\n')
        });
        mat.vertexColors = true;
        return mat;
    }
    function glowTexture(r, g, b) {
        var cv = document.createElement('canvas'); cv.width = cv.height = 64;
        var c = cv.getContext('2d'), rg = c.createRadialGradient(32, 32, 0, 32, 32, 32);
        rg.addColorStop(0, 'rgba(255,255,255,1)');
        rg.addColorStop(0.25, 'rgba(' + r + ',' + g + ',' + b + ',0.85)');
        rg.addColorStop(1, 'rgba(' + r + ',' + g + ',' + b + ',0)');
        c.fillStyle = rg; c.fillRect(0, 0, 64, 64);
        return new THREE.CanvasTexture(cv);
    }
    function glowSprite(tex) {
        return new THREE.Sprite(new THREE.SpriteMaterial({
            map: tex, blending: THREE.AdditiveBlending, depthTest: true, depthWrite: false, transparent: true
        }));
    }

    // ─── fx (js/ship-fx.js): streaks, exhaust cones, bolts, flash, sparks ───
    var fx = createFx(THREE, scene, camera, L);
    var fxCones = 0;
    // one exhaust set = a unit-scale holder (cones are authored in world units of L) following a hull's pose; cones pooled, never freed
    function makeEx(parent) { var h = new THREE.Group(); h.visible = false; parent.add(h); return { holder: h, cones: [], idx: [], th: [], col: -1 }; }
    function exColor(ex, hex) { ex.col = hex; for (var i = 0; i < ex.cones.length; i++) ex.cones[i].material.uniforms.uBase.value.setHex(hex); }
    function exSetup(ex, thr, hex) {
        var n = thr.length, i;
        while (ex.cones.length < n) { var m = fx.exhaust(1)[0]; ex.holder.add(m); ex.cones.push(m); ex.idx.push(fxCones++); }
        ex.th.length = 0;
        for (i = 0; i < ex.cones.length; i++) {
            if (i < n) { ex.th.push(thr[i]); ex.cones[i].visible = true; } else { ex.cones[i].visible = false; fx.setExhaust(ex.idx[i], 0); }
        }
        exColor(ex, hex);
    }
    function exUpdate(ex, pos, quat, sc, intensity) {
        ex.holder.position.copy(pos); ex.holder.quaternion.copy(quat);
        for (var i = 0; i < ex.th.length; i++) { ex.cones[i].position.copy(ex.th[i]).multiplyScalar(sc); fx.setExhaust(ex.idx[i], intensity); }
    }
    function exOff(ex) { ex.holder.visible = false; for (var i = 0; i < ex.idx.length; i++) fx.setExhaust(ex.idx[i], 0); }
    var exMe = makeEx(scene);

    var shipRoot = new THREE.Group();
    shipRoot.name = 'ship';
    shipRoot.scale.setScalar(mineS);
    var hullObj = null, hullLow = null;     // rev 12: hullObj = full hull (piloting), hullLow = low-LOD dock hull
    var muzzles = [new THREE.Vector3(-0.26, -0.04, -0.45), new THREE.Vector3(0.26, -0.04, -0.45)];
    function fallbackDart() {
        var b = new Builder(), HULL = new THREE.Color(0x7B2FBE), HULL_D = new THREE.Color(0x3D1D8E), WING = new THREE.Color(0xA855F7), FIN = new THREE.Color(0x00C8DC);
        b.loft([{ z: -0.5, w: 0, t: 0, b: 0 }, { z: -0.2, w: 0.07, t: 0.05, b: 0.04 }, { z: 0.2, w: 0.08, t: 0.06, b: 0.045 }, { z: 0.42, w: 0.05, t: 0.04, b: 0.03 }], 6, 0, 0, [HULL, HULL_D]);
        [-1, 1].forEach(function (s) {
            b.slab([[s * 0.07, 0, -0.12], [s * 0.5, -0.03, 0.38], [s * 0.05, 0, 0.36]], [0, 0.018, 0], WING, HULL_D);
        });
        b.slab([[0, 0.05, 0.2], [0, 0.2, 0.44], [0, 0.04, 0.42]], [0.014, 0, 0], FIN, HULL_D);
        var g = new THREE.Group(), m = new THREE.Mesh(b.geometry(), hullMaterial());
        m.frustumCulled = false; g.add(m);
        g.userData.thrusters = [new THREE.Vector3(-0.17, 0, 0.42), new THREE.Vector3(0.17, 0, 0.42)];
        g.userData.gunMuzzles = [new THREE.Vector3(-0.3, -0.02, -0.1), new THREE.Vector3(0.3, -0.02, -0.1)];
        return g;
    }
    function setHull(g) {
        if (hullObj) shipRoot.remove(hullObj);
        hullObj = g;
        g.traverse(function (o) { o.frustumCulled = false; });
        shipRoot.add(g);
        exSetup(exMe, g.userData.thrusters || [], effColor());
        var gm = g.userData.gunMuzzles;
        if (gm && gm.length >= 2) { muzzles[0].copy(gm[0]); muzzles[1].copy(gm[1]); }
        var pc = profileColor(); if (pc != null) tintGhost(g, pc);
        syncLod();
    }
    // full hull only while flying / boarding; the docked galaxy-scale ship is the cheap low-LOD hull
    function syncLod() {
        var full = !hullLow || (state === 'piloting' && (boarding ? boardT > 0.12 : (exiting ? exitT < 0.7 : true)));
        if (hullObj) hullObj.visible = full;
        if (hullLow) hullLow.visible = !full;
    }
    setHull(fallbackDart());
    import('./ship-hull.js').then(function (m) {
        try {
            var h = buildHullKind(m, hullFor(curProfile())); if (h) setHull(h);
            hullLow = m.buildHull(THREE, { lod: 'low' }); hullLow.traverse(function (o) { o.frustumCulled = false; }); shipRoot.add(hullLow);
            var pcL = profileColor(); if (pcL != null) tintGhost(hullLow, pcL);
            syncLod();
            allyTpl = m.buildHull(THREE); tintAlly(allyTpl);
        } catch (e) { console.warn('[ship] ship-hull failed, keeping dart', e); }
    }).catch(function () { /* keep the fallback dart */ });
    scene.add(shipRoot);

    // ─── combat root: enemies + projectiles + debris ────────────────
    var combatRoot = new THREE.Group();
    combatRoot.name = 'ship-combat';
    combatRoot.visible = false;
    scene.add(combatRoot);

    // enemy creatures (alien hybrids): geometry built once per palette (ship-enemies.js),
    // one pooled ShaderMaterial per live creature (uTime/uPh drive the vertex flex).
    var creat = buildCreatureGeoms(THREE);
    var enemyGlowTex = [glowTexture(255, 150, 50), glowTexture(190, 255, 60), glowTexture(90, 230, 255)];   // ember / acid / frost (sniper)
    var E_L = E_L_CREATURE * L, E_R = 0.55 * E_L;        // default creature length / hit radius (rescaled per wave via the group scale)
    var enemies = [], boss = null;
    (function buildEnemies() {
        for (var i = 0; i <= ENEMY_MAX; i++) {          // the last slot is the boss
            var isBoss = i === ENEMY_MAX, pi = i % 2, g = new THREE.Group(), body = new THREE.Group();
            var mat = makeCreatureMaterial(THREE);
            mat.uniforms.uPh.value = Math.random() * 6.283;
            var m = new THREE.Mesh(isBoss ? creat.boss[pi] : creat.geoms[pi], mat);
            m.frustumCulled = false;
            body.add(m);
            var gs = glowSprite(enemyGlowTex[pi]);          // eye-core glow
            gs.position.set(CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2] - 0.02); gs.scale.setScalar(0.24);
            body.add(gs);
            g.add(body);
            g.scale.setScalar(E_L);
            g.visible = false;
            combatRoot.add(g);
            var rec = {
                g: g, body: body, mat: mat, glow: gs, mesh: m, alive: false, hp: 0, maxHp: 0, state: 0, timer: 0, fireCd: 0,
                wander: new THREE.Vector3(), pass: new THREE.Vector3(), pal: pi, isBoss: isBoss,
                len: E_L, R: E_R, phase: Math.random() * 6.283, hitT: 0, orbCd: 0, side: 1, flip: 5,
                speed: 0, turn: 1, dive: 1.6, near: 0, rpSet: false, off: new THREE.Vector3(), dmg: 3, fireInt: 1, err: 0.1, lead: 0, cone: 0.2, curSpeed: 0,
                // rev 9: role, flee, weave, sniper, boss phases
                role: 0, fstate: 'idle', ftimer: 0, fleeN: 0, flinch: 0, grace: 0, hunt: null, burst: 0, charge: 0, snCd: 0, idle: 0, far: 0,
                wAmp: 0, wPer: 2, wPh: 0, wSide: 1, slantT: 0, rnd: Math.random(), beam: null, aim: new THREE.Vector3(), vel: new THREE.Vector3(), prev: new THREE.Vector3(), first: true,
                bphase: 1, open: false, cyc: 0, wind: 0, volley: 0, volleyCd: 0, pflash: 0, bSpeed: 0, bDmg: 0,
                // rev 12: world-anchored path (leashed base + fixed-plane spiral), flee dive/skim, boss signature glow
                base: new THREE.Vector3(), hasBase: false, pu: new THREE.Vector3(1, 0, 0), pv: new THREE.Vector3(0, 0, 1), rpT: 0, fp: null, skim: 0, skimOn: false, fdive: 0, sigGlow: 0
            };
            enemies.push(rec);
            if (isBoss) boss = rec;
        }
    })();
    var spawnPad = E_L, bossTitle = '';

    // projectiles: records over fx.spawnBolt (js/ship-fx.js owns position + rendering); collision stays here
    var C_P = 0x00f0ff, C_E = 0xff2a1a, C_A = 0xb888ff, BURST_COL = [0x00f0ff, 0xff7a2a, 0xc8ff3a];
    var bolts = [], vBp = new THREE.Vector3(), vFb = new THREE.Vector3(), vFd = new THREE.Vector3();
    (function buildBolts() {
        for (var i = 0; i < BOLT_MAX; i++) bolts.push({ id: -1, prev: new THREE.Vector3(), active: false, enemy: false, dmg: 0, remote: false, owner: null, boss: false, gz: 0 });
    })();
    function killBolts() { for (var i = 0; i < bolts.length; i++) if (bolts[i].active) { bolts[i].active = false; fx.killBolt(bolts[i].id); } }
    // boss homing orbs (pooled)
    var orbGeom = new THREE.SphereGeometry(1, 10, 8);
    var orbMat = new THREE.MeshBasicMaterial({ color: 0xff3ad0, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.9 });
    var orbGlowTex = glowTexture(255, 60, 200);
    var orbs = [];
    (function buildOrbs() {
        for (var i = 0; i < ORB_MAX; i++) {
            var m = new THREE.Mesh(orbGeom, orbMat);
            m.visible = false; m.frustumCulled = false;
            var gs = glowSprite(orbGlowTex); gs.scale.setScalar(3.2); m.add(gs);
            combatRoot.add(m);
            orbs.push({ m: m, vel: new THREE.Vector3(), life: 0, hp: 0, active: false });
        }
    })();

    // rev 12: planet surfaces (js/ship-planet.js, loaded lazily; everything below works if it is missing or throws)
    var ps = null, psCalls = 0, fOut = { r: 0, n: null };
    function planetFail(e) { console.info('[ship] planet surfaces unavailable', e); ps = null; }
    import('./ship-planet.js').then(function (m) {
        try { ps = m.createPlanetSurface(engine, L); } catch (e) { planetFail(e); }
    }).catch(function (e) { console.info('[ship] planet surfaces unavailable', e); });

    // rev 12: boss signature-attack pools: one beam, 12 ring orbs, 8 mines
    var sigBeam = new THREE.Mesh(new THREE.CylinderGeometry(1, 1, 1, 8, 1, true), new THREE.MeshBasicMaterial({ color: 0xff3ad0, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.9, side: THREE.DoubleSide }));
    sigBeam.visible = false; sigBeam.frustumCulled = false; combatRoot.add(sigBeam);
    var sigOrbs = [], mines = [];
    var mineMat = new THREE.MeshBasicMaterial({ color: 0xffa020, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.95 });
    var fuseMat = new THREE.MeshBasicMaterial({ color: 0xff2a2a, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.45 });
    (function buildSig() {
        var i, m, sh;
        for (i = 0; i < 12; i++) { m = new THREE.Mesh(orbGeom, orbMat); m.visible = false; m.frustumCulled = false; combatRoot.add(m); sigOrbs.push(m); }
        for (i = 0; i < 8; i++) {
            m = new THREE.Mesh(orbGeom, mineMat); m.visible = false; m.frustumCulled = false;
            sh = new THREE.Mesh(orbGeom, fuseMat); sh.visible = false; sh.scale.setScalar(1.7); m.add(sh);
            combatRoot.add(m);
            mines.push({ m: m, shell: sh, vel: new THREE.Vector3(), fuse: -1, life: 0, active: false });
        }
    })();

    // rev 9c: kill-dropped shield orbs (pooled, in-world pickups; violet = shield)
    var dropGeom = new THREE.SphereGeometry(1, 8, 6);
    var dropMat = new THREE.MeshBasicMaterial({ color: 0xa070ff, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.95 });
    var dropGlowTex = glowTexture(150, 110, 255);
    var drops = [];
    (function buildDrops() {
        for (var i = 0; i < DROP_MAX; i++) {
            var m = new THREE.Mesh(dropGeom, dropMat);
            m.visible = false; m.frustumCulled = false;
            var gs = glowSprite(dropGlowTex); gs.scale.setScalar(5); m.add(gs);
            combatRoot.add(m);
            drops.push({ m: m, vel: new THREE.Vector3(), life: 0, val: 0, active: false });
        }
    })();
    function makeBeam() {             // sniper charge beam: one thin additive Line per slot, created the first time that slot is a sniper
        var g = new THREE.BufferGeometry();
        g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
        var l = new THREE.Line(g, new THREE.LineBasicMaterial({ color: 0xbff4ff, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false }));
        l.frustumCulled = false; l.visible = false; combatRoot.add(l);
        return l;
    }

    // ─── state ──────────────────────────────────────────────────────
    var state = 'docked';           // docked | piloting | away
    var dockA = 1.3, clockT = 0;
    var vel = new THREE.Vector3(), speed = 0, throttle = 0, pulse = 0;
    var mdx = 0, mdy = 0;           // mouse deltas accumulated since last frame (consumed directly)
    var keys = Object.create(null);
    var camPos = new THREE.Vector3(), camQuat = new THREE.Quaternion();
    var fov = baseFov;
    var target = null;              // current body record under the reticle
    var edgeNow = false;
    // game
    var stepN = 0, gt = 0, hp = HP_MAX, sinceHit = 99, wave = 0, nextWave = FIRST_WAVE, kills = 0, wavePending = true;
    var vE = new THREE.Vector3(), vS = new THREE.Vector3();
    // persistence ('nmg' = {wave, kills, bestWave}); every access try/catch'd
    var bestWave = 0, resumeWave = 1, resumeKills = 0;
    var users = {}, curUser = '';             // profiles (/user NAME, /color), persisted in the 'nmg' save: users{name:{v,color,hull,upgrades,kills,bestWave,created}}, user
    var difficulty = 2, peaceful = false;
    // rev 11+: every save blob and profile carries a schema version `v`; ONE migrate() fills missing fields with defaults
    // and never deletes unknown fields (a newer/older game version round-trips them untouched).
    var SCHEMA_V = 1;
    var COLOR_NAMES = { orange: 0xFF7A1A, violet: 0x8a5cff, cyan: 0x00f0ff, red: 0xff3a2a, green: 0x3aff7a, white: 0xffffff };
    function pget(o, k, d) { var v = o ? o[k] : undefined; return (v === undefined || v === null) ? d : v; }   // accessor: never read raw
    function migrateProfile(p) {
        var o = (p && typeof p === 'object' && !Array.isArray(p)) ? p : {};
        if (!(o.v >= 1)) o.v = SCHEMA_V;
        if (typeof o.color !== 'number') o.color = null;
        if (typeof o.hull !== 'string') o.hull = 'hauler';
        if (!o.upgrades || typeof o.upgrades !== 'object') o.upgrades = {};
        o.kills = Math.max(0, o.kills | 0); o.bestWave = Math.max(0, o.bestWave | 0);
        if (!(o.created > 0)) o.created = Date.now();
        return o;
    }
    function migrate(raw) {
        var o = (raw && typeof raw === 'object' && !Array.isArray(raw)) ? raw : {};
        if (!(o.v >= 1)) o.v = SCHEMA_V;
        o.wave = Math.max(1, o.wave | 0); o.kills = Math.max(0, o.kills | 0); o.bestWave = Math.max(0, o.bestWave | 0);
        o.difficulty = clamp(o.difficulty | 0, 1, 3) || 2; o.peaceful = !!o.peaceful;
        if (!o.users || typeof o.users !== 'object' || Array.isArray(o.users)) o.users = {};
        Object.keys(o.users).forEach(function (k) { o.users[k] = migrateProfile(o.users[k]); });
        if (!Object.prototype.hasOwnProperty.call(o.users, 'claude')) o.users.claude = migrateProfile({ color: 0xFF7A1A });   // built-in test account: orange
        if (typeof o.user !== 'string') o.user = '';
        return o;
    }
    var saveData = migrate(null);
    function loadSave() {
        try { var o = JSON.parse(localStorage.getItem(SAVE_KEY) || 'null'); if (o && typeof o === 'object') saveData = migrate(o); } catch (e) { /* storage unavailable */ }
        return saveData;
    }
    function flushSave() { try { localStorage.setItem(SAVE_KEY, JSON.stringify(saveData)); } catch (e) { /* ignore */ } }
    function shownWave() { return Math.max(1, wavePending ? wave + 1 : wave); }
    function writeSave() {
        var w = shownWave();
        if (w > bestWave) bestWave = w;
        var pf = curProfile();
        if (pf) { pf.kills = Math.max(pget(pf, 'kills', 0) | 0, kills); pf.bestWave = Math.max(pget(pf, 'bestWave', 0) | 0, bestWave); }
        saveData.wave = w; saveData.kills = kills; saveData.bestWave = bestWave; saveData.difficulty = difficulty; saveData.peaceful = peaceful; saveData.users = users; saveData.user = curUser;
        flushSave();
    }
    (function restore() {
        var sv = loadSave();
        difficulty = sv.difficulty; peaceful = sv.peaceful; resumeWave = RESUME_WAVE ? sv.wave : 1; resumeKills = sv.kills; bestWave = Math.max(sv.bestWave, sv.wave); users = sv.users; curUser = sv.user;
    })();
    // profiles: the current user (default 'pilot-' + last 4 of the anonymous id) owns color + hull; hullFor is the one seam a ship registry plugs into
    function userName(id) {
        if (curUser) return curUser;
        var t = id || (net && net.id) || '';
        if (!t) { try { t = localStorage.getItem('nmg-id') || ''; } catch (e) { /* ignore */ } }
        return 'pilot-' + (t.slice(-4) || 'anon');
    }
    function curProfile() { var n = userName(); return (users && Object.prototype.hasOwnProperty.call(users, n)) ? users[n] : null; }
    function ensureProfile(name) { if (!Object.prototype.hasOwnProperty.call(users, name)) users[name] = migrateProfile({}); return users[name]; }
    function hullFor(profile) { return pget(profile, 'hull', 'hauler'); }
    function buildHullKind(m, kind) { return m.buildHull(THREE); }      // only 'hauler' exists today
    function profileColor() { var c = pget(curProfile(), 'color', null); return typeof c === 'number' ? c : null; }
    function effColor() { var c = profileColor(); return c == null ? 0x8a5cff : c; }
    function hexCss(c) { return '#' + ('000000' + (c >>> 0).toString(16)).slice(-6); }

    var dead = false, deathT = 0, waveMsgT = 0, curWave = waveParams(1);
    var bossWave = false;
    var firing = false, fireCd = 0, fireSide = 0, playerFired = false;   // playerFired: allies hold fire until the player fires this wave
    var gameFresh = true;
    // rev 9 combat state (all plain numbers / pooled objects: nothing allocates per frame)
    var waveActive = false, waveStartT = 0, qAt = [], qRole = [], qTry = [], qN = 0, dirA = new THREE.Vector3(1, 0, 0), dirB = new THREE.Vector3(-1, 0, 0);
    var rollT = 0, rollCd = 0, rollDir = 1, tapT = { KeyA: 0, KeyD: 0 }, driftT = 0, driftBoost = 0, wasBoost = false, pulseT = 0;
    var chainN = 0, chainT = 0, graze = 0, grazeIdle = 0, od = 0, focusE = FOCUS_MAX, focusing = false, zT = 0, zTarget = null;
    var fovKick = 0, hitStopN = 0, comboN = 0, comboT = 0, tickPunch = 0, ctarget = null, tlock = false, atT = 0, timeScale = 1;
    var ramInv = 0, ramFlash = 0, bounceV = new THREE.Vector3(), cRam = false, cAtmo = -1;
    var tailAlly = null, tailBy = null, tailLeft = 0, tailBase = 0, lastHitStop = 0;
    var prof = { on: false, ai: 0, bolts: 0, fx: 0, hud: 0, net: 0, flight: 0, total: 0, n: 0 };
    var net = null, hullMod = null, boostNow = false;   // multiplayer (js/ship-net.js), null until/unless the relay link exists

    // scratch (no per-frame allocation)
    var vF = new THREE.Vector3(), vU = new THREE.Vector3();
    var vA = new THREE.Vector3(), vB = new THREE.Vector3(), vP0 = new THREE.Vector3(), vTmp = new THREE.Vector3();
    var vM = new THREE.Vector3(), vD = new THREE.Vector3(), vAim = new THREE.Vector3();
    var qA = new THREE.Quaternion(), qB = new THREE.Quaternion();
    var mM = new THREE.Matrix4(), X = new THREE.Vector3(1, 0, 0), Y = new THREE.Vector3(0, 1, 0), Z = new THREE.Vector3(0, 0, 1);
    var NEG_Z = new THREE.Vector3(0, 0, -1), ORIGIN = new THREE.Vector3();

    // ─── DOM: label + HUD ───────────────────────────────────────────
    var label = document.createElement('button');
    label.className = 'ship-label'; label.type = 'button'; label.textContent = 'YOUR SHIP';
    label.setAttribute('aria-label', 'Board your ship');
    document.body.appendChild(label);
    label.addEventListener('click', function (e) { e.preventDefault(); enter(); });
    var lastLX = -1e9, lastLY = -1e9, labelShown = true, labelTxt = 'YOUR SHIP', labelCol = -1;
    function updateLabelLook() {          // 'YOUR SHIP' until a /user name is remembered; theme color from the profile
        var t = (curUser && !/^pilot-[a-z0-9]{1,4}$/i.test(curUser)) ? curUser.toUpperCase() : 'YOUR SHIP';
        if (t !== labelTxt) { labelTxt = t; label.textContent = t; }
        var c = effColor();
        if (c !== labelCol) { labelCol = c; label.style.color = hexCss(c); label.style.borderColor = hexCss(c) + '99'; }
    }

    function clearSave() {
        resumeWave = 1; resumeKills = 0; bestWave = 0;
        saveData.wave = 1; saveData.kills = 0; saveData.bestWave = 0;
        flushSave();
    }
    var resetEl = document.createElement('button');
    resetEl.className = 'ship-reset'; resetEl.type = 'button'; resetEl.textContent = 'reset';
    resetEl.title = 'Clear NO MANS GOR progress (wave, kills)';
    resetEl.setAttribute('aria-label', 'Reset NO MANS GOR progress');
    document.body.appendChild(resetEl);
    resetEl.addEventListener('click', function (e) {
        e.preventDefault(); e.stopPropagation();
        clearSave();
        if (state === 'away') state = 'docked';           // next entry starts fresh from the dock
    });

    var hud = document.createElement('div');
    hud.id = 'ship-hud';
    hud.innerHTML =
        '<div class="sh-ret"><i></i><i></i><i></i><i></i></div>' +
        '<div class="sh-roll"></div><div class="sh-chain"></div><div class="sh-pip"></div><div class="sh-lock"></div><div class="sh-tint"></div>' +
        '<div class="sh-arrows"></div>' +
        '<div class="sh-target"></div>' +
        '<div class="sh-edge"></div>' +
        '<div class="sh-boss"><span></span><b><u></u><s></s><s></s></b><em></em></div>' +
        '<div class="sh-wave"></div>' +
        '<div class="sh-hp"><span>SHIELD <em>100</em></span><b><u></u></b></div>' +
        '<div class="sh-graze"><span>GRAZE</span><b><u></u></b></div><div class="sh-focus"><span>FOCUS</span><b><u></u></b></div>' +
        '<div class="sh-atmo"><span>ATMOSPHERE</span><b><u></u></b></div><div class="sh-ram"></div>' +
        '<div class="sh-stats"><div class="sh-st-wave"></div><div class="sh-st-en"></div><div class="sh-st-al"></div><div class="sh-st-kills"></div></div>' +
        '<div class="sh-speed"></div>' +
        '<div class="sh-bars">' +
        '<div class="sh-bar sh-thr"><span>THR</span><b><u></u></b></div>' +
        '<div class="sh-bar sh-pul"><span>PULSE</span><b><u></u></b></div></div>' +
        '<div id="ship-chat"><div class="sc-log"></div></div>' +
        '<input id="ship-cmd" type="text" autocomplete="off" autocapitalize="off" spellcheck="false" maxlength="420" aria-label="Chat or command">' +
        '<div class="sh-hint">ESC exit · LMB fire · SPACE pulse · SHIFT boost · A/D x2 roll · T target · Q focus · Z focus fire · ENTER chat · / commands</div>';
    document.body.appendChild(hud);
    var elAtmo = hud.querySelector('.sh-atmo'), elAtmoU = elAtmo.querySelector('u'), elRam = hud.querySelector('.sh-ram');
    var elTarget = hud.querySelector('.sh-target'), elSpeed = hud.querySelector('.sh-speed');
    var elEdge = hud.querySelector('.sh-edge'), elWave = hud.querySelector('.sh-wave');
    var elHpU = hud.querySelector('.sh-hp u'), elHpN = hud.querySelector('.sh-hp em'), elHp = hud.querySelector('.sh-hp');
    var elStWave = hud.querySelector('.sh-st-wave'), elStEn = hud.querySelector('.sh-st-en'), elStKills = hud.querySelector('.sh-st-kills');
    var elBoss = hud.querySelector('.sh-boss'), elBossN = elBoss.querySelector('span'), elBossU = elBoss.querySelector('u'), elStAl = hud.querySelector('.sh-st-al');
    var cBossN = '', cBossP = -1, cBossShow = false, cAl = '';
    var elThr = hud.querySelector('.sh-thr'), elThrU = elThr.querySelector('u');
    var elPulU = hud.querySelector('.sh-pul u');
    var elRet = hud.querySelector('.sh-ret'), cLocked = false;
    var elRoll = hud.querySelector('.sh-roll'), elChain = hud.querySelector('.sh-chain'), elPip = hud.querySelector('.sh-pip'), elLock = hud.querySelector('.sh-lock');
    var elGraze = hud.querySelector('.sh-graze'), elGrazeU = elGraze.querySelector('u'), elFocus = hud.querySelector('.sh-focus'), elFocusU = elFocus.querySelector('u'), elTint = hud.querySelector('.sh-tint');
    var elArrows = hud.querySelector('.sh-arrows'), arrows = [], elBossPh = elBoss.querySelector('em'), elBossS = elBoss.querySelectorAll('s');
    var cRoll = -1, cChain = '', cGraze = -1, cFocus = -1, cOd = false, cFocusOn = false, cPip = '', cPipSolid = false, cLockTxt = '', cTickK = -1, cBossPh = '';
    (function buildArrows() {       // pooled bearing chevrons: 0..ENEMY_MAX = hostiles (boss = last), +1 = wingman-in-peril
        for (var i = 0; i < ENEMY_MAX + 2; i++) {
            var a = document.createElement('i'); a.className = 'sh-arr' + (i === ENEMY_MAX + 1 ? ' is-ally' : ''); a.style.display = 'none';
            elArrows.appendChild(a); arrows.push({ el: a, on: false, x: -1e9, y: -1e9, r: -999 });
        }
    })();
    var cTarget = '', cSpeed = '', cThr = -1, cPul = -1, cRev = false, cPulsing = false;
    var cTargetEnemy = '', cHp = -1, cWave = '', cEn = '', cKills = '', cEdge = false, cLow = false, cWaveMsg = '', cWaveShow = false, cDead = false;

    function updateHud(speedNow, enemiesAlive) {
        var s = (speedNow < 10 ? speedNow.toFixed(1) : String(Math.round(speedNow))) + ' u/s';
        if (s !== cSpeed) { cSpeed = s; elSpeed.textContent = s; }
        var t = Math.round(Math.abs(throttle) * 100);
        if (t !== cThr) { cThr = t; elThrU.style.transform = 'scaleX(' + (t / 100) + ')'; }
        var rev = throttle < -0.001;
        if (rev !== cRev) { cRev = rev; elThr.classList.toggle('is-rev', rev); }
        var p = Math.round(pulse * 100);
        if (p !== cPul) { cPul = p; elPulU.style.transform = 'scaleX(' + (p / 100) + ')'; }
        var pulsing = pulse > 0.5;
        if (pulsing !== cPulsing) { cPulsing = pulsing; hud.classList.toggle('is-pulsing', pulsing); }
        var ts = '';
        if (cTargetEnemy) ts = cTargetEnemy;
        else if (target) ts = (target.node.title || target.node.id || '').toUpperCase() + '  ·  ' + fmtDist(target.dist);
        if (ts !== cTarget) { cTarget = ts; elTarget.textContent = ts; }
        // combat readouts
        var h = Math.max(0, Math.round(hp));
        if (h !== cHp) {
            cHp = h; elHpU.style.transform = 'scaleX(' + (h / HP_MAX) + ')'; elHpN.textContent = h;
            var low = h <= 30;
            if (low !== cLow) { cLow = low; elHp.classList.toggle('is-low', low); }
        }
        var ws = peaceful ? 'PEACEFUL' : 'WAVE ' + shownWave();
        if (ws !== cWave) { cWave = ws; elStWave.textContent = ws; }
        var es = 'ENEMIES ' + enemiesAlive;
        if (es !== cEn) { cEn = es; elStEn.textContent = es; }
        var na = 0; for (var ai = 0; ai < allies.length; ai++) if (allies[ai].alive) na++;
        var als = na ? 'ALLIES ' + na : '';
        if (als !== cAl) { cAl = als; elStAl.textContent = als; }
        var bshow = boss.alive;
        if (bshow !== cBossShow) { cBossShow = bshow; elBoss.classList.toggle('is-on', bshow); }
        if (bshow) {
            if (bossTitle !== cBossN) { cBossN = bossTitle; elBossN.textContent = bossTitle; }
            var bp = Math.max(0, Math.round(boss.hp / boss.maxHp * 200));
            if (bp !== cBossP) { cBossP = bp; elBossU.style.transform = 'scaleX(' + (bp / 200) + ')'; }
            var bph = bsig.ph === 'tele' ? BOSS_ATTACKS[bsig.idx] + ' · INCOMING' : 'PHASE ' + boss.bphase + (boss.open ? ' · CORE EXPOSED' : '');
            if (bph !== cBossPh) { cBossPh = bph; elBossPh.textContent = bph; elBoss.classList.toggle('is-open', boss.open); }
        }
        var ks = 'KILLS ' + kills;
        if (ks !== cKills) { cKills = ks; elStKills.textContent = ks; }
        if (edgeNow !== cEdge) { cEdge = edgeNow; elEdge.textContent = edgeNow ? 'EDGE OF KNOWN SPACE' : ''; elEdge.classList.toggle('is-on', edgeNow); }
        var show = waveMsgT > 0;
        if (show !== cWaveShow) { cWaveShow = show; elWave.classList.toggle('is-show', show); }
        if (dead !== cDead) { cDead = dead; hud.classList.toggle('is-dead', dead); }
        var rf = ramFlash > 0;
        if (rf !== cRam) { cRam = rf; elRam.classList.toggle('is-on', rf); }
        var am = 0;
        if (ps) { try { am = (ps.active && ps.depth > 0.01) ? Math.round(clamp(ps.depth, 0, 1) * 50) / 50 : 0; } catch (e) { planetFail(e); } }
        if (am !== cAtmo) { cAtmo = am; elAtmoU.style.transform = 'scaleX(' + am + ')'; elAtmo.classList.toggle('is-on', am > 0); }
    }
    function announce(msg) {
        waveMsgT = 2.8;
        if (msg !== cWaveMsg) { cWaveMsg = msg; elWave.textContent = msg; }
    }

    // ─── bodies (real nodes only) ────────────────────────────────────
    var scratchBodies = [];
    function realRadius(n) {
        var r = engine.renderedRadius(n);
        return r > 0 && isFinite(r) ? r : (n.bodyR || L);
    }
    // Reuses record objects so there is no per-frame garbage.
    var realRecs = new Map();
    function gatherBodies() {
        var out = scratchBodies, c = 0, i, d = engine.drawOrder;
        for (i = 0; i < d.length; i++) {
            var n = d[i];
            if (!n.anchor) continue;
            var rec = realRecs.get(n);
            if (!rec) { rec = { node: n, R: 0, dist: 0, px: 0, py: 0, pz: 0, pN: -9 }; realRecs.set(n, rec); }
            rec.R = realRadius(n);
            out[c++] = rec;
        }
        out.length = c;
        return out;
    }
    // push a point out of every body's shell (factor x R); two passes so
    // resolving one body cannot leave it inside a neighbour
    // rev 12: terrain floor of the active planet (null = unavailable / over the 18-call frame budget). Budget keeps the JS noise port <= 20 calls a frame.
    function psFloor(p) {
        if (!ps || psCalls >= 18) return null;
        psCalls++;
        try { var o = ps.floorAt(p, fOut) || fOut; return o.r > 0 ? o : null; } catch (e) { planetFail(e); return null; }
    }
    // push an actor above the terrain of the active planet when it is inside 1.25 R (replaces the hard shell there)
    function terrainPush(p, rec, floorL) {
        var c = rec.node.anchor.position;
        vTmp.subVectors(p, c);
        var d = vTmp.length();
        if (d >= 1.25 * rec.R) return false;
        var o = psFloor(p), fr = (o ? o.r : rec.R * 1.04) + floorL * L;
        if (d < fr) {
            if (d < 1e-3) vTmp.set(1, 0, 0); else vTmp.divideScalar(d);
            p.copy(c).addScaledVector(vTmp, fr);
        }
        return true;
    }
    // floorL given (enemies / allies): inside 1.25 R of the active planet the actor rides the terrain floor + floorL L instead of the shell
    function pushOutOfBodies(p, bodies, f, floorL) {
        var pn = (floorL !== undefined && ps) ? ps.active : null;
        for (var pass = 0; pass < 2; pass++) {
            for (var i = 0; i < bodies.length; i++) {
                if (pn && bodies[i].node === pn) { if (pass === 0 && terrainPush(p, bodies[i], floorL)) continue; else if (pass > 0 && p.distanceToSquared(bodies[i].node.anchor.position) < 1.5625 * bodies[i].R * bodies[i].R) continue; }
                var c = bodies[i].node.anchor.position, sh = f * bodies[i].R;
                vTmp.subVectors(p, c);
                var d = vTmp.length();
                if (d < sh) {
                    if (d < 1e-3) vTmp.set(1, 0, 0); else vTmp.divideScalar(d);
                    p.copy(c).addScaledVector(vTmp, sh);
                }
            }
        }
    }

    // ─── input ──────────────────────────────────────────────────────
    function locked() { return document.pointerLockElement != null; }
    window.addEventListener('mousemove', function (e) {
        if (state !== 'piloting' || !locked()) return;
        mdx += e.movementX || 0; mdy += e.movementY || 0;
    });
    document.addEventListener('mousedown', function (e) {
        if (state !== 'piloting' || e.button !== 0 || cmdOpen) return;
        firing = true;
    }, true);
    document.addEventListener('mouseup', function (e) { if (e.button === 0) firing = false; }, true);
    window.addEventListener('keydown', function (e) {
        if (state !== 'piloting' || cmdOpen) return;
        if (e.key === '/') { e.preventDefault(); openCmd('/'); return; }
        if (e.code === 'KeyT' && !e.ctrlKey && !e.metaKey && !e.altKey) { e.preventDefault(); if (!e.repeat) cycleTarget(); return; }      // rev 9b: T cycles targets; chat is Enter or /
        if (e.key === 'Enter' && !e.ctrlKey && !e.metaKey && !e.altKey) { e.preventDefault(); openCmd(''); return; }
        if (e.code === 'Escape') {
            // engine's Esc handler may have just called enter() for this same keydown,
            // and it can also run after us: ignore early Escapes and mark handled ones
            if (e.defaultPrevented || e.__shipHandled || performance.now() - enteredAt < 150) return;
            e.__shipHandled = true;
            // with no pointer lock the browser never reports the Esc, so do it here.
            // Deferred a tick so the engine's own Esc handler (enter() = no-op while
            // piloting) cannot fire after exit() re-arms onEscape and bounce us back in.
            if (!locked()) setTimeout(function () { exit(); }, 0);
            return;
        }
        if (e.code === 'Space' || e.code === 'Tab' || e.code.indexOf('Arrow') === 0) e.preventDefault();
        if (e.repeat) { keys[e.code] = true; return; }
        keys[e.code] = true;
        if (e.code === 'KeyA' || e.code === 'KeyD') tapKey(e.code);
        else if (e.code === 'KeyZ') zFocusFire();
        if (e.code === 'KeyF' && target && !dead) { exit(target.node); }
    }, true);
    window.addEventListener('keyup', function (e) { keys[e.code] = false; });
    window.addEventListener('blur', function () { keys = Object.create(null); firing = false; });
    document.addEventListener('pointerlockchange', function () {
        if (state === 'piloting' && !locked() && !cmdOpen && performance.now() > cmdGuardUntil) exit();
    });

    // clicking the (huge) hull boards just like the label does; only YOUR ship is clickable
    var rayC = new THREE.Raycaster(), ndcC = new THREE.Vector2();
    window.addEventListener('click', function (e) {
        if (state === 'piloting' || e.button !== 0 || e.target !== engine.renderer.domElement) return;
        ndcC.set(e.clientX / window.innerWidth * 2 - 1, -(e.clientY / window.innerHeight) * 2 + 1);
        rayC.camera = camera;
        rayC.setFromCamera(ndcC, camera);
        shipRoot.updateMatrixWorld(true);
        if (rayC.intersectObject(shipRoot, true).length) { e.stopPropagation(); e.preventDefault(); enter(); }
    }, true);

    // ─── dock ───────────────────────────────────────────────────────
    function placeAtDock() {
        var r = DOCK_R * root.sysR;
        shipRoot.position.set(Math.cos(dockA) * r, DOCK_Y * root.sysR + Math.sin(clockT * 0.7) * L * 0.15, Math.sin(dockA) * r);
        pushOutOfBodies(shipRoot.position, gatherBodies(), 2.2);      // dock is always well clear of every body
        vA.set(-Math.sin(dockA), 0, Math.cos(dockA));
        mM.lookAt(vTmp.set(0, 0, 0), vA, Y);
        shipRoot.quaternion.setFromRotationMatrix(mM);
        shipRoot.quaternion.multiply(qA.setFromAxisAngle(Y, Math.sin(clockT * 0.25) * 0.5));      // slow yaw so it reads as a ship
    }

    // ─── enter / exit ───────────────────────────────────────────────
    var enteredAt = -1e9;
    var boarding = false, exiting = false, boardT = 1, exitT = 0, cineS0 = L, pendingFocus = null;
    var cineQ0 = new THREE.Quaternion(), cineQ1 = new THREE.Quaternion(), cineR0 = new THREE.Vector3(), cineR1 = new THREE.Vector3();
    var hudOp = -1;
    function setShipScale(s) { shipScale = s; shipRoot.scale.setScalar(s); }
    function setHudOpacity(v) { v = Math.round(v * 100) / 100; if (v !== hudOp) { hudOp = v; hud.style.opacity = v >= 1 ? '' : String(v); } }
    // camera orbits the ship's centre: log-lerped distance, slerped direction, slerped orientation
    function cinePose(e) {
        var m0 = Math.max(1e-6, cineR0.length()), m1 = Math.max(1e-6, cineR1.length());
        var mag = Math.exp(Math.log(m0) + (Math.log(m1) - Math.log(m0)) * e);
        vA.copy(cineR0).divideScalar(m0); vB.copy(cineR1).divideScalar(m1);
        qA.setFromUnitVectors(vA, vB);
        qB.identity().slerp(qA, e);
        vA.applyQuaternion(qB).multiplyScalar(mag);
        camera.position.copy(shipRoot.position).add(vA);
        camera.quaternion.copy(cineQ0).slerp(cineQ1, e);
    }
    function logLerp(a, b, e) { return Math.exp(Math.log(a) + (Math.log(b) - Math.log(a)) * e); }
    function cinematicStep(dt) {
        var e;
        if (exiting) {
            exitT = Math.min(1, exitT + dt / BOARD_DUR); e = easeInOut(exitT);
            setShipScale(exitT >= 1 ? mineS : logLerp(cineS0, mineS, e));
            cinePose(e);
            setHudOpacity(1 - clamp(exitT * BOARD_DUR / 0.4, 0, 1));
            syncLod();
        } else {
            boardT = Math.min(1, boardT + dt / BOARD_DUR); e = easeInOut(boardT);
            setShipScale(boardT >= 1 ? L : logLerp(cineS0, L, e));
            cinePose(e);
            setHudOpacity(clamp((boardT * BOARD_DUR - (BOARD_DUR - 0.4)) / 0.4, 0, 1));
            syncLod();
        }
        camera.updateMatrixWorld(true);
        exMe.holder.visible = shipScale <= 1.5 * L && !dead;
        if (exMe.holder.visible) exUpdate(exMe, shipRoot.position, shipRoot.quaternion, shipScale, 0.3);
        fx.setMotion(vel, 0, false); fx.update(dt, camera);
        if (net) { net.sendPos(); net.update(dt, dockA); ghostFx(); }
        if (exiting && exitT >= 1) finishExit();
        else if (!exiting && boardT >= 1) {
            boarding = false; setHudOpacity(1); mdx = mdy = 0; fov = camera.fov;
            chaseTargets(0); camera.position.copy(camPos); camera.quaternion.copy(camQuat); camera.updateMatrixWorld(true);   // land EXACTLY on the chase pose
        }
    }
    // exhaust on remote pilots' hulls while they fly (cones pooled and reused as pilots come and go)
    var ghostEx = new Map(), freeEx = [];
    function ghostFx() {
        var gh = net.ghosts;
        ghostEx.forEach(function (ex, id) { var g = gh.get(id); if (!g || !g.hull) { exOff(ex); ghostEx.delete(id); freeEx.push(ex); } });
        gh.forEach(function (g, id) {
            if (!g.hull) return;
            var ud = g.hull.userData;
            if (ud.fu) { var wf = g.s === 'piloting'; if (ud.fu.visible !== wf) { ud.fu.visible = wf; ud.lo.visible = !wf; } }
            var ex = ghostEx.get(id);
            if (!ex) { ex = freeEx.pop() || makeEx(scene); exSetup(ex, g.hull.userData.thrusters || [], g.color >= 0 ? g.color : 0x8a5cff); ghostEx.set(id, ex); }
            if (ex.col !== g.color && g.color >= 0) exColor(ex, g.color);
            var flying = g.s === 'piloting' && g.samples.length > 0 && g.hull.visible && g.root.scale.x < 20 * L;
            ex.holder.visible = flying;
            if (!flying) return;
            var sv = g.samples[g.samples.length - 1].v || 0;
            exUpdate(ex, g.root.position, g.root.quaternion, g.root.scale.x, (g.st & 2) ? 2 : ((g.st & 1) ? 1 : Math.min(0.7, Math.max(0, sv) / CRUISE * 0.7)));
        });
    }
    function enter() {
        if (state === 'piloting') return;
        stepN += 2;                                        // drop stale body-frame history
        enteredAt = performance.now();                     // engine Esc may call us while already flying
        try {
            var p = document.body.requestPointerLock();
            if (p && p.catch) p.catch(function () { console.log('[ship] pointer lock refused; keyboard only'); });
        } catch (err) { console.log('[ship] pointer lock unavailable; keyboard only'); }
        if (state === 'docked') {
            placeAtDock();
            resetGame(resumeWave, resumeKills);
        }
        vel.set(0, 0, 0); speed = 0; throttle = 0; pulse = 0; pulseT = 0;
        mdx = mdy = 0; firing = false;
        keys = Object.create(null);
        // boarding cinematic: from wherever the camera is right now to the chase pose, hull log-lerping galaxy scale -> true L
        cineS0 = shipScale; cineQ0.copy(camera.quaternion);
        cineR0.copy(camera.position).sub(shipRoot.position);
        chaseTargets(0);
        cineR1.copy(camPos).sub(shipRoot.position); cineQ1.copy(camQuat);
        boardT = 0; boarding = true; exiting = false;
        setHudOpacity(0);
        state = 'piloting';
        syncLod();
        if (ps) { try { ps.setVisible(true); } catch (e) { planetFail(e); } }
        net && net.setState('piloting');
        shipRoot.visible = !dead;
        combatRoot.visible = true; fx.bolts.visible = true;
        document.body.classList.add('is-piloting');
        hud.classList.add('is-on');
        label.style.display = 'none'; labelShown = false;
        engine.setPilot(step);
        engine.onEscape(enter);
    }
    // Leaving reverses the boarding move; the camera is only handed back (finishExit) once the hull is galaxy-scale again.
    function exit(focus) {
        if (state !== 'piloting' || exiting) return;
        exiting = true; boarding = false; exitT = 0;
        if (ps) { try { ps.setVisible(false); } catch (e) { planetFail(e); } }
        pendingFocus = (focus && focus.anchor) ? focus : null;
        net && net.setState('away');
        if (cmdOpen) closeCmd(false);
        firing = false; mdx = mdy = 0;
        keys = Object.create(null);
        throttle = 0; pulse = 0; speed = 0; vel.set(0, 0, 0);
        shipRoot.visible = true;
        hud.classList.remove('is-pulsing');
        cPulsing = false;
        if (locked()) { try { document.exitPointerLock(); } catch (e) { /* ignore */ } }
        cineS0 = shipScale; cineQ0.copy(camera.quaternion); cineQ1.copy(camera.quaternion);
        cineR0.copy(camera.position).sub(shipRoot.position);
        cineR1.copy(cineR0).normalize().multiplyScalar(1.6 * mineS);
    }
    function finishExit() {
        exiting = false; state = 'away'; syncLod();
        engine.setPilot(null);
        engine.onEscape(enter);
        combatRoot.visible = false;          // enemies stay in the scene, frozen and hidden
        hud.classList.remove('is-on', 'is-pulsing');
        setHudOpacity(1);
        document.body.classList.remove('is-piloting');
        camera.fov = baseFov; camera.updateProjectionMatrix();
        killBolts(); exOff(exMe); ghostEx.forEach(exOff);
        fx.setMotion(vel, 0, false); fx.update(1, camera); fx.streaks.visible = false; fx.bolts.visible = false;
        var pf = pendingFocus; pendingFocus = null;
        engine.focusNode(pf || engine.root, true);
        lastLX = lastLY = -1e9;
    }

    // ─── game reset / waves ─────────────────────────────────────────
    // startW = wave number about to begin; kills carried in. Clears every creature, ally, bolt, orb.
    function hideEnemy(e) { e.alive = false; e.g.visible = false; e.charge = 0; if (e.beam) e.beam.visible = false; if (e.isBoss) sigReset(); }
    function resetGame(startW, keepKills) {
        var i;
        for (i = 0; i < enemies.length; i++) hideEnemy(enemies[i]);
        for (i = 0; i < allies.length; i++) { allies[i].alive = false; allies[i].g.visible = false; }
        for (i = 0; i < orbs.length; i++) { orbs[i].active = false; orbs[i].m.visible = false; }
        for (i = 0; i < drops.length; i++) { drops[i].active = false; drops[i].m.visible = false; }
        killBolts();
        hp = HP_MAX; sinceHit = 99; kills = keepKills | 0; gt = 0;
        wave = Math.max(0, (startW | 0) - 1); wavePending = true;
        nextWave = startW > 1 ? DEATH_RESPAWN_DELAY : FIRST_WAVE;
        waveActive = false; qN = 0; chainN = 0; chainT = 0; graze = 0; od = 0; focusE = FOCUS_MAX; zT = 0; zTarget = null;
        tailAlly = null; tailBy = null; tailLeft = 0; ctarget = null; tlock = false; rollT = 0; rollCd = 0; hitStopN = 0; comboN = 0; comboT = 0; driftBoost = 0; pulseT = 0;
        dead = false; deathT = 0; waveMsgT = 0; fireCd = 0; playerFired = false; curWave = waveParams(WAVE_X * Math.max(1, startW)); bossWave = false;
        gameFresh = false;
    }
    function aliveCount() {
        var c = 0;
        for (var i = 0; i < enemies.length; i++) if (enemies[i].alive) c++;
        return c;
    }
    function randDir(out) {
        var u = Math.random() * 2 - 1, th = Math.random() * Math.PI * 2, s = Math.sqrt(1 - u * u);
        return out.set(s * Math.cos(th), u * 0.6, s * Math.sin(th)).normalize();
    }
    // rev 9b #4: 150-250 L from the player (never inside 100 L), around one of the wave's two approach directions
    // (dir) so a wave arrives from two sides; never inside a planet, inside the boundary. Fills vAim.
    function pickSpawn(bodies, dir) {
        var tries = 20, j;
        while (tries--) {
            if (dir && tries > 8) {
                vD.copy(dir); vD.x += (Math.random() - 0.5) * 0.7; vD.y += (Math.random() - 0.5) * 0.4; vD.z += (Math.random() - 0.5) * 0.7; vD.normalize();
            } else randDir(vD);
            var d = (SPAWN_NEAR + Math.random() * (SPAWN_FAR - SPAWN_NEAR)) * L;
            vAim.copy(shipRoot.position).addScaledVector(vD, d);
            if (vAim.length() > EDGE_R * 0.97) continue;
            var ok = true;
            for (j = 0; j < bodies.length; j++) {
                var c = bodies[j].node.anchor.position, r = bodies[j].R * 1.6 + spawnPad;
                if (vAim.distanceToSquared(c) < r * r) { ok = false; break; }
            }
            if (ok) return true;
        }
        return false;
    }
    function isBossWave(w) { return w >= BOSS_FIRST && (w - BOSS_FIRST) % BOSS_EVERY === 0; }
    function baseHp(w) { return clamp(48 + 16 * (w - 1), 48, 144); }       // rev 9b #5: ~6 hits at wave 1, ~10 at wave 3 (x role)
    function pickRole(i, want) {
        var avail = wave >= 3 ? 3 : (wave >= 2 ? 2 : 1);
        if (i === 0 || avail === 1) return 0;
        if (i === 1 && want >= 3) return 1;                     // every wave shows each unlocked role once
        if (i === 2 && avail >= 3 && want >= 4) return 2;
        var tot = 0, k, r;
        for (k = 0; k < avail; k++) tot += ROLE_T[k].mix;
        r = Math.random() * tot;
        for (k = 0; k < avail; k++) { r -= ROLE_T[k].mix; if (r <= 0) return k; }
        return 0;
    }
    function applyRole(e, r) {
        e.role = r;
        e.mesh.geometry = creat.roles[r];
        e.glow.material.map = enemyGlowTex[r];
        e.glow.position.set(CREATURE_EYE[0], CREATURE_EYE[1], roleEyeZ(r) - 0.02);
        if (r === 2 && !e.beam) e.beam = makeBeam();
    }
    // vAim = spawn position (set by pickSpawn / the caller)
    function initEnemy(e, role, grace) {
        var cw = curWave, D = DIFFS[difficulty], R = ROLE_T[role];
        applyRole(e, role);
        e.alive = true; e.hp = e.maxHp = Math.max(8, Math.round(baseHp(wave) * R.hp * D.hp));
        e.state = 0; e.fstate = 'patrol'; e.timer = 0; e.fireCd = 1 + Math.random() * cw.fireInt * R.fire; e.burst = 0;
        e.dive = cw.dive; e.bSpeed = cw.speed * R.spd; e.bDmg = role === 2 ? SNIPER_DMG : cw.dmg * R.dmg;
        e.speed = e.bSpeed * D.spd; e.turn = cw.turn * R.turn; e.dmg = e.bDmg * D.dmg; e.fireInt = cw.fireInt * R.fire;
        e.err = cw.err; e.lead = cw.lead; e.cone = cw.cone; e.curSpeed = e.speed * 0.4; e.hitT = 0; e.flinch = 0;
        e.hasBase = false; e.fp = null; e.skimOn = false; e.sigGlow = 0;
        e.fleeN = 0; e.ftimer = 0; e.charge = 0; e.snCd = 3 + Math.random() * 2; e.idle = 0; e.far = 0; e.grace = grace; e.hunt = null; e.first = true; e.open = false;
        e.wAmp = (3 + Math.random() * 5) * L; e.wPer = 1.5 + Math.random() * 1.5; e.wPh = Math.random() * 6.283; e.wSide = Math.random() < 0.5 ? 1 : -1; e.slantT = 2 + Math.random() * 3;
        e.len = cw.len * L; e.R = 0.55 * e.len; e.g.scale.setScalar(e.len); e.body.scale.setScalar(1);
        e.g.position.copy(vAim);
        vD.copy(shipRoot.position).sub(vAim);                  // initial heading: toward the player, spread so a wave is not a firing line
        vD.x += (Math.random() - 0.5) * L * 60; vD.z += (Math.random() - 0.5) * L * 60;
        mM.lookAt(ORIGIN, vD, Y);
        e.g.quaternion.setFromRotationMatrix(mM);
        if (role === 0 && wave >= 3 && Math.random() < 0.4) {   // some interceptors hunt a wingman instead of you
            var h0 = (Math.random() * 3) | 0;
            for (var k = 0; k < allies.length; k++) { var ha = allies[(h0 + k) % allies.length]; if (ha.alive) { e.hunt = ha; break; } }
        }
        pickWander(e, scratchBodies);
        e.g.visible = true;
    }
    function freeSlot() { for (var i = 0; i < ENEMY_MAX; i++) if (!enemies[i].alive) return enemies[i]; return null; }
    function spawnWave(bodies) {
        var i;
        wave++; wavePending = false; playerFired = false; waveActive = true; waveStartT = gt;
        curWave = waveParams(WAVE_X * wave);
        bossWave = isBossWave(wave);
        var D = DIFFS[difficulty], nn = Math.max(2, Math.min(10, curWave.n + D.n));
        var want = bossWave ? 2 : nn;                              // boss waves bring a pair of escorts; the boss adds more per phase
        spawnPad = curWave.len * L;
        randDir(dirA);
        for (i = 0; i < 6; i++) { randDir(dirB); if (dirA.dot(dirB) < 0.1) break; }
        qN = 0;
        for (i = 0; i < want; i++) {                               // roles stagger: interceptors now, spitters/snipers 4-6.5 s later
            var rl = bossWave ? 0 : pickRole(i, want);
            qAt[qN] = gt + (rl === 0 ? i * 0.25 : 4 + Math.random() * 2.5); qRole[qN] = rl; qTry[qN] = 0; qN++;
        }
        regenDelay = Math.max(0, curWave.regenDelay + D.regen);
        var msg;
        if (bossWave) { spawnBoss(bodies); msg = 'WARNING · ' + bossTitle; }
        else msg = 'WAVE ' + wave + (wave === 2 ? ' · SPITTERS' : (wave === 3 ? ' · SNIPERS' : '')) + ' · ' + want + ' HOSTILES';
        spawnAllies(allyCountFor(wave - D.allyLag));
        announce(msg);
        writeSave();
        pumpQueue(bodies);
    }
    function pumpQueue(bodies) {
        for (var i = 0; i < qN;) {
            if (qAt[i] > gt) { i++; continue; }
            var e = freeSlot();
            if (!e || !pickSpawn(bodies, (i & 1) ? dirB : dirA)) {
                qAt[i] = gt + 0.4; if (++qTry[i] > 10) { qN--; qAt[i] = qAt[qN]; qRole[i] = qRole[qN]; qTry[i] = qTry[qN]; } else i++;
                continue;
            }
            initEnemy(e, qRole[i], GRACE_T);
            qN--; qAt[i] = qAt[qN]; qRole[i] = qRole[qN]; qTry[i] = qTry[qN];
        }
    }
    function spawnAdds(n, bodies) {                                // boss phase change: n interceptors from the boss's flanks
        for (var k = 0; k < n; k++) {
            var e = freeSlot();
            if (!e) return;
            randDir(vD); vAim.copy(boss.g.position).addScaledVector(vD, boss.len * 0.9 + 20 * L);
            spawnPad = curWave.len * L;
            initEnemy(e, 0, 1.2);
        }
    }
    function spawnBoss(bodies) {
        var k = Math.max(0, Math.floor((wave - BOSS_FIRST) / BOSS_EVERY)), e = boss;
        e.len = BOSS_L * L * Math.pow(1.4, k); e.R = 0.55 * e.len; e.g.scale.setScalar(e.len);
        var DB = DIFFS[difficulty];
        e.maxHp = e.hp = Math.round(BOSS_HP * Math.pow(1.4, k) * DB.hp);
        e.bDmg = 7 + 1.2 * k; e.dmg = e.bDmg * DB.dmg; e.fireCd = 3; e.orbCd = 6; e.side = 1; e.flip = 6; e.curSpeed = 0; e.hitT = 0; e.flinch = 0;
        e.bSpeed = BOSS_SPEED; e.speed = e.bSpeed * DB.spd; e.turn = 0.7; e.state = 1; e.fstate = 'boss'; e.alive = true;
        e.bphase = 1; e.open = false; e.cyc = 6; e.wind = 0; e.volley = 0; e.pflash = 0; e.body.scale.setScalar(1);
        bossTitle = bossName(wave);
        sigReset(); bsig.idx = bossAttack(bossTitle); bsig.cd = 7 + Math.random() * 3;     // rev 12: signature attack seeded from the name
        spawnPad = e.len;
        if (!pickSpawn(bodies, dirA)) {
            vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
            vAim.copy(shipRoot.position).addScaledVector(vF, -120 * L - e.len);
        }
        e.g.position.copy(vAim);
        mM.lookAt(vAim, shipRoot.position, Y);
        e.g.quaternion.setFromRotationMatrix(mM);
        e.g.visible = true;
        spawnPad = E_L;
    }
    var regenDelay = 0;
    function pickWander(e, bodies) {
        // mostly drift toward the player's neighbourhood so waves find you; passive flight still escapes
        for (var t = 0; t < 6; t++) {
            if (Math.random() < 0.7) e.wander.copy(shipRoot.position); else e.wander.copy(e.g.position);
            e.wander.x += (Math.random() - 0.5) * 240 * L; e.wander.y += (Math.random() - 0.5) * 120 * L; e.wander.z += (Math.random() - 0.5) * 240 * L;
            if (e.wander.length() > EDGE_R * 0.95) { e.wander.multiplyScalar(0.5); }
            var ok = true;
            for (var j = 0; j < bodies.length; j++) {
                var r = bodies[j].R * 1.3;
                if (e.wander.distanceToSquared(bodies[j].node.anchor.position) < r * r) { ok = false; break; }
            }
            if (ok) break;
        }
        e.timer = 5 + Math.random() * 5;
    }

    // ─── projectiles / debris ───────────────────────────────────────
    function fireBolt(ox, oy, oz, dx, dy, dz, spd, life, enemy, dmg, len, wid, col) {
        for (var i = 0; i < bolts.length; i++) {
            var b = bolts[i];
            if (b.active) continue;
            vFb.set(ox, oy, oz); vFd.set(dx, dy, dz);
            var id = fx.spawnBolt(vFb, vFd, col != null ? col : (enemy ? C_E : effColor()), spd, life);
            if (id < 0) return null;
            b.id = id; b.active = true; b.enemy = enemy; b.dmg = dmg; b.remote = false; b.owner = null; b.boss = false; b.gz = 0;
            b.prev.copy(vFb);
            return b;
        }
        return null;
    }
    // hit / death sparks: fx.impact (pooled), size by target
    function burst(p, n, matIdx, speedL, sz) {
        var k = Math.max(1, Math.round(n / 8)), size = clamp((sz || 1) * speedL / 10, 0.5, 5);
        for (var i = 0; i < k; i++) fx.impact(p, BURST_COL[matIdx] || 0xffffff, size);
    }
    var god = false;
    var SAFE_R = DOCK_R * root.sysR * 1.2;    // pilots inside the dock radius cannot be hurt by other pilots
    function hurtPlayer(dmg, by, mult, raw) {
        if (dead || god) return;
        if (rollT > 0 && !raw) dmg *= 1 - ROLL_CUT;                     // rev 9b #3: barrel roll cuts incoming damage 40 %
        hp -= dmg; sinceHit = 0;
        shake = Math.max(shake, Math.min(1.2 * L, dmg / 20 * 0.25 * L * (mult || 1)));     // rev 9: camera shake on hit (boss 2x)
        if (hp <= 0) {
            hp = 0; dead = true; deathT = DEATH_TIME; firing = false;
            if (by && net) net.sendKill(by);        // killed by another pilot: they get the credit
            writeSave();
            burst(shipRoot.position, 22, 0, 18); burst(shipRoot.position, 14, 1, 12);
            shipRoot.visible = false;
        }
    }
    // rev 9c #1: kill-fed shield orbs (violet, pooled): drift toward you inside 20 L, collected inside 6 L
    function spawnDrop(pos, val) {
        for (var i = 0; i < drops.length; i++) {
            var d = drops[i];
            if (d.active) continue;
            d.active = true; d.life = 9; d.val = val;
            d.m.position.copy(pos);
            d.vel.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(8 * L);
            d.m.scale.setScalar((val > DROP_VAL ? 0.9 : 0.6) * L); d.m.visible = true;
            return d;
        }
        return null;
    }
    function updateDrops(dt) {
        var P = shipRoot.position, mg = DROP_MAGNET * L, col = DROP_COLLECT * L;
        for (var i = 0; i < drops.length; i++) {
            var d = drops[i];
            if (!d.active) continue;
            d.life -= dt;
            if (d.life <= 0) { d.active = false; d.m.visible = false; continue; }
            vA.subVectors(P, d.m.position);
            var dd = vA.length();
            if (!dead && dd < col) {
                d.active = false; d.m.visible = false;
                hp = Math.min(HP_MAX, hp + d.val);
                fx.impact(d.m.position, 0xa070ff, 1.2); fx.flash(d.m.position, 0xa070ff);
                continue;
            }
            if (!dead && dd < mg) {
                vA.divideScalar(dd);
                d.vel.lerp(vA.multiplyScalar((22 + 50 * (1 - dd / mg)) * L), Math.min(1, 6 * dt));
            } else d.vel.multiplyScalar(Math.max(0, 1 - 1.5 * dt));
            d.m.position.addScaledVector(d.vel, dt);
            var pl = 1 + 0.2 * Math.sin(gt * 8 + i);
            d.m.scale.setScalar((d.val > DROP_VAL ? 0.9 : 0.6) * L * pl);
        }
    }
    function killEnemy(e) {
        hideEnemy(e); kills++;
        var ksz = clamp(e.len / L / 6, 1, 4);
        burst(e.g.position, e.isBoss ? 30 : 14, e.pal === 0 ? 1 : 2, e.isBoss ? 30 : 14, ksz);
        if (ctarget === e) { ctarget = null; tlock = false; }
        if (zTarget === e) zTarget = null;
        chainN = chainT > 0 ? chainN + 1 : 1; chainT = CHAIN_T;                // rev 9c: kill chain, 4 s window
        spawnDrop(e.g.position, DROP_VAL * (chainN >= 3 ? 2 : 1));
        hitStopN = 2;                                                           // rev 9b #5: 40 ms world freeze on a kill
        fovKick = 2;
        if (e.isBoss) {
            kills += 4; burst(e.g.position, 20, 0, 20, ksz);
            nextWave = gt + 4;
            for (var oi = 0; oi < orbs.length; oi++) { orbs[oi].active = false; orbs[oi].m.visible = false; }
            for (var di = 0; di < 3; di++) { vA.copy(e.g.position); vA.x += (di - 1) * 6 * L; spawnDrop(vA, DROP_VAL * 2); }
            announce('BOSS DOWN · ' + bossTitle);
        }
    }
    // rev 9 C: at <= 45 % HP an enemy flees (telegraph -> 3x burst -> regen circle -> return); max FLEE_MAX times
    function beginFlee(e) {
        e.state = 4; e.fstate = 'tele'; e.ftimer = FLEE_TELE; e.fleeN++; e.charge = 0; e.grace = 0; e.burst = 0;
        e.fp = null; e.skimOn = false; e.skim = 0; e.fdive = 10;
        var fbd = 400 * L;                                  // rev 12: prefer diving to a planet within 400 L (chases go planetside)
        for (var fj = 0; fj < scratchBodies.length; fj++) {
            var fb = scratchBodies[fj];
            if (fb.node === root) continue;
            var fdd = e.g.position.distanceTo(fb.node.anchor.position) - fb.R;
            if (fdd < fbd) { fbd = fdd; e.fp = fb; }
        }
        if (e.beam) e.beam.visible = false;
    }
    // every player-bolt hit lands here (also the debug hook): flinch, combo tick, flee check, kill
    function damageEnemy(en, dmg, crit, bp) {
        en.hp -= dmg; en.hitT = 1; en.flinch = 0.1;
        comboN++; comboT = 1.5; tickPunch = 1;
        if (bp) burst(bp, crit ? 6 : 3, crit ? 2 : 0, crit ? 12 : 8, clamp(en.len / L / 6, 1, 4));
        if (en.hp <= 0) { killEnemy(en); return true; }
        if (!en.isBoss && en.state < 4 && en.fleeN < FLEE_MAX && en.hp <= en.maxHp * FLEE_AT) beginFlee(en);
        return false;
    }

    // ─── enemy AI ───────────────────────────────────────────────────
    // state: 0 patrol/idle, 1 LINE_UP, 2 DIVE, 3 PEEL  (attack runs; shared by enemies and allies)
    var vR1 = new THREE.Vector3(), vR2 = new THREE.Vector3(), vR3 = new THREE.Vector3();
    function steer(e, tx, ty, tz, turnRate, spd, dt) {
        vM.set(tx, ty, tz);
        if (vM.distanceToSquared(e.g.position) > 1e-6) {
            mM.lookAt(e.g.position, vM, Y);
            qA.setFromRotationMatrix(mM);
            e.g.quaternion.rotateTowards(qA, turnRate * dt);
        }
        e.curSpeed += (spd - e.curSpeed) * damp(1.5, dt);
        vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);              // actor forward (vF reused as scratch)
        e.g.position.addScaledVector(vF, e.curSpeed * dt);
    }
    // PEEL: bank hard away to a loop point 20-30 old L off, random side
    function beginPeel(a, tp, len) {
        vR1.subVectors(a.g.position, tp);
        if (vR1.lengthSq() < 1e-6) vR1.set(Math.random() - 0.5, 0, Math.random() - 0.5);
        vR1.normalize();
        vR2.crossVectors(vR1, Y);
        if (vR2.lengthSq() < 1e-4) vR2.copy(X);
        vR2.normalize().multiplyScalar(Math.random() < 0.5 ? -1 : 1);
        a.off.copy(vR1).multiplyScalar(0.4).addScaledVector(vR2, 0.9).addScaledVector(Y, (Math.random() - 0.5) * 0.5).normalize()
            .multiplyScalar((PEEL_MIN + Math.random() * PEEL_VAR) * L + len);
        a.state = 3; a.timer = 6; a.near = 0;
    }
    // one step of the run state machine against target point tp (forward fw, velocity tv).
    // close = dive ends inside this, nearD = force-peel radius. Returns distance to target.
    function runStep(a, tp, fw, tv, len, close, nearD, spd, tr, dive, lead, dt) {
        var pos = a.g.position, dist, lt;
        vR3.subVectors(tp, pos); dist = vR3.length() || 1;
        if (dist < nearD) a.near += dt; else a.near = 0;
        if (a.near > RUN_NEAR_T && a.state !== 3) { beginPeel(a, tp, len); }
        a.timer -= dt;
        if (a.state === 1) {                                     // LINE_UP
            if (!a.rpSet) {
                // run-in point roughly ahead of the target's motion, ~30 deg off its axis
                vR1.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5);
                vR1.addScaledVector(fw, -vR1.dot(fw));
                if (vR1.lengthSq() < 1e-4) vR1.copy(Y).addScaledVector(fw, -Y.dot(fw));
                vR1.normalize();
                a.off.copy(fw).multiplyScalar(Math.cos(RUN_OFF_DEG * Math.PI / 180)).addScaledVector(vR1, Math.sin(RUN_OFF_DEG * Math.PI / 180))
                    .multiplyScalar((RUN_IN_MIN + Math.random() * RUN_IN_VAR) * L + len);
                a.rpSet = true; a.timer = 10;
            }
            vR2.copy(tp).add(a.off);
            steer(a, vR2.x, vR2.y, vR2.z, tr * 1.3, spd * 1.1, dt);
            if (pos.distanceToSquared(vR2) < (15 * L) * (15 * L) || a.timer <= 0) { a.state = 2; a.timer = 10; }
        } else if (a.state === 2) {                              // DIVE
            lt = dist / Math.max(a.curSpeed, 0.4) * lead;
            vR2.copy(tp).addScaledVector(tv, lt);
            steer(a, vR2.x, vR2.y, vR2.z, tr, spd * dive, dt);
            vR1.copy(NEG_Z).applyQuaternion(a.g.quaternion);
            if (dist < close || (vR1.dot(vR3) < 0 && dist < 25 * L + len) || a.timer <= 0) beginPeel(a, tp, len);
        } else if (a.state === 3) {                              // PEEL
            vR2.copy(tp).add(a.off);
            steer(a, vR2.x, vR2.y, vR2.z, tr * 1.8, spd, dt);
            if (pos.distanceToSquared(vR2) < (12 * L) * (12 * L) || dist > a.off.length() * 0.9 || a.timer <= 0) { a.state = 1; a.rpSet = false; }
        }
        return dist;
    }
    var vW1 = new THREE.Vector3(), vW2 = new THREE.Vector3(), vW3 = new THREE.Vector3(), vSl = new THREE.Vector3(), vSv = new THREE.Vector3();
    var FSTATE = ['patrol', 'lineup', 'dive', 'peel'];
    // rev 12: WORLD-ANCHORED motion. Every enemy owns a leashed base waypoint (it only moves when the target leaves a leash of
    // max(0.2 x range, 5 L) around it) and a spiral plane fixed in world space (re-planned every 3-5 s). The aim point is
    // base + a rotating offset in that plane (radius 0.40-0.50 x range = 22-27 deg off the line to the base, so the 15 deg rule
    // holds and nobody ever flies straight in). Nothing is a displacement relative to the player, so flying around an enemy
    // reads as a solid object moving through space; the steady steering toward a moving aim point keeps it from ever parking.
    function planBase(e, tp, dist, dt) {
        var pos = e.g.position;
        if (!e.hasBase) { e.base.copy(tp); e.hasBase = true; e.rpT = 0; }
        vW1.subVectors(tp, e.base);
        var bl = vW1.length(), thr = Math.max(0.2 * dist, 5 * L);
        if (bl > thr) e.base.addScaledVector(vW1, 1 - thr / bl);
        e.rpT -= dt;
        if (e.rpT <= 0) {
            e.rpT = 3 + Math.random() * 2;
            vW1.subVectors(e.base, pos);
            if (vW1.lengthSq() < 1e-6) vW1.copy(NEG_Z).applyQuaternion(e.g.quaternion);
            vW1.normalize();
            e.pu.crossVectors(vW1, Y);
            if (e.pu.lengthSq() < 1e-4) e.pu.copy(X);
            e.pu.normalize();
            e.pv.crossVectors(vW1, e.pu);
        }
    }
    function pathPoint(e, out) {
        var db = e.g.position.distanceTo(e.base), amp = db * (0.40 + 0.10 * e.rnd);
        var th = gt * (6.2832 / e.wPer) + e.wPh;
        return out.copy(e.base).addScaledVector(e.pu, Math.cos(th) * amp).addScaledVector(e.pv, Math.sin(th) * amp);
    }
    // rev 12: flee dives to the nearest planet within 400 L and skims it at floor + 3 L for 3 s
    function planetFloorR(pos, rec) {
        if (ps && ps.active === rec.node) { var o = psFloor(pos); if (o) return o.r; }
        return rec.R * 1.2;
    }
    function fleeDive(e, dt, tp, sp) {                     // true when the dive + skim is finished
        var pos = e.g.position, fc = e.fp.node.anchor.position, fR = e.fp.R, pd = pos.distanceTo(fc) || 1;
        e.fdive -= dt;
        if (e.fdive <= 0) return true;
        vW2.subVectors(pos, fc).divideScalar(pd);         // radial out
        if (!e.skimOn && pd < fR * 1.3 + 3 * L) { e.skimOn = true; e.skim = 3; }
        if (e.skimOn) {
            e.skim -= dt;
            if (e.skim <= 0) return true;
            vW3.subVectors(pos, tp); vW3.addScaledVector(vW2, -vW3.dot(vW2));      // tangent, away from the threat
            if (vW3.lengthSq() < 1e-6) vW3.crossVectors(vW2, Y);
            vW3.normalize();
            vW1.copy(vW2).addScaledVector(vW3, 0.35).normalize().multiplyScalar(planetFloorR(pos, e.fp) + 3 * L).add(fc);
        } else vW1.copy(fc).addScaledVector(vW2, fR * 0.9);
        steer(e, vW1.x, vW1.y, vW1.z, e.turn * 2.4, sp, dt);
        return false;
    }
    function fleeStep(e, dt, tp, dist) {
        var pos = e.g.position, sp;
        e.ftimer -= dt;
        vW1.subVectors(pos, tp);
        var dl = vW1.length() || 1;
        vW1.divideScalar(dl);                                       // unit away from the threat
        if (e.state === 4) {                                         // telegraph: eye core flash, 0.3 s
            steer(e, pos.x + vW1.x * 50 * L, pos.y + vW1.y * 50 * L, pos.z + vW1.z * 50 * L, e.turn * 2, e.speed * 0.7, dt);
            if (e.ftimer <= 0) { e.state = 5; e.fstate = 'flee'; e.ftimer = FLEE_RUN; }
        } else if (e.state === 5) {                                  // burst away: 3x speed (capped so boost can still run it down)
            sp = Math.min(e.speed * FLEE_X, FLEE_CAP);
            if (e.fp) {
                if (fleeDive(e, dt, tp, sp)) { e.fp = null; e.state = 6; e.fstate = 'regen'; e.ftimer = FLEE_REGEN; }
            } else {
                steer(e, pos.x + vW1.x * 500 * L, pos.y + vW1.y * 500 * L, pos.z + vW1.z * 500 * L, e.turn * 2.4, sp, dt);
                if (e.ftimer <= 0) { e.state = 6; e.fstate = 'regen'; e.ftimer = FLEE_REGEN; }
            }
            e.curSpeed += (sp - e.curSpeed) * damp(4, dt);
        } else {                                                     // regen: 15 %/s while circling the player at 40-60 L
            e.hp = Math.min(e.maxHp, e.hp + e.maxHp * FLEE_RATE * dt);
            vW2.crossVectors(Y, vW1);
            if (vW2.lengthSq() < 1e-4) vW2.copy(X);
            vW2.normalize().multiplyScalar(e.wSide);
            var rc = (40 + 20 * e.rnd) * L;
            vW3.copy(tp).addScaledVector(vW1, rc).addScaledVector(vW2, rc * 0.7);
            steer(e, vW3.x, vW3.y, vW3.z, e.turn * 1.6, e.speed * 1.1, dt);
            if (e.ftimer <= 0) { e.state = 1; e.fstate = 'lineup'; e.rpSet = false; e.near = 0; }
        }
    }
    // sniper: holds 80 L, 1.2 s visible charge (growing eye glow + thin beam), then one high-damage bolt along the beam
    function sniperStep(e, dt, tp, dist) {
        var pos = e.g.position, hold = SNIPER_HOLD * L, spd = e.speed;
        if (e.grace > 0 || dist > hold * 1.15) { pathPoint(e, vSl); steer(e, vSl.x, vSl.y, vSl.z, e.turn, spd * (e.grace > 0 ? 1.5 : 1.3), dt); }
        else if (dist < hold * 0.85) { vW1.subVectors(pos, e.base).normalize(); steer(e, pos.x + vW1.x * 40 * L, pos.y + vW1.y * 40 * L, pos.z + vW1.z * 40 * L, e.turn, spd, dt); }
        else {
            vW1.subVectors(pos, e.base).normalize(); vW2.crossVectors(vW1, Y);
            if (vW2.lengthSq() < 1e-4) vW2.copy(X);
            vW2.normalize().multiplyScalar(e.wSide);
            steer(e, pos.x + vW2.x * 30 * L, pos.y + vW2.y * 30 * L, pos.z + vW2.z * 30 * L, e.turn, spd * 0.6, dt);
        }
        e.fstate = e.charge > 0 ? 'charge' : 'patrol';
        if (dead || e.grace > 0) { if (e.charge > 0) { e.charge = 0; e.beam.visible = false; } return; }
        if (e.charge <= 0) {
            e.snCd -= dt;
            if (e.snCd <= 0 && dist < hold * 1.6) e.charge = 1e-4;
            return;
        }
        e.charge += dt;
        var ez = roleEyeZ(2), bs = E_BOLT_MAX * 0.9 * L;
        vE.set(CREATURE_EYE[0], CREATURE_EYE[1], ez).multiplyScalar(e.len).applyQuaternion(e.g.quaternion).add(pos);
        if (e.charge < SNIPER_CHARGE * 0.75) {                       // tracks you, then LOCKS for the last 0.3 s: that is your window to move
            vW1.copy(tp);
            if (tp === shipRoot.position) vW1.addScaledVector(vel, dist / bs * 0.8);
            e.aim.subVectors(vW1, vE).normalize();
        }
        var arr = e.beam.geometry.attributes.position.array, ln = dist * 1.25;
        arr[0] = vE.x; arr[1] = vE.y; arr[2] = vE.z;
        arr[3] = vE.x + e.aim.x * ln; arr[4] = vE.y + e.aim.y * ln; arr[5] = vE.z + e.aim.z * ln;
        e.beam.geometry.attributes.position.needsUpdate = true;
        e.beam.material.opacity = 0.2 + 0.8 * Math.min(1, e.charge / SNIPER_CHARGE);
        e.beam.visible = true;
        if (e.charge >= SNIPER_CHARGE) {
            fireBolt(vE.x, vE.y, vE.z, e.aim.x, e.aim.y, e.aim.z, bs, 320 * L / bs, true, e.dmg, e.len * 0.8, e.len * 0.05, 0xbff4ff);
            e.charge = 0; e.beam.visible = false; e.snCd = 3.5 + Math.random() * 2;
        }
    }
    function updateEnemy(e, dt, bodies) {
        var P = shipRoot.position, pos = e.g.position, tp = P, tvv = vel, hunted = false, k;
        if (e.hunt) { if (e.hunt.alive) { tp = e.hunt.g.position; hunted = true; } else e.hunt = null; }
        vD.subVectors(tp, pos);
        var dist = vD.length();
        var aggro = AGGRO_L * L, eBolt = clamp(E_BOLT_PER_LEN * e.len / L, E_BOLT_MIN, E_BOLT_MAX), fireR = E_FIRE_PER_LEN * e.len;
        if (hunted) tvv = vSv.copy(NEG_Z).applyQuaternion(e.hunt.g.quaternion).multiplyScalar(e.hunt.curSpeed);
        if (pos.distanceToSquared(P) > (1200 * L) * (1200 * L)) { hideEnemy(e); return; }     // wandered off, recycle
        e.slantT -= dt; if (e.slantT <= 0) { e.wSide = -e.wSide; e.slantT = 3 + Math.random() * 2; }
        planBase(e, tp, dist, dt);
        if (e.grace > 0) e.grace -= dt;
        var tr = e.turn, saveD = dist, spit = e.role === 1;
        if (e.state >= 4) {
            fleeStep(e, dt, tp, dist);
        } else if (e.role === 2) {
            sniperStep(e, dt, tp, dist);
        } else {
            e.fstate = FSTATE[e.state] || 'patrol';
            if (e.state === 0) {
                if (!dead && e.grace <= 0 && dist < aggro) { e.state = 1; e.rpSet = false; e.near = 0; }
                // "patrol" = shadow the player: close to the edge of aggro at full speed so every wave finds you, then loiter on a
                // wander point near the player. Idle guard: nobody within 80 L for 15 s -> push straight in (never dead air).
                e.timer -= dt;
                if (e.timer <= 0 || pos.distanceToSquared(e.wander) < (18 * L) * (18 * L)) pickWander(e, bodies);
                if (dist > 80 * L) e.idle += dt; else e.idle = 0;
                if (e.grace > 0 || dist > aggro * 1.6 || e.idle > 15) { pathPoint(e, vSl); steer(e, vSl.x, vSl.y, vSl.z, tr, e.speed * (e.grace > 0 ? 1.5 : 1), dt); }
                else steer(e, e.wander.x, e.wander.y, e.wander.z, tr * 0.6, e.speed * 0.5, dt);
            } else if (dead || dist > aggro * 1.5) {
                e.state = 0;
            } else {
                if (tvv.lengthSq() > 0.01) vR3.copy(tvv).normalize(); else vR3.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                vTmp.copy(vR3);                                        // target forward (runStep clobbers vR3)
                pathPoint(e, vSl);
                runStep(e, vSl, vTmp, tvv, 0, DIVE_CLOSE * L, RUN_NEAR * L, e.speed, tr, e.dive, e.lead, dt);
            }
            // fire during the DIVE only, inside the (widened) cone and range; interceptors in 3-shot bursts, spitters one slow orb
            e.fireCd -= dt;
            if (!dead && e.grace <= 0 && e.state === 2 && saveD < fireR && e.fireCd <= 0) {
                vD.subVectors(tp, pos);
                vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
                if (vF.dot(vD) > saveD * Math.cos(e.cone * 1.25 + 0.17)) {
                    var bsp = (spit ? SPIT_SPEED : eBolt) * L;
                    var tl = (saveD / bsp) * e.lead * (spit ? 0.5 : 1);
                    vAim.copy(tp).addScaledVector(tvv, tl);
                    vD.subVectors(vAim, pos).normalize();
                    vD.x += (Math.random() - 0.5) * 2 * e.err; vD.y += (Math.random() - 0.5) * 2 * e.err; vD.z += (Math.random() - 0.5) * 2 * e.err;
                    vD.normalize();
                    vTmp.copy(pos).addScaledVector(vF, e.len * 0.5);
                    fireBolt(vTmp.x, vTmp.y, vTmp.z, vD.x, vD.y, vD.z, bsp, fireR * E_RANGE_X / bsp, true, e.dmg, e.len * 0.6, e.len * 0.05, spit ? 0xc8ff3a : null);
                    if (e.role === 0) { if (e.burst > 0) e.burst--; else e.burst = 2; e.fireCd = e.burst > 0 ? 0.11 : e.fireInt * (0.8 + Math.random() * 0.4); }
                    else e.fireCd = e.fireInt * (0.8 + Math.random() * 0.4);
                }
            }
        }
        // planets are solid for drones too (terrain floor + 2 L inside an active planet's atmosphere)
        pushOutOfBodies(pos, bodies, 1.2, 2);
        wobble(e, dt);
    }
    // swim-like wobble on the body child + shader flex clock + hit flash / flinch decay + eye-core glow states
    function wobble(e, dt) {
        var wt = gt * 3 + e.phase;
        e.body.rotation.z = Math.sin(wt) * 0.14;
        e.body.rotation.y = Math.sin(wt * 0.7 + 1) * 0.1;
        e.body.rotation.x = Math.sin(wt * 0.5 + 2) * 0.05;
        e.body.position.y = Math.sin(wt * 1.3) * 0.03;
        var gk;
        if (e.isBoss) gk = e.open ? 0.75 + 0.1 * Math.sin(gt * 14) : Math.max(0.1, 0.7 * e.sigGlow);            // the eye only glows while exposed (or charging a signature attack)
        else if (e.state === 4) gk = 0.85;                                           // flee telegraph: eye-core flash
        else if (e.charge > 0) gk = 0.2 + 0.7 * Math.min(1, e.charge / SNIPER_CHARGE);   // sniper charge: growing glow
        else gk = 0.2 + 0.05 * Math.sin(gt * 6 + e.phase) + 0.05 * (e.curSpeed / (e.speed + 1));
        e.glow.scale.setScalar(gk);
        e.glow.material.opacity = (e.isBoss && !e.open) ? 0.35 : 1;
        var u = e.mat.uniforms;
        u.uTime.value = gt;
        var tint = 0;
        if (e.hitT > 0) { e.hitT = Math.max(0, e.hitT - dt * 5); tint = e.hitT * 0.6; }
        if (e.state === 4) tint = Math.max(tint, 0.5 + 0.4 * Math.sin(gt * 40));
        if (e.pflash > 0) tint = Math.max(tint, e.pflash * 0.8);
        if (e.sigGlow > 0) tint = Math.max(tint, e.sigGlow * (0.25 + 0.2 * Math.sin(gt * 18)));
        if (u.uHit.value !== tint) u.uHit.value = tint;
        if (e.flinch > 0) { e.flinch = Math.max(0, e.flinch - dt); e.body.scale.setScalar(1 + 0.18 * (e.flinch / 0.1)); }      // 0.1 s scale punch
        else if (e.body.scale.x !== 1) e.body.scale.setScalar(1);
    }

    // ─── boss ───────────────────────────────────────────────────────
    function spawnOrb(from, toward) {
        for (var i = 0; i < orbs.length; i++) {
            var o = orbs[i];
            if (o.active) continue;
            o.active = true; o.life = 9; o.hp = ORB_HP;
            o.m.position.copy(from);
            o.vel.subVectors(toward, from).normalize().multiplyScalar(ORB_SPEED * L);
            o.m.scale.setScalar(1.65 * ORB_S * L);
            o.m.visible = true;
            return;
        }
    }
    // rev 9c #5: three phases (100 / 66 / 33 %). The eye core is exposed (2x) only during the 2 s telegraphed attack windup;
    // each phase change flashes, shakes, adds 2 interceptors and unlocks a new attack: P1 5-fan, P2 7-fan + homing orb, P3 double 7-fan + 2 orbs.
    function bossVolley(e, n, spread, aim) {
        vM.crossVectors(aim, Y).normalize();
        vTmp.copy(e.g.position).addScaledVector(vF, e.len * 0.45);
        var h = (n - 1) / 2;
        for (var k = 0; k < n; k++) {
            vD.copy(aim).addScaledVector(vM, (k - h) * spread).normalize();
            var bb = fireBolt(vTmp.x, vTmp.y, vTmp.z, vD.x, vD.y, vD.z, E_BOLT_MAX * 0.5 * L, 1.6 * 160 * L / (E_BOLT_MAX * 0.5 * L), true, e.dmg, 8 * L, 0.8 * L);
            if (bb) bb.boss = true;
        }
    }
    function bossAim(e) {
        vE.copy(shipRoot.position).addScaledVector(vel, (e.g.position.distanceTo(shipRoot.position) / (E_BOLT_MAX * 0.5 * L)) * 0.4);
        return vS.subVectors(vE, e.g.position).normalize();
    }
    // ─── rev 12: boss signature attacks (ONE per boss, seeded from its name): beam sweep, orb ring, ram charge, mine field, gravity pull.
    // Telegraph (HUD names it on the boss bar) -> act -> cooldown. Each lands 35-40 HP once; dodges are spatial, never twitch.
    var bsig = { idx: 0, ph: 'idle', t: 0, cd: 8, dir: new THREE.Vector3(), ax: new THREE.Vector3(), org: new THREE.Vector3(), u: new THREE.Vector3(), v: new THREE.Vector3(), hit: false, a: 0, b: 0 };
    var vG1 = new THREE.Vector3(), vG2 = new THREE.Vector3(), qG = new THREE.Quaternion(), BEAM_LEN = 320;
    function bossEye(e, out) { return out.set(CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2]).multiplyScalar(e.len).applyQuaternion(e.g.quaternion).add(e.g.position); }
    function setBeam(from, dir, len, rad, op) {
        sigBeam.position.copy(from).addScaledVector(dir, len * 0.5);
        sigBeam.quaternion.setFromUnitVectors(Y, dir);
        sigBeam.scale.set(rad, len, rad);
        sigBeam.material.opacity = op; sigBeam.visible = true;
    }
    function sigReset() {
        bsig.ph = 'idle'; bsig.cd = 6 + Math.random() * 3; bsig.hit = false;
        sigBeam.visible = false;
        for (var i = 0; i < sigOrbs.length; i++) sigOrbs[i].visible = false;
        for (i = 0; i < mines.length; i++) { mines[i].active = false; mines[i].m.visible = false; mines[i].shell.visible = false; }
        boss.sigGlow = 0;
    }
    function sigEnd(e) {
        sigReset();
        bsig.cd = (9 + Math.random() * 5) * (e.bphase >= 3 ? 0.65 : (e.bphase === 2 ? 0.8 : 1));
    }
    function sigHurt(e, dmg) { bsig.hit = true; bsig.hits = (bsig.hits | 0) + 1; bsig.lastDmg = dmg * DIFFS[difficulty].dmg; hurtPlayer(dmg * DIFFS[difficulty].dmg, undefined, 2.5); }
    function sigBegin(e) {                                                     // telegraph -> act
        var P = shipRoot.position, i, k, d;
        bossEye(e, vG1);
        bsig.ph = 'act'; bsig.hit = false;
        if (bsig.idx === 0) {
            bsig.t = 2; bsig.org.copy(vG1);                                    // beam origin + plane lock to the eye RIGHT NOW (the carapace keeps drifting)
            bsig.dir.subVectors(P, vG1).normalize(); bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize();
        }
        else if (bsig.idx === 1) {
            vG2.copy(P).addScaledVector(vel, 1.2);                              // lock slightly ahead of a steady pilot
            bsig.org.copy(vG1); bsig.dir.subVectors(vG2, vG1); d = bsig.dir.length(); bsig.dir.divideScalar(d || 1);
            bsig.u.crossVectors(bsig.dir, Y); if (bsig.u.lengthSq() < 1e-4) bsig.u.copy(X); bsig.u.normalize();
            bsig.v.crossVectors(bsig.dir, bsig.u);
            bsig.a = Math.max(60 * L, d); bsig.b = Math.max(2.5, bsig.a / (24 * L));   // travel length, travel time
            bsig.t = bsig.b * 1.25;
        } else if (bsig.idx === 2) {
            bsig.dir.subVectors(P, e.g.position); d = bsig.dir.length(); bsig.dir.divideScalar(d || 1);
            bsig.a = clamp(d / 2.2, 3 * BOOST, 80);                              // charge speed (3x boost minimum)
            bsig.t = 3;
        } else if (bsig.idx === 3) {
            k = 0;
            for (i = 0; i < mines.length; i++) {
                var mn = mines[i];
                randDir(vG2); d = (20 + Math.random() * 28) * L;
                mn.m.position.copy(P).addScaledVector(vel, 1.5).addScaledVector(vG2, d);
                mn.vel.copy(randDir(vG2)).multiplyScalar(1.5 * L);
                mn.fuse = -1; mn.life = 14; mn.active = true; mn.m.visible = true; mn.shell.visible = false; mn.m.scale.setScalar(L);
            }
            bsig.t = 14;
        } else {
            bsig.t = 3; bsig.a = 0;
            spawnAdds(2, scratchBodies);                                          // escorts fire while you are held
        }
    }
    function bossSig(e, dt, dist) {
        var P = shipRoot.position, i, ph = bsig.ph, idx = bsig.idx, tl;
        if (ph === 'idle') {
            e.sigGlow = Math.max(0, e.sigGlow - dt * 3);
            bsig.cd -= dt;
            if (bsig.cd <= 0 && !dead && dist < 320 * L + e.len && e.wind <= 0 && e.pflash <= 0 && e.state === 1) {
                bsig.ph = 'tele'; bsig.t = SIG_TELE[idx]; bsig.hit = false;
                bossEye(e, vG1);
                bsig.dir.subVectors(P, vG1).normalize();                          // beam: lock the plane now
                bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize();
                announce(BOSS_ATTACKS[idx]);
            }
            return;
        }
        if (dead) { sigEnd(e); return; }
        bossEye(e, vG1);
        if (ph === 'tele') {
            bsig.t -= dt; e.sigGlow = Math.min(1, e.sigGlow + dt * 3);
            if (idx === 0 && bsig.t > 0.35) { bsig.dir.subVectors(P, vG1).normalize(); bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize(); }
            if (idx === 0) { qG.setFromAxisAngle(bsig.ax, -0.85); vG2.copy(bsig.dir).applyQuaternion(qG); setBeam(vG1, vG2, BEAM_LEN * L, 0.12 * L, 0.35 + 0.35 * Math.sin(gt * 30)); }
            else if (idx === 2) { vG2.subVectors(P, e.g.position).normalize(); bsig.dir.copy(vG2); setBeam(e.g.position, vG2, 220 * L, 0.12 * L, 0.35 + 0.35 * Math.sin(gt * 30)); }
            if (bsig.t <= 0) { sigBeam.visible = false; sigBegin(e); }
            return;
        }
        // act
        bsig.t -= dt; e.sigGlow = 0.6;
        if (idx === 0) {                                                          // beam sweep: 2 s rotation through ~98 deg
            tl = 1 - bsig.t / 2;
            qG.setFromAxisAngle(bsig.ax, -0.85 + 1.7 * clamp(tl, 0, 1)); vG2.copy(bsig.dir).applyQuaternion(qG);
            vE.copy(bsig.org);
            setBeam(vE, vG2, BEAM_LEN * L, 0.7 * L, 0.9);
            vG1.copy(vE).addScaledVector(vG2, BEAM_LEN * L);
            if (!bsig.hit && segDistSq(vE.x, vE.y, vE.z, vG1.x, vG1.y, vG1.z, P.x, P.y, P.z) < (2 * L) * (2 * L)) sigHurt(e, SIG_DMG[0]);
            shake = Math.max(shake, 0.12 * L);
        } else if (idx === 1) {                                                   // orb ring: 12 orbs expand then contract onto the locked point
            var u = bsig.t > 0 ? (bsig.b * 1.25 - bsig.t) / bsig.b : 1.25, rad = L * (4.5 + 13 * Math.sin(Math.PI * clamp(u, 0, 1)));
            vG2.copy(bsig.org).addScaledVector(bsig.dir, bsig.a * clamp(u, 0, 1.25));
            for (i = 0; i < 12; i++) {
                var phi = i / 12 * 6.2832 + gt * 0.8, o = sigOrbs[i];
                o.position.copy(vG2).addScaledVector(bsig.u, Math.cos(phi) * rad).addScaledVector(bsig.v, Math.sin(phi) * rad);
                o.scale.setScalar(1.2 * L * (1 + 0.15 * Math.sin(gt * 9 + i))); o.visible = true;
                if (!bsig.hit && o.position.distanceToSquared(P) < (1.9 * L) * (1.9 * L)) { sigHurt(e, SIG_DMG[1]); burst(o.position, 12, 1, 14); break; }
            }
        } else if (idx === 2) {                                                   // ram charge: 3 s at speed, along the locked line (collision pays 40)
            e.g.position.addScaledVector(bsig.dir, bsig.a * dt);
            mM.lookAt(e.g.position, vG2.copy(e.g.position).add(bsig.dir), Y); qA.setFromRotationMatrix(mM); e.g.quaternion.rotateTowards(qA, 2.5 * dt);
            shake = Math.max(shake, 0.05 * L);
        } else if (idx === 3) {                                                   // mine field: 8 drifting mines, 2 s fuse on proximity
            var live = 0;
            for (i = 0; i < mines.length; i++) {
                var mn = mines[i];
                if (!mn.active) continue;
                live++;
                mn.life -= dt;
                mn.m.position.addScaledVector(mn.vel, dt);
                var md2 = mn.m.position.distanceToSquared(P);
                if (mn.fuse < 0) {
                    mn.m.scale.setScalar(L * (1 + 0.12 * Math.sin(gt * 4 + i)));
                    if (md2 < (8 * L) * (8 * L)) { mn.fuse = 2; mn.shell.visible = true; }
                    else if (mn.life <= 0) { mn.active = false; mn.m.visible = false; }
                } else {
                    mn.fuse -= dt;
                    mn.m.scale.setScalar(L * (1.1 + 0.4 * Math.abs(Math.sin(gt * (8 + 10 * (1 - mn.fuse / 2))))));
                    if (mn.fuse <= 0) {
                        mn.active = false; mn.m.visible = false; mn.shell.visible = false;
                        burst(mn.m.position, 20, 1, 20, 3); fx.flash(mn.m.position, 0xff7a2a);
                        if (!dead && !bsig.hit && md2 < (10 * L) * (10 * L)) sigHurt(e, SIG_DMG[3]);
                    }
                }
            }
            if (!live) bsig.t = 0;
        } else {                                                                  // gravity pull: 3 s drag toward the eye; boost straight out to escape
            bsig.a = Math.min(1, bsig.a + dt * 2);
            vG2.subVectors(vG1, P); var gd = vG2.length() || 1; vG2.divideScalar(gd);
            var surf = e.g.position.distanceTo(P) - e.R;
            if (surf > 12 * L) P.addScaledVector(vG2, 30 * L * bsig.a * dt);
            setBeam(P, vG2, gd, 0.18 * L, 0.3 + 0.2 * Math.sin(gt * 10));
            shake = Math.max(shake, 0.08 * L); fovKick = Math.max(fovKick, 1.5);
            if (bsig.t <= 0 && !bsig.hit && surf < 45 * L) sigHurt(e, SIG_DMG[4]);
        }
        if (bsig.t <= 0) sigEnd(e);
    }
    function updateBoss(e, dt, bodies) {
        var P = shipRoot.position, pos = e.g.position;
        e.prev.copy(pos);
        vD.subVectors(pos, P);
        var dist = vD.length() || 1;
        vD.divideScalar(dist);                                       // unit from player to boss
        var charging = bsig.ph === 'act' && bsig.idx === 2, sd = dist - e.R;
        e.flip -= dt; if (e.flip <= 0) { e.side = -e.side; e.flip = 7 + Math.random() * 4; }
        if (!charging) {
            // rev 12: near-stationary (drift <= 0.3 u/s, a slow orbit); holds its carapace 60-90 L from you: follows when you leave, lets you approach
            vS.crossVectors(vD, Y); if (vS.lengthSq() < 1e-4) vS.copy(X); vS.normalize();
            pos.addScaledVector(vS, e.side * Math.min(e.speed, 0.3) * dt);
            if (sd > BAND_MAX * L) pos.addScaledVector(vD, -Math.min(95, (sd - BAND_MAX * L) * 5 + 0.3) * dt);
            e.curSpeed = Math.min(e.speed, 0.3);
            mM.lookAt(pos, P, Y);
            qA.setFromRotationMatrix(mM);
            e.g.quaternion.rotateTowards(qA, e.turn * dt);
        }
        vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
        var fr = e.hp / e.maxHp, ph = fr > 0.66 ? 1 : (fr > 0.33 ? 2 : 3);
        if (ph > e.bphase) {
            e.bphase = ph; e.pflash = 1; e.hitT = 1; e.wind = 0; e.open = false; e.cyc = 3; e.volley = 0;
            shake = Math.max(shake, 0.9 * L); fovKick = 3;
            announce('PHASE ' + ph + ' · ' + bossTitle);
            spawnAdds(2, bodies);
        }
        if (e.pflash > 0) e.pflash = Math.max(0, e.pflash - dt);
        if (!dead && dist < 180 * L + e.len && e.pflash <= 0) {
            if (e.wind > 0) {
                e.open = true; e.wind -= dt;                        // telegraph: core exposed and glowing
                if (e.wind <= 0) {
                    e.open = false; e.cyc = ph === 3 ? 3 : 4;
                    bossVolley(e, ph === 1 ? 5 : 7, 0.085, bossAim(e));
                    vTmp.copy(pos).addScaledVector(vF, e.len * 0.45);
                    if (ph >= 2) spawnOrb(vTmp, P);
                    if (ph >= 3) { spawnOrb(vTmp, P); e.volley = 1; e.volleyCd = 0.45; }
                }
            } else {
                e.open = false; e.cyc -= dt;
                if (e.cyc <= 0) { if (bsig.ph === 'idle' && vF.dot(bossAim(e)) > 0.8) e.wind = 2; else e.cyc = 0.2; }
            }
            if (e.volley > 0) { e.volleyCd -= dt; if (e.volleyCd <= 0) { e.volley--; bossVolley(e, 7, 0.1, bossAim(e)); } }
        } else if (e.wind <= 0) e.open = false;
        bossSig(e, dt, dist);
        pushOutOfBodies(pos, bodies, 1.2);
        wobble(e, dt * 0.5);
        e.vel.subVectors(pos, e.prev).multiplyScalar(1 / Math.max(dt, 1e-4));
    }
    function updateOrbs(dt) {
        var P = shipRoot.position;
        for (var i = 0; i < orbs.length; i++) {
            var o = orbs[i];
            if (!o.active) continue;
            o.life -= dt;
            if (o.life <= 0) { o.active = false; o.m.visible = false; continue; }
            // slow homing: the velocity direction leans toward the player
            var sp = ORB_SPEED * L;
            vA.subVectors(P, o.m.position).normalize();
            vB.copy(o.vel).normalize().lerp(vA, Math.min(1, 1.1 * dt)).normalize();
            o.vel.copy(vB).multiplyScalar(sp);
            o.m.position.addScaledVector(o.vel, dt);
            o.m.scale.setScalar((1.5 + 0.24 * Math.sin(gt * 9 + i)) * ORB_S * L);
            if (!dead && o.m.position.distanceToSquared(P) < (2.7 * ORB_S * L) * (2.7 * ORB_S * L)) {
                o.active = false; o.m.visible = false;
                burst(o.m.position, 12, 1, 14); hurtPlayer(ORB_DMG * DIFFS[difficulty].dmg, undefined, 2);
            }
        }
    }

    // ─── allies ─────────────────────────────────────────────────────
    var allyTpl = null, allyGlowTex = glowTexture(150, 120, 255);
    var allies = [];
    function allyCountFor(w) { return w >= 7 ? 3 : (w >= 5 ? 2 : (w >= 3 ? 1 : 0)); }
    // re-livery: violet/cyan via the vertex color attribute (hull shader reads it directly)
    function tintAlly(g) {
        var vi = new THREE.Color(0x8A5BFF), cy = new THREE.Color(0x30E8FF), c = new THREE.Color();
        g.traverse(function (o) {
            if (!o.isMesh) return;
            var col = o.geometry.attributes.color, lit = !!o.material.isShaderMaterial;
            for (var i = 0; i < col.count; i++) {
                var r = col.getX(i), gg = col.getY(i), b = col.getZ(i), lum = 0.3 * r + 0.59 * gg + 0.11 * b;
                if (lit) {
                    var t = clamp((lum - 0.1) / 0.4, 0, 1); c.copy(vi).lerp(cy, t * t);
                    var k = 0.4 + lum * 1.5; col.setXYZ(i, c.r * k, c.g * k, c.b * k);
                } else col.setXYZ(i, r * 0.55, gg * 0.95, b);
            }
            col.needsUpdate = true;
        });
    }
    // re-livery from any hex (multiplayer ghosts, /color on the local hull); base vertex colors cached so it can be re-applied
    function tintGhost(g, hex) {
        // theme color = hull main paint: only the hull's magenta/violet vertices are repainted; orange/tan/blue decals and emissives stay
        if (hex == null) {                      // no profile color: restore the original livery
            g.traverse(function (o) { if (o.isMesh && o.geometry.userData.baseCol) { var ca = o.geometry.attributes.color; ca.array.set(o.geometry.userData.baseCol); ca.needsUpdate = true; } });
            return;
        }
        var main = new THREE.Color(hex), c = new THREE.Color();
        g.traverse(function (o) {
            if (!o.isMesh || !o.geometry.attributes.color || !o.material.isShaderMaterial) return;
            var col = o.geometry.attributes.color;
            if (!o.geometry.userData.baseCol) o.geometry.userData.baseCol = col.array.slice();
            var base = o.geometry.userData.baseCol;
            for (var i = 0; i < col.count; i++) {
                var r = base[i * 3], gg = base[i * 3 + 1], b = base[i * 3 + 2];
                if (gg < 0.6 * Math.min(r, b) && b > 0.5 * r && r > 0.5 * b) {
                    var lum = 0.3 * r + 0.59 * gg + 0.11 * b, k = clamp(lum / 0.2, 0.3, 1.6);
                    c.copy(main).multiplyScalar(k); col.setXYZ(i, c.r, c.g, c.b);
                } else col.setXYZ(i, r, gg, b);
            }
            col.needsUpdate = true;
        });
    }
    function ensureAllies() {
        if (allies.length) return;
        for (var k = 0; k < ALLY_MAX; k++) {
            var g = new THREE.Group(), h = allyTpl ? allyTpl.clone(true) : fallbackDart();
            var src = allyTpl || h;
            h.traverse(function (o) { o.frustumCulled = false; });
            g.add(h);
            var exA = makeEx(combatRoot); exSetup(exA, src.userData.thrusters || [], C_A);
            var mz = src.userData.gunMuzzles || [{ x: -0.26, y: -0.04, z: -0.45 }, { x: 0.26, y: -0.04, z: -0.45 }];
            g.scale.setScalar(L); g.visible = false;
            combatRoot.add(g);
            allies.push({
                g: g, ex: exA, mz: [new THREE.Vector3(mz[0].x, mz[0].y, mz[0].z), new THREE.Vector3(mz[1].x, mz[1].y, mz[1].z)],
                alive: false, hp: 0, target: null, retarget: 0, fireCd: 0, curSpeed: 0, state: 0, near: 0, rpSet: false, off: new THREE.Vector3(), timer: 0, side: k % 2 ? 1 : -1, flip: 3, slot: k
            });
        }
    }
    function spawnAllies(n) {
        if (n <= 0) return;
        ensureAllies();
        vA.copy(Z).applyQuaternion(shipRoot.quaternion);              // behind the player
        vB.copy(X).applyQuaternion(shipRoot.quaternion);
        for (var k = 0; k < n && k < ALLY_MAX; k++) {
            var a = allies[k];
            if (a.alive) { a.hp = Math.min(ALLY_HP, a.hp + ALLY_HP * 0.5); continue; }
            a.alive = true; a.hp = ALLY_HP; a.target = null; a.retarget = 0; a.curSpeed = Math.abs(speed) + 0.5;
            a.g.position.copy(shipRoot.position).addScaledVector(vA, (42 + k * 9) * L).addScaledVector(vB, (k - 1) * 18 * L);
            a.g.quaternion.copy(shipRoot.quaternion);
            a.g.visible = true;
        }
    }
    function allyPickTarget(a) {
        if (zT > 0 && zTarget && zTarget.alive) return zTarget;       // rev 9c: Z = every wingman peels onto your target
        if (boss.alive) return boss;
        var best = null, bd = 1e30;
        for (var i = 0; i < ENEMY_MAX; i++) {
            var e = enemies[i];
            if (!e.alive) continue;
            var d = a.g.position.distanceToSquared(e.g.position);
            if (d < bd) { bd = d; best = e; }
        }
        return best && bd < (450 * L) * (450 * L) ? best : null;
    }
    function updateAlly(a, dt, bodies) {
        var pos = a.g.position, P = shipRoot.position, t, d, i;
        a.retarget -= dt;
        if (a.retarget <= 0 || (a.target && !a.target.alive)) { a.target = allyPickTarget(a); a.retarget = 0.6 + Math.random() * 0.4; }
        a.flip -= dt; if (a.flip <= 0) { a.side = -a.side; a.flip = 2.5 + Math.random() * 2.5; }
        a.fireCd -= dt;
        t = a.target;
        if (t) {
            vD.subVectors(t.g.position, pos); d = vD.length() || 1;
            // attack runs on the target (same state machine as enemies): line up, dive firing, peel
            var stand = 3.5 * L + t.len * 0.8;
            if (a.state === 0) { a.state = 1; a.rpSet = false; a.near = 0; }
            vR3.copy(NEG_Z).applyQuaternion(t.g.quaternion);
            vTmp.copy(vR3);
            runStep(a, t.g.position, vTmp, ORIGIN, t.len, stand, stand + 8 * L, A_SPEED, A_TURN, 1.4, 0, dt);
            vD.subVectors(t.g.position, pos); d = vD.length() || 1;
            vF.copy(NEG_Z).applyQuaternion(a.g.quaternion);
            if (a.fireCd <= 0 && playerFired && d < 90 * L + t.len && a.state === 2 && vF.dot(vD) > d * Math.cos(0.26)) {
                for (i = 0; i < 2; i++) {
                    vTmp.copy(a.mz[i]).multiplyScalar(L).applyQuaternion(a.g.quaternion).add(pos);
                    vE.subVectors(t.g.position, vTmp).normalize();
                    vE.x += (Math.random() - 0.5) * 0.05; vE.y += (Math.random() - 0.5) * 0.05; vE.z += (Math.random() - 0.5) * 0.05;
                    vE.normalize();
                    fx.flash(vTmp, C_A);
                    fireBolt(vTmp.x, vTmp.y, vTmp.z, vE.x, vE.y, vE.z, P_BOLT_SPEED * L, P_BOLT_RANGE / P_BOLT_SPEED, false, A_DMG, L * 2.7, L * 0.15, C_A);
                }
                a.fireCd = 0.48 + Math.random() * 0.2;
            }
        } else {
            a.state = 0;
            // regroup: hold a slot off the player's quarter
            vS.set((a.slot - 1) * 15 * L, a.slot === 1 ? 9 * L : 0, 21 * L).applyQuaternion(shipRoot.quaternion).add(P);
            d = pos.distanceTo(vS);
            var spd = Math.min(A_SPEED, d * 1.5 + Math.abs(speed));
            if (d < 9 * L) {
                vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                steer(a, pos.x + vF.x * 90 * L, pos.y + vF.y * 90 * L, pos.z + vF.z * 90 * L, A_TURN, spd, dt);
            } else steer(a, vS.x, vS.y, vS.z, A_TURN, spd, dt);
        }
        pushOutOfBodies(pos, bodies, 1.2, 2);
        a.ex.holder.visible = true;
        exUpdate(a.ex, pos, a.g.quaternion, L, 0.3 + 0.9 * (a.curSpeed / A_SPEED));
    }
    function killAlly(a) {
        a.alive = false; a.g.visible = false; exOff(a.ex);
        burst(a.g.position, 18, 0, 16);
        if (tailAlly === a) { tailAlly = null; tailBy = null; tailLeft = 0; }
    }
    // rev 9c #4: wingman in peril. A hostile within 14 L behind an ally (25 deg cone) for 2 s -> callout row + bearing arrow;
    // break it (kill the tailer) within 10 s for an orb and +2 chain, or the ally drops to 40 % HP.
    function updatePeril(dt) {
        var i, j;
        if (tailAlly) {
            if (!tailAlly.alive) { tailAlly = null; tailBy = null; return; }
            if (!tailBy || !tailBy.alive) {
                spawnDrop(tailAlly.g.position, DROP_VAL);
                chainN = (chainT > 0 ? chainN : 0) + 2; chainT = CHAIN_T;
                addRow('', '', 'WINGMAN SAVED · +ORB', 'is-sys');
                tailAlly = null; tailBy = null; return;
            }
            tailLeft -= dt;
            if (tailLeft <= 0) {
                tailAlly.hp = Math.min(tailAlly.hp, ALLY_HP * 0.4);
                addRow('', '', 'WINGMAN HIT HARD', 'is-err');
                tailAlly = null; tailBy = null;
            }
            return;
        }
        for (i = 0; i < allies.length; i++) {
            var a = allies[i];
            if (!a.alive) { a.tailT = 0; continue; }
            vF.copy(Z).applyQuaternion(a.g.quaternion);              // ally's six
            var hit = null;
            for (j = 0; j < ENEMY_MAX; j++) {
                var e = enemies[j];
                if (!e.alive || e.state >= 4) continue;
                vA.subVectors(e.g.position, a.g.position);
                var d2 = vA.lengthSq();
                if (d2 < (PERIL_R * L) * (PERIL_R * L) && vA.dot(vF) > Math.sqrt(d2) * 0.906) { hit = e; break; }
            }
            if (hit) {
                a.tailT = (a.tailT || 0) + dt;
                if (a.tailT >= PERIL_DETECT) { tailAlly = a; tailBy = hit; tailLeft = PERIL_SAVE; a.tailT = 0; addRow('', '', 'WINGMAN ' + (i + 1) + ' TAILED · BREAK IT', 'is-err'); return; }
            } else a.tailT = 0;
        }
    }


    // ─── slash command line (rev 6) ─────────────────────────────────
    var cmdOpen = false, cmdGuardUntil = 0;   // guard: the lock-release event from openCmd can land after a fast close
    var elCmd = hud.querySelector('#ship-cmd'), elChatLog = hud.querySelector('.sc-log'), elChat = hud.querySelector('#ship-chat');
    // ─── chat log (rev 11): Minecraft style, newest at the bottom, max 8 rows, each fades after 10 s unless the input is open
    var CHAT_MAX = 8, CHAT_FADE_MS = 10000, chatRows = [], echoQ = [];
    function addRow(nameTxt, nameCol, text, cls) {
        var r = document.createElement('div');
        r.className = 'sc-row' + (cls ? ' ' + cls : '');
        if (nameTxt) { var n = document.createElement('span'); n.className = 'sc-n'; n.textContent = '<' + nameTxt + '> '; if (nameCol) n.style.color = nameCol; r.appendChild(n); }
        var t = document.createElement('span'); t.className = 'sc-t'; t.textContent = text; r.appendChild(t);
        elChatLog.appendChild(r); chatRows.push(r);
        while (chatRows.length > CHAT_MAX) { var o = chatRows.shift(); clearTimeout(o._t); if (o.parentNode) o.parentNode.removeChild(o); }
        r._t = setTimeout(function () { r.classList.add('is-faded'); }, CHAT_FADE_MS);
    }
    function showCmdRes(msg, err) { addRow('', '', msg, err ? 'is-err' : 'is-sys'); }      // command replies land in the same log
    function ghostByName(nm) { var c = null; if (net) net.ghosts.forEach(function (g) { if (g.name === nm) c = g; }); return c; }
    function onChat(from, text) {
        var now = performance.now(), mine = userName();
        for (var i = 0; i < echoQ.length; i++) {        // the relay may echo my own line back: show it once
            if (echoQ[i].text === text && from === mine && now - echoQ[i].t < 8000) { echoQ.splice(i, 1); return; }
        }
        var g = ghostByName(from);
        addRow(from, g && g.color >= 0 ? hexCss(g.color) : '', text);
    }
    function onGor(from, text) { addRow('neptr', '', text, 'is-gor'); }
    function sendChatLine(text) {
        text = text.slice(0, 200);
        addRow(userName(), hexCss(effColor()), text);                    // local echo always (relay may be down)
        echoQ.push({ text: text, t: performance.now() }); if (echoQ.length > 6) echoQ.shift();
        if (net && net.online) net.sendChat(text);
    }
    function openCmd(pre) {
        if (cmdOpen || state !== 'piloting') return;
        cmdGuardUntil = performance.now() + 600;
        cmdOpen = true;                                   // set BEFORE releasing the lock so pointerlockchange does not exit
        keys = Object.create(null); firing = false; mdx = mdy = 0;
        if (locked()) { try { document.exitPointerLock(); } catch (e) { /* ignore */ } }
        hud.classList.add('is-cmd');
        elChat.classList.add('is-open');
        elCmd.value = pre || '';
        elCmd.focus();
        try { elCmd.setSelectionRange(elCmd.value.length, elCmd.value.length); } catch (e) { /* ignore */ }
    }
    function closeCmd(relock) {
        if (!cmdOpen) return;
        cmdOpen = false;
        hud.classList.remove('is-cmd'); elChat.classList.remove('is-open');
        elCmd.value = ''; elCmd.blur();
        keys = Object.create(null); firing = false; mdx = mdy = 0;
        if (relock && state === 'piloting') {
            try {
                var p = document.body.requestPointerLock();
                if (p && p.catch) p.catch(function () { console.log('[ship] pointer lock refused; keyboard only'); });
            } catch (e) { /* ignore */ }
        }
    }
    elCmd.addEventListener('keydown', function (e) {
        e.stopPropagation();                              // galaxy3d's window Esc handler never sees these
        if (e.key === 'Escape') { e.preventDefault(); closeCmd(true); }
        else if (e.key === 'Enter') { e.preventDefault(); var v = elCmd.value; closeCmd(true); submitLine(v); }
    });
    elCmd.addEventListener('keyup', function (e) { e.stopPropagation(); });
    elCmd.addEventListener('keypress', function (e) { e.stopPropagation(); });
    function submitLine(v) {
        var t = String(v == null ? '' : v).trim();
        if (!t) return;
        if (t.charAt(0) === '/') runCommand(t); else sendChatLine(t);
    }
    function clearFight() {
        var i;
        for (i = 0; i < enemies.length; i++) hideEnemy(enemies[i]);
        for (i = 0; i < orbs.length; i++) { orbs[i].active = false; orbs[i].m.visible = false; }
        waveActive = false; qN = 0; ctarget = null; tlock = false; tailAlly = null; tailBy = null;
        killBolts();
    }
    function setWave(n) {
        var i;
        clearFight();
        for (i = 0; i < allies.length; i++) { allies[i].alive = false; allies[i].g.visible = false; }
        wave = Math.max(0, n - 1); wavePending = true; nextWave = gt + 0.05;
    }
    var HELP = 'KEYS · T cycle target · A/D x2 roll · Q focus · Z wingmen focus fire · ENTER or / chat · CMDS · /wave N · /peaceful · /hostile · /difficulty 1-3 · /user NAME · /color #hex|name · /gorcave TEXT · /help';
    function applyDifficultyLive() {
        var D = DIFFS[difficulty];
        for (var i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive || e.bDmg === undefined) continue;
            e.dmg = e.bDmg * D.dmg; e.speed = e.bSpeed * D.spd;
        }
    }
    function applyProfile() {                  // push the current profile's color/name to the local hull and the relay
        if (hullObj) tintGhost(hullObj, profileColor());
        if (hullLow) tintGhost(hullLow, profileColor());
        exColor(exMe, effColor());
        if (net) { net.setName(userName()); net.setColor(effColor()); }
    }
    function runCommand(str) {
        var s0 = String(str == null ? '' : str).trim().toLowerCase().replace(/^\/+/, ''), m;
        var ok = function (t) { showCmdRes(t, false); return t; };
        var bad = function (t) { showCmdRes(t, true); return t; };
        if (!s0) return bad('EMPTY COMMAND · /help');
        var raw = String(str == null ? '' : str).trim().replace(/^\/+/, '');
        if ((m = /^user(?:\s+(.*))?$/i.exec(raw))) {
            var un = String(m[1] || '').replace(/[^\x20-\x7e]/g, '').trim().slice(0, 16);
            if (!un || un === '__proto__') return bad('USAGE · /user NAME');
            curUser = un; ensureProfile(un);
            applyProfile(); writeSave();
            announce('USER ' + un.toUpperCase());
            return ok('USER ' + un);
        }
        if ((m = /^colou?r(?:\s+(.*))?$/i.exec(raw))) {
            var cn = String(m[1] || '').trim().toLowerCase(), cv = Object.prototype.hasOwnProperty.call(COLOR_NAMES, cn) ? COLOR_NAMES[cn] : null, hx = null;
            if (cv == null) hx = /^#?([0-9a-f]{6}|[0-9a-f]{3})$/i.exec(cn);
            if (cv == null && !hx) return bad('USAGE · /color #8a5cff | orange violet cyan red green white');
            if (cv == null) cv = parseInt(hx[1].length === 3 ? hx[1].replace(/./g, '$&$&') : hx[1], 16);
            ensureProfile(userName()).color = cv;                       // writes into the CURRENT user's profile: the one theme source
            if (!curUser) curUser = userName();
            applyProfile(); writeSave();
            return ok('COLOR ' + hexCss(cv));
        }
        if ((m = /^gorcave(?:\s+([\s\S]*))?$/i.exec(raw))) {
            var gt0 = String(m[1] || '').trim().replace(/^["'\u201c\u2018]([\s\S]*)["'\u201d\u2019]$/, '$1').trim().slice(0, 400);
            if (!gt0) return bad('USAGE · /gorcave TEXT');
            if (!net || !net.online) { addRow('', '', 'neptr is offline', 'is-sys'); return 'OFFLINE'; }
            addRow(userName(), hexCss(effColor()), gt0);
            net.sendGor(gt0);
            return 'GORCAVE';
        }
        if ((m = /^wave\s*(\d+)$/.exec(s0))) {
            var n = clamp(parseInt(m[1], 10), 1, 999);
            setWave(n);
            if (peaceful) { peaceful = false; }
            writeSave();
            return ok('WAVE ' + n);
        }
        if ((m = /^diff(?:iculty)?\s*([123])$/.exec(s0))) {
            difficulty = parseInt(m[1], 10);
            applyDifficultyLive();
            writeSave();
            return ok('DIFFICULTY ' + difficulty);
        }
        var parts = s0.split(/\s+/), c = parts[0];
        if (c === 'wave') return bad('USAGE · /wave N');
        if (c === 'diff' || c === 'difficulty') return bad('USAGE · /difficulty 1-3');
        if (c === 'peaceful') {
            var w = shownWave();
            peaceful = true;
            clearFight();
            for (var i = 0; i < allies.length; i++) { allies[i].alive = false; allies[i].g.visible = false; }
            wave = Math.max(0, w - 1); wavePending = true; bossWave = false;
            writeSave();
            return ok('PEACEFUL');
        }
        if (c === 'hostile') {
            if (peaceful) { peaceful = false; nextWave = gt + 1; }
            writeSave();
            return ok('HOSTILE');
        }
        if (c === 'help') return ok(HELP);
        return bad('UNKNOWN COMMAND · /help');
    }

    // ─── rev 9 / 9b / 9c combat helpers ─────────────────────────────
    function waveClear() {
        waveActive = false; wavePending = true;
        var big = bossWave || wave % 4 === 0;                               // every 4th wave / boss: longer breather
        nextWave = gt + (big ? 15 : BREATHER_MIN + Math.random() * BREATHER_VAR);
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
        vA.copy(shipRoot.position).addScaledVector(vF, 14 * L);
        spawnDrop(vA, DROP_VAL);                                            // wave-clear bonus orb
        announce('WAVE ' + wave + ' CLEAR' + (chainN > 1 ? ' · CHAIN ' + chainN : ''));
    }
    function onGraze() {
        if (od > 0) return;
        graze++; grazeIdle = 0;
        if (graze >= GRAZE_FULL) { graze = 0; od = OD_T; announce('OVERDRIVE'); }
    }
    function noseAngle(e) {                                                 // angle (rad) between the ship's nose and a hostile
        vW1.copy(e.g.position).sub(shipRoot.position);
        var d = vW1.length();
        if (d < 1e-6) return 0;
        vW2.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
        return Math.acos(clamp(vW1.dot(vW2) / d, -1, 1));
    }
    // T = cycle targets (nearest-to-nose first, next-further-from-nose each press), only when chat is closed
    function cycleTarget() {
        if (state !== 'piloting' || cmdOpen || dead) return null;
        var cur = (ctarget && ctarget.alive) ? noseAngle(ctarget) : -1, best = null, ba = 1e9, first = null, fa = 1e9, i, lim = LOCK_RANGE * L;
        for (i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive || e.g.position.distanceTo(shipRoot.position) > lim) continue;
            var a = noseAngle(e);
            if (a < fa) { fa = a; first = e; }
            if (a > cur + 1e-6 && a < ba) { ba = a; best = e; }
        }
        ctarget = best || first; tlock = !!ctarget;
        return ctarget;
    }
    function updateLock(dt) {
        if (ctarget && (!ctarget.alive || ctarget.g.position.distanceTo(shipRoot.position) > LOCK_RANGE * 1.3 * L)) { ctarget = null; tlock = false; }
        if (tlock && ctarget) return;
        atT -= dt;
        if (atT > 0) return;
        atT = 0.15;
        var best = null, ba = 0.5, lim = LOCK_RANGE * L;
        for (var i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive || e.g.position.distanceTo(shipRoot.position) > lim) continue;
            var a = noseAngle(e);
            if (a < ba) { ba = a; best = e; }
        }
        ctarget = best;
    }
    function zFocusFire() {
        if (state !== 'piloting' || dead || cmdOpen) return;
        if (!ctarget || !ctarget.alive) { addRow('', '', 'NO TARGET · T TO LOCK ONE', 'is-sys'); return; }
        zTarget = ctarget; zT = Z_FOCUS_T;
        addRow('', '', 'WINGMEN FOCUS ' + (ctarget.isBoss ? 'BOSS' : ROLE_T[ctarget.role].name), 'is-sys');
    }
    function startRoll(dir) {
        if (state !== 'piloting' || dead || cmdOpen || rollCd > 0 || rollT > 0) return false;
        rollT = ROLL_T; rollCd = ROLL_CD; rollDir = dir;
        return true;
    }
    function tapKey(code) {                                                 // double-tap A/D within TAP_MS = dodge roll
        var now = performance.now();
        if (now - tapT[code] < TAP_MS) { if (startRoll(code === 'KeyA' ? 1 : -1)) tapT[code] = 0; else tapT[code] = now; }
        else tapT[code] = now;
    }

    // ─── combat HUD (every DOM write is gated on a changed value) ───
    function hudCombat(wdt) {
        var v, i, e;
        v = rollCd > 0 ? Math.round((1 - rollCd / ROLL_CD) * 20) : 20;
        if (v !== cRoll) { cRoll = v; elRoll.style.setProperty('--p', (v * 5) + '%'); elRoll.classList.toggle('is-ready', v >= 20); }
        var cs = chainN >= 2 ? 'x' + chainN + (chainN >= 3 ? ' · x2 ORBS' : '') : '';
        if (cs !== cChain) { cChain = cs; elChain.textContent = cs; }
        v = od > 0 ? Math.round(od / OD_T * 100) : Math.round(graze / GRAZE_FULL * 100);
        if (v !== cGraze) { cGraze = v; elGrazeU.style.transform = 'scaleX(' + (v / 100) + ')'; }
        var hot = od > 0;
        if (hot !== cOd) { cOd = hot; elGraze.classList.toggle('is-od', hot); elRet.classList.toggle('is-hot', hot); elGraze.firstChild.textContent = hot ? 'OVERDRIVE' : 'GRAZE'; }
        v = Math.round(focusE / FOCUS_MAX * 50);
        if (v !== cFocus) { cFocus = v; elFocusU.style.transform = 'scaleX(' + (v / 50) + ')'; }
        if (focusing !== cFocusOn) { cFocusOn = focusing; hud.classList.toggle('is-focus', focusing); }
        var tk = Math.round((1 + Math.min(comboN, 12) * 0.06 + tickPunch * 0.45) * 20) / 20;       // reticle tick grows + brightens with combo
        if (tk !== cTickK) { cTickK = tk; elRet.style.setProperty('--tk', tk); }
        // lead pip + lock bracket on the current target
        var P = shipRoot.position, W = window.innerWidth, H = window.innerHeight, txt = '', solid = false, showP = false, showL = false, px = 0, py = 0, lx = 0, ly = 0;
        var ct = ctarget && ctarget.alive ? ctarget : null;
        if (ct && !dead) {
            vW3.copy(ct.g.position);
            if (ct.isBoss) vW3.add(vE.set(CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2]).multiplyScalar(ct.len).applyQuaternion(ct.g.quaternion));
            vA.copy(vW3).project(camera);
            if (vA.z < 1) { showL = true; lx = (vA.x * 0.5 + 0.5) * W; ly = (-vA.y * 0.5 + 0.5) * H; }
            var bs = P_BOLT_SPEED * L, t = vW3.distanceTo(P) / bs;
            for (i = 0; i < 2; i++) { vB.copy(vW3).addScaledVector(ct.vel, t); t = vB.distanceTo(P) / bs; }      // iterate twice: target.pos + target.vel x dist / boltSpeed
            vA.copy(vB).project(camera);
            if (vA.z < 1) {
                showP = true; px = (vA.x * 0.5 + 0.5) * W; py = (-vA.y * 0.5 + 0.5) * H;
                vU.copy(vB).sub(camera.position);
                vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                solid = vU.dot(vF) > vU.length() * Math.cos(1.5 * Math.PI / 180);                              // solid within 1.5 deg of the reticle
            }
            txt = (ct.isBoss ? 'BOSS' : ROLE_T[ct.role].name) + (ct.fstate === 'flee' || ct.fstate === 'regen' || ct.fstate === 'tele' ? ' · FLEEING' : '') + '  ·  ' + Math.round(vW3.distanceTo(P) / L) + ' L';
        }
        var pk = showP ? px.toFixed(0) + ',' + py.toFixed(0) : '';
        if (pk !== cPip) { cPip = pk; if (showP) elPip.style.transform = 'translate(' + px.toFixed(1) + 'px,' + py.toFixed(1) + 'px)'; elPip.style.display = showP ? 'block' : 'none'; }
        if (solid !== cPipSolid) { cPipSolid = solid; elPip.classList.toggle('is-solid', solid); }
        var lk = showL ? lx.toFixed(0) + ',' + ly.toFixed(0) : '';
        if (lk !== cLockTxt) { cLockTxt = lk; if (showL) elLock.style.transform = 'translate(' + lx.toFixed(1) + 'px,' + ly.toFixed(1) + 'px)'; elLock.style.display = showL ? 'block' : 'none'; }
        cTargetEnemy = txt;
        // bearing chevrons: off-screen hostiles (max ARROW_MAX) + the wingman in peril
        var shown = 0, sx, sy, rot, a;
        for (i = 0; i <= ENEMY_MAX + 1; i++) {
            a = arrows[i]; e = null; var show = false;
            if (!dead) {
                if (i <= ENEMY_MAX) { e = enemies[i]; if (e.alive && shown < ARROW_MAX) vW3.copy(e.g.position); else e = null; }
                else if (tailAlly && tailAlly.alive) { e = tailAlly; vW3.copy(tailAlly.g.position); }
            }
            if (e) {
                vA.copy(vW3).applyMatrix4(camera.matrixWorldInverse);      // camera space: -z is ahead
                if (vA.z < 0) { vB.copy(vA).applyMatrix4(camera.projectionMatrix); sx = vB.x; sy = vB.y; } else { sx = 9; sy = 9; }
                if (vA.z >= 0 || Math.abs(sx) > 0.93 || Math.abs(sy) > 0.9) {
                    show = true; if (i <= ENEMY_MAX) shown++;
                    // scale the camera-space direction so it lands on the screen rectangle edge (works behind you too)
                    var dx = vA.x, dy = -vA.y, dm = Math.max(Math.abs(dx) / (W / 2 - 34), Math.abs(dy) / (H / 2 - 34), 1e-6);
                    var ex = W / 2 + dx / dm, ey = H / 2 + dy / dm;
                    rot = Math.round(Math.atan2(dy, dx) * 57.2958 + 90);
                    if (Math.abs(ex - a.x) > 0.5 || Math.abs(ey - a.y) > 0.5 || rot !== a.r) {
                        a.x = ex; a.y = ey; a.r = rot;
                        a.el.style.transform = 'translate(' + ex.toFixed(1) + 'px,' + ey.toFixed(1) + 'px) rotate(' + rot + 'deg)';
                    }
                }
            }
            if (show !== a.on) { a.on = show; a.el.style.display = show ? 'block' : 'none'; }
        }
    }

    // ─── flight ─────────────────────────────────────────────────────
    // chase camera: rigid on the ship's orientation (no lag), offset behind and above
    function chaseTargets(dt) {
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
        vU.copy(Y).applyQuaternion(shipRoot.quaternion);
        var d = CAM_L * L * (1 + 0.7 * pulse);
        camPos.copy(shipRoot.position).addScaledVector(vF, -d).addScaledVector(vU, CAM_UP * CAM_L * L);
        camQuat.copy(shipRoot.quaternion);
    }
    function targetSpeed(boosting) {
        // throttle in [THROTTLE_MIN,0] maps to [-REVERSE,0]
        var base = throttle >= 0 ? throttle * CRUISE : (throttle / THROTTLE_MIN) * -REVERSE;
        if (boosting && throttle > 0.02) base *= BOOST / CRUISE;
        else if (boosting && throttle < -0.02) base = (throttle / THROTTLE_MIN) * -0.7 * BOOST;     // rev 9: reverse boost = 0.7 x boost
        return base;
    }
    function step(dt) {
        if (dt > 0.05) dt = 0.05;
        if (dt <= 0) return;
        if (boarding || exiting) { cinematicStep(dt); return; }
        var i, bodies = gatherBodies();
        var P = shipRoot.position;
        gt += dt; psCalls = 0;
        var tS = prof.on ? performance.now() : 0, tM = 0, tA0 = 0;
        // rev 9c #3: Focus (hold Q) slows the WORLD (enemies, bolts, allies) to 0.35x; your flight and mouse stay real-time.
        // Kill hit-stop freezes the world for 2 frames (HUD and your flight are unaffected).
        var fwant = !dead && !!keys.KeyQ && (focusing ? focusE > 0 : focusE > 0.3);
        if (fwant) { focusing = true; focusE = Math.max(0, focusE - dt); } else { focusing = false; focusE = Math.min(FOCUS_MAX, focusE + dt * FOCUS_MAX / FOCUS_RECHARGE); }
        timeScale = focusing ? FOCUS_TS : 1;
        var wdt = dt * timeScale;
        if (hitStopN > 0) { hitStopN--; wdt = 0; }
        if (chainT > 0) { chainT -= dt; if (chainT <= 0) chainN = 0; }
        if (comboT > 0) { comboT -= dt; if (comboT <= 0) comboN = 0; }
        if (od > 0) od = Math.max(0, od - dt);
        else if (graze > 0) { grazeIdle += dt; if (grazeIdle > 3) graze = Math.max(0, graze - dt * 3); }
        if (zT > 0) zT -= dt;
        if (rollCd > 0) rollCd -= dt;
        tickPunch = Math.max(0, tickPunch - dt * 12);
        fovKick *= Math.exp(-7 * dt);

        if (dead) {
            deathT -= dt;
            if (deathT <= 0) {
                // restart at the dock: waves, kills, hp reset; still piloting
                placeAtDock();
                resetGame(shownWave(), kills);
                vel.set(0, 0, 0); speed = 0; throttle = 0; pulse = 0; mdx = mdy = 0;
                shipRoot.visible = true;
            }
        } else {
            // throttle
            if (keys.KeyW) throttle += THROTTLE_RATE * dt;
            if (keys.KeyS) throttle -= THROTTLE_RATE * 1.5 * dt;
            throttle = clamp(throttle, THROTTLE_MIN, 1);
        }
        var boosting = !dead && !!(keys.ShiftLeft || keys.ShiftRight) && Math.abs(throttle) > 0.02;
        boostNow = boosting;

        // direct mouse: this frame's pointer delta maps straight to rotation
        // weighted mouse: the pointer delta sets a TARGET turn rate, and the
        // actual rate chases it fast (MOUSE_WEIGHT/s). Direct (infinite rate)
        // felt too light to Emory; the phase-1 5/s felt laggy. Split the difference.
        if (cmdOpen) { mdx = mdy = 0; firing = false; }
        // rev 9 weight curve: sign(d) * |d|^1.6 (d = raw rate / 2 rad/s): precise small moves, fast big ones
        var tYaw = clamp(wcurve(-mdx * MOUSE_SENS / Math.max(dt, 1e-3)), -MAX_MOUSE_RATE, MAX_MOUSE_RATE);
        var tPit = clamp(wcurve(-mdy * MOUSE_SENS / Math.max(dt, 1e-3)), -MAX_MOUSE_RATE, MAX_MOUSE_RATE);
        var mouseRaw = Math.sqrt(mdx * mdx + mdy * mdy);
        mdx = mdy = 0;
        var mk = damp(MOUSE_WEIGHT, dt);
        yawRate += (tYaw - yawRate) * mk; pitRate += (tPit - pitRate) * mk;
        var yawA = yawRate * dt, pitA = pitRate * dt;
        var rollA = dead ? 0 : ((keys.KeyA ? 1 : 0) - (keys.KeyD ? 1 : 0)) * ROLL_RATE * dt;
        if (dead) { yawA = pitA = 0; }
        // rev 9b #3: dodge roll = one full barrel roll over ROLL_T s (damage cut applied in hurtPlayer), plus a 6 L sidestep below
        var rolling = rollT > 0 && !dead, rollP = rolling ? 1 - rollT / ROLL_T : 0;
        if (rolling) { var rdt = Math.min(dt, rollT); rollA += rollDir * 6.2832 * rdt / ROLL_T; rollT = Math.max(0, rollT - dt); }
        // bank: the hull leans into the turn (roll chases BANK_K x yawRate; only the change is applied, so it returns level)
        var bankT = dead ? 0 : clamp(BANK_K * yawRate, -BANK_MAX, BANK_MAX), bankN = bank + (bankT - bank) * damp(BANK_SPRING, dt);
        rollA += bankN - bank; bank = bankN;
        shipRoot.quaternion.multiply(qA.setFromAxisAngle(X, pitA))
            .multiply(qB.setFromAxisAngle(Y, yawA))
            .multiply(qB.setFromAxisAngle(Z, rollA)).normalize();
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);

        // aim assist: soft magnet toward the hostile nearest the nose (4 deg cone), off while yanking/pulsing/dead
        var assistOn = false;
        if (!dead && pulse < 0.05 && mouseRaw / Math.max(dt, 1e-3) < AIM_YANK) {
            var bestAng = AIM_CONE, bx = 0, by = 0, bz = 0, aj;
            // measure from the chase-camera origin: the reticle ray (and the bolt convergence) starts there, not at the hull
            vU.copy(Y).applyQuaternion(shipRoot.quaternion);
            vR1.copy(P).addScaledVector(vF, -CAM_L * L).addScaledVector(vU, CAM_UP * CAM_L * L);
            for (aj = 0; aj < enemies.length + orbs.length; aj++) {
                var ae = aj < enemies.length ? enemies[aj] : orbs[aj - enemies.length], ap;
                if (aj < enemies.length) {
                    if (!ae.alive) continue;
                    ap = vE.copy(ae.g.position);
                    if (ae.isBoss) ap.add(vTmp.set(CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2]).multiplyScalar(ae.len).applyQuaternion(ae.g.quaternion));
                } else { if (!ae.active) continue; ap = vE.copy(ae.m.position); }
                vA.subVectors(ap, vR1);
                var ad = vA.length();
                if (ad < 1e-3) continue;
                var aang = Math.acos(clamp(vA.dot(vF) / ad, -1, 1));
                if (aang < bestAng) { bestAng = aang; bx = vA.x / ad; by = vA.y / ad; bz = vA.z / ad; assistOn = true; }
            }
            if (assistOn) {
                vB.set(bx, by, bz);
                vU.crossVectors(vF, vB);
                var sl = vU.length();
                if (sl > 1e-6) {
                    var rot = Math.min(bestAng, AIM_RATE * (bestAng / AIM_CONE) * dt);
                    shipRoot.quaternion.premultiply(qA.setFromAxisAngle(vU.divideScalar(sl), rot)).normalize();
                    vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                }
            }
        }
        if (assistOn !== cLocked) { cLocked = assistOn; elRet.classList.toggle('is-locked', assistOn); }

        // soft boundary: gently steer home, hard clamp just past the edge
        var pd = P.length();
        edgeNow = pd > EDGE_R;
        if (edgeNow && !dead) {
            var over = Math.min(1, (pd - EDGE_R) / (0.1 * EDGE_R));
            vU.copy(Y).applyQuaternion(shipRoot.quaternion);
            mM.lookAt(P, ORIGIN, vU);
            qA.setFromRotationMatrix(mM);
            shipRoot.quaternion.rotateTowards(qA, dt * (0.5 + 1.8 * over));
            P.multiplyScalar(1 - Math.min(0.5, (pd - EDGE_R) / pd * damp(1.5, dt)));
            if (pd > 1.12 * EDGE_R) P.multiplyScalar(1.12 * EDGE_R / pd);
        }

        // targeting + pulse lookahead in one pass over bodies
        var bestScore = Infinity, best = null, nearGap = Infinity;
        var look = 0.5 * speed + (speed < 0 ? -1 : 1) * vel.length() * dt;     // scales with the live speed: at 60 u/s the drop still lands in time
        vB.copy(P).addScaledVector(vF, look);
        for (i = 0; i < bodies.length; i++) {
            var b = bodies[i], ap = b.node.anchor.position;
            vA.subVectors(ap, P);
            var dist = vA.length();
            b.dist = dist;
            var gap = Math.sqrt(segDistSq(P.x, P.y, P.z, vB.x, vB.y, vB.z, ap.x, ap.y, ap.z)) - PULSE_DROP_F * b.R;
            if (gap < nearGap) nearGap = gap;
            if (dist < 1) continue;
            var ang = Math.acos(clamp(vA.dot(vF) / dist, -1, 1));
            ang = Math.max(0, ang - Math.asin(Math.min(1, b.R / dist)));    // big close planets are easy to aim at
            if (ang < TARGET_CONE) {
                var sc = ang < 1e-4 ? -1 + dist * 1e-9 : ang;            // inside the disc: nearer wins
                if (sc < bestScore) { bestScore = sc; best = b; }
            }
        }
        target = best;

        // frame drag: every body orbits at ~15-20 u/s, faster than boost (5 u/s), so near a body the
        // ship rides that body's motion (blended in by proximity). Without this a planet can
        // never be reached at NMS speeds. Skipped if a rec was not stepped last frame.
        stepN++;
        var dragB = null, dragW = 0;
        for (i = 0; i < bodies.length; i++) {
            var db = bodies[i];
            if (db.pN === stepN - 1 && !dead) {
                var w = clamp((10 * db.R - db.dist) / (8 * db.R), 0, 1);
                if (w > dragW) { dragW = w; dragB = db; }
            }
        }
        if (dragB) {
            var dcp = dragB.node.anchor.position;
            var ddx = (dcp.x - dragB.px) * dragW, ddy = (dcp.y - dragB.py) * dragW, ddz = (dcp.z - dragB.pz) * dragW;
            P.x += ddx; P.y += ddy; P.z += ddz;
            // everything in the fight rides the same frame, or enemies would be left behind at 20 u/s
            for (i = 0; i < enemies.length; i++) if (enemies[i].alive) { enemies[i].g.position.x += ddx; enemies[i].g.position.y += ddy; enemies[i].g.position.z += ddz; enemies[i].wander.x += ddx; enemies[i].wander.y += ddy; enemies[i].wander.z += ddz; enemies[i].base.x += ddx; enemies[i].base.y += ddy; enemies[i].base.z += ddz; }
            bsig.org.x += ddx; bsig.org.y += ddy; bsig.org.z += ddz;       // rev 12: world-anchored points ride the same frame
            for (i = 0; i < mines.length; i++) if (mines[i].active) { mines[i].m.position.x += ddx; mines[i].m.position.y += ddy; mines[i].m.position.z += ddz; }
            for (i = 0; i < allies.length; i++) if (allies[i].alive) { allies[i].g.position.x += ddx; allies[i].g.position.y += ddy; allies[i].g.position.z += ddz; }
            for (i = 0; i < drops.length; i++) if (drops[i].active) { drops[i].m.position.x += ddx; drops[i].m.position.y += ddy; drops[i].m.position.z += ddz; }
            for (i = 0; i < orbs.length; i++) if (orbs[i].active) { orbs[i].m.position.x += ddx; orbs[i].m.position.y += ddy; orbs[i].m.position.z += ddz; }
            var bpa = fx.bolts.geometry.attributes.aPos.array;
            for (i = 0; i < bolts.length; i++) if (bolts[i].active) { var bi3 = bolts[i].id * 3; bpa[bi3] += ddx; bpa[bi3 + 1] += ddy; bpa[bi3 + 2] += ddz; bolts[i].prev.x += ddx; bolts[i].prev.y += ddy; bolts[i].prev.z += ddz; }
        }
        for (i = 0; i < bodies.length; i++) {
            var db2 = bodies[i], dc2 = db2.node.anchor.position;
            db2.px = dc2.x; db2.py = dc2.y; db2.pz = dc2.z; db2.pN = stepN;
        }

        // pulse drive: spin-up charge, drops if the path would enter 4R
        var pulseHeld = !dead && !!keys.Space && nearGap > 0;
        // rev 9 (Emory): pulse ramps EXPONENTIALLY while held, pulse(t) = 1 - e^(-t/tau), tau 3 s, to PULSE_SPEED 60 u/s
        // (speed = base + (PULSE_SPEED - base) x pulse); release bleeds it off over 1 s; a planet ahead still drops it fast
        if (pulseHeld) { pulseT += dt; pulse = 1 - Math.exp(-pulseT / PULSE_TAU); }
        else { pulse = Math.max(0, pulse - dt * (nearGap <= 0 ? 8 : PULSE_DECAY)); pulseT = pulse > 0 ? -PULSE_TAU * Math.log(Math.max(1e-4, 1 - pulse)) : 0; }

        var tgt = targetSpeed(boosting);
        if (pulse > 0) tgt += ((throttle < -0.02 ? -0.6 * PULSE_SPEED : PULSE_SPEED) - tgt) * pulse;      // rev 9: reverse pulse = 0.6 x pulse
        // rev 9b #3: boost-drift exit = +25 % speed for 1 s, decaying
        if (driftBoost > 0) { tgt *= 1 + DRIFT_BOOST * driftBoost; driftBoost = Math.max(0, driftBoost - dt); }
        var rate = tgt >= speed ? (pulse > 0.01 ? 6 : 1.8) : (pulse > 0.01 ? 4 : 2.8);
        if (nearGap <= 0 && Math.abs(speed) > CRUISE * 2) rate = 5;               // brake hard when pulse was cut by a body
        speed += (tgt - speed) * damp(rate, dt);

        // velocity chases forward*speed: gentle lateral drift after hard turns
        // rev 9b #3: while boosting through a turn the slip drops to 0.4/s (the nose swings, the velocity does not); releasing boost
        // within 1.2 s of drifting pays +25 % speed for 1 s.
        var turning = boosting && Math.abs(yawRate) > DRIFT_YAW;
        if (turning) driftT = 1.2; else driftT = Math.max(0, driftT - dt);
        if (wasBoost && !boosting && driftT > 0) { driftBoost = 1; driftT = 0; }
        wasBoost = boosting;
        vA.copy(vF).multiplyScalar(speed);
        vel.lerp(vA, damp(turning ? DRIFT_SLIP : 3, dt));

        vP0.copy(P);
        if (!dead) P.addScaledVector(vel, dt);
        if (bounceV.lengthSq() > 1e-12) { P.addScaledVector(bounceV, dt); bounceV.multiplyScalar(Math.exp(-2.5 * dt)); }
        if (ramInv > 0) ramInv -= dt;
        if (ramFlash > 0) ramFlash -= dt;
        var psA = null;
        if (ps) { try { ps.update(dt, P); psA = ps.active; } catch (e) { planetFail(e); psA = null; } }
        if (rolling) { vA.copy(X).applyQuaternion(shipRoot.quaternion); P.addScaledVector(vA, -rollDir * (ROLL_SIDE * L / ROLL_T) * 1.5708 * Math.sin(3.1416 * rollP) * dt); }

        // HARD sphere at 1.15 R against CURRENT anchor positions (planets move):
        // (1) swept test along this frame's segment; (2) already inside -> pop to surface.
        for (var pass = 0; pass < 2; pass++) {
            for (i = 0; i < bodies.length; i++) {
                var cb = bodies[i], c = cb.node.anchor.position, shell = HARD_F * cb.R;
                if (psA && cb.node === psA) {                       // rev 12: inside 1.25 R the hard shell becomes the terrain floor + 1.5 L (slide along it)
                    vD.subVectors(P, c);
                    var pdd = vD.length();
                    if (pdd < 1.25 * cb.R) {
                        var fo = psFloor(P), fr = (fo ? fo.r : cb.R * 1.04) + 1.5 * L;
                        if (pdd < fr) {
                            if (pdd < 1e-3) vD.copy(vF).negate(); else vD.divideScalar(pdd);
                            P.copy(c).addScaledVector(vD, fr);
                            vP0.copy(P);
                            var vn2 = vel.dot(vD);
                            if (vn2 < 0) vel.addScaledVector(vD, -vn2);
                            var fv = vel.dot(vF);
                            if (fv < speed) speed = fv;
                        }
                        continue;
                    }
                }
                vD.subVectors(P, vP0);
                vTmp.subVectors(vP0, c);
                var a2 = vD.lengthSq(), b2 = 2 * vTmp.dot(vD), c2 = vTmp.lengthSq() - shell * shell;
                var tHit = -1;
                if (c2 <= 0) tHit = 0;                                  // started inside the sphere
                else if (a2 > 1e-9) {
                    var disc = b2 * b2 - 4 * a2 * c2;
                    if (disc >= 0) { var tt = (-b2 - Math.sqrt(disc)) / (2 * a2); if (tt >= 0 && tt <= 1) tHit = tt; }
                }
                if (tHit < 0) {
                    // end point inside (planet drifted in after the swept test): same fix
                    vTmp.subVectors(P, c);
                    if (vTmp.lengthSq() >= shell * shell) continue;
                    tHit = 1;
                }
                vB.copy(vP0).addScaledVector(vD, tHit).sub(c);          // hit point relative to centre
                var bl = vB.length();
                if (bl < 1e-3) vB.copy(vF).negate(); else vB.divideScalar(bl);
                P.copy(c).addScaledVector(vB, shell + 0.01 * shell);
                vP0.copy(P);                                              // remaining move already consumed
                var vn = vel.dot(vB);
                if (vn < 0) vel.addScaledVector(vB, -vn);                // kill inward velocity
                var fwdV = vel.dot(vF);
                if (fwdV < speed) speed = fwdV;
            }
        }

        // rev 12: ramming. Touching a hostile = 45 HP (boss 70) + hard bounce; it takes 25 % of its max HP; 1 s invulnerability after a ram.
        if (!dead) {
            for (i = 0; i < enemies.length; i++) {
                en = enemies[i];
                if (!en.alive) continue;
                var rr = en.R + 0.45 * L;
                vD.subVectors(P, en.g.position);
                var rd2 = vD.lengthSq();
                if (rd2 >= rr * rr) continue;
                var rd = Math.sqrt(rd2);
                if (rd < 1e-4) vD.copy(vF).negate(); else vD.divideScalar(rd);
                P.copy(en.g.position).addScaledVector(vD, rr + 0.02 * L);
                if (ramInv <= 0) {
                    var rdmg = en.isBoss ? ((bsig.ph === 'act' && bsig.idx === 2) ? SIG_DMG[2] : RAM_BOSS) : RAM_PLAYER;
                    ramInv = RAM_INV; ramFlash = 0.9;
                    hurtPlayer(rdmg, undefined, 2.5, true);
                    burst(P, 10, 1, 14, 2); fx.flash(P, 0xff4040);
                    shake = Math.max(shake, 1.3 * L); fovKick = 5;
                    var rv = (en.isBoss ? 120 : 75) * L + Math.abs(speed) * 0.5;
                    bounceV.copy(vD).multiplyScalar(rv);
                    vel.copy(vD).multiplyScalar(rv * 0.3);
                    speed = Math.min(speed, 0.3 * CRUISE); pulse = 0; pulseT = 0;
                    damageEnemy(en, en.maxHp * RAM_ENEMY_FRAC, false, P);
                }
                break;
            }
        }
        // chase camera (rigid) + fov
        chaseTargets(dt);
        camera.position.copy(camPos);
        camera.quaternion.copy(camQuat);
        shake *= Math.exp(-6 * dt);
        if (shake > 1e-4 * L) { vA.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(2 * shake).applyQuaternion(camQuat); camera.position.add(vA); }
        var fovT = baseFov + (boosting ? FOV_BOOST : 0) + FOV_PULSE * pulse + (focusing ? 3 : 0) + fovKick;
        fov += (fovT - fov) * damp(4, dt);
        if (Math.abs(camera.fov - fov) > 0.01) { camera.fov = fov; camera.updateProjectionMatrix(); }
        camera.updateMatrixWorld(true);

        // exhaust cones at every thruster, streaks from velocity
        var exI = pulse > 0.05 ? 1 + pulse : ((boosting && throttle > 0) ? 1 : Math.max(0, throttle) * 0.7);
        exMe.holder.visible = !dead;
        exUpdate(exMe, P, shipRoot.quaternion, L, exI);
        fx.setMotion(vel, Math.min(1, Math.abs(speed) / 20), pulse > 0.5);

        // ─── combat ───
        var en;
        if (prof.on) { tM = performance.now(); prof.flight += tM - tS; }
        if (!dead) {
            // waves (rev 9c tempo): clear -> 8-12 s breather -> next; a spike/boss wave earns 15 s; stragglers cannot stall the 60 s cycle
            if (!peaceful) {
                if (waveActive && qN === 0 && !boss.alive && (aliveCount() === 0 || (gt - waveStartT > 70 && aliveCount() <= 2))) waveClear();
                if (!waveActive && gt >= nextWave && !boss.alive) spawnWave(bodies);
                if (qN > 0) pumpQueue(bodies);
            }
            // regen (40 %): wave 1 always, later waves after a short hit-free delay
            sinceHit += dt;
            if (hp < HP_MAX && sinceHit >= regenDelay) hp = Math.min(HP_MAX, hp + REGEN * dt);
            // player lasers (Overdrive: fire x1.5, dmg x1.25)
            fireCd -= dt;
            if (firing && fireCd <= 0) {
                var guard = 3, pdmg = P_BOLT_DMG * (od > 0 ? OD_DMG : 1), prate = P_FIRE_RATE * (od > 0 ? OD_FIRE : 1);
                while (fireCd <= 0 && guard--) {
                    fireCd += 1 / prate; playerFired = true;
                    vU.copy(Y).applyQuaternion(shipRoot.quaternion);
                    // convergence point on the reticle ray, 105 L out (35 old L)
                    vAim.copy(camera.position).addScaledVector(vF, 105 * L);
                    for (var side = 0; side < 2; side++) {
                        vTmp.copy(muzzles[side]).multiplyScalar(L).applyQuaternion(shipRoot.quaternion).add(P);
                        vD.subVectors(vAim, vTmp).normalize();
                        fx.flash(vTmp, effColor());
                        fireBolt(vTmp.x, vTmp.y, vTmp.z, vD.x, vD.y, vD.z, P_BOLT_SPEED * L, P_BOLT_RANGE / P_BOLT_SPEED, false, pdmg, L * 2.7, L * 0.15);
                    }
                    shake = Math.min(shake + 0.03 * L, 0.6 * L);          // rev 9: a touch of kick per volley
                    if (net) { vB.subVectors(vAim, P).normalize(); net.sendFire(0, P.x, P.y, P.z, vB.x, vB.y, vB.z); }   // one announce per volley; victims decide hits
                }
                if (fireCd < -0.2) fireCd = 0;
            } else if (fireCd < 0) fireCd = 0;
        }
        updateLock(dt);
        // enemies (frozen while away because step() only runs while piloting). World sim runs on wdt (Focus / hit-stop).
        // Enemies > 120 L away update every other frame at double dt.
        if (wdt > 0) {
            for (i = 0; i < enemies.length; i++) {
                en = enemies[i];
                if (!en.alive) { if (en.beam && en.beam.visible) en.beam.visible = false; continue; }
                if (en.isBoss) { updateBoss(en, wdt, bodies); continue; }
                var dtE = wdt;
                if (en.g.position.distanceToSquared(P) > FAR2 * L * L) { en.far ^= 1; if (en.far) continue; dtE = wdt * 2; }
                en.prev.copy(en.g.position);
                updateEnemy(en, dtE, bodies);
                if (en.alive) en.vel.subVectors(en.g.position, en.prev).multiplyScalar(1 / dtE);
            }
            for (i = 0; i < allies.length; i++) { if (allies[i].alive) updateAlly(allies[i], wdt, bodies); else if (allies[i].ex.holder.visible) exOff(allies[i].ex); }
            updateOrbs(wdt);
        }
        updateDrops(dt);
        updatePeril(dt);
        if (prof.on) { tA0 = performance.now(); prof.ai += tA0 - tM; }
        // projectiles: move, planets eat bolts, hits
        var pr = 0.45 * L, nearR = 20 * L;
        fx.update(wdt, camera);                      // moves every bolt once, animates cones/streaks/sparks
        if (prof.on) { tM = performance.now(); prof.fx += tM - tA0; }
        for (i = 0; i < bolts.length; i++) {
            var bo = bolts[i];
            if (!bo.active) continue;
            var bp = fx.boltPos(bo.id, vBp);
            if (!bp) { bo.active = false; continue; }  // fx expired it
            var pv = bo.prev, dead1 = false, j;
            for (j = 0; j < bodies.length; j++) {
                var bc = bodies[j].node.anchor.position, br = bodies[j].R, rr = br + nearR;
                var bdx = bp.x - bc.x, bdy = bp.y - bc.y, bdz = bp.z - bc.z;
                if (bdx * bdx + bdy * bdy + bdz * bdz > rr * rr) continue;               // cheap reject: far from this body
                if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, bc.x, bc.y, bc.z) < br * br) { dead1 = true; break; }
            }
            if (!dead1) {
                if (bo.remote) {
                    // another pilot's bolt: victim-authoritative, tested against MY real ship only (never enemies/allies); dock radius is safe
                    if (!dead && segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, P.x, P.y, P.z) < pr * pr) {
                        dead1 = true; burst(bp, 4, 1, 8);
                        if (P.length() >= SAFE_R) hurtPlayer(bo.dmg, bo.owner);
                    }
                } else if (!bo.enemy) {
                    for (j = 0; j < enemies.length; j++) {
                        en = enemies[j];
                        if (!en.alive) continue;
                        var ep = en.g.position, dmg = bo.dmg, crit = false;
                        if (en.isBoss) {
                            // eye core = weak point: 2x, but ONLY while exposed (the 2 s telegraphed windup); closed, the carapace takes 0.5x
                            vE.set(CREATURE_EYE[0], CREATURE_EYE[1], CREATURE_EYE[2]).multiplyScalar(en.len).applyQuaternion(en.g.quaternion).add(ep);
                            var er = CREATURE_EYE_R * en.len * 0.85;
                            if (en.open && segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, vE.x, vE.y, vE.z) < er * er) { crit = true; dmg *= 2; }
                            else dmg *= 0.5;
                        }
                        if (crit || segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, ep.x, ep.y, ep.z) < en.R * en.R) {
                            dead1 = true;
                            damageEnemy(en, dmg, crit, bp);
                            break;
                        }
                    }
                    if (!dead1) {
                        for (j = 0; j < orbs.length; j++) {
                            var ob = orbs[j];
                            if (!ob.active) continue;
                            var orr = 2.4 * ORB_S * L, op = ob.m.position;
                            if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, op.x, op.y, op.z) < orr * orr) {
                                ob.hp -= bo.dmg; dead1 = true; burst(bp, 3, 1, 8);
                                if (ob.hp <= 0) { ob.active = false; ob.m.visible = false; burst(op, 10, 1, 12); }
                                break;
                            }
                        }
                    }
                } else {
                    var d2p = dead ? 1e30 : segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, P.x, P.y, P.z);
                    if (d2p < pr * pr) {
                        dead1 = true; bo.gz = 2; burst(bp, 4, 1, 8); hurtPlayer(bo.dmg, undefined, bo.boss ? 2 : 1);
                    } else {
                        // rev 9c #2: graze = a hostile bolt that passed within 1.5 L without hitting (credited once it is past)
                        if (bo.gz < 2) {
                            var gr = GRAZE_R * L;
                            if (d2p < gr * gr) bo.gz = 1;
                            else if (bo.gz === 1) { bo.gz = 3; onGraze(); }
                        }
                        // hostile bolts also hit allies (ally and player bolts never hit each other)
                        for (j = 0; j < allies.length; j++) {
                            var al = allies[j];
                            if (!al.alive) continue;
                            var apn = al.g.position;
                            if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, apn.x, apn.y, apn.z) < (0.5 * L) * (0.5 * L)) {
                                al.hp -= bo.dmg; dead1 = true; burst(bp, 3, 0, 8);
                                if (al.hp <= 0) killAlly(al);
                                break;
                            }
                        }
                    }
                }
            }
            if (dead1) { bo.active = false; fx.killBolt(bo.id); } else pv.copy(bp);
        }
        if (prof.on) { tA0 = performance.now(); prof.bolts += tA0 - tM; }
        if (waveMsgT > 0) waveMsgT -= dt;

        var tN = prof.on ? performance.now() : 0;
        if (net) { net.sendPos(); net.update(dt, dockA); ghostFx(); }
        var tH = prof.on ? performance.now() : 0;
        updateHud(vel.length(), aliveCount());
        hudCombat(wdt);
        if (prof.on) { var tE = performance.now(); prof.net += tH - tN; prof.hud += tE - tH; prof.total += tE - tS; prof.n++; }
    }

    // ─── idle loop: docked orbit, parked label ────────────────────────
    var lastT = performance.now();
    function idle() {
        requestAnimationFrame(idle);
        var now = performance.now(), dt = Math.min(0.1, (now - lastT) / 1000);
        lastT = now;
        if (state === 'piloting') return;
        clockT += dt;
        if (state === 'docked') {
            dockA += DOCK_RATE * dt;
            placeAtDock();
        }
        updateLabelLook();
        positionLabel();
        if (net) net.update(dt, dockA);
    }
    function positionLabel() {
        vA.copy(shipRoot.position).project(camera);
        var show = vA.z < 1 && vA.z > -1 && Math.abs(vA.x) < 1.1 && Math.abs(vA.y) < 1.1;
        if (show !== labelShown) { labelShown = show; label.style.display = show ? '' : 'none'; }
        if (!show) return;
        var x = (vA.x * 0.5 + 0.5) * window.innerWidth, y = (-vA.y * 0.5 + 0.5) * window.innerHeight;
        if (Math.abs(x - lastLX) < 0.1 && Math.abs(y - lastLY) < 0.1) return;
        lastLX = x; lastLY = y;
        label.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) translate(-50%, 18px)';
    }

    // park immediately so the first frame already has the ship in place
    dockA = 1.3;
    placeAtDock();
    idle();
    // ─── multiplayer (docs/multiplayer-plan.md) ─────────────────────
    // Lazy, after mount, never blocks. No relay = this block does nothing and the game is single-player.
    var rbA = new THREE.Vector3(), rbB = new THREE.Vector3(), rbC = new THREE.Vector3();
    function dockPose(angle, outP, outQ) {                  // same math as placeAtDock, for a ghost slot at `angle`
        var r = DOCK_R * root.sysR;
        outP.set(Math.cos(angle) * r, DOCK_Y * root.sysR + Math.sin(clockT * 0.7 + angle) * L * 0.15, Math.sin(angle) * r);
        pushOutOfBodies(outP, gatherBodies(), 2.2);
        vA.set(-Math.sin(angle), 0, Math.cos(angle));
        mM.lookAt(vTmp.set(0, 0, 0), vA, Y);
        outQ.setFromRotationMatrix(mM);
    }
    function remoteFire(id, w, ox, oy, oz, dx, dy, dz) {
        if (state !== 'piloting' || !(isFinite(ox + oy + oz + dx + dy + dz))) return;
        var live = 0, i;
        for (i = 0; i < bolts.length; i++) if (bolts[i].active && bolts[i].remote) live++;
        if (live > 62) return;                              // cap on incoming remote bolts
        var rg = net && net.ghosts.get(id), rcol = rg && rg.color >= 0 ? rg.color : C_A;
        rbA.set(dx, dy, dz);
        var dl = rbA.length();
        if (dl < 1e-6) return;
        rbA.divideScalar(dl);
        rbB.crossVectors(rbA, Y);
        if (rbB.lengthSq() < 1e-6) rbB.copy(X);
        rbB.normalize();
        for (var side = -1; side <= 1; side += 2) {
            // muzzle offset (+-0.26 L sideways, 0.45 L ahead), advanced by half the 150 ms render delay
            rbC.set(ox, oy, oz).addScaledVector(rbB, side * 0.26 * L).addScaledVector(rbA, 0.45 * L + P_BOLT_SPEED * L * 0.075);
            var b = fireBolt(rbC.x, rbC.y, rbC.z, rbA.x, rbA.y, rbA.z, P_BOLT_SPEED * L, P_BOLT_RANGE / P_BOLT_SPEED, false, P_BOLT_DMG, L * 2.7, L * 0.15, rcol);
            if (b) { b.remote = true; b.owner = id; }
        }
    }
    import('./ship-hull.js').catch(function () { return null; }).then(function (hm) {
        hullMod = hm;
        return import('./ship-net.js');
    }).then(function (m) {
        net = m.connect(engine, {
            L: L, galaxyScale: galS,
            getState: function () { return state; },
            getPose: function () {
                var p = shipRoot.position, q = shipRoot.quaternion;
                return { x: p.x, y: p.y, z: p.z, qx: q.x, qy: q.y, qz: q.z, qw: q.w, v: speed, st: (boostNow ? 1 : 0) | (pulse > 0.5 ? 2 : 0) | (dead ? 4 : 0), hp: hp / HP_MAX * 100 };
            },
            getPrefs: function (id) { return { name: userName(id), color: effColor() }; },
            buildGhost: function () {         // rev 12: docked ghosts show the low-LOD hull; the full hull swaps in while they fly (ghostFx)
                var g = null;
                try {
                    if (hullMod) {
                        var lo = hullMod.buildHull(THREE, { lod: 'low' }), fu = hullMod.buildHull(THREE);
                        g = new THREE.Group(); g.add(lo); g.add(fu); fu.visible = false;
                        g.userData.thrusters = fu.userData.thrusters; g.userData.gunMuzzles = fu.userData.gunMuzzles; g.userData.lo = lo; g.userData.fu = fu;
                    }
                } catch (e) { g = null; }
                return g || fallbackDart();
            },
            tint: tintGhost,
            dockSlot: dockPose,
            enter: enter,
            onClockSnap: function () { stepN += 2; },            // engine time just jumped: drop the frame-drag body history
            onRemoteFire: remoteFire,
            onChat: onChat, onGor: onGor,
            onKill: function (name, byMe) { if (byMe) { kills++; announce('KILL ' + String(name).toUpperCase()); writeSave(); } }
        });
        window.EMGOR_NET = net;       // debug / test hook
    }).catch(function () { /* single-player */ });
    var api = {
        enemies: enemies, shipRoot: shipRoot,
        destroy: function () { if (net) { net.destroy(); net = null; } },
        enter: enter, exit: exit, cmd: runCommand,
        get cmdOpen() { return cmdOpen; },
        get playerFired() { return playerFired; },
        allies: allies, orbs: orbs, boss: boss,
        spawnBoss: function () { spawnBoss(gatherBodies()); bossWave = true; },
        forceSig: function (i, now) { bsig.idx = clamp(i | 0, 0, 4); if (now) bsig.cd = 0; },     // debug: pick the boss signature attack
        get sig() { return { name: BOSS_ATTACKS[bsig.idx], idx: bsig.idx, ph: bsig.ph, t: bsig.t, cd: bsig.cd, hit: bsig.hit, hits: bsig.hits | 0, lastDmg: bsig.lastDmg }; },
        get sigDbg() { return { org: bsig.org.toArray(), dir: bsig.dir.toArray(), ax: bsig.ax.toArray(), P: shipRoot.position.toArray(), beamVis: sigBeam.visible }; },
        get god() { return god; }, set god(v) { god = !!v; },
        get planet() { return { loaded: !!ps, active: !!(ps && ps.active), depth: ps ? ps.depth : 0 }; },
        setWave: function (n) { setWave(n); },
        // debug / test hooks (rev 9): every state the spec names is readable here
        testBolt: function (offL, dL) {            // debug: an enemy bolt aimed past the ship, offL L to the side
            vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); vA.copy(X).applyQuaternion(shipRoot.quaternion);
            vTmp.copy(shipRoot.position).addScaledVector(vF, (dL || 30) * L).addScaledVector(vA, offL * L);
            return !!fireBolt(vTmp.x, vTmp.y, vTmp.z, -vF.x, -vF.y, -vF.z, 100 * L, 1, true, 6, L, L);
        },
        damage: function (e, d, crit) { return damageEnemy(e, d, !!crit, null); },
        roll: function (dir) { return startRoll(dir || 1); },
        cycleTarget: cycleTarget, zFocusFire: zFocusFire,
        prof: function (on) { if (on === false) { prof.on = false; } else { prof.on = true; } prof.ai = prof.bolts = prof.fx = prof.hud = prof.net = prof.flight = prof.total = prof.n = 0; return prof; },
        get profile() { var n = Math.max(1, prof.n), f = function (v) { return +(v / n).toFixed(3); }; return { frames: prof.n, total: f(prof.total), flight: f(prof.flight), ai: f(prof.ai), fx: f(prof.fx), bolts: f(prof.bolts), net: f(prof.net), hud: f(prof.hud) }; },
        get combat() { return { wave: wave, waveActive: waveActive, queued: qN, chain: chainN, graze: graze, od: od, focus: focusE, focusing: focusing, timeScale: timeScale, rollT: rollT, rollCd: rollCd, hp: hp, pulse: pulse, speed: speed, combo: comboN, hitStopN: hitStopN, zT: zT, drops: drops.filter(function (d) { return d.active; }).length, target: ctarget, tail: tailAlly ? tailLeft : 0, boostDrift: driftBoost, fovKick: fovKick, tickK: cTickK }; },
        get states() { return enemies.filter(function (e) { return e.alive; }).map(function (e) { return { role: e.isBoss ? 'BOSS' : ROLE_T[e.role].name, state: e.fstate, hp: Math.round(e.hp), max: e.maxHp, flees: e.fleeN, grace: +e.grace.toFixed(1), phase: e.bphase, open: e.open, dist: Math.round(e.g.position.distanceTo(shipRoot.position) / L) }; }); },
        drops: drops,
        get save() { try { return localStorage.getItem(SAVE_KEY); } catch (e) { return null; } },
        get state() { return state; },
        get stats() { return { hp: hp, wave: shownWave(), bossPhase: boss.bphase, boss: boss.alive ? boss.hp : 0, bossMax: boss.maxHp, allies: allies.filter(function (a) { return a.alive; }).length, kills: kills, enemies: aliveCount(), L: L, dead: dead, gt: gt }; }
    };
    window.EMGOR_SHIP = api;   // debug / test hook
    return api;
}
