// ship-styles.js — helpers only (the movement styles were cut 2026-11-27): the PILOT rig normaliser (normRig / rigParts / GAITS), the wall-normal helper (wallNormal2)
// and the jetpack input model (mkJetSM / jetStep), imported by ship-foot.js and ship-human.js. Pure maths: no DOM, no THREE. Original: js/_attic/ship-styles.full.js.

const clamp = (v, a, b) => v < a ? a : v > b ? b : v;

const RIG_ENUM = { headShape: ['round', 'cube', 'snout'], torso: ['normal', 'stout', 'slim', 'lizard', 'box', 'barrel', 'pear', 'dome', 'low', 'robe'], limbShape: ['cyl', 'cube'], feet: ['normal', 'big', 'none'], eyeStyle: ['helmet', 'visor', 'dots', 'none'] };
const HEAD_R = 0.14;       // round rig: head ellipsoid y-radius at headScale 1 (helmet top = head centre + HEAD_R * hs[1])
const FEATS = ['nose', 'moustache', 'swept', 'antenna', 'ears', 'longshoes', 'bigmitts', 'ridge', 'dome', 'wings', 'robe', 'hood', 'orbs', 'backspikes'];
const PILOT_HS = 1.3;      // 2026-11 scale retune: the default pilot's head reads at 1.3x
const TORSO_S = { normal: [1, 1, 1], stout: [1.3, 0.88, 1.25], slim: [0.85, 1.05, 0.85], lizard: [0.8, 1.12, 1.0], box: [1.1, 1, 1.1], barrel: [1.5, 1.0, 1.4], pear: [1.2, 0.92, 1.15], dome: [1.55, 0.78, 1.5], low: [1.4, 0.78, 1.25], robe: [1.05, 1.12, 1.05] };   // [x, y, z] bone scale of the torso
export const GAITS = { PILOT: { stride: 1, bounce: 1, arm: 1, lean: 1 } };      // stride = step frequency x, bounce = hip bob x, arm = arm swing x, lean = forward torso lean x
const num = (v, d, a, b) => { v = +v; return v === v ? clamp(v, a, b) : d; };
const pick = (v, list) => list.indexOf(v) >= 0 ? v : list[0];
// normRig(recipe, styleId?) -> flat, fully-defaulted rig: { headShape, hs:[x,y,z], neck, torso, ts:[x,y,z], arm, leg, limbShape, feet, tail:{len,spikes}|null, quills, cap:bool, crest, eyeStyle, palette,
//   gait:{stride,bounce,arm,lean}, extras:[names for ship-human genAtt], hipOff (hips rise so scaled legs still reach y=0), top (helmet top y), hitbox:{r,h} }
export function normRig(rc, id) {
  rc = rc || {};
  const hsr = rc.headScale === undefined && (!id || String(id).toUpperCase() === 'PILOT') ? PILOT_HS : rc.headScale, hs = typeof hsr === 'object' && hsr ? [num(hsr.x, 1, 0.5, 2.5), num(hsr.y, 1, 0.5, 2.5), num(hsr.z, 1, 0.5, 2.5)] : [num(hsr, 1, 0.5, 2.5), num(hsr, 1, 0.5, 2.5), num(hsr, 1, 0.5, 2.5)];
  const ll = rc.limbLen || {}, arm = num(ll.arm, 1, 0.4, 1.6), leg = num(ll.leg, 1, 0.5, 1.8);
  const headShape = pick(rc.headShape, RIG_ENUM.headShape), torso = pick(rc.torso, RIG_ENUM.torso), limbShape = pick(rc.limbShape, RIG_ENUM.limbShape), feet = pick(rc.feet, RIG_ENUM.feet), eyeStyle = pick(rc.eyeStyle, RIG_ENUM.eyeStyle);
  const ts = TORSO_S[torso].slice(), neck = rc.neck !== false;
  const tl = rc.tail ? { len: num(rc.tail.len, 1, 0.3, 3), spikes: Math.round(num(rc.tail.spikes, 0, 0, 6)) } : null;
  const quills = Math.round(num(rc.quills, 0, 0, 9)), stance = num(rc.stance, 0, 0, 0.4), feat = Array.isArray(rc.feat) ? rc.feat.filter(f => FEATS.indexOf(f) >= 0).slice(0, 6) : [];
  const gk = rc.gait && typeof rc.gait === 'object' ? rc.gait : (GAITS[String(id || '').toUpperCase()] || GAITS.PILOT), g0 = GAITS.PILOT;
  const gait = { stride: num(gk.stride, g0.stride, 0.3, 3), bounce: num(gk.bounce, g0.bounce, 0, 3), arm: num(gk.arm, g0.arm, 0, 3), lean: num(gk.lean, g0.lean, 0, 3) };
  const extras = [];
  if (rc.cap) extras.push('cap');
  if (headShape === 'cube') extras.push('cube'); else if (headShape === 'snout') extras.push('snout');
  if (tl) extras.push('tail');
  if (quills) extras.push('quills');
  if (eyeStyle === 'visor') extras.push('visor');
  if (rc.crest) extras.push('crest');
  if (limbShape === 'cube') extras.push('blocky');
  if (feet === 'big') extras.push('bigfeet');
  for (let i = 0; i < feat.length; i++) if (extras.indexOf(feat[i]) < 0) extras.push(feat[i]);
  const hipOff = 0.5 * (leg - 1), hipY = 0.52 + hipOff, headDrop = neck ? 0 : 0.04;
  const top = hipY + (0.93 - 0.52) * ts[1] - headDrop + HEAD_R * hs[1];
  const hitbox = { r: +(0.2 * Math.max(ts[0], ts[2], 0.8 * hs[0], 0.9 * (tl ? 1 + 0.1 * tl.len : 1))).toFixed(3), h: +top.toFixed(3) };
  return { headShape, hs, neck, torso, ts, arm, leg, limbShape, feet, tail: tl, quills, cap: !!rc.cap, crest: !!rc.crest, eyeStyle, palette: rc.palette && typeof rc.palette === 'object' ? { theme: rc.palette.theme != null ? rc.palette.theme : null, base: rc.palette.base != null ? rc.palette.base : null, accent: rc.palette.accent != null ? rc.palette.accent : null, deep: rc.palette.deep != null ? rc.palette.deep : null } : null, stance, gait, extras, hipOff, headDrop, top, hitbox };
}
// rigParts(normRigOut) -> flat list of part tokens ('torso:stout', 'feet:big', 'tail', ...) that ship-human setRig builds from a normalised rig. Pure; used to compare recipes.
export function rigParts(r) {
  const p = ['head:' + r.headShape, 'headScale:' + r.hs.join('x'), 'neck:' + (r.neck ? 1 : 0), 'torso:' + r.torso, 'torsoScale:' + r.ts.join('x'), 'arm:' + r.arm, 'leg:' + r.leg, 'limb:' + r.limbShape, 'feet:' + r.feet, 'eyes:' + r.eyeStyle];
  if (r.tail) p.push('tail:' + r.tail.len + '/' + r.tail.spikes);
  if (r.quills) p.push('quills:' + r.quills);
  if (r.stance) p.push('stance:' + r.stance);
  if (r.cap) p.push('cap'); if (r.crest) p.push('crest');
  for (let i = 0; i < r.extras.length; i++) p.push('x:' + r.extras[i]);
  return p;
}

// Q06: wall normal in the tangent frame (x right, z back) from the wanted move (dx,dz) and the first free slide direction (sx,sz):
// perpendicular to the slide, facing against the move. Writes out.x/out.z; returns false when the slide is degenerate or parallel to the move.
export function wallNormal2(dx, dz, sx, sz, out) {
  const sl = Math.sqrt(sx * sx + sz * sz); if (sl < 1e-6) return false;
  let nx = -sz / sl, nz = sx / sl;
  const d = nx * dx + nz * dz; if (Math.abs(d) < 1e-3) return false;
  if (d > 0) { nx = -nx; nz = -nz; }
  out.x = nx; out.z = nz; return true;
}

// ── jetpack input model (pure + allocation-free) ───────────────────────────────
// tap Space (released < HOLD) = the jump; hold Space >= HOLD s continuously while airborne = jetpack (a grounded hold jumps first, then ignites in the air);
// release = cut; Shift = dive whenever it is down in the air.
export const JET_K = { HOLD: 0.18, RAMP: 1.2, UP: 3, DIVE_V0: 3, DIVE_TAU: 0.6, DIVE_CAP: 48 };
export function mkJetSM() { return { t: 0, jet: false, k: 1, dive: false, diveT: 0, sh: false, shAir: false }; }
// j: state; held = Space down; air = airborne; shift = Shift down  Returns j (jet, k = ramp 1..3.5, dive, diveT).
export function jetStep(j, dt, held, air, shift) {
  if (air && held) j.t += dt; else j.t = 0;
  if (!air) j.shAir = false; else if (shift && !j.sh) j.shAir = true;
  if (!shift) j.shAir = false;
  j.sh = shift;
  j.dive = air && shift;
  if (j.dive) j.diveT += dt; else j.diveT = 0;
  j.jet = j.t > JET_K.HOLD && !j.dive;
  j.k = 1 + 2.5 * clamp((j.t - JET_K.HOLD) / JET_K.RAMP, 0, 1);
  return j;
}
