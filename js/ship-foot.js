// ship-foot.js — the on-foot controller split out of ship.js (Q27, zero behaviour change): footStep + jetpack flightStep + their private constants/scratch.
//   createFoot(ctx) -> { footStep(dt, c, qM) -> focus Vector3, flightStep(dt, c, qM) }
//   ctx = ship.js state. Stable objects/functions/constants are plain properties (captured once, at first use); state ship.js reassigns is read/written
//   through the accessor properties (S.mdx, S.keys, S.ps, ...), so there is exactly one copy of every variable.
//   2026-11-25: camera constants are CAM (top of this file): boom 3.2 H, wheel zoom 2-5 H, wall / roof pull-in (probes along the boom through ctx.objPush, snap in, ease out 6 H/s), indoor mode under a roof (boom 1.6 H,
//   lower head, FOV +10 deg via S.fovAdd, 0.25 s blend). Movement easing is FEEL (0.18 s up, 0.14 s down). SEAT is the Burger House pose freeze (written by ship-world.js interactNear).
import { hOf } from './ship-scale.js';
import { mkJetSM, jetStep, jetThrust } from './ship-styles.js';

// ── camera + feel constants (2026-11-25). Distances in human heights (H), speeds in H/s, times in s. Tune here, nothing else in this file hardcodes them. ──
export const CAM = {
  BOOM: 3.2,            // default outdoor boom (was 4.5)
  ZOOM_MIN: 2, ZOOM_MAX: 5, WHEEL_K: 0.0025,      // mouse wheel zoom range; one 120-unit notch = 0.3 H
  SHOULDER: 0.35,       // right-shoulder offset (camera and look point both slide this far)
  HEAD: 1.0,            // focus height above the feet
  MARGIN: 0.15,         // extra clearance kept from a wall / roof, on top of the probe radius (0.26 H)
  MIN: 0.8,             // the camera never comes closer than this to the head
  STEP: 0.3,            // wall probe spacing along the boom
  EASE_OUT: 6,          // H/s the boom grows back after a wall pull-in; pull-IN is instant
  INDOOR_BOOM: 1.6,     // boom under a roof (store, hut, Burger House, awning)
  INDOOR_HEAD: 0.85,    // focus height under a roof
  INDOOR_PITCH: 0.6,    // pitch scale under a roof (keeps the lens under the ceiling)
  INDOOR_SHOULDER: 0.5, // shoulder scale under a roof
  INDOOR_FOV: 10,       // degrees added to the foot FOV under a roof (ship.js reads S.fovAdd, see changes-world.md)
  INDOOR_EASE: 0.25,    // seconds for the indoor blend
  ROOF_PROBES: [1.6, 2.2, 2.8, 3.4],  // heights above the feet (H) probed for a roof / ceiling (a 2.8 H ceiling is hit by the top two); >= 2 hits = indoors
  ROOF_ON: 0.15, ROOF_OFF: 0.35,     // seconds the probes must agree before the mode flips (doorways do not flicker)
};
export const FEEL = {
  ACC_T: 0.18,          // s to reach walk / run speed from rest (was 0.067)
  FRIC_T: 0.14,         // s to stop from full speed (was 0.075)
  TURN_T: 0.08,         // s to brake a 180 degree reversal before walking the new way (was 0.033)
};
// a Burger House stool / booth: ship-world.js interactNear() writes SEAT.req = { pos:[x,y,z], face:[x,y,z] } (planet-local), foot owns the pose.
export const SEAT = { req: null, on: false, leave: false, armed: false, k: 0, pos: null, face: null, stand: null };

export function createFoot(ctx) {
    var S = ctx, THREE = ctx.THREE;
    var FOOT_SLIDE_A = ctx.FOOT_SLIDE_A, FOOT_SLIDE_K = ctx.FOOT_SLIDE_K, G_WALK = ctx.G_WALK, G_RUN = ctx.G_RUN, JET_HOLD = ctx.JET_HOLD, JET_UP = ctx.JET_UP, MOUSE_SENS = ctx.MOUSE_SENS;
    var R27 = ctx.R27, X = ctx.X, aPlay = ctx.aPlay, clamp = ctx.clamp, damp = ctx.damp, fl = ctx.fl, flHud = ctx.flHud, footFloor = ctx.footFloor, footWind = ctx.footWind;
    var fsC = ctx.fsC, fsC2 = ctx.fsC2, fsT = ctx.fsT, gA = ctx.gA, gB = ctx.gB, gC = ctx.gC, gD = ctx.gD, gE = ctx.gE, gQ = ctx.gQ, gQi = ctx.gQi;
    var groundCam = ctx.groundCam, hum = ctx.hum, jetFx = ctx.jetFx, mFloor = ctx.mFloor, mM = ctx.mM, objBegin = ctx.objBegin, objPush = ctx.objPush;
    var styEggTick = ctx.styEggTick, styU = ctx.styU, styUpW = ctx.styUpW, styWallO = ctx.styWallO, styleFx = ctx.styleFx, styleStep = ctx.styleStep, styleSync = ctx.styleSync, vTmp = ctx.vTmp;
    var G_GRAV = 16, G_GRAV_UP = 7.93, G_JUMP = 5.34;   // apex 1.8 H; walk / run speed = ctx.G_WALK / G_RUN (ship.js)   // in human heights (/s, /s^2); rev 29: walk = the old run speed, Shift run = 2x
    var camZoom = CAM.BOOM;      // the wheel moves this between CAM.ZOOM_MIN and CAM.ZOOM_MAX
    var FL_HOLD = 2, FL_V = 36;
    var JET_RAMP = 1.2, JET_DIVE = 16, DIVE_V0 = 3, DIVE_TAU = 0.6, DIVE_CAP = 48, stepPh = 0.6, camDip = 0;      // 2026-10-14 jump: apex 1.2 H in 0.55 s (v0 4.36 H/s, g_up 7.93, falling g 16); Shift dive v = v0 * 2^(t/0.6) capped at 48 H/s      // 2026-10-14: jetpack climb ramp (s to 3.5x), Shift dive speed (heights/s), footstep phase               // rev 29: hold Space this many s = jetpack FLIGHT; flight cruise speed (heights/s)
    var WALL_TAN = Math.tan(70 * Math.PI / 180);      // Q06: terrain steeper than 70 deg along the move counts as a wall
    var FOOT_SUB = 0.12, fsD = new THREE.Vector3();
    // unstyled (PILOT) ground model: speed ramps with FEEL.ACC_T / FRIC_T (0.18 s up, 0.14 s down); direction is instant, a reversal brakes to 0 in TURN_T first
    var gv = 0, gdir = new THREE.Vector3(), JET_A = 60, DIVE_A = 90;      // JET_A / DIVE_A: vertical H/s^2 toward the burn / dive target (linear)
    function pilotRamp(dt, up, moving, wish, top, topRef) {
        var lim = 0.02 * topRef;
        gdir.addScaledVector(up, -gdir.dot(up)); if (gdir.lengthSq() < 1e-8) gdir.copy(wish); gdir.normalize();
        if (moving) {
            if (gv > lim && gdir.dot(wish) < 0) { gv = Math.max(0, gv - 2 * topRef / FEEL.TURN_T * dt); if (gv <= lim) { gv = 0; gdir.copy(wish); } }
            else { gdir.copy(wish); gv = gv > top ? Math.max(top, gv - topRef / FEEL.FRIC_T * dt) : Math.min(top, gv + top / FEEL.ACC_T * dt); }
        } else gv = Math.max(0, gv - topRef / FEEL.FRIC_T * dt);
        return gv;
    }
    // character animation feed (ship-human.js update): signed yaw rate, ground under each foot, nearest creature to look at
    var ftOld = new THREE.Vector3(), ftX = new THREE.Vector3(), ftTurn = 0, gaV = new THREE.Vector3(), gaR = new THREE.Vector3(), lkP = new THREE.Vector3(), lkO = { x: 0, z: 0, d: 99 }, lkHave = false, lkT = 0;
    function groundAtFoot(lx, lz) {          // lx right, lz back (human heights, body frame) -> ground height under that foot relative to the one under the body (H)
        var H = hOf(S.L); gaR.crossVectors(hum.face, hum.up);
        gaV.copy(hum.pos).addScaledVector(gaR, lx * H).addScaledVector(hum.face, -lz * H); var r = gaV.length() || 1; gaV.divideScalar(r);
        return (footFloor(gaV, H, hum.hr) - hum.gr) / H;
    }
    function lookFeed(dt, H, up) {           // nearest creature within 6 H (re-scanned 5x / s); returns the look target in body-frame H, or undefined
        lkT -= dt;
        if (lkT <= 0) {
            lkT = 0.2; lkHave = false;
            try {
                var ps = S.ps; if (ps && ps.creatures && ps.creatLocal) {
                    var cs = ps.creatures(), bd = 36 * H * H, i, d2;
                    for (i = 0; i < cs.length; i++) { if (!ps.creatLocal(cs[i].id, gaV)) continue; d2 = gaV.distanceToSquared(hum.pos); if (d2 < bd) { bd = d2; lkP.copy(gaV); lkHave = true; } }
                }
            } catch (e0) { lkHave = false; }
        }
        if (!lkHave) return undefined;
        gaV.subVectors(lkP, hum.pos); gaR.crossVectors(hum.face, up);
        lkO.x = gaV.dot(gaR) / H; lkO.z = -gaV.dot(hum.face) / H; lkO.d = gaV.length() / H;
        return lkO;
    }
    var styCam = 1, flF = new THREE.Vector3(), flA = new THREE.Vector3();
    // ── camera: wall / roof pull-in + indoor mode ──────────────────────────────────────────────────────────────────────────────────────────────────
    // The solid sets (store / Burger House / CVS walls + roofs, village hut walls + roof slabs, the hull) live behind ctx.objBegin()/objPush(): objPush resolves a capsule (two spheres 0.25 H and 0.75 H
    // above the "feet" point, radius 0.26 H) against all of them. The camera probes with that same capsule centred on each sample (feet = sample - 0.5 H up), so "blocked" means the lens would be inside
    // or within 0.26 H of a solid. Everything below is planet-local (the frame objPush works in).
    var kH = new THREE.Vector3(), kW = new THREE.Vector3(), kP = new THREE.Vector3(), kQ = new THREE.Vector3(), kD = new THREE.Vector3(), kR = new THREE.Vector3();
    var camT = 1, camSnap = false, indK = 0, indTgt = 0, indRaw = false, indAgree = 0, indProbeT = 0;
    function probeHit(p, up, H) { kQ.copy(p).addScaledVector(up, -0.5 * H); return objPush(kQ, H); }
    // head -> wanted lens (planet-local): the fraction of the boom that is free. Pull-in is instant, the boom grows back at CAM.EASE_OUT H/s. Returns the fraction to apply to (want - head).
    function camClip(dt, head, want, up, H) {
        var len = head.distanceTo(want), n = Math.max(3, Math.min(24, Math.ceil(len / (CAM.STEP * H)))), i, t = 1;
        if (len < 1e-9) return 1;
        for (i = 1; i <= n; i++) {
            kP.lerpVectors(head, want, i / n);
            if (probeHit(kP, up, H)) { t = Math.max(0, (i - 1) / n - CAM.MARGIN * H / len); break; }
        }
        t = Math.max(t, Math.min(1, CAM.MIN * H / len));
        camSnap = t < camT;      // pulled in this frame: the smoother follows faster so the lens never lags through the wall
        if (camSnap) camT = t; else camT = Math.min(t, camT + CAM.EASE_OUT * H * dt / len);
        return camT;
    }
    // roof / ceiling above the head? three vertical probes, >= 2 hits, debounced; indK is the 0..1 smoothed blend (CAM.INDOOR_EASE s)
    function indoorTick(dt, up, hr, H) {
        var k, hits = 0, step;
        indProbeT -= dt;
        if (indProbeT <= 0) {
            indProbeT = 0.1;
            for (k = 0; k < CAM.ROOF_PROBES.length; k++) { kP.copy(up).multiplyScalar(hr + CAM.ROOF_PROBES[k] * H); if (probeHit(kP, up, H)) hits++; }
            indRaw = hits >= 2;
        }
        if (indRaw !== (indTgt > 0.5)) { indAgree += dt; if (indAgree >= (indRaw ? CAM.ROOF_ON : CAM.ROOF_OFF)) { indTgt = indRaw ? 1 : 0; indAgree = 0; } } else indAgree = 0;
        step = dt / CAM.INDOOR_EASE; indK += indTgt > indK ? Math.min(step, indTgt - indK) : -Math.min(step, indK - indTgt);
        return indK * indK * (3 - 2 * indK);
    }
    // seat: a Burger House stool / booth. SEAT.req is set by ship-world.js; pose is frozen at the seat, any move key (pressed fresh) or SEAT.leave stands up where you sat down from.
    function seatTick(dt) {
        var mv = !S.cmdOpen && !!(S.keys.KeyW || S.keys.KeyA || S.keys.KeyS || S.keys.KeyD || S.keys.Space || S.keys.Escape);
        if (SEAT.req) { if (!SEAT.on) { SEAT.on = true; SEAT.armed = false; SEAT.stand = hum.pos.toArray(); } SEAT.pos = SEAT.req.pos; SEAT.face = SEAT.req.face; SEAT.req = null; hum.air = false; hum.vv = 0; gv = 0; }
        if (!SEAT.on) { if (SEAT.k > 0) SEAT.k = Math.max(0, SEAT.k - dt * 4); return false; }
        if (!mv) SEAT.armed = true;
        if (SEAT.leave || (SEAT.armed && mv)) {
            SEAT.on = false; SEAT.leave = false; hum.pos.fromArray(SEAT.stand); hum.hr = hum.gr = hum.vh = hum.pos.length(); hum.air = false; hum.vv = 0; return false;
        }
        SEAT.k = Math.min(1, SEAT.k + dt * 4);
        return true;
    }
    if (typeof window !== 'undefined' && typeof window.addEventListener === 'function') window.addEventListener('wheel', function (e) {
        if (S.cmdOpen || typeof document === 'undefined' || !document.pointerLockElement) return;
        camZoom = clamp(camZoom + clamp(e.deltaY, -120, 120) * CAM.WHEEL_K, CAM.ZOOM_MIN, CAM.ZOOM_MAX);
    }, { passive: true });
    function vTo(tgt, step) { var d = tgt - hum.vv; hum.vv += d > step ? step : d < -step ? -step : d; }
    function flightStep(dt, c, qM) {
        var H = hOf(S.L), o = hum.obj, up = hum.up, k, v = fl.v;
        if (S.cmdOpen) { S.mdx = S.mdy = 0; }
        up.copy(hum.pos).normalize();
        var wy = clamp(S.mdx * MOUSE_SENS * 1.3 / dt, -3, 3), wp = clamp(S.mdy * MOUSE_SENS * 1.3 / dt, -2.5, 2.5);
        S.mdx = S.mdy = 0;
        fl.wy += (wy - fl.wy) * damp(7, dt); fl.wp += (wp - fl.wp) * damp(7, dt);      // weighted: the body lags the mouse like the ship does, lighter
        hum.hf.applyAxisAngle(up, -fl.wy * dt);
        hum.hf.addScaledVector(up, -hum.hf.dot(up));
        if (hum.hf.lengthSq() < 1e-8) hum.hf.crossVectors(up, X);
        hum.hf.normalize();
        var kW = !S.cmdOpen && !!S.keys.KeyW, kS = !S.cmdOpen && !!S.keys.KeyS, kSp = !S.cmdOpen && !!S.keys.Space, kSh = !!(S.keys.ShiftLeft || S.keys.ShiftRight), thr = kW || kSp;
        fl.pit = clamp(fl.pit - fl.wp * dt, -1.3, 1.3);
        if (!thr) fl.pit *= Math.exp(-1.6 * dt);                           // auto-level
        flF.copy(hum.hf).multiplyScalar(Math.cos(fl.pit)).addScaledVector(up, Math.sin(fl.pit));
        var vmax = FL_V * H * (1 + 0.12 * R27.su.jetpack) * (kSh ? 2 : 1) * S.fxSpeed;
        if (kW) v.addScaledVector(flF, 70 * H * (kSh ? 1.6 : 1) * dt);
        if (kSp) v.addScaledVector(up, 60 * H * (kSh ? 1.4 : 1) * dt);
        else if (kSh && !kW) v.addScaledVector(up, -80 * H * dt);           // 2026-10-14: Shift alone pulls you back down
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
            if (hr <= sup) {
                hr = sup; touched = true; vn = v.dot(dir); if (vn < 0) v.addScaledVector(dir, -vn);
                if (opN) { fsC.copy(dir).multiplyScalar(hr); if (objPush(fsC, H)) { hr = fsC.length(); dir.copy(fsC).divideScalar(hr || 1); } }      // Q47: the floor clamp must not seat the capsule back inside a store box
            }
        }
        hum.gr = sup; hum.hr = hr; hum.vv = v.dot(dir); hum.air = true; hum.moving = true; hum.running = true;
        hum.pos.copy(dir).multiplyScalar(hr); up.copy(dir); hum.vh = hr;
        if (touched && !kSp) { fl.on = false; fl.lock = false; hum.air = false; hum.vv = 0; fl.cr = clamp(0.45 + Math.abs(vUp0) / (14 * H), 0.45, 1); fl.v.set(0, 0, 0); aPlay('footLand', { vel: fl.cr }); }
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
        o.update(dt, { moving: true, running: true, airborne: true, speed: 1, facing: 0, jet: S.jetLvl, scan: S.scanT > 0 ? Math.min(1, S.scanT / 1.2) : 0 });
        jetFx(dt, fl.on ? Math.max(0.35, fl.th) : 0, H, qM, c, dir, sup, hr);
        flHud(fl.on, fl.alt);
        // camera: behind and a little above the flying human, along the (damped) flight pitch; never under the floor
        var cpi = fl.pit * 0.75;
        flA.copy(hum.hf).multiplyScalar(Math.cos(cpi)).addScaledVector(up, Math.sin(cpi)).negate().multiplyScalar(camZoom * H).addScaledVector(up, 0.7 * H);
        kH.copy(up).multiplyScalar(hr + 0.5 * H); kW.copy(kH).add(flA);
        flA.multiplyScalar(camClip(dt, kH, kW, up, H));      // flying past a hut / the store: the boom shortens instead of going through the wall
        gA.copy(flA).applyQuaternion(qM);
        gC.copy(gB).multiplyScalar(0.5 * H).add(hum.w);
        gD.copy(gC).add(gA);
        vTmp.subVectors(gD, c);
        var cr = vTmp.length();
        gQi.copy(qM).invert(); fsT.copy(vTmp).applyQuaternion(gQi);
        var cfr = mFloor(fsT.x, fsT.y, fsT.z);
        if (cr < cfr + 0.04 * S.L) gD.copy(c).addScaledVector(vTmp.divideScalar(cr || 1), cfr + 0.04 * S.L);
        mM.lookAt(gD, gC, gB); gQ.setFromRotationMatrix(mM);
        groundCam(dt, gC, gD, gQ, 10);
        return gC;
    }
    function footStep(dt, c, qM) {
        var H = hOf(S.L), o = hum.obj, up = hum.up, i, k;
        if (S.cmdOpen) { S.mdx = S.mdy = 0; }
        up.copy(hum.pos).normalize();
        // rev 29: hold Space FL_HOLD s (continuously, in the air) = jetpack FLIGHT
        styleSync(); styEggTick(dt); var sty = S.curStyle, sr = null, sSpd = -1, air0 = hum.air, vv0 = hum.vv;
        var flWant = !S.cmdOpen && !!S.keys.Space;
        if (flWant && !fl.lock) S.flT += dt; else { S.flT = 0; if (!flWant) fl.lock = false; }
        if (!fl.on && hum.air && S.flT >= FL_HOLD) { fl.on = true; fl.pit = 0; fl.wy = fl.wp = 0; fl.th = 0; fl.cr = 0; fl.v.copy(up).multiplyScalar(Math.max(0, hum.vv)); }
        if (fl.on) return flightStep(dt, c, qM);
        if (fl.cr > 0) fl.cr = Math.max(0, fl.cr - dt * 3.2);
        // look
        if (S.mdx !== 0) hum.hf.applyAxisAngle(up, -S.mdx * MOUSE_SENS * 1.3);
        hum.pitch = clamp(hum.pitch + S.mdy * MOUSE_SENS * 1.3, -0.3, 1.4);
        S.mdx = S.mdy = 0;
        hum.hf.addScaledVector(up, -hum.hf.dot(up));
        if (hum.hf.lengthSq() < 1e-8) hum.hf.crossVectors(up, X);
        hum.hf.normalize();
        gA.crossVectors(hum.hf, up);                                  // right
        var fw = S.cmdOpen ? 0 : ((S.keys.KeyW ? 1 : 0) - (S.keys.KeyS ? 1 : 0)), sd = S.cmdOpen ? 0 : ((S.keys.KeyD ? 1 : 0) - (S.keys.KeyA ? 1 : 0));
        gB.set(0, 0, 0).addScaledVector(hum.hf, fw).addScaledVector(gA, sd);
        var ml = gB.length(), moving = ml > 0.01, run = moving && !!(S.keys.ShiftLeft || S.keys.ShiftRight);
        if (moving) gB.divideScalar(ml);
        var sat = seatTick(dt);      // Burger House seat: frozen pose, no walking, no jumping
        if (sat) { moving = false; run = false; }
        var wantJump = !sat && !S.cmdOpen && !!S.keys.Space;
        if (sty) {          // rev 30: the style owns velocity (incl. gravity); we move the capsule by vel * dt, collide, and report onGround back next frame
            sr = styleStep(dt, up, gA, H);
            if (sr && sr.vel) {
                gB.set(0, 0, 0).addScaledVector(gA, sr.vel.x).addScaledVector(hum.hf, -sr.vel.z); var hl0 = gB.length(); moving = hl0 > 0.3; sSpd = hl0; hum.sSpd = hl0; if (moving) gB.divideScalar(hl0);
                run = !!(sr.anim && sr.anim.running);
                if (!hum.air && (sr.jumpImpulse > 0 || sr.groundSnap === false)) { hum.air = true; hum.airT = 0; if (sr.jumpImpulse > 0) aPlay('jump', { vel: 0.6 }); }      // slope-glued motion (SONIK) keeps vel.y but stays grounded
                if (hum.air) hum.vv = sr.vel.y * H;
                if (sr.fx && sr.fx.length) styleFx(sr.fx, H, up, gA);
                styCam += ((sr.camHint && sr.camHint.dist ? sr.camHint.dist : 1) - styCam) * damp(4, dt);
                if (sr.camHint) { S.styFov += ((sr.camHint.fov || 1) - S.styFov) * damp(8, dt); if (sr.camHint.shake > 0) S.shake = Math.max(S.shake, sr.camHint.shake * 0.12 * S.L); }
            }
        } else { styCam += (1 - styCam) * damp(4, dt); S.styFov += (1 - S.styFov) * damp(8, dt); }
        if (hum.air) hum.airT = (hum.airT || 0) + dt; else hum.airT = 0;
        hum.sAnim = sr && sr.anim != null ? sr.anim : null;
        hum.moving = moving; hum.running = run;
        ftOld.copy(hum.face);
        if (moving) hum.face.lerp(gB, damp(8.3, dt));      // the body turns toward the velocity with a ~0.12 s lag
        ftX.crossVectors(ftOld, hum.face); ftTurn = dt > 0 ? Math.atan2(ftX.dot(up), ftOld.dot(hum.face)) / dt : 0;
        // jump (once per press)
        if (!sty && !hum.air && wantJump && !S.jumpHeld) { hum.air = true; hum.vv = G_JUMP * H * S.fxJump; aPlay('jump', { vel: 0.6 }); }
        S.jumpHeld = wantJump;
        // jetpack input model (ship-styles.js jetStep): tap = style jump, hold >= 0.18 s in the air = jet for EVERY style, Shift = dive
        var jsm = S.jsm || (S.jsm = mkJetSM()), jt = jetStep(jsm, dt, wantJump, hum.air, !!(S.keys.ShiftLeft || S.keys.ShiftRight) && !S.cmdOpen, !!sty), jetMul = sty ? jetThrust(sty.id) : 1;
        S.jetT = jt.t;
        var dive = jt.dive;
        if (dive) { if (!(hum.diveT > 0)) hum.diveV0 = Math.max(DIVE_V0, -hum.vv / H); hum.diveT = jt.diveT; } else hum.diveT = 0;
        var diveV = Math.min(DIVE_CAP, (hum.diveV0 || DIVE_V0) * Math.pow(2, Math.min(hum.diveT, 8) / DIVE_TAU));      // exponential in hold time, from the speed you already have
        var jet = jt.jet, jetK = jt.k * jetMul;
        // substepped capsule motion in the local frame
        var spd = (sSpd >= 0 ? sSpd : (jet ? G_RUN * (1 + 0.12 * R27.su.jetpack) : (run ? G_RUN * (1 + 0.1 * R27.su.sprint) : G_WALK))) * H * S.fxSpeed, nSub = 1;
        if (sSpd < 0) {          // PILOT: explicit accel / friction / turnaround, then keep walking in the old direction while the speed coasts down
            spd = pilotRamp(dt, up, moving, gB, spd, G_WALK * H * S.fxSpeed);
            if (spd > 0.02 * G_WALK * H * S.fxSpeed) { if (!moving || gdir.dot(gB) < 0.999) gB.copy(gdir); moving = true; } else { moving = false; spd = 0; }
            hum.moving = moving;
        }
        if (moving) nSub = Math.max(nSub, Math.ceil(spd * dt / (FOOT_SUB * H)));
        if (hum.air) nSub = Math.max(nSub, Math.ceil(Math.abs(hum.vv) * dt / (0.25 * H)));
        nSub = Math.min(16, nSub);
        var h = dt / nSub, dir = fsD.copy(hum.pos).normalize(), hr = hum.hr, supNow = footFloor(dir, H, hr), opN = objBegin(), edgeG = !!(sr && sr.edgeGuard), wallHit = false, wallSl = false, wSx = 0, wSz = 0, wallNow = false, wRiseH = 0;
        if (sat) { dir.set(SEAT.pos[0], SEAT.pos[1], SEAT.pos[2]); hr = dir.length(); dir.normalize(); supNow = hr; nSub = 0; hum.air = false; hum.vv = 0;      // seated: pinned to the seat, no substeps
            hum.face.set(SEAT.face[0], SEAT.face[1], SEAT.face[2]); }
        for (k = 0; k < nSub; k++) {
            if (moving) {
                var step = spd * h;
                for (i = 0; i < 5; i++) {                              // the wanted direction, then slide around a wall / cliff
                    fsT.copy(gB); if (FOOT_SLIDE_A[i] !== 0) fsT.applyAxisAngle(dir, FOOT_SLIDE_A[i]);
                    var st = step * FOOT_SLIDE_K[i];
                    fsC.copy(dir).addScaledVector(fsT, st / hr).normalize();
                    var supC = footFloor(fsC, H, hr), rise = supC - supNow;
                    if (edgeG && !hum.air && supNow - supC > 0.4 * H) { if (i === 0) { wallHit = wallNow = true; wallSl = false; } continue; }      // mashup: sneak (STEEV) refuses to step off a ledge
                    if (hum.air ? supC <= hr + 0.05 * H : rise <= st + 0.05 * H) {
                        if (wallNow && i > 0) { wallSl = true; wSx = fsT.dot(gA); wSz = -fsT.dot(hum.hf); }      // Q06: first free slide direction = the wall's tangent
                        dir.copy(fsC); supNow = supC; wallNow = false;
                        break;
                    }
                    if (i === 0) { wallNow = rise > WALL_TAN * st; if (wallNow) { wallHit = true; wallSl = false; wRiseH = rise / H; } }      // Q06: only a wall steeper than WALL_DEG counts, whether or not a slide then succeeds
                }
            }
            if (opN && (moving || hum.air)) {                      // rev 29: store / burger-house walls, shelves and the ship hull are solid for the capsule
                fsT.copy(dir).multiplyScalar(hr);
                if (objPush(fsT, H)) {
                    var nl = fsT.length(); dir.copy(fsT).divideScalar(nl || 1); supNow = footFloor(dir, H, hr);
                    if (hum.air && nl < hr) { hr = nl; hum.vv = Math.min(hum.vv, 0); }
                }
            }
            if (hum.air) {
                if (jet) { vTo(JET_UP * jetK * (1 + 0.2 * R27.su.jetpack) * H, JET_A * H * h); if (hr > supNow + 400 * H) hum.vv = Math.min(hum.vv, 0); }
                else if (dive) vTo(-diveV * H, DIVE_A * H * h);
                else if (!sty) hum.vv -= (hum.vv > 0 ? G_GRAV_UP : G_GRAV) * H * h;      // styles integrate their own gravity
                hr += hum.vv * h;
                if (hr <= supNow && !(sty && hum.vv > 0)) { hr = supNow; hum.vv = 0; hum.air = false; }
            } else if (hr - supNow > 0.6 * H) { hum.air = true; hum.vv = 0; }
            else hr = supNow;
        }
        if (sty && sty.state && (jet || dive) && hum.air) sty.state.vy = hum.vv / H;      // the burn / dive is written back so the style resumes from it on release
        hum.wallT += dt;                                                // Q06: seconds since the last wall contact (styles read c.wallNormal inside 0.12 s)
        if (wallHit && moving) {
            hum.wallT = 0; hum.wallH = wRiseH; hum.wallX = -(gB.dot(gA)); hum.wallZ = gB.dot(hum.hf);
            if (wallSl && S.stylesMod && S.stylesMod.wallNormal2) { if (S.stylesMod.wallNormal2(gB.dot(gA), -gB.dot(hum.hf), wSx, wSz, styWallO)) { hum.wallX = styWallO.x; hum.wallZ = styWallO.z; } }
        }
        // 2026-10-14: ground-contact cues. Landing edge = footLand (impact from the fall speed) + a crouch above a hard drop; steps on a walk / run cadence.
        var landImp = -1;
        if (air0 && !hum.air) { landImp = clamp(-Math.min(vv0, hum.vv) / (18 * H), 0, 1); aPlay('footLand', { vel: 0.3 + 0.7 * landImp }); if (landImp > 0.35) fl.cr = Math.max(fl.cr, landImp); camDip = Math.max(camDip, (0.05 + 0.3 * landImp) * H); stepPh = 0.6; }
        else if (!hum.air && moving) { stepPh += dt * 2.4 * clamp(spd / (G_WALK * H), 0.5, 1.8); if (stepPh >= 1) { stepPh -= 1; aPlay('step', { level: run ? 1 : 0, vel: run ? 0.6 : 0.45 }); } }
        else if (!hum.air) stepPh = 0.6;      // the wall normal in the tangent frame (x right, z back) = against the wanted direction
        if (R27.windOn && moving && !(S.ps.inSite && S.ps.inSite(dir.x, dir.y, dir.z))) { var wd = footWind(dir, dt, hr, H, supNow); if (wd) { dir.copy(fsC2); supNow = wd; if (!hum.air) hr = supNow; } }
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
        styUpW.copy(gB);
        gC.crossVectors(gA, gB); gD.copy(gA).negate();
        mM.makeBasis(gC, gB, gD);
        o.group.quaternion.setFromRotationMatrix(mM);
        o.group.position.copy(hum.w); o.group.scale.set(H, H * (1 - 0.22 * fl.cr) * (1 - 0.28 * SEAT.k), H);      // seated: the rig squashes to 0.72 H (ship-human.js has no sit pose; see changes-world.md)
        flHud(false, 0);
        if (hum.sAnim) { var ua = Object.assign(styU, hum.sAnim); ua.facing = 0; ua.jet = S.jetLvl; ua.scan = S.scanT > 0 ? Math.min(1, S.scanT / 1.2) : 0; ua.vH = sSpd >= 0 ? sSpd : undefined; ua.dist = sSpd >= 0 && !hum.air ? sSpd * dt : undefined; ua.turn = ftTurn; ua.look = lookFeed(dt, H, up); o.group.userData.groundAt = groundAtFoot; o.update(dt, ua); }
        else { o.group.userData.groundAt = groundAtFoot; o.update(dt, { moving: moving, running: run || jet, airborne: hum.air, speed: moving ? (run || jet ? 1 : 0.5) : 0, cad: moving ? clamp(spd / (G_RUN * H), 0.3, 1.2) : 0, vH: moving ? spd / H : 0, dist: moving && !hum.air ? spd * dt / H : 0, turn: ftTurn, look: lookFeed(dt, H, up), facing: 0, jet: S.jetLvl, scan: S.scanT > 0 ? Math.min(1, S.scanT / 1.2) : 0 }); }
        jetFx(dt, jet, H, qM, c, dir, supNow, hr);
        if (landImp > 0.35) S.fx.impact(hum.w, 0xc8b89a, 0.6 + landImp);
        // camera: CAM.BOOM (3.2) H back, wheel-zoomable 2-5 H, pitched, right-shoulder; it follows the SMOOTHED position (hum.w), not the raw step. Under a roof (indoor mode) the boom shortens to 1.6 H, the lens drops,
        // the FOV widens 10 degrees. A wall / roof between the head and the lens pulls it in instantly (probes along the boom against the structure solids), it eases back out at 6 H/s. Never under the floor.
        var ik = indoorTick(dt, up, hum.hr, H), seatK = SEAT.k;
        camDip *= Math.exp(-9 * dt); if (camDip < 1e-6) camDip = 0;
        var pch = hum.pitch * (1 - (1 - CAM.INDOOR_PITCH) * ik), boom = (camZoom + (CAM.INDOOR_BOOM - camZoom) * ik) * styCam, headH = (CAM.HEAD + (CAM.INDOOR_HEAD - CAM.HEAD) * ik) * (1 - 0.28 * seatK) * H - camDip, shl = CAM.SHOULDER * (1 - (1 - CAM.INDOOR_SHOULDER) * ik) * H;
        kD.copy(hum.hf).negate().multiplyScalar(Math.cos(pch)).addScaledVector(up, Math.sin(pch));          // planet-local boom direction
        kR.crossVectors(hum.hf, up);                                                                          // planet-local right
        kH.copy(up).multiplyScalar(hum.vh + headH);                                                           // planet-local head
        kW.copy(kH).addScaledVector(kD, boom * H).addScaledVector(kR, shl);                                   // planet-local wanted lens
        var ct = camClip(dt, kH, kW, up, H);
        gA.copy(kD).applyQuaternion(qM);                                                                      // world boom direction
        gE.copy(kR).applyQuaternion(qM);                                                                      // world right
        gC.copy(gB).multiplyScalar(headH).add(hum.w);                                                         // focus (head), dips a little on landing / sitting
        gD.copy(gC).addScaledVector(gA, boom * H).addScaledVector(gE, shl);                                   // wanted lens, full boom
        gD.sub(gC).multiplyScalar(ct).add(gC);                                                                // the free part of it
        S.fovAdd = CAM.INDOOR_FOV * ik;                                                                       // degrees (ship.js adds it to the foot FOV)
        vTmp.subVectors(gD, c);
        var cr = vTmp.length();
        gQi.copy(qM).invert(); fsT.copy(vTmp).applyQuaternion(gQi);
        var cfr = mFloor(fsT.x, fsT.y, fsT.z);
        if (cr < cfr + 0.04 * S.L) gD.copy(c).addScaledVector(vTmp.divideScalar(cr || 1), cfr + 0.04 * S.L);      // the rendered ground now equals the floor used here, so the clearance is just a little more than the near plane
        gE.multiplyScalar(shl).add(gC);
        mM.lookAt(gD, gE, gB); gQ.setFromRotationMatrix(mM);
        groundCam(dt, gC, gD, gQ, camSnap ? 40 : 16);
        return gC;
    }
    return { footStep: footStep, flightStep: flightStep };
}
