// ship-foot.js — the on-foot controller split out of ship.js (Q27, zero behaviour change): footStep + jetpack flightStep + their private constants/scratch.
//   createFoot(ctx) -> { footStep(dt, c, qM) -> focus Vector3, flightStep(dt, c, qM) }
//   ctx = ship.js state. Stable objects/functions/constants are plain properties (captured once, at first use); state ship.js reassigns is read/written
//   through the accessor properties (S.mdx, S.keys, S.ps, ...), so there is exactly one copy of every variable.
import { hOf } from './ship-scale.js';

export function createFoot(ctx) {
    var S = ctx, THREE = ctx.THREE;
    var FOOT_SLIDE_A = ctx.FOOT_SLIDE_A, FOOT_SLIDE_K = ctx.FOOT_SLIDE_K, G_WALK = ctx.G_WALK, JET_HOLD = ctx.JET_HOLD, JET_UP = ctx.JET_UP, MOUSE_SENS = ctx.MOUSE_SENS;
    var R27 = ctx.R27, X = ctx.X, aPlay = ctx.aPlay, clamp = ctx.clamp, damp = ctx.damp, fl = ctx.fl, flHud = ctx.flHud, footFloor = ctx.footFloor, footWind = ctx.footWind;
    var fsC = ctx.fsC, fsC2 = ctx.fsC2, fsT = ctx.fsT, gA = ctx.gA, gB = ctx.gB, gC = ctx.gC, gD = ctx.gD, gE = ctx.gE, gQ = ctx.gQ, gQi = ctx.gQi;
    var groundCam = ctx.groundCam, hum = ctx.hum, jetFx = ctx.jetFx, mFloor = ctx.mFloor, mM = ctx.mM, objBegin = ctx.objBegin, objPush = ctx.objPush;
    var styEggTick = ctx.styEggTick, styU = ctx.styU, styUpW = ctx.styUpW, styWallO = ctx.styWallO, styleFx = ctx.styleFx, styleStep = ctx.styleStep, styleSync = ctx.styleSync, vTmp = ctx.vTmp;
    var G_RUN = 44, G_GRAV = 16, G_GRAV_UP = 7.93, G_JUMP = 4.36, FOOT_CAM = 6;   // in human heights (/s, /s^2); rev 29: walk = the old run speed, Shift run = 2x
    var FL_HOLD = 2, FL_V = 36;
    var JET_RAMP = 1.2, JET_DIVE = 16, DIVE_V0 = 3, DIVE_TAU = 0.6, DIVE_CAP = 48, stepPh = 0.6, camDip = 0;      // 2026-10-14 jump: apex 1.2 H in 0.55 s (v0 4.36 H/s, g_up 7.93, falling g 16); Shift dive v = v0 * 2^(t/0.6) capped at 48 H/s      // 2026-10-14: jetpack climb ramp (s to 3.5x), Shift dive speed (heights/s), footstep phase               // rev 29: hold Space this many s = jetpack FLIGHT; flight cruise speed (heights/s)
    var WALL_TAN = Math.tan(70 * Math.PI / 180);      // Q06: terrain steeper than 70 deg along the move counts as a wall
    var FOOT_SUB = 0.12, fsD = new THREE.Vector3();
    var styCam = 1, flF = new THREE.Vector3(), flA = new THREE.Vector3();
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
            if (hr <= sup) { hr = sup; touched = true; vn = v.dot(dir); if (vn < 0) v.addScaledVector(dir, -vn); }
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
        flA.copy(hum.hf).multiplyScalar(Math.cos(cpi)).addScaledVector(up, Math.sin(cpi)).negate().multiplyScalar(FOOT_CAM * H).addScaledVector(up, 0.7 * H);
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
        var wantJump = !S.cmdOpen && !!S.keys.Space;
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
        if (moving) hum.face.lerp(gB, damp(14, dt));
        // jump (once per press)
        if (!sty && !hum.air && wantJump && !S.jumpHeld) { hum.air = true; hum.vv = G_JUMP * H * S.fxJump; aPlay('jump', { vel: 0.6 }); }
        S.jumpHeld = wantJump;
        // rev 21 jetpack: hold Space in the air (after JET_HOLD s, so a tap is still a jump) = infinite, fast: climb JET_UP heights/s, forward thrust 2 x run
        if (hum.air && wantJump && !sty) S.jetT += dt; else S.jetT = 0;
        // 2026-10-14 (Emory: "space bar goes higher / faster, left shift pulls it back down"): the climb ramps 1x -> 3.5x over JET_RAMP s of holding Space;
        // Shift in the air (no style) dives at JET_DIVE heights/s and cuts the jet
        var dive = hum.air && !sty && !S.cmdOpen && !!(S.keys.ShiftLeft || S.keys.ShiftRight);
        if (dive) { if (!(hum.diveT > 0)) hum.diveV0 = Math.max(DIVE_V0, -hum.vv / H); hum.diveT = (hum.diveT || 0) + dt; } else hum.diveT = 0;
        var diveV = Math.min(DIVE_CAP, (hum.diveV0 || DIVE_V0) * Math.pow(2, Math.min(hum.diveT, 8) / DIVE_TAU));      // exponential in hold time, from the speed you already have
        var jet = S.jetT > JET_HOLD && !dive, jetK = 1 + 2.5 * clamp((S.jetT - JET_HOLD) / JET_RAMP, 0, 1);
        // substepped capsule motion in the local frame
        var spd = (sSpd >= 0 ? sSpd : (jet ? G_RUN * (1 + 0.12 * R27.su.jetpack) : (run ? G_RUN * (1 + 0.1 * R27.su.sprint) : G_WALK))) * H * S.fxSpeed, nSub = 1;
        if (moving) nSub = Math.max(nSub, Math.ceil(spd * dt / (FOOT_SUB * H)));
        if (hum.air) nSub = Math.max(nSub, Math.ceil(Math.abs(hum.vv) * dt / (0.25 * H)));
        nSub = Math.min(16, nSub);
        var h = dt / nSub, dir = fsD.copy(hum.pos).normalize(), hr = hum.hr, supNow = footFloor(dir, H, hr), opN = objBegin(), edgeG = !!(sr && sr.edgeGuard), wallHit = false, wallSl = false, wSx = 0, wSz = 0, wallNow = false, wRiseH = 0;
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
                if (jet) { hum.vv += (JET_UP * jetK * (1 + 0.2 * R27.su.jetpack) * H - hum.vv) * Math.min(1, 7 * h); if (hr > supNow + 400 * H) hum.vv = Math.min(hum.vv, 0); }
                else if (dive) hum.vv += (-diveV * H - hum.vv) * Math.min(1, 8 * h);
                else if (!sty) hum.vv -= (hum.vv > 0 ? G_GRAV_UP : G_GRAV) * H * h;
                hr += hum.vv * h;
                if (hr <= supNow && !(sty && hum.vv > 0)) { hr = supNow; hum.vv = 0; hum.air = false; }
            } else if (hr - supNow > 0.6 * H) { hum.air = true; hum.vv = 0; }
            else hr = supNow;
        }
        hum.wallT += dt;                                                // Q06: seconds since the last wall contact (styles read c.wallNormal inside 0.12 s)
        if (wallHit && moving) {
            hum.wallT = 0; hum.wallH = wRiseH; hum.wallX = -(gB.dot(gA)); hum.wallZ = gB.dot(hum.hf);
            if (wallSl && S.stylesMod && S.stylesMod.wallNormal2) { if (S.stylesMod.wallNormal2(gB.dot(gA), -gB.dot(hum.hf), wSx, wSz, styWallO)) { hum.wallX = styWallO.x; hum.wallZ = styWallO.z; } }
        }
        // 2026-10-14: ground-contact cues. Landing edge = footLand (impact from the fall speed) + a crouch above a hard drop; steps on a walk / run cadence.
        var landImp = -1;
        if (air0 && !hum.air) { landImp = clamp(-Math.min(vv0, hum.vv) / (18 * H), 0, 1); aPlay('footLand', { vel: 0.3 + 0.7 * landImp }); if (landImp > 0.35) fl.cr = Math.max(fl.cr, landImp); camDip = Math.max(camDip, (0.05 + 0.3 * landImp) * H); stepPh = 0.6; }
        else if (!hum.air && moving) { stepPh += dt * (run ? 3.6 : 2.4); if (stepPh >= 1) { stepPh -= 1; aPlay('step', { level: run ? 1 : 0, vel: run ? 0.6 : 0.45 }); } }
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
        o.group.position.copy(hum.w); o.group.scale.set(H, H * (1 - 0.22 * fl.cr), H);
        flHud(false, 0);
        if (hum.sAnim) { var ua = Object.assign(styU, hum.sAnim); ua.facing = 0; ua.jet = S.jetLvl; ua.scan = S.scanT > 0 ? Math.min(1, S.scanT / 1.2) : 0; o.update(dt, ua); }
        else o.update(dt, { moving: moving, running: run || jet, airborne: hum.air, speed: moving ? (run || jet ? 1 : 0.5) : 0, facing: 0, jet: S.jetLvl, scan: S.scanT > 0 ? Math.min(1, S.scanT / 1.2) : 0 });
        jetFx(dt, jet, H, qM, c, dir, supNow, hr);
        if (landImp > 0.35) S.fx.impact(hum.w, 0xc8b89a, 0.6 + landImp);
        // camera: 6 human heights back, pitched, never under the floor; it follows the SMOOTHED position (hum.w), not the raw step
        gA.copy(hum.hf).negate().multiplyScalar(Math.cos(hum.pitch)).addScaledVector(up, Math.sin(hum.pitch)).applyQuaternion(qM);   // world offset dir
        camDip *= Math.exp(-9 * dt); if (camDip < 1e-6) camDip = 0;
        gC.copy(gB).multiplyScalar(0.8 * H - camDip).add(hum.w);       // focus (head), dips a little on landing
        gD.copy(gC).addScaledVector(gA, FOOT_CAM * H * styCam);                 // wanted camera position
        vTmp.subVectors(gD, c);
        var cr = vTmp.length();
        gQi.copy(qM).invert(); fsT.copy(vTmp).applyQuaternion(gQi);
        var cfr = mFloor(fsT.x, fsT.y, fsT.z);
        if (cr < cfr + 0.04 * S.L) gD.copy(c).addScaledVector(vTmp.divideScalar(cr || 1), cfr + 0.04 * S.L);      // the rendered ground now equals the floor used here, so the clearance is just a little more than the near plane
        mM.lookAt(gD, gC, gB); gQ.setFromRotationMatrix(mM);
        groundCam(dt, gC, gD, gQ, 16);
        return gC;
    }
    return { footStep: footStep, flightStep: flightStep };
}
