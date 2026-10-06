/* ship.js — EMGOR galaxy "ship mode" game half. See docs/ship-mode.md (Revision 3).
   Lazy-loaded by galaxy3d.js after galaxy-ready; default export mount(engine).
   Everything here rides on the engine port (engine.THREE, setPilot...) so the
   ship flies through the very same scene, bodies and camera. Finite space:
   no procedural generation, no makeBody at runtime. */

import { createFx } from './ship-fx.js';
import * as ITM from './ship-items.js';
import * as LNG from './ship-lingo.js';
import * as ENM from './ship-enemies.js';
import * as CRF from './ship-craft.js';
import * as QST from './ship-quests.js';
import { generateEnemy, pickDistinctSeeds, ENEMY_ROLES, BOSS_TIERS, bossAttacksFor, bossName } from './ship-enemies.js';

// ─── tuning (world units / seconds; L = ship length) ────────────────
var CRUISE = 4, REVERSE = 1, BOOST = 60, PULSE_MAX = 450, PULSE_MAX2 = 675;   // world u/s (rev 13: x2); pulse ramps exponentially to PULSE_SPEED
var PULSE_E = 1.5, PULSE_BLEED = 10, PULSE_LN = Math.log(PULSE_MAX2 / BOOST);   // rev 20b: pulse starts AT BOOST SPEED the instant Space goes down (no slow start): v = BOOST * e^(t/1.5), rev 24: capped at PULSE_MAX = 450 u/s (Space) or PULSE_MAX2 = 675 (Space + Shift); tau 1.5 s; `pulse` (0..1) drives the fx
var THROTTLE_MIN = -0.2;           // S past zero reverses down to this
var THROTTLE_RATE = 0.6;           // throttle integration per second
var MOUSE_SENS = 0.0019;           // rad per px of pointer travel
var MOUSE_WEIGHT = 5.5;            // rev 13: 1/s, how fast the turn rate chases the weighted mouse target (angular inertia)
var BANK_K = 0.4, BANK_SPRING = 4, BANK_MAX = 0.6;   // rev 9: roll leans into yaw (rad per rad/s), spring 1/s, cap rad
var yawRate = 0, pitRate = 0, bank = 0, shake = 0;
var ROLL_RATE = 1.6;               // rad/s from A/D
var CAM_L = 3.2, CAM_UP = 0.3;     // chase distance / lift in ship lengths (rigid)
var BOARD_DUR = 1.6;               // rev 8: boarding / leaving cinematic (s)
var FOV_BOOST = 4, FOV_PULSE = 9, FOV_PLANET = 4, CAM_PLANET = 0.15;   // rev 20: within 4 R of a planet the chase cam backs off 15 % and the fov eases +4 (42 -> 46)     // rev 9: halved (the streaks sell speed now)
var DOCK_R = 1.05, DOCK_Y = 0.45, DOCK_RATE = 0.03;   // rev 12: dock radius / height as x root.sysR: outside every orbit, high above the plane
var HARD_F = 1.15;                 // hard collision sphere, x renderedRadius
var PULSE_LOOK = 1.0, PULLUP_LOOK = 1.6, PULLUP_RATE = 25 * Math.PI / 180;   // rev 13: pulse drops only if the swept path hits terrain/a body within PULSE_LOOK s; auto pull-up (rad/s) looks PULLUP_LOOK s ahead
var TERRAIN_DMG = 30, TERRAIN_INV = 0.8;    // terrain / body hit with a normal speed above BOOST: HP + bounce
var VEL_CHASE = 2.2, TURN_MAX = 8, GSHAKE_AT = 0.8;   // velocity chases the thrust vector at VEL_CHASE/s; max turn rate rad/s; g-shake above 0.8 of it
var FLIP_T = 0.8, FLIP_CD = 1.5, DRIFT_T = 1.2, DRIFT_CD = 1.5, DRIFT_CHASE = 0.18, DRIFT_TURN = 1.6;   // maneuvers: Immelmann flip (dbl-tap S), drift turn (hold Ctrl)
var ATM_R = 1.4, LF_ON = 1.6, LF_OFF = 1.7;     // rev 18: atmosphere top / local-frame engage / release (x R)
var ATM_BOOST = 12, ATM_PULSE = 40, ENTRY_T = 2, HOVER_L = 3, PROMPT_L = 3, CLEAR_L = 0.8, SUB_L = 0.45, APP_T = 1.2, ATM_FOV = 8;   // atmosphere speed caps (u/s), entry brake (s), hover / prompt altitude (L), hull clearance (L), substep (L), approach governor (s), fov gain
var BOUNDARY_F = 3.5;              // soft edge, x root.sysR
var TARGET_CONE = 6 * Math.PI / 180;
// combat
var HP_BASE = 100, HP_MAX = 100, REGEN = 8 * 0.4;       // rev 9c: passive regen at 40 %, kills feed the rest
var P_BOLT_SPEED = 270, P_BOLT_RANGE = 120, P_BOLT_DMG = 8, P_FIRE_RATE = 6;   // L/s, L, hp, volleys/s (2 bolts each)
var P_BOLT_LIFE = 20, P_BOLT_CULL = 12000;   // rev 19: bolts inherit the ship velocity, live 20 s, culled far from the pilot. rev 20: L shrank 4x (refR/12000): the same physical range (refR) is 12000 L; at 3000 L (153 u) bolts died inside a boss body
var E_BOLT_PER_LEN = 22, E_BOLT_MIN = 80, E_BOLT_MAX = 200;                    // enemy bolt speed = 22 L/s per L of body length, clamped (L/s)
var E_FIRE_PER_LEN = 18, E_RANGE_X = 1.5;                                      // fire range = 18 x body length; bolt range = 1.5 x fire range
var AGGRO_L = 80;                  // rev 21: squads engage inside 80 L
// rev 21 combat core
var WINDUP_T = 0.7, STALL_T = 1.0, STALL_X = 2, HARASS_R = 40, HARASS_WIND = 0.35;      // attack run: wind-up -> strike -> stall (x2 damage); harasser orbit radius (L)
var HEAT_PER = 0.07, HEAT_COOL = 0.35, OVERHEAT_T = 2, HEAT_GAP = 0.2;               // weapon heat per volley, cooling /s (only after HEAT_GAP s without firing), overheat lockout (s)
var RAM_T = 0.7, RAM_CD = 0.8, RAM_BASE = 40, RAM_SPD_K = 2, RAM_SELF = 12;          // Shift+LMB while boosting
var LOCK_JINK_T = 1.0, JINK_T = 0.45, JINK_V = 16, JINK_CD = 4;                       // lock-on evasion: kept in the 2.5 deg cone > 1 s -> one dodge roll
var UNIT_SHARD = 25, UNIT_KILL = 10, UNIT_LEADER = 20, UNIT_BOSS = 500, UNIT_ORB = 5, UNIT_CRATE = 50, QUIET_EVERY = 5;
var JET_UP = 3, JET_HOLD = 0.2, SCAN_T = 6, SCAN_CD = 1, HL_MAX = 24, BOMB_MAX = 6;
var RUN_IN_MIN = 60, RUN_IN_VAR = 30, RUN_OFF_DEG = 30;   // run-in point 25-35 old L = 75-105 L (L is 3x smaller)
var PEEL_MIN = 60, PEEL_VAR = 30, RUN_NEAR = 8, RUN_NEAR_T = 1.5, DIVE_CLOSE = 3;
var AIM_CONE = 2.5 * Math.PI / 180, AIM_RATE = 0.9 * 0.4, AIM_YANK = 900;   // rad, rad/s, px/s of raw mouse = 'yanking'
var ENEMY_MAX = 15, BOLT_MAX = 120;      // enemy slots = ENEMY_MAX + 1 (any slot can host a boss)
var FIRST_WAVE = 3, WAVE_MIN = 12, WAVE_VAR = 8;
var DEATH_TIME = 1.5;
// rev 9 C / 9b / 9c
var WAVE_X = 4;                                    // wave N uses the old curve's wave 4N
var FLEE_AT = 0.45, FLEE_TELE = 0.3, FLEE_RUN = 2.5, FLEE_REGEN = 3, FLEE_RATE = 0.15, FLEE_X = 3, FLEE_MAX = 2, FLEE_CAP = 0.85 * BOOST;   // flee state; burst speed capped so boost can still catch it
var SNIPER_DMG = 25, SNIPER_HOLD = 80, SNIPER_CHARGE = 1.2;
var ROLE_T = [                                     // index = ENEMY_ROLES index; hp/speed/turn/dmg come from the generator, fire = fireInt multiplier, mix = spawn weight
    { name: 'INTERCEPTOR', fire: 0.8, mix: 60 },
    { name: 'SPITTER', fire: 1.7, mix: 25 },
    { name: 'SNIPER', fire: 1, mix: 15 },
    { name: 'LANCER', fire: 1, mix: 14 },
    { name: 'BROOD', fire: 1.5, mix: 12 }
];
var SPIT_SPEED = 45;                               // spitter orb speed, L/s (dodgeable)
var SPAWN_NEAR = 150, SPAWN_FAR = 250, GRACE_T = 3, BREATHER_MIN = 8, BREATHER_VAR = 4, ARROW_MAX = 10;
var ROLL_T = 0.45, ROLL_CD = 1.5, ROLL_CUT = 0.4, ROLL_SIDE = 4, TAP_MS = 260;
var DRIFT_BOOST = 0.25;               // releasing a drift (>= 0.35 s) pays +25 % speed for 1 s
var DROP_MAX = 24, DROP_VAL = 12, DROP_MAGNET = 20, DROP_COLLECT = 6, CHAIN_T = 4;
var GRAZE_R = 1.5, GRAZE_FULL = 10, OD_T = 4, OD_FIRE = 1.5, OD_DMG = 1.25;
var FOCUS_MAX = 1.5, FOCUS_RECHARGE = 8, FOCUS_TS = 0.35, Z_FOCUS_T = 8;
var PERIL_DETECT = 2, PERIL_SAVE = 10, PERIL_R = 14;
var LOCK_RANGE = 300, FAR2 = 120 * 120;
// revision 3: allies, bosses, persistence
var ALLY_MAX = 3, ALLY_HP = 50, A_SPEED = 3, A_TURN = 2.2, A_DMG = 7 / 3;
var BOSS_SPEED = 0.25, TITAN_X = 1.5;   // rev 13: boss drift (u/s); titan length = TITAN_X x a planet's rendered radius
var BOSS_TTK = 90, BOSS_TTK_X = { 1: 0.6, 2: 1, 3: 1.6 };   // rev 20: boss hp = BOSS_TTK s x the player's current DPS x 0.8 x kind factor (1 mini, 2 giant, 3 titan) x difficulty hp
var LIMB_HP_F = 0.25, STAGGER_T = 1.5, EH_T = 0.6, EH_ALT = 6, CRATE_MAX = 4, CRATE_LIFE = 90, CRATE_R = 7, DN_MAX = 24;   // limb hp = 25 % of boss hp; sever stagger; hold-E seconds / altitude (L); weapon crates; damage-number pool
var ORB_MAX = 4, ORB_SPEED = 40, ORB_S = 2, ORB_DMG = 18, ORB_HP = 16;
var BAND_MAX = 90;                 // rev 12: boss holds its carapace 60-90 L from the player (follows when you leave, lets you approach)
var ATK = ['beamSweep', 'orbRing', 'ramCharge', 'mineField', 'gravityPull', 'gravityWell', 'planetRam', 'terrainBeam'];   // bossAttacksFor ids -> signature index
var ATK_NAME = ['BEAM SWEEP', 'ORB RING', 'RAM CHARGE', 'MINE FIELD', 'GRAVITY PULL', 'GRAVITY WELL', 'PLANET RAM', 'TERRAIN BEAM'];
var ATK_DMG_CAP = 60;
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

// squared distance between segments a-b and c-d (Ericson, Real-Time Collision Detection 5.1.9), plain numbers
function segSegDistSq(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz) {
    var ux = bx - ax, uy = by - ay, uz = bz - az, vx = dx - cx, vy = dy - cy, vz = dz - cz, wx = ax - cx, wy = ay - cy, wz = az - cz;
    var a = ux * ux + uy * uy + uz * uz, b = ux * vx + uy * vy + uz * vz, c = vx * vx + vy * vy + vz * vz, d = ux * wx + uy * wy + uz * wz, e = vx * wx + vy * wy + vz * wz;
    var D = a * c - b * b, sN, sD = D, tN, tD = D;
    if (D < 1e-12 * (a * c + 1e-30)) { sN = 0; sD = 1; tN = e; tD = c; }
    else { sN = b * e - c * d; tN = a * e - b * d; if (sN < 0) { sN = 0; tN = e; tD = c; } else if (sN > sD) { sN = sD; tN = e + b; tD = c; } }
    if (tN < 0) { tN = 0; if (-d < 0) sN = 0; else if (-d > a) sN = sD; else { sN = -d; sD = a; } }
    else if (tN > tD) { tN = tD; if ((-d + b) < 0) sN = 0; else if ((-d + b) > a) sN = sD; else { sN = -d + b; sD = a; } }
    var sc = Math.abs(sN) < 1e-12 ? 0 : sN / sD, tc = Math.abs(tN) < 1e-12 ? 0 : tN / tD;
    var rx = wx + sc * ux - tc * vx, ry = wy + sc * uy - tc * vy, rz = wz + sc * uz - tc * vz;
    return rx * rx + ry * ry + rz * rz;
}

// wave table: difficulty curve from the design doc (creature size, hp, speed, damage and turn come from generateEnemy now;
// this keeps the pacing: count, aim error, lead, cone, fire interval, dive speed, early-wave speed softening)
function waveParams(w) {
    if (w <= 1) return { n: 3, spd: 0.7, turn: 1.0, fireInt: 1.8, err: 0.16, lead: 0, cone: 0.28, regenDelay: 0, dive: 1.25 };
    if (w === 2) return { n: 6, spd: 0.9, turn: 2.2, fireInt: 0.9, err: 0.03, lead: 0.9, cone: 0.24, regenDelay: 1, dive: 1.6 };
    var n = Math.min(10, 4 + Math.floor((w - 2) / 2));
    return { n: n, spd: 1, turn: 1.6, fireInt: 1.3, err: 0.07, lead: 0.6, cone: 0.24, regenDelay: 4, dive: 1.6 };
}

export default function mount(engine) {
    var THREE = engine.THREE, scene = engine.scene, camera = engine.camera;
    var root = engine.root;

    // sizes derive from the live tree so a growing galaxy keeps the ship tiny. rev 13: L = refR / 2000 (planets read 2000 ship lengths in
    // radius). renderedRadius can differ before vs during pilot mode, so refreshScale() re-reads it on the first piloting frames and
    // rebuilds everything that baked L (fx, planet surfaces, ally scale, net hooks).
    // rev 20: L = refR_pilot / 12000 (was 5000), where refR_pilot = the root-level planet radius WITH the pilot scale (x3) applied (readRefR). The galaxy-scale
    // ship (docked / away) is sized from the unscaled radius (readRefRaw) because planets only grow while piloting.
    function readRefRaw() { var r = (root.kids && root.kids[0]) ? engine.renderedRadius(root.kids[0], true) : 0; return r > 0 && isFinite(r) ? r : 0; }
    function readRefR() { var k = root.kids && root.kids[0], r = readRefRaw(); if (!r) return 0; return r * (k && engine.pilotScale ? engine.pilotScale(k) : 1); }
    var refR0 = readRefRaw() || 100, refR = readRefR() || 300;
    var L = refR / 12000;
    // rev 20: the engine sizes every body's minimum pilot radius from the ship length
    function pushShipLength() { try { if (typeof engine.setShipLength === 'function') engine.setShipLength(L); } catch (e) { /* engine without the hook */ } }
    pushShipLength();
    var baseFov = camera.fov, baseNear = camera.near;
    // rev 20: audio (js/ship-audio.js, lazy; a silent stub until it loads or if it is missing). Unlocked by the first click / keydown.
    var audio = { unlock: function () { return false; }, play: function () {}, engine: function () {}, setMaster: function () {}, ready: false }, audioWanted = false, volume = 6;
    import('./ship-audio.js').then(function (m) {
        try { audio = m.createAudio(); duck(state !== 'piloting'); if (audioWanted) audio.unlock(); } catch (e) { console.info('[ship] audio unavailable', e); }
    }).catch(function (e) { console.info('[ship] ship-audio unavailable', e); });
    function aPlay(name, opts) { try { audio.play(name, opts); } catch (e) { /* ignore */ } }
    function aDist(p) { return p ? p.distanceTo(shipRoot.position) / L : 0; }     // ship lengths, for the audio gain falloff
    function aEngine(st) { try { audio.engine(st); } catch (e) { /* ignore */ } }
    function duck(on) { try { audio.setMaster(on ? 0 : volume / 10); } catch (e) { /* ignore */ } }      // rev 20: the engine drone has no stop, so the master is ducked to 0 whenever you are not piloting
    function unlockAudio() { audioWanted = true; try { if (audio.unlock() || audio.ready) { if (state !== 'piloting') duck(true); window.removeEventListener('pointerdown', unlockAudio, true); window.removeEventListener('keydown', unlockAudio, true); } } catch (e) { /* ignore */ } }
    window.addEventListener('pointerdown', unlockAudio, true);
    window.addEventListener('keydown', unlockAudio, true);
    // rev 20: procedural weapons (js/ship-weapons.js, lazy). STARTER mirrors its STARTER_WEAPON so firing works before the module arrives.
    var wpnMod = null;
    var STARTER = { id: 'starter-twin-laser', name: 'TWIN LASERS', cls: 'C', stats: { dmg: 8, rate: 6, spread: 0, count: 2, speed: 270, range: 120, special: null }, color: null, shape: 'bolt' };
    import('./ship-weapons.js').then(function (m) { wpnMod = m; }).catch(function (e) { console.info('[ship] ship-weapons unavailable', e); });
    var EDGE_R0 = BOUNDARY_F * root.sysR, EDGE_R = EDGE_R0;      // rev 20b: EDGE_R grows with the pilot layout (planets are 4-6x bigger and their orbits spread with them): see edgeFit()
    var galS = 0.9 * refR0;      // rev 8: galaxy-scale hull length of a remote ghost (0.9 x root-planet radius)
    var mineS = 1.35 * refR0;     // rev 12: YOUR docked ship = 1.35 x root-planet radius (half the rev 8 size)
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
    var fxCones = 0, exAll = [];
    // one exhaust set = a unit-scale holder (cones are authored in world units of L) following a hull's pose; cones pooled, never freed
    function makeEx(parent) { var h = new THREE.Group(), ex = { holder: h, cones: [], idx: [], th: [], col: -1 }; h.visible = false; parent.add(h); exAll.push(ex); return ex; }
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
        tintGhost(g, effColor());
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
            hullSigNow = (typeof m.hullSignature === 'function') ? m.hullSignature(hullOptsFor(curProfile())) : '';
            var h = buildHullKind(m, hullFor(curProfile())); if (h) setHull(h);
            hullLow = m.buildHull(THREE, hullOptsFor(curProfile(), 'low')); hullLow.traverse(function (o) { o.frustumCulled = false; }); shipRoot.add(hullLow);
            tintGhost(hullLow, effColor());
            syncLod();
            allyTpl = m.buildHull(THREE); tintAlly(allyTpl); if (net && net.setHullSig && hullSigNow) net.setHullSig(hullSigNow);
        } catch (e) { console.warn('[ship] ship-hull failed, keeping dart', e); }
    }).catch(function () { /* keep the fallback dart */ });
    scene.add(shipRoot);

    // ─── combat root: enemies + projectiles + debris ────────────────
    var combatRoot = new THREE.Group();
    combatRoot.name = 'ship-combat';
    combatRoot.visible = false;
    scene.add(combatRoot);

    // enemy creatures: ONE procedural generator (ship-enemies.js generateEnemy). Slots are pooled records; a live slot owns the creature it
    // was generated with (dispose()d on recycle). Any slot can be a regular enemy or a boss: kind 0 regular, 1 mini (tier 4), 2 giant (6), 3 titan (8).
    var enemies = [], E_EYE_MAX = 3;
    (function buildEnemies() {
        for (var i = 0; i <= ENEMY_MAX; i++) {
            var g = new THREE.Group(), body = new THREE.Group(), eyeW = [], q;
            g.add(body); g.visible = false; combatRoot.add(g);
            for (q = 0; q < E_EYE_MAX; q++) eyeW.push(new THREE.Vector3());
            enemies.push({
                g: g, body: body, cr: null, mat: null, eyeW: eyeW, eyeN: 0, sc: L, tier: 1, kind: 0, seed: 0, title: '',
                alive: false, hp: 0, maxHp: 0, state: 0, timer: 0, fireCd: 0,
                wander: new THREE.Vector3(), pass: new THREE.Vector3(), isBoss: false,
                len: 6 * L, R: 3.3 * L, phase: Math.random() * 6.283, hitT: 0, orbCd: 0, side: 1, flip: 5,
                speed: 0, turn: 1, dive: 1.6, near: 0, rpSet: false, off: new THREE.Vector3(), dmg: 3, fireInt: 1, err: 0.1, lead: 0, cone: 0.2, curSpeed: 0,
                // rev 9: role, flee, weave, sniper, boss phases
                role: 0, fstate: 'idle', ftimer: 0, fleeN: 0, flinch: 0, grace: 0, hunt: null, burst: 0, charge: 0, snCd: 0, idle: 0, far: 0,
                wAmp: 0, wPer: 2, wPh: 0, wSide: 1, slantT: 0, rnd: Math.random(), beam: null, aim: new THREE.Vector3(), vel: new THREE.Vector3(), prev: new THREE.Vector3(), first: true,
                bphase: 1, open: false, cyc: 0, wind: 0, volley: 0, volleyCd: 0, pflash: 0, bSpeed: 0, bDmg: 0,
                // rev 12: world-anchored path (leashed base + fixed-plane spiral), flee dive/skim, boss signature glow
                base: new THREE.Vector3(), hasBase: false, pu: new THREE.Vector3(1, 0, 0), pv: new THREE.Vector3(0, 0, 1), rpT: 0, fp: null, skim: 0, skimOn: false, fdive: 0, sigGlow: 0,
                // rev 13: lancer charge, boss attack list
                lphase: 0, lt: 0, atk: null, atkI: 0, sigCd: 0, nextAtk: -1,
                // rev 14: boss parking spot, gravity cycle, body plan label
                home: new THREE.Vector3(), gcyc: 0, plan: '',
                // rev 17 melee: state machine, current move, escorts, planet-throw cooldown
                ms: 'idle', mt: 0, mcd: 0, mi: 0, mv: null, mside: 1, hitDone: false, escT: 0, thrCd: 0,
                // rev 17 LOD: 0 hi, 1 lo (update every other frame), 2 impostor sprite
                lod: 0, lodT: 0, imp: null,
                // rev 20: limb hp, sever stagger, all-limbs-gone flag (core exposed at x4)
                limbHp: null, limbMax: 0, stagger: 0, noLimbs: false,
                // rev 21: squad / leader, attack-run sequence (ap 0 none, 1 wind-up, 2 strike), stall, lock-on jink, behaviour (hunter / harasser / bomber)
                squad: 0, leader: false, fOff: new THREE.Vector3(), beh: 'hunter', ap: 0, apT: 0, stallT: 0, jinkT: 0, jinkCd: 0, lockT: 0, jinkRoll: 0, jinkDir: new THREE.Vector3(),
                breakT: 0, breakCd: 0, bombed: false, elite: false, shielded: false, stalled: false, wv: 0, apDur: WINDUP_T, chargeSnd: false
            });
        }
    })();
    var spawnPad = 6 * L;

    // projectiles: records over fx.spawnBolt (js/ship-fx.js owns position + rendering); collision stays here
    var C_P = 0x00f0ff, C_E = 0xff2a1a, C_A = 0xb888ff, BURST_COL = [0x00f0ff, 0xff7a2a, 0xc8ff3a];
    var bolts = [], vBp = new THREE.Vector3(), vFb = new THREE.Vector3(), vFd = new THREE.Vector3();
    (function buildBolts() {
        for (var i = 0; i < BOLT_MAX; i++) bolts.push({ t0: 0, id: -1, prev: new THREE.Vector3(), active: false, enemy: false, dmg: 0, remote: false, owner: null, boss: false, gz: 0 });
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
    var psMod = null;
    // rev 17 C: adaptive quality (js/ship-perf.js). attach() again whenever fx / ps are recreated.
    var perfQ = null, perfMod = null;
    function perfAttach() { if (perfQ) { try { perfQ.attach({ fx: fx, ps: ps }); } catch (e) { /* ignore */ } } }
    import('./ship-perf.js').then(function (m) { perfMod = m; try { perfQ = m.createQualityManager(engine, { fx: fx, ps: ps }); } catch (e) { console.info('[ship] quality manager unavailable', e); } }).catch(function (e) { console.info('[ship] ship-perf unavailable', e); });
    function makePs() { try { ps = psMod.createPlanetSurface(engine, L); if (state === 'piloting') ps.setVisible(true); } catch (e) { planetFail(e); } perfAttach(); makeWorld(); }
    import('./ship-planet.js').then(function (m) { psMod = m; makePs(); }).catch(function (e) { console.info('[ship] planet surfaces unavailable', e); });

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
    var SCHEMA_V = 2;      // rev 25: v2 = gorCoin (profile.gor, migrated from units), stacks may carry a full item (rec.it), crew / pets / pens / missions
    var COLOR_NAMES = { orange: 0xFF7A1A, violet: 0x8a5cff, cyan: 0x00f0ff, red: 0xff3a2a, green: 0x3aff7a, white: 0xffffff };
    function pget(o, k, d) { var v = o ? o[k] : undefined; return (v === undefined || v === null) ? d : v; }   // accessor: never read raw
    function migrateProfile(p) {
        var o = (p && typeof p === 'object' && !Array.isArray(p)) ? p : {};
        var legacy = !(o.v >= 2);
        if (!(o.v >= 1)) o.v = SCHEMA_V;
        if (legacy) { o.gor = Math.max(0, (o.gor | 0) || (o.units | 0)); o.v = SCHEMA_V; }      // rev 25: units -> gorCoin (the old field stays untouched)
        o.gor = Math.max(0, o.gor | 0);
        if (typeof o.color !== 'number') o.color = null;
        if (typeof o.hull !== 'string') o.hull = 'hauler';
        if (!o.upgrades || typeof o.upgrades !== 'object') o.upgrades = {};
        if (!o.weapon || typeof o.weapon !== 'object' || !o.weapon.stats) o.weapon = null;           // rev 20: the equipped weapon (a generateWeapon() object) and every one owned
        if (!Array.isArray(o.weapons)) o.weapons = [];
        o.weapons = o.weapons.filter(function (w) { return w && typeof w === 'object' && w.stats && w.id; }).slice(-24);
        o.kills = Math.max(0, o.kills | 0); o.bestWave = Math.max(0, o.bestWave | 0); 
        if (!o.shards || typeof o.shards !== 'object' || Array.isArray(o.shards)) o.shards = { day: '', ids: [] };      // rev 22: taken shard ids, reset when the date key changes
        if (typeof o.shards.day !== 'string') o.shards.day = '';
        if (!Array.isArray(o.shards.ids)) o.shards.ids = [];
        o.shieldTier = clamp(o.shieldTier | 0, 0, 3); o.engineTier = clamp(o.engineTier | 0, 0, 3);
        if (!Array.isArray(o.items)) o.items = [];       // rev 23: inventory stacks {id, seed, n, d (dealer variant)}
        o.items = o.items.filter(function (r) { return r && typeof r === 'object' && typeof r.id === 'string' && r.n > 0 && (typeof r.seed === 'number' || (r.it && typeof r.it === 'object')); }).map(function (r) { var q = { id: r.id, seed: (r.seed >>> 0) || 0, n: Math.min(9999, r.n | 0), d: r.d ? 1 : 0 }; if (r.it && typeof r.it === 'object') q.it = r.it; return q; }).slice(-400);
        if (!Array.isArray(o.crew)) o.crew = [];          // rev 25: [{id,name,role,color,rank,lines[],busy}] cap 4
        o.crew = o.crew.filter(function (c) { return c && typeof c === 'object' && typeof c.name === 'string'; }).slice(0, 4);
        if (!Array.isArray(o.petPens)) o.petPens = [];    // rev 25: pet items living in the pens (index = pen)
        o.petPens = o.petPens.slice(0, 4);
        if (!Array.isArray(o.taken)) o.taken = [];        // rev 25: tamed creatures hidden on their planet [{n: planet id, x,y,z: planet-local}]
        o.taken = o.taken.filter(function (t) { return t && typeof t.n === 'string' && (typeof t.id === 'string' || isFinite(t.x + t.y + t.z)); }).slice(-40);      // rev 26: {n: planet id, id: ps creature id, k: kind}; old {x,y,z,c} entries are kept but no longer hide anything
        if (!o.suit || typeof o.suit !== 'object' || Array.isArray(o.suit)) o.suit = {};      // rev 27: on-foot suit tiers 0..3 (7/11 GEAR tab + crafted kits)
        ['jetpack', 'scanner', 'sprint', 'storage'].forEach(function (k) { o.suit[k] = clamp(o.suit[k] | 0, 0, 3); });
        if (!o.rep || typeof o.rep !== 'object' || Array.isArray(o.rep)) o.rep = {};      // rev 27: convoy reputation
        o.rep = { corp: isFinite(o.rep.corp) ? clamp(+o.rep.corp, -99, 99) : 0, dealers: isFinite(o.rep.dealers) ? clamp(+o.rep.dealers, -99, 99) : 0 };
        if (!Array.isArray(o.disc)) o.disc = [];      // rev 27: my discoveries [{k: kind, id, name, by}] (observation-room plaques)
        o.disc = o.disc.filter(function (d) { return d && typeof d.id === 'string' && typeof d.k === 'string'; }).slice(-120);
        if (!Array.isArray(o.hulls)) o.hulls = ['hauler'];      // rev 28: owned hull kinds (shipyard)
        o.hulls = o.hulls.filter(function (h, i, a) { return typeof h === 'string' && a.indexOf(h) === i; }).slice(0, 6);
        if (o.hulls.indexOf(o.hull) < 0) o.hulls.push(o.hull);
        if (!Array.isArray(o.mined)) o.mined = [];      // rev 28: mined rocks [[id, respawn epoch ms]]
        o.mined = o.mined.filter(function (m) { return Array.isArray(m) && isFinite(m[0]) && isFinite(m[1]); }).slice(-200);
        if (!o.moods || typeof o.moods !== 'object' || Array.isArray(o.moods)) o.moods = {};      // rev 28: npc id -> mood -2..2
        o.savedAt = isFinite(o.savedAt) ? +o.savedAt : 0;      // rev 27: last local change (relay profile sync picks the newer side)
        if (!o.board || typeof o.board !== 'object' || Array.isArray(o.board)) o.board = { seed: 0, day: '', list: [] };      // rev 25: mission board {seed, day, list[{id,name,kind,minutes,gor,crewId,start,end}]}
        if (!Array.isArray(o.board.list)) o.board.list = [];
        o.board.list = o.board.list.filter(function (m) { return m && typeof m === 'object' && typeof m.id === 'string'; }).slice(0, 6);
        if (!o.harv || typeof o.harv !== 'object' || Array.isArray(o.harv)) o.harv = {};      // rev 25: harvested resource nodes {id:{n,at}} (the world respawns them 20 min after `at`)
        o.visits = Math.max(0, o.visits | 0);             // Burger House visits (every 3rd offers a recruit)
        o.pens = Math.max(0, o.pens | 0); o.pods = Math.max(0, o.pods | 0);
        if (!Array.isArray(o.quests)) o.quests = [];     // rev 26: active quests (ship-quests.js objects + acceptedAt / expiresAt), max 3
        o.quests = o.quests.filter(function (q) { return q && typeof q === 'object' && typeof q.id === 'string' && Array.isArray(q.steps); }).slice(0, 3);
        if (!Array.isArray(o.qdone)) o.qdone = [];
        o.qdone = o.qdone.filter(function (x) { return typeof x === 'string'; }).slice(-80);
        if (!o.onb || typeof o.onb !== 'object' || Array.isArray(o.onb)) { var oldP = (o.created > 0) || (o.kills | 0) > 0 || (o.bestWave | 0) > 0; o.onb = { step: oldP ? 4 : 0, done: !!oldP, res: !!oldP, npc: !!oldP }; }      // rev 26: onboarding flags (only a brand-new profile sees the prompts)
        if (!Array.isArray(o.known)) o.known = [];       // rev 23: learned lingo words
        o.known = o.known.filter(function (w) { return typeof w === 'string'; }).slice(0, 400);
        o.glowUntil = (typeof o.glowUntil === 'number' && isFinite(o.glowUntil)) ? o.glowUntil : 0;     // rev 23: fries glow expiry (epoch ms)
        if (!(o.created > 0)) o.created = Date.now();
        return o;
    }
    function migrate(raw) {
        var o = (raw && typeof raw === 'object' && !Array.isArray(raw)) ? raw : {};
        if (!(o.v >= 1)) o.v = SCHEMA_V;
        o.wave = Math.max(1, o.wave | 0); o.kills = Math.max(0, o.kills | 0); o.bestWave = Math.max(0, o.bestWave | 0);
        o.difficulty = clamp(o.difficulty | 0, 1, 3) || 2; o.peaceful = !!o.peaceful;
        o.volume = typeof o.volume === 'number' && isFinite(o.volume) ? clamp(o.volume, 0, 10) : 6;
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
        if (pf) { pf.kills = Math.max(pget(pf, 'kills', 0) | 0, kills); pf.bestWave = Math.max(pget(pf, 'bestWave', 0) | 0, bestWave); r27Touch(pf); }
        saveData.wave = w; saveData.kills = kills; saveData.bestWave = bestWave; saveData.difficulty = difficulty; saveData.peaceful = peaceful; saveData.volume = volume; saveData.users = users; saveData.user = curUser;
        flushSave();
    }
    (function restore() {
        var sv = loadSave();
        difficulty = sv.difficulty; peaceful = sv.peaceful; volume = sv.volume; resumeWave = RESUME_WAVE ? sv.wave : 1; resumeKills = sv.kills; bestWave = Math.max(sv.bestWave, sv.wave); users = sv.users; curUser = sv.user;
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
    // rev 26: the hull follows the profile (tiers + weapon mods). ship-hull.js buildHull(THREE,{lod,kind,upgrades,mods}) / hullSignature(opts); every use is guarded so an older ship-hull still builds the plain hull.
    var hullSigNow = '', hullChk = 0;
    function hullOptsFor(pf, lod) {
        var u = (pf && pf.upgrades) || {}, wm = (pf && pf.weapon && pf.weapon.mods) || {}, o = {
            kind: hullFor(pf),
            upgrades: { engine: Math.max(pget(pf, 'engineTier', 0) | 0, u.engine | 0), shield: Math.max(pget(pf, 'shieldTier', 0) | 0, u.shield | 0), cargo: u.cargo | 0, jetpack: u.jetpack | 0 },
            mods: { scope: !!wm.scope, coil: !!wm.coil, chamber: !!wm.chamber }
        };
        if (lod) o.lod = lod;
        return o;
    }
    function optsFromSig(sig) {          // 'hauler|e1s2c0j0|SC' -> opts (ghosts)
        var m = /^(\w+)\|e(\d)s(\d)c(\d)j(\d)\|([SCH]*)$/.exec(String(sig || ''));
        if (!m) return null;
        return { kind: m[1], upgrades: { engine: +m[2], shield: +m[3], cargo: +m[4], jetpack: +m[5] }, mods: { scope: m[6].indexOf('S') >= 0, coil: m[6].indexOf('C') >= 0, chamber: m[6].indexOf('H') >= 0 } };
    }
    function buildHullKind(m, kind) { return m.buildHull(THREE, hullOptsFor(curProfile())); }
    function hullTick(dt) {              // rebuild the player hull when the profile's tiers / mods change; tell the relay so ghosts match
        hullChk -= dt; if (hullChk > 0) return; hullChk = 1;
        var m = hullMod; if (!m || typeof m.hullSignature !== 'function' || !hullLow) return;
        var pf = curProfile(), sig = '';
        try { sig = m.hullSignature(hullOptsFor(pf)); } catch (e0) { return; }
        if (sig === hullSigNow) return;
        hullSigNow = sig;
        try {
            setHull(buildHullKind(m, hullFor(pf)));
            var lo = m.buildHull(THREE, hullOptsFor(pf, 'low')); lo.traverse(function (o) { o.frustumCulled = false; });
            if (hullLow) shipRoot.remove(hullLow); hullLow = lo; shipRoot.add(lo); tintGhost(lo, effColor()); syncLod();
            if (net && net.setHullSig) net.setHullSig(sig);
        } catch (e1) { console.info('[ship] hull rebuild skipped', e1); }
    }
    function profileColor() { var c = pget(curProfile(), 'color', null); return typeof c === 'number' ? c : null; }
    // rev 13: every profile without a stored color gets a stable random saturated one seeded from its name hash (claude keeps orange; /color overrides)
    function hslHex(h, s, l) {
        function ch(n) { var k = (n + h * 12) % 12, a = s * Math.min(l, 1 - l); return Math.round(255 * (l - a * Math.max(-1, Math.min(k - 3, Math.min(9 - k, 1))))); }
        return (ch(0) << 16) | (ch(8) << 8) | ch(4);
    }
    function autoColor(name) {
        var h = 2166136261, s = String(name || '');
        for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
        h = Math.imul(h ^ (h >>> 15), 2246822519) >>> 0;
        h = Math.imul(h ^ (h >>> 13), 3266489917) >>> 0;
        return hslHex((h % 360) / 360, 0.92, 0.58);
    }
    function effColor() { var c = profileColor(); return c == null ? autoColor(userName()) : c; }
    function hexCss(c) { return '#' + ('000000' + (c >>> 0).toString(16)).slice(-6); }

    var dead = false, deathT = 0, waveMsgT = 0, curWave = waveParams(1);
    var bossWave = false, waveQuiet = false;
    var steamT = 0, steamN = 0, heat = 0, ohT = 0, lastFireGt = -9, ramT = 0, ramCd = 0, ramFx = 0, cHeat = -1, cHeatCls = 0;      // rev 21: weapon heat 0..1, overheat lockout, ram tool window / cooldown
var firing = false, fireCd = 0, fireSide = 0, playerFired = false;   // playerFired: allies hold fire until the player fires this wave
    var gameFresh = true;
    // rev 9 combat state (all plain numbers / pooled objects: nothing allocates per frame)
    var waveActive = false, waveStartT = 0, qAt = [], qRole = [], qTry = [], qTier = [], qSeed = [], qKind = [], qTitle = [], qSq = [], qLd = [], qEl = [], qN = 0, dirA = new THREE.Vector3(1, 0, 0), dirB = new THREE.Vector3(-1, 0, 0);
    var rollX = new THREE.Vector3(1, 0, 0), rollFx = 0, rollT = 0, rollCd = 0, rollDir = 1, tapT = { KeyA: 0, KeyD: 0, KeyS: 0 }, driftBoost = 0, pulseT = 0;
    // rev 13 maneuvers + terrain: flip (dbl-tap S), drift turn (hold Ctrl), terrain-hit invulnerability, scale re-read
    var flipT = 0, flipCd = 0, flipQ0 = new THREE.Quaternion(), driftOn = false, driftLeft = 0, driftCd = 0, terrInv = 0, scaleChecks = 3;
    var chainN = 0, chainT = 0, graze = 0, grazeIdle = 0, od = 0, focusE = FOCUS_MAX, focusing = false, zT = 0, zTarget = null;
    var nearPl = 0;      // rev 20: 0..1 proximity to a planet (2-4 R, fading out inside the atmosphere): chase cam +15 %, fov +4
    var fovKick = 0, hitStopN = 0, comboN = 0, comboT = 0, tickPunch = 0, ctarget = null, tlock = false, atT = 0, timeScale = 1;
    var ramIsHit = false, cRamHit = false, ramInv = 0, ramFlash = 0, bounceV = new THREE.Vector3(), cRam = false, cAtmo = -1;
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
        '<div class="sh-ret"><i></i><i></i><i></i><i></i></div><div class="sh-heat"></div><div class="sh-scan"></div><div class="sh-hls"></div><div class="sh-ems"></div><div class="sh-hm"><i></i><i></i></div><div class="sh-dmg"></div><div class="sh-eh"><b>E</b><span></span></div><div class="sh-eh sh-esc"><b>ESC</b><span>HOLD TO EXIT</span></div><div class="sh-guide"></div>' +
        '<div class="sh-roll"></div><div class="sh-flip"></div><div class="sh-drift"></div><div class="sh-chain"></div><div class="sh-pip"></div><div class="sh-lock"></div><div class="sh-tint"></div>' +
        '<div class="sh-arrows"></div>' +
        '<div class="sh-target"></div>' +
        '<div class="sh-edge"></div>' +
        '<div class="sh-bosses"></div><div class="sh-players"></div>' +
        '<div class="sh-wave"></div>' +
        '<div class="sh-hp"><span>SHIELD <em>100</em></span><b><u></u></b></div>' +
        '<div class="sh-graze"><span>GRAZE</span><b><u></u></b></div><div class="sh-focus"><span>FOCUS</span><b><u></u></b></div>' +
        '<div class="sh-atmo"><span>ATMOSPHERE</span><b><u></u></b></div><div class="sh-ram"></div>' +
        '<div class="sh-incoming"><i></i><span>PLANET INCOMING</span></div>' +
        '<div class="sh-entry">ENTRY</div><div class="sh-land"></div><div class="sh-alt"></div>' +
        '<div class="sh-stats"><div class="sh-st-wave"></div><div class="sh-st-en"></div><div class="sh-st-al"></div><div class="sh-st-kills"></div><div class="sh-st-u"></div></div>' +
        '<div class="sh-speed"></div>' +
        '<div class="sh-bars">' +
        '<div class="sh-bar sh-thr"><span>THR</span><b><u></u></b></div>' +
        '<div class="sh-bar sh-pul"><span>PULSE</span><b><u></u></b></div></div>' +
        '<div id="ship-chat"><div class="sc-log"></div></div>' +
        '<input id="ship-cmd" type="text" autocomplete="off" autocapitalize="off" spellcheck="false" maxlength="420" aria-label="Chat or command">' +
        '<div class="sh-hint">ESC exit · F land / board / talk · E inventory · LMB fire · SPACE pulse · SHIFT boost · SHIFT+LMB ram · V scan · A/D x2 roll · S x2 flip · CTRL drift · T target · Q focus · Z focus fire · ENTER chat · / commands</div>';
    document.body.appendChild(hud);
    var elAtmo = hud.querySelector('.sh-atmo'), elAtmoU = elAtmo.querySelector('u'), elRam = hud.querySelector('.sh-ram');
    var elTarget = hud.querySelector('.sh-target'), elSpeed = hud.querySelector('.sh-speed');
    var elHm = hud.querySelector('.sh-hm'), elDmg = hud.querySelector('.sh-dmg'), elEh = hud.querySelector('.sh-eh:not(.sh-esc)'), elEhT = elEh.querySelector('span');
    var elEdge = hud.querySelector('.sh-edge'), elWave = hud.querySelector('.sh-wave');
    var elHpU = hud.querySelector('.sh-hp u'), elHpN = hud.querySelector('.sh-hp em'), elHp = hud.querySelector('.sh-hp');
    var elStWave = hud.querySelector('.sh-st-wave'), elStEn = hud.querySelector('.sh-st-en'), elStKills = hud.querySelector('.sh-st-kills');
    var elStAl = hud.querySelector('.sh-st-al'), cAl = '', elStU = hud.querySelector('.sh-st-u'), cUnits = '';
    var bossLive = [], cBossPips = '', elBossPips = document.createElement('div');
    elBossPips.className = 'sh-bpips'; hud.querySelector('.sh-bosses').appendChild(elBossPips);
    var BOSS_ROWS = 4, bossRows = [];      // rev 13: one pooled HP bar per live boss, stacked (titan, giants, minis)
    (function buildBossRows() {
        var host = hud.querySelector('.sh-bosses');
        for (var i = 0; i < BOSS_ROWS; i++) {
            var el = document.createElement('div');
            el.className = 'sh-boss';
            el.innerHTML = '<span></span><b><u></u><s></s><s></s></b><em></em><div class="sh-lp"></div>';
            host.appendChild(el);
            var lpEl = el.querySelector('.sh-lp'), pips = [];
            for (var pi = 0; pi < 8; pi++) { var pe = document.createElement('i'); pe.style.display = 'none'; lpEl.appendChild(pe); pips.push(pe); }
            bossRows.push({ el: el, nameEl: el.querySelector('span'), u: el.querySelector('u'), em: el.querySelector('em'), on: false, n: '', p: -1, ph: '', open: false, k: 0, pips: pips, lk: '' });
        }
    })();
    var elThr = hud.querySelector('.sh-thr'), elThrU = elThr.querySelector('u');
    var elPulU = hud.querySelector('.sh-pul u');
    var elRet = hud.querySelector('.sh-ret'), cLocked = false;
    var elRoll = hud.querySelector('.sh-roll'), elChain = hud.querySelector('.sh-chain'), elPip = hud.querySelector('.sh-pip'), elLock = hud.querySelector('.sh-lock');
    var elGraze = hud.querySelector('.sh-graze'), elGrazeU = elGraze.querySelector('u'), elFocus = hud.querySelector('.sh-focus'), elFocusU = elFocus.querySelector('u'), elTint = hud.querySelector('.sh-tint');
    var elIncoming = hud.querySelector('.sh-incoming'), elIncomingI = elIncoming.querySelector('i');
    var elArrows = hud.querySelector('.sh-arrows'), arrows = [], elFlip = hud.querySelector('.sh-flip'), elDrift = hud.querySelector('.sh-drift'), elPlayers = hud.querySelector('.sh-players');
    var EM_MAX = 8, emPool = [], elHeat = hud.querySelector('.sh-heat');
    (function buildEm() {
        var host = hud.querySelector('.sh-ems');
        for (var i = 0; i < EM_MAX; i++) { var el = document.createElement('div'); el.className = 'sh-em'; el.style.display = 'none'; el.innerHTML = '<i></i><b></b>'; host.appendChild(el); emPool.push({ el: el, b: el.lastChild, on: false, txt: '', x: -1e9, y: -1e9 }); }
    })();
    var cRoll = -1, cFlip = -1, cDrift = -1, cChain = '', cGraze = -1, cFocus = -1, cOd = false, cFocusOn = false, cPip = '', cPipSolid = false, cLockTxt = '', cTickK = -1;
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
        else if (target) ts = (target.node.title || target.node.id || '').toUpperCase() + '  ·  ' + fmtDist(target.dist) + '';
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
        var rowN = 0, kk, bi;                                    // stacked boss bars: titan, then giants, then minis
        bossLive.length = 0;
        for (bi = 0; bi < enemies.length; bi++) { var be = enemies[bi]; if (be.alive && be.kind >= 1) { be.bd = be.g.position.distanceToSquared(shipRoot.position); bossLive.push(be); } }
        bossLive.sort(function (a, b) { return a.bd - b.bd; });          // rev 25: only the 2 nearest bosses get a bar, the rest are pips
        for (bi = 0; bi < bossLive.length && rowN < 2; bi++) setBossRow(bossRows[rowN++], bossLive[bi]);
        var pk = '';
        for (bi = 2; bi < bossLive.length; bi++) pk += '◆ ' + String(bossLive[bi].title).slice(0, 14) + ' ' + Math.round(bossLive[bi].hp / bossLive[bi].maxHp * 100) + '%   ';
        if (pk !== cBossPips) { cBossPips = pk; elBossPips.textContent = pk; }
        for (; rowN < BOSS_ROWS; rowN++) if (bossRows[rowN].on) { bossRows[rowN].on = false; bossRows[rowN].el.classList.remove('is-on'); }
        var ks = 'KILLS ' + kills;
        if (ks !== cKills) { cKills = ks; elStKills.textContent = ks; }
        var us = CRF.fmt(unitsNow());
        if (us !== cUnits) { cUnits = us; elStU.textContent = us; }
        if (edgeNow !== cEdge) { cEdge = edgeNow; elEdge.textContent = ''; elEdge.classList.toggle('is-on', edgeNow); }
        var show = waveMsgT > 0;
        if (show !== cWaveShow) { cWaveShow = show; elWave.classList.toggle('is-show', show); }
        if (dead !== cDead) { cDead = dead; hud.classList.toggle('is-dead', dead); }
        var rf = ramFlash > 0;
        if (rf !== cRam) { cRam = rf; elRam.classList.toggle('is-on', rf); }
        if (ramIsHit !== cRamHit) { cRamHit = ramIsHit; elRam.classList.toggle('is-hit', ramIsHit); }
        var am = 0;
        if (ps) { try { am = (ps.active && ps.depth > 0.01) ? Math.round(clamp(ps.depth, 0, 1) * 50) / 50 : 0; } catch (e) { planetFail(e); } }
        if (am !== cAtmo) { cAtmo = am; elAtmoU.style.transform = 'scaleX(' + am + ')'; elAtmo.classList.toggle('is-on', am > 0); }
    }
    function setBossRow(r, e) {
        if (!r.on) { r.on = true; r.el.classList.add('is-on'); }
        if (r.k !== e.kind) { r.k = e.kind; r.open = false; r.el.className = 'sh-boss is-on sh-k' + e.kind; }
        var bt = e.plan ? e.title + ' · ' + e.plan.toUpperCase() : e.title;
        if (r.n !== bt) { r.n = bt; r.nameEl.textContent = bt; }
        var bp = Math.max(0, Math.round(e.hp / e.maxHp * 200));
        if (bp !== r.p) { r.p = bp; r.u.style.transform = 'scaleX(' + (bp / 200) + ')'; }
        var ph = (e.ms === 'tele' && e.mv) ? e.mv.name.toUpperCase() + ' · INCOMING' : (e.ms === 'throw' ? 'PLANET THROW · INCOMING' : 'PHASE ' + e.bphase + (e.open ? ' · CORE EXPOSED' : ''));
        if (ph !== r.ph) { r.ph = ph; r.em.textContent = ph; }
        if (e.open !== r.open) { r.open = e.open; r.el.classList.toggle('is-open', e.open); }
        // rev 21: one pip per limb with the attack it owns; dead limbs are struck through, the limb about to strike blinks (telegraph)
        var lims = e.cr && e.cr.limbs, lk = '', li, hot = (e.ms === 'tele' || e.ms === 'strike') && e.mv && e.mv.limb >= 0 ? e.mv.limb : -9;
        if (lims) for (li = 0; li < lims.length && li < 8; li++) lk += (lims[li].dead ? 'x' : (lims[li].id === hot ? 't' : 'a')) + String(lims[li].attackId || '').slice(0, 3) + ',';
        if (lk !== r.lk) {
            r.lk = lk;
            for (li = 0; li < 8; li++) {
                var pe = r.pips[li], lm = lims && li < lims.length ? lims[li] : null;
                if (!lm) { pe.style.display = 'none'; continue; }
                pe.style.display = ''; pe.textContent = String(lm.attackId || '').slice(0, 3).toUpperCase() || '--';
                pe.className = lm.dead ? 'is-dead' : (lm.id === hot ? 'is-tele' : '');
            }
        }
    }
    function announce() { /* rev 20b: status titles are gone from the HUD (LEAVING ATMOSPHERE, WAVE n, EQUIPPED, KILL, USER, OVERDRIVE ...); only boss names are shown: announceBoss */ }
    function announceBoss(msg) {
        waveMsgT = 1.5;
        if (msg !== cWaveMsg) { cWaveMsg = msg; elWave.textContent = msg; }
    }

    // ─── bodies (real nodes only) ────────────────────────────────────
    var scratchBodies = [];
    function realRadius(n) {
        var r = engine.renderedRadius(n);
        return r > 0 && isFinite(r) ? r : (n.bodyR || L);
    }
    // rev 20b: the soft boundary must enclose the whole pilot-scale layout (with the old fixed 3.5 sysR edge the outer worlds sat OUTSIDE it, and the
    // boundary shoved the ship toward the origin by ~8 u/frame while it hovered over them). Edge = 1.25 x the farthest body's far limb, never below the old edge.
    function edgeFit(bs) {
        var far = 0, i;
        for (i = 0; i < bs.length; i++) { var a = bs[i].node.anchor.position, d = Math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z) + 2 * bs[i].R; if (d > far) far = d; }
        EDGE_R = Math.max(EDGE_R0, 1.25 * far);
        if (space) for (i = 0; i < space.belts.length; i++) EDGE_R = Math.max(EDGE_R, 1.15 * space.belts[i].outer);      // rev 22: the belts are inside the boundary
    }
    // Reuses record objects so there is no per-frame garbage.
    var realRecs = new Map();
    function gatherBodies() {
        var out = scratchBodies, c = 0, i, d = engine.drawOrder;
        for (i = 0; i < d.length; i++) {
            var n = d[i];
            if (!n.anchor) continue;
            var rec = realRecs.get(n);
            if (!rec) { rec = { node: n, R: 0, dist: 0, px: 0, py: 0, pz: 0, pN: -9, vx: 0, vy: 0, vz: 0 }; realRecs.set(n, rec); }
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
        if (state !== 'piloting' || e.button !== 0 || cmdOpen || gmode !== 'fly') return;
        if ((e.shiftKey || keys.ShiftLeft || keys.ShiftRight) && boostNow && !dead) { tryRam(); return; }      // rev 21: Shift+LMB while boosting = RAM (never fires the lasers)
        firing = true;
    }, true);
    document.addEventListener('mouseup', function (e) { if (e.button === 0) firing = false; }, true);
    window.addEventListener('contextmenu', function (e) { if (state === 'piloting') e.preventDefault(); }, true);   // Ctrl+click would open the menu while drifting
    window.addEventListener('keydown', function (e) {
        if (state === 'piloting' && menuOpen) { menuKey(e); return; }
        if (state === 'piloting' && storeOpen) { storeKey(e); return; }
        if (state === 'piloting' && invOpen) { invKey(e); return; }
        if (state !== 'piloting' || cmdOpen) return;
        if (e.key === '/') { e.preventDefault(); openCmd('/'); return; }
        if (e.code === 'KeyT' && !e.ctrlKey && !e.metaKey && !e.altKey) { e.preventDefault(); if (!e.repeat && gmode === 'fly') cycleTarget(); return; }      // rev 9b: T cycles targets; chat is Enter or /
        if (e.key === 'Enter' && !e.ctrlKey && !e.metaKey && !e.altKey) { e.preventDefault(); openCmd(''); return; }
        if (e.code === 'Escape') {
            // engine's Esc handler may have just called enter() for this same keydown,
            // and it can also run after us: ignore early Escapes and mark handled ones
            if (e.defaultPrevented || e.__shipHandled || performance.now() - enteredAt < 150) return;
            e.__shipHandled = true;
            if (!e.repeat) escStart();       // rev 24: hold 3 s to exit; a tap does nothing
            return;
        }
        if (e.code === 'Space' || e.code === 'Tab' || e.code.indexOf('Arrow') === 0) e.preventDefault();
        if (e.repeat) { keys[e.code] = true; return; }
        keys[e.code] = true;
        if (boarding || exiting) return;
        if (e.code === 'KeyE' && !e.ctrlKey && !e.metaKey && !e.altKey) { toggleInv(); return; }       // rev 23: E = inventory (grid overlay)
        if (e.code === 'KeyF' && !e.ctrlKey && !e.metaKey && !e.altKey) { if (gmode === 'landed') onKeyF(); else { eh.on = true; eh.held = 0; eh.t = 0; } return; }       // rev 20: tap = land / exit ship / board (fires on release), hold 0.6 s under 6 L = auto-land + step out, hold on foot = board + lift off
        if (e.code === 'KeyW' && gmode === 'landed') { liftOff(); return; }
        if (e.code === 'KeyV' && !e.ctrlKey && !e.metaKey && !e.altKey) { doScan(); return; }        // rev 21: scanner pulse
        if (gmode !== 'fly') return;
        if (e.code === 'KeyA' || e.code === 'KeyD' || e.code === 'KeyS') tapKey(e.code);
        else if (e.code === 'KeyZ') zFocusFire();
    }, true);
    window.addEventListener('keyup', function (e) {
        keys[e.code] = false;
        if (e.code === 'KeyF' && eh.on) { eh.on = false; eh.t = 0; ehShow(0, ''); if (eh.held < EH_T && state === 'piloting' && !boarding && !exiting && !cmdOpen) onKeyF(); }
    });
    window.addEventListener('blur', function () { keys = Object.create(null); firing = false; eh.on = false; eh.t = 0; });
    document.addEventListener('pointerlockchange', function () {
        if (state === 'piloting' && !locked() && !cmdOpen && performance.now() > cmdGuardUntil && !boarding && !exiting) { if (!escLatch) { esc.on = true; esc.t = 0; } }      // rev 24: the Esc that released the lock starts the 3 s hold; a tap (keyup) cancels
        else if (locked()) escCancel();
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

    // rev 13: the docked / away ship lives in SCREEN space. Each frame it is placed so it projects to a fixed anchor (14 % from the left, 78 % from
    // the top), on the camera ray at 0.9 x the distance to the orbital plane (just in front of it), sized to a constant fraction of the view height so
    // it reads instantly at any zoom. If a body's projected disc would overlap it, the target slides along the screen to the nearest clear spot.
    // It bobs gently and never leaves the anchor region. (placeAtDock above is only the respawn pose after a death.)
    var DOCK_NX = -0.72, DOCK_NY = -0.56, DOCK_LEN = 0.1;     // NDC anchor; hull length as a fraction of the view height
    var dk = { nx: DOCK_NX, ny: DOCK_NY, glide: false, tx: 0, ty: 0, vis: true, follow: true, still: 0, inT: -1e9, down: false, chk: 0, recov: false, rt: 0, rS0: 1, rS: 1, wp: new THREE.Vector3(), tp: new THREE.Vector3(), rp0: new THREE.Vector3(), q: new THREE.Quaternion(), cp: new THREE.Vector3(), cq: new THREE.Quaternion() };
    var DOCK_OFF = [];
    (function () { for (var r = 1; r <= 14; r++) for (var a = 0; a < 12; a++) DOCK_OFF.push([Math.cos(a / 12 * 6.2832) * r * 0.045, Math.sin(a / 12 * 6.2832) * r * 0.045]); })();
    function dockClear(nx, ny, rs, bodies, tanH, asp) {            // screen-space test in view-height units (x scaled by aspect)
        if (Math.abs(nx) > 0.92 || Math.abs(ny) > 0.9) return false;
        for (var i = 0; i < bodies.length; i++) {
            var b = bodies[i];
            vB.copy(b.node.anchor.position).applyMatrix4(camera.matrixWorldInverse);
            var zb = -vB.z;
            if (zb < 1e-3) continue;
            var bx = vB.x / (zb * tanH * asp) * asp - nx * asp, by = vB.y / (zb * tanH) - ny, br = b.R / (zb * tanH) * (b.node === root ? 1.3 : 1.12) + rs;
            if (bx * bx + by * by < br * br) return false;
        }
        return true;
    }
    // rev 17: WORLD anchor. The screen anchor (14 % / 78 %) is solved into a world position only when a route/focus changes, on first load, after the
    // exit cinematic or on a window resize ("follow" mode: screen-anchored while the camera moves); it latches the moment the viewer touches the galaxy
    // or the camera sits still for 0.5 s. Latched, the ship is fixed in world space and orbit drags carry it with the planets. If the galaxy has been
    // idle 0.5 s and the ship is fully off screen (or absurdly big / small after a zoom) it glides 1 s to a freshly solved spot.
    function anchorTarget(dt, snap) {                              // fills dk.tp (world position), dk.q (3/4 orientation) and returns the hull scale
        var tanH = Math.tan(camera.fov * Math.PI / 360), asp = camera.aspect || (window.innerWidth / window.innerHeight), i;
        camera.updateMatrixWorld();
        var bodies = gatherBodies(), rs = DOCK_LEN * 1.15;          // ship disc radius in view-height units (hull length = 2 x DOCK_LEN of the half-height)
        var tx = DOCK_NX, ty = DOCK_NY;
        if (!dockClear(tx, ty, rs, bodies, tanH, asp)) {
            for (i = 0; i < DOCK_OFF.length; i++) if (dockClear(DOCK_NX + DOCK_OFF[i][0] / asp, DOCK_NY + DOCK_OFF[i][1], rs, bodies, tanH, asp)) { tx = DOCK_NX + DOCK_OFF[i][0] / asp; ty = DOCK_NY + DOCK_OFF[i][1]; break; }
        }
        var k = snap ? 1 : damp(4, Math.max(dt, 1e-3));
        dk.nx += (tx - dk.nx) * k; dk.ny += (ty - dk.ny) * k;
        vA.set(dk.nx, dk.ny, 0.5).unproject(camera).sub(camera.position).normalize();
        vU.set(0, 0, -1).applyQuaternion(camera.quaternion);          // camera forward
        var cl = camera.position.length(), t = vA.y < -0.01 ? -camera.position.y / vA.y : -1;
        var dd = clamp((t > 0 ? t : cl) * 0.9, 0.05 * cl, 3 * cl), zv = dd * vA.dot(vU);
        var S = Math.max(1e-3, DOCK_LEN * 2 * tanH * zv);
        dk.tp.copy(camera.position).addScaledVector(vA, dd);
        vD.set(0.85, 0.22, -0.45).applyQuaternion(camera.quaternion);   // 3/4 view: nose toward screen-right and slightly away
        mM.lookAt(ORIGIN, vD, Y);
        dk.q.setFromRotationMatrix(mM);
        return S;
    }
    function dkLatch() { dk.follow = false; dk.recov = false; dk.wp.copy(shipRoot.position); dk.still = 0; }
    function dkTouch(e) {
        if (state === 'piloting') return;
        if (e.type === 'pointerdown') { if (e.target !== engine.renderer.domElement) return; dk.down = true; }
        dk.inT = performance.now();
        if (dk.follow && !dk.glide) dkLatch();
    }
    window.addEventListener('pointerdown', dkTouch, true);
    window.addEventListener('wheel', dkTouch, { capture: true, passive: true });
    window.addEventListener('pointermove', function (e) { if (dk.down) dkTouch(e); }, true);
    window.addEventListener('pointerup', function () { dk.down = false; dk.inT = performance.now(); }, true);
    window.addEventListener('pointercancel', function () { dk.down = false; dk.inT = performance.now(); }, true);
    window.addEventListener('hashchange', function () { dk.follow = true; dk.still = 0; dk.recov = false; });
    window.addEventListener('resize', function () { dk.follow = true; dk.still = 0; dk.recov = false; });
    function placeAtAnchor(dt, snap) {
        var S, now = performance.now(), asp, tanH;
        if (dk.follow || snap || dk.glide) {
            S = anchorTarget(dt, snap);
            if (dk.glide && !snap) {
                shipRoot.position.lerp(dk.tp, damp(2.5, dt));
                setShipScale(logLerp(shipScale, S, damp(2.5, dt)));
                if (shipRoot.position.distanceTo(dk.tp) < S * 0.6) { dk.glide = false; dk.follow = true; dk.still = 0; }
            } else { shipRoot.position.copy(dk.tp); setShipScale(S); }
            mineS = S;
            if (!snap && !dk.glide) {                                  // settle detector: camera still for 0.5 s -> latch
                if (camera.position.distanceToSquared(dk.cp) < 1e-14 * (1 + camera.position.lengthSq()) && Math.abs(camera.quaternion.dot(dk.cq)) > 0.9999999) dk.still += dt; else dk.still = 0;
                if (dk.still > 0.5) {                                  // never latch during the intro / a focus transition / before the galaxy is ready
                    var gs = window.EMGOR_GALAXY && window.EMGOR_GALAXY.state;
                    if (document.body.classList.contains('galaxy-ready') && gs && !gs.intro && !gs.trans && S > 0.01 && isFinite(S)) dkLatch(); else dk.still = 0;
                }
            }
            dk.cp.copy(camera.position); dk.cq.copy(camera.quaternion);
            shipRoot.quaternion.copy(dk.q);
        } else if (dk.recov) {                                         // 1 s ease to a freshly solved world spot
            var u = clamp((now - dk.rt) / 1000, 0, 1), e = easeInOut(u);
            shipRoot.position.lerpVectors(dk.rp0, dk.tp, e);
            setShipScale(logLerp(dk.rS0, dk.rS, e));
            mineS = shipScale;
            if (u >= 1) { dk.recov = false; dk.wp.copy(dk.tp); mineS = dk.rS; }
            shipRoot.quaternion.copy(dk.q);
        } else {
            shipRoot.position.copy(dk.wp); shipRoot.quaternion.copy(dk.q);
            shipRoot.position.y += 0.02 * shipScale * Math.sin(clockT * 0.9);
            if (shipScale !== mineS) setShipScale(mineS);
            if (!dk.down && now - dk.inT > 500 && now - dk.chk > 250 && state !== 'piloting') {
                dk.chk = now;
                camera.updateMatrixWorld();
                asp = camera.aspect || (window.innerWidth / window.innerHeight); tanH = Math.tan(camera.fov * Math.PI / 360);
                vA.copy(shipRoot.position).project(camera);
                vB.copy(shipRoot.position).applyMatrix4(camera.matrixWorldInverse);
                var zv2 = -vB.z, m = zv2 > 1e-6 ? shipScale / (tanH * zv2) : 0;
                var off = vA.z > 1 || vA.z < -1 || Math.abs(vA.x) > 1 + 0.12 / asp || Math.abs(vA.y) > 1.12 || m < 0.05 || m > 0.65;
                if (off) {
                    dk.rS0 = shipScale; dk.rp0.copy(shipRoot.position); dk.rt = now;
                    dk.rS = anchorTarget(dt, true);
                    dk.recov = true;
                }
            }
        }
        // 3/4 view held in world space; slow sway on top
        shipRoot.quaternion.premultiply(qA.setFromAxisAngle(Y, Math.sin(clockT * 0.25) * 0.4));
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
            if (engine.setPilotBlend) engine.setPilotBlend(1 - e);
            cinePose(e);
            setHudOpacity(1 - clamp(exitT * BOARD_DUR / 0.4, 0, 1));
            syncLod();
        } else {
            boardT = Math.min(1, boardT + dt / BOARD_DUR); e = easeInOut(boardT);
            setShipScale(boardT >= 1 ? L : logLerp(cineS0, L, e));
            if (engine.setPilotBlend) engine.setPilotBlend(e);
            cinePose(e);
            setHudOpacity(clamp((boardT * BOARD_DUR - (BOARD_DUR - 0.4)) / 0.4, 0, 1));
            syncLod();
        }
        camera.updateMatrixWorld(true);
        exMe.holder.visible = shipScale <= 1.5 * L && !dead;
        if (exMe.holder.visible) exUpdate(exMe, shipRoot.position, shipRoot.quaternion, shipScale, 0.3);
        fx.setMotion(vel, 0, false); fx.update(dt, camera);
        if (!exiting) aEngine({ throttle: 0.15 });
        if (net) { net.sendPos(); net.update(dt, dockA); ghostFx(); }
        if (exiting && exitT >= 1) finishExit();
        else if (!exiting && boardT >= 1) {
            boarding = false; setHudOpacity(1); mdx = mdy = 0; fov = camera.fov;
            chaseTargets(0); camera.position.copy(camPos); camera.quaternion.copy(camQuat); camera.updateMatrixWorld(true);   // land EXACTLY on the chase pose
            if (station) { try { station.rescale(); } catch (e) { /* ignore */ } }
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
            var flying = g.s === 'piloting' && g.mode !== 'landed' && g.mode !== 'foot' && g.samples.length > 0 && g.hull.visible && g.root.scale.x < 20 * L;
            ex.holder.visible = flying;
            if (!flying) return;
            var sv = g.samples[g.samples.length - 1].v || 0;
            exUpdate(ex, g.root.position, g.root.quaternion, g.root.scale.x, (g.st & 2) ? 2 : ((g.st & 1) ? 1 : Math.min(0.7, Math.max(0, sv) / CRUISE * 0.7)));
        });
    }
    function enter() {
        if (state === 'piloting') return;
        stepN += 2;                                        // drop stale body-frame history
        lfOff();
        scaleChecks = 3;                                   // re-read the planet radius on the first piloting frames (rev 13)
        enteredAt = performance.now();                     // engine Esc may call us while already flying
        try {
            var p = document.body.requestPointerLock();
            if (p && p.catch) p.catch(function () { console.log('[ship] pointer lock refused; keyboard only'); });
        } catch (err) { console.log('[ship] pointer lock unavailable; keyboard only'); }
        var rs = resumeSnap, inFrame = false; resumeSnap = null;
        if (state === 'docked') {
            if (!rs) pushOutOfBodies(shipRoot.position, gatherBodies(), 2.2);      // fly from where the ship sits on screen (seamless boarding), never inside a body
            resetGame(resumeWave, resumeKills);
        }
        vel.set(0, 0, 0); speed = 0; throttle = 0; pulse = 0; pulseT = 0;
        mdx = mdy = 0; firing = false;
        keys = Object.create(null);
        if (rs && rs.f === 'world') { try { inFrame = applyResume(rs); } catch (err) { console.info('[ship] resume', err); inFrame = false; } }
        else if (rs) { resumePend = rs; resumeWait = 3; inFrame = true; }      // planet / station frames: the bodies only take their piloting-scale pose after the first engine frames, so the pose is applied then
        // boarding cinematic: from wherever the camera is right now to the chase pose, hull log-lerping galaxy scale -> true L
        cineS0 = shipScale; cineQ0.copy(camera.quaternion);
        cineR0.copy(camera.position).sub(shipRoot.position);
        chaseTargets(0);
        cineR1.copy(camPos).sub(shipRoot.position); cineQ1.copy(camQuat);
        boardT = 0; boarding = true; exiting = false;
        setHudOpacity(0);
        if (inFrame) {                                    // rev 24: resuming inside a planet / station frame: no cinematic, a fade from black
            setShipScale(L); boardT = 1; boarding = false; setHudOpacity(1); mdx = mdy = 0; fov = camera.fov;
            chaseTargets(0); camera.position.copy(camPos); camera.quaternion.copy(camQuat);
            fadeIn();
        }
        state = 'piloting';
        syncLod();
        if (ps) { try { ps.setVisible(true); } catch (e) { planetFail(e); } }
        if (station) station.setVisible(true);
        if (space) space.group.visible = true;
        applyUpgrades(); bhInv = 4;      // the dock ring sits inside the pilot-scale black hole sphere: the first push-out is free
        net && net.setState('piloting');
        shipRoot.visible = !dead;
        combatRoot.visible = true; fx.bolts.visible = true;
        document.body.classList.add('is-piloting');
        hud.classList.add('is-on');
        label.style.display = 'none'; labelShown = false;
        if (engine.setPilotBlend) engine.setPilotBlend(inFrame ? 1 : 0);
        duck(false);
        engine.setPilot(step);
        engine.onEscape(escEnter);
    }

    // ─── rev 24: Esc resumes exactly. exit() freezes the pose in its current frame; enter() restores it relative to the moved planet / station ───
    var resumePend = null, resumeWait = 0, resumeAlt = -1;
    function resumeTick() {
        if (--resumeWait > 0) return;
        var rs = resumePend, ok = false; resumePend = null;
        try { ok = applyResume(rs); } catch (err) { console.info('[ship] resume', err); }
        if (!ok) pushOutOfBodies(shipRoot.position, gatherBodies(), 2.2);
        lfOff(); if (ok && rs.f === 'planet' && rs.mode === 'fly') lf.relNext = true;
        mdx = mdy = 0; fov = camera.fov; chaseTargets(0); camera.position.copy(camPos); camera.quaternion.copy(camQuat);
    }
    var resumeSnap = null, fadeEl = document.createElement('div');
    fadeEl.className = 'sh-fade'; document.body.appendChild(fadeEl);
    function fadeIn() { fadeEl.style.transition = 'none'; fadeEl.style.opacity = '1'; void fadeEl.offsetWidth; fadeEl.style.transition = 'opacity 0.7s ease'; fadeEl.style.opacity = '0'; }
    function takeSnap() {
        resumeSnap = null;
        if (dead) return;
        var P = shipRoot.position;
        if (gmode === 'sfoot' && station && sk.mode === 'deck') {
            resumeSnap = { f: 'station', pad: sk.pad, hp: sk.hp.clone(), yaw: sk.yaw, pitch: sk.pitch };
        } else if ((gmode === 'landed' || gmode === 'foot' || gmode === 'landing') && land.node) {
            var sp = gmode === 'landing' ? land.gDir.clone().multiplyScalar(land.rg) : land.sPos.clone();
            resumeSnap = { f: 'planet', node: land.node, mode: gmode === 'foot' ? 'foot' : 'landed', sPos: sp, sQuat: (gmode === 'landing' ? land.qTo : land.sQuat).clone(), rg: land.rg };
            if (gmode === 'foot' && hum.obj) resumeSnap.hum = { pos: hum.pos.clone(), hr: hum.hr, gr: hum.gr, hf: hum.hf.clone(), face: hum.face.clone(), pitch: hum.pitch };
        } else if (gmode === 'fly' && lf.on && lf.node) {
            resumeSnap = { f: 'planet', node: lf.node, mode: 'fly', lp: lf.lp.clone(), lq: lf.lq.clone(), R: lf.R, alt: lf.alt };
        } else if (gmode === 'fly') {
            resumeSnap = { f: 'world', pos: P.clone(), quat: shipRoot.quaternion.clone() };
        }
    }
    function applyResume(rs) {
        var P = shipRoot.position, n, c, q;
        if (rs.f === 'world') { P.copy(rs.pos); shipRoot.quaternion.copy(rs.quat); gmode = 'fly'; return false; }
        if (rs.f === 'station') {
            if (!station || !station.pads[rs.pad]) return false;
            gmode = 'sfoot'; sk.mode = 'deck'; sk.pad = rs.pad;
            var pad = station.pads[rs.pad]; pad.free = false;
            sk.shipL.copy(pad.local);
            station.group.updateMatrixWorld(true);
            station.toWorld(sk.shipL, P); shipRoot.quaternion.copy(station.quat);
            exOff(exMe); hullY(0);
            if (!hum.obj) { hum.obj = makeHuman(effColor()); scene.add(hum.obj.group); } else hum.obj.setColor(effColor());
            hum.obj.group.visible = true;
            sk.hp.copy(rs.hp); sk.yaw = rs.yaw; sk.pitch = rs.pitch; sk.vv = 0; sk.air = false; sk.jumpHeld = true; sk.moving = sk.running = false; sk.nl = 0.4;
            camRelInit = false; combatRoot.visible = false;
            setGround(true, true); setPrompt('', false);
            net && net.setMode && net.setMode('landed');
            return true;
        }
        n = rs.node;
        if (!n || !n.anchor || !n.mesh || !(n.mesh.scale.x > 0)) return false;
        syncPlanet(n); c = n.anchor.position; q = n.mesh.quaternion;
        if (rs.mode === 'fly') {
            P.copy(rs.lp).multiplyScalar(rs.R > 0 ? n.mesh.scale.x / rs.R : 1).applyQuaternion(q).add(c); shipRoot.quaternion.copy(q).multiply(rs.lq);
            resumeAlt = rs.alt > 0 && isFinite(rs.alt) ? rs.alt : -1;
            gmode = 'fly'; lf.relNext = true; lf.fromLand = false; landCheckT = 0.6;
            return true;
        }
        land.node = n; land.sPos.copy(rs.sPos); land.sQuat.copy(rs.sQuat); land.rg = rs.rg || rs.sPos.length(); land.r0 = land.rg;
        land.gDir.copy(rs.sPos).normalize(); land.settle = 0; land.t = 0; land.auto = false;
        P.copy(land.sPos).applyQuaternion(q).add(c); shipRoot.quaternion.copy(q).multiply(land.sQuat);
        hullY(0); exOff(exMe); combatRoot.visible = false;
        gmode = rs.mode; camRelInit = false;
        if (rs.mode === 'foot' && rs.hum) {
            if (!hum.obj) { hum.obj = makeHuman(effColor()); scene.add(hum.obj.group); } else hum.obj.setColor(effColor());
            hum.pos.copy(rs.hum.pos); hum.hr = rs.hum.hr; hum.gr = rs.hum.gr; hum.vv = 0; hum.air = false; hum.vh = 0;
            hum.hf.copy(rs.hum.hf); hum.face.copy(rs.hum.face); hum.pitch = rs.hum.pitch; hum.moving = hum.running = false;
            hum.obj.group.visible = true; jumpHeld = true;
        } else if (hum.obj) hum.obj.group.visible = false;
        setGround(true, rs.mode === 'foot'); setPrompt('', false);
        net && net.setMode && net.setMode(rs.mode);
        return true;
    }

    // ─── rev 24: guidance. Inside 2.5 R of a surface planet: markers for the nearest 7/11, the Burger House and the pads; the station mouth in
    // space within 3 x coreR. On-screen = icon + name + distance (L); off-screen = an edge chevron. Pooled DOM, text / icon written on change only. ───
    // rev 29: HUD markers are SYMBOLS only. Distance (a tiny number) only inside 60 L; the name shows once the marker has sat within 3 deg of the reticle for 1.5 s.
    var MK_G = {
        store: '<path d="M1 6V4l2-3h10l2 3v2z"/><path fill-rule="evenodd" d="M2 7h12v8H2zM6 10v5h4v-5z"/>',
        fries: '<path d="M3 7h10l-1 8H4z"/><path d="M4 2h1.5v5H4zM7.2 1h1.6v6H7.2zM10.5 2H12v5h-1.5z"/>',
        pad: '<path d="M1 11h14v3H1z"/><path d="M8 1l5 7H3z"/><path d="M7 8h2v3H7z"/>',
        station: '<path fill-rule="evenodd" d="M8 1a7 7 0 1 0 .01 0zM8 4a4 4 0 1 1-.01 0z"/><path d="M0 7h16v2H0z"/>',
        flag: '<path d="M3 1h2v14H3z"/><path d="M5 2h9l-3 3.5L14 9H5z"/>',
        freighter: '<path d="M1 5h10v6H1z"/><path d="M12 7h3l1 2v2h-4z"/><path d="M2 12h12v2H2z"/>',
        crystal: '<path d="M8 0l5 5-5 11-5-11z"/>',
        diamond: '<path d="M8 1l7 7-7 7-7-7z"/>',
        coin: '<path fill-rule="evenodd" d="M8 1a7 7 0 1 0 .01 0zM8 4a4 4 0 1 1-.01 0z"/><path d="M7 6h2v4H7z"/>'
    };
    var MK_KIND = { '7/11': 'store', store: 'store', burger: 'fries', pad: 'pad', landmark: 'flag', post: 'station', shard: 'coin', friend: 'diamond', resource: 'crystal' };
    function mkSvg(g) { return '<svg viewBox="0 0 16 16" width="16" height="16" shape-rendering="crispEdges" fill="currentColor">' + (MK_G[g] || MK_G.diamond) + '</svg>'; }
    var mkV = new THREE.Vector3();
    function mkAng(p) { mkV.copy(p).applyMatrix4(camera.matrixWorldInverse); var l = mkV.length() || 1; return mkV.z < 0 ? Math.acos(clamp(-mkV.z / l, -1, 1)) * 57.2958 : 180; }
    function mkNamed(m, ang) { var now = performance.now(); if (ang < 3) { if (!m.c0) m.c0 = now; return now - m.c0 >= 1500; } m.c0 = 0; return false; }
    function mkDist(d) { var l = d / L; return l <= 60 ? String(Math.round(l)) : ''; }
    var GD_N = 9, GD_R = 2.5, GD_EVERY = 0.5, gdPool = [], gdLocal = [], gdT = 0, gdNode = null, gdShown = 0, gdIcons = {}, elGuide = hud.querySelector('.sh-guide');
    (function buildGuide() {
        for (var i = 0; i < GD_N; i++) {
            var el = document.createElement('div'); el.className = 'sh-gd';
            el.innerHTML = '<i class="gd-c"></i><div class="gd-t"><span class="gd-i"></span><span class="gd-d"></span><span class="gd-n"></span></div>';
            elGuide.appendChild(el);
            gdPool.push({ el: el, c: el.firstChild, cv: el.querySelector('.gd-i'), c0: 0, n: el.querySelector('.gd-n'), d: el.querySelector('.gd-d'), key: '', name: '', dist: '', on: false, edge: false, rot: 0, tx: -1e9, ty: -1e9 });
        }
    })();
    function gdIcon(kind) {
        if (gdIcons[kind]) return gdIcons[kind];
        var cv = document.createElement('canvas'); cv.width = 16; cv.height = 16; var cx = cv.getContext('2d');
        if (kind === 'landmark') { cx.fillStyle = '#ff5ce1'; cx.fillRect(7, 1, 2, 14); cx.fillRect(1, 7, 14, 2); cx.fillStyle = '#fff'; cx.fillRect(6, 6, 4, 4); }
        else if (kind === 'pad') { cx.fillStyle = '#1a1030'; cx.fillRect(1, 5, 14, 7); cx.fillStyle = '#a855f7'; cx.fillRect(2, 6, 12, 5); cx.fillStyle = '#e8d4ff'; cx.fillRect(4, 8, 8, 1); cx.fillRect(7, 6, 2, 5); }
        else {
            if (!iconsMod) return null;      // icons module not loaded yet: do not cache
            try { cx.drawImage(iconsMod.iconFor(kind === 'burger' ? { base: 'Fries', name: 'Fries', kind: 'fries', color: 0xF2C14E } : { base: 'Gardetto chips', name: '7/11', kind: 'snack', color: 0xE04A3A }), 0, 0); } catch (e0) { return null; }
        }
        return (gdIcons[kind] = cv);
    }
    function gdFmt(d) { var l = d / L; return l >= 10000 ? (l / 1000).toFixed(0) + 'K L' : (l >= 1000 ? (l / 1000).toFixed(1) + 'K L' : Math.round(l) + ' L'); }
    function gdRefresh(n) {            // pois() allocates, so it runs at 2 Hz; markers are stored in the planet's local frame and re-spun every frame
        var list = [], pois = [], i, bs = null, bb = null, pads = [], lmk = null;
        try { pois = ps.pois(); } catch (e0) { pois = []; }
        syncPlanet(n); var c = n.anchor.position, qi = lfQi.copy(n.mesh.quaternion).invert();
        var org = (gmode === 'foot' && hum.obj) ? hum.w : shipRoot.position;
        for (i = 0; i < pois.length; i++) {
            var p = pois[i]; p.d = p.pos.distanceTo(org); p.loc = p.pos.sub(c).applyQuaternion(qi);
            if (p.kind === '7/11') { if (!bs || p.d < bs.d) bs = p; } else if (p.kind === 'burger') bb = p; else if (p.kind === 'landmark') { if (!lmk || p.d < lmk.d) lmk = p; } else pads.push(p);
        }
        pads.sort(function (a, b) { return a.d - b.d; });
        if (bs) list.push(bs); if (bb) list.push(bb); if (lmk) list.push(lmk);
        for (i = 0; i < pads.length && list.length < GD_N; i++) list.push(pads[i]);
        gdLocal = list.map(function (p) { return { kind: p.kind, name: p.name, loc: p.loc }; });
    }
    var gdW = new THREE.Vector3(), gdC = new THREE.Vector3();
    function guideTick(dt) {
        var i, m, on = 0, n = null;
        if (state === 'piloting' && !boarding && !exiting && !dead && !cmdOpen && ps && ps.active && gmode !== 'docking' && gmode !== 'launching' && gmode !== 'sfoot') {
            n = ps.active;
            var org = (gmode === 'foot' && hum.obj) ? hum.w : shipRoot.position, Rn = n.mesh.scale.x;
            if (!(Rn > 0) || org.distanceTo(n.anchor.position) > GD_R * Rn || !n.anchor.visible) n = null;
        }
        var stM = false, bh = engine.blackHole;
        if (!n && station && station.group.visible && state === 'piloting' && !boarding && !exiting && gmode === 'fly' && bh && bh.coreR > 0 && shipRoot.position.distanceTo(station.mouth.pos) < 3 * bh.coreR) stM = true;
        if (!n && !stM) { if (gdShown) { for (i = 0; i < GD_N; i++) if (gdPool[i].on) { gdPool[i].on = false; gdPool[i].el.classList.remove('is-on'); } gdShown = 0; } gdNode = null; gdLocal.length = 0; return; }
        var items = [];
        if (n) {
            gdT -= dt;
            if (gdNode !== n || gdT <= 0) { gdNode = n; gdT = GD_EVERY; gdRefresh(n); }
            syncPlanet(n); var c = n.anchor.position, q = n.mesh.quaternion;
            for (i = 0; i < gdLocal.length; i++) { var g = gdLocal[i]; items.push({ kind: g.kind, name: g.name, w: gdW.copy(g.loc).applyQuaternion(q).add(c).clone() }); }
        } else items.push({ kind: '7/11', name: 'INTERGALACTIC 7/11', w: station.mouth.pos.clone() });
        camera.updateMatrixWorld();
        var W = window.innerWidth, H = window.innerHeight, org2 = (gmode === 'foot' && hum.obj) ? hum.w : shipRoot.position;
        for (i = 0; i < GD_N; i++) {
            m = gdPool[i];
            if (i >= items.length) { if (m.on) { m.on = false; m.el.classList.remove('is-on'); } continue; }
            var it = items[i];
            gdC.copy(it.w).applyMatrix4(camera.matrixWorldInverse);
            var front = gdC.z < 0;
            gdW.copy(it.w).project(camera);
            var sx = gdW.x, sy = gdW.y, edge = !front || Math.abs(sx) > 0.9 || Math.abs(sy) > 0.88;
            if (edge) { if (!front) { sx = -sx; sy = -sy; } var k = Math.max(Math.abs(sx) / 0.9, Math.abs(sy) / 0.86, 1e-6); sx = sx / k; sy = sy / k; }
            var px = Math.round((sx * 0.5 + 0.5) * W), py = Math.round((-sy * 0.5 + 0.5) * H);
            px = clamp(px, 110, W - 110); py = clamp(py, px > W - 330 ? 150 : 40, edge ? H - 90 : H - 40);      // rev 25: the top-right band belongs to the WAVE / ENEMIES panel
            for (var gq = 0; gq < i && edge; gq++) { var o2 = gdPool[gq]; if (o2.on && o2.edge && Math.abs(o2.tx - px) < 240 && Math.abs(o2.ty - py) < 30) { py -= 34; gq = -1; if (py < 150) break; } }      // edge labels never stack on each other
            if (!m.on) { m.on = true; m.el.classList.add('is-on'); m.key = ''; }
            if (m.edge !== edge) { m.edge = edge; m.el.classList.toggle('is-edge', edge); }
            if (m.key !== it.kind) { m.key = it.kind; m.cv.innerHTML = mkSvg(MK_KIND[it.kind] || 'diamond'); }
            var nmOn = !edge && mkNamed(m, mkAng(it.w)), nmT = nmOn ? String(it.name).toUpperCase() : '';
            if (m.name !== nmT) { m.name = nmT; m.n.textContent = nmT; }
            var dt2 = mkDist(it.w.distanceTo(org2));
            if (m.dist !== dt2) { m.dist = dt2; m.d.textContent = dt2; }
            if (edge) {
                var rot = Math.round(Math.atan2(px - W / 2, -(py - H / 2)) * 180 / Math.PI / 5) * 5;
                if (rot !== m.rot) { m.rot = rot; m.c.style.transform = 'rotate(' + rot + 'deg)'; }
            }
            if (m.tx !== px || m.ty !== py) { m.tx = px; m.ty = py; m.el.style.transform = 'translate(' + px + 'px,' + py + 'px)'; }
        }
        gdShown = items.length;
    }
    // Esc: a TAP does nothing while piloting (it only releases the pointer lock); HOLDING it ESC_HOLD s exits to the galaxy. escLatch eats the
    // held key's auto-repeat so the engine's own Esc handler cannot bounce straight back in.
    var ESC_HOLD = 3, esc = { on: false, t: 0 }, escLatch = false, cEscP = -1;
    var elEsc = hud.querySelector('.sh-esc');
    function escShow(p) {
        var v = p > 0 ? Math.round(p * 60) / 60 : 0;
        if (v !== cEscP) { cEscP = v; elEsc.style.setProperty('--p', (v * 100).toFixed(1)); elEsc.classList.toggle('is-on', v > 0); }
    }
    function escStart() { if (!esc.on && !escLatch) { esc.on = true; esc.t = 0; } }
    function escCancel() { esc.on = false; esc.t = 0; escShow(0); }
    function escTick(dt) {
        if (!esc.on) return;
        if (cmdOpen || invOpen || storeOpen || boarding || exiting) { escCancel(); return; }
        esc.t += dt;
        if (esc.t < 0.25) return;                       // a tap never shows the ring
        escShow(Math.min(1, esc.t / ESC_HOLD));
        if (esc.t >= ESC_HOLD) { escCancel(); escLatch = true; exit(); }
    }
    function escEnter() { if (escLatch) return; enter(); }
    window.addEventListener('keyup', function (e) { if (e.code === 'Escape') { escLatch = false; escCancel(); } }, true);
    window.addEventListener('blur', function () { escLatch = false; escCancel(); });
    document.addEventListener('mousedown', function () {        // a click after an Esc tap takes the pointer lock back (and cancels a stray hold)
        if (state !== 'piloting' || locked() || cmdOpen || invOpen || storeOpen) return;
        escCancel();
        try { var p = document.body.requestPointerLock(); if (p && p.catch) p.catch(function () { /* keyboard only */ }); } catch (e0) { /* ignore */ }
    }, true);
    // Leaving reverses the boarding move; the camera is only handed back (finishExit) once the hull is galaxy-scale again.
    function exit(focus) {
        if (state !== 'piloting' || exiting) return;
        r25MenuClose(); closeInv(); closeStore(); fxReset();
        if (gmode === 'ifoot') leaveInterior(false);
        takeSnap();
        if (sk.mode) skAbort(); else if (gmode !== 'fly') leaveGround();
        aEngine({ throttle: 0, boost: false, pulse: 0, inAtmo: false });
        musicStop(); scanT = 0; ramT = 0; writeSave();
        setTimeout(function () { if (state !== 'piloting' || exiting) duck(true); }, 500);
        setPrompt('');
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
        exiting = false; state = 'away'; syncLod(); dk.glide = true; dk.follow = true; dk.still = 0; dk.recov = false;
        engine.setPilot(null);
        engine.onEscape(escEnter);
        combatRoot.visible = false;          // enemies stay in the scene, frozen and hidden
        hud.classList.remove('is-on', 'is-pulsing');
        setHudOpacity(1);
        document.body.classList.remove('is-piloting');
        camera.fov = baseFov; camera.near = baseNear; camera.updateProjectionMatrix();
        killBolts(); exOff(exMe); ghostEx.forEach(exOff); aEngine({ throttle: 0 });
        if (station) station.setVisible(false);
        if (space) space.group.visible = false;
        try { engine.lensing(0); bhLast = -1; } catch (e) { /* ignore */ }
        fx.setMotion(vel, 0, false); fx.update(1, camera); fx.streaks.visible = false; fx.bolts.visible = false;
        var pf = pendingFocus; pendingFocus = null;
        engine.focusNode(pf || engine.root, true);
        lastLX = lastLY = -1e9;
    }

    // ─── game reset / waves ─────────────────────────────────────────
    // startW = wave number about to begin; kills carried in. Clears every creature, ally, bolt, orb.
    function detachCreature(e) {
        if (!e.cr) return;
        e.body.remove(e.cr.group); e.cr.dispose(); e.cr = null; e.mat = null;      // geometry + materials freed on recycle
    }
    function hideEnemy(e) {
        e.alive = false; e.g.visible = false; e.charge = 0; if (e.beam) e.beam.visible = false;
        if (e.leader && e.squad && squadLead[e.squad] === e) squadLead[e.squad] = null;
        e.leader = false; e.squad = 0; e.stallT = 0; e.stalled = false; e.ap = 0; e.jinkT = 0; e.jinkRoll = 0; e.wv = 0;
        if (bsig.owner === e) sigReset();
        detachCreature(e); e.isBoss = false; e.kind = 0;
    }
    function resetGame(startW, keepKills) {
        var i;
        for (i = 0; i < enemies.length; i++) hideEnemy(enemies[i]);
        for (i = 0; i < allies.length; i++) { allies[i].alive = false; allies[i].g.visible = false; }
        for (i = 0; i < orbs.length; i++) { orbs[i].active = false; orbs[i].m.visible = false; }
        for (i = 0; i < bombs.length; i++) { bombs[i].active = false; bombs[i].m.visible = false; bombs[i].shell.visible = false; }
        heat = 0; ohT = 0; ramT = 0; ramCd = 0; squadLead.length = 0; waveQuiet = false;
        for (i = 0; i < drops.length; i++) { drops[i].active = false; drops[i].m.visible = false; }
        for (i = 0; i < crates.length; i++) { crates[i].active = false; crates[i].g.visible = false; }
        for (i = 0; i < boomQ.length; i++) boomQ[i].on = false;
        killBolts();
        applyUpgrades(); hp = HP_MAX; sinceHit = 99; kills = keepKills | 0; gt = 0;
        wave = Math.max(0, (startW | 0) - 1); wavePending = true;
        nextWave = startW > 1 ? DEATH_RESPAWN_DELAY : FIRST_WAVE;
        waveActive = false; qN = 0; chainN = 0; chainT = 0; graze = 0; od = 0; focusE = FOCUS_MAX; zT = 0; zTarget = null;
        tailAlly = null; tailBy = null; tailLeft = 0; ctarget = null; tlock = false; rollT = 0; rollCd = 0; hitStopN = 0; comboN = 0; comboT = 0; driftBoost = 0; pulseT = 0;
        flipT = 0; flipCd = 0; driftOn = false; driftLeft = 0; driftCd = 0; terrInv = 0; yawRate = 0; pitRate = 0;
        dead = false; deathT = 0; waveMsgT = 0; fireCd = 0; playerFired = false; curWave = waveParams(WAVE_X * Math.max(1, startW)); bossWave = false;
        gameFresh = false;
    }
    function aliveCount() {
        var c = 0;
        for (var i = 0; i < enemies.length; i++) if (enemies[i].alive) c++;
        return c;
    }
    function bossAlive() {
        for (var i = 0; i < enemies.length; i++) if (enemies[i].alive && enemies[i].isBoss) return true;
        return false;
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
    // rev 13: 0 regular wave, 1 mini-boss (every 2nd wave from 3), 2 giant (every 4th) with 2 mini escorts, 3 titan (waves 12, 24, ...)
    function bossKind(w) { return (w >= 12 && w % 12 === 0) ? 3 : ((w >= 4 && w % 4 === 0) ? 2 : ((w >= 3 && (w - 3) % 2 === 0) ? 1 : 0)); }
    function regTier(w) { return Math.min(6, 1 + ((w - 1) >> 1)); }          // tier 1 at waves 1-2, +1 every 2 waves, cap 6
    // rev 14: boss length = refR x clamp(0.05 x wave, 0.15, 1.0) (wave 20 ~ a planet; minis 0.6x); titans 1.5 x refR
    function bossLen(kind) { return kind === 3 ? TITAN_X * refR : refR * clamp(0.05 * wave, 0.15, 1) * (kind === 1 ? 0.6 : 1); }
    function genLen(tier, kind) { return kind > 0 ? bossLen(kind) : 6 * Math.pow(1.6, tier - 1) * L; }
    function pickRole(i, want) {
        var avail = wave >= 6 ? 5 : (wave >= 5 ? 4 : (wave >= 3 ? 3 : (wave >= 2 ? 2 : 1)));
        if (i === 0 || avail === 1) return 0;
        if (i === 1 && want >= 3) return 1;                     // every wave shows each unlocked role once
        if (i === 2 && avail >= 3 && want >= 4) return 2;
        if (i === 3 && avail >= 4 && want >= 5) return 3;
        if (i === 4 && avail >= 5 && want >= 6) return 4;
        var tot = 0, k, r;
        for (k = 0; k < avail; k++) tot += ROLE_T[k].mix;
        r = Math.random() * tot;
        for (k = 0; k < avail; k++) { r -= ROLE_T[k].mix; if (r <= 0) return k; }
        return 0;
    }
    function attachCreature(e, seed, tier, role, kind, name) {
        detachCreature(e);
        var cr = null;
        e.plan = '';
        if (kind > 0 && typeof ENM.generateBoss === 'function') {       // rev 14 bosses: body plans (serpent, crab, jelly, leviathan, hydra); falls back to a big generateEnemy
            try {
                var bnm = name || ('B' + seed), bpl = ENM.BOSS_PLANS[ENM.bossHash(bnm) % ENM.BOSS_PLANS.length];
                for (var pt = 0; pt < ENM.BOSS_PLANS.length && (bpl === lastBossPlan || wavePlans.indexOf(bpl) >= 0); pt++) bpl = ENM.BOSS_PLANS[(ENM.BOSS_PLANS.indexOf(bpl) + 1) % ENM.BOSS_PLANS.length];      // rev 25: distinct plans within a wave
                wavePlans.push(bpl);
                lastBossPlan = bpl;
                cr = ENM.generateBoss(THREE, bnm, wave, { kind: kind, refRL: refR / L, plan: bpl }); e.plan = (cr && cr.plan) ? String(cr.plan) : ''; } catch (err) { console.warn('[ship] generateBoss failed', err); cr = null; }
        }
        if (!cr) cr = generateEnemy(THREE, seed, tier, ENEMY_ROLES[role]);
        cr.group.traverse(function (o) {
            o.frustumCulled = false;
            if (!e.mat && o.isMesh && o.material && o.material.uniforms && o.material.uniforms.uHit) e.mat = o.material;
        });
        e.body.add(cr.group); e.cr = cr;
        e.tier = tier; e.role = role; e.seed = seed; e.kind = kind;
        e.sc = kind > 0 ? bossLen(kind) / cr.length : L;      // generator contents are in L units; bosses are scaled to the rev 14 size curve
        e.g.scale.setScalar(e.sc); e.body.scale.setScalar(1);
        e.len = cr.length * e.sc; e.R = cr.hitR * e.sc;
        e.eyeN = Math.min(E_EYE_MAX, cr.eyes.length);
        if ((role === 2 || role === 3) && !e.beam) e.beam = makeBeam();
    }
    // world positions of the eye cores (weak points), cached once per update after the wobble
    function refreshEyes(e) {
        if (!e.cr) return;
        e.g.updateMatrixWorld(true);
        for (var i = 0; i < e.eyeN; i++) { var m = e.cr.eyes[i].matrixWorld.elements; e.eyeW[i].set(m[12], m[13], m[14]); }
        if (!e.eyeN) e.eyeW[0].copy(e.g.position);      // rev 20: every eye gone with its limb: the body centre stands in for the core
    }
    // vAim = spawn position (set by pickSpawn / the caller). kind 0 regular, 1-3 boss.
    function initEnemy(e, role, tier, seed, kind, grace, title) {
        var cw = curWave, D = DIFFS[difficulty], R = ROLE_T[role], isB = kind > 0;
        attachCreature(e, seed, tier, role, kind, (title || '').replace(/^(TITAN|MINI) /, ''));
        var st = e.cr.stats, dm;
        e.isBoss = isB; e.title = title || '';
        // rev 21: squad membership, role behaviour (hunter / harasser / bomber from the generator's stats), attack-run + lock-on state
        e.squad = isB ? 0 : (pendSq | 0); e.leader = !isB && !!pendLd && e.squad > 0; e.elite = !isB && !!pendEl; pendSq = 0; pendLd = false; pendEl = false;
        if (e.leader) squadLead[e.squad] = e;
        e.beh = (!isB && st && st.behavior) || 'hunter'; e.shielded = !isB && !!(st && st.shield);
        e.ap = 0; e.apT = 0; e.stallT = 0; e.stalled = false; e.jinkT = 0; e.jinkCd = 0; e.lockT = 0; e.jinkRoll = 0; e.breakT = 0; e.breakCd = 1 + Math.random() * 2; e.bombed = false;
        e.fOff.set((Math.random() < 0.5 ? -1 : 1) * (5 + Math.random() * 6) * L, (Math.random() - 0.5) * 5 * L, (6 + Math.random() * 6) * L);
        // rev 20: a boss dies in ~BOSS_TTK s of sustained fire at the player's CURRENT dps (x0.8, kind factor, difficulty); each limb carries 25 % of that
        e.alive = true; e.hp = e.maxHp = isB ? Math.max(60, Math.round(BOSS_TTK * playerDps() * 0.8 * BOSS_TTK_X[kind] * D.hp)) : Math.max(8, Math.round(st.hp * D.hp));
        if (!isB && (e.elite || e.leader)) e.hp = e.maxHp = Math.round(e.maxHp * (e.elite ? 1.6 : 1) * (e.leader ? 1.3 : 1));
        e.stagger = 0; e.noLimbs = false; e.limbHp = null; e.limbMax = 0;
        if (isB && e.cr.limbs && e.cr.limbs.length) { e.limbMax = e.maxHp * LIMB_HP_F; e.limbHp = e.cr.limbs.map(function () { return e.limbMax; }); }
        e.state = 0; e.fstate = 'patrol'; e.timer = 0; e.fireCd = 1 + Math.random() * cw.fireInt * R.fire; e.burst = 0;
        dm = isB ? 6 + 1.6 * Math.max(0, tier - 4) : (role === 2 ? SNIPER_DMG * (1 + 0.12 * (tier - 1)) : Math.min(25, st.dmg * 0.5));
        e.dive = cw.dive; e.bSpeed = isB ? 0.1 : st.speed * L * cw.spd * (e.beh === 'bomber' ? 0.6 : 1); e.bDmg = dm;
        e.speed = e.bSpeed * D.spd; e.turn = isB ? (kind === 3 ? 0.12 : 0.7) : st.turn * Math.min(1.3, cw.turn); e.dmg = e.bDmg * D.dmg; e.fireInt = cw.fireInt * R.fire;
        e.err = cw.err; e.lead = cw.lead; e.cone = cw.cone; e.curSpeed = e.speed * 0.4; e.hitT = 0; e.flinch = 0;
        e.hasBase = false; e.fp = null; e.skimOn = false; e.sigGlow = 0;
        e.fleeN = 0; e.ftimer = 0; e.charge = 0; e.snCd = 3 + Math.random() * 2; e.idle = 0; e.far = 0; e.grace = grace; e.hunt = null; e.first = true; e.open = false;
        e.lphase = 0; e.lt = 1 + Math.random() * 2; e.pflash = 0; e.wind = 0; e.volley = 0;
        e.wAmp = (3 + Math.random() * 5) * L; e.wPer = 1.5 + Math.random() * 1.5; e.wPh = Math.random() * 6.283; e.wSide = Math.random() < 0.5 ? 1 : -1; e.slantT = 2 + Math.random() * 3;
        e.g.position.copy(vAim);
        vD.copy(shipRoot.position).sub(vAim);                  // initial heading: toward the player, spread so a wave is not a firing line
        vD.x += (Math.random() - 0.5) * L * 60; vD.z += (Math.random() - 0.5) * L * 60;
        mM.lookAt(ORIGIN, vD, Y);
        e.g.quaternion.setFromRotationMatrix(mM);
        if (isB) {
            e.home.copy(vAim); e.gcyc = 4 + Math.random() * 8; e.chaseD = vAim.distanceTo(shipRoot.position);      // rev 19: a boss keeps closing as long as the player is within 1.3 x its spawn distance
            e.fstate = 'boss'; e.state = 1; e.bphase = 1; e.cyc = 6; e.side = 1; e.flip = 6; e.orbCd = 6; e.fireCd = 3; e.curSpeed = 0; e.reachMax = 0;
            e.atk = bossAttacksFor(seed, tier); e.atkI = 0; e.nextAtk = -1; e.sigCd = 1e9;
            e.ms = 'idle'; e.mt = 0; e.mcd = 0.3 + Math.random() * 0.4; e.mi = 0; e.mv = null; e.hitDone = false; e.escT = 8 + Math.random() * 4; e.thrCd = 5 + Math.random() * 5;
            mM.lookAt(vAim, shipRoot.position, Y);
            e.g.quaternion.setFromRotationMatrix(mM);
        } else if (role === 0 && wave >= 3 && tier > 1 && Math.random() < 0.4) {   // some interceptors hunt a wingman instead of you
            var h0 = (Math.random() * 3) | 0;
            for (var k = 0; k < allies.length; k++) { var ha = allies[(h0 + k) % allies.length]; if (ha.alive) { e.hunt = ha; break; } }
        }
        pickWander(e, scratchBodies);
        e.g.visible = true; e.lod = 0; e.body.visible = true; if (e.imp) e.imp.visible = false;
        e.body.rotation.set(0, 0, 0);
        refreshEyes(e);
        if (isB) aPlay('bossRoar', { dist: Math.min(aDist(vAim), 400) * 0.15 });
    }
    function freeSlot() { for (var i = 0; i < enemies.length; i++) if (!enemies[i].alive) return enemies[i]; return null; }
    function qRemove(i) { qN--; qAt[i] = qAt[qN]; qRole[i] = qRole[qN]; qTry[i] = qTry[qN]; qTier[i] = qTier[qN]; qSeed[i] = qSeed[qN]; qKind[i] = qKind[qN]; qTitle[i] = qTitle[qN]; qSq[i] = qSq[qN]; qLd[i] = qLd[qN]; qEl[i] = qEl[qN]; }
    function qPush(at, role, tier, seed, kind, title, sq, ld, el) {
        qAt[qN] = at; qRole[qN] = role; qTry[qN] = 0; qTier[qN] = tier; qSeed[qN] = seed; qKind[qN] = kind; qTitle[qN] = title; qSq[qN] = sq | 0; qLd[qN] = !!ld; qEl[qN] = !!el; qN++;
    }
    // rev 21: squads. squadLead[id] = the slot currently marked LEADER of squad id (validated on read: alive, flagged, same squad)
    var squadLead = [], squadSeq = 0, pendSq = 0, pendLd = false, pendEl = false;
    function leaderOf(e) {
        if (!e.squad || e.leader) return null;
        var l = squadLead[e.squad];
        return (l && l.alive && l.leader && l.squad === e.squad) ? l : null;
    }
    var bossCtr = 0, lastBossPlan = '', wavePlans = [];
    function bossSeedName() {           // rev 17: every boss spawn is unique: seed = hash(wave, spawn counter, relay epoch)
        var h = 2166136261, ep = (net && net.epoch) | 0, a = [wave, ++bossCtr, ep, (Math.random() * 1e6) | 0];
        for (var i = 0; i < a.length; i++) { h ^= a[i] | 0; h = Math.imul(h, 16777619); h ^= h >>> 13; }
        return bountyNameFor() || bossName((h >>> 0) % 1000003);
    }
    var BOSS_ROLES = [4, 0, 1, 3, 2];                 // palette/stat family cycle for bosses (brood, interceptor, spitter, lancer, sniper)
    function spawnWave(bodies) {
        var i, bk = bossKind(wave + 1);
        var quiet = !peaceful && (wave + 1) % QUIET_EVERY === 0 && bk <= 1;      // rev 21 director: every 5th wave is a quiet one: a single elite squad, longer breather
        if (quiet) bk = 0;
        wave++; wavePending = false; playerFired = false; waveActive = true; waveStartT = gt; waveQuiet = quiet; wavePlans.length = 0;
        curWave = waveParams(WAVE_X * wave);
        bossWave = bk > 0;
        var D = DIFFS[difficulty], nn = Math.max(2, Math.min(10, curWave.n + D.n));
        if (quiet) nn = 4;
        var nReg = bk ? 2 : nn, rt = bk ? Math.max(1, regTier(wave) - 1) : (quiet ? regTier(wave) + 1 : regTier(wave));
        var roles = [], tiers = [], kinds = [], titles = [], nm = wave * 7;
        if (bk === 3) { roles.push(BOSS_ROLES[(wave >> 2) % 5]); tiers.push(BOSS_TIERS.titan); kinds.push(3); titles.push('TITAN ' + bossSeedName()); }
        if (bk === 2) {
            roles.push(BOSS_ROLES[wave % 5]); tiers.push(BOSS_TIERS.giant); kinds.push(2); titles.push(bossSeedName());
            for (i = 0; i < 2; i++) { roles.push(BOSS_ROLES[(wave + 1 + i) % 5]); tiers.push(BOSS_TIERS.mini); kinds.push(1); titles.push('MINI ' + bossSeedName()); }
        }
        if (bk === 1) { roles.push(BOSS_ROLES[wave % 5]); tiers.push(BOSS_TIERS.mini); kinds.push(1); titles.push(bossSeedName()); }
        for (i = 0; i < nReg; i++) { roles.push(pickRole(i, nReg)); tiers.push(rt); kinds.push(0); titles.push(''); }
        var names = roles.map(function (r) { return ENEMY_ROLES[r]; });
        var seeds = pickDistinctSeeds((wave * 7919 + 101 + ((Math.random() * 1e6) | 0)) | 0, roles.length, names);   // no two identical silhouettes in a wave
        spawnPad = 6 * L;
        randDir(dirA);
        for (i = 0; i < 6; i++) { randDir(dirB); if (dirA.dot(dirB) < 0.1) break; }
        qN = 0;
        planFormation(kinds, tiers, titles, bodies);
        // squads of 3-5 (a quiet wave is exactly one elite squad); the first of each chunk is the LEADER and spawns first, the rest cluster on it
        var regIdx = [], chunks, c0 = 0, sqOf = [], ldOf = [];
        for (i = 0; i < roles.length; i++) if (!kinds[i]) regIdx.push(i);
        chunks = regIdx.length >= 6 ? Math.ceil(regIdx.length / 5) : 1;
        for (var ci = 0; ci < chunks && regIdx.length >= 2; ci++) {
            var csz = Math.floor(regIdx.length / chunks) + (ci < regIdx.length % chunks ? 1 : 0), sid = ++squadSeq;
            for (var cj = 0; cj < csz; cj++) { sqOf[regIdx[c0 + cj]] = sid; ldOf[regIdx[c0 + cj]] = cj === 0; }
            c0 += csz;
        }
        var sqBase = {}, sqK = {};
        for (i = 0; i < roles.length; i++) {                       // bosses now; squads spawn together (squad 1 now, the others 3-5 s later)
            var at, sg = sqOf[i] | 0;
            if (kinds[i]) at = gt + i * 0.3;
            else if (sg) { if (sqBase[sg] === undefined) { sqBase[sg] = Object.keys(sqBase).length ? gt + 3 + Math.random() * 2 : gt; sqK[sg] = 0; } at = sqBase[sg] + (sqK[sg]++) * 0.25; }
            else at = gt + (roles[i] === 0 ? i * 0.25 : 4 + Math.random() * 2.5);
            qPush(at, roles[i], tiers[i], seeds[i], kinds[i], titles[i], sg, ldOf[i], quiet && !kinds[i]);
        }
        regenDelay = Math.max(0, curWave.regenDelay + D.regen);
        var msg;
        if (bossWave) msg = 'WARNING · ' + titles[0];
        else if (quiet) msg = 'QUIET · ELITE SQUAD';
        else msg = 'WAVE ' + wave + (wave === 2 ? ' · SPITTERS' : (wave === 3 ? ' · SNIPERS' : (wave === 5 ? ' · LANCERS' : (wave === 6 ? ' · BROOD' : '')))) + ' · ' + nReg + ' HOSTILES';
        spawnAllies(allyCountFor(wave - D.allyLag));
        if (bossWave) announceBoss(msg);
        writeSave();
        pumpQueue(bodies);
    }
    // rev 19: boss formation. Several bosses spawn on an arc around the player: the biggest is the LEAD straight ahead (0.7 length + 120-180 L out),
    // the others are WINGS swung out to +/- 70 degrees (and further for a 4th / 5th) and pushed out just far enough that every pair is at least 1.65 x the
    // longer of the two apart (the game then enforces 1.5 x). The whole formation is rotated until it clears every planet shell and the arena edge.
    var formPos = {}, fpA = new THREE.Vector3(), fpB = new THREE.Vector3(), fpU = new THREE.Vector3();
    function planFormation(kinds, tiers, titles, bodies) {
        formPos = {};
        var idx = [], i, j;
        for (i = 0; i < kinds.length; i++) if (kinds[i] === 1 || kinds[i] === 2) idx.push(i);
        if (!idx.length) return;
        var lens = {}; idx.forEach(function (k) { lens[k] = genLen(tiers[k], kinds[k]); });
        idx.sort(function (a, b) { return lens[b] - lens[a]; });
        var base = new THREE.Vector3().copy(NEG_Z).applyQuaternion(shipRoot.quaternion); base.y *= 0.5; base.normalize();      // rev 25: in FRONT of the ship, within +/-40 degrees
        var yaw0 = (Math.random() * 2 - 1) * 0.69, c0 = Math.cos(yaw0), s0 = Math.sin(yaw0); base.set(base.x * c0 + base.z * s0, base.y, -base.x * s0 + base.z * c0).normalize();
        for (var attempt = 0; attempt < 14; attempt++) {
            var yaw = attempt * 0.12 * (attempt & 1 ? -1 : 1), cy = Math.cos(yaw), sy = Math.sin(yaw);
            var placed = [], ok = true;
            for (i = 0; i < idx.length && ok; i++) {
                var k = idx[i], len = lens[k], rBase = 0.7 * len + (90 + 50 * (((k * 2654435761) >>> 0) % 100) / 100) * L;
                var ang = i === 0 ? 0 : (i % 2 ? 1 : -1) * 0.35 * Math.ceil(i / 2);                  // rev 25: 20 deg per wing rank (stay in front)
                var ca = Math.cos(ang), sa = Math.sin(ang);
                fpU.set(base.x * ca + base.z * sa, base.y, -base.x * sa + base.z * ca);               // base rotated about world Y by the wing angle
                fpU.set(fpU.x * cy + fpU.z * sy, fpU.y, -fpU.x * sy + fpU.z * cy).normalize();         // whole formation yawed by the attempt
                var r = rBase;
                for (j = 0; j < placed.length; j++) {
                    var o = placed[j], sep = 1.65 * Math.max(len, o.len), up = fpU.x * o.p.x + fpU.y * o.p.y + fpU.z * o.p.z;
                    var disc = up * up - o.p.lengthSq() + sep * sep;
                    if (disc > 0) r = Math.max(r, up + Math.sqrt(disc));
                }
                var pos = new THREE.Vector3().copy(shipRoot.position).addScaledVector(fpU, r), rel = new THREE.Vector3().copy(fpU).multiplyScalar(r);
                if (pos.length() > EDGE_R * 0.97) { ok = false; break; }
                for (j = 0; j < bodies.length; j++) { var rb = bodies[j].R * 1.1 + 0.45 * len; if (pos.distanceToSquared(bodies[j].node.anchor.position) < rb * rb) { ok = false; break; } }
                if (!ok) break;
                placed.push({ k: k, len: len, p: rel, w: pos });
            }
            if (ok) { placed.forEach(function (pl) { formPos[titles[pl.k]] = pl.w; }); return; }
        }
        // nothing fit (cramped arena): fall back to the plain per-boss placement in placeSpawn
    }
    // a titan spawns far out along the wave's approach direction (it is bigger than a planet: no body test); everything else uses pickSpawn
    function placeSpawn(bodies, i) {
        var kind = qKind[i];
        spawnPad = genLen(qTier[i], kind);
        if (kind === 3 && R27.titan && qTitle[i] === R27.titan.title) { vAim.copy(R27.titan.pos); R27.titan = null; return true; }      // rev 27: relay event titan, placed by the named planet
        if (kind === 3) { vAim.copy(shipRoot.position).addScaledVector(dirA, 1.4 * spawnPad + 400 * L); return true; }
        if (kind > 0 && formPos[qTitle[i]]) { vAim.copy(formPos[qTitle[i]]); return true; }
        if (kind > 0) {          // rev 14: a static boss parks 0.7 x its length + 160 L out, clear of every planet shell
            for (var tr = 0; tr < 14; tr++) {
                vD.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); vD.applyAxisAngle(Y, (Math.random() * 2 - 1) * 0.69);
                if (tr > 3) { vD.x += (Math.random() - 0.5) * 1.2; vD.y += (Math.random() - 0.5) * 0.6; vD.z += (Math.random() - 0.5) * 1.2; vD.normalize(); }
                vAim.copy(shipRoot.position).addScaledVector(vD, 0.7 * spawnPad + (90 + Math.random() * 50) * L);
                var okB = vAim.length() < EDGE_R * 0.97;
                for (var jb = 0; okB && jb < bodies.length; jb++) { var rb = bodies[jb].R * 1.1 + 0.45 * spawnPad; if (vAim.distanceToSquared(bodies[jb].node.anchor.position) < rb * rb) okB = false; }
                if (okB) return true;
            }
            vAim.copy(shipRoot.position).addScaledVector(dirA, 0.7 * spawnPad + 160 * L);
            return true;
        }
        var sq = qSq[i] | 0;
        if (sq && !qLd[i]) {            // a squad member forms up on its leader (if it is already out), else on the squad's approach side
            var lead = squadLead[sq];
            if (lead && lead.alive && lead.leader && lead.squad === sq) {
                for (var st = 0; st < 8; st++) {
                    randDir(vD); vAim.copy(lead.g.position).addScaledVector(vD, (9 + Math.random() * 8) * L);
                    if (vAim.length() > EDGE_R * 0.97) continue;
                    var okS = true;
                    for (var js = 0; js < bodies.length; js++) { var rbs = bodies[js].R * 1.6 + spawnPad; if (vAim.distanceToSquared(bodies[js].node.anchor.position) < rbs * rbs) { okS = false; break; } }
                    if (okS) return true;
                }
            }
        }
        if (pickSpawn(bodies, ((sq || i) & 1) ? dirB : dirA)) return true;
        return false;
    }
    function pumpQueue(bodies) {
        var made = 0;
        for (var i = 0; i < qN && made < 1;) {                      // at most 1 generateEnemy call a frame (3-10 ms each)
            if (qAt[i] > gt) { i++; continue; }
            var e = freeSlot();
            if (!e || !placeSpawn(bodies, i)) {
                qAt[i] = gt + 0.4; if (++qTry[i] > 10) qRemove(i); else i++;
                continue;
            }
            pendSq = qSq[i]; pendLd = qLd[i]; pendEl = qEl[i];
            initEnemy(e, qRole[i], qTier[i], qSeed[i], qKind[i], GRACE_T, qTitle[i]);
            spawnPad = 6 * L;
            qRemove(i); made++;
        }
    }
    function spawnAdds(n, own) {                                   // boss phase change: n interceptors from the boss's flanks (near the player for a titan)
        for (var k = 0; k < n; k++) {
            var e = freeSlot();
            if (!e) return;
            randDir(vD);
            if (own.len > 300 * L) vAim.copy(shipRoot.position).addScaledVector(vD, (SPAWN_NEAR + Math.random() * 60) * L);
            else vAim.copy(own.g.position).addScaledVector(vD, own.len * 0.9 + 20 * L);
            initEnemy(e, 0, Math.max(1, regTier(wave) - 1), (Math.random() * 1e9) | 0, 0, 1.2, '');
        }
    }
    function spawnLarvae(pos, len, n) {                            // brood: 3 tier-1 larvae burst out of the carcass
        for (var k = 0; k < n; k++) {
            var e = freeSlot();
            if (!e) return;
            randDir(vD); vAim.copy(pos).addScaledVector(vD, len * 0.35);
            initEnemy(e, 0, 1, (Math.random() * 1e9) | 0, 0, 0.5, '');
            e.curSpeed = e.speed * 1.4;
        }
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
        var oldest = null;
        if (enemy && holdFire) return null;       // rev 22: behind cover = no lock = no shot
        for (var i = 0; i < bolts.length; i++) {
            var b = bolts[i];
            if (b.active) { if (!enemy && !b.enemy && !b.remote && (!oldest || b.t0 < oldest.t0)) oldest = b; continue; }
            vFb.set(ox, oy, oz); vFd.set(dx, dy, dz);
            var id = fx.spawnBolt(vFb, vFd, col != null ? col : (enemy ? C_E : effColor()), spd, life);
            if (id < 0) return null;
            b.id = id; b.active = true; b.enemy = enemy; b.dmg = dmg; b.remote = false; b.owner = null; b.boss = false; b.gz = 0; b.t0 = gt;
            b.prev.copy(vFb);
            return b;
        }
        if (!enemy && oldest) {          // rev 19: 20 s bolts fill the pool: the oldest player bolt makes room
            oldest.active = false; fx.killBolt(oldest.id);
            return fireBolt(ox, oy, oz, dx, dy, dz, spd, life, enemy, dmg, len, wid, col);
        }
        return null;
    }
    // hit / death sparks: fx.impact (pooled), size by target
    function burst(p, n, matIdx, speedL, sz) {
        var k = Math.max(1, Math.round(n / 8)), size = clamp((sz || 1) * speedL / 10, 0.5, 5);
        for (var i = 0; i < k; i++) fx.impact(p, BURST_COL[matIdx] || 0xffffff, size);
    }
    var god = false, vBI = new THREE.Vector3(), boltsFired = 0, boltHits = 0, boltFrame = 0;
    var SAFE_R = DOCK_R * root.sysR * 1.2;    // pilots inside the dock radius cannot be hurt by other pilots
    var lastHurt = 0;
    function hurtPlayer(dmg, by, mult, raw) {
        lastHurt = dmg;      // rev 22: tests read what a hit WOULD cost, god mode or not
        if (dead || god) return;
        if (rollT > 0 && !raw) dmg *= 1 - ROLL_CUT;                     // rev 9b #3: barrel roll cuts incoming damage 40 %
        hp -= dmg; sinceHit = 0;
        aPlay('hit', { pitch: 0.55, vel: Math.min(1, dmg / 30) });
        shake = Math.max(shake, Math.min(2.4 * L, dmg / 20 * 0.25 * L * (mult || 1) * 2));     // rev 9: camera shake on hit (boss 2x); rev 17: doubled again for any hit
        if (hp <= 0) {
            hp = 0; dead = true; deathT = DEATH_TIME; firing = false;
            if (by && net) net.sendKill(by);        // killed by another pilot: they get the credit
            writeSave();
            burst(shipRoot.position, 22, 0, 18); burst(shipRoot.position, 14, 1, 12);
            aPlay('explosion', { pitch: 0.8 });
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
                addUnits(UNIT_ORB * (d.val > DROP_VAL ? 2 : 1), d.m.position);
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
    var vShA = new THREE.Vector3(), vShB = new THREE.Vector3();
    var vKill = new THREE.Vector3(), vLoot = new THREE.Vector3(), vSev = new THREE.Vector3();
    // ─── rev 20: visible damage (hit marker, floating numbers), chain-reaction deaths, limb severing, weapon loot ───
    var hmT = 0, hmCrit = false, cHm = 0;
    function hitMark(crit) { hmT = crit ? 0.24 : 0.13; hmCrit = !!crit; }
    var dnPool = [], dnCur = 0;
    (function buildDn() {
        for (var i = 0; i < DN_MAX; i++) {
            var el = document.createElement('i'); el.className = 'sh-dn'; el.style.display = 'none';
            elDmg.appendChild(el);
            dnPool.push({ el: el, on: false, p: new THREE.Vector3(), age: 0, life: 0.9, jx: 0, crit: false });
        }
    })();
    function dmgNumber(dmg, crit, pos) {                 // pooled: 24 DOM nodes, oldest recycled
        var d = dnPool[dnCur]; dnCur = (dnCur + 1) % DN_MAX;
        d.on = true; d.age = 0; d.life = crit ? 1.25 : 0.85; d.crit = !!crit; d.p.copy(pos); d.jx = (Math.random() - 0.5) * 40;
        d.el.textContent = String(Math.max(1, Math.round(dmg))) + (crit ? '!' : '');
        d.el.className = 'sh-dn' + (crit ? ' is-crit' : '');
        d.el.style.color = crit ? hexCss(effColor()) : '';
        d.el.style.display = 'block';
    }
    // ─── rev 21: units, floating text, scanner (V), shards, NPC lines, music ───────────────────────────────────────────
    function unitsNow() { return pget(curProfile(), 'gor', 0) | 0; }
    var unitSaveT = 0;
    function floatText(txt, pos, col, big) {              // pooled with the damage numbers (24 DOM nodes)
        var d = dnPool[dnCur]; dnCur = (dnCur + 1) % DN_MAX;
        d.on = true; d.age = 0; d.life = 1.3; d.crit = !!big; d.p.copy(pos); d.jx = (Math.random() - 0.5) * 30;
        d.el.textContent = txt; d.el.className = 'sh-dn is-unit' + (big ? ' is-crit' : '');
        d.el.style.color = col || '#ffd36a'; d.el.style.display = 'block';
    }
    function addUnits(n, pos, big) {
        n = Math.round(n); if (!(n > 0)) return;
        var pf = ensureProfile(userName()); if (!curUser) curUser = userName();
        pf.gor = Math.max(0, pf.gor | 0) + n;
        if (pos) floatText('+' + n + ' ɢ', pos, null, big);
        aPlay('units', { n: Math.min(12, chainN | 0) });
        if (!unitSaveT) unitSaveT = setTimeout(function () { unitSaveT = 0; writeSave(); }, 900);      // coalesced: kills come in bursts
    }
    var vPo = new THREE.Vector3();
    function posOf(o) {                                    // pos as Vector3 | {x,y,z} | [x,y,z], on o or o.pos / o.position
        var p = o && (o.pos || o.position || o);
        if (!p) return null;
        if (Array.isArray(p)) return p.length >= 3 ? vPo.set(p[0], p[1], p[2]) : null;
        return (typeof p.x === 'number' && isFinite(p.x)) ? p : null;
    }
    var scanT = 0, scanCd = 0, hlN = 0, hlPool = [], elScan = hud.querySelector('.sh-scan'), elHls = hud.querySelector('.sh-hls'), cScanOp = -1;
    (function buildHl() {
        for (var i = 0; i < HL_MAX; i++) {
            var el = document.createElement('div'); el.className = 'sh-hl'; el.style.display = 'none'; el.innerHTML = '<i></i><b></b>';
            elHls.appendChild(el);
            hlPool.push({ el: el, i: el.firstChild, g: '', col: '', c0: 0, b: el.lastChild, on: false, kind: '', txt: '', x: -1e9, y: -1e9 });
        }
    })();
    function doScan() {
        if (scanCd > 0 || dead || cmdOpen || state !== 'piloting' || boarding || exiting) return false;
        scanT = SCAN_T; scanCd = SCAN_CD;
        elScan.classList.remove('is-on'); void elScan.offsetWidth; elScan.classList.add('is-on');
        aPlay('scan', { vel: 1 });
        questScan();
        return true;
    }
    var vHl = new THREE.Vector3(), vHl2 = new THREE.Vector3();
    function hlMark(p, kind, txt, org, glyph, col) {
        if (!p || hlN >= HL_MAX) return;
        var m = hlPool[hlN++], W = window.innerWidth, H = window.innerHeight;
        vHl.set(p.x, p.y, p.z).project(camera);
        if (vHl.z >= 1 || vHl.z <= -1 || Math.abs(vHl.x) > 1.05 || Math.abs(vHl.y) > 1.05) { if (m.on) { m.on = false; m.el.style.display = 'none'; } hlN--; return; }
        var x = (vHl.x * 0.5 + 0.5) * W, y = (-vHl.y * 0.5 + 0.5) * H;
        if (!m.on) { m.on = true; m.el.style.display = ''; }
        if (kind !== m.kind) { m.kind = kind; m.el.className = 'sh-hl k-' + kind; m.g = ''; }
        var gl = glyph || MK_KIND[kind] || 'diamond';
        if (gl !== m.g) { m.g = gl; m.i.innerHTML = mkSvg(gl); }
        if ((col || '') !== m.col) { m.col = col || ''; m.el.style.color = m.col; }
        var dd = mkDist(Math.sqrt((p.x - org.x) * (p.x - org.x) + (p.y - org.y) * (p.y - org.y) + (p.z - org.z) * (p.z - org.z))), t = (mkNamed(m, mkAng(p)) ? txt : '') + (dd ? ' ' + dd : '');
        if (t !== m.txt) { m.txt = t; m.b.textContent = t; }
        if (Math.abs(x - m.x) > 0.5 || Math.abs(y - m.y) > 0.5) { m.x = x; m.y = y; m.el.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) translate(-50%,-50%)'; }
    }
    function scanUpdate(dt) {
        if (scanCd > 0) scanCd -= dt;
        var i, w, org;
        if (scanT <= 0) { if (hlN > 0 || cScanOp !== 0) { for (i = 0; i < HL_MAX; i++) if (hlPool[i].on) { hlPool[i].on = false; hlPool[i].el.style.display = 'none'; } hlN = 0; cScanOp = 0; elHls.style.opacity = '0'; } return; }
        scanT -= dt; hlN = 0;
        var op = Math.round(Math.min(1, scanT / 1.2) * 20) / 20;
        if (op !== cScanOp) { cScanOp = op; elHls.style.opacity = String(op); }
        org = ((gmode === 'foot' || gmode === 'sfoot') && hum.obj) ? hum.w : shipRoot.position;
        w = world && world.node ? world : null;
        if (gmode === 'foot') scanGlow(dt, org, w);
        if (w && w.stores) for (i = 0; i < w.stores.length; i++) { var st = w.stores[i]; hlMark(locToWorld(st.pos, vHl2), 'store', String(st.name || 'STORE').toUpperCase(), org); }
        if (ps && ps.outposts) for (i = 0; i < ps.outposts.length; i++) {
            var op0 = ps.outposts[i], pp = posOf(op0), dup = false;
            if (!pp) continue;
            if (w && w.stores) for (var si = 0; si < w.stores.length; si++) { locToWorld(w.stores[si].pos, vHl2); if (vHl2.distanceToSquared(pp) < (10 * L) * (10 * L)) { dup = true; break; } }
            if (!dup) hlMark(pp, 'post', String(op0.name || 'OUTPOST').toUpperCase(), org);
        }
        if (w && typeof w.scanTargets === 'function') { try { var stg = w.scanTargets(), rn2 = 0; for (i = 0; i < stg.length && rn2 < 16; i++) { var tg = stg[i]; if (tg.type !== 'resource') continue; locToWorld(tg.pos, vHl2); if (vHl2.distanceToSquared(org) > (120 * L) * (120 * L)) continue; hlMark(vHl2, 'resource', String(tg.name || tg.kind).toUpperCase(), org); rn2++; } } catch (e0) { /* ignore */ } }
        if (space && typeof space.scanTargets === 'function') {
            try {
                var sgt = space.scanTargets(), sn2 = 0, sr2 = (500 * L) * (500 * L);
                for (i = 0; i < sgt.length && sn2 < 14; i++) {
                    var tg2 = sgt[i]; if (tg2.pos.distanceToSquared(org) > sr2) continue;
                    hlMark(tg2.pos, tg2.kind === 'crate' ? 'shard' : (tg2.kind === 'derelict' ? 'post' : (tg2.kind === 'convoy' ? 'friend' : 'resource')), tg2.kind === 'derelict' ? 'DERELICT' : (tg2.kind === 'crate' ? 'CRATE' : (tg2.kind === 'convoy' ? 'CONVOY' : String(tg2.kind).toUpperCase())), org, tg2.kind === 'convoy' ? 'freighter' : ''); sn2++;
                }
            } catch (e0) { /* ignore */ }
        }
        if (w && w.shards) { var sn = 0; for (i = 0; i < w.shards.length && sn < 14; i++) { var sh = w.shards[i]; if (!sh || sh.taken) continue; hlMark(locToWorld(sh.pos, vHl2), 'shard', 'SHARD', org); sn++; } }
        if (station && station.group.visible) {
            var sst = station.interior.stores[0];
            hlMark(sst ? sst.counter.pos : station.pos, 'store', 'STATION', org);
        }
        if (net && net.ghosts) net.ghosts.forEach(function (g) { if (g.placed && g.root) hlMark((g.mk2 || g.root).position, 'friend', String(g.name || 'PILOT').toUpperCase(), org, 'diamond', hexCss(g.color >= 0 ? g.color : 0x8a5cff)); });
        for (i = 0; i < allies.length; i++) if (allies[i].alive) hlMark(allies[i].g.position, 'friend', 'WINGMAN', org);
        for (i = hlN; i < HL_MAX; i++) if (hlPool[i].on) { hlPool[i].on = false; hlPool[i].el.style.display = 'none'; }
    }
    function clerkName(st) { var c = st && st.clerk; return String((c && typeof c === 'object' ? c.name : c) || st.name || 'CLERK').toUpperCase(); }
    function npcNear() {
        var st = world && world.stores;
        if (!st || gmode !== 'foot' || !hum.obj || !world.node) return null;
        var bd = (2.2 * L) * (2.2 * L), best = null;
        for (var i = 0; i < st.length; i++) {
            if (!st[i] || !st[i].npcSpot) continue;
            locToWorld(st[i].npcSpot.pos, vSc);
            var d = vSc.distanceToSquared(hum.w);
            if (d < bd) { bd = d; best = st[i]; }
        }
        return best;
    }
    function talkNpc(st) {
        if (r28Talk({ id: st.id, name: (st.clerk && st.clerk.name) || clerkName(st), role: 'cashier', kind: 'clerk', obj: st, seed: st.clerk && st.clerk.seed })) return;
        addRow(clerkName(st), '#ffd36a', clerkLine(st) || '...');
        aPlay('npc', { seed: st.id }); openQuests(st.id, clerkName(st), 'clerk');
    }
    // music (audio.music, ship-audio.js): key from the nearest planet's palette hue, intensity from the combat state (0.2 idle / 0.6 enemies near / 0.9 boss)
    var musT = 0, musOn = false, musKey = -1, musInt = -1, musHsl = { h: 0, s: 0, l: 0 };
    function nodeKey(n) {
        if (!n) return 2;
        if (n._mkey === undefined) { try { n.pal.hi.getHSL(musHsl); n._mkey = ((Math.round(musHsl.h * 12) % 12) + 12) % 12; } catch (e) { n._mkey = 2; } }
        return n._mkey;
    }
    function musicTick(dt, bodies) {
        var m = audio && audio.music;
        if (!m) return;
        musT -= dt; if (musT > 0) return; musT = 0.8;
        try {
            if (!musOn) { if (!audio.ready) return; m.start(); musOn = true; musKey = musInt = -1; }
            var it = 0.2, i, nn = null, bd = 1e30;
            if (bossAlive()) it = 0.9;
            else for (i = 0; i < enemies.length; i++) if (enemies[i].alive && enemies[i].g.position.distanceToSquared(shipRoot.position) < (220 * L) * (220 * L)) { it = 0.6; break; }
            if (gmode !== 'fly' && land.node) nn = land.node;
            else if (bodies) for (i = 0; i < bodies.length; i++) { var dd = bodies[i].node.anchor.position.distanceTo(shipRoot.position) - bodies[i].R; if (dd < bd) { bd = dd; nn = bodies[i].node; } }
            var key = nodeKey(nn);
            if (it !== musInt || key !== musKey) { musInt = it; musKey = key; m.set({ key: key, intensity: it, mode: it > 0.8 ? 'aeolian' : (it > 0.5 ? 'dorian' : 'lydian') }); }
        } catch (e) { /* ignore */ }
    }
    function musicStop() { try { if (musOn && audio.music) audio.music.stop(); } catch (e) { /* ignore */ } musOn = false; musT = 0; }
    function hudDmg(dt) {                                // hit marker decay + number positions (real dt)
        if (hmT > 0) hmT = Math.max(0, hmT - dt);
        var hv = hmT > 0 ? (hmCrit ? 2 : 1) : 0;
        if (hv !== cHm) { cHm = hv; elHm.classList.toggle('is-on', hv > 0); elHm.classList.toggle('is-crit', hv === 2); if (hv === 2) elHm.style.color = hexCss(effColor()); else if (hv === 0) elHm.style.color = ''; }
        var W = window.innerWidth, H = window.innerHeight;
        for (var i = 0; i < dnPool.length; i++) {
            var d = dnPool[i];
            if (!d.on) continue;
            d.age += dt;
            if (d.age >= d.life) { d.on = false; d.el.style.display = 'none'; continue; }
            vA.copy(d.p).project(camera);
            if (vA.z >= 1 || vA.z <= -1) { d.el.style.opacity = '0'; continue; }
            var x = (vA.x * 0.5 + 0.5) * W + d.jx, y = (-vA.y * 0.5 + 0.5) * H - d.age * 52;
            d.el.style.transform = 'translate(' + x.toFixed(0) + 'px,' + y.toFixed(0) + 'px) translate(-50%,-50%)';
            d.el.style.opacity = Math.min(1, (d.life - d.age) / 0.35).toFixed(2);
        }
    }
    // chain-reaction death: a run of explosions scattered through the body, count + size scaled by the creature
    var boomQ = [], boomSnd = 0;
    (function buildBooms() { for (var i = 0; i < 56; i++) boomQ.push({ on: false, t: 0, p: new THREE.Vector3(), sz: 1, col: 0xffffff, snd: false }); })();
    function queueBoom(x, y, z, t, sz, col, snd) {
        for (var i = 0; i < boomQ.length; i++) {
            var b = boomQ[i];
            if (b.on) continue;
            b.on = true; b.t = t; b.p.set(x, y, z); b.sz = sz; b.col = col; b.snd = !!snd;
            return;
        }
    }
    function updateBooms(dt) {
        for (var i = 0; i < boomQ.length; i++) {
            var b = boomQ[i];
            if (!b.on) continue;
            b.t -= dt;
            if (b.t > 0) continue;
            b.on = false;
            fx.impact(b.p, b.col, b.sz); fx.flash(b.p, b.col);
            if (b.snd) aPlay('explosion', { dist: aDist(b.p) * 0.3, pitch: 1 / Math.sqrt(Math.max(1, b.sz * 0.4)) });
        }
    }
    function deathBooms(pos, len, R, boss, col) {
        var n = boss ? 18 : clamp(3 + Math.round(clamp(len / L / 6, 1, 4) * 2), 4, 11), spread = boss ? R * 0.7 : len * 0.4, span = boss ? 2 : 0.55;
        var base = boss ? clamp(len / L / 40, 4, 40) : clamp(len / L / 6, 1, 4);
        for (var i = 0; i < n; i++) {
            randDir(vD);
            var r0 = Math.random() * spread, last = i === n - 1, t = i === 0 ? 0 : (i / (n - 1)) * span * (0.7 + Math.random() * 0.3);
            queueBoom(pos.x + vD.x * r0, pos.y + vD.y * r0, pos.z + vD.z * r0, t, base * (last ? 1.7 : 0.6 + 0.5 * Math.random()), last ? 0xffffff : col, last || i % 4 === 0);
        }
    }
    // weapon crates (pooled): a glowing wire box + core + halo; fly through it to equip. Procedural weapon from ship-weapons generateWeapon.
    var crates = [], crateBox = new THREE.BoxGeometry(1, 0.7, 1), crateGlowTex = glowTexture(255, 255, 255);
    (function buildCrates() {
        for (var i = 0; i < CRATE_MAX; i++) {
            var g = new THREE.Group();
            var shell = new THREE.Mesh(crateBox, new THREE.MeshBasicMaterial({ color: 0xffffff, wireframe: true, transparent: true, opacity: 0.95, blending: THREE.AdditiveBlending, depthWrite: false }));
            var core = new THREE.Mesh(crateBox, new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.6, blending: THREE.AdditiveBlending, depthWrite: false }));
            core.scale.setScalar(0.6);
            var gs = glowSprite(crateGlowTex); gs.scale.setScalar(7);
            g.add(shell); g.add(core); g.add(gs);
            g.traverse(function (o) { o.frustumCulled = false; });
            g.visible = false; combatRoot.add(g);
            crates.push({ g: g, shell: shell, core: core, gs: gs, active: false, life: 0, w: null, spin: Math.random() * 6, prev: new THREE.Vector3(), has: false, born: 0 });
        }
    })();
    function curWeapon() { var w = pget(curProfile(), 'weapon', null); return (w && w.stats) ? w : STARTER; }
    function playerDps() { var st = curWeapon().stats; return st.dmg * st.rate * st.count; }
    function spawnCrate(pos, tier) {
        if (!wpnMod) return null;
        var w = null, c = null, i;
        try { w = wpnMod.generateWeapon((Math.random() * 1e9) | 0, tier); } catch (e) { return null; }
        if (!w) return null;
        for (i = 0; i < crates.length; i++) if (!crates[i].active) { c = crates[i]; break; }
        if (!c) { c = crates[0]; for (i = 1; i < crates.length; i++) if (crates[i].born < c.born) c = crates[i]; }
        c.active = true; c.life = CRATE_LIFE; c.w = w; c.has = false; c.born = gt;
        var col = typeof w.color === 'number' ? w.color : 0xffffff;
        c.shell.material.color.setHex(col); c.core.material.color.setHex(col); c.gs.material.color.setHex(col);
        c.g.position.copy(pos); c.g.visible = true; c.g.scale.setScalar(2.6 * L);
        fx.flash(pos, col); fx.impact(pos, col, 2);
        addRow('', '', 'WEAPON CRATE · ' + (wpnMod.weaponLine ? wpnMod.weaponLine(w) : w.name) + ' · FLY THROUGH IT', 'is-sys');
        return c;
    }
    function equipWeapon(w) {
        var pf = ensureProfile(userName());
        if (!curUser) curUser = userName();
        var own = pf.weapons.filter(function (x) { return x.id !== w.id; });
        own.push(w); pf.weapons = own.slice(-24); pf.weapon = w;
        writeSave();
        addRow('', '', 'EQUIPPED ' + w.name, 'is-sys');
        announce('EQUIPPED ' + w.name);
    }
    function updateCrates(dt) {
        var P = shipRoot.position, pr = CRATE_R * L;
        for (var i = 0; i < crates.length; i++) {
            var c = crates[i];
            if (!c.active) continue;
            c.life -= dt;
            if (c.life <= 0) { c.active = false; c.g.visible = false; continue; }
            c.spin += dt;
            c.shell.rotation.y = c.spin * 1.4; c.core.rotation.y = -c.spin * 2; c.core.rotation.x = c.spin * 0.9;
            var pl = 1 + 0.12 * Math.sin(gt * 5 + i);
            c.g.scale.setScalar(2.6 * L * pl);
            c.gs.material.opacity = c.life < 6 ? 0.4 + 0.6 * Math.abs(Math.sin(gt * 9)) : 1;
            var p0 = c.has ? c.prev : P;
            if (!dead && gmode === 'fly' && segDistSq(p0.x, p0.y, p0.z, P.x, P.y, P.z, c.g.position.x, c.g.position.y, c.g.position.z) < pr * pr) {
                c.active = false; c.g.visible = false;
                fx.flash(c.g.position, c.w.color); fx.impact(c.g.position, c.w.color, 2);
                equipWeapon(c.w); aPlay('ui', { vel: 1, pitch: 1.5 }); addUnits(UNIT_CRATE, c.g.position, true);
                continue;
            }
            c.prev.copy(P); c.has = true;
        }
    }
    function killEnemy(e) {
        var wasBoss = e.isBoss, len = e.len, eR = e.R, kids = (!wasBoss && e.cr && e.cr.stats.spawnOnDeath) | 0, title = e.title, col = e.role === 1 ? 2 : 1;
        var wasLead = e.leader, sqId = e.squad, uGain = wasBoss ? UNIT_BOSS * e.tier : UNIT_KILL * e.tier * (e.elite ? 2 : 1) + (e.leader ? UNIT_LEADER : 0) + (e.beh === 'bomber' ? UNIT_KILL * e.tier : 0);
        vKill.copy(e.g.position);
        if (wasLead && sqId) {             // the squad splits: every survivor commits to an attack run at once
            squadLead[sqId] = null;
            for (var si = 0; si < enemies.length; si++) { var m = enemies[si]; if (m !== e && m.alive && m.squad === sqId && m.state === 0 && m.grace <= 0) { m.state = 1; m.rpSet = false; m.near = 0; } }
        }
        hideEnemy(e); kills++;
        addUnits(uGain, vKill, wasBoss || wasLead); r25Scrap(wasBoss ? 3 : 0);
        var ksz = clamp(len / L / 6, 1, 4);
        burst(vKill, wasBoss ? 30 : 14, col, wasBoss ? 30 : 14, ksz);
        deathBooms(vKill, len, eR, wasBoss, BURST_COL[col] || 0xff7a2a);
        aPlay('kill', { dist: aDist(vKill) * (wasBoss ? 0.2 : 1) });
        if (wasBoss) aPlay('bossRoar', { pitch: 0.55, dist: aDist(vKill) * 0.1 });
        if (ctarget === e) { ctarget = null; tlock = false; }
        if (zTarget === e) zTarget = null;
        chainN = chainT > 0 ? chainN + 1 : 1; chainT = CHAIN_T;                // rev 9c: kill chain, 4 s window
        if (!wasBoss) spawnDrop(vKill, DROP_VAL * (chainN >= 3 ? 2 : 1));
        hitStopN = 2;                                                           // rev 9b #5: 40 ms world freeze on a kill
        fovKick = 2;
        if (kids > 0) spawnLarvae(vKill, len, kids);
        if (wasBoss) {
            kills += 4; burst(vKill, 20, 0, 20, ksz);
            for (var oi = 0; oi < orbs.length; oi++) { orbs[oi].active = false; orbs[oi].m.visible = false; }
            // rev 20 loot: a boss carcass can be thousands of L across, so the drops appear up to 60 L ahead of the player toward it
            vLoot.subVectors(vKill, shipRoot.position); var ld = vLoot.length() || 1;
            vLoot.divideScalar(ld).multiplyScalar(clamp(ld - eR, 14 * L, 60 * L)).add(shipRoot.position);
            vB.copy(X).applyQuaternion(shipRoot.quaternion);
            for (var di = 0; di < 3; di++) { vA.copy(vLoot).addScaledVector(vB, (di - 1) * 7 * L); spawnDrop(vA, DROP_VAL * 2); }
            spawnCrate(vLoot, wpnMod ? wpnMod.lootTier(wave, true) : 1);
            announceBoss('BOSS DOWN · ' + title); bossKilled(title);
        } else if (Math.random() < 0.1 && !peaceful) spawnCrate(vKill, wpnMod ? wpnMod.lootTier(wave, false) : 0);
    }
    // rev 9 C: at <= 45 % HP an enemy flees (telegraph -> 3x burst -> regen circle -> return); max FLEE_MAX times
    function beginFlee(e) {
        if (e.stallT > 0) endStall(e);
        e.state = 4; e.fstate = 'tele'; e.ftimer = FLEE_TELE; e.fleeN++; e.charge = 0; e.grace = 0; e.burst = 0;
        e.fp = null; e.skimOn = false; e.skim = 0; e.fdive = 10; e.lphase = 0; e.lt = 1.5;
        var fbd = 400 * L;                                  // rev 12: prefer diving to a planet within 400 L (chases go planetside)
        for (var fj = 0; fj < scratchBodies.length; fj++) {
            var fb = scratchBodies[fj];
            if (fb.node === root) continue;
            var fdd = e.g.position.distanceTo(fb.node.anchor.position) - fb.R;
            if (fdd < fbd) { fbd = fdd; e.fp = fb; }
        }
        if (e.beam) e.beam.visible = false;
    }
    // rev 20: sever a boss limb: hide its parts, kill its capsule + moves (ship-enemies setLimbDestroyed), big burst, 1.5 s stagger
    function severLimb(e, i) {
        var cr = e.cr, lm = cr && cr.limbs && cr.limbs[i], k, left = 0;
        if (!lm || lm.dead || typeof cr.setLimbDestroyed !== 'function') return;
        e.g.updateMatrixWorld(true);
        vSev.copy(lm.capsule.a).add(lm.capsule.b).multiplyScalar(0.5); cr.group.localToWorld(vSev);
        var rad = lm.capsule.r * e.sc, nm = String(lm.name || 'limb');
        cr.setLimbDestroyed(i);
        k = 0; for (left = 0; left < cr.eyes.length; left++) if (!cr.eyes[left].userData.dead) k++;
        e.eyeN = Math.min(E_EYE_MAX, k); left = 0;      // the live eyes sit first in cr.eyes, so eyes[0] never points at a hidden sprite
        for (k = 0; k < cr.limbs.length; k++) if (!cr.limbs[k].dead) left++;
        var sz = clamp(rad / L / 3, 3, 30);
        burst(vSev, 40, 1, 30, 4); fx.flash(vSev, 0xffb060);
        for (k = 0; k < 7; k++) { randDir(vD); queueBoom(vSev.x + vD.x * rad, vSev.y + vD.y * rad, vSev.z + vD.z * rad, k * 0.07, sz * (0.5 + 0.7 * Math.random()), k & 1 ? 0xffb060 : 0xff5a30, k === 0 || k === 6); }
        aPlay('limbSever', { dist: aDist(vSev) * 0.3 });
        e.stagger = STAGGER_T; e.hitT = 1; shake = Math.max(shake, 0.6 * L); fovKick = Math.max(fovKick, 3);
        if (bsig.owner === e) sigEnd(e);
        if (thr.ph !== 'idle' && thr.e === e) thrEnd();
        e.mv = null; e.ms = 'idle'; e.mt = 0; e.mcd = STAGGER_T + 1; e.hitDone = true;
        if (cr.att) { cr.att.ph = 'idle'; cr.att.u = 0; }
        if (!left) { e.noLimbs = true; e.open = true; announceBoss(e.title + ' · CORE EXPOSED'); addRow('', '', nm.toUpperCase() + ' DESTROYED · CORE EXPOSED x4', 'is-sys'); }
        else { announceBoss(e.title + ' · ' + nm.toUpperCase() + ' DESTROYED'); addRow('', '', nm.toUpperCase() + ' DESTROYED · ' + left + ' LEFT', 'is-sys'); }
    }
    // every player-bolt hit lands here (also the debug hook): marker, number, flinch, combo tick, limb hp, flee check, kill. limb = index into cr.limbs or -1
    function damageEnemy(en, dmg, crit, bp, limb, raw) {
        if (en.stallT > 0 && !raw && !en.isBoss) { dmg *= STALL_X; crit = true; }       // rev 21: a STALLED enemy takes x2
        en.hp -= dmg; en.hitT = 1; en.flinch = 0.1;
        comboN++; comboT = 1.5; tickPunch = 1;
        if (bp) burst(bp, crit ? 6 : 3, crit ? 2 : 0, crit ? 12 : 8, clamp(en.len / L / 6, 1, 4));
        hitMark(crit); dmgNumber(dmg, crit, bp || en.g.position);
        aPlay(crit ? 'crit' : 'hit', { dist: aDist(bp || en.g.position) * (en.isBoss ? 0.2 : 1), vel: crit ? 1 : 0.7 });
        if (en.hp <= 0) { killEnemy(en); return true; }
        if (limb >= 0 && en.limbHp && en.cr && en.cr.limbs[limb] && !en.cr.limbs[limb].dead) {
            en.limbHp[limb] -= dmg;
            if (en.limbHp[limb] <= 0) severLimb(en, limb);
        }
        if (en.beh === 'harasser' && en.breakCd <= 0 && en.breakT <= 0 && !en.isBoss) { en.breakT = 1.8; en.breakCd = 4; }       // harassers break off when shot
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
        var bs = E_BOLT_MAX * 0.9 * L;
        vE.copy(e.eyeW[0]);
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
            fireBolt(vE.x, vE.y, vE.z, e.aim.x, e.aim.y, e.aim.z, bs, 320 * L / bs, true, e.dmg, Math.min(e.len * 0.8, 14 * L), Math.min(e.len * 0.05, 1.2 * L), 0xbff4ff);
            e.charge = 0; e.beam.visible = false; e.snCd = 3.5 + Math.random() * 2;
            beginStall(e);
        }
    }
    // lancer: shadows you at 70-160 L, then a 1.1 s telegraph (eye flash + a beam down the line it is about to run; the line LOCKS for the last
    // 0.3 s: that is your window to sidestep), then a straight 1.3 s charge at 4.5x speed, then 1.6 s to turn around. Contact is the ram damage.
    function lancerStep(e, dt, tp, dist) {
        var pos = e.g.position, spd = e.speed, ln;
        if (e.lphase === 0) {
            pathPoint(e, vSl); steer(e, vSl.x, vSl.y, vSl.z, e.turn * 1.4, spd * (e.grace > 0 ? 1.5 : 1.1), dt);
            e.fstate = 'patrol';
            if (!dead && e.grace <= 0 && dist < 160 * L && dist > 40 * L) { e.lt -= dt; if (e.lt <= 0) { e.lphase = 1; e.lt = 1.1; e.charge = 1e-4; } }
        } else if (e.lphase === 1) {
            e.lt -= dt; e.charge = 1.2 - e.lt * 1.1;
            e.fstate = 'charge';
            if (e.lt > 0.3) {
                vW1.copy(tp).addScaledVector(vel, Math.min(1, dist / (spd * 4.5 + 1)) * 0.5);
                e.aim.subVectors(vW1, pos).normalize();
                steer(e, vW1.x, vW1.y, vW1.z, e.turn * 2.2, spd * 0.15, dt);
            } else {
                vM.copy(pos).add(e.aim);
                mM.lookAt(pos, vM, Y); qA.setFromRotationMatrix(mM); e.g.quaternion.rotateTowards(qA, 6 * dt);
            }
            ln = clamp(dist * 1.3, 60 * L, 400 * L);
            var arr = e.beam.geometry.attributes.position.array;
            arr[0] = e.eyeW[0].x; arr[1] = e.eyeW[0].y; arr[2] = e.eyeW[0].z;
            arr[3] = arr[0] + e.aim.x * ln; arr[4] = arr[1] + e.aim.y * ln; arr[5] = arr[2] + e.aim.z * ln;
            e.beam.geometry.attributes.position.needsUpdate = true;
            e.beam.material.opacity = 0.2 + 0.8 * Math.min(1, (1.1 - e.lt) / 0.8);
            e.beam.visible = true;
            if (e.lt <= 0) { e.lphase = 2; e.lt = 1.3; e.beam.visible = false; e.charge = 0; }
        } else if (e.lphase === 2) {
            e.lt -= dt; e.fstate = 'lance';
            e.curSpeed += (spd * 4.5 - e.curSpeed) * damp(8, dt);
            pos.addScaledVector(e.aim, e.curSpeed * dt);
            if (e.lt <= 0) { e.lphase = 3; e.lt = 1.6; beginStall(e); }
        } else {
            e.lt -= dt; e.fstate = 'peel';
            vW1.copy(pos).addScaledVector(e.aim, 60 * L); vW1.y += 25 * L * e.wSide;
            steer(e, vW1.x, vW1.y, vW1.z, e.turn * 2.4, spd * 0.8, dt);
            if (e.lt <= 0) { e.lphase = 0; e.lt = 1.5 + Math.random() * 2; }
        }
    }
    // ─── rev 21: attack-run phases (wind-up -> strike -> STALL), harassers, bombers, lock-on evasion ───────────────────
    function setWind(e, v) {
        var cr = e.cr; if (!cr || typeof cr.setWindup !== 'function') return;
        v = Math.round(clamp(v, 0, 1) * 20) / 20;
        if (v !== e.wv) { e.wv = v; try { cr.setWindup(v); } catch (err) { /* ignore */ } }
    }
    function beginWindup(e, dur) {
        e.ap = 1; e.apT = dur; e.apDur = dur;
        aPlay('windup', { dist: aDist(e.g.position), pitch: e.isBoss ? 0.6 : 1 });
    }
    function beginStall(e) {
        if (e.isBoss || e.stallT > 0 || e.state >= 4) return;
        e.stallT = STALL_T; e.stalled = true; e.ap = 0; e.charge = 0; if (e.beam) e.beam.visible = false;
        setWind(e, 0);
        try { if (e.cr && typeof e.cr.setStalled === 'function') e.cr.setStalled(true); } catch (err) { /* ignore */ }
        aPlay('stall', { dist: aDist(e.g.position) });
    }
    function endStall(e) {
        e.stallT = 0; e.stalled = false;
        try { if (e.cr && typeof e.cr.setStalled === 'function') e.cr.setStalled(false); } catch (err) { /* ignore */ }
    }
    function stallStep(e, dt) {
        e.stallT -= dt;
        e.curSpeed += (0 - e.curSpeed) * damp(3, dt);
        vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
        e.g.position.addScaledVector(vF, e.curSpeed * dt);
        if (e.stallT <= 0) endStall(e);
    }
    // harasser: orbits the target at HARASS_R L, one slow pot shot every fireInt (0.35 s wind-up), breaks off toward cover (a moon / planet) when shot
    function harasserStep(e, dt, tp, dist) {
        var pos = e.g.position, R = HARASS_R * L, spd = e.speed, i, spit = e.role === 1 || e.role === 4;
        e.fstate = e.ap === 1 ? 'charge' : 'patrol';
        e.breakCd -= dt;
        if (e.breakT > 0) {
            e.breakT -= dt; e.ap = 0; setWind(e, 0);
            var cov = null, cd = 400 * L;
            for (i = 0; i < scratchBodies.length; i++) {            // nearest planet / moon within 400 L: hide on its far side from the player
                var cb = scratchBodies[i]; if (cb.node === root) continue;
                var dd = pos.distanceTo(cb.node.anchor.position) - cb.R;
                if (dd < cd) { cd = dd; cov = cb; }
            }
            if (cov) { vW1.subVectors(cov.node.anchor.position, tp).normalize(); vW2.copy(cov.node.anchor.position).addScaledVector(vW1, cov.R * 1.25 + 6 * L); }
            else { vW1.subVectors(pos, tp).normalize(); vW2.copy(pos).addScaledVector(vW1, 60 * L); }
            steer(e, vW2.x, vW2.y, vW2.z, e.turn * 2.2, spd * 1.6, dt);
            e.fireCd = Math.max(e.fireCd, 0.8);
            return;
        }
        if (e.grace > 0 || dist > R * 3.2) { pathPoint(e, vSl); steer(e, vSl.x, vSl.y, vSl.z, e.turn, spd * (e.grace > 0 ? 1.5 : 1.3), dt); }
        else {
            vW1.subVectors(pos, tp); vW1.y *= 0.5; if (vW1.lengthSq() < 1e-6) vW1.set(1, 0, 0); vW1.normalize();
            vW1.applyAxisAngle(Y, e.wSide * 0.9).multiplyScalar(R).add(tp); vW1.y += Math.sin(gt * 0.7 + e.wPh) * 6 * L;
            steer(e, vW1.x, vW1.y, vW1.z, e.turn * 1.4, spd * (1 + clamp((dist - R) / R, -0.4, 0.8)), dt);
        }
        e.fireCd -= dt;
        if (dead || e.grace > 0) { if (e.ap) { e.ap = 0; setWind(e, 0); } return; }
        if (e.ap === 1) {
            e.apT -= dt; setWind(e, 1 - e.apT / e.apDur);
            if (e.apT <= 0) {
                e.ap = 0; setWind(e, 0);
                var bsp = SPIT_SPEED * L, eBolt = clamp(E_BOLT_PER_LEN * e.len / L, E_BOLT_MIN, E_BOLT_MAX) * L;
                vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
                vAim.copy(tp).addScaledVector(vel, (dist / (spit ? bsp : eBolt)) * e.lead * 0.5);
                vD.subVectors(vAim, pos).normalize();
                vD.x += (Math.random() - 0.5) * 2 * e.err * 1.4; vD.y += (Math.random() - 0.5) * 2 * e.err * 1.4; vD.z += (Math.random() - 0.5) * 2 * e.err * 1.4; vD.normalize();
                vTmp.copy(pos).addScaledVector(vF, e.len * 0.5);
                fireBolt(vTmp.x, vTmp.y, vTmp.z, vD.x, vD.y, vD.z, spit ? bsp : eBolt, E_FIRE_PER_LEN * e.len * E_RANGE_X / (spit ? bsp : eBolt), true, e.dmg, Math.min(e.len * 0.6, 10 * L), Math.min(e.len * 0.05, 1 * L), spit ? 0xc8ff3a : null);
                e.fireCd = e.fireInt * (1 + Math.random() * 0.5);
            }
        } else if (e.fireCd <= 0 && dist < R * 2.2) {
            vF.copy(NEG_Z).applyQuaternion(e.g.quaternion); vD.subVectors(tp, pos);
            if (vF.dot(vD) > dist * Math.cos(0.7)) beginWindup(e, HARASS_WIND);
        }
    }
    // bomber: three drifting mines (own pool, 6) dropped on a pass; proximity fuse 1.4 s, 10 L blast
    var bombs = [];
    (function buildBombs() {
        for (var i = 0; i < BOMB_MAX; i++) {
            var m = new THREE.Mesh(orbGeom, mineMat); m.visible = false; m.frustumCulled = false;
            var sh = new THREE.Mesh(orbGeom, fuseMat); sh.visible = false; sh.scale.setScalar(1.7); m.add(sh);
            combatRoot.add(m);
            bombs.push({ m: m, shell: sh, vel: new THREE.Vector3(), fuse: -1, life: 0, active: false });
        }
    })();
    function dropBombs(e) {
        var k = 0, i;
        vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
        for (i = 0; i < bombs.length && k < 3; i++) {
            var b = bombs[i]; if (b.active) continue;
            b.active = true; b.fuse = -1; b.life = 14; k++;
            b.m.position.copy(e.g.position).addScaledVector(vF, -e.len * (0.4 + 0.25 * k));
            randDir(vD); b.vel.copy(vD).multiplyScalar(2.5 * L).addScaledVector(vF, e.curSpeed * 0.25);
            b.m.scale.setScalar(L * 1.2); b.m.visible = true; b.shell.visible = false;
        }
        if (k) { fx.flash(e.g.position, 0xffa020); aPlay('hit', { dist: aDist(e.g.position), pitch: 0.5, vel: 0.5 }); }
    }
    function updateBombs(dt) {
        var P = shipRoot.position;
        for (var i = 0; i < bombs.length; i++) {
            var b = bombs[i];
            if (!b.active) continue;
            b.life -= dt; b.m.position.addScaledVector(b.vel, dt); b.vel.multiplyScalar(Math.max(0, 1 - 0.6 * dt));
            var d2 = b.m.position.distanceToSquared(P);
            if (b.fuse < 0) {
                b.m.scale.setScalar(L * (1.2 + 0.14 * Math.sin(gt * 4 + i)));
                if (!dead && d2 < (7 * L) * (7 * L)) { b.fuse = 1.4; b.shell.visible = true; }
                else if (b.life <= 0) { b.active = false; b.m.visible = false; }
            } else {
                b.fuse -= dt;
                b.m.scale.setScalar(L * (1.2 + 0.4 * Math.abs(Math.sin(gt * (8 + 10 * (1 - b.fuse / 1.4))))));
                if (b.fuse <= 0) {
                    b.active = false; b.m.visible = false; b.shell.visible = false;
                    burst(b.m.position, 20, 1, 20, 3); fx.flash(b.m.position, 0xff7a2a); aPlay('explosion', { dist: aDist(b.m.position) });
                    if (!dead && d2 < (10 * L) * (10 * L)) hurtPlayer(16 * DIFFS[difficulty].dmg, undefined, 1.6);
                }
            }
        }
    }
    // lock-on evasion: a hostile kept inside the 2.5 deg cone for > 1 s jinks once (a 0.45 s dodge roll, 16 L/s sideways), then 4 s cooldown
    function lockScan(dt) {
        for (var i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive || e.isBoss) continue;
            if (e.jinkT > 0) {
                e.jinkT -= dt; var u = 1 - Math.max(0, e.jinkT) / JINK_T;
                e.g.position.addScaledVector(e.jinkDir, JINK_V * L * Math.sin(3.1416 * u) * dt * 1.5708);
                e.jinkRoll = 6.2832 * u * e.wSide;
                if (e.jinkT <= 0) e.jinkRoll = 0;
                continue;
            }
            if (e.jinkCd > 0) e.jinkCd -= dt;
            if (e.state >= 4 || e.stallT > 0 || dead || e.jinkCd > 0) { e.lockT = 0; continue; }
            vW1.subVectors(e.g.position, shipRoot.position);
            var d = vW1.length();
            if (d < 220 * L && d > 1e-6 && noseAngle(e) < AIM_CONE) {
                e.lockT += dt;
                if (e.lockT > LOCK_JINK_T) {
                    e.lockT = 0; e.jinkT = JINK_T; e.jinkCd = JINK_CD;
                    vW2.copy(vW1).divideScalar(d); vW3.crossVectors(vW2, Y); if (vW3.lengthSq() < 1e-4) vW3.copy(X); vW3.normalize();
                    e.jinkDir.copy(vW3).multiplyScalar(e.wSide).addScaledVector(Y, (Math.random() - 0.5) * 0.6).normalize();
                }
            } else e.lockT = Math.max(0, e.lockT - dt * 2);
        }
    }
    function updateEnemy(e, dt, bodies) {
        var P = shipRoot.position, pos = e.g.position, tp = P, tvv = vel, hunted = false, k;
        if (e.hunt) { if (e.hunt.alive) { tp = e.hunt.g.position; hunted = true; } else e.hunt = null; }
        vD.subVectors(tp, pos);
        var dist = vD.length();
        var aggro = AGGRO_L * L, eBolt = clamp(E_BOLT_PER_LEN * e.len / L, E_BOLT_MIN, E_BOLT_MAX), fireR = E_FIRE_PER_LEN * e.len;
        if (hunted) tvv = vSv.copy(NEG_Z).applyQuaternion(e.hunt.g.quaternion).multiplyScalar(e.hunt.curSpeed);
        if (pos.distanceToSquared(P) > (1200 * L) * (1200 * L)) { hideEnemy(e); return; }     // wandered off, recycle (hideEnemy disposes the creature)
        e.slantT -= dt; if (e.slantT <= 0) { e.wSide = -e.wSide; e.slantT = 3 + Math.random() * 2; }
        planBase(e, tp, dist, dt);
        if (e.grace > 0) e.grace -= dt;
        var tr = e.turn, saveD = dist, spit = e.role === 1 || e.role === 4;
        if (e.stallT > 0) {                                   // rev 21: STALLED after an attack run: drifts to a stop, glowing, x2 damage
            stallStep(e, dt); e.fstate = 'stall';
        } else if (e.state >= 4) {
            fleeStep(e, dt, tp, dist);
        } else if (e.role === 2) {
            sniperStep(e, dt, tp, dist);
        } else if (e.role === 3) {
            lancerStep(e, dt, tp, dist);
        } else if (e.beh === 'harasser') {
            harasserStep(e, dt, tp, dist);
        } else {
            e.fstate = FSTATE[e.state] || 'patrol';
            var ldr = leaderOf(e);
            if (e.state === 0) {
                // rev 21: a squad member engages on its leader's call (the leader dives) or when the player is close; until then it holds a loose formation on the leader
                var engage = !holdFire && (ldr ? ((ldr.state >= 1 && ldr.state < 4) || dist < aggro * 0.6) : dist < aggro);
                if (!dead && e.grace <= 0 && engage) { e.state = 1; e.rpSet = false; e.near = 0; e.ap = 0; if (!ldr && e.leader) aPlay('windup', { dist: aDist(pos), pitch: 0.5, vel: 0.5 }); }
                // "patrol" = shadow the player: close to the edge of aggro at full speed so every wave finds you, then loiter on a
                // wander point near the player. Idle guard: nobody within 80 L for 15 s -> push straight in (never dead air).
                e.timer -= dt;
                if (e.timer <= 0 || pos.distanceToSquared(e.wander) < (18 * L) * (18 * L)) pickWander(e, bodies);
                if (dist > 80 * L) e.idle += dt; else e.idle = 0;
                if (ldr && e.grace <= 0 && dist > aggro * 0.45) {
                    vSl.copy(e.fOff).applyQuaternion(ldr.g.quaternion).add(ldr.g.position);
                    steer(e, vSl.x, vSl.y, vSl.z, tr * 1.2, e.speed * clamp(pos.distanceTo(vSl) / (18 * L), 0.5, 1.7), dt);
                } else if (e.grace > 0 || dist > aggro * 1.6 || e.idle > 15) { pathPoint(e, vSl); steer(e, vSl.x, vSl.y, vSl.z, tr, e.speed * (e.grace > 0 ? 1.5 : 1), dt); }
                else steer(e, e.wander.x, e.wander.y, e.wander.z, tr * 0.6, e.speed * 0.5, dt);
            } else if (dead || dist > aggro * 1.5) {
                e.state = 0;
            } else {
                if (tvv.lengthSq() > 0.01) vR3.copy(tvv).normalize(); else vR3.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                vTmp.copy(vR3);                                        // target forward (runStep clobbers vR3)
                pathPoint(e, vSl);
                var s0 = e.state, spRun = e.speed;
                if (e.ap === 1) { spRun = e.speed * 0.35; e.apT -= dt; setWind(e, 1 - e.apT / e.apDur); if (e.apT <= 0) { e.ap = 2; setWind(e, 0); } }       // wind-up: crawl, eye flare, rising tone
                runStep(e, vSl, vTmp, tvv, 0, DIVE_CLOSE * L, RUN_NEAR * L, spRun, tr, e.dive, e.lead, dt);
                if (s0 !== 2 && e.state === 2 && e.ap === 0) beginWindup(e, WINDUP_T);
                else if (s0 === 2 && e.state === 3) { if (e.ap === 2) beginStall(e); else { e.ap = 0; setWind(e, 0); } }                                  // the run is over: strike spent -> STALL
                if (e.state === 1) e.bombed = false;
                if (e.state !== 2 && e.ap && e.stallT <= 0) { e.ap = 0; setWind(e, 0); }
            }
            // fire during the STRIKE only (after the wind-up), inside the (widened) cone and range; interceptors in 3-shot bursts, spitters one slow orb. Bombers drop mines instead.
            e.fireCd -= dt;
            if (e.beh === 'bomber') {
                if (!dead && e.ap === 2 && !e.bombed && saveD < 26 * L) { e.bombed = true; dropBombs(e); }
            } else if (!dead && e.grace <= 0 && e.ap === 2 && e.state === 2 && saveD < fireR && e.fireCd <= 0) {
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
                    fireBolt(vTmp.x, vTmp.y, vTmp.z, vD.x, vD.y, vD.z, bsp, fireR * E_RANGE_X / bsp, true, e.dmg, Math.min(e.len * 0.6, 10 * L), Math.min(e.len * 0.05, 1 * L), spit ? 0xc8ff3a : null);
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
        e.body.rotation.z = Math.sin(wt) * 0.14 + e.jinkRoll;
        e.body.rotation.y = Math.sin(wt * 0.7 + 1) * 0.1;
        e.body.rotation.x = Math.sin(wt * 0.5 + 2) * 0.05;
        e.body.position.y = Math.sin(wt * 1.3) * 0.03;
        var gk;
        if (e.isBoss) gk = e.open ? 0.75 + 0.1 * Math.sin(gt * 14) : Math.max(0.1, 0.7 * e.sigGlow);            // the eye only glows while exposed (or charging a signature attack)
        else if (e.stallT > 0) gk = 0.95;
        else if (e.state === 4) gk = 0.85;                                           // flee telegraph: eye-core flash
        else if (e.charge > 0) gk = 0.2 + 0.7 * Math.min(1, e.charge / SNIPER_CHARGE);   // sniper / lancer charge: growing glow
        else gk = 0.2 + 0.05 * Math.sin(gt * 6 + e.phase) + 0.05 * (e.curSpeed / (e.speed + 1));
        if (!e.isBoss) {                                                 // rev 21: sniper / lancer charge doubles as the wind-up flare + tone
            if (e.charge > 0) { if (!e.chargeSnd) { e.chargeSnd = true; aPlay('windup', { dist: aDist(e.g.position), pitch: e.role === 3 ? 0.8 : 1.15 }); } if (e.ap === 0) setWind(e, e.charge / SNIPER_CHARGE); }
            else { e.chargeSnd = false; if (e.ap === 0 && e.wv) setWind(e, 0); }
        }
        var cr = e.cr;
        if (cr) {
            if (e.lod === 0 || e.isBoss || (e.lodT ^= 1)) cr.update(gt, dt);      // rev 17 LOD: far creatures animate every other frame (the shader keeps moving)
            cr.eye.material.opacity = (e.isBoss && !e.open) ? 0.35 : clamp(0.55 + gk * 0.6, 0, 1);
        }
        var u = e.mat ? e.mat.uniforms : null;
        if (u) u.uTime.value = gt;
        var tint = 0;
        if (e.hitT > 0) { e.hitT = Math.max(0, e.hitT - dt * 5); tint = e.hitT * 0.6; }
        if (e.state === 4) tint = Math.max(tint, 0.5 + 0.4 * Math.sin(gt * 40));
        if (e.pflash > 0) tint = Math.max(tint, e.pflash * 0.8);
        if (e.sigGlow > 0) tint = Math.max(tint, e.sigGlow * (0.25 + 0.2 * Math.sin(gt * 18)));
        if (e.role === 3 && e.lphase === 1) tint = Math.max(tint, 0.3 + 0.4 * Math.sin(gt * 30));     // lancer telegraph
        if (u && u.uHit.value !== tint) u.uHit.value = tint;
        if (e.flinch > 0) { e.flinch = Math.max(0, e.flinch - dt); e.body.scale.setScalar(1 + 0.18 * (e.flinch / 0.1)); }      // 0.1 s scale punch
        else if (e.body.scale.x !== 1) e.body.scale.setScalar(1);
        refreshEyes(e);
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
    // ─── rev 12/13: boss signature attacks. bossAttacksFor(seed, tier) gives each boss an ordered list; ONE attack runs at a time across all
    // bosses (bsig.owner holds the token, so pooled beam / orbs / mines never collide). Telegraph (HUD names it on the boss bar) -> act -> cooldown.
    // idx: 0 beam sweep, 1 orb ring, 2 ram charge, 3 mine field, 4 gravity pull, 5 gravity well (titan), 6 planet ram (titan), 7 terrain beam (titan)
    var bsig = { idx: 0, ph: 'idle', t: 0, dur: 2, tele: 1.5, dmg: 20, owner: null, tgt: null, dir: new THREE.Vector3(), ax: new THREE.Vector3(), org: new THREE.Vector3(), u: new THREE.Vector3(), v: new THREE.Vector3(), hit: false, a: 0, b: 0 };
    var vG1 = new THREE.Vector3(), vG2 = new THREE.Vector3(), vG3 = new THREE.Vector3(), qG = new THREE.Quaternion(), BEAM_LEN = 320;
    var lastRam = null;
    function bossEye(e, out) { return out.copy(e.eyeW[0]); }
    function setBeam(from, dir, len, rad, op) {
        sigBeam.position.copy(from).addScaledVector(dir, len * 0.5);
        sigBeam.quaternion.setFromUnitVectors(Y, dir);
        sigBeam.scale.set(rad, len, rad);
        sigBeam.material.opacity = op; sigBeam.visible = true;
    }
    function sigReset() {
        bsig.ph = 'idle'; bsig.hit = false; bsig.tgt = null;
        sigBeam.visible = false;
        for (var i = 0; i < sigOrbs.length; i++) sigOrbs[i].visible = false;
        for (i = 0; i < mines.length; i++) { mines[i].active = false; mines[i].m.visible = false; mines[i].shell.visible = false; }
        if (bsig.owner) bsig.owner.sigGlow = 0;
        bsig.owner = null;
    }
    function sigEnd(e) {
        sigReset();
        e.sigCd = (e.kind === 3 ? 6 + Math.random() * 3 : 9 + Math.random() * 5) * (e.bphase >= 3 ? 0.65 : (e.bphase === 2 ? 0.8 : 1));
    }
    function sigHurt(e, dmg) { bsig.hit = true; bsig.hits = (bsig.hits | 0) + 1; bsig.lastDmg = dmg * DIFFS[difficulty].dmg; hurtPlayer(dmg * DIFFS[difficulty].dmg, undefined, 2.5); }
    // titan planet ram: nearest planet to the titan that is not the root star
    function ramTarget(e) {
        var best = null, bd = 1e30;
        for (var i = 0; i < scratchBodies.length; i++) {
            var b = scratchBodies[i];
            if (b.node === root || !b.node.parentNode) continue;
            var d = e.g.position.distanceToSquared(b.node.anchor.position);
            if (d < bd) { bd = d; best = b; }
        }
        return best;
    }
    function nudgeNearby(e, dir, amt, within) {              // gravity well: planets near the titan lean toward it
        for (var i = 0; i < scratchBodies.length; i++) {
            var b = scratchBodies[i], c = b.node.anchor.position;
            if (b.node === root || !b.node.parentNode) continue;
            vG3.subVectors(e.g.position, c);
            var d = vG3.length();
            if (d > within || d < 1e-3) continue;
            vG3.multiplyScalar(amt * (1 - d / within) / d);
            engine.nudgeBody(b.node, vG3.x, vG3.y, vG3.z);
        }
    }
    function sigBegin(e) {                                                     // telegraph -> act
        var P = shipRoot.position, i, k, d;
        bossEye(e, vG1);
        bsig.ph = 'act'; bsig.hit = false;
        if (bsig.idx === 0 || bsig.idx === 7) {
            bsig.dur = bsig.idx === 7 ? 2.6 : 2; bsig.t = bsig.dur; bsig.org.copy(vG1);       // beam origin + plane lock to the eye RIGHT NOW (the carapace keeps drifting)
            bsig.dir.subVectors(P, vG1).normalize(); bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize();
            if (bsig.idx === 7 && ps && ps.active) {                           // carve a glowing scar across the terrain under the player
                try {
                    var fo = ps.floorAt(P, fOut);
                    if (fo && fo.r > 0) {
                        var pc = ps.active.anchor.position;
                        vG2.subVectors(P, pc).normalize();
                        vG3.copy(bsig.dir).addScaledVector(vG2, -bsig.dir.dot(vG2));
                        if (vG3.lengthSq() < 1e-6) vG3.crossVectors(vG2, Y);
                        vG3.normalize();
                        vG2.multiplyScalar(fo.r).add(pc);
                        ps.scar(vG2, vG3, 0.3 * realRadius(ps.active), 20);
                    }
                } catch (err) { planetFail(err); }
            }
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
            bsig.a = clamp(d / 2.2, 3 * BOOST, 12 * BOOST);                              // charge speed (3x boost minimum)
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
        } else if (bsig.idx === 4) {
            bsig.t = 3; bsig.a = 0;
            spawnAdds(2, e);                                                      // escorts fire while you are held
        } else if (bsig.idx === 5) {
            bsig.t = 4.5; bsig.a = 0;                                             // gravity well: ramps up, pulls you and the planets near the titan
        } else {                                                                  // planet ram: charge the telegraphed planet
            var tg = bsig.tgt;
            if (!tg) { bsig.t = 0; return; }
            vG2.subVectors(tg.node.anchor.position, e.g.position); d = vG2.length();
            bsig.a = Math.max(0, d - e.R - tg.R * 0.9) / 3; bsig.t = 3.4;
        }
    }
    function bossSig(e, dt, dist) {
        var P = shipRoot.position, i, own = bsig.owner === e, ph = own ? bsig.ph : 'idle', idx = bsig.idx, tl, at;
        if (ph === 'idle') {
            e.sigGlow = Math.max(0, e.sigGlow - dt * 3);
            e.sigCd -= dt;
            if (e.sigCd <= 0 && !bsig.owner && !dead && dist < 320 * L + e.len && e.wind <= 0 && e.pflash <= 0 && e.state === 1) {
                if (e.nextAtk >= 0) { at = { id: ATK[e.nextAtk], tele: e.kind === 3 ? 3 : 1.5, dmg: 30 }; e.nextAtk = -1; }
                else at = e.atk[e.atkI++ % e.atk.length];
                idx = ATK.indexOf(at.id); if (idx < 0) idx = 0;
                bsig.owner = e; bsig.idx = idx; bsig.tele = at.tele; bsig.dmg = Math.min(ATK_DMG_CAP, at.dmg); bsig.tgt = null;
                bsig.ph = 'tele'; bsig.t = at.tele; bsig.hit = false;
                if (idx === 6) { bsig.tgt = ramTarget(e); if (!bsig.tgt) bsig.idx = idx = 5; }
                bossEye(e, vG1);
                bsig.dir.subVectors(P, vG1).normalize();                          // beam: lock the plane now
                bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize();
                announceBoss(e.title + ' · ' + ATK_NAME[idx]);
                aPlay('bossRoar', { dist: Math.min(aDist(e.g.position), 400) * 0.1, pitch: 0.8 });
            }
            return;
        }
        if (dead) { sigEnd(e); return; }
        bossEye(e, vG1);
        if (ph === 'tele') {
            bsig.t -= dt; e.sigGlow = Math.min(1, e.sigGlow + dt * 3);
            var big = idx === 7 ? 6 : 1;
            if ((idx === 0 || idx === 7) && bsig.t > 0.35) { bsig.dir.subVectors(P, vG1).normalize(); bsig.ax.crossVectors(bsig.dir, Y); if (bsig.ax.lengthSq() < 1e-4) bsig.ax.copy(X); bsig.ax.normalize(); }
            if (idx === 0 || idx === 7) { qG.setFromAxisAngle(bsig.ax, -0.85); vG2.copy(bsig.dir).applyQuaternion(qG); setBeam(vG1, vG2, Math.max(BEAM_LEN * L, idx === 7 ? dist * 1.6 : 0), 0.12 * L * big, 0.35 + 0.35 * Math.sin(gt * 30)); }
            else if (idx === 2) { vG2.subVectors(P, e.g.position).normalize(); bsig.dir.copy(vG2); setBeam(e.g.position, vG2, 220 * L, 0.12 * L, 0.35 + 0.35 * Math.sin(gt * 30)); }
            else if (idx === 6 && bsig.tgt) {                                    // titan turns toward the planet and lights a line to its centre
                vG2.subVectors(bsig.tgt.node.anchor.position, vG1); var rd = vG2.length() || 1; vG2.divideScalar(rd);
                setBeam(vG1, vG2, rd, 3 * L, 0.3 + 0.3 * Math.sin(gt * 20));
                mM.lookAt(e.g.position, vG3.copy(e.g.position).add(vG2), Y); qA.setFromRotationMatrix(mM); e.g.quaternion.rotateTowards(qA, 0.5 * dt);
            }
            else if (idx === 5) { vG2.subVectors(e.g.position, P).normalize(); setBeam(P, vG2, Math.min(dist, 600 * L), 0.2 * L, 0.2 + 0.2 * Math.sin(gt * 14)); }
            if (bsig.t <= 0) { sigBeam.visible = false; sigBegin(e); }
            return;
        }
        // act
        bsig.t -= dt; e.sigGlow = 0.6;
        if (idx === 0 || idx === 7) {                                             // beam sweep: ~2 s rotation through ~98 deg (terrain beam: wider, longer)
            var bg = idx === 7 ? 6 : 1, blen = Math.max(BEAM_LEN * L, idx === 7 ? bsig.org.distanceTo(P) * 1.6 : 0);
            tl = 1 - bsig.t / bsig.dur;
            qG.setFromAxisAngle(bsig.ax, -0.85 + 1.7 * clamp(tl, 0, 1)); vG2.copy(bsig.dir).applyQuaternion(qG);
            vE.copy(bsig.org);
            setBeam(vE, vG2, blen, 0.7 * L * bg, 0.9);
            vG1.copy(vE).addScaledVector(vG2, blen);
            if (!bsig.hit && segDistSq(vE.x, vE.y, vE.z, vG1.x, vG1.y, vG1.z, P.x, P.y, P.z) < (2 * L * bg) * (2 * L * bg)) sigHurt(e, bsig.dmg);
            shake = Math.max(shake, (idx === 7 ? 0.3 : 0.12) * L);
        } else if (idx === 1) {                                                   // orb ring: 12 orbs expand then contract onto the locked point
            var u = bsig.t > 0 ? (bsig.b * 1.25 - bsig.t) / bsig.b : 1.25, rad = L * (4.5 + 13 * Math.sin(Math.PI * clamp(u, 0, 1)));
            vG2.copy(bsig.org).addScaledVector(bsig.dir, bsig.a * clamp(u, 0, 1.25));
            for (i = 0; i < 12; i++) {
                var phi = i / 12 * 6.2832 + gt * 0.8, o = sigOrbs[i];
                o.position.copy(vG2).addScaledVector(bsig.u, Math.cos(phi) * rad).addScaledVector(bsig.v, Math.sin(phi) * rad);
                o.scale.setScalar(1.2 * L * (1 + 0.15 * Math.sin(gt * 9 + i))); o.visible = true;
                if (!bsig.hit && o.position.distanceToSquared(P) < (1.9 * L) * (1.9 * L)) { sigHurt(e, bsig.dmg); burst(o.position, 12, 1, 14); break; }
            }
        } else if (idx === 2) {                                                   // ram charge: 3 s at speed, along the locked line (collision pays the attack's damage)
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
                        burst(mn.m.position, 20, 1, 20, 3); fx.flash(mn.m.position, 0xff7a2a); aPlay('explosion', { dist: aDist(mn.m.position) });
                        if (!dead && !bsig.hit && md2 < (10 * L) * (10 * L)) sigHurt(e, bsig.dmg);
                    }
                }
            }
            if (!live) bsig.t = 0;
        } else if (idx === 4) {                                                   // gravity pull: 3 s drag toward the eye; boost straight out to escape
            bsig.a = Math.min(1, bsig.a + dt * 2);
            vG2.subVectors(vG1, P); var gd = vG2.length() || 1; vG2.divideScalar(gd);
            var surf = e.g.position.distanceTo(P) - e.R;
            if (surf > 12 * L) P.addScaledVector(vG2, 30 * L * bsig.a * dt);
            setBeam(P, vG2, gd, 0.18 * L, 0.3 + 0.2 * Math.sin(gt * 10));
            shake = Math.max(shake, 0.08 * L); fovKick = Math.max(fovKick, 1.5);
            if (bsig.t <= 0 && !bsig.hit && surf < 45 * L) sigHurt(e, bsig.dmg);
        } else if (idx === 5) {                                                   // gravity well (titan): pulls YOU at ~boost speed and leans nearby planets toward it
            bsig.a = Math.min(1, bsig.a + dt * 1.5);
            vG2.subVectors(e.g.position, P); var wd = vG2.length() || 1; vG2.divideScalar(wd);
            var wsurf = wd - e.R;
            if (wsurf > 0.15 * e.len) P.addScaledVector(vG2, BOOST * bsig.a * dt);
            nudgeNearby(e, vG2, 0.03 * refR * bsig.a * dt, 6 * refR);
            setBeam(P, vG2, Math.min(wd, 600 * L), 0.25 * L, 0.25 + 0.2 * Math.sin(gt * 10));
            shake = Math.max(shake, 0.12 * L * bsig.a); fovKick = Math.max(fovKick, 2 * bsig.a);
            if (bsig.t <= 0 && !bsig.hit && wsurf < 0.3 * e.len) sigHurt(e, bsig.dmg);
        } else {                                                                  // planet ram (titan): charge the planet, then nudge it ~0.3 refR along the charge
            var tg = bsig.tgt;
            if (tg && !bsig.hit) {
                vG2.subVectors(tg.node.anchor.position, e.g.position); var pdd = vG2.length() || 1; vG2.divideScalar(pdd);
                e.g.position.addScaledVector(vG2, bsig.a * dt);
                mM.lookAt(e.g.position, vG3.copy(e.g.position).add(vG2), Y); qA.setFromRotationMatrix(mM); e.g.quaternion.rotateTowards(qA, 0.8 * dt);
                shake = Math.max(shake, 0.15 * L);
                if (pdd < e.R + tg.R * 0.95) {
                    var pcn = tg.node.anchor.position;
                    lastRam = { id: tg.node.id, before: [pcn.x, pcn.y, pcn.z], push: [vG2.x * 0.3 * refR, vG2.y * 0.3 * refR, vG2.z * 0.3 * refR], t: gt };
                    engine.nudgeBody(tg.node, vG2.x * 0.3 * refR, vG2.y * 0.3 * refR, vG2.z * 0.3 * refR);
                    bsig.hit = true; bsig.t = Math.min(bsig.t, 0.6);
                    shake = Math.max(shake, 2.5 * L); fovKick = 6;
                    vG3.copy(pcn).addScaledVector(vG2, -tg.R); burst(vG3, 30, 1, 30, 4); fx.flash(vG3, 0xff7a2a);
                    if (!dead && P.distanceTo(pcn) < tg.R * 1.6) sigHurt(e, bsig.dmg);        // shockwave
                }
            }
        }
        if (bsig.t <= 0) sigEnd(e);
    }
    // ─── rev 17: bosses fight with their bodies. They never shoot. State machine per boss: idle -> turn -> tele -> strike -> recover -> idle.
    // The generator's seeded combo (cr.moves: sweep / slam / lunge / whip / spin) drives cr.att; the generator poses each limb from the SAME
    // uniform arrays the shader reads and updates a hit capsule per limb. Contact on a strike = 25-45 HP, knockback 6 L, one hit per strike.
    var meleeLog = [], meleeUntil = 0, vC1 = new THREE.Vector3(), vC2 = new THREE.Vector3();
    var thr = { ph: 'idle', e: null, node: null, rec: null, t: 0, dir: new THREE.Vector3(), s0: new THREE.Vector3(), stop: 0, gc: null, force: false, d0: 0, peak: 0, offs: [], logT: 0, lastHit: false };
    function bossFace(e, tx, ty, tz, away, rate, dt) {      // rotate toward (or away from) a point; returns the remaining angle
        var pos = e.g.position;
        if (away) mM.lookAt(pos, vC1.set(2 * pos.x - tx, 2 * pos.y - ty, 2 * pos.z - tz), Y); else mM.lookAt(pos, vC1.set(tx, ty, tz), Y);
        qA.setFromRotationMatrix(mM);
        var ang = e.g.quaternion.angleTo(qA);
        e.g.quaternion.rotateTowards(qA, rate * dt);
        return ang;
    }
    function spawnEscorts(e) {
        var alive = 0, i, n = 2 + ((Math.random() * 3) | 0), avail = wave >= 5 ? 4 : (wave >= 3 ? 3 : (wave >= 2 ? 2 : 1));
        for (i = 0; i < enemies.length; i++) if (enemies[i].alive && !enemies[i].isBoss) alive++;
        for (i = 0; i < n && alive < 9; i++) {
            var f = freeSlot();
            if (!f) return;
            randDir(vD); vAim.copy(e.g.position).addScaledVector(vD, e.len * 0.55 + 15 * L);
            initEnemy(f, (Math.random() * avail) | 0, Math.max(1, regTier(wave)), (Math.random() * 1e9) | 0, 0, 1.0, '');
            alive++;
        }
        announceBoss(e.title + ' · ESCORTS');
    }
    function bossHit(e, mv, lm) {
        var P = shipRoot.position, cp = lm.capsule;
        vC1.copy(cp.a); e.cr.group.localToWorld(vC1); vC2.copy(cp.b); e.cr.group.localToWorld(vC2);
        vB.subVectors(vC2, vC1); var l2 = vB.lengthSq() || 1, t = clamp(vA.subVectors(P, vC1).dot(vB) / l2, 0, 1);
        vC1.addScaledVector(vB, t);                                       // closest point on the limb
        vD.subVectors(P, vC1); var dl = vD.length(); if (dl < 1e-4) vD.copy(vF).negate(); else vD.divideScalar(dl);
        e.hitDone = true; meleeUntil = gt + 0.5; if (meleeLog.length < 40) meleeLog.push({ gt: +gt.toFixed(2), move: mv.name, dmg: +(mv.dmg * DIFFS[difficulty].dmg).toFixed(1), hp: +hp.toFixed(1) });
        hurtPlayer(mv.dmg * DIFFS[difficulty].dmg, undefined, 2.5);
        ramFlash = 0.54; ramIsHit = true;
        bounceV.addScaledVector(vD, 15 * L);                              // total drift = v / 2.5 = 6 L
        vel.multiplyScalar(0.3); speed = Math.min(speed, 0.3 * CRUISE);
        burst(P, 10, 1, 14, 2); fx.flash(P, 0xff6040);
    }
    function bossContact(e) {
        var cr = e.cr, mv = e.mv, P = shipRoot.position, i;
        if (!cr || !cr.limbs || !mv || e.hitDone || dead || gt < meleeUntil) return;
        for (i = 0; i < cr.limbs.length; i++) {
            var lm = cr.limbs[i];
            if (lm.dead) continue;
            if (mv.limb >= 0 ? lm.id !== mv.limb : !(lm.atk.sweep || lm.atk.whip)) continue;
            vC1.copy(lm.capsule.a); cr.group.localToWorld(vC1); vC2.copy(lm.capsule.b); cr.group.localToWorld(vC2);
            var rad = lm.capsule.r * e.sc + 0.6 * L;
            if (segDistSq(vC1.x, vC1.y, vC1.z, vC2.x, vC2.y, vC2.z, P.x, P.y, P.z) < rad * rad) { bossHit(e, mv, lm); return; }
        }
    }
    function startThrow(e, bodies, force) {
        var best = null, bd = force ? 1e30 : 600 * L, i, P = shipRoot.position;
        for (i = 0; i < bodies.length; i++) {
            var b = bodies[i];
            if (b.node === root || !b.node.parentNode || !b.node.anchor) continue;
            var gap = e.g.position.distanceTo(b.node.anchor.position) - e.R - b.R;
            if (gap < bd) { bd = gap; best = b; }
        }
        if (!best) return false;
        thr.ph = 'tele'; thr.e = e; thr.rec = best; thr.node = best.node; thr.t = 0; thr.force = !!force; thr.gc = null; thr.lastHit = false;
        e.ms = 'throw'; e.mt = 0; e.open = true; e.hitDone = true;
        e.cr.att.type = 'spin'; e.cr.att.limb = -1; e.cr.att.ph = 'tele'; e.cr.att.u = 0; e.cr.att.side = 1;
        announceBoss(e.title + ' · PLANET THROW');
        aPlay('bossRoar', { dist: 4, pitch: 0.5 });
        return true;
    }
    function thrGlow(k) {
        var gs = thr.node && thr.node.glowSprite;
        if (!gs || !gs.material || !gs.material.color) return;
        if (!thr.gc) thr.gc = gs.material.color.clone();
        gs.material.color.copy(thr.gc).multiplyScalar(k);
    }
    function thrEnd() { thrGlow(1); thr.ph = 'idle'; thr.e = null; thr.gc = null; if (elIncoming) elIncoming.classList.remove('is-on'); }
    // drives the engine's decaying offset fields (node.nx/ny/nz) so the planet follows the throw line; when it stops writing the engine's decay is the way home
    function updateThrow(dt) {
        if (thr.ph === 'idle') return;
        var n = thr.node, P = shipRoot.position, ap;
        if (!n || !n.anchor) { thrEnd(); return; }
        ap = n.anchor.position;
        if (thr.ph === 'tele') {
            var e = thr.e;
            if (!e || !e.alive || dead) { thrEnd(); return; }
            thr.t += dt;
            var u = Math.min(1, thr.t / 3);
            thrGlow(1 + 3 * u * (0.5 + 0.5 * Math.sin(thr.t * (6 + 12 * u))));
            e.cr.att.u = u;
            if (thr.t >= 3) {                                                 // launch along the line to where the pilot is now
                thr.dir.subVectors(P, ap); thr.d0 = thr.dir.length() || 1; thr.dir.divideScalar(thr.d0);
                thr.s0.copy(ap); thr.t = 0; thr.ph = 'fly'; thr.peak = 0; thr.offs.length = 0; thr.logT = 0;
                thr.stop = Math.max(2 * n.sysR, thr.d0 + 3 * thr.rec.R);
                e.ms = 'recover'; e.mt = 0; e.mv = null; e.cr.att.ph = 'recover'; e.cr.att.u = 0; e.mcd = 3;
                shake = Math.max(shake, 1.2 * L);
            }
            return;
        }
        if (thr.ph === 'fly') {
            thr.t += dt;
            var tr = thr.t * 60;
            if (thr.t > 8 || tr > thr.stop) { thr.ph = 'ease'; thr.logT = 0; thrGlow(1); }
            else {
                n.nx = thr.s0.x + thr.dir.x * tr - (ap.x - (n.nx || 0));
                n.ny = thr.s0.y + thr.dir.y * tr - (ap.y - (n.ny || 0));
                n.nz = thr.s0.z + thr.dir.z * tr - (ap.z - (n.nz || 0));
                thr.peak = Math.sqrt(n.nx * n.nx + n.ny * n.ny + n.nz * n.nz);
                thrGlow(2);
                var R = engine.renderedRadius(n) || thr.rec.R;
                if (!dead && P.distanceTo(ap) < HARD_F * R) {                 // the disc reaches the pilot: instant death, god does not save you
                    var g0 = god; god = false; hurtPlayer(1e9, undefined, 3, true); god = g0; thr.lastHit = true;
                    announce('CRUSHED BY A PLANET');
                }
            }
            if (elIncoming) {                                                 // HUD: label + bearing arrow toward the planet
                if (!elIncoming.classList.contains('is-on')) elIncoming.classList.add('is-on');
                vA.copy(ap).project(camera);
                var bx = vA.x, by = vA.y;
                if (vA.z > 1) { bx = -bx; by = -by; }
                var ang = Math.atan2(-by, bx), m = Math.max(Math.abs(bx), Math.abs(by), 1e-3), k2 = Math.min(1, 0.82 / m);
                var px = window.innerWidth * (0.5 + 0.5 * bx * k2), py = window.innerHeight * (0.5 - 0.5 * by * k2);
                elIncomingI.style.transform = 'translate(' + px.toFixed(0) + 'px,' + py.toFixed(0) + 'px) rotate(' + (ang + 0.7854).toFixed(2) + 'rad)';
            }
            return;
        }
        // ease: the engine's decay brings it home; log the offset so the equilibrium is visible
        if (elIncoming) elIncoming.classList.remove('is-on');
        thr.logT -= dt;
        var mag = Math.sqrt((n.nx || 0) * (n.nx || 0) + (n.ny || 0) * (n.ny || 0) + (n.nz || 0) * (n.nz || 0));
        if (thr.logT <= 0) { thr.logT = 2; if (thr.offs.length < 60) thr.offs.push(+mag.toFixed(2)); }
        if (mag < Math.max(0.5, thr.peak * 0.02)) { thr.ph = 'idle'; thr.e = null; thr.node = null; thr.gc = null; }
    }
    function setLod(e, k) {
        e.lod = k; e.body.visible = k < 2;
        if (k === 2) {
            if (!e.imp) { e.imp = perfMod.makeImpostor(THREE, 0xff4a30); e.g.add(e.imp); }
            e.imp.scale.setScalar(Math.max(1, e.len / e.sc) * 1.2); e.imp.visible = true;
        } else if (e.imp) e.imp.visible = false;
    }
    // world-space limb capsules of a boss, cached per frame: [ax ay az bx by bz r] x capN
    function bossCaps(e) {
        var limbs = e.cr.limbs, n = limbs.length;
        if (!e.capW || e.capW.length < n * 7) e.capW = new Float64Array(n * 7);
        e.g.updateMatrixWorld(true);
        for (var i = 0; i < n; i++) {
            var cp = limbs[i].capsule, o = i * 7;
            vC1.copy(cp.a); e.cr.group.localToWorld(vC1); vC2.copy(cp.b); e.cr.group.localToWorld(vC2);
            e.capW[o] = vC1.x; e.capW[o + 1] = vC1.y; e.capW[o + 2] = vC1.z; e.capW[o + 3] = vC2.x; e.capW[o + 4] = vC2.y; e.capW[o + 5] = vC2.z; e.capW[o + 6] = cp.r * e.sc;
        }
        e.capN = n;
    }
    var BOSS_CLOSE = 0.3;            // rev 19: u/s
    function bossSep(a, b) { return 1.5 * Math.max(a.len, b.len); }
    // would moving boss e by `step` along -dir (toward the player) bring it inside the separation of another boss (while getting closer)?
    function formationFree(e, step, dir) {
        var px = e.g.position.x - dir.x * step, py = e.g.position.y - dir.y * step, pz = e.g.position.z - dir.z * step;
        for (var i = 0; i < enemies.length; i++) {
            var o = enemies[i]; if (o === e || !o.alive || !o.isBoss) continue;
            var sp = bossSep(e, o), dx = px - o.g.position.x, dy = py - o.g.position.y, dz = pz - o.g.position.z, dn = dx * dx + dy * dy + dz * dz;
            if (dn >= sp * sp) continue;
            dx = e.g.position.x - o.g.position.x; dy = e.g.position.y - o.g.position.y; dz = e.g.position.z - o.g.position.z;
            if (dn < dx * dx + dy * dy + dz * dz) return false;
        }
        return true;
    }
    // rev 22: a closing step that would crowd a wing boss is not skipped (that stalled the whole wing for seconds): the candidate slides out along the
    // neighbour's separation sphere, so the wing fans out around the player while every boss still gains ground. A step that cannot gain any is skipped.
    var fsC = new THREE.Vector3();
    function formationStep(e, step, dir, dist) {
        var pos = e.g.position;
        fsC.copy(pos).addScaledVector(dir, -step);
        for (var pass = 0; pass < 2; pass++) {
            for (var i = 0; i < enemies.length; i++) {
                var o = enemies[i]; if (o === e || !o.alive || !o.isBoss) continue;
                var sp = bossSep(e, o), dx = fsC.x - o.g.position.x, dy = fsC.y - o.g.position.y, dz = fsC.z - o.g.position.z, dn = Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (dn >= sp) continue;
                var cx = pos.x - o.g.position.x, cy = pos.y - o.g.position.y, cz = pos.z - o.g.position.z;
                if (dn < Math.sqrt(cx * cx + cy * cy + cz * cz) || dn < 1e-6) { if (dn < 1e-6) { dx = cx; dy = cy; dz = cz; dn = Math.sqrt(dx * dx + dy * dy + dz * dz) || 1; } fsC.set(o.g.position.x + dx / dn * sp, o.g.position.y + dy / dn * sp, o.g.position.z + dz / dn * sp); }
            }
        }
        vS.subVectors(fsC, shipRoot.position);
        var nd = vS.length();
        if (nd < dist - 0.25 * step) pos.copy(fsC);
    }
    function updateBoss(e, dt, bodies) {
        var P = shipRoot.position, pos = e.g.position, cr = e.cr, att = cr && cr.att, moves = cr && cr.moves;
        e.prev.copy(pos);
        vD.subVectors(pos, P);
        var dist = vD.length() || 1;
        vD.divideScalar(dist);                                       // unit from player to boss
        var sd = dist - e.R, ms = e.ms, mv = e.mv, rt = e.kind === 3 ? 0.35 : 1.3, i;
        vF.copy(NEG_Z).applyQuaternion(e.g.quaternion);
        var fr = e.hp / e.maxHp, ph = fr > 0.66 ? 1 : (fr > 0.33 ? 2 : 3);
        if (ph > e.bphase) {
            e.bphase = ph; e.pflash = 1; e.hitT = 1;
            shake = Math.max(shake, 0.9 * L); fovKick = 3;
            announceBoss('PHASE ' + ph + ' · ' + e.title);
            if (e.kind >= 2) spawnAdds(2, e);
        }
        if (e.pflash > 0) e.pflash = Math.max(0, e.pflash - dt);
        e.curSpeed = 0;
        if (e.stagger > 0) {            // rev 20: a severed limb staggers the boss: it cannot attack for STAGGER_T and flashes
            e.stagger = Math.max(0, e.stagger - dt);
            e.ms = 'idle'; e.mv = null; e.open = true; e.hitDone = true; e.hitT = Math.max(e.hitT, 0.55 + 0.35 * Math.sin(gt * 34));
            if (att) { att.ph = 'idle'; att.u = 0; }
            if (e.mcd < 0.8) e.mcd = 0.8;
            wobble(e, dt * 0.5); e.vel.set(0, 0, 0);
            return;
        }
        if (att && moves && moves.length) {
            if (ms === 'idle' || ms === 'turn') {
                // approach: close to the nearest move's reach, never into the ram shell; drift home when the pilot is far
                var minReach = 1e30;
                var maxReach = 0;
                for (i = 0; i < moves.length; i++) { minReach = Math.min(minReach, moves[i].reachL * e.sc); maxReach = Math.max(maxReach, moves[i].reachL * e.sc); }
                e.reachMax = maxReach;
                var stopD = Math.max(minReach * 0.9, e.R * 1.08 + 2 * L);
                if (!dead && dist < Math.max(e.len * 2 + 700 * L, (e.chaseD || 0) * 1.3)) {
                    if (dist > stopD && !(thr.ph === 'tele' && thr.e === e)) {
                        // rev 19: bosses CLOSE SLOWLY (0.3 u/s) and in formation; a step that would bring two bosses nearer than 1.5 x the longer length is skipped
                        var cstep = Math.min(dist - stopD, e.kind === 3 ? clamp(e.len * 0.02, 10 * L, 60 * L) * dt : BOSS_CLOSE * (dist > 40 * L ? 4 : 1) * dt);
                        formationStep(e, cstep, vD, dist);
                    }
                } else {
                    vS.subVectors(e.home, pos);
                    var hd = vS.length();
                    if (hd > 0.3 * L) pos.addScaledVector(vS, Math.min(hd * 0.5, CRUISE) / hd * dt);
                }
            }
            if (ms === 'idle') {
                att.ph = 'idle'; e.open = false; e.sigGlow = Math.max(0, e.sigGlow - dt * 3);
                bossFace(e, P.x, P.y, P.z, false, rt, dt);
                e.mcd -= dt;
                if (!dead && wave >= 20 && e.len >= refR && thr.ph === 'idle' && e.mcd <= 0.5) { e.thrCd -= dt; if (e.thrCd <= 0) { e.thrCd = 18 + Math.random() * 10; if (startThrow(e, bodies, false)) ms = e.ms; } }
                if (ms === 'idle' && e.mcd <= 0 && !dead) {
                    var nm = moves.length;
                    for (i = 0; i < nm; i++) {
                        var cand = moves[(e.mi + i) % nm];
                        if (dist < cand.reachL * e.sc * 1.05) { e.mv = cand; e.mi = (e.mi + i + 1) % nm; e.ms = 'turn'; e.mt = 0; break; }
                    }
                }
            } else if (ms === 'turn') {
                mv = e.mv; e.mt += dt;
                var back = mv.limb >= 0 && cr.limbs[mv.limb].back;
                var rem = bossFace(e, P.x, P.y, P.z, back, rt, dt);
                if (dist > mv.reachL * e.sc * 1.2) { e.ms = 'idle'; e.mcd = 0.4; }
                else if (rem < 0.3 || e.mt > 4) {
                    vM.set(1, 0, 0).applyQuaternion(e.g.quaternion);
                    vS.subVectors(P, pos);
                    e.mside = vS.dot(vM) >= 0 ? 1 : -1;
                    e.ms = 'tele'; e.mt = 0; e.hitDone = false;
                    att.type = mv.type; att.limb = mv.limb; att.ph = 'tele'; att.u = 0; att.side = e.mside;
                    announceBoss(e.title + ' · ' + mv.name.toUpperCase());
                    aPlay('bossRoar', { dist: Math.min(aDist(pos), 400) * 0.1 });
                }
            } else if (ms === 'tele') {
                e.mt += dt; att.u = Math.min(1, e.mt / mv.tele); e.open = true; e.sigGlow = 0.5 + 0.4 * att.u;
                if (e.mt >= mv.tele) { e.ms = 'strike'; e.mt = 0; att.ph = 'strike'; att.u = 0; }
            } else if (ms === 'strike') {
                e.mt += dt; att.u = Math.min(1, e.mt / mv.dur); e.open = true; e.sigGlow = 0.8;
                if (mv.type === 'spin') e.g.rotateY(e.mside * 6.2832 * dt / mv.dur);
                if (att.u >= 1) { e.ms = 'recover'; e.mt = 0; att.ph = 'recover'; att.u = 0; }
            } else if (ms === 'recover') {
                e.mt += dt; att.u = Math.min(1, e.mt / (mv ? mv.rec : 0.8)); e.open = true; e.sigGlow = Math.max(0, 0.8 * (1 - att.u));
                if (att.u >= 1) { e.ms = 'idle'; att.ph = 'idle'; e.open = false; e.mcd = mv ? mv.cd * (0.85 + Math.random() * 0.3) : 3; }
            } else if (ms === 'throw') {
                e.open = true; e.sigGlow = 0.7;
                if (thr.ph === 'tele' && thr.e === e) bossFace(e, thr.node.anchor.position.x, thr.node.anchor.position.y, thr.node.anchor.position.z, false, rt, dt);
                else { e.ms = 'idle'; att.ph = 'idle'; e.open = false; e.mcd = 2; }
            }
            // escorts do the shooting
            if (!dead && sd < e.len + 900 * L) { e.escT -= dt; if (e.escT <= 0) { e.escT = 20 + Math.random() * 10; spawnEscorts(e); } }
        }
        if (e.noLimbs) e.open = true;   // rev 20: no limbs left = the core stays exposed (x4)
        if (e.kind !== 3) pushOutOfBodies(pos, bodies, 1.2);
        wobble(e, dt * 0.5);
        bossContact(e);
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
                alive: false, hp: 0, target: null, retarget: 0, fireCd: 0, curSpeed: 0, state: 0, near: 0, rpSet: false, off: new THREE.Vector3(), timer: 0, side: k % 2 ? 1 : -1, flip: 3, slot: k,
                mk: { placed: true, root: g, name: '', color: C_A }
            });
        }
    }
    function spawnAllies(n) {
        if (n <= 0) return;
        ensureAllies();
        vA.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);          // rev 14: wingmen form up AHEAD of the player so they are in view
        vB.copy(X).applyQuaternion(shipRoot.quaternion);
        for (var k = 0; k < n && k < ALLY_MAX; k++) {
            var a = allies[k];
            if (a.alive) { a.hp = Math.min(ALLY_HP, a.hp + ALLY_HP * 0.5); continue; }
            a.alive = true; a.hp = ALLY_HP; a.target = null; a.retarget = 0; a.curSpeed = Math.abs(speed) + 0.5;
            a.g.position.copy(shipRoot.position).addScaledVector(vA, (22 + k * 8) * L).addScaledVector(vB, (k - 1) * 14 * L);
            a.g.quaternion.copy(shipRoot.quaternion);
            a.g.visible = true;
        }
    }
    function allyPickTarget(a) {
        if (zT > 0 && zTarget && zTarget.alive) return zTarget;       // rev 9c: Z = every wingman peels onto your target
        var best = null, bd = 1e30;
        for (var i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive) continue;
            var d = a.g.position.distanceTo(e.g.position) - e.R; d = d * d;
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
            // rev 14: slot 20-40 L AHEAD, sweeping side to side so each wingman crosses the view in a pass (period ~14 s, phase per slot)
            vS.set(Math.sin(gt * 0.45 + a.slot * 2.1) * 34 * L, Math.cos(gt * 0.37 + a.slot * 1.7) * 8 * L, -(24 + a.slot * 7) * L).applyQuaternion(shipRoot.quaternion).add(P);
            d = pos.distanceTo(vS);
            var spd = Math.min(Math.abs(speed) * 1.25 + A_SPEED, d * 1.5 + Math.abs(speed));
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
                
                tailAlly = null; tailBy = null; return;
            }
            tailLeft -= dt;
            if (tailLeft <= 0) {
                tailAlly.hp = Math.min(tailAlly.hp, ALLY_HP * 0.4);
                
                tailAlly = null; tailBy = null;
            }
            return;
        }
        for (i = 0; i < allies.length; i++) {
            var a = allies[i];
            if (!a.alive) { a.tailT = 0; continue; }
            vF.copy(Z).applyQuaternion(a.g.quaternion);              // ally's six
            var hit = null;
            for (j = 0; j < enemies.length; j++) {
                var e = enemies[j];
                if (!e.alive || e.state >= 4) continue;
                vA.subVectors(e.g.position, a.g.position);
                var d2 = vA.lengthSq();
                if (d2 < (PERIL_R * L) * (PERIL_R * L) && vA.dot(vF) > Math.sqrt(d2) * 0.906) { hit = e; break; }
            }
            if (hit) {
                a.tailT = (a.tailT || 0) + dt;
                if (a.tailT >= PERIL_DETECT) { tailAlly = a; tailBy = hit; tailLeft = PERIL_SAVE; a.tailT = 0;  return; }
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
        var t = document.createElement('span'); t.className = 'sc-t'; if (text && text.indexOf(LNG.ALIEN_OPEN) >= 0) lingoFill(t, text); else t.textContent = text; r.appendChild(t);
        elChatLog.appendChild(r); chatRows.push(r);
        aPlay('ui', { vel: 0.4 });
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
        for (i = 0; i < bombs.length; i++) { bombs[i].active = false; bombs[i].m.visible = false; bombs[i].shell.visible = false; }
        waveActive = false; qN = 0; ctarget = null; tlock = false; tailAlly = null; tailBy = null;
        killBolts();
    }
    function setWave(n) {
        var i;
        clearFight();
        for (i = 0; i < allies.length; i++) { allies[i].alive = false; allies[i].g.visible = false; }
        wave = Math.max(0, n - 1); wavePending = true; nextWave = gt + 0.05;
    }
    var HELP = 'KEYS · SHIFT+LMB ram (boosting) · V scan · T cycle target · A/D x2 roll · S x2 flip · CTRL drift · Q focus · Z wingmen focus fire · F interact (land, board, talk, shop) · E inventory · ENTER or / chat · CMDS · /wave N · /peaceful · /hostile · /difficulty 1-3 · /user NAME · /color #hex|name · /gorcave TEXT · /quality 0-3|auto · /volume 0-10 · /weapons [N] (owned weapons, N equips) · /ship [N|kind] (owned hulls, switch) · /help';
    function applyDifficultyLive() {
        var D = DIFFS[difficulty];
        for (var i = 0; i < enemies.length; i++) {
            var e = enemies[i];
            if (!e.alive || e.bDmg === undefined) continue;
            e.dmg = e.bDmg * D.dmg; e.speed = e.bSpeed * D.spd;
        }
    }
    function applyProfile() {                  // push the current profile's color/name to the local hull and the relay
        if (hullObj) tintGhost(hullObj, effColor());
        if (hullLow) tintGhost(hullLow, effColor());
        exColor(exMe, effColor());
        applyUpgrades();
        if (net) { net.setName(userName()); net.setColor(effColor()); }
    }
    function runCommand(str) {
        var s0 = String(str == null ? '' : str).trim().toLowerCase().replace(/^\/+/, ''), m;
        var ok = function (t) { showCmdRes(t, false); return t; };
        var bad = function (t) { showCmdRes(t, true); return t; };
        if (!s0) return bad('EMPTY COMMAND · /help');
        var raw = String(str == null ? '' : str).trim().replace(/^\/+/, '');
        if (perfMod && /^quality\b/i.test(raw)) { var qr = perfMod.applyQualityCommand(raw); if (qr !== null) return ok(qr); }
        if ((m = /^user(?:\s+(.*))?$/i.exec(raw))) {
            var ua = String(m[1] || '').replace(/[^\x20-\x7e]/g, '').trim().split(/\s+/), un = (ua[0] || '').slice(0, 16), ukey = ua.slice(1).join(' ').slice(0, 64);
            if (!un || un === '__proto__') return bad('USAGE · /user NAME [KEY]');
            curUser = un; ensureProfile(un);
            applyProfile(); writeSave(); suitSync(true);
            announce('USER ' + un.toUpperCase());
            if (ukey) { if (ukey.length < 4) return bad('KEY NEEDS 4+ CHARACTERS'); if (!/^[a-z0-9_-]{2,20}$/i.test(un)) return bad('RELAY NAME · 2-20 of a-z 0-9 _ -'); return ok('USER ' + un + ' · ' + r27Login(un, ukey)); }
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
        if ((m = /^volume(?:\s+(\d+(?:\.\d+)?))?$/i.exec(raw))) {
            if (m[1] === undefined) return ok('VOLUME ' + volume + ' / 10 · /volume 0-10');
            volume = clamp(parseFloat(m[1]), 0, 10);
            try { if (state === 'piloting') audio.setMaster(volume / 10); audio.unlock(); } catch (e) { /* ignore */ }
            writeSave();
            return ok('VOLUME ' + volume);
        }
        if ((m = /^weapons?(?:\s+(\d+))?$/i.exec(raw))) {
            var pw = curProfile(), own = pw ? pw.weapons : [], cwp = curWeapon(), wl = function (w) { return wpnMod && wpnMod.weaponLine ? wpnMod.weaponLine(w) : w.name; };
            if (m[1] !== undefined) {
                var wn = parseInt(m[1], 10);
                if (wn === 0) { ensureProfile(userName()).weapon = null; writeSave(); return ok('EQUIPPED ' + STARTER.name); }
                if (!own[wn - 1]) return bad('NO WEAPON ' + wn + ' · /weapons');
                equipWeapon(own[wn - 1]);
                return 'EQUIPPED';
            }
            showCmdRes((cwp.id === STARTER.id ? '> ' : '  ') + '0 ' + STARTER.name + ' (96 dps)', false);
            for (var wi = 0; wi < own.length; wi++) showCmdRes((own[wi].id === cwp.id ? '> ' : '  ') + (wi + 1) + ' ' + wl(own[wi]), false);
            return 'WEAPONS';
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
        if (c === 'ship' || c === 'ships') return r28ShipCmd(parts.slice(1).join(' '));
        if (c === 'help') return ok(HELP);
        return bad('UNKNOWN COMMAND · /help');
    }

    // ─── rev 9 / 9b / 9c combat helpers ─────────────────────────────
    function waveClear() {
        waveActive = false; wavePending = true;
        var big = bossWave || waveQuiet || wave % 4 === 0;                               // every 4th wave / boss: longer breather
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
    function tryRam() {
        if (dead || ramCd > 0 || ramT > 0 || state !== 'piloting' || gmode !== 'fly' || boarding || exiting) return false;
        ramT = RAM_T; ramCd = RAM_T + RAM_CD; ramFx = 0;
        aPlay('ram', { vel: 1 }); fovKick = Math.max(fovKick, 5); shake = Math.max(shake, 0.4 * L);
        return true;
    }
    function startRoll(dir) {
        if (state !== 'piloting' || dead || cmdOpen || rollCd > 0 || rollT > 0 || flipT > 0) return false;
        rollT = ROLL_T; rollCd = ROLL_CD; rollDir = dir; rollFx = 0; rollX.copy(X).applyQuaternion(shipRoot.quaternion);
        return true;
    }
    // rev 13: double-tap S = Immelmann-ish flip: 0.8 s, 180 deg pitch + 180 deg roll (q0 * Rx(pi s) * Rz(pi s) = a clean heading reversal, upright)
    function startFlip() {
        if (state !== 'piloting' || dead || cmdOpen || flipCd > 0 || flipT > 0 || rollT > 0) return false;
        flipT = FLIP_T; flipCd = FLIP_CD; flipQ0.copy(shipRoot.quaternion);
        return true;
    }
    function tapKey(code) {                                                 // double-tap A/D within TAP_MS = dodge roll; S = flip
        var now = performance.now(), ok;
        if (now - tapT[code] < TAP_MS) {
            ok = code === 'KeyS' ? startFlip() : startRoll(code === 'KeyA' ? 1 : -1);
            tapT[code] = ok ? 0 : now;
        } else tapT[code] = now;
    }

    // ─── combat HUD (every DOM write is gated on a changed value) ───
    function hudCombat(wdt) {
        var v, i, e;
        v = rollCd > 0 ? Math.round((1 - rollCd / ROLL_CD) * 20) : 20;
        if (v !== cRoll) { cRoll = v; elRoll.style.setProperty('--p', (v * 5) + '%'); elRoll.classList.toggle('is-ready', v >= 20); }
        v = flipT > 0 ? 0 : (flipCd > 0 ? Math.round((1 - flipCd / FLIP_CD) * 20) : 20);
        if (v !== cFlip) { cFlip = v; elFlip.style.setProperty('--p', (v * 5) + '%'); elFlip.classList.toggle('is-ready', v >= 20); }
        v = driftOn ? Math.round(driftLeft / DRIFT_T * 20) : (driftCd > 0 ? Math.round((1 - driftCd / DRIFT_CD) * 20) : 20);
        if (v !== cDrift) { cDrift = v; elDrift.style.setProperty('--p', (v * 5) + '%'); elDrift.classList.toggle('is-ready', v >= 20); elDrift.classList.toggle('is-on', driftOn); }
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
            if (ct.isBoss && ct.eyeN) vW3.copy(ct.eyeW[0]);
            vA.copy(vW3).project(camera);
            if (vA.z < 1) { showL = true; lx = (vA.x * 0.5 + 0.5) * W; ly = (-vA.y * 0.5 + 0.5) * H; }
            var bs = curWeapon().stats.speed * L, t = vW3.distanceTo(P) / bs;
            for (i = 0; i < 2; i++) { vB.copy(vW3).addScaledVector(ct.vel, t); t = vB.distanceTo(P) / bs; }      // iterate twice: target.pos + target.vel x dist / boltSpeed
            vA.copy(vB).project(camera);
            if (vA.z < 1) {
                showP = true; px = (vA.x * 0.5 + 0.5) * W; py = (-vA.y * 0.5 + 0.5) * H;
                vU.copy(vB).sub(camera.position);
                vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                solid = vU.dot(vF) > vU.length() * Math.cos(1.5 * Math.PI / 180);                              // solid within 1.5 deg of the reticle
            }
            txt = (ct.isBoss ? (ct.kind === 3 ? 'TITAN' : (ct.kind === 2 ? 'GIANT' : 'MINI-BOSS')) : ROLE_T[ct.role].name + ' T' + ct.tier) + (ct.fstate === 'flee' || ct.fstate === 'regen' || ct.fstate === 'tele' ? ' · FLEEING' : '') + '  ·  ' + Math.round(vW3.distanceTo(P) / L) + ' L';
        }
        var pk = showP ? px.toFixed(0) + ',' + py.toFixed(0) : '';
        if (pk !== cPip) { cPip = pk; if (showP) elPip.style.transform = 'translate(' + px.toFixed(1) + 'px,' + py.toFixed(1) + 'px)'; elPip.style.display = showP ? 'block' : 'none'; }
        if (solid !== cPipSolid) { cPipSolid = solid; elPip.classList.toggle('is-solid', solid); }
        var lk = showL ? lx.toFixed(0) + ',' + ly.toFixed(0) : '';
        if (lk !== cLockTxt) { cLockTxt = lk; if (showL) elLock.style.transform = 'translate(' + lx.toFixed(1) + 'px,' + ly.toFixed(1) + 'px)'; elLock.style.display = showL ? 'block' : 'none'; }
        cTargetEnemy = txt;
        // rev 21: heat ring around the reticle (fills with heat, amber above 70 %, red + pulsing during the overheat lockout)
        v = Math.round(heat * 50);
        var hcls = ohT > 0 ? 2 : (heat > 0.7 ? 1 : 0);
        if (v !== cHeat) { cHeat = v; elHeat.style.setProperty('--p', v * 2); elHeat.classList.toggle('is-on', v > 0); }
        if (hcls !== cHeatCls) { cHeatCls = hcls; elHeat.classList.toggle('is-hot', hcls === 1); elHeat.classList.toggle('is-over', hcls === 2); }
        // rev 21: LEADER chevron over the squad leader, STALLED tag over a stalled enemy (pooled, max 8)
        var mN = 0, W2 = W * 0.5, H2 = H * 0.5;
        if (!dead) for (i = 0; i < enemies.length && mN < EM_MAX; i++) {
            e = enemies[i];
            if (!e.alive || e.isBoss || (!e.leader && e.stallT <= 0)) continue;
            vA.copy(e.g.position).project(camera);
            if (vA.z >= 1 || vA.z <= -1 || Math.abs(vA.x) > 1.1 || Math.abs(vA.y) > 1.1) continue;
            var em = emPool[mN++], ex2 = vA.x * W2 + W2, ey2 = -vA.y * H2 + H2 - 30;
            var et = (e.leader ? 'LEADER' : '') + (e.leader && e.stallT > 0 ? ' · ' : '') + (e.stallT > 0 ? 'STALLED' : '');
            if (!em.on) { em.on = true; em.el.style.display = ''; }
            if (et !== em.txt) { em.txt = et; em.b.textContent = et; em.el.className = 'sh-em' + (e.leader ? ' is-lead' : '') + (e.stallT > 0 ? ' is-stall' : ''); }
            if (Math.abs(ex2 - em.x) > 0.5 || Math.abs(ey2 - em.y) > 0.5) { em.x = ex2; em.y = ey2; em.el.style.transform = 'translate(' + ex2.toFixed(1) + 'px,' + ey2.toFixed(1) + 'px) translate(-50%,-100%)'; }
        }
        for (i = mN; i < EM_MAX; i++) if (emPool[i].on) { emPool[i].on = false; emPool[i].el.style.display = 'none'; }
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

    // rev 13: HUD marks for the other PLAYERS (ship-net roster): a chevron on the screen edge in their theme color with the name under it when they
    // are off-screen, a small diamond + name when on-screen. Pooled DOM (max 32); every write is gated on a changed value.
    var PL_MAX = 32, plMarks = [];
    (function buildPlayerMarks() {
        for (var i = 0; i < PL_MAX; i++) {
            var el = document.createElement('div'); el.className = 'sh-pl'; el.style.display = 'none';
            el.innerHTML = '<i></i><b></b>';
            elPlayers.appendChild(el);
            plMarks.push({ el: el, c0: 0, i: el.firstChild, b: el.lastChild, on: false, x: -1e9, y: -1e9, r: -999, mode: -1, name: '', col: -1 });
        }
    })();
    var plN = 0;
    function plMark(g) {
        if (plN >= PL_MAX || !g.placed || !g.root) return;
        var W = window.innerWidth, H = window.innerHeight, m = plMarks[plN++], sx, sy, dx, dy, dm, ex, ey, rot, mode;
        vA.copy((g.mk2 || g.root).position).applyMatrix4(camera.matrixWorldInverse);        // camera space: -z is ahead
        if (vA.z < 0) { vB.copy(vA).applyMatrix4(camera.projectionMatrix); sx = vB.x; sy = vB.y; } else { sx = 9; sy = 9; }
        if (!m.on) { m.on = true; m.el.style.display = 'block'; }
        if (vA.z < 0 && Math.abs(sx) < 0.93 && Math.abs(sy) < 0.9) {
            mode = 1; ex = (sx * 0.5 + 0.5) * W; ey = (-sy * 0.5 + 0.5) * H; rot = 45;
        } else {
            mode = 0; dx = vA.x; dy = -vA.y; dm = Math.max(Math.abs(dx) / (W / 2 - 40), Math.abs(dy) / (H / 2 - 52), 1e-6);
            ex = W / 2 + dx / dm; ey = H / 2 + dy / dm; rot = Math.round(Math.atan2(dy, dx) * 57.2958 + 90);
        }
        if (mode !== m.mode) { m.mode = mode; m.el.classList.toggle('is-near', mode === 1); m.r = -999; }
        if (Math.abs(ex - m.x) > 0.5 || Math.abs(ey - m.y) > 0.5) { m.x = ex; m.y = ey; m.el.style.transform = 'translate(' + ex.toFixed(1) + 'px,' + ey.toFixed(1) + 'px) translate(-50%,-50%)'; }
        if (rot !== m.r) { m.r = rot; m.i.style.transform = 'rotate(' + rot + 'deg)'; }
        var nm = String(g.name || 'PILOT').toUpperCase(), col = g.color >= 0 ? g.color : 0x8a5cff;
        var shown = (mode === 1 && mkNamed(m, mkAng((g.mk2 || g.root).position)) ? nm : '') + (mode === 1 ? (function (d) { return d ? ' ' + d : ''; })(mkDist(vA.length())) : '');
        if (mode !== 1) m.c0 = 0;
        if (shown !== m.name) { m.name = shown; m.b.textContent = shown; }
        if (col !== m.col) { m.col = col; m.el.style.color = hexCss(col); }
    }
    function updatePlayerMarks() {
        var i;
        plN = 0;
        if (net && !dead && net.ghosts && net.ghosts.size) net.ghosts.forEach(plMark);
        if (!dead && gmode === 'fly') for (i = 0; i < allies.length; i++) if (allies[i].alive) plMark(allies[i].mk);      // rev 14: wingmen get chevrons + names like players
        for (i = plN; i < PL_MAX; i++) if (plMarks[i].on) { plMarks[i].on = false; plMarks[i].el.style.display = 'none'; }
    }

    // ─── flight ─────────────────────────────────────────────────────
    // chase camera: rigid on the ship's orientation (no lag), offset behind and above
    function chaseTargets(dt) {
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
        vU.copy(Y).applyQuaternion(shipRoot.quaternion);
        var d = CAM_L * L * (1 + 0.7 * pulse) * (1 + CAM_PLANET * nearPl);
        camPos.copy(shipRoot.position).addScaledVector(vF, -d).addScaledVector(vU, CAM_UP * CAM_L * L);
        camQuat.copy(shipRoot.quaternion);
    }
    // rev 13: earliest time (s, within H) the swept path P0 + V t hits a body shell or, in an active planet's atmosphere, the sampled terrain.
    // Fills phN (radial normal at the hit) and phRec. -1 = no hit. psFloor is budgeted, so terrain sampling is 4 points a frame.
    var phN = new THREE.Vector3(), phRec = null;
    function pathHit(P0, V, H, bodies) {
        var sp = V.length(), best = -1, i, k;
        phRec = null;
        if (sp < 0.2) return -1;
        for (i = 0; i < bodies.length; i++) {
            var b = bodies[i], c = b.node.anchor.position, reach = b.R * 1.3 + sp * H;
            if (ps && hasSurface(b.node)) continue;                       // rev 18: surface planets are handled by the approach governor + local-frame collision
            var dx = c.x - P0.x, dy = c.y - P0.y, dz = c.z - P0.z;
            if (dx * dx + dy * dy + dz * dz > reach * reach) continue;
            if (ps && ps.active === b.node) {
                for (k = 1; k <= 4; k++) {
                    var t = H * k / 4;
                    vG1.copy(P0).addScaledVector(V, t).sub(c);
                    var r = vG1.length(), fo = psFloor(vG1.add(c)), fr = (fo ? fo.r : b.R * 1.04) + 1.5 * L;
                    if (r < fr) { if (best < 0 || t < best) { best = t; phRec = b; phN.copy(vG1).sub(c).divideScalar(r || 1); } break; }
                }
            } else {
                var s = HARD_F * b.R;
                vG2.subVectors(P0, c);
                var a = sp * sp, bq = 2 * vG2.dot(V), cq = vG2.lengthSq() - s * s, t0 = -1;
                if (cq <= 0) t0 = 0;
                else { var disc = bq * bq - 4 * a * cq; if (disc >= 0) { var tt = (-bq - Math.sqrt(disc)) / (2 * a); if (tt >= 0 && tt <= H) t0 = tt; } }
                if (t0 >= 0 && (best < 0 || t0 < best)) { best = t0; phRec = b; phN.copy(vG2).addScaledVector(V, t0).normalize(); }
            }
        }
        return best;
    }
    // terrain / body contact: a NORMAL speed above BOOST costs TERRAIN_DMG and bounces; below that the ship just slides
    function terrainHit(vnIn, n) {
        if (vnIn <= BOOST || terrInv > 0 || dead) return;
        terrInv = TERRAIN_INV;
        hurtPlayer(TERRAIN_DMG, undefined, 2.5, true);
        burst(shipRoot.position, 14, 1, 16, 2); fx.flash(shipRoot.position, 0xff8040);
        shake = Math.max(shake, 2.6 * L); fovKick = 5;
        bounceV.copy(n).multiplyScalar(vnIn * 0.4);
        speed = Math.min(speed, 0.3 * CRUISE); pulse = 0; pulseT = 0;
    }
    function targetSpeed(boosting) {
        // throttle in [THROTTLE_MIN,0] maps to [-REVERSE,0]
        var base = throttle >= 0 ? throttle * CRUISE : (throttle / THROTTLE_MIN) * -REVERSE, bst = lf.cap ? ATM_BOOST : BOOST;    // rev 18: atmosphere boost cap
        if (boosting && throttle > 0.02) base *= bst / CRUISE;
        else if (boosting && throttle < -0.02) base = (throttle / THROTTLE_MIN) * -0.7 * bst;     // rev 9: reverse boost = 0.7 x boost
        return base * engMul * fxSpeed;      // rev 22: engine upgrade tiers (+10 % cruise / boost each)
    }
    // rev 13: renderedRadius can differ before vs during pilot mode, so the first piloting frames re-read it; if it moved, everything that baked L
    // (fx, planet surfaces, ally scale, net hooks, the pilot-scale ship) is rebuilt. Never caches a stale value.
    function refreshScale() {
        var r = readRefR();
        if (!r || Math.abs(r - refR) / refR < 0.01) return false;
        var i;
        refR0 = readRefRaw() || refR0; refR = r; L = r / 12000; pushShipLength(); galS = 0.9 * refR0; mineS = 1.35 * refR0;
        killBolts();
        fx.dispose(); fx = createFx(THREE, scene, camera, L); fxCones = 0; perfAttach();
        for (i = 0; i < exAll.length; i++) { var ex = exAll[i], th = ex.th.slice(), col = ex.col; ex.cones = []; ex.idx = []; ex.th = []; exSetup(ex, th, col); }
        if (state === 'piloting') fx.bolts.visible = true;
        if (psMod) { if (ps) { try { ps.dispose(); } catch (e) { /* ignore */ } ps = null; } makePs(); }
        if (sk.mode) skAbort();
        makeSpace(); makeStation();
        for (i = 0; i < allies.length; i++) allies[i].g.scale.setScalar(L);
        for (i = 0; i < enemies.length; i++) hideEnemy(enemies[i]);
        qN = 0; spawnPad = 6 * L;
        if (boarding) { chaseTargets(0); cineR1.copy(camPos).sub(shipRoot.position); cineQ1.copy(camQuat); }
        else if (state !== 'piloting') setShipScale(mineS);
        console.info('[ship] scale re-read: refR', r.toFixed(2), 'L', L.toFixed(5));
        return true;
    }
    // ─── rev 14: atmospheric entry burn ─────────────────────────────
    // Inside 1.4 R above BOOST speed: a copy of the hull (shared geometry, scale 1.08) with an additive fresnel fire shader wraps the ship,
    // orange -> white with speed; heat streaks ride fx.setMotion; the planet's rim rumble and a HUD "ENTRY" ride along.
    var entryHeat = 0, sheath = null, sheathHull = null, sheathMat = null;
    var elEntry = hud.querySelector('.sh-entry'), elLand = hud.querySelector('.sh-land'), elAlt = hud.querySelector('.sh-alt'), cEntry = false, cLandTxt = '', cAltTxt = '';
    function ensureSheath() {
        if (sheath && sheathHull === hullObj) return sheath;
        if (sheath) shipRoot.remove(sheath);
        if (!sheathMat) {
            sheathMat = new THREE.ShaderMaterial({
                transparent: true, depthWrite: false, side: THREE.DoubleSide, blending: THREE.AdditiveBlending,
                uniforms: { uTime: { value: 0 }, uHeat: { value: 0 }, uI: { value: 0 } },
                vertexShader: [
                    '#include <common>', '#include <logdepthbuf_pars_vertex>',
                    'varying vec3 vN; varying vec3 vV; varying float vZ;',
                    'void main(){',
                    '  vN = normalize(normalMatrix * normal); vZ = position.z;',
                    '  vec4 mv = modelViewMatrix * vec4(position, 1.0); vV = mv.xyz;',
                    '  gl_Position = projectionMatrix * mv;',
                    '  #include <logdepthbuf_vertex>',
                    '}'
                ].join('\n'),
                fragmentShader: [
                    '#include <common>', '#include <logdepthbuf_pars_fragment>',
                    'uniform float uTime; uniform float uHeat; uniform float uI;',
                    'varying vec3 vN; varying vec3 vV; varying float vZ;',
                    'void main(){',
                    '  #include <logdepthbuf_fragment>',
                    '  float f = pow(1.0 - abs(dot(normalize(vN), normalize(-vV))), 1.4);',
                    '  float fl = 0.6 + 0.4 * sin(vZ * 38.0 - uTime * 26.0) * sin(vZ * 17.0 + uTime * 19.0);',
                    '  float lead = 0.65 + 0.9 * smoothstep(0.3, -0.5, vZ);',
                    '  vec3 col = mix(vec3(1.0, 0.36, 0.05), vec3(1.0, 0.94, 0.82), uHeat);',
                    '  gl_FragColor = vec4(col * (0.9 + 1.6 * f), clamp((f * fl * lead + 0.07) * uI, 0.0, 1.0));',
                    '}'
                ].join('\n')
            });
        }
        sheath = new THREE.Group(); sheath.name = 'ship-sheath'; sheath.scale.setScalar(1.08);
        shipRoot.updateMatrixWorld(true);
        mM.copy(shipRoot.matrixWorld).invert();
        hullObj.traverse(function (o) {
            if (!o.isMesh || !o.geometry) return;
            var m = new THREE.Mesh(o.geometry, sheathMat);
            m.matrixAutoUpdate = false; m.matrix.multiplyMatrices(mM, o.matrixWorld); m.frustumCulled = false; m.renderOrder = 12;
            sheath.add(m);
        });
        shipRoot.add(sheath); sheathHull = hullObj;
        return sheath;
    }
    function updateEntry(dt, P, spd, bodies) {
        var heatT = 0, i;
        if (lf.on) {
            if (!dead && lf.entryT > 0) heatT = clamp(0.3 + 0.7 * lf.entryT / ENTRY_T, 0.3, 1);       // rev 18: the burn is the ENTRY brake
        } else if (!dead && spd > BOOST) {
            for (i = 0; i < bodies.length; i++) {
                if (bodies[i].R < MOON_L * L) continue;      // moonlets: no atmosphere, no entry burn
                vTmp.subVectors(P, bodies[i].node.anchor.position);
                var rr = 1.4 * bodies[i].R;
                if (vTmp.lengthSq() < rr * rr) { heatT = clamp(0.4 + (spd - BOOST) / 400, 0.4, 1); break; }
            }
        }
        entryHeat += (heatT - entryHeat) * damp(heatT > entryHeat ? 5 : 2.5, dt);
        if (entryHeat < 0.01) entryHeat = 0;
        if (entryHeat > 0) {
            ensureSheath().visible = true;
            var u = sheathMat.uniforms;
            u.uHeat.value = lf.on ? clamp(lf.entryT / ENTRY_T, 0, 1) * 0.85 : clamp((spd - BOOST) / 360, 0, 1); u.uI.value = entryHeat * 1.7; u.uTime.value = gt;
            shake = Math.max(shake, 0.033 * L * entryHeat); fovKick = Math.max(fovKick, 1.5 * entryHeat);      // rev 19: entry / exit rumble at 15 % of the old amplitude (0.22 L -> 0.033 L)
        } else if (sheath && sheath.visible) sheath.visible = false;
        var en = entryHeat > 0.15;
        if (en !== cEntry) { cEntry = en; elEntry.classList.toggle('is-on', en); if (en) aPlay('entry', { vel: 1 }); else aPlay('entry', { stop: true }); }
    }

    // ─── rev 14: landing + on foot ──────────────────────────────────
    // Sub-state of 'piloting': gmode fly | landing | landed | foot. Everything on the ground lives in the PLANET frame (the globe's spin +
    // orbit carried by node.mesh.quaternion / node.anchor.position), so a parked ship and a walking human stay glued to the terrain.
    // ship-human.js (createHuman) and ps.landable arrive from other modules: both have fallbacks.
    var gmode = 'fly', landOk = false, landCheckT = 0, legDrop = 0.4, jumpHeld = false;
    var land = { auto: false, settle: 0, dustT: 0, node: null, gDir: new THREE.Vector3(), r0: 0, rg: 0, t: 0, T: 2.2, qFrom: new THREE.Quaternion(), qTo: new THREE.Quaternion(), sPos: new THREE.Vector3(), sQuat: new THREE.Quaternion(), off: new THREE.Vector3() };
    var hum = { obj: null, pos: new THREE.Vector3(), hr: 0, gr: 0, vv: 0, air: false, hf: new THREE.Vector3(0, 0, -1), face: new THREE.Vector3(0, 0, -1), pitch: 0.3, moving: false, running: false, w: new THREE.Vector3(), up: new THREE.Vector3() };
    var gQ = new THREE.Quaternion(), gQi = new THREE.Quaternion(), gA = new THREE.Vector3(), gB = new THREE.Vector3(), gC = new THREE.Vector3(), gD = new THREE.Vector3(), gE = new THREE.Vector3();
    var camRel = new THREE.Vector3(), camRelInit = false, gFo = { r: 0, n: new THREE.Vector3() };
    var humanMod = null;
    import('./ship-human.js').then(function (m) { humanMod = m; }).catch(function () { /* capsule fallback */ });
    function makeHuman(color) {                 // { group (a holder, oriented + scaled by the caller), update(dt, st), setColor, dispose }
        var h = null, holder = new THREE.Group();
        if (humanMod && typeof humanMod.createHuman === 'function') { try { h = humanMod.createHuman(THREE, { color: color, suit: suitOf(curProfile()) }); } catch (e) { h = null; } }
        if (!h) {
            var cm = new THREE.Mesh(new THREE.CapsuleGeometry(0.16, 0.6, 3, 8), new THREE.MeshBasicMaterial({ color: color }));
            cm.position.y = 0.46;
            var cg = new THREE.Group(); cg.add(cm);
            h = { group: cg, update: function () {}, setColor: function (c) { cm.material.color.setHex(c); }, dispose: function () { cm.geometry.dispose(); cm.material.dispose(); } };
        }
        holder.add(h.group);
        holder.traverse(function (o) { o.frustumCulled = false; });
        return { group: holder, nozzles: function () { return (h.group && h.group.userData && h.group.userData.jetNozzles) || null; }, setSuit: function (s) { try { if (h.setSuit) h.setSuit({ suit: s }); } catch (e) { /* ignore */ } }, update: function (dt, st) { try { h.update(dt, st); } catch (e) { /* ignore */ } }, setColor: function (c) { try { h.setColor(c); } catch (e) { /* ignore */ } }, dispose: function () { try { h.dispose(); } catch (e) { /* ignore */ } } };
    }
    function hullDrop() {                       // how far below the hull origin the landing feet reach, in L
        var d = 0;
        if (hullObj) hullObj.traverse(function (o) {
            if (o.isMesh && o.geometry) { if (!o.geometry.boundingBox) o.geometry.computeBoundingBox(); d = Math.max(d, -o.geometry.boundingBox.min.y); }
        });
        return d > 0.05 && d < 2 ? d : 0.4;
    }
    function setGround(on, foot) { hud.classList.toggle('is-ground', on); hud.classList.toggle('is-foot', !!foot); }
    function surfNormal(dirW) { return gE.copy(dirW).normalize(); }
    function startLanding() {
        if (!ps || !ps.active || gmode !== 'fly' || dead || state !== 'piloting' || boarding || exiting) return false;
        var node = ps.active, c = node.anchor.position, P = shipRoot.position, fo;
        try { fo = ps.floorAt(P, { r: 0, n: new THREE.Vector3() }); } catch (e) { return false; }
        if (!fo || !(fo.r > 0)) return false;
        gA.subVectors(P, c); var r0 = gA.length(); if (r0 < 1e-6) return false;
        gA.divideScalar(r0);                                         // world up at the site
        legDrop = hullDrop();
        gQi.copy(node.mesh.quaternion).invert();
        land.node = node; land.r0 = r0; land.rg = fo.r + legDrop * L; land.t = 0; land.settle = 0; land.dustT = 0; land.auto = false; land.T = 2.2;
        land.gDir.copy(gA).applyQuaternion(gQi);
        gB.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); gB.addScaledVector(gA, -gB.dot(gA));
        if (gB.lengthSq() < 1e-6) gB.crossVectors(gA, X);
        gB.normalize(); gC.crossVectors(gB, gA); gD.copy(gB).negate();
        mM.makeBasis(gC, gA, gD); gQ.setFromRotationMatrix(mM);      // upright, nose along the old heading
        land.qFrom.copy(gQi).multiply(shipRoot.quaternion);
        land.qTo.copy(gQi).multiply(gQ);
        lfOff(); gmode = 'landing'; landOk = false; firing = false; mdx = mdy = 0;
        vel.set(0, 0, 0); speed = 0; throttle = 0; pulse = 0; pulseT = 0;
        killBolts(); combatRoot.visible = false;
        setGround(true, false);
        return true;
    }
    function touchdown() {
        gmode = 'landed'; camRelInit = false;
        aPlay('land', { vel: 1 });
        discPlanet(land.node);
        land.sPos.copy(land.gDir).multiplyScalar(land.rg); land.sQuat.copy(land.qTo);
        gA.copy(land.gDir).applyQuaternion(land.node.mesh.quaternion);
        gB.copy(shipRoot.position).addScaledVector(gA, -legDrop * L);
        for (var i = 0; i < 3; i++) fx.impact(gB, 0xc8b89a, 3);
        shake = Math.max(shake, 0.06 * L);
        land.settle = LAND_SETTLE; land.dustT = 0;
        if (land.auto) { land.auto = false; exitShip(); }      // rev 20: hold-E exit = land and step out in one motion
    }
    // rev 19: the landing sequence. The hull ships as ONE merged mesh (legs are not separate), so the legs "deploy" as the hull group sinking
    // 0.15 L onto them during the descent, then a damped settle (1.2 s) with dust puffs and the engine winding down.
    var LAND_SETTLE = 1.2;
    function hullY(y) { if (hullObj && hullObj.position.y !== y) hullObj.position.y = y; }
    function exitShip() {
        if (gmode !== 'landed' || !land.node) return;
        if (!hum.obj) {
            hum.obj = makeHuman(effColor());
            scene.add(hum.obj.group);
        } else hum.obj.setColor(effColor());
        // beside the ship (its right side), facing along the nose
        gA.copy(X).applyQuaternion(land.sQuat);                        // planet frame
        gB.copy(land.sPos).normalize();
        gC.copy(land.sPos).addScaledVector(gA, 0.75 * L).normalize();
        hum.pos.copy(gC).multiplyScalar(land.rg);
        gD.copy(hum.pos).applyQuaternion(land.node.mesh.quaternion).add(land.node.anchor.position);
        var fo = null; try { fo = ps.floorAt(gD, gFo); } catch (e) { fo = null; }
        hum.hr = fo && fo.r > 0 ? fo.r : land.rg - legDrop * L; hum.gr = hum.hr; hum.vv = 0; hum.air = false; hum.vh = 0;
        hum.pos.copy(gC).multiplyScalar(hum.hr);
        gD.copy(NEG_Z).applyQuaternion(land.sQuat); gD.addScaledVector(gB, -gD.dot(gB)); gD.normalize();
        hum.hf.copy(gD); hum.face.copy(gD); hum.pitch = 0.3; hum.moving = hum.running = false;
        hum.obj.group.visible = true;
        gmode = 'foot'; camRelInit = false; jumpHeld = true; mdx = mdy = 0;
        setGround(true, true);
        net && net.setMode && net.setMode('foot');
    }
    function jetKill() { jetT = 0; jetLvl = 0; flKill(); if (exJet) exOff(exJet); if (jetSnd) { jetSnd = false; aPlay('jetpackStop'); } }
    function boardShip() {
        jetKill();
        gmode = 'landed'; camRelInit = false;
        if (hum.obj) hum.obj.group.visible = false;
        setGround(true, false);
        net && net.setMode && net.setMode('landed');
    }
    function liftOff() {
        if (gmode !== 'landed' || !land.node) return;
        gA.copy(land.sPos).applyQuaternion(land.node.mesh.quaternion).normalize();     // world up
        gB.copy(shipRoot.position).addScaledVector(gA, -legDrop * L);
        for (var i = 0; i < 3; i++) fx.impact(gB, 0xc8b89a, 3);
        aPlay('liftoff', { vel: 1 });
        gmode = 'fly'; landOk = false; landCheckT = 0.6; lfOff(); lf.fromLand = true; lf.relNext = true;
        if (camera.near !== baseNear) { camera.near = baseNear; camera.updateProjectionMatrix(); }
        combatRoot.visible = true; setGround(false, false);
        vel.copy(gA).multiplyScalar(2.5 * CRUISE); speed = 1.5; throttle = 0.5; pulse = 0; pulseT = 0;
        shake = Math.max(shake, 0.075 * L); land.settle = 0; hullY(0);
        net && net.setMode && net.setMode('fly');
    }
    function leaveGround() {                    // Esc from any ground state: hand the ship back to the exit cinematic from where the player is
        jetKill();
        if (gmode === 'foot' && hum.obj && land.node) {
            shipRoot.position.copy(hum.obj.group.position);
            hum.obj.group.visible = false;
        }
        if (hum.obj) hum.obj.group.visible = false;
        if (camera.near !== baseNear) { camera.near = baseNear; camera.updateProjectionMatrix(); }
        gmode = 'fly'; landOk = false; lfOff(); setGround(false, false); land.settle = 0; hullY(0);
        net && net.setMode && net.setMode('fly');
    }
    // The engine runs the pilot step BEFORE updateBodies, so node.anchor / node.mesh.rotation are one frame stale here. For anything glued to a
    // planet's surface we write the CURRENT-frame pose (identical to what updateBodies is about to set) before reading it.
    var reducedM = false;
    try { reducedM = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches); } catch (e) { /* ignore */ }
    function syncPlanet(n) {
        if (!n || !n.anchor || !n.mesh) return;
        n.anchor.position.set(n.wx, n.ny || 0, n.wy);
        n.mesh.rotation.z = n.spinTilt; n.mesh.rotation.y = (reducedM ? 0 : engine.time) * n.spinRate + n.spinPhase;
    }

    // ─── rev 18: LOCAL-FRAME flight (docs/ship-mode.md "Revision 18") ─────────────────────────────────────────────
    // Inside LF_ON x R of a surface planet the ship is simulated in the planet's own rotating frame: lp / lq / lv are stored relative to the
    // planet anchor and its spin quaternion, and the world pose is derived at the START of every step from the planet's CURRENT pose (syncPlanet,
    // the same values updateBodies is about to write), so nothing is ever one frame stale and the old frame drag is gone. Everything else in
    // the fight rides the frame's translation (carryWorld). vel while the frame is on = the rotating-frame velocity in world axes.
    var lf = {
        on: false, node: null, rec: null, R: 0, cap: false, leaving: false, entryT: 0, entryV0: 0, hover: false, alt: 1e9, floorR: 0, atmK: 0,
        lp: new THREE.Vector3(), lq: new THREE.Quaternion(), lv: new THREE.Vector3(), endV: new THREE.Vector3(), endP: new THREE.Vector3(), endQ: new THREE.Quaternion(), qEnd: new THREE.Quaternion(),
        c0: new THREE.Vector3(), holdAlt: 0, relNext: false, fromLand: false, jump: 0, maxStep: 0, prevW: new THREE.Vector3(), prevOk: false, subN: 0
    };
    var lfQi = new THREE.Quaternion(), lfQt = new THREE.Quaternion(), lfA = new THREE.Vector3(), lfB = new THREE.Vector3(), lfC = new THREE.Vector3(), lfD = new THREE.Vector3(), lfU = new THREE.Vector3();
    var MOON_L = 60;     // MOONLET rule (rev 20): pilot rendered radius < 60 L = no atmosphere, no landing, hard sphere only (the 320 L floor means nothing is that small)
    function isMoonlet(n) { return realRadius(n) < MOON_L * L; }
    function hasSurface(n) { if (isMoonlet(n)) return false; var u = n.mesh && n.mesh.material && n.mesh.material.uniforms; return !!(u && u.uSeed && n.anchor && n.anchor.visible); }
    function lfOmega(n, p, c, out) { var w = reducedM ? 0 : n.spinRate; return out.set(w * (p.z - c.z), 0, -w * (p.x - c.x)); }   // spin is about world Y: w x r
    function carryWorld(ddx, ddy, ddz) {
        var i, P = shipRoot.position;
        for (i = 0; i < enemies.length; i++) if (enemies[i].alive && !enemies[i].isBoss) { enemies[i].g.position.x += ddx; enemies[i].g.position.y += ddy; enemies[i].g.position.z += ddz; enemies[i].wander.x += ddx; enemies[i].wander.y += ddy; enemies[i].wander.z += ddz; enemies[i].base.x += ddx; enemies[i].base.y += ddy; enemies[i].base.z += ddz; enemies[i].home.x += ddx; enemies[i].home.y += ddy; enemies[i].home.z += ddz; }
        bsig.org.x += ddx; bsig.org.y += ddy; bsig.org.z += ddz;
        for (i = 0; i < mines.length; i++) if (mines[i].active) { mines[i].m.position.x += ddx; mines[i].m.position.y += ddy; mines[i].m.position.z += ddz; }
        for (i = 0; i < allies.length; i++) if (allies[i].alive) { allies[i].g.position.x += ddx; allies[i].g.position.y += ddy; allies[i].g.position.z += ddz; }
        for (i = 0; i < bombs.length; i++) if (bombs[i].active) { bombs[i].m.position.x += ddx; bombs[i].m.position.y += ddy; bombs[i].m.position.z += ddz; }
        for (i = 0; i < drops.length; i++) if (drops[i].active) { drops[i].m.position.x += ddx; drops[i].m.position.y += ddy; drops[i].m.position.z += ddz; }
        for (i = 0; i < orbs.length; i++) if (orbs[i].active) { orbs[i].m.position.x += ddx; orbs[i].m.position.y += ddy; orbs[i].m.position.z += ddz; }
        for (i = 0; i < crates.length; i++) if (crates[i].active) { crates[i].g.position.x += ddx; crates[i].g.position.y += ddy; crates[i].g.position.z += ddz; crates[i].prev.x += ddx; crates[i].prev.y += ddy; crates[i].prev.z += ddz; }
        qCarry(ddx, ddy, ddz);
        for (i = 0; i < boomQ.length; i++) if (boomQ[i].on) { boomQ[i].p.x += ddx; boomQ[i].p.y += ddy; boomQ[i].p.z += ddz; }
        var bpa = fx.bolts.geometry.attributes.aPos.array;
        for (i = 0; i < bolts.length; i++) if (bolts[i].active) { var bi3 = bolts[i].id * 3; bpa[bi3] += ddx; bpa[bi3 + 1] += ddy; bpa[bi3 + 2] += ddz; bolts[i].prev.x += ddx; bolts[i].prev.y += ddy; bolts[i].prev.z += ddz; }
    }
    function lfOff() { lf.on = false; lf.cap = false; lf.leaving = false; lf.entryT = 0; lf.hover = false; lf.alt = 1e9; lf.prevOk = false; }
    function lfEntry(v0) { lf.cap = true; lf.entryT = ENTRY_T; lf.entryV0 = Math.max(v0, ATM_BOOST); lf.leaving = false; }
    // gather: current pose of every body + its world velocity (history), then (re)derive the ship's world pose from the local state
    function lfBegin(dt, bodies, P) {
        var i, rb, ap;
        stepN++;
        for (i = 0; i < bodies.length; i++) {
            rb = bodies[i]; syncPlanet(rb.node); ap = rb.node.anchor.position;
            if (rb.pN === stepN - 1 && dt > 1e-5) { rb.vx = (ap.x - rb.px) / dt; rb.vy = (ap.y - rb.py) / dt; rb.vz = (ap.z - rb.pz) / dt; } else { rb.vx = rb.vy = rb.vz = 0; }
            rb.px = ap.x; rb.py = ap.y; rb.pz = ap.z; rb.pN = stepN;
        }
        var n, c, q, Rn;
        if (lf.on) {
            n = lf.node; c = n.anchor.position; q = n.mesh.quaternion; Rn = n.mesh.scale.x;
            if (!(Rn > 0) || !n.anchor.visible) lfOff();
            else {
                if (Math.abs(Rn / lf.R - 1) > 1e-7) { lf.lp.multiplyScalar(Rn / lf.R); lf.R = Rn; }
                lfQi.copy(q).invert();
                if (vel.distanceToSquared(lf.endV) > 1e-12) lf.lv.copy(vel).applyQuaternion(lfQi);                                                      // velocity set from outside (debug / liftoff): take it as the local velocity
                if (Math.abs(shipRoot.quaternion.dot(lf.endQ)) < 1 - 1e-10) { lfQt.copy(lf.qEnd).invert(); lf.lq.copy(lfQt).multiply(shipRoot.quaternion); }      // orientation set from outside (debug): keep it
                if (P.distanceToSquared(lf.endP) > 1e-6 * L * L) {                                                                                      // position set from outside (respawn / debug): re-derive
                    lf.lp.copy(P).sub(c).applyQuaternion(lfQi); lf.jump++;
                    if (lf.lp.length() > LF_ON * Rn) { lfOff(); n = null; }
                }
                if (n === null) { /* teleported out of the frame: fall through to the engage test with the world pose as it is */ }
                else {
                var dx = c.x - lf.c0.x, dy = c.y - lf.c0.y, dz = c.z - lf.c0.z;
                P.copy(lf.lp).applyQuaternion(q).add(c);
                shipRoot.quaternion.copy(q).multiply(lf.lq);
                vel.copy(lf.lv).applyQuaternion(q);
                if (lf.prevOk) carryWorld(dx, dy, dz);
                lf.prevOk = true;
                return;
                }
            }
        }
        // not in a frame: engage on the nearest surface planet inside LF_ON x R (needs ps active on the same body)
        if (!ps) return;
        var best = null, bd = LF_ON, br = null;
        for (i = 0; i < bodies.length; i++) {
            rb = bodies[i]; n = rb.node;
            if (!hasSurface(n)) continue;
            Rn = n.mesh.scale.x; if (!(Rn > 0)) continue;
            var d = P.distanceTo(n.anchor.position) / Rn;
            if (d < bd) { bd = d; best = n; br = rb; }
        }
        if (!best || ps.active !== best) { lf.fromLand = false; lf.relNext = false; return; }
        n = best; c = n.anchor.position; q = n.mesh.quaternion; Rn = n.mesh.scale.x;
        lfQi.copy(q).invert();
        lf.node = n; lf.rec = br; lf.R = Rn;
        if (lf.fromLand && land.node === n) {
            lf.lp.copy(land.sPos); lf.lq.copy(land.sQuat); lf.lv.copy(land.sPos).normalize().multiplyScalar(2.5 * CRUISE);
        } else {
            lf.lp.copy(P).sub(c).applyQuaternion(lfQi);
            lf.lq.copy(lfQi).multiply(shipRoot.quaternion);
            lfA.copy(vel);
            if (!lf.relNext) { lfA.x -= br.vx; lfA.y -= br.vy; lfA.z -= br.vz; lfOmega(n, P, c, lfB); lfA.sub(lfB); }          // world -> local: subtract the planet's velocity and the spin
            lf.lv.copy(lfA).applyQuaternion(lfQi);
        }
        lf.fromLand = false; lf.relNext = false;
        lf.on = true; lf.cap = false; lf.leaving = false; lf.entryT = 0; lf.c0.copy(c); lf.prevOk = false; lf.atmK = 0;
        P.copy(lf.lp).applyQuaternion(q).add(c); shipRoot.quaternion.copy(q).multiply(lf.lq); vel.copy(lf.lv).applyQuaternion(q);
        if (lf.lp.length() < ATM_R * Rn) lfEntry(lf.lv.length());
        lfAlt();
        if (resumeAlt >= 0) {            // rev 25: Esc / re-enter keeps the altitude above the (re-seeded) ground, not the raw radius
            lf.lp.setLength(lf.floorR + resumeAlt); resumeAlt = -1;
            P.copy(lf.lp).applyQuaternion(q).add(c); lfAlt();
        }
    }
    function lfAlt() {
        var n = lf.node, r = lf.lp.length();
        lf.floorR = (ps && ps.active === n) ? ps.floorLocal(lf.lp.x, lf.lp.y, lf.lp.z) : lf.R * 1.04;
        lf.alt = r - lf.floorR;
        var hv = lf.cap && lf.alt < (lf.hover ? 1.2 : 1) * HOVER_L * L;          // hysteresis: in at 3 L, out at 3.6 L
        if (hv && !lf.hover) lf.holdAlt = Math.max(CLEAR_L, lf.alt / L);
        lf.hover = hv;
    }
    // approach governor + atmosphere caps: nothing arrives at the 1.4 R shell faster than ATM_PULSE (the radial/total speed is limited by the
    // distance left to the shell / APP_T), and once ENTRY starts the speed spools down to ATM_BOOST over ENTRY_T and can never leave the shell.
    function lfGovern(dt, bodies, P) {
        var i;
        if (!lf.cap) {
            if (!ps) return;
            for (i = 0; i < bodies.length; i++) {
                var b = bodies[i], n = b.node;
                if (!hasSurface(n)) continue;
                var Rn = n.mesh.scale.x, c = n.anchor.position;
                lfA.subVectors(P, c); var d = lfA.length();
                if (d > 8 * Rn || d < ATM_R * Rn) continue;
                lfA.divideScalar(d);
                var own = lf.on && lf.node === n;
                lfB.copy(vel); if (!own) { lfB.x -= b.vx; lfB.y -= b.vy; lfB.z -= b.vz; }
                if (lfB.dot(lfA) > 0.2) continue;                                      // receding (rev 23: a tangential graze is capped too, or it entered the shell at orbital speed)
                var vcap = Math.max(ATM_PULSE, (d - ATM_R * Rn) / APP_T), vt = lfB.length();
                if (vt > vcap) {
                    lfB.multiplyScalar(vcap / vt);
                    if (!own) { lfB.x += b.vx; lfB.y += b.vy; lfB.z += b.vz; }
                    vel.copy(lfB);
                    if (Math.abs(speed) > vcap) speed = speed < 0 ? -vcap : vcap;
                    if (pulseT > 0) pulseT = Math.max(1e-4, Math.min(pulseT, PULSE_E * Math.log(vcap / BOOST)));
                }
            }
            return;
        }
        if (lf.entryT > 0) lf.entryT = Math.max(0, lf.entryT - dt);
        var vmax = ATM_PULSE;
        if (lf.entryT > 0) { var f = Math.max(0, (lf.entryT / ENTRY_T - 0.08) / 0.92); vmax = ATM_BOOST + (lf.entryV0 - ATM_BOOST) * f * f; }      // rev 23: the brake is done at 92 % of ENTRY_T
        var vl = vel.length();
        if (vl > vmax) vel.multiplyScalar(vmax / vl);
        if (Math.abs(speed) > vmax) speed = speed < 0 ? -vmax : vmax;
        lfA.subVectors(P, lf.node.anchor.position); var r = lfA.length(); lfA.divideScalar(r || 1);
        var dd = -vel.dot(lfA), ddm = Math.max(ATM_PULSE, lf.alt / APP_T);            // descent governor: you can always stop before the floor
        if (dd > ddm) vel.addScaledVector(lfA, dd - ddm);
        if (lf.entryT > 0 && r > (ATM_R - 0.03) * lf.R) { var vo = vel.dot(lfA); if (vo > 0) vel.addScaledVector(lfA, -vo); bounceV.addScaledVector(lfA, -Math.max(0, bounceV.dot(lfA))); }   // ENTRY: nothing pushes you back out
    }
    // swept, substepped motion in the LOCAL frame against the local height function (ps.floorLocal is a pure function of the local position)
    var deckMsgT = -9, deckBounces = 0;
    function lfMove(dt) {
        var n = lf.node, c = n.anchor.position, q = n.mesh.quaternion, R = lf.R, P = shipRoot.position;
        lfQi.copy(q).invert();
        var lp = lf.lp, lv = lfB.copy(vel).add(bounceV).applyQuaternion(lfQi), lv0x = lv.x, lv0y = lv.y, lv0z = lv.z;
        lp.copy(P).sub(c).applyQuaternion(lfQi);
        if (lf.cap && ps && ps.active === n && ps.wind && R27.windOn) lp.addScaledVector(lfC.copy(ps.wind).applyQuaternion(lfQi), dt);      // rev 27: the air carries the ship (ps.wind is 0 outside the atmosphere)
        lfU.copy(lp).normalize().applyQuaternion(q);                                 // world up before the move (for the horizon transport)
        var rem = dt, it = 0, zone = 1.25 * R, atmR = ATM_R * R, MAXIT = 24, sub = 0, hit = false;
        while (rem > 1e-9 && it++ < MAXIT) {
            var sp = lv.length(), r = lp.length(), h = rem;
            if (sp > 1e-9) {
                if (r - zone < sp * h * 1.05) h = Math.min(h, Math.max(SUB_L * L / sp, dt / MAXIT));
                if (!lf.cap && r > atmR) {
                    var b = lp.dot(lv), a = sp * sp, cc = r * r - atmR * atmR, disc = b * b - a * cc;
                    if (b < 0 && disc >= 0) { var tc = (-b - Math.sqrt(disc)) / a; if (tc >= 0 && tc <= h) { h = tc; hit = true; } }
                }
            }
            lp.addScaledVector(lv, h); rem -= h; sub++;
            if (hit) { hit = false; lfEntry(lv.length()); lp.multiplyScalar(atmR * 0.999999 / lp.length()); }
            var r2 = lp.length();
            if (lf.entryT > 0 && r2 > atmR * 0.99999) { lp.multiplyScalar(atmR * 0.99999 / r2); r2 = atmR * 0.99999; }
            if (r2 < zone) {
                var fl = ((ps && ps.active === n) ? ps.floorLocal(lp.x, lp.y, lp.z) : R * 1.04) + CLEAR_L * L;
                if (r2 < fl) {
                    lp.multiplyScalar(fl / r2);
                    lfC.copy(lp).divideScalar(fl);
                    var vn = lv.dot(lfC);
                    if (vn < 0) { if (-vn > BOOST) { lfD.copy(lfC).applyQuaternion(q); terrainHit(-vn, lfD); } lv.addScaledVector(lfC, -vn); }
                }
            }
        }
        lf.subN = sub;
        if (lf.hover && lf.cap) {
            // hover = terrain following: keep the altitude above the ground under the ship (the user's own climb / dive still moves it)
            var rr = lp.length(), fl2 = (ps && ps.active === n) ? ps.floorLocal(lp.x, lp.y, lp.z) : R * 1.04, vrad = (lv.x * lp.x + lv.y * lp.y + lv.z * lp.z) / (rr || 1);
            lf.holdAlt = clamp(lf.holdAlt + vrad * dt / L, CLEAR_L, 1.2 * HOVER_L + 0.2);
            var tr = fl2 + lf.holdAlt * L;
            var rn = rr + (tr - rr) * damp(8, dt); rn = Math.max(rn, fl2 + CLEAR_L * L);
            lp.multiplyScalar(rn / rr);
        }
        P.copy(lp).applyQuaternion(q).add(c);
        lfC.set(lv.x - lv0x, lv.y - lv0y, lv.z - lv0z).applyQuaternion(q);          // what the floor took away (local axes -> world)
        vel.add(lfC);
        if (bounceV.lengthSq() > 1e-12) bounceV.multiplyScalar(Math.exp(-2.5 * dt));
        if (lf.cap) {
            // the horizon follows: parallel-transport the attitude and velocity as the ship moves over the curved ground
            lfD.copy(lp).normalize().applyQuaternion(q);
            lfQt.setFromUnitVectors(lfU, lfD);
            shipRoot.quaternion.premultiply(lfQt); vel.applyQuaternion(lfQt);
        }
    }
    // lift: with no pitch input the nose eases to the horizon (a climb attitude steeper than ~30 deg is held), and the wings level out
    function lfLevel(dt) {
        if (!lf.cap || dead || flipT > 0) return;
        var q = lf.node.mesh.quaternion, c = lf.node.anchor.position;
        lfA.subVectors(shipRoot.position, c).normalize();
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
        var e = Math.asin(clamp(vF.dot(lfA), -1, 1)), wP = (Math.abs(pitRate) < 0.12 ? 1 : 0) * (1 - clamp((Math.abs(e) - 0.45) / 0.35, 0, 1));
        if (wP > 0 && Math.abs(e) > 1e-4) {
            lfB.copy(vF).addScaledVector(lfA, -vF.dot(lfA)); var bl = lfB.length();
            if (bl > 1e-5) { lfB.divideScalar(bl); lfQt.setFromUnitVectors(vF, lfB); qA.identity().slerp(lfQt, 1 - Math.exp(-1.6 * wP * dt)); shipRoot.quaternion.premultiply(qA).normalize(); vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); }
        }
        if (!keys.KeyA && !keys.KeyD && rollT <= 0 && Math.abs(vF.dot(lfA)) < 0.95) {
            lfB.crossVectors(vF, lfA).normalize();                                    // level right
            lfC.copy(X).applyQuaternion(shipRoot.quaternion);                          // actual right
            var ang = Math.atan2(lfD.crossVectors(lfC, lfB).dot(vF), lfC.dot(lfB)), wR = 1 - clamp(Math.abs(yawRate) / 0.5, 0, 1);
            shipRoot.quaternion.premultiply(qA.setFromAxisAngle(vF, ang * (1 - Math.exp(-2.2 * wR * dt)))).normalize();
        }
    }
    function lfRelease() {
        var n = lf.node, c = n.anchor.position, q = n.mesh.quaternion, P = shipRoot.position;
        vel.copy(lf.lv).applyQuaternion(q);
        if (lf.rec) { vel.x += lf.rec.vx; vel.y += lf.rec.vy; vel.z += lf.rec.vz; }
        lfOmega(n, P, c, lfB); vel.add(lfB);
        lfOff();
    }
    function lfCommit(dt) {
        if (!lf.on) return;
        var n = lf.node, c = n.anchor.position, q = n.mesh.quaternion, P = shipRoot.position, R = lf.R;
        lfQi.copy(q).invert();
        lf.lp.copy(P).sub(c).applyQuaternion(lfQi);
        if (lf.cap || lf.lp.length() < 1.25 * R) {                          // whatever moved the ship after lfMove (dodge sidestep, ram) may not leave it inside the ground
            var rr0 = lf.lp.length(), fl0 = (ps && ps.active === n) ? ps.floorLocal(lf.lp.x, lf.lp.y, lf.lp.z) : R * 1.04;
            if (rr0 < fl0 + CLEAR_L * L) { lf.lp.multiplyScalar((fl0 + CLEAR_L * L) / rr0); P.copy(lf.lp).applyQuaternion(q).add(c); }
        }
        lf.lq.copy(lfQi).multiply(shipRoot.quaternion);
        lf.lv.copy(vel).applyQuaternion(lfQi);
        lf.endP.copy(P); lf.endV.copy(vel); lf.endQ.copy(shipRoot.quaternion); lf.qEnd.copy(q); lf.c0.copy(c);
        var r = lf.lp.length();
        if (lf.cap) {
            if (r > ATM_R * R) {
                if (!lf.leaving && lf.entryT <= 0 && lf.lv.dot(lf.lp) > 0) { lf.leaving = true; shake = Math.max(shake, 0.165 * L); fovKick = Math.max(fovKick, 3); }      // rev 19: shake 15 % of 1.1 L
            } else lf.leaving = false;
            if (r > LF_OFF * R) { lfRelease(); return; }
        } else if (r > LF_OFF * R) { lfRelease(); return; }
        lfAlt();
        // HUD altitude
        var txt = '';
        if (lf.cap || lf.alt < 4 * R * 0.4) { var aL = lf.alt / L; txt = 'ALT ' + (aL < 10 ? aL.toFixed(1) : (aL >= 1000 ? (aL / 1000).toFixed(1) + 'k' : Math.round(aL))) + ' L'; }
        if (txt !== cAltTxt) { cAltTxt = txt; elAlt.textContent = txt; elAlt.classList.toggle('is-on', !!txt); }
    }
    var landReq = false;
    // rev 20: hold E (EH_T s, radial fill on the HUD). Under EH_ALT L of altitude the ship auto-lands and you step out in one motion; on foot beside the ship it boards and lifts off.
    var eh = { on: false, held: 0, t: 0 }, cEhP = -1, cEhT = '';
    function ehContext() {
        if (cmdOpen || dead || boarding || exiting || state !== 'piloting') return '';
        if (gmode === 'fly') return (lf.on && lf.cap && lf.alt < EH_ALT * L) ? 'exit' : '';
        if (gmode === 'foot') return nearShip() ? 'board' : '';
        if (gmode === 'sfoot' && sk.mode === 'deck' && !storeOpen) return stationInteract().kind === 'ship' ? 'board' : '';
        return '';
    }
    function ehShow(p, ctx) {
        var v = p > 0 ? Math.round(p * 60) / 60 : 0;
        if (v !== cEhP) { cEhP = v; elEh.style.setProperty('--p', (v * 100).toFixed(1)); elEh.classList.toggle('is-on', v > 0); }
        var tx = v > 0 ? (ctx === 'exit' ? 'EXIT SHIP' : (ctx === 'board' ? 'LIFT OFF' : '')) : '';
        if (tx !== cEhT) { cEhT = tx; elEhT.textContent = tx; }
    }
    function ehExit() {
        var okL = true, why = '';
        if (ps) { try { var lr = ps.landable(shipRoot.position); okL = !!lr.ok; why = lr.why || ''; } catch (e) { okL = true; } }
        if (!okL) { announce(why === 'water' ? 'WATER · FIND LAND' : (why === 'slope' ? 'TOO STEEP · MOVE ON' : 'CANNOT LAND HERE')); return; }
        if (startLanding()) { land.auto = true; land.T = 1.1; }
    }
    function ehTick(dt) {
        if (!eh.on) return;
        if (!keys.KeyF) { eh.on = false; eh.t = 0; ehShow(0, ''); return; }
        eh.held += dt;
        var c = ehContext();
        if (!c) { eh.t = 0; ehShow(0, ''); return; }
        eh.t += dt;
        if (eh.t < EH_T) { ehShow(eh.t / EH_T, c); return; }
        eh.on = false; eh.t = 0; ehShow(0, '');
        if (c === 'exit') ehExit();
        else if (gmode === 'sfoot') beginLaunch();
        else { boardShip(); liftOff(); }
    }
    function onKeyF() {
        if (gmode === 'fly') { if (cvHail()) return; if (landOk) landReq = true; else if (target && !dead) exit(target.node); }
        else if (gmode === 'landed') exitShip();
        else if (gmode === 'foot') {
            var ft = footTarget();
            if (ft.kind === 'store') { questDeliver(ft.obj); openStore(ft.obj); } else if (ft.kind === 'checkout') r28Checkout(); else if (ft.kind === 'shelf') r28Grab(ft.obj); else if (ft.kind === 'burger') openBurger(ft.obj); else if (ft.kind === 'dealer') { if (!r28Talk({ id: ft.obj.id, name: ft.obj.name || 'DEALER', role: 'dealer', kind: 'dealer', obj: ft.obj, seed: ft.obj.seed })) openDealer(ft.obj); }
            else if (ft.kind === 'shopper') talkShopper(ft.obj); else if (ft.kind === 'clerk') talkNpc(ft.obj); else if (nearShip()) { if (R25.noInt || !enterInterior()) boardShip(); }
            else if (ft.kind === 'creature') tameStart(ft.obj);
            else if (ft.kind === 'resource') harvStart(ft.obj);
        }
        else if (gmode === 'ifoot') interiorF();
        else if (gmode === 'sfoot') stationE();
    }
    // rev 19: "near the ship" = within BOARD_L ship lengths of the spot on the ground directly under the hull (the old test measured to the hull origin, ~1.1 L above the feet, so it was never true on foot)
    var BOARD_L = 3.5, nsV = new THREE.Vector3();
    function nearShip() {
        if (!hum.obj || !land.node || gmode !== 'foot') return false;
        var sl = land.sPos.length() || 1;
        nsV.copy(land.sPos).multiplyScalar((sl - legDrop * L) / sl);
        return hum.pos.distanceTo(nsV) < BOARD_L * L;
    }
    // rev 19: lift the camera out of the ground. Works in world space against the active planet's floor under the camera.
    var ccFo = { r: 0, n: new THREE.Vector3() }, ccV = new THREE.Vector3();
    function clampCamToGround(clearL) {
        if (!ps || !ps.active) return;
        var n = ps.active, c = n.anchor.position, cp = camera.position;
        ccV.subVectors(cp, c); var r = ccV.length(); if (r > 1.3 * n.mesh.scale.x || r < 1e-6) return;
        var fo = null; try { fo = ps.floorAt(cp, ccFo); } catch (e) { fo = null; }
        if (!fo || !(fo.r > 0)) return;
        var minR = fo.r + clearL * L;
        if (r < minR) cp.copy(c).addScaledVector(ccV, minR / r);
    }
    var cLandBig = false;
    function setPrompt(t, big) {        // rev 19: big = the huge centred E · LAND / E · BOARD call to action
        big = !!big && !!t;
        if (t !== cLandTxt) { if (t) aPlay('ui', { vel: 0.5 }); cLandTxt = t; elLand.textContent = t; elLand.classList.toggle('is-on', !!t); }
        if (big !== cLandBig) { cLandBig = big; elLand.classList.toggle('is-big', big); }
    }
    // first-person-ish third-person orbit camera helper: camera follows `focus` with its offset smoothed in the focus frame (the world moves under us)
    function groundCam(dt, focus, tp, tq, rate) {
        if (!camRelInit) { camRel.copy(camera.position).sub(focus); camRelInit = true; }
        gE.copy(tp).sub(focus);
        camRel.lerp(gE, damp(rate, dt));
        camera.position.copy(focus).add(camRel);
        camera.quaternion.slerp(tq, damp(rate, dt));
    }
    var G_WALK = 22, G_RUN = 44, G_GRAV = 22, G_JUMP = 8.5, FOOT_CAM = 6;   // in human heights (/s, /s^2); rev 29: walk = the old run speed, Shift run = 2x
    var FL_HOLD = 2, FL_V = 36;               // rev 29: hold Space this many s = jetpack FLIGHT; flight cruise speed (heights/s)
    // rev 20b: the foot controller. The human lives in the planet's LOCAL frame (hum.pos), is a capsule (feet footprint FOOT_R) and walks on the ground AS RENDERED
    // (ps.meshFloorLocal: the patch's own triangles, not the finer height function that the mesh only approximates). Motion is substepped (<= FOOT_SUB heights per
    // substep) with a slope-limited step test (rise <= run + 0.05 H) and wall sliding; the position is written once per frame. hum.vh is the VISUAL height (what the
    // human mesh and camera follow): it eases toward the physical height but never sinks below the ground.
    var FOOT_R = 0.22, FOOT_SUB = 0.12, FOOT_SLIDE_A = [0, 0.7, -0.7, 1.4, -1.4], FOOT_SLIDE_K = [1, 0.8, 0.8, 0.5, 0.5];
    var fsD = new THREE.Vector3(), fsR = new THREE.Vector3(), fsF = new THREE.Vector3(), fsC = new THREE.Vector3(), fsT = new THREE.Vector3();
    function mFloor(x, y, z) { var r = ps.meshFloorLocal ? ps.meshFloorLocal(x, y, z) : ps.floorLocal(x, y, z); return r > 0 ? r : hum.gr; }
    // ground radius under the footprint: max over the centre and four points at FOOT_R human heights around it (dir = unit local direction)
    function footFloor(dir, H) {
        var a = FOOT_R * H / (ps.radius || 1), best = mFloor(dir.x, dir.y, dir.z), v;
        fsR.crossVectors(hum.hf, dir).normalize(); fsF.copy(hum.hf);
        v = mFloor(dir.x + fsR.x * a, dir.y + fsR.y * a, dir.z + fsR.z * a); if (v > best) best = v;
        v = mFloor(dir.x - fsR.x * a, dir.y - fsR.y * a, dir.z - fsR.z * a); if (v > best) best = v;
        v = mFloor(dir.x + fsF.x * a, dir.y + fsF.y * a, dir.z + fsF.z * a); if (v > best) best = v;
        v = mFloor(dir.x - fsF.x * a, dir.y - fsF.y * a, dir.z - fsF.z * a); if (v > best) best = v;
        return best;
    }
    // jetpack flame: two fx cones (the ship's exhaust shader) under the backpack tanks, rotated to point down; throttled audio loop; ground dust when low
    var exJet = null, jetT = 0, jetLvl = 0, jetAud = 0, jetSnd = false, jetDust = 0;
    function jetFx(dt, jet, H, qM, c, dir, floorR, hr) {
        jetLvl += ((+jet || 0) - jetLvl) * damp(jet ? 9 : 12, dt);
        if (jetLvl < 0.03) {
            if (exJet && exJet.holder.visible) exOff(exJet);
            if (jetSnd) { jetSnd = false; aPlay('jetpackStop'); }
            return;
        }
        var nz = hum.obj.nozzles ? hum.obj.nozzles() : null, nzk = nz ? nz.length : -1;
        if (!exJet || exJet.nzk !== nzk) {
            if (!exJet) exJet = makeEx(scene);
            var thr = nz && nz.length ? nz.map(function (n) { return new THREE.Vector3(n.x, n.z, -n.y); }) : [new THREE.Vector3(-0.058, 0.2, -0.43), new THREE.Vector3(0.058, 0.2, -0.43)];       // human-local nozzle (x,y,z) -> holder frame (rotated +90 deg about X)
            exSetup(exJet, thr, 0xffa040); exJet.nzk = nzk;
        }
        exJet.holder.visible = true;
        qA.copy(hum.obj.group.quaternion).multiply(qB.setFromAxisAngle(X, Math.PI / 2));          // holder +Z (the cone apex) -> human -Y (down)
        exUpdate(exJet, hum.obj.group.position, qA, H / 0.07, 0.5 + 0.5 * jetLvl);
        exJet.holder.scale.setScalar(0.07);
        jetAud -= dt;
        if (jetAud <= 0) { jetAud = 0.12; jetSnd = true; aPlay('jetpack', { level: 0.35 + 0.65 * jetLvl }); }
        if (jet && hr - floorR < 5 * H) {
            jetDust -= dt;
            if (jetDust <= 0) { jetDust = 0.09; fsT.copy(dir).multiplyScalar(floorR).applyQuaternion(qM).add(c); fx.impact(fsT, 0xc8b89a, 1.6); }
        }
    }

    // ─── rev 29: solid objects on the planet (store + burger-house boxes, the parked hull) and jetpack FLIGHT ───────────────────────────────────────────────
    var fl = { on: false, pit: 0, wy: 0, wp: 0, th: 0, cr: 0, lock: false, tag: '', v: new THREE.Vector3(), alt: 0 }, flT = 0;
    var flF = new THREE.Vector3(), flA = new THREE.Vector3();
    var elFlight = document.createElement('div'); elFlight.className = 'sh-flight'; hud.appendChild(elFlight);
    function flHud(on, alt) {
        var t = on ? 'FLIGHT · ' + Math.round(alt) + ' H' : '';
        if (t !== fl.tag) { fl.tag = t; elFlight.textContent = t; elFlight.classList.toggle('is-on', on); }
    }
    function flKill() { if (!fl) return; fl.on = false; fl.v.set(0, 0, 0); fl.th = 0; fl.cr = 0; flT = 0; flHud(false, 0); }
    var opList = [], opPool = [], opA = new THREE.Vector3(), opB = new THREE.Vector3(), opC = new THREE.Vector3(), opU = new THREE.Vector3(), opBhI = new THREE.Matrix4(), OP_HS = [0.25, 0.75];
    function objBegin() {      // the solid sets for this frame (planet-local frame); returns how many
        var n = 0, i, s, I, bh = world && world.burgerHouse;
        if (gmode !== 'foot') { opList.length = 0; return 0; }
        function put(m, inv, K, hw, hd, hh, a, b) { var e = opPool[n] || (opPool[n] = {}); e.m = m; e.inv = inv; e.K = K || 1; e.hw = hw; e.hd = hd; e.hh = hh; e.a = a; e.b = b; opList[n++] = e; }
        if (world && world.node && world.stores) for (i = 0; i < world.stores.length; i++) {
            s = world.stores[i]; I = s.interior;
            if (I && I.matrix && I.inverse && I.walls) put(I.matrix, I.inverse, I.unit, (I.size ? I.size.w : 40) / 2 + 1, (I.size ? I.size.d : 26) / 2 + 1, (I.size ? I.size.h : 16) + 1, I.walls, I.aisles);
        }
        if (world && world.node && bh && bh.walls && bh.matrix) put(bh.matrix, opBhI.copy(bh.matrix).invert(), bh.unit, 6, 5, 8, bh.walls, null);
        opList.length = n;
        return 1;     // the hull is always a solid
    }
    function opBoxes(arr, rs) {      // push opA (store metres) out of a box list; true if it moved
        var j, q, moved = false, cx, cy, cz, dx, dy, dz, d2, d, k, l, r, t, bb, u, dd;
        if (!arr) return false;
        for (j = 0; j < arr.length; j++) {
            q = arr[j];
            if (opA.x < q.min.x - rs || opA.x > q.max.x + rs || opA.y < q.min.y - rs || opA.y > q.max.y + rs || opA.z < q.min.z - rs || opA.z > q.max.z + rs) continue;
            cx = clamp(opA.x, q.min.x, q.max.x); cy = clamp(opA.y, q.min.y, q.max.y); cz = clamp(opA.z, q.min.z, q.max.z);
            dx = opA.x - cx; dy = opA.y - cy; dz = opA.z - cz; d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= rs * rs) continue;
            moved = true;
            if (d2 > 1e-14) { d = Math.sqrt(d2); k = (rs - d) / d; opA.x += dx * k; opA.y += dy * k; opA.z += dz * k; }
            else {      // centre inside the box: leave by the nearest face
                l = opA.x - q.min.x; r = q.max.x - opA.x; t = opA.z - q.min.z; bb = q.max.z - opA.z; u = opA.y - q.min.y; dd = q.max.y - opA.y;
                var m = Math.min(l, r, t, bb, u, dd);
                if (m === l) opA.x = q.min.x - rs; else if (m === r) opA.x = q.max.x + rs; else if (m === t) opA.z = q.min.z - rs; else if (m === bb) opA.z = q.max.z + rs; else if (m === u) opA.y = q.min.y - rs; else opA.y = q.max.y + rs;
            }
        }
        return moved;
    }
    // the capsule = two spheres (0.25 H and 0.75 H above the feet, FOOT_R H radius) against every box and the hull sphere; p (planet-local feet) is pushed out in place
    function objPush(p, H) {
        var pushed = false, li, si, o, rs, mv, dl;
        opU.copy(p).normalize();
        for (li = 0; li < opList.length; li++) {
            o = opList[li]; rs = FOOT_R * H / o.K;
            for (si = 0; si < 2; si++) {
                opA.copy(p).addScaledVector(opU, OP_HS[si] * H).applyMatrix4(o.inv);
                if (Math.abs(opA.x) > o.hw || Math.abs(opA.z) > o.hd || opA.y < -2 || opA.y > o.hh) continue;
                opB.copy(opA);
                mv = opBoxes(o.a, rs); if (opBoxes(o.b, rs)) mv = true;
                if (!mv) continue;
                opC.copy(opA).applyMatrix4(o.m); opB.applyMatrix4(o.m); opC.sub(opB); p.add(opC); pushed = true;
            }
        }
        if (land.node && land.sPos) {      // the parked hull: a sphere of 0.45 L at the ship's centre
            var hr0 = 0.45 * L;
            for (si = 0; si < 2; si++) {
                opA.copy(p).addScaledVector(opU, OP_HS[si] * H).sub(land.sPos); dl = opA.length();
                var need = hr0 + FOOT_R * H;
                if (dl < need) { if (dl < 1e-9) opA.copy(opU), dl = 1; p.addScaledVector(opA, (need - dl) / dl); pushed = true; }
            }
        }
        return pushed;
    }
    function flightStep(dt, c, qM) {
        var H = 0.09 * L, o = hum.obj, up = hum.up, k, v = fl.v;
        if (cmdOpen) { mdx = mdy = 0; }
        up.copy(hum.pos).normalize();
        var wy = clamp(mdx * MOUSE_SENS * 1.3 / dt, -3, 3), wp = clamp(mdy * MOUSE_SENS * 1.3 / dt, -2.5, 2.5);
        mdx = mdy = 0;
        fl.wy += (wy - fl.wy) * damp(7, dt); fl.wp += (wp - fl.wp) * damp(7, dt);      // weighted: the body lags the mouse like the ship does, lighter
        hum.hf.applyAxisAngle(up, -fl.wy * dt);
        hum.hf.addScaledVector(up, -hum.hf.dot(up));
        if (hum.hf.lengthSq() < 1e-8) hum.hf.crossVectors(up, X);
        hum.hf.normalize();
        var kW = !cmdOpen && !!keys.KeyW, kS = !cmdOpen && !!keys.KeyS, kSp = !cmdOpen && !!keys.Space, kSh = !!(keys.ShiftLeft || keys.ShiftRight), thr = kW || kSp;
        fl.pit = clamp(fl.pit - fl.wp * dt, -1.3, 1.3);
        if (!thr) fl.pit *= Math.exp(-1.6 * dt);                           // auto-level
        flF.copy(hum.hf).multiplyScalar(Math.cos(fl.pit)).addScaledVector(up, Math.sin(fl.pit));
        var vmax = FL_V * H * (1 + 0.12 * R27.su.jetpack) * (kSh ? 2 : 1) * fxSpeed;
        if (kW) v.addScaledVector(flF, 70 * H * (kSh ? 1.6 : 1) * dt);
        if (kSp) v.addScaledVector(up, 60 * H * (kSh ? 1.4 : 1) * dt);
        if (kS) v.multiplyScalar(Math.exp(-5 * dt));                         // brake
        v.multiplyScalar(Math.exp(-(thr ? 0.35 : 0.9) * dt));
        if (!kSp) v.addScaledVector(up, -G_GRAV * H * (kW ? 0.1 : 0.6) * dt);   // glide down
        var vl = v.length(); if (vl > vmax) { v.multiplyScalar(vmax / vl); vl = vmax; }
        var nSub = clamp(Math.ceil(vl * dt / (0.3 * H)), 1, 40), h = dt / nSub, dir = fsD.copy(hum.pos).normalize(), hr = hum.hr, sup = footFloor(dir, H), touched = false, vUp0 = v.dot(up), opN = objBegin(), vn, ln;
        if (hr - sup > 400 * H && vUp0 > 0) v.addScaledVector(up, -vUp0);
        for (k = 0; k < nSub; k++) {                                       // swept, substepped: each substep is <= 0.3 H, the capsule radius is 0.22 H
            fsC.copy(dir).multiplyScalar(hr).addScaledVector(v, h);
            if (opN) {
                fsT.copy(fsC);
                if (objPush(fsC, H)) {
                    flA.subVectors(fsC, fsT); ln = flA.length();
                    if (ln > 0) { flA.divideScalar(ln); vn = v.dot(flA); if (vn < 0) v.addScaledVector(flA, -vn); if (flA.dot(up) > 0.7) touched = true; }      // resting on a roof / shelf top counts as touchdown
                }
            }
            hr = fsC.length(); dir.copy(fsC).divideScalar(hr || 1);
            sup = footFloor(dir, H);
            if (hr <= sup) { hr = sup; touched = true; vn = v.dot(dir); if (vn < 0) v.addScaledVector(dir, -vn); }
        }
        hum.gr = sup; hum.hr = hr; hum.vv = v.dot(dir); hum.air = true; hum.moving = true; hum.running = true;
        hum.pos.copy(dir).multiplyScalar(hr); up.copy(dir); hum.vh = hr;
        if (touched && !kSp) { fl.on = false; fl.lock = false; hum.air = false; hum.vv = 0; fl.cr = clamp(0.45 + Math.abs(vUp0) / (14 * H), 0.45, 1); fl.v.set(0, 0, 0); }
        fl.alt = Math.max(0, (hr - sup) / H);
        fl.th += ((thr ? (kSh ? 1 : 0.7) : 0) - fl.th) * damp(6, dt);
        // pose: body forward = flight forward leaned nose-down with thrust
        var bp = clamp(fl.pit - 0.55 * fl.th * (kW ? 1 : 0.3), -1.45, 1.45);
        flA.copy(hum.hf).multiplyScalar(Math.cos(bp)).addScaledVector(up, Math.sin(bp));
        fsT.copy(up).multiplyScalar(hr).applyQuaternion(qM).add(c); hum.w.copy(fsT);
        hum.face.copy(hum.hf);
        gA.copy(flA).applyQuaternion(qM); gB.copy(up).applyQuaternion(qM);
        gC.crossVectors(gA, gB).normalize(); gE.crossVectors(gC, gA); gD.copy(gA).negate();
        mM.makeBasis(gC, gE, gD);
        o.group.quaternion.setFromRotationMatrix(mM);
        o.group.position.copy(hum.w); o.group.scale.set(H, H * (1 - 0.22 * fl.cr), H);
        o.update(dt, { moving: true, running: true, airborne: true, speed: 1, facing: 0, jet: jetLvl, scan: scanT > 0 ? Math.min(1, scanT / 1.2) : 0 });
        jetFx(dt, fl.on ? Math.max(0.35, fl.th) : 0, H, qM, c, dir, sup, hr);
        flHud(fl.on, fl.alt);
        // camera: behind and a little above the flying human, along the (damped) flight pitch; never under the floor
        var cpi = fl.pit * 0.75;
        flA.copy(hum.hf).multiplyScalar(Math.cos(cpi)).addScaledVector(up, Math.sin(cpi)).negate().multiplyScalar(FOOT_CAM * H).addScaledVector(up, 0.7 * H);
        gA.copy(flA).applyQuaternion(qM);
        gC.copy(gB).multiplyScalar(0.5 * H).add(hum.w);
        gD.copy(gC).add(gA);
        vTmp.subVectors(gD, c);
        var cr = vTmp.length();
        gQi.copy(qM).invert(); fsT.copy(vTmp).applyQuaternion(gQi);
        var cfr = mFloor(fsT.x, fsT.y, fsT.z);
        if (cr < cfr + 0.04 * L) gD.copy(c).addScaledVector(vTmp.divideScalar(cr || 1), cfr + 0.04 * L);
        mM.lookAt(gD, gC, gB); gQ.setFromRotationMatrix(mM);
        groundCam(dt, gC, gD, gQ, 10);
        return gC;
    }
    function footStep(dt, c, qM) {
        var H = 0.09 * L, o = hum.obj, up = hum.up, i, k;
        if (cmdOpen) { mdx = mdy = 0; }
        up.copy(hum.pos).normalize();
        // rev 29: hold Space FL_HOLD s (continuously, in the air) = jetpack FLIGHT
        var flWant = !cmdOpen && !!keys.Space;
        if (flWant && !fl.lock) flT += dt; else { flT = 0; if (!flWant) fl.lock = false; }
        if (!fl.on && hum.air && flT >= FL_HOLD) { fl.on = true; fl.pit = 0; fl.wy = fl.wp = 0; fl.th = 0; fl.cr = 0; fl.v.copy(up).multiplyScalar(Math.max(0, hum.vv)); }
        if (fl.on) return flightStep(dt, c, qM);
        if (fl.cr > 0) fl.cr = Math.max(0, fl.cr - dt * 3.2);
        // look
        if (mdx !== 0) hum.hf.applyAxisAngle(up, -mdx * MOUSE_SENS * 1.3);
        hum.pitch = clamp(hum.pitch + mdy * MOUSE_SENS * 1.3, -0.3, 1.4);
        mdx = mdy = 0;
        hum.hf.addScaledVector(up, -hum.hf.dot(up));
        if (hum.hf.lengthSq() < 1e-8) hum.hf.crossVectors(up, X);
        hum.hf.normalize();
        gA.crossVectors(hum.hf, up);                                  // right
        var fw = cmdOpen ? 0 : ((keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0)), sd = cmdOpen ? 0 : ((keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0));
        gB.set(0, 0, 0).addScaledVector(hum.hf, fw).addScaledVector(gA, sd);
        var ml = gB.length(), moving = ml > 0.01, run = moving && !!(keys.ShiftLeft || keys.ShiftRight);
        hum.moving = moving; hum.running = run;
        if (moving) { gB.divideScalar(ml); hum.face.lerp(gB, damp(14, dt)); }
        // jump (once per press)
        var wantJump = !cmdOpen && !!keys.Space;
        if (!hum.air && wantJump && !jumpHeld) { hum.air = true; hum.vv = G_JUMP * H * fxJump; }
        jumpHeld = wantJump;
        // rev 21 jetpack: hold Space in the air (after JET_HOLD s, so a tap is still a jump) = infinite, fast: climb JET_UP heights/s, forward thrust 2 x run
        if (hum.air && wantJump) jetT += dt; else jetT = 0;
        var jet = jetT > JET_HOLD;
        // substepped capsule motion in the local frame
        var spd = (jet ? G_RUN * (1 + 0.12 * R27.su.jetpack) : (run ? G_RUN * (1 + 0.1 * R27.su.sprint) : G_WALK)) * H * fxSpeed, nSub = 1;
        if (moving) nSub = Math.max(nSub, Math.ceil(spd * dt / (FOOT_SUB * H)));
        if (hum.air) nSub = Math.max(nSub, Math.ceil(Math.abs(hum.vv) * dt / (0.25 * H)));
        nSub = Math.min(16, nSub);
        var h = dt / nSub, dir = fsD.copy(hum.pos).normalize(), hr = hum.hr, supNow = footFloor(dir, H), opN = objBegin();
        for (k = 0; k < nSub; k++) {
            if (moving) {
                var step = spd * h;
                for (i = 0; i < 5; i++) {                              // the wanted direction, then slide around a wall / cliff
                    fsT.copy(gB); if (FOOT_SLIDE_A[i] !== 0) fsT.applyAxisAngle(dir, FOOT_SLIDE_A[i]);
                    var st = step * FOOT_SLIDE_K[i];
                    fsC.copy(dir).addScaledVector(fsT, st / hr).normalize();
                    var supC = footFloor(fsC, H), rise = supC - supNow;
                    if (hum.air ? supC <= hr + 0.05 * H : rise <= st + 0.05 * H) {
                        dir.copy(fsC); supNow = supC;
                        break;
                    }
                }
            }
            if (opN && (moving || hum.air)) {                      // rev 29: store / burger-house walls, shelves and the ship hull are solid for the capsule
                fsT.copy(dir).multiplyScalar(hr);
                if (objPush(fsT, H)) {
                    var nl = fsT.length(); dir.copy(fsT).divideScalar(nl || 1); supNow = footFloor(dir, H);
                    if (hum.air && nl < hr) { hr = nl; hum.vv = Math.min(hum.vv, 0); }
                }
            }
            if (hum.air) {
                if (jet) { hum.vv += (JET_UP * (1 + 0.2 * R27.su.jetpack) * H - hum.vv) * Math.min(1, 7 * h); if (hr > supNow + 400 * H) hum.vv = Math.min(hum.vv, 0); }
                else hum.vv -= G_GRAV * H * h;
                hr += hum.vv * h;
                if (hr <= supNow) { hr = supNow; hum.vv = 0; hum.air = false; }
            } else if (hr - supNow > 0.6 * H) { hum.air = true; hum.vv = 0; }
            else hr = supNow;
        }
        if (R27.windOn) { var wd = footWind(dir, dt, hr, H, supNow); if (wd) { dir.copy(fsC2); supNow = wd; if (!hum.air) hr = supNow; } }
        hum.gr = supNow; hum.hr = hr;
        hum.pos.copy(dir).multiplyScalar(hr);                           // written once
        up.copy(dir);
        // visual height: eases toward the physical one (hides the sub-height snaps of a patch re-centre), never below the ground
        if (!(hum.vh > 0)) hum.vh = hr;
        if (hum.air || Math.abs(hum.vh - hr) > 1.5 * H) hum.vh = hr;      // airborne, or teleported / dropped a long way: no easing
        else {
            hum.vh += (hr - hum.vh) * damp(26, dt);
            if (hum.vh < hr - 0.03 * H) hum.vh = hr - 0.03 * H;
        }
        fsT.copy(up).multiplyScalar(hum.vh);
        hum.w.copy(fsT).applyQuaternion(qM).add(c);
        hum.face.addScaledVector(up, -hum.face.dot(up)); if (hum.face.lengthSq() < 1e-8) hum.face.copy(hum.hf); hum.face.normalize();
        gA.copy(hum.face).applyQuaternion(qM);                         // world forward
        gB.copy(up).applyQuaternion(qM);                               // world up
        gC.crossVectors(gA, gB); gD.copy(gA).negate();
        mM.makeBasis(gC, gB, gD);
        o.group.quaternion.setFromRotationMatrix(mM);
        o.group.position.copy(hum.w); o.group.scale.set(H, H * (1 - 0.22 * fl.cr), H);
        flHud(false, 0);
        o.update(dt, { moving: moving, running: run || jet, airborne: hum.air, speed: moving ? (run || jet ? 1 : 0.5) : 0, facing: 0, jet: jetLvl, scan: scanT > 0 ? Math.min(1, scanT / 1.2) : 0 });
        jetFx(dt, jet, H, qM, c, dir, supNow, hr);
        // camera: 6 human heights back, pitched, never under the floor; it follows the SMOOTHED position (hum.w), not the raw step
        gA.copy(hum.hf).negate().multiplyScalar(Math.cos(hum.pitch)).addScaledVector(up, Math.sin(hum.pitch)).applyQuaternion(qM);   // world offset dir
        gC.copy(gB).multiplyScalar(0.8 * H).add(hum.w);                // focus (head)
        gD.copy(gC).addScaledVector(gA, FOOT_CAM * H);                 // wanted camera position
        vTmp.subVectors(gD, c);
        var cr = vTmp.length();
        gQi.copy(qM).invert(); fsT.copy(vTmp).applyQuaternion(gQi);
        var cfr = mFloor(fsT.x, fsT.y, fsT.z);
        if (cr < cfr + 0.04 * L) gD.copy(c).addScaledVector(vTmp.divideScalar(cr || 1), cfr + 0.04 * L);      // the rendered ground now equals the floor used here, so the clearance is just a little more than the near plane
        mM.lookAt(gD, gC, gB); gQ.setFromRotationMatrix(mM);
        groundCam(dt, gC, gD, gQ, 16);
        return gC;
    }
    function groundStep(dt) {
        var node = land.node, P = shipRoot.position, i;
        if (!node || !node.anchor || !node.mesh) { leaveGround(); return; }
        syncPlanet(node);
        var c = node.anchor.position, qM = node.mesh.quaternion, focus = null;
        psCalls = 0;
        if (gmode === 'landing') {
            if (land.t === 0) {            // the flight position was placed against last frame's planet: remember the offset and fade it out over the descent
                land.off.copy(P).sub(gA.copy(land.gDir).multiplyScalar(land.r0).applyQuaternion(qM).add(c));
            }
            land.t += dt;
            var k = clamp(land.t / land.T, 0, 1), e = easeInOut(k);
            P.copy(land.gDir).multiplyScalar(land.r0 + (land.rg - land.r0) * e).applyQuaternion(qM).add(c).addScaledVector(land.off, 1 - e);
            gQ.copy(land.qFrom).slerp(land.qTo, e); shipRoot.quaternion.copy(qM).multiply(gQ);
            hullY(0.15 * easeInOut(clamp(k / 0.25, 0, 1)) * (1 - easeInOut(clamp((k - 0.25) / 0.75, 0, 1))));       // legs deploy: the hull sinks onto them
            exMe.holder.visible = true; exUpdate(exMe, P, shipRoot.quaternion, L, 0.6 * (1 - e) + 0.2);
            chaseTargets(dt); camera.position.copy(camPos); camera.quaternion.copy(camQuat);
            if (k >= 1) touchdown();
        } else {
            P.copy(land.sPos).applyQuaternion(qM).add(c); shipRoot.quaternion.copy(qM).multiply(land.sQuat);
            if (land.settle > 0) {
                land.settle = Math.max(0, land.settle - dt);
                var ss = 1 - land.settle / LAND_SETTLE;
                hullY(-0.035 * Math.sin(Math.PI * Math.min(1, ss * 1.6)) * (1 - ss));                  // damped dip onto the struts
                if (land.settle > 0) { exMe.holder.visible = true; exUpdate(exMe, P, shipRoot.quaternion, L, 0.2 * (1 - ss) * (1 - ss)); } else { exOff(exMe); hullY(0); }      // engine winds down
                land.dustT -= dt;
                if (land.dustT <= 0 && ss < 0.85) {                                                       // dust puffs rolling out from under the hull
                    land.dustT = 0.11; gA.copy(land.sPos).applyQuaternion(qM).normalize();
                    gB.copy(P).addScaledVector(gA, -legDrop * L);
                    gC.copy(X).applyQuaternion(shipRoot.quaternion).multiplyScalar((Math.random() < 0.5 ? -1 : 1) * (0.5 + 0.9 * ss) * L);
                    gD.copy(NEG_Z).applyQuaternion(shipRoot.quaternion).multiplyScalar((Math.random() - 0.5) * 1.6 * L);
                    gB.add(gC).add(gD); fx.impact(gB, 0xc8b89a, 3);
                }
            } else exOff(exMe);
            if (gmode === 'foot') focus = footStep(dt, c, qM);
            else {                                                                                       // low 3/4 view from the front-right, near the ground
                gA.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); gB.copy(X).applyQuaternion(shipRoot.quaternion); gC.copy(Y).applyQuaternion(shipRoot.quaternion);
                gD.copy(P).addScaledVector(gA, 1.5 * L).addScaledVector(gB, 3.3 * L).addScaledVector(gC, (0.55 - legDrop) * L);
                gE.copy(P).addScaledVector(gC, -0.1 * L);
                mM.lookAt(gD, gE, gC); gQ.setFromRotationMatrix(mM);
                groundCam(dt, P, gD, gQ, 2.6);
            }
        }
        vel.set(0, 0, 0); speed = 0;
        shake *= Math.exp(-6 * dt);
        if (shake > 1e-4 * L) { gA.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(2 * shake); camera.position.add(gA); }
        fov += (baseFov - fov) * damp(4, dt);
        var gNear = gmode === 'foot' ? 0.01 * L : 0.02 * L;      // a human is ~0.0055 u tall: the 0.05 near plane would clip it
        if (Math.abs(camera.fov - fov) > 0.01 || camera.near !== gNear) { camera.fov = fov; camera.near = gNear; camera.updateProjectionMatrix(); }
        if (gmode !== 'foot') clampCamToGround(0.5);
        camera.updateMatrixWorld(true);
        if (ps) { try { ps.footMode = gmode === 'foot'; ps.update(dt, gmode === 'foot' ? hum.w : P); r25HideTaken(); } catch (e2) { planetFail(e2); } }
        if (rollCd > 0) rollCd -= dt;
        if (waveMsgT > 0) waveMsgT -= dt;
        fx.setMotion(vel, 0, false); try { fx.setAtmo(ps && ps.active ? ps.depth : 0); } catch (e0) { /* ignore */ } fx.update(dt, camera);
        // prompt
        if (gmode === 'landing' || gmode === 'landed') setPrompt('', false);
        else if (gmode === 'foot' && !storeOpen && !invOpen && footTarget().kind) setPrompt(ftI.label, true);
        else if (nearShip()) setPrompt(R25.mod ? 'F · ENTER SHIP · HOLD LIFT OFF' : 'F · BOARD', true);
        else setPrompt('', false);
        if (net) { net.sendPos(); net.update(dt, dockA); ghostFx(); }
        aEngine({ throttle: 0, inAtmo: true });
        if (gmode === 'foot' && hum.obj) { vTmp.copy(hum.w); shardTick(vTmp, 0.7 * L); } else if (gmode === 'landed') shardTick(P, 3 * L);
        scanUpdate(dt); musicTick(dt, null);
        if (gmode === 'foot') { mutterTick(dt); r27Foot(dt); }
        updateBooms(dt);
        updateHud(0, aliveCount());
        hudDmg(dt);
        updatePlayerMarks();
    }
    function step(dt) {
        if (dt > 0.05) dt = 0.05;
        if (dt <= 0) return;
        if (resumePend) { resumeTick(); return; }
        if (scaleChecks > 0) { scaleChecks--; refreshScale(); }
        if (station && station.group.visible) { try { station.update(engine.time, dt, shipRoot.position); } catch (e) { console.info('[ship] station.update', e); station.setVisible(false); } }
        if (sk.cd > 0) sk.cd -= dt;
        worldSync(); r25Tick(dt);
        fxTick(dt); glowTick(dt); escTick(dt); guideTick(dt);
        if (boarding || exiting) { cinematicStep(dt); return; }
        if (landReq) { landReq = false; startLanding(); }
        ehTick(dt);
        if (gmode === 'ifoot') { interiorStep(dt); return; }
        if (gmode === 'docking' || gmode === 'launching' || gmode === 'sfoot') { stationStep(dt); return; }
        if (gmode !== 'fly') { groundStep(dt); return; }
        var i, bodies = gatherBodies();
        edgeFit(bodies);
        var P = shipRoot.position;
        lfBegin(dt, bodies, P);                    // rev 18: current pose of every body + local-frame state -> world pose
        gt += dt; psCalls = 0;
        var tS = prof.on ? performance.now() : 0, tM = 0, tA0 = 0;
        // rev 9c #3: Focus (hold Q) slows the WORLD (enemies, bolts, allies) to 0.35x; your flight and mouse stay real-time.
        // Kill hit-stop freezes the world for 2 frames (HUD and your flight are unaffected).
        var fwant = !dead && !!keys.KeyQ && (focusing ? focusE > 0 : focusE > 0.3);
        if (fwant) { focusing = true; focusE = Math.max(0, focusE - dt); } else { focusing = false; focusE = Math.min(FOCUS_MAX, focusE + dt * FOCUS_MAX / FOCUS_RECHARGE); }
        timeScale = focusing ? FOCUS_TS : 1;
        var wdt = dt * timeScale;
        if (hitStopN > 0) { hitStopN--; wdt = 0; }
        updateThrow(wdt);
        if (perfQ) perfQ.update(dt);
        if (chainT > 0) { chainT -= dt; if (chainT <= 0) chainN = 0; }
        if (comboT > 0) { comboT -= dt; if (comboT <= 0) comboN = 0; }
        if (od > 0) od = Math.max(0, od - dt);
        else if (graze > 0) { grazeIdle += dt; if (grazeIdle > 3) graze = Math.max(0, graze - dt * 3); }
        if (zT > 0) zT -= dt;
        if (rollCd > 0) rollCd -= dt;
        // rev 21: ram window / cooldown, weapon heat (cools only after HEAT_GAP s without a volley), overheat lockout + steam
        if (ramCd > 0) ramCd -= dt;
        if (ramT > 0) { ramT = Math.max(0, ramT - dt); ramFx -= dt; if (ramFx <= 0) { ramFx = 0.07; fx.flash(shipRoot.position, effColor()); } }
        if (ohT > 0) {
            ohT -= dt; heat = 0.35 + 0.65 * Math.max(0, ohT) / OVERHEAT_T;
            steamT -= dt;
            if (steamT <= 0) { steamT = 0.1; vTmp.copy(muzzles[(steamN++) & 1]).multiplyScalar(L).applyQuaternion(shipRoot.quaternion).add(shipRoot.position); fx.impact(vTmp, 0xcfe8ff, 1.1); }
            if (ohT <= 0) heat = 0.35;
        } else if (heat > 0 && gt - lastFireGt > HEAT_GAP) heat = Math.max(0, heat - HEAT_COOL * dt);
        tickPunch = Math.max(0, tickPunch - dt * 12);
        fovKick *= Math.exp(-7 * dt);

        if (dead) {
            deathT -= dt;
            if (deathT <= 0) {
                // restart at the dock: waves, kills, hp reset; still piloting
                placeAtDock(); lfOff(); bhInv = 4;
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

        // rev 13 maneuvers. Ctrl = drift turn: the nose swings 1.6x as hard while the velocity keeps going (chase 0.18/s) for up to 1.2 s, then 1.5 s cooldown.
        var flipping = flipT > 0 && !dead;
        if (flipCd > 0) flipCd -= dt;
        if (driftCd > 0 && !driftOn) driftCd -= dt;
        var wantDrift = !dead && !cmdOpen && !flipping && !!(keys.ControlLeft || keys.ControlRight);
        if (wantDrift && !driftOn && driftCd <= 0) { driftOn = true; driftLeft = DRIFT_T; }
        if (driftOn) {
            driftLeft -= dt;
            if (!wantDrift || driftLeft <= 0) {
                driftOn = false; driftCd = DRIFT_CD;
                if (DRIFT_T - Math.max(0, driftLeft) > 0.35 && vel.length() > 0.5 * CRUISE) driftBoost = 1;      // clean exit: +25 % speed for 1 s
            }
        }

        // weighted mouse: the pointer delta sets a TARGET turn rate and the actual rate chases it at MOUSE_WEIGHT/s (angular inertia).
        // Turn authority shrinks with pulse (to half at full pulse), grows in a drift. All rates are per second: 60 Hz and 120 Hz behave the same.
        if (cmdOpen) { mdx = mdy = 0; firing = false; }
        var turnCap = TURN_MAX * (1 - 0.5 * pulse) * (driftOn ? DRIFT_TURN : 1);
        // rev 9 weight curve: sign(d) * |d|^1.6 (d = raw rate / 2 rad/s): precise small moves, fast big ones
        var tYaw = clamp(wcurve(-mdx * MOUSE_SENS / Math.max(dt, 1e-3)), -turnCap, turnCap);
        var tPit = clamp(wcurve(-mdy * MOUSE_SENS / Math.max(dt, 1e-3)), -turnCap, turnCap);
        var mouseRaw = Math.sqrt(mdx * mdx + mdy * mdy);
        mdx = mdy = 0;
        if (flipping) { tYaw = tPit = 0; mouseRaw = 0; }
        var mk = damp(MOUSE_WEIGHT * (driftOn ? 1.3 : 1), dt);
        yawRate += (tYaw - yawRate) * mk; pitRate += (tPit - pitRate) * mk;
        // g-shake: turning above 0.8 of the max turn rate rattles the camera
        var gk = clamp((Math.sqrt(yawRate * yawRate + pitRate * pitRate) / turnCap - GSHAKE_AT) / (1 - GSHAKE_AT), 0, 1);
        if (gk > 0) { shake = Math.max(shake, gk * 0.25 * L); fovKick = Math.max(fovKick, gk * 1.2); }
        var yawA = yawRate * dt, pitA = pitRate * dt;
        var hov = lf.on && lf.hover && !dead;                // rev 18: below 3 L the ship hovers: A/D strafe instead of rolling
        var rollA = (dead || hov) ? 0 : ((keys.KeyA ? 1 : 0) - (keys.KeyD ? 1 : 0)) * ROLL_RATE * dt;
        if (dead) { yawA = pitA = 0; }
        // rev 9b #3: dodge roll = one full barrel roll over ROLL_T s (damage cut applied in hurtPlayer), plus a 6 L sidestep below
        var rolling = rollT > 0 && !dead, rollP = rolling ? 1 - rollT / ROLL_T : 0;
        if (rolling) { var rdt = Math.min(dt, rollT); rollA += rollDir * 6.2832 * rdt / ROLL_T; rollT = Math.max(0, rollT - dt); }
        // bank: the hull leans into the turn (roll chases BANK_K x yawRate; only the change is applied, so it returns level)
        var bankT = (dead || hov) ? 0 : clamp(BANK_K * yawRate * (driftOn ? 1.6 : 1), -BANK_MAX, BANK_MAX), bankN = bank + (bankT - bank) * damp(BANK_SPRING, dt);
        if (flipping) {
            // scripted: q = q0 * Rx(pi s) * Rz(pi s), s eased over FLIP_T (heading reverses, ends upright)
            flipT = Math.max(0, flipT - dt);
            var fs = easeInOut(1 - flipT / FLIP_T);
            shipRoot.quaternion.copy(flipQ0).multiply(qA.setFromAxisAngle(X, Math.PI * fs)).multiply(qB.setFromAxisAngle(Z, Math.PI * fs)).normalize();
        } else {
            rollA += bankN - bank; bank = bankN;
            shipRoot.quaternion.multiply(qA.setFromAxisAngle(X, pitA))
                .multiply(qB.setFromAxisAngle(Y, yawA))
                .multiply(qB.setFromAxisAngle(Z, rollA)).normalize();
        }
        vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);

        // aim assist: soft magnet toward the hostile nearest the nose (4 deg cone), off while yanking/pulsing/dead
        var assistOn = false;
        if (!dead && !flipping && pulse < 0.05 && mouseRaw / Math.max(dt, 1e-3) < AIM_YANK) {
            var bestAng = AIM_CONE, bx = 0, by = 0, bz = 0, aj;
            // measure from the chase-camera origin: the reticle ray (and the bolt convergence) starts there, not at the hull
            vU.copy(Y).applyQuaternion(shipRoot.quaternion);
            vR1.copy(P).addScaledVector(vF, -CAM_L * L).addScaledVector(vU, CAM_UP * CAM_L * L);
            for (aj = 0; aj < enemies.length + orbs.length; aj++) {
                var ae = aj < enemies.length ? enemies[aj] : orbs[aj - enemies.length], ap;
                if (aj < enemies.length) {
                    if (!ae.alive) continue;
                    ap = vE.copy(ae.g.position);
                    if (ae.isBoss && ae.eyeN) ap.copy(ae.eyeW[0]);
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

        // targeting: the body nearest the reticle (inside TARGET_CONE)
        var bestScore = Infinity, best = null, npT = 0;
        for (i = 0; i < bodies.length; i++) {
            var b = bodies[i], ap = b.node.anchor.position;
            vA.subVectors(ap, P);
            var dist = vA.length();
            b.dist = dist;
            var nk = clamp((4 - dist / b.R) / 2, 0, 1) * clamp((dist / b.R - 1.4) / 0.3, 0, 1);
            if (nk > npT) npT = nk;
            if (dist < 1) continue;
            var ang = Math.acos(clamp(vA.dot(vF) / dist, -1, 1));
            ang = Math.max(0, ang - Math.asin(Math.min(1, b.R / dist)));    // big close planets are easy to aim at
            if (ang < TARGET_CONE) {
                var sc = ang < 1e-4 ? -1 + dist * 1e-9 : ang;            // inside the disc: nearer wins
                if (sc < bestScore) { bestScore = sc; best = b; }
            }
        }
        target = best;
        nearPl += (npT - nearPl) * damp(1.6, dt);

        // rev 18: the old frame drag is gone. Inside LF_ON x R the whole step runs in the planet's local frame (lfBegin / lfMove / lfCommit).

        // rev 13 pulse anywhere: no altitude rules. The swept path (current velocity, PULLUP_LOOK s ahead) is tested against every body's shell and,
        // inside an active planet's atmosphere, the actual terrain. Pulse drops only if that path hits within PULSE_LOOK s; before that a gentle auto
        // pull-up (max 25 deg/s) tilts the nose away so shallow approaches skim and steep ones do not.
        var hitT = (dead || lf.cap) ? -1 : pathHit(P, vel, PULLUP_LOOK, bodies), cut = hitT >= 0 && hitT <= PULSE_LOOK;
        if (hitT >= 0 && !dead && vel.length() > BOOST * 0.7) {
            vU.crossVectors(vF, phN);
            var pl = vU.length();
            if (pl > 1e-4 && vF.dot(phN) < 0.3) {
                vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
                shipRoot.quaternion.premultiply(qA.setFromAxisAngle(vU.divideScalar(pl), PULLUP_RATE * (0.35 + 0.65 * (1 - hitT / PULLUP_LOOK)) * dt)).normalize();
                vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion);
            }
        }
        var pulseHeld = !dead && !!keys.Space && !cut;
        // pulse ramps EXPONENTIALLY while held, pulse(t) = 1 - e^(-t/tau), tau 3 s, to PULSE_SPEED 140 u/s
        // (speed = base + (PULSE_SPEED - base) x pulse); release bleeds it off over 1 s; a projected hit drops it in ~0.3 s
        var pCap = boosting ? PULSE_MAX2 : PULSE_MAX;      // rev 24: Space caps at 450, Space + Shift at 675
        if (pulseHeld) pulseT = Math.min(pulseT + dt, PULSE_E * Math.log(pCap / BOOST));
        else pulseT = Math.max(0, pulseT - dt * (cut ? 60 : PULSE_BLEED));
        if (lf.cap) pulseT = pulseHeld ? 1e-4 : 0;      // rev 18: in the atmosphere pulse is a 40 u/s sprint, no ramp
        var pulseSpd = 0;
        if (pulseT > 0) { pulseT = Math.min(pulseT, PULSE_E * Math.log(pCap / BOOST)); pulseSpd = Math.min(pCap, BOOST * Math.exp(pulseT / PULSE_E)); pulse = clamp(0.2 + 0.8 * Math.log(pulseSpd / BOOST) / PULSE_LN, 0, 1); } else pulse = 0;

        var tgt = targetSpeed(boosting);
        if (pulseSpd > 0) { var psT = throttle < -0.02 ? -0.6 * pulseSpd : pulseSpd; if (Math.abs(psT) > Math.abs(tgt)) tgt = psT; }      // rev 14: exponential pulse speed; reverse pulse = 0.6 x
        // drift-turn exit = +25 % speed for 1 s, decaying
        if (driftBoost > 0) { tgt *= 1 + DRIFT_BOOST * driftBoost; driftBoost = Math.max(0, driftBoost - dt); }
        if (ramT > 0 && throttle > 0.02) tgt *= 1.3;                  // rev 21: the ram tool lunges
        if (lf.cap) tgt = clamp(tgt, -ATM_PULSE, pulseSpd > 0 ? ATM_PULSE : ATM_BOOST);
        // thrust is acceleration: the commanded speed spools up (1.4/s), and the velocity then chases the thrust vector at VEL_CHASE/s
        var rate = tgt >= speed ? (pulseT > 0 ? 14 : (ramT > 0 ? 6 : 1.4)) : (pulseT > 0 ? 4 : 2.2);
        if (cut && Math.abs(speed) > CRUISE * 2) rate = 6;                       // brake when pulse was cut by a body ahead
        speed += (tgt - speed) * damp(rate, dt);

        // velocity chases forward*speed: it carries through turns (mass). A drift turn drops the chase to 0.18/s: the nose swings, the velocity does not.
        vA.copy(vF).multiplyScalar(speed);
        if (hov && !cmdOpen) { var sdv = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0); if (sdv) vA.addScaledVector(vB.copy(X).applyQuaternion(shipRoot.quaternion), sdv * CRUISE); }     // hover strafe
        // rev 20b: near a surface planet (inside 3 R, frame not on yet) the commanded velocity is RELATIVE to that planet. Planet orbits are x6 faster now (>100 u/s,
        // above the 40 u/s atmosphere sprint), so a world-referenced thrust can never catch a planet moving away: the governor capped the ship's closing speed
        // while the planet outran it (stuck at 1.6-2 R). Blend the planet's world velocity into the target over 6 R -> 1.9 R (rev 23: from 6 R, a line aimed at a fast planet no longer drifts off it).
        if (!lf.on && ps) {
            var fbR = null, fbD = 6;
            for (i = 0; i < bodies.length; i++) {
                if (!hasSurface(bodies[i].node)) continue;
                var fdR = P.distanceTo(bodies[i].node.anchor.position) / bodies[i].R;
                if (fdR < fbD) { fbD = fdR; fbR = bodies[i]; }
            }
            if (fbR) {
                var fk = clamp((6 - fbD) / 4.1, 0, 1); fk = fk * fk * (3 - 2 * fk);
                vA.x += fbR.vx * fk; vA.y += fbR.vy * fk; vA.z += fbR.vz * fk;
            }
        }
        vel.lerp(vA, damp(driftOn ? DRIFT_CHASE : VEL_CHASE, dt));
        lfGovern(dt, bodies, P);                   // rev 18: approach governor / atmosphere caps / ENTRY brake

        vP0.copy(P); bhPrev.copy(P);
        if (lf.on) { if (!dead) { lfMove(dt); lfLevel(dt); } }          // rev 18: swept + substepped in the local frame
        else {
            if (!dead) P.addScaledVector(vel, dt);
            if (bounceV.lengthSq() > 1e-12) { P.addScaledVector(bounceV, dt); bounceV.multiplyScalar(Math.exp(-2.5 * dt)); }
        }
        lf.step = P.distanceTo(vP0);
        if (ramInv > 0) ramInv -= dt;
        if (terrInv > 0) terrInv -= dt;
        if (ramFlash > 0) ramFlash -= dt;
        var psA = null;
        if (ps) {
            try {
                ps.moonMin = MOON_L * L; ps.footMode = false; ps.update(dt, P); psA = ps.active;
                if (psA && ps.shake > 0.01) shake = Math.max(shake, ps.shake * 0.075 * L * Math.min(1, vel.length() / BOOST));      // atmospheric entry rumble (rev 19: 15 % of 0.5 L)
            } catch (e) { planetFail(e); psA = null; }
        }
        if (rolling) {          // rev 14: the roll IS the dodge: ROLL_SIDE (4) L sideways over ROLL_T, wing-thruster puff at the opposite wingtip
            P.addScaledVector(rollX, -rollDir * (ROLL_SIDE * L / ROLL_T) * 1.5708 * Math.sin(3.1416 * rollP) * dt);      // along the pre-roll right vector (the hull spins under it)
            vA.copy(X).applyQuaternion(shipRoot.quaternion);
            rollFx -= dt;
            if (rollFx <= 0) { rollFx = 0.08; vTmp.copy(vA).multiplyScalar(rollDir * 0.55 * L).add(P); fx.flash(vTmp, effColor()); fx.impact(vTmp, effColor(), 1); }
        }

        // HARD sphere at 1.15 R against CURRENT anchor positions (planets move):
        // (1) swept test along this frame's segment; (2) already inside -> pop to surface. Inside an active planet's atmosphere the shell is the terrain
        // floor + 1.5 L. Slide along it; a NORMAL speed above BOOST costs 30 HP and bounces.
        for (var pass = 0; pass < 2; pass++) {
            for (i = 0; i < bodies.length; i++) {
                var cb = bodies[i], c = cb.node.anchor.position, shell = HARD_F * cb.R;
                if (lf.on && cb.node === lf.node) continue;       // rev 18: the local-frame planet is collided inside lfMove (terrain floor)
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
                            if (vn2 < 0) { terrainHit(-vn2, vD); vel.addScaledVector(vD, -vn2); }
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
                if (vn < 0) { terrainHit(-vn, vB); vel.addScaledVector(vB, -vn); }                // kill inward velocity
                var fwdV = vel.dot(vF);
                if (fwdV < speed) speed = fwdV;
            }
        }

        // rev 22: the black hole is a hard sphere with a gravity well; rocks push the ship out softly; the station mouth docks the ship
        if (!dead) {
            bhStep(dt, P);
            if (space) {
                space.update(dt, P);
                if (space.pushOut(P, 0.45 * L, skOut)) {
                    vD.subVectors(skOut, P); var pdl = vD.length(); if (pdl > 1e-9) { vD.divideScalar(pdl); var pvn = vel.dot(vD); if (pvn < 0) vel.addScaledVector(vD, -pvn); }
                    P.copy(skOut); speed = Math.min(speed, vel.dot(vF));
                }
            }
            if (station && station.group.visible && sk.cd <= 0 && !lf.on) {
                for (var dk2 = 0; dk2 <= 4; dk2++) { vTmp.lerpVectors(bhPrev, P, dk2 / 4); if (station.mouth.trigger(vTmp)) { beginDock(); break; } }
            }
            if (space && sk.cd <= 0 && !lf.on && gmode === 'fly') dkTrigger(P);          // rev 27: derelict mouths dock like the station
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
                if (ramT > 0 && ramInv <= 0) {                // rev 21: the ram TOOL: 40 + speed x 2 to it, 12 to you, shield dish breaks, big shake
                    var rspd = Math.max(vel.length(), Math.abs(speed)), rdm2 = RAM_BASE + rspd * RAM_SPD_K;
                    ramT = 0; ramInv = RAM_INV * 0.6; ramFlash = 0.54; ramIsHit = false;
                    hurtPlayer(RAM_SELF, undefined, 1.5, true);
                    burst(P, 16, 2, 20, 3); fx.flash(P, effColor()); fx.flash(en.g.position, 0xffffff);
                    shake = Math.max(shake, 3.2 * L); fovKick = 7;
                    var rv2 = (en.isBoss ? 60 : 38) * L + Math.abs(speed) * 0.25;
                    bounceV.copy(vD).multiplyScalar(rv2); vel.copy(vD).multiplyScalar(rv2 * 0.2);
                    speed = Math.min(speed, 0.4 * CRUISE); pulse = 0; pulseT = 0;
                    if (en.cr && en.cr.stats && en.cr.stats.shield) { try { en.cr.setShieldDown(); } catch (e1) { /* ignore */ } aPlay('shieldBreak', { dist: aDist(en.g.position) }); fx.impact(en.g.position, 0x55e8ff, 3); }
                    aPlay('explosion', { pitch: 1.1, dist: aDist(en.g.position) * 0.5 });
                    damageEnemy(en, rdm2, true, P, -1, true);
                } else if (ramInv <= 0) {
                    var rdmg = en.isBoss ? ((bsig.owner === en && bsig.ph === 'act' && bsig.idx === 2) ? bsig.dmg : RAM_BOSS) : RAM_PLAYER;
                    ramInv = RAM_INV; ramFlash = 0.54; ramIsHit = false;
                    hurtPlayer(rdmg, undefined, 2.5, true);
                    burst(P, 10, 1, 14, 2); fx.flash(P, 0xff4040);
                    shake = Math.max(shake, 2.6 * L); fovKick = 5;
                    var rv = (en.isBoss ? 120 : 75) * L + Math.abs(speed) * 0.5;
                    bounceV.copy(vD).multiplyScalar(rv);
                    vel.copy(vD).multiplyScalar(rv * 0.3);
                    speed = Math.min(speed, 0.3 * CRUISE); pulse = 0; pulseT = 0;
                    damageEnemy(en, en.maxHp * (en.isBoss ? 0.03 : RAM_ENEMY_FRAC), false, P);
                }
                break;
            }
        }
        // rev 14: entry burn + the LAND prompt (sampled ~8 Hz: ps.landable allocates)
        updateEntry(dt, P, vel.length(), bodies);
        landCheckT -= dt;
        if (landCheckT <= 0) {
            landCheckT = 0.12; landOk = false;
            var lmsg = '';
            if (ps && psA && !dead && lf.on && lf.cap && lf.alt < PROMPT_L * L && vel.length() < 2) {     // rev 19: hovering (under 3 L) and nearly still
                try {
                    var lr = ps.landable(P); landOk = !!lr.ok;
                    lmsg = landOk ? 'F · LAND' : '';      // rev 20b: water / too steep / moonlet say nothing, the prompt just does not appear
                } catch (e) { landOk = false; }
            }
            if (!lmsg && !dead) {       // MOONLET: within 3 R of a body too small to land on
                for (i = 0; i < bodies.length; i++) {
                    var mb = bodies[i];
                    if (false) break;
                }
            }
            setPrompt(lmsg, landOk);
        }
        // chase camera (rigid) + fov
        chaseTargets(dt);
        camera.position.copy(camPos);
        camera.quaternion.copy(camQuat);
        shake *= Math.exp(-6 * dt);
        if (shake > 1e-4 * L) { vA.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(2 * shake).applyQuaternion(camQuat); camera.position.add(vA); }
        clampCamToGround(0.5);                     // rev 19: the chase camera never sits inside the terrain (hills rise behind the ship)
        var amv = dt / ENTRY_T;                    // rev 18: fov 42 -> 50 over the entry (ENTRY_T), back down on the way out
        lf.atmK += clamp(((lf.on && lf.cap && !lf.leaving) ? 1 : 0) - lf.atmK, -amv, amv);
        var fovT = baseFov + (boosting ? FOV_BOOST : 0) + FOV_PULSE * pulse + (focusing ? 3 : 0) + fovKick + ATM_FOV * lf.atmK + FOV_PLANET * nearPl;
        fov += (fovT - fov) * damp(4, dt);
        var fNear = (lf.on && lf.cap) ? 0.02 * L : baseNear;     // rev 19: near plane 0.02 L inside the atmosphere (log depth keeps it precise) so ground right under the camera is never clipped
        if (Math.abs(camera.fov - fov) > 0.01 || camera.near !== fNear) { camera.fov = fov; camera.near = fNear; camera.updateProjectionMatrix(); }
        camera.updateMatrixWorld(true);

        // exhaust cones at every thruster, streaks from velocity
        var exI = pulse > 0.05 ? 1 + pulse : ((boosting && throttle > 0) ? 1 : Math.max(0, throttle) * 0.7);
        if (rolling) exI = Math.max(exI, 1.5);
        exMe.holder.visible = !dead;
        exUpdate(exMe, P, shipRoot.quaternion, L, exI);
        aEngine({ throttle: dead ? 0 : Math.abs(throttle), boost: boosting, pulse: pulse, inAtmo: lf.on && lf.cap });
        fx.setMotion(vel, Math.min(1, Math.abs(speed) / (0.6 * BOOST)), pulse > 0.5 || entryHeat > 0.3); try { fx.setAtmo(ps && ps.active ? ps.depth : 0); } catch (e0) { /* ignore */ }

        // ─── combat ───
        var en;
        if (prof.on) { tM = performance.now(); prof.flight += tM - tS; }
        if (!dead) {
            // waves (rev 9c tempo): clear -> 8-12 s breather -> next; a spike/boss wave earns 15 s; stragglers cannot stall the 60 s cycle
            if (!peaceful) {
                if (waveActive && qN === 0 && !bossAlive() && (aliveCount() === 0 || (gt - waveStartT > 70 && aliveCount() <= 2))) waveClear();
                if (!waveActive && gt >= nextWave && !bossAlive()) spawnWave(bodies);
                if (qN > 0) pumpQueue(bodies);
            }
            // regen (40 %): wave 1 always, later waves after a short hit-free delay
            sinceHit += dt;
            if (hp < HP_MAX && sinceHit >= regenDelay) hp = Math.min(HP_MAX, hp + REGEN * dt);
            // player lasers (Overdrive: fire x1.5, dmg x1.25)
            fireCd -= dt;
            if (firing && fireCd <= 0 && ohT <= 0) {
                var wpn = curWeapon(), wst = wpn.stats, guard = 3, pdmg = wst.dmg * (od > 0 ? OD_DMG : 1), prate = wst.rate * (od > 0 ? OD_FIRE : 1);
                var wcnt = Math.max(1, wst.count | 0), wspr = (wst.spread || 0) * Math.PI / 180, wsp0 = wst.speed || P_BOLT_SPEED, wcol = typeof wpn.color === 'number' ? wpn.color : null;      // rev 20: the equipped weapon (dmg / rate / count / spread / speed / color)
                while (fireCd <= 0 && guard-- && ohT <= 0) {
                    fireCd += 1 / prate; playerFired = true;
                    heat += HEAT_PER; lastFireGt = gt;
                    if (heat >= 1) { heat = 1; ohT = OVERHEAT_T; steamT = 0; aPlay('overheat', { vel: 1 }); shake = Math.max(shake, 0.2 * L); }
                    vU.copy(Y).applyQuaternion(shipRoot.quaternion);
                    // convergence point on the reticle ray, 105 L out (35 old L)
                    vAim.copy(camera.position).addScaledVector(vF, 105 * L);
                    for (var side = 0; side < wcnt; side++) {
                        // one barrel alternates, two use both, more fan across the span between the barrels
                        var mu = wcnt === 1 ? (fireSide ^= 1) : side / (wcnt - 1);
                        vTmp.lerpVectors(muzzles[0], muzzles[1], mu).multiplyScalar(L).applyQuaternion(shipRoot.quaternion).add(P);
                        vD.subVectors(vAim, vTmp).normalize();
                        if (wspr > 0 && wcnt > 1) vD.applyAxisAngle(vU, (side / (wcnt - 1) - 0.5) * wspr);
                        else if (wspr > 0) vD.applyAxisAngle(vU, (Math.random() - 0.5) * wspr);
                        fx.flash(vTmp, wcol != null ? wcol : effColor());
                        // rev 19: bolt velocity = ship velocity + muzzle velocity (never slower than the ship), 20 s life
                        vBI.copy(vD).multiplyScalar(wsp0 * L).add(vel);
                        var bsp = Math.max(vBI.length(), vel.length() * 1.02 + 0.25 * wsp0 * L);
                        vBI.normalize();
                        fireBolt(vTmp.x, vTmp.y, vTmp.z, vBI.x, vBI.y, vBI.z, bsp, P_BOLT_LIFE, false, pdmg, L * 2.7, L * 0.15, wcol);
                        boltsFired++;
                    }
                    aPlay('fire', { twin: wcnt >= 2, pitch: wst.speed > 400 ? 1.25 : (wst.rate < 3 ? 0.8 : 1), vel: 0.8 });
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
            lockScan(wdt);
            for (i = 0; i < enemies.length; i++) {
                en = enemies[i];
                if (!en.alive) { if (en.beam && en.beam.visible) en.beam.visible = false; continue; }
                if (perfMod && en.cr) {                                  // rev 17 LOD, thresholds scaled by body size so bosses stay solid
                    var lodD = en.g.position.distanceTo(P) / L / Math.max(1, en.len / (6 * L)), lodK = lodD < perfMod.LOD_LO ? 0 : (lodD < perfMod.LOD_IMPOSTOR ? 1 : 2);
                    if (lodK !== en.lod) setLod(en, lodK);
                }
                if (en.isBoss) { updateBoss(en, wdt, bodies); continue; }
                var dtE = wdt;
                if (en.g.position.distanceToSquared(P) > FAR2 * L * L) { en.far ^= 1; if (en.far) continue; dtE = wdt * 2; }
                en.prev.copy(en.g.position);
                if (space && !dead && gt >= (en.cvChk || 0)) {         // rev 22: cover. A rock (>= 4 L) between the enemy and you breaks its lock for 2 s
                    en.cvChk = gt + 0.3 + Math.random() * 0.1;
                    if (en.g.position.distanceToSquared(P) < (AGGRO_L * 1.5 * L) * (AGGRO_L * 1.5 * L) && space.coverTest(en.g.position, P)) en.cvT = gt + 2;
                }
                holdFire = gt < (en.cvT || 0);
                updateEnemy(en, dtE, bodies);
                holdFire = false;
                if (space && en.alive && space.pushOut(en.g.position, en.R, skOut)) en.g.position.copy(skOut);
                if (en.alive) en.vel.subVectors(en.g.position, en.prev).multiplyScalar(1 / dtE);
            }
            for (i = 0; i < allies.length; i++) { if (allies[i].alive) updateAlly(allies[i], wdt, bodies); else if (allies[i].ex.holder.visible) exOff(allies[i].ex); }
            updateOrbs(wdt); updateBombs(wdt);
        }
        updateDrops(dt);
        updateCrates(dt);
        updateBooms(dt);
        updatePeril(dt);
        if (prof.on) { tA0 = performance.now(); prof.ai += tA0 - tM; }
        // projectiles: move, planets eat bolts, hits
        var pr = 0.45 * L, nearR = 20 * L, cullR2 = Math.max(P_BOLT_CULL * L, 70000) * Math.max(P_BOLT_CULL * L, 70000);      // rev 23: cover pulse (1500 u/s) + bolt speed over the 20 s life; time culls them too
        boltFrame++;
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
                    if (vBp.distanceToSquared(P) > cullR2) { bo.active = false; fx.killBolt(bo.id); continue; }      // rev 19: gone beyond the cull range
                    for (j = 0; j < enemies.length; j++) {
                        en = enemies[j];
                        if (!en.alive) continue;
                        var ep = en.g.position, dmg = bo.dmg, crit = false, k, limbHit = false, limbI = -1;
                        // eye cores are the weak points (x2). Bosses only show them while exposed (the windup / mid-attack); closed, the carapace takes 0.5x.
                        for (k = 0; k < en.eyeN; k++) {
                            if (en.cr.eyes[k].userData.dead) continue;
                            var ew = en.eyeW[k], er = en.cr.eyes[k].userData.r * en.sc;
                            if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, ew.x, ew.y, ew.z) < er * er) { crit = true; break; }
                        }
                        // rev 20: the core is a x2 weak point while exposed (x4 once every limb is gone); a closed carapace takes full damage (the 90 s boss hp budget assumes it)
                        if (en.isBoss) { if (crit && (en.open || en.noLimbs)) dmg *= en.noLimbs ? 4 : 2; else crit = false; }
                        else if (crit) dmg *= 2;
                        if (!crit && en.isBoss && en.cr && en.cr.limbs && en.cr.limbs.length) {          // rev 19: boss limbs (capsules) are solid too
                            if (en.capFrame !== boltFrame) { bossCaps(en); en.capFrame = boltFrame; }
                            var cw = en.capW;
                            for (k = 0; k < en.capN; k++) {
                                if (en.cr.limbs[k].dead) continue;
                                var co = k * 7, cr2 = cw[co + 6];
                                if (segSegDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, cw[co], cw[co + 1], cw[co + 2], cw[co + 3], cw[co + 4], cw[co + 5]) < cr2 * cr2) { limbHit = true; limbI = k; break; }
                            }
                        }
                        var bodyR = (en.isBoss && en.limbHp && !en.noLimbs && vBp.distanceToSquared(ep) < (en.reachMax || 0) * (en.reachMax || 0)) ? en.R * 0.5 : en.R;      // rev 25: the halved core only applies inside the limbs' reach; from range the whole hitR eats bolts      // rev 20: with limbs alive the bolt-eating body core is half the ram shell, so bolts reach the limb capsules inside it
                        if (crit || limbHit || segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, ep.x, ep.y, ep.z) < bodyR * bodyR) {
                            dead1 = true; boltHits++;
                            if (!en.isBoss && en.cr && en.cr.stats && en.cr.stats.shield) {       // rev 21 back-shield rule: the front dish eats bolts, the glowing back is the weak point (x1.5)
                                vShA.subVectors(bp, ep); vShB.copy(NEG_Z).applyQuaternion(en.g.quaternion);
                                if (vShA.dot(vShB) > 0) {
                                    try { en.cr.setShieldHit(); } catch (e0) { /* ignore */ }
                                    fx.impact(bp, 0x55e8ff, 1.4); aPlay('shieldHit', { dist: aDist(bp) });
                                    break;
                                }
                                dmg *= 1.5; crit = true;
                            }
                            damageEnemy(en, dmg, crit, bp, limbI);
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
                    if (!dead1 && R27.cvOn) dead1 = cvBoltHit(bo, bp);
                    if (!dead1 && space && gmode === 'fly') dead1 = r28MineBolt(bo, bp, pv);      // rev 28: ore / ice / crystal rocks
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
        if (space) freighterHud();
        hudDmg(dt);
        updatePlayerMarks();
        shardTick(P, 3 * L);
        scanUpdate(dt);
        musicTick(dt, bodies);
        lfCommit(dt);                              // rev 18: final world pose -> local state (also the exit at LF_OFF x R)
        if (prof.on) { var tE = performance.now(); prof.net += tH - tN; prof.hud += tE - tH; prof.total += tE - tS; prof.n++; }
    }

    // ═══ rev 22: world (stores + shards), space (belts, freighters), black hole, the 7/11 station ═════════════════════════════════════════
    var worldMod = null, partsMod = null, world = null, wShardsRef = null;
    var spaceMod = null, space = null, stationMod = null, station = null;
    var engMul = 1, holdFire = false;
    var skOut = new THREE.Vector3(), vSh = new THREE.Vector3(), vShU = new THREE.Vector3(), vSc = new THREE.Vector3();
    function makeWorld() {
        if (!ps || !worldMod) return;
        try {
            if (world) { try { world.dispose(); } catch (e0) { /* ignore */ } world = null; }
            world = worldMod.createWorld(THREE, ps, L, { parts: partsMod, weapons: wpnMod });
            world.onShard = grantShard; world.onBusted = r28Busted; world.onCopSay = r28CopSay; wShardsRef = null; knownSet = null; knownFor();
            ps.attachWorld(world);
        } catch (e) { console.info('[ship] world unavailable', e); world = null; }
    }
    function makeSpace() {
        if (!spaceMod) return;
        try { if (space) space.dispose(); space = spaceMod.createSpace(engine, L); space.group.visible = state === 'piloting'; } catch (e) { console.info('[ship] space unavailable', e); space = null; }
    }
    function makeStation() {
        if (!stationMod) return;
        try { if (station) station.dispose(); station = stationMod.createStation(engine, L); station.setVisible(state === 'piloting'); } catch (e) { console.info('[ship] station unavailable', e); station = null; }
    }
    Promise.all([import('./ship-world.js'), import('./ship-parts.js').catch(function () { return null; }), import('./ship-weapons.js').catch(function () { return null; })]).then(function (r) { worldMod = r[0]; partsMod = r[1]; if (!wpnMod) wpnMod = r[2]; makeWorld(); }).catch(function (e) { console.info('[ship] ship-world unavailable', e); });
    import('./ship-space.js').then(function (m) { spaceMod = m; makeSpace(); }).catch(function (e) { console.info('[ship] ship-space unavailable', e); });
    import('./ship-station.js').then(function (m) { stationMod = m; makeStation(); }).catch(function (e) { console.info('[ship] ship-station unavailable', e); });

    // planet-local -> world (the world group rides the planet frame; local units = world units)
    function locToWorld(v, out) {
        var n = world && world.node;
        if (!n || !n.mesh) return out.copy(v);
        return out.copy(v).applyQuaternion(n.mesh.quaternion).add(n.anchor.position);
    }
    // ─── shards: persisted per planet id, reset daily ───────────────────────────────────────────────────────────────────────────────
    function dayKey() { var d = new Date(); return d.getFullYear() + '-' + (d.getMonth() + 1) + '-' + d.getDate(); }
    function grantShard(sh) {
        if (!sh) return;
        var pf = ensureProfile(userName()); if (!curUser) curUser = userName();
        if (!pf.shards || pf.shards.day !== dayKey()) pf.shards = { day: dayKey(), ids: [] };
        if (pf.shards.ids.indexOf(sh.id) >= 0) { sh.taken = true; return; }
        pf.shards.ids.push(sh.id); sh.taken = true;
        locToWorld(sh.pos, vSh);
        addUnits(UNIT_SHARD, vSh, false); giveItem(R25_SHARD, 1); aPlay('shard'); fx.flash(vSh, 0xffd36a);
    }
    function worldSync() {
        if (!world || !world.node) { wShardsRef = null; return; }
        if (world.shards !== wShardsRef) {
            wShardsRef = world.shards;
            var pf = curProfile();
            if (pf && pf.shards && pf.shards.day === dayKey()) world.markTaken(pf.shards.ids);
            if (pf && pf.harv && typeof world.markHarvested === 'function') { try { world.markHarvested(pf.harv); } catch (e0) { /* ignore */ } }
        }
    }
    // fly / walk into a shard (the world also auto-takes within 1.6 L of the player; both paths end in grantShard)
    function shardTick(pos, rad) {
        var w = world;
        if (!w || !w.node || !w.shards || dead) return;
        var node = w.node, sh = w.shards;
        for (var i = 0; i < sh.length; i++) {
            var s = sh[i];
            if (!s || s.taken) continue;
            locToWorld(s.pos, vSh); vShU.subVectors(vSh, node.anchor.position).normalize(); vSh.addScaledVector(vShU, 0.9 * L);
            if (vSh.distanceToSquared(pos) > rad * rad) continue;
            if (w.takeShard(s.id)) grantShard(s);
        }
    }
    // ─── upgrades applied from the profile ──────────────────────────────────────────────────────────────────────────────────────────
    function applyUpgrades() {
        var pf = curProfile(), st = clamp(pget(pf, 'shieldTier', 0) | 0, 0, 3), et = clamp(pget(pf, 'engineTier', 0) | 0, 0, 3);
        HP_MAX = HP_BASE + 20 * st + 10 * (pget(pf && pf.upgrades, 'shield', 0) | 0); engMul = 1 + 0.1 * et + 0.03 * (pget(pf && pf.upgrades, 'engine', 0) | 0);
        var hs = r28HullStats(hullFor(pf)); if (hs) { HP_MAX = Math.max(40, Math.round(HP_MAX * (1 + (hs.hp || 0) / 100))); engMul *= 1 + ((hs.cruise || 0) + (hs.boost || 0)) / 200; }      // rev 28: hull kind stats
        if (hp > HP_MAX) hp = HP_MAX;
    }
    // ─── rev 23: items, lingo, inventory (E), eating effects, fries glow ───────────────────────────────────────────────────────────
    var itemCache = Object.create(null);
    function itemOf(rec) {
        if (!rec) return null;
        if (rec.it) return rec.it;      // rev 25: crafted / part / pet stacks carry the whole item
        var key = rec.id + (rec.d ? 'd' : ''), it = itemCache[key];
        if (it) return it;
        try { it = rec.id === 'fries' ? ITM.FRIES : ITM.generateItem(rec.seed >>> 0, rec.d ? { dealer: true } : undefined); } catch (e0) { it = null; }
        if (it) itemCache[key] = it;
        return it;
    }
    function giveItem(it, n) {
        var pf = ensureProfile(userName()), i, r;
        if (!curUser) curUser = userName();
        var sk0 = CRF.stackKeyOf(it), bpNew = !!(it.tags && it.tags.indexOf('bp') >= 0 && !pf.items.some(function (q) { return q.id === sk0; }));
        if (bpNew) setTimeout(function () { r28BpGiven(it); }, 0);
        for (i = 0; i < pf.items.length; i++) { r = pf.items[i]; if (r.id === sk0 && !!r.d === !!it.dealer) { r.n = Math.min(9999, r.n + n); return r; } }
        r = { id: sk0, seed: (it.seed >>> 0) || 0, n: n, d: it.dealer ? 1 : 0 };
        if (it.kind === 'part' || it.kind === 'material' || it.category || it.pet || it.stackKey) r.it = JSON.parse(JSON.stringify(it));
        pf.items.push(r); if (!r.it) itemCache[it.id + (it.dealer ? 'd' : '')] = it;
        if (pf.items.length > 400) pf.items.shift();
        return r;
    }
    var knownSet = null, knownPf = null;
    function knownFor() {                       // the profile's learned lingo words as ONE Set shared with the world's NPCs
        var pf = ensureProfile(userName());
        if (!knownSet || knownPf !== pf) { knownPf = pf; knownSet = new Set(pf.known); }
        if (world && world.known !== knownSet && typeof world.setKnown === 'function') { try { world.setKnown(knownSet); } catch (e0) { /* ignore */ } }
        return knownSet;
    }
    function learnWords(n) {
        var ks = knownFor(), got = [];
        try { got = LNG.teach(ks, n) || []; } catch (e0) { got = []; }
        if (got.length) { knownPf.known = Array.from(ks); for (var i = 0; i < got.length; i++) addRow('', '', 'learned: ' + got[i], 'is-sys'); }
        return got;
    }
    function lingoFill(el, text) {              // alien words in their own colour class, known words normal
        while (el.firstChild) el.removeChild(el.firstChild);
        var segs = LNG.render(String(text == null ? '' : text)), i, s;
        for (i = 0; i < segs.length; i++) { s = document.createElement('span'); if (segs[i].alien) s.className = 'sc-al'; s.textContent = segs[i].text; el.appendChild(s); }
    }
    function npcLineOf(id, fallbackNpc, ctx) {
        var line = '';
        knownFor();
        try {
            if (world && typeof world.npcLine === 'function') line = ctx && world.npcSay ? String(world.npcSay(id, ctx) || '') : String(world.npcLine(id) || '');
            if (!line && fallbackNpc && typeof fallbackNpc.say === 'function') line = String(fallbackNpc.say(ctx || 'greeting') || '');
        } catch (e0) { line = ''; }
        return line;
    }
    var GLYPH = { food: '▲', drink: '▼', snack: '◆', fries: '≡' };
    function cssHex(c) { return '#' + ('000000' + ((c >>> 0) & 0xffffff).toString(16)).slice(-6); }
    // active effects: timeline = 2 s ramp in, hold, fade over the last 15 %; several blend (max per param). engine.vision does the drawing.
    var FX_RAMP = 2, FX_TAIL = 0.15, FX_MAX = 8, fxAct = [], fxVisOn = false, fxSpeed = 1, fxJump = 1, fxWob = 0, fxWobC = -1, fxHudT = 0;
    var fxB = { blur: 0, chroma: 0, hue: 0, wobble: 0, double: 0, contrast: 0, invert: 0, fov: 0, timeScale: 1, tint: 0 }, fxTc = [0, 0, 0];
    var FX_AMP = ['blur', 'chroma', 'wobble', 'double', 'contrast', 'invert', 'fov'];
    var elFxl = document.createElement('div'); elFxl.className = 'sh-fxl'; hud.appendChild(elFxl);
    var fxRows = [];
    (function buildFxl() { for (var i = 0; i <= FX_MAX; i++) { var r = document.createElement('div'); r.className = 'fx-r'; r.style.display = 'none'; r.innerHTML = '<b></b><u></u>'; elFxl.appendChild(r); fxRows.push({ el: r, n: r.firstChild, t: r.lastChild, txt: '', on: false }); } })();
    function fmtT(s) { s = Math.max(0, Math.ceil(s)); var m = Math.floor(s / 60), r = s % 60; return m + ':' + (r < 10 ? '0' : '') + r; }
    function fxStart(it) {
        var e = it.effect || {}, pf = ensureProfile(userName()), i, a;
        if (it.kind === 'fries' || e.realtime) {
            pf.glowUntil = Date.now() + ((e.glow && e.glow.minutes) || 45) * 60000; glowChk = 0; glowTick(0);
            addRow('', '', 'you are glowing. ' + ((e.glow && e.glow.minutes) || 45) + ' minutes.', 'is-sys'); return;
        }
        for (i = 0; i < fxAct.length; i++) if (fxAct[i].key === it.id) { fxAct[i].t = 0; fxAct[i].dur = e.duration || 60; return; }
        a = { key: it.id, name: it.name, col: it.color, t: 0, dur: Math.max(5, e.duration || 60), p: e.params || {}, ex: e.extras || {}, tc: e.tintColor, sg: e.signature || null };
        if (fxAct.length >= FX_MAX) fxAct.shift();
        fxAct.push(a);
    }
    function fxReset() {
        fxAct.length = 0;
        if (fxVisOn) { fxVisOn = false; try { engine.vision.clear(); } catch (e0) { /* ignore */ } }
        fxSpeed = fxJump = 1; fxWob = 0; fxSetWob(0);
        for (var i = 0; i < fxRows.length; i++) if (fxRows[i].on) { fxRows[i].on = false; fxRows[i].el.style.display = 'none'; }
    }
    function fxSetWob(w) {
        w = Math.round(w * 10) / 10;
        if (w === fxWobC) return; fxWobC = w;
        elChat.classList.toggle('is-wob', w > 0); elChat.style.setProperty('--wob', String(w));
    }
    function fxTick(dt) {
        var i, k, a, env, p, v, x, tl, B = fxB, n = fxAct.length;
        if (n) {
            for (k = 0; k < FX_AMP.length; k++) B[FX_AMP[k]] = 0;
            B.hue = 0; B.timeScale = 1; B.tint = 0;
            var tsd = 0, sp = 0, jp = 0, wb = 0;
            for (i = n - 1; i >= 0; i--) {
                a = fxAct[i]; a.t += dt;
                if (a.t >= a.dur) { fxAct.splice(i, 1); continue; }
                env = Math.min(1, a.t / FX_RAMP); tl = a.dur * FX_TAIL;
                if (a.dur - a.t < tl) env = Math.min(env, (a.dur - a.t) / tl);
                env = env * env * (3 - 2 * env);
                p = a.p; x = a.ex;
                var sg = a.sg, sid = sg ? sg.id : '', em = env, TT = 6.2832 * a.t;      // rev 28: effect signatures (all periods >= 4 s: strobe-safe)
                if (sid === 'pulse') em = env * (1 + sg.depth * Math.sin(TT / Math.max(4, sg.period)));
                for (k = 0; k < FX_AMP.length; k++) {
                    var fk = FX_AMP[k], fm = em;
                    if (sid === 'tunnel' && fk === 'fov') fm *= 1 + 0.25 * Math.sin(TT / 9);
                    else if (sid === 'dreamy' && fk === 'blur') fm *= 1 + 0.25 * Math.sin(TT / 7);
                    v = (p[fk] || 0) * fm; if (sid === 'tunnel' && fk === 'contrast') v += (sg.vignette || 0.4) * 0.2 * env;
                    if (v > B[fk]) B[fk] = v;
                }
                v = (p.hue || 0) * env; if (sid === 'mirror') v *= Math.cos(TT / 14); else if (sid === 'dreamy') v += (sg.drift || 0.05) * Math.sin(TT / 11) * env;
                if (Math.abs(v) > Math.abs(B.hue)) B.hue = v;
                if (p.timeScale) { v = (p.timeScale - 1) * env; if (Math.abs(v) > Math.abs(tsd)) tsd = v; }
                v = (p.tint || 0) * env; if (v > B.tint) { B.tint = v; if (a.tc != null) { fxTc[0] = ((a.tc >> 16) & 255) / 255; fxTc[1] = ((a.tc >> 8) & 255) / 255; fxTc[2] = (a.tc & 255) / 255; } }
                if (x.speed) { v = (x.speed - 1) * env; if (Math.abs(v) > Math.abs(sp)) sp = v; }
                if (x.jump) { v = (x.jump - 1) * env; if (Math.abs(v) > Math.abs(jp)) jp = v; }
                v = (x.chatWobble || 0) * env; if (v > wb) wb = v;
            }
            B.timeScale = 1 + tsd; fxSpeed = 1 + sp; fxJump = 1 + jp; fxWob = wb;
            if (fxAct.length) {
                try { engine.vision.set({ blur: B.blur, chroma: B.chroma, hue: B.hue, wobble: B.wobble, double: B.double, contrast: B.contrast, invert: B.invert, fov: B.fov, timeScale: B.timeScale, tint: B.tint }); if (B.tint > 0) engine.vision.set({ tint: fxTc }); fxVisOn = true; } catch (e0) { /* ignore */ }
            }
        }
        if (!fxAct.length && fxVisOn) { fxVisOn = false; fxSpeed = fxJump = 1; fxWob = 0; try { engine.vision.clear(); } catch (e1) { /* ignore */ } }
        fxSetWob(fxWob);
        fxHudT -= dt;
        if (fxHudT <= 0) { fxHudT = 0.25; fxHud(); }
    }
    function fxHud() {
        var i, r, row, t, nm, rows = 0, pf = curProfile(), gl = glowOn && pf ? (pf.glowUntil - Date.now()) / 1000 : 0;
        for (i = 0; i < fxAct.length && rows < FX_MAX; i++) {
            row = fxRows[rows++]; nm = fxAct[i].name; t = fmtT(fxAct[i].dur - fxAct[i].t);
            if (!row.on) { row.on = true; row.el.style.display = ''; }
            if (row.txt !== nm + t) { row.txt = nm + t; row.n.textContent = nm; row.t.textContent = t; row.el.style.borderLeftColor = cssHex(fxAct[i].col); }
        }
        if (gl > 0 && rows <= FX_MAX) {
            row = fxRows[rows++]; t = fmtT(gl);
            if (!row.on) { row.on = true; row.el.style.display = ''; }
            if (row.txt !== 'GLOW' + t) { row.txt = 'GLOW' + t; row.n.textContent = 'GLOW'; row.t.textContent = t; row.el.style.borderLeftColor = '#7fd8ff'; }
        }
        for (i = rows; i < fxRows.length; i++) if (fxRows[i].on) { fxRows[i].on = false; fxRows[i].el.style.display = 'none'; }
    }
    // fries glow: light blue emissive + glow sprite on the human and the hull until profile.glowUntil (epoch ms, survives reloads); bit 8 of the net pose
    var glowOn = false, glowChk = 0, glowHull = null, glowHumObj = null, glowHumSpr = null, glowTexC = null, glowRimT = 0;
    function glowTexture() {
        if (glowTexC) return glowTexC;
        var cv = document.createElement('canvas'); cv.width = cv.height = 64;
        var c = cv.getContext('2d'), rg = c.createRadialGradient(32, 32, 0, 32, 32, 32);
        rg.addColorStop(0, 'rgba(255,255,255,1)'); rg.addColorStop(0.3, 'rgba(255,255,255,0.5)'); rg.addColorStop(1, 'rgba(255,255,255,0)');
        c.fillStyle = rg; c.fillRect(0, 0, 64, 64);
        glowTexC = new THREE.CanvasTexture(cv); return glowTexC;
    }
    function glowSprite() {
        var s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: 0x7FD8FF, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
        s.frustumCulled = false; return s;
    }
    function glowHullRim(on) {
        shipRoot.traverse(function (o) {
            var m = o.material, u = m && m.uniforms;
            if (!u || !u.uRim) return;
            if (!m.userData.rim0) m.userData.rim0 = u.uRim.value.clone();
            if (on) u.uRim.value.set(0.55, 1.5, 2.4); else u.uRim.value.copy(m.userData.rim0);
        });
    }
    function glowHumMats(h, on) {
        h.group.traverse(function (o) { var m = o.material; if (m && m.isMeshBasicMaterial) { if (on) m.color.setRGB(0.4, 1.6, 2.6); else m.color.setRGB(1, 1, 1); } });
    }
    function glowTick(dt) {
        glowChk -= dt;
        var pf, on = glowOn, i;
        if (glowChk <= 0) { glowChk = 0.5; pf = curProfile(); on = !!(pf && pf.glowUntil > Date.now()); }
        if (on !== glowOn) {
            glowOn = on;
            if (!glowHull && on) { glowHull = glowSprite(); glowHull.scale.setScalar(3.2); shipRoot.add(glowHull); }
            if (glowHull) glowHull.visible = on;
            glowHullRim(on); glowRimT = 0;
            if (!on && glowHumObj) { glowHumMats(glowHumObj, false); if (glowHumSpr) glowHumSpr.visible = false; }
        }
        if (!glowOn) return;
        glowRimT -= dt; if (glowRimT <= 0) { glowRimT = 0.5; glowHullRim(true); }
        if (hum.obj && glowHumObj !== hum.obj) {
            glowHumObj = hum.obj; glowHumSpr = glowSprite(); glowHumSpr.position.set(0, 0.5, 0); glowHumSpr.scale.setScalar(4.2); hum.obj.group.add(glowHumSpr);
            glowHumMats(hum.obj, true);
        }
        if (glowHumSpr) glowHumSpr.visible = true;
        var pu = 0.85 + 0.15 * Math.sin(gt * 2.4);
        if (glowHull) glowHull.material.opacity = pu;
        if (glowHumSpr) glowHumSpr.material.opacity = pu;
    }
    // ─── inventory (E): Minecraft grid, right click = eat / drink, left click a weapon = equip ─────────────────────────────────────
    var INV_COLS = 9, INV_ROWS = 4, INV_SLOTS = INV_COLS * INV_ROWS;      // rev 24: 36 slots + a 9-slot hotbar row (weapons)
    var invOpen = false, invTop = 0, invWTop = 0, invSlots = [], invWSlots = [], iconsMod = null;
    import('./ship-icons.js').then(function (m) { iconsMod = m; if (invOpen) invFill(); }).catch(function (e) { console.info('[ship] ship-icons unavailable', e); });
    var elInv = document.createElement('div');
    elInv.className = 'sh-inv';
    elInv.innerHTML = '<div class="si-h"><b>Inventory</b><span class="si-u"></span></div><div class="si-tabs"><span class="is-on">ITEMS</span><span>RECIPE BOOK</span></div><div class="si-book"></div><div class="si-grid"></div><div class="si-wrow"></div><div class="si-f">Right click eat / drink / fit mod · Left click equip · Wheel scroll · E / Esc close</div>';
    hud.appendChild(elInv);
    var elInvTip = document.createElement('div'); elInvTip.className = 'si-tip'; hud.appendChild(elInvTip);
    var elInvGrid = elInv.querySelector('.si-grid'), elInvW = elInv.querySelector('.si-wrow'), elInvU = elInv.querySelector('.si-u'), invTipKey = '';
    (function buildInv() {
        var i, s, cv;
        for (i = 0; i < INV_SLOTS + INV_COLS; i++) {
            s = document.createElement('div'); s.className = 'si-s is-empty'; s.innerHTML = '<canvas width="16" height="16"></canvas><em></em>';
            s.setAttribute('data-i', String(i)); cv = s.firstChild;
            var rec = { el: s, cv: cv, ctx: cv.getContext('2d'), n: s.lastChild, k: '' };
            if (i < INV_SLOTS) { elInvGrid.appendChild(s); invSlots.push(rec); }
            else { s.setAttribute('data-w', String(i - INV_SLOTS)); elInvW.appendChild(s); invWSlots.push(rec); }
        }
    })();
    function invPaint(sl, ic, key) {          // pooled canvas per slot; repaint only when the icon changes
        if (sl.k === key) return;
        sl.k = key; sl.ctx.clearRect(0, 0, 16, 16);
        if (ic) sl.ctx.drawImage(ic, 0, 0);
    }
    function invSlotOf(t) { while (t && t !== elInv && !(t.getAttribute && (t.getAttribute('data-i') != null || t.getAttribute('data-c') != null || t.getAttribute('data-o') != null || t.getAttribute('data-m') != null))) t = t.parentNode; return (t && t !== elInv) ? t : null; }
    function invFill() {
        var pf = ensureProfile(userName()), items = pf.items, ws = pf.weapons, i, s, rec, it, w, rows = Math.max(INV_ROWS, Math.ceil(items.length / INV_COLS));
        invTop = clamp(invTop, 0, Math.max(0, rows - INV_ROWS)); invWTop = clamp(invWTop, 0, Math.max(0, ws.length - INV_COLS));
        for (i = 0; i < INV_SLOTS; i++) {
            s = invSlots[i]; rec = items[invTop * INV_COLS + i]; it = rec ? itemOf(rec) : null;
            if (it) {
                s.el.classList.remove('is-empty');
                var ic = null; try { ic = iconsMod ? iconsMod.iconFor(it) : null; } catch (e0) { ic = null; }
                invPaint(s, ic, ic ? 'I' + it.id + (it.dealer ? 'd' : '') : '');
                var shown = rec.n - r25Reserved(rec.id); s.n.textContent = shown > 1 ? String(shown) : ''; s.el.style.opacity = shown > 0 ? '' : '0.4';
            } else { s.el.classList.add('is-empty'); invPaint(s, null, ''); s.n.textContent = ''; }
        }
        for (i = 0; i < INV_COLS; i++) {
            s = invWSlots[i]; w = ws[invWTop + i];
            if (w) {
                s.el.classList.remove('is-empty');
                var wi = null; try { wi = iconsMod ? iconsMod.weaponIcon(w) : null; } catch (e1) { wi = null; }
                invPaint(s, wi, wi ? 'W' + (w.id || w.name) + (w.shape || '') + (w.color | 0) + (w.cls || '') : '');
                s.n.textContent = ''; s.el.classList.toggle('is-eq', !!(pf.weapon && pf.weapon.id === w.id));
            } else { s.el.classList.add('is-empty'); s.el.classList.remove('is-eq'); invPaint(s, null, ''); s.n.textContent = ''; }
        }
        r25Fill(); if (R28.book) r28BookFill();
        elInvU.textContent = CRF.fmt(pf.gor) + ' · ' + items.reduce(function (a, r) { return a + r.n; }, 0) + ' items';
    }
    function invTip(sl) {
        var pf = ensureProfile(userName()), t = '', rec, it, w;
        if (sl) {
            if (sl.getAttribute('data-c') != null || sl.getAttribute('data-o') != null || sl.getAttribute('data-m') != null) t = r25Tip(sl);
            else if (sl.getAttribute('data-w') != null) { w = pf.weapons[invWTop + (+sl.getAttribute('data-w'))]; if (w) t = w.name + '\n' + (wpnMod && wpnMod.weaponLine ? wpnMod.weaponLine(w) : '') + '\nLeft click to equip'; }
            else { rec = pf.items[invTop * INV_COLS + (+sl.getAttribute('data-i'))]; it = rec ? itemOf(rec) : null; if (it) t = describeItem(it).replace(/\nPrice: .*$/, '') + '\nx' + rec.n + invAct(it); }
        }
        if (t === invTipKey) return;
        invTipKey = t;
        if (!t) { elInvTip.classList.remove('is-on'); return; }
        elInvTip.textContent = '';
        t.split('\n').forEach(function (ln, k) { var d = document.createElement('div'); d.className = k === 0 ? 'st-n' : 'st-l'; d.textContent = ln; elInvTip.appendChild(d); });
        elInvTip.classList.add('is-on');
    }
    function invTipPos(e) {
        if (!elInvTip.classList.contains('is-on')) return;
        var x = e.clientX + 16, y = e.clientY - 30, w = elInvTip.offsetWidth, h = elInvTip.offsetHeight;
        if (x + w > window.innerWidth - 6) x = e.clientX - w - 12;
        if (y < 6) y = e.clientY + 18;
        if (y + h > window.innerHeight - 6) y = window.innerHeight - h - 6;
        elInvTip.style.transform = 'translate(' + Math.round(x) + 'px,' + Math.round(y) + 'px)';
    }
    function eatSlot(idx) {
        var pf = ensureProfile(userName()), rec = pf.items[idx], it = rec ? itemOf(rec) : null;
        if (!it) return;
        if (r25Use(it, rec, idx)) return;       // rev 25: mods / upgrades / pets are not food
        rec.n--; if (rec.n <= 0) pf.items.splice(idx, 1);
        aPlay('ui', { vel: 0.7 }); aPlay('hit', { pitch: 0.35, vel: 0.5 });
        fxStart(it);
        addRow('', '', (it.kind === 'drink' ? 'drank ' : 'ate ') + it.name, 'is-sys');
        writeSave(); invFill(); invTip(null);
    }
    elInv.addEventListener('mouseover', function (e) { invTip(invSlotOf(e.target)); invTipPos(e); });
    elInv.addEventListener('mousemove', function (e) { invTipPos(e); });
    elInv.addEventListener('mouseleave', function () { invTip(null); });
    elInv.addEventListener('contextmenu', function (e) { e.preventDefault(); var s = invSlotOf(e.target); if (s && s.getAttribute('data-i') != null) eatSlot(invTop * INV_COLS + (+s.getAttribute('data-i'))); });
    elInv.addEventListener('click', function (e) {
        var s = invSlotOf(e.target); if (!s || s.getAttribute('data-w') == null) return;
        var w = ensureProfile(userName()).weapons[invWTop + (+s.getAttribute('data-w'))];
        if (w) { equipWeapon(w); aPlay('ui', { vel: 0.6 }); invFill(); invTip(s); }
    });
    elInv.addEventListener('wheel', function (e) {
        if (R28.book && e.target.closest && e.target.closest('.si-book')) return;      // rev 28: the recipe book scrolls natively
        e.preventDefault();
        var dir = e.deltaY > 0 ? 1 : -1;
        if (invSlotOf(e.target) && invSlotOf(e.target).getAttribute('data-w') != null) invWTop += dir; else invTop += dir;
        invFill();
    }, { passive: false });
    function openInv() {
        if (invOpen || cmdOpen || state !== 'piloting' || boarding || exiting || dead) return;
        if (R28.book) { R28.book = false; elInv.classList.remove('is-book'); for (var bi = 0; bi < elBookTabs.children.length; bi++) elBookTabs.children[bi].classList.toggle('is-on', bi === 0); }
        invOpen = true; cmdOpen = true; cmdGuardUntil = performance.now() + 600;
        keys = Object.create(null); firing = false; mdx = mdy = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        if (locked()) { try { document.exitPointerLock(); } catch (e0) { /* ignore */ } }
        hud.classList.add('is-inv'); elInv.classList.add('is-on');
        invTop = 0; invWTop = 0; r25CraftAvail(); invFill(); invTip(null); aPlay('ui', { vel: 0.5 });
    }
    function closeInv() {
        if (!invOpen) return;
        invOpen = false; cmdOpen = false; r25ClearGrid();
        hud.classList.remove('is-inv'); elInv.classList.remove('is-on'); invTip(null);
        keys = Object.create(null); mdx = mdy = 0;
        aPlay('ui', { vel: 0.4 });
        if (state === 'piloting') { try { var p = document.body.requestPointerLock(); if (p && p.catch) p.catch(function () { /* keyboard only */ }); } catch (e0) { /* ignore */ } }
    }
    function toggleInv() { if (invOpen) closeInv(); else openInv(); }
    function invKey(e) {
        var c = e.code;
        e.preventDefault(); e.stopPropagation();
        if (c === 'Escape') { e.__shipHandled = true; closeInv(); return; }
        if (e.repeat) return;
        if (c === 'KeyE' || c === 'Backspace') { closeInv(); return; }
        if (c === 'KeyR' || c === 'Tab') { r28BookSet(!R28.book); return; }
        if (c === 'ArrowDown') { invTop++; invFill(); } else if (c === 'ArrowUp') { invTop--; invFill(); }
    }
    // ─── rev 25: gorCoin stacks, 3x3 crafting, interior, pets, crew, missions (ship-craft.js / ship-interior.js, guarded) ──────────────────────────
    var R25 = { mod: null, int: null, L: 0, craftOn: false, force: false, cand: null, t: 0, ctm: null, ctF: 0, tdt: 0 };
    var ix = { on: false, snap: 0, from: '', w: new THREE.Vector3(), q: new THREE.Quaternion(), sc: 1 };
    var ik = { hp: new THREE.Vector3(), yaw: 0, pitch: 0.3, vv: 0, air: false, jumpHeld: false, moving: false, running: false };
    import('./ship-interior.js').then(function (m) { R25.mod = m; }).catch(function (e) { console.info('[ship] ship-interior unavailable', e); });
    function hash25(s) { var h = 2166136261; s = String(s); for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
    function mkScrap(id, name, color, hint) { return { id: id, name: name, kind: 'junk', base: name, infusion: null, modifier: null, color: color, price: 6, tier: 1, category: 'junk', tags: ['junk'], stackKey: id, iconHint: hint, blurb: 'Bits off something that used to shoot back.', dealer: false }; }
    var R25_SCRAP = [mkScrap('scrap-plate', 'Scrap Plate', 0x9AA0AA, 'tray'), mkScrap('scrap-wire', 'Scrap Wire', 0xFFB05C, 'bar'), mkScrap('scrap-gear', 'Scrap Gear', 0xC8C0E8, 'donut')];
    var R25_SHARD = { id: 'signal-shard', name: 'Signal Shard', kind: 'material', base: 'Signal Shard', infusion: null, modifier: null, color: 0xFFD36A, price: 28, tier: 1, category: 'material', tags: ['material'], stackKey: 'signal-shard', iconHint: 'candy', blurb: 'A crystal that still remembers a radio station.', dealer: false };
    function r25Scrap(bonus) {
        var n = 1 + ((Math.random() * 3) | 0) + (bonus | 0), i;
        if (!curUser) curUser = userName();
        for (i = 0; i < n; i++) giveItem(R25_SCRAP[(Math.random() * 3) | 0], 1 + ((Math.random() * 2) | 0));
        unitSaveT = unitSaveT || setTimeout(function () { unitSaveT = 0; writeSave(); }, 900);
    }
    function describeItem(it) {
        if (!it) return '';
        if (!(it.category || it.kind === 'part' || it.pet)) { try { return ITM.describe(it).replace(/ units$/, ' gorCoin'); } catch (e0) { /* fall through */ } }
        var l = [it.name];
        if (it.blurb) l.push(it.blurb);
        if (it.pet) l.push('Pet · happiness ' + (it.pet.happy | 0));
        if (it.mod) {
            var d = it.mod.delta || {}, s = Object.keys(d).filter(function (k) { return d[k]; }).map(function (k) { return k + ' ' + (k === 'count' ? '+' + d[k] : (d[k] > 0 ? '+' : '') + Math.round(d[k] * 100) + '%'); });
            l.push(String(it.mod.slot).toUpperCase() + ' MOD · ' + s.join(' ') + (it.mod.special ? ' · ' + it.mod.special : ''));
        }
        if (it.upgrade) l.push('SHIP UPGRADE · ' + String(it.upgrade.slot).toUpperCase() + ' +' + it.upgrade.level);
        if (it.category === 'food' && it.effect) l.push('Food · ' + it.effect.duration + ' s');
        l.push('Price: ' + CRF.fmt(it.price || 0));
        return l.join('\n');
    }
    function invAct(it) {
        if (it.category === 'weapon-mod') return ' · right click to fit your weapon';
        if (it.category === 'ship-upgrade') return ' · right click to install';
        if (it.pet || it.kind === 'part' || it.kind === 'material' || it.kind === 'junk') return R25.craftOn ? ' · click to place in the grid' : '';
        return ' · right click to ' + (it.kind === 'drink' ? 'drink' : 'eat');
    }
    function takeKey(key, n) {
        var pf = ensureProfile(userName()), i;
        for (i = 0; i < pf.items.length; i++) if (pf.items[i].id === key) { pf.items[i].n -= n; if (pf.items[i].n <= 0) pf.items.splice(i, 1); return true; }
        return false;
    }
    function r25Use(it, rec, idx) {
        var pf = ensureProfile(userName()), res;
        if (it.category === 'weapon-mod' && it.mod) {
            var w = pf.weapon;
            if (!w && wpnMod && wpnMod.STARTER_WEAPON) { w = JSON.parse(JSON.stringify(wpnMod.STARTER_WEAPON)); if (!w.id) w.id = 'starter'; }
            if (!w) { addRow('', '', 'no weapon to fit', 'is-sys'); return true; }
            res = CRF.applyMod(w, it); var nw = res.weapon;
            pf.weapons = pf.weapons.filter(function (x) { return x.id !== nw.id; }); pf.weapons.push(nw); pf.weapon = nw;
            rec.n--; if (rec.n <= 0) pf.items.splice(idx, 1);
            aPlay('buy', { pitch: 1.2 }); addRow('', '', res.line, 'is-sys'); writeSave(); invFill(); invTip(null); return true;
        }
        if (it.category === 'ship-upgrade' && it.upgrade) {
            res = CRF.applyUpgrade(pf, it); Object.assign(pf, res.profile); applyUpgrades();
            rec.n--; if (rec.n <= 0) pf.items.splice(idx, 1);
            aPlay('buy', { pitch: 1.2 }); addRow('', '', 'SHIP · ' + res.line, 'is-sys'); writeSave(); invFill(); invTip(null); return true;
        }
        if (it.pet && petFollowToggle(it)) { invFill(); invTip(null); return true; }
        if (it.pet || it.kind === 'part' || it.kind === 'material' || it.kind === 'junk') { addRow('', '', 'not edible: ' + it.name, 'is-sys'); return true; }
        return false;
    }
    // ── crafting grid ──
    var craftGrid = [null, null, null, null, null, null, null, null, null], craftOut = null, craftCells = [], elCraft, elCraftO, elMods, modCells = [];
    (function buildCraft() {
        function cellEl(attr, val) { var d = document.createElement('div'); d.className = 'si-s is-empty'; d.setAttribute(attr, String(val)); d.innerHTML = '<canvas width="16" height="16"></canvas><em></em>'; var cv = d.firstChild; return { el: d, cv: cv, ctx: cv.getContext('2d'), n: d.lastChild, k: '' }; }
        elCraft = document.createElement('div'); elCraft.className = 'si-craft'; elCraft.style.display = 'none';
        elCraft.innerHTML = '<div class="sc-h">CRAFTING</div><div class="sc-b"><div class="sc-grid"></div><i>&#9654;</i><div class="sc-out"></div></div>';
        var g = elCraft.querySelector('.sc-grid'), i, c;
        for (i = 0; i < 9; i++) { c = cellEl('data-c', i); g.appendChild(c.el); craftCells.push(c); }
        craftOut = cellEl('data-o', 0); elCraft.querySelector('.sc-out').appendChild(craftOut.el);
        elMods = document.createElement('div'); elMods.className = 'si-mods'; elMods.innerHTML = '<b>MODS</b>';
        ['scope', 'coil', 'chamber'].forEach(function (sl) { var d = document.createElement('div'); d.className = 'si-s is-empty sm-s'; d.setAttribute('data-m', sl); d.innerHTML = '<span>' + sl.slice(0, 3).toUpperCase() + '</span>'; elMods.appendChild(d); modCells.push({ el: d, sl: sl, sp: d.firstChild }); });
        var f = elInv.querySelector('.si-f'); elInv.insertBefore(elMods, f); elInv.insertBefore(elCraft, f);
    })();
    function r25Reserved(key) { var n = 0; for (var i = 0; i < 9; i++) if (craftGrid[i] && craftGrid[i].key === key) n += craftGrid[i].n; return n; }
    function r25ClearGrid() { for (var i = 0; i < 9; i++) craftGrid[i] = null; }
    function gridStacks() { return craftGrid.map(function (c) { return c ? { id: c.key, item: c.it, n: c.n } : null; }); }
    function ixNear(o, extra) { return !!(o && o.pos && hum.w.distanceTo(o.pos) < Math.max(o.radius || 0, 0.18 * L) + (extra || 0)); }
    function r25CraftAvail() {
        var on = R25.force;
        try {
            if (gmode === 'ifoot' && R25.int && ixNear(R25.int.table, 0.1 * L)) on = true;
            else if (gmode === 'foot' && counterNear()) on = true;
            else if (gmode === 'sfoot' && sk.mode === 'deck' && stationInteract().kind === 'store') on = true;
        } catch (e0) { /* ignore */ }
        R25.craftOn = on; if (!on) r25ClearGrid();
    }
    function paintCell(c, it, n) {
        if (!it) { c.el.classList.add('is-empty'); invPaint(c, null, ''); c.n.textContent = ''; return; }
        c.el.classList.remove('is-empty');
        var ic = null; try { ic = iconsMod ? iconsMod.iconFor(it) : null; } catch (e0) { ic = null; }
        invPaint(c, ic, ic ? 'C' + CRF.stackKeyOf(it) + (it.color | 0) : '');
        c.n.textContent = n > 1 ? String(n) : '';
    }
    function r25Fill() {
        var pf = ensureProfile(userName()), i;
        elCraft.style.display = R25.craftOn ? '' : 'none';
        if (R25.craftOn) {
            for (i = 0; i < 9; i++) paintCell(craftCells[i], craftGrid[i] && craftGrid[i].it, craftGrid[i] ? craftGrid[i].n : 0);
            var m = null; try { m = CRF.match(gridStacks(), r28CraftCtx()); } catch (e1) { m = null; }
            craftCells.matchRes = m;
            paintCell(craftOut, m && m.output, m ? m.count : 0);
        }
        var w = pf.weapon;
        for (i = 0; i < modCells.length; i++) { var md = w && w.mods && w.mods[modCells[i].sl]; modCells[i].el.classList.toggle('is-eq', !!md); modCells[i].sp.textContent = md ? String(md.name || '').split(' ')[0].slice(0, 7).toUpperCase() : modCells[i].sl.slice(0, 3).toUpperCase(); }
    }
    function r25Tip(sl) {
        var pf = ensureProfile(userName()), c = sl.getAttribute('data-c'), o = sl.getAttribute('data-o'), m = sl.getAttribute('data-m');
        if (c != null) { var cc = craftGrid[+c]; return cc ? describeItem(cc.it).replace(/\nPrice: .*$/, '') + '\nclick to take back' : ''; }
        if (o != null) { var r = craftCells.matchRes; return r ? describeItem(r.output).replace(/\nPrice: .*$/, '') + '\nx' + r.count + ' · click to craft' + (r.recipe ? '' : ' · improvised') : ''; }
        var md = pf.weapon && pf.weapon.mods && pf.weapon.mods[m];
        return md ? String(m).toUpperCase() + ' · ' + md.name + '\n' + JSON.stringify(md.delta).replace(/[{}"]/g, '') + (md.special ? ' · ' + md.special : '') : String(m).toUpperCase() + ' slot · empty\nCraft a mod, right click it';
    }
    elInv.addEventListener('click', function (e) {
        var s = invSlotOf(e.target); if (!s || !R25.craftOn) return;
        var pf = ensureProfile(userName()), i, c;
        if (s.getAttribute('data-i') != null) {
            var rec = pf.items[invTop * INV_COLS + (+s.getAttribute('data-i'))], it = rec ? itemOf(rec) : null;
            if (!it || it.pet || rec.n - r25Reserved(rec.id) <= 0) return;
            for (i = 0; i < 9; i++) if (craftGrid[i] && craftGrid[i].key === rec.id) { craftGrid[i].n++; aPlay('ui', { vel: 0.5 }); invFill(); return; }
            for (i = 0; i < 9; i++) if (!craftGrid[i]) { craftGrid[i] = { key: rec.id, it: it, n: 1 }; aPlay('ui', { vel: 0.5 }); invFill(); return; }
        } else if (s.getAttribute('data-c') != null) {
            c = craftGrid[+s.getAttribute('data-c')]; if (!c) return;
            c.n--; if (c.n <= 0) craftGrid[+s.getAttribute('data-c')] = null;
            aPlay('ui', { vel: 0.4 }); invFill(); invTip(s);
        } else if (s.getAttribute('data-o') != null) {
            var res = craftCells.matchRes; if (!res) return;
            for (i = 0; i < res.consumes.length; i++) { c = craftGrid[res.consumes[i]]; if (!c) continue; takeKey(c.key, 1); c.n--; if (c.n <= 0) craftGrid[res.consumes[i]] = null; }
            if (!curUser) curUser = userName();
            giveItem(res.output, res.count);
            aPlay('craft'); addRow('', '', 'CRAFTED ' + res.output.name + (res.count > 1 ? ' x' + res.count : ''), 'is-sys');
            writeSave(); invFill(); invTip(s);
        }
    });
    // ── menus (kitchen, pens, pods, board) ──
    var menuOpen = false, menuSpec = null, menuRows = [], menuSel = 0;
    var elMenu = document.createElement('div'); elMenu.className = 'sh-imenu'; elMenu.innerHTML = '<div class="im-h"></div><div class="im-l"></div><div class="im-f">1-9 / ENTER pick · ESC close</div>'; hud.appendChild(elMenu);
    var elMenuH = elMenu.querySelector('.im-h'), elMenuL = elMenu.querySelector('.im-l');
    function menuRender() {
        var b = menuSpec.build(); menuRows = b.rows; elMenuH.textContent = b.title; elMenuL.textContent = '';
        menuSel = clamp(menuSel, 0, Math.max(0, menuRows.length - 1));
        if (!menuRows.length) menuRows = [{ name: 'NOTHING HERE', sub: '', fn: null }];
        menuRows.forEach(function (r, i) {
            var d = document.createElement('div'); d.className = 'im-r' + (i === menuSel ? ' is-sel' : ''); d.innerHTML = '<i></i><span></span><em></em>';
            d.children[0].textContent = String(i + 1); d.children[1].textContent = r.name; d.children[2].textContent = r.sub || '';
            d.addEventListener('click', function () { menuSel = i; menuPick(); });
            elMenuL.appendChild(d);
        });
    }
    function openMenu(spec) {
        if (menuOpen || cmdOpen || state !== 'piloting') return;
        menuSpec = spec; menuOpen = true; cmdOpen = true; cmdGuardUntil = performance.now() + 600; menuSel = 0;
        keys = Object.create(null); firing = false; mdx = mdy = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        if (locked()) { try { document.exitPointerLock(); } catch (e0) { /* ignore */ } }
        elMenu.classList.add('is-on'); menuRender(); aPlay('ui', { vel: 0.6 });
    }
    function closeMenu() {
        if (!menuOpen) return;
        menuOpen = false; cmdOpen = false; menuSpec = null; elMenu.classList.remove('is-on');
        keys = Object.create(null); mdx = mdy = 0; aPlay('ui', { vel: 0.4 }); r25Refresh();
        if (state === 'piloting') { try { var p = document.body.requestPointerLock(); if (p && p.catch) p.catch(function () { /* keyboard only */ }); } catch (e0) { /* ignore */ } }
    }
    function r25MenuClose() { closeMenu(); }
    function menuPick() {
        var r = menuRows[menuSel]; if (!r || !r.fn) return;
        var keep = r.fn();
        if (keep && menuOpen) menuRender(); else closeMenu();
    }
    function menuKey(e) {
        var c = e.code; e.preventDefault(); e.stopPropagation();
        if (c === 'Escape') { e.__shipHandled = true; closeMenu(); return; }
        if (c === 'ArrowDown' || c === 'ArrowUp') { menuSel = clamp(menuSel + (c === 'ArrowDown' ? 1 : -1), 0, menuRows.length - 1); menuRender(); return; }
        if (e.repeat) return;
        if (c === 'KeyF' || c === 'KeyE' || c === 'Backspace') { closeMenu(); return; }
        if (c === 'Enter' || c === 'NumpadEnter') { menuPick(); return; }
        var m = /^(?:Digit|Numpad)([1-9])$/.exec(c);
        if (m && +m[1] <= menuRows.length) { menuSel = +m[1] - 1; menuPick(); }
    }
    // ═══ rev 26: quests (js/ship-quests.js) + onboarding ═══════════════════════════════════════════════════════════════════════════════
    // Any NPC (shopper / clerk / dealer via Q in the shop / station NPC) offers 3 seeded quests (F -> talk line + QUESTS panel). Active quests live on
    // profile.quests (max 3); events from the game feed QST.progress(); crates (retrieve) and the freighter (escort) are spawned here.
    var Q_MAX = 3, qOpen = false, qOffers = [], qSel = 0, qWho = '', qGiver = 'npc', qOfferSeed = '', qAcc = {}, qT = 0, qObjs = {}, qTrkC = '', qMsg = '';
    var elQ = document.createElement('div'); elQ.className = 'sh-quest';
    elQ.innerHTML = '<div class="sq-h"><b>QUESTS</b><span class="sq-who"></span></div><div class="sq-list"></div><div class="sq-msg"></div><div class="sq-f">1-3 / ENTER ACCEPT · ARROWS · ESC LEAVE</div>';
    hud.appendChild(elQ);
    var elQWho = elQ.querySelector('.sq-who'), elQList = elQ.querySelector('.sq-list'), elQMsg = elQ.querySelector('.sq-msg');
    var elQT = document.createElement('div'); elQT.className = 'sh-qt'; hud.appendChild(elQT);
    var elOnb = document.createElement('div'); elOnb.className = 'sh-onb'; hud.appendChild(elOnb);
    function qDay() { var d = new Date(); return d.getFullYear() + '-' + (d.getMonth() + 1) + '-' + d.getDate(); }
    function planetLabel(n) { return n ? String(n.name || n.title || n.id || 'Planet') : ''; }
    function curPlanetLabel() { return planetLabel((land && land.node) || (world && world.node)); }
    function questCtx(kind) {
        var ids = [], i, d = engine.drawOrder || [];
        for (i = 0; i < d.length && ids.length < 12; i++) { var n = d[i]; if (n && n.anchor && n.mesh && !n.isSun) { var lb = planetLabel(n); if (ids.indexOf(lb) < 0) ids.push(lb); } }
        var cur = curPlanetLabel() || ids[0] || 'Home';
        if (ids.indexOf(cur) < 0) ids.unshift(cur);
        return { planetIds: ids, currentPlanetId: cur, giverKind: kind || 'npc', wave: Math.max(1, shownWave()), bossNameFn: bossName };
    }
    function activeQuests() { var pf = curProfile(); return pf && Array.isArray(pf.quests) ? pf.quests : []; }
    function qPf() { if (!curUser) curUser = userName(); return ensureProfile(userName()); }
    function openQuests(seedId, who, kind) {
        if (qOpen || state !== 'piloting' || storeOpen || invOpen) return false;
        var pf = qPf(); qOfferSeed = String(seedId) + '|' + qDay(); qGiver = kind || 'npc'; qWho = String(who || 'NPC').toUpperCase();
        try { qOffers = QST.offerSet(qOfferSeed, questCtx(qGiver), 3); } catch (e) { console.info('[ship] quests', e); qOffers = []; }
        qOpen = true; qSel = 0; qMsg = ''; storeOpen = true; cmdOpen = true; cmdGuardUntil = performance.now() + 600;
        keys = Object.create(null); firing = false; mdx = mdy = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        if (locked()) { try { document.exitPointerLock(); } catch (e) { /* ignore */ } }
        hud.classList.add('is-quest'); elQ.classList.add('is-on'); elQWho.textContent = qWho; qRender(); aPlay('ui', { vel: 0.6 });
        return true;
    }
    function qHide() { qOpen = false; hud.classList.remove('is-quest'); elQ.classList.remove('is-on'); }
    function qState(q) { var pf = qPf(); if (pf.qdone && pf.qdone.indexOf(q.id) >= 0) return 'DONE'; if (activeQuests().some(function (a) { return a.id === q.id; })) return 'ACTIVE'; return ''; }
    function qRender() {
        var h = '', i;
        for (i = 0; i < qOffers.length; i++) {
            var q = qOffers[i], st = qState(q), r = q.reward, it = r.items && r.items.length ? ' + ' + r.items.map(function (x) { return x.n + ' ' + (x.item && x.item.name || 'item'); }).join(', ') : '';
            h += '<div class="sq-r' + (i === qSel ? ' is-sel' : '') + (st ? ' is-' + st.toLowerCase() : '') + '" data-i="' + i + '"><i>' + (i + 1) + '</i><div><b>' + qEsc(q.title) + (st ? ' · ' + st : '') + '</b><span>' + qEsc(q.giverLine) + '</span><em>+' + r.gor + ' gorCoin' + qEsc(it) + ' · ' + r.words + ' word' + (r.words > 1 ? 's' : '') + ' · ' + q.expiresMin + ' min</em></div></div>';
        }
        elQList.innerHTML = h || '<div class="sq-r"><div><b>NOTHING TODAY</b></div></div>';
        elQMsg.textContent = qMsg;
    }
    function qEsc(t) { return String(t).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
    elQList.addEventListener('click', function (e) { var t = e.target; while (t && t !== elQList && !(t.getAttribute && t.getAttribute('data-i'))) t = t.parentNode; if (t && t !== elQList) { qSel = +t.getAttribute('data-i'); qAccept(qSel); } });
    function qAccept(i) {
        var q = qOffers[i], pf = qPf(); if (!q) return;
        if (qState(q)) { qMsg = qState(q) === 'DONE' ? 'ALREADY DONE' : 'ALREADY ACTIVE'; qRender(); return; }
        if (activeQuests().length >= Q_MAX) { qMsg = 'QUEST LOG FULL (' + Q_MAX + ')'; aPlay('ui', { vel: 0.3, pitch: 0.6 }); qRender(); return; }
        var a = JSON.parse(JSON.stringify(q)); a.acceptedAt = Date.now(); a.expiresAt = a.acceptedAt + (a.expiresMin || 30) * 60000;
        pf.quests.push(a); aPlay('questAccept'); addRow(qWho, '#ffd36a', 'Deal. ' + a.title + '.'); qMsg = 'ACCEPTED'; writeSave(); qTrkC = ''; qRender();
    }
    function qKey(e) {
        var c = e.code; e.preventDefault(); e.stopPropagation();
        if (c === 'Escape') { e.__shipHandled = true; closeStore(); return; }
        if (c === 'ArrowDown' || c === 'ArrowUp') { qSel = clamp(qSel + (c === 'ArrowDown' ? 1 : -1), 0, Math.max(0, qOffers.length - 1)); qRender(); return; }
        if (e.repeat) return;
        if (c === 'KeyF' || c === 'KeyE' || c === 'Backspace') { closeStore(); return; }
        if (c === 'Enter' || c === 'NumpadEnter') { qAccept(qSel); return; }
        var m = /^(?:Digit|Numpad)([1-3])$/.exec(c);
        if (m && +m[1] <= qOffers.length) { qSel = +m[1] - 1; qAccept(qSel); }
    }
    // ── events ──
    function questEvent(ev) {
        var pf = curProfile(); if (!pf || !pf.quests || !pf.quests.length) return;
        var done = [], i;
        for (i = 0; i < pf.quests.length; i++) {
            var r; try { r = QST.progress(pf.quests[i], ev); } catch (e0) { continue; }
            if (!r || !r.message) continue;
            pf.quests[i] = r.quest; if (!r.completed) addRow('', '', r.message, 'is-sys'); aPlay('ui', { vel: 0.7, pitch: 1.2 });
            if (r.completed) done.push(r.quest);
        }
        qTrkC = '';
        for (i = 0; i < done.length; i++) questComplete(done[i]);
        writeSave();
    }
    function questComplete(q) {
        var pf = qPf(), i, rw = q.reward || {};
        pf.quests = pf.quests.filter(function (x) { return x.id !== q.id; });
        pf.qdone = (pf.qdone || []).concat([q.id]).slice(-80);
        addUnits(rw.gor || 0, shipRoot.position, true);
        (rw.items || []).forEach(function (it) { try { giveItem(it.item, it.n || 1); } catch (e0) { /* ignore */ } });
        if (rw.words > 0) learnWords(rw.words);
        var bpQ = q.kind === 'kill' ? r28BpDrop(q.id, 0.5) : r28BpDrop(q.id, 0.15); if (bpQ) giveItem(bpQ, 1);
        addRow('', '', 'QUEST COMPLETE · ' + q.title + ' · +' + (rw.gor || 0) + ' gorCoin' + ((rw.items || []).length ? ' + ' + rw.items.map(function (x) { return x.n + ' ' + (x.item && x.item.name || 'item'); }).join(', ') : ''), 'is-sys');
        aPlay('questDone'); qKill(q.id); qTrkC = '';
    }
    function questExpire() {
        var pf = curProfile(); if (!pf || !pf.quests) return; var now = Date.now();
        pf.quests = pf.quests.filter(function (q) { if (q.expiresAt && now > q.expiresAt) { addRow('', '', 'QUEST EXPIRED · ' + q.title, 'is-sys'); aPlay('questFail'); qKill(q.id); qTrkC = ''; return false; } return true; });
    }
    var bountyUse = { wave: -1, name: '' };
    function bountyNameFor() {          // a boss spawn takes the name of an open bounty (so the bounty is actually killable)
        var q = activeQuests(), i, j;
        for (i = 0; i < q.length; i++) {
            var s = q[i].steps[0]; if (!s || s.type !== 'kill' || s.done) continue;
            var nm = s.bossName, taken = false;
            for (j = 0; j < enemies.length; j++) if (enemies[j].alive && enemies[j].isBoss && String(enemies[j].title).replace(/^(MINI|TITAN) /, '') === nm) { taken = true; break; }
            if (!taken && !(bountyUse.wave === wave && bountyUse.name === nm)) { bountyUse.wave = wave; bountyUse.name = nm; return nm; }      // one boss per wave carries the bounty name
        }
        return '';
    }
    function bossKilled(title) { questEvent({ type: 'kill', bossName: String(title || '').replace(/^(MINI|TITAN) /, '') }); }
    // ── objects: retrieve crate / escort freighter ──
    var qGlowTex = null;
    function qGlow() {
        if (qGlowTex) return qGlowTex;
        var c = document.createElement('canvas'); c.width = c.height = 64; var x = c.getContext('2d'), g = x.createRadialGradient(32, 32, 0, 32, 32, 32);
        g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.3, 'rgba(255,255,255,0.45)'); g.addColorStop(1, 'rgba(255,255,255,0)'); x.fillStyle = g; x.fillRect(0, 0, 64, 64);
        qGlowTex = new THREE.CanvasTexture(c); return qGlowTex;
    }
    function qMakeObj(color) {
        var g = new THREE.Group(), box = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(1, 0.7, 1)), new THREE.LineBasicMaterial({ color: color })), core = new THREE.Mesh(new THREE.OctahedronGeometry(0.28), new THREE.MeshBasicMaterial({ color: color }));
        var halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: qGlow(), color: color, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
        var beacon = new THREE.Sprite(new THREE.SpriteMaterial({ map: qGlow(), color: color, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false, transparent: true, sizeAttenuation: false }));
        halo.scale.setScalar(9); beacon.scale.setScalar(0.05); beacon.renderOrder = 10;
        g.add(box); g.add(core); g.add(halo); g.add(beacon); g.userData.core = core; g.userData.box = box; g.frustumCulled = false;
        g.traverse(function (o) { o.frustumCulled = false; }); scene.add(g); return g;
    }
    function qKill(id) {
        var o = qObjs[id]; if (!o) return;
        scene.remove(o.g); o.g.traverse(function (m) { if (m.geometry) m.geometry.dispose(); if (m.material) m.material.dispose(); });
        delete qObjs[id];
    }
    function qCarry(dx, dy, dz) { for (var k in qObjs) { var o = qObjs[k]; o.g.position.x += dx; o.g.position.y += dy; o.g.position.z += dz; if (o.wp) for (var i = 0; i < o.wp.length; i++) { o.wp[i].x += dx; o.wp[i].y += dy; o.wp[i].z += dz; } } }
    function qEnsure(q) {
        var sp = QST.spawnSpecFor(q); if (!sp || qObjs[q.id]) return;
        if (sp.type === 'crate') {
            if (sp.where === 'belt') {
                var b = space && space.belts && space.belts[0], rk = space && space.rocks; if (!b || !rk || !rk.center || gmode !== 'fly') return;
                var o = { g: qMakeObj(0x7CFFB0), kind: 'crate', belt: true }, r = (b.mid || (b.inner + b.outer) / 2) * sp.pos[1];
                o.g.position.set(rk.center.x + Math.cos(sp.pos[0]) * r, rk.center.y + sp.pos[2] * (b.yMax || 0) * 2, rk.center.z + Math.sin(sp.pos[0]) * r);
                o.g.scale.setScalar(2.6 * L); qObjs[q.id] = o;
            } else if (gmode === 'foot' && ps && ps.active && land.node === ps.active && curPlanetLabel() === sp.planetId && hum.obj) {
                var dir = new THREE.Vector3().copy(hum.pos).normalize(), tg = new THREE.Vector3().crossVectors(dir, Y).normalize(), tg2 = new THREE.Vector3().crossVectors(dir, tg);
                var a = sp.seed % 628 / 100, dd = (28 + sp.seed % 17) * L / (ps.radius || 1);
                dir.addScaledVector(tg, Math.cos(a) * dd).addScaledVector(tg2, Math.sin(a) * dd).normalize();
                var fr = mFloor(dir.x, dir.y, dir.z);
                qObjs[q.id] = { g: qMakeObj(0x7CFFB0), kind: 'crate', local: dir.multiplyScalar(fr + 0.5 * L) };
                qObjs[q.id].g.scale.setScalar(1.6 * L);
            }
        } else if (sp.type === 'freighter') {
            if (gmode !== 'fly' || !hullMod) return;
            var fr2 = hullMod.buildHull(THREE, { kind: 'hauler' }); fr2.traverse(function (m) { m.frustumCulled = false; }); fr2.scale.setScalar(26 * L);
            var g2 = new THREE.Group(); g2.add(fr2); var hb = new THREE.Sprite(new THREE.SpriteMaterial({ map: qGlow(), color: 0xFFD36A, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false, transparent: true, sizeAttenuation: false })); hb.scale.setScalar(0.05); g2.add(hb); scene.add(g2);
            vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); g2.position.copy(shipRoot.position).addScaledVector(vF, 160 * L);
            var wp = sp.route.map(function (p) { return new THREE.Vector3(g2.position.x + p[0] * 0.25 * L, g2.position.y + p[1] * 0.25 * L, g2.position.z + p[2] * 0.25 * L); });
            qObjs[q.id] = { g: g2, kind: 'freighter', wp: wp, wi: 0, speed: Math.min(30, sp.speed * 0.4) * L, hull: fr2 };
        }
    }
    function qObjTick(dt) {
        var pf = curProfile(); if (!pf) return;
        var act = pf.quests || [], i, ids = {};
        for (i = 0; i < act.length; i++) {
            var q = act[i]; ids[q.id] = 1; if (q.state === 'complete') continue;
            qEnsure(q);
            var o = qObjs[q.id]; if (!o) continue;
            if (o.kind === 'crate') {
                o.g.userData.core.rotation.y += dt * 2; o.g.userData.box.rotation.y -= dt * 1.2;
                if (o.local) { locToWorld(o.local, o.g.position); }
                var d = o.g.position.distanceTo(o.local ? (gmode === 'foot' ? hum.w : shipRoot.position) : shipRoot.position) / L;
                if (o.local ? (gmode === 'foot' && d < 3) : (gmode === 'fly' && d < 7)) {
                    fx.flash(o.g.position, 0x7CFFB0); fx.impact(o.g.position, 0x7CFFB0, 2); aPlay('ui', { vel: 1, pitch: 1.5 });
                    questEvent({ type: 'retrieve', questId: q.id }); qKill(q.id);
                }
            } else if (o.kind === 'freighter') {
                var wp = o.wp[o.wi % o.wp.length]; vA.subVectors(wp, o.g.position); var dl = vA.length();
                if (dl < 20 * L) o.wi++; else { o.g.position.addScaledVector(vA, Math.min(dl, o.speed * dt) / dl); vA.normalize(); vTmp.copy(NEG_Z); qA.setFromUnitVectors(vTmp, vA); o.g.quaternion.slerp(qA, Math.min(1, dt * 1.5)); }
                if (gmode === 'fly' && !dead && o.g.position.distanceTo(shipRoot.position) < 60 * L) { o.acc = (o.acc || 0) + dt; if (o.acc >= 1) { questEvent({ type: 'escort', seconds: o.acc }); o.acc = 0; } }
            }
        }
        for (var k in qObjs) if (!ids[k]) qKill(k);
    }
    function trackerText() {
        var a = activeQuests(), t = [], i;
        for (i = 0; i < a.length; i++) {
            var d = QST.describe(a[i]).split('\n'), o = qObjs[a[i].id], ds = '';
            if (o) { var dd = o.g.position.distanceTo(shipRoot.position) / L; ds = ' · ' + (dd >= 1000 ? (dd / 1000).toFixed(1) + 'k' : Math.round(dd)) + ' L'; }
            t.push(d[0] + '\n' + (d[1] || '') + ds);
        }
        return t.join('\n\n');
    }
    function questTick(dt) {
        qT -= dt; onbTick(dt);
        if (qT > 0) return; qT = 0.5;
        var pf = curProfile(); if (!pf) { if (qTrkC) { qTrkC = ''; elQT.textContent = ''; elQT.classList.remove('is-on'); } return; }
        questExpire(); qObjTick(0.5);
        var t = state === 'piloting' ? trackerText() : '';
        if (t !== qTrkC) { qTrkC = t; elQT.textContent = ''; var parts = t ? t.split('\n\n') : []; for (var i = 0; i < parts.length; i++) { var l = parts[i].split('\n'), d = document.createElement('div'); d.innerHTML = '<b>' + qEsc(l[0]) + '</b><span>' + qEsc(l[1] || '') + '</span>'; elQT.appendChild(d); } elQT.classList.toggle('is-on', !!t); }
    }
    function questScan() {          // V on the target planet: every highlighted resource / creature in range counts
        var a = activeQuests().filter(function (q) { var s = q.steps.filter(function (x) { return !x.done; })[0]; return s && s.type === 'scan'; });
        if (!a.length || !world || !world.node || gmode === 'fly') return;
        var pl = curPlanetLabel(), n = 0, i, org = (gmode === 'foot' && hum.obj) ? hum.w : shipRoot.position;
        try {
            var tg = world.scanTargets();
            for (i = 0; i < tg.length && n < 16; i++) { if (tg[i].type !== 'resource') continue; locToWorld(tg[i].pos, vHl2); if (vHl2.distanceToSquared(org) > (120 * L) * (120 * L)) continue; n++; questEvent({ type: 'scan', planetId: pl, target: 'resource' }); if (tg[i].kind === 'plant' || tg[i].kind === 'spore') questEvent({ type: 'scan', planetId: pl, target: 'plant' }); }
        } catch (e0) { /* ignore */ }
        try {
            if (ps && ps.active && land.node === ps.active) {
                var cl = creatList(), cn = 0, cc;
                for (i = 0; i < cl.length && cn < 12; i++) { if (cl[i].lp.distanceToSquared(gmode === 'foot' ? hum.pos : land.sPos) < (120 * L) * (120 * L)) { cn++; questEvent({ type: 'scan', planetId: pl, target: 'creature' }); } }
            }
        } catch (e1) { /* ignore */ }
    }
    function questDeliver(st) {         // F at a 7/11 counter on the target planet: hand over the stacks
        var pf = curProfile(); if (!pf || !pf.quests) return;
        var pl = curPlanetLabel(), moved = false;
        pf.quests.slice().forEach(function (q) {
            var s = q.steps.filter(function (x) { return !x.done; })[0]; if (!s || s.type !== 'deliver' || s.planetId !== pl) return;
            for (var i = 0; i < pf.items.length; i++) {
                var rec = pf.items[i]; if (rec.id !== s.item.stackKey) continue;
                var n = Math.min(rec.n, s.item.n - s.have); if (n <= 0) break;
                rec.n -= n; if (rec.n <= 0) pf.items.splice(i, 1);
                moved = true; questEvent({ type: 'deliver', planetId: pl, stackKey: s.item.stackKey, n: n }); break;
            }
        });
        return moved;
    }
    // ── onboarding: four prompts for a NEW profile (fly / pulse / land / shop), each dismissed when done, never shown again; plus two first-time hints ──
    var onb = { t: 0, fly: 0, shown: '', hintT: 0, hint: '' }, ONB_TXT = ['FLY · HOLD W + MOVE THE MOUSE', 'PULSE · HOLD SPACE NEAR A PLANET', 'LAND · FLY UNDER 3 L OF THE SURFACE, PRESS F', 'SHOP · WALK TO A 7/11 COUNTER, PRESS F'];
    function onbSet(t) { if (t !== onb.shown) { onb.shown = t; elOnb.textContent = t; elOnb.classList.toggle('is-on', !!t); } }
    function onbTick(dt) {
        var pf = curProfile(); if (!pf || !pf.onb || state !== 'piloting') { onbSet(''); return; }
        var o = pf.onb, txt = '';
        if (!o.done && !storeOpen && !invOpen) {
            var s = o.step | 0; onb.t += dt;
            var ok = false;
            if (s === 0) { if (keys.KeyW || speed > 1.2 * CRUISE) onb.fly += dt; ok = onb.fly > 1.5; }
            else if (s === 1) ok = pulse > 0.5 || pulseT > 0.2;
            else if (s === 2) ok = gmode === 'landing' || gmode === 'landed' || gmode === 'foot';
            else if (s === 3) ok = !!(shopCtx && shopCtx.mode === 'store' && storeOpen) || (storeOpen && !qOpen);
            if (ok || onb.t > 60) { o.step = s + 1; onb.t = 0; if (o.step >= 4) o.done = true; writeSave(); aPlay('ui', { vel: 0.5, pitch: 1.3 }); }
            else txt = ONB_TXT[s];
        }
        if (!txt && !storeOpen && !invOpen) {      // first-time hints (one line, 6 s)
            if (onb.hintT > 0) { onb.hintT -= dt; txt = onb.hint; }
            else if (!o.res && gmode === 'foot' && resFind()) { o.res = true; onb.hint = 'RESOURCE NODE · F TO HARVEST · SELL IT OR CRAFT AT A 7/11'; onb.hintT = 6; writeSave(); }
            else if (!o.npc && gmode === 'foot' && world && (world.nearNpc || npcNear())) { o.npc = true; onb.hint = 'F TO TALK · THEY MAY HAVE WORK (QUESTS)'; onb.hintT = 6; writeSave(); }
        }
        onbSet(txt);
    }
    // ── pets ──
    function petItem(p) { return { id: 'pet:' + p.seed, name: p.name, kind: 'pet', base: p.name, infusion: null, modifier: null, color: p.color, price: 150 + 20 * (p.happy | 0), tier: 1, category: 'pet', tags: ['pet'], stackKey: 'pet:' + p.seed, pet: p, iconHint: 'candy', blurb: 'Follows you home for fries. Lives in a pen aboard your ship.', dealer: false }; }
    function treatIdx(pf) {
        for (var i = 0; i < pf.items.length; i++) { var it = itemOf(pf.items[i]); if (it && (it.kind === 'fries' || (it.effect && it.effect.extras && it.effect.extras.treat))) return i; }
        return -1;
    }
    // rev 27: taming rides ps.creatures() / ps.setCreatureTamed (the instanced-colour hack is gone). Tamed ids live on profile.taken [{n: planet, id, k}].
    var CR_NAMES = ['Walker', 'Jelly', 'Ox', 'Strider'], CR_PLAN = ['walker', 'jelly', 'walker', 'strider'];
    function crKind(c) { var k = c && c.kind; if (typeof k === 'number') return ((k % 4) + 4) % 4; var i = CR_NAMES.map(function (s) { return s.toLowerCase(); }).indexOf(String(k).toLowerCase()); return i < 0 ? 0 : i; }
    var tame = { on: false, t: 0, id: '', T: 3 }, tcRes = { id: '', d: 0, kind: 0, v: 0, pos: new THREE.Vector3() }, crCache = { t: -1e9, list: [] };
    function creatList() {
        var now = performance.now();
        if (now - crCache.t > 250) {
            crCache.t = now;
            try {
                crCache.list = (ps && typeof ps.creatures === 'function') ? ps.creatures() : [];
                if (ps && ps.active && crCache.list.length) {          // the planet moves fast: keep planet-LOCAL positions (compare with hum.pos), world ones go stale within a frame
                    var nd = ps.active; qW.copy(nd.mesh.quaternion).invert();
                    for (var ci = 0; ci < crCache.list.length; ci++) { var cc = crCache.list[ci]; cc.lp = (cc.lp || new THREE.Vector3()).copy(cc.pos).sub(nd.anchor.position).applyQuaternion(qW); }
                }
            } catch (e0) { crCache.list = []; }
        }
        return crCache.list;
    }
    function tameFind() {
        if (gmode !== 'foot' || !hum.obj || !ps || !ps.active || land.node !== ps.active) return null;
        var pf = curProfile(); if (!pf || treatIdx(pf) < 0) return null;
        var list = creatList(), best = null, bd = (12 * L) * (12 * L), i, d2;      // grazers bolt inside 10 L, so the treat works from just outside it
        for (i = 0; i < list.length; i++) { d2 = list[i].lp.distanceToSquared(hum.pos); if (d2 < bd) { bd = d2; best = list[i]; } }
        if (!best) return null;
        tcRes.id = best.id; tcRes.d = Math.sqrt(bd); tcRes.kind = crKind(best); tcRes.v = tcRes.kind === 1 ? 1 : 0; tcRes.pos.copy(best.lp);
        return tcRes;
    }
    function tameStart(c) { if (tame.on || !c) return; tame.on = true; tame.t = 0; tame.id = c.id; aPlay('ui', { vel: 0.6 }); addRow('', '', 'taming... hold still', 'is-sys'); }
    // ── harvesting: F near a resource node = 3 s channel, chunk bursts, then the stack goes to the inventory; spent nodes persist on the profile ──
    var harv = { on: false, t: 0, id: '', pos: new THREE.Vector3(), bt: 0 };
    function resFind() {
        if (gmode !== 'foot' || !hum.obj || !world || !world.node || typeof world.nearResource !== 'function' || !world.resources || !world.resources.length) return null;
        try { return world.nearResource(hum.pos, Math.max(1.6 * L, 0.5 * L + 4 * 0.09 * L)) || null; } catch (e0) { return null; }
    }
    function harvStart(r) { if (harv.on || !r) return; harv.on = true; harv.t = 0; harv.id = r.id; harv.bt = 0; aPlay('ui', { vel: 0.6 }); addRow('', '', 'harvesting ' + r.kind + '...', 'is-sys'); }
    function harvTick(dt) {
        if (!harv.on) return;
        var r = resFind();
        if (!r || r.id !== harv.id) { harv.on = false; return; }
        harv.t += dt; harv.bt -= dt;
        locToWorld(r.pos, harv.pos);
        if (harv.bt <= 0) { harv.bt = 0.25; fx.impact(harv.pos, r.kind === 'plant' || r.kind === 'spore' ? 0x7CFF8A : (r.kind === 'ice' ? 0xBEEFFF : (r.kind === 'ore' ? 0xC8A878 : 0xFF5CE1)), 2); aPlay('hit', { pitch: 0.5, vel: 0.3 }); }
        if (harv.t < 3) return;
        harv.on = false;
        var got = null; try { got = world.harvest(r.id); } catch (e1) { got = null; }
        if (!got || !got.item) { addRow('', '', 'nothing left', 'is-sys'); return; }
        if (!curUser) curUser = userName();
        var pf = ensureProfile(userName()); giveItem(got.item, got.n || 1);
        try { var st = world.resourceState(); for (var k in st) pf.harv[k] = st[k]; } catch (e2) { /* ignore */ }
        for (var b = 0; b < 3; b++) fx.impact(harv.pos, got.item.color || 0xffffff, 3);
        aPlay('harvest'); addRow('', '', 'HARVESTED ' + (got.n || 1) + ' x ' + got.item.name, 'is-sys'); writeSave();
        questEvent({ type: 'harvest', stackKey: got.item.stackKey || got.item.id, kind: got.item.base, n: got.n || 1 });
    }
    function tameTick(dt) {
        if (!tame.on) return;
        var c = null, cl0 = gmode === 'foot' && hum.obj ? creatList() : [], ci;
        for (ci = 0; ci < cl0.length; ci++) if (cl0[ci].id === tame.id && cl0[ci].lp.distanceToSquared(hum.pos) < (25 * L) * (25 * L)) { c = tcRes; tcRes.id = cl0[ci].id; tcRes.kind = crKind(cl0[ci]); tcRes.v = tcRes.kind === 1 ? 1 : 0; break; }
        if (!c) { tame.on = false; addRow('', '', 'it wandered off', 'is-sys'); return; }
        tame.t += dt;
        if (tame.t < tame.T) return;
        tame.on = false;
        var pf = ensureProfile(userName()), ti = treatIdx(pf); if (ti < 0) return;
        var rec = pf.items[ti]; rec.n--; if (rec.n <= 0) pf.items.splice(ti, 1);
        var nm = land.node && (land.node.name || land.node.title || land.node.id) || 'Somewhere', pn = String(nm).replace(/^\w/, function (q) { return q.toUpperCase(); }), pid = String(land.node.id);
        var seed = hash25(pid + ':' + c.id), hue = (seed % 360) / 360;
        var pet = { seed: seed, cid: c.id, plan: CR_PLAN[c.kind], name: CR_NAMES[c.kind] + ' of ' + pn, color: new THREE.Color().setHSL(hue, 0.55, 0.55).getHex(), v: c.v, happy: 1 };
        giveItem(petItem(pet), 1);
        try { ps.setCreatureTamed(c.id, true); } catch (e0) { /* ignore */ }
        crCache.t = -1e9; pf.taken.push({ n: pid, id: c.id, k: c.kind }); pf.taken = pf.taken.slice(-40);
        aPlay('tame'); addRow('', '', 'TAMED ' + pet.name + ' · right-click it in your inventory to take it along', 'is-sys'); writeSave();
        discClaim('creature', pid + ':' + CR_NAMES[c.kind], CR_NAMES[c.kind] + ' ' + pn);
    }
    function r25HideTaken() {          // re-hide this profile's tamed creatures once per planet / profile (ps remembers them per planet after that)
        var pf = curProfile(); if (!pf || !pf.taken || !ps || !ps.active || typeof ps.setCreatureTamed !== 'function') return;
        var key = String(ps.active.id) + '|' + userName() + '|' + pf.taken.length;
        if (R27.tamedKey === key) return;
        R27.tamedKey = key;
        for (var ti = 0; ti < pf.taken.length; ti++) { var t = pf.taken[ti]; if (t.id && t.n === String(ps.active.id)) { try { ps.setCreatureTamed(t.id, true, t.n); } catch (e0) { /* ignore */ } } }
    }
    // ── crew + missions ──
    function r25Cand() { var pf = curProfile(); return (R25.cand && pf && pf.crew.length < 4) ? R25.cand : null; }
    function r25BurgerVisit(bh) {
        var pf = ensureProfile(userName()); pf.visits = (pf.visits | 0) + 1; R25.cand = null;
        try { if (bh && typeof bh.recruit === 'function' && pf.crew.length < 4) R25.cand = bh.recruit(pf.visits); } catch (e0) { R25.cand = null; }
        writeSave();
    }
    function r25Hire() {
        var pf = ensureProfile(userName()), c = r25Cand(); if (!c) return;
        var m = { id: 'c' + c.seed, seed: c.seed, name: c.name, role: c.role, color: c.color, rank: 1, lines: (c.lines || []).map(String).slice(0, 3), busy: '' };
        pf.crew.push(m); R25.cand = null;
        try { if (world && world.burgerHouse) world.burgerHouse.candidate = null; } catch (e0) { /* ignore */ }
        aPlay('hire'); addRow(String(m.name).toUpperCase(), cssHex(m.color), m.lines[0] || 'Reporting for duty.');
        addRow('', '', m.name + ' joined your crew (' + m.role + ') · ' + pf.crew.length + '/4', 'is-sys'); writeSave();
    }
    var PART_MAP = { 'plasma-coil': 'coil', 'flux-capacitor': 'capacitor', 'hull-plate': 'plate', 'logic-chip': 'chip', 'fuel-cell': 'battery' };
    function genMission(seed) {
        var m = null, r = mulberry32(seed);
        try { if (station && station.interior && station.interior.missions) m = station.interior.missions(seed, 1)[0]; } catch (e0) { m = null; }
        if (!m || !(m.title || m.name)) {
            var kinds = [['mine', 'Mine ore on '], ['scout', 'Scout the belt of '], ['trade', 'Trade run to '], ['bounty', 'Bounty hunt over ']], k = kinds[(r() * 4) | 0], pl = ['Papers', 'Ember', 'the Rim', 'Vesper', 'Cinder'][(r() * 5) | 0], mins = 3 + ((r() * 18) | 0);
            m = { kind: k[0], title: k[1] + pl, minutes: mins, reward: { gor: Math.round((40 + mins * (8 + r() * 10)) / 5) * 5, stacks: [{ item: 'hull-plate', n: 1 + ((r() * 3) | 0) }], itemChance: 0.25 } };
        }
        var rw = m.reward || {};
        return { id: 'm' + seed.toString(36), name: String(m.title || m.name), kind: m.kind || 'mine', minutes: clamp(m.minutes | 0 || 5, 1, 30), gor: Math.max(10, rw.gor | 0 || 60), stacks: (rw.stacks || []).map(function (s) { return { k: PART_MAP[s.item] || s.item, n: Math.max(1, s.n | 0) }; }), chance: rw.itemChance || 0.2, crewId: '', start: 0, end: 0 };
    }
    function mulberry32(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; var t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
    function r25Board() {
        var pf = ensureProfile(userName()), b = pf.board;
        if (!b.seed) b.seed = (hash25(userName()) ^ (pf.created | 0)) >>> 0 || 7;
        while (b.list.length < 3) { b.seed = (Math.imul(b.seed, 1664525) + 1013904223) >>> 0; b.list.push(genMission(b.seed)); }
        return b;
    }
    function missionDone(pf, m, i) {
        var c = pf.crew.filter(function (x) { return x.id === m.crewId; })[0], got = [], r = Math.random();
        pf.gor = (pf.gor | 0) + m.gor;
        m.stacks.forEach(function (s) {
            var base = null, pi; for (pi = 0; pi < CRF.PARTS.length; pi++) if (CRF.PARTS[pi].id === s.k) base = CRF.PARTS[pi];
            if (!base) base = R25_SCRAP[0]; giveItem(base, s.n); got.push(s.n + ' ' + base.name);
        });
        if (r < m.chance) { try { var gi = ITM.generateItem((Math.random() * 1e9) >>> 0); giveItem(gi, 1); got.push(gi.name); } catch (e0) { /* ignore */ } }
        if (c) { c.rank = (c.rank | 0) + 1; c.busy = ''; }
        aPlay('units', { n: 6 }); if (c) aPlay('levelUp');
        addRow(c ? String(c.name).toUpperCase() : 'CREW', c ? cssHex(c.color) : '#ffd36a', 'back from "' + m.name + '": +' + CRF.fmt(m.gor) + (got.length ? ', ' + got.join(', ') : '') + (c ? ' · rank ' + c.rank : ''));
        pf.board.seed = (Math.imul(pf.board.seed || 1, 1664525) + 1013904223) >>> 0; pf.board.list[i] = genMission(pf.board.seed);
    }
    function missionTick() {
        var pf = curProfile(); if (!pf || !pf.board) return;
        var now = Date.now(), i, ch = false;
        for (i = 0; i < pf.board.list.length; i++) { var m = pf.board.list[i]; if (m && m.end > 0 && m.end <= now) { missionDone(pf, m, i); ch = true; } }
        if (ch) { writeSave(); r25Refresh(); }
    }
    function boardSync() {
        var int = R25.int, pf = curProfile(); if (!int || !pf || !int.board || !int.board.setMissions) return;
        var b = r25Board(), now = Date.now();
        try { int.board.setMissions(b.list.map(function (m) { var c = pf.crew.filter(function (x) { return x.id === m.crewId; })[0]; return { title: m.name, status: m.end > 0 ? (c ? c.name : 'CREW') + ' ' + fmtT((m.end - now) / 1000) : 'OPEN ' + m.minutes + ' MIN · ' + CRF.fmt(m.gor) }; })); } catch (e0) { /* ignore */ }
    }
    function r25Tick(dt) {
        questTick(dt); hullTick(dt); r27Tick(dt);
        if (tame.on) tameTick(dt);
        if (harv.on) harvTick(dt);
        R25.t += dt; if (R25.t < 1) return; R25.t = 0;
        missionTick(); if (ix.on) boardSync();
    }
    // ── interior ──
    function ensureInt() {
        if (!R25.mod || !R25.mod.createInterior) return null;
        if (R25.int && Math.abs(R25.L - L) < 1e-9) return R25.int;
        try { if (R25.int) R25.int.dispose(); R25.int = R25.mod.createInterior(engine, L, { scale: 0.35 }); R25.L = L; R25.int.setVisible(false); } catch (e0) { console.info('[ship] interior failed', e0); R25.int = null; }
        return R25.int;
    }
    function r25Refresh() {
        var int = R25.int, pf = curProfile(); if (!ix.on || !int || !pf) return;
        var i, n;
        try {
            int.cargo.setStacks(pf.items.slice(0, 24).map(function (r) { var it = itemOf(r); return { id: r.id, name: it ? it.name : r.id, count: r.n, color: it ? it.color : 0x888888 }; }));
            for (i = 0; i < 4; i++) {
                int.pens[i].clear(); var p = pf.petPens[i]; if (p) int.pens[i].setPet({ seed: p.seed, name: p.name, tier: 1 });
                int.pods[i].clear(); var c = pf.crew[i]; if (c) int.pods[i].setCrew({ seed: c.seed, name: c.name, color: c.color });
            }
        } catch (e0) { console.info('[ship] interior sync', e0); }
        r27Interior(int, pf);
        boardSync();
    }
    function enterInterior() {
        if (ix.on) return true;
        if (state !== 'piloting' || boarding || exiting || dead) return false;
        if (gmode === 'foot') { if (!nearShip()) return false; } else if (gmode === 'sfoot') { if (sk.mode !== 'deck') return false; } else return false;
        var int = ensureInt(); if (!int) return false;
        jetKill(); eh.on = false; eh.t = 0; ehShow(0, '');
        if (!hum.obj) { hum.obj = makeHuman(effColor()); scene.add(hum.obj.group); }
        ix.from = gmode; ix.w.copy(hum.w); ix.q.copy(hum.obj.group.quaternion); ix.sc = hum.obj.group.scale.x;
        ix.on = true; gmode = 'ifoot'; shipRoot.visible = false; hum.obj.group.visible = true;
        int.setVisible(true); int.group.updateMatrixWorld(true);
        var sp = int.spawn, lp = sp.localPos ? sp.localPos : int.toLocal(sp.pos, new THREE.Vector3());
        ik.hp.copy(lp); ik.hp.y = 0; ik.yaw = Math.atan2(-sp.dir.x, -sp.dir.z); ik.pitch = 0.3; ik.vv = 0; ik.air = false; ik.jumpHeld = true;
        camRelInit = true; ix.snap = 3; mdx = mdy = 0; setGround(true, true); setPrompt('', false);
        net && net.setMode && net.setMode('landed');
        r25Refresh(); aPlay('land', { vel: 0.4 }); addRow('', '', 'ABOARD · F at the hatch to step out', 'is-sys');
        return true;
    }
    function leaveInterior(fadeSound) {
        if (!ix.on) return;
        ix.on = false; gmode = ix.from; closeMenu(); closeInv();
        if (R25.int) R25.int.setVisible(false);
        shipRoot.visible = true;
        if (hum.obj) { hum.w.copy(ix.w); hum.obj.group.position.copy(ix.w); hum.obj.group.quaternion.copy(ix.q); hum.obj.group.scale.setScalar(ix.sc); hum.obj.group.visible = gmode === 'foot' || gmode === 'sfoot'; }
        camRelInit = false; mdx = mdy = 0; setPrompt('', false);
        camera.near = 0.01 * L; camera.updateProjectionMatrix();
        net && net.setMode && net.setMode('foot');
        if (fadeSound !== false) aPlay('ui', { vel: 0.5 });
    }
    var ixI = { kind: '', obj: null, label: '', i: 0 };
    function ixTarget() {
        var int = R25.int, pf = curProfile(), best = null, bd = 1e30, i;
        ixI.kind = ''; ixI.obj = null; ixI.label = '';
        if (!int || !hum.obj || menuOpen || invOpen) return ixI;
        function test(o, kind, label, idx) {
            if (!o || !o.pos) return;
            var d = hum.w.distanceTo(o.pos), lim = Math.max(o.radius || 0, 0.18 * L) + 0.1 * L;
            if (d < lim && d / lim < bd) { bd = d / lim; best = { kind: kind, obj: o, label: label, i: idx || 0 }; }
        }
        test(int.hatch, 'hatch', 'F · STEP OUT');
        test(int.table, 'table', 'F · CRAFTING TABLE');
        test(int.kitchen, 'kitchen', 'F · KITCHEN');
        test(int.board, 'board', 'F · MISSIONS');
        for (i = 0; i < 4; i++) { test(int.pens[i], 'pen', 'F · PEN ' + (i + 1) + (pf && pf.petPens[i] ? ' · ' + pf.petPens[i].name : ''), i); test(int.pods[i], 'pod', 'F · POD ' + (i + 1) + (pf && pf.crew[i] ? ' · ' + pf.crew[i].name : ''), i); }
        if (best) { ixI.kind = best.kind; ixI.obj = best.obj; ixI.label = best.label; ixI.i = best.i; }
        return ixI;
    }
    function interiorF() {
        var t = ixTarget(), pf = ensureProfile(userName());
        if (t.kind === 'hatch') leaveInterior(true);
        else if (t.kind === 'table') { R25.force = true; openInv(); R25.force = false; }
        else if (t.kind === 'kitchen') openMenu({ build: function () {
            var rows = [], p2 = ensureProfile(userName());
            p2.items.forEach(function (r, idx) { var it = itemOf(r); if (it && (it.kind === 'food' || it.kind === 'drink' || it.kind === 'snack' || it.kind === 'fries' || it.category === 'food')) rows.push({ name: it.name, sub: 'x' + r.n, fn: function () { eatSlot(idx); return true; } }); });
            return { title: 'KITCHEN · EAT', rows: rows };
        } });
        else if (t.kind === 'board') openMenu({ build: function () {
            var p2 = ensureProfile(userName()), b = r25Board(), now = Date.now();
            return { title: 'MISSION BOARD', rows: b.list.map(function (m, mi) {
                var c = p2.crew.filter(function (x) { return x.id === m.crewId; })[0];
                return { name: m.name, sub: m.end > 0 ? (c ? c.name : 'CREW') + ' · ' + fmtT((m.end - now) / 1000) : m.minutes + ' MIN · ' + CRF.fmt(m.gor), fn: function () {
                    if (m.end > 0) return true;
                    var free = p2.crew.filter(function (x) { return !x.busy; });
                    if (!free.length) { addRow('', '', p2.crew.length ? 'all crew are out on missions' : 'no crew · recruit at the Burger House', 'is-sys'); return false; }
                    closeMenu(); setTimeout(function () { openMenu({ build: function () { return { title: 'ASSIGN CREW · ' + m.name, rows: free.map(function (cw) { return { name: cw.name.toUpperCase(), sub: cw.role + ' · rank ' + cw.rank, fn: function () { m.crewId = cw.id; m.start = Date.now(); m.end = m.start + m.minutes * 60000; cw.busy = m.id; addRow(String(cw.name).toUpperCase(), cssHex(cw.color), 'on it. ' + m.minutes + ' min.'); writeSave(); boardSync(); return false; } }; }) }; } }); }, 0);
                    return true;
                } };
            }) };
        } });
        else if (t.kind === 'pen') {
            var pi = t.i;
            if (pi >= Math.min(4, 1 + (pf.pens | 0))) { addRow('', '', 'pen locked · craft a Creature Pen Kit', 'is-sys'); return; }
            openMenu({ build: function () {
                var p2 = ensureProfile(userName()), pet = p2.petPens[pi], rows = [];
                if (pet) {
                    rows.push({ name: 'FEED ' + pet.name.toUpperCase(), sub: 'happiness ' + (pet.happy | 0) + ' · needs fries / treat', fn: function () { var ti = treatIdx(p2); if (ti < 0) { addRow('', '', 'nothing to feed it', 'is-sys'); return true; } var rr = p2.items[ti]; rr.n--; if (rr.n <= 0) p2.items.splice(ti, 1); pet.happy = (pet.happy | 0) + 1; pet.mood = clamp((pet.mood | 0) + 1, 0, 5); aPlay('petHappy'); addRow('', '', pet.name + ' is happier (' + pet.happy + ') · mood ' + pet.mood, 'is-sys'); writeSave(); return true; } });
                    rows.push({ name: 'TAKE BACK', sub: 'to your inventory', fn: function () { giveItem(petItem(pet), 1); p2.petPens[pi] = null; writeSave(); r25Refresh(); return false; } });
                } else p2.items.forEach(function (r) { var it = itemOf(r); if (it && it.pet) rows.push({ name: it.name, sub: 'x' + r.n + ' · put in pen', fn: function () { takeKey(r.id, 1); p2.petPens[pi] = JSON.parse(JSON.stringify(it.pet)); addRow('', '', it.name + ' settles in', 'is-sys'); writeSave(); r25Refresh(); return false; } }); });
                return { title: 'PEN ' + (pi + 1), rows: rows };
            } });
        } else if (t.kind === 'pod') {
            var c = pf.crew[t.i];
            if (!c) { addRow('', '', 'empty bunk · recruit crew at a Burger House', 'is-sys'); return; }
            c.li = ((c.li | 0) + 1) % Math.max(1, c.lines.length);
            addRow(String(c.name).toUpperCase(), cssHex(c.color), (c.lines[c.li] || '...') + '  [' + c.role + ' · rank ' + c.rank + (c.busy ? ' · on a mission' : '') + ']'); aPlay('npc', { seed: c.name });
        }
    }
    var ikA = new THREE.Vector3(), ikB = new THREE.Vector3(), ikC = new THREE.Vector3(), ikQ = new THREE.Quaternion();
    function ikFloor(z) { return z <= 21 ? 0 : (z >= 23.5 ? 0.8 : 0.8 * (z - 21) / 2.5); }
    function ikCollide(p, yFoot, r, H) {
        var w = R25.int.deck.walls, i, q;
        p.x = clamp(p.x, -10 + r, 10 - r); p.z = clamp(p.z, -30 + r, 29.6 - r);
        for (i = 0; i < w.length; i++) {
            q = w[i];
            if (yFoot >= q.max.y - 0.02 || yFoot + H <= q.min.y) continue;
            var cx = clamp(p.x, q.min.x, q.max.x), cz = clamp(p.z, q.min.z, q.max.z), dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
            if (d2 >= r * r) continue;
            if (d2 > 1e-12) { var d = Math.sqrt(d2), k = (r - d) / d; p.x += dx * k; p.z += dz * k; }
            else { var l = p.x - q.min.x, rr = q.max.x - p.x, t = p.z - q.min.z, bb = q.max.z - p.z, m = Math.min(l, rr, t, bb); if (m === l) p.x = q.min.x - r; else if (m === rr) p.x = q.max.x + r; else if (m === t) p.z = q.min.z - r; else p.z = q.max.z + r; }
        }
    }
    function interiorStep(dt) {
        var int = R25.int;
        if (!int) { leaveInterior(false); return; }
        var f = L / int.unit, H = 0.09 * f, rad = 0.022 * f, cmd = cmdOpen, p = ik.hp, grp = int.group;
        if (cmd) { mdx = mdy = 0; }
        ik.yaw -= mdx * MOUSE_SENS * 1.3; ik.pitch = clamp(ik.pitch + mdy * MOUSE_SENS * 1.3, -0.3, 1.4); mdx = mdy = 0;
        var fw = cmd ? 0 : ((keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0)), sd = cmd ? 0 : ((keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0));
        var sy = Math.sin(ik.yaw), cy = Math.cos(ik.yaw), mx = -sy * fw + cy * sd, mz = -cy * fw - sy * sd, ml = Math.sqrt(mx * mx + mz * mz);
        var moving = ml > 0.01, run = moving && !!(keys.ShiftLeft || keys.ShiftRight);
        ik.moving = moving; ik.running = run;
        var spd = (run ? 44 : 22) * H;
        if (moving) { mx /= ml; mz /= ml; }
        var wantJump = !cmd && !!keys.Space;
        if (!ik.air && wantJump && !ik.jumpHeld) { ik.air = true; ik.vv = 8.5 * H; }
        ik.jumpHeld = wantJump;
        var nSub = clamp(Math.ceil(spd * dt / (0.15 * H)), 1, 24), h = dt / nSub, k, ceil = 8 - H;
        for (k = 0; k < nSub; k++) {
            if (moving) { p.x += mx * spd * h; p.z += mz * spd * h; }
            var fl = ikFloor(p.z);
            if (ik.air) { ik.vv -= 22 * H * h; p.y += ik.vv * h; if (p.y >= ceil) { p.y = ceil; ik.vv = Math.min(0, ik.vv); } if (p.y <= fl) { p.y = fl; ik.vv = 0; ik.air = false; } }
            else if (p.y > fl + 0.02 * H) { ik.air = true; ik.vv = 0; } else p.y = fl;
            ikCollide(p, p.y, rad, H);
        }
        grp.updateMatrixWorld(true);
        int.toWorld(p, hum.w);
        hum.obj.group.position.copy(hum.w);
        ikQ.setFromAxisAngle(Y, ik.yaw); hum.obj.group.quaternion.copy(grp.quaternion).multiply(ikQ);
        hum.obj.group.scale.setScalar(H * int.unit);
        hum.obj.update(dt, { moving: moving, running: run, airborne: ik.air, speed: moving ? (run ? 1 : 0.5) : 0, facing: 0 });
        var cp = Math.cos(ik.pitch), spn = Math.sin(ik.pitch), cd = 6 * H;
        ikA.set(p.x, p.y + 0.8 * H, p.z);
        ikB.set(ikA.x + sy * cp * cd, ikA.y + spn * cd, ikA.z + cy * cp * cd);
        ikB.x = clamp(ikB.x, -9.9, 9.9); ikB.y = clamp(ikB.y, 0.05, 7.8); ikB.z = clamp(ikB.z, -29.8, 29.4);
        int.toWorld(ikA, ikC); int.toWorld(ikB, vTmp);
        vA.copy(Y).applyQuaternion(grp.quaternion);
        mM.lookAt(vTmp, ikC, vA); gQ.setFromRotationMatrix(mM);
        groundCam(dt, ikC, vTmp, gQ, 16);
        if (ix.snap > 0) { ix.snap--; camera.position.copy(vTmp); camera.quaternion.copy(gQ); camRel.copy(vTmp).sub(ikC); }
        try { int.update(performance.now() / 1000, dt, hum.w); } catch (e0) { console.info('[ship] interior.update', e0); }
        skCommon(dt);
        var t = ixTarget(); setPrompt(t.label, !!t.label);
    }
    // ─── store UI v2 (chat font): scrollable item menu + gear + sell tabs; dealers (6 items) and the Burger House window (fries only) reuse it ──
    var PRICE_FB = { C: 120, B: 300, A: 700, S: 1500 }, SELL_K = 0.4;
    var storeOpen = false, shopSt = null, shopCtx = null, shopTab = 0, shopRows = [], SHOP_ROWS = 8, shopTop = 0, shopSel = 0, shopDrag = null, shopWheel = 0;
    var TAB_NAMES = ['ITEMS', 'GEAR', 'SELL'];
    var elStore = document.createElement('div');
    elStore.className = 'sh-store';
    elStore.innerHTML = '<div class="ss-h"><b class="ss-name"></b><span class="ss-u"></span></div><div class="ss-tabs"><span>ITEMS</span><span>GEAR</span><span>SELL</span><span>PARTS</span></div><div class="ss-clerk"></div><div class="ss-lw"><div class="ss-list"></div><div class="ss-sb"><u></u></div></div><div class="ss-info"></div><div class="ss-msg"></div><div class="ss-f"></div>';
    hud.appendChild(elStore);
    var elSsName = elStore.querySelector('.ss-name'), elSsU = elStore.querySelector('.ss-u'), elSsClerk = elStore.querySelector('.ss-clerk'), elSsList = elStore.querySelector('.ss-list'), elSsMsg = elStore.querySelector('.ss-msg'), elSsF = elStore.querySelector('.ss-f');
    var elSsTabs = elStore.querySelector('.ss-tabs'), elSsInfo = elStore.querySelector('.ss-info'), elSsThumb = elStore.querySelector('.ss-sb u'), elSsLw = elStore.querySelector('.ss-lw');
    var shopEls = [];
    (function buildShopRows() {
        for (var i = 0; i < SHOP_ROWS; i++) {
            var r = document.createElement('div'); r.className = 'ss-r'; r.style.display = 'none'; r.setAttribute('data-i', String(i));
            r.innerHTML = '<i></i><u></u><span></span><em></em><b></b><s>ⓘ</s>';
            elSsList.appendChild(r);
            shopEls.push({ el: r, k: r.children[0], c: r.children[1], n: r.children[2], d: r.children[3], p: r.children[4], s: r.children[5] });
        }
    })();
    function shopRowOf(t) { while (t && t !== elSsList && !(t.getAttribute && t.getAttribute('data-i'))) t = t.parentNode; return (t && t !== elSsList) ? parseInt(t.getAttribute('data-i'), 10) : -1; }
    elSsList.addEventListener('click', function (e) {
        if (shopDrag && shopDrag.moved) return;
        var i = shopRowOf(e.target); if (i >= 0) { shopSel = shopTop + i; shopPick(shopTop + i); }
    });
    elSsList.addEventListener('mouseover', function (e) { var i = shopRowOf(e.target); if (i >= 0 && shopTop + i !== shopSel) { shopSel = shopTop + i; shopRender(); } });
    elSsList.addEventListener('wheel', function (e) {
        e.preventDefault(); shopWheel += e.deltaY;
        var st = Math.trunc(shopWheel / 40); if (st) { shopWheel -= st * 40; shopScroll(st); }
    }, { passive: false });
    elSsList.addEventListener('pointerdown', function (e) { if (e.button === 0) shopDrag = { y: e.clientY, top: shopTop, moved: false, on: true }; });
    window.addEventListener('pointermove', function (e) {
        if (!shopDrag || !shopDrag.on) return;
        var dy = e.clientY - shopDrag.y, rh = (shopEls[0].el.offsetHeight || 36);
        if (Math.abs(dy) > 5) shopDrag.moved = true;
        if (shopDrag.moved) { var t = clamp(shopDrag.top - Math.round(dy / rh), 0, Math.max(0, shopRows.length - SHOP_ROWS)); if (t !== shopTop) { shopTop = t; shopRender(); } }
    });
    window.addEventListener('pointerup', function () { if (shopDrag) { shopDrag.on = false; var d = shopDrag; setTimeout(function () { if (shopDrag === d) shopDrag = null; }, 0); } });
    elSsTabs.addEventListener('click', function (e) { var i = Array.prototype.indexOf.call(elSsTabs.children, e.target); if (i >= 0) shopSetTab(i); });
    function shopPrice(w) { return w.price != null ? w.price : (PRICE_FB[w.cls] || 200); }
    function shopMsg(t, bad) { elSsMsg.textContent = t || ''; elSsMsg.classList.toggle('is-bad', !!bad); }
    function shopScroll(d) { shopTop = clamp(shopTop + d, 0, Math.max(0, shopRows.length - SHOP_ROWS)); shopSel = clamp(shopSel, shopTop, Math.min(shopRows.length - 1, shopTop + SHOP_ROWS - 1)); shopRender(); }
    function shopSetTab(i) {
        if (!shopCtx || (shopCtx.mode !== 'store' && shopCtx.mode !== 'trade')) return;
        if (shopCtx.mode === 'trade' && i !== 0 && i !== 2) i = shopTab === 0 ? 2 : 0;
        shopTab = clamp(i, 0, 3); shopTop = 0; shopSel = 0; shopMsg(''); shopFill(); aPlay('ui', { vel: 0.5 });
    }
    function itemSub(it) {
        var p = it.effect && it.effect.params ? Object.keys(it.effect.params) : [], d = it.effect ? it.effect.duration : 0;
        if (it.kind === 'fries') return 'LIGHT BLUE GLOW · ' + ((it.effect && it.effect.glow && it.effect.glow.minutes) || 45) + ' MIN' + (it.effect && it.effect.extras && it.effect.extras.speed ? ' · FASTER' : '');
        return (d >= 120 ? Math.round(d / 6) / 10 + ' MIN' : d + ' S') + ' · ' + (p.slice(0, 4).join(' ') || 'chill');
    }
    function shopFill() {
        var st = shopSt, inv = st && st.inventory, pf = ensureProfile(userName()), i, ctx = shopCtx;
        shopRows.length = 0;
        var tab = (ctx.mode === 'store' || ctx.mode === 'trade') ? shopTab : 0;
        if (ctx.mode === 'trade') { tradeFill(ctx, pf); tab = -1; }
        if (tab === 0) {
            var menu = ctx.items || [];
            if (ctx.mode === 'burger' && r25Cand()) { var cd0 = r25Cand(); shopRows.push({ k: 'r', tag: 'CR', name: 'HIRE ' + String(cd0.name).toUpperCase(), sub: String(cd0.role).toUpperCase() + ' · JOINS YOUR CREW', price: 0 }); }
            if (ctx.mode === 'store' && st && ITM.dailyDeal) {
                try { var dl = ITM.dailyDeal(st.id); shopRows.push({ k: 'deal', deal: dl, tag: 'DL', name: dl.name, sub: 'TODAY ONLY · COMBO OF 3', price: dl.price, was: dl.originalPrice, special: true, info: dl.name + '\n' + dl.items.map(function (x) { return '- ' + x.name; }).join('\n') + '\n' + dl.blurb }); } catch (e0) { /* ignore */ }
            }
            for (i = 0; i < menu.length; i++) {
                var it = menu[i], have = 0; pf.items.forEach(function (r) { if (r.id === CRF.stackKeyOf(it) && !!r.d === !!it.dealer) have = r.n; });
                shopRows.push({ k: 'i', item: it, name: it.name, sub: itemSub(it), price: ctx.mode === 'burger' ? salePrice(it.price) : it.price, own: have > 0, have: have, was: it.special ? it.originalPrice : null, special: !!it.special });
            }
        } else if (tab === 1 && inv) {
            for (i = 0; i < inv.weapons.length; i++) { var w = inv.weapons[i]; shopRows.push({ k: 'w', i: i, tag: String(w.cls || 'C'), name: w.name, sub: wpnMod && wpnMod.weaponLine ? wpnMod.weaponLine(w).replace(/^[A-Z] /, '').replace(w.name, '').trim() : '', price: shopPrice(w), own: pf.weapons.some(function (x) { return x.id === w.id; }) }); }
            var sn = (pf.shieldTier | 0) + 1, en = (pf.engineTier | 0) + 1;
            if (sn <= 3 && inv.upgrades.shield[sn - 1]) { var su = inv.upgrades.shield[sn - 1]; shopRows.push({ k: 's', tag: 'SH', name: su.name, sub: 'MAX SHIELD +20 · TIER ' + sn + '/3', price: su.price }); }
            else shopRows.push({ k: '-', tag: 'SH', name: 'SHIELD CELL', sub: 'MAXED · ' + (HP_MAX) + ' SHIELD', price: null });
            if (en <= 3 && inv.upgrades.engine[en - 1]) { var eu = inv.upgrades.engine[en - 1]; shopRows.push({ k: 'e', tag: 'DR', name: eu.name, sub: 'CRUISE + BOOST +10 % · TIER ' + en + '/3', price: eu.price }); }
            else shopRows.push({ k: '-', tag: 'DR', name: 'DRIVE TUNE', sub: 'MAXED · +' + Math.round((engMul - 1) * 100) + ' %', price: null });
            if (inv.snack) shopRows.push({ k: 'f', tag: 'FD', name: inv.snack.name, sub: 'FULL SHIELD', price: inv.snack.price });
            suitRows(pf);
        } else if (tab === 3) {
            var pm = (inv && inv.parts && inv.parts.length) ? inv.parts : CRF.PARTS;
            for (i = 0; i < pm.length; i++) { var pi = pm[i]; shopRows.push({ k: 'i', item: pi, name: pi.name, sub: String(pi.blurb || '').slice(0, 44), price: pi.price }); }
        } else if (tab === 2) {
            for (i = 0; i < pf.weapons.length; i++) { var o = pf.weapons[i]; shopRows.push({ k: 'x', i: i, tag: String(o.cls || 'C'), name: o.name, sub: (pf.weapon && pf.weapon.id === o.id) ? 'EQUIPPED' : 'OWNED', price: Math.round(shopPrice(o) * SELL_K) }); }
            for (i = 0; i < pf.items.length; i++) { var r = pf.items[i], io = itemOf(r); if (io) shopRows.push({ k: 'xi', i: i, item: io, name: io.name, sub: 'x' + r.n + ' OWNED', price: (io.category || io.kind === 'part') ? CRF.sellPrice(io) : Math.max(1, Math.round(io.price * SELL_K)) }); }
            if (!shopRows.length) shopRows.push({ k: '-', tag: '--', name: 'NOTHING TO SELL', sub: '', price: null });
        }
        shopTop = clamp(shopTop, 0, Math.max(0, shopRows.length - SHOP_ROWS)); shopSel = clamp(shopSel, 0, Math.max(0, shopRows.length - 1));
        shopRender();
    }
    function shopRender() {
        var pf = ensureProfile(userName()), i, e, row, sell = ctxTab() === 2, n = shopRows.length;
        if (shopSel < shopTop) shopTop = shopSel; else if (shopSel > shopTop + SHOP_ROWS - 1) shopTop = shopSel - SHOP_ROWS + 1;
        shopTop = clamp(shopTop, 0, Math.max(0, n - SHOP_ROWS));
        elSsU.textContent = CRF.fmt(pf.gor);
        for (i = 0; i < SHOP_ROWS; i++) {
            e = shopEls[i];
            if (shopTop + i >= n) { e.el.style.display = 'none'; continue; }
            row = shopRows[shopTop + i];
            e.el.style.display = ''; e.k.textContent = String(i + 1); e.n.textContent = row.name; e.d.textContent = (row.special && row.k !== 'deal' ? 'TODAY ONLY · ' : '') + (row.sub || '');
            if (row.item) { e.c.textContent = GLYPH[row.item.kind] || '■'; e.c.style.background = cssHex(row.item.color); e.c.style.color = '#101018'; e.s.style.visibility = 'visible'; }
            else { e.c.textContent = row.tag || ''; e.c.style.background = ''; e.c.style.color = ''; e.s.style.visibility = 'hidden'; }
            if (row.was != null && row.price != null) e.p.innerHTML = '<s class="ss-was">' + (row.was | 0) + '</s> ' + (row.price | 0); else e.p.textContent = row.price == null ? '' : (sell ? '+' : '') + row.price;
            e.el.classList.toggle('is-special', !!row.special);
            e.el.classList.toggle('is-own', !!row.own); e.el.classList.toggle('is-sel', shopTop + i === shopSel);
            e.el.classList.toggle('is-poor', row.price != null && !sell && row.k !== '-' && !(row.own && row.k === 'w') && (pf.gor | 0) < row.price);
        }
        var sel = shopRows[shopSel];
        elSsInfo.textContent = sel && sel.info ? sel.info : (sel && sel.item ? describeItem(sel.item) + (sel.special ? '\nTODAY ONLY · was ' + sel.was : '') : (sel && sel.sub ? sel.name + '\n' + sel.sub : ''));
        elSsLw.classList.toggle('has-sb', n > SHOP_ROWS);
        elSsThumb.style.height = Math.max(8, SHOP_ROWS / Math.max(SHOP_ROWS, n) * 100) + '%';
        elSsThumb.style.top = (n > SHOP_ROWS ? shopTop / (n - SHOP_ROWS) * (100 - Math.max(8, SHOP_ROWS / n * 100)) : 0) + '%';
        for (i = 0; i < 4; i++) { elSsTabs.children[i].classList.toggle('is-on', i === ctxTab()); elSsTabs.children[i].style.display = (shopCtx && shopCtx.mode !== 'store' && !(shopCtx.mode === 'trade' && (i === 0 || i === 2)) && i > 0) ? 'none' : ''; }
        if (shopCtx && shopCtx.mode === 'trade') { elSsTabs.children[0].textContent = 'BUY'; elSsTabs.children[2].textContent = 'SELL'; } else if (elSsTabs.children[0].textContent === 'BUY') { elSsTabs.children[0].textContent = 'ITEMS'; elSsTabs.children[2].textContent = 'SELL'; }
        elSsF.textContent = '1-' + Math.min(SHOP_ROWS, n) + ' / ENTER ' + (sell ? 'SELL (40 %)' : 'BUY') + ' · WHEEL / ARROWS / DRAG SCROLL' + (shopCtx && (shopCtx.mode === 'store' || shopCtx.mode === 'trade') ? ' · TAB SWITCH' : '') + (shopCtx && shopCtx.qSeed != null ? ' · Q QUESTS' : '') + ' · ESC LEAVE';
    }
    function ctxTab() { return shopCtx && (shopCtx.mode === 'store' || shopCtx.mode === 'trade') ? shopTab : 0; }
    function clerkLine(st) {
        var line = '';
        try {
            if (world && world.stores && world.stores.indexOf(st) >= 0 && typeof world.npcLine === 'function') line = npcLineOf(st.id, null);
            else if (st.clerk && st.clerk.lines && st.clerk.lines.length) { st.lineIdx = ((st.lineIdx | 0) + 1) % st.clerk.lines.length; line = st.clerk.lines[st.lineIdx]; }
        } catch (e) { line = ''; }
        return line;
    }
    function shopOpenCommon(ctx, title, who, line, seedId) {
        ctx.qSeed = seedId; ctx.qWho = who; shopCtx = ctx; shopTab = 0; shopTop = 0; shopSel = 0; storeOpen = true; cmdOpen = true; cmdGuardUntil = performance.now() + 600;
        keys = Object.create(null); firing = false; mdx = mdy = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        if (locked()) { try { document.exitPointerLock(); } catch (e) { /* ignore */ } }
        hud.classList.add('is-store'); elStore.classList.add('is-on');
        elSsName.textContent = String(title).toUpperCase();
        lingoFill(elSsClerk, who + ': ' + (line || '...'));
        addRow(who, '#ffd36a', line || '...');
        aPlay('npc', { seed: seedId }); aPlay('ui', { vel: 0.6 });
        shopMsg('');
        shopFill();
    }
    function openStore(st) {
        if (storeOpen || !st || !st.inventory || state !== 'piloting' || cmdOpen) return;
        knownFor();
        shopSt = st;
        shopOpenCommon({ mode: 'store', items: st.menu || [] }, st.name || '7/11', clerkName(st), clerkLine(st), st.id);
    }
    function openDealer(n) {
        if (storeOpen || !n || !n.menu || state !== 'piloting' || cmdOpen) return;
        knownFor(); shopSt = null;
        var line = '';
        try { line = world.npcLine(n.id) || ''; } catch (e) { line = ''; }
        if (!line && typeof n.say === 'function') line = n.say('dealer', n.menu[0]);
        shopOpenCommon({ mode: 'dealer', items: n.menu }, String(n.name || 'DEALER') + ' · DEALER', String(n.name || 'DEALER').toUpperCase(), line, n.id);
    }
    function openBurger(bh) {
        if (storeOpen || !bh || state !== 'piloting' || cmdOpen) return;
        knownFor(); shopSt = null;
        var c = bh.clerk, line = '';
        try { line = world.npcSay('burger', 'fries') || ''; } catch (e) { line = ''; }
        if (!line && c && typeof c.say === 'function') line = c.say('fries');
        r25BurgerVisit(bh);
        shopOpenCommon({ mode: 'burger', items: ITM.FRIES_TIERS || (bh.menu && bh.menu.length ? bh.menu : [ITM.FRIES]) }, 'BURGER HOUSE', String((c && c.name) || 'CLERK').toUpperCase(), line, 'burger');
    }
    function closeStore() {
        if (!storeOpen) return;
        if (DLG.open) dlgHide();
        if (qOpen) qHide();
        storeOpen = false; cmdOpen = false; shopSt = null; shopCtx = null; shopDrag = null;
        hud.classList.remove('is-store'); elStore.classList.remove('is-on');
        keys = Object.create(null); mdx = mdy = 0;
        aPlay('ui', { vel: 0.4 });
        if (state === 'piloting') { try { var p = document.body.requestPointerLock(); if (p && p.catch) p.catch(function () { /* keyboard only */ }); } catch (e) { /* ignore */ } }
    }
    function shopPick(i) {
        var row = shopRows[i], pf = ensureProfile(userName()), st = shopSt;
        if (!row || !shopCtx) return;
        if (!curUser) curUser = userName();
        if (row.k === '-') { aPlay('ui', { vel: 0.3 }); return; }
        if (row.k === 'r') { r25Hire(); shopFill(); return; }
        if (row.k === 'tb' || row.k === 'ts') { tradePick(row, pf); return; }
        if (row.k === 'x') {
            var ow = pf.weapons[row.i]; if (!ow) return;
            pf.weapons = pf.weapons.filter(function (x, j) { return j !== row.i; });
            if (pf.weapon && pf.weapon.id === ow.id) pf.weapon = null;
            pf.gor = (pf.gor | 0) + row.price; aPlay('buy', { pitch: 0.8 }); writeSave(); shopMsg('SOLD ' + ow.name + ' · +' + row.price); shopFill(); return;
        }
        if (row.k === 'xi') {
            var rec = pf.items[row.i]; if (!rec) return;
            rec.n--; if (rec.n <= 0) pf.items.splice(row.i, 1);
            pf.gor = (pf.gor | 0) + row.price; aPlay('buy', { pitch: 0.8 }); writeSave(); shopMsg('SOLD ' + row.name + ' · +' + row.price); shopFill(); return;
        }
        if (row.k === 'deal') {
            if ((pf.gor | 0) < row.price) { shopMsg('NOT ENOUGH gorCoin', true); aPlay('ui', { vel: 0.3, pitch: 0.6 }); return; }
            pf.gor -= row.price; row.deal.items.forEach(function (x) { giveItem(x, 1); });
            aPlay('buy'); shopMsg('BOUGHT THE COMBO'); writeSave(); shopFill(); return;
        }
        if (row.k === 'i') {
            if ((pf.gor | 0) < row.price) { shopMsg('NOT ENOUGH gorCoin', true); aPlay('ui', { vel: 0.3, pitch: 0.6 }); return; }
            pf.gor -= row.price; giveItem(row.item, 1);
            aPlay('buy'); shopMsg('BOUGHT ' + row.item.name); writeSave();
            learnWords(1 + Math.floor(Math.random() * 3));
            shopFill(); return;
        }
        if (row.k === 'w') {
            var w = st.inventory.weapons[row.i];
            if (row.own) { var mine = pf.weapons.filter(function (x) { return x.id === w.id; })[0]; if (mine) { equipWeapon(mine); shopMsg('EQUIPPED ' + mine.name); aPlay('ui', { vel: 0.6 }); shopFill(); } return; }
            if ((pf.gor | 0) < row.price) { shopMsg('NOT ENOUGH gorCoin', true); aPlay('ui', { vel: 0.3, pitch: 0.6 }); return; }
            pf.gor -= row.price;
            var copy = JSON.parse(JSON.stringify(w));
            equipWeapon(copy);
            aPlay('buy'); shopMsg('BOUGHT ' + w.name); shopFill(); return;
        }
        if ((pf.gor | 0) < row.price) { shopMsg('NOT ENOUGH gorCoin', true); aPlay('ui', { vel: 0.3, pitch: 0.6 }); return; }
        pf.gor -= row.price;
        if (row.k === 's') { pf.shieldTier = clamp((pf.shieldTier | 0) + 1, 0, 3); applyUpgrades(); hp = Math.min(HP_MAX, hp + 20); shopMsg('SHIELD TIER ' + pf.shieldTier + ' · MAX ' + HP_MAX); }
        else if (row.k === 'e') { pf.engineTier = clamp((pf.engineTier | 0) + 1, 0, 3); applyUpgrades(); shopMsg('DRIVE TIER ' + pf.engineTier + ' · +' + Math.round((engMul - 1) * 100) + ' %'); }
        else if (row.k === 'f') { hp = HP_MAX; sinceHit = 99; shopMsg('SHIELD FULL'); }
        else if (row.k === 'u') { pf.suit[row.slot] = clamp((pf.suit[row.slot] | 0) + 1, 0, 3); suitSync(true); shopMsg('SUIT ' + row.slot.toUpperCase() + ' TIER ' + pf.suit[row.slot]); }
        aPlay('buy'); writeSave(); shopFill();
    }
    function storeKey(e) {
        if (DLG.open) { dlgKey(e); return; }
        if (qOpen) { qKey(e); return; }
        var c = e.code;
        e.preventDefault(); e.stopPropagation();
        if (c === 'Escape') { e.__shipHandled = true; closeStore(); return; }
        if (c === 'ArrowDown' || c === 'ArrowUp') { shopSel = clamp(shopSel + (c === 'ArrowDown' ? 1 : -1), 0, Math.max(0, shopRows.length - 1)); shopRender(); return; }
        if (c === 'PageDown' || c === 'PageUp') { shopSel = clamp(shopSel + (c === 'PageDown' ? SHOP_ROWS : -SHOP_ROWS), 0, Math.max(0, shopRows.length - 1)); shopRender(); return; }
        if (e.repeat) return;
        if (c === 'KeyQ' && shopCtx && shopCtx.qSeed != null) { var qs = shopCtx; closeStore(); openQuests(qs.qSeed, qs.qWho, qs.mode === 'dealer' ? 'dealer' : 'clerk'); return; }
        if (c === 'KeyF' || c === 'KeyE' || c === 'Backspace') { closeStore(); return; }
        if (c === 'Enter' || c === 'NumpadEnter') { shopPick(shopSel); return; }
        if (c === 'KeyX' || c === 'Tab') { if (shopCtx && shopCtx.mode === 'store') shopSetTab((shopTab + (e.shiftKey ? 3 : 1)) % 4); else if (shopCtx && shopCtx.mode === 'trade') shopSetTab(shopTab === 0 ? 2 : 0); return; }
        var m = /^(?:Digit|Numpad)([1-8])$/.exec(c);
        if (m) { var ri = shopTop + parseInt(m[1], 10) - 1; if (ri < shopRows.length) { shopSel = ri; shopPick(ri); } }
    }
    // F on foot: the nearest thing to use. counter -> store, Burger House window -> fries, dealer -> street menu, shopper -> a line, clerk -> a line
    var ftI = { kind: '', obj: null, label: '' }, mutterT = 0;
    function footTarget() {
        ftI.kind = ''; ftI.obj = null; ftI.label = '';
        if (gmode !== 'foot' || !hum.obj || !world || !world.node) return ftI;
        var c = counterNear(), n, k;
        if (c) {
            if (world.cart && world.cart.length) { ftI.kind = 'checkout'; ftI.obj = c; ftI.label = 'F · CHECKOUT ' + CRF.fmt(world.cartTotal()); return ftI; }
            ftI.kind = 'store'; ftI.obj = c; ftI.label = 'F · SHOP'; return ftI;
        }
        if (world.nearBurger) { ftI.kind = 'burger'; ftI.obj = world.nearBurger; ftI.label = 'F · FRIES'; return ftI; }
        var shf = r28ShelfFind(); if (shf) { ftI.kind = 'shelf'; ftI.obj = shf; ftI.label = 'F · GRAB ' + String(shf.item && shf.item.name || 'ITEM').toUpperCase() + ' ' + ((shf.item && shf.item.price) | 0); return ftI; }
        var rn = resFind(); if (rn) { ftI.kind = 'resource'; ftI.obj = rn; ftI.label = 'F · HARVEST ' + String(rn.kind).toUpperCase(); return ftI; }
        var tc = tameFind(); if (tc) { ftI.kind = 'creature'; ftI.obj = tc; ftI.label = 'F · TAME'; return ftI; }
        n = world.nearNpc;
        if (n) { k = n.role === 'dealer' ? 'dealer' : 'shopper'; ftI.kind = k; ftI.obj = n; ftI.label = k === 'dealer' ? 'F · DEAL' : 'F · TALK'; return ftI; }
        c = npcNear();
        if (c) { ftI.kind = 'clerk'; ftI.obj = c; ftI.label = 'F · TALK'; }
        return ftI;
    }
    function talkShopper(n) {
        knownFor();
        if (r28Talk({ id: n.id, name: n.name || 'SHOPPER', role: n.role || 'shopper', kind: 'shopper', obj: n, seed: n.seed })) return;
        addRow(String(n.name || 'SHOPPER').toUpperCase(), '#8fa0ff', npcLineOf(n.id, n) || '...');
        aPlay('npc', { seed: n.id }); openQuests(n.id, n.name || 'SHOPPER', 'npc');
    }
    function mutterTick(dt) {              // a shopper you pass within 2 human heights mutters a line (once per 8 s)
        mutterT -= dt;
        if (mutterT > 0 || storeOpen || invOpen || !world || !world.node || !hum.obj) return;
        var n = world.nearNpc;
        if (!n || n.role === 'dealer' || n.role === 'cashier' || n.role === 'fries') return;
        if (locToWorld(n.pos, vSc).distanceTo(hum.w) > 2 * 0.09 * L) return;
        mutterT = 8;
        addRow(String(n.name || 'SHOPPER').toUpperCase(), '#8fa0ff', npcLineOf(n.id, n) || '...');
        aPlay('npc', { seed: n.id, vel: 0.5 });
    }
    function counterNear() {
        if (gmode !== 'foot' || !hum.obj || !world || !world.node || !world.stores) return null;
        if (world.nearStore) return world.nearStore;
        var best = null, bd = 1e30, i, st, r, d;
        for (i = 0; i < world.stores.length; i++) {
            st = world.stores[i]; locToWorld(st.counter.pos, vSc);
            r = Math.max(st.counter.radius, 2 * 0.09 * L); d = vSc.distanceTo(hum.w);
            if (d < r && d < bd) { bd = d; best = st; }
        }
        return best;
    }
    // ─── black hole (hard sphere 1.3 x coreR, gravity inside 3 x coreR, lensing) ────────────────────────────────────────────────────
    var bhPrev = new THREE.Vector3(), bhInv = 0, bhLast = -1;
    function bhStep(dt, P) {
        var b = engine.blackHole;
        if (!b || !(b.coreR > 0)) return;
        var c = b.pos, cr = b.coreR, hard = 1.3 * cr;
        if (bhInv > 0) bhInv -= dt;
        vA.subVectors(P, bhPrev); vB.subVectors(bhPrev, c);
        var a2 = vA.lengthSq(), b2 = 2 * vB.dot(vA), c2 = vB.lengthSq() - hard * hard, tHit = -1;
        if (c2 <= 0) tHit = 0;
        else if (a2 > 1e-12) { var disc = b2 * b2 - 4 * a2 * c2; if (disc >= 0) { var tt = (-b2 - Math.sqrt(disc)) / (2 * a2); if (tt >= 0 && tt <= 1) tHit = tt; } }
        if (tHit < 0 && vTmp.subVectors(P, c).lengthSq() < hard * hard) tHit = 1;
        if (tHit >= 0) {
            vD.copy(vB).addScaledVector(vA, tHit);
            var dl = vD.length();
            if (dl < 1e-6) vD.copy(vF).negate(); else vD.divideScalar(dl);
            P.copy(c).addScaledVector(vD, hard * 1.01);
            var vn = vel.dot(vD);
            if (vn < 0) vel.addScaledVector(vD, -vn * 1.5);
            speed = Math.min(speed, 0.3 * CRUISE); pulse = 0; pulseT = 0;
            if (bhInv <= 0) {
                bhInv = 1.5;
                bounceV.addScaledVector(vD, 3 * BOOST); throttle = Math.min(throttle, 0);
                hurtPlayer(35, undefined, 3.2, true);
                shake = Math.max(shake, 4 * L); fovKick = 8;
                burst(P, 16, 1, 20, 3); fx.flash(P, 0xff8a3a);
                aPlay('ram', { vel: 1 });
            }
        }
        vTmp.subVectors(c, P); var dist = vTmp.length();
        if (dist < 3 * cr && dist > 1e-6) {
            var k = clamp((3 * cr - dist) / (3 * cr - hard), 0, 1); k = k * k * (3 - 2 * k);
            P.addScaledVector(vTmp, 0.25 * CRUISE * k * dt / dist);
        }
        var lens = clamp(3 * cr / Math.max(dist, 1e-6) - 1, 0, 1);
        if (Math.abs(lens - bhLast) > 0.004) { bhLast = lens; try { engine.lensing(lens); } catch (e) { /* ignore */ } }
    }
    // ─── freighter chevrons (HUD) ────────────────────────────────────────────────────────────────────────────────────────────────
    var frtEls = [];
    (function buildFrt() {
        for (var i = 0; i < 2; i++) { var el = document.createElement('div'); el.className = 'sh-frt'; el.style.display = 'none'; el.innerHTML = '<i></i><b></b>'; hud.appendChild(el); frtEls.push({ el: el, b: el.lastChild, on: false, txt: '', x: -1e9, y: -1e9 }); }
    })();
    function freighterHud() {
        var fr = space && space.freighters, i, W = window.innerWidth, H = window.innerHeight;
        cvTick();
        for (i = 0; i < frtEls.length; i++) {
            var m = frtEls[i], f = fr && fr[i], show = false;
            if (f && f.pos) {
                var d = f.pos.distanceTo(shipRoot.position) / L;
                if (d < 300) {
                    vSc.copy(f.pos).project(camera);
                    if (vSc.z < 1 && vSc.z > -1 && Math.abs(vSc.x) < 1.05 && Math.abs(vSc.y) < 1.05) {
                        show = true;
                        var x = (vSc.x * 0.5 + 0.5) * W, y = (-vSc.y * 0.5 + 0.5) * H, t = 'HAULER ' + Math.round(d) + ' L';
                        if (t !== m.txt) { m.txt = t; m.b.textContent = t; }
                        if (Math.abs(x - m.x) > 0.5 || Math.abs(y - m.y) > 0.5) { m.x = x; m.y = y; m.el.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) translate(-50%,-50%)'; }
                    }
                }
            }
            if (show !== m.on) { m.on = show; m.el.style.display = show ? '' : 'none'; }
        }
    }
    // ─── station docking: scripted approach, deck on foot, scripted launch ────────────────────────────────────────────────────────
    // The ship and the human are stored STATION-LOCAL (L units) and ride SITE().pos / quat every frame, like the planet frame.
    var SK_R = 0.022, SK_H = 0.09, SK_CAM = 0.54;
    var sk = { mode: '', pad: 0, pts: [], cum: [], len: 0, t: 0, T: 6, q0: new THREE.Quaternion(), shipL: new THREE.Vector3(), hp: new THREE.Vector3(), yaw: 0, pitch: 0.3, vv: 0, air: false, moving: false, running: false, jumpHeld: false, nl: 0, cd: 0, fade: 0 };
    var skA = new THREE.Vector3(), skB = new THREE.Vector3(), skC = new THREE.Vector3(), skI = { kind: '', obj: null, label: '' };
    function skPathAt(s, out) {
        var c = sk.cum, p = sk.pts, n = p.length, i = 1;
        if (n < 2 || s <= 0) return out.copy(p[0]);
        if (s >= sk.len) return out.copy(p[n - 1]);
        while (i < n - 1 && c[i] < s) i++;
        return out.lerpVectors(p[i - 1], p[i], (s - c[i - 1]) / Math.max(1e-6, c[i] - c[i - 1]));
    }
    function skBuild(first, worldPts) {
        var i, l;
        sk.pts.length = 0; sk.cum.length = 0;
        if (first) sk.pts.push(first.clone());
        for (i = 0; i < worldPts.length; i++) { l = SITE().toLocal(worldPts[i], new THREE.Vector3()); if (first && l.z >= first.z - 0.8) continue; sk.pts.push(l); }
        if (sk.pts.length < 2) sk.pts.push(sk.pts[0].clone().add(new THREE.Vector3(0, 0, -1)));
        sk.cum.push(0);
        for (i = 1; i < sk.pts.length; i++) sk.cum.push(sk.cum[i - 1] + sk.pts[i].distanceTo(sk.pts[i - 1]));
        sk.len = sk.cum[sk.cum.length - 1];
    }
    function beginDock(dd) {
        R27.dsite = dd ? dkSite(dd) : null;
        if (!SITE() || sk.mode || gmode !== 'fly' || dead || state !== 'piloting' || boarding || exiting) return false;
        var pad = 0, i;
        for (i = 0; i < SITE().pads.length; i++) if (SITE().pads[i].free) { pad = i; break; }
        SITE().group.updateMatrixWorld(true);
        SITE().toLocal(shipRoot.position, skA);
        skBuild(skA, SITE().dockPath(pad));
        sk.q0.copy(SITE().quat).invert().multiply(shipRoot.quaternion);
        sk.pad = pad; sk.t = 0; sk.T = clamp(sk.len / 12, 4.5, 9); sk.mode = 'docking'; gmode = 'docking';
        lfOff(); killBolts(); combatRoot.visible = false; firing = false; mdx = mdy = 0; keys = Object.create(null);
        vel.set(0, 0, 0); speed = 0; throttle = 0; pulse = 0; pulseT = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        setGround(true, false); setPrompt('DOCKING', true);
        addRow('', '', 'DOCKING · ' + (R27.dsite ? 'DERELICT' : (SITE().interior.stores[0] ? String(SITE().interior.stores[0].name).toUpperCase() : '7/11 ORBITAL')), 'is-sys');
        aPlay('liftoff', { vel: 0.6 }); aPlay('warp');
        return true;
    }
    function beginLaunch() {
        if (sk.mode !== 'deck' || !SITE()) return;
        closeStore();
        SITE().group.updateMatrixWorld(true);
        skBuild(null, SITE().launchPath(sk.pad));
        sk.t = 0; sk.T = clamp(sk.len / 11, 4, 8); sk.mode = 'launching'; gmode = 'launching';
        if (hum.obj) hum.obj.group.visible = false;
        jetKill(); setPrompt('', false);
        setGround(true, false);
        net && net.setMode && net.setMode('fly');
        aPlay('liftoff', { vel: 1 }); aPlay('warp');
    }
    function skAbort() {       // Esc / exit() while anywhere in the station sequence: put the ship outside the mouth and hand back to the exit cinematic
        closeStore();
        if (hum.obj) hum.obj.group.visible = false;
        if (SITE() && sk.mode) {
            try { SITE().group.updateMatrixWorld(true); var lp = SITE().launchPath(sk.pad); shipRoot.position.copy(lp[lp.length - 1]); } catch (e) { /* ignore */ }
            if (SITE().pads[sk.pad]) SITE().pads[sk.pad].free = true;
        }
        sk.mode = ''; sk.cd = 3; exOff(exMe); hullY(0);
        if (camera.near !== baseNear) { camera.near = baseNear; camera.updateProjectionMatrix(); }
        combatRoot.visible = true; gmode = 'fly'; setGround(false, false); setPrompt('', false);
        net && net.setMode && net.setMode('fly');
        dkLeave();
    }
    function skLand() {
        sk.mode = 'deck'; gmode = 'sfoot';
        var pad = SITE().pads[sk.pad];
        if (pad) pad.free = false;
        sk.shipL.copy(pad ? pad.local : skA.set(0, 0.4, 0));
        exOff(exMe); hullY(0);
        if (!hum.obj) { hum.obj = makeHuman(effColor()); scene.add(hum.obj.group); } else hum.obj.setColor(effColor());
        hum.obj.group.visible = true;
        var px = sk.shipL.x;
        if (R27.dsite && R27.dsite.spawn) sk.hp.copy(R27.dsite.spawn); else sk.hp.set(px + (px > 0 ? -1.7 : 1.7), 0, sk.shipL.z + 0.4);
        sk.yaw = Math.atan2(-(sk.shipL.x - sk.hp.x), -(sk.shipL.z - sk.hp.z)); sk.pitch = 0.3; sk.vv = 0; sk.air = false; sk.jumpHeld = true;
        sk.moving = sk.running = false; camRelInit = false; mdx = mdy = 0;
        setGround(true, true); setPrompt('', false);
        net && net.setMode && net.setMode('landed'); sk.nl = 0.4;
        shake = Math.max(shake, 0.06 * L); aPlay('land', { vel: 1 });
        SITE().toWorld(sk.shipL, skB); for (var i = 0; i < 3; i++) fx.impact(skB, 0x9ab0ff, 2);
        if (R27.dsite) { addRow('', '', 'DERELICT · loot crates glow in the far room (F) · something is awake in here', 'is-sys'); dkOnDock(R27.dsite.d); }
        else addRow('', '', 'DOCKED · F near the counter, NPCs, shipyard, map pedestal or your ship', 'is-sys');
        r28Dock();
    }
    function finishLaunch() {
        var pad = SITE().pads[sk.pad]; if (pad) pad.free = true;
        sk.mode = ''; sk.cd = 3; gmode = 'fly';
        vA.copy(SITE().mouth.dir);
        shipRoot.quaternion.setFromUnitVectors(NEG_Z, vA.normalize());
        vel.copy(vA).multiplyScalar(2.5 * CRUISE).add(SITE().vel); speed = 2.5 * CRUISE; throttle = 0.6; pulse = 0; pulseT = 0; bank = 0; yawRate = pitRate = 0;
        combatRoot.visible = true; setGround(false, false); setPrompt('', false); hullY(0); exOff(exMe);
        mdx = mdy = 0; keys = Object.create(null); lfOff();
        if (camera.near !== baseNear) { camera.near = baseNear; camera.updateProjectionMatrix(); }
        net && net.setMode && net.setMode('fly');
        shake = Math.max(shake, 0.05 * L);
        dkLeave();
    }
    function skCollide(p, yFoot) {
        var b = SITE().deck.bounds, w = SITE().deck.walls, i, r = SK_R, q;
        p.x = clamp(p.x, b.min.x + r, b.max.x - r); p.z = clamp(p.z, b.min.z + r, b.max.z - r);
        for (i = 0; i < w.length; i++) {
            q = w[i];
            if (q.name === 'wallL' || q.name === 'wallR' || q.name === 'back' || q.name === 'mouthField') continue;
            if (yFoot >= q.max.y || yFoot + SK_H <= q.min.y) continue;
            var cx = clamp(p.x, q.min.x, q.max.x), cz = clamp(p.z, q.min.z, q.max.z), dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
            if (d2 >= r * r) continue;
            if (d2 > 1e-12) { var d = Math.sqrt(d2), k = (r - d) / d; p.x += dx * k; p.z += dz * k; }
            else {
                var l = p.x - q.min.x, rr = q.max.x - p.x, t = p.z - q.min.z, bb = q.max.z - p.z, m = Math.min(l, rr, t, bb);
                if (m === l) p.x = q.min.x - r; else if (m === rr) p.x = q.max.x + r; else if (m === t) p.z = q.min.z - r; else p.z = q.max.z + r;
            }
        }
        if (yFoot < sk.shipL.y + 0.45) {                      // the parked ship is solid too
            var sx = p.x - sk.shipL.x, sz = p.z - sk.shipL.z, sd = Math.sqrt(sx * sx + sz * sz), sr = 0.62 + r;
            if (sd < sr) { if (sd < 1e-6) { sx = 1; sz = 0; sd = 1; } p.x = sk.shipL.x + sx / sd * sr; p.z = sk.shipL.z + sz / sd * sr; }
        }
    }
    function stationInteract() {
        skI.kind = ''; skI.obj = null; skI.label = '';
        if (!SITE() || !hum.obj || sk.mode !== 'deck' || storeOpen) return skI;
        var hw = hum.w, st0 = SITE().interior.stores[0], npcs = SITE().interior.npcs, ped = SITE().interior.mapPedestal, i;
        if (st0 && hw.distanceTo(st0.counter.pos) < Math.max(st0.counter.radius * 1.8, 2 * SK_H * L)) { skI.kind = 'store'; skI.obj = st0; skI.label = 'F · SHOP'; return skI; }
        for (i = 0; i < npcs.length; i++) if (hw.distanceTo(npcs[i].pos) < 1.4 * L) { skI.kind = 'npc'; skI.obj = npcs[i]; skI.label = 'F · TALK'; return skI; }
        var tr = SITE().interior.trade; if (tr && tr.pos && hw.distanceTo(tr.pos) < Math.max(tr.radius || 0, 1.8 * L)) { skI.kind = 'trade'; skI.obj = tr; skI.label = 'F · TRADE'; return skI; }
        var yd = !R27.dsite && SITE().interior.shipyard; if (yd && yd.pos && yd.ships && hw.distanceTo(yd.pos) < Math.max(yd.radius || 0, 1.5 * L)) { skI.kind = 'shipyard'; skI.obj = yd; skI.label = 'F · SHIPYARD'; return skI; }
        if (R27.dsite) { var dc = dkCrateNear(); if (dc) { skI.kind = 'loot'; skI.obj = dc; skI.label = 'F · LOOT'; return skI; } }
        if (hw.distanceTo(ped.pos) < ped.radius + 0.3 * L) { skI.kind = 'map'; skI.label = 'F · GALAXY MAP'; return skI; }
        if (hw.distanceTo(shipRoot.position) < BOARD_L * L) { skI.kind = 'ship'; skI.label = R25.mod ? 'F · ENTER SHIP · HOLD LAUNCH' : 'F · LAUNCH'; return skI; }
        return skI;
    }
    function stationE() {
        var si = stationInteract();
        if (si.kind === 'store') openStore(si.obj);
        else if (si.kind === 'trade') openTrade(si.obj, 'STATION TRADE', 'TRADER', null);
        else if (si.kind === 'shipyard') openShipyard();
        else if (si.kind === 'loot') dkLoot(si.obj);
        else if (si.kind === 'npc') {
            var n = si.obj; n.li = ((n.li | 0) + 1) % Math.max(1, n.lines.length);
            if (r28Talk({ id: 'station-' + n.name, name: n.name, role: 'shopper', kind: 'station', obj: n, seed: n.name })) return;
            addRow(String(n.name).toUpperCase(), '#ffd36a', n.lines[n.li] || '...'); aPlay('npc', { seed: n.name }); openQuests('station-' + n.name, n.name, 'board');
        } else if (si.kind === 'map') { setPrompt('', false); exit(); }
        else if (si.kind === 'ship') { if (R25.noInt || !enterInterior()) beginLaunch(); }
    }
    var skQ = new THREE.Quaternion(), skQ2 = new THREE.Quaternion();
    function skShipPose(dt, snap, qL) {
        SITE().toWorld(skA, skB);
        if (snap) shipRoot.position.copy(skB); else shipRoot.position.lerp(skB, damp(16, dt));
        shipRoot.quaternion.copy(SITE().quat).multiply(qL);
    }
    function skCommon(dt) {      // shake / fov / camera plane / fx / hud / net: shared by every station sub-state
        shake *= Math.exp(-6 * dt);
        if (shake > 1e-4 * L) { vA.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(2 * shake); camera.position.add(vA); }
        fov += (baseFov - fov) * damp(4, dt);
        var near = gmode === 'ifoot' ? Math.min(0.002, 0.02 * L) : (gmode === 'sfoot' ? (R27.dsite ? Math.min(0.003, 0.01 * L) : 0.01 * L) : 0.02 * L);      // rev 27: 0.003 inside a derelict
        if (Math.abs(camera.fov - fov) > 0.01 || camera.near !== near) { camera.fov = fov; camera.near = near; camera.updateProjectionMatrix(); }
        camera.updateMatrixWorld(true);
        fx.setMotion(vel, 0, false); fx.update(dt, camera);
        if (net) { net.sendPos(); net.update(dt, dockA); ghostFx(); }
        scanUpdate(dt); musicTick(dt, null); updateBooms(dt); updateHud(0, aliveCount()); hudDmg(dt); updatePlayerMarks();
    }
    function stationStep(dt) {
        if (!SITE()) { skAbort(); return; }
        var m = sk.mode, u, e;
        if (m === 'docking' || m === 'launching') {
            sk.t += dt; u = clamp(sk.t / sk.T, 0, 1);
            if (m === 'docking') {
                e = u * u * (3 - 2 * u);
                skPathAt(e * sk.len, skA);
                skQ.slerpQuaternions(sk.q0, skQ2.identity(), clamp(u / 0.55, 0, 1));
                skShipPose(dt, sk.t <= dt, skQ);
                exUpdate(exMe, shipRoot.position, shipRoot.quaternion, L, 0.15 + 0.5 * (1 - e)); exMe.holder.visible = true;
                chaseTargets(dt); camera.position.copy(camPos); camera.quaternion.copy(camQuat);
                if (u >= 1) { setPrompt('', false); skLand(); }
            } else {
                e = u * u;
                skPathAt(e * sk.len, skA);
                sk.q0.identity(); skQ2.setFromAxisAngle(Y, Math.PI);
                skQ.slerpQuaternions(sk.q0, skQ2, easeInOut(clamp(u / 0.4, 0, 1)));
                skShipPose(dt, sk.t <= dt, skQ);
                exUpdate(exMe, shipRoot.position, shipRoot.quaternion, L, 0.3 + 0.7 * u); exMe.holder.visible = true;
                chaseTargets(dt); camera.position.copy(camPos); camera.quaternion.copy(camQuat);
                if (u >= 1) finishLaunch();
            }
        } else if (m === 'deck') {
            sfootStep(dt);
        }
        if (R27.dsite) dkTick(dt);
        skCommon(dt);
        if (m === 'deck' || sk.mode === 'deck') {
            if (sk.nl > 0) { sk.nl -= dt; if (sk.nl <= 0 && net && net.setMode) net.setMode('foot'); }
            var si = stationInteract(); setPrompt(si.label, !!si.label);
        }
    }
    function sfootStep(dt) {
        var H = SK_H, cmd = cmdOpen;
        if (cmd) { mdx = mdy = 0; }
        sk.yaw -= mdx * MOUSE_SENS * 1.3; sk.pitch = clamp(sk.pitch + mdy * MOUSE_SENS * 1.3, -0.3, 1.4); mdx = mdy = 0;
        var fw = cmd ? 0 : ((keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0)), sd = cmd ? 0 : ((keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0));
        var sy = Math.sin(sk.yaw), cy = Math.cos(sk.yaw);
        var mx = -sy * fw + cy * sd, mz = -cy * fw - sy * sd, ml = Math.sqrt(mx * mx + mz * mz);
        var moving = ml > 0.01, run = moving && !!(keys.ShiftLeft || keys.ShiftRight);
        sk.moving = moving; sk.running = run;
        var spd = (run ? 44 : 22) * H;
        if (moving) { mx /= ml; mz /= ml; }
        var wantJump = !cmd && !!keys.Space;
        if (!sk.air && wantJump && !sk.jumpHeld) { sk.air = true; sk.vv = 8.5 * H; }
        sk.jumpHeld = wantJump;
        var nSub = clamp(Math.ceil(spd * dt / (0.15 * H)), 1, 24), h = dt / nSub, k, p = sk.hp, ceil = SITE().deck.bounds.max.y - H;
        for (k = 0; k < nSub; k++) {
            if (moving) { p.x += mx * spd * h; p.z += mz * spd * h; }
            if (sk.air) { sk.vv -= 22 * H * h; p.y += sk.vv * h; if (p.y >= ceil) { p.y = ceil; sk.vv = Math.min(0, sk.vv); } if (p.y <= 0) { p.y = 0; sk.vv = 0; sk.air = false; } }
            skCollide(p, p.y);
        }
        // the human + its world pose
        SITE().toWorld(p, hum.w);
        hum.obj.group.position.copy(hum.w);
        skQ.setFromAxisAngle(Y, sk.yaw);
        hum.obj.group.quaternion.copy(SITE().quat).multiply(skQ);
        hum.obj.group.scale.setScalar(H * L);
        hum.obj.update(dt, { moving: moving, running: run, airborne: sk.air, speed: moving ? (run ? 1 : 0.5) : 0, facing: 0 });
        // the parked ship rides the station
        skA.copy(sk.shipL); skShipPose(dt, true, skQ2.identity());
        // camera: 6 human heights behind, never outside the hangar volume
        var b = SITE().deck.bounds, cp = Math.cos(sk.pitch), spn = Math.sin(sk.pitch);
        skA.set(p.x, p.y + 0.8 * H, p.z);
        skB.set(skA.x + sy * cp * SK_CAM, skA.y + spn * SK_CAM, skA.z + cy * cp * SK_CAM);
        skB.x = clamp(skB.x, b.min.x + 0.08, b.max.x - 0.08); skB.y = clamp(skB.y, 0.06, b.max.y - 0.1); skB.z = clamp(skB.z, b.min.z + 0.08, b.max.z - 0.1);
        SITE().toWorld(skA, skC); SITE().toWorld(skB, vTmp);
        vA.copy(Y).applyQuaternion(SITE().quat);
        mM.lookAt(vTmp, skC, vA); gQ.setFromRotationMatrix(mM);
        groundCam(dt, skC, vTmp, gQ, 16);
    }

    // ═══ rev 27: wiring (audio ambience, weather, creatures + followers, suit, convoys, derelicts, ground predators, relay profile / discoveries / events) ═══
    // Every module hook is guarded: an older ship-audio / ship-planet / ship-enemies / ship-interior simply leaves its feature off.
    var R27 = {
        su: { jetpack: 0, scanner: 0, sprint: 0, storage: 0 }, suitSig: '', suitT: 0, windOn: true, tamedKey: '', dsite: null, cvOn: false, titan: null,
        fire: false, ffCd: 0, gnd: [], gT: 0, nestT: {}, nestSpec: null, nestFor: '', ambT: 0, wxTxt: '', evT: 0, evt: null, evKey: '', sale: 0, sg: 0, ndT: 0,
        meteor: { node: null, until: 0, t: 0, list: [] }, claimed: {}, touchSig: '', intSig: '', follow: { key: '', pet: null, holder: null, pp: new THREE.Vector3(), lpp: new THREE.Vector3(), init: false, name: '', chk: 0 },
        rl: { name: '', key: '', loaded: false, pend: false, retry: 0, sig: '', lastSend: 0, bigWarned: false }, wxOn: false
    };
    function SITE() { return R27.dsite || station; }
    var fsC2 = new THREE.Vector3(), fsW = new THREE.Vector3(), qW = new THREE.Quaternion();
    var r27R = new THREE.Vector3(), r27U = new THREE.Vector3(), r27F = new THREE.Vector3(), r27G = new THREE.Vector3(), r27T = new THREE.Vector3(), r27D = new THREE.Vector3(), r27Qi = new THREE.Quaternion(), r27Fo = { r: 0, n: new THREE.Vector3() };
    var r27A = new THREE.Vector3(), r27B = new THREE.Vector3(), r27C = new THREE.Vector3();

    // ── HUD bits ──
    var elWx = document.createElement('div'); elWx.className = 'sh-wx'; hud.appendChild(elWx);
    var elEvt = document.createElement('div'); elEvt.className = 'sh-evt'; hud.appendChild(elEvt);
    var evtHide = 0;
    function evtBanner(t) { elEvt.textContent = t; elEvt.classList.add('is-on'); clearTimeout(evtHide); evtHide = setTimeout(function () { elEvt.classList.remove('is-on'); }, 9000); }

    // ── 4. suit: tiers from the profile (7/11 GEAR tab rows + crafted kits) ──
    function suitOf(pf) {
        var s = (pf && pf.suit) || {}, u = (pf && pf.upgrades) || {};
        return { jetpack: clamp(Math.max(s.jetpack | 0, u.jetpack | 0), 0, 3), scanner: clamp(s.scanner | 0, 0, 3), sprint: clamp(s.sprint | 0, 0, 3), storage: clamp(Math.max(s.storage | 0, u.cargo | 0), 0, 3) };
    }
    function suitSync() {
        var s = suitOf(curProfile()), sig = s.jetpack + '' + s.scanner + s.sprint + s.storage;
        R27.su = s;
        if (sig === R27.suitSig) return;
        R27.suitSig = sig;
        if (hum.obj && hum.obj.setSuit) hum.obj.setSuit(s);
        if (exJet) exJet.nzk = -2;      // nozzle layout may have changed: rebuild the flame set on the next burn
    }
    var SUIT_P = [140, 320, 720], SUIT_N = { jetpack: 'JETPACK', scanner: 'SCANNER', sprint: 'SPRINT BOOTS', storage: 'STORAGE PACK' }, SUIT_D = { jetpack: 'CLIMB + FLY BOOST', scanner: 'WIDER SCAN GLOW', sprint: 'RUN SPEED +10 %', storage: 'BIGGER SUIT POCKETS' };
    function suitRows(pf) {
        ['jetpack', 'scanner', 'sprint', 'storage'].forEach(function (k) {
            var t = pf.suit[k] | 0;
            if (t >= 3) shopRows.push({ k: '-', tag: 'SU', name: 'SUIT · ' + SUIT_N[k], sub: 'MAXED · TIER 3', price: null });
            else shopRows.push({ k: 'u', slot: k, tag: 'SU', name: 'SUIT · ' + SUIT_N[k] + ' T' + (t + 1), sub: SUIT_D[k], price: SUIT_P[t] });
        });
    }
    function salePrice(p) { return R27.sale > Date.now() ? 0 : p; }

    // ── 5. trade overlay (convoys via space.hail, station via interior.trade) ──
    function openTrade(trade, title, who, convoyId) {
        if (storeOpen || cmdOpen || state !== 'piloting' || !trade) return false;
        knownFor(); shopSt = null; shopTab = 0;
        shopOpenCommon({ mode: 'trade', trade: trade, cid: convoyId || '' }, title, who, convoyId ? 'Cargo is cargo. Make it quick.' : 'Prices move with the day.', null);
        return true;
    }
    function tradeFill(ctx, pf) {
        var t = ctx.trade, i;
        if (shopTab !== 2) {
            shopTab = 0;
            for (i = 0; i < t.sells.length; i++) { var s = t.sells[i]; shopRows.push({ k: 'tb', i: i, item: s.item, name: s.item.name, sub: 'x' + s.n + ' IN STOCK', price: s.price }); }
            if (!shopRows.length) shopRows.push({ k: '-', tag: '--', name: 'SOLD OUT', sub: '', price: null });
        } else {
            for (i = 0; i < pf.items.length; i++) {
                var rec = pf.items[i], io = itemOf(rec), pr = io && t.buyPrices ? t.buyPrices[rec.id] : 0;
                if (io && pr) shopRows.push({ k: 'ts', i: i, item: io, name: io.name, sub: 'x' + rec.n + ' · THEY PAY', price: pr });
            }
            if (!shopRows.length) shopRows.push({ k: '-', tag: '--', name: 'NOTHING THEY WANT', sub: (t.buys || []).length ? 'THEY WANT ' + t.buys.length + ' KINDS' : '', price: null });
        }
    }
    function tradePick(row, pf) {
        var t = shopCtx.trade;
        if (row.k === 'tb') {
            var s = t.sells[row.i]; if (!s || s.n <= 0) return;
            if ((pf.gor | 0) < s.price) { shopMsg('NOT ENOUGH gorCoin', true); aPlay('ui', { vel: 0.3, pitch: 0.6 }); return; }
            pf.gor -= s.price; giveItem(s.item, 1); s.n--; if (s.n <= 0) t.sells.splice(row.i, 1);
            aPlay('buy'); shopMsg('BOUGHT ' + s.item.name); writeSave(); shopFill(); return;
        }
        var rec = pf.items[row.i]; if (!rec) return;
        rec.n--; if (rec.n <= 0) pf.items.splice(row.i, 1);
        pf.gor = (pf.gor | 0) + row.price; aPlay('buy', { pitch: 0.8 }); shopMsg('SOLD ' + row.name + ' · +' + row.price); writeSave(); shopFill();
    }
    // convoys: HUD markers (<= 300 L), hail at <= 20 L, piracy, hostile escorts, crates
    var cvEls = [], cvNear = null, cvNearD = 1e30;
    function cvEnsure() {
        while (cvEls.length < 3) { var el = document.createElement('div'); el.className = 'sh-cvm'; el.style.display = 'none'; el.innerHTML = '<i></i><b></b>'; hud.appendChild(el); cvEls.push({ el: el, b: el.lastChild, on: false, txt: '', x: -1e9, y: -1e9, hot: false }); }
    }
    function cvTick() {
        if (!space || !space.convoys || !space.convoys.length) { R27.cvOn = false; return; }
        cvEnsure();
        var P = shipRoot.position, i, j, c, W = window.innerWidth, H = window.innerHeight, any = false;
        cvNear = null; cvNearD = 1e30;
        for (i = 0; i < cvEls.length; i++) {
            var m = cvEls[i], show = false; c = space.convoys[i];
            if (c && c.ships[0] && c.ships[0].pos.x < 1e11 && !dead) {
                var f = c.ships[0], d = f.pos.distanceTo(P) / L;
                if (d < 600) any = true;
                if (c.state === 'attacked') {          // hostile escorts: they harry the player
                    for (j = 0; j < c.ships.length; j++) {
                        var s = c.ships[j];
                        if (s.kind !== 'fighter' || !s.alive) continue;
                        var ds = s.pos.distanceTo(P) / L;
                        if (ds < 160 && gt > (s.fireT || 0)) {
                            s.fireT = gt + 1.3 + Math.random() * 1.2; r27A.subVectors(P, s.pos).normalize(); r27A.x += (Math.random() - 0.5) * 0.04; r27A.y += (Math.random() - 0.5) * 0.04; r27A.normalize();
                            fireBolt(s.pos.x, s.pos.y, s.pos.z, r27A.x, r27A.y, r27A.z, 90 * L, 2.4, true, 5, L * 2, L * 0.15);
                        }
                    }
                }
                if (d <= 20 && c.state !== 'attacked' && c.state !== 'fled' && d < cvNearD) { cvNear = c; cvNearD = d; }
                if (d < 300) {
                    vSc.copy(f.pos).project(camera);
                    if (vSc.z < 1 && vSc.z > -1 && Math.abs(vSc.x) < 1.05 && Math.abs(vSc.y) < 1.05) {
                        show = true;
                        var tx = (c.faction === 'dealers' ? 'DEALER' : 'CORP') + ' CONVOY ' + Math.round(d) + ' L' + (c.state === 'attacked' ? ' · HOSTILE' : (c === cvNear ? ' · F HAIL' : ''));
                        if (tx !== m.txt) { m.txt = tx; m.b.textContent = tx; }
                        var hot = c.state === 'attacked' || c === cvNear; if (hot !== m.hot) { m.hot = hot; m.el.classList.toggle('is-hot', hot); }
                        var x = (vSc.x * 0.5 + 0.5) * W, y = (-vSc.y * 0.5 + 0.5) * H;
                        if (Math.abs(x - m.x) > 0.5 || Math.abs(y - m.y) > 0.5) { m.x = x; m.y = y; m.el.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) translate(-50%,-50%)'; }
                    }
                }
            }
            if (show !== m.on) { m.on = show; m.el.style.display = show ? '' : 'none'; }
        }
        R27.cvOn = any;
        var cr = space.crates;
        for (i = 0; i < cr.length; i++) {          // crates: fly through them
            c = cr[i]; if (c.taken || c.pos.distanceToSquared(P) > (8 * L) * (8 * L)) continue;
            c.taken = true; var got = [];
            for (j = 0; j < c.stacks.length; j++) { var st = c.stacks[j]; giveItem(st.item, st.n); got.push(st.n + ' ' + st.item.name); }
            aPlay('buy', { pitch: 1.2 }); addRow('', '', 'CARGO CRATE · ' + got.join(', '), 'is-sys'); writeSave(); questEvent({ type: 'crate' });
        }
    }
    function cvAttackNow(c) {
        var d = space.attack(c.id), pf = ensureProfile(userName());
        pf.rep.corp = clamp(pf.rep.corp + d.corp, -99, 99); pf.rep.dealers = clamp(pf.rep.dealers + d.dealers, -99, 99);
        addRow('', '', 'PIRACY · ' + (c.faction === 'dealers' ? 'DEALERS' : 'CORP') + ' CONVOY HOSTILE · rep corp ' + pf.rep.corp + ' dealers ' + pf.rep.dealers, 'is-sys'); writeSave();
    }
    function cvBoltHit(bo, bp) {
        var cs = space.convoys, i, j, pv = bo.prev;
        for (i = 0; i < cs.length; i++) {
            var c = cs[i];
            for (j = 0; j < c.ships.length; j++) {
                var s = c.ships[j]; if (!s.alive || s.pos.x > 1e11) continue;
                var r = (s.kind === 'hauler' ? 14 : 3.5) * L;
                if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, s.pos.x, s.pos.y, s.pos.z) > r * r) continue;
                if (c.state === 'cruising' || c.state === 'hailed') cvAttackNow(c);
                if (s.hit) {
                    var left = s.hit(bo.dmg); fx.impact(bp, 0xff8a5c, 1.6); aPlay('hit', { dist: aDist(bp), vel: 0.6 });
                    if (left <= 0) { burst(s.pos, 18, 0, 18, 2); aPlay('explosion', { dist: aDist(s.pos), pitch: 1.1 }); addUnits(UNIT_KILL, s.pos, false); }
                } else { fx.impact(bp, 0xffb35c, 2); aPlay('shieldHit', { dist: aDist(bp) }); }
                return true;
            }
        }
        return false;
    }
    function cvHail() {
        if (!cvNear || dead || !space || typeof space.hail !== 'function') return false;
        var t = null; try { t = space.hail(cvNear.id); } catch (e0) { t = null; }
        if (!t) return false;
        return openTrade(t, (cvNear.faction === 'dealers' ? 'DEALER' : 'CORP') + ' CONVOY', 'HAULER', cvNear.id);
    }

    // ── 6. derelicts: the station dock sequence on an adapter that quacks like `station` (deck floor shifted to y = 0) ──
    function dkSite(d) {
        if (d._site) return d._site;
        var FY = d.crates && d.crates[0] ? d.crates[0].localPos.y : -0.5, b = d.deck.bounds, tmp = new THREE.Vector3(), V = THREE.Vector3;
        var walls = d.deck.walls.map(function (w) { return { name: w.name, field: w.field, min: new V(w.min.x, w.min.y - FY, w.min.z), max: new V(w.max.x, w.max.y - FY, w.max.z) }; });
        var pts = d.dockPath(), lastW = pts[pts.length - 1], lp = d.toLocal(lastW, new V());
        var S = {
            isDerelict: true, d: d, group: d.group, pos: d.pos, quat: d.quat, vel: new V(), mouth: d.mouth,
            pads: [{ free: true, local: new V(lp.x, lp.y - FY, lp.z) }],
            spawn: new V(lp.x, 0, lp.z - 1.5),
            deck: { bounds: { min: new V(b.min.x, b.min.y - FY, b.min.z), max: new V(b.max.x, b.max.y - FY, b.max.z) }, walls: walls },
            interior: { stores: [], npcs: [], mapPedestal: { pos: new V(1e9, 1e9, 1e9), radius: 0 }, trade: null },
            toLocal: function (wp, out) { d.toLocal(wp, out); out.y -= FY; return out; },
            toWorld: function (lv, out) { tmp.set(lv.x, lv.y + FY, lv.z); return d.toWorld(tmp, out); },
            dockPath: function () { return d.dockPath(); }, launchPath: function () { return d.launchPath(); }, setVisible: function () {}
        };
        d._site = S; return S;
    }
    function dkTrigger(P) {
        var ds = space.derelicts, i, k;
        for (i = 0; i < ds.length; i++) {
            var d = ds[i]; if (!d.group.visible || !d.mouth || !d.mouth.trigger) continue;
            for (k = 0; k <= 4; k++) { vTmp.lerpVectors(bhPrev, P, k / 4); if (d.mouth.trigger(vTmp)) { beginDock(d); return; } }
        }
    }
    function dkLeave() { R27.dsite = null; for (var i = 0; i < R27.gnd.length; i++) if (R27.gnd[i].site) R27.gnd[i].g.visible = false; R27.fire = false; }
    function dkOnDock(d) {
        if (d._gdDead) return;
        for (var i = 0; i < R27.gnd.length; i++) if (R27.gnd[i].site && R27.gnd[i].site.d === d && R27.gnd[i].alive) { R27.gnd[i].g.visible = true; return; }
        var tier = Math.max(1, (d.guardian && d.guardian.tier) | 0), seed = 0xD3AD + (d.index | 0) * 7717;
        var e = gndMake(seed, tier, null);
        if (!e) return;
        e.site = R27.dsite; e.guard = d; e.lp.copy(R27.dsite.toLocal(d.guardian.pos, r27A)); e.lp.y = 0; e.aggro = 60; e.mode = 'derelict';
        gndPlace(e);
    }
    function dkCrateNear() {
        var d = R27.dsite && R27.dsite.d, i; if (!d) return null;
        for (i = 0; i < d.crates.length; i++) { var c = d.crates[i]; if (!c.taken && c.pos.distanceToSquared(hum.w) < (0.7 * L) * (0.7 * L)) return c; }
        return null;
    }
    function dkLoot(c) {
        c.taken = true; var got = [], i;
        for (i = 0; i < c.stacks.length; i++) { giveItem(c.stacks[i].item, c.stacks[i].n); got.push(c.stacks[i].n + ' ' + c.stacks[i].item.name); }
        var bpI = r28BpDrop(c.id, c.rare ? 0.6 : 0.12); if (bpI) { giveItem(bpI, 1); got.push(bpI.name); }      // rev 28
        aPlay(c.rare ? 'levelUp' : 'harvest'); addRow('', '', 'SALVAGE · ' + got.join(', '), 'is-sys'); writeSave();
        for (i = 0; i < 3; i++) fx.impact(c.pos, c.rare ? 0xff5ce1 : 0xffb030, 2.5);
    }
    function dkTick(dt) { try { if (space) space.update(dt, shipRoot.position); } catch (e0) { /* ignore */ } if (sk.mode === 'deck') r27Foot(dt); }
    function dkCollide(p, r) {
        var st = R27.dsite, b = st.deck.bounds, w = st.deck.walls, i, q;
        p.x = clamp(p.x, b.min.x + r, b.max.x - r); p.z = clamp(p.z, b.min.z + r, b.max.z - r);
        for (i = 0; i < w.length; i++) {
            q = w[i]; if (q.name === 'mouthField') { if (p.z > q.min.z - r) p.z = q.min.z - r; continue; }
            var cx = clamp(p.x, q.min.x, q.max.x), cz = clamp(p.z, q.min.z, q.max.z), dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
            if (d2 >= r * r) continue;
            if (d2 > 1e-12) { var dd = Math.sqrt(d2), k = (r - dd) / dd; p.x += dx * k; p.z += dz * k; }
            else { var l = p.x - q.min.x, rr = q.max.x - p.x, t = p.z - q.min.z, bb = q.max.z - p.z, mn = Math.min(l, rr, t, bb); if (mn === l) p.x = q.min.x - r; else if (mn === rr) p.x = q.max.x + r; else if (mn === t) p.z = q.min.z - r; else p.z = q.max.z + r; }
        }
    }

    // ── 7. ground predators (ship-enemies generateGroundEnemy / nestSpec; guarded) ──
    var GM = 0.05;                                   // world units per metre = 0.09 L / 1.8 m, x L
    function gndMake(seed, tier, kind) {
        if (typeof ENM.generateGroundEnemy !== 'function') return null;
        var cr; try { cr = ENM.generateGroundEnemy(THREE, seed, tier, kind || undefined); } catch (e0) { console.info('[ship] ground enemy', e0); return null; }
        if (!cr || !cr.group) return null;
        var g = new THREE.Group(); g.add(cr.group); g.scale.setScalar(GM * L); g.traverse(function (o) { o.frustumCulled = false; }); scene.add(g);
        var e = { cr: cr, g: g, hp: cr.stats.hp, hpMax: cr.stats.hp, alive: true, mode: 'planet', node: null, dir: new THREE.Vector3(0, 1, 0), hr: 0, site: null, guard: null, lp: new THREE.Vector3(), face: new THREE.Vector3(0, 0, -1), w: new THREE.Vector3(), up: new THREE.Vector3(0, 1, 0),
            ph: 'idle', u: 0, t: 0, cd: 1 + Math.random(), mv: null, aggro: 30, nest: '', hitF: 0, hurtT: 0 };
        R27.gnd.push(e); return e;
    }
    function gndFree(e) { e.alive = false; scene.remove(e.g); try { e.cr.dispose(); } catch (e0) { /* ignore */ } var i = R27.gnd.indexOf(e); if (i >= 0) R27.gnd.splice(i, 1); }
    function gndClear(mode) { for (var i = R27.gnd.length - 1; i >= 0; i--) if (!mode || R27.gnd[i].mode === mode) gndFree(R27.gnd[i]); }
    function gndPlace(e) {          // world position / up from the entity's frame, then the holder pose
        if (e.mode === 'derelict') { if (!e.site) return; e.site.toWorld(e.lp, e.w); e.up.set(0, 1, 0).applyQuaternion(e.site.quat); }
        else { if (!e.node) return; e.w.copy(e.dir).multiplyScalar(e.hr).applyQuaternion(e.node.mesh.quaternion).add(e.node.anchor.position); e.up.copy(e.dir).applyQuaternion(e.node.mesh.quaternion); }
        e.face.addScaledVector(e.up, -e.face.dot(e.up)); if (e.face.lengthSq() < 1e-8) e.face.set(1, 0, 0).addScaledVector(e.up, 0); e.face.normalize();
        r27R.crossVectors(e.face, e.up); r27B.copy(e.face).negate();
        mM.makeBasis(r27R, e.up, r27B); e.g.quaternion.setFromRotationMatrix(mM); e.g.position.copy(e.w);
    }
    function gndStep(e, dt, hw) {
        var cr = e.cr, st = cr.stats, H = 0.09 * L, d = e.w.distanceTo(hw), dM = d / (GM * L), mv = e.mv, moving = false, k;
        r27C.subVectors(hw, e.w); r27C.addScaledVector(e.up, -r27C.dot(e.up)); var tl = r27C.length(); if (tl > 1e-9) r27C.divideScalar(tl);
        if (e.hurtT > 0) e.hurtT -= dt;
        if (e.ph === 'idle') {
            e.cd -= dt;
            if (dM < e.aggro && tl > 1e-9) {
                e.face.lerp(r27C, damp(6, dt)).normalize();
                var reach = cr.moves[1].reachL;
                if (dM > reach * 0.8) { gndMove(e, r27C, st.speed * H * 0.85 * dt); moving = true; }
                if (e.cd <= 0 && dM < cr.moves[0].reachL * 1.1) { e.mv = dM > reach * 1.3 ? cr.moves[0] : cr.moves[1]; e.ph = 'tele'; e.t = 0; cr.att.type = e.mv.type; cr.att.ph = 'tele'; cr.att.u = 0; }
            }
            cr.setSpeed(moving ? st.speed : 0);
            if (e.mode === 'derelict' && cr.setBurrowed) cr.setBurrowed(false);
        } else if (e.ph === 'tele') {
            e.t += dt; k = clamp(e.t / mv.tele, 0, 1); cr.att.ph = 'tele'; cr.att.u = k; cr.setWindup(k); cr.setSpeed(0);
            if (tl > 1e-9) e.face.lerp(r27C, damp(8, dt)).normalize();
            if (k >= 1) {
                e.ph = 'strike'; e.t = 0; cr.att.ph = 'strike'; cr.att.u = 0;
                if (dM < mv.reachL * 1.25 + 1.2) footHurt(mv.dmg);
            }
        } else if (e.ph === 'strike') {
            e.t += dt; k = clamp(e.t / mv.dur, 0, 1); cr.att.u = k;
            if (mv.type === 'lunge' && dM > 1.2) gndMove(e, e.face, (mv.reachL * GM * L) / mv.dur * dt * 0.8);
            if (k >= 1) { e.ph = 'rec'; e.t = 0; cr.att.ph = 'rec'; cr.att.u = 0; cr.setWindup(0); }
        } else {
            e.t += dt; k = clamp(e.t / mv.rec, 0, 1); cr.att.u = k;
            if (k >= 1) { e.ph = 'idle'; cr.att.ph = 'idle'; e.cd = mv.cd; }
        }
        if (e.hitF > 0) { e.hitF = Math.max(0, e.hitF - dt * 5); cr.setHit(e.hitF); }
    }
    function gndMove(e, v, s) {
        if (!(s > 0)) return;
        if (e.mode === 'derelict') {
            r27A.copy(e.w).addScaledVector(v, s); e.site.toLocal(r27A, r27B); e.lp.x = r27B.x; e.lp.z = r27B.z; e.lp.y = 0; dkCollide(e.lp, 0.04);
        } else {
            r27A.copy(e.w).addScaledVector(v, s).sub(e.node.anchor.position).applyQuaternion(qW.copy(e.node.mesh.quaternion).invert()).normalize();
            var fr = ps && ps.active === e.node ? mFloor(r27A.x, r27A.y, r27A.z) : e.hr;
            if (fr - e.hr < 0.03 * L + s) { e.dir.copy(r27A); e.hr = fr; }          // never up a cliff taller than ~0.7 m
        }
    }
    function gndSphere(e, out) { out.copy(e.w).addScaledVector(e.up, e.cr.stats.height * 0.5 * GM * L); return e.cr.hitR * GM * L; }
    function footHurt(dmg) {          // on-foot damage never kills: you are knocked to a sliver and the shield regrows
        hp = Math.max(6, hp - dmg); sinceHit = 0; aPlay('hit', { pitch: 0.55, vel: Math.min(1, dmg / 30) });
        shake = Math.max(shake, Math.min(0.5 * L, dmg / 20 * 0.2 * L)); fovKick = Math.max(fovKick, 1.5);
    }
    function gndKill(e) {
        for (var i = 0; i < 4; i++) fx.impact(e.w, 0xff8a5c, 3, 'explode');
        aPlay('explosion', { dist: aDist(e.w), pitch: 1.3 }); addUnits(UNIT_KILL * 2, e.w, true);
        if (e.guard) { e.guard._gdDead = true; addRow('', '', 'GUARDIAN DOWN', 'is-sys'); }
        if (e.nest) R27.nestT[e.nest] = Date.now() + (e.respawn || 120) * 1000;
        questEvent({ type: 'kill', n: 1 });
        gndFree(e); writeSave();
    }
    function footFireTick(dt) {
        if (R27.ffCd > 0) R27.ffCd -= dt;
        if (!R27.fire || R27.ffCd > 0 || cmdOpen || storeOpen || invOpen || !hum.obj || dead) return;
        var wp = curWeapon(), st = wp.stats || STARTER.stats, H = 0.09 * L;
        R27.ffCd = 1 / clamp((st.rate || 4) * 0.7, 1, 5);
        r27A.set(0, 0, -1).applyQuaternion(camera.quaternion);                           // aim ray: camera centre
        var best = null, bt = 60 * L, i, e, r, t, ocx, ocy, ocz, b, c;
        for (i = 0; i < R27.gnd.length; i++) {
            e = R27.gnd[i]; if (!e.alive || !e.g.visible) continue;
            gndPlace(e); r = gndSphere(e, r27B); ocx = r27B.x - camera.position.x; ocy = r27B.y - camera.position.y; ocz = r27B.z - camera.position.z;
            b = ocx * r27A.x + ocy * r27A.y + ocz * r27A.z; if (b < 0) continue;
            c = ocx * ocx + ocy * ocy + ocz * ocz - r * r; var disc = b * b - c; if (disc < 0) continue;
            t = b - Math.sqrt(disc); if (t < 0) t = 0;
            if (t < bt) { bt = t; best = e; }
        }
        r27R.set(1, 0, 0).applyQuaternion(hum.obj.group.quaternion); r27F.set(0, 0, -1).applyQuaternion(hum.obj.group.quaternion); r27U.set(0, 1, 0).applyQuaternion(hum.obj.group.quaternion);
        r27G.copy(hum.w).addScaledVector(r27U, 0.52 * H).addScaledVector(r27R, 0.14 * H).addScaledVector(r27F, 0.32 * H);      // the hand
        r27T.copy(camera.position).addScaledVector(r27A, best ? bt : 40 * L);
        r27D.subVectors(r27T, r27G).normalize();
        fx.spawnBolt(r27G, r27D, effColor(), 170 * L, 0.28); fx.muzzle(r27G, r27D, effColor(), 'bolt'); aPlay('fire', { vel: 0.5, pitch: 1.35 });
        if (best) {
            var dmg = (st.dmg || 8) * (st.count || 1) * 0.8;
            best.hp -= dmg; best.hitF = 1; fx.impact(r27T, 0xff8a5c, 1.4, 'hit'); aPlay('hit', { dist: aDist(r27T), vel: 0.5 });
            if (best.ph === 'idle') best.cd = Math.min(best.cd, 0.4);
            if (best.hp <= 0) gndKill(best);
        }
    }
    function biomeWord(node) {
        var u = node && node.mesh && node.mesh.material && node.mesh.material.uniforms; if (u && u.uBiome && u.uBiome.value > 0.5) return '';
        var lk = ps && ps.lookOf ? ps.lookOf(node) : 'rocky'; return lk === 'lush' ? 'forest' : (lk === 'icy' ? 'ice' : 'rock');
    }
    function nestTick(dt) {
        R27.gT -= dt; if (R27.gT > 0) return; R27.gT = 1.5;
        var node = land.node; if (!node || gmode !== 'foot' || !hum.obj || !ps || ps.active !== node || typeof ENM.nestSpec !== 'function') return;
        var pid = String(node.id), now = Date.now(), i, j, live = 0;
        if (R27.nestFor !== pid) { R27.nestFor = pid; R27.nestSpec = null; var bw = biomeWord(node); if (bw) { try { R27.nestSpec = ENM.nestSpec(hash25(pid), bw); } catch (e0) { R27.nestSpec = null; } } }
        if (!R27.nestSpec) return;
        for (i = 0; i < R27.gnd.length; i++) if (R27.gnd[i].alive && R27.gnd[i].mode === 'planet') live++;
        if (live >= 4) return;
        var R = ps.radius || node.mesh.scale.x, hd = r27A.copy(hum.pos).normalize();
        for (i = 0; i < R27.nestSpec.nests.length; i++) {
            var nd = R27.nestSpec.nests[i], key = pid + ':' + nd.id;
            r27B.set(nd.dir[0], nd.dir[1], nd.dir[2]);
            var distM = Math.acos(clamp(r27B.dot(hd), -1, 1)) * R / (GM * L);
            if (distM > nd.aggro * 3.5 || now < (R27.nestT[key] || 0)) continue;
            var mine = 0; for (j = 0; j < R27.gnd.length; j++) if (R27.gnd[j].nest === key) mine++;
            if (mine >= nd.count) continue;
            var kind = nd.kinds[Math.floor(Math.random() * nd.kinds.length)], e = gndMake((hash25(key) + (now & 0xffff)) | 0, nd.tier, kind);
            if (!e) return;
            e.mode = 'planet'; e.node = node; e.nest = key; e.respawn = nd.respawn; e.aggro = nd.aggro;
            r27C.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5); r27C.addScaledVector(r27B, -r27C.dot(r27B)).normalize();
            e.dir.copy(r27B).addScaledVector(r27C, (Math.random() * nd.radius * GM * L) / R).normalize();
            e.hr = mFloor(e.dir.x, e.dir.y, e.dir.z); e.face.crossVectors(e.dir, r27C).normalize();
            R27.nestT[key] = now + 3500; gndPlace(e);
            return;
        }
    }
    function gndTick(dt, hw) {
        for (var i = R27.gnd.length - 1; i >= 0; i--) {
            var e = R27.gnd[i];
            if (e.mode === 'planet' && (gmode !== 'foot' || !land.node || e.node !== land.node)) { gndFree(e); continue; }
            if (e.mode === 'derelict' && !(R27.dsite && R27.dsite === e.site)) { e.g.visible = false; continue; }
            e.g.visible = true; gndPlace(e); gndStep(e, dt, hw); gndPlace(e); e.cr.update(gt + R27.t, dt);
        }
    }
    function r27Foot(dt) {          // on foot on a planet, or on foot inside a derelict
        var onP = gmode === 'foot' && hum.obj, onD = !!R27.dsite && gmode === 'sfoot' && hum.obj;
        if (!onP && !onD) { R27.fire = false; return; }
        sinceHit += dt; if (sinceHit > 5 && hp < HP_MAX) hp = Math.min(HP_MAX, hp + 5 * dt);
        footFireTick(dt);
        if (onP) { nestTick(dt); petFollowTick(dt); meteorTick(dt); }
        if (R27.gnd.length) gndTick(dt, hum.w);
    }
    document.addEventListener('mousedown', function (e) {
        if (state !== 'piloting' || e.button !== 0 || cmdOpen || !locked()) return;
        if (gmode === 'foot' || (gmode === 'sfoot' && R27.dsite && sk.mode === 'deck')) R27.fire = true;
    }, true);
    document.addEventListener('mouseup', function (e) { if (e.button === 0) R27.fire = false; }, true);

    // ── 3. followers (inventory right-click a pet) + scan glow + discoveries ──
    function petStow() { var f = R27.follow; if (f.pet) { try { f.pet.dispose(); } catch (e0) { /* ignore */ } } if (f.holder) scene.remove(f.holder); f.pet = null; f.holder = null; f.key = ''; f.init = false; }
    function petFollowToggle(it) {
        var p = it.pet, f = R27.follow;
        if (f.key === it.id) { var nm = f.name; petStow(); addRow('', '', nm + ' stowed', 'is-sys'); return true; }
        if (gmode !== 'foot' || !hum.obj || !ps || !ps.active) { addRow('', '', 'call ' + p.name + ' out on foot, on a planet', 'is-sys'); return true; }
        if (!humanMod || typeof humanMod.createPet !== 'function') return false;
        petStow();
        var pet; try { pet = humanMod.createPet(THREE, { seed: p.seed, plan: p.plan }); } catch (e0) { return false; }
        pet.group.traverse(function (o) { o.frustumCulled = false; });
        var hd = new THREE.Group(); hd.add(pet.group); scene.add(hd);
        f.key = it.id; f.pet = pet; f.holder = hd; f.init = false; f.name = p.name; f.chk = 0;
        addRow('', '', p.name + ' follows you · right-click again to stow', 'is-sys'); aPlay('tame', { vel: 0.5 });
        return true;
    }
    function petFollowTick(dt) {
        var f = R27.follow; if (!f.pet) return;
        f.chk -= dt; if (f.chk <= 0) { f.chk = 1; var pf = curProfile(); if (!pf || !pf.items.some(function (r) { return r.id === f.key; })) { petStow(); return; } }
        if (!hum.obj || !ps || !ps.active) { f.holder.visible = false; return; }
        var H = 0.09 * L, hq = hum.obj.group.quaternion;
        f.holder.visible = true;
        r27R.set(1, 0, 0).applyQuaternion(hq); r27U.set(0, 1, 0).applyQuaternion(hq); r27F.set(0, 0, -1).applyQuaternion(hq);
        if (f.init) f.pp.copy(f.lpp).applyQuaternion(ps.active.mesh.quaternion).add(ps.active.anchor.position);      // the planet moves: the pet lives in its LOCAL frame between frames
        if (!f.init || f.pp.distanceToSquared(hum.w) > (10 * H) * (10 * H)) { f.pp.copy(hum.w).addScaledVector(r27R, 1.1 * H).addScaledVector(r27F, -0.9 * H); f.init = true; }
        r27G.copy(hum.w).addScaledVector(r27R, -0.9 * H).addScaledVector(r27F, -0.8 * H);
        f.holder.position.copy(f.pp); f.holder.quaternion.copy(hq); f.holder.scale.setScalar(H);
        r27T.subVectors(r27G, f.pp).applyQuaternion(r27Qi.copy(hq).invert()).divideScalar(H); r27T.y = 0;
        f.pet.group.position.set(0, 0, 0);
        f.pet.follow(r27T, dt, { dist: 0.45, rate: 3 });
        r27D.copy(f.pet.group.position).multiplyScalar(H).applyQuaternion(hq); f.pp.add(r27D); f.pet.group.position.set(0, 0, 0);
        try {
            var fo = ps.floorAt(f.pp, r27Fo);
            if (fo && fo.r > 0) {
                var c = ps.active.anchor.position; r27D.subVectors(f.pp, c).normalize(); f.pp.copy(c).addScaledVector(r27D, fo.r);
                f.holder.quaternion.premultiply(r27Qi.setFromUnitVectors(r27U, r27D));
            }
        } catch (e0) { /* ignore */ }
        f.holder.position.copy(f.pp);
        f.lpp.copy(f.pp).sub(ps.active.anchor.position).applyQuaternion(qW.copy(ps.active.mesh.quaternion).invert());
    }
    function scanGlow(dt, org, w) {
        R27.sg -= dt; if (R27.sg > 0) return; R27.sg = 0.6;
        var tier = R27.su.scanner | 0;
        try { if (ps && typeof ps.glowNear === 'function') ps.glowNear(org, (60 + 25 * tier) * L, 1.2); } catch (e0) { /* ignore */ }
        if (!land.node) return;
        var pid = String(land.node.id), pl = planetLabel(land.node), cl = creatList(), i, bd = (40 * L) * (40 * L), best = null;
        for (i = 0; i < cl.length; i++) { var d2 = cl[i].lp.distanceToSquared(hum.pos); if (d2 < bd) { bd = d2; best = cl[i]; } }
        if (best) { var kn = CR_NAMES[crKind(best)]; discClaim('creature', pid + ':' + kn, kn + ' ' + pl); }
        if (w && typeof w.scanTargets === 'function') {
            try { var tg = w.scanTargets(); for (i = 0; i < tg.length; i++) { if (tg[i].type !== 'resource') continue; locToWorld(tg[i].pos, vHl2); if (vHl2.distanceToSquared(org) < (40 * L) * (40 * L)) { discClaim('resource', pid + ':' + (tg[i].kind || 'ore'), String(tg[i].kind || 'ore').toUpperCase() + ' ' + pl); break; } } } catch (e1) { /* ignore */ }
        }
    }
    function footWind(dir, dt, hr, H, supNow) {
        if (!ps || !ps.wind || !land.node) return 0;
        var ws = ps.wind; if (ws.x * ws.x + ws.y * ws.y + ws.z * ws.z < 1e-12) return 0;
        fsW.copy(ws).applyQuaternion(qW.copy(land.node.mesh.quaternion).invert()); fsW.addScaledVector(dir, -fsW.dot(dir));
        fsC2.copy(dir).addScaledVector(fsW, 0.15 * dt / hr).normalize();
        var f = footFloor(fsC2, H); if (f - supNow > 0.1 * H) return 0;
        return f;
    }

    // ── 8. relay client: profile sync, discoveries, events ──
    function r27Touch(pf) {          // stamp savedAt only when the profile content really changed
        var sa = pf.savedAt; pf.savedAt = 0; var s = JSON.stringify(pf);
        if (s !== R27.touchSig) { R27.touchSig = s; sa = Date.now(); }
        pf.savedAt = sa;
    }
    function r27Login(name, key) {
        var r = R27.rl; r.name = String(name).toLowerCase(); r.key = key; r.loaded = false; r.pend = true; r.retry = 0; r.sig = '';
        try { sessionStorage.setItem('nmg-rk', JSON.stringify({ n: name, k: key })); } catch (e0) { /* ignore */ }
        if (net && net.online && net.profLoad) { r.pend = false; r.retry = performance.now() + 12000; net.profLoad(r.name, r.key); return 'LOADING PROFILE'; }
        return 'RELAY OFFLINE, WILL SYNC WHEN CONNECTED';
    }
    function r27NetOpen() {
        var r = R27.rl;
        if (!r.name) { try { var o = JSON.parse(sessionStorage.getItem('nmg-rk') || 'null'); if (o && o.n && o.k && String(o.n) === curUser) { r.name = String(o.n).toLowerCase(); r.key = String(o.k); r.pend = true; } } catch (e0) { /* ignore */ } }
        if (r.name && r.key && !r.loaded && net) { r.pend = false; r.retry = performance.now() + 12000; net.profLoad(r.name, r.key); }
        if (net && net.eventNow) net.eventNow();
    }
    function relayData(pf) { return JSON.parse(JSON.stringify(pf)); }
    function relaySig(pf) { var sa = pf.savedAt; pf.savedAt = 0; var s = JSON.stringify(pf); pf.savedAt = sa; return s; }
    function r27Prof(m) {
        var r = R27.rl, pf = curProfile();
        if (!r.name || !pf) return;
        if (m.loaded) {
            var srv = migrateProfile(m.data);
            if (srv.savedAt >= (pf.savedAt | 0)) { users[curUser] = srv; pf = srv; applyProfile(); suitSync(); R27.touchSig = relaySig(srv); writeSave(); addRow('', '', 'PROFILE LOADED · ' + String(r.name).toUpperCase(), 'is-sys'); }
            else { net.profSave(r.name, r.key, relayData(pf)); r.lastSend = performance.now(); addRow('', '', 'PROFILE SENT · local copy is newer', 'is-sys'); }
            r.loaded = true; r.sig = relaySig(pf); r.retry = 0; return;
        }
        if (m.ok) { r.retry = 0; return; }          // prof.ok
        if (m.why === 'none') { r.loaded = true; r.retry = 0; if (net && net.profSave) { net.profSave(r.name, r.key, relayData(pf)); r.lastSend = performance.now(); r.sig = relaySig(pf); addRow('', '', 'PROFILE REGISTERED · ' + String(r.name).toUpperCase(), 'is-sys'); } return; }
        if (m.why === 'key') { addRow('', '', 'RELAY · wrong key for ' + String(r.name).toUpperCase(), 'is-sys'); r.name = ''; r.key = ''; return; }
        if (m.why === 'rate') { r.retry = performance.now() + 11000; if (r.loaded) { r.sig = ''; r.lastSend = performance.now() - 60000 + 11000; } return; }      // the relay rate-limits load+save to 1 per 10 s: a save right after the load was dropped, so re-send in ~11 s
        if (m.why === 'big') { if (!r.bigWarned) { r.bigWarned = true; addRow('', '', 'RELAY · profile too large to sync', 'is-sys'); } return; }
        addRow('', '', 'RELAY · ' + m.why, 'is-sys');
    }
    function relayTick(dt) {
        var r = R27.rl; if (!r.name || !net || !net.online) return;
        var now = performance.now();
        if (!r.loaded) { if (r.retry && now > r.retry) { r.retry = now + 12000; net.profLoad(r.name, r.key); } return; }
        if (now - r.lastSend < 60000) return;
        var pf = curProfile(); if (!pf) return;
        var sig = relaySig(pf); if (sig === r.sig) return;
        if (sig.length > 60000) { if (!r.bigWarned) { r.bigWarned = true; addRow('', '', 'RELAY · profile too large to sync', 'is-sys'); } return; }
        r.lastSend = now; r.sig = sig; net.profSave(r.name, r.key, relayData(pf));
    }
    function discClaim(kind, id, name) {
        var key = kind + ':' + id; if (R27.claimed[key]) return; R27.claimed[key] = 1;
        var pf = ensureProfile(userName()), cid = String(id).replace(/[^A-Za-z0-9_.:-]/g, '_').slice(0, 48), nm = String(name).slice(0, 24), me = userName();
        if (!pf.disc.some(function (d) { return d.k === kind && d.id === cid; })) { pf.disc.push({ k: kind, id: cid, name: nm, by: me }); pf.disc = pf.disc.slice(-120); writeSave(); }
        if (!(net && net.online && net.discClaim && net.discClaim(kind, cid, nm, me))) addRow('', '', 'DISCOVERED · ' + nm.toUpperCase(), 'is-sys');
    }
    function discPlanet(node) { if (node) discClaim('planet', String(node.id), planetLabel(node)); }
    function r27Disc(m) {
        if (m.list) return;
        var pf = curProfile(), me = userName();
        if (m.won) {
            addRow('', '', 'DISCOVERY · ' + String(m.name).toUpperCase() + ' · first seen by ' + String(m.by).toUpperCase(), 'is-sys');
            if (pf && m.by !== me) { var d = pf.disc.filter(function (x) { return x.k === m.kind && x.id === m.id; })[0]; if (d) d.by = m.by; }
        } else if (pf) {
            var dd = pf.disc.filter(function (x) { return x.k === m.kind && x.id === m.id; })[0]; if (dd) dd.by = m.by;
            addRow('', '', String(m.name).toUpperCase() + ' was already found by ' + String(m.by).toUpperCase(), 'is-sys');
        }
    }
    function planetNodes() { var d = engine.drawOrder || [], out = [], i; for (i = 0; i < d.length; i++) { var n = d[i]; if (n && n.anchor && n.mesh && !n.isSun && n.mesh.scale && n.mesh.scale.x > 0) out.push(n); } return out; }
    function planetOfId(pid) { var a = planetNodes(); return a.length ? a[((pid | 0) % a.length + a.length) % a.length] : null; }
    var EVT_TXT = { titan: 'A TITAN IS HUNTING NEAR ', meteor: 'METEOR SHOWER OVER ', friesSale: 'BURGER HOUSE FRIES ARE FREE', blockade: 'PIRATE BLOCKADE AT THE STATION' };
    function r27Event(ev) {
        if (!EVT_TXT[ev.kind]) return;
        var key = ev.kind + ':' + Math.round((Date.now() + ev.left * 1000) / 20000);
        if (key === R27.evKey) return; R27.evKey = key;
        var pl = ev.planetId != null ? planetOfId(ev.planetId) : null, txt = EVT_TXT[ev.kind] + ((ev.kind === 'titan' || ev.kind === 'meteor') ? (pl ? planetLabel(pl).toUpperCase() : 'THE SYSTEM') : '');
        evtBanner('EVENT · ' + txt); addRow('', '', 'EVENT · ' + txt + ' · ' + Math.round(ev.minutes || ev.left / 60) + ' MIN', 'is-sys');
        R27.evt = { kind: ev.kind, until: Date.now() + Math.max(1, ev.left) * 1000, node: pl, applied: false };
        if (ev.kind === 'friesSale') R27.sale = R27.evt.until;
        if (ev.kind === 'meteor' && pl) { R27.meteor.node = pl; R27.meteor.until = Date.now() + Math.min(300, Math.max(1, ev.left)) * 1000; R27.meteor.t = 0; }
    }
    function evTick(dt) {
        var ev = R27.evt; if (!ev || ev.applied) return;
        if (Date.now() > ev.until) { R27.evt = null; return; }
        if (state !== 'piloting' || gmode !== 'fly' || dead || !space) return;
        if (ev.kind === 'titan') {
            var pl = ev.node || planetNodes()[0]; if (!pl) { ev.applied = true; return; }
            var len = TITAN_X * refR, out = r27A.copy(pl.anchor.position); if (out.lengthSq() < 1e-6) out.set(0, 0, 1); out.normalize();
            var title = 'TITAN ' + bossSeedName();
            R27.titan = { title: title, pos: new THREE.Vector3().copy(pl.anchor.position).addScaledVector(out, pl.mesh.scale.x * 1.5 + 0.7 * len + 300 * L) };
            qPush(gt + 0.5, BOSS_ROLES[(wave >> 2) % 5], BOSS_TIERS.titan, (Math.random() * 1e9) | 0, 3, title, 0, false, false);
            ev.applied = true; addRow('', '', 'TITAN SIGHTED · ' + planetLabel(pl).toUpperCase(), 'is-sys');
        } else if (ev.kind === 'blockade') {
            var cs = space.convoys, anchor = station && station.group.visible ? station.pos : shipRoot.position, i, j;
            for (i = 0; i < cs.length && i < 2; i++) {
                var c = cs[i];
                try { if (space.debugNear) space.debugNear('convoy', i, anchor, 90 + i * 25, null); } catch (e0) { /* ignore */ }
                c.state = 'attacked'; c.faction = i ? 'dealers' : 'corp';
                for (j = 0; j < c.ships.length; j++) if (c.ships[j].kind === 'fighter') { c.ships[j].alive = true; c.ships[j].hp = 60; }
            }
            ev.applied = true; addRow('', '', 'BLOCKADE · 2 convoys are hostile near the station', 'is-sys');
        } else ev.applied = true;
    }
    var meteorFx = [];
    function meteorTick(dt) {          // shard rain: glowing shards fall near you; walk into one to pick it up
        var m = R27.meteor; if (!m.node || Date.now() > m.until) { if (m.list.length) m.list.length = 0; return; }
        if (land.node !== m.node || !hum.obj) return;
        m.t -= dt; var H = 0.09 * L, i, s;
        if (m.t <= 0 && m.list.length < 6) {
            m.t = 1.6 + Math.random() * 1.4;
            r27A.set(Math.random() - 0.5, 0, Math.random() - 0.5).normalize().multiplyScalar((6 + Math.random() * 12) * H);
            r27B.copy(hum.w).add(r27A); var fo = null; try { fo = ps.floorAt(r27B, r27Fo); } catch (e0) { fo = null; }
            if (fo && fo.r > 0) { r27C.subVectors(r27B, ps.active.anchor.position).normalize(); r27B.copy(ps.active.anchor.position).addScaledVector(r27C, fo.r); m.list.push({ p: new THREE.Vector3().copy(r27B), lp: new THREE.Vector3().copy(r27B).sub(ps.active.anchor.position).applyQuaternion(qW.copy(ps.active.mesh.quaternion).invert()), t: 0 }); for (i = 0; i < 3; i++) fx.impact(r27B, 0xffd36a, 3, 'explode'); aPlay('shard', { dist: aDist(r27B) }); }
        }
        for (i = m.list.length - 1; i >= 0; i--) {
            s = m.list[i]; s.t += dt; s.p.copy(s.lp).applyQuaternion(land.node.mesh.quaternion).add(land.node.anchor.position); if (((s.t * 4) | 0) !== (((s.t - dt) * 4) | 0)) fx.flash(s.p, 0xffd36a, 1.2);
            if (s.p.distanceToSquared(hum.w) < (1.3 * H) * (1.3 * H)) { m.list.splice(i, 1); giveItem(R25_SHARD, 1); aPlay('buy', { pitch: 1.5 }); addRow('', '', 'METEOR SHARD +1', 'is-sys'); writeSave(); }
            else if (s.t > 60) m.list.splice(i, 1);
        }
    }

    // ── 9. interior: trophies from discoveries, kitchen menu from food in the inventory ──
    function r27Interior(int, pf) {
        var foods = [], i;
        for (i = 0; i < pf.items.length && foods.length < 6; i++) { var it = itemOf(pf.items[i]); if (it && (it.kind === 'fries' || it.effect || it.category === 'food')) foods.push({ name: it.name, color: it.color, count: pf.items[i].n }); }
        var sig = pf.disc.length + '|' + foods.map(function (f) { return f.name + f.count; }).join(',');
        if (sig === R27.intSig) return; R27.intSig = sig;
        try { if (int.trophies && typeof int.trophies.setDiscoveries === 'function') int.trophies.setDiscoveries(pf.disc.slice(-24).map(function (d) { return { kind: d.k, name: d.name, by: d.by }; })); } catch (e0) { console.info('[ship] trophies', e0); }
        try { if (int.kitchen && typeof int.kitchen.setMenu === 'function') int.kitchen.setMenu(foods); } catch (e1) { console.info('[ship] kitchen', e1); }
    }

    // ── 1 + 2. ambience + weather HUD, the per-frame hook ──
    var ambO = { place: 'space', weather: 'clear', biome: 'rocky', night: 0, intensity: 0.4, seed: 7 }, ambOn = false;
    function nightAt(node, p) {
        try { var u = node.mesh.material.uniforms, ld = u.uLightDir && u.uLightDir.value; if (!ld) return 0; r27A.subVectors(p, node.anchor.position).normalize(); return clamp(0.5 - 1.3 * r27A.dot(ld), 0, 1); } catch (e0) { return 0; }
    }
    function ambTick(dt) {
        var am = audio && audio.ambience;
        if (state !== 'piloting') { if (ambOn && am && am.stop) { try { am.stop(); } catch (e0) { /* ignore */ } } ambOn = false; return; }
        if (!am || typeof am.set !== 'function') return;
        R27.ambT -= dt; if (R27.ambT > 0) return; R27.ambT = 0.1;
        var o = ambO, node = ps && ps.active, depth = (node && ps.depth > 0.02) ? ps.depth : 0, place = 'space';
        if (gmode === 'ifoot' || R27.dsite) place = 'interior';
        else if (gmode === 'docking' || gmode === 'launching' || gmode === 'sfoot') place = storeOpen ? 'store' : 'station';
        else if (storeOpen) place = 'store';
        else if (depth > 0) place = (gmode === 'foot' || gmode === 'landed' || gmode === 'landing') ? 'surface' : 'atmo';
        o.place = place;
        var onPl = depth > 0 && (place === 'surface' || place === 'atmo');
        o.weather = onPl && ps.weather ? ps.weather.state : 'clear';
        o.biome = onPl ? (biomeWord(node) ? ps.lookOf(node) : 'gas') : 'rocky';
        o.night = onPl ? nightAt(node, gmode === 'foot' && hum.obj ? hum.w : shipRoot.position) : 0;
        o.intensity = clamp(0.3 + 0.4 * depth + (waveActive && gmode === 'fly' ? 0.25 : 0) + (R27.gnd.length ? 0.2 : 0), 0, 1);
        o.seed = node ? (hash25(String(node.id)) & 0x7fffffff) : 7;
        try { am.set(o); ambOn = true; } catch (e1) { /* ignore */ }
    }
    var WX_G = { wind: '≋ WIND', storm: '↯ STORM', aurora: '✦ AURORA' };
    function wxTick() {
        var node = ps && ps.active, tx = '';
        if (node && ps.depth > 0.02 && ps.weather && WX_G[ps.weather.state]) tx = WX_G[ps.weather.state];
        if (tx !== R27.wxTxt) { R27.wxTxt = tx; elWx.textContent = tx; elWx.classList.toggle('is-on', !!tx); elWx.setAttribute('data-wx', tx ? ps.weather.state : ''); }
    }
    function r27Tick(dt) {
        R27.t += dt;
        ambTick(dt);
        R27.suitT -= dt; if (R27.suitT <= 0) { R27.suitT = 0.5; suitSync(); wxTick(); }
        R27.evT -= dt; if (R27.evT <= 0) { R27.evT = 1; evTick(dt); relayTick(1); if (state !== 'piloting' && R27.gnd.length) gndClear(); if (R27.follow.pet && state !== 'piloting') petStow(); }
        if (R27.gnd.length && gmode !== 'foot' && gmode !== 'sfoot') gndClear('planet');
        r28Tick(dt);
    }

    // ─── idle loop: docked orbit, parked label ────────────────────────
    var lastT = performance.now();
    function idle() {
        requestAnimationFrame(idle);
        var now = performance.now(), dt = Math.min(0.1, (now - lastT) / 1000);
        lastT = now;
        idleBody(dt);
    }
    // stepped frames (EMGOR_GALAXY.step, throttled / hidden tabs) never get a rAF: the engine's frame hook drives the same body when rAF has stalled
    if (typeof engine.onFrame === 'function') engine.onFrame(function (a, b, dt) { var now = performance.now(); if (now - lastT > 120) idleBody(Math.min(0.1, dt || 0.05)); });
    function idleBody(dt) {
        if (state === 'piloting') return;
        clockT += dt;
        dockA += DOCK_RATE * dt;
        if (state === 'docked' || state === 'away') placeAtAnchor(dt, false);
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
    placeAtAnchor(0, true);
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
            get L() { return L; }, get galaxyScale() { return galS; },
            getState: function () { return state; },
            getMode: function () { return gmode === 'foot' ? 'foot' : ((gmode === 'fly' || gmode === 'docking' || gmode === 'launching') ? 'fly' : 'landed'); },
            stationToWorld: function (v, q) { if (!station) return false; station.toWorld(v, v); q.premultiply(station.quat); return true; },
            onProf: r27Prof, onDisc: r27Disc, onEvent: r27Event, onOpen: r27NetOpen,
            buildHuman: function (color) { return makeHuman(color); },
            getPose: function () {
                if (gmode === 'sfoot' && !R27.dsite) {        // rev 22: station frame. x/y/z are STATION-LOCAL (L units); v = -1 flags it (the relay passes v through untouched)
                    var onDeck = sk.nl <= 0, qq = skQ2.setFromAxisAngle(Y, sk.yaw);
                    if (onDeck) return { x: sk.hp.x, y: sk.hp.y, z: sk.hp.z, qx: qq.x, qy: qq.y, qz: qq.z, qw: qq.w, v: -1, st: (sk.moving ? 1 : 0) | (sk.running ? 2 : 0) | (sk.air ? 4 : 0) | (glowOn ? 8 : 0), hp: hp / HP_MAX * 100 };
                    return { x: sk.shipL.x, y: sk.shipL.y, z: sk.shipL.z, qx: 0, qy: 0, qz: 0, qw: 1, v: -1, st: 0, hp: hp / HP_MAX * 100 };
                }
                if (gmode === 'foot' && hum.obj) {
                    var hp0 = hum.obj.group.position, hq = hum.obj.group.quaternion;
                    return { x: hp0.x, y: hp0.y, z: hp0.z, qx: hq.x, qy: hq.y, qz: hq.z, qw: hq.w, v: 0, st: (hum.moving ? 1 : 0) | (hum.running ? 2 : 0) | (hum.air ? 4 : 0) | (glowOn ? 8 : 0), hp: hp / HP_MAX * 100 };
                }
                var p = shipRoot.position, q = shipRoot.quaternion;
                return { x: p.x, y: p.y, z: p.z, qx: q.x, qy: q.y, qz: q.z, qw: q.w, v: speed, st: (boostNow ? 1 : 0) | (pulse > 0.5 ? 2 : 0) | (dead ? 4 : 0) | (glowOn ? 8 : 0), hp: hp / HP_MAX * 100 };
            },
            getPrefs: function (id) { return { name: userName(id), color: effColor(), hs: hullSigNow }; },
            buildGhost: function (sig) {         // rev 12: docked ghosts show the low-LOD hull; the full hull swaps in while they fly (ghostFx)
                var g = null;
                try {
                    if (hullMod) {
                        var go = sig ? optsFromSig(sig) : null, lo = hullMod.buildHull(THREE, go ? Object.assign({ lod: 'low' }, go) : { lod: 'low' }), fu = hullMod.buildHull(THREE, go || undefined);
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
    // ═══ rev 28: shelves + shoplifting, conversations, mining, blueprints, shipyard, station board, landmarks, specials, pet joy ═══════════════
    // Every module hook is guarded: an older ship-world / ship-space / ship-station leaves its feature off.
    var R28 = { seenAt: -1e9, inStore: '', t: 0, t2: 0, siren: false, cartTxt: '', cubes: null, got: {}, gotT: 0, pilotT: 0, mineLoaded: false, mineSig: '', mineT: 0, lmSeen: {}, lmT: 0, book: false, bookSig: '', hullStats: null,
        mine: { n: 0, id: [], x: [], y: [], z: [], r: [], t: -9 } };
    var DLG = { open: false, id: '', name: '', role: '', kind: '', obj: null, seed: 0, st: null, res: null, ch: [], mood: 0, tm: 0 };
    var r28V = new THREE.Vector3(), r28Q = new THREE.Quaternion(), r28M = new THREE.Matrix4(), r28S = new THREE.Vector3(), r28E = new THREE.Euler(), r28Col = new THREE.Color();

    // ── 1. shelves, cart, checkout, cops ──
    var elCart = document.createElement('div'); elCart.className = 'sh-cart'; hud.appendChild(elCart);
    function r28ShelfFind() {
        if (gmode !== 'foot' || !hum.obj || !world || !world.node || typeof world.nearShelf !== 'function' || dead) return null;
        try { return world.nearShelf(hum.pos, 0.14 * L) || null; } catch (e0) { return null; }
    }
    function r28Grab(sh) {
        if (!sh || !world || typeof world.grabShelf !== 'function') return null;
        var it = world.grabShelf(sh.id);
        if (!it) return null;
        if (!curUser) curUser = userName();
        giveItem(it, 1); aPlay('grab'); writeSave();
        addRow('', '', 'GRABBED ' + it.name + ' · CART ' + world.cart.length + ' · ' + CRF.fmt(world.cartTotal()) + (world.heat > 0 ? ' · HEAT ' + world.heat : ''), 'is-sys');
        return it;
    }
    function r28Checkout() {
        if (!world || !world.cart || !world.cart.length) return null;
        var pf = qPf(), tot = world.cartTotal(), names = world.cart.map(function (c) { return c.item.name + ' ' + (c.item.price | 0); });
        if ((pf.gor | 0) < tot) {
            aPlay('ui', { vel: 0.3, pitch: 0.6 });
            addRow('', '', 'CART ' + CRF.fmt(tot) + ' · you have ' + CRF.fmt(pf.gor) + ' · ' + names.join(', '), 'is-err');
            return { paid: 0, total: tot, short: true };
        }
        pf.gor -= tot; world.checkout(); aPlay('checkout'); writeSave();
        addRow('', '', 'PAID ' + CRF.fmt(tot) + ' · ' + names.join(', '), 'is-sys');
        r28SirenOff();
        return { paid: tot, total: tot, short: false };
    }
    function r28SirenOff() { if (R28.siren) { R28.siren = false; aPlay('sirenStop'); } }
    function r28CopSay(m) {
        var nm = String((m.cop && m.cop.name) || 'GUARD').toUpperCase();
        addRow(nm, '#ff6a6a', m.line || '...');
        if (m.state === 'chase') { if (!R28.siren) { R28.siren = true; aPlay('siren', { level: 0.9 }); } } else aPlay('copWarn');
    }
    function r28Busted(b) {
        var pf = qPf(), i, fine = Math.min(pf.gor | 0, b.fine | 0);
        for (i = 0; i < b.items.length; i++) takeKey(CRF.stackKeyOf(b.items[i]), 1);
        pf.gor -= fine; r28SirenOff(); aPlay('copWarn'); aPlay('questFail');
        if (b.line) addRow('GUARD', '#ff6a6a', b.line);
        addRow('', '', 'BUSTED · ' + b.items.length + ' items returned · fine ' + CRF.fmt(fine), 'is-err');
        shake = Math.max(shake, 0.04 * L); writeSave();
    }
    function r28StoreAt(p) {
        if (!world || !world.stores) return null;
        for (var i = 0; i < world.stores.length; i++) {
            var s = world.stores[i], I = s.interior; if (!I || !I.toStore) continue;
            I.toStore(p, r28V); var sz = I.size || { w: 40, d: 26, h: 16 };      // tolerant: the human stands up to ~1.3 m under the slab top
            if (Math.abs(r28V.x) < sz.w / 2 + 0.5 && Math.abs(r28V.z) < sz.d / 2 + 0.5 && r28V.y > -4 && r28V.y < sz.h + 1) return s;
        }
        return null;
    }
    function r28ShopTick() {
        var s = (gmode === 'foot' && hum.obj && world && world.node) ? r28StoreAt(hum.pos) : null, id = s ? s.id : '';
        if (R28.inStore && !id && world && world.cart && world.cart.length) {                          // walked out with an unpaid cart
            var st = world.stores.filter(function (q) { return q.id === R28.inStore; })[0], sec = st && st.security;
            if (sec && sec.cop && (sec.seen || world.heat >= 1 || performance.now() - R28.seenAt < 2500)) { world.heat = Math.max(world.heat, 2.6); world.heatStore = st.id; addRow(String(sec.cop.name || 'GUARD').toUpperCase(), '#ff6a6a', 'HEY. THAT CART IS NOT PAID FOR.'); aPlay('copWarn'); }
            else { world.cart.length = 0; addRow('', '', 'walked out clean · nobody saw', 'is-sys'); }
        }
        if (id) { var st1 = world.stores.filter(function (q) { return q.id === id; })[0]; if (st1 && st1.security && st1.security.seen) R28.seenAt = performance.now(); }      // sec.seen drops the instant you step outside (sees() needs you inside), so remember it
        R28.inStore = id;
        var tx = '';
        if (world && world.cart && world.cart.length) tx = 'CART ' + world.cart.length + ' · ' + CRF.fmt(world.cartTotal()) + (world.heat > 0 ? ' · HEAT ' + world.heat : '');
        if (tx !== R28.cartTxt) { R28.cartTxt = tx; elCart.textContent = tx; elCart.classList.toggle('is-on', !!tx); elCart.classList.toggle('is-hot', !!(world && world.heat >= 2)); }
        if (R28.siren) {
            var chase = false; if (world && world.stores) for (var i = 0; i < world.stores.length; i++) if (world.stores[i].security && world.stores[i].security.state === 'chase') chase = true;
            if (!chase) r28SirenOff();
        }
    }

    // ── 2. conversations (world.converse / lingo.converse) ──
    var elDlg = document.createElement('div'); elDlg.className = 'sh-dlg';
    elDlg.innerHTML = '<div class="sd-h"><b class="sd-n"></b><span class="sd-r"></span></div><div class="sd-t"></div><div class="sd-c"></div><div class="sd-f"></div>';
    hud.appendChild(elDlg);
    var elDlgN = elDlg.querySelector('.sd-n'), elDlgR = elDlg.querySelector('.sd-r'), elDlgT = elDlg.querySelector('.sd-t'), elDlgC = elDlg.querySelector('.sd-c'), elDlgF = elDlg.querySelector('.sd-f');
    var DLG_MOOD = ['HOSTILE', 'COLD', 'NEUTRAL', 'WARM', 'FRIEND'], DLG_AROLE = { cashier: 'clerk', clerk: 'clerk', stationcop: 'cop', cop: 'cop', fries: 'chef', conspiracy: 'shopper', retired: 'pilot', dealer: 'dealer' };
    function dlgPf() { var pf = qPf(); if (!pf.moods || typeof pf.moods !== 'object') pf.moods = {}; return pf; }
    function dlgConverse(st) {
        var res = null;
        if (DLG.kind !== 'station' && world && typeof world.converse === 'function') { try { res = world.converse(DLG.id, st); } catch (e0) { res = null; } }
        if (!res) res = LNG.converse({ seed: DLG.seed, role: DLG.role, name: DLG.name, known: knownFor(), mood: DLG.mood }, st);
        return res;
    }
    function dlgMoodSet(delta) {
        var pf = dlgPf();
        if (DLG.kind !== 'station' && world && typeof world.npcMood === 'function') { DLG.mood = world.npcMood(DLG.id, delta | 0); }
        else DLG.mood = clamp(DLG.mood + (delta | 0), -2, 2);
        pf.moods[DLG.id] = DLG.mood;
        var ks = Object.keys(pf.moods); if (ks.length > 80) delete pf.moods[ks[0]];
    }
    function dlgApply(e) {
        if (!e) return;
        var pf = dlgPf(), ks = knownFor(), i;
        if (e.learn && e.learn.length) {
            for (i = 0; i < e.learn.length; i++) if (!ks.has(e.learn[i])) { ks.add(e.learn[i]); addRow('', '', 'learned: ' + e.learn[i], 'is-sys'); }
            pf.known = Array.from(ks);
        } else if (e.teach > 0) learnWords(e.teach | 0);
        if (e.gor) { pf.gor = (pf.gor | 0) + (e.gor | 0); aPlay('buy', { pitch: 1.3 }); addRow('', '', (e.gor > 0 ? '+' : '') + e.gor + ' gorCoin', 'is-sys'); }
        if (e.mood) dlgMoodSet(e.mood);
        if (e.craft) addRow('', '', 'HINT · try crafting ' + e.craft, 'is-sys');
        writeSave();
    }
    function dlgExtras(res) {
        var ex = [], e = res.effects || {}, k = DLG.kind;
        if ((k === 'clerk' || k === 'dealer') && (e.item || (DLG.st.steps | 0) <= 1)) ex.push({ extra: 'shop', label: k === 'dealer' ? '[ BROWSE THEIR STOCK ]' : '[ OPEN THE SHOP ]' });
        if (e.quest) ex.push({ extra: 'quest', label: '[ SEE THEIR QUESTS ]' });
        return ex;
    }
    function dlgRender() {
        var d = DLG, r = d.res, i, sp;
        elDlgN.textContent = String(d.name).toUpperCase(); elDlgR.textContent = String(d.role).toUpperCase() + ' · ' + DLG_MOOD[clamp(d.mood + 2, 0, 4)];
        while (elDlgT.firstChild) elDlgT.removeChild(elDlgT.firstChild);
        var segs = (r.segments && r.segments.length) ? r.segments : [{ text: LNG.strip(r.text || '') }];
        for (i = 0; i < segs.length; i++) { sp = document.createElement('span'); if (segs[i].alien) sp.className = 'sc-al'; sp.textContent = segs[i].text; elDlgT.appendChild(sp); }
        while (elDlgC.firstChild) elDlgC.removeChild(elDlgC.firstChild);
        for (i = 0; i < d.ch.length; i++) {
            var b = document.createElement('div'); b.className = 'sd-b' + (d.ch[i].extra ? ' is-x' : ''); b.setAttribute('data-i', String(i));
            b.innerHTML = '<i>' + (i + 1) + '</i><span></span>'; b.lastChild.textContent = d.ch[i].label; elDlgC.appendChild(b);
        }
        elDlgF.textContent = r.done ? 'ANY KEY CLOSE' : '1-' + Math.max(1, d.ch.length) + ' CHOOSE · ESC LEAVE';
    }
    function dlgGo(next) {
        var st = DLG.st; if (next) st.next = next; else delete st.next;
        var res = dlgConverse(st);
        if (!res) { closeStore(); return; }
        DLG.res = res; DLG.mood = clamp(st.mood != null ? st.mood : DLG.mood, -2, 2);
        dlgApply(res.effects);
        DLG.ch = (res.choices || []).slice(0, 3).map(function (c) { return { label: c.label, next: c.next }; });
        if (!res.done) DLG.ch = DLG.ch.concat(dlgExtras(res));
        dlgRender();
        addRow(String(DLG.name).toUpperCase(), '#ffd36a', res.text || '...');
        aPlay('talk', { role: DLG_AROLE[DLG.role] || DLG.role, seed: DLG.id + ':' + (st.steps | 0) });
        if (res.done) { clearTimeout(DLG.tm); DLG.tm = setTimeout(function () { if (DLG.open) closeStore(); }, 2400); }
    }
    function r28Talk(sp) {
        if (DLG.open || qOpen || storeOpen || invOpen || cmdOpen || state !== 'piloting' || !sp) return false;
        var pf = dlgPf();
        knownFor();
        DLG.id = String(sp.id); DLG.name = String(sp.name || 'NPC'); DLG.role = String(sp.role || 'shopper'); DLG.kind = sp.kind || 'shopper'; DLG.obj = sp.obj || null; DLG.seed = sp.seed != null ? sp.seed : DLG.id;
        var stored = pf.moods[DLG.id];
        if (DLG.kind !== 'station' && world && typeof world.npcMood === 'function') {
            var cur = world.npcMood(DLG.id, 0); if (stored != null && stored !== cur) world.npcMood(DLG.id, stored - cur); DLG.mood = world.npcMood(DLG.id, 0);
        } else DLG.mood = stored != null ? clamp(stored, -2, 2) : 0;
        DLG.st = { mood: DLG.mood }; DLG.res = null; DLG.ch = [];
        DLG.open = true; storeOpen = true; cmdOpen = true; cmdGuardUntil = performance.now() + 600;
        keys = Object.create(null); firing = false; mdx = mdy = 0; eh.on = false; eh.t = 0; ehShow(0, '');
        if (locked()) { try { document.exitPointerLock(); } catch (e0) { /* ignore */ } }
        hud.classList.add('is-dlg'); elDlg.classList.add('is-on');
        aPlay('ui', { vel: 0.6 });
        dlgGo(null);
        return true;
    }
    function dlgHide() { DLG.open = false; clearTimeout(DLG.tm); hud.classList.remove('is-dlg'); elDlg.classList.remove('is-on'); }
    function dlgPick(i) {
        var c = DLG.ch[i], d = DLG, obj = d.obj, kind = d.kind, id = d.id, nm = d.name;
        if (!DLG.open || !DLG.res) return;
        if (DLG.res.done || !c) { closeStore(); return; }
        aPlay('ui', { vel: 0.5 });
        if (c.extra === 'shop') { closeStore(); if (kind === 'dealer') openDealer(obj); else if (obj) openStore(obj); return; }
        if (c.extra === 'quest') { closeStore(); openQuests(id, nm, kind === 'dealer' ? 'dealer' : (kind === 'clerk' ? 'clerk' : 'npc')); return; }
        dlgGo(c.next);
    }
    function dlgKey(e) {
        var c = e.code; e.preventDefault(); e.stopPropagation();
        if (c === 'Escape') { e.__shipHandled = true; closeStore(); return; }
        if (e.repeat) return;
        if (DLG.res && DLG.res.done) { closeStore(); return; }
        var m = /^(?:Digit|Numpad)([1-4])$/.exec(c);
        if (m && +m[1] <= DLG.ch.length) { dlgPick(+m[1] - 1); return; }
        if (c === 'KeyF' || c === 'KeyE' || c === 'Backspace') closeStore();
    }
    elDlgC.addEventListener('click', function (e) { var t = e.target; while (t && t !== elDlgC && !(t.getAttribute && t.getAttribute('data-i'))) t = t.parentNode; if (t && t !== elDlgC) dlgPick(+t.getAttribute('data-i')); });
    elDlg.addEventListener('click', function (e) { if (DLG.res && DLG.res.done) closeStore(); });

    // ── 3. mining (space.minable / hitRock / chunks) ──
    function r28Cubes() {
        if (R28.cubes && R28.cubesFor === space) return R28.cubes;
        if (!space || !space.group) return null;
        var geo = new THREE.BoxGeometry(1, 1, 1), mat = new THREE.MeshBasicMaterial({ color: 0xffffff, toneMapped: false, fog: false });
        var m = new THREE.InstancedMesh(geo, mat, 64); m.frustumCulled = false; m.count = 0; m.renderOrder = 3;
        m.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        m.setColorAt(0, r28Col.setRGB(1, 1, 1));
        space.group.add(m); R28.cubes = m; R28.cubesFor = space;
        return m;
    }
    function r28MineList() {
        var mi = R28.mine, now = performance.now();
        if (now - mi.t < 250) return mi;
        mi.t = now; mi.n = 0;
        var list = space.minable();
        for (var i = 0; i < list.length && i < 64; i++) { var o = list[i]; mi.id[i] = o.id; mi.x[i] = o.pos.x; mi.y[i] = o.pos.y; mi.z[i] = o.pos.z; mi.r[i] = o.r; mi.n++; }
        return mi;
    }
    function r28MineBolt(bo, bp, pv) {
        if (!space || typeof space.minable !== 'function' || typeof space.hitRock !== 'function') return false;
        var mi = r28MineList(), i;
        for (i = 0; i < mi.n; i++) {
            var rr = mi.r[i] * 0.9;
            if (segDistSq(pv.x, pv.y, pv.z, bp.x, bp.y, bp.z, mi.x[i], mi.y[i], mi.z[i]) < rr * rr) {
                var res = space.hitRock(mi.id[i], bo.dmg || 10);
                fx.impact(bp, res.broken ? 0xffc46b : 0xb8aa98, res.broken ? 2 : 1);
                if (res.broken) {
                    aPlay('rockCrack', { size: clamp(mi.r[i] / L / 12, 0.4, 2.5), dist: aDist(bp), seed: mi.id[i] });
                    mi.t = -9; R28.mineSig = ''; r28V.set(mi.x[i], mi.y[i], mi.z[i]); fx.flash(r28V, 0xffc46b);
                } else aPlay('hit', { pitch: 0.45, vel: 0.35, dist: aDist(bp) });
                return true;
            }
        }
        return false;
    }
    function r28ChunkTick(dt) {
        if (!space || !space.chunks) return;
        var ch = space.chunks, n = ch.length, i, c, cen = space.rocks && space.rocks.center, mesh = R28.cubes && R28.cubesFor === space ? R28.cubes : (n ? r28Cubes() : null);
        if (mesh) {
            var k = 0;
            for (i = 0; i < n && k < 64; i++) {
                c = ch[i]; var f = c.age > 15 ? Math.max(0.05, (20 - c.age) / 5) : 1, sz = 0.55 * L * f * (1 + 0.15 * Math.sin(c.age * 6 + i));
                r28E.set(c.age * 1.7 + i, c.age * 2.3, i * 0.7); r28Q.setFromEuler(r28E);
                r28V.set(c.pos.x - (cen ? cen.x : 0), c.pos.y - (cen ? cen.y : 0), c.pos.z - (cen ? cen.z : 0)); r28S.set(sz, sz, sz);
                r28M.compose(r28V, r28Q, r28S); mesh.setMatrixAt(k, r28M);
                r28Col.setHex(c.item.color); r28Col.multiplyScalar(1.9 * f); mesh.setColorAt(k, r28Col); k++;
            }
            mesh.count = k; mesh.visible = k > 0; mesh.instanceMatrix.needsUpdate = true; if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
        }
        if (!n || gmode !== 'fly' || dead || state !== 'piloting') return;
        var P = shipRoot.position, take = null;
        for (i = 0; i < n; i++) { c = ch[i]; if (c.pos.distanceToSquared(P) < (4 * L) * (4 * L)) { (take || (take = [])).push(c.id); } }
        if (!take) return;
        if (!curUser) curUser = userName();
        for (i = 0; i < take.length; i++) {
            var got = space.takeChunk(take[i]); if (!got) continue;
            giveItem(got.item, got.n | 1); aPlay('chunk', { n: Math.min(11, (R28.gotN = ((R28.gotN | 0) + 1) % 12)) });
            R28.got[got.item.name] = (R28.got[got.item.name] || 0) + (got.n | 1); R28.gotT = 0.9;
        }
    }
    function r28MineSave(dt) {
        if (!space || typeof space.markMined !== 'function' || state !== 'piloting') { R28.pilotT = 0; return; }
        var pf = curProfile(); if (!pf) return;
        R28.pilotT += dt;
        if (!R28.mineLoaded) { if (R28.pilotT > 3) { R28.mineLoaded = true; try { space.markMined(pf.mined || []); } catch (e0) { /* ignore */ } } return; }
        R28.mineT -= dt; if (R28.mineT > 0) return; R28.mineT = 4;
        var st = space.mineState(), sig = st.length + ':' + (st.length ? st[st.length - 1][0] : 0);
        if (sig === R28.mineSig) return;
        R28.mineSig = sig; pf.mined = st.slice(-200); writeSave();
    }

    // ── 4. blueprints + recipe book ──
    function r28Known() { var pf = curProfile(), out = []; if (pf) pf.items.forEach(function (r) { var it = itemOf(r); if (it) out.push(it); }); return CRF.knownFrom ? CRF.knownFrom(out) : new Set(); }
    function r28CraftCtx() { return { known: r28Known() }; }
    function r28BpGiven(it) {
        if (!it || !it.tags || it.tags.indexOf('bp') < 0 || !CRF.knownFrom) return;
        addRow('', '', 'BLUEPRINT · ' + it.name + ' · recipe unlocked (E > RECIPE BOOK)', 'is-sys'); aPlay('blueprint');
    }
    function r28BpDrop(seed, chance) {
        if (!CRF.BLUEPRINTS || !CRF.BLUEPRINTS.length || Math.random() > chance) return null;
        var known = r28Known(), pool = CRF.BLUEPRINTS.filter(function (b) { return !known.has(b.id); }), b = (pool.length ? pool : CRF.BLUEPRINTS)[((hash25(seed) + Math.floor(Math.random() * 7)) >>> 0) % (pool.length || CRF.BLUEPRINTS.length)];
        return CRF.blueprintItem(b.id);
    }
    var elBookTabs = elInv.querySelector('.si-tabs'), elBook = elInv.querySelector('.si-book');
    function r28BookSet(on) {
        R28.book = !!on; elInv.classList.toggle('is-book', R28.book);
        for (var i = 0; i < elBookTabs.children.length; i++) elBookTabs.children[i].classList.toggle('is-on', (i === 1) === R28.book);
        R28.bookSig = ''; if (R28.book) r28BookFill();
        aPlay('ui', { vel: 0.5 });
    }
    function r28BookFill() {
        if (!R28.book) return;
        var book = []; try { book = CRF.recipeBook(r28CraftCtx()); } catch (e0) { book = []; }
        var sig = book.map(function (b) { return b.id + (b.locked ? 'L' : ''); }).join(',');
        if (sig === R28.bookSig) return; R28.bookSig = sig;
        book = book.slice().sort(function (a, b) { return (a.locked ? 1 : 0) - (b.locked ? 1 : 0); });
        var h = '', un = 0;
        function pretty(t) { return String(t).replace(/^(part|res|base|out|food|infusion|crafted):/, '').replace(/-/g, ' '); }
        for (var i = 0; i < book.length; i++) {
            var b = book[i], ing = b.ingredients.map(function (x) { return (x.n > 1 ? x.n + 'x ' : '') + pretty(x.token); }).join(' + '); if (!b.locked) un++;
            h += '<div class="sb-r' + (b.locked ? ' is-lock' : '') + '"><b>' + qEsc(b.name) + '</b><em>' + (b.locked ? 'LOCKED · find the blueprint' : qEsc(b.output.name) + (b.output.count > 1 ? ' x' + b.output.count : '')) + '</em><span>' + (b.locked ? '??? + ???' : qEsc(ing)) + (b.shapeless ? '' : ' · shaped') + '</span></div>';
        }
        elBook.innerHTML = '<div class="sb-h">' + un + ' / ' + book.length + ' RECIPES KNOWN</div>' + h;
    }
    elBookTabs.addEventListener('click', function (e) { var i = Array.prototype.indexOf.call(elBookTabs.children, e.target); if (i >= 0) r28BookSet(i === 1); });

    // ── 5. shipyard (hull kinds) + /ship ──
    var R28_HULLS = { hauler: { name: 'Hauler', stats: {} }, fighter: { name: 'Wasp-class Fighter', stats: { cruise: 25, boost: 40, cargo: -50, guns: 2, hp: -20 } }, explorer: { name: 'Drifter Explorer', stats: { cruise: 10, boost: 20, cargo: -25, guns: 0, hp: 15 } } };
    function r28YardShips() { var y = station && station.interior && station.interior.shipyard; return (y && y.ships) || []; }
    function r28HullStats(kind) {
        var s = r28YardShips().filter(function (q) { return q.kind === kind; })[0];
        return (s && s.stats) || (R28_HULLS[kind] && R28_HULLS[kind].stats) || null;
    }
    function r28HullName(kind) { var s = r28YardShips().filter(function (q) { return q.kind === kind; })[0]; return (s && s.name) || (R28_HULLS[kind] && R28_HULLS[kind].name) || String(kind); }
    function r28Hulls(pf) { var h = pf.hulls; if (!Array.isArray(h) || !h.length) h = pf.hulls = ['hauler']; if (h.indexOf(pf.hull) < 0) h.push(pf.hull); return h; }
    function r28SetHull(pf, kind) { pf.hull = kind; applyUpgrades(); hullChk = 0; writeSave(); }
    function r28StatsLine(s) { var o = []; if (!s) return 'stock'; if (s.cruise) o.push('cruise ' + (s.cruise > 0 ? '+' : '') + s.cruise + '%'); if (s.boost) o.push('boost ' + (s.boost > 0 ? '+' : '') + s.boost + '%'); if (s.hp) o.push('hull ' + (s.hp > 0 ? '+' : '') + s.hp + '%'); if (s.cargo) o.push('cargo ' + (s.cargo > 0 ? '+' : '') + s.cargo + '%'); if (s.guns) o.push('+' + s.guns + ' guns'); return o.join(' · ') || 'stock'; }
    function r28Buy(kind) {
        var pf = qPf(), sh = r28YardShips().filter(function (q) { return q.kind === kind; })[0], own = r28Hulls(pf);
        if (own.indexOf(kind) >= 0) { r28SetHull(pf, kind); aPlay('shipBuy', { pitch: 0.9 }); addRow('', '', 'HULL · ' + r28HullName(kind).toUpperCase() + ' active', 'is-sys'); return true; }
        if (!sh) return false;
        if ((pf.gor | 0) < sh.price) { aPlay('ui', { vel: 0.3, pitch: 0.6 }); addRow('', '', 'NOT ENOUGH gorCoin · ' + CRF.fmt(sh.price), 'is-err'); return false; }
        pf.gor -= sh.price; own.push(kind); r28SetHull(pf, kind); aPlay('shipBuy');
        addRow('', '', 'BOUGHT ' + sh.name.toUpperCase() + ' · ' + r28StatsLine(sh.stats), 'is-sys');
        return true;
    }
    function openShipyard() {
        if (menuOpen || cmdOpen || state !== 'piloting') return false;
        openMenu({ build: function () {
            var pf = qPf(), own = r28Hulls(pf), rows = [];
            rows.push({ name: 'HAULER' + (pf.hull === 'hauler' ? ' · ACTIVE' : ''), sub: 'stock · your starting hull', fn: function () { r28Buy('hauler'); return false; } });
            r28YardShips().forEach(function (s) {
                var has = own.indexOf(s.kind) >= 0;
                rows.push({ name: s.name.toUpperCase() + (pf.hull === s.kind ? ' · ACTIVE' : ''), sub: (has ? 'OWNED · switch · ' : s.price + ' gorCoin · ') + r28StatsLine(s.stats), fn: function () { r28Buy(s.kind); return false; } });
            });
            return { title: 'SHIPYARD · ' + CRF.fmt(pf.gor), rows: rows };
        } });
        return true;
    }
    function r28ShipCmd(arg) {
        var pf = qPf(), own = r28Hulls(pf), a = String(arg || '').trim().toLowerCase(), i;
        if (!a) { own.forEach(function (k, j) { showCmdRes((pf.hull === k ? '> ' : '  ') + (j + 1) + ' ' + k + ' · ' + r28HullName(k) + ' · ' + r28StatsLine(r28HullStats(k)), false); }); return 'SHIPS'; }
        var kind = /^\d+$/.test(a) ? own[parseInt(a, 10) - 1] : own.filter(function (k) { return k === a; })[0];
        if (!kind) { showCmdRes('NOT OWNED · /ship (buy hulls at the station shipyard)', true); return 'NO SHIP'; }
        r28SetHull(pf, kind); showCmdRes('HULL ' + kind.toUpperCase(), false); return 'HULL ' + kind;
    }

    // ── 6. station board + greeting ──
    var R28_QK = { kill: 'bounty', harvest: 'mine', deliver: 'trade', scan: 'scout', retrieve: 'mine', escort: 'trade' };
    function r28Dock() {
        var site = SITE(); if (!site || !site.interior || R27.dsite) return;
        try {
            var qb = site.interior.questBoard;
            if (qb && qb.setQuests) { var offers = QST.offerSet('board|' + qDay(), questCtx('board'), 3); qb.setQuests(offers.map(function (q) { return { title: q.title, reward: (q.reward && q.reward.gor) | 0, kind: R28_QK[q.kind] || 'bounty' }; })); }
            if (site.interior.greetingLine) addRow('STATION', '#8fa0ff', site.interior.greetingLine(qDay()));
        } catch (e0) { console.info('[ship] r28Dock', e0); }
    }

    // ── 7. landmarks ──
    function r28LmTick() {
        if (!ps || !ps.active || typeof ps.landmarks !== 'function' || state !== 'piloting' || dead || boarding || exiting) return;
        if (gmode !== 'foot' && gmode !== 'fly' && gmode !== 'landed') return;
        var org = (gmode === 'foot' && hum.obj) ? hum.w : shipRoot.position, list = ps.landmarks(), pf = curProfile(), i;
        if (!pf) return;
        for (i = 0; i < list.length; i++) {
            var lm = list[i], key = 'lm-' + lm.id;
            if (R28.lmSeen[key] || lm.pos.distanceTo(org) > 15 * L) continue;
            R28.lmSeen[key] = 1;
            var cid = String(key).replace(/[^A-Za-z0-9_.:-]/g, '_').slice(0, 48);
            if (pf.disc.some(function (d) { return d.k === 'resource' && d.id === cid; })) continue;
            evtBanner('LANDMARK DISCOVERED: ' + String(lm.name).toUpperCase()); addRow('', '', 'LANDMARK DISCOVERED: ' + String(lm.name).toUpperCase(), 'is-sys');
            aPlay('landmark'); discClaim('resource', key, lm.name);
        }
    }

    // ── master tick (called from r27Tick every frame) ──
    function r28Tick(dt) {
        if (space) r28ChunkTick(dt);
        if (R28.gotT > 0) { R28.gotT -= dt; if (R28.gotT <= 0) { var ks = Object.keys(R28.got); if (ks.length) addRow('', '', 'MINED · ' + ks.map(function (k) { return '+' + R28.got[k] + ' ' + k; }).join(', '), 'is-sys'); R28.got = {}; writeSave(); } }
        r28MineSave(dt);
        R28.t -= dt; if (R28.t <= 0) { R28.t = 0.25; r28ShopTick(); if (invOpen && R28.book) r28BookFill(); }
        R28.lmT -= dt; if (R28.lmT <= 0) { R28.lmT = 0.6; r28LmTick(); }
    }

    var api = {
        rev27: {          // test hooks
            get R() { return R27; }, get sk() { return sk; }, get space() { return space; }, get hum() { return hum; }, get gnd() { return R27.gnd; }, get L() { return L; }, get gmode() { return gmode; }, get hp() { return hp; },
            hail: cvHail, get cvNear() { return cvNear; }, openTrade: openTrade, tradeFill: tradeFill, discClaim: discClaim, event: r27Event, suitOf: suitOf, suitSync: suitSync, petToggle: petFollowToggle, creatList: creatList, flushCreat: function () { crCache.t = -1e9; }, tameFind: tameFind,
            beginDock: beginDock, dkSite: dkSite, gndMove: gndMove, gndPlace: gndPlace, gndStep: gndStep, spawnGround: function (kind, tier) {
                var e = gndMake((Math.random() * 1e9) | 0, tier || 1, kind || undefined); if (!e) return null;
                if (R27.dsite) { e.mode = 'derelict'; e.site = R27.dsite; e.lp.copy(sk.hp); e.lp.z -= 4; e.lp.y = 0; }
                else { e.mode = 'planet'; e.node = land.node; e.dir.copy(hum.pos).normalize(); r27C.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5); r27C.addScaledVector(e.dir, -r27C.dot(e.dir)).normalize(); e.dir.addScaledVector(r27C, 8 * 0.09 * L / (ps.radius || 1)).normalize(); e.hr = mFloor(e.dir.x, e.dir.y, e.dir.z); }
                e.aggro = 40; gndPlace(e); return e;
            },
            footFire: function (on) { R27.fire = !!on; }, windOff: function (b) { R27.windOn = !b; }, wind: function () { return ps && ps.wind ? ps.wind.clone() : null; }
        },
        rev25: {
            set noInterior(b) { R25.noInt = !!b; },
            get snap() { return resumeSnap; }, get lfState() { return { on: lf.on, alt: lf.alt, hover: lf.hover }; },
            get ix() { return ix.on; }, get int() { return R25.int; }, get craftOn() { return R25.craftOn; }, get grid() { return craftGrid; },
            enterInterior: enterInterior, leaveInterior: leaveInterior, interiorF: interiorF, ixTarget: ixTarget, forceCraft: function (b) { R25.force = !!b; r25CraftAvail(); if (invOpen) invFill(); },
            give: function (it, n) { giveItem(it, n || 1); writeSave(); return ensureProfile(userName()).items.length; }, openInv: openInv,
            fastForward: function () { var pf = curProfile(); pf.board.list.forEach(function (m) { if (m.end > 0) m.end = Date.now() - 1; }); missionTick(); return pf.gor; },
            assign: function (i) { var pf = curProfile(), b = r25Board(), m = b.list[i | 0], c = pf.crew.filter(function (x) { return !x.busy; })[0]; if (!m || !c) return false; m.crewId = c.id; m.start = Date.now(); m.end = m.start + m.minutes * 60000; c.busy = m.id; boardSync(); writeSave(); return true; },
            petItem: petItem, hire: function (c) { R25.cand = c; r25Hire(); }, tameFind: tameFind, tameStart: tameStart, get tame() { return tame; }, scrap: r25Scrap, ik: ik,
        },
        rev28: {
            get R() { return R28; }, get dlg() { return { open: DLG.open, id: DLG.id, name: DLG.name, text: DLG.res ? DLG.res.text : '', choices: DLG.ch.map(function (c) { return c.label; }), done: !!(DLG.res && DLG.res.done), mood: DLG.mood }; },
            shelfFind: r28ShelfFind, grab: r28Grab, checkout: r28Checkout, talk: r28Talk, pick: dlgPick, mineList: function () { return r28MineList(); }, mineBolt: r28MineBolt, chunkTick: r28ChunkTick, bookSet: r28BookSet,
            buy: r28Buy, hulls: function () { return r28Hulls(qPf()).slice(); }, shipCmd: r28ShipCmd, openShipyard: openShipyard, dock: r28Dock, lmTick: function () { R28.lmT = 0; r28LmTick(); }, craftCtx: r28CraftCtx, bpDrop: r28BpDrop,
            get pf() { return qPf(); }, busted: r28Busted, copSay: r28CopSay, shopTick: r28ShopTick, storeAt: r28StoreAt
        },
        enemies: enemies, shipRoot: shipRoot,
        destroy: function () { if (net) { net.destroy(); net = null; } },
        enter: enter, exit: exit, cmd: runCommand,
        get cmdOpen() { return cmdOpen; },
        get playerFired() { return playerFired; },
        allies: allies, orbs: orbs,
        get boss() { for (var i = 0; i < enemies.length; i++) if (enemies[i].alive && enemies[i].isBoss) return enemies[i]; return null; },
        get bosses() { return enemies.filter(function (e) { return e.alive && e.isBoss; }); },
        forceSig: function (i, now) { for (var k = 0; k < enemies.length; k++) if (enemies[k].alive && enemies[k].isBoss) { enemies[k].nextAtk = clamp(i | 0, 0, 7); if (now) enemies[k].sigCd = 0; } },     // debug: next boss signature attack (0-4 all bosses, 5-7 titan)
        get sig() { return { name: ATK_NAME[bsig.idx], idx: bsig.idx, ph: bsig.ph, t: bsig.t, owner: bsig.owner ? bsig.owner.title : '', hit: bsig.hit, hits: bsig.hits | 0, lastDmg: bsig.lastDmg }; },
        get lastRam() { return lastRam; },
        get lastHurt() { return lastHurt; },
        get edgeR() { return EDGE_R; },
        get hpMax() { return HP_MAX; }, get engMul() { return engMul; },
        get world() { return world; }, get space() { return space; }, get station() { return station; },
        get sk() { return { mode: sk.mode, pad: sk.pad, t: sk.t, T: sk.T, len: sk.len, hp: sk.hp.toArray(), shipL: sk.shipL.toArray(), nl: sk.nl, cd: sk.cd, yaw: sk.yaw }; },
        get store() { return { open: storeOpen, rows: shopRows.map(function (r) { return r.name + ' ' + r.price; }), sell: shopSell }; },
        skPlace: function (x, z, yaw) { sk.hp.x = x; sk.hp.z = z; if (yaw !== undefined) sk.yaw = yaw; },
        resetUpgrades: function () { var pf = ensureProfile(userName()); pf.shieldTier = 0; pf.engineTier = 0; pf.weapon = null; applyUpgrades(); writeSave(); return [HP_MAX, engMul]; },
        openStore: openStore, closeStore: closeStore, openDealer: openDealer, openBurger: openBurger, openInv: openInv, closeInv: closeInv, toggleInv: toggleInv, eatSlot: eatSlot, giveItem: giveItem, itemOf: itemOf, fxStart: fxStart, fxReset: fxReset, footTarget: footTarget, shopTab: function (i) { shopSetTab(i); }, get shopRows() { return shopRows; }, get shopTopRow() { return shopTop; }, get fxActive() { return fxAct; }, get fxMods() { return { speed: fxSpeed, jump: fxJump, wob: fxWob }; }, get glowOn() { return glowOn; }, get invIsOpen() { return invOpen; }, get storeIsOpen() { return storeOpen; }, learnWords: learnWords, knownFor: knownFor, shopPick: shopPick, stationE: stationE, beginDock: beginDock, counterNear: counterNear, grantShard: grantShard,
        get meleeLog() { return meleeLog; },
        get melee() { var b = api.boss; return b ? { name: b.title, plan: b.plan, ms: b.ms, mv: b.mv ? b.mv.name : '', moves: b.cr && b.cr.moves ? b.cr.moves.map(function (m) { return m.name + ' t' + m.tele + ' d' + m.dmg + ' cd' + m.cd; }) : [], att: b.cr && b.cr.att ? JSON.stringify(b.cr.att) : '', len: b.len / L, escT: b.escT, thrCd: b.thrCd } : null; },
        forceMove: function (i) { var b = api.boss; if (!b || !b.cr || !b.cr.moves) return false; b.mv = b.cr.moves[((i | 0) % b.cr.moves.length + b.cr.moves.length) % b.cr.moves.length]; b.ms = 'turn'; b.mt = 0; return b.mv.name; },
        forceThrow: function () { var b = api.boss; if (!b) return false; return startThrow(b, scratchBodies, true); },
        get thrown() { return { ph: thr.ph, t: thr.t, node: thr.node ? thr.node.id : '', dir: thr.dir.toArray(), s0: thr.s0.toArray(), off: thr.node ? [thr.node.nx || 0, thr.node.ny || 0, thr.node.nz || 0] : null, peak: thr.peak, offs: thr.offs, hit: thr.lastHit, stop: thr.stop, anchor: thr.node ? thr.node.anchor.position.toArray() : null }; },
        get boltsByOwner() { var o = { boss: 0, enemy: 0, player: 0 }; for (var i = 0; i < bolts.length; i++) { var b = bolts[i]; if (!b.active) continue; if (b.boss) o.boss++; else if (b.enemy) o.enemy++; else o.player++; } return o; },
        get color() { return effColor(); },
        get weapon() { return curWeapon(); },
        get playerDps() { return playerDps(); },
        get crates() { return crates.filter(function (c) { return c.active; }).map(function (c) { return { name: c.w.name, p: c.g.position.toArray(), life: c.life, dist: c.g.position.distanceTo(shipRoot.position) / L }; }); },
        dropCrate: function (tier) { vA.copy(NEG_Z).applyQuaternion(shipRoot.quaternion).multiplyScalar(30 * L).add(shipRoot.position); return !!spawnCrate(vA, tier | 0); },
        severLimb: function (e, i) { severLimb(e, i); return e.cr && e.cr.limbs[i] ? !!e.cr.limbs[i].dead : false; },
        get hold() { return { on: eh.on, t: eh.t, held: eh.held, ctx: ehContext() }; },
        get nearPl() { return nearPl; },
        stepN: function (n, dt) { for (var i = 0; i < (n | 0); i++) step(dt || 0.033); return gt; },     // rev 26 debug: advance the sim without rAF (hidden tabs)
        quests: { ctx: questCtx, offer: function (seed, kind) { return QST.offerSet(seed, questCtx(kind || 'npc'), 3); }, accept: function (q) { var pf = qPf(), a = JSON.parse(JSON.stringify(q)); a.acceptedAt = Date.now(); a.expiresAt = a.acceptedAt + 36e5; pf.quests.push(a); qTrkC = ''; return pf.quests.length; }, event: questEvent, get active() { return activeQuests(); }, get objs() { return Object.keys(qObjs); }, open: openQuests, get onb() { var p = curProfile(); return p && p.onb; }, get hullSig() { return hullSigNow; }, get pose() { vF.copy(NEG_Z).applyQuaternion(shipRoot.quaternion); return { p: shipRoot.position.toArray(), f: vF.toArray(), L: L }; } },
        dockTick: function (dt) { clockT += dt; if (state === 'docked' || state === 'away') placeAtAnchor(dt, false); },     // debug: advance the dock one frame without rAF
        get dock() { vA.copy(shipRoot.position).project(camera); return { ndc: [vA.x, vA.y], scale: shipScale, glide: dk.glide, follow: dk.follow, recov: dk.recov, still: +dk.still.toFixed(2) }; },
        get scale() { return { L: L, refR: refR, refR0: refR0, live: readRefR() }; },
        get ps() { return ps; },
        get lf() { return lf; },
        dbg: { setLocal: function (lx, ly, lz, qx, qy, qz, qw) {         // rev 18 test hook: place the ship at an exact LOCAL position (and local attitude) inside the active frame
            if (!lf.on) return false;
            lf.lp.set(lx, ly, lz); lf.lv.set(0, 0, 0); lf.endV.set(0, 0, 0); vel.set(0, 0, 0);
            if (qw !== undefined) lf.lq.set(qx, qy, qz, qw);
            var inAtm = lf.lp.length() < ATM_R * lf.R;
            if (inAtm && !lf.cap) { lf.cap = true; lf.entryT = 0; lf.leaving = false; }
            if (!inAtm && lf.cap && lf.lp.length() > LF_OFF * lf.R) { lf.cap = false; }
            lf.endQ.set(9, 9, 9, 9);        // force the world pose to be re-derived from lq / lp on the next step
            shipRoot.position.copy(lf.lp).applyQuaternion(lf.node.mesh.quaternion).add(lf.node.anchor.position); lf.endP.copy(shipRoot.position);
            shipRoot.quaternion.copy(lf.node.mesh.quaternion).multiply(lf.lq); lf.endQ.copy(shipRoot.quaternion); lf.qEnd.copy(lf.node.mesh.quaternion);
            lf.hover = false; lf.holdAlt = 0; lfAlt(); return true;
        }, bounce: bounceV, vel: vel, speedNow: function () { return speed; }, inject: function (x, y) { mdx += x; mdy += y; }, setSpeed: function (v) { speed = v; }, setPulseT: function (v) { pulseT = v; }, pulseTNow: function () { return pulseT; }, PULSE_FULL: PULSE_E * PULSE_LN, setThrottle: function (v) { throttle = v; }, syncPlanet: syncPlanet, keys: function () { return keys; }, setKeys: function (k) { keys = k; },
            fire: function (on) { firing = !!on; if (on) fireCd = 0; }, setHeat: function (v) { heat = v; ohT = v >= 1 ? OVERHEAT_T : 0; }, stall: function (e) { beginStall(e); return e.stallT; }, windup: function (e) { beginWindup(e, WINDUP_T); return e.ap; }, setBoost: function (v) { boostNow = !!v; }, enemies: enemies, liveBolts: function () { var out = []; for (var i = 0; i < bolts.length; i++) if (bolts[i].active && !bolts[i].enemy) { var bp = fx.boltPos(bolts[i].id, new THREE.Vector3()); if (bp) out.push({ id: bolts[i].id, p: bp, age: gt - bolts[i].t0 }); } return out; } },     // rev 18/19 test hooks
        get bossList() { return enemies.filter(function (e) { return e.alive && e.isBoss; }).map(function (e) { return { p: e.g.position.toArray(), len: e.len, dist: e.g.position.distanceTo(shipRoot.position), R: e.R, state: e.ms, reachD: (function () { var r = 1e30; if (e.cr && e.cr.moves) for (var i = 0; i < e.cr.moves.length; i++) r = Math.min(r, e.cr.moves[i].reachL * e.sc); return Math.max(r * 0.9, e.R * 1.08 + 2 * L); })() }; }); }, get hullObjY() { return hullObj ? hullObj.position.y : 0; }, get shakeNow() { return shake; }, get boltStats() { return { fired: boltsFired, hits: boltHits }; }, get deckBounces() { return deckBounces; },
        get maneuver() { return { flipT: flipT, flipCd: flipCd, driftOn: driftOn, driftLeft: driftLeft, driftCd: driftCd, vel: vel.length(), speed: speed, terrInv: terrInv }; },
        flip: function () { return startFlip(); },
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
        get combat() { return { wave: wave, waveActive: waveActive, queued: qN, chain: chainN, graze: graze, od: od, focus: focusE, focusing: focusing, timeScale: timeScale, rollT: rollT, rollCd: rollCd, hp: hp, pulse: pulse, speed: speed, combo: comboN, hitStopN: hitStopN, zT: zT, drops: drops.filter(function (d) { return d.active; }).length, target: ctarget, tail: tailAlly ? tailLeft : 0, boostDrift: driftBoost, fovKick: fovKick, tickK: cTickK, flipT: flipT, flipCd: flipCd, driftOn: driftOn, driftCd: driftCd }; },
        // rev 21 test hooks
        get rev21() { return { heat: +heat.toFixed(3), ohT: +ohT.toFixed(2), ramT: +ramT.toFixed(2), ramCd: +ramCd.toFixed(2), units: unitsNow(), scanT: +scanT.toFixed(2), jetLvl: +jetLvl.toFixed(2), musOn: musOn, musInt: musInt, musKey: musKey, quiet: waveQuiet, bombs: bombs.filter(function (b) { return b.active; }).length, hl: hlN,
            squads: enemies.filter(function (e) { return e.alive && !e.isBoss; }).map(function (e) { return { sq: e.squad, lead: e.leader, role: e.role, beh: e.beh, shield: !!(e.cr && e.cr.stats && e.cr.stats.shield), ap: e.ap, stall: +e.stallT.toFixed(2), state: e.state, hp: Math.round(e.hp), elite: e.elite, jink: +e.jinkT.toFixed(2), lockT: +e.lockT.toFixed(2) }; }) }; },
        tryRam: tryRam, doScan: doScan, addUnits: function (n) { addUnits(n, shipRoot.position); return unitsNow(); }, nearNpc: npcNear, talkNpc: talkNpc,
        get states() { return enemies.filter(function (e) { return e.alive; }).map(function (e) { return { role: e.isBoss ? 'BOSS' + e.kind : ROLE_T[e.role].name, tier: e.tier, sig: e.cr ? e.cr.signature : '', state: e.fstate, hp: Math.round(e.hp), max: e.maxHp, flees: e.fleeN, grace: +e.grace.toFixed(1), phase: e.bphase, open: e.open, dist: Math.round(e.g.position.distanceTo(shipRoot.position) / L) }; }); },
        drops: drops,
        get save() { try { return localStorage.getItem(SAVE_KEY); } catch (e) { return null; } },
        get state() { return state; },
        get gmode() { return gmode; },
        get ground() { return { gmode: gmode, landOk: landOk, entryHeat: entryHeat, legDrop: legDrop, human: hum.obj ? hum.obj.group.position.toArray() : null, hr: hum.hr, gr: hum.gr, air: hum.air, ship: shipRoot.position.toArray(), prompt: cLandTxt }; },
        land: function () { return startLanding(); }, onKeyF: onKeyF, onKeyE: onKeyF,
        get humanObj() { return hum.obj ? hum.obj.group : null; },
        _g: { hum: hum, land: land, fl: fl, world: function () { return world; }, objBegin: function () { return objBegin(); }, objPush: function (p, H) { return objPush(p, H); } },
        get stats() { return { hp: hp, wave: shownWave(), bossPhase: (api.boss || { bphase: 0 }).bphase, boss: (api.boss || { hp: 0 }).hp, bossMax: (api.boss || { maxHp: 0 }).maxHp, allies: allies.filter(function (a) { return a.alive; }).length, kills: kills, enemies: aliveCount(), L: L, dead: dead, gt: gt }; }
    };
    window.EMGOR_SHIP = api;   // debug / test hook
    return api;
}
