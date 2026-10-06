// ship-styles.js — StyleModule locomotion state machines for the on-foot capsule (docs/mashup-plan.md §1). Pure maths: no DOM, no THREE, no Math.random/Date,
// no allocation in step() (reused out/fx objects). Working names MARO/JOSHI/STEEV/SONIK only; mechanics/ratios from public community docs, no third-party art/code.
//
// createStyle(id, env) -> { id, name, portraitHint, skin, abilities, hitbox, reset(), step(input, ctx, dt) -> out }
//   env  = { walk:22, grav:22, jump:8.5 }   units: human heights (H) and seconds; defaults = ship.js G_WALK / G_GRAV / G_JUMP
//   input= { keys:{KeyW..}, mouse:{l,r}, mdx, mdy, camYaw (rad; wish = keys rotated by it) | wx,wz (pre-resolved tangent-frame wish, overrides keys) }
//   ctx  = { onGround, floorNormal:{x,y,z}, slope (deg), wallNormal?:{x,y,z}, speed, airTime, facing, mouse,
//            targets?:[{x,y,z}] relative to the player (SONIK homing), target?:{dist,mat,hardness,cx,cy,cz,px,py,pz} (STEEV ray), hostileNear?, fuse?, aim?:{x,y,z} }
//   out  = { vel:{x,y,z} (H/s, local tangent frame, y = up, forward at yaw f is (-sin f, -cos f)), jumpImpulse, groundSnap, anim:{...rig state + extras},
//            fx:[{type,x,y,z,dx,dy,dz,v,n,dmg,r,tag}], camHint:{fov,dist,shake}, caution, edgeGuard }   (out and everything in it is REUSED each call: copy what you keep)
// The style owns vel completely (incl. vertical and gravity); the controller moves the capsule by vel*dt, applies collisions, and reports onGround/wallNormal back.
// NUMBERS: [doc] = from the cited public docs (ratios/constants converted); [guess] = ours, tune by playing.

const D2R = Math.PI / 180, R2D = 180 / Math.PI;
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const wrapA = a => { while (a > Math.PI) a -= 2 * Math.PI; while (a < -Math.PI) a += 2 * Math.PI; return a; };

// ── shared plumbing ──────────────────────────────────────────────────────────────────────────────
function mkIO() { return { wx: 0, wz: 0, m: 0, jump: false, jumpP: false, crouch: false, crouchP: false, run: false, L: false, LP: false, R: false, RP: false, F: false, FP: false, fwdP: false, slot: -1, pj: false, pc: false, pL: false, pR: false, pF: false, pW: false }; }
function readIn(io, input, ctx) {
  const k = input.keys || EMPTY, ms = input.mouse || ctx.mouse || EMPTY;
  io.pj = io.jump; io.pc = io.crouch; io.pL = io.L; io.pR = io.R; io.pF = io.F; io.pW = io.fwdH;
  io.jump = !!k.Space; io.crouch = !!(k.KeyC || k.ControlLeft || k.ControlRight); io.run = !!(k.ShiftLeft || k.ShiftRight);
  io.L = !!(ms.l || ms.left); io.R = !!(ms.r || ms.right); io.F = !!k.KeyF; io.fwdH = !!k.KeyW;
  io.jumpP = io.jump && !io.pj; io.crouchP = io.crouch && !io.pc; io.LP = io.L && !io.pL; io.RP = io.R && !io.pR; io.FP = io.F && !io.pF; io.fwdP = io.fwdH && !io.pW;
  io.slot = -1; for (let i = 1; i <= 9; i++) if (k['Digit' + i]) { io.slot = i - 1; break; }
  let wx, wz;
  if (input.wx != null) { wx = input.wx; wz = input.wz || 0; }
  else {
    const f = (k.KeyW ? 1 : 0) - (k.KeyS ? 1 : 0), s = (k.KeyD ? 1 : 0) - (k.KeyA ? 1 : 0), c = input.camYaw || 0, sn = Math.sin(c), cs = Math.cos(c);
    wx = -sn * f + cs * s; wz = -cs * f - sn * s;
  }
  let m = Math.sqrt(wx * wx + wz * wz); if (m > 1) { wx /= m; wz /= m; m = 1; }
  io.wx = wx; io.wz = wz; io.m = m;
}
const EMPTY = Object.freeze({});

function mkFx() {
  const pool = [], N = 16; for (let i = 0; i < N; i++) pool.push({ type: '', x: 0, y: 0, z: 0, dx: 0, dy: 0, dz: 0, v: 0, n: 0, dmg: 0, r: 0, tag: '' });
  let i = 0;
  return { list: [], clear() { this.list.length = 0; }, emit(type) { const e = pool[i]; i = (i + 1) % N; e.type = type; e.x = e.y = e.z = e.dx = e.dy = e.dz = e.v = e.n = e.dmg = e.r = 0; e.tag = ''; this.list.push(e); return e; } };
}
function mkOut(fx) {
  return {
    vel: { x: 0, y: 0, z: 0 }, jumpImpulse: 0, groundSnap: true, fx: fx.list, caution: false, edgeGuard: false,
    anim: { moving: false, running: false, airborne: false, speed: 0, facing: 0, jet: 0, scan: 0, mode: 0, flip: 0, flipKind: 0, crouch: 0, pitch: 0, ball: 0, stamina: 1, tongue: 0, mine: 0, charge: 0, flutter: 0, sprint: 0, caution: 0, slot: 0, hurt: 0 },
    camHint: { fov: 1, dist: 1, shake: 0 },
  };
}
// shared horizontal helpers (operate on a state object s with vx,vz,face)
function approach(s, dx, dz, rate, dt) {
  const ex = dx - s.vx, ez = dz - s.vz, d = Math.sqrt(ex * ex + ez * ez), st = rate * dt;
  if (d <= st || d < 1e-9) { s.vx = dx; s.vz = dz; } else { s.vx += ex / d * st; s.vz += ez / d * st; }
}
function brake(s, rate, dt) { const sp = Math.sqrt(s.vx * s.vx + s.vz * s.vz), d = rate * dt; if (sp <= d) { s.vx = s.vz = 0; } else { const k = (sp - d) / sp; s.vx *= k; s.vz *= k; } }
function turn(s, io, dt, rate) { if (io.m > 0.05) { const t = Math.atan2(-io.wx, -io.wz), d = wrapA(t - s.face), st = rate * dt; s.face = wrapA(s.face + clamp(d, -st, st)); } }
function airSteer(s, io, dt, acc, cap) { // accelerate toward wish but never beyond max(current speed, cap)
  if (io.m < 0.05) return;
  const before = Math.sqrt(s.vx * s.vx + s.vz * s.vz), lim = Math.max(before, cap);
  s.vx += io.wx * io.m * acc * dt; s.vz += io.wz * io.m * acc * dt;
  const sp = Math.sqrt(s.vx * s.vx + s.vz * s.vz); if (sp > lim) { const k = lim / sp; s.vx *= k; s.vz *= k; }
}
function dirOfVel(s, o) { const sp = Math.sqrt(s.vx * s.vx + s.vz * s.vz); if (sp > 0.3) { o.x = s.vx / sp; o.z = s.vz / sp; } else { o.x = -Math.sin(s.face); o.z = -Math.cos(s.face); } return sp; }

// ── MARO: 3D-platformer runner [doc S1 ratios] ────────────────────────────────────────────────────
const M_G = 0, M_AIR = 1, M_SLIDE = 3, M_DIVE = 4, M_BELLY = 5, M_HANG = 6, M_POUND = 7;
const CHAIN_V = [1, 1.238, 1.643];                 // [doc] initial jump speeds 42/52/69 -> v ratios (apex 1/1.53/2.7)
function maro(env) {
  const W = env.walk, g = env.grav, J = env.jump, RUN = 1.5 * W, A = RUN / 0.35;   // [guess] run top 1.5x walk, reach top in 0.35 s; [doc] decel 2x accel
  const fx = mkFx(), out = mkOut(fx), io = mkIO(), a = out.anim, dd = { x: 0, z: 0 };
  const s = { vx: 0, vz: 0, vy: 0, face: 0, fi: false, mode: M_AIR, chain: 0, landAge: 9, jb: 0, coyote: 0, skidT: 0, skidHs: 0, wallT: 0, wnx: 0, wnz: 0, wdx: 0, wdz: 0, wspd: 0, hang: 0, stun: 0, combo: 0, comboT: 0, punchCd: 0, jgrace: 0, was: false, flipT: 1, flipKind: 0, ctrl: 1, kicks: 0 };
  function reset() { Object.assign(s, { vx: 0, vz: 0, vy: 0, face: 0, fi: false, mode: M_AIR, chain: 0, landAge: 9, jb: 0, coyote: 0, skidT: 0, skidHs: 0, wallT: 0, hang: 0, stun: 0, combo: 0, comboT: 0, punchCd: 0, jgrace: 0, was: false, flipT: 1, flipKind: 0, ctrl: 1, kicks: 0 }); io.jump = io.crouch = io.L = io.R = io.F = io.fwdH = false; }
  function doJump(hs) {
    let vy, kind = 1;
    if (io.crouch && hs > 0.6 * W && s.stun <= 0) { vy = J * 0.775; const sp = dirOfVel(s, dd); const t = Math.max(sp, W) * 1.8; s.vx = dd.x * t; s.vz = dd.z * t; s.chain = 0; kind = 4; s.ctrl = 0.15; s.flipKind = 0; }   // [doc] long jump: low arc (apex 0.6), 1.8x horizontal
    else if (io.crouch) { vy = J * 1.304; s.vx = s.vz = 0; s.chain = 0; kind = 5; s.flipKind = 2; s.flipT = 0; }                                          // [doc] backflip apex 1.7
    else if (s.skidT > 0 && s.skidHs > 0.5 * W) { vy = J * 1.14; const t = 0.8 * s.skidHs; s.vx = io.wx * t; s.vz = io.wz * t; if (io.m > 0.05) s.face = Math.atan2(-io.wx, -io.wz); s.chain = 0; kind = 6; s.flipKind = 3; s.flipT = 0; s.skidT = 0; }  // [doc] side flip apex 1.3
    else if (s.mode === M_BELLY) { vy = J * 0.9; kind = 7; s.chain = 0; }                                                                                  // [doc] hop out of belly slide
    else if (s.mode === M_SLIDE) { vy = J; kind = 1; s.chain = 0; }
    else {
      let c = (s.landAge < 0.4 && s.chain > 0 && hs > 0.35 * W) ? s.chain + 1 : 1;                                                                           // [doc] ~0.4 s window, must be moving
      if (c >= 3 && hs < 0.6 * W) c = 2; if (c > 3) c = 1;
      s.chain = c; vy = J * CHAIN_V[c - 1]; kind = c; if (c >= 2) { s.flipKind = 1; s.flipT = 0; }
    }
    s.vy = vy; out.jumpImpulse = vy; s.mode = M_AIR; s.jgrace = 0.06; s.jb = 0; s.coyote = 0;
    const e = fx.emit('jump'); e.n = kind; e.v = vy;
  }
  function step(input, ctx, dt) {
    readIn(io, input, ctx); fx.clear(); out.jumpImpulse = 0;
    if (!s.fi) { s.face = ctx.facing || 0; s.fi = true; }
    const grounded = ctx.onGround && s.jgrace <= 0; s.jgrace = Math.max(0, s.jgrace - dt);
    const n = ctx.floorNormal, slope = ctx.slope != null ? ctx.slope : (n ? Math.acos(clamp(n.y, -1, 1)) * R2D : 0);
    s.landAge += dt; s.comboT -= dt; s.wallT -= dt; s.jb -= dt; s.coyote -= dt; s.skidT -= dt; s.stun -= dt; s.punchCd -= dt;
    if (io.jumpP) s.jb = 0.12;
    if (grounded && !s.was) {                                         // touchdown
      s.landAge = 0; s.ctrl = 1;
      if (s.mode === M_DIVE) { s.mode = M_BELLY; s.chain = 0; }
      else if (s.mode === M_POUND) { const e = fx.emit('pound'); e.dmg = 12; e.r = 2; s.stun = 0.3; s.vx = s.vz = 0; s.mode = M_G; s.chain = 0; }   // [doc/plan] 12 dmg, 2 H radius
      else { s.mode = M_G; const e = fx.emit('land'); e.v = -s.vy; }
      s.vy = 0;
    } else if (!grounded && s.was && s.mode !== M_AIR && s.mode < M_DIVE) { s.mode = M_AIR; s.coyote = 0.08; }
    s.was = grounded;
    let hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz);

    if (grounded) {
      if (s.mode === M_G && slope > 30) s.mode = M_SLIDE; else if (s.mode === M_SLIDE && slope < 26) s.mode = M_G;   // forced slide on > 30 deg (hysteresis)
      if (s.jb > 0 && s.stun <= 0) { doJump(hs); }
      else if (s.mode === M_G) {
        const top = io.crouch ? W * 0.3 : (io.run ? RUN : W);
        if (s.stun > 0) brake(s, 3 * A, dt);
        else if (io.m > 0.05) {
          if (s.skidT <= 0 && hs > 0.6 * top && s.vx * io.wx + s.vz * io.wz < -0.866 * hs * io.m) { s.skidT = 0.25; s.skidHs = hs; }   // [doc] skid on ~150 deg turn above 60% speed
          if (s.skidT > 0) brake(s, 3 * A, dt);
          else { const tx = io.wx * top * io.m, tz = io.wz * top * io.m; approach(s, tx, tz, (tx * tx + tz * tz < hs * hs) ? 2 * A : A, dt); turn(s, io, dt, 16); }
        } else brake(s, 2 * A, dt);
        if (io.LP && s.punchCd <= 0 && s.stun <= 0) {                // jab-jab-kick 6/6/10 [plan]
          s.combo = s.comboT > 0 ? (s.combo % 3) + 1 : 1; s.comboT = 0.5; s.punchCd = 0.2;
          const e = fx.emit('melee'); e.n = s.combo; e.dmg = s.combo === 3 ? 10 : 6; e.dx = -Math.sin(s.face); e.dz = -Math.cos(s.face); e.r = 0.9;
        }
        if (io.crouch) s.chain = 0;
      } else if (s.mode === M_SLIDE) {
        const nx = n ? n.x : 0, nz = n ? n.z : 0, dl = Math.sqrt(nx * nx + nz * nz) || 1;
        const sinS = Math.sin(slope * D2R);
        s.vx += nx / dl * g * sinS * 1.6 * dt; s.vz += nz / dl * g * sinS * 1.6 * dt;        // [guess] keeps speed, accelerates downhill
        s.vx += io.wx * io.m * 0.35 * A * dt; s.vz += io.wz * io.m * 0.35 * A * dt;
        hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz); if (hs > 2.2 * W) { const k = 2.2 * W / hs; s.vx *= k; s.vz *= k; }
        if (hs > 0.5) s.face = Math.atan2(-s.vx, -s.vz);
        if (io.LP) { /* no punching on a slide */ }
      } else if (s.mode === M_BELLY) {
        brake(s, 0.6 * W, dt); hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz);            // low friction
        if (hs < 0.25 * W) s.mode = M_G;
      }
      if (out.jumpImpulse === 0) s.vy = 0;
    } else {
      // airborne
      if (s.coyote > 0 && s.jb > 0 && s.mode === M_AIR && s.vy <= 0) { doJump(hs); }     // coyote jump
      if (ctx.wallNormal) {
        const wx = ctx.wallNormal.x, wz = ctx.wallNormal.z, wl = Math.sqrt(wx * wx + wz * wz);
        if (wl > 0.1 && s.mode !== M_HANG && s.mode !== M_POUND) {
          if (s.wallT <= 0) { s.wdx = hs > 0.3 ? s.vx / hs : -Math.sin(s.face); s.wdz = hs > 0.3 ? s.vz / hs : -Math.cos(s.face); s.wspd = hs; }
          s.wallT = 0.2; s.wnx = wx / wl; s.wnz = wz / wl;                                // [doc] kick window ~0.2 s after contact
          const into = s.vx * s.wnx + s.vz * s.wnz; if (into < 0) { s.vx -= into * s.wnx; s.vz -= into * s.wnz; }
        }
      }
      if (s.jb > 0 && s.wallT > 0 && s.mode !== M_HANG && s.mode !== M_POUND) {                // wall kick: reflect, up 1.0, chainable
        const dn = s.wdx * s.wnx + s.wdz * s.wnz; let rx, rz;
        if (dn >= 0) { rx = s.wnx; rz = s.wnz; } else { rx = s.wdx - 2 * dn * s.wnx; rz = s.wdz - 2 * dn * s.wnz; }
        const rl = Math.sqrt(rx * rx + rz * rz) || 1, sp = Math.max(s.wspd, 0.8 * W);
        s.vx = rx / rl * sp; s.vz = rz / rl * sp; s.vy = J; out.jumpImpulse = J; s.face = Math.atan2(-s.vx, -s.vz);
        s.mode = M_AIR; s.wallT = 0; s.jb = 0; s.kicks++; s.flipKind = 4; s.flipT = 0; s.chain = 0; s.ctrl = 0.6;
        const e = fx.emit('wallKick'); e.n = s.kicks; e.dx = s.wnx; e.dz = s.wnz; e.v = sp;
      }
      if (s.mode === M_AIR && io.RP) {                                                       // dive: 1.6x forward, pitched down [doc]
        const sp = dirOfVel(s, dd), t = Math.max(sp, W) * 1.6; s.vx = dd.x * t; s.vz = dd.z * t; s.vy = Math.max(s.vy, 0) * 0 + J * 0.3; s.mode = M_DIVE; s.chain = 0;
        s.face = Math.atan2(-dd.x, -dd.z); const e = fx.emit('dive'); e.v = t;
      } else if ((s.mode === M_AIR || s.mode === M_DIVE) && io.crouchP) {                    // ground pound: 0.25 s hang, then 2.5x grav [doc/plan]
        s.mode = M_HANG; s.hang = 0.25; s.chain = 0; fx.emit('poundStart');
      }
      if (s.mode === M_HANG) { s.vx = s.vz = s.vy = 0; s.hang -= dt; if (s.hang <= 0) { s.mode = M_POUND; s.vy = -J * 1.5; } }
      else if (s.mode === M_POUND) { s.vy -= g * 2.5 * dt; s.vy = Math.max(s.vy, -70); brake(s, 6 * W, dt); }
      else {
        if (s.mode === M_AIR) { s.ctrl = Math.min(1, s.ctrl + dt * 1.2); airSteer(s, io, dt, 0.55 * A * s.ctrl, io.run ? RUN : W); turn(s, io, dt, 8); }
        s.vy -= g * dt; s.vy = Math.max(s.vy, -40);
      }
    }
    // outputs
    hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz); const gm = s.mode === M_G || s.mode === M_SLIDE || s.mode === M_BELLY;
    if (s.flipT < 1) s.flipT = Math.min(1, s.flipT + dt / 0.6);
    out.vel.x = s.vx; out.vel.z = s.vz; out.vel.y = (grounded && out.jumpImpulse === 0) ? 0 : s.vy; out.groundSnap = grounded && out.jumpImpulse === 0;
    a.moving = hs > 0.5; a.running = hs > W * 1.15; a.airborne = !grounded; a.speed = clamp(hs / RUN, 0, 1); a.facing = s.face; a.mode = s.mode;
    a.flip = s.flipT < 1 ? s.flipT : 0; a.flipKind = s.flipKind; a.crouch = (io.crouch && grounded) || s.stun > 0 || s.mode === M_BELLY ? 1 : 0; a.pitch = s.mode === M_DIVE || s.mode === M_BELLY ? 1 : 0;
    out.camHint.fov = 1 + 0.06 * clamp(hs / RUN, 0, 1.4) + (s.chain === 3 && !grounded ? 0.04 : 0); out.camHint.dist = s.chain === 3 && !grounded ? 1.15 : 1; out.camHint.shake = s.mode === M_POUND ? 0.3 : 0;
    return out;
  }
  return { reset, step, state: s, out };
}

// ── JOSHI: flutter jumper with eggs + tongue stub [plan; no public numbers] ───────────────────────
function joshi(env) {
  const W = env.walk, g = env.grav, J = env.jump, A = W / 0.3;                       // [guess] run 1.0x, no sprint
  const fx = mkFx(), out = mkOut(fx), io = mkIO(), a = out.anim;
  const s = { vx: 0, vz: 0, vy: 0, face: 0, fi: false, air: true, jb: 0, coyote: 0, jgrace: 0, was: false, fn: 0, fa: false, ft: 0, eggs: 3, eggCd: 0, tongue: -1, hang: 0, pound: 0, stun: 0 };
  const FL_N = 3, FL_T = 0.3;                                                          // [plan] 0.9 s total = 3 x 0.3 s
  function reset() { Object.assign(s, { vx: 0, vz: 0, vy: 0, face: 0, fi: false, air: true, jb: 0, coyote: 0, jgrace: 0, was: false, fn: 0, fa: false, ft: 0, eggs: 3, eggCd: 0, tongue: -1, hang: 0, pound: 0, stun: 0 }); io.jump = io.crouch = io.L = io.R = io.F = false; }
  function step(input, ctx, dt) {
    readIn(io, input, ctx); fx.clear(); out.jumpImpulse = 0;
    if (!s.fi) { s.face = ctx.facing || 0; s.fi = true; }
    const grounded = ctx.onGround && s.jgrace <= 0; s.jgrace = Math.max(0, s.jgrace - dt);
    s.jb -= dt; s.coyote -= dt; s.eggCd -= dt; s.stun -= dt;
    if (io.jumpP) s.jb = 0.12;
    if (grounded && !s.was) { s.air = false; s.fn = 0; s.fa = false; s.vy = 0; if (s.pound === 2) { const e = fx.emit('pound'); e.dmg = 8; e.r = 1.6; e.v = 1; s.stun = 0.25; s.vx = s.vz = 0; } else { const e = fx.emit('land'); e.v = -s.vy; } s.pound = 0; }
    if (!grounded && s.was) { s.air = true; s.coyote = 0.1; }
    s.was = grounded;
    let hs;
    if (grounded) {
      if (s.jb > 0 && s.stun <= 0) { s.vy = J; out.jumpImpulse = J; s.air = true; s.jgrace = 0.06; s.jb = 0; const e = fx.emit('jump'); e.n = 1; e.v = J; }
      else {
        if (s.stun > 0) brake(s, 3 * A, dt);
        else if (io.m > 0.05) { approach(s, io.wx * W * io.m, io.wz * W * io.m, A, dt); turn(s, io, dt, 14); } else brake(s, 2 * A, dt);
        s.vy = 0;
      }
    } else {
      if (s.coyote > 0 && s.jb > 0 && s.vy <= 0 && s.fn === 0 && !s.fa) { s.vy = J; out.jumpImpulse = J; s.jgrace = 0.06; s.jb = 0; s.coyote = 0; const e = fx.emit('jump'); e.n = 1; e.v = J; }
      airSteer(s, io, dt, 0.9 * A, W); turn(s, io, dt, 10);                          // [plan] air control 0.9 of ground accel
      if (s.pound === 0 && io.crouchP) { s.pound = 1; s.hang = 0.2; fx.emit('poundStart'); }     // butt stomp
      if (s.pound === 1) { s.vx = s.vz = s.vy = 0; s.hang -= dt; if (s.hang <= 0) { s.pound = 2; s.vy = -J * 1.3; } }
      else if (s.pound === 2) { s.vy -= g * 2.2 * dt; brake(s, 6 * W, dt); }
      else {
        // flutter: after the apex, hold Space; 3 bursts of 0.3 s (gravity x0.12 + a small kick), chained automatically while held, cancelled on release
        if (io.jump && s.vy <= 0.2 * J && !s.fa && s.fn < FL_N) { s.fa = true; s.ft = FL_T; s.fn++; s.vy = Math.max(s.vy, 0) + J * 0.18; const e = fx.emit('flutter'); e.n = s.fn; }
        if (s.fa) { if (!io.jump) s.fa = false; else { s.ft -= dt; if (s.ft <= 0) s.fa = false; } }
        s.vy -= g * (s.fa ? 0.12 : 1) * dt; s.vy = Math.max(s.vy, -40);
      }
    }
    // egg throw [plan: 8 dmg, 2 bounces, needs an egg]; the game turns this event into a projectile
    if (io.LP && s.eggs > 0 && s.eggCd <= 0 && s.stun <= 0) {
      s.eggs--; s.eggCd = 0.3; const e = fx.emit('egg'); const ax = ctx.aim; let dx, dy, dz;
      if (ax) { dx = ax.x; dy = ax.y; dz = ax.z; } else { const p = 0.35, cp = Math.cos(p); dx = -Math.sin(s.face) * cp; dz = -Math.cos(s.face) * cp; dy = Math.sin(p); }
      e.dx = dx; e.dy = dy; e.dz = dz; e.v = 30; e.dmg = 8; e.n = 2; e.tag = 'egg';             // v [guess] 30 H/s; n = bounces
    }
    // tongue stub: mouse2 = 0.25 s out (emits a grab query at full reach 6 H), 0.25 s in; the game answers via swallow(n)
    if (s.tongue < 0 && io.R && s.stun <= 0) { s.tongue = 0; }
    if (s.tongue >= 0) {
      const t0 = s.tongue; s.tongue += dt;
      if (t0 < 0.25 && s.tongue >= 0.25) { const e = fx.emit('tongue'); e.r = 6; e.dx = -Math.sin(s.face); e.dz = -Math.cos(s.face); }
      if (s.tongue >= 0.5) s.tongue = -1;
    }
    hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz);
    out.vel.x = s.vx; out.vel.z = s.vz; out.vel.y = (grounded && out.jumpImpulse === 0) ? 0 : s.vy; out.groundSnap = grounded && out.jumpImpulse === 0;
    a.moving = hs > 0.5; a.running = false; a.airborne = !grounded; a.speed = clamp(hs / W, 0, 1); a.facing = s.face; a.mode = s.pound ? 2 : s.fa ? 1 : 0;
    a.flutter = s.fa ? 1 : 0; a.stamina = (FL_N - s.fn + (s.fa ? s.ft / FL_T : 0)) / FL_N; if (a.stamina > 1) a.stamina = 1;
    a.tongue = s.tongue < 0 ? 0 : (s.tongue < 0.25 ? s.tongue / 0.25 : (0.5 - s.tongue) / 0.25); a.crouch = s.stun > 0 ? 1 : 0;
    out.camHint.fov = 1; out.camHint.dist = 1; out.camHint.shake = s.pound === 2 ? 0.2 : 0;
    return out;
  }
  return { reset, step, state: s, out, swallow(n) { s.eggs = Math.min(9, s.eggs + (n | 0)); }, get eggs() { return s.eggs; },
    previewArc(buf, ctx) {                                                            // 8 points (x,y,z relative) of the egg arc for the dotted-line preview; buf = Float32Array(24)
      const ax = ctx && ctx.aim; let dx, dy, dz; if (ax) { dx = ax.x; dy = ax.y; dz = ax.z; } else { const p = 0.35, cp = Math.cos(p); dx = -Math.sin(s.face) * cp; dz = -Math.cos(s.face) * cp; dy = Math.sin(p); }
      for (let i = 0; i < 8; i++) { const t = (i + 1) * 0.1; buf[i * 3] = dx * 30 * t; buf[i * 3 + 1] = dy * 30 * t - 0.5 * g * 0.6 * t * t; buf[i * 3 + 2] = dz * 30 * t; }
      return buf;
    } };
}

// ── STEEV: block-world walker/builder [doc S3 ratios] ─────────────────────────────────────────────
const HOTBAR = ['dirt', 'stone', 'plank', 'table', 'torch', 'door', 'glass', 'brick', 'wool'];
function steev(env) {
  const W = env.walk, g = env.grav, JS = Math.sqrt(2 * g * 1.0), A = W / 0.12;      // [guess] apex 1.0 H (plan: 0.5 H + tune); [doc] walk 4.317 / sprint 5.612 = 1.3x, sprint-jump flat 1.65x [plan]
  const BLK = 0.4, REACH = 5 * BLK;                                                  // [doc] reach 5 blocks, block = 0.4 H [plan]
  const fx = mkFx(), out = mkOut(fx), io = mkIO(), a = out.anim, dd = { x: 0, z: 0 };
  const s = { vx: 0, vz: 0, vy: 0, face: 0, fi: false, jgrace: 0, was: false, sprint: false, tapT: 9, mineT: 0, mx: 0, my: 0, mz: 0, mineCd: 0, placeCd: 0, slot: 0, tool: 1, airSp: 0, sprintJump: false };
  const hot = env.hotbar || HOTBAR;
  function reset() { Object.assign(s, { vx: 0, vz: 0, vy: 0, face: 0, fi: false, jgrace: 0, was: false, sprint: false, tapT: 9, mineT: 0, mineCd: 0, placeCd: 0, slot: 0, tool: 1, airSp: 0, sprintJump: false }); io.jump = io.crouch = io.L = io.R = io.F = io.fwdH = false; }
  function step(input, ctx, dt) {
    readIn(io, input, ctx); fx.clear(); out.jumpImpulse = 0;
    if (!s.fi) { s.face = ctx.facing || 0; s.fi = true; }
    const grounded = ctx.onGround && s.jgrace <= 0; s.jgrace = Math.max(0, s.jgrace - dt);
    s.tapT += dt; s.mineCd -= dt; s.placeCd -= dt;
    if (io.slot >= 0) s.slot = io.slot;
    // sprint: Shift held, or double-tap W within 0.3 s; ends when W is released / movement stops
    if (io.fwdP) { if (s.tapT < 0.3) s.sprint = true; s.tapT = 0; }
    if (io.run && io.m > 0.05) s.sprint = true;
    if (io.m < 0.05 || io.crouch) s.sprint = false;
    const top = io.crouch ? W * 0.3 : (s.sprint ? 1.3 * W : W);                      // [doc] sneak 0.3, sprint 1.3
    if (grounded && !s.was) { s.vy = 0; s.sprintJump = false; const e = fx.emit('land'); e.v = 0; }
    s.was = grounded;
    if (grounded) {
      if (io.jump) {                                                                  // holding Space auto-hops, like the original
        s.vy = JS; out.jumpImpulse = JS; s.jgrace = 0.06; s.sprintJump = s.sprint; if (s.sprint) { dirOfVel(s, dd); const t = 1.65 * W; s.vx = (io.m > 0.05 ? io.wx : dd.x) * t; s.vz = (io.m > 0.05 ? io.wz : dd.z) * t; }
        const e = fx.emit('jump'); e.n = s.sprint ? 2 : 1; e.v = JS;
      } else {
        if (io.m > 0.05) { approach(s, io.wx * top * io.m, io.wz * top * io.m, A, dt); turn(s, io, dt, 20); } else brake(s, 2.5 * A, dt);
        s.vy = 0;
      }
    } else {
      airSteer(s, io, dt, s.sprintJump ? 0.12 * A : 0.25 * A, s.sprintJump ? 1.65 * W : top); turn(s, io, dt, 8);    // flat momentum in the air (no drag)
      s.vy -= g * dt; s.vy = Math.max(s.vy, -40);
    }
    // block interaction through ctx.target (ray result supplied by the game)
    const T = ctx.target, ok = T && T.dist != null && T.dist <= REACH;
    if (io.L && ok && T.mat !== 'air') {
      if (s.mineT === 0 || s.mx !== T.cx || s.my !== T.cy || s.mz !== T.cz) { s.mx = T.cx; s.my = T.cy; s.mz = T.cz; s.mineT = 0.0001; fx.emit('mineStart').tag = T.mat || ''; }
      const hard = T.hardness != null ? T.hardness : 0.75;                            // [doc] dirt ~0.75 s by hand
      if (s.mineCd <= 0) { s.mineT += dt * s.tool; if (s.mineT >= hard) { const e = fx.emit('blockBreak'); e.x = T.cx; e.y = T.cy; e.z = T.cz; e.tag = T.mat || ''; e.v = hard; s.mineT = 0; s.mineCd = 0.25; } }
    } else s.mineT = 0;
    if (ok && (io.RP || (io.R && s.placeCd <= 0)) && T.px != null) {                  // place the selected hotbar block on the face cell; 4/s while held
      const e = fx.emit('blockPlace'); e.x = T.px; e.y = T.py; e.z = T.pz; e.n = s.slot; e.tag = hot[s.slot] || 'dirt'; s.placeCd = 0.25;
    }
    if (io.FP && ok && T.mat === 'table') { const e = fx.emit('craft'); e.x = T.cx; e.y = T.cy; e.z = T.cz; e.tag = 'table'; }   // crafting-table ability -> game opens ship-craft 3x3
    const hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz);
    out.caution = (ctx.hostileNear != null && ctx.hostileNear < 7) || (ctx.fuse != null && ctx.fuse > 0);                   // creeper-ish: hostile close / fuse hissing -> HUD/camera nudge
    out.edgeGuard = io.crouch && grounded;                                          // sneak: controller should refuse to walk off ledges
    out.vel.x = s.vx; out.vel.z = s.vz; out.vel.y = (grounded && out.jumpImpulse === 0) ? 0 : s.vy; out.groundSnap = grounded && out.jumpImpulse === 0;
    a.moving = hs > 0.5; a.running = s.sprint; a.airborne = !grounded; a.speed = clamp(hs / (1.65 * W), 0, 1); a.facing = s.face; a.mode = s.sprint ? 1 : 0;
    a.crouch = io.crouch && grounded ? 1 : 0; a.mine = s.mineT > 0 ? Math.min(1, s.mineT / (T && T.hardness != null ? T.hardness : 0.75)) : 0; a.sprint = s.sprint ? 1 : 0; a.caution = out.caution ? 1 : 0; a.slot = s.slot;
    out.camHint.fov = 1 + (s.sprint ? 0.08 : 0); out.camHint.dist = 1; out.camHint.shake = 0;
    return out;
  }
  return { reset, step, state: s, out, setTool(t) { s.tool = t; } };
}

// ── SONIK: momentum runner [doc S4 constants, px/frame@60 converted] ─────────────────────────────
function sonik(env) {
  const W = env.walk, g = env.grav, J = env.jump;
  const TOP = 1.5 * W, SC = TOP / 6;                                                 // [guess] top 1.5x walk; SC = H/s per (px/frame) so every guide constant scales with it
  const ACC = 0.046875 * SC * 60, DEC = 0.5 * SC * 60, FRC = 0.046875 * SC * 60, AIRA = 0.09375 * SC * 60;     // [doc]
  const SLP = 0.125 * SC * 60, SLP_UP = 0.078125 * SC * 60, SLP_DN = 0.3125 * SC * 60;                       // [doc] slope factors (stand / roll uphill / roll downhill)
  const RFRC = 0.0234375 * SC * 60, RDEC = 0.125 * SC * 60;                                                  // [doc] roll friction / decel
  const FALL = 2.5 * SC, ROLLMIN = 1.03 * SC, UNROLL = 0.5 * SC, VMAX = 2.6 * TOP;                          // [doc] fall-off 2.5, roll start 1.03, unroll 0.5; max [guess]
  const HOME_SP = 2 * W, HOME_T = 0.35, HOME_R = 8;                                  // [plan] 8 H range, 0.35 s; speed [guess] 2x walk
  const fx = mkFx(), out = mkOut(fx), io = mkIO(), a = out.anim, hitbox = { r: 0.2, h: 0.9, rolling: false };
  const s = { gsp: 0, vx: 0, vy: 0, vz: 0, face: 0, fi: false, air: true, roll: false, charge: -1, lock: 0, jb: 0, jgrace: 0, was: false, jumped: false, homing: 0, hrx: 0, hry: 0, hrz: 0, hused: false, tx: 0, ty: 0, tz: 0, ny: 1, relCut: false, hitIdx: -1 };
  function reset() { Object.assign(s, { gsp: 0, vx: 0, vy: 0, vz: 0, face: 0, fi: false, air: true, roll: false, charge: -1, lock: 0, jb: 0, jgrace: 0, was: false, jumped: false, homing: 0, hused: false, relCut: false, hitIdx: -1 }); hitbox.rolling = false; hitbox.h = 0.9; io.jump = io.crouch = io.L = io.R = io.F = false; }
  // tangent along the heading on the floor plane (writes s.tx/ty/tz)
  function tangent(n) {
    const hx = -Math.sin(s.face), hz = -Math.cos(s.face);
    let nx = 0, ny = 1, nz = 0; if (n) { nx = n.x; ny = n.y; nz = n.z; }
    const d = hx * nx + hz * nz; let tx = hx - d * nx, ty = -d * ny, tz = hz - d * nz; const l = Math.sqrt(tx * tx + ty * ty + tz * tz) || 1;
    s.tx = tx / l; s.ty = ty / l; s.tz = tz / l; s.ny = ny;
  }
  function step(input, ctx, dt) {
    readIn(io, input, ctx); fx.clear(); out.jumpImpulse = 0;
    if (!s.fi) { s.face = ctx.facing || 0; s.fi = true; }
    const grounded = ctx.onGround && s.jgrace <= 0 && s.homing <= 0; s.jgrace = Math.max(0, s.jgrace - dt);
    const n = ctx.floorNormal, slope = ctx.slope != null ? ctx.slope : (n ? Math.acos(clamp(n.y, -1, 1)) * R2D : 0);
    s.jb -= dt; s.lock -= dt; if (io.jumpP) s.jb = 0.1;
    if (grounded && !s.was) {                                                        // landing: horizontal speed becomes ground speed along the new tangent
      s.air = false; s.hused = false; s.jumped = false; s.homing = 0;
      const hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz);
      if (hs > 1) s.face = Math.atan2(-s.vx, -s.vz);
      tangent(n); s.gsp = hs * (hs > 1 ? 1 : 0) + (s.vy < 0 && slope > 25 ? -s.vy * 0.3 * Math.sin(slope * D2R) : 0);
      const e = fx.emit('land'); e.v = -s.vy; s.vy = 0;
    }
    if (!grounded && s.was) s.air = true;
    s.was = grounded;

    if (grounded) {
      tangent(n);
      const ag = Math.abs(s.gsp);
      // spin dash: crouch, then jump taps rev it up (+2 each, cap 8, decays x31/32 per frame), releasing crouch launches [doc]
      if (s.charge >= 0) {
        s.charge *= Math.pow(31 / 32, dt * 60);
        if (io.jumpP) { s.charge = Math.min(8, s.charge + 2); const e = fx.emit('spinCharge'); e.v = s.charge; e.n = 1; }
        s.gsp = 0;
        if (!io.crouch) { const rel = (8 + Math.floor(s.charge) / 2) * SC; s.gsp = rel; s.roll = true; const e = fx.emit('spinRelease'); e.v = rel; e.n = Math.floor(s.charge); s.charge = -1; s.lock = 0; }
        else if (io.m > 0.05) turn(s, io, dt, 6);
      } else if (io.crouch && io.jumpP && ag < ROLLMIN && !s.roll) {
        s.charge = 2; s.gsp = 0; const e = fx.emit('spinCharge'); e.v = 2; e.n = 0;
      } else if (s.jb > 0 && s.lock <= 0) {                                          // jump: leaves along the floor normal; height scales with speed [task/guess]
        const frac = clamp(ag / TOP, 0, 1.5), vj = J * (0.95 + 0.35 * Math.min(1, frac)) + (s.roll ? 0 : 0);
        const nx = n ? n.x : 0, ny = n ? n.y : 1, nz = n ? n.z : 0;
        s.vx = s.tx * s.gsp + nx * vj; s.vy = s.ty * s.gsp + ny * vj; s.vz = s.tz * s.gsp + nz * vj;
        out.jumpImpulse = vj; s.jgrace = 0.06; s.jb = 0; s.jumped = true; s.air = true; s.roll = false; s.hused = false;
        const e = fx.emit('jump'); e.v = vj; e.n = Math.round(frac * 100);
      } else {
        if (!s.roll && io.crouch && ag >= ROLLMIN) { s.roll = true; fx.emit('roll'); }
        if (s.roll && ag < UNROLL) s.roll = false;
        // slope: gravity along the heading tangent (ty > 0 = uphill)
        const sf = s.roll ? (s.gsp * s.ty > 0 ? SLP_UP : SLP_DN) : SLP;
        s.gsp -= sf * s.ty * dt;      // ty>0 = heading is uphill: slows gsp>0, speeds gsp<0
        if (s.lock <= 0) {
          if (s.roll) {
            if (io.m > 0.05) { const al = io.wx * -Math.sin(s.face) + io.wz * -Math.cos(s.face); if (al * Math.sign(s.gsp || 1) < -0.3) s.gsp -= Math.sign(s.gsp) * RDEC * dt; turn(s, io, dt, 1.5); }
            s.gsp -= Math.sign(s.gsp) * RFRC * dt;
          } else if (io.m > 0.05) {
            const hx = -Math.sin(s.face), hz = -Math.cos(s.face), al = (io.wx * hx + io.wz * hz) / io.m;     // wish alignment with heading
            if (s.gsp > 0.3 * TOP && al < -0.5) s.gsp = Math.max(0, s.gsp - DEC * dt);                         // brake against the run
            else {
              turn(s, io, dt, 14 / (1 + 3 * clamp(Math.abs(s.gsp) / TOP, 0, 1.5)));                           // turn rate falls with speed
              if (s.gsp < TOP * io.m) s.gsp = Math.min(TOP * io.m, s.gsp + ACC * dt * Math.max(0, al + 0.3));
              else if (s.gsp > TOP * io.m && s.gsp <= TOP) s.gsp = Math.max(TOP * io.m, s.gsp - FRC * dt);
            }
          } else { const d = FRC * dt; s.gsp = Math.abs(s.gsp) <= d ? 0 : s.gsp - Math.sign(s.gsp) * d; }
        }
        if (Math.abs(s.gsp) < FALL && slope >= 45) { s.lock = 0.5; if (slope >= 60) { s.vx = s.tx * s.gsp; s.vy = s.ty * s.gsp; s.vz = s.tz * s.gsp; s.jgrace = 0.2; s.air = true; } }   // [doc] fall-off + 0.5 s control lock
        s.gsp = clamp(s.gsp, -VMAX, VMAX);
      }
      if (s.air) { /* became airborne this step via jump/detach */ }
      else if (s.charge < 0) { s.vx = s.tx * s.gsp; s.vy = s.ty * s.gsp; s.vz = s.tz * s.gsp; }
      else { s.vx = s.vy = s.vz = 0; }
    } else if (s.homing > 0) {
      // homing dash toward the locked target (relative position tracked and refreshed from ctx.targets)
      s.homing -= dt; const T = ctx.targets;
      if (T && T.length) { let bi = -1, bd = 9; for (let i = 0; i < T.length; i++) { const dx = T[i].x - s.hrx, dy = T[i].y - s.hry, dz = T[i].z - s.hrz, d = dx * dx + dy * dy + dz * dz; if (d < bd) { bd = d; bi = i; } } if (bi >= 0) { s.hrx = T[bi].x; s.hry = T[bi].y; s.hrz = T[bi].z; s.hitIdx = bi; } }
      const d = Math.sqrt(s.hrx * s.hrx + s.hry * s.hry + s.hrz * s.hrz) || 1;
      s.vx = s.hrx / d * HOME_SP; s.vy = s.hry / d * HOME_SP; s.vz = s.hrz / d * HOME_SP;
      s.hrx -= s.vx * dt; s.hry -= s.vy * dt; s.hrz -= s.vz * dt;
      if (d < 0.6 || ctx.hitTarget) {                                                // connect: bounce (apex 0.7 [plan]) and re-arm
        const e = fx.emit('homingHit'); e.n = s.hitIdx; e.dmg = 6; s.vy = J * Math.sqrt(0.7) * 1.05; s.vx *= 0.25; s.vz *= 0.25; s.homing = 0; s.hused = false; s.jumped = true; out.jumpImpulse = s.vy;
      } else if (s.homing <= 0) { s.vx *= 0.5; s.vz *= 0.5; s.vy = 0; }
    } else {
      // airborne: variable jump height, air accel (only while under top speed), gravity
      if (s.jumped && !io.jump && s.vy > 0.615 * J) s.vy = 0.615 * J;                // [doc] release cap 4/6.5 of jump speed
      if (io.m > 0.05) { const hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz); if (hs < TOP || (s.vx * io.wx + s.vz * io.wz) < 0) { s.vx += io.wx * io.m * AIRA * dt; s.vz += io.wz * io.m * AIRA * dt; } turn(s, io, dt, 8); }
      s.vy -= g * dt; s.vy = Math.max(s.vy, -45);
      if (!s.hused && (io.RP || io.jumpP) && s.jumped !== undefined && ctx.targets && ctx.targets.length) {      // homing attack: nearest in the forward cone within 8 H
        const fx0 = -Math.sin(s.face), fz0 = -Math.cos(s.face); let bi = -1, bd = HOME_R * HOME_R;
        for (let i = 0; i < ctx.targets.length; i++) { const t = ctx.targets[i], d2 = t.x * t.x + t.y * t.y + t.z * t.z; if (d2 > bd || d2 < 1e-6) continue; const dl = Math.sqrt(d2); if ((t.x * fx0 + t.z * fz0) / dl < 0.34) continue; bd = d2; bi = i; }   // cone ~70 deg
        if (bi >= 0) { const t = ctx.targets[bi]; s.hrx = t.x; s.hry = t.y; s.hrz = t.z; s.homing = HOME_T; s.hused = true; s.hitIdx = bi; s.face = Math.atan2(-t.x, -t.z); const e = fx.emit('homing'); e.n = bi; e.dx = t.x; e.dy = t.y; e.dz = t.z; e.v = HOME_SP; }
      }
    }
    // outputs
    const hs = Math.sqrt(s.vx * s.vx + s.vz * s.vz), onG = grounded && !s.air;
    hitbox.rolling = s.roll || s.charge >= 0 || (s.air && s.jumped); hitbox.h = hitbox.rolling ? 0.5 : 0.9;
    out.vel.x = s.vx; out.vel.y = s.vy; out.vel.z = s.vz; out.groundSnap = onG && out.jumpImpulse === 0 && s.homing <= 0;       // glued to terrain at speed while on the ground
    a.moving = hs > 0.5; a.running = hs > 0.7 * TOP; a.airborne = !onG; a.speed = clamp(hs / TOP, 0, 1); a.facing = s.face; a.mode = s.homing > 0 ? 3 : s.charge >= 0 ? 2 : s.roll ? 1 : 0;
    a.ball = hitbox.rolling ? 1 : 0; a.charge = s.charge >= 0 ? s.charge / 8 : 0; a.crouch = s.charge >= 0 ? 1 : 0; a.pitch = s.homing > 0 ? 1 : 0;
    out.camHint.fov = 1 + 0.25 * clamp(hs / (2 * TOP), 0, 1); out.camHint.dist = 1 + 0.35 * clamp(hs / (2 * TOP), 0, 1); out.camHint.shake = s.charge >= 0 ? 0.15 * s.charge / 8 : 0;
    return out;
  }
  return { reset, step, state: s, out, hitbox };
}

// ── registry + skins ─────────────────────────────────────────────────────────────────────────────
const SKINS = {
  MARO:  { color: 0xB86CFF, suit: { jetpack: 0, scanner: 0, sprint: 1, storage: 0 }, role: 'miner', extra: ['cap'] },                       // stout runner, round cap, violet + orange
  JOSHI: { color: 0x59E08A, suit: { jetpack: 0, scanner: 0, sprint: 0, storage: 1 }, role: 'scout', extra: ['snout', 'crest', 'tail'] },    // round lizard, nub nose, mint + orange crest
  STEEV: { color: 0x30C0C8, suit: { jetpack: 0, scanner: 0, sprint: 0, storage: 2 }, role: 'miner', extra: ['cube', 'blocky'] },           // cube head, block limbs, teal
  SONIK: { color: 0xFFC93C, suit: { jetpack: 0, scanner: 1, sprint: 3, storage: 0 }, role: null,    extra: ['quills', 'visor'] },          // quilled runner, pointed visor, gold + violet
};
export const STYLES = {
  MARO:  { id: 'MARO',  name: 'MARO',  portraitHint: 'stout runner, round cap, violet and orange',        skin: SKINS.MARO,  abilities: ['jumpChain', 'longJump', 'backflip', 'sideFlip', 'wallKick', 'dive', 'groundPound', 'slide', 'punch'], hitbox: { r: 0.24, h: 1.0 }, make: maro },
  JOSHI: { id: 'JOSHI', name: 'JOSHI', portraitHint: 'round lizard, nub nose, mint with orange crest',    skin: SKINS.JOSHI, abilities: ['flutter', 'eggThrow', 'tongue', 'groundPound'],                                                      hitbox: { r: 0.26, h: 0.95 }, make: joshi },
  STEEV: { id: 'STEEV', name: 'STEEV', portraitHint: 'cube head, block limbs, teal shirt',                skin: SKINS.STEEV, abilities: ['sprintJump', 'blockMine', 'blockPlace', 'craftTable', 'sneak'],                                      hitbox: { r: 0.2, h: 1.05 }, make: steev },
  SONIK: { id: 'SONIK', name: 'SONIK', portraitHint: 'quilled gold runner, pointed visor',                skin: SKINS.SONIK, abilities: ['momentum', 'roll', 'spinDash', 'homingAttack'],                                                      hitbox: { r: 0.2, h: 0.9 }, make: sonik },
};
export function skinFor(id) {
  const k = SKINS[String(id || '').toUpperCase()];
  if (!k) return { color: 0xB86CFF };
  return { color: k.color, suit: Object.assign({}, k.suit), role: k.role, extra: k.extra.slice() };
}
export function createStyle(id, env) {
  const S = STYLES[String(id || '').toUpperCase()]; if (!S) return null;
  const e = Object.assign({ walk: 22, grav: 22, jump: 8.5 }, env || {});
  const m = S.make(e), api = { id: S.id, name: S.name, portraitHint: S.portraitHint, skin: skinFor(S.id), abilities: S.abilities.slice(), hitbox: m.hitbox || Object.assign({}, S.hitbox), reset: m.reset, step: m.step, state: m.state };
  for (const k of ['swallow', 'previewArc', 'setTool']) if (m[k]) api[k] = m[k];
  if ('eggs' in m) Object.defineProperty(api, 'eggs', { get() { return m.eggs; } });
  m.reset(); return api;
}
