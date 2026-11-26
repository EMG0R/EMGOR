// GOR BRAWL shared geometry: deterministic sin/cos, capsule tests, hurt / hit capsules and the launch-arc predictor.
// Pure, DOM-free, imports only smash-phys.js + the AUTO angle constant, so BOTH the engine and the bot can import it (the bot must not
// import the engine: the engine imports the bot). `BRAWL_API` below is the read-only surface the bot uses for geometry + kill prediction.
import { PHYS, physOf, knockback, launchSpeed, hitstunOf, diBend } from './smash-phys.js';
import { AUTO } from './smash-fighters.js';

// ---- deterministic math (own sin table: no Math.sin in the sim) -------------------------------------------------------
const SIN = new Float64Array(362);
(function () {
  for (let d = 0; d <= 361; d++) {
    const dd = d <= 90 ? d : d <= 270 ? 180 - d : d - 360, x = dd * 3.141592653589793 / 180, x2 = x * x;
    SIN[d] = x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110 * (1 - x2 / 156))))));
  }
})();
export function sinD(a) { a %= 360; if (a < 0) a += 360; const i = a | 0, fr = a - i; return SIN[i] + (SIN[i + 1] - SIN[i]) * fr; }
export function cosD(a) { return sinD(a + 90); }
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

export function segSeg2(p1x, p1y, q1x, q1y, p2x, p2y, q2x, q2y) {
  const dx1 = q1x - p1x, dy1 = q1y - p1y, dx2 = q2x - p2x, dy2 = q2y - p2y, rx = p1x - p2x, ry = p1y - p2y;
  const a = dx1 * dx1 + dy1 * dy1, e = dx2 * dx2 + dy2 * dy2, f = dx2 * rx + dy2 * ry; let s, t;
  if (a <= 1e-9 && e <= 1e-9) return rx * rx + ry * ry;
  if (a <= 1e-9) { s = 0; t = clamp(f / e, 0, 1); }
  else {
    const c = dx1 * rx + dy1 * ry;
    if (e <= 1e-9) { t = 0; s = clamp(-c / a, 0, 1); }
    else {
      const b = dx1 * dx2 + dy1 * dy2, den = a * e - b * b;
      s = den !== 0 ? clamp((b * f - c * e) / den, 0, 1) : 0; t = (b * s + f) / e;
      if (t < 0) { t = 0; s = clamp(-c / a, 0, 1); } else if (t > 1) { t = 1; s = clamp((b - c) / a, 0, 1); }
    }
  }
  const cx = rx + dx1 * s - dx2 * t, cy = ry + dy1 * s - dy2 * t; return cx * cx + cy * cy;
}

// ---- hurt / hit shapes -------------------------------------------------------------------------------------------------
const HURT = [[0, -6, 0, -14, 7], [0, -20, 0, -28, 8], [0, -35, 0, -35, 6]];
// f needs: x y hurtMul state def (fid optional). `out` is a pooled array of 3 capsules [x0,y0,x1,y1,r] reused across calls.
export function hurtCaps(f, out) {
  let k = f.hurtMul == null ? 1 : f.hurtMul;
  const st = f.state;
  if (st === 'crouch' || st === 'roll' || st === 'dodge' || st === 'getup') k *= 0.62; else if (st === 'land') k *= 0.85;
  if (out.length !== 3) { out.length = 0; for (let i = 0; i < 3; i++) out.push([0, 0, 0, 0, 0]); }
  const hr = f.def.hurtR || (f.fid === 'swift' ? 0.9 : 1);
  for (let i = 0; i < 3; i++) { const c = HURT[i], o = out[i]; o[0] = f.x + c[0]; o[1] = f.y + c[1] * k; o[2] = f.x + c[2]; o[3] = f.y + c[3] * k; o[4] = c[4] * hr; }
  return out;
}
export function hitCapsuleWorld(f, h) { const d = f.face; return [f.x + d * h.seg[0], f.y + h.seg[1], f.x + d * h.seg[2], f.y + h.seg[3], h.r]; }
// does hit def `h`, thrown by an attacker at (ax, ay) facing `face`, touch the hurt capsules `caps`?
const _hc = [0, 0, 0, 0, 0];
export function hitTouches(ax, ay, face, h, caps) {
  const q = _hc; q[0] = ax + face * h.seg[0]; q[1] = ay + h.seg[1]; q[2] = ax + face * h.seg[2]; q[3] = ay + h.seg[3]; q[4] = h.r;
  for (let i = 0; i < caps.length; i++) { const c = caps[i], rr = q[4] + c[4]; if (segSeg2(q[0], q[1], q[2], q[3], c[0], c[1], c[2], c[3]) < rr * rr) return true; }
  return false;
}

// ---- launch ----------------------------------------------------------------------------------------------------------------
export function launchDir(grounded, ang, dir) {
  if (ang === AUTO) ang = grounded ? 25 : 44;
  return [cosD(ang) * dir, sinD(ang)];
}
const ARC = { ko: false, t: 0, x: 0, y: 0, land: false, stun: 0, vx: 0, vy: 0 };
// Predict the flight of a launched fighter `v` (needs x y grounded def) from a hit of knockback kb at angle ang toward dir, with optional DI stick (sx, sy).
// No air control and no tech after the hitstun: ko = leaves the blast zone before landing on the main solid, within hitstun + 25 frames.
// Returns a pooled object (valid until the next call): { ko, t (frames to exit/land), x, y, land, stun }.
export function launchArc(stage, v, kb, ang, dir, sx, sy) {
  const P = physOf(v.def), dl = launchDir(v.grounded, ang, dir);
  let lx = dl[0], ly = dl[1];
  if (sx || sy) { const b = diBend(lx, ly, sx, sy, cosD, sinD); lx = b[0]; ly = b[1]; }
  const sp = launchSpeed(kb); let vx = lx * sp, vy = -ly * sp; const kdx = Math.abs(lx) * PHYS.kbDecay, kdy = Math.abs(ly) * PHYS.kbDecay;
  let x = v.x, y = v.y, grounded = v.grounded;
  if (grounded) { if (vy > 0) vy = -vy * 0.4; if (vy > -1.4) vy = 0; else grounded = false; }
  const stun = Math.max(6, hitstunOf(kb)), b = stage.blast, ms = stage.solids[0], lim = stun + 25; let ko = false, land = false, t = 0;
  const cap = P.fall;
  if (!grounded) {
    for (t = 1; t <= lim; t++) {
      if (t <= stun) { if (vx > 0) vx = Math.max(0, vx - kdx); else if (vx < 0) vx = Math.min(0, vx + kdx); if (vy < 0) vy = Math.min(0, vy + kdy); }
      else { const d = PHYS.airDrag; vx = vx > d ? vx - d : vx < -d ? vx + d : 0; }
      vy += P.g; if (vy > cap) vy = cap;
      x += vx; const py = y; y += vy;
      if (x < b.l || x > b.r || y < b.t || y > b.b) { ko = true; break; }
      if (vy >= 0 && x + 5 > ms.x0 && x - 5 < ms.x1 && py <= ms.y0 + 0.02 && y >= ms.y0) { land = true; break; }
    }
  } else { land = true; }
  ARC.ko = ko; ARC.t = t; ARC.x = x; ARC.y = y; ARC.land = land; ARC.stun = stun; ARC.vx = vx; ARC.vy = vy; return ARC;
}
// the damage percent (victim's current pct excluded) at which hit `h` (dmg, bkb, gr, ang) from direction dir first KOs victim v; 999 when it never does within 300 %
export function koPct(stage, v, h, dir, dmgMul) {
  const dmg = h.dmg * (dmgMul || 1), w = v.def.weight || 100;
  for (let p = 0; p <= 300; p += 10) { const kb = knockback(dmg, p + dmg, w, h.gr, h.bkb); if (launchArc(stage, v, kb, h.ang, dir, 0, 0).ko) return p; }
  return 999;
}

// read-only surface for the bot (nothing here mutates the sim)
export const BRAWL_API = Object.freeze({ hurtCaps, hitCapsuleWorld, hitTouches, segSeg2, launchArc, launchDir, koPct, sinD, cosD });
