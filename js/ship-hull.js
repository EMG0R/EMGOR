// Procedural ship hulls for NO MANS GOR, composed from js/ship-parts.js. Nose -Z, tail +Z, length 1.0, up +Y.
//   buildHull(THREE, { lod: 'low' | anything, kind: 'hauler' | 'fighter' | 'explorer' }) -> Group
//     hauler (default) = the NMS hauler replica (magenta boxy fuselage, orange V + tan band decals, 3 tank boosters, wing pods, engine ring)
//     fighter / explorer = seeded part recipes (fixed seeds, deterministic)
//   Group.userData: thrusters[], gunMuzzles[] (Vector3, hull units), norm, triangles, kind.   Group.lo = lazily built low-LOD twin (same normalization).
//   Two meshes (lit + emissive) on the SHARED static materials from makeMaterials (hull geometry is private per call: ship.js re-tints vertex colours).
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { compose, makeMaterials, mulberry, seededRecipe, hullSection } from './ship-parts.js';

const C = {
  hull: 0x8E3A9C, panel: 0x5E2470, orange: 0xF05A28, tan: 0xC9A96A, glass: 0x14101E,
  light: 0xE8F6FF, grey: 0x8A8691, dark: 0x2A2430, yellow: 0xF2C94C, pink: 0xE060B0, tanDark: 0x4A3A22,
};
const PAL = { base: C.hull, panel: C.panel, accent: C.orange, glow: C.light, dark: C.dark, metal: C.grey };
const K = 9.4;                 // unit hull (length 1) -> original hauler raw frame (fuselage -4.7 .. 4.7)
const u = (x, y, z) => [x / K, y / K, z / K];

// measured from the original full hauler so thrusters / muzzles / the low-LOD land at exactly the same normalized spots
const NORM_FULL = { x: 0, y: 1.0750000476837158, z: 0.16249990463256836, s: 0.09341429305249369 };
const THRUSTERS_RAW = [[0, 0, 5.52], [-1.85, 0.42, 5.1], [-1.85, -0.42, 5.1], [1.85, 0.42, 5.1], [1.85, -0.42, 5.1]];
const MUZZLES_RAW = [[-0.9, -0.4, -5.2], [0.9, -0.4, -5.2]];

function haulerRecipe(lod, up, mods) {
  const lo = lod === 'lo', P = (o) => Object.assign({}, o);
  const W = 0.65, H = 0.53;
  const kids = [];
  const add = (part, params, pos, extra) => kids.push(Object.assign({ part, params, offset: u(pos[0], pos[1], pos[2]) }, extra || {}));
  // cockpit canopy (glass frame bars come with the part)
  add('canopy', { len: 2.8 / K, wid: 1.05 / K, h: 0.75 / K, glass: C.glass, tintK: 0.25 }, [0, 0.8, -1.85]);
  // saddle under the boosters + three stacked ribbed tank boosters on the top rear
  if (!lo) {
    add('plate', { w: 1.8 / K, d: 2.2 / K, th: 0.4 / K }, [0, 0.7, 2.2]);
    for (const [bx, by] of [[-0.55, 1.28], [0.55, 1.28], [0, 1.95]]) {
      add('tank', { r: 0.46 / K, h: 2.1 / K, glowCol: C.orange }, [bx, by, 1.1], { rot: [Math.PI / 2, 0, 0], id: 'boost' });
    }
  }
  // tail fin on the left, small counter fin on the right
  add('fin', { len: 3.4 / K, w: 3.4 / K, sweep: 0.44, thick: 0.15 / K, taper: 0.4, tip: 0x7A2F5A }, [-1.14, 0.85, 3.3], { rot: [0, 0, 0] });
  if (!lo) add('fin', { len: 0.95 / K, w: 1.0 / K, sweep: 0.2, thick: 0.1 / K, taper: 0.7 }, [1.2, 0.85, 3.9], { rot: [-0.3, 0, 0] });
  // side cargo pods on the front flanks + nose cannons
  if (!lo) {
    add('block', { w: 0.85 / K, h: 1.3 / K, d: 1.9 / K, wall: C.panel, rows: 0 }, [1.9, -0.7, -1.7], { mirror: 'x', id: 'cargo' });
    add('block', { w: 0.5 / K, h: 0.5 / K, d: 0.8 / K, wall: C.grey, rows: 0, simple: true }, [1.85, -0.45, -0.4], { mirror: 'x', id: 'bin' });
    add('barrel', { len: 1.3 / K, r: 0.14 / K, glowCol: C.light }, [0.9, -0.4, -3.9], { rot: [-Math.PI / 2, 0, 0], mirror: 'x', id: 'cannon' });
  }
  // rear wing pods with stacked thruster slots, wing arms + hooks, antenna rods, underslung bits
  add('block', { w: 1.1 / K, h: 1.7 / K, d: 2.3 / K, rows: 1, glowCol: C.light }, [1.85, -0.85, 3.85], { mirror: 'x', id: 'wingpod' });
  add('block', { w: 1.7 / K, h: 0.22 / K, d: 0.4 / K, wall: C.dark, rows: 0, simple: true }, [3.0, 0.64, 3.3], { mirror: 'x', id: 'arm' });
  add('block', { w: 0.25 / K, h: 0.9 / K, d: 0.35 / K, wall: C.grey, rows: 0, simple: true }, [3.8, -0.1, 3.3], { mirror: 'x', id: 'hook' });
  if (!lo) {
    add('antenna', { len: 2.9 / K }, [3.8, 0.35, 3.3], { rot: [0, 0.34, -Math.PI / 2], mirror: 'x', id: 'rodA' });
    add('antenna', { len: 2.4 / K, dish: false }, [3.8, -0.1, 3.4], { rot: [0, -0.42, -Math.PI / 2 - 0.12], mirror: 'x', id: 'rodB' });
    add('pod', { len: 0.5 / K, r: 0.16 / K }, [1.95, -1.15, 3.35], { rot: [Math.PI / 2, 0, 0], mirror: 'x', id: 'under' });
    for (const y of [0.42, -0.42]) add('slot', { w: 0.78 / K, h: 0.6 / K, th: 0.06 / K, glowCol: 0xF4F8FF }, [1.85, y, 5.0], { rot: [Math.PI / 2, 0, 0], mirror: 'x', id: 'slot' });
    // landing legs
    kids.push({ part: 'leg', params: { len: 1.25 / K, knee: -0.04 }, at: 'legF', mirror: 'x', id: 'legF' });
    kids.push({ part: 'leg', params: { len: 1.25 / K, knee: -0.04 }, at: 'legR', mirror: 'x', id: 'legR' });
  }
  // main engine ring
  add('ring', { r: 1.12 / K, len: 1.0 / K, tileA: 0x4A7CFF, tileB: 0x3A66E0 }, [0, 0, 4.35], { rot: [Math.PI / 2, 0, 0], id: 'engine' });
  // optional upgrades / mods: appended last so the default hauler's rng stream and look are untouched
  if (up) {
    const add2 = (part, params, pos, extra) => kids.push(Object.assign({ part, params, offset: u(pos[0], pos[1], pos[2]) }, extra || {}));
    for (let t = 1; t <= up.engine; t++) {            // nacelle extensions above / below the engine ring
      const y = 0.95 + (t - 1) * 0.5, z = 5.0 + (t - 1) * 0.3;
      add2('block', { w: 0.7 / K, h: 0.55 / K, d: (1.6 + t * 0.4) / K, wall: C.panel, rows: 0 }, [0, y + 0.3, z - 0.3], { id: 'nacU' + t });
      add2('block', { w: 0.7 / K, h: 0.55 / K, d: (1.6 + t * 0.4) / K, wall: C.panel, rows: 0 }, [0, -y - 0.1, z - 0.3], { id: 'nacD' + t });
      add2('ring', { r: 0.34 / K, len: 0.35 / K, tileA: 0x4A7CFF, tileB: 0x3A66E0 }, [0, y + 0.3, z + 0.9 + t * 0.2], { rot: [Math.PI / 2, 0, 0], id: 'nacRU' + t });
      add2('ring', { r: 0.34 / K, len: 0.35 / K, tileA: 0x4A7CFF, tileB: 0x3A66E0 }, [0, -y - 0.1, z + 0.9 + t * 0.2], { rot: [Math.PI / 2, 0, 0], id: 'nacRD' + t });
    }
    for (let t = 1; t <= up.shield; t++) {            // emitter studs along the top edge + a faint plate ring
      add2('pod', { len: 0.5 / K, r: 0.2 / K }, [0.9 + t * 0.15, 1.5, -0.4 + t * 0.9], { rot: [0, 0, 0], mirror: 'x', id: 'stud' + t });
    }
    if (up.shield > 0) add2('ring', { r: 2.3 / K, len: 0.08 / K, tileA: 0x7A3C8C, tileB: 0x6A2C7A }, [0, 0, 0.6], { rot: [0, 0, 0], id: 'shieldRing' });
    for (let t = 1; t <= up.cargo; t++) {             // side cargo pods growing per tier
      add2('block', { w: (0.5 + 0.12 * t) / K, h: (0.8 + 0.3 * t) / K, d: 1.0 / K, wall: C.panel, rows: 0 }, [2.35 + 0.1 * t, -0.9 - 0.1 * t, -0.1 + (t - 1) * 1.15], { mirror: 'x', id: 'xcargo' + t });
    }
    if (up.jetpack > 0) {                             // dorsal booster tank
      add2('tank', { r: (0.4 + 0.08 * up.jetpack) / K, h: (1.8 + 0.4 * up.jetpack) / K, glowCol: C.orange }, [0, 2.9, 1.1], { rot: [Math.PI / 2, 0, 0], id: 'jet' });
    }
  }
  if (mods && !lo) {
    const add2 = (part, params, pos, extra) => kids.push(Object.assign({ part, params, offset: u(pos[0], pos[1], pos[2]) }, extra || {}));
    if (mods.scope) add2('tank', { r: 0.12 / K, h: 0.9 / K, glowCol: C.light }, [0.9, -0.1, -3.7], { rot: [Math.PI / 2, 0, 0], mirror: 'x', id: 'scope' });
    if (mods.coil) for (const z of [-4.2, -3.8, -3.4]) add2('ring', { r: 0.26 / K, len: 0.12 / K, tileA: C.light, tileB: C.orange }, [0.9, -0.4, z], { rot: [Math.PI / 2, 0, 0], mirror: 'x', id: 'coil' });
    if (mods.chamber) add2('block', { w: 0.8 / K, h: 0.7 / K, d: 1.0 / K, wall: C.panel, rows: 0 }, [0.9, -0.4, -3.0], { mirror: 'x', id: 'chamber' });
  }
  return {
    part: 'hull', params: { kind: 'brick', wid: W, hgt: H, chamfer: 0.2 }, palette: PAL, lod, flex: false,
    paint: lo ? undefined : { lines: 5, stripes: 0, grime: 0.4, base: C.hull, accent: C.orange, panel: C.panel },
    children: kids,
  };
}

// flat colour decals on the flanks (orange block, V chevron, yellow triangle, tan cargo band) - thin proud geometry hugging the hull surface
function haulerDecals(T) {
  const out = [];
  const sec = (zr) => hullSection('brick', zr / K + 0.5, 0.65, 0.53);
  const face = (zr) => sec(zr).w * K + 0.006;
  const put = (geo, hex, jit) => {
    if (geo.index) geo = geo.toNonIndexed();
    geo.deleteAttribute('normal'); geo.deleteAttribute('uv');
    const c = new T.Color(hex), n = geo.attributes.position.count, a = new Float32Array(n * 3), P = geo.attributes.position;
    for (let i = 0; i < n; i++) {
      const f = 1 - (jit || 0) * (Math.sin(P.getZ(i) * 91.7 + P.getY(i) * 37.1) * 0.5 + 0.5);
      a[i * 3] = c.r * f; a[i * 3 + 1] = c.g * f; a[i * 3 + 2] = c.b * f;
    }
    geo.setAttribute('color', new T.BufferAttribute(a, 3));
    out.push(geo);
  };
  const flank = (z0, z1, y0, y1, hex, s, jit = 0.06) => {
    const g = new T.PlaneGeometry(z1 - z0, y1 - y0); g.rotateY(s > 0 ? Math.PI / 2 : -Math.PI / 2); g.translate(s * face((z0 + z1) / 2), (y0 + y1) / 2, (z0 + z1) / 2); put(g, hex, jit);
  };
  const shape = (pts, s, zc, yc, hex) => {
    const sh = new T.Shape(pts.map(([x, y]) => new T.Vector2(x, y)));
    const g = new T.ShapeGeometry(sh);
    if (s > 0) { g.rotateY(Math.PI / 2); g.translate(face(zc), yc, zc); } else { g.rotateY(-Math.PI / 2); g.translate(-face(zc), yc, zc); }
    put(g, hex, 0.05);
  };
  const Vc = [[-0.65, 0.45], [-0.35, 0.45], [0, -0.1], [0.35, 0.45], [0.65, 0.45], [0, -0.6]];
  for (const s of [-1, 1]) {
    flank(-1.9, 1.3, -0.5, 0.35, C.grey, s, 0.2);                       // grey mid band
    flank(-3.0, -1.9, -0.55, 0.2, C.panel, s, 0.1);
    flank(0.3, 1.1, -0.35, 0.5, C.orange, s);                           // orange block
    shape(Vc, s, 1.95, 0.0, C.orange);                                 // V chevron
    shape([[-0.2, 0], [0.2, 0], [0, 0.35]], s, 1.95, -0.62, C.yellow);  // yellow triangle
    flank(-2.5, -2.1, 0.1, 0.45, C.pink, s);                            // magenta marks
    flank(-2.0, -1.8, -0.4, -0.2, C.orange, s);
    flank(2.9, 3.1, 0.1, 0.25, C.yellow, s);
    for (let i = 0; i < 4; i++) flank(-0.5, -0.1, -0.1 - i * 0.07, -0.07 - i * 0.07, C.dark, s);   // vents
    flank(3.0, 4.2, -0.7, 0.7, C.tan, s, 0.12);                         // tan cargo band with dark stripes
    for (let i = 0; i < 7; i++) flank(3.05 + i * 0.16, 3.13 + i * 0.16, -0.7, 0.7, C.tanDark, s, 0.05);
  }
  return out;
}

const SEEDS = { fighter: 21, explorer: 5 };

function buildRaw(T, kind, lod, o) {
  if (kind === 'hauler') {
    const res = compose(T, haulerRecipe(lod, o && o.up, o && o.mods), mulberry(1407));
    let lit = res.geo;
    if (lod === 'hi') { const dec = haulerDecals(T); dec.forEach(d => d.scale(1 / K, 1 / K, 1 / K)); lit = mergeGeometries([lit].concat(dec), false); dec.forEach(d => d.dispose()); res.geo.dispose(); }
    lit.scale(K, K, K); if (res.emissive) res.emissive.scale(K, K, K);
    const thr = THRUSTERS_RAW.slice();
    if (o && o.up) for (let t = 1; t <= o.up.engine; t++) { const y = 0.95 + (t - 1) * 0.5, z = 5.0 + (t - 1) * 0.3; thr.push([0, y + 0.3, z + 1.3 + t * 0.2], [0, -y - 0.1, z + 1.3 + t * 0.2]); }
    return { lit, emi: res.emissive, thr, mz: MUZZLES_RAW, norm: NORM_FULL };
  }
  const rec = seededRecipe(kind, mulberry((SEEDS[kind] || 7) * 977 + 131));
  rec.lod = lod; rec.flex = false;
  const res = compose(T, rec, mulberry((SEEDS[kind] || 7) + 3));
  const thr = [], mz = [];
  for (const k in res.sockets) {
    const p = res.sockets[k].pos;
    if (/\.exhaust$/.test(k)) thr.push([p.x, p.y, p.z]); else if (/\.muzzle$/.test(k)) mz.push([p.x, p.y, p.z]);
  }
  const bb = new T.Box3().setFromBufferAttribute(res.geo.attributes.position);
  if (res.emissive) bb.union(new T.Box3().setFromBufferAttribute(res.emissive.attributes.position));
  const c = bb.getCenter(new T.Vector3());
  return { lit: res.geo, emi: res.emissive, thr, mz, norm: { x: c.x, y: c.y, z: c.z, s: 1 / (bb.max.z - bb.min.z) }, free: true };
}

function assemble(T, kind, lod, normOverride, o) {
  const raw = buildRaw(T, kind, lod, o);
  const nm = normOverride || raw.norm;
  const fixG = g => { g.translate(-nm.x, -nm.y, -nm.z); g.scale(nm.s, nm.s, nm.s); };
  fixG(raw.lit); if (raw.emi) fixG(raw.emi);
  const fix = a => new T.Vector3(a[0] - nm.x, a[1] - nm.y, a[2] - nm.z).multiplyScalar(nm.s);
  const mats = makeMaterials(T, { flex: false });
  const group = new T.Group();
  const m1 = new T.Mesh(raw.lit, mats.lit); m1.frustumCulled = false; group.add(m1);
  if (raw.emi) { const m2 = new T.Mesh(raw.emi, mats.emissive); m2.frustumCulled = false; group.add(m2); }
  let thr = raw.thr.map(fix), mz = raw.mz.map(fix);
  if (!thr.length) thr = [fix([0, 0, 0.5 / nm.s])];
  if (!mz.length) mz = [fix([-0.04 / nm.s, 0, -0.5 / nm.s]), fix([0.04 / nm.s, 0, -0.5 / nm.s])];
  if (raw.free && mz.length === 1) mz.push(new T.Vector3(-mz[0].x, mz[0].y, mz[0].z));
  if (raw.free) { thr.sort((a, b) => a.x - b.x); mz = mz.length > 2 ? [mz.reduce((a, b) => a.x < b.x ? a : b), mz.reduce((a, b) => a.x > b.x ? a : b)] : mz; }
  group.userData.thrusters = thr;
  group.userData.gunMuzzles = mz;
  group.userData.norm = { x: nm.x, y: nm.y, z: nm.z, s: nm.s };
  group.userData.kind = kind;
  group.userData.triangles = (raw.lit.attributes.position.count + (raw.emi ? raw.emi.attributes.position.count : 0)) / 3;
  return { group, norm: nm };
}

export const HULL_KINDS = ['hauler', 'fighter', 'explorer'];

const clampT = v => Math.max(0, Math.min(3, v | 0));
function norm(opts) {
  const U = (opts && opts.upgrades) || {}, M = (opts && opts.mods) || {};
  const up = { engine: clampT(U.engine), shield: clampT(U.shield), cargo: clampT(U.cargo), jetpack: clampT(U.jetpack) };
  const mods = { scope: !!M.scope, coil: !!M.coil, chamber: !!M.chamber };
  const any = up.engine || up.shield || up.cargo || up.jetpack || mods.scope || mods.coil || mods.chamber;
  return { up: any ? up : null, mods: any ? mods : null, up0: up, mods0: mods };
}
export function hullSignature(opts) {
  const kind = HULL_KINDS.indexOf(opts && opts.kind) >= 0 ? opts.kind : 'hauler', n = norm(opts), u0 = n.up0, m = n.mods0;
  return kind + '|e' + u0.engine + 's' + u0.shield + 'c' + u0.cargo + 'j' + u0.jetpack + '|' + (m.scope ? 'S' : '') + (m.coil ? 'C' : '') + (m.chamber ? 'H' : '');
}

export function buildHull(THREE, opts) {
  const kind = HULL_KINDS.indexOf(opts && opts.kind) >= 0 ? opts.kind : 'hauler';
  const low = !!(opts && opts.lod === 'low');
  const o = kind === 'hauler' ? norm(opts) : null;
  const { group, norm: nrm } = assemble(THREE, kind, low ? 'lo' : 'hi', null, o);
  if (!low) {
    let lo = null;
    Object.defineProperty(group, 'lo', { enumerable: false, configurable: true, get() { return lo || (lo = assemble(THREE, kind, 'lo', nrm, o).group); } });
  }
  return group;
}
