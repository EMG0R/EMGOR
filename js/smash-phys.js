// GOR BRAWL physics contract: pure, DOM-free, deterministic (only + - * / sqrt). No gravity / wind / planet input exists here:
// the ring runs in its own 2D frame with ONE constant gravity (PHYS.grav). Nothing outside the match can push a fighter.
//
// Units: px and frames at 60 Hz. H = character height (the standing hurt capsule), 36 px. The reference platform-fighter numbers are
// kept in their native "reference unit" per frame and multiplied by REF (= 36 px / 20 ref-units = 1.8 px per unit) so the feel
// matches numerically: gravity 0.095 u/f^2, fall cap 1.7, fast-fall 1.6x, air accel 0.04, air speed 0.95, run 1.2..1.8 by weight, traction 0.06.
export const H = 36;
export const REF = 1.8;
// ---- presets: CLASSIC = the original reference numbers (pinned by the old tests), FAST = the default feel (higher gravity / fall speed, dash, dash dance,
// short landing lag, shorter endlag, harder launches). PHYS is a mutable exported object: setPreset() patches it in place and clears the physOf cache.
// Everything in a preset is a plain number: both net peers must run the same preset (the default FAST is applied at import on both).
const CLASSIC = {
  grav: 0.095 * REF,          // 0.171 px/f^2
  fallCap: 1.7 * REF,         // 3.06 px/f
  fastFall: 1.6,              // multiplier on the cap
  hopFull: 1.45 * H, hopShort: 0.55 * H, hopDouble: 1.3 * H, hopMulti: 1.0 * H,   // apex heights (px): full hop / short hop / double jump / 3rd+ jump
  airAccel: 0.04 * REF, airSpeed: 0.95 * REF, airDrag: 0.02 * REF,
  runMin: 1.2 * REF, runMax: 1.8 * REF, traction: 0.06 * REF, brake: 3,            // run speed range by weight; ground friction; run-brake multiplier when the stick is released
  squat: 3, turn: 4, shortWindow: 3,         // jump-squat frames; turnaround frames; frames after takeoff in which a released button still makes a short hop
  buffer: 6,                  // input buffer frames
  launch: 0.03 * REF,         // px/f of launch speed per point of knockback
  kbDecay: 0.051 * REF,       // knockback speed lost per frame in the air
  di: 18,                     // max directional-influence bend, degrees
  smashCharge: 60, smashMul: 1.4,
  hitlagBase: 4,
  // ---- fields below were added with the preset system; CLASSIC values reproduce the old literals exactly
  accel: 0.4, revMul: 4, dashFrames: 0, dashAccel: 0.4, dashMul: 1, ddWindow: 0,      // ground accel; reverse-skid multiplier; dash burst frames / accel / speed mult; dash-dance window (frames of a run)
  landLag: 4, adLandLag: 6, adSpeed: 3.2, adInv: 30, adEnd: 40,                       // generic landing lag; air-dodge landing lag, speed, last intangible frame, length
  hitstunMul: 0.4, hitlagKb: 1, koLag: 0,                                             // hitstun = floor(kb * mul); hitlag x on kb>=60 hits; KO-range hit freeze (0 = off)
  startScale: 1, endScale: 1, landScale: 1,                                           // global move startup / endlag / aerial landing-lag multipliers (applied in buildMove)
  ffSnap: 0, ffHeld: 0, perFighter: 0,                                                // fast fall snaps to its speed; D held (not just a fresh press) fast-falls; honour def.run/grav/fall/air
  count: 150, speed: 1,                                                               // count-in frames; local sim speed multiplier (net lockstep always runs 1.0)
  camMin: 7 * H, camMax: 20 * H, camPos: 0.08, camZoom: 0.04,                         // camera width clamp (px) + lerp rates
};
const FAST = Object.assign({}, CLASSIC, {
  grav: 0.14 * REF, fallCap: 2.4 * REF, fastFall: 1.7,
  airAccel: 0.13 * REF, airSpeed: 1.3 * REF, airDrag: 0.04 * REF,
  runMin: 1.7 * REF, runMax: 2.4 * REF, traction: 0.15, brake: 4,
  buffer: 8, launch: 0.065, kbDecay: 0.11,
  accel: 0.8, revMul: 4, dashFrames: 6, dashAccel: 0.9, dashMul: 1.15, ddWindow: 8,
  landLag: 2, adLandLag: 4, adSpeed: 4.5, adInv: 22, adEnd: 32,
  hitstunMul: 0.36, hitlagKb: 1.3, koLag: 14,
  startScale: 0.9, endScale: 0.8, landScale: 0.75,
  ffSnap: 1, ffHeld: 1, perFighter: 1, count: 90, speed: 1.15,
  camMin: 5 * H, camMax: 12 * H, camPos: 0.1, camZoom: 0.12,
});
export const PRESETS = { classic: CLASSIC, fast: FAST };
export const PHYS = Object.assign({ H, REF, preset: 'fast', epoch: 0 }, FAST);
const LISTEN = [];
export function onPreset(fn) { LISTEN.push(fn); }
export function setPreset(name) {
  const p = PRESETS[String(name).toLowerCase()]; if (!p) return PHYS.preset;
  Object.assign(PHYS, p); PHYS.preset = String(name).toLowerCase(); PHYS.epoch++; CACHE = new WeakMap();
  for (const fn of LISTEN) fn(PHYS.preset);
  return PHYS.preset;
}

export const clampN = (v, a, b) => (v < a ? a : v > b ? b : v);

// ---- per-fighter derived physics (cached per def) ---------------------------------------------------------------
let CACHE = new WeakMap();
export function physOf(def) {
  let p = CACHE.get(def); if (p) return p;
  const w = clampN(def.weight || 100, 55, 140), wf = clampN((w - 60) / 80, 0, 1);
  if (PHYS.perFighter) {
    // per-fighter feel: the def's own run / grav / fall / air numbers (reference units, pilot = 1.5 / 0.095 / 1.7 / 0.10) scale the preset values
    const gr = clampN((def.grav || 0.095) / 0.095, 0.68, 1.3), fr = clampN((def.fall || 1.7) / 1.7, 0.65, 1.4), ar = clampN((def.air || 0.10) / 0.10, 0.7, 1.4);
    const run = PHYS.runMin + (clampN(def.run || 1.5, 1.0, 2.0) - 1.0) * (PHYS.runMax - PHYS.runMin);      // def.run 1.0..2.0 -> runMin..runMax
    p = {
      g: PHYS.grav * gr, fall: PHYS.fallCap * fr, run: def.runPx != null ? def.runPx : run,
      air: PHYS.airAccel * ar, airMax: def.airSpeed != null ? def.airSpeed * REF : PHYS.airSpeed * (0.8 + 0.2 * ar), trac: PHYS.traction,
      jumps: def.jumps || 1, multi: (def.jumps || 1) > 2,
    };
  } else {
    const gm = 0.94 + 0.12 * wf;                       // heavier = a touch faster fall; the base gravity is the same for everyone
    p = {
      g: PHYS.grav * gm, fall: PHYS.fallCap * gm, run: PHYS.runMax - (PHYS.runMax - PHYS.runMin) * wf,
      air: PHYS.airAccel, airMax: PHYS.airSpeed, trac: PHYS.traction,
      jumps: def.jumps || 1, multi: (def.jumps || 1) > 2,
    };
  }
  p.vFull = jumpSpeed(p.g, PHYS.hopFull); p.vShort = jumpSpeed(p.g, PHYS.hopShort);
  p.vDbl = jumpSpeed(p.g, p.multi ? PHYS.hopMulti : PHYS.hopDouble);
  CACHE.set(def, p); return p;
}
// discrete apex of the engine's integrator (v applied at takeoff, then vy += g before every later y += vy) = v^2/2g + v/2  ->  solve for v
export function jumpSpeed(g, apex) { return Math.sqrt(g * g / 4 + 2 * g * apex) - g / 2; }

// ---- combat numbers ------------------------------------------------------------------------------------------------
// kb = ((dmg/10 + dmg*d/20) * (200/(w+100)) * 1.4 + 18) * s + b      d = victim percent after the hit, s = scaling/100, b = base
export function knockback(dmg, pctAfter, weight, scaling, base) {
  return ((dmg / 10 + dmg * pctAfter / 20) * (200 / (weight + 100)) * 1.4 + 18) * (scaling / 100) + base;
}
// hitlag: floor(dmg/3)+4; FAST adds +30 % on kb>=60 hits and freezes 14-18 f on a hit predicted to KO (ko = launch-arc test done by the engine)
export function hitlagOf(dmg, kb, ko) {
  let l = Math.floor(dmg / 3) + PHYS.hitlagBase;
  if (kb != null && kb >= 60 && PHYS.hitlagKb !== 1) l = Math.floor(l * PHYS.hitlagKb);
  if (ko && PHYS.koLag) l = Math.max(l, Math.min(PHYS.koLag + 4, PHYS.koLag + Math.floor((kb || 0) / 60)));
  return l;
}
export const hitstunOf = (kb) => Math.floor(kb * PHYS.hitstunMul);
export const launchSpeed = (kb) => kb * PHYS.launch;

// ---- the ground / air state machine (pure: reads the fighter record, writes its kinematics + state, returns a request code) ----
// Fighter record fields used: state st grounded x y vx vy face inp ed bufA bufB bufG bufJ jumps ff jumpT adUsed flutterT sh dropT sq def
// Returns '' or a request the sim must serve: 'A' attack, 'B' special, 'G' grab, 'S' shield, 'DJ' double jump just fired, 'JUMP' takeoff, 'DROP' drop-through, 'LAND'.
export const L = 1, R = 2, U = 4, D = 8, A = 16, K = 32, S = 64, G = 128, J = 256;
export const dirOf = (f) => (f.inp & R ? 1 : 0) - (f.inp & L ? 1 : 0);
const sgn = (v) => (v > 0 ? 1 : v < 0 ? -1 : 0);

export function groundVx(f, P, dir, crouch) {
  if (crouch) { f.vx = brakeTo(f.vx, P.trac * 2.5); return; }
  if (dir) {
    if (sgn(f.vx) !== dir && f.vx !== 0) f.vx = brakeTo(f.vx, P.trac * PHYS.revMul);      // reversing: skid first
    else {
      const dash = f.dashT > 0, cap = P.run * (dash ? PHYS.dashMul : 1);
      const pv = f.vx * dir; let nv = pv + (dash ? PHYS.dashAccel : PHYS.accel);
      if (nv > cap) nv = pv > cap ? Math.max(cap, pv - P.trac * 2) : cap;      // the dash burst bleeds back to run speed instead of snapping
      f.vx = dir * nv;
    }
  } else f.vx = brakeTo(f.vx, P.trac * PHYS.brake);
}
function brakeTo(v, d) { return v > d ? v - d : v < -d ? v + d : 0; }
export function airVx(f, P, dir, mult) {
  const a = P.air * (mult == null ? 1 : mult);
  if (dir) { if (f.vx * dir < P.airMax) { f.vx += dir * a; if (f.vx * dir > P.airMax) f.vx = dir * P.airMax; } else f.vx = brakeTo(f.vx, PHYS.airDrag); }
  else f.vx = brakeTo(f.vx, PHYS.airDrag);
}
export function fall(f, P, gmul) {
  let cap = P.fall * (f.ff ? PHYS.fastFall : 1);
  f.vy += P.g * (gmul == null ? 1 : gmul); if (f.vy > cap) f.vy = cap;
}

export function locoStep(f, P, env) {
  const dir = dirOf(f);
  switch (f.state) {
    case 'idle': case 'run': case 'crouch': case 'turn': {
      if (!f.grounded) { f.state = 'air'; f.st = 0; f.ff = false; return ''; }
      f.jumps = f.def.jumps || 1; f.upUsed = false; f.flutterT = 0; f.adUsed = 0;
      if (f.bufA > 0 || f.bufC > 0) return 'A';
      if (f.bufB > 0) return 'B';
      if (f.bufG > 0) return 'G';
      if (f.inp & S && f.sh > 0) return 'S';
      if (f.bufJ > 0) { f.state = 'squat'; f.st = 0; f.bufJ = 0; return ''; }
      if (f.ed & D && env && env.thin) return 'DROP';
      if (f.state === 'turn') {
        groundVx(f, P, 0, false);
        if (f.st >= PHYS.turn) { if (f.turnTo) f.face = f.turnTo; f.state = 'idle'; f.st = 0; }
        return '';
      }
      if (f.dashT > 0) f.dashT--;
      if (dir && dir !== f.face) {
        if (f.state === 'run' && Math.abs(f.vx) > 1.4) {
          if (PHYS.ddWindow > 0 && f.st <= PHYS.ddWindow) {      // dash dance: a reversal inside the first frames of a run flips instantly (no turn state)
            f.face = dir; f.vx = dir * P.run * 0.95; f.dashT = PHYS.dashFrames; f.st = 0; return '';
          }
          f.state = 'turn'; f.st = 0; f.turnTo = dir; return '';      // pivot: the run has to brake first
        }
        f.face = dir;
      }
      const cr = !!(f.inp & D);
      if (dir && !cr && PHYS.dashFrames > 0 && f.state !== 'run' && !(f.dashT > 0) && Math.abs(f.vx) < 1) f.dashT = PHYS.dashFrames;      // run start = dash burst
      groundVx(f, P, cr ? 0 : dir, cr);
      const ns = cr ? 'crouch' : Math.abs(f.vx) > 0.5 ? 'run' : 'idle';
      if (ns === 'run' && f.state !== 'run') f.st = 0;      // st = frames into the run (the dash-dance window)
      f.state = ns;
      return '';
    }
    case 'squat': {
      if (!f.grounded) { f.state = 'air'; f.st = 0; return ''; }
      f.vx = brakeTo(f.vx, P.trac * 2);
      if (f.st >= PHYS.squat) {
        const held = (f.inp & (U | J)) !== 0;
        f.vy = -(held ? P.vFull : P.vShort); f.grounded = false; f.jumpT = held ? 0 : 99; f.state = 'air'; f.st = 0; f.ff = false; f.jumps = f.def.jumps || 1;
        return 'JUMP';
      }
      return '';
    }
    case 'land': {
      if (f.grounded) f.vx = brakeTo(f.vx, P.trac * 3);
      if (--f.lag <= 0) { f.state = 'idle'; f.st = 0; return ''; }
      if (f.bufJ > 0) { f.state = 'squat'; f.st = 0; f.bufJ = 0; }
      return '';
    }
    case 'air': {
      if (f.landed || f.grounded) { f.state = 'land'; f.lag = PHYS.landLag; f.st = 0; f.adUsed = 0; f.helpless = 0; return 'LAND'; }
      if (f.jumpT < PHYS.shortWindow + 1) { f.jumpT++; if (!(f.inp & (U | J)) && f.vy < 0) { f.vy = -P.vShort; f.jumpT = 99; } }
      if (!f.helpless && (f.bufA > 0 || f.bufC > 0)) return 'A';
      if (!f.helpless && f.bufB > 0) return 'B';
      let code = '';
      if (f.bufJ > 0 && f.jumps > 0 && !f.helpless) {
        if (f.def.flutter) { f.vy = -2.9; f.flutterT = 45; f.jumps--; } else { f.vy = -P.vDbl; f.jumps--; }
        f.jumpT = 99; f.ff = false; f.bufJ = 0; airVx(f, P, dir, 1); return 'DJ';      // takeoff frame: no gravity, same integrator as the ground jump
      }
      if (f.ed & S && !f.adUsed && !f.helpless) { f.state = 'airdodge'; f.st = 0; f.adUsed = 1; const ax = dir, ay = (f.inp & D ? 1 : 0) - (f.inp & U ? 1 : 0); const m = ax && ay ? 0.7071 : 1; f.vx = ax * PHYS.adSpeed * m; f.vy = ay * PHYS.adSpeed * m; return 'AD'; }
      if ((PHYS.ffHeld ? f.inp & D : f.ed & D) && f.vy >= 0 && !f.ff) { f.ff = true; if (PHYS.ffSnap && f.vy < P.fall * PHYS.fastFall) f.vy = P.fall * PHYS.fastFall; code = 'FF'; }
      airVx(f, P, dir, 1);
      if (f.flutterT > 0) { f.flutterT--; if (f.inp & (U | J) && f.vy > -1.8) { f.vy = Math.max(f.vy - 0.05, -1.8); fall(f, P, 0.1); return code; } }
      if (f.def.glide && f.vy > 0 && !f.ff && f.inp & (U | J)) { f.vy += P.g * 0.4; if (f.vy > P.fall * 0.65) f.vy = P.fall * 0.65; return code; }
      fall(f, P, 1);
      return code;
    }
    case 'airdodge': {
      // intangible 3..30, ends at 40; momentum bleeds off, no gravity until the end, then a special-fall (cannot act)
      f.vx *= 0.9; f.vy *= 0.9;
      if (f.st >= 3 && f.st <= PHYS.adInv) f.inv = Math.max(f.inv, 2);      // intangible frames 3..adInv (30 classic)
      if (f.landed || f.grounded) { f.state = 'land'; f.lag = PHYS.adLandLag; f.st = 0; f.adUsed = 0; return 'LAND'; }
      if (f.st >= PHYS.adEnd) { f.state = 'air'; f.st = 0; f.ff = false; f.helpless = 1; }
      return '';
    }
    case 'down': {
      f.vx = brakeTo(f.vx, P.trac * 2);
      return '';
    }
  }
  return '';
}

// DI: bend the launch vector by up to PHYS.di degrees toward the stick component perpendicular to it. lx,ly = unit launch vector (y up).
export function diBend(lx, ly, sx, sy, cosD, sinD) {
  if (!sx && !sy) return [lx, ly];
  const L2 = Math.sqrt(sx * sx + sy * sy), ux = sx / L2, uy = sy / L2, pp = ux * -ly + uy * lx, phi = PHYS.di * pp, c = cosD(phi), s = sinD(phi);
  return [lx * c - ly * s, lx * s + ly * c];
}
